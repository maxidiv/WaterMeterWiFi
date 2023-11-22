// Load Wi-Fi library
#include <Arduino.h>
#include "Bounce2.h";
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>
#ifdef ESP8266
#include <Updater.h>
#include <ESP8266mDNS.h>
#define U_PART U_FLASH
#else
#include <Update.h>
#include <ESPmDNS.h>
#define U_PART U_SPIFFS
#endif

#define ssid "ASUS-2.4"
#define password "1qaz]'/."

AsyncWebServer server(80);
size_t content_len;

// Variable to store the HTTP request
String header;
const char* PARAM_INPUT_1 = "input1";
const char* PARAM_INPUT_2 = "input2";
// Assign output variables to GPIO pins
const int COLD_PIN = D2;
const int HOT_PIN = D1;
bool last_COLD_PIN_State = false;
bool last_HOT_PIN_State = false;
Bounce debouncer_COLD = Bounce();
Bounce debouncer_HOT = Bounce();
float COLD_counter = 0;
float HOT_counter = 0;
float COLD_counter_last=0;
float HOT_counter_last=0;
float COLD_eeprom=0;
float HOT_eeprom=0;
float Cena_pulse=0.001;
int addr = 0;
unsigned long period_time = (long)60000;
unsigned long my_timer;

String IP = "open-monitoring.online";
//*-- Параметры полученные при регистрации на http://open-monitoring.online
String ID = "1559"; // ID                                             (!)
String KEY = "tviEY2"; // Код доступа                                (!)
long interval = 120000; // Периодичность отправки пакетов на сервер (120 секунд)

//*-- GET Information
String GET = "GET /get?cid=" + ID + "&key=" + KEY; // GET request
String HOST = "Host: " + IP + "\r\n\r\n";



void handleRoot(AsyncWebServerRequest *request) {
  request->redirect("/update");
}

void handleUpdate(AsyncWebServerRequest *request) {
  char* html = "<form method='POST' action='/doUpdate' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  request->send(200, "text/html", html);
}

void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index){
    Serial.println("Update");
    content_len = request->contentLength();
    // if filename includes spiffs, update the spiffs partition
    int cmd = (filename.indexOf("spiffs") > -1) ? U_PART : U_FLASH;
#ifdef ESP8266
    Update.runAsync(true);
    if (!Update.begin(content_len, cmd)) {
#else
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
#endif
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
#ifdef ESP8266
  } else {
    Serial.printf("Progress: %d%%\n", (Update.progress()*100)/Update.size());
#endif
  }

  if (final) {
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
    response->addHeader("Refresh", "20");  
    response->addHeader("Location", "/");
    request->send(response);
    if (!Update.end(true)){
      Update.printError(Serial);
    } else {
      Serial.println("Update complete");
      Serial.flush();
      ESP.restart();
    }
  }
}

void printProgress(size_t prg, size_t sz) {
  Serial.printf("Progress: %d%%\n", (prg*100)/content_len);
}
  

String readCOLD() {
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float C = COLD_counter;
  if (isnan(C)) {
    Serial.println("Failed to read from DHT sensor!");
    return "--";
  }
  else {
    //Serial.println(C);
    return String(C, 3);
  }
}
String readHOT() {
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float H = HOT_counter;
  if (isnan(H)) {
    Serial.println("Failed to read from DHT sensor!");
    return "--";
  }
  else {
    //Serial.println(H);
    return String(H, 3);
  }
}
void send_data(){
  Serial.print("connecting to ");
  Serial.println(HOST);
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(IP, httpPort)) { //works!
    Serial.println("connection failed");
    return;
  }  
  String url = "/get?cid="+ID+"&key="+KEY+"&p1="+String(COLD_counter,3)+"&p2="+String(HOT_counter,3)+"&p3="+String(COLD_counter-COLD_counter_last,3)+"&p4="+String(HOT_counter-HOT_counter_last,3);
  Serial.print("Requesting URL: ");
  Serial.println(url);
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + IP + "\r\n" + 
               "Connection: close\r\n\r\n");

  Serial.println();
  Serial.println("closing connection");
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>

<h2>Maxi House Arduino </h2>
<p>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span class="dht-labels">COLD</span> 
    <span id="COLD_counter">%COLD_counter%</span>
    <sup class="units">m3</sup>
    <form action="/get">
    <input type="text" name="input1">
    <input type="submit" value="Submit">
  </form> 
  </p>
<p>
    <i class="fas fa-tint" style="color:#d60000;"></i> 
    <span class="dht-labels">HOT</span>
    <span id="HOT_counter">%HOT_counter%</span>
    <sup class="units">m3</sup>
    <form action="/get">
    <input type="text" name="input2">
    <input type="submit" value="Submit">
  </form> 
</p>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("COLD_counter").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/COLD_counter", true);
  xhttp.send();
}, 1000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("HOT_counter").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/HOT_counter", true);
  xhttp.send();
}, 1000 ) ;
</script>

</html>)rawliteral";

String processor(const String& var){
  //Serial.println(var);
  if(var == "COLD_counter"){
    return readCOLD();
  }
  else if(var == "HOT_counter"){
    return readHOT();
  }
  return String();
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);
  char host[16];
  #ifdef ESP8266
  snprintf(host, 12, "ESP%08X", ESP.getChipId());
  #else
  snprintf(host, 16, "ESP%012llX", ESP.getEfuseMac());
  #endif
  EEPROM.begin(32);
  EEPROM.get(0,COLD_counter);
  EEPROM.get(4,HOT_counter);
  COLD_eeprom=COLD_counter;
  HOT_eeprom=HOT_counter;
  COLD_counter_last=COLD_counter;
  HOT_counter_last=HOT_counter;
  Serial.println("");
  Serial.println(COLD_counter, 3);
  Serial.println(HOT_counter, 3);
  
  // Initialize the output variables as outputs
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(COLD_PIN, INPUT);
  pinMode(HOT_PIN, INPUT);
  
  debouncer_COLD.attach(COLD_PIN);
  debouncer_HOT.attach(HOT_PIN);
  debouncer_COLD.interval(50);
  debouncer_HOT.interval(50);
  
  // Set outputs to LOW
  digitalWrite(LED_BUILTIN, HIGH);

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP()); 
 
 // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send_P(200, "text/html", index_html, processor);
});

  server.on("/COLD_counter", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", readCOLD().c_str());
  });
  server.on("/HOT_counter", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", readHOT().c_str());
  });
  
   // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      COLD_counter=inputMessage.toFloat();
      EEPROM.put(0,COLD_counter);
      EEPROM.commit();
      COLD_eeprom=COLD_counter;
      COLD_counter_last=COLD_counter;
    }
    // GET input2 value on <ESP_IP>/get?input2=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_2)->value();
      inputParam = PARAM_INPUT_2;
      HOT_counter=inputMessage.toFloat();
      EEPROM.put(4,HOT_counter);
      EEPROM.commit();
      HOT_eeprom=HOT_counter;
      HOT_counter_last=HOT_counter;
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage);
    request->send(200, "text/html", index_html);
  });
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){handleUpdate(request);});
  server.on("/doUpdate", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {handleDoUpdate(request, filename, index, data, len, final);}
  );
  server.onNotFound(notFound);
  server.begin();
  #ifdef ESP32
  Update.onProgress(printProgress);
#endif
}

void loop(){
debouncer_COLD.update();
int value_COLD = debouncer_COLD.read();
if ( value_COLD == HIGH ) {
    //digitalWrite(LED_BUILTIN, LOW);
    if (last_COLD_PIN_State == false)
    {
      COLD_counter=COLD_counter+Cena_pulse;
      Serial.println("Cold Water: " + String(COLD_counter, 3) + " l.");
      my_timer = millis();
        if (COLD_counter-COLD_eeprom>=0.1){
        EEPROM.put(0,COLD_counter);
        EEPROM.commit();
        COLD_eeprom=COLD_counter;        
        Serial.println("Cold Water eeprom: " + String(COLD_eeprom, 3) + " l.");
        }
    }
    last_COLD_PIN_State = true;
  }
  else {
    if (last_COLD_PIN_State == true){
    //digitalWrite(LED_BUILTIN, HIGH);
    }
    last_COLD_PIN_State = false;
  }
 
debouncer_HOT.update();
int value_HOT = debouncer_HOT.read();
if ( value_HOT == HIGH ) {
    //digitalWrite(LED_BUILTIN, LOW);
    if (last_HOT_PIN_State == false)
    {
      HOT_counter=HOT_counter+Cena_pulse;
      Serial.println("HOT Water: " + String(HOT_counter, 3) + " l.");
      my_timer = millis();
        if (HOT_counter-HOT_eeprom>=0.1){
        EEPROM.put(4,HOT_counter);
        EEPROM.commit();
        HOT_eeprom=HOT_counter;        
        Serial.println("HOT Water eeprom: " + String(HOT_eeprom, 3) + " l.");
        }
    }
    last_HOT_PIN_State = true;
  }
  else {
    if (last_HOT_PIN_State == true){
    //digitalWrite(LED_BUILTIN, HIGH);
    }
    last_HOT_PIN_State = false;
  }
  
        if (COLD_counter_last!=COLD_counter or HOT_counter_last!=HOT_counter){
            if (millis() - my_timer >= period_time) {
            my_timer = millis();   // "сбросить" таймер
            send_data();
            COLD_counter_last=COLD_counter;
            HOT_counter_last=HOT_counter;
            }
            
          }


   
}

#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <Syslog.h>
#include <WiFiUdp.h>

const char* ssid = "Briarhill";
const char* password = "12221222";
const int  buttonPin = D2;
const int  relayPin = D6;
const String clientName = "GarageDoorSensor";

#define REPORT_INTERVAL 2 // in sec
#define UPDATE_INTERVAL 30 // in seconds multiplied by REPORT_INTERVAL

// Syslog server connection info
#define SYSLOG_SERVER "192.168.0.161"
#define SYSLOG_PORT 514

// This device info
#define DEVICE_HOSTNAME "GarageDoorSensor"
#define APP_NAME "GarageDoor"

char* topic = "/sensors/doors/garage";
char* server = "192.168.0.161";
char* hellotopic = "sensors/hello";
int lastState = 2;
int updateCounter = UPDATE_INTERVAL;

WiFiClient wifiClient;
WiFiUDP udpClient;

Syslog syslog(udpClient, SYSLOG_PROTO_IETF);


void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  Serial.print("Recieved");
  syslog.log(LOG_INFO, "Got MQTT Message");
  if (strcmp(topic,"/controls/garagedoor")==0){

    digitalWrite(relayPin,LOW);
    delay(1000);
    digitalWrite(relayPin,HIGH);

  }
}

PubSubClient client(server, 1883, callback, wifiClient);

void connectWiFi(){
  WiFi.hostname("GarageDoorSensor");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  syslog.log(LOG_INFO, "Wifi Connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT(){
  if (client.connect((char*) clientName.c_str())) {
    client.subscribe("/controls/garagedoor");
    Serial.println("Connected to MQTT broker");
    syslog.log(LOG_INFO, "MQTT Connected");
    Serial.print("Topic is: ");
    Serial.println(topic);

    if (client.publish(hellotopic, "hello from GarageDoorSensor")) {
      Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
    }
  }
  else {
    Serial.println("MQTT connect failed");
    Serial.println("Will reset and try again...");
    abort();
  }
}

void configureOTA(){
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("GarageDoorSensor");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("\n OTA Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n OTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("OTA Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("OTA Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("OTA Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("OTA Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("OTA End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}
 void configureSyslog(){
   syslog.server(SYSLOG_SERVER, SYSLOG_PORT);
   syslog.deviceHostname(DEVICE_HOSTNAME);
   syslog.appName(APP_NAME);
   syslog.defaultPriority(LOG_KERN);
 }

void sendMQTTData(String payload) {
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker again");
      Serial.print("Topic is: ");
      Serial.println(topic);
    }
    else {
      Serial.println("MQTT connect failed");
      syslog.log(LOG_INFO, "MQTT connect failed");
      Serial.println("Will reset and try again...");
      abort();
    }
  }

  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.println(payload);

    if (client.publish(topic, (char*) payload.c_str())) {
      Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
      client.disconnect();
    }
  }

}

void readMQTTData() {
  if (!client.connected()) {
    syslog.log(LOG_INFO, "MQTT Disconnected");
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker again");
    }
    else {
      Serial.println("MQTT connect failed");
      Serial.println("Will reset and try again...");
      abort();
    }
  }
  Serial.println("Reading from MQTT..");
  client.loop();
}

void sendGarageStatusMQTT(String garageState){
  String payload;
  payload = "{\"GarageDoor\":";
  payload += "\"";
  payload += garageState;
  payload += "\"";
  payload += "}";
  topic = "/sensors/doors/garage";
  sendMQTTData(payload);
}

void setup() {
  pinMode(buttonPin, INPUT);
  pinMode (relayPin, OUTPUT);
  digitalWrite(relayPin,HIGH);

  Serial.begin(38400);
  delay(20);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  connectWiFi();

  Serial.print("Connecting to ");
  Serial.print(server);
  Serial.print(" as ");
  Serial.println(clientName);

  connectMQTT();
  configureOTA();
  configureSyslog();
}

void loop() {
  String garageState = "unchecked";
  int buttonState;
  String payload = "null";

  if (WiFi.status() != WL_CONNECTED) {
    syslog.log(LOG_INFO, "Wifi Disconnected");
    connectWiFi();
  }

  //Read garage door state
  buttonState = digitalRead(buttonPin);
  if (buttonState == HIGH) {
  // if the current state is HIGH then the garage door is open
    garageState = "open";
    Serial.print(garageState);
    syslog.log(LOG_INFO, "GarageState: " + garageState);
  } else {
  // if the current state is LOW then the garage door is closed
    garageState = "closed";
    Serial.print(garageState);
    syslog.log(LOG_INFO, "GarageState: " + garageState);
  }

  updateCounter--;
  if (updateCounter <= 0){
    syslog.log(LOG_INFO, "Sending status update");
    sendGarageStatusMQTT(garageState);
  }

  if (buttonState != lastState){
    syslog.log(LOG_INFO, "Door status changed, sending update");
    sendGarageStatusMQTT(garageState);
  }

  lastState = buttonState;

  readMQTTData();
  ArduinoOTA.handle();

  int cnt = REPORT_INTERVAL;
  while (cnt--)
    delay(1000);
  }

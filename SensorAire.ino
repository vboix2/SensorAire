
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include "SDS011.h"

#define SDS_TX D6
#define SDS_RX D7
#define DHTPIN D4
#define DHTTYPE DHT22

#define SENSEBOX_ID "SenseBox ID"
#define SENSOR1_ID "Temperature sensor id"  // Temperatura
#define SENSOR2_ID "Humidity sensor id"  // Humitat
#define SENSOR3_ID "PM10 sensor id"  // PM10
#define SENSOR4_ID "PM2.5 sensor id"  // PM2.5
#define APIKEY "Access token"

const char* ssid     = "SSID";
const char* password = "password";

const unsigned long minutes_time_interval = 10;  // Minuts entre medicions
const unsigned long n_measures = 60;  // Nombre de mesures per medició
const unsigned long min_successes = 30;  // Mínim de mesures exitoses

float temp, hum, p10, p25;
int error;
uint8_t successes;
unsigned long previous_millis = 0;

struct Air {
  float pm25;
  float pm10;
  float humidity;
  float temperature;
};


DHT dht(DHTPIN, DHTTYPE);
SDS011 sds;
WiFiClientSecure client;

void setup() {
  Serial.begin(9600);
  sds.begin(SDS_RX, SDS_TX);
  dht.begin();
  connectToWiFi();
  client.setInsecure();
}

void loop() {
  sds.wakeup();
  Air data = readData();
  if (successes > min_successes) {
    sendData(data.temperature, 1, SENSOR1_ID);
    sendData(data.humidity, 0, SENSOR2_ID);
    sendData(data.pm10, 2, SENSOR3_ID);
    sendData(data.pm25, 2, SENSOR4_ID);
  }
  sds.sleep();
  waitTime();
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connectant a la xarxa ");
  Serial.print(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connectada");
  Serial.print("Adreça IP: ");
  Serial.println(WiFi.localIP());
}


Air readData() {

  Serial.println("Mesurant...");
  float sum_pm25 = 0.0f;
  float sum_pm10 = 0.0f;
  float sum_hum = 0.0f;
  float sum_temp = 0.0f;
  successes = 0;

  for (uint8_t i = 0; i < n_measures; i++) {
    delay(1000);

    error = sds.read(&p25, &p10);
    hum = dht.readHumidity();
    temp = dht.readTemperature();

    if (error || isnan(temp) || isnan(hum)) {
      Serial.print("Error llegint les dades del sensor");

    } else {
      sum_pm25 += p25;
      sum_pm10 += p10;
      sum_hum += hum;
      sum_temp += temp;
      successes += 1;
      Serial.print("Temp: " + String(temp) + "ºC\tHum: " + String(hum) + "%\t");
      Serial.println("P2.5: " + String(p25) + "\tP10: " + String(p10));
    }
  }

  float av_pm25 = sum_pm25 / successes;
  float av_pm10 = sum_pm10 / successes;
  float av_hum = sum_hum / successes;
  float av_temp = sum_temp / successes;

  Air result = (Air) {
    normalizePM25(av_pm25, av_hum), normalizePM10(av_pm10, av_hum), av_hum, av_temp
  };
  
  Serial.println("\nMitjana de " + String(successes) + " mesures:");
  Serial.print("Temp: " + String(result.temperature) + "ºC\tHum: " + String(result.humidity) + "%");
  Serial.println("\tP2.5: " + String(result.pm25) + "\tP10: " + String(result.pm10));
  return result;
}

void sendData(float measure, int digits, String sensorId) {
  // Dades en format json
  char obs[10];
  dtostrf(measure, 5, digits, obs);
  String dataJson = "{\"value\":";
  dataJson += obs;
  dataJson += "}";

  if (client.connect("api.opensensemap.org", 443)) {
    Serial.println("Connectat a api.opensensemap.org");

    client.print("POST /boxes/");
    client.print(SENSEBOX_ID);
    client.print("/");
    client.print(sensorId);
    client.print(" HTTP/1.1\r\n");
    client.print("Host: api.opensensemap.org\r\n");
    client.print("Connection: close\r\n");
    client.print("Authorization: ");
    client.println(APIKEY);
    client.print("Cache-Control: no-cache\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("Content-Length: ");
    client.println(dataJson.length() + 2);
    client.println("");
    client.println(dataJson);

    Serial.print("Enviat: ");
    Serial.println(dataJson);
    delay(500);

    while (client.available()) {
      char c = client.read();
      Serial.print(c);
    }
  } else {
    Serial.println("No es pot connectar");
  }
  client.stop();
  delay(1000);
  Serial.println();
}


float normalizePM25(float pm25, float humidity) {
  return pm25 / (1.0 + 0.48756 * pow((humidity / 100.0), 8.60068));
}

float normalizePM10(float pm10, float humidity) {
  return pm10 / (1.0 + 0.81559 * pow((humidity / 100.0), 5.83411));
}

void waitTime() {
  unsigned long current_millis = millis();
  while (current_millis <
         (previous_millis + (minutes_time_interval * 60 * 1000))) {
    delay(10000);
    current_millis = millis();
    Serial.print(".");

  }
  previous_millis = current_millis;
  Serial.println();
}

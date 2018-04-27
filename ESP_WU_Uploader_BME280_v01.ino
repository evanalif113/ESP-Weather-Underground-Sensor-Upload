#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

#include <WiFiClientSecure.h>
#include <Wire.h>             // Built-in 
#include "system_variables.h"
#include <time.h>

#include <Adafruit_BME280.h>

Adafruit_BME280 bme; // I2C

const char* ssid      = "your-SSID";
const char* password  = "your-PASSWORD";
String WU_pwsID       = "your-PWS-ID";       // The PWS ID as registered and assigned to you by wunderground.com
String WU_pwsPASSWORD = "your WU PASSOWRD";  // **** The PASSWORD registered for your wunderground.com account, not the password issued for the new PWS ID !

const char* host      = "rtupdate.wunderground.com"; //"weatherstation.wunderground.com";
const int httpsPort   = 443;

#ifdef ESP32
  const unsigned long  UpdateInterval     = 30L*60L*1000000L; // Delay between updates, in milliseconds, WU allows 500 requests per-day maximum, this sets the time to 30-mins
#else
  const unsigned long  UpdateInterval     = 30L*60L*1000L;    // Delay between updates, in milliseconds, WU allows 500 requests per-day maximum, this sets the time to 30-mins
#endif

String UploadData, timenow; 
float  sensor_temperature, sensor_humidity, sensor_pressure, sensor_spare; 

void setup() {
  Serial.begin(115200);
  boolean Status = bme.begin();  
  if (!Status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  StartWiFi();
  StartAndGetTime();
}

void loop() {
  // NOTE-1: You must define the parameters for upload to WU, goto 'system_variables' tab and uncomment the required parameter e.g. '//#define SW_winddir' becomes '#define SW_winddir'
  // NOTE-2: If you read a sensor as a floating point, you must assign the result to a corresponding String variable as listed in the WU_variable_names in the 'system_variable.h list'
  // NOTE: e.g. float Winddir = 270; then WU_winddir = String(Winddir);
  ReadSensorInformation();
  UpdateTime();
  timenow = "now"; // or comment this line to upload the local time from the ESP time update
  if (!UploadDataToWU()); Serial.println("Error uploading to Weather Underground, trying next time");
  delay(UpdateInterval);
}

boolean UploadDataToWU(){
  WiFiClientSecure client;
  // Use WiFiClientSecure class to create SSL connection
  Serial.println("Connecting to   : "+String(host));
  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection failed");
    return false;
  }
  String url = "/weatherstation/updateweatherstation.php?ID="+WU_pwsID+"&PASSWORD="+WU_pwsPASSWORD+"&dateutc="+timenow +
               #ifdef SW_tempf
                 "&tempf="+WU_tempf +
               #endif
               #ifdef SW_humidity
                 "&humidity="+WU_humidity +
               #endif
               #ifdef SW_dewptf
                 "&dewptf="+WU_dewptf +             // ***** You must report Dew Point for a valid display to be shown on WU
               #endif
               #ifdef SW_baromin
                 "&baromin="+WU_baromin + 
               #endif
               // ancillary parameters
               #ifdef SW_winddir
                 "&winddir="+WU_winddir + 
               #endif
               #ifdef SW_windspeedmph
                 "&windspeedmph="+WU_windspeedmph + 
               #endif
               #ifdef SW_windgustmph
                 "&windgustmph="+WU_windgustmph +
               #endif
               #ifdef SW_rainin
                 "&rainin="+WU_rainin +
               #endif
               #ifdef SW_dailyrainin
                 "&dailyrainin="+WU_dailyrainin + 
               #endif
               #ifdef SW_solarradiation
                 "&solarradiation="+WU_solarradiation +
               #endif
               #ifdef SW_UV
                 "&UV="+WU_UV +
               #endif
               #ifdef SW_indoortempf
                 "&indoortempf="+WU_indoortempf + 
               #endif
               #ifdef SW_indoorhumidity
                 "&indoorhumidity="+WU_indoorhumidity + 
               #endif
               #ifdef SW_soiltempf
                 "&soiltempf="+WU_soiltempf + 
               #endif
               #ifdef SW_soilmoisture
                 "&soilmoisture="+WU_soilmoisture + 
               #endif
               #ifdef SW_leafwetness
                 "&leafwetness="+WU_leafwetness + 
               #endif
               #ifdef SW_visibility
                 "&visibility="+WU_visibility + 
               #endif
               #ifdef SW_weather
                 "&weather="+WU_weather +
               #endif
               #ifdef SW_clouds
                 "&clouds="+WU_clouds +
               #endif
               "&action=updateraw&realtime=1&rtfreq=60";
  Serial.println("Requesting      : "+url);
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: G6EJDFailureDetectionFunction\r\n" +
               "Connection: close\r\n\r\n");
  Serial.print("Request sent    : ");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("Headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  //Serial.println(line);
  boolean Status = true;
  if (line == "success") line = "Server confirmed all data received";
  if (line == "INVALIDPASSWORDID|Password or key and/or id are incorrect") {
    line = "Invalid PWS/User data entered in the ID and PASSWORD or GET parameters";
    Status = false;
  }
  if (line == "RapidFire Server") {
    line = "The minimum GET parameters of ID, PASSWORD, action and dateutc were not set correctly";
    Status = false;
  }
  Serial.println("Server Response : "+line);
  Serial.println("Status          : Closing connection");
  return Status;
}

void ReadSensorInformation(){
  float sensor_humidity    = bme.readHumidity(); 
  float sensor_temperature = bme.readTemperature() * 9/5+32;         // Read temperature as Fahrenheit
  float sensor_pressure    = bme.readPressure() / 100.0F * 0.02953 ; // Read pressure in in's"
  if (isnan(sensor_humidity) || isnan(sensor_temperature) || isnan(sensor_pressure)) { 
    Serial.println("Failed to read from BME280 sensor!"); 
    return; 
  } 
  WU_tempf     = String(sensor_temperature);
  WU_dewptf    = String((sensor_temperature-(100-sensor_humidity)/5.0)*9/5+32);
  WU_humidity  = String(sensor_humidity,2);
  WU_baromin   = String(sensor_pressure,2);
}

void StartWiFi(){
  Serial.print("\nConnecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void StartAndGetTime(){
  configTime(0, 0, "0.uk.pool.ntp.org", "time.nist.gov"); 
  setenv("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02",1); 
  delay(200); 
  time_t rawtime;
  struct tm *info;
  char buffer[80];
  time( &rawtime );
  info = localtime( &rawtime );
  // Upload format for time = dateutc [CCYY-MM-DD HH:MM:SS (mysql format)]
  // 2018-04-30 10:32:35 becomes 2018-04-30+10%3A32%3A35 in url escaped format
  // See: http://www.cplusplus.com/reference/ctime/strftime/
  Serial.println("Upload date-time: CCYY-MM-DD HH:MM:SS   expected");
  strftime(buffer,80,"%Y-%m-%d+%H:%M:%S", info);
  //printf("Upload date-time: %s\n", buffer );
  timenow = buffer; // timenow is correctly formated for WU
  timenow.replace(":", "%3A");
}

void UpdateTime(){
  time_t rawtime;
  struct tm *info;
  char buffer[80];
  time( &rawtime );
  info = localtime( &rawtime );
  strftime(buffer,80,"%Y-%m-%d+%H:%M:%S", info);
  printf("Upload date-time: %s\n", buffer );
  timenow = buffer; 
  timenow.replace(":", "%3A"); // timenow is correctly formated for WU
}

//####################################################################
/*
  A new Personal Weather Station (PWS) needs to setup and wait for it to be activiated, this can take a few minutes, maybe up to 20mins.
 Upload values allowed:
 &dateutc=2000-01-01+10%3A32%3A35 or dateutc=now
 &winddir=230
 &windspeedmph=12
 &windgustmph=12
 &tempf=70
    * for extra outdoor sensors use temp2f, temp3f, and so on
 &soiltempf - [F soil temperature]
    * for sensors 2,3,4 use soiltemp2f, soiltemp3f, and soiltemp4f
 &soilmoisture - [%]
    * for sensors 2,3,4 use soilmoisture2, soilmoisture3, and soilmoisture4
 &leafwetness  - [%]
    * for sensor 2 use leafwetness2
 &solarradiation - [W/m^2]
 &UV - [index]
 &visibility - [nm visibility]
 &indoortempf - [F indoor temperature F]
 &indoorhumidity - [% indoor humidity 0-100]
 &rainin=0
 &baromin=29.1
 &dewptf=68.2
 &humidity=90
 &weather= - [text] -- metar style (+RA) means light rain showers
 &clouds= -- metar style SKC, FEW, SCT, BKN, OVC meaning  Sky Clear, Few, Scattered, Broken, Overcast
 &softwaretype=vws%20versionxx
 &action=updateraw

OR using their Rapid Fire Server

http://wiki.wunderground.com/index.php/PWS_-_Upload_Protocol

usage:
 action [action = updateraw]
 ID [PWS ID as registered by wunderground.com]
 PASSWORD [PASSWORD registered with this ID]
 dateutc - [YYYY-MM-DD HH:MM:SS (mysql format)] or dateutc=now
 winddir - [0-360]
 windspeedmph - [mph]
 windgustmph - [windgustmph ]
 humidity - [%]
 tempf - [temperature F]
 rainin - [rain in]
 dailyrainin - [daily rain in accumulated]
 baromin - [barom in]
 dewptf- [dewpoint F]
 weather - [text] -- metar style (+RA) 
 clouds - [text] -- SKC, FEW, SCT, BKN, OVC
 softwaretype - [text] ie: vws or weatherdisplay
*/

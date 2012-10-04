/*

  Open Kontrol Gateway (OKG) Example  
  Receive data from an emonTx via RFM12B wireless then post online to emoncms using Wiznet 5200 Ethernet Module
  
  Part of the openenergymonitor.org project
  Licence: GNU GPL V3
  Authors: Trystan Lea and Glyn Hudson 
  Created: 13/08/12

  Use the following libraries:  
  https://github.com/openenergymonitor/Ethernet
  https://github.com/openenergymonitor/jeelib
  
  For full step-by-step instructions See: http://wiki.openenergymonitor.org/index.php?title=Open_Kontrol_Gateway 
 
  Credits:
  Based on Arduino DNS and DHCP-based Web client by David A. Mellis
  modified 12 April 2011by Tom Igoe, based on work by Adrian McEwen
  Uses Jeelabs.org JeeLib library for RFM12B 

  Example will also work with Arduino Ethernet, Arduino + newer Ethernet shields with addition of RFM12B. Bug in older Ethernet shields stops RFM12B and Wiznet being using together
 
*/

#include <SPI.h>
#include <Ethernet.h>
#include <JeeLib.h>

//------------------------------------------------------------------------------------------------------
// RFM12B Wireless Config
//------------------------------------------------------------------------------------------------------
#define MYNODE 30            // node ID of OKG 
#define freq RF12_868MHZ     // frequency - must match RFM12B module and emonTx
#define group 210            // network group - must match emonTx 
//------------------------------------------------------------------------------------------------------
 
//------------------------------------------------------------------------------------------------------
// Ethernet Config
//------------------------------------------------------------------------------------------------------
byte mac[] = { 0x00, 0xAB, 0xBB, 0xCC, 0xDE, 0x02 };  // OKG MAC - experiment with different MAC addresses if you have trouble connecting 
byte ip[] = {192, 168, 1, 99 };                       // OKG static IP - only used if DHCP failes

// Enter your apiurl here including apikey:
char apiurl[] = "http://emoncms.org/api/post.json?apikey=YOURAPIKEY";
char timeurl[] = "http://emoncms.org/time/local.json?apikey=YOURAPIKEY";
// For posting to emoncms server with host name, (DNS lookup) comment out if using static IP address below
// emoncms.org is the public emoncms server. Emoncms can also be downloaded and run on any server.
char server[] = "emoncms.org";    

//IPAddress server(xxx,xxx,xxx,xxx);                  // emoncms server IP for posting to server without a host name, can be used for posting to local emoncms server

EthernetClient client;
//------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------
// Open Kontrol Gateway Config
//------------------------------------------------------------------------------------------------------
const int LEDpin=17;         // front status LED on OKG
//#define SERIALCOMMS          // comment out if you don't want debug output on the serial port
//------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------
// The PacketBuffer class is used to generate the json string that is send via ethernet - JeeLabs
//------------------------------------------------------------------------------------------------------
class PacketBuffer : public Print
{
  public:
    PacketBuffer()
      : fill(0)
    {
    }

    const char* buffer() const
    {
      return buf;
    }

    const byte length() const
    {
      return fill;
    }

    void reset()
    { 
      memset(buf,NULL,sizeof(buf));
      fill = 0; 
    }

    virtual size_t write (uint8_t ch)
    {
      if (fill < sizeof buf)
      {
        buf[fill++] = ch;
      }
    }

  private:
    byte fill;
    char buf[150];
};

PacketBuffer str;

//--------------------------------------------------------------------------------------------------------

int data_ready, rf_error;
unsigned long last_rf, time60s;
unsigned long last_mem_report;
boolean lastConnected = false;

char line_buf[50];                        // Used to store line of http reply header

void flash()
{
  pinMode(LEDpin, OUTPUT);
  digitalWrite(LEDpin, HIGH);
  delay(200);
  pinMode(LEDpin, OUTPUT);
  digitalWrite(LEDpin, LOW);
}

//------------------------------------------------------------------------------------------------------
// SETUP
//------------------------------------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  while (!Serial) ;
  #ifdef SERIALCOMMS
  Serial.println("openenergymonitor.org RFM12B > OKG > Wiznet, > emoncms MULTI-NODE");
  #endif

  // Flash twice to indicate setup start.
  flash();
  flash();
  
  rf12_set_cs(9);
  rf12_initialize(MYNODE, freq,group);
  last_rf = millis()-40000;                                       // setting lastRF back 40s is useful as it forces the ethernet code to run straight away
  last_mem_report = last_rf;
  
  if (Ethernet.begin(mac) == 0)
  {
    #ifdef SERIALCOMMS
    Serial.println("Failed to configure Ethernet using DHCP");
    #endif
    Ethernet.begin(mac, ip);                                      //configure manually 
  }
  
  // print your local IP address:
  #ifdef SERIALCOMMS
  Serial.print("Local IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++)
  {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print("."); 
  }
  Serial.println();
  #endif

  // print RFM12B settings
  #ifdef SERIALCOMMS
  Serial.print("Node: "); 
  Serial.print(MYNODE); 
  Serial.print(" Freq: "); 
  if (freq == RF12_433MHZ) Serial.print("433Mhz");
  if (freq == RF12_868MHZ) Serial.print("868Mhz");
  if (freq == RF12_915MHZ) Serial.print("915Mhz");
  Serial.print(" Network: "); 
  Serial.println(group);
  #endif

  // Flash three times then switch off (should be anyway!) to indicate setup success
  flash();
  flash();
  flash();
  pinMode(LEDpin, OUTPUT);
  digitalWrite(LEDpin, LOW);
}
//------------------------------------------------------------------------------------------------------

void reportAvailableMemory()
{
  #ifdef SERIALCOMMS // no point doing this unless we can report it ;-)
  int size = 1024;
  byte *buf;

  while ((buf = (byte *) malloc(--size)) == NULL)
  {
    // Loop until we successfully allocate some memory
  }
  free(buf); // then free it!

  Serial.print(size); Serial.println(" bytes free.");
  #endif
}

//------------------------------------------------------------------------------------------------------
// LOOP
//------------------------------------------------------------------------------------------------------
void loop()
{
  if (millis() - last_mem_report > 1000)
  {
    reportAvailableMemory();
    last_mem_report = millis();
  }
  int availableData = client.available();
  if (availableData)
  {
    #ifdef SERIALCOMMS
    Serial.print("Data available: "); Serial.print(availableData); Serial.println(" bytes");
    #endif
    memset(line_buf,NULL,sizeof(line_buf));

    int pos = 0;
    while (client.available())
    {
      char c = client.read();
      line_buf[pos] = c;
      pos++;
      #ifdef SERIALCOMMS
      if (pos % 100 == 0)
      {
        Serial.print("Data received: " ); Serial.print(pos); Serial.print(" bytes: "); Serial.println(line_buf);
      }
      #endif
    }
    #ifdef SERIALCOMMS
    Serial.print("All data received: " ); Serial.print(pos); Serial.print(" bytes: "); Serial.println(line_buf);
    #endif

    if (strcmp(line_buf,"ok")==0)
    {
      #ifdef SERIALCOMMS
      Serial.println("OK received");
      #endif
    }
    else if(line_buf[0]=='t')
    {
      #ifdef SERIALCOMMS
      Serial.print("Time: "); Serial.println(line_buf);
      #endif

      char tmp[] = {line_buf[1],line_buf[2]};
      byte hour = atoi(tmp);
      tmp[0] = line_buf[4]; tmp[1] = line_buf[5];
      byte minute = atoi(tmp);
      tmp[0] = line_buf[7]; tmp[1] = line_buf[8];
      byte second = atoi(tmp);

      if (hour>0 || minute>0 || second>0) 
      {  
        char data[] = {'t',hour,minute,second};
        int i = 0; while (!rf12_canSend() && i<10) {rf12_recvDone(); i++;}
        rf12_sendStart(0, data, sizeof data);
        rf12_sendWait(0);
      }
    }
  }
  
  if (!client.connected() && lastConnected)
  {
    #ifdef SERIALCOMMS
    Serial.println("Disconnecting from CMS.");
    #endif
    client.stop();
  }
  
  //-----------------------------------------------------------------------------------------------------------------
  // 1) Receive date from emonTx via RFM12B
  //-----------------------------------------------------------------------------------------------------------------
  if (rf12_recvDone())
  {
    if (rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)
    {
      int node_id = (rf12_hdr & 0x1F);
      byte n = rf12_len;

      str.reset();
      str.print("&node=");  str.print(node_id);
      str.print("&csv=");
      for (byte i=0; i<n; i+=2)
      {
        int num = ((unsigned char)rf12_data[i+1] << 8 | (unsigned char)rf12_data[i]);
        if (i) str.print(",");
        str.print(num);
      }

      str.print("\0");  //  End of json string
      #ifdef SERIALCOMMS
      Serial.print("RF received: "); Serial.println(str.buffer());
      #endif
      data_ready = 1;
      last_rf = millis();
      rf_error=0;
    }
  }

  //-----------------------------------------------------------------------------------------------------------------
  // 2) If no data is recieved from rf12 emoncms is updated every 30s with RFfail = 1 indicator for debugging
  //-----------------------------------------------------------------------------------------------------------------
  if ((millis()-last_rf)>30000)
  {
    #ifdef SERIALCOMMS
    Serial.print("No RF for " ); Serial.print(millis() - last_rf); Serial.println("ms");
    #endif
    last_rf = millis();                                                 // reset lastRF timer
    str.reset();                                                        // reset json string
    str.print("&json={rf_fail:1}\0");                                   // No RF received in 30 seconds so send failure
    data_ready = 1;                                                     // Ok, data is ready
    rf_error=1;
  }

  //-----------------------------------------------------------------------------------------------------------------
  // 3) Post Data
  //-----------------------------------------------------------------------------------------------------------------
  if (!client.connected() && data_ready)
  {
    int connectStatus = client.connect(server, 80);
    if (connectStatus)
    {
      client.print("GET "); client.print(apiurl); client.print(str.buffer()); client.println();
      #ifdef SERIALCOMMS
      Serial.print("Sent data: "); Serial.print(apiurl); Serial.println(str.buffer());
      #endif
    
      delay(300);
      data_ready=0;
      digitalWrite(LEDpin,LOW);		  // turn off status LED to indicate succesful data receive and online posting
    } 
    else
    {
      #ifdef SERIALCOMMS
      Serial.print("Can't connect to send data, error "); Serial.println(connectStatus); delay(500); client.stop();
      #endif
    }
  }

  if (!client.connected() && ((millis()-time60s)>10000))
  {
    time60s = millis();                                                 // reset lastRF timer

    int connectStatus = client.connect(server, 80);
    if (connectStatus)
    {
      #ifdef SERIALCOMMS
      Serial.println("Requested time");
      #endif
      client.print("GET "); client.print(timeurl); client.println();
    }
    else
    {
      #ifdef SERIALCOMMS
      Serial.print("Can't connect to request time, error "); Serial.println(connectStatus); delay(500); client.stop();
      #endif
    }
  }

  flash(); // flash once to indicate we've been through the loop
  lastConnected = client.connected();
}//end loop

//------------------------------------------------------------------------------------------------------


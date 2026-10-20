#include <Windy.h>


Windy::Windy(){
    client = NULL;
    xMutex = NULL;
    MyClient = NULL;
}

Windy::~Windy(){
  if (MyClient != NULL){
    delete MyClient;
  }
}

void Windy::setClient(Client *_client){
    client = _client;
}

void Windy::setMutex(SemaphoreHandle_t *_xMutex){
    xMutex = _xMutex;
}

bool Windy::sendData(String ID, String APIKEY, wData *data)
{
    bool bRet = true;

    if (data == NULL)
        return false;

    if (xMutex == NULL) {
        xMutex = new SemaphoreHandle_t();
        *xMutex = xSemaphoreCreateMutex();

        if (*xMutex == NULL)
            return false;
    }

    char msg[1024];
    char msg2[255];

    float dewpoint = dewPointFast(data->temp, data->humidity);

    // Zeitstempel: Hier müssen UTC-Werte verwendet werden.
    // year(), month(), day(), hour() usw. können lokale Zeit liefern.
    snprintf(msg, sizeof(msg),"/api/v2/observation/update?id=%s&PASSWORD=%s&time=%04d-%02d-%02dT%02d%%3A%02d%%3A%02dZ",
        ID.c_str(),
        APIKEY.c_str(),
        year(),
		month(),
		day(),
        hour(),
		minute(),
	    second()
		);

    if (data->bWind) {
        snprintf(msg2, sizeof(msg2),"&winddir=%.2f&wind=%.2f&gust=%.2f",
            data->winddir,
            data->windspeed,
            data->windgust);
        strncat(msg, msg2, sizeof(msg) - strlen(msg) - 1);
    }

    if (data->bHum) {
        snprintf(msg2, sizeof(msg2), "&rh=%.2f",
            data->humidity);
        strncat(msg, msg2, sizeof(msg) - strlen(msg) - 1);
    }

    if (data->bTemp) {
        snprintf(msg2, sizeof(msg2), "&temp=%.2f",
            data->temp);
        strncat(msg, msg2, sizeof(msg) - strlen(msg) - 1);
    }

    if (data->bTemp && data->bHum) {
        snprintf(msg2, sizeof(msg2), "&dewpoint=%.2f",
            dewpoint);
        strncat(msg, msg2, sizeof(msg) - strlen(msg) - 1);
    }

    if (data->bTemp) {
      if (data->pressure>500 ){
        snprintf(msg2, sizeof(msg2), "&mbar=%.2f",
            data->pressure);
        strncat(msg, msg2, sizeof(msg) - strlen(msg) - 1);
      }
    }

    // rain1h wird hier bewusst nicht übertragen:
    // Die korrekte Zuordnung zum V2-Niederschlagsfeld
    // muss anhand der Einheit und des Messzeitraums erfolgen.

  log_i("Windy Upload %s !",msg);

  uint16_t serverPort = 80;

    if (client == NULL) {
#ifdef SSLCONNECTION
        MyClient = new WiFiClientSecure();
        MyClient->setInsecure();
    serverPort = 443;
#else
        MyClient = new WiFiClient();
#endif
        if (MyClient == NULL)
            return false;

        client = MyClient;
    }

    xSemaphoreTake(*xMutex, portMAX_DELAY);

    HttpClient http(*client, "stations.windy.com", serverPort);

    int httpResponseCode = http.get(msg);

    if (httpResponseCode == 0) {
        httpResponseCode = http.responseStatusCode();

        if (httpResponseCode != 200) {
            log_e("Windy V2 response=%d", httpResponseCode);
            bRet = false;
        }
    } else {
        log_e("Windy V2 connection failed=%d", httpResponseCode);
        bRet = false;
    }

    http.stop();
    xSemaphoreGive(*xMutex);

    return bRet;
}

/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozStumbler.h"
#include "nsDataHashtable.h"
#include "nsGeoPosition.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"
#include "StumblerLogging.h"
#include "WriteStumbleOnThread.h"
#include "../GeolocationUtil.h"

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIMobileConnectionInfo.h"
#include "nsIMobileConnectionService.h"
#include "nsIMobileCellInfo.h"
#include "nsIMobileNetworkInfo.h"
#include "nsINetworkInterface.h"
#include "nsIRadioInterfaceLayer.h"

using namespace mozilla;
using namespace mozilla::dom;


NS_IMPL_ISUPPORTS(StumblerInfo, nsICellInfoListCallback, nsIWifiScanResultsReady)

class RequestCellInfoEvent : public nsRunnable {
public:
  RequestCellInfoEvent(StumblerInfo *callback)
  : mRequestCallback(callback)
  {}

  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());
    // Get Cell Info
    nsCOMPtr<nsIMobileConnectionService> service =
    do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);

    if (!service) {
      STUMBLER_ERR("Stumbler-can not get nsIMobileConnectionService \n");
      return NS_OK;
    }
    nsCOMPtr<nsIMobileConnection> connection;
    uint32_t numberOfRilServices = 1, cellInfoNum = 0;

    service->GetNumItems(&numberOfRilServices);
    for (uint32_t rilNum = 0; rilNum < numberOfRilServices; rilNum++) {
      service->GetItemByServiceId(rilNum /* Client Id */, getter_AddRefs(connection));
      if (!connection) {
        STUMBLER_ERR("Stumbler-can not get nsIMobileConnection by ServiceId %d \n", rilNum);
      } else {
        cellInfoNum++;
        connection->GetCellInfoList(mRequestCallback);
      }
    }
    mRequestCallback->SetCellInfoResponsesExpected(cellInfoNum);

    // Get Wifi AP Info
    nsCOMPtr<nsIInterfaceRequestor> ir = do_GetService("@mozilla.org/telephony/system-worker-manager;1");
    nsCOMPtr<nsIWifi> wifi = do_GetInterface(ir);
    if (!wifi) {
      mRequestCallback->SetWifiInfoResponseReceived();
      STUMBLER_ERR("Stumbler-can not get nsIWifi interface\n");
      return NS_OK;
    }
    wifi->GetWifiScanResults(mRequestCallback);
    return NS_OK;
  }
private:
  nsRefPtr<StumblerInfo> mRequestCallback;
};

void
MozStumble(nsGeoPosition* position)
{
  if (WriteStumbleOnThread::IsFileWaitingForUpload()) {
    nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    MOZ_ASSERT(target);
    // Knowing that file is waiting to upload, and no collection will take place,
    // just trigger the thread with an empty string.
    nsCOMPtr<nsIRunnable> event = new WriteStumbleOnThread(EmptyCString());
    target->Dispatch(event, NS_DISPATCH_NORMAL);
    return;
  }

  nsCOMPtr<nsIDOMGeoPositionCoords> coords;
  position->GetCoords(getter_AddRefs(coords));
  if (!coords) {
    return;
  }

  double latitude, longitude;
  coords->GetLatitude(&latitude);
  coords->GetLongitude(&longitude);

  const double kMinChangeInMeters = 30;
  static int64_t lastTime_ms = 0;
  static double sLastLat = 0;
  static double sLastLon = 0;
  double delta = -1.0;
  int64_t timediff = (PR_Now() / PR_USEC_PER_MSEC) - lastTime_ms;

  if (0 != sLastLon || 0 != sLastLat) {
    delta = CalculateDeltaInMeter(latitude, longitude, sLastLat, sLastLon);
  }
  STUMBLER_DBG("Stumbler-Location. [%f , %f] time_diff:%lld, delta : %f\n",
                                      longitude, latitude, timediff, delta);

  // Consecutive GPS locations must be 30 meters and 3 seconds apart
  if (lastTime_ms == 0 || ((timediff >= STUMBLE_INTERVAL_MS) && (delta > kMinChangeInMeters))){
    lastTime_ms = (PR_Now() / PR_USEC_PER_MSEC);
    sLastLat = latitude;
    sLastLon = longitude;
    nsRefPtr<StumblerInfo> requestCallback = new StumblerInfo(position);
    nsRefPtr<RequestCellInfoEvent> runnable = new RequestCellInfoEvent(requestCallback);
    NS_DispatchToMainThread(runnable);
  } else {
    STUMBLER_DBG("Stumbler-GPS locations less than 30 meters and 3 seconds. Ignore!\n");
  }
}

void
StumblerInfo::SetWifiInfoResponseReceived()
{
  mIsWifiInfoResponseReceived = true;

  if (mIsWifiInfoResponseReceived && mCellInfoResponsesReceived == mCellInfoResponsesExpected) {
    STUMBLER_DBG("Call DumpStumblerInfo from SetWifiInfoResponseReceived\n");
    DumpStumblerInfo();
  }
}

void
StumblerInfo::SetCellInfoResponsesExpected(uint8_t count)
{
  mCellInfoResponsesExpected = count;
  STUMBLER_DBG("SetCellInfoNum (%d)\n", count);

  if (mIsWifiInfoResponseReceived && mCellInfoResponsesReceived == mCellInfoResponsesExpected) {
    STUMBLER_DBG("Call DumpStumblerInfo from SetCellInfoResponsesExpected\n");
    DumpStumblerInfo();
  }
}


#define TEXT_LAT NS_LITERAL_CSTRING("latitude")
#define TEXT_LON NS_LITERAL_CSTRING("longitude")
#define TEXT_ACC NS_LITERAL_CSTRING("accuracy")
#define TEXT_ALT NS_LITERAL_CSTRING("altitude")
#define TEXT_ALTACC NS_LITERAL_CSTRING("altitudeAccuracy")
#define TEXT_HEAD NS_LITERAL_CSTRING("heading")
#define TEXT_SPD NS_LITERAL_CSTRING("speed")

nsresult
StumblerInfo::LocationInfoToString(nsACString& aLocDesc)
{
  nsCOMPtr<nsIDOMGeoPositionCoords> coords;
  mPosition->GetCoords(getter_AddRefs(coords));
  if (!coords) {
    return NS_ERROR_FAILURE;
  }

  nsDataHashtable<nsCStringHashKey, double> info;

  double val;
  coords->GetLatitude(&val);
  info.Put(TEXT_LAT, val);
  coords->GetLongitude(&val);
  info.Put(TEXT_LON, val);
  coords->GetAccuracy(&val);
  info.Put(TEXT_ACC, val);
  coords->GetAltitude(&val);
  info.Put(TEXT_ALT, val);
  coords->GetAltitudeAccuracy(&val);
  info.Put(TEXT_ALTACC, val);
  coords->GetHeading(&val);
  info.Put(TEXT_HEAD, val);
  coords->GetSpeed(&val);
  info.Put(TEXT_SPD, val);

  for (auto it = info.Iter(); !it.Done(); it.Next()) {
    const nsACString& key = it.Key();
    val = it.UserData();
    if (!IsNaN(val)) {
      aLocDesc += nsPrintfCString("\"%s\":%f,", key.BeginReading(), val);
    }
  }

  aLocDesc += nsPrintfCString("\"timestamp\":%lld,", PR_Now() / PR_USEC_PER_MSEC).get();
  return NS_OK;
}

#define TEXT_RADIOTYPE NS_LITERAL_CSTRING("radioType")
#define TEXT_MCC NS_LITERAL_CSTRING("mobileCountryCode")
#define TEXT_MNC NS_LITERAL_CSTRING("mobileNetworkCode")
#define TEXT_LAC NS_LITERAL_CSTRING("locationAreaCode")
#define TEXT_CID NS_LITERAL_CSTRING("cellId")
#define TEXT_PSC NS_LITERAL_CSTRING("psc")
#define TEXT_STRENGTH_ASU NS_LITERAL_CSTRING("asu")
#define TEXT_STRENGTH_DBM NS_LITERAL_CSTRING("signalStrength")
#define TEXT_REGISTERED NS_LITERAL_CSTRING("serving")
#define TEXT_TIMEING_ADVANCE NS_LITERAL_CSTRING("timingAdvance")

template <class T> void
ExtractCommonNonCDMACellInfoItems(nsCOMPtr<T>& cell, nsDataHashtable<nsCStringHashKey, int32_t>& info)
{
  int32_t mcc, mnc, cid, sig;

  cell->GetMcc(&mcc);
  cell->GetMnc(&mnc);
  cell->GetCid(&cid);
  cell->GetSignalStrength(&sig);

  info.Put(TEXT_MCC, mcc);
  info.Put(TEXT_MNC, mnc);
  info.Put(TEXT_CID, cid);
  info.Put(TEXT_STRENGTH_ASU, sig);
}

void
StumblerInfo::CellNetworkInfoToString(nsACString& aCellDesc)
{
  aCellDesc += "\"cellTowers\": [";

  for (uint32_t idx = 0; idx < mCellInfo.Length() ; idx++) {
    const char* radioType = 0;
    int32_t type;
    mCellInfo[idx]->GetType(&type);
    bool registered;
    mCellInfo[idx]->GetRegistered(&registered);
    if (idx) {
      aCellDesc += ",{";
    } else {
      aCellDesc += "{";
    }

    STUMBLER_DBG("type=%d\n", type);

    nsDataHashtable<nsCStringHashKey, int32_t> info;
    info.Put(TEXT_REGISTERED, registered);

    if(type == nsICellInfo::CELL_INFO_TYPE_GSM) {
      radioType = "gsm";
      nsCOMPtr<nsIGsmCellInfo> gsmCellInfo = do_QueryInterface(mCellInfo[idx]);
      ExtractCommonNonCDMACellInfoItems(gsmCellInfo, info);
      int32_t lac;
      gsmCellInfo->GetLac(&lac);
      info.Put(TEXT_LAC, lac);
    } else if (type == nsICellInfo::CELL_INFO_TYPE_WCDMA) {
      radioType = "wcdma";
      nsCOMPtr<nsIWcdmaCellInfo> wcdmaCellInfo = do_QueryInterface(mCellInfo[idx]);
      ExtractCommonNonCDMACellInfoItems(wcdmaCellInfo, info);
      int32_t lac, psc;
      wcdmaCellInfo->GetLac(&lac);
      wcdmaCellInfo->GetPsc(&psc);
      info.Put(TEXT_LAC, lac);
      info.Put(TEXT_PSC, psc);
    } else if (type == nsICellInfo::CELL_INFO_TYPE_CDMA) {
      radioType = "cdma";
      nsCOMPtr<nsICdmaCellInfo> cdmaCellInfo = do_QueryInterface(mCellInfo[idx]);
      int32_t mnc, lac, cid, sig;
      cdmaCellInfo->GetSystemId(&mnc);
      cdmaCellInfo->GetNetworkId(&lac);
      cdmaCellInfo->GetBaseStationId(&cid);
      info.Put(TEXT_MNC, mnc);
      info.Put(TEXT_LAC, lac);
      info.Put(TEXT_CID, cid);

      cdmaCellInfo->GetEvdoDbm(&sig);
      if (sig < 0 || sig == nsICellInfo::UNKNOWN_VALUE) {
        cdmaCellInfo->GetCdmaDbm(&sig);
      }
      if (sig > -1 && sig != nsICellInfo::UNKNOWN_VALUE)  {
        sig *= -1;
        info.Put(TEXT_STRENGTH_DBM, sig);
      }
    } else if (type == nsICellInfo::CELL_INFO_TYPE_LTE) {
      radioType = "lte";
      nsCOMPtr<nsILteCellInfo> lteCellInfo = do_QueryInterface(mCellInfo[idx]);
      ExtractCommonNonCDMACellInfoItems(lteCellInfo, info);
      int32_t lac, timingAdvance, pcid, rsrp;
      lteCellInfo->GetTac(&lac);
      lteCellInfo->GetTimingAdvance(&timingAdvance);
      lteCellInfo->GetPcid(&pcid);
      lteCellInfo->GetRsrp(&rsrp);
      info.Put(TEXT_LAC, lac);
      info.Put(TEXT_TIMEING_ADVANCE, timingAdvance);
      info.Put(TEXT_PSC, pcid);
      if (rsrp != nsICellInfo::UNKNOWN_VALUE) {
        info.Put(TEXT_STRENGTH_DBM, rsrp * -1);
      }
    }

    aCellDesc += nsPrintfCString("\"%s\":\"%s\"", TEXT_RADIOTYPE.get(), radioType);
    for (auto it = info.Iter(); !it.Done(); it.Next()) {
      const nsACString& key = it.Key();
      int32_t value = it.UserData();
      if (value != nsICellInfo::UNKNOWN_VALUE) {
        aCellDesc += nsPrintfCString(",\"%s\":%d", key.BeginReading(), value);
      }
    }

    aCellDesc += "}";
  }
  aCellDesc += "]";
}

void
StumblerInfo::DumpStumblerInfo()
{
  if (!mIsWifiInfoResponseReceived || mCellInfoResponsesReceived != mCellInfoResponsesExpected) {
    STUMBLER_DBG("CellInfoReceived=%d (Expected=%d), WifiInfoResponseReceived=%d\n",
                  mCellInfoResponsesReceived, mCellInfoResponsesExpected, mIsWifiInfoResponseReceived);
    return;
  }
  mIsWifiInfoResponseReceived = false;
  mCellInfoResponsesReceived = 0;

  nsAutoCString desc;
  nsresult rv = LocationInfoToString(desc);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    STUMBLER_ERR("LocationInfoToString failed, skip this dump");
    return;
  }

  CellNetworkInfoToString(desc);
  desc += mWifiDesc;

  STUMBLER_DBG("dispatch write event to thread\n");
  nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target);

  nsCOMPtr<nsIRunnable> event = new WriteStumbleOnThread(desc);
  target->Dispatch(event, NS_DISPATCH_NORMAL);
}

/* void notifyGetCellInfoList (in uint32_t count, [array, size_is (count)] in nsICellInfo result); */
NS_IMETHODIMP
StumblerInfo::NotifyGetCellInfoList(uint32_t count, nsICellInfo** aCellInfos)
{
  MOZ_ASSERT(NS_IsMainThread());
  STUMBLER_DBG("There are %d cellinfo in the result\n", count);

  for (uint32_t i = 0; i < count; i++) {
    mCellInfo.AppendElement(aCellInfos[i]);
  }
  mCellInfoResponsesReceived++;
  DumpStumblerInfo();
  return NS_OK;
}

/* void notifyGetCellInfoListFailed (in DOMString error); */
NS_IMETHODIMP StumblerInfo::NotifyGetCellInfoListFailed(const nsAString& error)
{
  MOZ_ASSERT(NS_IsMainThread());
  mCellInfoResponsesReceived++;
  STUMBLER_ERR("NotifyGetCellInfoListFailedm CellInfoReadyNum=%d, mCellInfoResponsesExpected=%d, mIsWifiInfoResponseReceived=%d",
                mCellInfoResponsesReceived, mCellInfoResponsesExpected, mIsWifiInfoResponseReceived);
  DumpStumblerInfo();
  return NS_OK;
}

NS_IMETHODIMP
StumblerInfo::Onready(uint32_t count, nsIWifiScanResult** results)
{
  MOZ_ASSERT(NS_IsMainThread());
  STUMBLER_DBG("There are %d wifiAPinfo in the result\n",count);

  mWifiDesc += ",\"wifiAccessPoints\": [";
  bool firstItem = true;
  for (uint32_t i = 0 ; i < count ; i++) {
    nsString ssid;
    results[i]->GetSsid(ssid);
    if (ssid.IsEmpty()) {
      STUMBLER_DBG("no ssid, skip this AP\n");
      continue;
    }

    if (ssid.Length() >= 6) {
      if (StringEndsWith(ssid, NS_LITERAL_STRING("_nomap"))) {
        STUMBLER_DBG("end with _nomap. skip this AP(ssid :%s)\n", ssid.get());
        continue;
      }
    }

    if (firstItem) {
      mWifiDesc += "{";
      firstItem = false;
    } else {
      mWifiDesc += ",{";
    }

    // mac address
    nsString bssid;
    results[i]->GetBssid(bssid);
    //   00:00:00:00:00:00 --> 000000000000
    bssid.StripChars(":");
    mWifiDesc += "\"macAddress\":\"";
    mWifiDesc += NS_ConvertUTF16toUTF8(bssid);

    uint32_t signal;
    results[i]->GetSignalStrength(&signal);
    mWifiDesc += "\",\"signalStrength\":";
    mWifiDesc.AppendInt(signal);

    mWifiDesc += "}";
  }
  mWifiDesc += "]";

  mIsWifiInfoResponseReceived = true;
  DumpStumblerInfo();
  return NS_OK;
}

NS_IMETHODIMP
StumblerInfo::Onfailure()
{
  MOZ_ASSERT(NS_IsMainThread());
  STUMBLER_ERR("GetWifiScanResults Onfailure\n");
  mIsWifiInfoResponseReceived = true;
  DumpStumblerInfo();
  return NS_OK;
}


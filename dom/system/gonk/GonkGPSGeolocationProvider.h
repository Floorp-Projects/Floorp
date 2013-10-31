/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GonkGPSGeolocationProvider_h
#define GonkGPSGeolocationProvider_h

#include <hardware/gps.h> // for GpsInterface
#include "nsCOMPtr.h"
#include "nsIGeolocationProvider.h"
#ifdef MOZ_B2G_RIL
#include "nsIRadioInterfaceLayer.h"
#include "nsISettingsService.h"
#endif

class nsIThread;

#define GONK_GPS_GEOLOCATION_PROVIDER_CID \
{ 0x48525ec5, 0x5a7f, 0x490a, { 0x92, 0x77, 0xba, 0x66, 0xe0, 0xd2, 0x2c, 0x8b } }

#define GONK_GPS_GEOLOCATION_PROVIDER_CONTRACTID \
"@mozilla.org/gonk-gps-geolocation-provider;1"

class GonkGPSGeolocationProvider : public nsIGeolocationProvider
#ifdef MOZ_B2G_RIL
                                 , public nsIRILDataCallback
                                 , public nsISettingsServiceCallback
#endif
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONPROVIDER
#ifdef MOZ_B2G_RIL
  NS_DECL_NSIRILDATACALLBACK
  NS_DECL_NSISETTINGSSERVICECALLBACK
#endif

  static already_AddRefed<GonkGPSGeolocationProvider> GetSingleton();

private:

  /* Client should use GetSingleton() to get the provider instance. */
  GonkGPSGeolocationProvider();
  GonkGPSGeolocationProvider(const GonkGPSGeolocationProvider &);
  GonkGPSGeolocationProvider & operator = (const GonkGPSGeolocationProvider &);
  ~GonkGPSGeolocationProvider();

  static void LocationCallback(GpsLocation* location);
  static void StatusCallback(GpsStatus* status);
  static void SvStatusCallback(GpsSvStatus* sv_info);
  static void NmeaCallback(GpsUtcTime timestamp, const char* nmea, int length);
  static void SetCapabilitiesCallback(uint32_t capabilities);
  static void AcquireWakelockCallback();
  static void ReleaseWakelockCallback();
  static pthread_t CreateThreadCallback(const char* name, void (*start)(void*), void* arg);
  static void RequestUtcTimeCallback();
#ifdef MOZ_B2G_RIL
  static void AGPSStatusCallback(AGpsStatus* status);
  static void AGPSRILSetIDCallback(uint32_t flags);
  static void AGPSRILRefLocCallback(uint32_t flags);
#endif

  static GpsCallbacks mCallbacks;
#ifdef MOZ_B2G_RIL
  static AGpsCallbacks mAGPSCallbacks;
  static AGpsRilCallbacks mAGPSRILCallbacks;
#endif

  void Init();
  void StartGPS();
  void ShutdownGPS();
#ifdef MOZ_B2G_RIL
  void SetupAGPS();
  int32_t GetDataConnectionState();
  void SetAGpsDataConn(nsAString& aApn);
  void RequestDataConnection();
  void ReleaseDataConnection();
  void RequestSettingValue(char* aKey);
  void RequestSetID(uint32_t flags);
  void SetReferenceLocation();
#endif

  const GpsInterface* GetGPSInterface();

  static GonkGPSGeolocationProvider* sSingleton;

  bool mStarted;

  bool mSupportsScheduling;
#ifdef MOZ_B2G_RIL
  bool mSupportsMSB;
  bool mSupportsMSA;
#endif
  bool mSupportsSingleShot;
  bool mSupportsTimeInjection;

  const GpsInterface* mGpsInterface;
#ifdef MOZ_B2G_RIL
  const AGpsInterface* mAGpsInterface;
  const AGpsRilInterface* mAGpsRilInterface;
  nsCOMPtr<nsIRadioInterface> mRadioInterface;
#endif
  nsCOMPtr<nsIGeolocationUpdate> mLocationCallback;
  nsCOMPtr<nsIThread> mInitThread;
};

#endif /* GonkGPSGeolocationProvider_h */

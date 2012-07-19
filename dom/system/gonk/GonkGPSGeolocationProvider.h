/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GonkGPSGeolocationProvider_h
#define GonkGPSGeolocationProvider_h

#include <hardware/gps.h> // for GpsInterface
#include "nsCOMPtr.h"
#include "nsIGeolocationProvider.h"
#include "nsIRadioInterfaceLayer.h"
#include "nsString.h"

class nsIThread;

class GonkGPSGeolocationProvider : public nsIGeolocationProvider
                                 , public nsIRILDataCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONPROVIDER
  NS_DECL_NSIRILDATACALLBACK

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
  static void AGPSStatusCallback(AGpsStatus* status);
  static void AGPSRILSetIDCallback(uint32_t flags);
  static void AGPSRILRefLocCallback(uint32_t flags);

  static GpsCallbacks mCallbacks;
  static AGpsCallbacks mAGPSCallbacks;
  static AGpsRilCallbacks mAGPSRILCallbacks;

  void Init();
  void SetupAGPS();
  void StartGPS();
  void ShutdownNow();
  void RequestDataConnection();
  void ReleaseDataConnection();
  void RequestSetID(uint32_t flags);
  void SetReferenceLocation();

  const GpsInterface* GetGPSInterface();

  static GonkGPSGeolocationProvider* sSingleton;

  bool mStarted;

  bool mSupportsScheduling;
  bool mSupportsMSB;
  bool mSupportsMSA;
  bool mSupportsSingleShot;
  bool mSupportsTimeInjection;

  const GpsInterface* mGpsInterface;
  const AGpsInterface* mAGpsInterface;
  const AGpsRilInterface* mAGpsRilInterface;
  nsCOMPtr<nsIGeolocationUpdate> mLocationCallback;
  nsCOMPtr<nsIThread> mInitThread;
  nsCOMPtr<nsIRadioInterfaceLayer> mRIL;
  nsAutoString mCid;
};

#endif /* GonkGPSGeolocationProvider_h */

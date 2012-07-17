/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GonkGPSGeolocationProvider_h
#define GonkGPSGeolocationProvider_h

#include <hardware/gps.h> // for GpsInterface
#include "nsIGeolocationProvider.h"

class nsIThread;

class GonkGPSGeolocationProvider : public nsIGeolocationProvider
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONPROVIDER

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

  static GpsCallbacks mCallbacks;

  void Init();
  void StartGPS();
  void ShutdownNow();

  const GpsInterface* GetGPSInterface();

  static GonkGPSGeolocationProvider* sSingleton;

  bool mStarted;
  const GpsInterface* mGpsInterface;
  nsCOMPtr<nsIGeolocationUpdate> mLocationCallback;
  nsCOMPtr<nsIThread> mInitThread;
};

#endif /* GonkGPSGeolocationProvider_h */

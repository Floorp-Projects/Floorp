/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <pthread.h>
#include <hardware/gps.h>

#include "mozilla/Preferences.h"
#include "nsGeoPosition.h"
#include "GonkGPSGeolocationProvider.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS1(GonkGPSGeolocationProvider, nsIGeolocationProvider)

GonkGPSGeolocationProvider* GonkGPSGeolocationProvider::sSingleton;

static void
LocationCallback(GpsLocation* location)
{
  nsRefPtr<GonkGPSGeolocationProvider> provider =
    GonkGPSGeolocationProvider::GetSingleton();
  nsCOMPtr<nsIGeolocationUpdate> callback = provider->GetLocationCallback();
  
  if (!callback)
    return;

  nsRefPtr<nsGeoPosition> somewhere = new nsGeoPosition(location->latitude,
                                                        location->longitude,
                                                        location->altitude,
                                                        location->accuracy,
                                                        location->accuracy,
                                                        location->bearing,
                                                        location->speed,
                                                        location->timestamp);
  callback->Update(somewhere);
}

static void
StatusCallback(GpsStatus* status)
{}

static void
SvStatusCallback(GpsSvStatus* sv_info)
{}

static void
NmeaCallback(GpsUtcTime timestamp, const char* nmea, int length)
{}

static void
SetCapabilitiesCallback(uint32_t capabilities)
{}

static void
AcquireWakelockCallback()
{}

static void
ReleaseWakelockCallback()
{}

typedef void *(*pthread_func)(void *);

/** Callback for creating a thread that can call into the JS codes.
 */
static pthread_t
CreateThreadCallback(const char* name, void (*start)(void *), void* arg)
{
  pthread_t thread;
  pthread_attr_t attr;

  pthread_attr_init(&attr);

  /* Unfortunately pthread_create and the callback disagreed on what
   * start function should return.
   */
  pthread_create(&thread, &attr, reinterpret_cast<pthread_func>(start), arg);

  return thread;
}

static void
RequestUtcTimeCallback()
{}

static GpsCallbacks gCallbacks = {
  sizeof(GpsCallbacks),
  LocationCallback,
  StatusCallback,
  SvStatusCallback,
  NmeaCallback,
  SetCapabilitiesCallback,
  AcquireWakelockCallback,
  ReleaseWakelockCallback,
  CreateThreadCallback,
#ifdef GPS_CAPABILITY_ON_DEMAND_TIME
  RequestUtcTimeCallback,
#endif
};

GonkGPSGeolocationProvider::GonkGPSGeolocationProvider()
  : mStarted(false)
  , mGpsInterface(nsnull)
{
}

GonkGPSGeolocationProvider::~GonkGPSGeolocationProvider()
{
  Shutdown();
  sSingleton = NULL;
}

/* static */ already_AddRefed<GonkGPSGeolocationProvider>
GonkGPSGeolocationProvider::GetSingleton()
{
  if (!sSingleton)
    sSingleton = new GonkGPSGeolocationProvider();

  NS_ADDREF(sSingleton);
  return sSingleton;
}

already_AddRefed<nsIGeolocationUpdate>
GonkGPSGeolocationProvider::GetLocationCallback()
{
  nsCOMPtr<nsIGeolocationUpdate> callback = mLocationCallback;
  return callback.forget();
}

const GpsInterface*
GonkGPSGeolocationProvider::GetGPSInterface()
{
  hw_module_t* module;

  if (hw_get_module(GPS_HARDWARE_MODULE_ID, (hw_module_t const**)&module))
    return NULL;

  hw_device_t* device;
  if (module->methods->open(module, GPS_HARDWARE_MODULE_ID, &device))
    return NULL;

  gps_device_t* gps_device = (gps_device_t *)device;
  const GpsInterface* result = gps_device->get_gps_interface(gps_device);

  if (result->size != sizeof(GpsInterface)) {
    return nsnull;
  }
  return result;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::Startup()
{
  if (mStarted)
    return NS_OK;

  mGpsInterface = GetGPSInterface();

  NS_ENSURE_TRUE(mGpsInterface, NS_ERROR_FAILURE);

  PRInt32 update = Preferences::GetInt("geo.default.update", 1000);

  if (mGpsInterface->init(&gCallbacks) != 0)
    return NS_ERROR_FAILURE;

  mGpsInterface->start();
  mGpsInterface->set_position_mode(GPS_POSITION_MODE_STANDALONE,
                                   GPS_POSITION_RECURRENCE_PERIODIC,
                                   update, 0, 0);

  mStarted = true;
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::Watch(nsIGeolocationUpdate* aCallback)
{
  mLocationCallback = aCallback;
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::Shutdown()
{
  if (!mStarted)
    return NS_OK;

  NS_ENSURE_TRUE(mGpsInterface, NS_OK);

  mGpsInterface->stop();
  mGpsInterface->cleanup();
  return NS_OK;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::SetHighAccuracy(bool)
{
  return NS_OK;
}

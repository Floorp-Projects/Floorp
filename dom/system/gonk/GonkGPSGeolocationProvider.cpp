/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <pthread.h>
#include <hardware/gps.h>

#include "mozilla/Preferences.h"
#include "nsGeoPosition.h"
#include "nsThreadUtils.h"
#include "GonkGPSGeolocationProvider.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS1(GonkGPSGeolocationProvider, nsIGeolocationProvider)

GonkGPSGeolocationProvider* GonkGPSGeolocationProvider::sSingleton;
GpsCallbacks GonkGPSGeolocationProvider::mCallbacks = {
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

void
GonkGPSGeolocationProvider::LocationCallback(GpsLocation* location)
{
  class UpdateLocationEvent : public nsRunnable {
  public:
    UpdateLocationEvent(nsGeoPosition* aPosition)
      : mPosition(aPosition)
    {}
    NS_IMETHOD Run() {
      nsRefPtr<GonkGPSGeolocationProvider> provider =
        GonkGPSGeolocationProvider::GetSingleton();
      nsCOMPtr<nsIGeolocationUpdate> callback = provider->mLocationCallback;
      if (callback) {
        callback->Update(mPosition);
      }
      return NS_OK;
    }
  private:
    nsRefPtr<nsGeoPosition> mPosition;
  };

  nsRefPtr<nsGeoPosition> somewhere = new nsGeoPosition(location->latitude,
                                                        location->longitude,
                                                        location->altitude,
                                                        location->accuracy,
                                                        location->accuracy,
                                                        location->bearing,
                                                        location->speed,
                                                        location->timestamp);
  NS_DispatchToMainThread(new UpdateLocationEvent(somewhere));
}

void
GonkGPSGeolocationProvider::StatusCallback(GpsStatus* status)
{
}

void
GonkGPSGeolocationProvider::SvStatusCallback(GpsSvStatus* sv_info)
{
}

void
GonkGPSGeolocationProvider::NmeaCallback(GpsUtcTime timestamp, const char* nmea, int length)
{
}

void
GonkGPSGeolocationProvider::SetCapabilitiesCallback(uint32_t capabilities)
{
}

void
GonkGPSGeolocationProvider::AcquireWakelockCallback()
{
}

void
GonkGPSGeolocationProvider::ReleaseWakelockCallback()
{
}

typedef void *(*pthread_func)(void *);

/** Callback for creating a thread that can call into the JS codes.
 */
pthread_t
GonkGPSGeolocationProvider::CreateThreadCallback(const char* name, void (*start)(void *), void* arg)
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

void
GonkGPSGeolocationProvider::RequestUtcTimeCallback()
{
}

GonkGPSGeolocationProvider::GonkGPSGeolocationProvider()
  : mStarted(false)
  , mGpsInterface(nsnull)
{
}

GonkGPSGeolocationProvider::~GonkGPSGeolocationProvider()
{
  ShutdownNow();
  sSingleton = NULL;
}

already_AddRefed<GonkGPSGeolocationProvider>
GonkGPSGeolocationProvider::GetSingleton()
{
  if (!sSingleton)
    sSingleton = new GonkGPSGeolocationProvider();

  NS_ADDREF(sSingleton);
  return sSingleton;
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

void
GonkGPSGeolocationProvider::Init()
{
  // Must not be main thread. Some GPS driver's first init takes very long.
  MOZ_ASSERT(!NS_IsMainThread());

  mGpsInterface = GetGPSInterface();
  if (!mGpsInterface) {
    return;
  }

  if (mGpsInterface->init(&mCallbacks) != 0) {
    return;
  }

  NS_DispatchToMainThread(NS_NewRunnableMethod(this, &GonkGPSGeolocationProvider::StartGPS));
}

void
GonkGPSGeolocationProvider::StartGPS()
{
  PRInt32 update = Preferences::GetInt("geo.default.update", 1000);

  int positionMode = GPS_POSITION_MODE_STANDALONE;

  mGpsInterface->set_position_mode(positionMode,
                                   GPS_POSITION_RECURRENCE_PERIODIC,
                                   update, 0, 0);

  mGpsInterface->start();
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::Startup()
{
  if (mStarted) {
    return NS_OK;
  }

  if (!mInitThread) {
    nsresult rv = NS_NewThread(getter_AddRefs(mInitThread));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mInitThread->Dispatch(NS_NewRunnableMethod(this, &GonkGPSGeolocationProvider::Init),
                        NS_DISPATCH_NORMAL);

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
  if (!mStarted) {
    return NS_OK;
  }

  mInitThread->Dispatch(NS_NewRunnableMethod(this, &GonkGPSGeolocationProvider::ShutdownNow),
                        NS_DISPATCH_NORMAL);

  return NS_OK;
}

void
GonkGPSGeolocationProvider::ShutdownNow()
{
  if (!mGpsInterface) {
    return;
  }

  mGpsInterface->stop();
  mGpsInterface->cleanup();
  mStarted = false;
  mInitThread = nsnull;
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::SetHighAccuracy(bool)
{
  return NS_OK;
}

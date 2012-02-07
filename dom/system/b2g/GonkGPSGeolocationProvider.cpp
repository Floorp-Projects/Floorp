/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kan-Ru Chen <kchen@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

static GpsCallbacks gCallbacks = {
  sizeof(GpsCallbacks),
  LocationCallback,
  NULL, /* StatusCallback */
  NULL, /* SvStatusCallback */
  NULL, /* NmeaCallback */
  NULL, /* SetCapabilitiesCallback */
  NULL, /* AcquireWakelockCallback */
  NULL, /* ReleaseWakelockCallback */
  CreateThreadCallback,
};

GonkGPSGeolocationProvider::GonkGPSGeolocationProvider()
  : mStarted(false)
{
  mGpsInterface = GetGPSInterface();
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
  return gps_device->get_gps_interface(gps_device);
}

NS_IMETHODIMP
GonkGPSGeolocationProvider::Startup()
{
  if (mStarted)
    return NS_OK;

  NS_ENSURE_TRUE(mGpsInterface, NS_ERROR_FAILURE);

  PRInt32 update = Preferences::GetInt("geo.default.update", 1000);

  mGpsInterface->init(&gCallbacks);
  mGpsInterface->start();
  mGpsInterface->set_position_mode(GPS_POSITION_MODE_STANDALONE,
                                   GPS_POSITION_RECURRENCE_PERIODIC,
                                   update, 0, 0);
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

  mGpsInterface->stop();
  mGpsInterface->cleanup();

  return NS_OK;
}

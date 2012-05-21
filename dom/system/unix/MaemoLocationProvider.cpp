/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <math.h>
#include "nsGeoPosition.h"
#include "MaemoLocationProvider.h"
#include "nsIClassInfo.h"
#include "nsDOMClassInfoID.h"
#include "nsIServiceManager.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS2(MaemoLocationProvider, nsIGeolocationProvider, nsITimerCallback)

MaemoLocationProvider::MaemoLocationProvider() :
  mLocationChanged(0),
  mControlError(0),
  mDeviceDisconnected(0),
  mControlStopped(0),
  mHasSeenLocation(false),
  mHasGPS(true),
  mGPSControl(nsnull),
  mGPSDevice(nsnull),
  mIgnoreMinorChanges(false),
  mPrevLat(0.0),
  mPrevLong(0.0),
  mIgnoreBigHErr(true),
  mMaxHErr(1000),
  mIgnoreBigVErr(true),
  mMaxVErr(100)
{
}

MaemoLocationProvider::~MaemoLocationProvider()
{
}

void MaemoLocationProvider::DeviceDisconnected(LocationGPSDevice* device, void* self)
{
}

void MaemoLocationProvider::ControlStopped(LocationGPSDControl* device, void* self)
{
  MaemoLocationProvider* provider = static_cast<MaemoLocationProvider*>(self);
  provider->StartControl();
}

void MaemoLocationProvider::ControlError(LocationGPSDControl* control, void* self)
{
}

void MaemoLocationProvider::LocationChanged(LocationGPSDevice* device, void* self)
{
  if (!device || !device->fix)
    return;

  guint32 &fields = device->fix->fields;
  if (!(fields & LOCATION_GPS_DEVICE_LATLONG_SET))
    return;

  if (!(device->fix->eph && !isnan(device->fix->eph)))
    return;

  MaemoLocationProvider* provider = static_cast<MaemoLocationProvider*>(self);
  NS_ENSURE_TRUE(provider, );
  provider->LocationUpdate(device);
}

nsresult
MaemoLocationProvider::LocationUpdate(LocationGPSDevice* aDev)
{
  double hErr = aDev->fix->eph/100;
  if (mIgnoreBigHErr && hErr > (double)mMaxHErr)
    hErr = (double)mMaxHErr;

  double vErr = aDev->fix->epv/2;
  if (mIgnoreBigVErr && vErr > (double)mMaxVErr)
    vErr = (double)mMaxVErr;

  double altitude = 0, speed = 0, track = 0;
  if (aDev->fix->epv && !isnan(aDev->fix->epv))
    altitude = aDev->fix->altitude;
  if (aDev->fix->eps && !isnan(aDev->fix->eps))
    speed = aDev->fix->speed;
  if (aDev->fix->epd && !isnan(aDev->fix->epd))
    track = aDev->fix->track;

#ifdef DEBUG
  double dist = location_distance_between(mPrevLat, mPrevLong, aDev->fix->latitude, aDev->fix->longitude)*1000;
  fprintf(stderr, "dist:%.9f, Lat: %.6f, Long:%.6f, HErr:%g, Alt:%.6f, VErr:%g, dir:%g[%g], sp:%g[%g]\n",
           dist, aDev->fix->latitude, aDev->fix->longitude,
           hErr, altitude,
           aDev->fix->epv/2, track, aDev->fix->epd,
           speed, aDev->fix->eps);
  mPrevLat = aDev->fix->latitude;
  mPrevLong = aDev->fix->longitude;
#endif

  nsRefPtr<nsGeoPosition> somewhere = new nsGeoPosition(aDev->fix->latitude,
                                                        aDev->fix->longitude,
                                                        altitude,
                                                        hErr,
                                                        vErr,
                                                        track,
                                                        speed,
                                                        PR_Now());
  Update(somewhere);

  return NS_OK;
}

NS_IMETHODIMP
MaemoLocationProvider::Notify(nsITimer* aTimer)
{
  LocationChanged(mGPSDevice, this);
  return NS_OK;
}

nsresult
MaemoLocationProvider::StartControl()
{
  if (mGPSControl)
    return NS_OK;

  mGPSControl = location_gpsd_control_get_default();
  NS_ENSURE_TRUE(mGPSControl, NS_ERROR_FAILURE);

  mControlError = g_signal_connect(mGPSControl, "error",
                                   G_CALLBACK(ControlError), this);

  mControlStopped = g_signal_connect(mGPSControl, "gpsd_stopped",
                                     G_CALLBACK(ControlStopped), this);

  location_gpsd_control_start(mGPSControl);
  return NS_OK;
}

nsresult
MaemoLocationProvider::StartDevice()
{
  if (mGPSDevice)
    return NS_OK;

  mGPSDevice = (LocationGPSDevice*)g_object_new(LOCATION_TYPE_GPS_DEVICE, NULL);
  NS_ENSURE_TRUE(mGPSDevice, NS_ERROR_FAILURE);

  mLocationChanged    = g_signal_connect(mGPSDevice, "changed",
                                         G_CALLBACK(LocationChanged), this);

  mDeviceDisconnected = g_signal_connect(mGPSDevice, "disconnected",
                                         G_CALLBACK(DeviceDisconnected), this);
  return NS_OK;
}

NS_IMETHODIMP MaemoLocationProvider::Startup()
{
  nsresult rv(NS_OK);

  rv = StartControl();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = StartDevice();
  NS_ENSURE_SUCCESS(rv, rv);

  mIgnoreBigHErr =
    Preferences::GetBool("geo.herror.ignore.big", mIgnoreBigHErr);

  if (mIgnoreBigHErr) {
    mMaxHErr = Preferences::GetInt("geo.herror.max.value", mMaxHErr);
  }

  mIgnoreBigVErr =
    Preferences::GetBool("geo.verror.ignore.big", mIgnoreBigVErr);

  if (mIgnoreBigVErr) {
    mMaxVErr = Preferences::GetInt("geo.verror.max.value", mMaxVErr);
  }

  if (mUpdateTimer)
    return NS_OK;

  // 0 second no timer created
  PRInt32 update = Preferences::GetInt("geo.default.update", 0);

  if (!update)
    return NS_OK;

  mUpdateTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);

  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  if (update)
    mUpdateTimer->InitWithCallback(this, update, nsITimer::TYPE_REPEATING_SLACK);

  return NS_OK;
}

NS_IMETHODIMP MaemoLocationProvider::Watch(nsIGeolocationUpdate *callback)
{
  if (mCallback)
    return NS_OK;

  mCallback = callback;
  return NS_OK;
}

NS_IMETHODIMP MaemoLocationProvider::Shutdown()
{
  if (mUpdateTimer)
    mUpdateTimer->Cancel();

  g_signal_handler_disconnect(mGPSDevice, mLocationChanged);
  g_signal_handler_disconnect(mGPSDevice, mDeviceDisconnected);

  g_signal_handler_disconnect(mGPSDevice, mControlError);
  g_signal_handler_disconnect(mGPSDevice, mControlStopped);

  mHasSeenLocation = false;
  mCallback = nsnull;

  if (mGPSControl) {
    location_gpsd_control_stop(mGPSControl);
    g_object_unref(mGPSControl);
    mGPSControl = nsnull;
  }
  if (mGPSDevice) {
    g_object_unref(mGPSDevice);
    mGPSDevice = nsnull;
  }

  return NS_OK;
}

void MaemoLocationProvider::Update(nsIDOMGeoPosition* aPosition)
{
  mHasSeenLocation = true;
  if (mCallback)
    mCallback->Update(aPosition);
}


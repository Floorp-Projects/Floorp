/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsGeoPosition.h"
#include "nsIConsoleService.h"
#include "nsServiceManagerUtils.h"
#include "nsIDOMGeoPositionError.h"
#include "CoreLocationLocationProvider.h"
#include "nsCocoaFeatures.h"
#include "prtime.h"

#include <CoreLocation/CLError.h>
#include <CoreLocation/CLLocation.h>
#include <CoreLocation/CLLocationManager.h>
#include <CoreLocation/CLLocationManagerDelegate.h>

#include <objc/objc.h>
#include <objc/objc-runtime.h>

#include "nsObjCExceptions.h"

using namespace mozilla;

static const CLLocationAccuracy kHIGH_ACCURACY = kCLLocationAccuracyBest;
static const CLLocationAccuracy kDEFAULT_ACCURACY = kCLLocationAccuracyNearestTenMeters;

@interface LocationDelegate : NSObject <CLLocationManagerDelegate>
{
  CoreLocationLocationProvider* mProvider;
}

- (id)init:(CoreLocationLocationProvider*)aProvider;
- (void)locationManager:(CLLocationManager*)aManager
  didFailWithError:(NSError *)aError;

/* XXX (ggp) didUpdateToLocation is supposedly deprecated in favor of
 * locationManager:didUpdateLocations, which is undocumented and didn't seem to
 * work for me. This should be changed in the future, though.
 */
- (void)locationManager:(CLLocationManager*)aManager
  didUpdateToLocation:(CLLocation *)aNewLocation
  fromLocation:(CLLocation *)aOldLocation;
@end

@implementation LocationDelegate
- (id) init:(CoreLocationLocationProvider*) aProvider
{
  if (self = [super init]) {
    mProvider = aProvider;
  }

  return self;
}

- (void)locationManager:(CLLocationManager*)aManager
  didFailWithError:(NSError *)aError
{
  nsCOMPtr<nsIConsoleService> console =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);

  NS_ENSURE_TRUE_VOID(console);

  NSString* message =
    [@"Failed to acquire position: " stringByAppendingString: [aError localizedDescription]];

  console->LogStringMessage(NS_ConvertUTF8toUTF16([message UTF8String]).get());

  uint16_t err = nsIDOMGeoPositionError::POSITION_UNAVAILABLE;
  if ([aError code] == kCLErrorDenied) {
    err = nsIDOMGeoPositionError::PERMISSION_DENIED;
  }

  mProvider->NotifyError(err);
}

- (void)locationManager:(CLLocationManager*)aManager
  didUpdateToLocation:(CLLocation *)aNewLocation
  fromLocation:(CLLocation *)aOldLocation
{
  nsCOMPtr<nsIDOMGeoPosition> geoPosition =
    new nsGeoPosition(aNewLocation.coordinate.latitude,
                      aNewLocation.coordinate.longitude,
                      aNewLocation.altitude,
                      aNewLocation.horizontalAccuracy,
                      aNewLocation.verticalAccuracy,
                      aNewLocation.course,
                      aNewLocation.speed,
                      PR_Now());

  mProvider->Update(geoPosition);
}
@end

class CoreLocationObjects {
public:
  NS_METHOD Init(CoreLocationLocationProvider* aProvider) {
    mLocationManager = [[CLLocationManager alloc] init];
    NS_ENSURE_TRUE(mLocationManager, NS_ERROR_NOT_AVAILABLE);

    mLocationDelegate = [[LocationDelegate alloc] init:aProvider];
    NS_ENSURE_TRUE(mLocationDelegate, NS_ERROR_NOT_AVAILABLE);

    mLocationManager.desiredAccuracy = kDEFAULT_ACCURACY;
    mLocationManager.delegate = mLocationDelegate;

    return NS_OK;
  }

  ~CoreLocationObjects() {
    if (mLocationManager) {
      [mLocationManager release];
    }

    if (mLocationDelegate) {
      [mLocationDelegate release];
    }
  }

  LocationDelegate* mLocationDelegate;
  CLLocationManager* mLocationManager;
};

NS_IMPL_ISUPPORTS(CoreLocationLocationProvider, nsIGeolocationProvider)

CoreLocationLocationProvider::CoreLocationLocationProvider()
  : mCLObjects(nullptr)
{
}

NS_IMETHODIMP
CoreLocationLocationProvider::Startup()
{
  if (!mCLObjects) {
    nsAutoPtr<CoreLocationObjects> clObjs(new CoreLocationObjects());

    nsresult rv = clObjs->Init(this);
    NS_ENSURE_SUCCESS(rv, rv);

    mCLObjects = clObjs.forget();
  }

  [mCLObjects->mLocationManager startUpdatingLocation];
  return NS_OK;
}

NS_IMETHODIMP
CoreLocationLocationProvider::Watch(nsIGeolocationUpdate* aCallback)
{
  if (mCallback) {
    return NS_OK;
  }

  mCallback = aCallback;
  return NS_OK;
}

NS_IMETHODIMP
CoreLocationLocationProvider::Shutdown()
{
  NS_ENSURE_STATE(mCLObjects);

  [mCLObjects->mLocationManager stopUpdatingLocation];

  delete mCLObjects;
  mCLObjects = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
CoreLocationLocationProvider::SetHighAccuracy(bool aEnable)
{
  NS_ENSURE_STATE(mCLObjects);

  mCLObjects->mLocationManager.desiredAccuracy =
    (aEnable ? kHIGH_ACCURACY : kDEFAULT_ACCURACY);

  return NS_OK;
}

void
CoreLocationLocationProvider::Update(nsIDOMGeoPosition* aSomewhere)
{
  if (aSomewhere && mCallback) {
    mCallback->Update(aSomewhere);
  }
}

void
CoreLocationLocationProvider::NotifyError(uint16_t aErrorCode)
{
  mCallback->NotifyError(aErrorCode);
}

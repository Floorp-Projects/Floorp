/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsGeoPosition.h"
#include "nsIConsoleService.h"
#include "nsServiceManagerUtils.h"
#include "CoreLocationLocationProvider.h"
#include "nsCocoaFeatures.h"
#include "prtime.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/PositionErrorBinding.h"
#include "MLSFallback.h"

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

bool isMacGeoSystemPermissionEnabled() { return [CLLocationManager locationServicesEnabled]; }

@interface LocationDelegate : NSObject <CLLocationManagerDelegate> {
  CoreLocationLocationProvider* mProvider;
}

- (id)init:(CoreLocationLocationProvider*)aProvider;
- (void)locationManager:(CLLocationManager*)aManager didFailWithError:(NSError*)aError;
- (void)locationManager:(CLLocationManager*)aManager didUpdateLocations:(NSArray*)locations;

@end

@implementation LocationDelegate
- (id)init:(CoreLocationLocationProvider*)aProvider {
  if ((self = [super init])) {
    mProvider = aProvider;
  }

  return self;
}

- (void)locationManager:(CLLocationManager*)aManager didFailWithError:(NSError*)aError {
  nsCOMPtr<nsIConsoleService> console = do_GetService(NS_CONSOLESERVICE_CONTRACTID);

  NS_ENSURE_TRUE_VOID(console);

  NSString* message =
      [@"Failed to acquire position: " stringByAppendingString:[aError localizedDescription]];

  console->LogStringMessage(NS_ConvertUTF8toUTF16([message UTF8String]).get());

  if ([aError code] == kCLErrorDenied) {
    mProvider->NotifyError(dom::PositionError_Binding::PERMISSION_DENIED);
    return;
  }

  // The CL provider does not fallback to GeoIP, so use NetworkGeolocationProvider for this.
  // The concept here is: on error, hand off geolocation to MLS, which will then report
  // back a location or error.
  mProvider->CreateMLSFallbackProvider();
}

- (void)locationManager:(CLLocationManager*)aManager didUpdateLocations:(NSArray*)aLocations {
  if (aLocations.count < 1) {
    return;
  }

  mProvider->CancelMLSFallbackProvider();

  CLLocation* location = [aLocations objectAtIndex:0];

  double altitude;
  double altitudeAccuracy;

  // A negative verticalAccuracy indicates that the altitude value is invalid.
  if (location.verticalAccuracy >= 0) {
    altitude = location.altitude;
    altitudeAccuracy = location.verticalAccuracy;
  } else {
    altitude = UnspecifiedNaN<double>();
    altitudeAccuracy = UnspecifiedNaN<double>();
  }

  double speed = location.speed >= 0 ? location.speed : UnspecifiedNaN<double>();

  double heading = location.course >= 0 ? location.course : UnspecifiedNaN<double>();

  // nsGeoPositionCoords will convert NaNs to null for optional properties of
  // the JavaScript Coordinates object.
  nsCOMPtr<nsIDOMGeoPosition> geoPosition = new nsGeoPosition(
      location.coordinate.latitude, location.coordinate.longitude, altitude,
      location.horizontalAccuracy, altitudeAccuracy, heading, speed, PR_Now() / PR_USEC_PER_MSEC);

  mProvider->Update(geoPosition);
  Telemetry::Accumulate(Telemetry::GEOLOCATION_OSX_SOURCE_IS_MLS, false);
}
@end

NS_IMPL_ISUPPORTS(CoreLocationLocationProvider::MLSUpdate, nsIGeolocationUpdate);

CoreLocationLocationProvider::MLSUpdate::MLSUpdate(CoreLocationLocationProvider& parentProvider)
    : mParentLocationProvider(parentProvider) {}

NS_IMETHODIMP
CoreLocationLocationProvider::MLSUpdate::Update(nsIDOMGeoPosition* position) {
  nsCOMPtr<nsIDOMGeoPositionCoords> coords;
  position->GetCoords(getter_AddRefs(coords));
  if (!coords) {
    return NS_ERROR_FAILURE;
  }
  mParentLocationProvider.Update(position);
  Telemetry::Accumulate(Telemetry::GEOLOCATION_OSX_SOURCE_IS_MLS, true);
  return NS_OK;
}

NS_IMETHODIMP
CoreLocationLocationProvider::MLSUpdate::NotifyError(uint16_t error) {
  mParentLocationProvider.NotifyError(error);
  return NS_OK;
}

class CoreLocationObjects {
 public:
  nsresult Init(CoreLocationLocationProvider* aProvider) {
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
    : mCLObjects(nullptr), mMLSFallbackProvider(nullptr) {}

NS_IMETHODIMP
CoreLocationLocationProvider::Startup() {
  if (!mCLObjects) {
    nsAutoPtr<CoreLocationObjects> clObjs(new CoreLocationObjects());

    nsresult rv = clObjs->Init(this);
    NS_ENSURE_SUCCESS(rv, rv);

    mCLObjects = clObjs.forget();
  }

  // Must be stopped before starting or response (success or failure) is not guaranteed
  [mCLObjects->mLocationManager stopUpdatingLocation];
  [mCLObjects->mLocationManager startUpdatingLocation];
  return NS_OK;
}

NS_IMETHODIMP
CoreLocationLocationProvider::Watch(nsIGeolocationUpdate* aCallback) {
  if (mCallback) {
    return NS_OK;
  }

  mCallback = aCallback;
  return NS_OK;
}

NS_IMETHODIMP
CoreLocationLocationProvider::Shutdown() {
  NS_ENSURE_STATE(mCLObjects);

  [mCLObjects->mLocationManager stopUpdatingLocation];

  delete mCLObjects;
  mCLObjects = nullptr;

  if (mMLSFallbackProvider) {
    mMLSFallbackProvider->Shutdown();
    mMLSFallbackProvider = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
CoreLocationLocationProvider::SetHighAccuracy(bool aEnable) {
  NS_ENSURE_STATE(mCLObjects);

  mCLObjects->mLocationManager.desiredAccuracy = (aEnable ? kHIGH_ACCURACY : kDEFAULT_ACCURACY);

  return NS_OK;
}

void CoreLocationLocationProvider::Update(nsIDOMGeoPosition* aSomewhere) {
  if (aSomewhere && mCallback) {
    mCallback->Update(aSomewhere);
  }
}
void CoreLocationLocationProvider::NotifyError(uint16_t aErrorCode) {
  nsCOMPtr<nsIGeolocationUpdate> callback(mCallback);
  callback->NotifyError(aErrorCode);
}
void CoreLocationLocationProvider::CreateMLSFallbackProvider() {
  if (mMLSFallbackProvider) {
    return;
  }

  mMLSFallbackProvider = new MLSFallback(0);
  mMLSFallbackProvider->Startup(new MLSUpdate(*this));
}

void CoreLocationLocationProvider::CancelMLSFallbackProvider() {
  if (!mMLSFallbackProvider) {
    return;
  }

  mMLSFallbackProvider->Shutdown();
  mMLSFallbackProvider = nullptr;
}

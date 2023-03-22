/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GeolocationPosition.h"
#include "mozilla/dom/GeolocationCoordinates.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/dom/GeolocationPositionBinding.h"

using mozilla::EqualOrBothNaN;

// NaN() is a more convenient function name.
inline double NaN() { return mozilla::UnspecifiedNaN<double>(); }

////////////////////////////////////////////////////
// nsGeoPositionCoords
////////////////////////////////////////////////////
nsGeoPositionCoords::nsGeoPositionCoords(double aLat, double aLong, double aAlt,
                                         double aHError, double aVError,
                                         double aHeading, double aSpeed)
    : mLat(aLat),
      mLong(aLong),
      mAlt(aAlt),
      mHError((aHError >= 0) ? aHError : 0)
      // altitudeAccuracy without an altitude doesn't make any sense.
      ,
      mVError((aVError >= 0 && !std::isnan(aAlt)) ? aVError : NaN())
      // If the hosting device is stationary (i.e. the value of the speed
      // attribute is 0), then the value of the heading attribute must be NaN
      // (or null).
      ,
      mHeading((aHeading >= 0 && aHeading < 360 && aSpeed > 0) ? aHeading
                                                               : NaN()),
      mSpeed(aSpeed >= 0 ? aSpeed : NaN()) {
  // Sanity check the location provider's results in debug builds. If the
  // location provider is returning bogus results, we'd like to know, but
  // we prefer to return some position data to JavaScript over a
  // POSITION_UNAVAILABLE error.
  MOZ_ASSERT(aLat >= -90 && aLat <= 90);
  MOZ_ASSERT(aLong >= -180 && aLong <= 180);
  MOZ_ASSERT(!(aLat == 0 && aLong == 0));  // valid but probably a bug

  MOZ_ASSERT(EqualOrBothNaN(mAlt, aAlt));
  MOZ_ASSERT(mHError == aHError);
  MOZ_ASSERT(EqualOrBothNaN(mVError, aVError));
  MOZ_ASSERT(EqualOrBothNaN(mHeading, aHeading));
  MOZ_ASSERT(EqualOrBothNaN(mSpeed, aSpeed));
}

nsGeoPositionCoords::~nsGeoPositionCoords() = default;

NS_INTERFACE_MAP_BEGIN(nsGeoPositionCoords)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPositionCoords)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionCoords)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsGeoPositionCoords)
NS_IMPL_RELEASE(nsGeoPositionCoords)

NS_IMETHODIMP
nsGeoPositionCoords::GetLatitude(double* aLatitude) {
  *aLatitude = mLat;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetLongitude(double* aLongitude) {
  *aLongitude = mLong;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetAltitude(double* aAltitude) {
  *aAltitude = mAlt;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetAccuracy(double* aAccuracy) {
  *aAccuracy = mHError;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetAltitudeAccuracy(double* aAltitudeAccuracy) {
  *aAltitudeAccuracy = mVError;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetHeading(double* aHeading) {
  *aHeading = mHeading;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetSpeed(double* aSpeed) {
  *aSpeed = mSpeed;
  return NS_OK;
}

////////////////////////////////////////////////////
// nsGeoPosition
////////////////////////////////////////////////////

nsGeoPosition::nsGeoPosition(double aLat, double aLong, double aAlt,
                             double aHError, double aVError, double aHeading,
                             double aSpeed, EpochTimeStamp aTimestamp)
    : mTimestamp(aTimestamp) {
  mCoords = new nsGeoPositionCoords(aLat, aLong, aAlt, aHError, aVError,
                                    aHeading, aSpeed);
}

nsGeoPosition::nsGeoPosition(nsIDOMGeoPositionCoords* aCoords,
                             EpochTimeStamp aTimestamp)
    : mTimestamp(aTimestamp), mCoords(aCoords) {}

nsGeoPosition::~nsGeoPosition() = default;

NS_INTERFACE_MAP_BEGIN(nsGeoPosition)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPosition)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPosition)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsGeoPosition)
NS_IMPL_RELEASE(nsGeoPosition)

NS_IMETHODIMP
nsGeoPosition::GetTimestamp(EpochTimeStamp* aTimestamp) {
  *aTimestamp = mTimestamp;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPosition::GetCoords(nsIDOMGeoPositionCoords** aCoords) {
  NS_IF_ADDREF(*aCoords = mCoords);
  return NS_OK;
}

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(GeolocationPosition, mParent,
                                      mCoordinates)
NS_IMPL_CYCLE_COLLECTING_ADDREF(GeolocationPosition)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GeolocationPosition)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GeolocationPosition)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

GeolocationPosition::GeolocationPosition(nsISupports* aParent,
                                         nsIDOMGeoPosition* aGeoPosition)
    : mParent(aParent), mGeoPosition(aGeoPosition) {}

GeolocationPosition::~GeolocationPosition() = default;

nsISupports* GeolocationPosition::GetParentObject() const { return mParent; }

JSObject* GeolocationPosition::WrapObject(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return GeolocationPosition_Binding::Wrap(aCx, this, aGivenProto);
}

GeolocationCoordinates* GeolocationPosition::Coords() {
  if (!mCoordinates) {
    nsCOMPtr<nsIDOMGeoPositionCoords> coords;
    mGeoPosition->GetCoords(getter_AddRefs(coords));
    MOZ_ASSERT(coords, "coords should not be null");

    mCoordinates = new GeolocationCoordinates(this, coords);
  }

  return mCoordinates;
}

uint64_t GeolocationPosition::Timestamp() const {
  uint64_t rv;

  mGeoPosition->GetTimestamp(&rv);
  return rv;
}

}  // namespace mozilla::dom

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGeoPosition.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/dom/PositionBinding.h"
#include "mozilla/dom/CoordinatesBinding.h"

using mozilla::IsNaN;

// NaN() is a more convenient function name.
inline
double NaN()
{
  return mozilla::UnspecifiedNaN<double>();
}

#ifdef DEBUG
static
bool EqualOrNaN(double a, double b)
{
  return (a == b) || (IsNaN(a) && IsNaN(b));
}
#endif

////////////////////////////////////////////////////
// nsGeoPositionCoords
////////////////////////////////////////////////////
nsGeoPositionCoords::nsGeoPositionCoords(double aLat, double aLong,
                                         double aAlt, double aHError,
                                         double aVError, double aHeading,
                                         double aSpeed)
  : mLat(aLat)
  , mLong(aLong)
  , mAlt(aAlt)
  , mHError((aHError >= 0) ? aHError : 0)
    // altitudeAccuracy without an altitude doesn't make any sense.
  , mVError((aVError >= 0 && !IsNaN(aAlt)) ? aVError : NaN())
    // If the hosting device is stationary (i.e. the value of the speed attribute
    // is 0), then the value of the heading attribute must be NaN (or null).
  , mHeading((aHeading >= 0 && aHeading < 360 && aSpeed > 0)
             ? aHeading : NaN())
  , mSpeed(aSpeed >= 0 ? aSpeed : NaN())
{
  // Sanity check the location provider's results in debug builds. If the
  // location provider is returning bogus results, we'd like to know, but
  // we prefer to return some position data to JavaScript over a
  // POSITION_UNAVAILABLE error.
  MOZ_ASSERT(aLat >= -90 && aLat <= 90);
  MOZ_ASSERT(aLong >= -180 && aLong <= 180);
  MOZ_ASSERT(!(aLat == 0 && aLong == 0)); // valid but probably a bug

  MOZ_ASSERT(EqualOrNaN(mAlt, aAlt));
  MOZ_ASSERT(mHError == aHError);
  MOZ_ASSERT(EqualOrNaN(mVError, aVError));
  MOZ_ASSERT(EqualOrNaN(mHeading, aHeading));
  MOZ_ASSERT(EqualOrNaN(mSpeed, aSpeed));
}

nsGeoPositionCoords::~nsGeoPositionCoords()
{
}

NS_INTERFACE_MAP_BEGIN(nsGeoPositionCoords)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPositionCoords)
NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionCoords)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsGeoPositionCoords)
NS_IMPL_RELEASE(nsGeoPositionCoords)

NS_IMETHODIMP
nsGeoPositionCoords::GetLatitude(double *aLatitude)
{
  *aLatitude = mLat;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetLongitude(double *aLongitude)
{
  *aLongitude = mLong;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetAltitude(double *aAltitude)
{
  *aAltitude = mAlt;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetAccuracy(double *aAccuracy)
{
  *aAccuracy = mHError;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetAltitudeAccuracy(double *aAltitudeAccuracy)
{
  *aAltitudeAccuracy = mVError;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetHeading(double *aHeading)
{
  *aHeading = mHeading;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetSpeed(double *aSpeed)
{
  *aSpeed = mSpeed;
  return NS_OK;
}

////////////////////////////////////////////////////
// nsGeoPosition
////////////////////////////////////////////////////

nsGeoPosition::nsGeoPosition(double aLat, double aLong,
                             double aAlt, double aHError,
                             double aVError, double aHeading,
                             double aSpeed, DOMTimeStamp aTimestamp) :
    mTimestamp(aTimestamp)
{
    mCoords = new nsGeoPositionCoords(aLat, aLong,
                                      aAlt, aHError,
                                      aVError, aHeading,
                                      aSpeed);
}

nsGeoPosition::nsGeoPosition(nsIDOMGeoPositionCoords *aCoords,
                             DOMTimeStamp aTimestamp) :
  mTimestamp(aTimestamp),
  mCoords(aCoords)
{
}

nsGeoPosition::~nsGeoPosition()
{
}

NS_INTERFACE_MAP_BEGIN(nsGeoPosition)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPosition)
NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPosition)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsGeoPosition)
NS_IMPL_RELEASE(nsGeoPosition)

NS_IMETHODIMP
nsGeoPosition::GetTimestamp(DOMTimeStamp* aTimestamp)
{
  *aTimestamp = mTimestamp;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPosition::GetCoords(nsIDOMGeoPositionCoords * *aCoords)
{
  NS_IF_ADDREF(*aCoords = mCoords);
  return NS_OK;
}

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Position, mParent, mCoordinates)
NS_IMPL_CYCLE_COLLECTING_ADDREF(Position)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Position)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Position)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Position::Position(nsISupports* aParent, nsIDOMGeoPosition* aGeoPosition)
  : mParent(aParent)
  , mGeoPosition(aGeoPosition)
{
}

Position::~Position()
{
}

nsISupports*
Position::GetParentObject() const
{
  return mParent;
}

JSObject*
Position::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return Position_Binding::Wrap(aCx, this, aGivenProto);
}

Coordinates*
Position::Coords()
{
  if (!mCoordinates) {
    nsCOMPtr<nsIDOMGeoPositionCoords> coords;
    mGeoPosition->GetCoords(getter_AddRefs(coords));
    MOZ_ASSERT(coords, "coords should not be null");

    mCoordinates = new Coordinates(this, coords);
  }

  return mCoordinates;
}

uint64_t
Position::Timestamp() const
{
  uint64_t rv;

  mGeoPosition->GetTimestamp(&rv);
  return rv;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Coordinates, mPosition)
NS_IMPL_CYCLE_COLLECTING_ADDREF(Coordinates)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Coordinates)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Coordinates)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Coordinates::Coordinates(Position* aPosition, nsIDOMGeoPositionCoords* aCoords)
  : mPosition(aPosition)
  , mCoords(aCoords)
{
}

Coordinates::~Coordinates()
{
}

Position*
Coordinates::GetParentObject() const
{
  return mPosition;
}

JSObject*
Coordinates::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return Coordinates_Binding::Wrap(aCx, this, aGivenProto);
}

#define GENERATE_COORDS_WRAPPED_GETTER(name) \
double                                       \
Coordinates::name() const                    \
{                                            \
  double rv;                                 \
  mCoords->Get##name(&rv);                   \
  return rv;                                 \
}

#define GENERATE_COORDS_WRAPPED_GETTER_NULLABLE(name) \
Nullable<double>                                      \
Coordinates::Get##name() const                        \
{                                                     \
  double value;                                       \
  mCoords->Get##name(&value);                         \
  Nullable<double> rv;                                \
  if (!IsNaN(value)) {                                \
    rv.SetValue(value);                               \
  }                                                   \
  return rv;                                          \
}

GENERATE_COORDS_WRAPPED_GETTER(Latitude)
GENERATE_COORDS_WRAPPED_GETTER(Longitude)
GENERATE_COORDS_WRAPPED_GETTER_NULLABLE(Altitude)
GENERATE_COORDS_WRAPPED_GETTER(Accuracy)
GENERATE_COORDS_WRAPPED_GETTER_NULLABLE(AltitudeAccuracy)
GENERATE_COORDS_WRAPPED_GETTER_NULLABLE(Heading)
GENERATE_COORDS_WRAPPED_GETTER_NULLABLE(Speed)

#undef GENERATE_COORDS_WRAPPED_GETTER
#undef GENERATE_COORDS_WRAPPED_GETTER_NULLABLE

} // namespace dom
} // namespace mozilla

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGeoPosition.h"
#include "nsDOMClassInfoID.h"
#include "nsIClassInfo.h"
#include "nsContentUtils.h"

#include "mozilla/dom/PositionBinding.h"
#include "mozilla/dom/CoordinatesBinding.h"

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
  , mHError(aHError)
  , mVError(aVError)
  , mHeading(aHeading)
  , mSpeed(aSpeed)
{
}

nsGeoPositionCoords::~nsGeoPositionCoords()
{
}

DOMCI_DATA(GeoPositionCoords, nsGeoPositionCoords)

NS_INTERFACE_MAP_BEGIN(nsGeoPositionCoords)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPositionCoords)
NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionCoords)
NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(GeoPositionCoords)
NS_INTERFACE_MAP_END

NS_IMPL_THREADSAFE_ADDREF(nsGeoPositionCoords)
NS_IMPL_THREADSAFE_RELEASE(nsGeoPositionCoords)

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
                             double aSpeed, long long aTimestamp) :
    mTimestamp(aTimestamp)
{
    mCoords = new nsGeoPositionCoords(aLat, aLong,
                                      aAlt, aHError,
                                      aVError, aHeading,
                                      aSpeed);
}

nsGeoPosition::nsGeoPosition(nsIDOMGeoPositionCoords *aCoords,
                             long long aTimestamp) :
    mTimestamp(aTimestamp),
    mCoords(aCoords)
{
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

DOMCI_DATA(GeoPosition, nsGeoPosition)

NS_INTERFACE_MAP_BEGIN(nsGeoPosition)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPosition)
NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPosition)
NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(GeoPosition)
NS_INTERFACE_MAP_END

NS_IMPL_THREADSAFE_ADDREF(nsGeoPosition)
NS_IMPL_THREADSAFE_RELEASE(nsGeoPosition)

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


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_2(Position, mParent, mCoordinates)
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
  SetIsDOMBinding();
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
Position::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return PositionBinding::Wrap(aCx, aScope, this);
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

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(Coordinates, mPosition)
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
  SetIsDOMBinding();
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
Coordinates::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return CoordinatesBinding::Wrap(aCx, aScope, this);
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
  double rv;                                          \
  mCoords->Get##name(&rv);                            \
  return Nullable<double>(rv);                        \
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

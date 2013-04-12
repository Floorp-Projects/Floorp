/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGeoPosition_h
#define nsGeoPosition_h

#include "nsAutoPtr.h"
#include "nsIDOMGeoPositionCoords.h"
#include "nsIDOMGeoPosition.h"
#include "nsString.h"
#include "mozilla/Attributes.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/Nullable.h"

struct JSContext;

////////////////////////////////////////////////////
// nsGeoPositionCoords
////////////////////////////////////////////////////

/**
 * Simple object that holds a single point in space.
 */
class nsGeoPositionCoords MOZ_FINAL : public nsIDOMGeoPositionCoords
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMGEOPOSITIONCOORDS
  
  nsGeoPositionCoords(double aLat, double aLong,
                      double aAlt, double aHError,
                      double aVError, double aHeading,
                      double aSpeed);
  ~nsGeoPositionCoords();
private:
  const double mLat, mLong, mAlt, mHError, mVError, mHeading, mSpeed;
};


////////////////////////////////////////////////////
// nsGeoPosition
////////////////////////////////////////////////////

class nsGeoPosition MOZ_FINAL : public nsIDOMGeoPosition
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMGEOPOSITION
  
  nsGeoPosition(double aLat, double aLong,
                double aAlt, double aHError,
                double aVError, double aHeading,
                double aSpeed, long long aTimestamp);
  

  nsGeoPosition(nsIDOMGeoPositionCoords *aCoords,
                long long aTimestamp);

  nsGeoPosition(nsIDOMGeoPositionCoords *aCoords,
                DOMTimeStamp aTimestamp);

private:
  ~nsGeoPosition();
  long long mTimestamp;
  nsRefPtr<nsIDOMGeoPositionCoords> mCoords;
};

////////////////////////////////////////////////////
// WebIDL wrappers for the classes above
////////////////////////////////////////////////////

namespace mozilla {
namespace dom {

class Coordinates;

class Position MOZ_FINAL : public nsISupports,
                           public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Position)

public:
  Position(nsISupports* aParent, nsIDOMGeoPosition* aGeoPosition);

  ~Position();

  nsISupports* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;

  Coordinates* Coords();

  uint64_t Timestamp() const;

  nsIDOMGeoPosition* GetWrappedGeoPosition() { return mGeoPosition; }

private:
  nsRefPtr<Coordinates> mCoordinates;
  nsCOMPtr<nsISupports> mParent;
  nsCOMPtr<nsIDOMGeoPosition> mGeoPosition;
};

class Coordinates MOZ_FINAL : public nsISupports,
                              public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Coordinates)

public:
  Coordinates(Position* aPosition, nsIDOMGeoPositionCoords* aCoords);

  ~Coordinates();

  Position* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;

  double Latitude() const;

  double Longitude() const;

  Nullable<double> GetAltitude() const;

  double Accuracy() const;

  Nullable<double> GetAltitudeAccuracy() const;

  Nullable<double> GetHeading() const;

  Nullable<double> GetSpeed() const;
private:
  nsRefPtr<Position> mPosition;
  nsCOMPtr<nsIDOMGeoPositionCoords> mCoords;
};

} // namespace dom
} // namespace mozilla

#endif /* nsGeoPosition_h */


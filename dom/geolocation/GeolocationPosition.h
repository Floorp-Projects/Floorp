/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GeolocationPosition_h
#define mozilla_dom_GeolocationPosition_h

#include "nsIDOMGeoPositionCoords.h"
#include "nsIDOMGeoPosition.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/Nullable.h"
#include "js/TypeDecls.h"

////////////////////////////////////////////////////
// nsGeoPositionCoords
////////////////////////////////////////////////////

/**
 * Simple object that holds a single point in space.
 */
class nsGeoPositionCoords final : public nsIDOMGeoPositionCoords {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDOMGEOPOSITIONCOORDS

  nsGeoPositionCoords(double aLat, double aLong, double aAlt, double aHError,
                      double aVError, double aHeading, double aSpeed);

 private:
  ~nsGeoPositionCoords();
  const double mLat, mLong, mAlt, mHError, mVError, mHeading, mSpeed;
};

////////////////////////////////////////////////////
// nsGeoPosition
////////////////////////////////////////////////////

class nsGeoPosition final : public nsIDOMGeoPosition {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDOMGEOPOSITION

  nsGeoPosition(double aLat, double aLong, double aAlt, double aHError,
                double aVError, double aHeading, double aSpeed,
                DOMTimeStamp aTimestamp);

  nsGeoPosition(nsIDOMGeoPositionCoords* aCoords, DOMTimeStamp aTimestamp);

 private:
  ~nsGeoPosition();
  DOMTimeStamp mTimestamp;
  RefPtr<nsIDOMGeoPositionCoords> mCoords;
};

////////////////////////////////////////////////////
// WebIDL wrappers for the classes above
////////////////////////////////////////////////////

namespace mozilla {
namespace dom {

class GeolocationCoordinates;

class GeolocationPosition final : public nsISupports, public nsWrapperCache {
  ~GeolocationPosition();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(GeolocationPosition)

 public:
  GeolocationPosition(nsISupports* aParent, nsIDOMGeoPosition* aGeoPosition);

  nsISupports* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  GeolocationCoordinates* Coords();

  uint64_t Timestamp() const;

  nsIDOMGeoPosition* GetWrappedGeoPosition() { return mGeoPosition; }

 private:
  RefPtr<GeolocationCoordinates> mCoordinates;
  nsCOMPtr<nsISupports> mParent;
  nsCOMPtr<nsIDOMGeoPosition> mGeoPosition;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_GeolocationPosition_h */

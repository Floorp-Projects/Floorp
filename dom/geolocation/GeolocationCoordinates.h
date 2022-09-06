/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GeolocationCoordinates_h
#define mozilla_dom_GeolocationCoordinates_h

#include "nsIDOMGeoPositionCoords.h"
#include "GeolocationPosition.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/Nullable.h"
#include "js/TypeDecls.h"

namespace mozilla::dom {

class GeolocationCoordinates final : public nsISupports, public nsWrapperCache {
  ~GeolocationCoordinates();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(GeolocationCoordinates)

 public:
  GeolocationCoordinates(GeolocationPosition* aPosition,
                         nsIDOMGeoPositionCoords* aCoords);

  GeolocationPosition* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  double Latitude() const;

  double Longitude() const;

  Nullable<double> GetAltitude() const;

  double Accuracy() const;

  Nullable<double> GetAltitudeAccuracy() const;

  Nullable<double> GetHeading() const;

  Nullable<double> GetSpeed() const;

 private:
  RefPtr<GeolocationPosition> mPosition;
  nsCOMPtr<nsIDOMGeoPositionCoords> mCoords;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_GeolocationCoordinates_h */

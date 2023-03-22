/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GeolocationCoordinates.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/dom/GeolocationPosition.h"
#include "mozilla/dom/GeolocationCoordinatesBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(GeolocationCoordinates, mPosition)
NS_IMPL_CYCLE_COLLECTING_ADDREF(GeolocationCoordinates)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GeolocationCoordinates)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GeolocationCoordinates)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

GeolocationCoordinates::GeolocationCoordinates(GeolocationPosition* aPosition,
                                               nsIDOMGeoPositionCoords* aCoords)
    : mPosition(aPosition), mCoords(aCoords) {}

GeolocationCoordinates::~GeolocationCoordinates() = default;

GeolocationPosition* GeolocationCoordinates::GetParentObject() const {
  return mPosition;
}

JSObject* GeolocationCoordinates::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return GeolocationCoordinates_Binding::Wrap(aCx, this, aGivenProto);
}

#define GENERATE_COORDS_WRAPPED_GETTER(name)    \
  double GeolocationCoordinates::name() const { \
    double rv;                                  \
    mCoords->Get##name(&rv);                    \
    return rv;                                  \
  }

#define GENERATE_COORDS_WRAPPED_GETTER_NULLABLE(name)          \
  Nullable<double> GeolocationCoordinates::Get##name() const { \
    double value;                                              \
    mCoords->Get##name(&value);                                \
    Nullable<double> rv;                                       \
    if (!std::isnan(value)) {                                  \
      rv.SetValue(value);                                      \
    }                                                          \
    return rv;                                                 \
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

}  // namespace mozilla::dom

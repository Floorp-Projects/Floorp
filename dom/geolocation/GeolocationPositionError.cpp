/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GeolocationPositionError.h"
#include "mozilla/dom/GeolocationPositionErrorBinding.h"
#include "mozilla/CycleCollectedJSContext.h"  // for nsAutoMicroTask
#include "Geolocation.h"

extern mozilla::LazyLogModule gGeolocationLog;

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(GeolocationPositionError, mParent)

GeolocationPositionError::GeolocationPositionError(Geolocation* aParent,
                                                   int16_t aCode)
    : mCode(aCode), mParent(aParent) {}

GeolocationPositionError::~GeolocationPositionError() = default;

void GeolocationPositionError::GetMessage(nsAString& aMessage) const {
  switch (mCode) {
    case GeolocationPositionError_Binding::PERMISSION_DENIED:
      aMessage = u"User denied geolocation prompt"_ns;
      break;
    case GeolocationPositionError_Binding::POSITION_UNAVAILABLE:
      aMessage = u"Unknown error acquiring position"_ns;
      break;
    case GeolocationPositionError_Binding::TIMEOUT:
      aMessage = u"Position acquisition timed out"_ns;
      break;
    default:
      break;
  }
}

nsWrapperCache* GeolocationPositionError::GetParentObject() const {
  return mParent;
}

JSObject* GeolocationPositionError::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return GeolocationPositionError_Binding::Wrap(aCx, this, aGivenProto);
}

void GeolocationPositionError::NotifyCallback(
    const GeoPositionErrorCallback& aCallback) {
  MOZ_LOG(gGeolocationLog, LogLevel::Debug,
          ("GeolocationPositionError::NotifyCallback"));
  nsAutoMicroTask mt;
  if (aCallback.HasWebIDLCallback()) {
    RefPtr<PositionErrorCallback> callback = aCallback.GetWebIDLCallback();

    if (callback) {
      callback->Call(*this);
    }
  } else {
    nsIDOMGeoPositionErrorCallback* callback = aCallback.GetXPCOMCallback();
    if (callback) {
      callback->HandleEvent(this);
    }
  }
}

}  // namespace mozilla::dom

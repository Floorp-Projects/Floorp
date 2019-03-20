/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PositionError.h"
#include "mozilla/dom/PositionErrorBinding.h"
#include "mozilla/CycleCollectedJSContext.h"  // for nsAutoMicroTask
#include "nsGeolocation.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PositionError, mParent)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(PositionError, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(PositionError, Release)

PositionError::PositionError(Geolocation* aParent, int16_t aCode)
    : mCode(aCode), mParent(aParent) {}

PositionError::~PositionError() = default;

void PositionError::GetMessage(nsAString& aMessage) const {
  switch (mCode) {
    case PositionError_Binding::PERMISSION_DENIED:
      aMessage = NS_LITERAL_STRING("User denied geolocation prompt");
      break;
    case PositionError_Binding::POSITION_UNAVAILABLE:
      aMessage = NS_LITERAL_STRING("Unknown error acquiring position");
      break;
    case PositionError_Binding::TIMEOUT:
      aMessage = NS_LITERAL_STRING("Position acquisition timed out");
      break;
    default:
      break;
  }
}

nsWrapperCache* PositionError::GetParentObject() const { return mParent; }

JSObject* PositionError::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return PositionError_Binding::Wrap(aCx, this, aGivenProto);
}

void PositionError::NotifyCallback(const GeoPositionErrorCallback& aCallback) {
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

}  // namespace dom
}  // namespace mozilla

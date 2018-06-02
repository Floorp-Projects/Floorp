/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PositionError.h"
#include "mozilla/dom/PositionErrorBinding.h"
#include "nsGeolocation.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PositionError)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPositionError)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionError)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PositionError, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(PositionError)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PositionError)

PositionError::PositionError(Geolocation* aParent, int16_t aCode)
  : mCode(aCode)
  , mParent(aParent)
{
}

PositionError::~PositionError() = default;


NS_IMETHODIMP
PositionError::GetCode(int16_t *aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  *aCode = Code();
  return NS_OK;
}

NS_IMETHODIMP
PositionError::GetMessage(nsAString& aMessage)
{
  switch (mCode)
  {
    case nsIDOMGeoPositionError::PERMISSION_DENIED:
      aMessage = NS_LITERAL_STRING("User denied geolocation prompt");
      break;
    case nsIDOMGeoPositionError::POSITION_UNAVAILABLE:
      aMessage = NS_LITERAL_STRING("Unknown error acquiring position");
      break;
    case nsIDOMGeoPositionError::TIMEOUT:
      aMessage = NS_LITERAL_STRING("Position acquisition timed out");
      break;
    default:
      break;
  }
  return NS_OK;
}

nsWrapperCache*
PositionError::GetParentObject() const
{
  return mParent;
}

JSObject*
PositionError::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PositionErrorBinding::Wrap(aCx, this, aGivenProto);
}

void
PositionError::NotifyCallback(const GeoPositionErrorCallback& aCallback)
{
  nsAutoMicroTask mt;
  if (aCallback.HasWebIDLCallback()) {
    PositionErrorCallback* callback = aCallback.GetWebIDLCallback();

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

} // namespace mozilla
} // namespace dom


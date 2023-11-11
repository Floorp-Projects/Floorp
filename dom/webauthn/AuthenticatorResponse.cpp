/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Base64.h"
#include "mozilla/dom/AuthenticatorResponse.h"
#include "mozilla/dom/TypedArray.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WITH_JS_MEMBERS(
    AuthenticatorResponse, (mParent), (mClientDataJSONCachedObj))

NS_IMPL_CYCLE_COLLECTING_ADDREF(AuthenticatorResponse)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AuthenticatorResponse)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AuthenticatorResponse)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

AuthenticatorResponse::AuthenticatorResponse(nsPIDOMWindowInner* aParent)
    : mParent(aParent), mClientDataJSONCachedObj(nullptr) {
  // Call HoldJSObjects() in subclasses.
}

AuthenticatorResponse::~AuthenticatorResponse() {
  // Call DropJSObjects() in subclasses.
}

nsISupports* AuthenticatorResponse::GetParentObject() const { return mParent; }

void AuthenticatorResponse::GetClientDataJSON(
    JSContext* aCx, JS::MutableHandle<JSObject*> aValue, ErrorResult& aRv) {
  if (!mClientDataJSONCachedObj) {
    mClientDataJSONCachedObj = ArrayBuffer::Create(aCx, mClientDataJSON, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
  aValue.set(mClientDataJSONCachedObj);
}

void AuthenticatorResponse::SetClientDataJSON(const nsCString& aBuffer) {
  mClientDataJSON.Assign(aBuffer);
}

}  // namespace mozilla::dom

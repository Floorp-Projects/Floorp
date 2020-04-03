/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AuthenticatorResponse.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(AuthenticatorResponse)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AuthenticatorResponse)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mClientDataJSONCachedObj = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(AuthenticatorResponse)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mClientDataJSONCachedObj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AuthenticatorResponse)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

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

void AuthenticatorResponse::GetClientDataJSON(
    JSContext* aCx, JS::MutableHandle<JSObject*> aRetVal) {
  if (!mClientDataJSONCachedObj) {
    mClientDataJSONCachedObj = mClientDataJSON.ToArrayBuffer(aCx);
  }
  aRetVal.set(mClientDataJSONCachedObj);
}

nsresult AuthenticatorResponse::SetClientDataJSON(CryptoBuffer& aBuffer) {
  if (NS_WARN_IF(!mClientDataJSON.Assign(aBuffer))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthenticationBinding.h"
#include "mozilla/dom/AuthenticatorResponse.h"

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AuthenticatorResponse, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(AuthenticatorResponse)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AuthenticatorResponse)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AuthenticatorResponse)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

AuthenticatorResponse::AuthenticatorResponse(nsPIDOMWindowInner* aParent)
  : mParent(aParent)
{}

AuthenticatorResponse::~AuthenticatorResponse()
{}

JSObject*
AuthenticatorResponse::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto)
{
  return AuthenticatorResponseBinding::Wrap(aCx, this, aGivenProto);
}

void
AuthenticatorResponse::GetClientDataJSON(JSContext* aCx,
                                         JS::MutableHandle<JSObject*> aRetVal) const
{
  aRetVal.set(mClientDataJSON.ToUint8Array(aCx));
}

nsresult
AuthenticatorResponse::SetClientDataJSON(CryptoBuffer& aBuffer)
{
  if (NS_WARN_IF(!mClientDataJSON.Assign(aBuffer))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

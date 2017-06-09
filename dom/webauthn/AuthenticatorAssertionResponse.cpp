/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthenticationBinding.h"
#include "mozilla/dom/AuthenticatorAssertionResponse.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(AuthenticatorAssertionResponse, AuthenticatorResponse)
NS_IMPL_RELEASE_INHERITED(AuthenticatorAssertionResponse, AuthenticatorResponse)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AuthenticatorAssertionResponse)
NS_INTERFACE_MAP_END_INHERITING(AuthenticatorResponse)

AuthenticatorAssertionResponse::AuthenticatorAssertionResponse(nsPIDOMWindowInner* aParent)
  : AuthenticatorResponse(aParent)
{}

AuthenticatorAssertionResponse::~AuthenticatorAssertionResponse()
{}

JSObject*
AuthenticatorAssertionResponse::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto)
{
  return AuthenticatorAssertionResponseBinding::Wrap(aCx, this, aGivenProto);
}

void
AuthenticatorAssertionResponse::GetAuthenticatorData(JSContext* aCx,
                                                     JS::MutableHandle<JSObject*> aRetVal) const
{
  aRetVal.set(mAuthenticatorData.ToUint8Array(aCx));
}

nsresult
AuthenticatorAssertionResponse::SetAuthenticatorData(CryptoBuffer& aBuffer)
{
  if (NS_WARN_IF(!mAuthenticatorData.Assign(aBuffer))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

void
AuthenticatorAssertionResponse::GetSignature(JSContext* aCx,
                                             JS::MutableHandle<JSObject*> aRetVal) const
{
  aRetVal.set(mSignature.ToUint8Array(aCx));
}

nsresult
AuthenticatorAssertionResponse::SetSignature(CryptoBuffer& aBuffer)
{
  if (NS_WARN_IF(!mSignature.Assign(aBuffer))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

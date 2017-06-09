/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthenticationBinding.h"
#include "mozilla/dom/AuthenticatorAttestationResponse.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(AuthenticatorAttestationResponse, AuthenticatorResponse)
NS_IMPL_RELEASE_INHERITED(AuthenticatorAttestationResponse, AuthenticatorResponse)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AuthenticatorAttestationResponse)
NS_INTERFACE_MAP_END_INHERITING(AuthenticatorResponse)

AuthenticatorAttestationResponse::AuthenticatorAttestationResponse(nsPIDOMWindowInner* aParent)
  : AuthenticatorResponse(aParent)
{}

AuthenticatorAttestationResponse::~AuthenticatorAttestationResponse()
{}

JSObject*
AuthenticatorAttestationResponse::WrapObject(JSContext* aCx,
                                             JS::Handle<JSObject*> aGivenProto)
{
  return AuthenticatorAttestationResponseBinding::Wrap(aCx, this, aGivenProto);
}

void
AuthenticatorAttestationResponse::GetAttestationObject(JSContext* aCx,
                                                       JS::MutableHandle<JSObject*> aRetVal) const
{
  aRetVal.set(mAttestationObject.ToUint8Array(aCx));
}

nsresult
AuthenticatorAttestationResponse::SetAttestationObject(CryptoBuffer& aBuffer)
{
  if (NS_WARN_IF(!mAttestationObject.Assign(aBuffer))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

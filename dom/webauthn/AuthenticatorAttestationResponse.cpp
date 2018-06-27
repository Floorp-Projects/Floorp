/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthenticationBinding.h"
#include "mozilla/dom/AuthenticatorAttestationResponse.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(AuthenticatorAttestationResponse)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AuthenticatorAttestationResponse,
                                                AuthenticatorResponse)
  tmp->mAttestationObjectCachedObj = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(AuthenticatorAttestationResponse,
                                               AuthenticatorResponse)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAttestationObjectCachedObj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AuthenticatorAttestationResponse,
                                                  AuthenticatorResponse)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(AuthenticatorAttestationResponse, AuthenticatorResponse)
NS_IMPL_RELEASE_INHERITED(AuthenticatorAttestationResponse, AuthenticatorResponse)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AuthenticatorAttestationResponse)
NS_INTERFACE_MAP_END_INHERITING(AuthenticatorResponse)

AuthenticatorAttestationResponse::AuthenticatorAttestationResponse(nsPIDOMWindowInner* aParent)
  : AuthenticatorResponse(aParent)
  , mAttestationObjectCachedObj(nullptr)
{
  mozilla::HoldJSObjects(this);
}

AuthenticatorAttestationResponse::~AuthenticatorAttestationResponse()
{
  mozilla::DropJSObjects(this);
}

JSObject*
AuthenticatorAttestationResponse::WrapObject(JSContext* aCx,
                                             JS::Handle<JSObject*> aGivenProto)
{
  return AuthenticatorAttestationResponse_Binding::Wrap(aCx, this, aGivenProto);
}

void
AuthenticatorAttestationResponse::GetAttestationObject(JSContext* aCx,
                                                       JS::MutableHandle<JSObject*> aRetVal)
{
  if (!mAttestationObjectCachedObj) {
    mAttestationObjectCachedObj = mAttestationObject.ToArrayBuffer(aCx);
  }
  aRetVal.set(mAttestationObjectCachedObj);
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

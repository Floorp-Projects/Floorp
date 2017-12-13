/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthenticationBinding.h"
#include "mozilla/dom/AuthenticatorAssertionResponse.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(AuthenticatorAssertionResponse)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AuthenticatorAssertionResponse,
                                                AuthenticatorResponse)
  tmp->mAuthenticatorDataCachedObj = nullptr;
  tmp->mSignatureCachedObj = nullptr;
  tmp->mUserHandleCachedObj = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(AuthenticatorAssertionResponse,
                                               AuthenticatorResponse)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAuthenticatorDataCachedObj)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mSignatureCachedObj)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mUserHandleCachedObj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AuthenticatorAssertionResponse,
                                                  AuthenticatorResponse)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(AuthenticatorAssertionResponse, AuthenticatorResponse)
NS_IMPL_RELEASE_INHERITED(AuthenticatorAssertionResponse, AuthenticatorResponse)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AuthenticatorAssertionResponse)
NS_INTERFACE_MAP_END_INHERITING(AuthenticatorResponse)

AuthenticatorAssertionResponse::AuthenticatorAssertionResponse(nsPIDOMWindowInner* aParent)
  : AuthenticatorResponse(aParent)
  , mAuthenticatorDataCachedObj(nullptr)
  , mSignatureCachedObj(nullptr)
  , mUserHandleCachedObj(nullptr)
{
  mozilla::HoldJSObjects(this);
}

AuthenticatorAssertionResponse::~AuthenticatorAssertionResponse()
{
  mozilla::DropJSObjects(this);
}

JSObject*
AuthenticatorAssertionResponse::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto)
{
  return AuthenticatorAssertionResponseBinding::Wrap(aCx, this, aGivenProto);
}

void
AuthenticatorAssertionResponse::GetAuthenticatorData(JSContext* aCx,
                                                     JS::MutableHandle<JSObject*> aRetVal)
{
  if (!mAuthenticatorDataCachedObj) {
    mAuthenticatorDataCachedObj = mAuthenticatorData.ToArrayBuffer(aCx);
  }
  aRetVal.set(mAuthenticatorDataCachedObj);
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
                                             JS::MutableHandle<JSObject*> aRetVal)
{
  if (!mSignatureCachedObj) {
    mSignatureCachedObj = mSignature.ToArrayBuffer(aCx);
  }
  aRetVal.set(mSignatureCachedObj);
}

nsresult
AuthenticatorAssertionResponse::SetSignature(CryptoBuffer& aBuffer)
{
  if (NS_WARN_IF(!mSignature.Assign(aBuffer))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

void
AuthenticatorAssertionResponse::GetUserHandle(JSContext* aCx,
                                              JS::MutableHandle<JSObject*> aRetVal)
{
  if (!mUserHandleCachedObj) {
    mUserHandleCachedObj = mUserHandle.ToArrayBuffer(aCx);
  }
  aRetVal.set(mUserHandleCachedObj);
}

nsresult
AuthenticatorAssertionResponse::SetUserHandle(CryptoBuffer& aBuffer)
{
  if (NS_WARN_IF(!mUserHandle.Assign(aBuffer))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

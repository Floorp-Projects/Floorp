/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Base64.h"
#include "mozilla/dom/WebAuthenticationBinding.h"
#include "mozilla/dom/AuthenticatorAssertionResponse.h"
#include "mozilla/HoldDropJSObjects.h"

namespace mozilla::dom {

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

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    AuthenticatorAssertionResponse, AuthenticatorResponse)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(AuthenticatorAssertionResponse, AuthenticatorResponse)
NS_IMPL_RELEASE_INHERITED(AuthenticatorAssertionResponse, AuthenticatorResponse)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AuthenticatorAssertionResponse)
NS_INTERFACE_MAP_END_INHERITING(AuthenticatorResponse)

AuthenticatorAssertionResponse::AuthenticatorAssertionResponse(
    nsPIDOMWindowInner* aParent)
    : AuthenticatorResponse(aParent),
      mAuthenticatorDataCachedObj(nullptr),
      mSignatureCachedObj(nullptr),
      mUserHandleCachedObj(nullptr) {
  mozilla::HoldJSObjects(this);
}

AuthenticatorAssertionResponse::~AuthenticatorAssertionResponse() {
  mozilla::DropJSObjects(this);
}

JSObject* AuthenticatorAssertionResponse::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return AuthenticatorAssertionResponse_Binding::Wrap(aCx, this, aGivenProto);
}

void AuthenticatorAssertionResponse::GetAuthenticatorData(
    JSContext* aCx, JS::MutableHandle<JSObject*> aValue, ErrorResult& aRv) {
  if (!mAuthenticatorDataCachedObj) {
    mAuthenticatorDataCachedObj =
        ArrayBuffer::Create(aCx, mAuthenticatorData, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
  aValue.set(mAuthenticatorDataCachedObj);
}

void AuthenticatorAssertionResponse::SetAuthenticatorData(
    const nsTArray<uint8_t>& aBuffer) {
  mAuthenticatorData.Assign(aBuffer);
}

void AuthenticatorAssertionResponse::GetSignature(
    JSContext* aCx, JS::MutableHandle<JSObject*> aValue, ErrorResult& aRv) {
  if (!mSignatureCachedObj) {
    mSignatureCachedObj = ArrayBuffer::Create(aCx, mSignature, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
  aValue.set(mSignatureCachedObj);
}

void AuthenticatorAssertionResponse::SetSignature(
    const nsTArray<uint8_t>& aBuffer) {
  mSignature.Assign(aBuffer);
}

void AuthenticatorAssertionResponse::GetUserHandle(
    JSContext* aCx, JS::MutableHandle<JSObject*> aValue, ErrorResult& aRv) {
  // Per
  // https://w3c.github.io/webauthn/#ref-for-dom-authenticatorassertionresponse-userhandle%E2%91%A0
  // this should return null if the handle is unset.
  if (mUserHandle.IsEmpty()) {
    aValue.set(nullptr);
  } else {
    if (!mUserHandleCachedObj) {
      mUserHandleCachedObj = ArrayBuffer::Create(aCx, mUserHandle, aRv);
      if (aRv.Failed()) {
        return;
      }
    }
    aValue.set(mUserHandleCachedObj);
  }
}

void AuthenticatorAssertionResponse::SetUserHandle(
    const nsTArray<uint8_t>& aBuffer) {
  mUserHandle.Assign(aBuffer);
}

void AuthenticatorAssertionResponse::ToJSON(
    AuthenticatorAssertionResponseJSON& aJSON, ErrorResult& aError) {
  nsAutoCString clientDataJSONBase64;
  nsresult rv = Base64URLEncode(
      mClientDataJSON.Length(),
      reinterpret_cast<const uint8_t*>(mClientDataJSON.get()),
      mozilla::Base64URLEncodePaddingPolicy::Omit, clientDataJSONBase64);
  // This will only fail if the length is so long that it overflows 32-bits
  // when calculating the encoded size.
  if (NS_FAILED(rv)) {
    aError.ThrowDataError("clientDataJSON too long");
    return;
  }
  aJSON.mClientDataJSON.Assign(NS_ConvertUTF8toUTF16(clientDataJSONBase64));

  nsAutoCString authenticatorDataBase64;
  rv = Base64URLEncode(
      mAuthenticatorData.Length(), mAuthenticatorData.Elements(),
      mozilla::Base64URLEncodePaddingPolicy::Omit, authenticatorDataBase64);
  if (NS_FAILED(rv)) {
    aError.ThrowDataError("authenticatorData too long");
    return;
  }
  aJSON.mAuthenticatorData.Assign(
      NS_ConvertUTF8toUTF16(authenticatorDataBase64));

  nsAutoCString signatureBase64;
  rv = Base64URLEncode(mSignature.Length(), mSignature.Elements(),
                       mozilla::Base64URLEncodePaddingPolicy::Omit,
                       signatureBase64);
  if (NS_FAILED(rv)) {
    aError.ThrowDataError("signature too long");
    return;
  }
  aJSON.mSignature.Assign(NS_ConvertUTF8toUTF16(signatureBase64));

  if (!mUserHandle.IsEmpty()) {
    nsAutoCString userHandleBase64;
    rv = Base64URLEncode(mUserHandle.Length(), mUserHandle.Elements(),
                         mozilla::Base64URLEncodePaddingPolicy::Omit,
                         userHandleBase64);
    if (NS_FAILED(rv)) {
      aError.ThrowDataError("userHandle too long");
      return;
    }
    aJSON.mUserHandle.Construct(NS_ConvertUTF8toUTF16(userHandleBase64));
  }

  // attestationObject is not currently supported on assertion responses
}

}  // namespace mozilla::dom

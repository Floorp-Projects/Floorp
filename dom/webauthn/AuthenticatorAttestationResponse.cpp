/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AuthrsBridge_ffi.h"
#include "mozilla/Base64.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/AuthenticatorAttestationResponse.h"
#include "mozilla/dom/WebAuthenticationBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(AuthenticatorAttestationResponse)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(
    AuthenticatorAttestationResponse, AuthenticatorResponse)
  tmp->mAttestationObjectCachedObj = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(AuthenticatorAttestationResponse,
                                               AuthenticatorResponse)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAttestationObjectCachedObj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    AuthenticatorAttestationResponse, AuthenticatorResponse)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(AuthenticatorAttestationResponse,
                         AuthenticatorResponse)
NS_IMPL_RELEASE_INHERITED(AuthenticatorAttestationResponse,
                          AuthenticatorResponse)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AuthenticatorAttestationResponse)
NS_INTERFACE_MAP_END_INHERITING(AuthenticatorResponse)

AuthenticatorAttestationResponse::AuthenticatorAttestationResponse(
    nsPIDOMWindowInner* aParent)
    : AuthenticatorResponse(aParent), mAttestationObjectCachedObj(nullptr) {
  mozilla::HoldJSObjects(this);
}

AuthenticatorAttestationResponse::~AuthenticatorAttestationResponse() {
  mozilla::DropJSObjects(this);
}

JSObject* AuthenticatorAttestationResponse::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return AuthenticatorAttestationResponse_Binding::Wrap(aCx, this, aGivenProto);
}

void AuthenticatorAttestationResponse::GetAttestationObject(
    JSContext* aCx, JS::MutableHandle<JSObject*> aValue, ErrorResult& aRv) {
  if (!mAttestationObjectCachedObj) {
    mAttestationObjectCachedObj =
        ArrayBuffer::Create(aCx, mAttestationObject, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
  aValue.set(mAttestationObjectCachedObj);
}

void AuthenticatorAttestationResponse::SetAttestationObject(
    const nsTArray<uint8_t>& aBuffer) {
  mAttestationObject.Assign(aBuffer);
}

void AuthenticatorAttestationResponse::GetTransports(
    nsTArray<nsString>& aTransports) {
  aTransports.Assign(mTransports);
}

void AuthenticatorAttestationResponse::SetTransports(
    const nsTArray<nsString>& aTransports) {
  mTransports.Assign(aTransports);
}

nsresult AuthenticatorAttestationResponse::GetAuthenticatorDataBytes(
    nsTArray<uint8_t>& aAuthenticatorData) {
  if (!mAttestationObjectParsed) {
    nsresult rv = authrs_webauthn_att_obj_constructor(
        mAttestationObject, /* anonymize */ false,
        getter_AddRefs(mAttestationObjectParsed));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  nsresult rv =
      mAttestationObjectParsed->GetAuthenticatorData(aAuthenticatorData);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

void AuthenticatorAttestationResponse::GetAuthenticatorData(
    JSContext* aCx, JS::MutableHandle<JSObject*> aValue, ErrorResult& aRv) {
  nsTArray<uint8_t> authenticatorData;
  nsresult rv = GetAuthenticatorDataBytes(authenticatorData);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  JS::Heap<JSObject*> buffer(ArrayBuffer::Create(aCx, authenticatorData, aRv));
  if (aRv.Failed()) {
    return;
  }
  aValue.set(buffer);
}

nsresult AuthenticatorAttestationResponse::GetPublicKeyBytes(
    nsTArray<uint8_t>& aPublicKeyBytes) {
  if (!mAttestationObjectParsed) {
    nsresult rv = authrs_webauthn_att_obj_constructor(
        mAttestationObject, /* anonymize */ false,
        getter_AddRefs(mAttestationObjectParsed));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  nsresult rv = mAttestationObjectParsed->GetPublicKey(aPublicKeyBytes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

void AuthenticatorAttestationResponse::GetPublicKey(
    JSContext* aCx, JS::MutableHandle<JSObject*> aValue, ErrorResult& aRv) {
  nsTArray<uint8_t> publicKey;
  nsresult rv = GetPublicKeyBytes(publicKey);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      aValue.set(nullptr);
    } else {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    }
    return;
  }

  JS::Heap<JSObject*> buffer(ArrayBuffer::Create(aCx, publicKey, aRv));
  if (aRv.Failed()) {
    return;
  }
  aValue.set(buffer);
}

COSEAlgorithmIdentifier AuthenticatorAttestationResponse::GetPublicKeyAlgorithm(
    ErrorResult& aRv) {
  if (!mAttestationObjectParsed) {
    nsresult rv = authrs_webauthn_att_obj_constructor(
        mAttestationObject, false, getter_AddRefs(mAttestationObjectParsed));
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return 0;
    }
  }

  COSEAlgorithmIdentifier alg;
  mAttestationObjectParsed->GetPublicKeyAlgorithm(&alg);
  return alg;
}

void AuthenticatorAttestationResponse::ToJSON(
    AuthenticatorAttestationResponseJSON& aJSON, ErrorResult& aError) {
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

  nsTArray<uint8_t> authenticatorData;
  rv = GetAuthenticatorDataBytes(authenticatorData);
  if (NS_FAILED(rv)) {
    aError.ThrowUnknownError("could not get authenticatorData");
    return;
  }
  nsAutoCString authenticatorDataBase64;
  rv = Base64URLEncode(authenticatorData.Length(), authenticatorData.Elements(),
                       mozilla::Base64URLEncodePaddingPolicy::Omit,
                       authenticatorDataBase64);
  if (NS_FAILED(rv)) {
    aError.ThrowDataError("authenticatorData too long");
    return;
  }
  aJSON.mAuthenticatorData.Assign(
      NS_ConvertUTF8toUTF16(authenticatorDataBase64));

  if (!aJSON.mTransports.Assign(mTransports, mozilla::fallible)) {
    aError.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  nsTArray<uint8_t> publicKeyBytes;
  rv = GetPublicKeyBytes(publicKeyBytes);
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString publicKeyBytesBase64;
    rv = Base64URLEncode(publicKeyBytes.Length(), publicKeyBytes.Elements(),
                         mozilla::Base64URLEncodePaddingPolicy::Omit,
                         publicKeyBytesBase64);
    if (NS_FAILED(rv)) {
      aError.ThrowDataError("publicKey too long");
      return;
    }
    aJSON.mPublicKey.Construct(NS_ConvertUTF8toUTF16(publicKeyBytesBase64));
  } else if (rv != NS_ERROR_NOT_AVAILABLE) {
    aError.ThrowUnknownError("could not get publicKey");
    return;
  }

  COSEAlgorithmIdentifier publicKeyAlgorithm = GetPublicKeyAlgorithm(aError);
  if (aError.Failed()) {
    aError.ThrowUnknownError("could not get publicKeyAlgorithm");
    return;
  }
  aJSON.mPublicKeyAlgorithm = publicKeyAlgorithm;

  nsAutoCString attestationObjectBase64;
  rv = Base64URLEncode(
      mAttestationObject.Length(), mAttestationObject.Elements(),
      mozilla::Base64URLEncodePaddingPolicy::Omit, attestationObjectBase64);
  if (NS_FAILED(rv)) {
    aError.ThrowDataError("attestationObject too long");
    return;
  }
  aJSON.mAttestationObject.Assign(
      NS_ConvertUTF8toUTF16(attestationObjectBase64));
}

}  // namespace mozilla::dom

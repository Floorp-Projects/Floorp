/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AuthrsBridge_ffi.h"
#include "mozilla/dom/WebAuthenticationBinding.h"
#include "mozilla/dom/AuthenticatorAttestationResponse.h"

#include "mozilla/HoldDropJSObjects.h"

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
    mAttestationObjectCachedObj = ArrayBuffer::Create(
        aCx, mAttestationObject.Length(), mAttestationObject.Elements());
    if (!mAttestationObjectCachedObj) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }
  aValue.set(mAttestationObjectCachedObj);
}

nsresult AuthenticatorAttestationResponse::SetAttestationObject(
    const nsTArray<uint8_t>& aBuffer) {
  if (!mAttestationObject.Assign(aBuffer, mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void AuthenticatorAttestationResponse::GetAuthenticatorData(
    JSContext* aCx, JS::MutableHandle<JSObject*> aValue, ErrorResult& aRv) {
  if (!mAttestationObjectParsed) {
    nsresult rv = authrs_webauthn_att_obj_constructor(
        mAttestationObject, /* anonymize */ false,
        getter_AddRefs(mAttestationObjectParsed));
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
  }

  nsTArray<uint8_t> authenticatorData;
  nsresult rv =
      mAttestationObjectParsed->GetAuthenticatorData(authenticatorData);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  JS::Heap<JSObject*> buffer(ArrayBuffer::Create(
      aCx, authenticatorData.Length(), authenticatorData.Elements()));
  if (!buffer) {
    aRv.NoteJSContextException(aCx);
    return;
  }
  aValue.set(buffer);
}

void AuthenticatorAttestationResponse::GetPublicKey(
    JSContext* aCx, JS::MutableHandle<JSObject*> aValue, ErrorResult& aRv) {
  if (!mAttestationObjectParsed) {
    nsresult rv = authrs_webauthn_att_obj_constructor(
        mAttestationObject, false, getter_AddRefs(mAttestationObjectParsed));
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
  }

  nsTArray<uint8_t> publicKey;
  nsresult rv = mAttestationObjectParsed->GetPublicKey(publicKey);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      aValue.set(nullptr);
    } else {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    }
    return;
  }

  JS::Heap<JSObject*> buffer(
      ArrayBuffer::Create(aCx, publicKey.Length(), publicKey.Elements()));
  if (!buffer) {
    aRv.NoteJSContextException(aCx);
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

}  // namespace mozilla::dom

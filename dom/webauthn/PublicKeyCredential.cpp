/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PublicKeyCredential.h"
#include "mozilla/dom/WebAuthenticationBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/AuthenticatorResponse.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/Preferences.h"

#ifdef OS_WIN
#  include "WinWebAuthnManager.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/java/WebAuthnTokenManagerWrappers.h"
#endif

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(PublicKeyCredential)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PublicKeyCredential, Credential)
  tmp->mRawIdCachedObj = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(PublicKeyCredential, Credential)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mRawIdCachedObj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PublicKeyCredential,
                                                  Credential)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResponse)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(PublicKeyCredential, Credential)
NS_IMPL_RELEASE_INHERITED(PublicKeyCredential, Credential)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PublicKeyCredential)
NS_INTERFACE_MAP_END_INHERITING(Credential)

PublicKeyCredential::PublicKeyCredential(nsPIDOMWindowInner* aParent)
    : Credential(aParent), mRawIdCachedObj(nullptr) {
  mozilla::HoldJSObjects(this);
}

PublicKeyCredential::~PublicKeyCredential() { mozilla::DropJSObjects(this); }

JSObject* PublicKeyCredential::WrapObject(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return PublicKeyCredential_Binding::Wrap(aCx, this, aGivenProto);
}

void PublicKeyCredential::GetRawId(JSContext* aCx,
                                   JS::MutableHandle<JSObject*> aValue,
                                   ErrorResult& aRv) {
  if (!mRawIdCachedObj) {
    mRawIdCachedObj = mRawId.ToArrayBuffer(aCx);
    if (!mRawIdCachedObj) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }
  aValue.set(mRawIdCachedObj);
}

already_AddRefed<AuthenticatorResponse> PublicKeyCredential::Response() const {
  RefPtr<AuthenticatorResponse> temp(mResponse);
  return temp.forget();
}

nsresult PublicKeyCredential::SetRawId(CryptoBuffer& aBuffer) {
  if (NS_WARN_IF(!mRawId.Assign(aBuffer))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

void PublicKeyCredential::SetResponse(RefPtr<AuthenticatorResponse> aResponse) {
  mResponse = aResponse;
}

/* static */
already_AddRefed<Promise>
PublicKeyCredential::IsUserVerifyingPlatformAuthenticatorAvailable(
    GlobalObject& aGlobal, ErrorResult& aError) {
  RefPtr<Promise> promise =
      Promise::Create(xpc::CurrentNativeGlobal(aGlobal.Context()), aError);
  if (aError.Failed()) {
    return nullptr;
  }

// https://w3c.github.io/webauthn/#isUserVerifyingPlatformAuthenticatorAvailable
//
// If on latest windows, call system APIs, otherwise return false, as we don't
// have other UVPAAs available at this time.
#ifdef OS_WIN

  if (WinWebAuthnManager::IsUserVerifyingPlatformAuthenticatorAvailable()) {
    promise->MaybeResolve(true);
    return promise.forget();
  }

  promise->MaybeResolve(false);
#elif defined(MOZ_WIDGET_ANDROID)
  auto result = java::WebAuthnTokenManager::
      WebAuthnIsUserVerifyingPlatformAuthenticatorAvailable();
  auto geckoResult = java::GeckoResult::LocalRef(std::move(result));
  MozPromise<bool, bool, false>::FromGeckoResult(geckoResult)
      ->Then(GetMainThreadSerialEventTarget(), __func__,
             [promise](const MozPromise<bool, bool,
                                        false>::ResolveOrRejectValue& aValue) {
               if (aValue.IsResolve()) {
                 promise->MaybeResolve(aValue.ResolveValue());
               }
             });
#else
  promise->MaybeResolve(false);
#endif
  return promise.forget();
}

/* static */
already_AddRefed<Promise>
PublicKeyCredential::IsExternalCTAP2SecurityKeySupported(GlobalObject& aGlobal,
                                                         ErrorResult& aError) {
  RefPtr<Promise> promise =
      Promise::Create(xpc::CurrentNativeGlobal(aGlobal.Context()), aError);
  if (aError.Failed()) {
    return nullptr;
  }

#ifdef OS_WIN
  if (WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    promise->MaybeResolve(true);
  } else {
    promise->MaybeResolve(Preferences::GetBool("security.webauthn.ctap2"));
  }
#elif defined(MOZ_WIDGET_ANDROID)
  promise->MaybeResolve(false);
#else
  promise->MaybeResolve(Preferences::GetBool("security.webauthn.ctap2"));
#endif

  return promise.forget();
}

void PublicKeyCredential::GetClientExtensionResults(
    AuthenticationExtensionsClientOutputs& aResult) {
  aResult = mClientExtensionOutputs;
}

void PublicKeyCredential::SetClientExtensionResultAppId(bool aResult) {
  mClientExtensionOutputs.mAppid.Construct();
  mClientExtensionOutputs.mAppid.Value() = aResult;
}

void PublicKeyCredential::SetClientExtensionResultHmacSecret(
    bool aHmacCreateSecret) {
  mClientExtensionOutputs.mHmacCreateSecret.Construct();
  mClientExtensionOutputs.mHmacCreateSecret.Value() = aHmacCreateSecret;
}

}  // namespace mozilla::dom

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Base64.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/dom/AuthenticatorResponse.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PublicKeyCredential.h"
#include "mozilla/dom/WebAuthenticationBinding.h"
#include "nsCycleCollectionParticipant.h"

#ifdef XP_WIN
#  include "WinWebAuthnManager.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/MozPromise.h"
#  include "mozilla/java/GeckoResultNatives.h"
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAttestationResponse)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAssertionResponse)
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
    mRawIdCachedObj =
        ArrayBuffer::Create(aCx, mRawId.Length(), mRawId.Elements());
    if (!mRawIdCachedObj) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }
  aValue.set(mRawIdCachedObj);
}

already_AddRefed<AuthenticatorResponse> PublicKeyCredential::Response() const {
  if (mAttestationResponse) {
    return do_AddRef(mAttestationResponse);
  }
  if (mAssertionResponse) {
    return do_AddRef(mAssertionResponse);
  }
  return nullptr;
}

void PublicKeyCredential::SetRawId(const nsTArray<uint8_t>& aBuffer) {
  mRawId.Assign(aBuffer);
}

void PublicKeyCredential::SetAttestationResponse(
    const RefPtr<AuthenticatorAttestationResponse>& aAttestationResponse) {
  mAttestationResponse = aAttestationResponse;
}

void PublicKeyCredential::SetAssertionResponse(
    const RefPtr<AuthenticatorAssertionResponse>& aAssertionResponse) {
  mAssertionResponse = aAssertionResponse;
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
#ifdef XP_WIN

  if (WinWebAuthnManager::IsUserVerifyingPlatformAuthenticatorAvailable()) {
    promise->MaybeResolve(true);
    return promise.forget();
  }

  promise->MaybeResolve(false);
#elif defined(MOZ_WIDGET_ANDROID)
  if (StaticPrefs::
          security_webauthn_webauthn_enable_android_fido2_residentkey()) {
    auto result = java::WebAuthnTokenManager::
        WebAuthnIsUserVerifyingPlatformAuthenticatorAvailable();
    auto geckoResult = java::GeckoResult::LocalRef(std::move(result));
    MozPromise<bool, bool, false>::FromGeckoResult(geckoResult)
        ->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [promise](const MozPromise<bool, bool, false>::ResolveOrRejectValue&
                          aValue) {
              if (aValue.IsResolve()) {
                promise->MaybeResolve(aValue.ResolveValue());
              }
            });
  } else {
    promise->MaybeResolve(false);
  }
#else
  promise->MaybeResolve(false);
#endif
  return promise.forget();
}

/* static */
already_AddRefed<Promise> PublicKeyCredential::IsConditionalMediationAvailable(
    GlobalObject& aGlobal, ErrorResult& aError) {
  RefPtr<Promise> promise =
      Promise::Create(xpc::CurrentNativeGlobal(aGlobal.Context()), aError);
  if (aError.Failed()) {
    return nullptr;
  }
  // Support for conditional mediation will be added in Bug 1838932
  promise->MaybeResolve(false);
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

#ifdef XP_WIN
  if (WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    promise->MaybeResolve(true);
  } else {
    promise->MaybeResolve(StaticPrefs::security_webauthn_ctap2());
  }
#elif defined(MOZ_WIDGET_ANDROID)
  promise->MaybeResolve(false);
#else
  promise->MaybeResolve(StaticPrefs::security_webauthn_ctap2());
#endif

  return promise.forget();
}

void PublicKeyCredential::GetClientExtensionResults(
    AuthenticationExtensionsClientOutputs& aResult) {
  aResult = mClientExtensionOutputs;
}

void PublicKeyCredential::ToJSON(JSContext* aCx,
                                 JS::MutableHandle<JSObject*> aRetval,
                                 ErrorResult& aError) {
  JS::Rooted<JS::Value> value(aCx);
  if (mAttestationResponse) {
    RegistrationResponseJSON json;
    GetId(json.mId);
    GetId(json.mRawId);
    mAttestationResponse->ToJSON(json.mResponse, aError);
    if (aError.Failed()) {
      return;
    }
    // TODO(bug 1810851): authenticatorAttachment
    if (mClientExtensionOutputs.mHmacCreateSecret.WasPassed()) {
      json.mClientExtensionResults.mHmacCreateSecret.Construct(
          mClientExtensionOutputs.mHmacCreateSecret.Value());
    }
    json.mType.Assign(u"public-key"_ns);
    if (!ToJSValue(aCx, json, &value)) {
      aError.StealExceptionFromJSContext(aCx);
      return;
    }
  } else if (mAssertionResponse) {
    AuthenticationResponseJSON json;
    GetId(json.mId);
    GetId(json.mRawId);
    mAssertionResponse->ToJSON(json.mResponse, aError);
    if (aError.Failed()) {
      return;
    }
    // TODO(bug 1810851): authenticatorAttachment
    if (mClientExtensionOutputs.mAppid.WasPassed()) {
      json.mClientExtensionResults.mAppid.Construct(
          mClientExtensionOutputs.mAppid.Value());
    }
    json.mType.Assign(u"public-key"_ns);
    if (!ToJSValue(aCx, json, &value)) {
      aError.StealExceptionFromJSContext(aCx);
      return;
    }
  } else {
    MOZ_ASSERT_UNREACHABLE(
        "either mAttestationResponse or mAssertionResponse should be set");
  }
  JS::Rooted<JSObject*> result(aCx, &value.toObject());
  aRetval.set(result);
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

bool Base64DecodeToArrayBuffer(GlobalObject& aGlobal, const nsAString& aString,
                               ArrayBuffer& aArrayBuffer, ErrorResult& aRv) {
  JSContext* cx = aGlobal.Context();
  JS::Rooted<JSObject*> result(cx);
  Base64URLDecodeOptions options;
  options.mPadding = Base64URLDecodePadding::Ignore;
  mozilla::dom::ChromeUtils::Base64URLDecode(
      aGlobal, NS_ConvertUTF16toUTF8(aString), options, &result, aRv);
  if (aRv.Failed()) {
    return false;
  }
  return aArrayBuffer.Init(result);
}

void PublicKeyCredential::ParseCreationOptionsFromJSON(
    GlobalObject& aGlobal,
    const PublicKeyCredentialCreationOptionsJSON& aOptions,
    PublicKeyCredentialCreationOptions& aResult, ErrorResult& aRv) {
  if (aOptions.mRp.mId.WasPassed()) {
    aResult.mRp.mId.Construct(aOptions.mRp.mId.Value());
  }
  aResult.mRp.mName.Assign(aOptions.mRp.mName);

  aResult.mUser.mName.Assign(aOptions.mUser.mName);
  if (!Base64DecodeToArrayBuffer(aGlobal, aOptions.mUser.mId,
                                 aResult.mUser.mId.SetAsArrayBuffer(), aRv)) {
    aRv.ThrowEncodingError("could not decode user ID as urlsafe base64");
    return;
  }
  aResult.mUser.mDisplayName.Assign(aOptions.mUser.mDisplayName);

  if (!Base64DecodeToArrayBuffer(aGlobal, aOptions.mChallenge,
                                 aResult.mChallenge.SetAsArrayBuffer(), aRv)) {
    aRv.ThrowEncodingError("could not decode challenge as urlsafe base64");
    return;
  }

  aResult.mPubKeyCredParams = aOptions.mPubKeyCredParams;

  if (aOptions.mTimeout.WasPassed()) {
    aResult.mTimeout.Construct(aOptions.mTimeout.Value());
  }

  for (const auto& excludeCredentialJSON : aOptions.mExcludeCredentials) {
    PublicKeyCredentialDescriptor* excludeCredential =
        aResult.mExcludeCredentials.AppendElement(fallible);
    if (!excludeCredential) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    excludeCredential->mType = excludeCredentialJSON.mType;
    if (!Base64DecodeToArrayBuffer(aGlobal, excludeCredentialJSON.mId,
                                   excludeCredential->mId.SetAsArrayBuffer(),
                                   aRv)) {
      aRv.ThrowEncodingError(
          "could not decode excluded credential ID as urlsafe base64");
      return;
    }
    if (excludeCredentialJSON.mTransports.WasPassed()) {
      excludeCredential->mTransports.Construct(
          excludeCredentialJSON.mTransports.Value());
    }
  }

  if (aOptions.mAuthenticatorSelection.WasPassed()) {
    aResult.mAuthenticatorSelection = aOptions.mAuthenticatorSelection.Value();
  }

  aResult.mAttestation = aOptions.mAttestation;

  if (aOptions.mExtensions.WasPassed()) {
    if (aOptions.mExtensions.Value().mAppid.WasPassed()) {
      aResult.mExtensions.mAppid.Construct(
          aOptions.mExtensions.Value().mAppid.Value());
    }
    if (aOptions.mExtensions.Value().mHmacCreateSecret.WasPassed()) {
      aResult.mExtensions.mHmacCreateSecret.Construct(
          aOptions.mExtensions.Value().mHmacCreateSecret.Value());
    }
  }
}

void PublicKeyCredential::ParseRequestOptionsFromJSON(
    GlobalObject& aGlobal,
    const PublicKeyCredentialRequestOptionsJSON& aOptions,
    PublicKeyCredentialRequestOptions& aResult, ErrorResult& aRv) {
  if (!Base64DecodeToArrayBuffer(aGlobal, aOptions.mChallenge,
                                 aResult.mChallenge.SetAsArrayBuffer(), aRv)) {
    aRv.ThrowEncodingError("could not decode challenge as urlsafe base64");
    return;
  }

  if (aOptions.mTimeout.WasPassed()) {
    aResult.mTimeout.Construct(aOptions.mTimeout.Value());
  }

  if (aOptions.mRpId.WasPassed()) {
    aResult.mRpId.Construct(aOptions.mRpId.Value());
  }

  for (const auto& allowCredentialJSON : aOptions.mAllowCredentials) {
    PublicKeyCredentialDescriptor* allowCredential =
        aResult.mAllowCredentials.AppendElement(fallible);
    if (!allowCredential) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    allowCredential->mType = allowCredentialJSON.mType;
    if (!Base64DecodeToArrayBuffer(aGlobal, allowCredentialJSON.mId,
                                   allowCredential->mId.SetAsArrayBuffer(),
                                   aRv)) {
      aRv.ThrowEncodingError(
          "could not decode allowed credential ID as urlsafe base64");
      return;
    }
    if (allowCredentialJSON.mTransports.WasPassed()) {
      allowCredential->mTransports.Construct(
          allowCredentialJSON.mTransports.Value());
    }
  }

  aResult.mUserVerification = aOptions.mUserVerification;

  if (aOptions.mExtensions.WasPassed()) {
    if (aOptions.mExtensions.Value().mAppid.WasPassed()) {
      aResult.mExtensions.mAppid.Construct(
          aOptions.mExtensions.Value().mAppid.Value());
    }
    if (aOptions.mExtensions.Value().mHmacCreateSecret.WasPassed()) {
      aResult.mExtensions.mHmacCreateSecret.Construct(
          aOptions.mExtensions.Value().mHmacCreateSecret.Value());
    }
  }
}

}  // namespace mozilla::dom

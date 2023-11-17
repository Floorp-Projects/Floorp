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
#include "mozilla/dom/CredentialsContainer.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PublicKeyCredential.h"
#include "mozilla/dom/WebAuthenticationBinding.h"
#include "mozilla/dom/WebAuthnManager.h"
#include "nsCycleCollectionParticipant.h"

#ifdef XP_WIN
#  include "WinWebAuthnService.h"
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
    mRawIdCachedObj = ArrayBuffer::Create(aCx, mRawId, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
  aValue.set(mRawIdCachedObj);
}

void PublicKeyCredential::GetAuthenticatorAttachment(
    DOMString& aAuthenticatorAttachment) {
  if (mAuthenticatorAttachment.isSome()) {
    aAuthenticatorAttachment.SetKnownLiveString(mAuthenticatorAttachment.ref());
  } else {
    aAuthenticatorAttachment.SetNull();
  }
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

void PublicKeyCredential::SetAuthenticatorAttachment(
    const Maybe<nsString>& aAuthenticatorAttachment) {
  mAuthenticatorAttachment = aAuthenticatorAttachment;
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
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<WebAuthnManager> manager =
      window->Navigator()->Credentials()->GetWebAuthnManager();
  return manager->IsUVPAA(aGlobal, aError);
}

/* static */
already_AddRefed<Promise> PublicKeyCredential::IsConditionalMediationAvailable(
    GlobalObject& aGlobal, ErrorResult& aError) {
  RefPtr<Promise> promise =
      Promise::Create(xpc::CurrentNativeGlobal(aGlobal.Context()), aError);
  if (aError.Failed()) {
    return nullptr;
  }
#if defined(MOZ_WIDGET_ANDROID)
  promise->MaybeResolve(false);
#else
  promise->MaybeResolve(
      StaticPrefs::security_webauthn_enable_conditional_mediation());
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

#ifdef XP_WIN
  if (WinWebAuthnService::AreWebAuthNApisAvailable()) {
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
    if (mAuthenticatorAttachment.isSome()) {
      json.mAuthenticatorAttachment.Construct();
      json.mAuthenticatorAttachment.Value() = mAuthenticatorAttachment.ref();
    }
    if (mClientExtensionOutputs.mCredProps.WasPassed()) {
      json.mClientExtensionResults.mCredProps.Construct(
          mClientExtensionOutputs.mCredProps.Value());
    }
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
    if (mAuthenticatorAttachment.isSome()) {
      json.mAuthenticatorAttachment.Construct();
      json.mAuthenticatorAttachment.Value() = mAuthenticatorAttachment.ref();
    }
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

void PublicKeyCredential::SetClientExtensionResultCredPropsRk(bool aResult) {
  mClientExtensionOutputs.mCredProps.Construct();
  mClientExtensionOutputs.mCredProps.Value().mRk.Construct();
  mClientExtensionOutputs.mCredProps.Value().mRk.Value() = aResult;
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
    if (aOptions.mExtensions.Value().mCredProps.WasPassed()) {
      aResult.mExtensions.mCredProps.Construct(
          aOptions.mExtensions.Value().mCredProps.Value());
    }
    if (aOptions.mExtensions.Value().mHmacCreateSecret.WasPassed()) {
      aResult.mExtensions.mHmacCreateSecret.Construct(
          aOptions.mExtensions.Value().mHmacCreateSecret.Value());
    }
    if (aOptions.mExtensions.Value().mMinPinLength.WasPassed()) {
      aResult.mExtensions.mMinPinLength.Construct(
          aOptions.mExtensions.Value().mMinPinLength.Value());
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
    if (aOptions.mExtensions.Value().mCredProps.WasPassed()) {
      aResult.mExtensions.mCredProps.Construct(
          aOptions.mExtensions.Value().mCredProps.Value());
    }
    if (aOptions.mExtensions.Value().mHmacCreateSecret.WasPassed()) {
      aResult.mExtensions.mHmacCreateSecret.Construct(
          aOptions.mExtensions.Value().mHmacCreateSecret.Value());
    }
    if (aOptions.mExtensions.Value().mMinPinLength.WasPassed()) {
      aResult.mExtensions.mMinPinLength.Construct(
          aOptions.mExtensions.Value().mMinPinLength.Value());
    }
  }
}

}  // namespace mozilla::dom

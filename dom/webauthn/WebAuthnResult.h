/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnResult_h_
#define mozilla_dom_WebAuthnResult_h_

#include "nsIWebAuthnResult.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/java/WebAuthnTokenManagerNatives.h"
#endif

namespace mozilla::dom {

class WebAuthnRegisterResult final : public nsIWebAuthnRegisterResult {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBAUTHNREGISTERRESULT

  WebAuthnRegisterResult(const nsTArray<uint8_t>& aAttestationObject,
                         const nsCString& aClientDataJSON,
                         const nsTArray<uint8_t>& aCredentialId,
                         const nsTArray<nsString>& aTransports)
      : mClientDataJSON(aClientDataJSON), mCredPropsRk(Nothing()) {
    mAttestationObject.AppendElements(aAttestationObject);
    mCredentialId.AppendElements(aCredentialId);
    mTransports.AppendElements(aTransports);
  }

#ifdef MOZ_WIDGET_ANDROID
  explicit WebAuthnRegisterResult(
      const java::WebAuthnTokenManager::MakeCredentialResponse::LocalRef&
          aResponse) {
    mAttestationObject.AppendElements(
        reinterpret_cast<uint8_t*>(
            aResponse->AttestationObject()->GetElements().Elements()),
        aResponse->AttestationObject()->Length());
    mClientDataJSON.Assign(
        reinterpret_cast<const char*>(
            aResponse->ClientDataJson()->GetElements().Elements()),
        aResponse->ClientDataJson()->Length());
    mCredentialId.AppendElements(
        reinterpret_cast<uint8_t*>(
            aResponse->KeyHandle()->GetElements().Elements()),
        aResponse->KeyHandle()->Length());
    auto transports = aResponse->Transports();
    for (size_t i = 0; i < transports->Length(); i++) {
      mTransports.AppendElement(
          jni::String::LocalRef(transports->GetElement(i))->ToString());
    }
  }
#endif

 private:
  ~WebAuthnRegisterResult() = default;

  nsTArray<uint8_t> mAttestationObject;
  nsTArray<uint8_t> mCredentialId;
  nsTArray<nsString> mTransports;
  nsCString mClientDataJSON;
  Maybe<bool> mCredPropsRk;
};

class WebAuthnSignResult final : public nsIWebAuthnSignResult {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBAUTHNSIGNRESULT

  WebAuthnSignResult(const nsTArray<uint8_t>& aAuthenticatorData,
                     const nsCString& aClientDataJSON,
                     const nsTArray<uint8_t>& aCredentialId,
                     const nsTArray<uint8_t>& aSignature,
                     const nsTArray<uint8_t>& aUserHandle)
      : mClientDataJSON(aClientDataJSON) {
    mAuthenticatorData.AppendElements(aAuthenticatorData);
    mCredentialId.AppendElements(aCredentialId);
    mSignature.AppendElements(aSignature);
    mUserHandle.AppendElements(aUserHandle);
  }

#ifdef MOZ_WIDGET_ANDROID
  explicit WebAuthnSignResult(
      const java::WebAuthnTokenManager::GetAssertionResponse::LocalRef&
          aResponse) {
    mAuthenticatorData.AppendElements(
        reinterpret_cast<uint8_t*>(
            aResponse->AuthData()->GetElements().Elements()),
        aResponse->AuthData()->Length());
    mClientDataJSON.Assign(
        reinterpret_cast<const char*>(
            aResponse->ClientDataJson()->GetElements().Elements()),
        aResponse->ClientDataJson()->Length());
    mCredentialId.AppendElements(
        reinterpret_cast<uint8_t*>(
            aResponse->KeyHandle()->GetElements().Elements()),
        aResponse->KeyHandle()->Length());
    mSignature.AppendElements(
        reinterpret_cast<uint8_t*>(
            aResponse->Signature()->GetElements().Elements()),
        aResponse->Signature()->Length());
    mUserHandle.AppendElements(
        reinterpret_cast<uint8_t*>(
            aResponse->UserHandle()->GetElements().Elements()),
        aResponse->UserHandle()->Length());
  }
#endif

 private:
  ~WebAuthnSignResult() = default;

  nsTArray<uint8_t> mAuthenticatorData;
  nsCString mClientDataJSON;
  nsTArray<uint8_t> mCredentialId;
  nsTArray<uint8_t> mSignature;
  nsTArray<uint8_t> mUserHandle;
};

}  // namespace mozilla::dom
#endif  // mozilla_dom_WebAuthnResult_h

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnResult_h_
#define mozilla_dom_WebAuthnResult_h_

#include "nsIWebAuthnResult.h"
#include "nsString.h"
#include "nsTArray.h"

#include "mozilla/Maybe.h"
#include "nsString.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/java/WebAuthnTokenManagerNatives.h"
#endif

#ifdef XP_WIN
#  include <windows.h>
#  include "mozilla/dom/PWebAuthnTransactionParent.h"
#  include "winwebauthn/webauthn.h"
#endif

namespace mozilla::dom {

class WebAuthnRegisterResult final : public nsIWebAuthnRegisterResult {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBAUTHNREGISTERRESULT

  WebAuthnRegisterResult(const nsTArray<uint8_t>& aAttestationObject,
                         const Maybe<nsCString>& aClientDataJSON,
                         const nsTArray<uint8_t>& aCredentialId,
                         const nsTArray<nsString>& aTransports,
                         const Maybe<nsString>& aAuthenticatorAttachment)
      : mClientDataJSON(aClientDataJSON),
        mCredPropsRk(Nothing()),
        mAuthenticatorAttachment(aAuthenticatorAttachment) {
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
    mClientDataJSON = Some(nsAutoCString(
        reinterpret_cast<const char*>(
            aResponse->ClientDataJson()->GetElements().Elements()),
        aResponse->ClientDataJson()->Length()));
    mCredentialId.AppendElements(
        reinterpret_cast<uint8_t*>(
            aResponse->KeyHandle()->GetElements().Elements()),
        aResponse->KeyHandle()->Length());
    auto transports = aResponse->Transports();
    for (size_t i = 0; i < transports->Length(); i++) {
      mTransports.AppendElement(
          jni::String::LocalRef(transports->GetElement(i))->ToString());
    }
    // authenticator attachment is not available on Android
    mAuthenticatorAttachment = Nothing();
  }
#endif

#ifdef XP_WIN
  WebAuthnRegisterResult(nsCString& aClientDataJSON,
                         PCWEBAUTHN_CREDENTIAL_ATTESTATION aResponse)
      : mClientDataJSON(Some(aClientDataJSON)) {
    mCredentialId.AppendElements(aResponse->pbCredentialId,
                                 aResponse->cbCredentialId);

    mAttestationObject.AppendElements(aResponse->pbAttestationObject,
                                      aResponse->cbAttestationObject);

    nsTArray<WebAuthnExtensionResult> extensions;
    if (aResponse->dwVersion >= WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_2) {
      PCWEBAUTHN_EXTENSIONS pExtensionList = &aResponse->Extensions;
      if (pExtensionList->cExtensions != 0 &&
          pExtensionList->pExtensions != NULL) {
        for (DWORD dwIndex = 0; dwIndex < pExtensionList->cExtensions;
             dwIndex++) {
          PWEBAUTHN_EXTENSION pExtension =
              &pExtensionList->pExtensions[dwIndex];
          if (pExtension->pwszExtensionIdentifier &&
              (0 == _wcsicmp(pExtension->pwszExtensionIdentifier,
                             WEBAUTHN_EXTENSIONS_IDENTIFIER_HMAC_SECRET)) &&
              pExtension->cbExtension == sizeof(BOOL)) {
            BOOL* pCredentialCreatedWithHmacSecret =
                (BOOL*)pExtension->pvExtension;
            if (*pCredentialCreatedWithHmacSecret) {
              mHmacCreateSecret = Some(true);
            }
          }
        }
      }
    }

    if (aResponse->dwVersion >= WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_3) {
      if (aResponse->dwUsedTransport & WEBAUTHN_CTAP_TRANSPORT_USB) {
        mTransports.AppendElement(u"usb"_ns);
      }
      if (aResponse->dwUsedTransport & WEBAUTHN_CTAP_TRANSPORT_NFC) {
        mTransports.AppendElement(u"nfc"_ns);
      }
      if (aResponse->dwUsedTransport & WEBAUTHN_CTAP_TRANSPORT_BLE) {
        mTransports.AppendElement(u"ble"_ns);
      }
      if (aResponse->dwUsedTransport & WEBAUTHN_CTAP_TRANSPORT_INTERNAL) {
        mTransports.AppendElement(u"internal"_ns);
      }
    }
    // WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_5 corresponds to
    // WEBAUTHN_API_VERSION_6 which is where WEBAUTHN_CTAP_TRANSPORT_HYBRID was
    // defined.
    if (aResponse->dwVersion >= WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_5) {
      if (aResponse->dwUsedTransport & WEBAUTHN_CTAP_TRANSPORT_HYBRID) {
        mTransports.AppendElement(u"hybrid"_ns);
      }
    }

    if (aResponse->dwVersion >= WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_3) {
      if (aResponse->dwUsedTransport & WEBAUTHN_CTAP_TRANSPORT_INTERNAL) {
        mAuthenticatorAttachment = Some(u"platform"_ns);
      } else {
        mAuthenticatorAttachment = Some(u"cross-platform"_ns);
      }
    }
  }
#endif

  nsresult Anonymize();

 private:
  ~WebAuthnRegisterResult() = default;

  nsTArray<uint8_t> mAttestationObject;
  nsTArray<uint8_t> mCredentialId;
  nsTArray<nsString> mTransports;
  Maybe<nsCString> mClientDataJSON;
  Maybe<bool> mCredPropsRk;
  Maybe<bool> mHmacCreateSecret;
  Maybe<nsString> mAuthenticatorAttachment;
};

class WebAuthnSignResult final : public nsIWebAuthnSignResult {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBAUTHNSIGNRESULT

  WebAuthnSignResult(const nsTArray<uint8_t>& aAuthenticatorData,
                     const Maybe<nsCString>& aClientDataJSON,
                     const nsTArray<uint8_t>& aCredentialId,
                     const nsTArray<uint8_t>& aSignature,
                     const nsTArray<uint8_t>& aUserHandle,
                     const Maybe<nsString>& aAuthenticatorAttachment)
      : mClientDataJSON(aClientDataJSON),
        mAuthenticatorAttachment(aAuthenticatorAttachment) {
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
    mClientDataJSON = Some(nsAutoCString(
        reinterpret_cast<const char*>(
            aResponse->ClientDataJson()->GetElements().Elements()),
        aResponse->ClientDataJson()->Length()));
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
    // authenticator attachment is not available on Android
    mAuthenticatorAttachment = Nothing();
  }
#endif

#ifdef XP_WIN
  WebAuthnSignResult(nsCString& aClientDataJSON, PCWEBAUTHN_ASSERTION aResponse)
      : mClientDataJSON(Some(aClientDataJSON)) {
    mSignature.AppendElements(aResponse->pbSignature, aResponse->cbSignature);

    mCredentialId.AppendElements(aResponse->Credential.pbId,
                                 aResponse->Credential.cbId);

    mUserHandle.AppendElements(aResponse->pbUserId, aResponse->cbUserId);

    mAuthenticatorData.AppendElements(aResponse->pbAuthenticatorData,
                                      aResponse->cbAuthenticatorData);

    mAuthenticatorAttachment = Nothing();  // not available
  }
#endif

 private:
  ~WebAuthnSignResult() = default;

  nsTArray<uint8_t> mAuthenticatorData;
  Maybe<nsCString> mClientDataJSON;
  nsTArray<uint8_t> mCredentialId;
  nsTArray<uint8_t> mSignature;
  nsTArray<uint8_t> mUserHandle;
  Maybe<nsString> mAuthenticatorAttachment;
  Maybe<bool> mUsedAppId;
};

}  // namespace mozilla::dom
#endif  // mozilla_dom_WebAuthnResult_h

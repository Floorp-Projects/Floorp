/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/dom/PWebAuthnTransactionParent.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Unused.h"
#include "nsIWebAuthnController.h"
#include "nsTextFormatter.h"
#include "nsWindowsHelpers.h"
#include "AuthrsBridge_ffi.h"
#include "WebAuthnEnumStrings.h"
#include "WebAuthnTransportIdentifiers.h"
#include "winwebauthn/webauthn.h"
#include "WinWebAuthnManager.h"

namespace mozilla::dom {

namespace {
static mozilla::LazyLogModule gWinWebAuthnManagerLog("winwebauthnkeymanager");
StaticAutoPtr<WinWebAuthnManager> gWinWebAuthnManager;
static HMODULE gWinWebAuthnModule = 0;

static decltype(WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable)*
    gWinWebauthnIsUVPAA = nullptr;
static decltype(WebAuthNAuthenticatorMakeCredential)*
    gWinWebauthnMakeCredential = nullptr;
static decltype(WebAuthNFreeCredentialAttestation)*
    gWinWebauthnFreeCredentialAttestation = nullptr;
static decltype(WebAuthNAuthenticatorGetAssertion)* gWinWebauthnGetAssertion =
    nullptr;
static decltype(WebAuthNFreeAssertion)* gWinWebauthnFreeAssertion = nullptr;
static decltype(WebAuthNGetCancellationId)* gWinWebauthnGetCancellationId =
    nullptr;
static decltype(WebAuthNCancelCurrentOperation)*
    gWinWebauthnCancelCurrentOperation = nullptr;
static decltype(WebAuthNGetErrorName)* gWinWebauthnGetErrorName = nullptr;
static decltype(WebAuthNGetApiVersionNumber)* gWinWebauthnGetApiVersionNumber =
    nullptr;

}  // namespace

/***********************************************************************
 * WinWebAuthnManager Implementation
 **********************************************************************/

constexpr uint32_t kMinWinWebAuthNApiVersion = WEBAUTHN_API_VERSION_1;

WinWebAuthnManager::WinWebAuthnManager() {
  // Create on the main thread to make sure ClearOnShutdown() works.
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gWinWebAuthnModule);

  gWinWebAuthnModule = LoadLibrarySystem32(L"webauthn.dll");

  if (gWinWebAuthnModule) {
    gWinWebauthnIsUVPAA = reinterpret_cast<
        decltype(WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable)*>(
        GetProcAddress(
            gWinWebAuthnModule,
            "WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable"));
    gWinWebauthnMakeCredential =
        reinterpret_cast<decltype(WebAuthNAuthenticatorMakeCredential)*>(
            GetProcAddress(gWinWebAuthnModule,
                           "WebAuthNAuthenticatorMakeCredential"));
    gWinWebauthnFreeCredentialAttestation =
        reinterpret_cast<decltype(WebAuthNFreeCredentialAttestation)*>(
            GetProcAddress(gWinWebAuthnModule,
                           "WebAuthNFreeCredentialAttestation"));
    gWinWebauthnGetAssertion =
        reinterpret_cast<decltype(WebAuthNAuthenticatorGetAssertion)*>(
            GetProcAddress(gWinWebAuthnModule,
                           "WebAuthNAuthenticatorGetAssertion"));
    gWinWebauthnFreeAssertion =
        reinterpret_cast<decltype(WebAuthNFreeAssertion)*>(
            GetProcAddress(gWinWebAuthnModule, "WebAuthNFreeAssertion"));
    gWinWebauthnGetCancellationId =
        reinterpret_cast<decltype(WebAuthNGetCancellationId)*>(
            GetProcAddress(gWinWebAuthnModule, "WebAuthNGetCancellationId"));
    gWinWebauthnCancelCurrentOperation =
        reinterpret_cast<decltype(WebAuthNCancelCurrentOperation)*>(
            GetProcAddress(gWinWebAuthnModule,
                           "WebAuthNCancelCurrentOperation"));
    gWinWebauthnGetErrorName =
        reinterpret_cast<decltype(WebAuthNGetErrorName)*>(
            GetProcAddress(gWinWebAuthnModule, "WebAuthNGetErrorName"));
    gWinWebauthnGetApiVersionNumber =
        reinterpret_cast<decltype(WebAuthNGetApiVersionNumber)*>(
            GetProcAddress(gWinWebAuthnModule, "WebAuthNGetApiVersionNumber"));

    if (gWinWebauthnIsUVPAA && gWinWebauthnMakeCredential &&
        gWinWebauthnFreeCredentialAttestation && gWinWebauthnGetAssertion &&
        gWinWebauthnFreeAssertion && gWinWebauthnGetCancellationId &&
        gWinWebauthnCancelCurrentOperation && gWinWebauthnGetErrorName &&
        gWinWebauthnGetApiVersionNumber) {
      mWinWebAuthNApiVersion = gWinWebauthnGetApiVersionNumber();
    }
  }
}

WinWebAuthnManager::~WinWebAuthnManager() {
  if (gWinWebAuthnModule) {
    FreeLibrary(gWinWebAuthnModule);
  }
  gWinWebAuthnModule = 0;
}

// static
void WinWebAuthnManager::Initialize() {
  if (!gWinWebAuthnManager) {
    gWinWebAuthnManager = new WinWebAuthnManager();
    ClearOnShutdown(&gWinWebAuthnManager);
  }
}

// static
WinWebAuthnManager* WinWebAuthnManager::Get() {
  MOZ_ASSERT(gWinWebAuthnManager);
  return gWinWebAuthnManager;
}

uint32_t WinWebAuthnManager::GetWebAuthNApiVersion() {
  return mWinWebAuthNApiVersion;
}

// static
bool WinWebAuthnManager::AreWebAuthNApisAvailable() {
  WinWebAuthnManager* mgr = WinWebAuthnManager::Get();
  return mgr->GetWebAuthNApiVersion() >= kMinWinWebAuthNApiVersion;
}

bool WinWebAuthnManager::
    IsUserVerifyingPlatformAuthenticatorAvailableInternal() {
  BOOL isUVPAA = FALSE;
  return (gWinWebauthnIsUVPAA(&isUVPAA) == S_OK && isUVPAA == TRUE);
}

// static
bool WinWebAuthnManager::IsUserVerifyingPlatformAuthenticatorAvailable() {
  if (WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    return WinWebAuthnManager::Get()
        ->IsUserVerifyingPlatformAuthenticatorAvailableInternal();
  }
  return false;
}

void WinWebAuthnManager::AbortTransaction(const uint64_t& aTransactionId,
                                          const nsresult& aError) {
  Unused << mTransactionParent->SendAbort(aTransactionId, aError);
  ClearTransaction();
}

void WinWebAuthnManager::MaybeClearTransaction(
    PWebAuthnTransactionParent* aParent) {
  // Only clear if we've been requested to do so by our current transaction
  // parent.
  if (mTransactionParent == aParent) {
    ClearTransaction();
  }
}

void WinWebAuthnManager::ClearTransaction() { mTransactionParent = nullptr; }

void WinWebAuthnManager::Register(
    PWebAuthnTransactionParent* aTransactionParent,
    const uint64_t& aTransactionId, const WebAuthnMakeCredentialInfo& aInfo) {
  MOZ_LOG(gWinWebAuthnManagerLog, LogLevel::Debug, ("WinWebAuthNRegister"));

  ClearTransaction();
  mTransactionParent = aTransactionParent;

  WEBAUTHN_EXTENSION rgExtension[1] = {};
  DWORD cExtensions = 0;
  BOOL HmacCreateSecret = FALSE;

  // RP Information
  WEBAUTHN_RP_ENTITY_INFORMATION rpInfo = {
      WEBAUTHN_RP_ENTITY_INFORMATION_CURRENT_VERSION, aInfo.RpId().get(),
      nullptr, nullptr};

  // User Information
  WEBAUTHN_USER_ENTITY_INFORMATION userInfo = {
      WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION,
      0,
      nullptr,
      nullptr,
      nullptr,
      nullptr};

  // Client Data
  WEBAUTHN_CLIENT_DATA WebAuthNClientData = {
      WEBAUTHN_CLIENT_DATA_CURRENT_VERSION,
      (DWORD)aInfo.ClientDataJSON().Length(),
      (BYTE*)(aInfo.ClientDataJSON().get()), WEBAUTHN_HASH_ALGORITHM_SHA_256};

  // Algorithms
  nsTArray<WEBAUTHN_COSE_CREDENTIAL_PARAMETER> coseParams;

  // User Verification Requirement
  DWORD winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_ANY;

  // Attachment
  DWORD winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;

  // Resident Key
  BOOL winRequireResidentKey = FALSE;
  BOOL winPreferResidentKey = FALSE;

  // AttestationConveyance
  DWORD winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_ANY;

  rpInfo.pwszName = aInfo.Rp().Name().get();
  rpInfo.pwszIcon = nullptr;

  userInfo.cbId = static_cast<DWORD>(aInfo.User().Id().Length());
  userInfo.pbId = const_cast<unsigned char*>(aInfo.User().Id().Elements());
  userInfo.pwszName = aInfo.User().Name().get();
  userInfo.pwszIcon = nullptr;
  userInfo.pwszDisplayName = aInfo.User().DisplayName().get();

  for (const auto& coseAlg : aInfo.coseAlgs()) {
    WEBAUTHN_COSE_CREDENTIAL_PARAMETER coseAlgorithm = {
        WEBAUTHN_COSE_CREDENTIAL_PARAMETER_CURRENT_VERSION,
        WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY, coseAlg.alg()};
    coseParams.AppendElement(coseAlgorithm);
  }

  const auto& sel = aInfo.AuthenticatorSelection();

  const nsString& userVerificationRequirement =
      sel.userVerificationRequirement();
  // This mapping needs to be reviewed if values are added to the
  // UserVerificationRequirement enum.
  static_assert(MOZ_WEBAUTHN_ENUM_STRINGS_VERSION == 2);
  if (userVerificationRequirement.EqualsLiteral(
          MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED)) {
    winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
  } else if (userVerificationRequirement.EqualsLiteral(
                 MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED)) {
    winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED;
  } else if (userVerificationRequirement.EqualsLiteral(
                 MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_DISCOURAGED)) {
    winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED;
  } else {
    winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_ANY;
  }

  if (sel.authenticatorAttachment().isSome()) {
    const nsString& authenticatorAttachment =
        sel.authenticatorAttachment().value();
    // This mapping needs to be reviewed if values are added to the
    // AuthenticatorAttachement enum.
    static_assert(MOZ_WEBAUTHN_ENUM_STRINGS_VERSION == 2);
    if (authenticatorAttachment.EqualsLiteral(
            MOZ_WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM)) {
      winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM;
    } else if (authenticatorAttachment.EqualsLiteral(
                   MOZ_WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM)) {
      winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM;
    } else {
      winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;
    }
  }

  const nsString& residentKey = sel.residentKey();
  // This mapping needs to be reviewed if values are added to the
  // ResidentKeyRequirement enum.
  static_assert(MOZ_WEBAUTHN_ENUM_STRINGS_VERSION == 2);
  if (residentKey.EqualsLiteral(
          MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_REQUIRED)) {
    winRequireResidentKey = TRUE;
    winPreferResidentKey = TRUE;
  } else if (residentKey.EqualsLiteral(
                 MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_PREFERRED)) {
    winRequireResidentKey = FALSE;
    winPreferResidentKey = TRUE;
  } else if (residentKey.EqualsLiteral(
                 MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_DISCOURAGED)) {
    winRequireResidentKey = FALSE;
    winPreferResidentKey = FALSE;
  } else {
    // WebAuthnManager::MakeCredential is supposed to assign one of the above
    // values, so this shouldn't happen.
    MOZ_ASSERT_UNREACHABLE();
    MaybeAbortRegister(aTransactionId, NS_ERROR_DOM_UNKNOWN_ERR);
    return;
  }

  // AttestationConveyance
  const nsString& attestation = aInfo.attestationConveyancePreference();
  // This mapping needs to be reviewed if values are added to the
  // AttestationConveyancePreference enum.
  static_assert(MOZ_WEBAUTHN_ENUM_STRINGS_VERSION == 2);
  if (attestation.EqualsLiteral(
          MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE)) {
    winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE;
  } else if (attestation.EqualsLiteral(
                 MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_INDIRECT)) {
    winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_INDIRECT;
  } else if (attestation.EqualsLiteral(
                 MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT)) {
    winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT;
  } else {
    winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_ANY;
  }

  if (aInfo.Extensions().Length() >
      (int)(sizeof(rgExtension) / sizeof(rgExtension[0]))) {
    nsresult aError = NS_ERROR_DOM_INVALID_STATE_ERR;
    MaybeAbortRegister(aTransactionId, aError);
    return;
  }
  for (const WebAuthnExtension& ext : aInfo.Extensions()) {
    if (ext.type() == WebAuthnExtension::TWebAuthnExtensionHmacSecret) {
      HmacCreateSecret =
          ext.get_WebAuthnExtensionHmacSecret().hmacCreateSecret() == true;
      if (HmacCreateSecret) {
        rgExtension[cExtensions].pwszExtensionIdentifier =
            WEBAUTHN_EXTENSIONS_IDENTIFIER_HMAC_SECRET;
        rgExtension[cExtensions].cbExtension = sizeof(BOOL);
        rgExtension[cExtensions].pvExtension = &HmacCreateSecret;
        cExtensions++;
      }
    }
  }

  WEBAUTHN_COSE_CREDENTIAL_PARAMETERS WebAuthNCredentialParameters = {
      static_cast<DWORD>(coseParams.Length()), coseParams.Elements()};

  // Exclude Credentials
  nsTArray<WEBAUTHN_CREDENTIAL_EX> excludeCredentials;
  WEBAUTHN_CREDENTIAL_EX* pExcludeCredentials = nullptr;
  nsTArray<WEBAUTHN_CREDENTIAL_EX*> excludeCredentialsPtrs;
  WEBAUTHN_CREDENTIAL_LIST excludeCredentialList = {0};
  WEBAUTHN_CREDENTIAL_LIST* pExcludeCredentialList = nullptr;

  for (auto& cred : aInfo.ExcludeList()) {
    uint8_t transports = cred.transports();
    DWORD winTransports = 0;
    if (transports & MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_USB) {
      winTransports |= WEBAUTHN_CTAP_TRANSPORT_USB;
    }
    if (transports & MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_NFC) {
      winTransports |= WEBAUTHN_CTAP_TRANSPORT_NFC;
    }
    if (transports & MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_BLE) {
      winTransports |= WEBAUTHN_CTAP_TRANSPORT_BLE;
    }
    if (transports & MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_INTERNAL) {
      winTransports |= WEBAUTHN_CTAP_TRANSPORT_INTERNAL;
    }

    WEBAUTHN_CREDENTIAL_EX credential = {
        WEBAUTHN_CREDENTIAL_EX_CURRENT_VERSION,
        static_cast<DWORD>(cred.id().Length()), (PBYTE)(cred.id().Elements()),
        WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY, winTransports};
    excludeCredentials.AppendElement(credential);
  }

  if (!excludeCredentials.IsEmpty()) {
    pExcludeCredentials = excludeCredentials.Elements();
    for (DWORD i = 0; i < excludeCredentials.Length(); i++) {
      excludeCredentialsPtrs.AppendElement(&pExcludeCredentials[i]);
    }
    excludeCredentialList.cCredentials = excludeCredentials.Length();
    excludeCredentialList.ppCredentials = excludeCredentialsPtrs.Elements();
    pExcludeCredentialList = &excludeCredentialList;
  }

  // MakeCredentialOptions
  WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS WebAuthNCredentialOptions = {
      WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_4,
      aInfo.TimeoutMS(),
      {0, NULL},
      {0, NULL},
      winAttachment,
      winRequireResidentKey,
      winUserVerificationReq,
      winAttestation,
      0,     // Flags
      NULL,  // CancellationId
      pExcludeCredentialList,
      WEBAUTHN_ENTERPRISE_ATTESTATION_NONE,
      WEBAUTHN_LARGE_BLOB_SUPPORT_NONE,
      winPreferResidentKey,  // PreferResidentKey
  };

  GUID cancellationId = {0};
  if (gWinWebauthnGetCancellationId(&cancellationId) == S_OK) {
    WebAuthNCredentialOptions.pCancellationId = &cancellationId;
    mCancellationIds.emplace(aTransactionId, &cancellationId);
  }

  if (cExtensions != 0) {
    WebAuthNCredentialOptions.Extensions.cExtensions = cExtensions;
    WebAuthNCredentialOptions.Extensions.pExtensions = rgExtension;
  }

  WEBAUTHN_CREDENTIAL_ATTESTATION* pWebAuthNCredentialAttestation = nullptr;

  // Bug 1518876: Get Window Handle from Content process for Windows WebAuthN
  // APIs
  HWND hWnd = GetForegroundWindow();

  HRESULT hr = gWinWebauthnMakeCredential(
      hWnd, &rpInfo, &userInfo, &WebAuthNCredentialParameters,
      &WebAuthNClientData, &WebAuthNCredentialOptions,
      &pWebAuthNCredentialAttestation);

  mCancellationIds.erase(aTransactionId);

  if (hr == S_OK) {
    nsTArray<uint8_t> credentialId;
    credentialId.AppendElements(pWebAuthNCredentialAttestation->pbCredentialId,
                                pWebAuthNCredentialAttestation->cbCredentialId);

    nsTArray<uint8_t> authenticatorData;

    authenticatorData.AppendElements(
        pWebAuthNCredentialAttestation->pbAuthenticatorData,
        pWebAuthNCredentialAttestation->cbAuthenticatorData);

    nsTArray<uint8_t> attObject;
    attObject.AppendElements(
        pWebAuthNCredentialAttestation->pbAttestationObject,
        pWebAuthNCredentialAttestation->cbAttestationObject);

    if (winAttestation == WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE) {
      // The anonymize flag in the nsIWebAuthnAttObj constructor causes the
      // attestation statement to be removed during deserialization. It also
      // causes the AAGUID to be zeroed out. If we can't deserialize the
      // existing attestation, then we can't ensure that it is anonymized, so we
      // act as though the user denied consent and we return NotAllowed.
      nsCOMPtr<nsIWebAuthnAttObj> anonymizedAttObj;
      nsresult rv = authrs_webauthn_att_obj_constructor(
          attObject,
          /* anonymize */ true, getter_AddRefs(anonymizedAttObj));
      if (NS_FAILED(rv)) {
        MaybeAbortRegister(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR);
        return;
      }
      attObject.Clear();
      rv = anonymizedAttObj->GetAttestationObject(attObject);
      if (NS_FAILED(rv)) {
        MaybeAbortRegister(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR);
        return;
      }
    }

    nsTArray<WebAuthnExtensionResult> extensions;
    if (pWebAuthNCredentialAttestation->dwVersion >=
        WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_2) {
      PCWEBAUTHN_EXTENSIONS pExtensionList =
          &pWebAuthNCredentialAttestation->Extensions;
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
              extensions.AppendElement(WebAuthnExtensionResultHmacSecret(true));
            }
          }
        }
      }
    }

    nsTArray<nsString> transports;
    if (pWebAuthNCredentialAttestation->dwVersion >=
        WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_3) {
      if (pWebAuthNCredentialAttestation->dwUsedTransport &
          WEBAUTHN_CTAP_TRANSPORT_USB) {
        transports.AppendElement(u"usb"_ns);
      }
      if (pWebAuthNCredentialAttestation->dwUsedTransport &
          WEBAUTHN_CTAP_TRANSPORT_NFC) {
        transports.AppendElement(u"nfc"_ns);
      }
      if (pWebAuthNCredentialAttestation->dwUsedTransport &
          WEBAUTHN_CTAP_TRANSPORT_BLE) {
        transports.AppendElement(u"ble"_ns);
      }
      if (pWebAuthNCredentialAttestation->dwUsedTransport &
          WEBAUTHN_CTAP_TRANSPORT_INTERNAL) {
        transports.AppendElement(u"internal"_ns);
      }
    }

    WebAuthnMakeCredentialResult result(aInfo.ClientDataJSON(), attObject,
                                        credentialId, transports, extensions);

    Unused << mTransactionParent->SendConfirmRegister(aTransactionId, result);
    ClearTransaction();
    gWinWebauthnFreeCredentialAttestation(pWebAuthNCredentialAttestation);

  } else {
    PCWSTR errorName = gWinWebauthnGetErrorName(hr);
    nsresult aError = NS_ERROR_DOM_ABORT_ERR;

    if (_wcsicmp(errorName, L"InvalidStateError") == 0) {
      aError = NS_ERROR_DOM_INVALID_STATE_ERR;
    } else if (_wcsicmp(errorName, L"ConstraintError") == 0 ||
               _wcsicmp(errorName, L"UnknownError") == 0) {
      aError = NS_ERROR_DOM_UNKNOWN_ERR;
    } else if (_wcsicmp(errorName, L"NotSupportedError") == 0) {
      aError = NS_ERROR_DOM_INVALID_STATE_ERR;
    } else if (_wcsicmp(errorName, L"NotAllowedError") == 0) {
      aError = NS_ERROR_DOM_NOT_ALLOWED_ERR;
    }

    MaybeAbortRegister(aTransactionId, aError);
  }
}

void WinWebAuthnManager::MaybeAbortRegister(const uint64_t& aTransactionId,
                                            const nsresult& aError) {
  AbortTransaction(aTransactionId, aError);
}

void WinWebAuthnManager::Sign(PWebAuthnTransactionParent* aTransactionParent,
                              const uint64_t& aTransactionId,
                              const WebAuthnGetAssertionInfo& aInfo) {
  MOZ_LOG(gWinWebAuthnManagerLog, LogLevel::Debug, ("WinWebAuthNSign"));

  ClearTransaction();
  mTransactionParent = aTransactionParent;

  // User Verification Requirement
  DWORD winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_ANY;

  // RPID
  PCWSTR rpID = nullptr;

  // Attachment
  DWORD winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;

  // AppId
  BOOL bU2fAppIdUsed = FALSE;
  BOOL* pbU2fAppIdUsed = nullptr;
  PCWSTR winAppIdentifier = nullptr;

  // Client Data
  WEBAUTHN_CLIENT_DATA WebAuthNClientData = {
      WEBAUTHN_CLIENT_DATA_CURRENT_VERSION,
      (DWORD)aInfo.ClientDataJSON().Length(),
      (BYTE*)(aInfo.ClientDataJSON().get()), WEBAUTHN_HASH_ALGORITHM_SHA_256};

  for (const WebAuthnExtension& ext : aInfo.Extensions()) {
    if (ext.type() == WebAuthnExtension::TWebAuthnExtensionAppId) {
      winAppIdentifier = ext.get_WebAuthnExtensionAppId().appIdentifier().get();
      pbU2fAppIdUsed = &bU2fAppIdUsed;
      break;
    }
  }

  // RPID
  rpID = aInfo.RpId().get();

  // User Verification Requirement
  const nsString& userVerificationReq = aInfo.userVerificationRequirement();
  // This mapping needs to be reviewed if values are added to the
  // UserVerificationRequirement enum.
  static_assert(MOZ_WEBAUTHN_ENUM_STRINGS_VERSION == 2);
  if (userVerificationReq.EqualsLiteral(
          MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED)) {
    winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
  } else if (userVerificationReq.EqualsLiteral(
                 MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED)) {
    winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED;
  } else if (userVerificationReq.EqualsLiteral(
                 MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_DISCOURAGED)) {
    winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED;
  } else {
    winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_ANY;
  }

  // allow Credentials
  nsTArray<WEBAUTHN_CREDENTIAL_EX> allowCredentials;
  WEBAUTHN_CREDENTIAL_EX* pAllowCredentials = nullptr;
  nsTArray<WEBAUTHN_CREDENTIAL_EX*> allowCredentialsPtrs;
  WEBAUTHN_CREDENTIAL_LIST allowCredentialList = {0};
  WEBAUTHN_CREDENTIAL_LIST* pAllowCredentialList = nullptr;

  for (auto& cred : aInfo.AllowList()) {
    uint8_t transports = cred.transports();
    DWORD winTransports = 0;
    if (transports & MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_USB) {
      winTransports |= WEBAUTHN_CTAP_TRANSPORT_USB;
    }
    if (transports & MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_NFC) {
      winTransports |= WEBAUTHN_CTAP_TRANSPORT_NFC;
    }
    if (transports & MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_BLE) {
      winTransports |= WEBAUTHN_CTAP_TRANSPORT_BLE;
    }
    if (transports & MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_INTERNAL) {
      winTransports |= WEBAUTHN_CTAP_TRANSPORT_INTERNAL;
    }

    WEBAUTHN_CREDENTIAL_EX credential = {
        WEBAUTHN_CREDENTIAL_EX_CURRENT_VERSION,
        static_cast<DWORD>(cred.id().Length()), (PBYTE)(cred.id().Elements()),
        WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY, winTransports};
    allowCredentials.AppendElement(credential);
  }

  if (allowCredentials.Length()) {
    pAllowCredentials = allowCredentials.Elements();
    for (DWORD i = 0; i < allowCredentials.Length(); i++) {
      allowCredentialsPtrs.AppendElement(&pAllowCredentials[i]);
    }
    allowCredentialList.cCredentials = allowCredentials.Length();
    allowCredentialList.ppCredentials = allowCredentialsPtrs.Elements();
    pAllowCredentialList = &allowCredentialList;
  }

  WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS WebAuthNAssertionOptions = {
      WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_CURRENT_VERSION,
      aInfo.TimeoutMS(),
      {0, NULL},
      {0, NULL},
      winAttachment,
      winUserVerificationReq,
      0,  // dwFlags
      winAppIdentifier,
      pbU2fAppIdUsed,
      nullptr,  // pCancellationId
      pAllowCredentialList,
  };

  GUID cancellationId = {0};
  if (gWinWebauthnGetCancellationId(&cancellationId) == S_OK) {
    WebAuthNAssertionOptions.pCancellationId = &cancellationId;
    mCancellationIds.emplace(aTransactionId, &cancellationId);
  }

  PWEBAUTHN_ASSERTION pWebAuthNAssertion = nullptr;

  // Bug 1518876: Get Window Handle from Content process for Windows WebAuthN
  // APIs
  HWND hWnd = GetForegroundWindow();

  HRESULT hr =
      gWinWebauthnGetAssertion(hWnd, rpID, &WebAuthNClientData,
                               &WebAuthNAssertionOptions, &pWebAuthNAssertion);

  mCancellationIds.erase(aTransactionId);

  if (hr == S_OK) {
    nsTArray<uint8_t> signature;
    signature.AppendElements(pWebAuthNAssertion->pbSignature,
                             pWebAuthNAssertion->cbSignature);

    nsTArray<uint8_t> keyHandle;
    keyHandle.AppendElements(pWebAuthNAssertion->Credential.pbId,
                             pWebAuthNAssertion->Credential.cbId);

    nsTArray<uint8_t> userHandle;
    userHandle.AppendElements(pWebAuthNAssertion->pbUserId,
                              pWebAuthNAssertion->cbUserId);

    nsTArray<uint8_t> authenticatorData;
    authenticatorData.AppendElements(pWebAuthNAssertion->pbAuthenticatorData,
                                     pWebAuthNAssertion->cbAuthenticatorData);

    nsTArray<WebAuthnExtensionResult> extensions;

    if (pbU2fAppIdUsed && *pbU2fAppIdUsed) {
      extensions.AppendElement(WebAuthnExtensionResultAppId(true));
    }

    WebAuthnGetAssertionResult result(aInfo.ClientDataJSON(), keyHandle,
                                      signature, authenticatorData, extensions,
                                      userHandle);

    Unused << mTransactionParent->SendConfirmSign(aTransactionId, result);
    ClearTransaction();

    gWinWebauthnFreeAssertion(pWebAuthNAssertion);

  } else {
    PCWSTR errorName = gWinWebauthnGetErrorName(hr);
    nsresult aError = NS_ERROR_DOM_ABORT_ERR;

    if (_wcsicmp(errorName, L"InvalidStateError") == 0) {
      aError = NS_ERROR_DOM_INVALID_STATE_ERR;
    } else if (_wcsicmp(errorName, L"ConstraintError") == 0 ||
               _wcsicmp(errorName, L"UnknownError") == 0) {
      aError = NS_ERROR_DOM_UNKNOWN_ERR;
    } else if (_wcsicmp(errorName, L"NotSupportedError") == 0) {
      aError = NS_ERROR_DOM_INVALID_STATE_ERR;
    } else if (_wcsicmp(errorName, L"NotAllowedError") == 0) {
      aError = NS_ERROR_DOM_NOT_ALLOWED_ERR;
    }

    MaybeAbortSign(aTransactionId, aError);
  }
}

void WinWebAuthnManager::MaybeAbortSign(const uint64_t& aTransactionId,
                                        const nsresult& aError) {
  AbortTransaction(aTransactionId, aError);
}

void WinWebAuthnManager::Cancel(PWebAuthnTransactionParent* aParent,
                                const Tainted<uint64_t>& aTransactionId) {
  if (mTransactionParent != aParent) {
    return;
  }

  ClearTransaction();

  auto iter = mCancellationIds.find(
      MOZ_NO_VALIDATE(aTransactionId,
                      "Transaction ID is checked against a global container, "
                      "so an invalid entry can affect another origin's "
                      "request. This issue is filed as Bug 1696159."));
  if (iter != mCancellationIds.end()) {
    gWinWebauthnCancelCurrentOperation(iter->second);
  }
}

}  // namespace mozilla::dom

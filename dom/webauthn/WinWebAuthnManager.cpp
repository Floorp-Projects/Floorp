/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PWebAuthnTransactionParent.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Unused.h"
#include "nsTextFormatter.h"
#include "winwebauthn/webauthn.h"
#include "WinWebAuthnManager.h"

namespace mozilla {
namespace dom {

namespace {
static mozilla::LazyLogModule gWinWebAuthnManagerLog("winwebauthnkeymanager");
StaticAutoPtr<WinWebAuthnManager> gWinWebAuthnManager;
static HMODULE gWinWebAuthnModule = 0;

static decltype(WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable)*
    gWinWebauthnIsUVPAA = nullptr;
static decltype(
    WebAuthNAuthenticatorMakeCredential)* gWinWebauthnMakeCredential = nullptr;
static decltype(
    WebAuthNFreeCredentialAttestation)* gWinWebauthnFreeCredentialAttestation =
    nullptr;
static decltype(WebAuthNAuthenticatorGetAssertion)* gWinWebauthnGetAssertion =
    nullptr;
static decltype(WebAuthNFreeAssertion)* gWinWebauthnFreeAssertion = nullptr;
static decltype(WebAuthNGetCancellationId)* gWinWebauthnGetCancellationId =
    nullptr;
static decltype(
    WebAuthNCancelCurrentOperation)* gWinWebauthnCancelCurrentOperation =
    nullptr;
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

  gWinWebAuthnModule = LoadLibrary(L"webauthn.dll");

  if (gWinWebAuthnModule) {
    gWinWebauthnIsUVPAA = reinterpret_cast<decltype(
        WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable)*>(
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

  BYTE U2FUserId = 0x01;

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
      WEBAUTHN_CLIENT_DATA_CURRENT_VERSION, aInfo.ClientDataJSON().Length(),
      (BYTE*)(aInfo.ClientDataJSON().get()), WEBAUTHN_HASH_ALGORITHM_SHA_256};

  // Algorithms
  nsTArray<WEBAUTHN_COSE_CREDENTIAL_PARAMETER> coseParams;

  // User Verification Requirement
  DWORD winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_ANY;

  // Attachment
  DWORD winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;

  // Resident Key
  BOOL winRequireResidentKey = FALSE;

  // AttestationConveyance
  DWORD winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_ANY;

  if (aInfo.Extra().isSome()) {
    const auto& extra = aInfo.Extra().ref();

    rpInfo.pwszName = extra.Rp().Name().get();
    rpInfo.pwszIcon = extra.Rp().Icon().get();

    userInfo.cbId = static_cast<DWORD>(extra.User().Id().Length());
    userInfo.pbId = const_cast<unsigned char*>(extra.User().Id().Elements());
    userInfo.pwszName = extra.User().Name().get();
    userInfo.pwszIcon = extra.User().Icon().get();
    userInfo.pwszDisplayName = extra.User().DisplayName().get();

    for (const auto& coseAlg : extra.coseAlgs()) {
      WEBAUTHN_COSE_CREDENTIAL_PARAMETER coseAlgorithm = {
          WEBAUTHN_COSE_CREDENTIAL_PARAMETER_CURRENT_VERSION,
          WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY, coseAlg.alg()};
      coseParams.AppendElement(coseAlgorithm);
    }

    const auto& sel = extra.AuthenticatorSelection();

    UserVerificationRequirement userVerificationReq =
        sel.userVerificationRequirement();
    switch (userVerificationReq) {
      case UserVerificationRequirement::Required:
        winUserVerificationReq =
            WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
        break;
      case UserVerificationRequirement::Preferred:
        winUserVerificationReq =
            WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED;
        break;
      case UserVerificationRequirement::Discouraged:
        winUserVerificationReq =
            WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED;
        break;
      default:
        winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_ANY;
        break;
    }

    if (sel.authenticatorAttachment().isSome()) {
      const AuthenticatorAttachment authenticatorAttachment =
          sel.authenticatorAttachment().value();
      switch (authenticatorAttachment) {
        case AuthenticatorAttachment::Platform:
          winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM;
          break;
        case AuthenticatorAttachment::Cross_platform:
          winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM;
          break;
        default:
          break;
      }
    }

    winRequireResidentKey = sel.requireResidentKey();

    // AttestationConveyance
    AttestationConveyancePreference attestation =
        extra.attestationConveyancePreference();
    DWORD winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_ANY;
    switch (attestation) {
      case AttestationConveyancePreference::Direct:
        winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT;
        break;
      case AttestationConveyancePreference::Indirect:
        winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_INDIRECT;
        break;
      case AttestationConveyancePreference::None:
        winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE;
        break;
      default:
        winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_ANY;
        break;
    }
  } else {
    userInfo.cbId = sizeof(BYTE);
    userInfo.pbId = &U2FUserId;

    WEBAUTHN_COSE_CREDENTIAL_PARAMETER coseAlgorithm = {
        WEBAUTHN_COSE_CREDENTIAL_PARAMETER_CURRENT_VERSION,
        WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY,
        WEBAUTHN_COSE_ALGORITHM_ECDSA_P256_WITH_SHA256};
    coseParams.AppendElement(coseAlgorithm);

    winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM_U2F_V2;
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
    if (transports & U2F_AUTHENTICATOR_TRANSPORT_USB) {
      winTransports |= WEBAUTHN_CTAP_TRANSPORT_USB;
    }
    if (transports & U2F_AUTHENTICATOR_TRANSPORT_NFC) {
      winTransports |= WEBAUTHN_CTAP_TRANSPORT_NFC;
    }
    if (transports & U2F_AUTHENTICATOR_TRANSPORT_BLE) {
      winTransports |= WEBAUTHN_CTAP_TRANSPORT_BLE;
    }
    if (transports & CTAP_AUTHENTICATOR_TRANSPORT_INTERNAL) {
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
      WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_CURRENT_VERSION,
      aInfo.TimeoutMS(),
      {0, NULL},
      {0, NULL},
      winAttachment,
      winRequireResidentKey,
      winUserVerificationReq,
      winAttestation,
      0,     // Flags
      NULL,  // CancellationId
      pExcludeCredentialList};

  GUID cancellationId = {0};
  if (gWinWebauthnGetCancellationId(&cancellationId) == S_OK) {
    WebAuthNCredentialOptions.pCancellationId = &cancellationId;
    mCancellationIds.emplace(aTransactionId, &cancellationId);
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
    nsTArray<uint8_t> attObject;
    attObject.AppendElements(
        pWebAuthNCredentialAttestation->pbAttestationObject,
        pWebAuthNCredentialAttestation->cbAttestationObject);

    nsTArray<uint8_t> credentialId;
    credentialId.AppendElements(pWebAuthNCredentialAttestation->pbCredentialId,
                                pWebAuthNCredentialAttestation->cbCredentialId);

    nsTArray<uint8_t> authenticatorData;

    if (aInfo.Extra().isSome()) {
      authenticatorData.AppendElements(
          pWebAuthNCredentialAttestation->pbAuthenticatorData,
          pWebAuthNCredentialAttestation->cbAuthenticatorData);
    } else {
      PWEBAUTHN_COMMON_ATTESTATION attestation =
          reinterpret_cast<PWEBAUTHN_COMMON_ATTESTATION>(
              pWebAuthNCredentialAttestation->pvAttestationDecode);

      DWORD coseKeyOffset = 32 +  // RPIDHash
                            1 +   // Flags
                            4 +   // Counter
                            16 +  // AAGuid
                            2 +   // Credential ID Length field
                            pWebAuthNCredentialAttestation->cbCredentialId;

      // Hardcoding as couldn't finder decoder and it is an ECC key.
      DWORD xOffset = coseKeyOffset + 10;
      DWORD yOffset = coseKeyOffset + 45;

      // Authenticator Data length check.
      if (pWebAuthNCredentialAttestation->cbAuthenticatorData < yOffset + 32) {
        MaybeAbortRegister(aTransactionId, NS_ERROR_DOM_INVALID_STATE_ERR);
      }

      authenticatorData.AppendElement(0x05);  // Reserved Byte
      authenticatorData.AppendElement(0x04);  // ECC Uncompressed Key
      authenticatorData.AppendElements(
          pWebAuthNCredentialAttestation->pbAuthenticatorData + xOffset,
          32);  // X Coordinate
      authenticatorData.AppendElements(
          pWebAuthNCredentialAttestation->pbAuthenticatorData + yOffset,
          32);  // Y Coordinate
      authenticatorData.AppendElement(
          pWebAuthNCredentialAttestation->cbCredentialId);
      authenticatorData.AppendElements(
          pWebAuthNCredentialAttestation->pbCredentialId,
          pWebAuthNCredentialAttestation->cbCredentialId);
      authenticatorData.AppendElements(attestation->pX5c->pbData,
                                       attestation->pX5c->cbData);
      authenticatorData.AppendElements(attestation->pbSignature,
                                       attestation->cbSignature);
    }

    WebAuthnMakeCredentialResult result(aInfo.ClientDataJSON(), attObject,
                                        credentialId, authenticatorData);

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
      WEBAUTHN_CLIENT_DATA_CURRENT_VERSION, aInfo.ClientDataJSON().Length(),
      (BYTE*)(aInfo.ClientDataJSON().get()), WEBAUTHN_HASH_ALGORITHM_SHA_256};

  if (aInfo.Extra().isSome()) {
    const auto& extra = aInfo.Extra().ref();

    for (const WebAuthnExtension& ext : extra.Extensions()) {
      if (ext.type() == WebAuthnExtension::TWebAuthnExtensionAppId) {
        winAppIdentifier =
            ext.get_WebAuthnExtensionAppId().appIdentifier().get();
        pbU2fAppIdUsed = &bU2fAppIdUsed;
        break;
      }
    }

    // RPID
    rpID = aInfo.RpId().get();

    // User Verification Requirement
    UserVerificationRequirement userVerificationReq =
        extra.userVerificationRequirement();

    switch (userVerificationReq) {
      case UserVerificationRequirement::Required:
        winUserVerificationReq =
            WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
        break;
      case UserVerificationRequirement::Preferred:
        winUserVerificationReq =
            WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED;
        break;
      case UserVerificationRequirement::Discouraged:
        winUserVerificationReq =
            WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED;
        break;
      default:
        winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_ANY;
        break;
    }
  } else {
    rpID = aInfo.Origin().get();
    winAppIdentifier = aInfo.RpId().get();
    pbU2fAppIdUsed = &bU2fAppIdUsed;
    winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM_U2F_V2;
    winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED;
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
    if (transports & U2F_AUTHENTICATOR_TRANSPORT_USB) {
      winTransports |= WEBAUTHN_CTAP_TRANSPORT_USB;
    }
    if (transports & U2F_AUTHENTICATOR_TRANSPORT_NFC) {
      winTransports |= WEBAUTHN_CTAP_TRANSPORT_NFC;
    }
    if (transports & U2F_AUTHENTICATOR_TRANSPORT_BLE) {
      winTransports |= WEBAUTHN_CTAP_TRANSPORT_BLE;
    }
    if (transports & CTAP_AUTHENTICATOR_TRANSPORT_INTERNAL) {
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
      WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY,
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
    if (aInfo.Extra().isSome()) {
      signature.AppendElements(pWebAuthNAssertion->pbSignature,
                               pWebAuthNAssertion->cbSignature);
    } else {
      // AuthenticatorData Length check.
      // First 32 bytes: RPID Hash
      // Next 1 byte: Flags
      // Next 4 bytes: Counter
      if (pWebAuthNAssertion->cbAuthenticatorData < 32 + 1 + 4) {
        MaybeAbortRegister(aTransactionId, NS_ERROR_DOM_INVALID_STATE_ERR);
      }

      signature.AppendElement(0x01);  // User Presence bit
      signature.AppendElements(pWebAuthNAssertion->pbAuthenticatorData +
                                   32 +  // RPID Hash length
                                   1,    // Flags
                               4);       // Counter length
      signature.AppendElements(pWebAuthNAssertion->pbSignature,
                               pWebAuthNAssertion->cbSignature);
    }

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
                                      signature, userHandle);

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
                                const uint64_t& aTransactionId) {
  if (mTransactionParent != aParent) {
    return;
  }

  ClearTransaction();

  auto iter = mCancellationIds.find(aTransactionId);
  if (iter != mCancellationIds.end()) {
    gWinWebauthnCancelCurrentOperation(iter->second);
  }
}

}  // namespace dom
}  // namespace mozilla

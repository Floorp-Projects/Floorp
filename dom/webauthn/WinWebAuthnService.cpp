/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PWebAuthnTransactionParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Assertions.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/Unused.h"

#include "nsTextFormatter.h"
#include "nsWindowsHelpers.h"
#include "WebAuthnEnumStrings.h"
#include "WebAuthnResult.h"
#include "WebAuthnTransportIdentifiers.h"
#include "winwebauthn/webauthn.h"
#include "WinWebAuthnService.h"

namespace mozilla::dom {

namespace {
StaticRWLock gWinWebAuthnModuleLock;

static bool gWinWebAuthnModuleUnusable = false;
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
 * WinWebAuthnService Implementation
 **********************************************************************/

constexpr uint32_t kMinWinWebAuthNApiVersion = WEBAUTHN_API_VERSION_1;

NS_IMPL_ISUPPORTS(WinWebAuthnService, nsIWebAuthnService)

/* static */
nsresult WinWebAuthnService::EnsureWinWebAuthnModuleLoaded() {
  {
    StaticAutoReadLock lock(gWinWebAuthnModuleLock);
    if (gWinWebAuthnModule) {
      // The module is already loaded.
      return NS_OK;
    }
    if (gWinWebAuthnModuleUnusable) {
      // A previous attempt to load the module failed.
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  StaticAutoWriteLock lock(gWinWebAuthnModuleLock);
  if (gWinWebAuthnModule) {
    // Another thread successfully loaded the module while we were waiting.
    return NS_OK;
  }
  if (gWinWebAuthnModuleUnusable) {
    // Another thread failed to load the module while we were waiting.
    return NS_ERROR_NOT_AVAILABLE;
  }

  gWinWebAuthnModule = LoadLibrarySystem32(L"webauthn.dll");
  auto markModuleUnusable = MakeScopeExit([]() {
    if (gWinWebAuthnModule) {
      FreeLibrary(gWinWebAuthnModule);
      gWinWebAuthnModule = 0;
    }
    gWinWebAuthnModuleUnusable = true;
  });

  if (!gWinWebAuthnModule) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  gWinWebauthnIsUVPAA = reinterpret_cast<
      decltype(WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable)*>(
      GetProcAddress(gWinWebAuthnModule,
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
          GetProcAddress(gWinWebAuthnModule, "WebAuthNCancelCurrentOperation"));
  gWinWebauthnGetErrorName = reinterpret_cast<decltype(WebAuthNGetErrorName)*>(
      GetProcAddress(gWinWebAuthnModule, "WebAuthNGetErrorName"));
  gWinWebauthnGetApiVersionNumber =
      reinterpret_cast<decltype(WebAuthNGetApiVersionNumber)*>(
          GetProcAddress(gWinWebAuthnModule, "WebAuthNGetApiVersionNumber"));

  if (!(gWinWebauthnIsUVPAA && gWinWebauthnMakeCredential &&
        gWinWebauthnFreeCredentialAttestation && gWinWebauthnGetAssertion &&
        gWinWebauthnFreeAssertion && gWinWebauthnGetCancellationId &&
        gWinWebauthnCancelCurrentOperation && gWinWebauthnGetErrorName &&
        gWinWebauthnGetApiVersionNumber)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  markModuleUnusable.release();
  return NS_OK;
}

WinWebAuthnService::~WinWebAuthnService() {
  StaticAutoWriteLock lock(gWinWebAuthnModuleLock);
  if (gWinWebAuthnModule) {
    FreeLibrary(gWinWebAuthnModule);
  }
  gWinWebAuthnModule = 0;
}

// static
bool WinWebAuthnService::AreWebAuthNApisAvailable() {
  nsresult rv = EnsureWinWebAuthnModuleLoaded();
  NS_ENSURE_SUCCESS(rv, false);

  StaticAutoReadLock lock(gWinWebAuthnModuleLock);
  return gWinWebAuthnModule &&
         gWinWebauthnGetApiVersionNumber() >= kMinWinWebAuthNApiVersion;
}

NS_IMETHODIMP
WinWebAuthnService::GetIsUVPAA(bool* aAvailable) {
  nsresult rv = EnsureWinWebAuthnModuleLoaded();
  NS_ENSURE_SUCCESS(rv, rv);

  if (WinWebAuthnService::AreWebAuthNApisAvailable()) {
    BOOL isUVPAA = FALSE;
    StaticAutoReadLock lock(gWinWebAuthnModuleLock);
    *aAvailable = gWinWebAuthnModule && gWinWebauthnIsUVPAA(&isUVPAA) == S_OK &&
                  isUVPAA == TRUE;
  } else {
    *aAvailable = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
WinWebAuthnService::Cancel(uint64_t aTransactionId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WinWebAuthnService::Reset() {
  // Reset will never be the first function to use gWinWebAuthnModule, so
  // we shouldn't try to initialize it here.
  if (mTransactionId.isSome()) {
    StaticAutoReadLock lock(gWinWebAuthnModuleLock);
    if (gWinWebAuthnModule) {
      gWinWebauthnCancelCurrentOperation(&mCancellationId);
    }
    mTransactionId.reset();
  }

  return NS_OK;
}

NS_IMETHODIMP
WinWebAuthnService::MakeCredential(uint64_t aTransactionId,
                                   uint64_t browsingContextId,
                                   nsIWebAuthnRegisterArgs* aArgs,
                                   nsIWebAuthnRegisterPromise* aPromise) {
  nsresult rv = EnsureWinWebAuthnModuleLoaded();
  NS_ENSURE_SUCCESS(rv, rv);

  Reset();
  mTransactionId = Some(aTransactionId);
  {
    StaticAutoReadLock lock(gWinWebAuthnModuleLock);
    if (gWinWebauthnGetCancellationId(&mCancellationId) != S_OK) {
      // caller will reject promise
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }
  }

  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "WinWebAuthnService::MakeCredential",
      [self = RefPtr{this}, aArgs = RefPtr{aArgs}, aPromise = RefPtr{aPromise},
       aCancellationId = mCancellationId]() mutable {
        // Take a read lock on gWinWebAuthnModuleLock to prevent the module from
        // being unloaded while the operation is in progress. This does not
        // prevent the operation from being cancelled, so it does not block a
        // clean shutdown.
        StaticAutoReadLock lock(gWinWebAuthnModuleLock);
        if (!gWinWebAuthnModule) {
          aPromise->Reject(NS_ERROR_DOM_UNKNOWN_ERR);
          return;
        }

        BOOL HmacCreateSecret = FALSE;
        BOOL MinPinLength = FALSE;

        // RP Information
        nsString rpId;
        Unused << aArgs->GetRpId(rpId);
        WEBAUTHN_RP_ENTITY_INFORMATION rpInfo = {
            WEBAUTHN_RP_ENTITY_INFORMATION_CURRENT_VERSION, rpId.get(), nullptr,
            nullptr};

        // User Information
        WEBAUTHN_USER_ENTITY_INFORMATION userInfo = {
            WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION,
            0,
            nullptr,
            nullptr,
            nullptr,
            nullptr};

        // Client Data
        nsCString clientDataJSON;
        Unused << aArgs->GetClientDataJSON(clientDataJSON);
        WEBAUTHN_CLIENT_DATA WebAuthNClientData = {
            WEBAUTHN_CLIENT_DATA_CURRENT_VERSION,
            (DWORD)clientDataJSON.Length(), (BYTE*)(clientDataJSON.get()),
            WEBAUTHN_HASH_ALGORITHM_SHA_256};

        // User Verification Requirement
        DWORD winUserVerificationReq =
            WEBAUTHN_USER_VERIFICATION_REQUIREMENT_ANY;

        // Resident Key
        BOOL winRequireResidentKey = FALSE;
        BOOL winPreferResidentKey = FALSE;

        // AttestationConveyance
        DWORD winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_ANY;

        nsString rpName;
        Unused << aArgs->GetRpName(rpName);
        rpInfo.pwszName = rpName.get();
        rpInfo.pwszIcon = nullptr;

        nsTArray<uint8_t> userId;
        Unused << aArgs->GetUserId(userId);
        userInfo.cbId = static_cast<DWORD>(userId.Length());
        userInfo.pbId = const_cast<unsigned char*>(userId.Elements());

        nsString userName;
        Unused << aArgs->GetUserName(userName);
        userInfo.pwszName = userName.get();

        userInfo.pwszIcon = nullptr;

        nsString userDisplayName;
        Unused << aArgs->GetUserDisplayName(userDisplayName);
        userInfo.pwszDisplayName = userDisplayName.get();

        // Algorithms
        nsTArray<WEBAUTHN_COSE_CREDENTIAL_PARAMETER> coseParams;
        nsTArray<int32_t> coseAlgs;
        Unused << aArgs->GetCoseAlgs(coseAlgs);
        for (const int32_t& coseAlg : coseAlgs) {
          WEBAUTHN_COSE_CREDENTIAL_PARAMETER coseAlgorithm = {
              WEBAUTHN_COSE_CREDENTIAL_PARAMETER_CURRENT_VERSION,
              WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY, coseAlg};
          coseParams.AppendElement(coseAlgorithm);
        }

        nsString userVerificationReq;
        Unused << aArgs->GetUserVerification(userVerificationReq);
        // This mapping needs to be reviewed if values are added to the
        // UserVerificationRequirement enum.
        static_assert(MOZ_WEBAUTHN_ENUM_STRINGS_VERSION == 2);
        if (userVerificationReq.EqualsLiteral(
                MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED)) {
          winUserVerificationReq =
              WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
        } else if (userVerificationReq.EqualsLiteral(
                       MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED)) {
          winUserVerificationReq =
              WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED;
        } else if (userVerificationReq.EqualsLiteral(
                       MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_DISCOURAGED)) {
          winUserVerificationReq =
              WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED;
        } else {
          winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_ANY;
        }

        // Attachment
        DWORD winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;
        nsString authenticatorAttachment;
        nsresult rv =
            aArgs->GetAuthenticatorAttachment(authenticatorAttachment);
        if (rv != NS_ERROR_NOT_AVAILABLE) {
          if (NS_FAILED(rv)) {
            aPromise->Reject(rv);
            return;
          }
          // This mapping needs to be reviewed if values are added to the
          // AuthenticatorAttachement enum.
          static_assert(MOZ_WEBAUTHN_ENUM_STRINGS_VERSION == 2);
          if (authenticatorAttachment.EqualsLiteral(
                  MOZ_WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM)) {
            winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM;
          } else if (
              authenticatorAttachment.EqualsLiteral(
                  MOZ_WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM)) {
            winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM;
          } else {
            winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;
          }
        }

        nsString residentKey;
        Unused << aArgs->GetResidentKey(residentKey);
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
          // WebAuthnManager::MakeCredential is supposed to assign one of the
          // above values, so this shouldn't happen.
          MOZ_ASSERT_UNREACHABLE();
          aPromise->Reject(NS_ERROR_DOM_UNKNOWN_ERR);
          return;
        }

        // AttestationConveyance
        nsString attestation;
        Unused << aArgs->GetAttestationConveyancePreference(attestation);
        bool anonymize = false;
        // This mapping needs to be reviewed if values are added to the
        // AttestationConveyancePreference enum.
        static_assert(MOZ_WEBAUTHN_ENUM_STRINGS_VERSION == 2);
        if (attestation.EqualsLiteral(
                MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE)) {
          winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE;
          anonymize = true;
        } else if (
            attestation.EqualsLiteral(
                MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_INDIRECT)) {
          winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_INDIRECT;
        } else if (attestation.EqualsLiteral(
                       MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT)) {
          winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT;
        } else {
          winAttestation = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_ANY;
        }

        bool requestedCredProps;
        Unused << aArgs->GetCredProps(&requestedCredProps);

        bool requestedMinPinLength;
        Unused << aArgs->GetMinPinLength(&requestedMinPinLength);

        bool requestedHmacCreateSecret;
        Unused << aArgs->GetHmacCreateSecret(&requestedHmacCreateSecret);

        // Extensions that might require an entry: hmac-secret, minPinLength.
        WEBAUTHN_EXTENSION rgExtension[2] = {};
        DWORD cExtensions = 0;
        if (requestedHmacCreateSecret) {
          HmacCreateSecret = TRUE;
          rgExtension[cExtensions].pwszExtensionIdentifier =
              WEBAUTHN_EXTENSIONS_IDENTIFIER_HMAC_SECRET;
          rgExtension[cExtensions].cbExtension = sizeof(BOOL);
          rgExtension[cExtensions].pvExtension = &HmacCreateSecret;
          cExtensions++;
        }
        if (requestedMinPinLength) {
          MinPinLength = TRUE;
          rgExtension[cExtensions].pwszExtensionIdentifier =
              WEBAUTHN_EXTENSIONS_IDENTIFIER_MIN_PIN_LENGTH;
          rgExtension[cExtensions].cbExtension = sizeof(BOOL);
          rgExtension[cExtensions].pvExtension = &MinPinLength;
          cExtensions++;
        }

        WEBAUTHN_COSE_CREDENTIAL_PARAMETERS WebAuthNCredentialParameters = {
            static_cast<DWORD>(coseParams.Length()), coseParams.Elements()};

        // Exclude Credentials
        nsTArray<nsTArray<uint8_t>> excludeList;
        Unused << aArgs->GetExcludeList(excludeList);

        nsTArray<uint8_t> excludeListTransports;
        Unused << aArgs->GetExcludeListTransports(excludeListTransports);

        if (excludeList.Length() != excludeListTransports.Length()) {
          aPromise->Reject(NS_ERROR_DOM_UNKNOWN_ERR);
          return;
        }

        nsTArray<WEBAUTHN_CREDENTIAL_EX> excludeCredentials;
        WEBAUTHN_CREDENTIAL_EX* pExcludeCredentials = nullptr;
        nsTArray<WEBAUTHN_CREDENTIAL_EX*> excludeCredentialsPtrs;
        WEBAUTHN_CREDENTIAL_LIST excludeCredentialList = {0};
        WEBAUTHN_CREDENTIAL_LIST* pExcludeCredentialList = nullptr;

        for (size_t i = 0; i < excludeList.Length(); i++) {
          nsTArray<uint8_t>& cred = excludeList[i];
          uint8_t& transports = excludeListTransports[i];
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
              static_cast<DWORD>(cred.Length()), (PBYTE)(cred.Elements()),
              WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY, winTransports};
          excludeCredentials.AppendElement(credential);
        }

        if (!excludeCredentials.IsEmpty()) {
          pExcludeCredentials = excludeCredentials.Elements();
          for (DWORD i = 0; i < excludeCredentials.Length(); i++) {
            excludeCredentialsPtrs.AppendElement(&pExcludeCredentials[i]);
          }
          excludeCredentialList.cCredentials = excludeCredentials.Length();
          excludeCredentialList.ppCredentials =
              excludeCredentialsPtrs.Elements();
          pExcludeCredentialList = &excludeCredentialList;
        }

        uint32_t timeout_u32;
        Unused << aArgs->GetTimeoutMS(&timeout_u32);
        DWORD timeout = timeout_u32;

        // MakeCredentialOptions
        WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS
        WebAuthNCredentialOptions = {
            WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_7,
            timeout,
            {0, NULL},
            {0, NULL},
            winAttachment,
            winRequireResidentKey,
            winUserVerificationReq,
            winAttestation,
            0,                 // Flags
            &aCancellationId,  // CancellationId
            pExcludeCredentialList,
            WEBAUTHN_ENTERPRISE_ATTESTATION_NONE,
            WEBAUTHN_LARGE_BLOB_SUPPORT_NONE,
            winPreferResidentKey,  // PreferResidentKey
            FALSE,                 // BrowserInPrivateMode
            FALSE,                 // EnablePrf
            NULL,                  // LinkedDevice
            0,                     // size of JsonExt
            NULL,                  // JsonExt
        };

        if (cExtensions != 0) {
          WebAuthNCredentialOptions.Extensions.cExtensions = cExtensions;
          WebAuthNCredentialOptions.Extensions.pExtensions = rgExtension;
        }

        PWEBAUTHN_CREDENTIAL_ATTESTATION pWebAuthNCredentialAttestation =
            nullptr;

        // Bug 1518876: Get Window Handle from Content process for Windows
        // WebAuthN APIs
        HWND hWnd = GetForegroundWindow();

        HRESULT hr = gWinWebauthnMakeCredential(
            hWnd, &rpInfo, &userInfo, &WebAuthNCredentialParameters,
            &WebAuthNClientData, &WebAuthNCredentialOptions,
            &pWebAuthNCredentialAttestation);

        if (hr == S_OK) {
          RefPtr<WebAuthnRegisterResult> result = new WebAuthnRegisterResult(
              clientDataJSON, pWebAuthNCredentialAttestation);

          // WEBAUTHN_CREDENTIAL_ATTESTATION structs of version >= 4 always
          // include a flag to indicate whether a resident key was created. We
          // copy that flag to the credProps extension output only if the RP
          // requested the credProps extension.
          if (requestedCredProps &&
              pWebAuthNCredentialAttestation->dwVersion >=
                  WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_4) {
            BOOL rk = pWebAuthNCredentialAttestation->bResidentKey;
            Unused << result->SetCredPropsRk(rk == TRUE);
          }
          gWinWebauthnFreeCredentialAttestation(pWebAuthNCredentialAttestation);

          if (anonymize) {
            nsresult rv = result->Anonymize();
            if (NS_FAILED(rv)) {
              aPromise->Reject(NS_ERROR_DOM_NOT_ALLOWED_ERR);
              return;
            }
          }
          aPromise->Resolve(result);
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

          aPromise->Reject(aError);
        }
      }));

  NS_DispatchBackgroundTask(runnable, NS_DISPATCH_EVENT_MAY_BLOCK);
  return NS_OK;
}

NS_IMETHODIMP
WinWebAuthnService::GetAssertion(uint64_t aTransactionId,
                                 uint64_t browsingContextId,
                                 nsIWebAuthnSignArgs* aArgs,
                                 nsIWebAuthnSignPromise* aPromise) {
  nsresult rv = EnsureWinWebAuthnModuleLoaded();
  NS_ENSURE_SUCCESS(rv, rv);

  Reset();
  mTransactionId = Some(aTransactionId);
  {
    StaticAutoReadLock lock(gWinWebAuthnModuleLock);
    if (gWinWebauthnGetCancellationId(&mCancellationId) != S_OK) {
      // caller will reject promise
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }
  }

  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "WinWebAuthnService::MakeCredential",
      [self = RefPtr{this}, aArgs = RefPtr{aArgs}, aPromise = RefPtr{aPromise},
       aCancellationId = mCancellationId]() mutable {
        // Take a read lock on gWinWebAuthnModuleLock to prevent the module from
        // being unloaded while the operation is in progress. This does not
        // prevent the operation from being cancelled, so it does not block a
        // clean shutdown.
        StaticAutoReadLock lock(gWinWebAuthnModuleLock);
        if (!gWinWebAuthnModule) {
          aPromise->Reject(NS_ERROR_DOM_UNKNOWN_ERR);
          return;
        }

        // Attachment
        DWORD winAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;

        // AppId
        BOOL bAppIdUsed = FALSE;
        BOOL* pbAppIdUsed = nullptr;
        PCWSTR winAppIdentifier = nullptr;

        // Client Data
        nsCString clientDataJSON;
        Unused << aArgs->GetClientDataJSON(clientDataJSON);
        WEBAUTHN_CLIENT_DATA WebAuthNClientData = {
            WEBAUTHN_CLIENT_DATA_CURRENT_VERSION,
            (DWORD)clientDataJSON.Length(), (BYTE*)(clientDataJSON.get()),
            WEBAUTHN_HASH_ALGORITHM_SHA_256};

        nsString appId;
        nsresult rv = aArgs->GetAppId(appId);
        if (rv != NS_ERROR_NOT_AVAILABLE) {
          if (NS_FAILED(rv)) {
            aPromise->Reject(rv);
            return;
          }
          winAppIdentifier = appId.get();
          pbAppIdUsed = &bAppIdUsed;
        }

        // RPID
        nsString rpId;
        Unused << aArgs->GetRpId(rpId);

        // User Verification Requirement
        nsString userVerificationReq;
        Unused << aArgs->GetUserVerification(userVerificationReq);
        DWORD winUserVerificationReq;
        // This mapping needs to be reviewed if values are added to the
        // UserVerificationRequirement enum.
        static_assert(MOZ_WEBAUTHN_ENUM_STRINGS_VERSION == 2);
        if (userVerificationReq.EqualsLiteral(
                MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED)) {
          winUserVerificationReq =
              WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
        } else if (userVerificationReq.EqualsLiteral(
                       MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED)) {
          winUserVerificationReq =
              WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED;
        } else if (userVerificationReq.EqualsLiteral(
                       MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_DISCOURAGED)) {
          winUserVerificationReq =
              WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED;
        } else {
          winUserVerificationReq = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_ANY;
        }

        // allow Credentials
        nsTArray<nsTArray<uint8_t>> allowList;
        Unused << aArgs->GetAllowList(allowList);

        nsTArray<uint8_t> allowListTransports;
        Unused << aArgs->GetAllowListTransports(allowListTransports);

        if (allowList.Length() != allowListTransports.Length()) {
          aPromise->Reject(NS_ERROR_DOM_UNKNOWN_ERR);
          return;
        }

        nsTArray<WEBAUTHN_CREDENTIAL_EX> allowCredentials;
        WEBAUTHN_CREDENTIAL_EX* pAllowCredentials = nullptr;
        nsTArray<WEBAUTHN_CREDENTIAL_EX*> allowCredentialsPtrs;
        WEBAUTHN_CREDENTIAL_LIST allowCredentialList = {0};
        WEBAUTHN_CREDENTIAL_LIST* pAllowCredentialList = nullptr;

        for (size_t i = 0; i < allowList.Length(); i++) {
          nsTArray<uint8_t>& cred = allowList[i];
          uint8_t& transports = allowListTransports[i];
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
              static_cast<DWORD>(cred.Length()), (PBYTE)(cred.Elements()),
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

        uint32_t timeout_u32;
        Unused << aArgs->GetTimeoutMS(&timeout_u32);
        DWORD timeout = timeout_u32;

        WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS WebAuthNAssertionOptions =
            {
                WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_7,
                timeout,
                {0, NULL},
                {0, NULL},
                winAttachment,
                winUserVerificationReq,
                0,  // dwFlags
                winAppIdentifier,
                pbAppIdUsed,
                &aCancellationId,  // CancellationId
                pAllowCredentialList,
                WEBAUTHN_CRED_LARGE_BLOB_OPERATION_NONE,
                0,      // Size of CredLargeBlob
                NULL,   // CredLargeBlob
                NULL,   // HmacSecretSaltValues
                FALSE,  // BrowserInPrivateMode
                NULL,   // LinkedDevice
                FALSE,  // AutoFill
                0,      // Size of JsonExt
                NULL,   // JsonExt
            };

        PWEBAUTHN_ASSERTION pWebAuthNAssertion = nullptr;

        // Bug 1518876: Get Window Handle from Content process for Windows
        // WebAuthN APIs
        HWND hWnd = GetForegroundWindow();

        HRESULT hr = gWinWebauthnGetAssertion(
            hWnd, rpId.get(), &WebAuthNClientData, &WebAuthNAssertionOptions,
            &pWebAuthNAssertion);

        if (hr == S_OK) {
          RefPtr<WebAuthnSignResult> result =
              new WebAuthnSignResult(clientDataJSON, pWebAuthNAssertion);
          gWinWebauthnFreeAssertion(pWebAuthNAssertion);
          if (winAppIdentifier != nullptr) {
            // The gWinWebauthnGetAssertion call modified bAppIdUsed through
            // a pointer provided in WebAuthNAssertionOptions.
            Unused << result->SetUsedAppId(bAppIdUsed == TRUE);
          }
          aPromise->Resolve(result);
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

          aPromise->Reject(aError);
        }
      }));

  NS_DispatchBackgroundTask(runnable, NS_DISPATCH_EVENT_MAY_BLOCK);
  return NS_OK;
}

NS_IMETHODIMP
WinWebAuthnService::PinCallback(uint64_t aTransactionId,
                                const nsACString& aPin) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WinWebAuthnService::ResumeMakeCredential(uint64_t aTransactionId,
                                         bool aForceNoneAttestation) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WinWebAuthnService::SelectionCallback(uint64_t aTransactionId,
                                      uint64_t aIndex) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WinWebAuthnService::AddVirtualAuthenticator(
    const nsACString& protocol, const nsACString& transport,
    bool hasResidentKey, bool hasUserVerification, bool isUserConsenting,
    bool isUserVerified, uint64_t* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WinWebAuthnService::RemoveVirtualAuthenticator(uint64_t authenticatorId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WinWebAuthnService::AddCredential(uint64_t authenticatorId,
                                  const nsACString& credentialId,
                                  bool isResidentCredential,
                                  const nsACString& rpId,
                                  const nsACString& privateKey,
                                  const nsACString& userHandle,
                                  uint32_t signCount) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WinWebAuthnService::GetCredentials(
    uint64_t authenticatorId,
    nsTArray<RefPtr<nsICredentialParameters>>& _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WinWebAuthnService::RemoveCredential(uint64_t authenticatorId,
                                     const nsACString& credentialId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WinWebAuthnService::RemoveAllCredentials(uint64_t authenticatorId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WinWebAuthnService::SetUserVerified(uint64_t authenticatorId,
                                    bool isUserVerified) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WinWebAuthnService::Listen() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
WinWebAuthnService::RunCommand(const nsACString& cmd) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace mozilla::dom

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CtapArgs.h"
#include "WebAuthnEnumStrings.h"
#include "WebAuthnUtil.h"
#include "mozilla/dom/PWebAuthnTransactionParent.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(CtapRegisterArgs, nsICtapRegisterArgs)

NS_IMETHODIMP
CtapRegisterArgs::GetOrigin(nsAString& aOrigin) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  aOrigin = mInfo.Origin();
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetClientDataHash(nsTArray<uint8_t>& aClientDataHash) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  CryptoBuffer clientDataHash;
  nsresult rv = HashCString(mInfo.ClientDataJSON(), clientDataHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }
  aClientDataHash.Clear();
  aClientDataHash.AppendElements(clientDataHash);

  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetRpId(nsAString& aRpId) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  aRpId = mInfo.RpId();
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetRpName(nsAString& aRpName) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  aRpName = mInfo.Rp().Name();
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetUserId(nsTArray<uint8_t>& aUserId) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  aUserId.Clear();
  aUserId.AppendElements(mInfo.User().Id());
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetUserName(nsAString& aUserName) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  aUserName = mInfo.User().Name();
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetUserDisplayName(nsAString& aUserDisplayName) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  aUserDisplayName = mInfo.User().DisplayName();
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetCoseAlgs(nsTArray<int32_t>& aCoseAlgs) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  aCoseAlgs.Clear();
  for (const CoseAlg& coseAlg : mInfo.coseAlgs()) {
    aCoseAlgs.AppendElement(coseAlg.alg());
  }
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetExcludeList(nsTArray<nsTArray<uint8_t> >& aExcludeList) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  aExcludeList.Clear();
  for (const WebAuthnScopedCredential& cred : mInfo.ExcludeList()) {
    aExcludeList.AppendElement(cred.id().Clone());
  }
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetHmacCreateSecret(bool* aHmacCreateSecret) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  for (const WebAuthnExtension& ext : mInfo.Extensions()) {
    if (ext.type() == WebAuthnExtension::TWebAuthnExtensionHmacSecret) {
      *aHmacCreateSecret =
          ext.get_WebAuthnExtensionHmacSecret().hmacCreateSecret();
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
CtapRegisterArgs::GetResidentKey(nsAString& aResidentKey) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  aResidentKey = mInfo.AuthenticatorSelection().residentKey();
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetUserVerification(nsAString& aUserVerificationRequirement) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  aUserVerificationRequirement =
      mInfo.AuthenticatorSelection().userVerificationRequirement();
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetAuthenticatorAttachment(
    nsAString& aAuthenticatorAttachment) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  if (mInfo.AuthenticatorSelection().authenticatorAttachment().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aAuthenticatorAttachment =
      *mInfo.AuthenticatorSelection().authenticatorAttachment();
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetTimeoutMS(uint32_t* aTimeoutMS) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  *aTimeoutMS = mInfo.TimeoutMS();
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetAttestationConveyancePreference(
    nsAString& aAttestationConveyancePreference) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  if (mForceNoneAttestation) {
    aAttestationConveyancePreference = NS_ConvertUTF8toUTF16(
        MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE);
  } else {
    aAttestationConveyancePreference = mInfo.attestationConveyancePreference();
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(CtapSignArgs, nsICtapSignArgs)

NS_IMETHODIMP
CtapSignArgs::GetOrigin(nsAString& aOrigin) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  aOrigin = mInfo.Origin();
  return NS_OK;
}

NS_IMETHODIMP
CtapSignArgs::GetRpId(nsAString& aRpId) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  aRpId = mInfo.RpId();
  return NS_OK;
}

NS_IMETHODIMP
CtapSignArgs::GetClientDataHash(nsTArray<uint8_t>& aClientDataHash) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  CryptoBuffer clientDataHash;
  nsresult rv = HashCString(mInfo.ClientDataJSON(), clientDataHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }
  aClientDataHash.Clear();
  aClientDataHash.AppendElements(clientDataHash);

  return NS_OK;
}

NS_IMETHODIMP
CtapSignArgs::GetAllowList(nsTArray<nsTArray<uint8_t> >& aAllowList) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  aAllowList.Clear();
  for (const WebAuthnScopedCredential& cred : mInfo.AllowList()) {
    aAllowList.AppendElement(cred.id().Clone());
  }
  return NS_OK;
}

NS_IMETHODIMP
CtapSignArgs::GetHmacCreateSecret(bool* aHmacCreateSecret) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  for (const WebAuthnExtension& ext : mInfo.Extensions()) {
    if (ext.type() == WebAuthnExtension::TWebAuthnExtensionHmacSecret) {
      *aHmacCreateSecret =
          ext.get_WebAuthnExtensionHmacSecret().hmacCreateSecret();
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
CtapSignArgs::GetAppId(nsAString& aAppId) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  for (const WebAuthnExtension& ext : mInfo.Extensions()) {
    if (ext.type() == WebAuthnExtension::TWebAuthnExtensionAppId) {
      aAppId = ext.get_WebAuthnExtensionAppId().appIdentifier();
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
CtapSignArgs::GetAppIdHash(nsTArray<uint8_t>& aAppIdHash) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  for (const WebAuthnExtension& ext : mInfo.Extensions()) {
    if (ext.type() == WebAuthnExtension::TWebAuthnExtensionAppId) {
      aAppIdHash.Clear();
      aAppIdHash.AppendElements(ext.get_WebAuthnExtensionAppId().AppId());
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
CtapSignArgs::GetUserVerification(nsAString& aUserVerificationRequirement) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  aUserVerificationRequirement = mInfo.userVerificationRequirement();
  return NS_OK;
}

NS_IMETHODIMP
CtapSignArgs::GetTimeoutMS(uint32_t* aTimeoutMS) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  *aTimeoutMS = mInfo.TimeoutMS();
  return NS_OK;
}

}  // namespace mozilla::dom

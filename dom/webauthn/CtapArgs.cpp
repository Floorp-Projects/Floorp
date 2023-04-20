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
  if (mInfo.Extra().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aRpName = (*mInfo.Extra()).Rp().Name();
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetUserId(nsTArray<uint8_t>& aUserId) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  if (mInfo.Extra().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aUserId.Clear();
  aUserId.AppendElements((*mInfo.Extra()).User().Id());
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetUserName(nsAString& aUserName) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  if (mInfo.Extra().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aUserName = (*mInfo.Extra()).User().Name();
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetUserDisplayName(nsAString& aUserDisplayName) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  if (mInfo.Extra().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aUserDisplayName = (*mInfo.Extra()).User().DisplayName();
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetCoseAlgs(nsTArray<int32_t>& aCoseAlgs) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  if (mInfo.Extra().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aCoseAlgs.Clear();
  for (const CoseAlg& coseAlg : (*mInfo.Extra()).coseAlgs()) {
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
  if (mInfo.Extra().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  for (const WebAuthnExtension& ext : (*mInfo.Extra()).Extensions()) {
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
  if (mInfo.Extra().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aResidentKey = (*mInfo.Extra()).AuthenticatorSelection().residentKey();
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetUserVerification(nsAString& aUserVerificationRequirement) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  if (mInfo.Extra().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aUserVerificationRequirement =
      (*mInfo.Extra()).AuthenticatorSelection().userVerificationRequirement();
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterArgs::GetAuthenticatorAttachment(
    nsAString& aAuthenticatorAttachment) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  if (mInfo.Extra().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if ((*mInfo.Extra())
          .AuthenticatorSelection()
          .authenticatorAttachment()
          .isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aAuthenticatorAttachment =
      *(*mInfo.Extra()).AuthenticatorSelection().authenticatorAttachment();
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
  if (mInfo.Extra().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mForceNoneAttestation) {
    aAttestationConveyancePreference = NS_ConvertUTF8toUTF16(
        MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE);
  } else {
    aAttestationConveyancePreference =
        (*mInfo.Extra()).attestationConveyancePreference();
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
  if (mInfo.Extra().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  for (const WebAuthnExtension& ext : (*mInfo.Extra()).Extensions()) {
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
  if (mInfo.Extra().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  for (const WebAuthnExtension& ext : (*mInfo.Extra()).Extensions()) {
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
  if (mInfo.Extra().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  for (const WebAuthnExtension& ext : (*mInfo.Extra()).Extensions()) {
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
  if (mInfo.Extra().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aUserVerificationRequirement = (*mInfo.Extra()).userVerificationRequirement();
  return NS_OK;
}

NS_IMETHODIMP
CtapSignArgs::GetTimeoutMS(uint32_t* aTimeoutMS) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  *aTimeoutMS = mInfo.TimeoutMS();
  return NS_OK;
}

}  // namespace mozilla::dom

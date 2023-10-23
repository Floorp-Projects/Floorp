/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebAuthnArgs.h"
#include "WebAuthnEnumStrings.h"
#include "WebAuthnUtil.h"
#include "mozilla/dom/PWebAuthnTransactionParent.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(WebAuthnRegisterArgs, nsIWebAuthnRegisterArgs)

NS_IMETHODIMP
WebAuthnRegisterArgs::GetOrigin(nsAString& aOrigin) {
  aOrigin = mInfo.Origin();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetChallenge(nsTArray<uint8_t>& aChallenge) {
  aChallenge.Assign(mInfo.Challenge());
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetClientDataJSON(nsACString& aClientDataJSON) {
  aClientDataJSON = mInfo.ClientDataJSON();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetClientDataHash(nsTArray<uint8_t>& aClientDataHash) {
  nsresult rv = HashCString(mInfo.ClientDataJSON(), aClientDataHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetRpId(nsAString& aRpId) {
  aRpId = mInfo.RpId();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetRpName(nsAString& aRpName) {
  aRpName = mInfo.Rp().Name();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetUserId(nsTArray<uint8_t>& aUserId) {
  aUserId.Assign(mInfo.User().Id());
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetUserName(nsAString& aUserName) {
  aUserName = mInfo.User().Name();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetUserDisplayName(nsAString& aUserDisplayName) {
  aUserDisplayName = mInfo.User().DisplayName();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetCoseAlgs(nsTArray<int32_t>& aCoseAlgs) {
  aCoseAlgs.Clear();
  for (const CoseAlg& coseAlg : mInfo.coseAlgs()) {
    aCoseAlgs.AppendElement(coseAlg.alg());
  }
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetExcludeList(
    nsTArray<nsTArray<uint8_t> >& aExcludeList) {
  aExcludeList.Clear();
  for (const WebAuthnScopedCredential& cred : mInfo.ExcludeList()) {
    aExcludeList.AppendElement(cred.id().Clone());
  }
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetExcludeListTransports(
    nsTArray<uint8_t>& aExcludeListTransports) {
  aExcludeListTransports.Clear();
  for (const WebAuthnScopedCredential& cred : mInfo.ExcludeList()) {
    aExcludeListTransports.AppendElement(cred.transports());
  }
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetCredProps(bool* aCredProps) {
  *aCredProps = mCredProps;

  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetHmacCreateSecret(bool* aHmacCreateSecret) {
  *aHmacCreateSecret = mHmacCreateSecret;

  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetMinPinLength(bool* aMinPinLength) {
  *aMinPinLength = mMinPinLength;

  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetResidentKey(nsAString& aResidentKey) {
  aResidentKey = mInfo.AuthenticatorSelection().residentKey();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetUserVerification(
    nsAString& aUserVerificationRequirement) {
  aUserVerificationRequirement =
      mInfo.AuthenticatorSelection().userVerificationRequirement();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetAuthenticatorAttachment(
    nsAString& aAuthenticatorAttachment) {
  if (mInfo.AuthenticatorSelection().authenticatorAttachment().isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aAuthenticatorAttachment =
      *mInfo.AuthenticatorSelection().authenticatorAttachment();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetTimeoutMS(uint32_t* aTimeoutMS) {
  *aTimeoutMS = mInfo.TimeoutMS();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnRegisterArgs::GetAttestationConveyancePreference(
    nsAString& aAttestationConveyancePreference) {
  aAttestationConveyancePreference = mInfo.attestationConveyancePreference();
  return NS_OK;
}

NS_IMPL_ISUPPORTS(WebAuthnSignArgs, nsIWebAuthnSignArgs)

NS_IMETHODIMP
WebAuthnSignArgs::GetOrigin(nsAString& aOrigin) {
  aOrigin = mInfo.Origin();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnSignArgs::GetRpId(nsAString& aRpId) {
  aRpId = mInfo.RpId();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnSignArgs::GetChallenge(nsTArray<uint8_t>& aChallenge) {
  aChallenge.Assign(mInfo.Challenge());
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnSignArgs::GetClientDataJSON(nsACString& aClientDataJSON) {
  aClientDataJSON = mInfo.ClientDataJSON();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnSignArgs::GetClientDataHash(nsTArray<uint8_t>& aClientDataHash) {
  nsresult rv = HashCString(mInfo.ClientDataJSON(), aClientDataHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebAuthnSignArgs::GetAllowList(nsTArray<nsTArray<uint8_t> >& aAllowList) {
  aAllowList.Clear();
  for (const WebAuthnScopedCredential& cred : mInfo.AllowList()) {
    aAllowList.AppendElement(cred.id().Clone());
  }
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnSignArgs::GetAllowListTransports(
    nsTArray<uint8_t>& aAllowListTransports) {
  aAllowListTransports.Clear();
  for (const WebAuthnScopedCredential& cred : mInfo.AllowList()) {
    aAllowListTransports.AppendElement(cred.transports());
  }
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnSignArgs::GetHmacCreateSecret(bool* aHmacCreateSecret) {
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
WebAuthnSignArgs::GetAppId(nsAString& aAppId) {
  if (mAppId.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aAppId = mAppId.ref();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnSignArgs::GetUserVerification(nsAString& aUserVerificationRequirement) {
  aUserVerificationRequirement = mInfo.userVerificationRequirement();
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnSignArgs::GetTimeoutMS(uint32_t* aTimeoutMS) {
  *aTimeoutMS = mInfo.TimeoutMS();
  return NS_OK;
}

}  // namespace mozilla::dom

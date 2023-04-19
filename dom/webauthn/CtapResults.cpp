/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CtapResults.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(CtapRegisterResult, nsICtapRegisterResult)

NS_IMETHODIMP
CtapRegisterResult::GetStatus(nsresult* aStatus) {
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterResult::GetAttestationObject(
    nsTArray<uint8_t>& aAttestationObject) {
  aAttestationObject.Clear();
  aAttestationObject.AppendElements(mAttestationObject);
  return NS_OK;
}

NS_IMETHODIMP
CtapRegisterResult::GetCredentialId(nsTArray<uint8_t>& aCredentialId) {
  aCredentialId.Clear();
  aCredentialId.AppendElements(mCredentialId);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(CtapSignResult, nsICtapSignResult)

NS_IMETHODIMP
CtapSignResult::GetStatus(nsresult* aStatus) {
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
CtapSignResult::GetCredentialId(nsTArray<uint8_t>& aCredentialId) {
  aCredentialId.Clear();
  aCredentialId.AppendElements(mCredentialId);
  return NS_OK;
}

NS_IMETHODIMP
CtapSignResult::GetAuthenticatorData(nsTArray<uint8_t>& aAuthenticatorData) {
  aAuthenticatorData.Clear();
  aAuthenticatorData.AppendElements(mAuthenticatorData);
  return NS_OK;
}

NS_IMETHODIMP
CtapSignResult::GetSignature(nsTArray<uint8_t>& aSignature) {
  aSignature.Clear();
  aSignature.AppendElements(mSignature);
  return NS_OK;
}

NS_IMETHODIMP
CtapSignResult::GetUserHandle(nsTArray<uint8_t>& aUserHandle) {
  aUserHandle.Clear();
  aUserHandle.AppendElements(mUserHandle);
  return NS_OK;
}

NS_IMETHODIMP
CtapSignResult::GetUserName(nsACString& aUserName) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CtapSignResult::GetRpIdHash(nsTArray<uint8_t>& aRpIdHash) {
  aRpIdHash.Clear();
  aRpIdHash.AppendElements(mRpIdHash);
  return NS_OK;
}

}  // namespace mozilla::dom

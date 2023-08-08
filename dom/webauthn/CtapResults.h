/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CtapResults_h
#define CtapResults_h

#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "nsIWebAuthnController.h"

namespace mozilla::dom {

class CtapRegisterResult final : public nsICtapRegisterResult {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICTAPREGISTERRESULT

  explicit CtapRegisterResult(nsresult aStatus,
                              nsTArray<uint8_t>&& aAttestationObject,
                              nsTArray<uint8_t>&& aCredentialId)
      : mStatus(aStatus),
        mAttestationObject(std::move(aAttestationObject)),
        mCredentialId(std::move(aCredentialId)) {}

 private:
  ~CtapRegisterResult() = default;

  nsresult mStatus;
  nsTArray<uint8_t> mAttestationObject;
  nsTArray<uint8_t> mCredentialId;
};

class CtapSignResult final : public nsICtapSignResult {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICTAPSIGNRESULT

  explicit CtapSignResult(nsresult aStatus, nsTArray<uint8_t>&& aCredentialId,
                          nsTArray<uint8_t>&& aSignature,
                          nsTArray<uint8_t>&& aAuthenticatorData,
                          nsTArray<uint8_t>&& aUserHandle,
                          nsTArray<uint8_t>&& aRpIdHash)
      : mStatus(aStatus),
        mCredentialId(std::move(aCredentialId)),
        mSignature(std::move(aSignature)),
        mAuthenticatorData(std::move(aAuthenticatorData)),
        mUserHandle(std::move(aUserHandle)),
        mRpIdHash(std::move(aRpIdHash)) {}

 private:
  ~CtapSignResult() = default;

  nsresult mStatus;
  nsTArray<uint8_t> mCredentialId;
  nsTArray<uint8_t> mSignature;
  nsTArray<uint8_t> mAuthenticatorData;
  nsTArray<uint8_t> mUserHandle;
  nsTArray<uint8_t> mRpIdHash;
};

}  // namespace mozilla::dom

#endif  // CtapResult_h

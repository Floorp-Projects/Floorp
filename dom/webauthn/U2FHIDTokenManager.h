/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2FHIDTokenManager_h
#define mozilla_dom_U2FHIDTokenManager_h

#include "mozilla/dom/U2FTokenTransport.h"
#include "authenticator/src/u2fhid-capi.h"

/*
 * U2FHIDTokenManager is a Rust implementation of a secure token manager
 * for the U2F and WebAuthn APIs, talking to HIDs.
 */

namespace mozilla {
namespace dom {

class U2FAppIds {
 public:
  explicit U2FAppIds(const nsTArray<nsTArray<uint8_t>>& aApplications) {
    mAppIds = rust_u2f_app_ids_new();

    for (auto& app_id : aApplications) {
      rust_u2f_app_ids_add(mAppIds, app_id.Elements(), app_id.Length());
    }
  }

  rust_u2f_app_ids* Get() { return mAppIds; }

  ~U2FAppIds() { rust_u2f_app_ids_free(mAppIds); }

 private:
  rust_u2f_app_ids* mAppIds;
};

class U2FKeyHandles {
 public:
  explicit U2FKeyHandles(
      const nsTArray<WebAuthnScopedCredential>& aCredentials) {
    mKeyHandles = rust_u2f_khs_new();

    for (auto& cred : aCredentials) {
      rust_u2f_khs_add(mKeyHandles, cred.id().Elements(), cred.id().Length(),
                       cred.transports());
    }
  }

  rust_u2f_key_handles* Get() { return mKeyHandles; }

  ~U2FKeyHandles() { rust_u2f_khs_free(mKeyHandles); }

 private:
  rust_u2f_key_handles* mKeyHandles;
};

class U2FResult {
 public:
  explicit U2FResult(uint64_t aTransactionId, rust_u2f_result* aResult)
      : mTransactionId(aTransactionId), mResult(aResult) {
    MOZ_ASSERT(mResult);
  }

  ~U2FResult() { rust_u2f_res_free(mResult); }

  uint64_t GetTransactionId() { return mTransactionId; }

  bool IsError() { return NS_FAILED(GetError()); }

  nsresult GetError() {
    switch (rust_u2f_result_error(mResult)) {
      case U2F_ERROR_UKNOWN:
      case U2F_ERROR_CONSTRAINT:
        return NS_ERROR_DOM_UNKNOWN_ERR;
      case U2F_ERROR_NOT_SUPPORTED:
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      case U2F_ERROR_INVALID_STATE:
        return NS_ERROR_DOM_INVALID_STATE_ERR;
      case U2F_ERROR_NOT_ALLOWED:
        return NS_ERROR_DOM_NOT_ALLOWED_ERR;
      default:
        return NS_OK;
    }
  }

  bool CopyRegistration(nsTArray<uint8_t>& aBuffer) {
    return CopyBuffer(U2F_RESBUF_ID_REGISTRATION, aBuffer);
  }

  bool CopyKeyHandle(nsTArray<uint8_t>& aBuffer) {
    return CopyBuffer(U2F_RESBUF_ID_KEYHANDLE, aBuffer);
  }

  bool CopySignature(nsTArray<uint8_t>& aBuffer) {
    return CopyBuffer(U2F_RESBUF_ID_SIGNATURE, aBuffer);
  }

  bool CopyAppId(nsTArray<uint8_t>& aBuffer) {
    return CopyBuffer(U2F_RESBUF_ID_APPID, aBuffer);
  }

 private:
  bool CopyBuffer(uint8_t aResBufID, nsTArray<uint8_t>& aBuffer) {
    size_t len;
    if (!rust_u2f_resbuf_length(mResult, aResBufID, &len)) {
      return false;
    }

    if (!aBuffer.SetLength(len, fallible)) {
      return false;
    }

    return rust_u2f_resbuf_copy(mResult, aResBufID, aBuffer.Elements());
  }

  uint64_t mTransactionId;
  rust_u2f_result* mResult;
};

class U2FHIDTokenManager final : public U2FTokenTransport {
 public:
  explicit U2FHIDTokenManager();

  RefPtr<U2FRegisterPromise> Register(const WebAuthnMakeCredentialInfo& aInfo,
                                      bool aForceNoneAttestation) override;

  RefPtr<U2FSignPromise> Sign(const WebAuthnGetAssertionInfo& aInfo) override;

  void Cancel() override;
  void Drop() override;

  void HandleRegisterResult(UniquePtr<U2FResult>&& aResult);
  void HandleSignResult(UniquePtr<U2FResult>&& aResult);

 private:
  ~U2FHIDTokenManager() = default;

  void ClearPromises() {
    mRegisterPromise.RejectIfExists(NS_ERROR_DOM_UNKNOWN_ERR, __func__);
    mSignPromise.RejectIfExists(NS_ERROR_DOM_UNKNOWN_ERR, __func__);
  }

  class Transaction {
   public:
    Transaction(uint64_t aId, const nsTArray<uint8_t>& aRpIdHash,
                const nsCString& aClientDataJSON,
                bool aForceNoneAttestation = false)
        : mId(aId),
          mRpIdHash(aRpIdHash.Clone()),
          mClientDataJSON(aClientDataJSON),
          mForceNoneAttestation(aForceNoneAttestation) {}

    // The transaction ID.
    uint64_t mId;

    // The RP ID hash.
    nsTArray<uint8_t> mRpIdHash;

    // The clientData JSON.
    nsCString mClientDataJSON;

    // Whether we'll force "none" attestation.
    bool mForceNoneAttestation;
  };

  rust_u2f_manager* mU2FManager;
  Maybe<Transaction> mTransaction;
  MozPromiseHolder<U2FRegisterPromise> mRegisterPromise;
  MozPromiseHolder<U2FSignPromise> mSignPromise;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_U2FHIDTokenManager_h

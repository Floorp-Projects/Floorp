/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CTAPHIDTokenManager_h
#define mozilla_dom_CTAPHIDTokenManager_h

#include "mozilla/dom/U2FTokenTransport.h"
#include "authenticator/src/u2fhid-capi.h"
#include "authenticator/src/ctap2-capi.h"

/*
 * CTAPHIDTokenManager is a Rust implementation of a secure token manager
 * for the CTAP2, U2F and WebAuthn APIs, talking to HIDs.
 */

namespace mozilla::dom {

class Ctap2PubKeyCredentialDescriptor {
 public:
  explicit Ctap2PubKeyCredentialDescriptor(
      const nsTArray<WebAuthnScopedCredential>& aCredentials) {
    cred_descriptors = rust_ctap2_pkcd_new();

    for (auto& cred : aCredentials) {
      rust_ctap2_pkcd_add(cred_descriptors, cred.id().Elements(),
                          cred.id().Length(), cred.transports());
    }
  }

  rust_ctap2_pub_key_cred_descriptors* Get() { return cred_descriptors; }

  ~Ctap2PubKeyCredentialDescriptor() { rust_ctap2_pkcd_free(cred_descriptors); }

 private:
  rust_ctap2_pub_key_cred_descriptors* cred_descriptors;
};

class CTAPResult {
 public:
  explicit CTAPResult(uint64_t aTransactionId, rust_u2f_result* aResult)
      : mTransactionId(aTransactionId), mU2FResult(aResult) {
    MOZ_ASSERT(mU2FResult);
  }

  explicit CTAPResult(uint64_t aTransactionId,
                      rust_ctap2_register_result* aResult)
      : mTransactionId(aTransactionId), mRegisterResult(aResult) {
    MOZ_ASSERT(mRegisterResult);
  }

  explicit CTAPResult(uint64_t aTransactionId, rust_ctap2_sign_result* aResult)
      : mTransactionId(aTransactionId), mSignResult(aResult) {
    MOZ_ASSERT(mSignResult);
  }

  ~CTAPResult() {
    // Rust-API can handle possible NULL-pointers
    rust_u2f_res_free(mU2FResult);
    rust_ctap2_register_res_free(mRegisterResult);
    rust_ctap2_sign_res_free(mSignResult);
  }

  uint64_t GetTransactionId() { return mTransactionId; }

  bool IsError() { return NS_FAILED(GetError()); }

  nsresult GetError() {
    uint8_t res;
    if (mU2FResult) {
      res = rust_u2f_result_error(mU2FResult);
    } else if (mRegisterResult) {
      res = rust_ctap2_register_result_error(mRegisterResult);
    } else if (mSignResult) {
      res = rust_ctap2_sign_result_error(mSignResult);
    } else {
      return NS_ERROR_FAILURE;
    }

    switch (res) {
      case U2F_OK:
        return NS_OK;
      case U2F_ERROR_UKNOWN:
      case U2F_ERROR_CONSTRAINT:
        return NS_ERROR_DOM_UNKNOWN_ERR;
      case U2F_ERROR_NOT_SUPPORTED:
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      case U2F_ERROR_INVALID_STATE:
        return NS_ERROR_DOM_INVALID_STATE_ERR;
      case U2F_ERROR_NOT_ALLOWED:
        return NS_ERROR_DOM_NOT_ALLOWED_ERR;
      case CTAP_ERROR_PIN_REQUIRED:
      case CTAP_ERROR_PIN_INVALID:
      case CTAP_ERROR_PIN_AUTH_BLOCKED:
      case CTAP_ERROR_PIN_BLOCKED:
        // This is not perfect, but we are reusing an existing error-code here.
        // We need to differentiate only PIN-errors from non-PIN errors
        // to know if the Popup-Dialog should be removed or stay
        // after the operation errors out. We don't want to define
        // new NS-errors at the moment, since it's all internal anyways.
        return NS_ERROR_DOM_OPERATION_ERR;
      default:
        // Generic error
        return NS_ERROR_FAILURE;
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

  bool CopyClientDataStr(nsCString& aBuffer) {
    if (mU2FResult) {
      return false;
    } else if (mRegisterResult) {
      size_t len;
      if (!rust_ctap2_register_result_client_data_len(mRegisterResult, &len)) {
        return false;
      }

      if (!aBuffer.SetLength(len, fallible)) {
        return false;
      }

      return rust_ctap2_register_result_client_data_copy(mRegisterResult,
                                                         aBuffer.Data());
    } else if (mSignResult) {
      size_t len;
      if (!rust_ctap2_sign_result_client_data_len(mSignResult, &len)) {
        return false;
      }

      if (!aBuffer.SetLength(len, fallible)) {
        return false;
      }

      return rust_ctap2_sign_result_client_data_copy(mSignResult,
                                                     aBuffer.Data());
    } else {
      return false;
    }
  }

  bool IsCtap2() {
    // If it's not an U2F result, we already know its CTAP2
    return !mU2FResult;
  }

  bool HasAppId() { return Contains(U2F_RESBUF_ID_APPID); }

  bool HasKeyHandle() { return Contains(U2F_RESBUF_ID_KEYHANDLE); }

  bool Ctap2GetNumberOfSignAssertions(size_t& len) {
    return rust_ctap2_sign_result_assertions_len(mSignResult, &len);
  }

  bool Ctap2CopyAttestation(nsTArray<uint8_t>& aBuffer) {
    if (!mRegisterResult) {
      return false;
    }

    size_t len;
    if (!rust_ctap2_register_result_attestation_len(mRegisterResult, &len)) {
      return false;
    }

    if (!aBuffer.SetLength(len, fallible)) {
      return false;
    }

    return rust_ctap2_register_result_attestation_copy(mRegisterResult,
                                                       aBuffer.Elements());
  }

  bool Ctap2CopyPubKeyCredential(nsTArray<uint8_t>& aBuffer, size_t index) {
    return Ctap2SignResCopyBuffer(index, CTAP2_SIGN_RESULT_PUBKEY_CRED_ID,
                                  aBuffer);
  }

  bool Ctap2CopySignature(nsTArray<uint8_t>& aBuffer, size_t index) {
    return Ctap2SignResCopyBuffer(index, CTAP2_SIGN_RESULT_SIGNATURE, aBuffer);
  }

  bool Ctap2CopyUserId(nsTArray<uint8_t>& aBuffer, size_t index) {
    return Ctap2SignResCopyBuffer(index, CTAP2_SIGN_RESULT_USER_ID, aBuffer);
  }

  bool Ctap2CopyAuthData(nsTArray<uint8_t>& aBuffer, size_t index) {
    return Ctap2SignResCopyBuffer(index, CTAP2_SIGN_RESULT_AUTH_DATA, aBuffer);
  }

  bool Ctap2HasPubKeyCredential(size_t index) {
    return Ctap2SignResContains(index, CTAP2_SIGN_RESULT_PUBKEY_CRED_ID);
  }

  bool HasUserId(size_t index) {
    return Ctap2SignResContains(index, CTAP2_SIGN_RESULT_USER_ID);
  }

  bool HasUserName(size_t index) {
    return Ctap2SignResContains(index, CTAP2_SIGN_RESULT_USER_NAME);
  }

  bool CopyUserName(nsCString& aBuffer, size_t index) {
    if (!mSignResult) {
      return false;
    }

    size_t len;
    if (!rust_ctap2_sign_result_username_len(mSignResult, index, &len)) {
      return false;
    }

    if (!aBuffer.SetLength(len, fallible)) {
      return false;
    }

    return rust_ctap2_sign_result_username_copy(mSignResult, index,
                                                aBuffer.Data());
  }

 private:
  bool Contains(uint8_t aResBufId) {
    if (mU2FResult) {
      return rust_u2f_resbuf_contains(mU2FResult, aResBufId);
    }
    return false;
  }
  bool CopyBuffer(uint8_t aResBufID, nsTArray<uint8_t>& aBuffer) {
    if (!mU2FResult) {
      return false;
    }

    size_t len;
    if (!rust_u2f_resbuf_length(mU2FResult, aResBufID, &len)) {
      return false;
    }

    if (!aBuffer.SetLength(len, fallible)) {
      return false;
    }

    return rust_u2f_resbuf_copy(mU2FResult, aResBufID, aBuffer.Elements());
  }

  bool Ctap2SignResContains(size_t assertion_idx, uint8_t item_idx) {
    if (mSignResult) {
      return rust_ctap2_sign_result_item_contains(mSignResult, assertion_idx,
                                                  item_idx);
    }
    return false;
  }
  bool Ctap2SignResCopyBuffer(size_t assertion_idx, uint8_t item_idx,
                              nsTArray<uint8_t>& aBuffer) {
    if (!mSignResult) {
      return false;
    }

    size_t len;
    if (!rust_ctap2_sign_result_item_len(mSignResult, assertion_idx, item_idx,
                                         &len)) {
      return false;
    }

    if (!aBuffer.SetLength(len, fallible)) {
      return false;
    }

    return rust_ctap2_sign_result_item_copy(mSignResult, assertion_idx,
                                            item_idx, aBuffer.Elements());
  }

  uint64_t mTransactionId;
  rust_u2f_result* mU2FResult = nullptr;
  rust_ctap2_register_result* mRegisterResult = nullptr;
  rust_ctap2_sign_result* mSignResult = nullptr;
};

class CTAPHIDTokenManager final : public U2FTokenTransport {
 public:
  explicit CTAPHIDTokenManager();

  // TODO(MS): Once we completely switch over to CTAPHIDTokenManager and remove
  // the old U2F version, this needs to be renamed to CTAPRegisterPromise. Same
  // for Sign.
  virtual RefPtr<U2FRegisterPromise> Register(
      const WebAuthnMakeCredentialInfo& aInfo, bool aForceNoneAttestation,
      void status_callback(rust_ctap2_status_update_res*)) override;

  virtual RefPtr<U2FSignPromise> Sign(
      const WebAuthnGetAssertionInfo& aInfo,
      void status_callback(rust_ctap2_status_update_res*)) override;

  void Cancel() override;
  void Drop() override;

  void HandleRegisterResult(UniquePtr<CTAPResult>&& aResult);
  void HandleSignResult(UniquePtr<CTAPResult>&& aResult);

 private:
  ~CTAPHIDTokenManager() = default;

  void HandleRegisterResultCtap1(UniquePtr<CTAPResult>&& aResult);
  void HandleRegisterResultCtap2(UniquePtr<CTAPResult>&& aResult);
  void HandleSignResultCtap1(UniquePtr<CTAPResult>&& aResult);
  void HandleSignResultCtap2(UniquePtr<CTAPResult>&& aResult);
  mozilla::Maybe<WebAuthnGetAssertionResultWrapper>
  HandleSelectedSignResultCtap2(UniquePtr<CTAPResult>&& aResult, size_t index);
  void ClearPromises() {
    mRegisterPromise.RejectIfExists(NS_ERROR_DOM_UNKNOWN_ERR, __func__);
    mSignPromise.RejectIfExists(NS_ERROR_DOM_UNKNOWN_ERR, __func__);
  }

  class Transaction {
   public:
    Transaction(uint64_t aId, const nsTArray<uint8_t>& aRpIdHash,
                const Maybe<nsTArray<uint8_t>>& aAppIdHash,
                const nsCString& aClientDataJSON,
                bool aForceNoneAttestation = false)
        : mId(aId),
          mRpIdHash(aRpIdHash.Clone()),
          mClientDataJSON(aClientDataJSON),
          mForceNoneAttestation(aForceNoneAttestation) {
      if (aAppIdHash) {
        mAppIdHash = Some(aAppIdHash->Clone());
      } else {
        mAppIdHash = Nothing();
      }
    }

    // The transaction ID.
    uint64_t mId;

    // The RP ID hash.
    nsTArray<uint8_t> mRpIdHash;

    // The App ID hash, if the AppID extension was set
    Maybe<nsTArray<uint8_t>> mAppIdHash;

    // The clientData JSON.
    nsCString mClientDataJSON;

    // Whether we'll force "none" attestation.
    bool mForceNoneAttestation;
  };

  rust_ctap_manager* mCTAPManager;
  Maybe<Transaction> mTransaction;
  MozPromiseHolder<U2FRegisterPromise> mRegisterPromise;
  MozPromiseHolder<U2FSignPromise> mSignPromise;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CTAPHIDTokenManager_h

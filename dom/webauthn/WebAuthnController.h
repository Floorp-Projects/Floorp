/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnController_h
#define mozilla_dom_WebAuthnController_h

#include "nsIWebAuthnController.h"
#include "mozilla/dom/PWebAuthnTransaction.h"
#include "mozilla/Tainting.h"

/*
 * Parent process manager for WebAuthn API transactions. Handles process
 * transactions from all content processes, make sure only one transaction is
 * live at any time. Manages access to hardware and software based key systems.
 *
 */

namespace mozilla::dom {

class WebAuthnController final : public nsIWebAuthnController {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIU2FTOKENMANAGER
  NS_DECL_NSIWEBAUTHNCONTROLLER

  static void Initialize();
  static WebAuthnController* Get();
  void Register(PWebAuthnTransactionParent* aTransactionParent,
                const uint64_t& aTransactionId,
                const WebAuthnMakeCredentialInfo& aInfo);

  void Sign(PWebAuthnTransactionParent* aTransactionParent,
            const uint64_t& aTransactionId,
            const WebAuthnGetAssertionInfo& aInfo);

  void Cancel(PWebAuthnTransactionParent* aTransactionParent,
              const Tainted<uint64_t>& aTransactionId);

  void MaybeClearTransaction(PWebAuthnTransactionParent* aParent);

  uint64_t GetCurrentTransactionId() {
    return mTransaction.isNothing() ? 0 : mTransaction.ref().mTransactionId;
  }

  bool CurrentTransactionIsRegister() { return mPendingRegisterInfo.isSome(); }

  bool CurrentTransactionIsSign() { return mPendingSignInfo.isSome(); }

  // Sends a "webauthn-prompt" observer notification with the given data.
  template <typename... T>
  void SendPromptNotification(const char16_t* aFormat, T... aArgs);

  // Same as SendPromptNotification, but with the already formatted string
  // void SendPromptNotificationPreformatted(const nsACString& aJSON);
  // The main thread runnable function for "SendPromptNotification".
  void RunSendPromptNotification(const nsString& aJSON);

 private:
  WebAuthnController();
  ~WebAuthnController() = default;
  nsCOMPtr<nsIWebAuthnTransport> GetTransportImpl();

  void AbortTransaction(const uint64_t& aTransactionId, const nsresult& aError,
                        bool shouldCancelActiveDialog);
  void AbortOngoingTransaction();
  void ClearTransaction(bool cancel_prompt);

  void DoRegister(const WebAuthnMakeCredentialInfo& aInfo,
                  bool aForceNoneAttestation);
  void DoSign(const WebAuthnGetAssertionInfo& aTransactionInfo);

  void RunFinishRegister(uint64_t aTransactionId,
                         const RefPtr<nsICtapRegisterResult>& aResult);
  void RunFinishSign(uint64_t aTransactionId,
                     const nsTArray<RefPtr<nsICtapSignResult>>& aResult);

  // The main thread runnable function for "nsIU2FTokenManager.ResumeRegister".
  void RunResumeRegister(uint64_t aTransactionId, bool aForceNoneAttestation);
  void RunResumeSign(uint64_t aTransactionId);
  void RunResumeWithSelectedSignResult(uint64_t aTransactionId, uint64_t idx);
  void RunPinCallback(uint64_t aTransactionId, const nsCString& aPin);

  // The main thread runnable function for "nsIU2FTokenManager.Cancel".
  void RunCancel(uint64_t aTransactionId);

  // Using a raw pointer here, as the lifetime of the IPC object is managed by
  // the PBackground protocol code. This means we cannot be left holding an
  // invalid IPC protocol object after the transaction is finished.
  PWebAuthnTransactionParent* mTransactionParent;

  nsCOMPtr<nsIWebAuthnTransport> mTransportImpl;

  // Pending registration info while we wait for user input.
  Maybe<WebAuthnMakeCredentialInfo> mPendingRegisterInfo;

  // Pending registration info while we wait for user input.
  Maybe<WebAuthnGetAssertionInfo> mPendingSignInfo;

  nsTArray<RefPtr<nsICtapSignResult>> mPendingSignResults;

  class Transaction {
   public:
    Transaction(uint64_t aTransactionId, const nsTArray<uint8_t>& aRpIdHash,
                const Maybe<nsTArray<uint8_t>>& aAppIdHash,
                const nsCString& aClientDataJSON,
                bool aForceNoneAttestation = false)
        : mTransactionId(aTransactionId),
          mRpIdHash(aRpIdHash.Clone()),
          mClientDataJSON(aClientDataJSON) {
      if (aAppIdHash.isSome()) {
        mAppIdHash = Some(aAppIdHash.ref().Clone());
      } else {
        mAppIdHash = Nothing();
      }
    }
    uint64_t mTransactionId;
    nsTArray<uint8_t> mRpIdHash;
    Maybe<nsTArray<uint8_t>> mAppIdHash;
    nsCString mClientDataJSON;
  };

  Maybe<Transaction> mTransaction;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_U2FTokenManager_h

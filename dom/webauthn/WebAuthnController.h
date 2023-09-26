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
  NS_DECL_NSIWEBAUTHNCONTROLLER

  // Main thread only
  static void Initialize();

  // IPDL Background thread only
  static WebAuthnController* Get();

  // IPDL Background thread only
  void Register(PWebAuthnTransactionParent* aTransactionParent,
                const uint64_t& aTransactionId,
                const WebAuthnMakeCredentialInfo& aInfo);

  // IPDL Background thread only
  void Sign(PWebAuthnTransactionParent* aTransactionParent,
            const uint64_t& aTransactionId,
            const WebAuthnGetAssertionInfo& aInfo);

  // IPDL Background thread only
  void Cancel(PWebAuthnTransactionParent* aTransactionParent,
              const Tainted<uint64_t>& aTransactionId);

  // IPDL Background thread only
  void MaybeClearTransaction(PWebAuthnTransactionParent* aParent);

 private:
  WebAuthnController();
  ~WebAuthnController() = default;

  // All of the private functions and members are to be
  // accessed on the IPDL background thread only.

  nsCOMPtr<nsIWebAuthnTransport> GetTransportImpl();
  nsCOMPtr<nsIWebAuthnTransport> mTransportImpl;

  void AbortTransaction(const nsresult& aError);
  void ClearTransaction();
  void RunCancel(uint64_t aTransactionId);
  void RunFinishRegister(uint64_t aTransactionId,
                         const RefPtr<nsICtapRegisterResult>& aResult);
  void RunFinishSign(uint64_t aTransactionId,
                     const RefPtr<nsICtapSignResult>& aResult);

  // Using a raw pointer here, as the lifetime of the IPC object is managed by
  // the PBackground protocol code. This means we cannot be left holding an
  // invalid IPC protocol object after the transaction is finished.
  PWebAuthnTransactionParent* mTransactionParent;

  // The current transaction ID.
  Maybe<uint64_t> mTransactionId;

  // Client data associated with mTransactionId.
  Maybe<nsCString> mPendingClientData;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_U2FTokenManager_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WinWebAuthnService_h
#define mozilla_dom_WinWebAuthnService_h

#include "mozilla/dom/PWebAuthnTransaction.h"
#include "mozilla/Tainting.h"
#include "nsIWebAuthnService.h"

namespace mozilla::dom {

class WinWebAuthnService final : public nsIWebAuthnService {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBAUTHNSERVICE

  static bool IsUserVerifyingPlatformAuthenticatorAvailable();
  static bool AreWebAuthNApisAvailable();
  static nsresult EnsureWinWebAuthnModuleLoaded();

  WinWebAuthnService()
      : mTransactionState(Nothing(), "WinWebAuthnService::mTransactionState"){};

 private:
  ~WinWebAuthnService();

  uint32_t GetWebAuthNApiVersion();

  struct TransactionState {
    uint64_t transactionId;
    uint64_t browsingContextId;
    Maybe<RefPtr<nsIWebAuthnSignArgs>> pendingSignArgs;
    Maybe<RefPtr<nsIWebAuthnSignPromise>> pendingSignPromise;
    GUID cancellationId;
  };

  using TransactionStateMutex = DataMutex<Maybe<TransactionState>>;
  TransactionStateMutex mTransactionState;
  void DoGetAssertion(Maybe<nsTArray<uint8_t>>&& aSelectedCredentialId,
                      const TransactionStateMutex::AutoLock& aGuard);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_WinWebAuthnService_h

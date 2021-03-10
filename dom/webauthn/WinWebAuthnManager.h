/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WinWebAuthnManager_h
#define mozilla_dom_WinWebAuthnManager_h

#include "mozilla/dom/U2FTokenTransport.h"
#include "mozilla/dom/PWebAuthnTransaction.h"

namespace mozilla {
namespace dom {

class WebAuthnTransactionParent;

class WinWebAuthnManager final {
 public:
  static WinWebAuthnManager* Get();
  void Register(PWebAuthnTransactionParent* aTransactionParent,
                const uint64_t& aTransactionId,
                const WebAuthnMakeCredentialInfo& aTransactionInfo);
  void Sign(PWebAuthnTransactionParent* aTransactionParent,
            const uint64_t& aTransactionId,
            const WebAuthnGetAssertionInfo& aTransactionInfo);
  void Cancel(PWebAuthnTransactionParent* aTransactionParent,
              const Tainted<uint64_t>& aTransactionId);
  void MaybeClearTransaction(PWebAuthnTransactionParent* aParent);
  static void Initialize();
  static bool IsUserVerifyingPlatformAuthenticatorAvailable();
  static bool AreWebAuthNApisAvailable();

  WinWebAuthnManager();
  ~WinWebAuthnManager();

 private:
  void AbortTransaction(const uint64_t& aTransactionId, const nsresult& aError);
  void ClearTransaction();
  void MaybeAbortRegister(const uint64_t& aTransactionId,
                          const nsresult& aError);
  void MaybeAbortSign(const uint64_t& aTransactionId, const nsresult& aError);
  bool IsUserVerifyingPlatformAuthenticatorAvailableInternal();
  uint32_t GetWebAuthNApiVersion();

  PWebAuthnTransactionParent* mTransactionParent;
  uint32_t mWinWebAuthNApiVersion = 0;
  std::map<uint64_t, GUID*> mCancellationIds;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_WinWebAuthnManager_h

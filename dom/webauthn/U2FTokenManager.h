/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2FTokenManager_h
#define mozilla_dom_U2FTokenManager_h

#include "mozilla/dom/U2FTokenTransport.h"

/*
 * Parent process manager for U2F and WebAuthn API transactions. Handles process
 * transactions from all content processes, make sure only one transaction is
 * live at any time. Manages access to hardware and software based key systems.
 *
 * U2FTokenManager is created on the first access to functions of either the U2F
 * or WebAuthn APIs that require key registration or signing. It lives until the
 * end of the browser process.
 */

namespace mozilla {
namespace dom {

class U2FSoftTokenManager;
class WebAuthnTransactionParent;

class U2FTokenManager final
{
public:
  enum TransactionType
  {
    RegisterTransaction = 0,
    SignTransaction,
    NumTransactionTypes
  };
  NS_INLINE_DECL_REFCOUNTING(U2FTokenManager)
  static U2FTokenManager* Get();
  void Register(WebAuthnTransactionParent* aTransactionParent,
                const WebAuthnTransactionInfo& aTransactionInfo);
  void Sign(WebAuthnTransactionParent* aTransactionParent,
            const WebAuthnTransactionInfo& aTransactionInfo);
  void Cancel(WebAuthnTransactionParent* aTransactionParent);
  void MaybeClearTransaction(WebAuthnTransactionParent* aParent);
  static void Initialize();
private:
  U2FTokenManager();
  ~U2FTokenManager();
  RefPtr<U2FTokenTransport> GetTokenManagerImpl();
  void AbortTransaction(const nsresult& aError);
  void ClearTransaction();
  void MaybeAbortTransaction(uint64_t aTransactionId,
                             const nsresult& aError);
  void MaybeConfirmRegister(uint64_t aTransactionId,
                            const nsTArray<uint8_t>& aRegister);
  void MaybeConfirmSign(uint64_t aTransactionId,
                        const nsTArray<uint8_t>& aKeyHandle,
                        const nsTArray<uint8_t>& aSignature);
  // Using a raw pointer here, as the lifetime of the IPC object is managed by
  // the PBackground protocol code. This means we cannot be left holding an
  // invalid IPC protocol object after the transaction is finished.
  WebAuthnTransactionParent* mTransactionParent;
  RefPtr<U2FTokenTransport> mTokenManagerImpl;
  RefPtr<ResultPromise> mResultPromise;
  // Guards the asynchronous promise resolution of token manager impls.
  // We don't need to protect this with a lock as it will only be modified
  // and checked on the PBackground thread in the parent process.
  uint64_t mTransactionId;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2FTokenManager_h

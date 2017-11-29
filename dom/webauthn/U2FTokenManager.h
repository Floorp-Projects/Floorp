/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
  NS_INLINE_DECL_REFCOUNTING(U2FTokenManager)
  static U2FTokenManager* Get();
  void Register(PWebAuthnTransactionParent* aTransactionParent,
                const uint64_t& aTransactionId,
                const WebAuthnMakeCredentialInfo& aTransactionInfo);
  void Sign(PWebAuthnTransactionParent* aTransactionParent,
            const uint64_t& aTransactionId,
            const WebAuthnGetAssertionInfo& aTransactionInfo);
  void Cancel(PWebAuthnTransactionParent* aTransactionParent,
              const uint64_t& aTransactionId);
  void MaybeClearTransaction(PWebAuthnTransactionParent* aParent);
  static void Initialize();
private:
  U2FTokenManager();
  ~U2FTokenManager();
  RefPtr<U2FTokenTransport> GetTokenManagerImpl();
  void AbortTransaction(const uint64_t& aTransactionId, const nsresult& aError);
  void ClearTransaction();
  void MaybeConfirmRegister(const uint64_t& aTransactionId, U2FRegisterResult& aResult);
  void MaybeAbortRegister(const uint64_t& aTransactionId, const nsresult& aError);
  void MaybeConfirmSign(const uint64_t& aTransactionId, U2FSignResult& aResult);
  void MaybeAbortSign(const uint64_t& aTransactionId, const nsresult& aError);
  // Using a raw pointer here, as the lifetime of the IPC object is managed by
  // the PBackground protocol code. This means we cannot be left holding an
  // invalid IPC protocol object after the transaction is finished.
  PWebAuthnTransactionParent* mTransactionParent;
  RefPtr<U2FTokenTransport> mTokenManagerImpl;
  MozPromiseRequestHolder<U2FRegisterPromise> mRegisterPromise;
  MozPromiseRequestHolder<U2FSignPromise> mSignPromise;
  // The last transaction id, non-zero if there's an active transaction. This
  // guards any cancel messages to ensure we don't cancel newer transactions
  // due to a stale message.
  uint64_t mLastTransactionId;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2FTokenManager_h

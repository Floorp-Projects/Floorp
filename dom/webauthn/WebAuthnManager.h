/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnManager_h
#define mozilla_dom_WebAuthnManager_h

#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RandomNum.h"
#include "mozilla/dom/AbortSignal.h"
#include "mozilla/dom/PWebAuthnTransaction.h"
#include "mozilla/dom/WebAuthnManagerBase.h"

/*
 * Content process manager for the WebAuthn protocol. Created on calls to the
 * WebAuthentication DOM object, this manager handles establishing IPC channels
 * for WebAuthn transactions, as well as keeping track of JS Promise objects
 * representing transactions in flight.
 *
 * The WebAuthn spec (https://www.w3.org/TR/webauthn/) allows for two different
 * types of transactions: registration and signing. When either of these is
 * requested via the DOM API, the following steps are executed in the
 * WebAuthnManager:
 *
 * - Validation of the request. Return a failed promise to js if request does
 *   not have correct parameters.
 *
 * - If request is valid, open a new IPC channel for running the transaction. If
 *   another transaction is already running in this content process, cancel it.
 *   Return a pending promise to js.
 *
 * - Send transaction information to parent process (by running the Start*
 *   functions of WebAuthnManager). Assuming another transaction is currently in
 *   flight in another content process, parent will handle canceling it.
 *
 * - On return of successful transaction information from parent process, turn
 *   information into DOM object format required by spec, and resolve promise
 *   (by running the Finish* functions of WebAuthnManager). On cancellation
 *   request from parent, reject promise with corresponding error code. Either
 *   outcome will also close the IPC channel.
 *
 */

namespace mozilla::dom {

class Credential;

class WebAuthnTransaction {
 public:
  explicit WebAuthnTransaction(const RefPtr<Promise>& aPromise)
      : mPromise(aPromise), mId(NextId()), mVisibilityChanged(false) {
    MOZ_ASSERT(mId > 0);
  }

  // JS Promise representing the transaction status.
  RefPtr<Promise> mPromise;

  // Unique transaction id.
  uint64_t mId;

  // Whether or not visibility has changed for the window during this
  // transaction
  bool mVisibilityChanged;

 private:
  // Generates a probabilistically unique ID for the new transaction. IDs are 53
  // bits, as they are used in javascript. We use a random value if possible,
  // otherwise a counter.
  static uint64_t NextId() {
    static uint64_t counter = 0;
    Maybe<uint64_t> rand = mozilla::RandomUint64();
    uint64_t id =
        rand.valueOr(++counter) & UINT64_C(0x1fffffffffffff);  // 2^53 - 1
    // The transaction ID 0 is reserved.
    return id ? id : 1;
  }
};

class WebAuthnManager final : public WebAuthnManagerBase, public AbortFollower {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(WebAuthnManager, WebAuthnManagerBase)

  explicit WebAuthnManager(nsPIDOMWindowInner* aParent)
      : WebAuthnManagerBase(aParent) {}

  already_AddRefed<Promise> MakeCredential(
      const PublicKeyCredentialCreationOptions& aOptions,
      const Optional<OwningNonNull<AbortSignal>>& aSignal, ErrorResult& aError);

  already_AddRefed<Promise> GetAssertion(
      const PublicKeyCredentialRequestOptions& aOptions,
      const Optional<OwningNonNull<AbortSignal>>& aSignal, ErrorResult& aError);

  already_AddRefed<Promise> Store(const Credential& aCredential,
                                  ErrorResult& aError);

  already_AddRefed<Promise> IsUVPAA(GlobalObject& aGlobal, ErrorResult& aError);

  // WebAuthnManagerBase

  void FinishMakeCredential(
      const uint64_t& aTransactionId,
      const WebAuthnMakeCredentialResult& aResult) override;

  void FinishGetAssertion(const uint64_t& aTransactionId,
                          const WebAuthnGetAssertionResult& aResult) override;

  void RequestAborted(const uint64_t& aTransactionId,
                      const nsresult& aError) override;

  // AbortFollower

  void RunAbortAlgorithm() override;

 protected:
  // Upon a visibility change, makes note of it in the current transaction.
  void HandleVisibilityChange() override;

 private:
  virtual ~WebAuthnManager();

  // Send a Cancel message to the parent, reject the promise with the given
  // reason (an nsresult or JS::Handle<JS::Value>), and clear the transaction.
  template <typename T>
  void CancelTransaction(const T& aReason) {
    CancelParent();
    RejectTransaction(aReason);
  }

  // Reject the promise with the given reason (an nsresult or JS::Value), and
  // clear the transaction.
  template <typename T>
  void RejectTransaction(const T& aReason) {
    if (!NS_WARN_IF(mTransaction.isNothing())) {
      mTransaction.ref().mPromise->MaybeReject(aReason);
    }

    ClearTransaction();
  }

  // Send a Cancel message to the parent.
  void CancelParent();

  // Clears all information we have about the current transaction.
  void ClearTransaction();

  // The current transaction, if any.
  Maybe<WebAuthnTransaction> mTransaction;
};

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    WebAuthnTransaction& aTransaction, const char* aName, uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(aCallback, aTransaction.mPromise, aName, aFlags);
}

inline void ImplCycleCollectionUnlink(WebAuthnTransaction& aTransaction) {
  ImplCycleCollectionUnlink(aTransaction.mPromise);
}

}  // namespace mozilla::dom

#endif  // mozilla_dom_WebAuthnManager_h

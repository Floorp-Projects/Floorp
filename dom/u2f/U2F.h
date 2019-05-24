/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2F_h
#define mozilla_dom_U2F_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/U2FBinding.h"
#include "mozilla/dom/WebAuthnManagerBase.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "nsProxyRelease.h"
#include "nsWrapperCache.h"
#include "U2FAuthenticator.h"

namespace mozilla {
namespace dom {

class WebAuthnMakeCredentialResult;
class WebAuthnGetAssertionResult;

class U2FRegisterCallback;
class U2FSignCallback;

// Defined in U2FBinding.h by the U2F.webidl; their use requires a JSContext.
struct RegisterRequest;
struct RegisteredKey;

class U2FTransaction {
  typedef Variant<nsMainThreadPtrHandle<U2FRegisterCallback>,
                  nsMainThreadPtrHandle<U2FSignCallback>>
      U2FCallback;

 public:
  explicit U2FTransaction(const U2FCallback&& aCallback)
      : mCallback(std::move(aCallback)),
        mId(NextId()),
        mVisibilityChanged(false) {
    MOZ_ASSERT(mId > 0);
  }

  bool HasRegisterCallback() {
    return mCallback.is<nsMainThreadPtrHandle<U2FRegisterCallback>>();
  }

  auto& GetRegisterCallback() {
    return mCallback.as<nsMainThreadPtrHandle<U2FRegisterCallback>>();
  }

  bool HasSignCallback() {
    return mCallback.is<nsMainThreadPtrHandle<U2FSignCallback>>();
  }

  auto& GetSignCallback() {
    return mCallback.as<nsMainThreadPtrHandle<U2FSignCallback>>();
  }

  // The callback passed to the API.
  U2FCallback mCallback;

  // Unique transaction id.
  uint64_t mId;

  // Whether or not visibility has changed for the window during this
  // transaction
  bool mVisibilityChanged;

 private:
  // Generates a unique id for new transactions. This doesn't have to be unique
  // forever, it's sufficient to differentiate between temporally close
  // transactions, where messages can intersect. Can overflow.
  static uint64_t NextId() {
    static uint64_t id = 0;
    return ++id;
  }
};

class U2F final : public WebAuthnManagerBase, public nsWrapperCache {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(U2F,
                                                         WebAuthnManagerBase)

  explicit U2F(nsPIDOMWindowInner* aParent) : WebAuthnManagerBase(aParent) {}

  nsPIDOMWindowInner* GetParentObject() const { return mParent; }

  void Init(ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  MOZ_CAN_RUN_SCRIPT
  void Register(const nsAString& aAppId,
                const Sequence<RegisterRequest>& aRegisterRequests,
                const Sequence<RegisteredKey>& aRegisteredKeys,
                U2FRegisterCallback& aCallback,
                const Optional<Nullable<int32_t>>& opt_aTimeoutSeconds,
                ErrorResult& aRv);

  void GetRegister(JSContext* aCx, JS::MutableHandle<JSObject*> aRegisterFunc,
                   ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT
  void Sign(const nsAString& aAppId, const nsAString& aChallenge,
            const Sequence<RegisteredKey>& aRegisteredKeys,
            U2FSignCallback& aCallback,
            const Optional<Nullable<int32_t>>& opt_aTimeoutSeconds,
            ErrorResult& aRv);

  void GetSign(JSContext* aCx, JS::MutableHandle<JSObject*> aSignFunc,
               ErrorResult& aRv);

  // WebAuthnManagerBase

  MOZ_CAN_RUN_SCRIPT
  void FinishMakeCredential(
      const uint64_t& aTransactionId,
      const WebAuthnMakeCredentialResult& aResult) override;

  MOZ_CAN_RUN_SCRIPT
  void FinishGetAssertion(const uint64_t& aTransactionId,
                          const WebAuthnGetAssertionResult& aResult) override;

  MOZ_CAN_RUN_SCRIPT
  void RequestAborted(const uint64_t& aTransactionId,
                      const nsresult& aError) override;

 protected:
  // Cancels the current transaction (by sending a Cancel message to the
  // parent) and rejects it by calling RejectTransaction().
  MOZ_CAN_RUN_SCRIPT void CancelTransaction(const nsresult& aError);
  // Upon a visibility change, makes note of it in the current transaction.
  MOZ_CAN_RUN_SCRIPT void HandleVisibilityChange() override;

 private:
  MOZ_CAN_RUN_SCRIPT ~U2F();

  template <typename T, typename C>
  MOZ_CAN_RUN_SCRIPT void ExecuteCallback(T& aResp,
                                          nsMainThreadPtrHandle<C>& aCb);

  // Rejects the current transaction and clears it.
  MOZ_CAN_RUN_SCRIPT void RejectTransaction(const nsresult& aError);

  // Clears all information we have about the current transaction.
  void ClearTransaction();

  nsString mOrigin;

  // The current transaction, if any.
  Maybe<U2FTransaction> mTransaction;
};

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, U2FTransaction& aTransaction,
    const char* aName, uint32_t aFlags = 0) {
  if (aTransaction.HasRegisterCallback()) {
    CycleCollectionNoteChild(
        aCallback, aTransaction.GetRegisterCallback().get(), aName, aFlags);
  } else {
    CycleCollectionNoteChild(aCallback, aTransaction.GetSignCallback().get(),
                             aName, aFlags);
  }
}

inline void ImplCycleCollectionUnlink(U2FTransaction& aTransaction) {
  if (aTransaction.HasRegisterCallback()) {
    aTransaction.GetRegisterCallback() = nullptr;
  } else {
    aTransaction.GetSignCallback() = nullptr;
  }
}

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_U2F_h

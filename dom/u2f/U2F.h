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
#include "mozilla/ErrorResult.h"
#include "mozilla/MozPromise.h"
#include "nsProxyRelease.h"
#include "nsWrapperCache.h"
#include "U2FAuthenticator.h"
#include "nsIDOMEventListener.h"

class nsISerialEventTarget;

namespace mozilla {
namespace dom {

class U2FTransactionChild;
class U2FRegisterCallback;
class U2FSignCallback;

// Defined in U2FBinding.h by the U2F.webidl; their use requires a JSContext.
struct RegisterRequest;
struct RegisteredKey;

class U2FTransaction
{
public:
  explicit U2FTransaction(const nsCString& aClientData)
    : mClientData(aClientData)
    , mId(NextId())
  {
    MOZ_ASSERT(mId > 0);
  }

  // Client data used to assemble reply objects.
  nsCString mClientData;

  // Unique transaction id.
  uint64_t mId;

private:
  // Generates a unique id for new transactions. This doesn't have to be unique
  // forever, it's sufficient to differentiate between temporally close
  // transactions, where messages can intersect. Can overflow.
  static uint64_t NextId() {
    static uint64_t id = 0;
    return ++id;
  }
};

class U2F final : public nsIDOMEventListener
                , public nsWrapperCache
{
public:
  NS_DECL_NSIDOMEVENTLISTENER

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(U2F)

  explicit U2F(nsPIDOMWindowInner* aParent);

  nsPIDOMWindowInner*
  GetParentObject() const
  {
    return mParent;
  }

  void
  Init(ErrorResult& aRv);

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void
  Register(const nsAString& aAppId,
           const Sequence<RegisterRequest>& aRegisterRequests,
           const Sequence<RegisteredKey>& aRegisteredKeys,
           U2FRegisterCallback& aCallback,
           const Optional<Nullable<int32_t>>& opt_aTimeoutSeconds,
           ErrorResult& aRv);

  void
  Sign(const nsAString& aAppId,
       const nsAString& aChallenge,
       const Sequence<RegisteredKey>& aRegisteredKeys,
       U2FSignCallback& aCallback,
       const Optional<Nullable<int32_t>>& opt_aTimeoutSeconds,
       ErrorResult& aRv);

  void
  FinishRegister(const uint64_t& aTransactionId, nsTArray<uint8_t>& aRegBuffer);

  void
  FinishSign(const uint64_t& aTransactionId,
             nsTArray<uint8_t>& aCredentialId,
             nsTArray<uint8_t>& aSigBuffer);

  void
  RequestAborted(const uint64_t& aTransactionId, const nsresult& aError);

  void ActorDestroyed();

private:
  ~U2F();

  // Visibility event handling.
  void ListenForVisibilityEvents();
  void StopListeningForVisibilityEvents();

  // Clears all information we have about the current transaction.
  void ClearTransaction();
  // Rejects the current transaction and calls ClearTransaction().
  void RejectTransaction(const nsresult& aError);
  // Cancels the current transaction (by sending a Cancel message to the
  // parent) and rejects it by calling RejectTransaction().
  void CancelTransaction(const nsresult& aError);

  bool MaybeCreateBackgroundActor();

  nsString mOrigin;
  nsCOMPtr<nsPIDOMWindowInner> mParent;

  // U2F API callbacks.
  Maybe<nsMainThreadPtrHandle<U2FRegisterCallback>> mRegisterCallback;
  Maybe<nsMainThreadPtrHandle<U2FSignCallback>> mSignCallback;

  // IPC Channel to the parent process.
  RefPtr<U2FTransactionChild> mChild;

  // The current transaction, if any.
  Maybe<U2FTransaction> mTransaction;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2F_h

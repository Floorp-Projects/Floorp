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
#include "mozilla/MozPromise.h"
#include "nsProxyRelease.h"
#include "nsWrapperCache.h"
#include "U2FAuthenticator.h"

namespace mozilla {
namespace dom {

class U2FRegisterCallback;
class U2FSignCallback;

// Defined in U2FBinding.h by the U2F.webidl; their use requires a JSContext.
struct RegisterRequest;
struct RegisteredKey;

class U2FTransaction
{
  typedef Variant<nsMainThreadPtrHandle<U2FRegisterCallback>,
                  nsMainThreadPtrHandle<U2FSignCallback>> U2FCallback;

public:
  explicit U2FTransaction(const nsCString& aClientData,
                          const U2FCallback&& aCallback)
    : mClientData(aClientData)
    , mCallback(Move(aCallback))
    , mId(NextId())
  {
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

  // Client data used to assemble reply objects.
  nsCString mClientData;

  // The callback passed to the API.
  U2FCallback mCallback;

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

class U2F final : public WebAuthnManagerBase
                , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(U2F)

  explicit U2F(nsPIDOMWindowInner* aParent)
    : WebAuthnManagerBase(aParent)
  { }

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

  // WebAuthnManagerBase

  void
  FinishMakeCredential(const uint64_t& aTransactionId,
                       nsTArray<uint8_t>& aRegBuffer) override;

  void
  FinishGetAssertion(const uint64_t& aTransactionId,
                     nsTArray<uint8_t>& aCredentialId,
                     nsTArray<uint8_t>& aSigBuffer) override;

  void
  RequestAborted(const uint64_t& aTransactionId,
                 const nsresult& aError) override;

protected:
  // Cancels the current transaction (by sending a Cancel message to the
  // parent) and rejects it by calling RejectTransaction().
  void CancelTransaction(const nsresult& aError) override;

private:
  ~U2F();

  template<typename T, typename C>
  void ExecuteCallback(T& aResp, nsMainThreadPtrHandle<C>& aCb);

  // Clears all information we have about the current transaction.
  void ClearTransaction();
  // Rejects the current transaction and clears it.
  void RejectTransaction(const nsresult& aError);

  nsString mOrigin;

  // The current transaction, if any.
  Maybe<U2FTransaction> mTransaction;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2F_h

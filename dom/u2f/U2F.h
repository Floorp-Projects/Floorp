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

class nsISerialEventTarget;

namespace mozilla {
namespace dom {

class U2FRegisterCallback;
class U2FSignCallback;

// Defined in U2FBinding.h by the U2F.webidl; their use requires a JSContext.
struct RegisterRequest;
struct RegisteredKey;

// The U2F Class is used by the JS engine to initiate U2F operations.
class U2F final : public nsISupports
                , public nsWrapperCache
{
public:
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

private:
  void
  Cancel();

  nsString mOrigin;
  nsCOMPtr<nsPIDOMWindowInner> mParent;
  nsCOMPtr<nsISerialEventTarget> mEventTarget;
  Maybe<nsMainThreadPtrHandle<U2FRegisterCallback>> mRegisterCallback;
  Maybe<nsMainThreadPtrHandle<U2FSignCallback>> mSignCallback;
  MozPromiseRequestHolder<U2FPromise> mPromiseHolder;

  ~U2F();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2F_h

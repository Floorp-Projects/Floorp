/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Lock_h
#define mozilla_dom_Lock_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/LockManagerBinding.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class LockManager;

class Lock final : public PromiseNativeHandler, public nsWrapperCache {
  friend class LockManager;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Lock)

  Lock(nsIGlobalObject* aGlobal, const RefPtr<LockManager>& aLockManager,
       const nsString& aName, LockMode aMode,
       const RefPtr<Promise>& aReleasedPromise, ErrorResult& aRv);

  bool operator==(const Lock& aOther) const {
    // This operator is needed to remove released locks from the queue.
    // This assumes each lock has a unique promise
    MOZ_ASSERT(mReleasedPromise && aOther.mReleasedPromise,
               "Promises are null when locks are unreleased??");
    return mReleasedPromise == aOther.mReleasedPromise;
  }

 protected:
  ~Lock() = default;

 public:
  nsIGlobalObject* GetParentObject() const { return mOwner; };

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void GetName(nsString& aRetVal) const;

  LockMode Mode() const;

  Promise& GetWaitingPromise();

  // PromiseNativeHandler
  virtual void ResolvedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override;
  virtual void RejectedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override;

 private:
  nsCOMPtr<nsIGlobalObject> mOwner;
  RefPtr<LockManager> mLockManager;

  nsString mName;
  LockMode mMode;
  RefPtr<Promise> mWaitingPromise;
  RefPtr<Promise> mReleasedPromise;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_Lock_h

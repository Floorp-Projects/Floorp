/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_LockManager_h
#define mozilla_dom_LockManager_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Lock.h"
#include "mozilla/dom/LockManagerBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsHashKeys.h"
#include "nsTHashMap.h"
#include "nsTHashSet.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla::dom {

class LockGrantedCallback;
struct LockOptions;
class Promise;

struct LockRequest {
  // agent;
  // clientId;
  // origin;
  nsString mName;
  LockMode mMode;
  RefPtr<Promise> mPromise;
  RefPtr<LockGrantedCallback> mCallback;

  bool operator==(const LockRequest& aOther) const {
    // This operator is needed to remove aborted requests from the queue.
    // This assumes each request has a unique promise, which should be true
    // since each creates a new one.
    MOZ_ASSERT(mPromise && aOther.mPromise,
               "Promises are null when requests are still active??");
    return mPromise == aOther.mPromise;
  }
};

class LockManager final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(LockManager)

  explicit LockManager(nsIGlobalObject* aGlobal) : mOwner(aGlobal) {}

 protected:
  ~LockManager() = default;

 public:
  nsIGlobalObject* GetParentObject() const { return mOwner; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void Shutdown();

  already_AddRefed<Promise> Request(const nsAString& name,
                                    LockGrantedCallback& callback,
                                    ErrorResult& aRv);
  already_AddRefed<Promise> Request(const nsAString& name,
                                    const LockOptions& options,
                                    LockGrantedCallback& callback,
                                    ErrorResult& aRv);

  already_AddRefed<Promise> Query(ErrorResult& aRv);

  void ReleaseHeldLock(Lock* aLock);

 private:
  void ProcessRequestQueue(nsTArray<mozilla::dom::LockRequest>& aQueue);
  bool IsGrantableRequest(const LockRequest& aRequest,
                          nsTArray<mozilla::dom::LockRequest>& aQueue);
  bool HasBlockingHeldLock(const nsString& aName, LockMode aMode);

  nsCOMPtr<nsIGlobalObject> mOwner;

  // TODO: These should be behind IPC
  // XXX: Before that these should be cycle collected or explicitly cleared
  nsTHashSet<RefPtr<Lock>> mHeldLockSet;
  nsTHashMap<nsStringHashKey, nsTArray<LockRequest>> mQueueMap;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_LockManager_h

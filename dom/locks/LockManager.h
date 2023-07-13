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
#include "mozilla/dom/WorkerRef.h"
#include "nsCycleCollectionParticipant.h"
#include "nsHashKeys.h"
#include "nsTHashMap.h"
#include "nsTHashSet.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla::dom {

class LockGrantedCallback;
struct LockOptions;

namespace locks {
class LockManagerChild;
}

class LockManager final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(LockManager)

 private:
  explicit LockManager(nsIGlobalObject* aGlobal);

 public:
  static already_AddRefed<LockManager> Create(nsIGlobalObject& aGlobal);

  nsIGlobalObject* GetParentObject() const { return mOwner; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise> Request(const nsAString& aName,
                                    LockGrantedCallback& aCallback,
                                    ErrorResult& aRv);
  already_AddRefed<Promise> Request(const nsAString& aName,
                                    const LockOptions& aOptions,
                                    LockGrantedCallback& aCallback,
                                    ErrorResult& aRv);

  already_AddRefed<Promise> Query(ErrorResult& aRv);

  void Shutdown();

 private:
  ~LockManager() = default;

  nsCOMPtr<nsIGlobalObject> mOwner;
  RefPtr<locks::LockManagerChild> mActor;

  // Revokes itself and triggers LockManagerChild deletion on worker shutdown
  // callback.
  RefPtr<WeakWorkerRef> mWorkerRef;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_LockManager_h

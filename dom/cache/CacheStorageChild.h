/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheStorageChild_h
#define mozilla_dom_cache_CacheStorageChild_h

#include "mozilla/dom/cache/ActorChild.h"
#include "mozilla/dom/cache/Types.h"
#include "mozilla/dom/cache/PCacheStorageChild.h"

class nsIGlobalObject;

namespace mozilla::dom {

class Promise;

namespace cache {

class CacheOpArgs;
class CacheStorage;
class CacheWorkerRef;
class PCacheChild;

class CacheStorageChild final : public PCacheStorageChild, public ActorChild {
  friend class PCacheStorageChild;

 public:
  CacheStorageChild(CacheStorage* aListener,
                    SafeRefPtr<CacheWorkerRef> aWorkerRef);

  // Must be called by the associated CacheStorage listener in its
  // DestroyInternal() method.  Also, CacheStorage must call
  // SendDestroyFromListener() on the actor in its destructor to trigger
  // ActorDestroy() if it has not been called yet.
  void ClearListener();

  void ExecuteOp(nsIGlobalObject* aGlobal, Promise* aPromise,
                 nsISupports* aParent, const CacheOpArgs& aArgs);

  // Our parent Listener object has gone out of scope and is being destroyed.
  void StartDestroyFromListener();

  NS_INLINE_DECL_REFCOUNTING(CacheStorageChild, override)
  void NoteDeletedActor() override;

 private:
  ~CacheStorageChild();

  void DestroyInternal();

  // ActorChild methods
  // CacheWorkerRef is trying to destroy due to worker shutdown.
  virtual void StartDestroy() override;

  // PCacheStorageChild methods
  virtual void ActorDestroy(ActorDestroyReason aReason) override;

  PCacheOpChild* AllocPCacheOpChild(const CacheOpArgs& aOpArgs);

  bool DeallocPCacheOpChild(PCacheOpChild* aActor);

  // utility methods
  inline uint32_t NumChildActors() { return ManagedPCacheOpChild().Count(); }

  // Use a weak ref so actor does not hold DOM object alive past content use.
  // The CacheStorage object must call ClearListener() to null this before its
  // destroyed.
  CacheStorage* MOZ_NON_OWNING_REF mListener;
  bool mDelayedDestroy;
};

}  // namespace cache
}  // namespace mozilla::dom

#endif  // mozilla_dom_cache_CacheStorageChild_h

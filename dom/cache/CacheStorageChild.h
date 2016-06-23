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

namespace mozilla {
namespace dom {

class Promise;

namespace cache {

class CacheOpArgs;
class CacheStorage;
class CacheWorkerHolder;
class PCacheChild;

class CacheStorageChild final : public PCacheStorageChild
                              , public ActorChild
{
public:
  CacheStorageChild(CacheStorage* aListener, CacheWorkerHolder* aWorkerHolder);
  ~CacheStorageChild();

  // Must be called by the associated CacheStorage listener in its
  // DestroyInternal() method.  Also, CacheStorage must call
  // SendDestroyFromListener() on the actor in its destructor to trigger
  // ActorDestroy() if it has not been called yet.
  void ClearListener();

  void
  ExecuteOp(nsIGlobalObject* aGlobal, Promise* aPromise,
            nsISupports* aParent, const CacheOpArgs& aArgs);

  // Our parent Listener object has gone out of scope and is being destroyed.
  void StartDestroyFromListener();

private:
  // ActorChild methods

  // CacheWorkerHolder is trying to destroy due to worker shutdown.
  virtual void StartDestroy() override;

  // PCacheStorageChild methods
  virtual void ActorDestroy(ActorDestroyReason aReason) override;

  virtual PCacheOpChild*
  AllocPCacheOpChild(const CacheOpArgs& aOpArgs) override;

  virtual bool
  DeallocPCacheOpChild(PCacheOpChild* aActor) override;

  // utility methods
  void
  NoteDeletedActor();

  // Use a weak ref so actor does not hold DOM object alive past content use.
  // The CacheStorage object must call ClearListener() to null this before its
  // destroyed.
  CacheStorage* MOZ_NON_OWNING_REF mListener;
  uint32_t mNumChildActors;
  bool mDelayedDestroy;

  NS_DECL_OWNINGTHREAD
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStorageChild_h

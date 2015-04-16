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
class PCacheChild;
class Feature;

class CacheStorageChild final : public PCacheStorageChild
                              , public ActorChild
{
public:
  CacheStorageChild(CacheStorage* aListener, Feature* aFeature);
  ~CacheStorageChild();

  // Must be called by the associated CacheStorage listener in its
  // ActorDestroy() method.  Also, CacheStorage must call SendDestroy() on the
  // actor in its destructor to trigger ActorDestroy() if it has not been
  // called yet.
  void ClearListener();

  void
  ExecuteOp(nsIGlobalObject* aGlobal, Promise* aPromise,
            const CacheOpArgs& aArgs);

  // ActorChild methods

  // Synchronously call ActorDestroy on our CacheStorage listener and then start
  // the actor destruction asynchronously from the parent-side.
  virtual void StartDestroy() override;

private:
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

  NS_DECL_OWNINGTHREAD
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStorageChild_h

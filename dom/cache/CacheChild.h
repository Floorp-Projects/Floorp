/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheChild_h
#define mozilla_dom_cache_CacheChild_h

#include "mozilla/dom/cache/ActorChild.h"
#include "mozilla/dom/cache/PCacheChild.h"

class nsIAsyncInputStream;
class nsIGlobalObject;

namespace mozilla {
namespace dom {

class Promise;

namespace cache {

class Cache;
class CacheOpArgs;
class CachePushStreamChild;

class CacheChild final : public PCacheChild
                       , public ActorChild
{
public:
  CacheChild();
  ~CacheChild();

  void SetListener(Cache* aListener);

  // Must be called by the associated Cache listener in its ActorDestroy()
  // method.  Also, Cache must Send__delete__() the actor in its destructor to
  // trigger ActorDestroy() if it has not been called yet.
  void ClearListener();

  void
  ExecuteOp(nsIGlobalObject* aGlobal, Promise* aPromise,
            const CacheOpArgs& aArgs);

  CachePushStreamChild*
  CreatePushStream(nsIAsyncInputStream* aStream);

  // ActorChild methods

  // Synchronously call ActorDestroy on our Cache listener and then start the
  // actor destruction asynchronously from the parent-side.
  virtual void StartDestroy() override;

private:
  // PCacheChild methods
  virtual void
  ActorDestroy(ActorDestroyReason aReason) override;

  virtual PCacheOpChild*
  AllocPCacheOpChild(const CacheOpArgs& aOpArgs) override;

  virtual bool
  DeallocPCacheOpChild(PCacheOpChild* aActor) override;

  virtual PCachePushStreamChild*
  AllocPCachePushStreamChild() override;

  virtual bool
  DeallocPCachePushStreamChild(PCachePushStreamChild* aActor) override;

  // utility methods
  void
  NoteDeletedActor();

  // Use a weak ref so actor does not hold DOM object alive past content use.
  // The Cache object must call ClearListener() to null this before its
  // destroyed.
  Cache* MOZ_NON_OWNING_REF mListener;
  uint32_t mNumChildActors;

  NS_DECL_OWNINGTHREAD
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheChild_h

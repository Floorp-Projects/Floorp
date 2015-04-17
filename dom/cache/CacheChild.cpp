/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheChild.h"

#include "mozilla/unused.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/cache/Cache.h"
#include "mozilla/dom/cache/CacheOpChild.h"
#include "mozilla/dom/cache/CachePushStreamChild.h"

namespace mozilla {
namespace dom {
namespace cache {

// Declared in ActorUtils.h
PCacheChild*
AllocPCacheChild()
{
  return new CacheChild();
}

// Declared in ActorUtils.h
void
DeallocPCacheChild(PCacheChild* aActor)
{
  delete aActor;
}

CacheChild::CacheChild()
  : mListener(nullptr)
  , mNumChildActors(0)
{
  MOZ_COUNT_CTOR(cache::CacheChild);
}

CacheChild::~CacheChild()
{
  MOZ_COUNT_DTOR(cache::CacheChild);
  NS_ASSERT_OWNINGTHREAD(CacheChild);
  MOZ_ASSERT(!mListener);
  MOZ_ASSERT(!mNumChildActors);
}

void
CacheChild::SetListener(Cache* aListener)
{
  NS_ASSERT_OWNINGTHREAD(CacheChild);
  MOZ_ASSERT(!mListener);
  mListener = aListener;
  MOZ_ASSERT(mListener);
}

void
CacheChild::ClearListener()
{
  NS_ASSERT_OWNINGTHREAD(CacheChild);
  MOZ_ASSERT(mListener);
  mListener = nullptr;
}

void
CacheChild::ExecuteOp(nsIGlobalObject* aGlobal, Promise* aPromise,
                      const CacheOpArgs& aArgs)
{
  mNumChildActors += 1;
  MOZ_ALWAYS_TRUE(SendPCacheOpConstructor(
    new CacheOpChild(GetFeature(), aGlobal, aPromise), aArgs));
}

CachePushStreamChild*
CacheChild::CreatePushStream(nsIAsyncInputStream* aStream)
{
  mNumChildActors += 1;
  auto actor = SendPCachePushStreamConstructor(
    new CachePushStreamChild(GetFeature(), aStream));
  MOZ_ASSERT(actor);
  return static_cast<CachePushStreamChild*>(actor);
}

void
CacheChild::StartDestroy()
{
  nsRefPtr<Cache> listener = mListener;

  // StartDestroy() can get called from either Cache or the Feature.
  // Theoretically we can get double called if the right race happens.  Handle
  // that by just ignoring the second StartDestroy() call.
  if (!listener) {
    return;
  }

  listener->DestroyInternal(this);

  // Cache listener should call ClearListener() in DestroyInternal()
  MOZ_ASSERT(!mListener);

  // If we have outstanding child actors, then don't destroy ourself yet.
  // The child actors should be short lived and we should allow them to complete
  // if possible.  SendTeardown() will be called when the count drops to zero
  // in NoteDeletedActor().
  if (mNumChildActors) {
    return;
  }

  // Start actor destruction from parent process
  unused << SendTeardown();
}

void
CacheChild::ActorDestroy(ActorDestroyReason aReason)
{
  NS_ASSERT_OWNINGTHREAD(CacheChild);
  nsRefPtr<Cache> listener = mListener;
  if (listener) {
    listener->DestroyInternal(this);
    // Cache listener should call ClearListener() in DestroyInternal()
    MOZ_ASSERT(!mListener);
  }

  RemoveFeature();
}

PCacheOpChild*
CacheChild::AllocPCacheOpChild(const CacheOpArgs& aOpArgs)
{
  MOZ_CRASH("CacheOpChild should be manually constructed.");
  return nullptr;
}

bool
CacheChild::DeallocPCacheOpChild(PCacheOpChild* aActor)
{
  delete aActor;
  NoteDeletedActor();
  return true;
}

PCachePushStreamChild*
CacheChild::AllocPCachePushStreamChild()
{
  MOZ_CRASH("CachePushStreamChild should be manually constructed.");
  return nullptr;
}

bool
CacheChild::DeallocPCachePushStreamChild(PCachePushStreamChild* aActor)
{
  delete aActor;
  NoteDeletedActor();
  return true;
}

void
CacheChild::NoteDeletedActor()
{
  mNumChildActors -= 1;
  if (!mNumChildActors && !mListener) {
    unused << SendTeardown();
  }
}

} // namespace cache
} // namespace dom
} // namesapce mozilla

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
  , mDelayedDestroy(false)
  , mLocked(false)
{
  MOZ_COUNT_CTOR(cache::CacheChild);
}

CacheChild::~CacheChild()
{
  MOZ_COUNT_DTOR(cache::CacheChild);
  NS_ASSERT_OWNINGTHREAD(CacheChild);
  MOZ_ASSERT(!mListener);
  MOZ_ASSERT(!mNumChildActors);
  MOZ_ASSERT(!mLocked);
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
                      nsISupports* aParent, const CacheOpArgs& aArgs)
{
  mNumChildActors += 1;
  MOZ_ALWAYS_TRUE(SendPCacheOpConstructor(
    new CacheOpChild(GetFeature(), aGlobal, aParent, aPromise), aArgs));
}

void
CacheChild::StartDestroyFromListener()
{
  NS_ASSERT_OWNINGTHREAD(CacheChild);

  // The listener should be held alive by any async operations, so if it
  // is going away then there must not be any child actors.  This in turn
  // ensures that StartDestroy() will not trigger the delayed path.
  MOZ_ASSERT(!mNumChildActors);

  StartDestroy();
}

void
CacheChild::StartDestroy()
{
  NS_ASSERT_OWNINGTHREAD(CacheChild);

  // If we have outstanding child actors, then don't destroy ourself yet.
  // The child actors should be short lived and we should allow them to complete
  // if possible.  NoteDeletedActor() will call back into this Shutdown()
  // method when the last child actor is gone.  Also, delay destruction if we
  // have been explicitly locked by someone using us on the stack.
  if (mNumChildActors || mLocked) {
    mDelayedDestroy = true;
    return;
  }

  RefPtr<Cache> listener = mListener;

  // StartDestroy() can get called from either Cache or the Feature.
  // Theoretically we can get double called if the right race happens.  Handle
  // that by just ignoring the second StartDestroy() call.
  if (!listener) {
    return;
  }

  listener->DestroyInternal(this);

  // Cache listener should call ClearListener() in DestroyInternal()
  MOZ_ASSERT(!mListener);

  // Start actor destruction from parent process
  Unused << SendTeardown();
}

void
CacheChild::ActorDestroy(ActorDestroyReason aReason)
{
  NS_ASSERT_OWNINGTHREAD(CacheChild);
  RefPtr<Cache> listener = mListener;
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

void
CacheChild::NoteDeletedActor()
{
  mNumChildActors -= 1;
  MaybeFlushDelayedDestroy();
}

void
CacheChild::MaybeFlushDelayedDestroy()
{
  if (!mNumChildActors && !mLocked && mDelayedDestroy) {
    StartDestroy();
  }
}

void
CacheChild::Lock()
{
  NS_ASSERT_OWNINGTHREAD(CacheChild);
  MOZ_ASSERT(!mLocked);
  mLocked = true;
}

void
CacheChild::Unlock()
{
  NS_ASSERT_OWNINGTHREAD(CacheChild);
  MOZ_ASSERT(mLocked);
  mLocked = false;
  MaybeFlushDelayedDestroy();
}

} // namespace cache
} // namespace dom
} // namespace mozilla

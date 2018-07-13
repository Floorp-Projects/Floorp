/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheWorkerHolder.h"

#include "mozilla/dom/cache/ActorChild.h"
#include "mozilla/dom/WorkerPrivate.h"

namespace mozilla {
namespace dom {
namespace cache {

// static
already_AddRefed<CacheWorkerHolder>
CacheWorkerHolder::Create(WorkerPrivate* aWorkerPrivate, Behavior aBehavior)
{
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);

  RefPtr<CacheWorkerHolder> workerHolder = new CacheWorkerHolder(aBehavior);
  if (NS_WARN_IF(!workerHolder->HoldWorker(aWorkerPrivate, Canceling))) {
    return nullptr;
  }

  return workerHolder.forget();
}

// static
already_AddRefed<CacheWorkerHolder>
CacheWorkerHolder::PreferBehavior(CacheWorkerHolder* aCurrentHolder,
                                  Behavior aBehavior)
{
  if (!aCurrentHolder) {
    return nullptr;
  }

  RefPtr<CacheWorkerHolder> orig = aCurrentHolder;
  if (orig->GetBehavior() == aBehavior) {
    return orig.forget();
  }

  RefPtr<CacheWorkerHolder> replace = Create(orig->mWorkerPrivate, aBehavior);
  if (!replace) {
    return orig.forget();
  }

  return replace.forget();
}

void
CacheWorkerHolder::AddActor(ActorChild* aActor)
{
  NS_ASSERT_OWNINGTHREAD(CacheWorkerHolder);
  MOZ_DIAGNOSTIC_ASSERT(aActor);
  MOZ_ASSERT(!mActorList.Contains(aActor));

  mActorList.AppendElement(aActor);

  // Allow an actor to be added after we've entered the Notifying case.  We
  // can't stop the actor creation from racing with out destruction of the
  // other actors and we need to wait for this extra one to close as well.
  // Signal it should destroy itself right away.
  if (mNotified) {
    aActor->StartDestroy();
  }
}

void
CacheWorkerHolder::RemoveActor(ActorChild* aActor)
{
  NS_ASSERT_OWNINGTHREAD(CacheWorkerHolder);
  MOZ_DIAGNOSTIC_ASSERT(aActor);

#if defined(RELEASE_OR_BETA)
  mActorList.RemoveElement(aActor);
#else
  MOZ_DIAGNOSTIC_ASSERT(mActorList.RemoveElement(aActor));
#endif

  MOZ_ASSERT(!mActorList.Contains(aActor));
}

bool
CacheWorkerHolder::Notified() const
{
  return mNotified;
}

bool
CacheWorkerHolder::Notify(WorkerStatus aStatus)
{
  NS_ASSERT_OWNINGTHREAD(CacheWorkerHolder);

  // When the service worker thread is stopped we will get Canceling,
  // but nothing higher than that.  We must shut things down at Canceling.
  if (aStatus < Canceling || mNotified) {
    return true;
  }

  mNotified = true;

  // Start the asynchronous destruction of our actors.  These will call back
  // into RemoveActor() once the actor is destroyed.
  for (uint32_t i = 0; i < mActorList.Length(); ++i) {
    MOZ_DIAGNOSTIC_ASSERT(mActorList[i]);
    mActorList[i]->StartDestroy();
  }

  return true;
}

CacheWorkerHolder::CacheWorkerHolder(Behavior aBehavior)
  : WorkerHolder("CacheWorkerHolder", aBehavior)
  , mNotified(false)
{
}

CacheWorkerHolder::~CacheWorkerHolder()
{
  NS_ASSERT_OWNINGTHREAD(CacheWorkerHolder);
  MOZ_DIAGNOSTIC_ASSERT(mActorList.IsEmpty());
}

} // namespace cache
} // namespace dom
} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheWorkerHolder.h"

#include "mozilla/dom/cache/ActorChild.h"
#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::dom::workers::Terminating;
using mozilla::dom::workers::Status;
using mozilla::dom::workers::WorkerPrivate;

// static
already_AddRefed<CacheWorkerHolder>
CacheWorkerHolder::Create(WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(aWorkerPrivate);

  RefPtr<CacheWorkerHolder> workerHolder = new CacheWorkerHolder();
  if (NS_WARN_IF(!workerHolder->HoldWorker(aWorkerPrivate, Terminating))) {
    return nullptr;
  }

  return workerHolder.forget();
}

void
CacheWorkerHolder::AddActor(ActorChild* aActor)
{
  NS_ASSERT_OWNINGTHREAD(CacheWorkerHolder);
  MOZ_ASSERT(aActor);
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
  MOZ_ASSERT(aActor);

  DebugOnly<bool> removed = mActorList.RemoveElement(aActor);

  MOZ_ASSERT(removed);
  MOZ_ASSERT(!mActorList.Contains(aActor));
}

bool
CacheWorkerHolder::Notified() const
{
  return mNotified;
}

bool
CacheWorkerHolder::Notify(Status aStatus)
{
  NS_ASSERT_OWNINGTHREAD(CacheWorkerHolder);

  // When the service worker thread is stopped we will get Terminating,
  // but nothing higher than that.  We must shut things down at Terminating.
  if (aStatus < Terminating || mNotified) {
    return true;
  }

  mNotified = true;

  // Start the asynchronous destruction of our actors.  These will call back
  // into RemoveActor() once the actor is destroyed.
  for (uint32_t i = 0; i < mActorList.Length(); ++i) {
    mActorList[i]->StartDestroy();
  }

  return true;
}

CacheWorkerHolder::CacheWorkerHolder()
  : mNotified(false)
{
}

CacheWorkerHolder::~CacheWorkerHolder()
{
  NS_ASSERT_OWNINGTHREAD(CacheWorkerHolder);
  MOZ_ASSERT(mActorList.IsEmpty());
}

} // namespace cache
} // namespace dom
} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerHolderToken.h"

#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {

// static
already_AddRefed<WorkerHolderToken>
WorkerHolderToken::Create(WorkerPrivate* aWorkerPrivate,
                          WorkerStatus aShutdownStatus,
                          Behavior aBehavior)
{
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<WorkerHolderToken> workerHolder =
    new WorkerHolderToken(aShutdownStatus, aBehavior);

  if (NS_WARN_IF(!workerHolder->HoldWorker(aWorkerPrivate, aShutdownStatus))) {
    return nullptr;
  }

  return workerHolder.forget();
}

void
WorkerHolderToken::AddListener(Listener* aListener)
{
  NS_ASSERT_OWNINGTHREAD(WorkerHolderToken);
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(!mListenerList.Contains(aListener));

  mListenerList.AppendElement(aListener);

  // Allow an actor to be added after we've entered the Notifying case.  We
  // can't stop the actor creation from racing with out destruction of the
  // other actors and we need to wait for this extra one to close as well.
  // Signal it should destroy itself right away.
  if (mShuttingDown) {
    aListener->WorkerShuttingDown();
  }
}

void
WorkerHolderToken::RemoveListener(Listener* aListener)
{
  NS_ASSERT_OWNINGTHREAD(WorkerHolderToken);
  MOZ_ASSERT(aListener);

  DebugOnly<bool> removed = mListenerList.RemoveElement(aListener);

  MOZ_ASSERT(removed);
  MOZ_ASSERT(!mListenerList.Contains(aListener));
}

bool
WorkerHolderToken::IsShuttingDown() const
{
  return mShuttingDown;
}

WorkerPrivate*
WorkerHolderToken::GetWorkerPrivate() const
{
  NS_ASSERT_OWNINGTHREAD(WorkerHolderToken);
  return mWorkerPrivate;
}

WorkerHolderToken::WorkerHolderToken(WorkerStatus aShutdownStatus,
                                     Behavior aBehavior)
  : WorkerHolder("WorkerHolderToken", aBehavior)
  , mShutdownStatus(aShutdownStatus)
  , mShuttingDown(false)
{
}

WorkerHolderToken::~WorkerHolderToken()
{
  NS_ASSERT_OWNINGTHREAD(WorkerHolderToken);
  MOZ_ASSERT(mListenerList.IsEmpty());
}

bool
WorkerHolderToken::Notify(WorkerStatus aStatus)
{
  NS_ASSERT_OWNINGTHREAD(WorkerHolderToken);

  // When the service worker thread is stopped we will get Canceling,
  // but nothing higher than that.  We must shut things down at Canceling.
  if (aStatus < mShutdownStatus || mShuttingDown) {
    return true;
  }

  // Start the asynchronous destruction of our actors.  These will call back
  // into RemoveActor() once the actor is destroyed.
  nsTObserverArray<Listener*>::ForwardIterator iter(mListenerList);
  while (iter.HasMore()) {
    iter.GetNext()->WorkerShuttingDown();
  }

  // Set this after calling WorkerShuttingDown() on listener list in case
  // one callback triggers another listener to be added.
  mShuttingDown = true;

  return true;
}

} // dom namespace
} // mozilla namespace

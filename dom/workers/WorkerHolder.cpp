/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerHolder.h"
#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {

namespace {

void
AssertOnOwningThread(void* aThread)
{
  if (MOZ_UNLIKELY(aThread != GetCurrentVirtualThread())) {
    MOZ_CRASH_UNSAFE_OOL("WorkerHolder on the wrong thread.");
  }
}

} // anonymous

WorkerHolder::WorkerHolder(const char* aName, Behavior aBehavior)
  : mWorkerPrivate(nullptr)
  , mBehavior(aBehavior)
  , mThread(GetCurrentVirtualThread())
  , mName(aName)
{
}

WorkerHolder::~WorkerHolder()
{
  AssertOnOwningThread(mThread);
  ReleaseWorkerInternal();
  MOZ_ASSERT(mWorkerPrivate == nullptr);
}

bool
WorkerHolder::HoldWorker(WorkerPrivate* aWorkerPrivate,
                         WorkerStatus aFailStatus)
{
  AssertOnOwningThread(mThread);
  MOZ_ASSERT(aWorkerPrivate);
  MOZ_ASSERT(aFailStatus >= Canceling);

  aWorkerPrivate->AssertIsOnWorkerThread();

  if (!aWorkerPrivate->AddHolder(this, aFailStatus)) {
    return false;
  }

  mWorkerPrivate = aWorkerPrivate;
  return true;
}

void
WorkerHolder::ReleaseWorker()
{
  AssertOnOwningThread(mThread);
  MOZ_ASSERT(mWorkerPrivate);

  ReleaseWorkerInternal();
}

WorkerHolder::Behavior
WorkerHolder::GetBehavior() const
{
  return mBehavior;
}

void
WorkerHolder::ReleaseWorkerInternal()
{
  AssertOnOwningThread(mThread);

  if (mWorkerPrivate) {
    mWorkerPrivate->AssertIsOnWorkerThread();
    mWorkerPrivate->RemoveHolder(this);
    mWorkerPrivate = nullptr;
  }
}

} // dom namespace
} // mozilla namespace

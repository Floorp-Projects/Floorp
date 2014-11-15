/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerThread_h__
#define mozilla_dom_workers_WorkerThread_h__

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "nsISupportsImpl.h"
#include "nsRefPtr.h"
#include "nsThread.h"

class nsIRunnable;

namespace mozilla {
namespace dom {
namespace workers {

class WorkerPrivate;
class WorkerRunnable;

class WorkerThread MOZ_FINAL
  : public nsThread
{
  class Observer;

  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<Observer> mObserver;

#ifdef DEBUG
  // Protected by nsThread::mLock.
  bool mAcceptingNonWorkerRunnables;
#endif

public:
  static already_AddRefed<WorkerThread>
  Create();

  void
  SetWorker(WorkerPrivate* aWorkerPrivate);

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD
  Dispatch(nsIRunnable* aRunnable, uint32_t aFlags) MOZ_OVERRIDE;

#ifdef DEBUG
  bool
  IsAcceptingNonWorkerRunnables()
  {
    MutexAutoLock lock(mLock);
    return mAcceptingNonWorkerRunnables;
  }

  void
  SetAcceptingNonWorkerRunnables(bool aAcceptingNonWorkerRunnables)
  {
    MutexAutoLock lock(mLock);
    mAcceptingNonWorkerRunnables = aAcceptingNonWorkerRunnables;
  }
#endif

private:
  WorkerThread();

  ~WorkerThread();
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_WorkerThread_h__

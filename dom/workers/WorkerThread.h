/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerThread_h__
#define mozilla_dom_workers_WorkerThread_h__

#include "mozilla/Attributes.h"
#include "mozilla/CondVar.h"
#include "mozilla/DebugOnly.h"
#include "nsISupportsImpl.h"
#include "nsRefPtr.h"
#include "nsThread.h"

class nsIRunnable;

namespace mozilla {
namespace dom {
namespace workers {

class RuntimeService;
class WorkerPrivate;
template <class> class WorkerPrivateParent;
class WorkerRunnable;

// This class lets us restrict the public methods that can be called on
// WorkerThread to RuntimeService and WorkerPrivate without letting them gain
// full access to private methods (as would happen if they were simply friends).
class WorkerThreadFriendKey
{
  friend class RuntimeService;
  friend class WorkerPrivate;
  friend class WorkerPrivateParent<WorkerPrivate>;

  WorkerThreadFriendKey();
  ~WorkerThreadFriendKey();
};

class WorkerThread MOZ_FINAL
  : public nsThread
{
  class Observer;

  CondVar mWorkerPrivateCondVar;

  // Protected by nsThread::mLock.
  WorkerPrivate* mWorkerPrivate;

  // Only touched on the target thread.
  nsRefPtr<Observer> mObserver;

  // Protected by nsThread::mLock and waited on with mWorkerPrivateCondVar.
  uint32_t mOtherThreadsDispatchingViaEventTarget;

  // Protected by nsThread::mLock.
  DebugOnly<bool> mAcceptingNonWorkerRunnables;

public:
  static already_AddRefed<WorkerThread>
  Create(const WorkerThreadFriendKey& aKey);

  void
  SetWorker(const WorkerThreadFriendKey& aKey, WorkerPrivate* aWorkerPrivate);

  nsresult
  DispatchPrimaryRunnable(const WorkerThreadFriendKey& aKey,
                          nsIRunnable* aRunnable);

  nsresult
  Dispatch(const WorkerThreadFriendKey& aKey,
           WorkerRunnable* aWorkerRunnable);

  uint32_t
  RecursionDepth(const WorkerThreadFriendKey& aKey) const;

  NS_DECL_ISUPPORTS_INHERITED

private:
  WorkerThread();
  ~WorkerThread();

  // This should only be called by consumers that have an
  // nsIEventTarget/nsIThread pointer.
  NS_IMETHOD
  Dispatch(nsIRunnable* aRunnable, uint32_t aFlags) MOZ_OVERRIDE;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_WorkerThread_h__

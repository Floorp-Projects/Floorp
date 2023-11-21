/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerThread_h__
#define mozilla_dom_workers_WorkerThread_h__

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/SafeRefPtr.h"
#include "nsISupports.h"
#include "nsThread.h"
#include "nscore.h"

class nsIRunnable;

namespace mozilla {
class Runnable;

namespace dom {

class WorkerRunnable;
class WorkerPrivate;
template <class>
class WorkerPrivateParent;

namespace workerinternals {
class RuntimeService;
}

// This class lets us restrict the public methods that can be called on
// WorkerThread to RuntimeService and WorkerPrivate without letting them gain
// full access to private methods (as would happen if they were simply friends).
class WorkerThreadFriendKey {
  friend class workerinternals::RuntimeService;
  friend class WorkerPrivate;
  friend class WorkerPrivateParent<WorkerPrivate>;

  WorkerThreadFriendKey();
  ~WorkerThreadFriendKey();
};

class WorkerThread final : public nsThread {
  class Observer;

  Mutex mLock MOZ_UNANNOTATED;
  CondVar mWorkerPrivateCondVar;

  // Protected by nsThread::mLock.
  WorkerPrivate* mWorkerPrivate;

  // Only touched on the target thread.
  RefPtr<Observer> mObserver;

  // Protected by nsThread::mLock and waited on with mWorkerPrivateCondVar.
  uint32_t mOtherThreadsDispatchingViaEventTarget;

#ifdef DEBUG
  // Protected by nsThread::mLock.
  bool mAcceptingNonWorkerRunnables;
#endif

  // Using this struct we restrict access to the constructor while still being
  // able to use MakeSafeRefPtr.
  struct ConstructorKey {};

 public:
  explicit WorkerThread(ConstructorKey);

  static SafeRefPtr<WorkerThread> Create(const WorkerThreadFriendKey& aKey);

  void SetWorker(const WorkerThreadFriendKey& aKey,
                 WorkerPrivate* aWorkerPrivate);

  nsresult DispatchPrimaryRunnable(const WorkerThreadFriendKey& aKey,
                                   already_AddRefed<nsIRunnable> aRunnable);

  nsresult DispatchAnyThread(const WorkerThreadFriendKey& aKey,
                             already_AddRefed<WorkerRunnable> aWorkerRunnable);

  uint32_t RecursionDepth(const WorkerThreadFriendKey& aKey) const;

  // Override HasPendingEvents to allow HasPendingEvents could be accessed by
  // the parent thread. WorkerPrivate::IsEligibleForCC calls this method on the
  // parent thread to check if there is any pending events on the worker thread.
  NS_IMETHOD HasPendingEvents(bool* aHasPendingEvents) override;

  NS_INLINE_DECL_REFCOUNTING_INHERITED(WorkerThread, nsThread)

 private:
  ~WorkerThread();

  // This should only be called by consumers that have an
  // nsIEventTarget/nsIThread pointer.
  NS_IMETHOD
  Dispatch(already_AddRefed<nsIRunnable> aRunnable, uint32_t aFlags) override;

  NS_IMETHOD
  DispatchFromScript(nsIRunnable* aRunnable, uint32_t aFlags) override;

  NS_IMETHOD
  DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t) override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_workers_WorkerThread_h__

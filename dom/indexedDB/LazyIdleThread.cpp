/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "LazyIdleThread.h"

#include "nsIObserverService.h"

#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#ifdef DEBUG
#define ASSERT_OWNING_THREAD()                                                 \
  PR_BEGIN_MACRO                                                               \
    nsIThread* currentThread = NS_GetCurrentThread();                          \
    if (currentThread) {                                                       \
      nsCOMPtr<nsISupports> current(do_QueryInterface(currentThread));         \
      nsCOMPtr<nsISupports> test(do_QueryInterface(mOwningThread));            \
      NS_ASSERTION(current == test, "Wrong thread!");                          \
    }                                                                          \
  PR_END_MACRO
#else
#define ASSERT_OWNING_THREAD() /* nothing */
#endif

USING_INDEXEDDB_NAMESPACE

using mozilla::MutexAutoLock;

LazyIdleThread::LazyIdleThread(PRUint32 aIdleTimeoutMS,
                               ShutdownMethod aShutdownMethod,
                               nsIObserver* aIdleObserver)
: mMutex("LazyIdleThread::mMutex"),
  mOwningThread(NS_GetCurrentThread()),
  mIdleObserver(aIdleObserver),
  mQueuedRunnables(nsnull),
  mIdleTimeoutMS(aIdleTimeoutMS),
  mPendingEventCount(0),
  mIdleNotificationCount(0),
  mShutdownMethod(aShutdownMethod),
  mShutdown(PR_FALSE),
  mThreadIsShuttingDown(PR_FALSE),
  mIdleTimeoutEnabled(PR_TRUE)
{
  NS_ASSERTION(mOwningThread, "This should never fail!");
}

LazyIdleThread::~LazyIdleThread()
{
  ASSERT_OWNING_THREAD();

  Shutdown();
}

void
LazyIdleThread::SetWeakIdleObserver(nsIObserver* aObserver)
{
  ASSERT_OWNING_THREAD();

  if (mShutdown) {
    NS_WARN_IF_FALSE(!aObserver,
                     "Setting an observer after Shutdown was called!");
    return;
  }

  mIdleObserver = aObserver;
}

void
LazyIdleThread::DisableIdleTimeout()
{
  ASSERT_OWNING_THREAD();
  if (!mIdleTimeoutEnabled) {
    return;
  }
  mIdleTimeoutEnabled = PR_FALSE;

  if (mIdleTimer && NS_FAILED(mIdleTimer->Cancel())) {
    NS_WARNING("Failed to cancel timer!");
  }

  MutexAutoLock lock(mMutex);

  // Pretend we have a pending event to keep the idle timer from firing.
  NS_ASSERTION(mPendingEventCount < PR_UINT32_MAX, "Way too many!");
  mPendingEventCount++;
}

void
LazyIdleThread::EnableIdleTimeout()
{
  ASSERT_OWNING_THREAD();
  if (mIdleTimeoutEnabled) {
    return;
  }
  mIdleTimeoutEnabled = PR_TRUE;

  {
    MutexAutoLock lock(mMutex);

    NS_ASSERTION(mPendingEventCount, "Mismatched calls to observer methods!");
    --mPendingEventCount;
  }

  if (mThread) {
    nsCOMPtr<nsIRunnable> runnable(new nsRunnable());
    if (NS_FAILED(Dispatch(runnable, NS_DISPATCH_NORMAL))) {
      NS_WARNING("Failed to dispatch!");
    }
  }
}

void
LazyIdleThread::PreDispatch()
{
  MutexAutoLock lock(mMutex);

  NS_ASSERTION(mPendingEventCount < PR_UINT32_MAX, "Way too many!");
  mPendingEventCount++;
}

nsresult
LazyIdleThread::EnsureThread()
{
  ASSERT_OWNING_THREAD();

  if (mShutdown) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mThread) {
    return NS_OK;
  }

  NS_ASSERTION(!mPendingEventCount, "Shouldn't have events yet!");
  NS_ASSERTION(!mIdleNotificationCount, "Shouldn't have idle events yet!");
  NS_ASSERTION(!mIdleTimer, "Should have killed this long ago!");
  NS_ASSERTION(!mThreadIsShuttingDown, "Should have cleared that!");

  nsresult rv;

  if (mShutdownMethod == AutomaticShutdown && NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> obs =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = obs->AddObserver(this, "xpcom-shutdown-threads", PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mIdleTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  NS_ENSURE_TRUE(mIdleTimer, NS_ERROR_FAILURE);

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &LazyIdleThread::InitThread);
  NS_ENSURE_TRUE(runnable, NS_ERROR_FAILURE);

  rv = NS_NewThread(getter_AddRefs(mThread), runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
LazyIdleThread::InitThread()
{
  // Happens on mThread but mThread may not be set yet...

  nsCOMPtr<nsIThreadInternal> thread(do_QueryInterface(NS_GetCurrentThread()));
  NS_ASSERTION(thread, "This should always succeed!");

  if (NS_FAILED(thread->SetObserver(this))) {
    NS_WARNING("Failed to set thread observer!");
  }
}

void
LazyIdleThread::CleanupThread()
{
  nsCOMPtr<nsIThreadInternal> thread(do_QueryInterface(NS_GetCurrentThread()));
  NS_ASSERTION(thread, "This should always succeed!");

  if (NS_FAILED(thread->SetObserver(nsnull))) {
    NS_WARNING("Failed to set thread observer!");
  }

  MutexAutoLock lock(mMutex);

  NS_ASSERTION(!mThreadIsShuttingDown, "Shouldn't be true ever!");
  mThreadIsShuttingDown = PR_TRUE;
}

void
LazyIdleThread::ScheduleTimer()
{
  ASSERT_OWNING_THREAD();

  bool shouldSchedule;
  {
    MutexAutoLock lock(mMutex);

    NS_ASSERTION(mIdleNotificationCount, "Should have at least one!");
    --mIdleNotificationCount;

    shouldSchedule = !mIdleNotificationCount && !mPendingEventCount;
  }

  if (NS_FAILED(mIdleTimer->Cancel())) {
    NS_WARNING("Failed to cancel timer!");
  }

  if (shouldSchedule &&
      NS_FAILED(mIdleTimer->InitWithCallback(this, mIdleTimeoutMS,
                                             nsITimer::TYPE_ONE_SHOT))) {
    NS_WARNING("Failed to schedule timer!");
  }
}

nsresult
LazyIdleThread::ShutdownThread()
{
  ASSERT_OWNING_THREAD();

  // Before calling Shutdown() on the real thread we need to put a queue in
  // place in case a runnable is posted to the thread while it's in the
  // process of shutting down. This will be our queue.
  nsAutoTArray<nsCOMPtr<nsIRunnable>, 10> queuedRunnables;

  nsresult rv;

  if (mThread) {
    if (mShutdownMethod == AutomaticShutdown && NS_IsMainThread()) {
      nsCOMPtr<nsIObserverService> obs =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
      NS_WARN_IF_FALSE(obs, "Failed to get observer service!");

      if (obs &&
          NS_FAILED(obs->RemoveObserver(this, "xpcom-shutdown-threads"))) {
        NS_WARNING("Failed to remove observer!");
      }
    }

    if (mIdleObserver) {
      mIdleObserver->Observe(static_cast<nsIThread*>(this), IDLE_THREAD_TOPIC,
                             nsnull);
    }

#ifdef DEBUG
    {
      MutexAutoLock lock(mMutex);
      NS_ASSERTION(!mThreadIsShuttingDown, "Huh?!");
    }
#endif

    nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableMethod(this, &LazyIdleThread::CleanupThread);
    NS_ENSURE_TRUE(runnable, NS_ERROR_FAILURE);

    PreDispatch();

    rv = mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    // Put the temporary queue in place before calling Shutdown().
    mQueuedRunnables = &queuedRunnables;

    if (NS_FAILED(mThread->Shutdown())) {
      NS_ERROR("Failed to shutdown the thread!");
    }

    // Now unset the queue.
    mQueuedRunnables = nsnull;

    mThread = nsnull;

    {
      MutexAutoLock lock(mMutex);

      NS_ASSERTION(!mPendingEventCount, "Huh?!");
      NS_ASSERTION(!mIdleNotificationCount, "Huh?!");
      NS_ASSERTION(mThreadIsShuttingDown, "Huh?!");
      mThreadIsShuttingDown = PR_FALSE;
    }
  }

  if (mIdleTimer) {
    rv = mIdleTimer->Cancel();
    NS_ENSURE_SUCCESS(rv, rv);

    mIdleTimer = nsnull;
  }

  // If our temporary queue has any runnables then we need to dispatch them.
  if (queuedRunnables.Length()) {
    // If the thread manager has gone away then these runnables will never run.
    if (mShutdown) {
      NS_ERROR("Runnables dispatched to LazyIdleThread will never run!");
      return NS_OK;
    }

    // Re-dispatch the queued runnables.
    for (PRUint32 index = 0; index < queuedRunnables.Length(); index++) {
      nsCOMPtr<nsIRunnable> runnable;
      runnable.swap(queuedRunnables[index]);
      NS_ASSERTION(runnable, "Null runnable?!");

      if (NS_FAILED(Dispatch(runnable, NS_DISPATCH_NORMAL))) {
        NS_ERROR("Failed to re-dispatch queued runnable!");
      }
    }
  }

  return NS_OK;
}

void
LazyIdleThread::SelfDestruct()
{
  NS_ASSERTION(mRefCnt == 1, "Bad refcount!");
  delete this;
}

NS_IMPL_THREADSAFE_ADDREF(LazyIdleThread)

NS_IMETHODIMP_(nsrefcnt)
LazyIdleThread::Release()
{
  nsrefcnt count = NS_AtomicDecrementRefcnt(mRefCnt);
  NS_LOG_RELEASE(this, count, "LazyIdleThread");

  if (!count) {
    // Stabilize refcount.
    mRefCnt = 1;

    nsCOMPtr<nsIRunnable> runnable =
      NS_NewNonOwningRunnableMethod(this, &LazyIdleThread::SelfDestruct);
    NS_WARN_IF_FALSE(runnable, "Couldn't make runnable!");

    if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
      // The only way this could fail is if we're in shutdown, and in that case
      // threads should have been joined already. Deleting here isn't dangerous
      // anymore because we won't spin the event loop waiting to join the
      // thread.
      SelfDestruct();
    }
  }

  return count;
}

NS_IMPL_THREADSAFE_QUERY_INTERFACE5(LazyIdleThread, nsIThread,
                                                    nsIEventTarget,
                                                    nsITimerCallback,
                                                    nsIThreadObserver,
                                                    nsIObserver)

NS_IMETHODIMP
LazyIdleThread::Dispatch(nsIRunnable* aEvent,
                         PRUint32 aFlags)
{
  ASSERT_OWNING_THREAD();

  // LazyIdleThread can't always support synchronous dispatch currently.
  NS_ENSURE_TRUE(aFlags == NS_DISPATCH_NORMAL, NS_ERROR_NOT_IMPLEMENTED);

  // If our thread is shutting down then we can't actually dispatch right now.
  // Queue this runnable for later.
  if (UseRunnableQueue()) {
    mQueuedRunnables->AppendElement(aEvent);
    return NS_OK;
  }

  nsresult rv = EnsureThread();
  NS_ENSURE_SUCCESS(rv, rv);

  PreDispatch();

  return mThread->Dispatch(aEvent, aFlags);
}

NS_IMETHODIMP
LazyIdleThread::IsOnCurrentThread(bool* aIsOnCurrentThread)
{
  if (mThread) {
    return mThread->IsOnCurrentThread(aIsOnCurrentThread);
  }

  *aIsOnCurrentThread = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
LazyIdleThread::GetPRThread(PRThread** aPRThread)
{
  if (mThread) {
    return mThread->GetPRThread(aPRThread);
  }

  *aPRThread = nsnull;
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
LazyIdleThread::Shutdown()
{
  ASSERT_OWNING_THREAD();

  mShutdown = PR_TRUE;

  nsresult rv = ShutdownThread();
  NS_ASSERTION(!mThread, "Should have destroyed this by now!");

  mIdleObserver = nsnull;

  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
LazyIdleThread::HasPendingEvents(bool* aHasPendingEvents)
{
  // This is only supposed to be called from the thread itself so it's not
  // implemented here.
  NS_NOTREACHED("Shouldn't ever call this!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
LazyIdleThread::ProcessNextEvent(bool aMayWait,
                                 bool* aEventWasProcessed)
{
  // This is only supposed to be called from the thread itself so it's not
  // implemented here.
  NS_NOTREACHED("Shouldn't ever call this!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
LazyIdleThread::Notify(nsITimer* aTimer)
{
  ASSERT_OWNING_THREAD();

  {
    MutexAutoLock lock(mMutex);

    if (mPendingEventCount || mIdleNotificationCount) {
      // Another event was scheduled since this timer was set. Don't do
      // anything and wait for the timer to fire again.
      return NS_OK;
    }
  }

  nsresult rv = ShutdownThread();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
LazyIdleThread::OnDispatchedEvent(nsIThreadInternal* /*aThread */)
{
  NS_ASSERTION(NS_GetCurrentThread() == mOwningThread, "Wrong thread!");
  return NS_OK;
}

NS_IMETHODIMP
LazyIdleThread::OnProcessNextEvent(nsIThreadInternal* /* aThread */,
                                   bool /* aMayWait */,
                                   PRUint32 /* aRecursionDepth */)
{
  return NS_OK;
}

NS_IMETHODIMP
LazyIdleThread::AfterProcessNextEvent(nsIThreadInternal* /* aThread */,
                                      PRUint32 /* aRecursionDepth */)
{
  bool shouldNotifyIdle;
  {
    MutexAutoLock lock(mMutex);

    NS_ASSERTION(mPendingEventCount, "Mismatched calls to observer methods!");
    --mPendingEventCount;

    if (mThreadIsShuttingDown) {
      // We're shutting down, no need to fire any timer.
      return NS_OK;
    }

    shouldNotifyIdle = !mPendingEventCount;
    if (shouldNotifyIdle) {
      NS_ASSERTION(mIdleNotificationCount < PR_UINT32_MAX, "Way too many!");
      mIdleNotificationCount++;
    }
  }

  if (shouldNotifyIdle) {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableMethod(this, &LazyIdleThread::ScheduleTimer);
    NS_ENSURE_TRUE(runnable, NS_ERROR_FAILURE);

    nsresult rv = mOwningThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
LazyIdleThread::Observe(nsISupports* /* aSubject */,
                        const char*  aTopic,
                        const PRUnichar* /* aData */)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mShutdownMethod == AutomaticShutdown,
               "Should not receive notifications if not AutomaticShutdown!");
  NS_ASSERTION(!strcmp("xpcom-shutdown-threads", aTopic), "Bad topic!");

  Shutdown();
  return NS_OK;
}

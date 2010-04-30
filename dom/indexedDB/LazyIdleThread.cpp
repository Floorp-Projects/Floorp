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

USING_INDEXEDDB_NAMESPACE

using mozilla::MutexAutoLock;

namespace {

/**
 * Simple class to cancel timers on the correct thread.
 */
class CancelTimerRunnable : public nsRunnable
{
public:
  CancelTimerRunnable(nsITimer* aTimer)
  : mTimer(aTimer)
  { }

  NS_IMETHOD Run() {
    return mTimer->Cancel();
  }

private:
  nsCOMPtr<nsITimer> mTimer;
};

} // anonymous namespace

LazyIdleThread::LazyIdleThread(PRUint32 aIdleTimeoutMS)
: mMutex("LazyIdleThread::mMutex"),
  mOwningThread(NS_GetCurrentThread()),
  mIdleTimeoutMS(aIdleTimeoutMS),
  mShutdown(PR_FALSE),
  mThreadHasTimedOut(PR_FALSE)
{
  NS_ASSERTION(mOwningThread, "This should never fail!");
}

LazyIdleThread::~LazyIdleThread()
{
  NS_ASSERTION(NS_GetCurrentThread() == mOwningThread, "Wrong thread!");

  Shutdown();
}

/**
 * Make sure that a valid thread exists in mThread.
 */
nsresult
LazyIdleThread::EnsureThread()
{
  NS_ASSERTION(NS_GetCurrentThread() == mOwningThread, "Wrong thread!");

  if (mShutdown) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  if (mThread) {
    return NS_OK;
  }

  if (!mOwningThread) {
    NS_ASSERTION(mOwningThread, "This should never be null!");
  }

  nsresult rv;

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> obs =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = obs->AddObserver(this, "xpcom-shutdown-threads", PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &LazyIdleThread::InitThread);
  NS_ENSURE_TRUE(runnable, NS_ERROR_FAILURE);

  rv = NS_NewThread(getter_AddRefs(mThread), runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/**
 * This is the first runnable to execute on the thread. It sets up the thread
 * observer.
 */
void
LazyIdleThread::InitThread()
{
  // Happens on mThread but mThread may not be set yet...

  nsCOMPtr<nsIThreadInternal> thread(do_QueryInterface(NS_GetCurrentThread()));
  NS_ASSERTION(thread, "This should always succeed!");

#ifdef DEBUG
  nsCOMPtr<nsIThreadObserver> oldObserver;
  nsresult rv = thread->GetObserver(getter_AddRefs(oldObserver));
  NS_ASSERTION(NS_SUCCEEDED(rv) && !oldObserver, "Already have an observer!");
#endif

  if (NS_FAILED(thread->SetObserver(this))) {
    NS_WARNING("Failed to set thread observer!");
  }
}

/**
 * Called when the thread should be cleaned up. Happens on timeout, Shutdown(),
 * or when XPCOM is shutting down.
 */
void
LazyIdleThread::ShutdownThread()
{
  NS_ASSERTION(NS_GetCurrentThread() == mOwningThread, "Wrong thread!");

  if (mThread) {
    if (NS_FAILED(mThread->Shutdown())) {
      NS_WARNING("Failed to shutdown thread!");
    }
    mThread = nsnull;
  }

  // Don't need to lock here because the thread is gone.
  if (mIdleTimer) {
    if (NS_FAILED(mIdleTimer->Cancel())) {
      NS_WARNING("Failed to cancel timer!");
    }
    mIdleTimer = nsnull;
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS3(LazyIdleThread, nsITimerCallback,
                                              nsIThreadObserver,
                                              nsIObserver)

/**
 * Dispatch an event to the thread.
 */
nsresult
LazyIdleThread::Dispatch(nsIRunnable* aEvent)
{
  NS_ASSERTION(NS_GetCurrentThread() == mOwningThread, "Wrong thread!");

  nsresult rv = EnsureThread();
  NS_ENSURE_SUCCESS(rv, rv);

  return mThread->Dispatch(aEvent, NS_DISPATCH_NORMAL);
}

/**
 * Shut down the thread (if it has been created) and prevent future dispatch.
 */
void
LazyIdleThread::Shutdown()
{
  NS_ASSERTION(NS_GetCurrentThread() == mOwningThread, "Wrong thread!");

  mShutdown = PR_TRUE;

  ShutdownThread();
}

/**
 * Called when the idle timer fires. Additional events may have been dispatched
 * to the thread since the timer was intialized so we must check to make sure
 * that the firing timer is the most recent before shutting down the thread.
 */
NS_IMETHODIMP
LazyIdleThread::Notify(nsITimer* aTimer)
{
  NS_ASSERTION(NS_GetCurrentThread() == mOwningThread, "Wrong thread!");

  {
    MutexAutoLock lock(mMutex);

    // We got notified for a timer that is no longer current. It will be taken
    // care of by the CancelTimerRunnable.
    if (aTimer != mIdleTimer) {
      return NS_OK;
    }

    // Our timer is current and the thread has therefore timed out.
    mIdleTimer = nsnull;
    mThreadHasTimedOut = PR_TRUE;
  }

  ShutdownThread();

  // Don't need to lock here because the thread is gone.
  mThreadHasTimedOut = PR_FALSE;
  return NS_OK;
}

/**
 * Called on any thread when an event is dispatched to the thread. We use it to
 * grab the current timer and cancel it via the CancelTimerRunnable.
 */
NS_IMETHODIMP
LazyIdleThread::OnDispatchedEvent(nsIThreadInternal* /*aThread */)
{
  // This can happen on *any* thread...

  nsCOMPtr<nsITimer> timer;
  {
    MutexAutoLock lock(mMutex);

    if (mThreadHasTimedOut) {
      return NS_OK;
    }

    timer.swap(mIdleTimer);
  }

  if (timer) {
    nsresult rv = mOwningThread->Dispatch(new CancelTimerRunnable(timer),
                                          NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

/**
 * Unused.
 */
NS_IMETHODIMP
LazyIdleThread::OnProcessNextEvent(nsIThreadInternal* /* aThread */,
                                   PRBool /* aMayWait */,
                                   PRUint32 /* aRecursionDepth */)
{
  return NS_OK;
}

/**
 * Called after the thread has processed an event. We use it to set a timer to
 * kill the thread if there are no more pending events.
 */
NS_IMETHODIMP
LazyIdleThread::AfterProcessNextEvent(nsIThreadInternal* aThread,
                                      PRUint32 /* aRecursionDepth */)
{
  PRBool hasPendingEvents;
  nsresult rv = aThread->HasPendingEvents(&hasPendingEvents);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hasPendingEvents) {
    return NS_OK;
  }

  MutexAutoLock lock(mMutex);

  if (mThreadHasTimedOut) {
    return NS_OK;
  }

  NS_ASSERTION(!mIdleTimer, "Should have been cleared!");

  mIdleTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mIdleTimer->SetTarget(mOwningThread);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mIdleTimer->InitWithCallback(this, mIdleTimeoutMS,
                                    nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/**
 * Called when XPCOM has requested that all threads join.
 */
NS_IMETHODIMP
LazyIdleThread::Observe(nsISupports* /* aSubject */,
                        const char* /* aTopic */,
                        const PRUnichar* /* aData */)
{
  NS_ASSERTION(NS_GetCurrentThread() == mOwningThread, "Wrong thread!");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  ShutdownThread();
  return NS_OK;
}

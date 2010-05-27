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

#include "TransactionThreadPool.h"

#include "nsIObserverService.h"
#include "nsIThreadPool.h"

#include "mozilla/CondVar.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOMCIDInternal.h"

#include "IDBTransactionRequest.h"

using mozilla::Mutex;
using mozilla::MutexAutoLock;
using mozilla::MutexAutoUnlock;
using mozilla::CondVar;

USING_INDEXEDDB_NAMESPACE

BEGIN_INDEXEDDB_NAMESPACE

class TransactionQueue : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  inline TransactionQueue(nsIRunnable* aRunnable);

  inline void Dispatch(nsIRunnable* aRunnable);

  inline void Finish();

private:
  Mutex mMutex;
  CondVar mCondVar;
  nsAutoTArray<nsCOMPtr<nsIRunnable>, 10> mQueue;
  bool mShouldFinish;
};

END_INDEXEDDB_NAMESPACE

namespace {

const PRUint32 kThreadLimit = 20;
const PRUint32 kIdleThreadLimit = 5;
const PRUint32 kIdleThreadTimeoutMs = 30000;

TransactionThreadPool* gInstance = nsnull;
bool gShutdown = false;

PLDHashOperator
FinishTransactions(IDBTransactionRequest* aKey,
                   nsRefPtr<TransactionQueue>& aData,
                   void* aUserArg)
{
  aData->Finish();
  return PL_DHASH_REMOVE;
}

} // anonymous namespace

TransactionThreadPool::TransactionThreadPool()
: mMutex("TransactionThreadPool::mMutex")
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance, "More than one instance!");
}

TransactionThreadPool::~TransactionThreadPool()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance, "More than one instance!");
}

// static
TransactionThreadPool*
TransactionThreadPool::GetOrCreate()
{
  if (!gInstance && !gShutdown) {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    nsRefPtr<TransactionThreadPool> pool(new TransactionThreadPool());

    nsresult rv = pool->Init();
    NS_ENSURE_SUCCESS(rv, nsnull);

    nsCOMPtr<nsIObserverService> obs =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, nsnull);

    rv = obs->AddObserver(pool, "xpcom-shutdown-threads", PR_FALSE);
    NS_ENSURE_SUCCESS(rv, nsnull);

    // The observer service now owns us.
    gInstance = pool;
  }
  return gInstance;
}

// static
void
TransactionThreadPool::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  gShutdown = true;

  if (gInstance) {
    if (NS_FAILED(gInstance->Cleanup())) {
      NS_WARNING("Failed to shutdown thread pool!");
    }
    gInstance = nsnull;
  }
}

nsresult
TransactionThreadPool::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mTransactionsInProgress.Init()) {
    NS_WARNING("Failed to init hash!");
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  mThreadPool = do_CreateInstance(NS_THREADPOOL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mThreadPool->SetThreadLimit(kThreadLimit);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mThreadPool->SetIdleThreadLimit(kIdleThreadLimit);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mThreadPool->SetIdleThreadTimeout(kIdleThreadTimeoutMs);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
TransactionThreadPool::Cleanup()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mTransactionsInProgress.Count()) {
    // This is actually really bad, but if we don't force everything awake then
    // we will deadlock on shutdown...
    NS_ERROR("Transactions still in progress!");
    mTransactionsInProgress.Enumerate(FinishTransactions, nsnull);
  }

  nsresult rv = mThreadPool->Shutdown();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
TransactionThreadPool::Dispatch(IDBTransactionRequest* aTransaction,
                                nsIRunnable* aRunnable,
                                bool aFinish)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "Null pointer!");
  NS_ASSERTION(aRunnable, "Null pointer!");

  nsRefPtr<TransactionQueue> queue;
  bool queueIsNew = false;

  {
    MutexAutoLock lock(mMutex);
    if (mTransactionsInProgress.Get(aTransaction, getter_AddRefs(queue))) {
      if (aFinish) {
        mTransactionsInProgress.Remove(aTransaction);
      }
    }
    else {
      queueIsNew = true;
      queue = new TransactionQueue(aRunnable);

      if (!aFinish && !mTransactionsInProgress.Put(aTransaction, queue)) {
        NS_WARNING("Failed to add to the hash!");
        return NS_ERROR_FAILURE;
      }
    }
  }

  NS_ASSERTION(queue, "Should never be null!");

  if (!queueIsNew) {
    queue->Dispatch(aRunnable);
  }

  if (aFinish) {
    queue->Finish();
  }

  return queueIsNew ? mThreadPool->Dispatch(queue, NS_DISPATCH_NORMAL) : NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(TransactionThreadPool, nsIObserver)

NS_IMETHODIMP
TransactionThreadPool::Observe(nsISupports* /* aSubject */,
                               const char*  aTopic,
                               const PRUnichar* /* aData */)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!strcmp("xpcom-shutdown-threads", aTopic), "Wrong topic!");

  Shutdown();

  return NS_OK;
}

TransactionQueue::TransactionQueue(nsIRunnable* aRunnable)
: mMutex("TransactionQueue::mMutex"),
  mCondVar(mMutex, "TransactionQueue::mCondVar"),
  mShouldFinish(false)
{
  NS_ASSERTION(aRunnable, "Null pointer!");
  mQueue.AppendElement(aRunnable);
}

void
TransactionQueue::Dispatch(nsIRunnable* aRunnable)
{
  MutexAutoLock lock(mMutex);

  NS_ASSERTION(!mShouldFinish, "Dispatch called after Finish!");

  if (!mQueue.AppendElement(aRunnable)) {
    MutexAutoUnlock unlock(mMutex);
    NS_RUNTIMEABORT("Out of memory!");
  }

  mCondVar.Notify();
}

void
TransactionQueue::Finish()
{
  MutexAutoLock lock(mMutex);

  NS_ASSERTION(!mShouldFinish, "Finish called more than once!");

  mShouldFinish = true;

  mCondVar.Notify();
}

NS_IMPL_THREADSAFE_ISUPPORTS1(TransactionQueue, nsIRunnable)

NS_IMETHODIMP
TransactionQueue::Run()
{
  nsAutoTArray<nsCOMPtr<nsIRunnable>, 10> queue;
  bool shouldFinish = false;

  while(!shouldFinish) {
    NS_ASSERTION(queue.IsEmpty(), "Should have cleared this!");

    {
      MutexAutoLock lock(mMutex);
      while (!mShouldFinish && mQueue.IsEmpty()) {
        if (NS_FAILED(mCondVar.Wait())) {
          NS_ERROR("Failed to wait!");
        }
      }

      mQueue.SwapElements(queue);
      shouldFinish = mShouldFinish;
    }

    PRUint32 count = queue.Length();
    for (PRUint32 index = 0; index < count; index++) {
      nsCOMPtr<nsIRunnable>& runnable = queue[index];
      runnable->Run();
      runnable = nsnull;
    }

    if (count) {
      queue.Clear();
    }
  }

  return NS_OK;
}

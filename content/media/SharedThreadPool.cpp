/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedThreadPool.h"
#include "mozilla/Monitor.h"
#include "mozilla/StaticPtr.h"
#include "nsDataHashtable.h"
#include "VideoUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Preferences.h"

#ifdef XP_WIN
// Required to init MSCOM by MSCOMInitThreadPoolListener.
#include <Objbase.h>
#endif

namespace mozilla {

// Created and destroyed on the main thread.
static StaticAutoPtr<ReentrantMonitor> sMonitor;

// Hashtable, maps thread pool name to SharedThreadPool instance.
// Modified only on the main thread.
static StaticAutoPtr<nsDataHashtable<nsCStringHashKey, SharedThreadPool*>> sPools;

static already_AddRefed<nsIThreadPool>
CreateThreadPool(const nsCString& aName);

static void
DestroySharedThreadPoolHashTable();

void
SharedThreadPool::EnsureInitialized()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (sMonitor || sPools) {
    // Already initalized.
    return;
  }
  sMonitor = new ReentrantMonitor("SharedThreadPool");
  sPools = new nsDataHashtable<nsCStringHashKey, SharedThreadPool*>();
}

class ShutdownPoolsEvent : public nsRunnable {
public:
  NS_IMETHODIMP Run() {
    MOZ_ASSERT(NS_IsMainThread());
    DestroySharedThreadPoolHashTable();
    return NS_OK;
  }
};

static void
DestroySharedThreadPoolHashTable()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sMonitor && sPools);
  if (!sPools->Count()) {
    // No more SharedThreadPool singletons. Delete the hash table.
    // Note we don't need to lock sMonitor, since we only modify the
    // hash table on the main thread, and if the hash table is empty
    // there are no external references into its contents.
    sPools = nullptr;
    sMonitor = nullptr;
  }
}

TemporaryRef<SharedThreadPool>
SharedThreadPool::Get(const nsCString& aName)
{
  MOZ_ASSERT(NS_IsMainThread());
  EnsureInitialized();
  MOZ_ASSERT(sMonitor);
  ReentrantMonitorAutoEnter mon(*sMonitor);
  SharedThreadPool* pool = nullptr;
  if (!sPools->Get(aName, &pool)) {
    nsCOMPtr<nsIThreadPool> threadPool(CreateThreadPool(aName));
    NS_ENSURE_TRUE(threadPool, nullptr);
    pool = new SharedThreadPool(aName, threadPool);
    sPools->Put(aName, pool);
  }
  MOZ_ASSERT(pool);
  RefPtr<SharedThreadPool> instance(pool);
  return instance.forget();
}

NS_IMETHODIMP_(nsrefcnt) SharedThreadPool::AddRef(void)
{
  MOZ_ASSERT(sMonitor);
  ReentrantMonitorAutoEnter mon(*sMonitor);
  MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
  nsrefcnt count = ++mRefCnt;
  NS_LOG_ADDREF(this, count, "SharedThreadPool", sizeof(*this));
  return count;
}

NS_IMETHODIMP_(nsrefcnt) SharedThreadPool::Release(void)
{
  MOZ_ASSERT(sMonitor);
  bool dispatchShutdownEvent;
  {
    ReentrantMonitorAutoEnter mon(*sMonitor);
    nsrefcnt count = --mRefCnt;
    NS_LOG_RELEASE(this, count, "SharedThreadPool");
    if (count) {
      return count;
    }

    // Zero refcount. Must shutdown and then delete the thread pool.

    // First, dispatch an event to the main thread to call Shutdown() on
    // the nsIThreadPool. The Runnable here  will add a refcount to the pool,
    // and when the Runnable releases the nsIThreadPool it will be deleted.
    RefPtr<nsIRunnable> r = NS_NewRunnableMethod(mPool, &nsIThreadPool::Shutdown);
    NS_DispatchToMainThread(r);

    // Remove SharedThreadPool from table of pools.
    sPools->Remove(mName);
    MOZ_ASSERT(!sPools->Get(mName));

    // Stabilize refcount, so that if something in the dtor QIs,
    // it won't explode.
    mRefCnt = 1;

    delete this;

    dispatchShutdownEvent = sPools->Count() == 0;
  }
  if (dispatchShutdownEvent) {
    // No more SharedThreadPools alive. Destroy the hash table.
    // Ensure that we only run on the main thread.
    // Do this in an event so that if something holds the monitor we won't
    // be deleting the monitor while it's held.
    NS_DispatchToMainThread(new ShutdownPoolsEvent(), NS_DISPATCH_NORMAL);
  }
  return 0;
}

NS_IMPL_QUERY_INTERFACE1(SharedThreadPool, nsIThreadPool)

SharedThreadPool::SharedThreadPool(const nsCString& aName,
                                   nsIThreadPool* aPool)
  : mName(aName)
  , mPool(aPool)
  , mRefCnt(0)
{
  mEventTarget = do_QueryInterface(aPool);
}

SharedThreadPool::~SharedThreadPool()
{
}

#ifdef XP_WIN

// Thread pool listener which ensures that MSCOM is initialized and
// deinitialized on the thread pool thread. We may call into WMF or
// DirectShow on this thread, so we need MSCOM working.
class MSCOMInitThreadPoolListener MOZ_FINAL : public nsIThreadPoolListener {
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITHREADPOOLLISTENER
};

NS_IMPL_ISUPPORTS1(MSCOMInitThreadPoolListener, nsIThreadPoolListener)

NS_IMETHODIMP
MSCOMInitThreadPoolListener::OnThreadCreated()
{
  HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    NS_WARNING("Failed to initialize MSCOM on WMFByteStream thread.");
  }
  return NS_OK;
}

NS_IMETHODIMP
MSCOMInitThreadPoolListener::OnThreadShuttingDown()
{
  CoUninitialize();
  return NS_OK;
}

#endif // XP_WIN

static already_AddRefed<nsIThreadPool>
CreateThreadPool(const nsCString& aName)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  nsCOMPtr<nsIThreadPool> pool = do_CreateInstance(NS_THREADPOOL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = pool->SetName(aName);
  NS_ENSURE_SUCCESS(rv, nullptr);

  // We limit the number of threads that we use for media. Note that the
  // default thread limit is the same as the idle limit so that we're not
  // constantly creating and destroying threads (see Bug 881954). When the
  // thread pool threads shutdown they dispatch an event to the main thread
  // to call nsIThread::Shutdown(), and if we're very busy that can take a
  // while to run, and we end up with dozens of extra threads. Note that
  // threads that are idle for 60 seconds are shutdown naturally.
  rv = pool->SetThreadLimit(
    Preferences::GetUint("media.thread-pool.thread-limit", 4));
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = pool->SetIdleThreadLimit(
    Preferences::GetUint("media.thread-pool.idle-thread-limit", 4));
  NS_ENSURE_SUCCESS(rv, nullptr);

#ifdef XP_WIN
  // Ensure MSCOM is initialized on the thread pools threads.
  nsCOMPtr<nsIThreadPoolListener> listener = new MSCOMInitThreadPoolListener();
  rv = pool->SetListener(listener);
  NS_ENSURE_SUCCESS(rv, nullptr);
#endif

  return pool.forget();
}

} // namespace mozilla

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecodePool.h"

#include <algorithm>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Monitor.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsIThreadPool.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "prsystem.h"

#include "gfxPrefs.h"

#include "Decoder.h"
#include "IDecodingTask.h"
#include "RasterImage.h"

using std::max;
using std::min;

namespace mozilla {
namespace image {

///////////////////////////////////////////////////////////////////////////////
// DecodePool implementation.
///////////////////////////////////////////////////////////////////////////////

/* static */ StaticRefPtr<DecodePool> DecodePool::sSingleton;
/* static */ uint32_t DecodePool::sNumCores = 0;

NS_IMPL_ISUPPORTS(DecodePool, nsIObserver)

struct Work
{
  enum class Type {
    TASK,
    SHUTDOWN
  } mType;

  RefPtr<IDecodingTask> mTask;
};

class DecodePoolImpl
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(DecodePoolImpl)
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecodePoolImpl)

  DecodePoolImpl()
    : mMonitor("DecodePoolImpl")
    , mShuttingDown(false)
  { }

  /// Initialize the current thread for use by the decode pool.
  void InitCurrentThread()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    mThreadNaming.SetThreadPoolName(NS_LITERAL_CSTRING("ImgDecoder"));
  }

  /// Shut down the provided decode pool thread.
  static void ShutdownThread(nsIThread* aThisThread)
  {
    // Threads have to be shut down from another thread, so we'll ask the
    // main thread to do it for us.
    NS_DispatchToMainThread(NewRunnableMethod(aThisThread, &nsIThread::Shutdown));
  }

  /**
   * Requests shutdown. New work items will be dropped on the floor, and all
   * decode pool threads will be shut down once existing work items have been
   * processed.
   */
  void RequestShutdown()
  {
    MonitorAutoLock lock(mMonitor);
    mShuttingDown = true;
    mMonitor.NotifyAll();
  }

  /// Pushes a new decode work item.
  void PushWork(IDecodingTask* aTask)
  {
    MOZ_ASSERT(aTask);
    RefPtr<IDecodingTask> task(aTask);

    MonitorAutoLock lock(mMonitor);

    if (mShuttingDown) {
      // Drop any new work on the floor if we're shutting down.
      return;
    }

    if (task->Priority() == TaskPriority::eHigh) {
      mHighPriorityQueue.AppendElement(Move(task));
    } else {
      mLowPriorityQueue.AppendElement(Move(task));
    }

    mMonitor.Notify();
  }

  /// Pops a new work item, blocking if necessary.
  Work PopWork()
  {
    MonitorAutoLock lock(mMonitor);

    do {
      if (!mHighPriorityQueue.IsEmpty()) {
        return PopWorkFromQueue(mHighPriorityQueue);
      }

      if (!mLowPriorityQueue.IsEmpty()) {
        return PopWorkFromQueue(mLowPriorityQueue);
      }

      if (mShuttingDown) {
        Work work;
        work.mType = Work::Type::SHUTDOWN;
        return work;
      }

      // Nothing to do; block until some work is available.
      mMonitor.Wait();
    } while (true);
  }

private:
  ~DecodePoolImpl() { }

  Work PopWorkFromQueue(nsTArray<RefPtr<IDecodingTask>>& aQueue)
  {
    Work work;
    work.mType = Work::Type::TASK;
    work.mTask = aQueue.LastElement().forget();
    aQueue.RemoveElementAt(aQueue.Length() - 1);

    return work;
  }

  nsThreadPoolNaming mThreadNaming;

  // mMonitor guards the queues and mShuttingDown.
  Monitor mMonitor;
  nsTArray<RefPtr<IDecodingTask>> mHighPriorityQueue;
  nsTArray<RefPtr<IDecodingTask>> mLowPriorityQueue;
  bool mShuttingDown;
};

class DecodePoolWorker : public Runnable
{
public:
  explicit DecodePoolWorker(DecodePoolImpl* aImpl)
    : mImpl(aImpl)
  { }

  NS_IMETHOD Run() override
  {
#ifdef MOZ_ENABLE_PROFILER_SPS
    char stackBaseGuess; // Need to be the first variable of main loop function.
#endif // MOZ_ENABLE_PROFILER_SPS

    MOZ_ASSERT(!NS_IsMainThread());

    mImpl->InitCurrentThread();

    nsCOMPtr<nsIThread> thisThread;
    nsThreadManager::get().GetCurrentThread(getter_AddRefs(thisThread));

#ifdef MOZ_ENABLE_PROFILER_SPS
    // InitCurrentThread() has assigned the thread name.
    profiler_register_thread(PR_GetThreadName(PR_GetCurrentThread()), &stackBaseGuess);
#endif // MOZ_ENABLE_PROFILER_SPS

    do {
      Work work = mImpl->PopWork();
      switch (work.mType) {
        case Work::Type::TASK:
          work.mTask->Run();
          break;

        case Work::Type::SHUTDOWN:
          DecodePoolImpl::ShutdownThread(thisThread);

#ifdef MOZ_ENABLE_PROFILER_SPS
          profiler_unregister_thread();
#endif // MOZ_ENABLE_PROFILER_SPS

          return NS_OK;

        default:
          MOZ_ASSERT_UNREACHABLE("Unknown work type");
      }
    } while (true);

    MOZ_ASSERT_UNREACHABLE("Exiting thread without Work::Type::SHUTDOWN");
    return NS_OK;
  }

private:
  RefPtr<DecodePoolImpl> mImpl;
};

/* static */ void
DecodePool::Initialize()
{
  MOZ_ASSERT(NS_IsMainThread());
  sNumCores = max<int32_t>(PR_GetNumberOfProcessors(), 1);
  DecodePool::Singleton();
}

/* static */ DecodePool*
DecodePool::Singleton()
{
  if (!sSingleton) {
    MOZ_ASSERT(NS_IsMainThread());
    sSingleton = new DecodePool();
    ClearOnShutdown(&sSingleton);
  }

  return sSingleton;
}

/* static */ uint32_t
DecodePool::NumberOfCores()
{
  return sNumCores;
}

DecodePool::DecodePool()
  : mImpl(new DecodePoolImpl)
  , mMutex("image::DecodePool")
{
  // Determine the number of threads we want.
  int32_t prefLimit = gfxPrefs::ImageMTDecodingLimit();
  uint32_t limit;
  if (prefLimit <= 0) {
    int32_t numCores = NumberOfCores();
    if (numCores <= 1) {
      limit = 1;
    } else if (numCores == 2) {
      // On an otherwise mostly idle system, having two image decoding threads
      // doubles decoding performance, so it's worth doing on dual-core devices,
      // even if under load we can't actually get that level of parallelism.
      limit = 2;
    } else {
      limit = numCores - 1;
    }
  } else {
    limit = static_cast<uint32_t>(prefLimit);
  }
  if (limit > 32) {
    limit = 32;
  }

  // Initialize the thread pool.
  for (uint32_t i = 0 ; i < limit ; ++i) {
    nsCOMPtr<nsIRunnable> worker = new DecodePoolWorker(mImpl);
    nsCOMPtr<nsIThread> thread;
    nsresult rv = NS_NewThread(getter_AddRefs(thread), worker);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv) && thread,
                       "Should successfully create image decoding threads");
    mThreads.AppendElement(Move(thread));
  }

  // Initialize the I/O thread.
  nsresult rv = NS_NewNamedThread("ImageIO", getter_AddRefs(mIOThread));
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv) && mIOThread,
                     "Should successfully create image I/O thread");

  nsCOMPtr<nsIObserverService> obsSvc = services::GetObserverService();
  if (obsSvc) {
    obsSvc->AddObserver(this, "xpcom-shutdown-threads", false);
  }
}

DecodePool::~DecodePool()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must shut down DecodePool on main thread!");
}

NS_IMETHODIMP
DecodePool::Observe(nsISupports*, const char* aTopic, const char16_t*)
{
  MOZ_ASSERT(strcmp(aTopic, "xpcom-shutdown-threads") == 0, "Unexpected topic");

  nsTArray<nsCOMPtr<nsIThread>> threads;
  nsCOMPtr<nsIThread> ioThread;

  {
    MutexAutoLock lock(mMutex);
    threads.SwapElements(mThreads);
    ioThread.swap(mIOThread);
  }

  mImpl->RequestShutdown();

  for (uint32_t i = 0 ; i < threads.Length() ; ++i) {
    threads[i]->Shutdown();
  }

  if (ioThread) {
    ioThread->Shutdown();
  }

  return NS_OK;
}

void
DecodePool::AsyncRun(IDecodingTask* aTask)
{
  MOZ_ASSERT(aTask);
  mImpl->PushWork(aTask);
}

void
DecodePool::SyncRunIfPreferred(IDecodingTask* aTask)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTask);

  if (aTask->ShouldPreferSyncRun()) {
    aTask->Run();
    return;
  }

  AsyncRun(aTask);
}

void
DecodePool::SyncRunIfPossible(IDecodingTask* aTask)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTask);
  aTask->Run();
}

already_AddRefed<nsIEventTarget>
DecodePool::GetIOEventTarget()
{
  MutexAutoLock threadPoolLock(mMutex);
  nsCOMPtr<nsIEventTarget> target = do_QueryInterface(mIOThread);
  return target.forget();
}

} // namespace image
} // namespace mozilla

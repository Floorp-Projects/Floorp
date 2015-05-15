/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecodePool.h"

#include <algorithm>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Monitor.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsIThreadPool.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "prsystem.h"

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

#include "gfxPrefs.h"

#include "Decoder.h"
#include "RasterImage.h"

using std::max;
using std::min;

namespace mozilla {
namespace image {

///////////////////////////////////////////////////////////////////////////////
// Helper runnables.
///////////////////////////////////////////////////////////////////////////////

class NotifyProgressWorker : public nsRunnable
{
public:
  /**
   * Called by the DecodePool when it's done some significant portion of
   * decoding, so that progress can be recorded and notifications can be sent.
   */
  static void Dispatch(RasterImage* aImage,
                       Progress aProgress,
                       const nsIntRect& aInvalidRect,
                       uint32_t aFlags)
  {
    MOZ_ASSERT(aImage);

    nsCOMPtr<nsIRunnable> worker =
      new NotifyProgressWorker(aImage, aProgress, aInvalidRect, aFlags);
    NS_DispatchToMainThread(worker);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    mImage->NotifyProgress(mProgress, mInvalidRect, mFlags);
    return NS_OK;
  }

private:
  NotifyProgressWorker(RasterImage* aImage, Progress aProgress,
                       const nsIntRect& aInvalidRect, uint32_t aFlags)
    : mImage(aImage)
    , mProgress(aProgress)
    , mInvalidRect(aInvalidRect)
    , mFlags(aFlags)
  { }

  nsRefPtr<RasterImage> mImage;
  const Progress mProgress;
  const nsIntRect mInvalidRect;
  const uint32_t mFlags;
};

class NotifyDecodeCompleteWorker : public nsRunnable
{
public:
  /**
   * Called by the DecodePool when decoding is complete, so that final cleanup
   * can be performed.
   */
  static void Dispatch(Decoder* aDecoder)
  {
    MOZ_ASSERT(aDecoder);

    nsCOMPtr<nsIRunnable> worker = new NotifyDecodeCompleteWorker(aDecoder);
    NS_DispatchToMainThread(worker);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    mDecoder->Finish();
    mDecoder->GetImage()->FinalizeDecoder(mDecoder);
    return NS_OK;
  }

private:
  explicit NotifyDecodeCompleteWorker(Decoder* aDecoder)
    : mDecoder(aDecoder)
  { }

  nsRefPtr<Decoder> mDecoder;
};

#ifdef MOZ_NUWA_PROCESS

class RegisterDecodeIOThreadWithNuwaRunnable : public nsRunnable
{
public:
  NS_IMETHOD Run()
  {
    NuwaMarkCurrentThread(static_cast<void(*)(void*)>(nullptr), nullptr);
    return NS_OK;
  }
};

#endif // MOZ_NUWA_PROCESS


///////////////////////////////////////////////////////////////////////////////
// DecodePool implementation.
///////////////////////////////////////////////////////////////////////////////

/* static */ StaticRefPtr<DecodePool> DecodePool::sSingleton;

NS_IMPL_ISUPPORTS(DecodePool, nsIObserver)

struct Work
{
  enum class Type {
    DECODE,
    SHUTDOWN
  } mType;

  nsRefPtr<Decoder> mDecoder;
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

#ifdef MOZ_NUWA_PROCESS
    if (IsNuwaProcess()) {
      NuwaMarkCurrentThread(static_cast<void(*)(void*)>(nullptr), nullptr);
    }
#endif // MOZ_NUWA_PROCESS
  }

  /// Shut down the provided decode pool thread.
  static void ShutdownThread(nsIThread* aThisThread)
  {
    // Threads have to be shut down from another thread, so we'll ask the
    // main thread to do it for us.
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableMethod(aThisThread, &nsIThread::Shutdown);
    NS_DispatchToMainThread(runnable);
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
  void PushWork(Decoder* aDecoder)
  {
    nsRefPtr<Decoder> decoder(aDecoder);

    MonitorAutoLock lock(mMonitor);

    if (mShuttingDown) {
      // Drop any new work on the floor if we're shutting down.
      return;
    }

    mQueue.AppendElement(Move(decoder));
    mMonitor.Notify();
  }

  /// Pops a new work item, blocking if necessary.
  Work PopWork()
  {
    Work work;

    MonitorAutoLock lock(mMonitor);

    do {
      if (!mQueue.IsEmpty()) {
        // XXX(seth): This is NOT efficient, obviously, since we're removing an
        // element from the front of the array. However, it's not worth
        // implementing something better right now, because we are replacing
        // this FIFO behavior with LIFO behavior very soon.
        work.mType = Work::Type::DECODE;
        work.mDecoder = mQueue.ElementAt(0);
        mQueue.RemoveElementAt(0);

#ifdef MOZ_NUWA_PROCESS
        nsThreadManager::get()->SetThreadWorking();
#endif // MOZ_NUWA_PROCESS

        return work;
      }

      if (mShuttingDown) {
        work.mType = Work::Type::SHUTDOWN;
        return work;
      }

#ifdef MOZ_NUWA_PROCESS
      nsThreadManager::get()->SetThreadIdle(nullptr);
#endif // MOZ_NUWA_PROCESS

      // Nothing to do; block until some work is available.
      mMonitor.Wait();
    } while (true);
  }

private:
  ~DecodePoolImpl() { }

  nsThreadPoolNaming mThreadNaming;

  // mMonitor guards mQueue and mShuttingDown.
  Monitor mMonitor;
  nsTArray<nsRefPtr<Decoder>> mQueue;
  bool mShuttingDown;
};

class DecodePoolWorker : public nsRunnable
{
public:
  explicit DecodePoolWorker(DecodePoolImpl* aImpl) : mImpl(aImpl) { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    mImpl->InitCurrentThread();

    nsCOMPtr<nsIThread> thisThread;
    nsThreadManager::get()->GetCurrentThread(getter_AddRefs(thisThread));

    do {
      Work work = mImpl->PopWork();
      switch (work.mType) {
        case Work::Type::DECODE:
          DecodePool::Singleton()->Decode(work.mDecoder);
          break;

        case Work::Type::SHUTDOWN:
          DecodePoolImpl::ShutdownThread(thisThread);
          return NS_OK;

        default:
          MOZ_ASSERT_UNREACHABLE("Unknown work type");
      }
    } while (true);

    MOZ_ASSERT_UNREACHABLE("Exiting thread without Work::Type::SHUTDOWN");
    return NS_OK;
  }

private:
  nsRefPtr<DecodePoolImpl> mImpl;
};

/* static */ void
DecodePool::Initialize()
{
  MOZ_ASSERT(NS_IsMainThread());
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

DecodePool::DecodePool()
  : mImpl(new DecodePoolImpl)
  , mMutex("image::DecodePool")
{
  // Determine the number of threads we want.
  int32_t prefLimit = gfxPrefs::ImageMTDecodingLimit();
  uint32_t limit;
  if (prefLimit <= 0) {
    limit = max(PR_GetNumberOfProcessors(), 2) - 1;
  } else {
    limit = static_cast<uint32_t>(prefLimit);
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

#ifdef MOZ_NUWA_PROCESS
  nsCOMPtr<nsIRunnable> worker = new RegisterDecodeIOThreadWithNuwaRunnable();
  rv = mIOThread->Dispatch(worker, NS_DISPATCH_NORMAL);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv),
                     "Should register decode IO thread with Nuwa process");
#endif

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

  nsCOMArray<nsIThread> threads;
  nsCOMPtr<nsIThread> ioThread;

  {
    MutexAutoLock lock(mMutex);
    threads.AppendElements(mThreads);
    mThreads.Clear();
    ioThread.swap(mIOThread);
  }

  mImpl->RequestShutdown();

  for (int32_t i = 0 ; i < threads.Count() ; ++i) {
    threads[i]->Shutdown();
  }

  if (ioThread) {
    ioThread->Shutdown();
  }

  return NS_OK;
}

void
DecodePool::AsyncDecode(Decoder* aDecoder)
{
  MOZ_ASSERT(aDecoder);
  mImpl->PushWork(aDecoder);
}

void
DecodePool::SyncDecodeIfSmall(Decoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDecoder);

  if (aDecoder->ShouldSyncDecode(gfxPrefs::ImageMemDecodeBytesAtATime())) {
    Decode(aDecoder);
    return;
  }

  AsyncDecode(aDecoder);
}

void
DecodePool::SyncDecodeIfPossible(Decoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());
  Decode(aDecoder);
}

already_AddRefed<nsIEventTarget>
DecodePool::GetIOEventTarget()
{
  MutexAutoLock threadPoolLock(mMutex);
  nsCOMPtr<nsIEventTarget> target = do_QueryInterface(mIOThread);
  return target.forget();
}

void
DecodePool::Decode(Decoder* aDecoder)
{
  MOZ_ASSERT(aDecoder);

  nsresult rv = aDecoder->Decode();

  if (NS_SUCCEEDED(rv) && !aDecoder->GetDecodeDone()) {
    if (aDecoder->HasProgress()) {
      NotifyProgress(aDecoder);
    }
    // The decoder will ensure that a new worker gets enqueued to continue
    // decoding when more data is available.
  } else {
    NotifyDecodeComplete(aDecoder);
  }
}

void
DecodePool::NotifyProgress(Decoder* aDecoder)
{
  MOZ_ASSERT(aDecoder);

  if (!NS_IsMainThread() ||
      (aDecoder->GetFlags() & imgIContainer::FLAG_ASYNC_NOTIFY)) {
    NotifyProgressWorker::Dispatch(aDecoder->GetImage(),
                                   aDecoder->TakeProgress(),
                                   aDecoder->TakeInvalidRect(),
                                   aDecoder->GetDecodeFlags());
    return;
  }

  aDecoder->GetImage()->NotifyProgress(aDecoder->TakeProgress(),
                                       aDecoder->TakeInvalidRect(),
                                       aDecoder->GetDecodeFlags());
}

void
DecodePool::NotifyDecodeComplete(Decoder* aDecoder)
{
  MOZ_ASSERT(aDecoder);

  if (!NS_IsMainThread() ||
      (aDecoder->GetFlags() & imgIContainer::FLAG_ASYNC_NOTIFY)) {
    NotifyDecodeCompleteWorker::Dispatch(aDecoder);
    return;
  }

  aDecoder->Finish();
  aDecoder->GetImage()->FinalizeDecoder(aDecoder);
}

} // namespace image
} // namespace mozilla

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecodePool.h"

#include <algorithm>

#include "mozilla/ClearOnShutdown.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsIThreadPool.h"
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

  NS_IMETHOD Run() MOZ_OVERRIDE
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

  NS_IMETHOD Run() MOZ_OVERRIDE
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

class DecodeWorker : public nsRunnable
{
public:
  explicit DecodeWorker(Decoder* aDecoder)
    : mDecoder(aDecoder)
  {
    MOZ_ASSERT(mDecoder);
  }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread());
    DecodePool::Singleton()->Decode(mDecoder);
    return NS_OK;
  }

private:
  nsRefPtr<Decoder> mDecoder;
};

#ifdef MOZ_NUWA_PROCESS

class DecodePoolNuwaListener MOZ_FINAL : public nsIThreadPoolListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHODIMP OnThreadCreated()
  {
    if (IsNuwaProcess()) {
      NuwaMarkCurrentThread(static_cast<void(*)(void*)>(nullptr), nullptr);
    }
    return NS_OK;
  }

  NS_IMETHODIMP OnThreadShuttingDown() { return NS_OK; }

private:
  ~DecodePoolNuwaListener() { }
};

NS_IMPL_ISUPPORTS(DecodePoolNuwaListener, nsIThreadPoolListener)

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
  : mMutex("image::DecodePool")
{
  // Initialize the thread pool.
  mThreadPool = do_CreateInstance(NS_THREADPOOL_CONTRACTID);
  MOZ_RELEASE_ASSERT(mThreadPool,
                     "Should succeed in creating image decoding thread pool");

  mThreadPool->SetName(NS_LITERAL_CSTRING("ImageDecoder"));
  int32_t prefLimit = gfxPrefs::ImageMTDecodingLimit();
  uint32_t limit;
  if (prefLimit <= 0) {
    limit = max(PR_GetNumberOfProcessors(), 2) - 1;
  } else {
    limit = static_cast<uint32_t>(prefLimit);
  }

  mThreadPool->SetThreadLimit(limit);
  mThreadPool->SetIdleThreadLimit(limit);

#ifdef MOZ_NUWA_PROCESS
  if (IsNuwaProcess()) {
    mThreadPool->SetListener(new DecodePoolNuwaListener());
  }
#endif

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

  nsCOMPtr<nsIThreadPool> threadPool;
  nsCOMPtr<nsIThread> ioThread;

  {
    MutexAutoLock lock(mMutex);
    threadPool.swap(mThreadPool);
    ioThread.swap(mIOThread);
  }

  if (threadPool) {
    threadPool->Shutdown();
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

  nsCOMPtr<nsIRunnable> worker = new DecodeWorker(aDecoder);

  // Dispatch to the thread pool if it exists. If it doesn't, we're currently
  // shutting down, so it's OK to just drop the job on the floor.
  MutexAutoLock threadPoolLock(mMutex);
  if (mThreadPool) {
    mThreadPool->Dispatch(worker, nsIEventTarget::DISPATCH_NORMAL);
  }
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
DecodePool::GetEventTarget()
{
  MutexAutoLock threadPoolLock(mMutex);
  nsCOMPtr<nsIEventTarget> target = do_QueryInterface(mThreadPool);
  return target.forget();
}

already_AddRefed<nsIEventTarget>
DecodePool::GetIOEventTarget()
{
  MutexAutoLock threadPoolLock(mMutex);
  nsCOMPtr<nsIEventTarget> target = do_QueryInterface(mIOThread);
  return target.forget();
}

already_AddRefed<nsIRunnable>
DecodePool::CreateDecodeWorker(Decoder* aDecoder)
{
  MOZ_ASSERT(aDecoder);
  nsCOMPtr<nsIRunnable> worker = new DecodeWorker(aDecoder);
  return worker.forget();
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

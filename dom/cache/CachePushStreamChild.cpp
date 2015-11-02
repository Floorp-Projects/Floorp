/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CachePushStreamChild.h"

#include "mozilla/unused.h"
#include "nsIAsyncInputStream.h"
#include "nsICancelableRunnable.h"
#include "nsIThread.h"
#include "nsStreamUtils.h"

namespace mozilla {
namespace dom {
namespace cache {

class CachePushStreamChild::Callback final : public nsIInputStreamCallback
                                           , public nsICancelableRunnable
{
public:
  explicit Callback(CachePushStreamChild* aActor)
    : mActor(aActor)
    , mOwningThread(NS_GetCurrentThread())
  {
    MOZ_ASSERT(mActor);
  }

  NS_IMETHOD
  OnInputStreamReady(nsIAsyncInputStream* aStream) override
  {
    // any thread
    if (mOwningThread == NS_GetCurrentThread()) {
      return Run();
    }

    // If this fails, then it means the owning thread is a Worker that has
    // been shutdown.  Its ok to lose the event in this case because the
    // CachePushStreamChild listens for this event through the Feature.
    nsresult rv = mOwningThread->Dispatch(this, nsIThread::DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch stream readable event to owning thread");
    }

    return NS_OK;
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(mOwningThread == NS_GetCurrentThread());
    if (mActor) {
      mActor->OnStreamReady(this);
    }
    return NS_OK;
  }

  NS_IMETHOD
  Cancel() override
  {
    // Cancel() gets called when the Worker thread is being shutdown.  We have
    // nothing to do here because CachePushStreamChild handles this case via
    // the Feature.
    return NS_OK;
  }

  void
  ClearActor()
  {
    MOZ_ASSERT(mOwningThread == NS_GetCurrentThread());
    MOZ_ASSERT(mActor);
    mActor = nullptr;
  }

private:
  ~Callback()
  {
    // called on any thread

    // ClearActor() should be called before the Callback is destroyed
    MOZ_ASSERT(!mActor);
  }

  CachePushStreamChild* mActor;
  nsCOMPtr<nsIThread> mOwningThread;

  NS_DECL_THREADSAFE_ISUPPORTS
};

NS_IMPL_ISUPPORTS(CachePushStreamChild::Callback, nsIInputStreamCallback,
                                                  nsIRunnable,
                                                  nsICancelableRunnable);

CachePushStreamChild::CachePushStreamChild(Feature* aFeature,
                                           nsISupports* aParent,
                                           nsIAsyncInputStream* aStream)
  : mParent(aParent)
  , mStream(aStream)
  , mClosed(false)
{
  MOZ_ASSERT(mParent);
  MOZ_ASSERT(mStream);
  MOZ_ASSERT_IF(!NS_IsMainThread(), aFeature);
  SetFeature(aFeature);
}

CachePushStreamChild::~CachePushStreamChild()
{
  NS_ASSERT_OWNINGTHREAD(CachePushStreamChild);
  MOZ_ASSERT(mClosed);
  MOZ_ASSERT(!mCallback);
}

void
CachePushStreamChild::Start()
{
  DoRead();
}

void
CachePushStreamChild::StartDestroy()
{
  // The worker has signaled its shutting down, but continue streaming.  The
  // Cache is now designed to hold the worker open until all async operations
  // complete.
}

void
CachePushStreamChild::ActorDestroy(ActorDestroyReason aReason)
{
  NS_ASSERT_OWNINGTHREAD(CachePushStreamChild);

  // If the parent side runs into a problem then the actor will be destroyed.
  // In this case we have not run OnEnd(), so still need to close the input
  // stream.
  if (!mClosed) {
    mStream->CloseWithStatus(NS_ERROR_ABORT);
    mClosed = true;
  }

  if (mCallback) {
    mCallback->ClearActor();
    mCallback = nullptr;
  }

  RemoveFeature();
}

void
CachePushStreamChild::DoRead()
{
  NS_ASSERT_OWNINGTHREAD(CachePushStreamChild);
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(!mCallback);

  // The input stream (likely a pipe) probably uses a segment size of
  // 4kb.  If there is data already buffered it would be nice to aggregate
  // multiple segments into a single IPC call.  Conversely, don't send too
  // too large of a buffer in a single call to avoid spiking memory.
  static const uint64_t kMaxBytesPerMessage = 32 * 1024;
  static_assert(kMaxBytesPerMessage <= static_cast<uint64_t>(UINT32_MAX),
                "kMaxBytesPerMessage must cleanly cast to uint32_t");

  while (!mClosed) {
    // Use non-auto here as we're unlikely to hit stack storage with the
    // sizes we are sending.  Also, it would be nice to avoid another copy
    // to the IPC layer which we avoid if we use COW strings.  Unfortunately
    // IPC does not seem to support passing dependent storage types.
    nsCString buffer;

    uint64_t available = 0;
    nsresult rv = mStream->Available(&available);
    if (NS_FAILED(rv)) {
      OnEnd(rv);
      return;
    }

    if (available == 0) {
      Wait();
      return;
    }

    uint32_t expectedBytes =
      static_cast<uint32_t>(std::min(available, kMaxBytesPerMessage));

    buffer.SetLength(expectedBytes);

    uint32_t bytesRead = 0;
    rv = mStream->Read(buffer.BeginWriting(), buffer.Length(), &bytesRead);
    buffer.SetLength(bytesRead);

    // If we read any data from the stream, send it across.
    if (!buffer.IsEmpty()) {
      Unused << SendBuffer(buffer);
    }

    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      Wait();
      return;
    }

    // Any other error or zero-byte read indicates end-of-stream
    if (NS_FAILED(rv) || buffer.IsEmpty()) {
      OnEnd(rv);
      return;
    }
  }
}

void
CachePushStreamChild::Wait()
{
  NS_ASSERT_OWNINGTHREAD(CachePushStreamChild);
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(!mCallback);

  // Set mCallback immediately instead of waiting for success.  Its possible
  // AsyncWait() will callback synchronously.
  mCallback = new Callback(this);
  nsresult rv = mStream->AsyncWait(mCallback, 0, 0, nullptr);
  if (NS_FAILED(rv)) {
    OnEnd(rv);
    return;
  }
}

void
CachePushStreamChild::OnStreamReady(Callback* aCallback)
{
  NS_ASSERT_OWNINGTHREAD(CachePushStreamChild);
  MOZ_ASSERT(mCallback);
  MOZ_ASSERT(aCallback == mCallback);
  mCallback->ClearActor();
  mCallback = nullptr;
  DoRead();
}

void
CachePushStreamChild::OnEnd(nsresult aRv)
{
  NS_ASSERT_OWNINGTHREAD(CachePushStreamChild);
  MOZ_ASSERT(aRv != NS_BASE_STREAM_WOULD_BLOCK);

  if (mClosed) {
    return;
  }

  mClosed = true;

  mStream->CloseWithStatus(aRv);

  if (aRv == NS_BASE_STREAM_CLOSED) {
    aRv = NS_OK;
  }

  // This will trigger an ActorDestroy() from the parent side
  Unused << SendClose(aRv);
}

} // namespace cache
} // namespace dom
} // namespace mozilla

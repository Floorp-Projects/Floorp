/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCStreamSource.h"
#include "BackgroundParent.h"  // for AssertIsOnBackgroundThread
#include "mozilla/dom/RemoteWorkerService.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "nsIAsyncInputStream.h"
#include "nsICancelableRunnable.h"
#include "nsIRunnable.h"
#include "nsISerialEventTarget.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"

using mozilla::wr::ByteBuffer;

namespace mozilla {
namespace ipc {

class IPCStreamSource::Callback final : public nsIInputStreamCallback,
                                        public nsIRunnable,
                                        public nsICancelableRunnable {
 public:
  explicit Callback(IPCStreamSource* aSource)
      : mSource(aSource),
        mOwningEventTarget(GetCurrentThreadSerialEventTarget()) {
    MOZ_ASSERT(mSource);
  }

  NS_IMETHOD
  OnInputStreamReady(nsIAsyncInputStream* aStream) override {
    // any thread
    if (mOwningEventTarget->IsOnCurrentThread()) {
      return Run();
    }

    // If this fails, then it means the owning thread is a Worker that has
    // been shutdown.  Its ok to lose the event in this case because the
    // IPCStreamChild listens for this event through the WorkerRef.
    nsresult rv =
        mOwningEventTarget->Dispatch(this, nsIThread::DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch stream readable event to owning thread");
    }

    return NS_OK;
  }

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(mOwningEventTarget->IsOnCurrentThread());
    if (mSource) {
      mSource->OnStreamReady(this);
    }
    return NS_OK;
  }

  nsresult Cancel() override {
    // Cancel() gets called when the Worker thread is being shutdown.  We have
    // nothing to do here because IPCStreamChild handles this case via
    // the WorkerRef.
    return NS_OK;
  }

  void ClearSource() {
    MOZ_ASSERT(mOwningEventTarget->IsOnCurrentThread());
    MOZ_ASSERT(mSource);
    mSource = nullptr;
  }

 private:
  ~Callback() {
    // called on any thread

    // ClearSource() should be called before the Callback is destroyed
    MOZ_ASSERT(!mSource);
  }

  // This is a raw pointer because the source keeps alive the callback and,
  // before beeing destroyed, it nullifies this pointer (this happens when
  // ActorDestroyed() is called).
  IPCStreamSource* mSource;

  nsCOMPtr<nsISerialEventTarget> mOwningEventTarget;

  NS_DECL_THREADSAFE_ISUPPORTS
};

NS_IMPL_ISUPPORTS(IPCStreamSource::Callback, nsIInputStreamCallback,
                  nsIRunnable, nsICancelableRunnable);

IPCStreamSource::IPCStreamSource(nsIAsyncInputStream* aInputStream)
    : mStream(aInputStream), mState(ePending) {
  MOZ_ASSERT(aInputStream);
}

IPCStreamSource::~IPCStreamSource() {
  NS_ASSERT_OWNINGTHREAD(IPCStreamSource);
  MOZ_ASSERT(mState == eClosed);
  MOZ_ASSERT(!mCallback);
  MOZ_ASSERT(!mWorkerRef);
}

bool IPCStreamSource::Initialize() {
  bool nonBlocking = false;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mStream->IsNonBlocking(&nonBlocking)));
  // IPCStreamChild reads in the current thread, so it is only supported on
  // non-blocking, async channels
  if (!nonBlocking) {
    return false;
  }

  // A source can be used on any thread, but we only support IPCStream on
  // main thread, Workers, Worker Launcher, and PBackground thread right now.
  // This is due to the requirement  that the thread be guaranteed to live long
  // enough to receive messages. We can enforce this guarantee with a
  // StrongWorkerRef on worker threads, but not other threads. Main-thread,
  // PBackground, and Worker Launcher threads do not need anything special in
  // order to be kept alive.
  if (!NS_IsMainThread()) {
    if (const auto workerPrivate = dom::GetCurrentThreadWorkerPrivate()) {
      RefPtr<dom::StrongWorkerRef> workerRef =
          dom::StrongWorkerRef::CreateForcibly(workerPrivate,
                                               "IPCStreamSource");
      if (NS_WARN_IF(!workerRef)) {
        return false;
      }

      mWorkerRef = std::move(workerRef);
    } else {
      MOZ_DIAGNOSTIC_ASSERT(
          IsOnBackgroundThread() ||
          dom::RemoteWorkerService::Thread()->IsOnCurrentThread());
    }
  }

  return true;
}

void IPCStreamSource::ActorConstructed() {
  MOZ_ASSERT(mState == ePending);
  mState = eActorConstructed;
}

void IPCStreamSource::ActorDestroyed() {
  NS_ASSERT_OWNINGTHREAD(IPCStreamSource);

  mState = eClosed;

  if (mCallback) {
    mCallback->ClearSource();
    mCallback = nullptr;
  }

  mWorkerRef = nullptr;
}

void IPCStreamSource::Start() {
  NS_ASSERT_OWNINGTHREAD(IPCStreamSource);
  DoRead();
}

void IPCStreamSource::StartDestroy() {
  NS_ASSERT_OWNINGTHREAD(IPCStreamSource);
  OnEnd(NS_ERROR_ABORT);
}

void IPCStreamSource::DoRead() {
  NS_ASSERT_OWNINGTHREAD(IPCStreamSource);
  MOZ_ASSERT(mState == eActorConstructed);
  MOZ_ASSERT(!mCallback);

  // The input stream (likely a pipe) probably uses a segment size of
  // 4kb.  If there is data already buffered it would be nice to aggregate
  // multiple segments into a single IPC call.  Conversely, don't send too
  // too large of a buffer in a single call to avoid spiking memory.
  static const uint64_t kMaxBytesPerMessage = 32 * 1024;
  static_assert(kMaxBytesPerMessage <= static_cast<uint64_t>(UINT32_MAX),
                "kMaxBytesPerMessage must cleanly cast to uint32_t");

  char buffer[kMaxBytesPerMessage];

  while (true) {
    // It should not be possible to transition to closed state without
    // this loop terminating via a return.
    MOZ_ASSERT(mState == eActorConstructed);

    // See if the stream is closed by checking the return of Available.
    uint64_t dummy;
    nsresult rv = mStream->Available(&dummy);
    if (NS_FAILED(rv)) {
      OnEnd(rv);
      return;
    }

    uint32_t bytesRead = 0;
    rv = mStream->Read(buffer, kMaxBytesPerMessage, &bytesRead);

    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      MOZ_ASSERT(bytesRead == 0);
      Wait();
      return;
    }

    if (NS_FAILED(rv)) {
      MOZ_ASSERT(bytesRead == 0);
      OnEnd(rv);
      return;
    }

    // Zero-byte read indicates end-of-stream.
    if (bytesRead == 0) {
      OnEnd(NS_BASE_STREAM_CLOSED);
      return;
    }

    // We read some data from the stream, send it across.
    SendData(ByteBuffer(bytesRead, reinterpret_cast<uint8_t*>(buffer)));
  }
}

void IPCStreamSource::Wait() {
  NS_ASSERT_OWNINGTHREAD(IPCStreamSource);
  MOZ_ASSERT(mState == eActorConstructed);
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

void IPCStreamSource::OnStreamReady(Callback* aCallback) {
  NS_ASSERT_OWNINGTHREAD(IPCStreamSource);
  MOZ_ASSERT(mCallback);
  MOZ_ASSERT(aCallback == mCallback);
  mCallback->ClearSource();
  mCallback = nullptr;

  // Possibly closed if this callback is (indirectly) called by
  // IPCStreamSourceParent::RecvRequestClose().
  if (mState == eClosed) {
    return;
  }

  DoRead();
}

void IPCStreamSource::OnEnd(nsresult aRv) {
  NS_ASSERT_OWNINGTHREAD(IPCStreamSource);
  MOZ_ASSERT(aRv != NS_BASE_STREAM_WOULD_BLOCK);

  if (mState == eClosed) {
    return;
  }

  mState = eClosed;

  mStream->CloseWithStatus(aRv);

  if (aRv == NS_BASE_STREAM_CLOSED) {
    aRv = NS_OK;
  }

  // This will trigger an ActorDestroy() from the other side
  Close(aRv);
}

}  // namespace ipc
}  // namespace mozilla

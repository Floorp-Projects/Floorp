/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/SendStream.h"

#include "mozilla/Unused.h"
#include "mozilla/dom/nsIContentChild.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/workers/bindings/WorkerHolder.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsIAsyncInputStream.h"
#include "nsICancelableRunnable.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsStreamUtils.h"

namespace mozilla {
namespace ipc {

using mozilla::dom::nsIContentChild;
using mozilla::dom::workers::Canceling;
using mozilla::dom::workers::GetCurrentThreadWorkerPrivate;
using mozilla::dom::workers::Status;
using mozilla::dom::workers::WorkerHolder;
using mozilla::dom::workers::WorkerPrivate;

namespace {

class SendStreamChildImpl final : public SendStreamChild
                                , public WorkerHolder
{
public:
  explicit SendStreamChildImpl(nsIAsyncInputStream* aStream);
  ~SendStreamChildImpl();

  void Start() override;
  void StartDestroy() override;

  bool
  AddAsWorkerHolder(dom::workers::WorkerPrivate* aWorkerPrivate);

private:
  class Callback;

  // PSendStreamChild methods
  virtual void
  ActorDestroy(ActorDestroyReason aReason) override;

  virtual mozilla::ipc::IPCResult
  RecvRequestClose(const nsresult& aRv) override;

  // WorkerHolder methods
  virtual bool
  Notify(Status aStatus) override;

  void DoRead();

  void Wait();

  void OnStreamReady(Callback* aCallback);

  void OnEnd(nsresult aRv);

  nsCOMPtr<nsIAsyncInputStream> mStream;
  RefPtr<Callback> mCallback;
  WorkerPrivate* mWorkerPrivate;
  bool mClosed;

  NS_DECL_OWNINGTHREAD
};

class SendStreamChildImpl::Callback final : public nsIInputStreamCallback
                                          , public nsIRunnable
                                          , public nsICancelableRunnable
{
public:
  explicit Callback(SendStreamChildImpl* aActor)
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
    // SendStreamChild listens for this event through the WorkerHolder.
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

  nsresult
  Cancel() override
  {
    // Cancel() gets called when the Worker thread is being shutdown.  We have
    // nothing to do here because SendStreamChild handles this case via
    // the WorkerHolder.
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

  SendStreamChildImpl* mActor;
  nsCOMPtr<nsIThread> mOwningThread;

  NS_DECL_THREADSAFE_ISUPPORTS
};

NS_IMPL_ISUPPORTS(SendStreamChildImpl::Callback, nsIInputStreamCallback,
                                                 nsIRunnable,
                                                 nsICancelableRunnable);

SendStreamChildImpl::SendStreamChildImpl(nsIAsyncInputStream* aStream)
  : mStream(aStream)
  , mWorkerPrivate(nullptr)
  , mClosed(false)
{
  MOZ_ASSERT(mStream);
}

SendStreamChildImpl::~SendStreamChildImpl()
{
  NS_ASSERT_OWNINGTHREAD(SendStreamChild);
  MOZ_ASSERT(mClosed);
  MOZ_ASSERT(!mCallback);
  MOZ_ASSERT(!mWorkerPrivate);
}

void
SendStreamChildImpl::Start()
{
  NS_ASSERT_OWNINGTHREAD(SendStreamChild);
  MOZ_ASSERT_IF(!NS_IsMainThread(), mWorkerPrivate);
  DoRead();
}

void
SendStreamChildImpl::StartDestroy()
{
  NS_ASSERT_OWNINGTHREAD(SendStreamChild);
  OnEnd(NS_ERROR_ABORT);
}

bool
SendStreamChildImpl::AddAsWorkerHolder(WorkerPrivate* aWorkerPrivate)
{
  NS_ASSERT_OWNINGTHREAD(SendStreamChild);
  MOZ_ASSERT(aWorkerPrivate);
  bool result = HoldWorker(aWorkerPrivate, Canceling);
  if (result) {
    mWorkerPrivate = aWorkerPrivate;
  }
  return result;
}

void
SendStreamChildImpl::ActorDestroy(ActorDestroyReason aReason)
{
  NS_ASSERT_OWNINGTHREAD(SendStreamChild);

  // If the parent side runs into a problem it will ask the child to
  // close the connection via RequestClose().  Therefore OnEnd() should
  // always run before the actor is destroyed.
  MOZ_ASSERT(mClosed);

  if (mCallback) {
    mCallback->ClearActor();
    mCallback = nullptr;
  }

  if (mWorkerPrivate) {
    ReleaseWorker();
    mWorkerPrivate = nullptr;
  }
}

mozilla::ipc::IPCResult
SendStreamChildImpl::RecvRequestClose(const nsresult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(SendStreamChild);
  OnEnd(aRv);
  return IPC_OK();
}

bool
SendStreamChildImpl::Notify(Status aStatus)
{
  NS_ASSERT_OWNINGTHREAD(SendStreamChild);

  // Keep the worker thread alive until the stream is finished.
  return true;
}

void
SendStreamChildImpl::DoRead()
{
  NS_ASSERT_OWNINGTHREAD(SendStreamChild);
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(!mCallback);

  // The input stream (likely a pipe) probably uses a segment size of
  // 4kb.  If there is data already buffered it would be nice to aggregate
  // multiple segments into a single IPC call.  Conversely, don't send too
  // too large of a buffer in a single call to avoid spiking memory.
  static const uint64_t kMaxBytesPerMessage = 32 * 1024;
  static_assert(kMaxBytesPerMessage <= static_cast<uint64_t>(UINT32_MAX),
                "kMaxBytesPerMessage must cleanly cast to uint32_t");

  while (true) {
    // It should not be possible to transition to closed state without
    // this loop terminating via a return.
    MOZ_ASSERT(!mClosed);

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
    MOZ_ASSERT_IF(NS_FAILED(rv), bytesRead == 0);
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
SendStreamChildImpl::Wait()
{
  NS_ASSERT_OWNINGTHREAD(SendStreamChild);
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
SendStreamChildImpl::OnStreamReady(Callback* aCallback)
{
  NS_ASSERT_OWNINGTHREAD(SendStreamChild);
  MOZ_ASSERT(mCallback);
  MOZ_ASSERT(aCallback == mCallback);
  mCallback->ClearActor();
  mCallback = nullptr;
  DoRead();
}

void
SendStreamChildImpl::OnEnd(nsresult aRv)
{
  NS_ASSERT_OWNINGTHREAD(SendStreamChild);
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

bool
IsBlocking(nsIAsyncInputStream* aInputStream)
{
  bool nonBlocking = false;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(aInputStream->IsNonBlocking(&nonBlocking)));
  return !nonBlocking;
}

} // anonymous namespace

// static
SendStreamChild*
SendStreamChild::Create(nsIAsyncInputStream* aInputStream,
                        nsIContentChild* aManager)
{
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aManager);

  // PContent can only be used on the main thread
  MOZ_ASSERT(NS_IsMainThread());

  // SendStreamChild reads in the current thread, so it is only supported
  // on non-blocking, async channels
  if (NS_WARN_IF(IsBlocking(aInputStream))) {
    return nullptr;
  }

  SendStreamChild* actor = new SendStreamChildImpl(aInputStream);
  aManager->SendPSendStreamConstructor(actor);

  return actor;
}

// static
SendStreamChild*
SendStreamChild::Create(nsIAsyncInputStream* aInputStream,
                        PBackgroundChild* aManager)
{
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aManager);

  // PBackground can be used on any thread, but we only support SendStream on
  // main thread and Worker threads right now.  This is due to the requirement
  // that the thread be guaranteed to live long enough to receive messages
  // sent from parent to child.  We can enforce this guarantee with a feature
  // on worker threads, but not other threads.
  WorkerPrivate* workerPrivate = nullptr;
  if (!NS_IsMainThread()) {
    workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
  }

  // SendStreamChild reads in the current thread, so it is only supported
  // on non-blocking, async channels
  if (NS_WARN_IF(IsBlocking(aInputStream))) {
    return nullptr;
  }

  SendStreamChildImpl* actor = new SendStreamChildImpl(aInputStream);

  if (workerPrivate && !actor->AddAsWorkerHolder(workerPrivate)) {
    delete actor;
    return nullptr;
  }

  aManager->SendPSendStreamConstructor(actor);
  return actor;
}

SendStreamChild::~SendStreamChild()
{
}

void
DeallocPSendStreamChild(PSendStreamChild* aActor)
{
  delete aActor;
}

} // namespace ipc
} // namespace mozilla

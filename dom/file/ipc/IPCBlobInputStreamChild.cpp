/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobInputStreamChild.h"

namespace mozilla {
namespace dom {

namespace {

// This runnable is used in case the last stream is forgotten on the 'wrong'
// thread.
class ShutdownRunnable final : public Runnable
{
public:
  explicit ShutdownRunnable(IPCBlobInputStreamChild* aActor)
    : mActor(aActor)
  {}

  NS_IMETHOD
  Run() override
  {
    mActor->Shutdown();
    return NS_OK;
  }

private:
  RefPtr<IPCBlobInputStreamChild> mActor;
};

// This runnable is used in case StreamNeeded() has been called on a non-owning
// thread.
class StreamNeededRunnable final : public Runnable
{
public:
  explicit StreamNeededRunnable(IPCBlobInputStreamChild* aActor)
    : mActor(aActor)
  {}

  NS_IMETHOD
  Run() override
  {
    if (mActor->IsAlive()) {
      mActor->SendStreamNeeded();
    }
    return NS_OK;
  }

private:
  RefPtr<IPCBlobInputStreamChild> mActor;
};

// When the stream has been received from the parent, we inform the
// IPCBlobInputStream.
class StreamReadyRunnable final : public CancelableRunnable
{
public:
  StreamReadyRunnable(IPCBlobInputStream* aDestinationStream,
                      nsIInputStream* aCreatedStream)
    : mDestinationStream(aDestinationStream)
    , mCreatedStream(aCreatedStream)
  {
    MOZ_ASSERT(mDestinationStream);
    // mCreatedStream can be null.
  }

  NS_IMETHOD
  Run() override
  {
    mDestinationStream->StreamReady(mCreatedStream);
    return NS_OK;
  }

private:
  RefPtr<IPCBlobInputStream> mDestinationStream;
  nsCOMPtr<nsIInputStream> mCreatedStream;
};

} // anonymous

IPCBlobInputStreamChild::IPCBlobInputStreamChild(const nsID& aID,
                                                 uint64_t aSize)
  : mMutex("IPCBlobInputStreamChild::mMutex")
  , mID(aID)
  , mSize(aSize)
  , mActorAlive(true)
  , mOwningThread(NS_GetCurrentThread())
{}

IPCBlobInputStreamChild::~IPCBlobInputStreamChild()
{}

void
IPCBlobInputStreamChild::Shutdown()
{
  MutexAutoLock lock(mMutex);

  RefPtr<IPCBlobInputStreamChild> kungFuDeathGrip = this;

  mPendingOperations.Clear();

  if (mActorAlive) {
    SendClose();
    mActorAlive = false;
  }
}

void
IPCBlobInputStreamChild::ActorDestroy(IProtocol::ActorDestroyReason aReason)
{
  {
    MutexAutoLock lock(mMutex);
    mActorAlive = false;
  }

  Shutdown();
}

bool
IPCBlobInputStreamChild::IsAlive()
{
  MutexAutoLock lock(mMutex);
  return mActorAlive;
}

already_AddRefed<nsIInputStream>
IPCBlobInputStreamChild::CreateStream()
{
  MutexAutoLock lock(mMutex);

  if (!mActorAlive) {
    return nullptr;
  }

  RefPtr<IPCBlobInputStream> stream = new IPCBlobInputStream(this);
  mStreams.AppendElement(stream);
  return stream.forget();
}

void
IPCBlobInputStreamChild::ForgetStream(IPCBlobInputStream* aStream)
{
  MOZ_ASSERT(aStream);

  RefPtr<IPCBlobInputStreamChild> kungFuDeathGrip = this;

  {
    MutexAutoLock lock(mMutex);
    mStreams.RemoveElement(aStream);

    if (!mStreams.IsEmpty() || !mActorAlive) {
      return;
    }
  }

  if (mOwningThread == NS_GetCurrentThread()) {
    Shutdown();
    return;
  }

  RefPtr<ShutdownRunnable> runnable = new ShutdownRunnable(this);
  mOwningThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

void
IPCBlobInputStreamChild::StreamNeeded(IPCBlobInputStream* aStream,
                                      nsIEventTarget* aEventTarget)
{
  MutexAutoLock lock(mMutex);

  if (!mActorAlive) {
    return;
  }

  MOZ_ASSERT(mStreams.Contains(aStream));

  PendingOperation* opt = mPendingOperations.AppendElement();
  opt->mStream = aStream;
  opt->mEventTarget = aEventTarget ? aEventTarget : NS_GetCurrentThread();

  if (mOwningThread == NS_GetCurrentThread()) {
    SendStreamNeeded();
    return;
  }

  RefPtr<StreamNeededRunnable> runnable = new StreamNeededRunnable(this);
  mOwningThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

mozilla::ipc::IPCResult
IPCBlobInputStreamChild::RecvStreamReady(const OptionalIPCStream& aStream)
{
  nsCOMPtr<nsIInputStream> stream = DeserializeIPCStream(aStream);

  RefPtr<IPCBlobInputStream> pendingStream;
  nsCOMPtr<nsIEventTarget> eventTarget;

  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(!mPendingOperations.IsEmpty());
    MOZ_ASSERT(mActorAlive);

    pendingStream = mPendingOperations[0].mStream;
    eventTarget = mPendingOperations[0].mEventTarget;

    mPendingOperations.RemoveElementAt(0);
  }

  RefPtr<StreamReadyRunnable> runnable =
    new StreamReadyRunnable(pendingStream, stream);
  eventTarget->Dispatch(runnable, NS_DISPATCH_NORMAL);

  return IPC_OK();
}

} // namespace dom
} // namespace mozilla

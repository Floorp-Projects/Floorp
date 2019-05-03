/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobInputStreamChild.h"
#include "IPCBlobInputStreamThread.h"

#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerRef.h"

namespace mozilla {
namespace dom {

namespace {

// This runnable is used in case the last stream is forgotten on the 'wrong'
// thread.
class ShutdownRunnable final : public CancelableRunnable {
 public:
  explicit ShutdownRunnable(IPCBlobInputStreamChild* aActor)
      : CancelableRunnable("dom::ShutdownRunnable"), mActor(aActor) {}

  NS_IMETHOD
  Run() override {
    mActor->Shutdown();
    return NS_OK;
  }

 private:
  RefPtr<IPCBlobInputStreamChild> mActor;
};

// This runnable is used in case StreamNeeded() has been called on a non-owning
// thread.
class StreamNeededRunnable final : public CancelableRunnable {
 public:
  explicit StreamNeededRunnable(IPCBlobInputStreamChild* aActor)
      : CancelableRunnable("dom::StreamNeededRunnable"), mActor(aActor) {}

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(mActor->State() != IPCBlobInputStreamChild::eActiveMigrating &&
               mActor->State() != IPCBlobInputStreamChild::eInactiveMigrating);
    if (mActor->State() == IPCBlobInputStreamChild::eActive) {
      mActor->SendStreamNeeded();
    }
    return NS_OK;
  }

 private:
  RefPtr<IPCBlobInputStreamChild> mActor;
};

// When the stream has been received from the parent, we inform the
// IPCBlobInputStream.
class StreamReadyRunnable final : public CancelableRunnable {
 public:
  StreamReadyRunnable(IPCBlobInputStream* aDestinationStream,
                      already_AddRefed<nsIInputStream> aCreatedStream)
      : CancelableRunnable("dom::StreamReadyRunnable"),
        mDestinationStream(aDestinationStream),
        mCreatedStream(std::move(aCreatedStream)) {
    MOZ_ASSERT(mDestinationStream);
    // mCreatedStream can be null.
  }

  NS_IMETHOD
  Run() override {
    mDestinationStream->StreamReady(mCreatedStream.forget());
    return NS_OK;
  }

 private:
  RefPtr<IPCBlobInputStream> mDestinationStream;
  nsCOMPtr<nsIInputStream> mCreatedStream;
};

// This runnable is used in case LengthNeeded() has been called on a non-owning
// thread.
class LengthNeededRunnable final : public CancelableRunnable {
 public:
  explicit LengthNeededRunnable(IPCBlobInputStreamChild* aActor)
      : CancelableRunnable("dom::LengthNeededRunnable"), mActor(aActor) {}

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(mActor->State() != IPCBlobInputStreamChild::eActiveMigrating &&
               mActor->State() != IPCBlobInputStreamChild::eInactiveMigrating);
    if (mActor->State() == IPCBlobInputStreamChild::eActive) {
      mActor->SendLengthNeeded();
    }
    return NS_OK;
  }

 private:
  RefPtr<IPCBlobInputStreamChild> mActor;
};

// When the stream has been received from the parent, we inform the
// IPCBlobInputStream.
class LengthReadyRunnable final : public CancelableRunnable {
 public:
  LengthReadyRunnable(IPCBlobInputStream* aDestinationStream, int64_t aSize)
      : CancelableRunnable("dom::LengthReadyRunnable"),
        mDestinationStream(aDestinationStream),
        mSize(aSize) {
    MOZ_ASSERT(mDestinationStream);
  }

  NS_IMETHOD
  Run() override {
    mDestinationStream->LengthReady(mSize);
    return NS_OK;
  }

 private:
  RefPtr<IPCBlobInputStream> mDestinationStream;
  int64_t mSize;
};

}  // namespace

IPCBlobInputStreamChild::IPCBlobInputStreamChild(const nsID& aID,
                                                 uint64_t aSize)
    : mMutex("IPCBlobInputStreamChild::mMutex"),
      mID(aID),
      mSize(aSize),
      mState(eActive),
      mOwningEventTarget(GetCurrentThreadSerialEventTarget()) {
  // If we are running in a worker, we need to send a Close() to the parent side
  // before the thread is released.
  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    if (!workerPrivate) {
      return;
    }

    RefPtr<StrongWorkerRef> workerRef =
        StrongWorkerRef::Create(workerPrivate, "IPCBlobInputStreamChild");
    if (!workerRef) {
      return;
    }

    // We must keep the worker alive until the migration is completed.
    mWorkerRef = new ThreadSafeWorkerRef(workerRef);
  }
}

IPCBlobInputStreamChild::~IPCBlobInputStreamChild() {}

void IPCBlobInputStreamChild::Shutdown() {
  MutexAutoLock lock(mMutex);

  RefPtr<IPCBlobInputStreamChild> kungFuDeathGrip = this;

  mWorkerRef = nullptr;
  mPendingOperations.Clear();

  if (mState == eActive) {
    SendClose();
    mState = eInactive;
  }
}

void IPCBlobInputStreamChild::ActorDestroy(
    IProtocol::ActorDestroyReason aReason) {
  bool migrating = false;

  {
    MutexAutoLock lock(mMutex);
    migrating = mState == eActiveMigrating;
    mState = migrating ? eInactiveMigrating : eInactive;
  }

  if (!migrating) {
    // Let's cleanup the workerRef and the pending operation queue.
    Shutdown();
    return;
  }
}

IPCBlobInputStreamChild::ActorState IPCBlobInputStreamChild::State() {
  MutexAutoLock lock(mMutex);
  return mState;
}

already_AddRefed<IPCBlobInputStream> IPCBlobInputStreamChild::CreateStream() {
  bool shouldMigrate = false;

  RefPtr<IPCBlobInputStream> stream;

  {
    MutexAutoLock lock(mMutex);

    if (mState == eInactive) {
      return nullptr;
    }

    // The stream is active but maybe it is not running in the DOM-File thread.
    // We should migrate it there.
    if (mState == eActive &&
        !IPCBlobInputStreamThread::IsOnFileEventTarget(mOwningEventTarget)) {
      MOZ_ASSERT(mStreams.IsEmpty());

      shouldMigrate = true;
      mState = eActiveMigrating;

      RefPtr<IPCBlobInputStreamThread> thread =
          IPCBlobInputStreamThread::GetOrCreate();
      MOZ_ASSERT(thread, "We cannot continue without DOMFile thread.");

      // Create a new actor object to connect to the target thread.
      RefPtr<IPCBlobInputStreamChild> newActor =
          new IPCBlobInputStreamChild(mID, mSize);
      {
        MutexAutoLock newActorLock(newActor->mMutex);

        // Move over our local state onto the new actor object.
        newActor->mWorkerRef = mWorkerRef;
        newActor->mState = eInactiveMigrating;
        newActor->mPendingOperations.SwapElements(mPendingOperations);

        // Create the actual stream object.
        stream = new IPCBlobInputStream(newActor);
        newActor->mStreams.AppendElement(stream);
      }

      // Perform the actual migration.
      thread->MigrateActor(newActor);
    } else {
      stream = new IPCBlobInputStream(this);
      mStreams.AppendElement(stream);
    }
  }

  // Send__delete__ will call ActorDestroy(). mMutex cannot be locked at this
  // time.
  if (shouldMigrate) {
    Send__delete__(this);
  }

  return stream.forget();
}

void IPCBlobInputStreamChild::ForgetStream(IPCBlobInputStream* aStream) {
  MOZ_ASSERT(aStream);

  RefPtr<IPCBlobInputStreamChild> kungFuDeathGrip = this;

  {
    MutexAutoLock lock(mMutex);
    mStreams.RemoveElement(aStream);

    if (!mStreams.IsEmpty() || mState != eActive) {
      return;
    }
  }

  if (mOwningEventTarget->IsOnCurrentThread()) {
    Shutdown();
    return;
  }

  RefPtr<ShutdownRunnable> runnable = new ShutdownRunnable(this);
  mOwningEventTarget->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

void IPCBlobInputStreamChild::StreamNeeded(IPCBlobInputStream* aStream,
                                           nsIEventTarget* aEventTarget) {
  MutexAutoLock lock(mMutex);

  if (mState == eInactive) {
    return;
  }

  MOZ_ASSERT(mStreams.Contains(aStream));

  PendingOperation* opt = mPendingOperations.AppendElement();
  opt->mStream = aStream;
  opt->mEventTarget = aEventTarget;
  opt->mOp = PendingOperation::eStreamNeeded;

  if (mState == eActiveMigrating || mState == eInactiveMigrating) {
    // This operation will be continued when the migration is completed.
    return;
  }

  MOZ_ASSERT(mState == eActive);

  if (mOwningEventTarget->IsOnCurrentThread()) {
    SendStreamNeeded();
    return;
  }

  RefPtr<StreamNeededRunnable> runnable = new StreamNeededRunnable(this);
  mOwningEventTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
}

mozilla::ipc::IPCResult IPCBlobInputStreamChild::RecvStreamReady(
    const Maybe<IPCStream>& aStream) {
  nsCOMPtr<nsIInputStream> stream = mozilla::ipc::DeserializeIPCStream(aStream);

  RefPtr<IPCBlobInputStream> pendingStream;
  nsCOMPtr<nsIEventTarget> eventTarget;

  {
    MutexAutoLock lock(mMutex);

    // We have been shutdown in the meantime.
    if (mState == eInactive) {
      return IPC_OK();
    }

    MOZ_ASSERT(!mPendingOperations.IsEmpty());
    MOZ_ASSERT(mState == eActive);

    pendingStream = mPendingOperations[0].mStream;
    eventTarget = mPendingOperations[0].mEventTarget;
    MOZ_ASSERT(mPendingOperations[0].mOp == PendingOperation::eStreamNeeded);

    mPendingOperations.RemoveElementAt(0);
  }

  RefPtr<StreamReadyRunnable> runnable =
      new StreamReadyRunnable(pendingStream, stream.forget());

  // If IPCBlobInputStream::AsyncWait() has been executed without passing an
  // event target, we run the callback synchronous because any thread could be
  // result to be the wrong one. See more in nsIAsyncInputStream::asyncWait
  // documentation.
  if (eventTarget) {
    eventTarget->Dispatch(runnable, NS_DISPATCH_NORMAL);
  } else {
    runnable->Run();
  }

  return IPC_OK();
}

void IPCBlobInputStreamChild::LengthNeeded(IPCBlobInputStream* aStream,
                                           nsIEventTarget* aEventTarget) {
  MutexAutoLock lock(mMutex);

  if (mState == eInactive) {
    return;
  }

  MOZ_ASSERT(mStreams.Contains(aStream));

  PendingOperation* opt = mPendingOperations.AppendElement();
  opt->mStream = aStream;
  opt->mEventTarget = aEventTarget;
  opt->mOp = PendingOperation::eLengthNeeded;

  if (mState == eActiveMigrating || mState == eInactiveMigrating) {
    // This operation will be continued when the migration is completed.
    return;
  }

  MOZ_ASSERT(mState == eActive);

  if (mOwningEventTarget->IsOnCurrentThread()) {
    SendLengthNeeded();
    return;
  }

  RefPtr<LengthNeededRunnable> runnable = new LengthNeededRunnable(this);
  mOwningEventTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
}

mozilla::ipc::IPCResult IPCBlobInputStreamChild::RecvLengthReady(
    const int64_t& aLength) {
  RefPtr<IPCBlobInputStream> pendingStream;
  nsCOMPtr<nsIEventTarget> eventTarget;

  {
    MutexAutoLock lock(mMutex);

    // We have been shutdown in the meantime.
    if (mState == eInactive) {
      return IPC_OK();
    }

    MOZ_ASSERT(!mPendingOperations.IsEmpty());
    MOZ_ASSERT(mState == eActive);

    pendingStream = mPendingOperations[0].mStream;
    eventTarget = mPendingOperations[0].mEventTarget;
    MOZ_ASSERT(mPendingOperations[0].mOp == PendingOperation::eLengthNeeded);

    mPendingOperations.RemoveElementAt(0);
  }

  RefPtr<LengthReadyRunnable> runnable =
      new LengthReadyRunnable(pendingStream, aLength);

  MOZ_ASSERT(eventTarget);
  eventTarget->Dispatch(runnable, NS_DISPATCH_NORMAL);

  return IPC_OK();
}
void IPCBlobInputStreamChild::Migrated() {
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mState == eInactiveMigrating);

  mWorkerRef = nullptr;

  mOwningEventTarget = GetCurrentThreadSerialEventTarget();
  MOZ_ASSERT(IPCBlobInputStreamThread::IsOnFileEventTarget(mOwningEventTarget));

  // Maybe we have no reasons to keep this actor alive.
  if (mStreams.IsEmpty()) {
    mState = eInactive;
    SendClose();
    return;
  }

  mState = eActive;

  // Let's processing the pending operations. We need a stream for each pending
  // operation.
  for (uint32_t i = 0; i < mPendingOperations.Length(); ++i) {
    if (mPendingOperations[i].mOp == PendingOperation::eStreamNeeded) {
      SendStreamNeeded();
    } else {
      MOZ_ASSERT(mPendingOperations[i].mOp == PendingOperation::eLengthNeeded);
      SendLengthNeeded();
    }
  }
}

}  // namespace dom
}  // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteLazyInputStreamChild.h"
#include "RemoteLazyInputStreamThread.h"

#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerRef.h"

namespace mozilla {

using namespace dom;

namespace {

// This runnable is used in case the last stream is forgotten on the 'wrong'
// thread.
class ShutdownRunnable final : public DiscardableRunnable {
 public:
  explicit ShutdownRunnable(RemoteLazyInputStreamChild* aActor)
      : DiscardableRunnable("dom::ShutdownRunnable"), mActor(aActor) {}

  NS_IMETHOD
  Run() override {
    mActor->Shutdown();
    return NS_OK;
  }

 private:
  RefPtr<RemoteLazyInputStreamChild> mActor;
};

// This runnable is used in case StreamNeeded() has been called on a non-owning
// thread.
class StreamNeededRunnable final : public DiscardableRunnable {
 public:
  explicit StreamNeededRunnable(RemoteLazyInputStreamChild* aActor)
      : DiscardableRunnable("dom::StreamNeededRunnable"), mActor(aActor) {}

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(
        mActor->State() != RemoteLazyInputStreamChild::eActiveMigrating &&
        mActor->State() != RemoteLazyInputStreamChild::eInactiveMigrating);
    if (mActor->State() == RemoteLazyInputStreamChild::eActive) {
      mActor->SendStreamNeeded();
    }
    return NS_OK;
  }

 private:
  RefPtr<RemoteLazyInputStreamChild> mActor;
};

// When the stream has been received from the parent, we inform the
// RemoteLazyInputStream.
class StreamReadyRunnable final : public DiscardableRunnable {
 public:
  StreamReadyRunnable(RemoteLazyInputStream* aDestinationStream,
                      already_AddRefed<nsIInputStream> aCreatedStream)
      : DiscardableRunnable("dom::StreamReadyRunnable"),
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
  RefPtr<RemoteLazyInputStream> mDestinationStream;
  nsCOMPtr<nsIInputStream> mCreatedStream;
};

// This runnable is used in case LengthNeeded() has been called on a non-owning
// thread.
class LengthNeededRunnable final : public DiscardableRunnable {
 public:
  explicit LengthNeededRunnable(RemoteLazyInputStreamChild* aActor)
      : DiscardableRunnable("dom::LengthNeededRunnable"), mActor(aActor) {}

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(
        mActor->State() != RemoteLazyInputStreamChild::eActiveMigrating &&
        mActor->State() != RemoteLazyInputStreamChild::eInactiveMigrating);
    if (mActor->State() == RemoteLazyInputStreamChild::eActive) {
      mActor->SendLengthNeeded();
    }
    return NS_OK;
  }

 private:
  RefPtr<RemoteLazyInputStreamChild> mActor;
};

// When the stream has been received from the parent, we inform the
// RemoteLazyInputStream.
class LengthReadyRunnable final : public DiscardableRunnable {
 public:
  LengthReadyRunnable(RemoteLazyInputStream* aDestinationStream, int64_t aSize)
      : DiscardableRunnable("dom::LengthReadyRunnable"),
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
  RefPtr<RemoteLazyInputStream> mDestinationStream;
  int64_t mSize;
};

}  // namespace

RemoteLazyInputStreamChild::RemoteLazyInputStreamChild(const nsID& aID,
                                                       uint64_t aSize)
    : mMutex("RemoteLazyInputStreamChild::mMutex"),
      mID(aID),
      mSize(aSize),
      mState(eActive),
      mOwningEventTarget(GetCurrentSerialEventTarget()) {
  // If we are running in a worker, we need to send a Close() to the parent side
  // before the thread is released.
  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    if (!workerPrivate) {
      return;
    }

    RefPtr<StrongWorkerRef> workerRef =
        StrongWorkerRef::Create(workerPrivate, "RemoteLazyInputStreamChild");
    if (!workerRef) {
      return;
    }

    // We must keep the worker alive until the migration is completed.
    mWorkerRef = new ThreadSafeWorkerRef(workerRef);
  }
}

RemoteLazyInputStreamChild::~RemoteLazyInputStreamChild() = default;

void RemoteLazyInputStreamChild::Shutdown() {
  MutexAutoLock lock(mMutex);

  RefPtr<RemoteLazyInputStreamChild> kungFuDeathGrip = this;

  mWorkerRef = nullptr;
  mPendingOperations.Clear();

  if (mState == eActive) {
    SendClose();
    mState = eInactive;
  }
}

void RemoteLazyInputStreamChild::ActorDestroy(
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

RemoteLazyInputStreamChild::ActorState RemoteLazyInputStreamChild::State() {
  MutexAutoLock lock(mMutex);
  return mState;
}

already_AddRefed<RemoteLazyInputStream>
RemoteLazyInputStreamChild::CreateStream() {
  bool shouldMigrate = false;

  RefPtr<RemoteLazyInputStream> stream;

  {
    MutexAutoLock lock(mMutex);

    if (mState == eInactive) {
      return nullptr;
    }

    // The stream is active but maybe it is not running in the DOM-File thread.
    // We should migrate it there.
    if (mState == eActive &&
        !RemoteLazyInputStreamThread::IsOnFileEventTarget(mOwningEventTarget)) {
      MOZ_ASSERT(mStreams.IsEmpty());

      shouldMigrate = true;
      mState = eActiveMigrating;

      RefPtr<RemoteLazyInputStreamThread> thread =
          RemoteLazyInputStreamThread::GetOrCreate();
      MOZ_ASSERT(thread, "We cannot continue without DOMFile thread.");

      // Create a new actor object to connect to the target thread.
      RefPtr<RemoteLazyInputStreamChild> newActor =
          new RemoteLazyInputStreamChild(mID, mSize);
      {
        MutexAutoLock newActorLock(newActor->mMutex);

        // Move over our local state onto the new actor object.
        newActor->mWorkerRef = mWorkerRef;
        newActor->mState = eInactiveMigrating;
        newActor->mPendingOperations = std::move(mPendingOperations);

        // Create the actual stream object.
        stream = new RemoteLazyInputStream(newActor);
        newActor->mStreams.AppendElement(stream);
      }

      // Perform the actual migration.
      thread->MigrateActor(newActor);
    } else {
      stream = new RemoteLazyInputStream(this);
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

void RemoteLazyInputStreamChild::ForgetStream(RemoteLazyInputStream* aStream) {
  MOZ_ASSERT(aStream);

  RefPtr<RemoteLazyInputStreamChild> kungFuDeathGrip = this;

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

void RemoteLazyInputStreamChild::StreamNeeded(RemoteLazyInputStream* aStream,
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

mozilla::ipc::IPCResult RemoteLazyInputStreamChild::RecvStreamReady(
    const Maybe<IPCStream>& aStream) {
  nsCOMPtr<nsIInputStream> stream = mozilla::ipc::DeserializeIPCStream(aStream);

  RefPtr<RemoteLazyInputStream> pendingStream;
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

  // If RemoteLazyInputStream::AsyncWait() has been executed without passing an
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

void RemoteLazyInputStreamChild::LengthNeeded(RemoteLazyInputStream* aStream,
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

mozilla::ipc::IPCResult RemoteLazyInputStreamChild::RecvLengthReady(
    const int64_t& aLength) {
  RefPtr<RemoteLazyInputStream> pendingStream;
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
void RemoteLazyInputStreamChild::Migrated() {
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mState == eInactiveMigrating);

  mWorkerRef = nullptr;

  mOwningEventTarget = GetCurrentSerialEventTarget();
  MOZ_ASSERT(
      RemoteLazyInputStreamThread::IsOnFileEventTarget(mOwningEventTarget));

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

}  // namespace mozilla

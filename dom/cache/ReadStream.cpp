/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/ReadStream.h"

#include "mozilla/Unused.h"
#include "mozilla/dom/cache/CacheStreamControlChild.h"
#include "mozilla/dom/cache/CacheStreamControlParent.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/SnappyUncompressInputStream.h"
#include "nsIAsyncInputStream.h"
#include "nsIThread.h"
#include "nsStringStream.h"
#include "nsTArray.h"

namespace mozilla::dom::cache {

using mozilla::Unused;
using mozilla::ipc::AutoIPCStream;
using mozilla::ipc::IPCStream;

// ----------------------------------------------------------------------------

// The inner stream class.  This is where all of the real work is done.  As
// an invariant Inner::Close() must be called before ~Inner().  This is
// guaranteed by our outer ReadStream class.
class ReadStream::Inner final : public ReadStream::Controllable {
 public:
  Inner(StreamControl* aControl, const nsID& aId, nsIInputStream* aStream);

  void Serialize(Maybe<CacheReadStream>* aReadStreamOut,
                 nsTArray<UniquePtr<AutoIPCStream>>& aStreamCleanupList,
                 ErrorResult& aRv);

  void Serialize(CacheReadStream* aReadStreamOut,
                 nsTArray<UniquePtr<AutoIPCStream>>& aStreamCleanupList,
                 ErrorResult& aRv);

  // ReadStream::Controllable methods
  virtual void CloseStream() override;

  virtual void CloseStreamWithoutReporting() override;

  virtual bool HasEverBeenRead() const override;

  // Simulate nsIInputStream methods, but we don't actually inherit from it
  nsresult Close();

  nsresult Available(uint64_t* aNumAvailableOut);

  nsresult Read(char* aBuf, uint32_t aCount, uint32_t* aNumReadOut);

  nsresult ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                        uint32_t aCount, uint32_t* aNumReadOut);

  nsresult IsNonBlocking(bool* aNonBlockingOut);

  NS_DECL_OWNINGTHREAD;

  ~Inner();

 private:
  class NoteClosedRunnable;
  class ForgetRunnable;

  void NoteClosed();

  void Forget();

  void NoteClosedOnOwningThread();

  void ForgetOnOwningThread();

  nsIInputStream* EnsureStream();

  void AsyncOpenStreamOnOwningThread();

  void MaybeAbortAsyncOpenStream();

  void OpenStreamFailed();

  inline SafeRefPtr<Inner> SafeRefPtrFromThis() {
    return Controllable::SafeRefPtrFromThis().downcast<Inner>();
  }

  // Weak ref to the stream control actor.  The actor will always call either
  // CloseStream() or CloseStreamWithoutReporting() before it's destroyed.  The
  // weak ref is cleared in the resulting NoteClosedOnOwningThread() or
  // ForgetOnOwningThread() method call.
  StreamControl* mControl;

  const nsID mId;
  nsCOMPtr<nsISerialEventTarget> mOwningEventTarget;

  enum State { Open, Closed, NumStates };
  Atomic<State> mState;
  Atomic<bool> mHasEverBeenRead;
  bool mAsyncOpenStarted;

  // The wrapped stream objects may not be threadsafe.  We need to be able
  // to close a stream on our owning thread while an IO thread is simultaneously
  // reading the same stream.  Therefore, protect all access to these stream
  // objects with a mutex.
  Mutex mMutex;
  CondVar mCondVar;
  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsIInputStream> mSnappyStream;
};

// ----------------------------------------------------------------------------

// Runnable to notify actors that the ReadStream has closed.  This must
// be done on the thread associated with the PBackground actor.  Must be
// cancelable to execute on Worker threads (which can occur when the
// ReadStream is constructed on a child process Worker thread).
class ReadStream::Inner::NoteClosedRunnable final : public CancelableRunnable {
 public:
  explicit NoteClosedRunnable(SafeRefPtr<ReadStream::Inner> aStream)
      : CancelableRunnable("dom::cache::ReadStream::Inner::NoteClosedRunnable"),
        mStream(std::move(aStream)) {}

  NS_IMETHOD Run() override {
    mStream->NoteClosedOnOwningThread();
    return NS_OK;
  }

  // Note, we must proceed with the Run() method since our actor will not
  // clean itself up until we note that the stream is closed.
  nsresult Cancel() override {
    Run();
    return NS_OK;
  }

 private:
  ~NoteClosedRunnable() = default;

  const SafeRefPtr<ReadStream::Inner> mStream;
};

// ----------------------------------------------------------------------------

// Runnable to clear actors without reporting that the ReadStream has
// closed.  Since this can trigger actor destruction, we need to do
// it on the thread associated with the PBackground actor.  Must be
// cancelable to execute on Worker threads (which can occur when the
// ReadStream is constructed on a child process Worker thread).
class ReadStream::Inner::ForgetRunnable final : public CancelableRunnable {
 public:
  explicit ForgetRunnable(SafeRefPtr<ReadStream::Inner> aStream)
      : CancelableRunnable("dom::cache::ReadStream::Inner::ForgetRunnable"),
        mStream(std::move(aStream)) {}

  NS_IMETHOD Run() override {
    mStream->ForgetOnOwningThread();
    return NS_OK;
  }

  // Note, we must proceed with the Run() method so that we properly
  // call RemoveListener on the actor.
  nsresult Cancel() override {
    Run();
    return NS_OK;
  }

 private:
  ~ForgetRunnable() = default;

  const SafeRefPtr<ReadStream::Inner> mStream;
};

// ----------------------------------------------------------------------------

ReadStream::Inner::Inner(StreamControl* aControl, const nsID& aId,
                         nsIInputStream* aStream)
    : mControl(aControl),
      mId(aId),
      mOwningEventTarget(GetCurrentSerialEventTarget()),
      mState(Open),
      mHasEverBeenRead(false),
      mAsyncOpenStarted(false),
      mMutex("dom::cache::ReadStream"),
      mCondVar(mMutex, "dom::cache::ReadStream"),
      mStream(aStream),
      mSnappyStream(aStream ? new SnappyUncompressInputStream(aStream)
                            : nullptr) {
  MOZ_DIAGNOSTIC_ASSERT(mControl);
  mControl->AddReadStream(SafeRefPtrFromThis());
}

void ReadStream::Inner::Serialize(
    Maybe<CacheReadStream>* aReadStreamOut,
    nsTArray<UniquePtr<AutoIPCStream>>& aStreamCleanupList, ErrorResult& aRv) {
  MOZ_ASSERT(mOwningEventTarget->IsOnCurrentThread());
  MOZ_DIAGNOSTIC_ASSERT(aReadStreamOut);
  aReadStreamOut->emplace(CacheReadStream());
  Serialize(&aReadStreamOut->ref(), aStreamCleanupList, aRv);
}

void ReadStream::Inner::Serialize(
    CacheReadStream* aReadStreamOut,
    nsTArray<UniquePtr<AutoIPCStream>>& aStreamCleanupList, ErrorResult& aRv) {
  MOZ_ASSERT(mOwningEventTarget->IsOnCurrentThread());
  MOZ_DIAGNOSTIC_ASSERT(aReadStreamOut);

  if (mState != Open) {
    aRv.ThrowTypeError(
        "Response body is a cache file stream that has already been closed.");
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(mControl);

  aReadStreamOut->id() = mId;
  mControl->SerializeControl(aReadStreamOut);

  {
    MutexAutoLock lock(mMutex);
    mControl->SerializeStream(aReadStreamOut, mStream, aStreamCleanupList);
  }

  MOZ_DIAGNOSTIC_ASSERT(
      aReadStreamOut->stream().isNothing() ||
      (aReadStreamOut->stream().ref().stream().type() !=
           mozilla::ipc::InputStreamParams::TIPCRemoteStreamParams &&
       aReadStreamOut->stream().ref().stream().type() !=
           mozilla::ipc::InputStreamParams::T__None));

  // We're passing ownership across the IPC barrier with the control, so
  // do not signal that the stream is closed here.
  Forget();
}

void ReadStream::Inner::CloseStream() {
  MOZ_ASSERT(mOwningEventTarget->IsOnCurrentThread());
  MOZ_ALWAYS_SUCCEEDS(Close());
}

void ReadStream::Inner::CloseStreamWithoutReporting() {
  MOZ_ASSERT(mOwningEventTarget->IsOnCurrentThread());
  Forget();
}

bool ReadStream::Inner::HasEverBeenRead() const {
  MOZ_ASSERT(mOwningEventTarget->IsOnCurrentThread());
  return mHasEverBeenRead;
}

nsresult ReadStream::Inner::Close() {
  // stream ops can happen on any thread
  nsresult rv = NS_OK;
  {
    MutexAutoLock lock(mMutex);
    if (mSnappyStream) {
      rv = mSnappyStream->Close();
    }
  }
  NoteClosed();
  return rv;
}

nsresult ReadStream::Inner::Available(uint64_t* aNumAvailableOut) {
  // stream ops can happen on any thread
  nsresult rv = NS_OK;
  {
    MutexAutoLock lock(mMutex);
    rv = EnsureStream()->Available(aNumAvailableOut);
  }

  if (NS_FAILED(rv)) {
    Close();
  }

  return rv;
}

nsresult ReadStream::Inner::Read(char* aBuf, uint32_t aCount,
                                 uint32_t* aNumReadOut) {
  // stream ops can happen on any thread
  MOZ_DIAGNOSTIC_ASSERT(aNumReadOut);

  nsresult rv = NS_OK;
  {
    MutexAutoLock lock(mMutex);
    rv = EnsureStream()->Read(aBuf, aCount, aNumReadOut);
  }

  if ((NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK) ||
      *aNumReadOut == 0) {
    Close();
  }

  mHasEverBeenRead = true;

  return rv;
}

nsresult ReadStream::Inner::ReadSegments(nsWriteSegmentFun aWriter,
                                         void* aClosure, uint32_t aCount,
                                         uint32_t* aNumReadOut) {
  // stream ops can happen on any thread
  MOZ_DIAGNOSTIC_ASSERT(aNumReadOut);

  if (aCount) {
    mHasEverBeenRead = true;
  }

  nsresult rv = NS_OK;
  {
    MutexAutoLock lock(mMutex);
    rv = EnsureStream()->ReadSegments(aWriter, aClosure, aCount, aNumReadOut);
  }

  if ((NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK &&
       rv != NS_ERROR_NOT_IMPLEMENTED) ||
      *aNumReadOut == 0) {
    Close();
  }

  // Verify bytes were actually read before marking as being ever read.  For
  // example, code can test if the stream supports ReadSegments() by calling
  // this method with a dummy callback which doesn't read anything.  We don't
  // want to trigger on that.
  if (*aNumReadOut) {
    mHasEverBeenRead = true;
  }

  return rv;
}

nsresult ReadStream::Inner::IsNonBlocking(bool* aNonBlockingOut) {
  // stream ops can happen on any thread
  MutexAutoLock lock(mMutex);
  if (mSnappyStream) {
    return mSnappyStream->IsNonBlocking(aNonBlockingOut);
  }
  *aNonBlockingOut = false;
  return NS_OK;
}

ReadStream::Inner::~Inner() {
  // Any thread
  MOZ_DIAGNOSTIC_ASSERT(mState == Closed);
  MOZ_DIAGNOSTIC_ASSERT(!mControl);
}

void ReadStream::Inner::NoteClosed() {
  // Any thread
  if (mState == Closed) {
    return;
  }

  if (mOwningEventTarget->IsOnCurrentThread()) {
    NoteClosedOnOwningThread();
    return;
  }

  nsCOMPtr<nsIRunnable> runnable = new NoteClosedRunnable(SafeRefPtrFromThis());
  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(runnable.forget(),
                                                   nsIThread::DISPATCH_NORMAL));
}

void ReadStream::Inner::Forget() {
  // Any thread
  if (mState == Closed) {
    return;
  }

  if (mOwningEventTarget->IsOnCurrentThread()) {
    ForgetOnOwningThread();
    return;
  }

  nsCOMPtr<nsIRunnable> runnable = new ForgetRunnable(SafeRefPtrFromThis());
  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(runnable.forget(),
                                                   nsIThread::DISPATCH_NORMAL));
}

void ReadStream::Inner::NoteClosedOnOwningThread() {
  MOZ_ASSERT(mOwningEventTarget->IsOnCurrentThread());

  // Mark closed and do nothing if we were already closed
  if (!mState.compareExchange(Open, Closed)) {
    return;
  }

  MaybeAbortAsyncOpenStream();

  MOZ_DIAGNOSTIC_ASSERT(mControl);
  mControl->NoteClosed(SafeRefPtrFromThis(), mId);
  mControl = nullptr;
}

void ReadStream::Inner::ForgetOnOwningThread() {
  MOZ_ASSERT(mOwningEventTarget->IsOnCurrentThread());

  // Mark closed and do nothing if we were already closed
  if (!mState.compareExchange(Open, Closed)) {
    return;
  }

  MaybeAbortAsyncOpenStream();

  MOZ_DIAGNOSTIC_ASSERT(mControl);
  mControl->ForgetReadStream(SafeRefPtrFromThis());
  mControl = nullptr;
}

nsIInputStream* ReadStream::Inner::EnsureStream() {
  mMutex.AssertCurrentThreadOwns();

  // We need to block the current thread while we open the stream.  We
  // cannot do this safely from the main owning thread since it would
  // trigger deadlock.  This should be ok, though, since a blocking
  // stream like this should never be read on the owning thread anyway.
  if (mOwningEventTarget->IsOnCurrentThread()) {
    MOZ_CRASH("Blocking read on the js/ipc owning thread!");
  }

  if (mSnappyStream) {
    return mSnappyStream;
  }

  nsCOMPtr<nsIRunnable> r = NewCancelableRunnableMethod(
      "ReadStream::Inner::AsyncOpenStreamOnOwningThread", this,
      &ReadStream::Inner::AsyncOpenStreamOnOwningThread);
  nsresult rv =
      mOwningEventTarget->Dispatch(r.forget(), nsIThread::DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    OpenStreamFailed();
    return mSnappyStream;
  }

  mCondVar.Wait();
  MOZ_DIAGNOSTIC_ASSERT(mSnappyStream);

  return mSnappyStream;
}

void ReadStream::Inner::AsyncOpenStreamOnOwningThread() {
  MOZ_ASSERT(mOwningEventTarget->IsOnCurrentThread());

  if (!mControl || mState == Closed) {
    MutexAutoLock lock(mMutex);
    OpenStreamFailed();
    mCondVar.NotifyAll();
    return;
  }

  if (mAsyncOpenStarted) {
    return;
  }
  mAsyncOpenStarted = true;

  RefPtr<ReadStream::Inner> self = this;
  mControl->OpenStream(mId, [self](nsCOMPtr<nsIInputStream>&& aStream) {
    MutexAutoLock lock(self->mMutex);
    self->mAsyncOpenStarted = false;
    if (!self->mStream) {
      if (!aStream) {
        self->OpenStreamFailed();
      } else {
        self->mStream = std::move(aStream);
        self->mSnappyStream = new SnappyUncompressInputStream(self->mStream);
      }
    }
    self->mCondVar.NotifyAll();
  });
}

void ReadStream::Inner::MaybeAbortAsyncOpenStream() {
  if (!mAsyncOpenStarted) {
    return;
  }

  MutexAutoLock lock(mMutex);
  OpenStreamFailed();
  mCondVar.NotifyAll();
}

void ReadStream::Inner::OpenStreamFailed() {
  MOZ_DIAGNOSTIC_ASSERT(!mStream);
  MOZ_DIAGNOSTIC_ASSERT(!mSnappyStream);
  mMutex.AssertCurrentThreadOwns();
  Unused << NS_NewCStringInputStream(getter_AddRefs(mStream), ""_ns);
  mSnappyStream = mStream;
  mStream->Close();
  NoteClosed();
}

// ----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(cache::ReadStream, nsIInputStream, ReadStream);

// static
already_AddRefed<ReadStream> ReadStream::Create(
    const Maybe<CacheReadStream>& aMaybeReadStream) {
  if (aMaybeReadStream.isNothing()) {
    return nullptr;
  }

  return Create(aMaybeReadStream.ref());
}

// static
already_AddRefed<ReadStream> ReadStream::Create(
    const CacheReadStream& aReadStream) {
  // The parameter may or may not be for a Cache created stream.  The way we
  // tell is by looking at the stream control actor.  If the actor exists,
  // then we know the Cache created it.
  if (!aReadStream.controlChild() && !aReadStream.controlParent()) {
    return nullptr;
  }

  MOZ_DIAGNOSTIC_ASSERT(
      aReadStream.stream().isNothing() ||
      (aReadStream.stream().ref().stream().type() !=
           mozilla::ipc::InputStreamParams::TIPCRemoteStreamParams &&
       aReadStream.stream().ref().stream().type() !=
           mozilla::ipc::InputStreamParams::T__None));

  // Control is guaranteed to survive this method as ActorDestroy() cannot
  // run on this thread until we complete.
  StreamControl* control;
  if (aReadStream.controlChild()) {
    auto actor =
        static_cast<CacheStreamControlChild*>(aReadStream.controlChild());
    control = actor;
  } else {
    auto actor =
        static_cast<CacheStreamControlParent*>(aReadStream.controlParent());
    control = actor;
  }
  MOZ_DIAGNOSTIC_ASSERT(control);

  nsCOMPtr<nsIInputStream> stream = DeserializeIPCStream(aReadStream.stream());

  // Currently we expect all cache read streams to be blocking file streams.
#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
  if (stream) {
    nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(stream);
    MOZ_DIAGNOSTIC_ASSERT(!asyncStream);
  }
#endif

  return MakeAndAddRef<ReadStream>(MakeSafeRefPtr<ReadStream::Inner>(
      std::move(control), aReadStream.id(), stream));
}

// static
already_AddRefed<ReadStream> ReadStream::Create(
    PCacheStreamControlParent* aControl, const nsID& aId,
    nsIInputStream* aStream) {
  MOZ_DIAGNOSTIC_ASSERT(aControl);

  return MakeAndAddRef<ReadStream>(MakeSafeRefPtr<ReadStream::Inner>(
      static_cast<CacheStreamControlParent*>(aControl), aId, aStream));
}

void ReadStream::Serialize(
    Maybe<CacheReadStream>* aReadStreamOut,
    nsTArray<UniquePtr<AutoIPCStream>>& aStreamCleanupList, ErrorResult& aRv) {
  mInner->Serialize(aReadStreamOut, aStreamCleanupList, aRv);
}

void ReadStream::Serialize(
    CacheReadStream* aReadStreamOut,
    nsTArray<UniquePtr<AutoIPCStream>>& aStreamCleanupList, ErrorResult& aRv) {
  mInner->Serialize(aReadStreamOut, aStreamCleanupList, aRv);
}

ReadStream::ReadStream(SafeRefPtr<ReadStream::Inner> aInner)
    : mInner(std::move(aInner)) {
  MOZ_DIAGNOSTIC_ASSERT(mInner);
}

ReadStream::~ReadStream() {
  // Explicitly close the inner stream so that it does not have to
  // deal with implicitly closing at destruction time.
  mInner->Close();
}

NS_IMETHODIMP
ReadStream::Close() { return mInner->Close(); }

NS_IMETHODIMP
ReadStream::Available(uint64_t* aNumAvailableOut) {
  return mInner->Available(aNumAvailableOut);
}

NS_IMETHODIMP
ReadStream::Read(char* aBuf, uint32_t aCount, uint32_t* aNumReadOut) {
  return mInner->Read(aBuf, aCount, aNumReadOut);
}

NS_IMETHODIMP
ReadStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                         uint32_t aCount, uint32_t* aNumReadOut) {
  return mInner->ReadSegments(aWriter, aClosure, aCount, aNumReadOut);
}

NS_IMETHODIMP
ReadStream::IsNonBlocking(bool* aNonBlockingOut) {
  return mInner->IsNonBlocking(aNonBlockingOut);
}

}  // namespace mozilla::dom::cache

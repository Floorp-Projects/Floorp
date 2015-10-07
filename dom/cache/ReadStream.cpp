/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/ReadStream.h"

#include "mozilla/unused.h"
#include "mozilla/dom/cache/CacheStreamControlChild.h"
#include "mozilla/dom/cache/CacheStreamControlParent.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/SnappyUncompressInputStream.h"
#include "nsIAsyncInputStream.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::unused;
using mozilla::ipc::FileDescriptor;

// ----------------------------------------------------------------------------

// The inner stream class.  This is where all of the real work is done.  As
// an invariant Inner::Close() must be called before ~Inner().  This is
// guaranteed by our outer ReadStream class.
class ReadStream::Inner final : public ReadStream::Controllable
{
public:
  Inner(StreamControl* aControl, const nsID& aId,
        nsIInputStream* aStream);

  void
  Serialize(CacheReadStreamOrVoid* aReadStreamOut);

  void
  Serialize(CacheReadStream* aReadStreamOut);

  // ReadStream::Controllable methods
  virtual void
  CloseStream() override;

  virtual void
  CloseStreamWithoutReporting() override;

  virtual bool
  MatchId(const nsID& aId) const override;

  virtual bool
  HasEverBeenRead() const override;

  // Simulate nsIInputStream methods, but we don't actually inherit from it
  NS_METHOD
  Close();

  NS_METHOD
  Available(uint64_t *aNumAvailableOut);

  NS_METHOD
  Read(char *aBuf, uint32_t aCount, uint32_t *aNumReadOut);

  NS_METHOD
  ReadSegments(nsWriteSegmentFun aWriter, void *aClosure, uint32_t aCount,
               uint32_t *aNumReadOut);

  NS_METHOD
  IsNonBlocking(bool *aNonBlockingOut);

private:
  class NoteClosedRunnable;
  class ForgetRunnable;

  ~Inner();

  void
  NoteClosed();

  void
  Forget();

  void
  NoteClosedOnOwningThread();

  void
  ForgetOnOwningThread();

  // Weak ref to the stream control actor.  The actor will always call either
  // CloseStream() or CloseStreamWithoutReporting() before it's destroyed.  The
  // weak ref is cleared in the resulting NoteClosedOnOwningThread() or
  // ForgetOnOwningThread() method call.
  StreamControl* mControl;

  const nsID mId;
  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsIInputStream> mSnappyStream;
  nsCOMPtr<nsIThread> mOwningThread;

  enum State
  {
    Open,
    Closed,
    NumStates
  };
  Atomic<State> mState;
  Atomic<bool> mHasEverBeenRead;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(cache::ReadStream::Inner, override)
};

// ----------------------------------------------------------------------------

// Runnable to notify actors that the ReadStream has closed.  This must
// be done on the thread associated with the PBackground actor.  Must be
// cancelable to execute on Worker threads (which can occur when the
// ReadStream is constructed on a child process Worker thread).
class ReadStream::Inner::NoteClosedRunnable final : public nsCancelableRunnable
{
public:
  explicit NoteClosedRunnable(ReadStream::Inner* aStream)
    : mStream(aStream)
  { }

  NS_IMETHOD Run()
  {
    mStream->NoteClosedOnOwningThread();
    mStream = nullptr;
    return NS_OK;
  }

  // Note, we must proceed with the Run() method since our actor will not
  // clean itself up until we note that the stream is closed.
  NS_IMETHOD Cancel()
  {
    Run();
    return NS_OK;
  }

private:
  ~NoteClosedRunnable() { }

  nsRefPtr<ReadStream::Inner> mStream;
};

// ----------------------------------------------------------------------------

// Runnable to clear actors without reporting that the ReadStream has
// closed.  Since this can trigger actor destruction, we need to do
// it on the thread associated with the PBackground actor.  Must be
// cancelable to execute on Worker threads (which can occur when the
// ReadStream is constructed on a child process Worker thread).
class ReadStream::Inner::ForgetRunnable final : public nsCancelableRunnable
{
public:
  explicit ForgetRunnable(ReadStream::Inner* aStream)
    : mStream(aStream)
  { }

  NS_IMETHOD Run()
  {
    mStream->ForgetOnOwningThread();
    mStream = nullptr;
    return NS_OK;
  }

  // Note, we must proceed with the Run() method so that we properly
  // call RemoveListener on the actor.
  NS_IMETHOD Cancel()
  {
    Run();
    return NS_OK;
  }

private:
  ~ForgetRunnable() { }

  nsRefPtr<ReadStream::Inner> mStream;
};

// ----------------------------------------------------------------------------

ReadStream::Inner::Inner(StreamControl* aControl, const nsID& aId,
                         nsIInputStream* aStream)
  : mControl(aControl)
  , mId(aId)
  , mStream(aStream)
  , mSnappyStream(new SnappyUncompressInputStream(aStream))
  , mOwningThread(NS_GetCurrentThread())
  , mState(Open)
{
  MOZ_ASSERT(mStream);
  MOZ_ASSERT(mControl);
  mControl->AddReadStream(this);
}

void
ReadStream::Inner::Serialize(CacheReadStreamOrVoid* aReadStreamOut)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mOwningThread);
  MOZ_ASSERT(aReadStreamOut);
  CacheReadStream stream;
  Serialize(&stream);
  *aReadStreamOut = stream;
}

void
ReadStream::Inner::Serialize(CacheReadStream* aReadStreamOut)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mOwningThread);
  MOZ_ASSERT(aReadStreamOut);
  MOZ_ASSERT(mState == Open);
  MOZ_ASSERT(mControl);

  // If we are sending a ReadStream, then we never want to set the
  // pushStream actors at the same time.
  aReadStreamOut->pushStreamChild() = nullptr;
  aReadStreamOut->pushStreamParent() = nullptr;

  aReadStreamOut->id() = mId;
  mControl->SerializeControl(aReadStreamOut);

  nsAutoTArray<FileDescriptor, 4> fds;
  SerializeInputStream(mStream, aReadStreamOut->params(), fds);

  mControl->SerializeFds(aReadStreamOut, fds);

  // We're passing ownership across the IPC barrier with the control, so
  // do not signal that the stream is closed here.
  Forget();
}

void
ReadStream::Inner::CloseStream()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mOwningThread);
  Close();
}

void
ReadStream::Inner::CloseStreamWithoutReporting()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mOwningThread);
  Forget();
}

bool
ReadStream::Inner::MatchId(const nsID& aId) const
{
  MOZ_ASSERT(NS_GetCurrentThread() == mOwningThread);
  return mId.Equals(aId);
}

bool
ReadStream::Inner::HasEverBeenRead() const
{
  MOZ_ASSERT(NS_GetCurrentThread() == mOwningThread);
  return mHasEverBeenRead;
}

NS_IMETHODIMP
ReadStream::Inner::Close()
{
  // stream ops can happen on any thread
  nsresult rv = mStream->Close();
  NoteClosed();
  return rv;
}

NS_IMETHODIMP
ReadStream::Inner::Available(uint64_t* aNumAvailableOut)
{
  // stream ops can happen on any thread
  nsresult rv = mSnappyStream->Available(aNumAvailableOut);

  if (NS_FAILED(rv)) {
    Close();
  }

  return rv;
}

NS_IMETHODIMP
ReadStream::Inner::Read(char* aBuf, uint32_t aCount, uint32_t* aNumReadOut)
{
  // stream ops can happen on any thread
  MOZ_ASSERT(aNumReadOut);

  nsresult rv = mSnappyStream->Read(aBuf, aCount, aNumReadOut);

  if ((NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK) ||
      *aNumReadOut == 0) {
    Close();
  }

  mHasEverBeenRead = true;

  return rv;
}

NS_IMETHODIMP
ReadStream::Inner::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                uint32_t aCount, uint32_t* aNumReadOut)
{
  // stream ops can happen on any thread
  MOZ_ASSERT(aNumReadOut);

  if (aCount) {
    mHasEverBeenRead = true;
  }

  nsresult rv = mSnappyStream->ReadSegments(aWriter, aClosure, aCount,
                                            aNumReadOut);

  if ((NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK &&
                        rv != NS_ERROR_NOT_IMPLEMENTED) || *aNumReadOut == 0) {
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

NS_IMETHODIMP
ReadStream::Inner::IsNonBlocking(bool* aNonBlockingOut)
{
  // stream ops can happen on any thread
  return mSnappyStream->IsNonBlocking(aNonBlockingOut);
}

ReadStream::Inner::~Inner()
{
  // Any thread
  MOZ_ASSERT(mState == Closed);
  MOZ_ASSERT(!mControl);
}

void
ReadStream::Inner::NoteClosed()
{
  // Any thread
  if (mState == Closed) {
    return;
  }

  if (NS_GetCurrentThread() == mOwningThread) {
    NoteClosedOnOwningThread();
    return;
  }

  nsCOMPtr<nsIRunnable> runnable = new NoteClosedRunnable(this);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    mOwningThread->Dispatch(runnable, nsIThread::DISPATCH_NORMAL)));
}

void
ReadStream::Inner::Forget()
{
  // Any thread
  if (mState == Closed) {
    return;
  }

  if (NS_GetCurrentThread() == mOwningThread) {
    ForgetOnOwningThread();
    return;
  }

  nsCOMPtr<nsIRunnable> runnable = new ForgetRunnable(this);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    mOwningThread->Dispatch(runnable, nsIThread::DISPATCH_NORMAL)));
}

void
ReadStream::Inner::NoteClosedOnOwningThread()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mOwningThread);

  // Mark closed and do nothing if we were already closed
  if (!mState.compareExchange(Open, Closed)) {
    return;
  }

  MOZ_ASSERT(mControl);
  mControl->NoteClosed(this, mId);
  mControl = nullptr;
}

void
ReadStream::Inner::ForgetOnOwningThread()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mOwningThread);

  // Mark closed and do nothing if we were already closed
  if (!mState.compareExchange(Open, Closed)) {
    return;
  }

  MOZ_ASSERT(mControl);
  mControl->ForgetReadStream(this);
  mControl = nullptr;
}

// ----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(cache::ReadStream, nsIInputStream, ReadStream);

// static
already_AddRefed<ReadStream>
ReadStream::Create(const CacheReadStreamOrVoid& aReadStreamOrVoid)
{
  if (aReadStreamOrVoid.type() == CacheReadStreamOrVoid::Tvoid_t) {
    return nullptr;
  }

  return Create(aReadStreamOrVoid.get_CacheReadStream());
}

// static
already_AddRefed<ReadStream>
ReadStream::Create(const CacheReadStream& aReadStream)
{
  // The parameter may or may not be for a Cache created stream.  The way we
  // tell is by looking at the stream control actor.  If the actor exists,
  // then we know the Cache created it.
  if (!aReadStream.controlChild() && !aReadStream.controlParent()) {
    return nullptr;
  }

  MOZ_ASSERT(!aReadStream.pushStreamChild());
  MOZ_ASSERT(!aReadStream.pushStreamParent());

  // Control is guaranteed to survive this method as ActorDestroy() cannot
  // run on this thread until we complete.
  StreamControl* control;
  if (aReadStream.controlChild()) {
    auto actor = static_cast<CacheStreamControlChild*>(aReadStream.controlChild());
    control = actor;
  } else {
    auto actor = static_cast<CacheStreamControlParent*>(aReadStream.controlParent());
    control = actor;
  }
  MOZ_ASSERT(control);

  nsAutoTArray<FileDescriptor, 4> fds;
  control->DeserializeFds(aReadStream, fds);

  nsCOMPtr<nsIInputStream> stream =
    DeserializeInputStream(aReadStream.params(), fds);
  MOZ_ASSERT(stream);

  // Currently we expect all cache read streams to be blocking file streams.
#ifdef DEBUG
  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(stream);
  MOZ_ASSERT(!asyncStream);
#endif

  nsRefPtr<Inner> inner = new Inner(control, aReadStream.id(), stream);
  nsRefPtr<ReadStream> ref = new ReadStream(inner);
  return ref.forget();
}

// static
already_AddRefed<ReadStream>
ReadStream::Create(PCacheStreamControlParent* aControl, const nsID& aId,
                   nsIInputStream* aStream)
{
  MOZ_ASSERT(aControl);
  auto actor = static_cast<CacheStreamControlParent*>(aControl);
  nsRefPtr<Inner> inner = new Inner(actor, aId, aStream);
  nsRefPtr<ReadStream> ref = new ReadStream(inner);
  return ref.forget();
}

void
ReadStream::Serialize(CacheReadStreamOrVoid* aReadStreamOut)
{
  mInner->Serialize(aReadStreamOut);
}

void
ReadStream::Serialize(CacheReadStream* aReadStreamOut)
{
  mInner->Serialize(aReadStreamOut);
}

ReadStream::ReadStream(ReadStream::Inner* aInner)
  : mInner(aInner)
{
  MOZ_ASSERT(mInner);
}

ReadStream::~ReadStream()
{
  // Explicitly close the inner stream so that it does not have to
  // deal with implicitly closing at destruction time.
  mInner->Close();
}

NS_IMETHODIMP
ReadStream::Close()
{
  return mInner->Close();
}

NS_IMETHODIMP
ReadStream::Available(uint64_t* aNumAvailableOut)
{
  return mInner->Available(aNumAvailableOut);
}

NS_IMETHODIMP
ReadStream::Read(char* aBuf, uint32_t aCount, uint32_t* aNumReadOut)
{
  return mInner->Read(aBuf, aCount, aNumReadOut);
}

NS_IMETHODIMP
ReadStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                         uint32_t aCount, uint32_t* aNumReadOut)
{
  return mInner->ReadSegments(aWriter, aClosure, aCount, aNumReadOut);
}

NS_IMETHODIMP
ReadStream::IsNonBlocking(bool* aNonBlockingOut)
{
  return mInner->IsNonBlocking(aNonBlockingOut);
}

} // namespace cache
} // namespace dom
} // namespace mozilla

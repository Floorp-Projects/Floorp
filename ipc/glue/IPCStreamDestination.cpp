/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCStreamDestination.h"
#include "mozilla/Mutex.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIBufferedStreams.h"
#include "nsICloneableInputStream.h"
#include "nsIPipe.h"

namespace mozilla {
namespace ipc {

// ----------------------------------------------------------------------------
// IPCStreamDestination::DelayedStartInputStream
//
// When AutoIPCStream is used with delayedStart, we need to ask for data at the
// first real use of the nsIInputStream. In order to do so, we wrap the
// nsIInputStream, created by the nsIPipe, with this wrapper.

class IPCStreamDestination::DelayedStartInputStream final
  : public nsIAsyncInputStream
  , public nsISearchableInputStream
  , public nsICloneableInputStream
  , public nsIBufferedInputStream
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  DelayedStartInputStream(IPCStreamDestination* aDestination,
                          already_AddRefed<nsIAsyncInputStream>&& aStream)
    : mDestination(aDestination)
    , mStream(aStream)
    , mMutex("IPCStreamDestination::DelayedStartInputStream::mMutex")
  {
    MOZ_ASSERT(mDestination);
    MOZ_ASSERT(mStream);
  }

  void
  DestinationShutdown()
  {
    MutexAutoLock lock(mMutex);
    mDestination = nullptr;
  }

  // nsIInputStream interface

  NS_IMETHOD
  Close() override
  {
    MaybeCloseDestination();
    return mStream->Close();
  }

  NS_IMETHOD
  Available(uint64_t* aLength) override
  {
    MaybeStartReading();
    return mStream->Available(aLength);
  }

  NS_IMETHOD
  Read(char* aBuffer, uint32_t aCount, uint32_t* aReadCount) override
  {
    MaybeStartReading();
    return mStream->Read(aBuffer, aCount, aReadCount);
  }

  NS_IMETHOD
  ReadSegments(nsWriteSegmentFun aWriter, void* aClosure, uint32_t aCount,
               uint32_t *aResult) override
  {
    MaybeStartReading();
    return mStream->ReadSegments(aWriter, aClosure, aCount, aResult);
  }

  NS_IMETHOD
  IsNonBlocking(bool* aNonBlocking) override
  {
    MaybeStartReading();
    return mStream->IsNonBlocking(aNonBlocking);
  }

  // nsIAsyncInputStream interface

  NS_IMETHOD
  CloseWithStatus(nsresult aReason) override
  {
    MaybeCloseDestination();
    return mStream->CloseWithStatus(aReason);
  }

  NS_IMETHOD
  AsyncWait(nsIInputStreamCallback* aCallback, uint32_t aFlags,
            uint32_t aRequestedCount, nsIEventTarget* aTarget) override
  {
    MaybeStartReading();
    return mStream->AsyncWait(aCallback, aFlags, aRequestedCount, aTarget);
  }

  NS_IMETHOD
  Search(const char* aForString, bool aIgnoreCase, bool* aFound,
         uint32_t* aOffsetSearchedTo) override
  {
    MaybeStartReading();
    nsCOMPtr<nsISearchableInputStream> searchable = do_QueryInterface(mStream);
    MOZ_ASSERT(searchable);
    return searchable->Search(aForString, aIgnoreCase, aFound, aOffsetSearchedTo);
  }

  // nsICloneableInputStream interface

  NS_IMETHOD
  GetCloneable(bool* aCloneable) override
  {
    MaybeStartReading();
    nsCOMPtr<nsICloneableInputStream> cloneable = do_QueryInterface(mStream);
    MOZ_ASSERT(cloneable);
    return cloneable->GetCloneable(aCloneable);
  }

  NS_IMETHOD
  Clone(nsIInputStream** aResult) override
  {
    MaybeStartReading();
    nsCOMPtr<nsICloneableInputStream> cloneable = do_QueryInterface(mStream);
    MOZ_ASSERT(cloneable);
    return cloneable->Clone(aResult);
  }

  // nsIBufferedInputStream

  NS_IMETHOD
  Init(nsIInputStream* aStream, uint32_t aBufferSize) override
  {
    MaybeStartReading();
    nsCOMPtr<nsIBufferedInputStream> stream = do_QueryInterface(mStream);
    MOZ_ASSERT(stream);
    return stream->Init(aStream, aBufferSize);
  }

  void
  MaybeStartReading();

  void
  MaybeCloseDestination();

private:
  ~DelayedStartInputStream() = default;

  IPCStreamDestination* mDestination;
  nsCOMPtr<nsIAsyncInputStream> mStream;

  // This protects mDestination: any method can be called by any thread.
  Mutex mMutex;

  class HelperRunnable;
};

class IPCStreamDestination::DelayedStartInputStream::HelperRunnable final
  : public Runnable
{
public:
  enum Op {
    eStartReading,
    eCloseDestination,
  };

  HelperRunnable(IPCStreamDestination::DelayedStartInputStream* aDelayedStartInputStream,
                 Op aOp)
    : mDelayedStartInputStream(aDelayedStartInputStream)
    , mOp(aOp)
  {
    MOZ_ASSERT(aDelayedStartInputStream);
  }

  NS_IMETHOD
  Run() override
  {
    switch (mOp) {
    case eStartReading:
      mDelayedStartInputStream->MaybeStartReading();
      break;
    case eCloseDestination:
      mDelayedStartInputStream->MaybeCloseDestination();
      break;
    }

    return NS_OK;
  }

private:
  RefPtr<IPCStreamDestination::DelayedStartInputStream> mDelayedStartInputStream;
  Op mOp;
};

void
IPCStreamDestination::DelayedStartInputStream::MaybeStartReading()
{
  MutexAutoLock lock(mMutex);
  if (!mDestination) {
    return;
  }

  if (mDestination->IsOnOwningThread()) {
    mDestination->StartReading();
    mDestination = nullptr;
    return;
  }

  RefPtr<Runnable> runnable =
    new HelperRunnable(this, HelperRunnable::eStartReading);
  mDestination->DispatchRunnable(runnable.forget());
}

void
IPCStreamDestination::DelayedStartInputStream::MaybeCloseDestination()
{
  MutexAutoLock lock(mMutex);
  if (!mDestination) {
    return;
  }

  if (mDestination->IsOnOwningThread()) {
    mDestination->RequestClose(NS_ERROR_ABORT);
    mDestination = nullptr;
    return;
  }

  RefPtr<Runnable> runnable =
    new HelperRunnable(this, HelperRunnable::eCloseDestination);
  mDestination->DispatchRunnable(runnable.forget());
}

NS_IMPL_ADDREF(IPCStreamDestination::DelayedStartInputStream);
NS_IMPL_RELEASE(IPCStreamDestination::DelayedStartInputStream);

NS_INTERFACE_MAP_BEGIN(IPCStreamDestination::DelayedStartInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY(nsISearchableInputStream)
  NS_INTERFACE_MAP_ENTRY(nsICloneableInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIBufferedInputStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIInputStream, nsIAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAsyncInputStream)
NS_INTERFACE_MAP_END

// ----------------------------------------------------------------------------
// IPCStreamDestination

IPCStreamDestination::IPCStreamDestination()
  : mOwningThread(NS_GetCurrentThread())
  , mDelayedStart(false)
{
}

IPCStreamDestination::~IPCStreamDestination()
{
}

nsresult
IPCStreamDestination::Initialize()
{
  MOZ_ASSERT(!mReader);
  MOZ_ASSERT(!mWriter);

  // use async versions for both reader and writer even though we are
  // opening the writer as an infinite stream.  We want to be able to
  // use CloseWithStatus() to communicate errors through the pipe.

  // Use an "infinite" pipe because we cannot apply back-pressure through
  // the async IPC layer at the moment.  Blocking the IPC worker thread
  // is not desirable, either.
  nsresult rv = NS_NewPipe2(getter_AddRefs(mReader),
                            getter_AddRefs(mWriter),
                            true, true,   // non-blocking
                            0,            // segment size
                            UINT32_MAX);  // "infinite" pipe
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
IPCStreamDestination::SetDelayedStart(bool aDelayedStart)
{
  mDelayedStart = aDelayedStart;
}

already_AddRefed<nsIInputStream>
IPCStreamDestination::TakeReader()
{
  MOZ_ASSERT(mReader);
  MOZ_ASSERT(!mDelayedStartInputStream);

  if (mDelayedStart) {
    mDelayedStartInputStream =
      new DelayedStartInputStream(this, mReader.forget());
    RefPtr<nsIAsyncInputStream> inputStream = mDelayedStartInputStream;
    return inputStream.forget();
  }

  return mReader.forget();
}

bool
IPCStreamDestination::IsOnOwningThread() const
{
  return mOwningThread == NS_GetCurrentThread();
}

void
IPCStreamDestination::DispatchRunnable(already_AddRefed<nsIRunnable>&& aRunnable)
{
  nsCOMPtr<nsIRunnable> runnable = aRunnable;
  mOwningThread->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
}

void
IPCStreamDestination::ActorDestroyed()
{
  MOZ_ASSERT(mWriter);

  // If we were gracefully closed we should have gotten RecvClose().  In
  // that case, the writer will already be closed and this will have no
  // effect.  This just aborts the writer in the case where the child process
  // crashes.
  mWriter->CloseWithStatus(NS_ERROR_ABORT);

  if (mDelayedStartInputStream) {
    mDelayedStartInputStream->DestinationShutdown();
    mDelayedStartInputStream = nullptr;
  }
}

void
IPCStreamDestination::BufferReceived(const nsCString& aBuffer)
{
  MOZ_ASSERT(mWriter);

  uint32_t numWritten = 0;

  // This should only fail if we hit an OOM condition.
  nsresult rv = mWriter->Write(aBuffer.get(), aBuffer.Length(), &numWritten);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    RequestClose(rv);
  }
}

void
IPCStreamDestination::CloseReceived(nsresult aRv)
{
  MOZ_ASSERT(mWriter);
  mWriter->CloseWithStatus(aRv);
  TerminateDestination();
}

} // namespace ipc
} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobInputStream.h"
#include "IPCBlobInputStreamChild.h"
#include "IPCBlobInputStreamStorage.h"
#include "mozilla/ipc/InputStreamParams.h"
#include "mozilla/SlicedInputStream.h"
#include "mozilla/NonBlockingAsyncInputStream.h"
#include "IPCBlobInputStreamThread.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIPipe.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"

namespace mozilla {
namespace dom {

namespace {

class InputStreamCallbackRunnable final : public CancelableRunnable
{
public:
  // Note that the execution can be synchronous in case the event target is
  // null.
  static void
  Execute(nsIInputStreamCallback* aCallback,
          nsIEventTarget* aEventTarget,
          IPCBlobInputStream* aStream)
  {
    MOZ_ASSERT(aCallback);

    RefPtr<InputStreamCallbackRunnable> runnable =
      new InputStreamCallbackRunnable(aCallback, aStream);

    nsCOMPtr<nsIEventTarget> target = aEventTarget;
    if (aEventTarget) {
      target->Dispatch(runnable, NS_DISPATCH_NORMAL);
    } else {
      runnable->Run();
    }
  }

  NS_IMETHOD
  Run() override
  {
    mCallback->OnInputStreamReady(mStream);
    mCallback = nullptr;
    mStream = nullptr;
    return NS_OK;
  }

private:
  InputStreamCallbackRunnable(nsIInputStreamCallback* aCallback,
                              IPCBlobInputStream* aStream)
    : CancelableRunnable("dom::InputStreamCallbackRunnable")
    , mCallback(aCallback)
    , mStream(aStream)
  {
    MOZ_ASSERT(mCallback);
    MOZ_ASSERT(mStream);
  }

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  RefPtr<IPCBlobInputStream> mStream;
};

class FileMetadataCallbackRunnable final : public CancelableRunnable
{
public:
  static void
  Execute(nsIFileMetadataCallback* aCallback,
          nsIEventTarget* aEventTarget,
          IPCBlobInputStream* aStream)
  {
    MOZ_ASSERT(aCallback);
    MOZ_ASSERT(aEventTarget);

    RefPtr<FileMetadataCallbackRunnable> runnable =
      new FileMetadataCallbackRunnable(aCallback, aStream);

    nsCOMPtr<nsIEventTarget> target = aEventTarget;
    target->Dispatch(runnable, NS_DISPATCH_NORMAL);
  }

  NS_IMETHOD
  Run() override
  {
    mCallback->OnFileMetadataReady(mStream);
    mCallback = nullptr;
    mStream = nullptr;
    return NS_OK;
  }

private:
  FileMetadataCallbackRunnable(nsIFileMetadataCallback* aCallback,
                               IPCBlobInputStream* aStream)
    : CancelableRunnable("dom::FileMetadataCallbackRunnable")
    , mCallback(aCallback)
    , mStream(aStream)
  {
    MOZ_ASSERT(mCallback);
    MOZ_ASSERT(mStream);
  }

  nsCOMPtr<nsIFileMetadataCallback> mCallback;
  RefPtr<IPCBlobInputStream> mStream;
};

} // anonymous

NS_IMPL_ADDREF(IPCBlobInputStream);
NS_IMPL_RELEASE(IPCBlobInputStream);

NS_INTERFACE_MAP_BEGIN(IPCBlobInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamCallback)
  NS_INTERFACE_MAP_ENTRY(nsICloneableInputStream)
  NS_INTERFACE_MAP_ENTRY(nsICloneableInputStreamWithRange)
  NS_INTERFACE_MAP_ENTRY(nsIIPCSerializableInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIFileMetadata)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncFileMetadata)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

IPCBlobInputStream::IPCBlobInputStream(IPCBlobInputStreamChild* aActor)
  : mActor(aActor)
  , mState(eInit)
  , mStart(0)
  , mLength(0)
  , mMutex("IPCBlobInputStream::mMutex")
{
  MOZ_ASSERT(aActor);

  mLength = aActor->Size();

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIInputStream> stream;
    IPCBlobInputStreamStorage::Get()->GetStream(mActor->ID(),
                                                0, mLength,
                                                getter_AddRefs(stream));
    if (stream) {
      mState = eRunning;
      mRemoteStream = stream;
    }
  }
}

IPCBlobInputStream::~IPCBlobInputStream()
{
  Close();
}

// nsIInputStream interface

NS_IMETHODIMP
IPCBlobInputStream::Available(uint64_t* aLength)
{
  // We don't have a remoteStream yet: let's return 0.
  if (mState == eInit || mState == ePending) {
    *aLength = 0;
    return NS_OK;
  }

  if (mState == eRunning) {
    MOZ_ASSERT(mRemoteStream || mAsyncRemoteStream);

    nsresult rv = EnsureAsyncRemoteStream();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(mAsyncRemoteStream);
    return mAsyncRemoteStream->Available(aLength);
  }

  MOZ_ASSERT(mState == eClosed);
  return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP
IPCBlobInputStream::Read(char* aBuffer, uint32_t aCount, uint32_t* aReadCount)
{
  // Read is not available is we don't have a remoteStream.
  if (mState == eInit || mState == ePending) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  if (mState == eRunning) {
    MOZ_ASSERT(mRemoteStream || mAsyncRemoteStream);

    nsresult rv = EnsureAsyncRemoteStream();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(mAsyncRemoteStream);
    return mAsyncRemoteStream->Read(aBuffer, aCount, aReadCount);
  }

  MOZ_ASSERT(mState == eClosed);
  return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP
IPCBlobInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                 uint32_t aCount, uint32_t *aResult)
{
  // ReadSegments is not available is we don't have a remoteStream.
  if (mState == eInit || mState == ePending) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  if (mState == eRunning) {
    MOZ_ASSERT(mRemoteStream || mAsyncRemoteStream);

    nsresult rv = EnsureAsyncRemoteStream();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(mAsyncRemoteStream);
    return mAsyncRemoteStream->ReadSegments(aWriter, aClosure, aCount, aResult);
  }

  MOZ_ASSERT(mState == eClosed);
  return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP
IPCBlobInputStream::IsNonBlocking(bool* aNonBlocking)
{
  *aNonBlocking = true;
  return NS_OK;
}

NS_IMETHODIMP
IPCBlobInputStream::Close()
{
  if (mActor) {
    mActor->ForgetStream(this);
    mActor = nullptr;
  }

  if (mAsyncRemoteStream) {
    mAsyncRemoteStream->CloseWithStatus(NS_BASE_STREAM_CLOSED);
    mAsyncRemoteStream = nullptr;
  }

  if (mRemoteStream) {
    mRemoteStream->Close();
    mRemoteStream = nullptr;
  }

  {
    MutexAutoLock lock(mMutex);

    mInputStreamCallback = nullptr;
    mInputStreamCallbackEventTarget = nullptr;
  }

  mFileMetadataCallback = nullptr;
  mFileMetadataCallbackEventTarget = nullptr;

  mState = eClosed;
  return NS_OK;
}

// nsICloneableInputStream interface

NS_IMETHODIMP
IPCBlobInputStream::GetCloneable(bool* aCloneable)
{
  *aCloneable = mState != eClosed;
  return NS_OK;
}

NS_IMETHODIMP
IPCBlobInputStream::Clone(nsIInputStream** aResult)
{
  if (mState == eClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  MOZ_ASSERT(mActor);

  RefPtr<IPCBlobInputStream> stream = mActor->CreateStream();
  if (!stream) {
    return NS_ERROR_FAILURE;
  }

  stream->InitWithExistingRange(mStart, mLength);

  stream.forget(aResult);
  return NS_OK;
}

// nsICloneableInputStreamWithRange interface

NS_IMETHODIMP
IPCBlobInputStream::CloneWithRange(uint64_t aStart, uint64_t aLength,
                                   nsIInputStream** aResult)
{
  if (mState == eClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  // Too short or out of range.
  if (aLength == 0 || aStart >= mLength) {
    return NS_NewCStringInputStream(aResult, EmptyCString());
  }

  MOZ_ASSERT(mActor);

  RefPtr<IPCBlobInputStream> stream = mActor->CreateStream();
  if (!stream) {
    return NS_ERROR_FAILURE;
  }

  CheckedInt<uint64_t> streamSize = mLength;
  streamSize -= aStart;
  if (!streamSize.isValid()) {
    return NS_ERROR_FAILURE;
  }

  if (aLength > streamSize.value()) {
    aLength = streamSize.value();
  }

  stream->InitWithExistingRange(aStart + mStart, aLength);

  stream.forget(aResult);
  return NS_OK;
}

// nsIAsyncInputStream interface

NS_IMETHODIMP
IPCBlobInputStream::CloseWithStatus(nsresult aStatus)
{
  return Close();
}

NS_IMETHODIMP
IPCBlobInputStream::AsyncWait(nsIInputStreamCallback* aCallback,
                              uint32_t aFlags, uint32_t aRequestedCount,
                              nsIEventTarget* aEventTarget)
{
  // See IPCBlobInputStream.h for more information about this state machine.

  switch (mState) {
  // First call, we need to retrieve the stream from the parent actor.
  case eInit:
    MOZ_ASSERT(mActor);

    mInputStreamCallback = aCallback;
    mInputStreamCallbackEventTarget = aEventTarget;
    mState = ePending;

    mActor->StreamNeeded(this, aEventTarget);
    return NS_OK;

  // We are still waiting for the remote inputStream
  case ePending: {
    MutexAutoLock lock(mMutex);

    if (mInputStreamCallback && aCallback) {
      return NS_ERROR_FAILURE;
    }

    mInputStreamCallback = aCallback;
    mInputStreamCallbackEventTarget = aEventTarget;
    return NS_OK;
  }

  // We have the remote inputStream, let's check if we can execute the callback.
  case eRunning: {
    {
      MutexAutoLock lock(mMutex);
      mInputStreamCallback = aCallback;
      mInputStreamCallbackEventTarget = aEventTarget;
    }

    nsresult rv = EnsureAsyncRemoteStream();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(mAsyncRemoteStream);
    return mAsyncRemoteStream->AsyncWait(aCallback ? this : nullptr,
                                         0, 0, aEventTarget);
  }

  // Stream is closed.
  default:
    MOZ_ASSERT(mState == eClosed);
    return NS_BASE_STREAM_CLOSED;
  }
}

void
IPCBlobInputStream::StreamReady(already_AddRefed<nsIInputStream> aInputStream)
{
  nsCOMPtr<nsIInputStream> inputStream = Move(aInputStream);

  // We have been closed in the meantime.
  if (mState == eClosed) {
    if (inputStream) {
      inputStream->Close();
    }
    return;
  }

  // If inputStream is null, it means that the serialization went wrong or the
  // stream is not available anymore. We keep the state as pending just to block
  // any additional operation.

  if (!inputStream) {
    return;
  }

  // Now it's the right time to apply a slice if needed.
  if (mStart > 0 || mLength < mActor->Size()) {
    inputStream =
      new SlicedInputStream(inputStream.forget(), mStart, mLength);
  }

  mRemoteStream = inputStream;

  MOZ_ASSERT(mState == ePending);
  mState = eRunning;

  nsCOMPtr<nsIFileMetadataCallback> fileMetadataCallback;
  fileMetadataCallback.swap(mFileMetadataCallback);

  nsCOMPtr<nsIEventTarget> fileMetadataCallbackEventTarget;
  fileMetadataCallbackEventTarget.swap(mFileMetadataCallbackEventTarget);

  if (fileMetadataCallback) {
    FileMetadataCallbackRunnable::Execute(fileMetadataCallback,
                                          fileMetadataCallbackEventTarget,
                                          this);
  }

  nsCOMPtr<nsIInputStreamCallback> inputStreamCallback = this;
  nsCOMPtr<nsIEventTarget> inputStreamCallbackEventTarget;
  {
    MutexAutoLock lock(mMutex);
    inputStreamCallbackEventTarget = mInputStreamCallbackEventTarget;
    if (!mInputStreamCallback) {
      inputStreamCallback = nullptr;
    }
  }

  if (inputStreamCallback) {
    nsresult rv = EnsureAsyncRemoteStream();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    MOZ_ASSERT(mAsyncRemoteStream);

    rv = mAsyncRemoteStream->AsyncWait(inputStreamCallback, 0, 0,
                                       inputStreamCallbackEventTarget);
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }
}

void
IPCBlobInputStream::InitWithExistingRange(uint64_t aStart, uint64_t aLength)
{
  MOZ_ASSERT(mActor->Size() >= aStart + aLength);
  mStart = aStart;
  mLength = aLength;

  // In the child, we slice in StreamReady() when we set mState to eRunning.
  // But in the parent, we start out eRunning, so it's necessary to slice the
  // stream as soon as we have the information during the initialization phase
  // because the stream is immediately consumable.
  if (mState == eRunning && mRemoteStream && XRE_IsParentProcess() &&
      (mStart > 0 || mLength < mActor->Size())) {
    mRemoteStream =
      new SlicedInputStream(mRemoteStream.forget(), mStart, mLength);
  }
}

// nsIInputStreamCallback

NS_IMETHODIMP
IPCBlobInputStream::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  nsCOMPtr<nsIInputStreamCallback> callback;
  nsCOMPtr<nsIEventTarget> callbackEventTarget;
  {
    MutexAutoLock lock(mMutex);

    // We have been closed in the meantime.
    if (mState == eClosed) {
      return NS_OK;
    }

    MOZ_ASSERT(mState == eRunning);
    MOZ_ASSERT(mAsyncRemoteStream == aStream);

    // The callback has been canceled in the meantime.
    if (!mInputStreamCallback) {
      return NS_OK;
    }

    callback.swap(mInputStreamCallback);
    callbackEventTarget.swap(mInputStreamCallbackEventTarget);
  }

  // This must be the last operation because the execution of the callback can
  // be synchronous.
  MOZ_ASSERT(callback);
  InputStreamCallbackRunnable::Execute(callback, callbackEventTarget, this);
  return NS_OK;
}

// nsIIPCSerializableInputStream

void
IPCBlobInputStream::Serialize(mozilla::ipc::InputStreamParams& aParams,
                              FileDescriptorArray& aFileDescriptors)
{
  mozilla::ipc::IPCBlobInputStreamParams params;
  params.id() = mActor->ID();
  params.start() = mStart;
  params.length() = mLength;

  aParams = params;
}

bool
IPCBlobInputStream::Deserialize(const mozilla::ipc::InputStreamParams& aParams,
                                const FileDescriptorArray& aFileDescriptors)
{
  MOZ_CRASH("This should never be called.");
  return false;
}

mozilla::Maybe<uint64_t>
IPCBlobInputStream::ExpectedSerializedLength()
{
  return mozilla::Nothing();
}

// nsIAsyncFileMetadata

NS_IMETHODIMP
IPCBlobInputStream::AsyncWait(nsIFileMetadataCallback* aCallback,
                              nsIEventTarget* aEventTarget)
{
  MOZ_ASSERT(!!aCallback == !!aEventTarget);

  // If we have the callback, we must have the event target.
  if (NS_WARN_IF(!!aCallback != !!aEventTarget)) {
    return NS_ERROR_FAILURE;
  }

  // See IPCBlobInputStream.h for more information about this state machine.

  switch (mState) {
  // First call, we need to retrieve the stream from the parent actor.
  case eInit:
    MOZ_ASSERT(mActor);

    mFileMetadataCallback = aCallback;
    mFileMetadataCallbackEventTarget = aEventTarget;
    mState = ePending;

    mActor->StreamNeeded(this, aEventTarget);
    return NS_OK;

  // We are still waiting for the remote inputStream
  case ePending:
    if (mFileMetadataCallback && aCallback) {
      return NS_ERROR_FAILURE;
    }

    mFileMetadataCallback = aCallback;
    mFileMetadataCallbackEventTarget = aEventTarget;
    return NS_OK;

  // We have the remote inputStream, let's check if we can execute the callback.
  case eRunning:
    FileMetadataCallbackRunnable::Execute(aCallback, aEventTarget, this);
    return NS_OK;

  // Stream is closed.
  default:
    MOZ_ASSERT(mState == eClosed);
    return NS_BASE_STREAM_CLOSED;
  }
}

// nsIFileMetadata

NS_IMETHODIMP
IPCBlobInputStream::GetSize(int64_t* aRetval)
{
  nsCOMPtr<nsIFileMetadata> fileMetadata = do_QueryInterface(mRemoteStream);
  if (!fileMetadata) {
    return mState == eClosed ? NS_BASE_STREAM_CLOSED : NS_ERROR_FAILURE;
  }

  return fileMetadata->GetSize(aRetval);
}

NS_IMETHODIMP
IPCBlobInputStream::GetLastModified(int64_t* aRetval)
{
  nsCOMPtr<nsIFileMetadata> fileMetadata = do_QueryInterface(mRemoteStream);
  if (!fileMetadata) {
    return mState == eClosed ? NS_BASE_STREAM_CLOSED : NS_ERROR_FAILURE;
  }

  return fileMetadata->GetLastModified(aRetval);
}

NS_IMETHODIMP
IPCBlobInputStream::GetFileDescriptor(PRFileDesc** aRetval)
{
  nsCOMPtr<nsIFileMetadata> fileMetadata = do_QueryInterface(mRemoteStream);
  if (!fileMetadata) {
    return mState == eClosed ? NS_BASE_STREAM_CLOSED : NS_ERROR_FAILURE;
  }

  return fileMetadata->GetFileDescriptor(aRetval);
}

nsresult
IPCBlobInputStream::EnsureAsyncRemoteStream()
{
  // We already have an async remote stream.
  if (mAsyncRemoteStream) {
    return NS_OK;
  }

  if (!mRemoteStream) {
    return NS_ERROR_FAILURE;
  }

  // If the stream is blocking, we want to make it unblocking using a pipe.
  bool nonBlocking = false;
  nsresult rv = mRemoteStream->IsNonBlocking(&nonBlocking);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(mRemoteStream);

  // If non-blocking and non-async, let's use NonBlockingAsyncInputStream.
  if (nonBlocking && !asyncStream) {
    rv = NonBlockingAsyncInputStream::Create(mRemoteStream.forget(),
                                             getter_AddRefs(asyncStream));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(asyncStream);
  }

  if (!asyncStream) {
    // Let's make the stream async using the DOMFile thread.
    nsCOMPtr<nsIAsyncInputStream> pipeIn;
    nsCOMPtr<nsIAsyncOutputStream> pipeOut;
    rv = NS_NewPipe2(getter_AddRefs(pipeIn),
                     getter_AddRefs(pipeOut),
                     true, true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    RefPtr<IPCBlobInputStreamThread> thread =
      IPCBlobInputStreamThread::GetOrCreate();
    if (NS_WARN_IF(!thread)) {
      return NS_ERROR_FAILURE;
    }

    rv = NS_AsyncCopy(mRemoteStream, pipeOut, thread,
                      NS_ASYNCCOPY_VIA_WRITESEGMENTS);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    asyncStream = pipeIn;
  }

  MOZ_ASSERT(asyncStream);
  mAsyncRemoteStream = asyncStream;
  mRemoteStream = nullptr;

  return NS_OK;
}

} // namespace dom
} // namespace mozilla

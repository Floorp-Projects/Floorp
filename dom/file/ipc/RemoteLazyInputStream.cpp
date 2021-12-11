/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteLazyInputStream.h"
#include "RemoteLazyInputStreamChild.h"
#include "RemoteLazyInputStreamParent.h"
#include "mozilla/ipc/InputStreamParams.h"
#include "mozilla/net/SocketProcessParent.h"
#include "mozilla/SlicedInputStream.h"
#include "mozilla/NonBlockingAsyncInputStream.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIPipe.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "RemoteLazyInputStreamStorage.h"
#include "RemoteLazyInputStreamThread.h"

namespace mozilla {

using namespace dom;
using net::SocketProcessParent;

class RemoteLazyInputStream;

namespace {

class InputStreamCallbackRunnable final : public DiscardableRunnable {
 public:
  // Note that the execution can be synchronous in case the event target is
  // null.
  static void Execute(nsIInputStreamCallback* aCallback,
                      nsIEventTarget* aEventTarget,
                      RemoteLazyInputStream* aStream) {
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
  Run() override {
    mCallback->OnInputStreamReady(mStream);
    mCallback = nullptr;
    mStream = nullptr;
    return NS_OK;
  }

 private:
  InputStreamCallbackRunnable(nsIInputStreamCallback* aCallback,
                              RemoteLazyInputStream* aStream)
      : DiscardableRunnable("dom::InputStreamCallbackRunnable"),
        mCallback(aCallback),
        mStream(aStream) {
    MOZ_ASSERT(mCallback);
    MOZ_ASSERT(mStream);
  }

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  RefPtr<RemoteLazyInputStream> mStream;
};

class FileMetadataCallbackRunnable final : public DiscardableRunnable {
 public:
  static void Execute(nsIFileMetadataCallback* aCallback,
                      nsIEventTarget* aEventTarget,
                      RemoteLazyInputStream* aStream) {
    MOZ_ASSERT(aCallback);
    MOZ_ASSERT(aEventTarget);

    RefPtr<FileMetadataCallbackRunnable> runnable =
        new FileMetadataCallbackRunnable(aCallback, aStream);

    nsCOMPtr<nsIEventTarget> target = aEventTarget;
    target->Dispatch(runnable, NS_DISPATCH_NORMAL);
  }

  NS_IMETHOD
  Run() override {
    mCallback->OnFileMetadataReady(mStream);
    mCallback = nullptr;
    mStream = nullptr;
    return NS_OK;
  }

 private:
  FileMetadataCallbackRunnable(nsIFileMetadataCallback* aCallback,
                               RemoteLazyInputStream* aStream)
      : DiscardableRunnable("dom::FileMetadataCallbackRunnable"),
        mCallback(aCallback),
        mStream(aStream) {
    MOZ_ASSERT(mCallback);
    MOZ_ASSERT(mStream);
  }

  nsCOMPtr<nsIFileMetadataCallback> mCallback;
  RefPtr<RemoteLazyInputStream> mStream;
};

}  // namespace

NS_IMPL_ADDREF(RemoteLazyInputStream);
NS_IMPL_RELEASE(RemoteLazyInputStream);

NS_INTERFACE_MAP_BEGIN(RemoteLazyInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamCallback)
  NS_INTERFACE_MAP_ENTRY(nsICloneableInputStream)
  NS_INTERFACE_MAP_ENTRY(nsICloneableInputStreamWithRange)
  NS_INTERFACE_MAP_ENTRY(nsIIPCSerializableInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIFileMetadata)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncFileMetadata)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamLength)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncInputStreamLength)
  NS_INTERFACE_MAP_ENTRY(mozIRemoteLazyInputStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

RemoteLazyInputStream::RemoteLazyInputStream(RemoteLazyInputStreamChild* aActor)
    : mActor(aActor),
      mState(eInit),
      mStart(0),
      mLength(0),
      mConsumed(false),
      mMutex("RemoteLazyInputStream::mMutex") {
  MOZ_ASSERT(aActor);

  mLength = aActor->Size();

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIInputStream> stream;
    auto storage = RemoteLazyInputStreamStorage::Get().unwrapOr(nullptr);
    if (storage) {
      storage->GetStream(mActor->ID(), 0, mLength, getter_AddRefs(stream));
      if (stream) {
        mState = eRunning;
        mRemoteStream = stream;
      }
    }
  }
}

RemoteLazyInputStream::~RemoteLazyInputStream() { Close(); }

// nsIInputStream interface

NS_IMETHODIMP
RemoteLazyInputStream::Available(uint64_t* aLength) {
  nsCOMPtr<nsIAsyncInputStream> asyncRemoteStream;
  {
    MutexAutoLock lock(mMutex);

    // We don't have a remoteStream yet: let's return 0.
    if (mState == eInit || mState == ePending) {
      *aLength = 0;
      return NS_OK;
    }

    if (mState == eClosed) {
      return NS_BASE_STREAM_CLOSED;
    }

    MOZ_ASSERT(mState == eRunning);
    MOZ_ASSERT(mRemoteStream || mAsyncRemoteStream);

    nsresult rv = EnsureAsyncRemoteStream(lock);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    asyncRemoteStream = mAsyncRemoteStream;
  }

  MOZ_ASSERT(asyncRemoteStream);
  return asyncRemoteStream->Available(aLength);
}

NS_IMETHODIMP
RemoteLazyInputStream::Read(char* aBuffer, uint32_t aCount,
                            uint32_t* aReadCount) {
  nsCOMPtr<nsIAsyncInputStream> asyncRemoteStream;
  {
    MutexAutoLock lock(mMutex);

    // Read is not available is we don't have a remoteStream.
    if (mState == eInit || mState == ePending) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }

    if (mState == eClosed) {
      return NS_BASE_STREAM_CLOSED;
    }

    MOZ_ASSERT(mState == eRunning);
    MOZ_ASSERT(mRemoteStream || mAsyncRemoteStream);

    nsresult rv = EnsureAsyncRemoteStream(lock);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    asyncRemoteStream = mAsyncRemoteStream;
  }

  MOZ_ASSERT(asyncRemoteStream);
  nsresult rv = asyncRemoteStream->Read(aBuffer, aCount, aReadCount);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  {
    MutexAutoLock lock(mMutex);
    mConsumed = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
RemoteLazyInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                    uint32_t aCount, uint32_t* aResult) {
  nsCOMPtr<nsIAsyncInputStream> asyncRemoteStream;
  {
    MutexAutoLock lock(mMutex);

    // ReadSegments is not available is we don't have a remoteStream.
    if (mState == eInit || mState == ePending) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }

    if (mState == eClosed) {
      return NS_BASE_STREAM_CLOSED;
    }

    MOZ_ASSERT(mState == eRunning);
    MOZ_ASSERT(mRemoteStream || mAsyncRemoteStream);

    nsresult rv = EnsureAsyncRemoteStream(lock);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    asyncRemoteStream = mAsyncRemoteStream;
  }

  MOZ_ASSERT(asyncRemoteStream);
  nsresult rv =
      asyncRemoteStream->ReadSegments(aWriter, aClosure, aCount, aResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If some data has been read, we mark the stream as consumed.
  if (*aResult != 0) {
    MutexAutoLock lock(mMutex);
    mConsumed = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
RemoteLazyInputStream::IsNonBlocking(bool* aNonBlocking) {
  *aNonBlocking = true;
  return NS_OK;
}

NS_IMETHODIMP
RemoteLazyInputStream::Close() {
  nsCOMPtr<nsIAsyncInputStream> asyncRemoteStream;
  nsCOMPtr<nsIInputStream> remoteStream;

  nsCOMPtr<nsIInputStreamCallback> inputStreamCallback;
  nsCOMPtr<nsIEventTarget> inputStreamCallbackEventTarget;

  {
    MutexAutoLock lock(mMutex);

    if (mActor) {
      mActor->ForgetStream(this);
      mActor = nullptr;
    }

    asyncRemoteStream.swap(mAsyncRemoteStream);
    remoteStream.swap(mRemoteStream);

    // TODO(Bug 1737783): Notify to the mFileMetadataCallback that this
    // lazy input stream has been closed.
    mFileMetadataCallback = nullptr;
    mFileMetadataCallbackEventTarget = nullptr;

    inputStreamCallback = std::move(mInputStreamCallback);
    inputStreamCallbackEventTarget = std::move(mInputStreamCallbackEventTarget);

    mState = eClosed;
  }

  if (inputStreamCallback) {
    InputStreamCallbackRunnable::Execute(inputStreamCallback,
                                         inputStreamCallbackEventTarget, this);
  }

  if (asyncRemoteStream) {
    asyncRemoteStream->CloseWithStatus(NS_BASE_STREAM_CLOSED);
  }

  if (remoteStream) {
    remoteStream->Close();
  }

  return NS_OK;
}

// nsICloneableInputStream interface

NS_IMETHODIMP
RemoteLazyInputStream::GetCloneable(bool* aCloneable) {
  MutexAutoLock lock(mMutex);
  *aCloneable = mState != eClosed;
  return NS_OK;
}

NS_IMETHODIMP
RemoteLazyInputStream::Clone(nsIInputStream** aResult) {
  MutexAutoLock lock(mMutex);

  if (mState == eClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  MOZ_ASSERT(mActor);

  RefPtr<RemoteLazyInputStream> stream = mActor->CreateStream();
  if (!stream) {
    return NS_ERROR_FAILURE;
  }

  stream->InitWithExistingRange(mStart, mLength, lock);

  stream.forget(aResult);
  return NS_OK;
}

// nsICloneableInputStreamWithRange interface

NS_IMETHODIMP
RemoteLazyInputStream::CloneWithRange(uint64_t aStart, uint64_t aLength,
                                      nsIInputStream** aResult) {
  MutexAutoLock lock(mMutex);

  if (mState == eClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  // Too short or out of range.
  if (aLength == 0 || aStart >= mLength) {
    return NS_NewCStringInputStream(aResult, ""_ns);
  }

  MOZ_ASSERT(mActor);

  RefPtr<RemoteLazyInputStream> stream = mActor->CreateStream();
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

  stream->InitWithExistingRange(aStart + mStart, aLength, lock);

  stream.forget(aResult);
  return NS_OK;
}

// nsIAsyncInputStream interface

NS_IMETHODIMP
RemoteLazyInputStream::CloseWithStatus(nsresult aStatus) { return Close(); }

NS_IMETHODIMP
RemoteLazyInputStream::AsyncWait(nsIInputStreamCallback* aCallback,
                                 uint32_t aFlags, uint32_t aRequestedCount,
                                 nsIEventTarget* aEventTarget) {
  nsCOMPtr<nsIAsyncInputStream> asyncRemoteStream;
  {
    MutexAutoLock lock(mMutex);

    // See RemoteLazyInputStream.h for more information about this state
    // machine.

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
        if (mInputStreamCallback && aCallback) {
          return NS_ERROR_FAILURE;
        }

        mInputStreamCallback = aCallback;
        mInputStreamCallbackEventTarget = aEventTarget;
        return NS_OK;
      }

      // We have the remote inputStream, let's check if we can execute the
      // callback.
      case eRunning: {
        if (mInputStreamCallback && aCallback) {
          return NS_ERROR_FAILURE;
        }

        nsresult rv = EnsureAsyncRemoteStream(lock);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        mInputStreamCallback = aCallback;
        mInputStreamCallbackEventTarget = aEventTarget;

        asyncRemoteStream = mAsyncRemoteStream;
        break;
      }

      case eClosed:
        [[fallthrough]];
      default:
        MOZ_ASSERT(mState == eClosed);
        if (mInputStreamCallback && aCallback) {
          return NS_ERROR_FAILURE;
        }
        break;
    }
  }

  if (asyncRemoteStream) {
    return asyncRemoteStream->AsyncWait(aCallback ? this : nullptr, 0, 0,
                                        aEventTarget);
  }

  // if asyncRemoteStream is nullptr here, that probably means the stream has
  // been closed and the callback can be executed immediately
  InputStreamCallbackRunnable::Execute(aCallback, aEventTarget, this);
  return NS_OK;
}

void RemoteLazyInputStream::StreamReady(
    already_AddRefed<nsIInputStream> aInputStream) {
  nsCOMPtr<nsIInputStream> inputStream = std::move(aInputStream);

  // If inputStream is null, it means that the serialization went wrong or the
  // stream is not available anymore. We keep the state as pending just to
  // block any additional operation.

  if (!inputStream) {
    return;
  }

  nsCOMPtr<nsIFileMetadataCallback> fileMetadataCallback;
  nsCOMPtr<nsIEventTarget> fileMetadataCallbackEventTarget;
  nsCOMPtr<nsIInputStreamCallback> inputStreamCallback;
  nsCOMPtr<nsIEventTarget> inputStreamCallbackEventTarget;
  nsCOMPtr<nsIAsyncInputStream> asyncRemoteStream;
  {
    MutexAutoLock lock(mMutex);

    // We have been closed in the meantime.
    if (mState == eClosed) {
      if (inputStream) {
        MutexAutoUnlock unlock(mMutex);
        inputStream->Close();
      }
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

    fileMetadataCallback.swap(mFileMetadataCallback);
    fileMetadataCallbackEventTarget.swap(mFileMetadataCallbackEventTarget);

    inputStreamCallback = mInputStreamCallback ? this : nullptr;
    inputStreamCallbackEventTarget = mInputStreamCallbackEventTarget;

    if (inputStreamCallback) {
      nsresult rv = EnsureAsyncRemoteStream(lock);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return;
      }

      MOZ_ASSERT(mAsyncRemoteStream);
      asyncRemoteStream = mAsyncRemoteStream;
    }
  }

  if (fileMetadataCallback) {
    FileMetadataCallbackRunnable::Execute(
        fileMetadataCallback, fileMetadataCallbackEventTarget, this);
  }

  if (inputStreamCallback) {
    MOZ_ASSERT(asyncRemoteStream);

    nsresult rv = asyncRemoteStream->AsyncWait(inputStreamCallback, 0, 0,
                                               inputStreamCallbackEventTarget);
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }
}

void RemoteLazyInputStream::InitWithExistingRange(
    uint64_t aStart, uint64_t aLength, const MutexAutoLock& aProofOfLock) {
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
RemoteLazyInputStream::OnInputStreamReady(nsIAsyncInputStream* aStream) {
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

void RemoteLazyInputStream::Serialize(
    mozilla::ipc::InputStreamParams& aParams,
    FileDescriptorArray& aFileDescriptors, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed,
    mozilla::ipc::ParentToChildStreamActorManager* aManager) {
  MOZ_ASSERT(aSizeUsed);
  *aSizeUsed = 0;

  // So far we support only socket process serialization.
  MOZ_DIAGNOSTIC_ASSERT(
      aManager == SocketProcessParent::GetSingleton(),
      "Serializing an RemoteLazyInputStream parent to child is "
      "wrong! The caller must be fixed! See IPCBlobUtils.h.");
  SocketProcessParent* socketActor = SocketProcessParent::GetSingleton();

  nsresult rv;
  nsCOMPtr<nsIAsyncInputStream> asyncRemoteStream;
  RefPtr<RemoteLazyInputStreamParent> parentActor;
  {
    MutexAutoLock lock(mMutex);
    rv = EnsureAsyncRemoteStream(lock);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    asyncRemoteStream = mAsyncRemoteStream;
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  MOZ_ASSERT(asyncRemoteStream);

  parentActor = RemoteLazyInputStreamParent::Create(asyncRemoteStream, mLength,
                                                    0, &rv, socketActor);
  MOZ_ASSERT(parentActor);

  if (!socketActor->SendPRemoteLazyInputStreamConstructor(
          parentActor, parentActor->ID(), parentActor->Size())) {
    MOZ_CRASH("The serialization is not supposed to fail");
  }

  aParams = mozilla::ipc::RemoteLazyInputStreamParams(parentActor);
}

void RemoteLazyInputStream::Serialize(
    mozilla::ipc::InputStreamParams& aParams,
    FileDescriptorArray& aFileDescriptors, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed,
    mozilla::ipc::ChildToParentStreamActorManager* aManager) {
  MOZ_ASSERT(aSizeUsed);
  *aSizeUsed = 0;

  MutexAutoLock lock(mMutex);

  mozilla::ipc::RemoteLazyInputStreamRef params;
  params.id() = mActor->ID();
  params.start() = mStart;
  params.length() = mLength;

  aParams = params;
}

bool RemoteLazyInputStream::Deserialize(
    const mozilla::ipc::InputStreamParams& aParams,
    const FileDescriptorArray& aFileDescriptors) {
  MOZ_CRASH("This should never be called.");
  return false;
}

// nsIAsyncFileMetadata

NS_IMETHODIMP
RemoteLazyInputStream::AsyncFileMetadataWait(nsIFileMetadataCallback* aCallback,
                                             nsIEventTarget* aEventTarget) {
  MOZ_ASSERT(!!aCallback == !!aEventTarget);

  // If we have the callback, we must have the event target.
  if (NS_WARN_IF(!!aCallback != !!aEventTarget)) {
    return NS_ERROR_FAILURE;
  }

  // See RemoteLazyInputStream.h for more information about this state
  // machine.

  {
    MutexAutoLock lock(mMutex);

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

      // We have the remote inputStream, let's check if we can execute the
      // callback.
      case eRunning:
        break;

      // Stream is closed.
      default:
        MOZ_ASSERT(mState == eClosed);
        return NS_BASE_STREAM_CLOSED;
    }

    MOZ_ASSERT(mState == eRunning);
  }

  FileMetadataCallbackRunnable::Execute(aCallback, aEventTarget, this);
  return NS_OK;
}

// nsIFileMetadata

NS_IMETHODIMP
RemoteLazyInputStream::GetSize(int64_t* aRetval) {
  nsCOMPtr<nsIFileMetadata> fileMetadata;
  {
    MutexAutoLock lock(mMutex);
    fileMetadata = do_QueryInterface(mRemoteStream);
    if (!fileMetadata) {
      return mState == eClosed ? NS_BASE_STREAM_CLOSED : NS_ERROR_FAILURE;
    }
  }

  return fileMetadata->GetSize(aRetval);
}

NS_IMETHODIMP
RemoteLazyInputStream::GetLastModified(int64_t* aRetval) {
  nsCOMPtr<nsIFileMetadata> fileMetadata;
  {
    MutexAutoLock lock(mMutex);
    fileMetadata = do_QueryInterface(mRemoteStream);
    if (!fileMetadata) {
      return mState == eClosed ? NS_BASE_STREAM_CLOSED : NS_ERROR_FAILURE;
    }
  }

  return fileMetadata->GetLastModified(aRetval);
}

NS_IMETHODIMP
RemoteLazyInputStream::GetFileDescriptor(PRFileDesc** aRetval) {
  nsCOMPtr<nsIFileMetadata> fileMetadata;
  {
    MutexAutoLock lock(mMutex);
    fileMetadata = do_QueryInterface(mRemoteStream);
    if (!fileMetadata) {
      return mState == eClosed ? NS_BASE_STREAM_CLOSED : NS_ERROR_FAILURE;
    }
  }

  return fileMetadata->GetFileDescriptor(aRetval);
}

nsresult RemoteLazyInputStream::EnsureAsyncRemoteStream(
    const MutexAutoLock& aProofOfLock) {
  // We already have an async remote stream.
  if (mAsyncRemoteStream) {
    return NS_OK;
  }

  if (!mRemoteStream) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIInputStream> stream = mRemoteStream;
  // We don't return NS_ERROR_NOT_IMPLEMENTED from ReadSegments,
  // so it's possible that callers are expecting us to succeed in the future.
  // We need to make sure the stream we return here supports ReadSegments,
  // so wrap if in a buffered stream if necessary.
  if (!NS_InputStreamIsBuffered(stream)) {
    nsCOMPtr<nsIInputStream> bufferedStream;
    nsresult rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                            stream.forget(), 4096);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    stream = bufferedStream;
  }

  // If the stream is blocking, we want to make it unblocking using a pipe.
  bool nonBlocking = false;
  nsresult rv = stream->IsNonBlocking(&nonBlocking);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(stream);

  // If non-blocking and non-async, let's use NonBlockingAsyncInputStream.
  if (nonBlocking && !asyncStream) {
    rv = NonBlockingAsyncInputStream::Create(stream.forget(),
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
    rv = NS_NewPipe2(getter_AddRefs(pipeIn), getter_AddRefs(pipeOut), true,
                     true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    RefPtr<RemoteLazyInputStreamThread> thread =
        RemoteLazyInputStreamThread::GetOrCreate();
    if (NS_WARN_IF(!thread)) {
      return NS_ERROR_FAILURE;
    }

    rv = NS_AsyncCopy(stream, pipeOut, thread, NS_ASYNCCOPY_VIA_WRITESEGMENTS);
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

// nsIInputStreamLength

NS_IMETHODIMP
RemoteLazyInputStream::Length(int64_t* aLength) {
  MutexAutoLock lock(mMutex);

  if (mState == eClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  if (mConsumed) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_BASE_STREAM_WOULD_BLOCK;
}

namespace {

class InputStreamLengthCallbackRunnable final : public DiscardableRunnable {
 public:
  static void Execute(nsIInputStreamLengthCallback* aCallback,
                      nsIEventTarget* aEventTarget,
                      RemoteLazyInputStream* aStream, int64_t aLength) {
    MOZ_ASSERT(aCallback);
    MOZ_ASSERT(aEventTarget);

    RefPtr<InputStreamLengthCallbackRunnable> runnable =
        new InputStreamLengthCallbackRunnable(aCallback, aStream, aLength);

    nsCOMPtr<nsIEventTarget> target = aEventTarget;
    target->Dispatch(runnable, NS_DISPATCH_NORMAL);
  }

  NS_IMETHOD
  Run() override {
    mCallback->OnInputStreamLengthReady(mStream, mLength);
    mCallback = nullptr;
    mStream = nullptr;
    return NS_OK;
  }

 private:
  InputStreamLengthCallbackRunnable(nsIInputStreamLengthCallback* aCallback,
                                    RemoteLazyInputStream* aStream,
                                    int64_t aLength)
      : DiscardableRunnable("dom::InputStreamLengthCallbackRunnable"),
        mCallback(aCallback),
        mStream(aStream),
        mLength(aLength) {
    MOZ_ASSERT(mCallback);
    MOZ_ASSERT(mStream);
  }

  nsCOMPtr<nsIInputStreamLengthCallback> mCallback;
  RefPtr<RemoteLazyInputStream> mStream;
  const int64_t mLength;
};

}  // namespace

// nsIAsyncInputStreamLength

NS_IMETHODIMP
RemoteLazyInputStream::AsyncLengthWait(nsIInputStreamLengthCallback* aCallback,
                                       nsIEventTarget* aEventTarget) {
  // If we have the callback, we must have the event target.
  if (NS_WARN_IF(!!aCallback != !!aEventTarget)) {
    return NS_ERROR_FAILURE;
  }

  {
    MutexAutoLock lock(mMutex);

    mLengthCallback = aCallback;
    mLengthCallbackEventTarget = aEventTarget;

    if (mState != eClosed && !mConsumed) {
      MOZ_ASSERT(mActor);

      if (aCallback) {
        mActor->LengthNeeded(this, aEventTarget);
      }

      return NS_OK;
    }
  }

  // If execution has reached here, it means the stream is either closed or
  // consumed, and therefore the callback can be executed immediately
  InputStreamLengthCallbackRunnable::Execute(aCallback, aEventTarget, this, -1);
  return NS_OK;
}

void RemoteLazyInputStream::LengthReady(int64_t aLength) {
  nsCOMPtr<nsIInputStreamLengthCallback> lengthCallback;
  nsCOMPtr<nsIEventTarget> lengthCallbackEventTarget;

  {
    MutexAutoLock lock(mMutex);

    // Stream has been closed in the meantime. Callback can be executed
    // immediately
    if (mState == eClosed || mConsumed) {
      aLength = -1;
    } else {
      if (mStart > 0) {
        aLength -= mStart;
      }

      if (mLength < mActor->Size()) {
        // If the remote stream must be sliced, we must return here the
        // correct value.
        aLength = XPCOM_MIN(aLength, (int64_t)mLength);
      }
    }

    lengthCallback.swap(mLengthCallback);
    lengthCallbackEventTarget.swap(mLengthCallbackEventTarget);
  }

  if (lengthCallback) {
    InputStreamLengthCallbackRunnable::Execute(
        lengthCallback, lengthCallbackEventTarget, this, aLength);
  }
}

}  // namespace mozilla

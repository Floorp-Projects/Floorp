/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobInputStream.h"
#include "IPCBlobInputStreamChild.h"
#include "nsIAsyncInputStream.h"
#include "mozilla/SystemGroup.h"

namespace mozilla {
namespace dom {

namespace {

class CallbackRunnable final : public CancelableRunnable
{
public:
  static void
  Execute(nsIInputStreamCallback* aCallback,
          nsIEventTarget* aEventTarget,
          IPCBlobInputStream* aStream)
  {
    RefPtr<CallbackRunnable> runnable =
      new CallbackRunnable(aCallback, aStream);

    nsCOMPtr<nsIEventTarget> target = aEventTarget;
    if (!target) {
      target = SystemGroup::EventTargetFor(TaskCategory::Other);
    }

    target->Dispatch(runnable, NS_DISPATCH_NORMAL);
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
  CallbackRunnable(nsIInputStreamCallback* aCallback,
                   IPCBlobInputStream* aStream)
    : mCallback(aCallback)
    , mStream(aStream)
  {
    MOZ_ASSERT(mCallback);
    MOZ_ASSERT(mStream);
  }

  nsCOMPtr<nsIInputStreamCallback> mCallback;
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
  NS_INTERFACE_MAP_ENTRY(nsIIPCSerializableInputStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

IPCBlobInputStream::IPCBlobInputStream(IPCBlobInputStreamChild* aActor)
  : mActor(aActor)
  , mState(eInit)
{
  MOZ_ASSERT(aActor);
}

IPCBlobInputStream::~IPCBlobInputStream()
{
  Close();
}

// nsIInputStream interface

NS_IMETHODIMP
IPCBlobInputStream::Available(uint64_t* aLength)
{
  // We don't have a remoteStream yet. Let's return the full known size.
  if (mState == eInit || mState == ePending) {
    *aLength = mActor->Size();
    return NS_OK;
  }

  if (mState == eRunning) {
    MOZ_ASSERT(mRemoteStream);
    return mRemoteStream->Available(aLength);
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
    return mRemoteStream->Read(aBuffer, aCount, aReadCount);
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
    return mRemoteStream->ReadSegments(aWriter, aClosure, aCount, aResult);
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

  if (mRemoteStream) {
    mRemoteStream->Close();
    mRemoteStream = nullptr;
  }

  mCallback = nullptr;

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

  nsCOMPtr<nsIInputStream> stream = mActor->CreateStream();
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

    mCallback = aCallback;
    mCallbackEventTarget = aEventTarget;
    mState = ePending;

    mActor->StreamNeeded(this);
    return NS_OK;

  // We are still waiting for the remote inputStream
  case ePending:
    if (mCallback && aCallback) {
      return NS_ERROR_FAILURE;
    }

    mCallback = aCallback;
    mCallbackEventTarget = aEventTarget;
    return NS_OK;

  // We have the remote inputStream, let's check if we can execute the callback.
  case eRunning:
    return MaybeExecuteCallback(aCallback, aEventTarget);

  // Stream is closed.
  default:
    MOZ_ASSERT(mState == eClosed);
    return NS_BASE_STREAM_CLOSED;
  }
}

void
IPCBlobInputStream::StreamReady(nsIInputStream* aInputStream)
{
  // We have been closed in the meantime.
  if (mState == eClosed) {
    if (aInputStream) {
      aInputStream->Close();
    }
    return;
  }

  // If aInputStream is null, it means that the serialization went wrong or the
  // stream is not available anymore. We keep the state as pending just to block
  // any additional operation.

  nsCOMPtr<nsIInputStreamCallback> callback;
  callback.swap(mCallback);

  nsCOMPtr<nsIEventTarget> callbackEventTarget;
  callbackEventTarget.swap(mCallbackEventTarget);

  if (aInputStream && callback) {
    MOZ_ASSERT(mState == ePending);

    mRemoteStream = aInputStream;
    mState = eRunning;

    MaybeExecuteCallback(callback, callbackEventTarget);
  }
}

nsresult
IPCBlobInputStream::MaybeExecuteCallback(nsIInputStreamCallback* aCallback,
                                         nsIEventTarget* aCallbackEventTarget)
{
  MOZ_ASSERT(mState == eRunning);
  MOZ_ASSERT(mRemoteStream);

  // If the stream supports nsIAsyncInputStream, we need to call its AsyncWait
  // and wait for OnInputStreamReady.
  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(mRemoteStream);
  if (asyncStream) {
    // If the callback has been already set, we return an error.
    if (mCallback && aCallback) {
      return NS_ERROR_FAILURE;
    }

    mCallback = aCallback;
    mCallbackEventTarget = aCallbackEventTarget;

    if (!mCallback) {
      return NS_OK;
    }

    nsCOMPtr<nsIEventTarget> target = NS_GetCurrentThread();
    return asyncStream->AsyncWait(this, 0, 0, target);
  }

  MOZ_ASSERT(!mCallback);
  MOZ_ASSERT(!mCallbackEventTarget);

  if (!aCallback) {
    return NS_OK;
  }

  CallbackRunnable::Execute(aCallback, aCallbackEventTarget, this);
  return NS_OK;
}

// nsIInputStreamCallback

NS_IMETHODIMP
IPCBlobInputStream::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  // We have been closed in the meantime.
  if (mState == eClosed) {
    return NS_OK;
  }

  MOZ_ASSERT(mState == eRunning);
  MOZ_ASSERT(mRemoteStream == aStream);

  // The callback has been canceled in the meantime.
  if (!mCallback) {
    return NS_OK;
  }

  CallbackRunnable::Execute(mCallback, mCallbackEventTarget, this);

  mCallback = nullptr;
  mCallbackEventTarget = nullptr;

  return NS_OK;
}

// nsIIPCSerializableInputStream

void
IPCBlobInputStream::Serialize(mozilla::ipc::InputStreamParams& aParams,
                              FileDescriptorArray& aFileDescriptors)
{
  IPCBlobInputStreamParams params;
  params.id() = mActor->ID();

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

} // namespace dom
} // namespace mozilla

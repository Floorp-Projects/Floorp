/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BlobURLInputStream.h"
#include "BlobURL.h"
#include "BlobURLChannel.h"
#include "BlobURLProtocolHandler.h"

#include "mozilla/ScopeExit.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "nsStreamUtils.h"
#include "nsMimeTypes.h"

namespace mozilla::dom {

NS_IMPL_ADDREF(BlobURLInputStream);
NS_IMPL_RELEASE(BlobURLInputStream);

NS_INTERFACE_MAP_BEGIN(BlobURLInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamLength)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncInputStreamLength)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAsyncInputStream)
NS_INTERFACE_MAP_END

/* static */
already_AddRefed<nsIInputStream> BlobURLInputStream::Create(
    BlobURLChannel* const aChannel, BlobURL* const aBlobURL) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aChannel) || NS_WARN_IF(!aBlobURL)) {
    return nullptr;
  }

  nsAutoCString spec;

  nsresult rv = aBlobURL->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return MakeAndAddRef<BlobURLInputStream>(aChannel, spec);
}

// from nsIInputStream interface
NS_IMETHODIMP BlobURLInputStream::Close() {
  return CloseWithStatus(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP BlobURLInputStream::Available(uint64_t* aLength) {
  MutexAutoLock lock(mStateMachineMutex);

  if (mState == State::ERROR) {
    MOZ_ASSERT(NS_FAILED(mError));
    return mError;
  }

  if (mState == State::CLOSED) {
    return NS_BASE_STREAM_CLOSED;
  }

  if (mState == State::READY) {
    MOZ_ASSERT(mAsyncInputStream);
    return mAsyncInputStream->Available(aLength);
  }

  return NS_OK;
}

NS_IMETHODIMP BlobURLInputStream::StreamStatus() {
  MutexAutoLock lock(mStateMachineMutex);

  if (mState == State::ERROR) {
    MOZ_ASSERT(NS_FAILED(mError));
    return mError;
  }

  if (mState == State::CLOSED) {
    return NS_BASE_STREAM_CLOSED;
  }

  if (mState == State::READY) {
    MOZ_ASSERT(mAsyncInputStream);
    return mAsyncInputStream->StreamStatus();
  }

  return NS_OK;
}

NS_IMETHODIMP BlobURLInputStream::Read(char* aBuffer, uint32_t aCount,
                                       uint32_t* aReadCount) {
  MutexAutoLock lock(mStateMachineMutex);
  if (mState == State::ERROR) {
    MOZ_ASSERT(NS_FAILED(mError));
    return mError;
  }

  // Read() should not return NS_BASE_STREAM_CLOSED if stream is closed.
  // A read count of 0 should indicate closed or consumed stream.
  // See:
  // https://searchfox.org/mozilla-central/rev/559b25eb41c1cbffcb90a34e008b8288312fcd25/xpcom/io/nsIInputStream.idl#104
  if (mState == State::CLOSED) {
    *aReadCount = 0;
    return NS_OK;
  }

  if (mState == State::READY) {
    MOZ_ASSERT(mAsyncInputStream);
    nsresult rv = mAsyncInputStream->Read(aBuffer, aCount, aReadCount);
    if (NS_SUCCEEDED(rv) && aReadCount && !*aReadCount) {
      mState = State::CLOSED;
      ReleaseUnderlyingStream(lock);
    }
    return rv;
  }

  return NS_BASE_STREAM_WOULD_BLOCK;
}

NS_IMETHODIMP BlobURLInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                                               void* aClosure, uint32_t aCount,
                                               uint32_t* aResult) {
  // This means the caller will have to wrap the stream in an
  // nsBufferedInputStream in order to use ReadSegments
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP BlobURLInputStream::IsNonBlocking(bool* aNonBlocking) {
  *aNonBlocking = true;
  return NS_OK;
}

// from nsIAsyncInputStream interface
NS_IMETHODIMP BlobURLInputStream::CloseWithStatus(nsresult aStatus) {
  MutexAutoLock lock(mStateMachineMutex);
  if (mState == State::READY) {
    MOZ_ASSERT(mAsyncInputStream);
    mAsyncInputStream->CloseWithStatus(aStatus);
  }

  mState = State::CLOSED;
  ReleaseUnderlyingStream(lock);
  return NS_OK;
}

NS_IMETHODIMP BlobURLInputStream::AsyncWait(nsIInputStreamCallback* aCallback,
                                            uint32_t aFlags,
                                            uint32_t aRequestedCount,
                                            nsIEventTarget* aEventTarget) {
  MutexAutoLock lock(mStateMachineMutex);

  if (mState == State::ERROR) {
    MOZ_ASSERT(NS_FAILED(mError));
    return NS_ERROR_FAILURE;
  }

  // Pre-empting a valid callback with another is not allowed.
  if (NS_WARN_IF(mAsyncWaitCallback && aCallback &&
                 mAsyncWaitCallback != aCallback)) {
    return NS_ERROR_FAILURE;
  }

  mAsyncWaitTarget = aEventTarget;
  mAsyncWaitRequestedCount = aRequestedCount;
  mAsyncWaitFlags = aFlags;
  mAsyncWaitCallback = aCallback;

  if (mState == State::INITIAL) {
    mState = State::WAITING;
    // RetrieveBlobData will execute NotifyWWaitTarget() when retrieve succeeds
    // or fails
    if (NS_IsMainThread()) {
      RetrieveBlobData(lock);
      return NS_OK;
    }

    nsCOMPtr<nsIRunnable> runnable = mozilla::NewRunnableMethod(
        "BlobURLInputStream::CallRetrieveBlobData", this,
        &BlobURLInputStream::CallRetrieveBlobData);
    NS_DispatchToMainThread(runnable.forget(), NS_DISPATCH_NORMAL);
    return NS_OK;
  }

  if (mState == State::WAITING) {
    // RetrieveBlobData is already in progress and will execute
    // NotifyWaitTargets when retrieve succeeds or fails
    return NS_OK;
  }

  if (mState == State::READY) {
    // Ask the blob's input stream if reading is possible or not
    return mAsyncInputStream->AsyncWait(
        mAsyncWaitCallback ? this : nullptr, mAsyncWaitFlags,
        mAsyncWaitRequestedCount, mAsyncWaitTarget);
  }

  MOZ_ASSERT(mState == State::CLOSED);
  NotifyWaitTargets(lock);
  return NS_OK;
}

// from nsIInputStreamLength interface
NS_IMETHODIMP BlobURLInputStream::Length(int64_t* aLength) {
  MutexAutoLock lock(mStateMachineMutex);

  if (mState == State::CLOSED) {
    return NS_BASE_STREAM_CLOSED;
  }

  if (mState == State::ERROR) {
    MOZ_ASSERT(NS_FAILED(mError));
    return NS_ERROR_FAILURE;
  }

  if (mState == State::READY) {
    *aLength = mBlobSize;
    return NS_OK;
  }
  return NS_BASE_STREAM_WOULD_BLOCK;
}

// from nsIAsyncInputStreamLength interface
NS_IMETHODIMP BlobURLInputStream::AsyncLengthWait(
    nsIInputStreamLengthCallback* aCallback, nsIEventTarget* aEventTarget) {
  MutexAutoLock lock(mStateMachineMutex);

  if (mState == State::ERROR) {
    MOZ_ASSERT(NS_FAILED(mError));
    return mError;
  }

  // Pre-empting a valid callback with another is not allowed.
  if (mAsyncLengthWaitCallback && aCallback) {
    return NS_ERROR_FAILURE;
  }

  mAsyncLengthWaitTarget = aEventTarget;
  mAsyncLengthWaitCallback = aCallback;

  if (mState == State::INITIAL) {
    mState = State::WAITING;
    // RetrieveBlobData will execute NotifyWWaitTarget() when retrieve succeeds
    // or fails
    if (NS_IsMainThread()) {
      RetrieveBlobData(lock);
      return NS_OK;
    }

    nsCOMPtr<nsIRunnable> runnable = mozilla::NewRunnableMethod(
        "BlobURLInputStream::CallRetrieveBlobData", this,
        &BlobURLInputStream::CallRetrieveBlobData);
    NS_DispatchToMainThread(runnable.forget(), NS_DISPATCH_NORMAL);
    return NS_OK;
  }

  if (mState == State::WAITING) {
    // RetrieveBlobData is already in progress and will execute
    // NotifyWaitTargets when retrieve succeeds or fails
    return NS_OK;
  }

  // Since here the state must be READY (in which case the size of the blob is
  // already known) or CLOSED, callback can be called immediately
  NotifyWaitTargets(lock);
  return NS_OK;
}

// from nsIInputStreamCallback interface
NS_IMETHODIMP BlobURLInputStream::OnInputStreamReady(
    nsIAsyncInputStream* aStream) {
  nsCOMPtr<nsIInputStreamCallback> callback;

  {
    MutexAutoLock lock(mStateMachineMutex);
    MOZ_ASSERT_IF(mAsyncInputStream, aStream == mAsyncInputStream);

    // aborted in the meantime
    if (!mAsyncWaitCallback) {
      return NS_OK;
    }

    mAsyncWaitCallback.swap(callback);
    mAsyncWaitTarget = nullptr;
  }

  MOZ_ASSERT(callback);
  return callback->OnInputStreamReady(this);
}

// from nsIInputStreamLengthCallback interface
NS_IMETHODIMP BlobURLInputStream::OnInputStreamLengthReady(
    nsIAsyncInputStreamLength* aStream, int64_t aLength) {
  nsCOMPtr<nsIInputStreamLengthCallback> callback;
  {
    MutexAutoLock lock(mStateMachineMutex);

    // aborted in the meantime
    if (!mAsyncLengthWaitCallback) {
      return NS_OK;
    }

    mAsyncLengthWaitCallback.swap(callback);
    mAsyncLengthWaitCallback = nullptr;
  }

  return callback->OnInputStreamLengthReady(this, aLength);
}

// private:
BlobURLInputStream::~BlobURLInputStream() {
  if (mChannel) {
    NS_ReleaseOnMainThread("BlobURLInputStream::mChannel", mChannel.forget());
  }
}

BlobURLInputStream::BlobURLInputStream(BlobURLChannel* const aChannel,
                                       nsACString& aBlobURLSpec)
    : mChannel(aChannel),
      mBlobURLSpec(std::move(aBlobURLSpec)),
      mStateMachineMutex("BlobURLInputStream::mStateMachineMutex"),
      mState(State::INITIAL),
      mError(NS_OK),
      mBlobSize(-1),
      mAsyncWaitFlags(),
      mAsyncWaitRequestedCount() {}

void BlobURLInputStream::WaitOnUnderlyingStream(
    const MutexAutoLock& aProofOfLock) {
  if (mAsyncWaitCallback || mAsyncWaitTarget) {
    // AsyncWait should be called on the underlying stream
    mAsyncInputStream->AsyncWait(mAsyncWaitCallback ? this : nullptr,
                                 mAsyncWaitFlags, mAsyncWaitRequestedCount,
                                 mAsyncWaitTarget);
  }

  if (mAsyncLengthWaitCallback || mAsyncLengthWaitTarget) {
    // AsyncLengthWait should be called on the underlying stream
    nsCOMPtr<nsIAsyncInputStreamLength> asyncStreamLength =
        do_QueryInterface(mAsyncInputStream);
    if (asyncStreamLength) {
      asyncStreamLength->AsyncLengthWait(
          mAsyncLengthWaitCallback ? this : nullptr, mAsyncLengthWaitTarget);
    }
  }
}

void BlobURLInputStream::CallRetrieveBlobData() {
  MutexAutoLock lock(mStateMachineMutex);
  RetrieveBlobData(lock);
}

void BlobURLInputStream::RetrieveBlobData(const MutexAutoLock& aProofOfLock) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");

  MOZ_ASSERT(mState == State::WAITING);

  auto cleanupOnEarlyExit = MakeScopeExit([&] {
    mState = State::ERROR;
    mError = NS_ERROR_FAILURE;
    NS_ReleaseOnMainThread("BlobURLInputStream::mChannel", mChannel.forget());
    NotifyWaitTargets(aProofOfLock);
  });

  nsCOMPtr<nsILoadInfo> loadInfo = mChannel->LoadInfo();
  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  nsCOMPtr<nsIPrincipal> loadingPrincipal;
  if (NS_WARN_IF(NS_FAILED(loadInfo->GetTriggeringPrincipal(
          getter_AddRefs(triggeringPrincipal)))) ||
      NS_WARN_IF(!triggeringPrincipal)) {
    NS_WARNING("Failed to get owning channel's triggering principal");
    return;
  }

  if (NS_WARN_IF(NS_FAILED(
          loadInfo->GetLoadingPrincipal(getter_AddRefs(loadingPrincipal))))) {
    NS_WARNING("Failed to get owning channel's loading principal");
    return;
  }

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  loadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));

  nsAutoString partKey;
  cookieJarSettings->GetPartitionKey(partKey);

  if (XRE_IsParentProcess() || !BlobURLSchemeIsHTTPOrHTTPS(mBlobURLSpec)) {
    RefPtr<BlobImpl> blobImpl;

    // Since revoked blobs are also retrieved, it is possible that the blob no
    // longer exists (due to the 5 second timeout) when execution reaches here
    if (!BlobURLProtocolHandler::GetDataEntry(
            mBlobURLSpec, getter_AddRefs(blobImpl), loadingPrincipal,
            triggeringPrincipal, loadInfo->GetOriginAttributes(),
            loadInfo->GetInnerWindowID(), NS_ConvertUTF16toUTF8(partKey),
            true /* AlsoIfRevoked */)) {
      NS_WARNING("Failed to get data entry principal. URL revoked?");
      return;
    }

    if (NS_WARN_IF(
            NS_FAILED(StoreBlobImplStream(blobImpl.forget(), aProofOfLock)))) {
      return;
    }

    mState = State::READY;

    // By design, execution can only reach here when a caller has called
    // AsyncWait or AsyncLengthWait on this stream. The underlying stream is
    // valid, but the caller should not be informed until that stream has data
    // to read or it is closed.
    WaitOnUnderlyingStream(aProofOfLock);

    cleanupOnEarlyExit.release();
    return;
  }

  ContentChild* contentChild{ContentChild::GetSingleton()};
  MOZ_ASSERT(contentChild);

  const RefPtr<BlobURLInputStream> self = this;

  cleanupOnEarlyExit.release();

  contentChild
      ->SendBlobURLDataRequest(
          mBlobURLSpec, triggeringPrincipal, loadingPrincipal,
          loadInfo->GetOriginAttributes(), loadInfo->GetInnerWindowID(),
          NS_ConvertUTF16toUTF8(partKey))
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self](const BlobURLDataRequestResult& aResult) {
            MutexAutoLock lock(self->mStateMachineMutex);
            if (aResult.type() == BlobURLDataRequestResult::TIPCBlob) {
              if (self->mState == State::WAITING) {
                RefPtr<BlobImpl> blobImpl =
                    IPCBlobUtils::Deserialize(aResult.get_IPCBlob());
                if (blobImpl && self->StoreBlobImplStream(blobImpl.forget(),
                                                          lock) == NS_OK) {
                  self->mState = State::READY;
                  // By design, execution can only reach here when a caller has
                  // called AsyncWait or AsyncLengthWait on this stream. The
                  // underlying stream is valid, but the caller should not be
                  // informed until that stream has data to read or it is
                  // closed.
                  self->WaitOnUnderlyingStream(lock);
                  return;
                }
              } else {
                MOZ_ASSERT(self->mState == State::CLOSED);
                // Callback can be called immediately
                self->NotifyWaitTargets(lock);
                return;
              }
            }
            NS_WARNING("Blob data was not retrieved!");
            self->mState = State::ERROR;
            self->mError = aResult.type() == BlobURLDataRequestResult::Tnsresult
                               ? aResult.get_nsresult()
                               : NS_ERROR_FAILURE;
            NS_ReleaseOnMainThread("BlobURLInputStream::mChannel",
                                   self->mChannel.forget());
            self->NotifyWaitTargets(lock);
          },
          [self](mozilla::ipc::ResponseRejectReason aReason) {
            MutexAutoLock lock(self->mStateMachineMutex);
            NS_WARNING("IPC call to SendBlobURLDataRequest failed!");
            self->mState = State::ERROR;
            self->mError = NS_ERROR_FAILURE;
            NS_ReleaseOnMainThread("BlobURLInputStream::mChannel",
                                   self->mChannel.forget());
            self->NotifyWaitTargets(lock);
          });
}

nsresult BlobURLInputStream::StoreBlobImplStream(
    already_AddRefed<BlobImpl> aBlobImpl, const MutexAutoLock& aProofOfLock) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread");
  RefPtr<BlobImpl> blobImpl = aBlobImpl;
  nsAutoString blobContentType;
  nsAutoCString channelContentType;

  // If a Range header was in the request then fetch/XHR will have set a
  // ContentRange on the channel earlier so we may slice the blob now.
  blobImpl->GetType(blobContentType);
  const Maybe<nsBaseChannel::ContentRange>& contentRange =
      mChannel->GetContentRange();
  if (contentRange.isSome()) {
    IgnoredErrorResult result;
    uint64_t start = contentRange->Start();
    uint64_t end = contentRange->End();
    RefPtr<BlobImpl> slice =
        blobImpl->CreateSlice(start, end - start + 1, blobContentType, result);
    if (!result.Failed()) {
      blobImpl = slice;
    }
  }

  mChannel->GetContentType(channelContentType);
  // A empty content type is the correct channel content type in the case of a
  // fetch of a blob where the type was not set. It is invalid in others cases
  // such as a XHR (See https://xhr.spec.whatwg.org/#response-mime-type). The
  // XMLHttpRequestMainThread will set the channel content type to the correct
  // fallback value before this point, so we need to be careful to only override
  // it when the blob type is valid.
  if (!blobContentType.IsEmpty() ||
      channelContentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE)) {
    mChannel->SetContentType(NS_ConvertUTF16toUTF8(blobContentType));
  }

  auto cleanupOnExit = MakeScopeExit([&] { mChannel = nullptr; });

  if (blobImpl->IsFile()) {
    nsAutoString filename;
    blobImpl->GetName(filename);

    // Don't overwrite existing name.
    nsString ignored;
    bool hasName =
        NS_SUCCEEDED(mChannel->GetContentDispositionFilename(ignored));

    if (!filename.IsEmpty() && !hasName) {
      mChannel->SetContentDispositionFilename(filename);
    }
  }

  mozilla::ErrorResult errorResult;

  mBlobSize = blobImpl->GetSize(errorResult);

  if (NS_WARN_IF(errorResult.Failed())) {
    return errorResult.StealNSResult();
  }

  mChannel->SetContentLength(mBlobSize);

  nsCOMPtr<nsIInputStream> inputStream;
  blobImpl->CreateInputStream(getter_AddRefs(inputStream), errorResult);

  if (NS_WARN_IF(errorResult.Failed())) {
    return errorResult.StealNSResult();
  }

  if (NS_WARN_IF(!inputStream)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = NS_MakeAsyncNonBlockingInputStream(
      inputStream.forget(), getter_AddRefs(mAsyncInputStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!mAsyncInputStream)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

void BlobURLInputStream::NotifyWaitTargets(const MutexAutoLock& aProofOfLock) {
  if (mAsyncWaitCallback) {
    auto callback = mAsyncWaitTarget
                        ? NS_NewInputStreamReadyEvent(
                              "BlobURLInputStream::OnInputStreamReady",
                              mAsyncWaitCallback, mAsyncWaitTarget)
                        : mAsyncWaitCallback;

    mAsyncWaitCallback = nullptr;
    mAsyncWaitTarget = nullptr;
    callback->OnInputStreamReady(this);
  }

  if (mAsyncLengthWaitCallback) {
    const RefPtr<BlobURLInputStream> self = this;
    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
        "BlobURLInputStream::OnInputStreamLengthReady", [self] {
          self->mAsyncLengthWaitCallback->OnInputStreamLengthReady(
              self, self->mBlobSize);
        });

    mAsyncLengthWaitCallback = nullptr;

    if (mAsyncLengthWaitTarget) {
      mAsyncLengthWaitTarget->Dispatch(runnable, NS_DISPATCH_NORMAL);
      mAsyncLengthWaitTarget = nullptr;
    } else {
      runnable->Run();
    }
  }
}

void BlobURLInputStream::ReleaseUnderlyingStream(
    const MutexAutoLock& aProofOfLock) {
  mAsyncInputStream = nullptr;
  mBlobSize = -1;
}

}  // namespace mozilla::dom

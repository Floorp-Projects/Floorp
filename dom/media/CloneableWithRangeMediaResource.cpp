/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CloneableWithRangeMediaResource.h"

#include "mozilla/AbstractThread.h"
#include "mozilla/Monitor.h"
#include "nsContentUtils.h"
#include "nsIAsyncInputStream.h"
#include "nsITimedChannel.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {

namespace {

class InputStreamReader final : public nsIInputStreamCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  static already_AddRefed<InputStreamReader> Create(
      nsICloneableInputStreamWithRange* aStream, int64_t aStart,
      uint32_t aLength) {
    MOZ_ASSERT(aStream);

    nsCOMPtr<nsIInputStream> stream;
    nsresult rv =
        aStream->CloneWithRange(aStart, aLength, getter_AddRefs(stream));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    RefPtr<InputStreamReader> reader = new InputStreamReader(stream);
    return reader.forget();
  }

  nsresult Read(char* aBuffer, uint32_t aSize, uint32_t* aRead) {
    uint32_t done = 0;
    do {
      uint32_t read;
      nsresult rv = SyncRead(aBuffer + done, aSize - done, &read);
      if (NS_SUCCEEDED(rv) && read == 0) {
        break;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      done += read;
    } while (done != aSize);

    *aRead = done;
    return NS_OK;
  }

  NS_IMETHOD
  OnInputStreamReady(nsIAsyncInputStream* aStream) override {
    // Let's continue with SyncRead().
    MonitorAutoLock lock(mMonitor);
    lock.Notify();
    return NS_OK;
  }

 private:
  explicit InputStreamReader(nsIInputStream* aStream)
      : mStream(aStream), mMonitor("InputStreamReader::mMonitor") {
    MOZ_ASSERT(aStream);
  }

  ~InputStreamReader() = default;

  nsresult SyncRead(char* aBuffer, uint32_t aSize, uint32_t* aRead) {
    while (1) {
      nsresult rv = mStream->Read(aBuffer, aSize, aRead);
      // All good.
      if (rv == NS_BASE_STREAM_CLOSED || NS_SUCCEEDED(rv)) {
        return NS_OK;
      }

      // An error.
      if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK) {
        return rv;
      }

      // We need to proceed async.
      if (!mAsyncStream) {
        mAsyncStream = do_QueryInterface(mStream);
      }

      if (!mAsyncStream) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIEventTarget> target =
          do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      MOZ_ASSERT(target);

      {
        // We wait for ::OnInputStreamReady() to be called.
        MonitorAutoLock lock(mMonitor);

        rv = mAsyncStream->AsyncWait(this, 0, aSize, target);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        lock.Wait();
      }
    }
  }

  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsIAsyncInputStream> mAsyncStream;
  Monitor mMonitor MOZ_UNANNOTATED;
};

NS_IMPL_ADDREF(InputStreamReader);
NS_IMPL_RELEASE(InputStreamReader);

NS_INTERFACE_MAP_BEGIN(InputStreamReader)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStreamCallback)
NS_INTERFACE_MAP_END

}  // namespace

void CloneableWithRangeMediaResource::MaybeInitialize() {
  if (!mInitialized) {
    mInitialized = true;
    mCallback->AbstractMainThread()->Dispatch(NewRunnableMethod<nsresult>(
        "MediaResourceCallback::NotifyDataEnded", mCallback.get(),
        &MediaResourceCallback::NotifyDataEnded, NS_OK));
  }
}

nsresult CloneableWithRangeMediaResource::GetCachedRanges(
    MediaByteRangeSet& aRanges) {
  MaybeInitialize();
  aRanges += MediaByteRange(0, (int64_t)mSize);
  return NS_OK;
}

nsresult CloneableWithRangeMediaResource::Open(
    nsIStreamListener** aStreamListener) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aStreamListener);

  *aStreamListener = nullptr;
  return NS_OK;
}

RefPtr<GenericPromise> CloneableWithRangeMediaResource::Close() {
  return GenericPromise::CreateAndResolve(true, __func__);
}

already_AddRefed<nsIPrincipal>
CloneableWithRangeMediaResource::GetCurrentPrincipal() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIPrincipal> principal;
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  if (!secMan || !mChannel) {
    return nullptr;
  }

  secMan->GetChannelResultPrincipal(mChannel, getter_AddRefs(principal));
  return principal.forget();
}

bool CloneableWithRangeMediaResource::HadCrossOriginRedirects() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsITimedChannel> timedChannel = do_QueryInterface(mChannel);
  if (!timedChannel) {
    return false;
  }

  bool allRedirectsSameOrigin = false;
  return NS_SUCCEEDED(timedChannel->GetAllRedirectsSameOrigin(
             &allRedirectsSameOrigin)) &&
         !allRedirectsSameOrigin;
}

nsresult CloneableWithRangeMediaResource::ReadFromCache(char* aBuffer,
                                                        int64_t aOffset,
                                                        uint32_t aCount) {
  MaybeInitialize();
  if (!aCount) {
    return NS_OK;
  }

  RefPtr<InputStreamReader> reader =
      InputStreamReader::Create(mStream, aOffset, aCount);
  if (!reader) {
    return NS_ERROR_FAILURE;
  }

  uint32_t bytes = 0;
  nsresult rv = reader->Read(aBuffer, aCount, &bytes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return bytes == aCount ? NS_OK : NS_ERROR_FAILURE;
}

nsresult CloneableWithRangeMediaResource::ReadAt(int64_t aOffset, char* aBuffer,
                                                 uint32_t aCount,
                                                 uint32_t* aBytes) {
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<InputStreamReader> reader =
      InputStreamReader::Create(mStream, aOffset, aCount);
  if (!reader) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = reader->Read(aBuffer, aCount, aBytes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

}  // namespace mozilla

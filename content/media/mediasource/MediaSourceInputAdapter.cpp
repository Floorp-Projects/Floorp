/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaSourceInputAdapter.h"

#include "nsStreamUtils.h"
#include "nsCycleCollectionParticipant.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaSourceLog;
#define LOG(type, msg) PR_LOG(gMediaSourceLog, type, msg)
#else
#define LOG(type, msg)
#endif

namespace mozilla {
namespace dom {

NS_IMETHODIMP
MediaSourceInputAdapter::Close()
{
  MonitorAutoLock mon(mMediaSource->GetMonitor());
  LOG(PR_LOG_DEBUG, ("%p IA::Close", this));
  //MOZ_ASSERT(!mClosed);
  mClosed = true;
  NotifyListener();
  return NS_OK;
}

NS_IMETHODIMP
MediaSourceInputAdapter::Available(uint64_t* aAvailable)
{
  MonitorAutoLock mon(mMediaSource->GetMonitor());
  if (mClosed) {
    LOG(PR_LOG_DEBUG, ("%p IA::Available (closed)", this));
    return NS_BASE_STREAM_CLOSED;
  }
  *aAvailable = Available();
  LOG(PR_LOG_DEBUG, ("%p IA::Available available=%llu", this, *aAvailable));
  return NS_OK;
}

NS_IMETHODIMP
MediaSourceInputAdapter::Read(char* aBuf, uint32_t aCount, uint32_t* aWriteCount)
{
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, aWriteCount);
}

NS_IMETHODIMP
MediaSourceInputAdapter::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                      uint32_t aCount, uint32_t* aWriteCount)
{
  MonitorAutoLock mon(mMediaSource->GetMonitor());

  uint32_t available = Available();
  LOG(PR_LOG_DEBUG, ("%p IA::ReadSegments aCount=%u available=%u appendDone=%d rv=%x",
                     this, aCount, available, mMediaSource->AppendDone(),
                     mMediaSource->AppendDone() ? NS_OK : NS_BASE_STREAM_WOULD_BLOCK));
  if (available == 0) {
    *aWriteCount = 0;
    return mMediaSource->AppendDone() ? NS_OK : NS_BASE_STREAM_WOULD_BLOCK;
  }

  uint32_t count = std::min(aCount, available);
  nsresult rv = aWriter(this, aClosure,
                        reinterpret_cast<const char*>(&mMediaSource->GetData()[mOffset]),
                        0, count, aWriteCount);
  if (NS_SUCCEEDED(rv)) {
    MOZ_ASSERT(*aWriteCount <= count);
    mOffset += *aWriteCount;
  }
  return NS_OK;
}

NS_IMETHODIMP
MediaSourceInputAdapter::IsNonBlocking(bool* aNonBlocking)
{
  LOG(PR_LOG_DEBUG, ("%p IA::IsNonBlocking", this));
  *aNonBlocking = true;
  return NS_OK;
}

NS_IMETHODIMP
MediaSourceInputAdapter::CloseWithStatus(nsresult aStatus)
{
  return Close();
}

NS_IMETHODIMP
MediaSourceInputAdapter::AsyncWait(nsIInputStreamCallback* aCallback, uint32_t aFlags,
                                   uint32_t aRequestedCount, nsIEventTarget* aTarget)
{
  LOG(PR_LOG_DEBUG, ("%p IA::AsyncWait aCallback=%p aFlags=%u aRequestedCount=%u aTarget=%p",
                     this, aCallback, aFlags, aRequestedCount, aTarget));

  if (aFlags != 0) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mCallback || mCallbackTarget) {
    return NS_ERROR_UNEXPECTED;
  }

  mCallback = aCallback;
  mCallbackTarget = aTarget;
  mNotifyThreshold = aRequestedCount;
  if (!aRequestedCount) {
    mNotifyThreshold = 1024;
  }

  NotifyListener();

  return NS_OK;
}

void
MediaSourceInputAdapter::NotifyListener()
{
  if (!mCallback) {
    return;
  }
  // Don't notify unless more data is available than the threshold, except
  // in the case that there's no more data coming.
  if (Available() < mNotifyThreshold && !mClosed && !mMediaSource->AppendDone()) {
    return;
  }
  nsCOMPtr<nsIInputStreamCallback> callback;
  if (mCallbackTarget) {
    callback = NS_NewInputStreamReadyEvent(mCallback, mCallbackTarget);
  } else {
    callback = mCallback;
  }
  MOZ_ASSERT(callback);
  mCallback = nullptr;
  mCallbackTarget = nullptr;
  mNotifyThreshold = 0;
  LOG(PR_LOG_DEBUG, ("%p IA::NotifyListener", this));
  callback->OnInputStreamReady(this);

}

uint64_t
MediaSourceInputAdapter::Available()
{
  return mMediaSource->GetData().Length() - mOffset;
}

MediaSourceInputAdapter::~MediaSourceInputAdapter()
{
  LOG(PR_LOG_DEBUG, ("%p Destroy input adapter", this));
}

MediaSourceInputAdapter::MediaSourceInputAdapter(MediaSource* aMediaSource)
  : mMediaSource(aMediaSource)
  , mOffset(0)
  , mClosed(false)
{
  LOG(PR_LOG_DEBUG, ("%p Create input adapter for %p", this, aMediaSource));
}

NS_IMPL_CYCLE_COLLECTION_1(MediaSourceInputAdapter, mMediaSource)

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaSourceInputAdapter)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaSourceInputAdapter)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaSourceInputAdapter)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncInputStream)
NS_INTERFACE_MAP_END

} // namespace dom
} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncTransactionTracker.h"

#include "mozilla/layers/ImageBridgeChild.h"  // for ImageBridgeChild

namespace mozilla {
namespace layers {

void
AsyncTransactionWaiter::WaitComplete()
{
  MOZ_ASSERT(!InImageBridgeChildThread());

  MonitorAutoLock mon(mCompletedMonitor);
  int count = 0;
  const int maxCount = 5;
  while (mWaitCount > 0 && (count < maxCount)) {
    if (!NS_SUCCEEDED(mCompletedMonitor.Wait(PR_MillisecondsToInterval(10000)))) {
      NS_WARNING("Failed to wait Monitor");
      return;
    }
    if (count > 1) {
      printf_stderr("Waiting async transaction complete.\n");
    }
    count++;
  }

  if (mWaitCount > 0) {
    printf_stderr("Timeout of waiting transaction complete.");
  }

  if (count == maxCount) {
    gfxDevCrash(LogReason::AsyncTransactionTimeout) << "Bug 1244883: AsyncTransactionWaiter timed out.";
  }
}

Atomic<uint64_t> AsyncTransactionTracker::sSerialCounter(0);

AsyncTransactionTracker::AsyncTransactionTracker(AsyncTransactionWaiter* aWaiter)
    : mSerial(GetNextSerial())
    , mWaiter(aWaiter)
#ifdef DEBUG
    , mCompleted(false)
#endif
{
  if (mWaiter) {
    mWaiter->IncrementWaitCount();
  }
}

AsyncTransactionTracker::~AsyncTransactionTracker()
{
}

void
AsyncTransactionTracker::NotifyComplete()
{
  MOZ_ASSERT(!mCompleted);
#ifdef DEBUG
  mCompleted = true;
#endif
  Complete();
  if (mWaiter) {
    mWaiter->DecrementWaitCount();
  }
}

void
AsyncTransactionTracker::NotifyCancel()
{
  MOZ_ASSERT(!mCompleted);
#ifdef DEBUG
  mCompleted = true;
#endif
  Cancel();
  if (mWaiter) {
    mWaiter->DecrementWaitCount();
  }
}

Atomic<uint64_t> AsyncTransactionTrackersHolder::sSerialCounter(0);

AsyncTransactionTrackersHolder::AsyncTransactionTrackersHolder()
  : mSerial(GetNextSerial())
  , mIsTrackersHolderDestroyed(false)
{
  MOZ_COUNT_CTOR(AsyncTransactionTrackersHolder);
}

AsyncTransactionTrackersHolder::~AsyncTransactionTrackersHolder()
{
  if (!mIsTrackersHolderDestroyed) {
    DestroyAsyncTransactionTrackersHolder();
  }
  MOZ_COUNT_DTOR(AsyncTransactionTrackersHolder);
}

void
AsyncTransactionTrackersHolder::HoldUntilComplete(AsyncTransactionTracker* aTransactionTracker)
{
  if (!aTransactionTracker) {
    return;
  }

  if (mIsTrackersHolderDestroyed && aTransactionTracker) {
    aTransactionTracker->NotifyComplete();
    return;
  }

  if (aTransactionTracker) {
    mAsyncTransactionTrackers[aTransactionTracker->GetId()] = aTransactionTracker;
  }
}

void
AsyncTransactionTrackersHolder::TransactionCompleteted(uint64_t aTransactionId)
{
  TransactionCompletetedInternal(aTransactionId);
}

void
AsyncTransactionTrackersHolder::TransactionCompletetedInternal(uint64_t aTransactionId)
{
  std::map<uint64_t, RefPtr<AsyncTransactionTracker> >::iterator it
    = mAsyncTransactionTrackers.find(aTransactionId);
  if (it != mAsyncTransactionTrackers.end()) {
    it->second->NotifyComplete();
    mAsyncTransactionTrackers.erase(it);
  }
}

void
AsyncTransactionTrackersHolder::SetReleaseFenceHandle(FenceHandle& aReleaseFenceHandle,
                                                      uint64_t aTransactionId)
{
  std::map<uint64_t, RefPtr<AsyncTransactionTracker> >::iterator it
    = mAsyncTransactionTrackers.find(aTransactionId);
  if (it != mAsyncTransactionTrackers.end()) {
    it->second->SetReleaseFenceHandle(aReleaseFenceHandle);
  }
}

void
AsyncTransactionTrackersHolder::ClearAllAsyncTransactionTrackers()
{
  std::map<uint64_t, RefPtr<AsyncTransactionTracker> >::iterator it;
  for (it = mAsyncTransactionTrackers.begin();
       it != mAsyncTransactionTrackers.end(); it++) {
    it->second->NotifyCancel();
  }
  mAsyncTransactionTrackers.clear();
}

void
AsyncTransactionTrackersHolder::DestroyAsyncTransactionTrackersHolder() {
  mIsTrackersHolderDestroyed = true;
  ClearAllAsyncTransactionTrackers();
}


} // namespace layers
} // namespace mozilla

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

uint64_t AsyncTransactionTracker::sSerialCounter(0);
Mutex* AsyncTransactionTracker::sLock = nullptr;

AsyncTransactionTracker::AsyncTransactionTracker()
    : mSerial(GetNextSerial())
    , mCompletedMonitor("AsyncTransactionTracker.mCompleted")
    , mCompleted(false)
{
}

AsyncTransactionTracker::~AsyncTransactionTracker()
{
}

void
AsyncTransactionTracker::WaitComplete()
{
  MOZ_ASSERT(!InImageBridgeChildThread());

  MonitorAutoLock mon(mCompletedMonitor);
  int count = 0;
  while (!mCompleted) {
    if (!NS_SUCCEEDED(mCompletedMonitor.Wait(PR_MillisecondsToInterval(10000)))) {
      NS_WARNING("Failed to wait Monitor");
      return;
    }
    if (count > 1) {
      NS_WARNING("Waiting async transaction complete.");
    }
    count++;
  }
}

void
AsyncTransactionTracker::NotifyComplete()
{
  MonitorAutoLock mon(mCompletedMonitor);
  MOZ_ASSERT(!mCompleted);
  mCompleted = true;
  Complete();
  mCompletedMonitor.Notify();
}

void
AsyncTransactionTracker::NotifyCancel()
{
  MonitorAutoLock mon(mCompletedMonitor);
  MOZ_ASSERT(!mCompleted);
  mCompleted = true;
  Cancel();
  mCompletedMonitor.Notify();
}

AsyncTransactionTrackersHolder::AsyncTransactionTrackersHolder()
  : mIsTrackersHolderDestroyed(false)
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
    mAsyncTransactionTrackeres[aTransactionTracker->GetId()] = aTransactionTracker;
  }
}

void
AsyncTransactionTrackersHolder::TransactionCompleteted(uint64_t aTransactionId)
{
  std::map<uint64_t, RefPtr<AsyncTransactionTracker> >::iterator it
    = mAsyncTransactionTrackeres.find(aTransactionId);
  if (it != mAsyncTransactionTrackeres.end()) {
    it->second->NotifyCancel();
    mAsyncTransactionTrackeres.erase(it);
  }
}

void
AsyncTransactionTrackersHolder::ClearAllAsyncTransactionTrackers()
{
  std::map<uint64_t, RefPtr<AsyncTransactionTracker> >::iterator it;
  for (it = mAsyncTransactionTrackeres.begin();
       it != mAsyncTransactionTrackeres.end(); it++) {
    it->second->NotifyCancel();
  }
  mAsyncTransactionTrackeres.clear();
}

void
AsyncTransactionTrackersHolder::DestroyAsyncTransactionTrackersHolder() {
  mIsTrackersHolderDestroyed = true;
  ClearAllAsyncTransactionTrackers();
}


} // namespace layers
} // namespace mozilla

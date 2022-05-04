/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZTaskRunnable.h"

#include "mozilla/PresShell.h"
#include "nsRefreshDriver.h"

namespace mozilla::layers {

NS_IMETHODIMP
APZTaskRunnable::Run() {
  if (!mController) {
    mRegisteredPresShellId = 0;
    return NS_OK;
  }

  // Move these variables first since below RequestContentPaint and
  // NotifyFlushComplete might spin event loop so that any new incoming requests
  // will be properly queued and run in the next refresh driver's tick.
  const bool needsFlushCompleteNotification = mNeedsFlushCompleteNotification;
  auto requests = std::move(mPendingRepaintRequestQueue);
  mPendingRepaintRequestMap.clear();
  mNeedsFlushCompleteNotification = false;
  mRegisteredPresShellId = 0;
  RefPtr<GeckoContentController> controller = mController;

  // We need to process pending RepaintRequests first.
  while (!requests.empty()) {
    controller->RequestContentRepaint(requests.front());
    requests.pop_front();
  }

  if (needsFlushCompleteNotification) {
    // Then notify "apz-repaints-flushed" so that we can ensure that all pending
    // scroll position updates have finished when the "apz-repaints-flushed"
    // arrives.
    controller->NotifyFlushComplete();
  }

  return NS_OK;
}

void APZTaskRunnable::QueueRequest(const RepaintRequest& aRequest) {
  // If we are in test-controlled refreshes mode, process this |aRequest|
  // synchronously.
  if (IsTestControllingRefreshesEnabled()) {
    // Flush all pending requests and notification just in case the refresh
    // driver mode was changed before flushing them.
    RefPtr<GeckoContentController> controller = mController;
    Run();
    controller->RequestContentRepaint(aRequest);
    return;
  }
  EnsureRegisterAsEarlyRunner();

  RepaintRequestKey key{aRequest.GetScrollId(), aRequest.GetScrollUpdateType()};

  auto lastDiscardableRequest = mPendingRepaintRequestMap.find(key);
  // If there's an existing request with the same key, we can discard it and we
  // push the incoming one into the queue's tail so that we can ensure the order
  // of processing requests.
  if (lastDiscardableRequest != mPendingRepaintRequestMap.end()) {
    for (auto it = mPendingRepaintRequestQueue.begin();
         it != mPendingRepaintRequestQueue.end(); it++) {
      if (RepaintRequestKey{it->GetScrollId(), it->GetScrollUpdateType()} ==
          key) {
        mPendingRepaintRequestQueue.erase(it);
        break;
      }
    }
  }
  mPendingRepaintRequestMap.insert(key);
  mPendingRepaintRequestQueue.push_back(aRequest);
}

void APZTaskRunnable::QueueFlushCompleteNotification() {
  // If we are in test-controlled refreshes mode, notify apz-repaints-flushed
  // synchronously.
  if (IsTestControllingRefreshesEnabled()) {
    // Flush all pending requests and notification just in case the refresh
    // driver mode was changed before flushing them.
    RefPtr<GeckoContentController> controller = mController;
    Run();
    controller->NotifyFlushComplete();
    return;
  }

  EnsureRegisterAsEarlyRunner();

  mNeedsFlushCompleteNotification = true;
}

bool APZTaskRunnable::IsRegisteredWithCurrentPresShell() const {
  MOZ_ASSERT(mController);

  uint32_t current = 0;
  if (PresShell* presShell = mController->GetTopLevelPresShell()) {
    current = presShell->GetPresShellId();
  }
  return mRegisteredPresShellId == current;
}

void APZTaskRunnable::EnsureRegisterAsEarlyRunner() {
  if (IsRegisteredWithCurrentPresShell()) {
    return;
  }

  // If the registered presshell id has been changed, we need to discard pending
  // requests and notification since all of them are for documents which
  // have been torn down.
  if (mRegisteredPresShellId) {
    mPendingRepaintRequestMap.clear();
    mPendingRepaintRequestQueue.clear();
    mNeedsFlushCompleteNotification = false;
  }

  if (PresShell* presShell = mController->GetTopLevelPresShell()) {
    if (nsRefreshDriver* driver = presShell->GetRefreshDriver()) {
      driver->AddEarlyRunner(this);
      mRegisteredPresShellId = presShell->GetPresShellId();
    }
  }
}

bool APZTaskRunnable::IsTestControllingRefreshesEnabled() const {
  if (!mController) {
    return false;
  }

  if (PresShell* presShell = mController->GetTopLevelPresShell()) {
    if (nsRefreshDriver* driver = presShell->GetRefreshDriver()) {
      return driver->IsTestControllingRefreshesEnabled();
    }
  }
  return false;
}

}  // namespace mozilla::layers

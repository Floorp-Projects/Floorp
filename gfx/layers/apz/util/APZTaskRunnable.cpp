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
  RepaintRequests requests = std::move(mPendingRepaintRequests);
  mNeedsFlushCompleteNotification = false;
  mRegisteredPresShellId = 0;

  // We need to process pending RepaintRequests first.
  for (const auto& [viewId, request] : requests) {
    (void)viewId;
    mController->RequestContentRepaint(request);
    if (!mController) {
      return NS_OK;
    }
  }

  if (needsFlushCompleteNotification) {
    // Then notify "apz-repaints-flushed" so that we can ensure that all pending
    // scroll position updates have finished when the "apz-repaints-flushed"
    // arrives.
    RefPtr<GeckoContentController> controller = mController;
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
    Run();
    if (mController) {
      mController->RequestContentRepaint(aRequest);
    }
    return;
  }
  EnsureRegisterAsEarlyRunner();

  mPendingRepaintRequests[aRequest.GetScrollId()] = aRequest;
}

void APZTaskRunnable::QueueFlushCompleteNotification() {
  // If we are in test-controlled refreshes mode, notify apz-repaints-flushed
  // synchronously.
  if (IsTestControllingRefreshesEnabled()) {
    // Flush all pending requests and notification just in case the refresh
    // driver mode was changed before flushing them.
    Run();
    if (mController) {
      RefPtr<GeckoContentController> controller = mController;
      controller->NotifyFlushComplete();
    }
    return;
  }

  EnsureRegisterAsEarlyRunner();

  mNeedsFlushCompleteNotification = true;
}

bool APZTaskRunnable::IsRegistereddWithCurrentPresShell() const {
  MOZ_ASSERT(mController);

  uint32_t current = 0;
  if (PresShell* presShell = mController->GetTopLevelPresShell()) {
    current = presShell->GetPresShellId();
  }
  return mRegisteredPresShellId == current;
}

void APZTaskRunnable::EnsureRegisterAsEarlyRunner() {
  if (IsRegistereddWithCurrentPresShell()) {
    return;
  }

  // If the registered presshell id has been changed, we need to discard pending
  // requests and notification since all of them are for documents which
  // have been torn down.
  if (mRegisteredPresShellId) {
    mPendingRepaintRequests.clear();
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

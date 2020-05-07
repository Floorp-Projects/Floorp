/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentProcessController.h"

#include "mozilla/PresShell.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZChild.h"
#include "nsIContentInlines.h"

#include "InputData.h"  // for InputData

namespace mozilla {
namespace layers {

ContentProcessController::ContentProcessController(
    const RefPtr<dom::BrowserChild>& aBrowser)
    : mBrowser(aBrowser) {
  MOZ_ASSERT(mBrowser);
}

void ContentProcessController::NotifyLayerTransforms(
    const nsTArray<MatrixMessage>& aTransforms) {
  // This should never get called
  MOZ_ASSERT(false);
}

void ContentProcessController::RequestContentRepaint(
    const RepaintRequest& aRequest) {
  if (mBrowser) {
    mBrowser->UpdateFrame(aRequest);
  }
}

void ContentProcessController::HandleTap(TapType aType,
                                         const LayoutDevicePoint& aPoint,
                                         Modifiers aModifiers,
                                         const ScrollableLayerGuid& aGuid,
                                         uint64_t aInputBlockId) {
  // This should never get called
  MOZ_ASSERT(false);
}

void ContentProcessController::NotifyPinchGesture(
    PinchGestureInput::PinchGestureType aType, const ScrollableLayerGuid& aGuid,
    LayoutDeviceCoord aSpanChange, Modifiers aModifiers) {
  // This should never get called
  MOZ_ASSERT_UNREACHABLE("Unexpected message to content process");
}

void ContentProcessController::NotifyAPZStateChange(
    const ScrollableLayerGuid& aGuid, APZStateChange aChange, int aArg) {
  if (mBrowser) {
    mBrowser->NotifyAPZStateChange(aGuid.mScrollId, aChange, aArg);
  }
}

void ContentProcessController::NotifyMozMouseScrollEvent(
    const ScrollableLayerGuid::ViewID& aScrollId, const nsString& aEvent) {
  if (mBrowser) {
    APZCCallbackHelper::NotifyMozMouseScrollEvent(aScrollId, aEvent);
  }
}

void ContentProcessController::NotifyFlushComplete() {
  if (mBrowser) {
    RefPtr<PresShell> presShell = mBrowser->GetTopLevelPresShell();
    APZCCallbackHelper::NotifyFlushComplete(presShell);
  }
}

void ContentProcessController::NotifyAsyncScrollbarDragInitiated(
    uint64_t aDragBlockId, const ScrollableLayerGuid::ViewID& aScrollId,
    ScrollDirection aDirection) {
  APZCCallbackHelper::NotifyAsyncScrollbarDragInitiated(aDragBlockId, aScrollId,
                                                        aDirection);
}

void ContentProcessController::NotifyAsyncScrollbarDragRejected(
    const ScrollableLayerGuid::ViewID& aScrollId) {
  APZCCallbackHelper::NotifyAsyncScrollbarDragRejected(aScrollId);
}

void ContentProcessController::NotifyAsyncAutoscrollRejected(
    const ScrollableLayerGuid::ViewID& aScrollId) {
  APZCCallbackHelper::NotifyAsyncAutoscrollRejected(aScrollId);
}

void ContentProcessController::CancelAutoscroll(
    const ScrollableLayerGuid& aGuid) {
  // This should never get called
  MOZ_ASSERT_UNREACHABLE("Unexpected message to content process");
}

bool ContentProcessController::IsRepaintThread() { return NS_IsMainThread(); }

void ContentProcessController::DispatchToRepaintThread(
    already_AddRefed<Runnable> aTask) {
  NS_DispatchToMainThread(std::move(aTask));
}

}  // namespace layers
}  // namespace mozilla

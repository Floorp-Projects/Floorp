/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentProcessController.h"

#include "mozilla/dom/TabChild.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZChild.h"

#include "InputData.h"                  // for InputData

namespace mozilla {
namespace layers {

ContentProcessController::ContentProcessController(const RefPtr<dom::TabChild>& aBrowser)
    : mBrowser(aBrowser)
{
  MOZ_ASSERT(mBrowser);
}

void
ContentProcessController::RequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
  if (mBrowser) {
    mBrowser->UpdateFrame(aFrameMetrics);
  }
}

void
ContentProcessController::HandleTap(
                        TapType aType,
                        const LayoutDevicePoint& aPoint,
                        Modifiers aModifiers,
                        const ScrollableLayerGuid& aGuid,
                        uint64_t aInputBlockId)
{
  // This should never get called
  MOZ_ASSERT(false);
}

void
ContentProcessController::NotifyPinchGesture(
                        PinchGestureInput::PinchGestureType aType,
                        const ScrollableLayerGuid& aGuid,
                        LayoutDeviceCoord aSpanChange,
                        Modifiers aModifiers)
{
  // This should never get called
  MOZ_ASSERT_UNREACHABLE("Unexpected message to content process");
}

void
ContentProcessController::NotifyAPZStateChange(
                                  const ScrollableLayerGuid& aGuid,
                                  APZStateChange aChange,
                                  int aArg)
{
  if (mBrowser) {
    mBrowser->NotifyAPZStateChange(aGuid.mScrollId, aChange, aArg);
  }
}

void
ContentProcessController::NotifyMozMouseScrollEvent(
                                  const FrameMetrics::ViewID& aScrollId,
                                  const nsString& aEvent)
{
  if (mBrowser) {
    APZCCallbackHelper::NotifyMozMouseScrollEvent(aScrollId, aEvent);
  }
}

void
ContentProcessController::NotifyFlushComplete()
{
  if (mBrowser) {
    nsCOMPtr<nsIPresShell> shell;
    if (nsCOMPtr<nsIDocument> doc = mBrowser->GetDocument()) {
      shell = doc->GetShell();
    }
    APZCCallbackHelper::NotifyFlushComplete(shell.get());
  }
}

void
ContentProcessController::NotifyAsyncScrollbarDragRejected(const FrameMetrics::ViewID& aScrollId)
{
  APZCCallbackHelper::NotifyAsyncScrollbarDragRejected(aScrollId);
}

void
ContentProcessController::PostDelayedTask(already_AddRefed<Runnable> aRunnable, int aDelayMs)
{
  MOZ_ASSERT_UNREACHABLE("ContentProcessController should only be used remotely.");
}

bool
ContentProcessController::IsRepaintThread()
{
  return NS_IsMainThread();
}

void
ContentProcessController::DispatchToRepaintThread(already_AddRefed<Runnable> aTask)
{
  NS_DispatchToMainThread(Move(aTask));
}

} // namespace layers
} // namespace mozilla

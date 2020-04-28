/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeProcessController.h"

#include "MainThreadUtils.h"    // for NS_IsMainThread()
#include "base/message_loop.h"  // for MessageLoop
#include "mozilla/PresShell.h"
#include "mozilla/dom/Element.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/DoubleTapToZoom.h"
#include "mozilla/dom/Document.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsLayoutUtils.h"
#include "nsView.h"

static mozilla::LazyLogModule sApzChromeLog("apz.cc.chrome");

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::widget;

ChromeProcessController::ChromeProcessController(
    nsIWidget* aWidget, APZEventState* aAPZEventState,
    IAPZCTreeManager* aAPZCTreeManager)
    : mWidget(aWidget),
      mAPZEventState(aAPZEventState),
      mAPZCTreeManager(aAPZCTreeManager),
      mUILoop(MessageLoop::current()) {
  // Otherwise we're initializing mUILoop incorrectly.
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aAPZEventState);
  MOZ_ASSERT(aAPZCTreeManager);

  mUILoop->PostTask(
      NewRunnableMethod("layers::ChromeProcessController::InitializeRoot", this,
                        &ChromeProcessController::InitializeRoot));
}

ChromeProcessController::~ChromeProcessController() = default;

void ChromeProcessController::InitializeRoot() {
  APZCCallbackHelper::InitializeRootDisplayport(GetPresShell());
}

void ChromeProcessController::NotifyLayerTransforms(
    const nsTArray<MatrixMessage>& aTransforms) {
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(NewRunnableMethod<nsTArray<MatrixMessage>>(
        "layers::ChromeProcessController::NotifyLayerTransforms", this,
        &ChromeProcessController::NotifyLayerTransforms, aTransforms));
    return;
  }

  APZCCallbackHelper::NotifyLayerTransforms(aTransforms);
}

void ChromeProcessController::RequestContentRepaint(
    const RepaintRequest& aRequest) {
  MOZ_ASSERT(IsRepaintThread());

  if (aRequest.IsRootContent()) {
    APZCCallbackHelper::UpdateRootFrame(aRequest);
  } else {
    APZCCallbackHelper::UpdateSubFrame(aRequest);
  }
}

void ChromeProcessController::PostDelayedTask(already_AddRefed<Runnable> aTask,
                                              int aDelayMs) {
  MessageLoop::current()->PostDelayedTask(std::move(aTask), aDelayMs);
}

bool ChromeProcessController::IsRepaintThread() { return NS_IsMainThread(); }

void ChromeProcessController::DispatchToRepaintThread(
    already_AddRefed<Runnable> aTask) {
  NS_DispatchToMainThread(std::move(aTask));
}

void ChromeProcessController::Destroy() {
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(
        NewRunnableMethod("layers::ChromeProcessController::Destroy", this,
                          &ChromeProcessController::Destroy));
    return;
  }

  MOZ_ASSERT(MessageLoop::current() == mUILoop);
  mWidget = nullptr;
  mAPZEventState = nullptr;
}

PresShell* ChromeProcessController::GetPresShell() const {
  if (!mWidget) {
    return nullptr;
  }
  if (nsView* view = nsView::GetViewFor(mWidget)) {
    return view->GetPresShell();
  }
  return nullptr;
}

dom::Document* ChromeProcessController::GetRootDocument() const {
  if (PresShell* presShell = GetPresShell()) {
    return presShell->GetDocument();
  }
  return nullptr;
}

dom::Document* ChromeProcessController::GetRootContentDocument(
    const ScrollableLayerGuid::ViewID& aScrollId) const {
  nsIContent* content = nsLayoutUtils::FindContentFor(aScrollId);
  if (!content) {
    return nullptr;
  }
  if (PresShell* presShell =
          APZCCallbackHelper::GetRootContentDocumentPresShellForContent(
              content)) {
    return presShell->GetDocument();
  }
  return nullptr;
}

void ChromeProcessController::HandleDoubleTap(
    const mozilla::CSSPoint& aPoint, Modifiers aModifiers,
    const ScrollableLayerGuid& aGuid) {
  MOZ_ASSERT(MessageLoop::current() == mUILoop);

  RefPtr<dom::Document> document = GetRootContentDocument(aGuid.mScrollId);
  if (!document.get()) {
    return;
  }

  // CalculateRectToZoomTo performs a hit test on the frame associated with the
  // Root Content Document. Unfortunately that frame does not know about the
  // resolution of the document and so we must remove it before calculating
  // the zoomToRect.
  PresShell* presShell = document->GetPresShell();
  const float resolution = presShell->GetResolution();
  CSSPoint point(aPoint.x / resolution, aPoint.y / resolution);
  CSSRect zoomToRect = CalculateRectToZoomTo(document, point);

  uint32_t presShellId;
  ScrollableLayerGuid::ViewID viewId;
  if (APZCCallbackHelper::GetOrCreateScrollIdentifiers(
          document->GetDocumentElement(), &presShellId, &viewId)) {
    APZThreadUtils::RunOnControllerThread(
        NewRunnableMethod<ScrollableLayerGuid, CSSRect, uint32_t>(
            "IAPZCTreeManager::ZoomToRect", mAPZCTreeManager,
            &IAPZCTreeManager::ZoomToRect,
            ScrollableLayerGuid(aGuid.mLayersId, presShellId, viewId),
            zoomToRect, ZoomToRectBehavior::DEFAULT_BEHAVIOR));
  }
}

void ChromeProcessController::HandleTap(
    TapType aType, const mozilla::LayoutDevicePoint& aPoint,
    Modifiers aModifiers, const ScrollableLayerGuid& aGuid,
    uint64_t aInputBlockId) {
  MOZ_LOG(sApzChromeLog, LogLevel::Debug,
          ("HandleTap called with %d\n", (int)aType));
  if (MessageLoop::current() != mUILoop) {
    MOZ_LOG(sApzChromeLog, LogLevel::Debug, ("HandleTap redispatching\n"));
    mUILoop->PostTask(
        NewRunnableMethod<TapType, mozilla::LayoutDevicePoint, Modifiers,
                          ScrollableLayerGuid, uint64_t>(
            "layers::ChromeProcessController::HandleTap", this,
            &ChromeProcessController::HandleTap, aType, aPoint, aModifiers,
            aGuid, aInputBlockId));
    return;
  }

  if (!mAPZEventState) {
    return;
  }

  RefPtr<PresShell> presShell = GetPresShell();
  if (!presShell) {
    return;
  }
  if (!presShell->GetPresContext()) {
    return;
  }
  CSSToLayoutDeviceScale scale(
      presShell->GetPresContext()->CSSToDevPixelScale());
  CSSPoint point =
      APZCCallbackHelper::ApplyCallbackTransform(aPoint / scale, aGuid);

  switch (aType) {
    case TapType::eSingleTap:
      mAPZEventState->ProcessSingleTap(point, scale, aModifiers, 1);
      break;
    case TapType::eDoubleTap:
      HandleDoubleTap(point, aModifiers, aGuid);
      break;
    case TapType::eSecondTap:
      mAPZEventState->ProcessSingleTap(point, scale, aModifiers, 2);
      break;
    case TapType::eLongTap: {
      RefPtr<APZEventState> eventState(mAPZEventState);
      eventState->ProcessLongTap(presShell, point, scale, aModifiers,
                                 aInputBlockId);
      break;
    }
    case TapType::eLongTapUp: {
      RefPtr<APZEventState> eventState(mAPZEventState);
      eventState->ProcessLongTapUp(presShell, point, scale, aModifiers);
      break;
    }
  }
}

void ChromeProcessController::NotifyPinchGesture(
    PinchGestureInput::PinchGestureType aType, const ScrollableLayerGuid& aGuid,
    LayoutDeviceCoord aSpanChange, Modifiers aModifiers) {
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(
        NewRunnableMethod<PinchGestureInput::PinchGestureType,
                          ScrollableLayerGuid, LayoutDeviceCoord, Modifiers>(
            "layers::ChromeProcessController::NotifyPinchGesture", this,
            &ChromeProcessController::NotifyPinchGesture, aType, aGuid,
            aSpanChange, aModifiers));
    return;
  }

  if (mWidget) {
    APZCCallbackHelper::NotifyPinchGesture(aType, aSpanChange, aModifiers,
                                           mWidget.get());
  }
}

void ChromeProcessController::NotifyAPZStateChange(
    const ScrollableLayerGuid& aGuid, APZStateChange aChange, int aArg) {
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(
        NewRunnableMethod<ScrollableLayerGuid, APZStateChange, int>(
            "layers::ChromeProcessController::NotifyAPZStateChange", this,
            &ChromeProcessController::NotifyAPZStateChange, aGuid, aChange,
            aArg));
    return;
  }

  if (!mAPZEventState) {
    return;
  }

  mAPZEventState->ProcessAPZStateChange(aGuid.mScrollId, aChange, aArg);
}

void ChromeProcessController::NotifyMozMouseScrollEvent(
    const ScrollableLayerGuid::ViewID& aScrollId, const nsString& aEvent) {
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(NewRunnableMethod<ScrollableLayerGuid::ViewID, nsString>(
        "layers::ChromeProcessController::NotifyMozMouseScrollEvent", this,
        &ChromeProcessController::NotifyMozMouseScrollEvent, aScrollId,
        aEvent));
    return;
  }

  APZCCallbackHelper::NotifyMozMouseScrollEvent(aScrollId, aEvent);
}

void ChromeProcessController::NotifyFlushComplete() {
  MOZ_ASSERT(IsRepaintThread());

  APZCCallbackHelper::NotifyFlushComplete(GetPresShell());
}

void ChromeProcessController::NotifyAsyncScrollbarDragInitiated(
    uint64_t aDragBlockId, const ScrollableLayerGuid::ViewID& aScrollId,
    ScrollDirection aDirection) {
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(NewRunnableMethod<uint64_t, ScrollableLayerGuid::ViewID,
                                        ScrollDirection>(
        "layers::ChromeProcessController::NotifyAsyncScrollbarDragInitiated",
        this, &ChromeProcessController::NotifyAsyncScrollbarDragInitiated,
        aDragBlockId, aScrollId, aDirection));
    return;
  }

  APZCCallbackHelper::NotifyAsyncScrollbarDragInitiated(aDragBlockId, aScrollId,
                                                        aDirection);
}

void ChromeProcessController::NotifyAsyncScrollbarDragRejected(
    const ScrollableLayerGuid::ViewID& aScrollId) {
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(NewRunnableMethod<ScrollableLayerGuid::ViewID>(
        "layers::ChromeProcessController::NotifyAsyncScrollbarDragRejected",
        this, &ChromeProcessController::NotifyAsyncScrollbarDragRejected,
        aScrollId));
    return;
  }

  APZCCallbackHelper::NotifyAsyncScrollbarDragRejected(aScrollId);
}

void ChromeProcessController::NotifyAsyncAutoscrollRejected(
    const ScrollableLayerGuid::ViewID& aScrollId) {
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(NewRunnableMethod<ScrollableLayerGuid::ViewID>(
        "layers::ChromeProcessController::NotifyAsyncAutoscrollRejected", this,
        &ChromeProcessController::NotifyAsyncAutoscrollRejected, aScrollId));
    return;
  }

  APZCCallbackHelper::NotifyAsyncAutoscrollRejected(aScrollId);
}

void ChromeProcessController::CancelAutoscroll(
    const ScrollableLayerGuid& aGuid) {
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(NewRunnableMethod<ScrollableLayerGuid>(
        "layers::ChromeProcessController::CancelAutoscroll", this,
        &ChromeProcessController::CancelAutoscroll, aGuid));
    return;
  }

  APZCCallbackHelper::CancelAutoscroll(aGuid.mScrollId);
}

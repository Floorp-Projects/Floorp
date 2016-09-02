/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeProcessController.h"

#include "MainThreadUtils.h"    // for NS_IsMainThread()
#include "base/message_loop.h"  // for MessageLoop
#include "mozilla/dom/Element.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/DoubleTapToZoom.h"
#include "nsIDocument.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPresShell.h"
#include "nsLayoutUtils.h"
#include "nsView.h"

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::widget;

ChromeProcessController::ChromeProcessController(nsIWidget* aWidget,
                                                 APZEventState* aAPZEventState,
                                                 IAPZCTreeManager* aAPZCTreeManager)
  : mWidget(aWidget)
  , mAPZEventState(aAPZEventState)
  , mAPZCTreeManager(aAPZCTreeManager)
  , mUILoop(MessageLoop::current())
{
  // Otherwise we're initializing mUILoop incorrectly.
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aAPZEventState);
  MOZ_ASSERT(aAPZCTreeManager);

  mUILoop->PostTask(NewRunnableMethod(this, &ChromeProcessController::InitializeRoot));
}

ChromeProcessController::~ChromeProcessController() {}

void
ChromeProcessController::InitializeRoot()
{
  APZCCallbackHelper::InitializeRootDisplayport(GetPresShell());
}

void
ChromeProcessController::RequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
  MOZ_ASSERT(IsRepaintThread());

  FrameMetrics metrics = aFrameMetrics;
  if (metrics.IsRootContent()) {
    APZCCallbackHelper::UpdateRootFrame(metrics);
  } else {
    APZCCallbackHelper::UpdateSubFrame(metrics);
  }
}

void
ChromeProcessController::PostDelayedTask(already_AddRefed<Runnable> aTask, int aDelayMs)
{
  MessageLoop::current()->PostDelayedTask(Move(aTask), aDelayMs);
}

bool
ChromeProcessController::IsRepaintThread()
{
  return NS_IsMainThread();
}

void
ChromeProcessController::DispatchToRepaintThread(already_AddRefed<Runnable> aTask)
{
  NS_DispatchToMainThread(Move(aTask));
}

void
ChromeProcessController::Destroy()
{
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(NewRunnableMethod(this, &ChromeProcessController::Destroy));
    return;
  }

  MOZ_ASSERT(MessageLoop::current() == mUILoop);
  mWidget = nullptr;
  mAPZEventState = nullptr;
}

nsIPresShell*
ChromeProcessController::GetPresShell() const
{
  if (!mWidget) {
    return nullptr;
  }
  if (nsView* view = nsView::GetViewFor(mWidget)) {
    return view->GetPresShell();
  }
  return nullptr;
}

nsIDocument*
ChromeProcessController::GetRootDocument() const
{
  if (nsIPresShell* presShell = GetPresShell()) {
    return presShell->GetDocument();
  }
  return nullptr;
}

nsIDocument*
ChromeProcessController::GetRootContentDocument(const FrameMetrics::ViewID& aScrollId) const
{
  nsIContent* content = nsLayoutUtils::FindContentFor(aScrollId);
  if (!content) {
    return nullptr;
  }
  nsIPresShell* presShell = APZCCallbackHelper::GetRootContentDocumentPresShellForContent(content);
  if (presShell) {
    return presShell->GetDocument();
  }
  return nullptr;
}

void
ChromeProcessController::HandleDoubleTap(const mozilla::CSSPoint& aPoint,
                                         Modifiers aModifiers,
                                         const ScrollableLayerGuid& aGuid)
{
  MOZ_ASSERT(MessageLoop::current() == mUILoop);

  nsCOMPtr<nsIDocument> document = GetRootContentDocument(aGuid.mScrollId);
  if (!document.get()) {
    return;
  }

  // CalculateRectToZoomTo performs a hit test on the frame associated with the
  // Root Content Document. Unfortunately that frame does not know about the
  // resolution of the document and so we must remove it before calculating
  // the zoomToRect.
  nsIPresShell* presShell = document->GetShell();
  const float resolution = presShell->ScaleToResolution() ? presShell->GetResolution () : 1.0f;
  CSSPoint point(aPoint.x / resolution, aPoint.y / resolution);
  CSSRect zoomToRect = CalculateRectToZoomTo(document, point);

  uint32_t presShellId;
  FrameMetrics::ViewID viewId;
  if (APZCCallbackHelper::GetOrCreateScrollIdentifiers(
      document->GetDocumentElement(), &presShellId, &viewId)) {
    mAPZCTreeManager->ZoomToRect(
      ScrollableLayerGuid(aGuid.mLayersId, presShellId, viewId), zoomToRect);
  }
}

void
ChromeProcessController::HandleTap(TapType aType,
                                   const mozilla::LayoutDevicePoint& aPoint,
                                   Modifiers aModifiers,
                                   const ScrollableLayerGuid& aGuid,
                                   uint64_t aInputBlockId)
{
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(NewRunnableMethod<TapType, mozilla::LayoutDevicePoint, Modifiers,
                                        ScrollableLayerGuid, uint64_t>(this,
                        &ChromeProcessController::HandleTap,
                        aType, aPoint, aModifiers, aGuid, aInputBlockId));
    return;
  }

  if (!mAPZEventState) {
    return;
  }

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (!presShell) {
    return;
  }
  if (!presShell->GetPresContext()) {
    return;
  }
  CSSToLayoutDeviceScale scale(presShell->GetPresContext()->CSSToDevPixelScale());
  CSSPoint point = APZCCallbackHelper::ApplyCallbackTransform(aPoint / scale, aGuid);

  switch (aType) {
  case TapType::eSingleTap:
    mAPZEventState->ProcessSingleTap(point, scale, aModifiers, aGuid);
    break;
  case TapType::eDoubleTap:
    HandleDoubleTap(point, aModifiers, aGuid);
    break;
  case TapType::eLongTap:
    mAPZEventState->ProcessLongTap(presShell, point, scale, aModifiers, aGuid,
        aInputBlockId);
    break;
  case TapType::eLongTapUp:
    mAPZEventState->ProcessLongTapUp(presShell, point, scale, aModifiers);
    break;
  case TapType::eSentinel:
    // Should never happen, but we need to handle this case branch for the
    // compiler to be happy.
    MOZ_ASSERT(false);
    break;
  }
}

void
ChromeProcessController::NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                              APZStateChange aChange,
                                              int aArg)
{
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(NewRunnableMethod
                      <ScrollableLayerGuid,
                       APZStateChange,
                       int>(this, &ChromeProcessController::NotifyAPZStateChange,
                            aGuid, aChange, aArg));
    return;
  }

  if (!mAPZEventState) {
    return;
  }

  mAPZEventState->ProcessAPZStateChange(GetRootDocument(), aGuid.mScrollId, aChange, aArg);
}

void
ChromeProcessController::NotifyMozMouseScrollEvent(const FrameMetrics::ViewID& aScrollId, const nsString& aEvent)
{
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(NewRunnableMethod
                      <FrameMetrics::ViewID,
                       nsString>(this, &ChromeProcessController::NotifyMozMouseScrollEvent,
                                 aScrollId, aEvent));
    return;
  }

  APZCCallbackHelper::NotifyMozMouseScrollEvent(aScrollId, aEvent);
}

void
ChromeProcessController::NotifyFlushComplete()
{
  MOZ_ASSERT(IsRepaintThread());

  APZCCallbackHelper::NotifyFlushComplete(GetPresShell());
}

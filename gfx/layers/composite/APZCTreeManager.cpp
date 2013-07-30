/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManager.h"
#include "Compositor.h"

namespace mozilla {
namespace layers {

APZCTreeManager::APZCTreeManager()
    : mTreeLock("APZCTreeLock")
{
  MOZ_ASSERT(NS_IsMainThread());
  AsyncPanZoomController::InitializeGlobalState();
}

void
APZCTreeManager::UpdatePanZoomControllerTree(CompositorParent* aCompositor, Layer* aRoot,
                                             uint64_t aLayersId, bool aIsFirstPaint)
{
  Compositor::AssertOnCompositorThread();

  MonitorAutoLock lock(mTreeLock);
  AsyncPanZoomController* controller = nullptr;
  if (aRoot && aRoot->AsContainerLayer()) {
    ContainerLayer* container = aRoot->AsContainerLayer();

    // If the layer is scrollable, it needs an APZC. If there is already an APZC on it, use that,
    // otherwise create one if we can. We might not be able to create one if we don't have a
    // GeckoContentController for it, in which case we just leave it as null.
    controller = container->GetAsyncPanZoomController();
    const FrameMetrics& metrics = container->GetFrameMetrics();
    if (metrics.IsScrollable()) {
      // Scrollable, create an APZC if it doesn't have one and we can create it
      if (!controller) {
        const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(aLayersId);
        if (state && state->mController.get()) {
          controller = new AsyncPanZoomController(state->mController,
                                                  AsyncPanZoomController::USE_GESTURE_DETECTOR);
          controller->SetCompositorParent(aCompositor);
        }
      }
    } else if (controller) {
      // Not scrollable, so clear the APZC instance
      controller = nullptr;
    }
    container->SetAsyncPanZoomController(controller);

    if (controller) {
      controller->NotifyLayersUpdated(container->GetFrameMetrics(), aIsFirstPaint);
    }
  }

  if (controller) {
    mApzcs[aLayersId] = controller;
  } else {
    mApzcs.erase(aLayersId);
  }
}

nsEventStatus
APZCTreeManager::ReceiveInputEvent(const InputData& aEvent)
{
  nsRefPtr<AsyncPanZoomController> apzc;
  switch (aEvent.mInputType) {
    case MULTITOUCH_INPUT: {
      const MultiTouchInput& multiTouchInput = aEvent.AsMultiTouchInput();
      apzc = GetTargetAPZC(ScreenPoint(multiTouchInput.mTouches[0].mScreenPoint));
      break;
    } case PINCHGESTURE_INPUT: {
      const PinchGestureInput& pinchInput = aEvent.AsPinchGestureInput();
      apzc = GetTargetAPZC(pinchInput.mFocusPoint);
      break;
    } case TAPGESTURE_INPUT: {
      const TapGestureInput& tapInput = aEvent.AsTapGestureInput();
      apzc = GetTargetAPZC(ScreenPoint(tapInput.mPoint));
      break;
    } default: {
      // leave apzc as nullptr
      break;
    }
  }
  if (apzc) {
    return apzc->ReceiveInputEvent(aEvent);
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus
APZCTreeManager::ReceiveInputEvent(const nsInputEvent& aEvent,
                                   nsInputEvent* aOutEvent)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<AsyncPanZoomController> apzc;
  switch (aEvent.eventStructType) {
    case NS_TOUCH_EVENT: {
      const nsTouchEvent& touchEvent = static_cast<const nsTouchEvent&>(aEvent);
      if (touchEvent.touches.Length() > 0) {
        nsIntPoint point = touchEvent.touches[0]->mRefPoint;
        apzc = GetTargetAPZC(ScreenPoint::FromUnknownPoint(gfx::Point(point.x, point.y)));
      }
      break;
    } case NS_MOUSE_EVENT: {
      const nsMouseEvent& mouseEvent = static_cast<const nsMouseEvent&>(aEvent);
      apzc = GetTargetAPZC(ScreenPoint::FromUnknownPoint(gfx::Point(mouseEvent.refPoint.x,
                                                                    mouseEvent.refPoint.y)));
      break;
    } default: {
      // leave apzc as nullptr
      break;
    }
  }
  if (apzc) {
    return apzc->ReceiveInputEvent(aEvent, aOutEvent);
  }
  return nsEventStatus_eIgnore;
}

void
APZCTreeManager::UpdateCompositionBounds(const ScrollableLayerGuid& aGuid,
                                         const ScreenIntRect& aCompositionBounds)
{
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (apzc) {
    apzc->UpdateCompositionBounds(aCompositionBounds);
  }
}

void
APZCTreeManager::CancelDefaultPanZoom(const ScrollableLayerGuid& aGuid)
{
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (apzc) {
    apzc->CancelDefaultPanZoom();
  }
}

void
APZCTreeManager::DetectScrollableSubframe(const ScrollableLayerGuid& aGuid)
{
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (apzc) {
    apzc->DetectScrollableSubframe();
  }
}

void
APZCTreeManager::ZoomToRect(const ScrollableLayerGuid& aGuid,
                            const CSSRect& aRect)
{
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (apzc) {
    apzc->ZoomToRect(aRect);
  }
}

void
APZCTreeManager::ContentReceivedTouch(const ScrollableLayerGuid& aGuid,
                                      bool aPreventDefault)
{
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (apzc) {
    apzc->ContentReceivedTouch(aPreventDefault);
  }
}

void
APZCTreeManager::UpdateZoomConstraints(const ScrollableLayerGuid& aGuid,
                                       bool aAllowZoom,
                                       float aMinScale,
                                       float aMaxScale)
{
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (apzc) {
    apzc->UpdateZoomConstraints(aAllowZoom, aMinScale, aMaxScale);
  }
}

void
APZCTreeManager::UpdateScrollOffset(const ScrollableLayerGuid& aGuid,
                                    const CSSPoint& aScrollOffset)
{
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (apzc) {
    apzc->UpdateScrollOffset(aScrollOffset);
  }
}

void
APZCTreeManager::CancelAnimation(const ScrollableLayerGuid &aGuid)
{
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (apzc) {
    apzc->CancelAnimation();
  }
}

void
APZCTreeManager::ClearTree()
{
  MonitorAutoLock lock(mTreeLock);

  std::map< uint64_t, nsRefPtr<AsyncPanZoomController> >::iterator it = mApzcs.begin();
  while (it != mApzcs.end()) {
    nsRefPtr<AsyncPanZoomController> apzc = it->second;
    apzc->Destroy();
    it++;
  }
  mApzcs.clear();
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetTargetAPZC(const ScrollableLayerGuid& aGuid)
{
  MonitorAutoLock lock(mTreeLock);
  std::map< uint64_t, nsRefPtr<AsyncPanZoomController> >::iterator it = mApzcs.find(aGuid.mLayersId);
  if (it == mApzcs.end()) {
    return nullptr;
  }
  nsRefPtr<AsyncPanZoomController> target = it->second;
  return target.forget();
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetTargetAPZC(const ScreenPoint& aPoint)
{
  MonitorAutoLock lock(mTreeLock);
  // TODO: Do a hit test on the tree of
  // APZC instances and return the right one.
  return nullptr;
}

}
}

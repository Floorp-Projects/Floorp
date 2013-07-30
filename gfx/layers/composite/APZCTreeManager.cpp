/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManager.h"
#include "AsyncCompositionManager.h"    // for ViewTransform
#include "LayerManagerComposite.h"      // for AsyncCompositionManager.h
#include "Compositor.h"

#define APZC_LOG(args...)
// #define APZC_LOG(args...) printf_stderr(args)

namespace mozilla {
namespace layers {

APZCTreeManager::APZCTreeManager()
    : mTreeLock("APZCTreeLock")
{
  MOZ_ASSERT(NS_IsMainThread());
  AsyncPanZoomController::InitializeGlobalState();
}

void
APZCTreeManager::AssertOnCompositorThread()
{
  Compositor::AssertOnCompositorThread();
}

/* Flatten the tree of APZC instances into the given nsTArray */
static void
Collect(AsyncPanZoomController* aApzc, nsTArray< nsRefPtr<AsyncPanZoomController> >* aCollection)
{
  if (aApzc) {
    aCollection->AppendElement(aApzc);
    Collect(aApzc->GetLastChild(), aCollection);
    Collect(aApzc->GetPrevSibling(), aCollection);
  }
}

void
APZCTreeManager::UpdatePanZoomControllerTree(CompositorParent* aCompositor, Layer* aRoot,
                                             bool aIsFirstPaint, uint64_t aFirstPaintLayersId)
{
  AssertOnCompositorThread();

  MonitorAutoLock lock(mTreeLock);

  // We do this business with collecting the entire tree into an array because otherwise
  // it's very hard to determine which APZC instances need to be destroyed. In the worst
  // case, there are two scenarios: (a) a layer with an APZC is removed from the layer
  // tree and (b) a layer with an APZC is moved in the layer tree from one place to a
  // completely different place. In scenario (a) we would want to destroy the APZC while
  // walking the layer tree and noticing that the layer/APZC is no longer there. But if
  // we do that then we run into a problem in scenario (b) because we might encounter that
  // layer later during the walk. To handle both of these we have to 'remember' that the
  // layer was not found, and then do the destroy only at the end of the tree walk after
  // we are sure that the layer was removed and not just transplanted elsewhere. Doing that
  // as part of a recursive tree walk is hard and so maintaining a list and removing
  // APZCs that are still alive is much simpler.
  nsTArray< nsRefPtr<AsyncPanZoomController> > apzcsToDestroy;
  Collect(mRootApzc, &apzcsToDestroy);
  mRootApzc = nullptr;

  if (aRoot) {
    UpdatePanZoomControllerTree(aCompositor,
                                aRoot,
                                // aCompositor is null in gtest scenarios
                                aCompositor ? aCompositor->RootLayerTreeId() : 0,
                                nullptr, nullptr,
                                aIsFirstPaint, aFirstPaintLayersId,
                                &apzcsToDestroy);
  }

  for (int i = apzcsToDestroy.Length() - 1; i >= 0; i--) {
    APZC_LOG("Destroying APZC at %p\n", apzcsToDestroy[i].get());
    apzcsToDestroy[i]->Destroy();
  }
}

AsyncPanZoomController*
APZCTreeManager::UpdatePanZoomControllerTree(CompositorParent* aCompositor,
                                             Layer* aLayer, uint64_t aLayersId,
                                             AsyncPanZoomController* aParent,
                                             AsyncPanZoomController* aNextSibling,
                                             bool aIsFirstPaint, uint64_t aFirstPaintLayersId,
                                             nsTArray< nsRefPtr<AsyncPanZoomController> >* aApzcsToDestroy)
{
  ContainerLayer* container = aLayer->AsContainerLayer();
  AsyncPanZoomController* controller = nullptr;
  if (container) {
    if (container->GetFrameMetrics().IsScrollable()) {
      const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(aLayersId);
      if (state && state->mController.get()) {
        // If we get here, aLayer is a scrollable container layer and somebody
        // has registered a GeckoContentController for it, so we need to ensure
        // it has an APZC instance to manage its scrolling.

        controller = container->GetAsyncPanZoomController();
        if (!controller) {
          controller = new AsyncPanZoomController(aLayersId, state->mController,
                                                  AsyncPanZoomController::USE_GESTURE_DETECTOR);
          controller->SetCompositorParent(aCompositor);
        } else {
          // If there was already an APZC for the layer clear the tree pointers
          // so that it doesn't continue pointing to APZCs that should no longer
          // be in the tree. These pointers will get reset properly as we continue
          // building the tree. Also remove it from the set of APZCs that are going
          // to be destroyed, because it's going to remain active.
          aApzcsToDestroy->RemoveElement(controller);
          controller->SetPrevSibling(nullptr);
          controller->SetLastChild(nullptr);
        }
        APZC_LOG("Using APZC %p for layer %p with identifiers %lld %lld\n", controller, aLayer, aLayersId, container->GetFrameMetrics().mScrollId);

        controller->NotifyLayersUpdated(container->GetFrameMetrics(),
                                        aIsFirstPaint && (aLayersId == aFirstPaintLayersId));

        gfx3DMatrix transform = container->GetEffectiveTransform();
        LayerRect visible = container->GetFrameMetrics().mViewport * container->GetFrameMetrics().LayersPixelsPerCSSPixel();
        gfxRect transformed = transform.TransformBounds(gfxRect(visible.x, visible.y, visible.width, visible.height));
        controller->SetVisibleRegion(transformed);
        APZC_LOG("Setting rect(%f %f %f %f) as visible region for %p\n", transformed.x, transformed.y,
                                                                         transformed.width, transformed.height,
                                                                         controller);

        // Bind the APZC instance into the tree of APZCs
        if (aNextSibling) {
          aNextSibling->SetPrevSibling(controller);
        } else if (aParent) {
          aParent->SetLastChild(controller);
        } else {
          mRootApzc = controller;
        }

        // Let this controller be the parent of other controllers when we recurse downwards
        aParent = controller;
      }
    }

    container->SetAsyncPanZoomController(controller);
  }

  uint64_t childLayersId = (aLayer->AsRefLayer() ? aLayer->AsRefLayer()->GetReferentId() : aLayersId);
  AsyncPanZoomController* next = nullptr;
  for (Layer* child = aLayer->GetLastChild(); child; child = child->GetPrevSibling()) {
    next = UpdatePanZoomControllerTree(aCompositor, child, childLayersId, aParent, next,
                                       aIsFirstPaint, aFirstPaintLayersId, aApzcsToDestroy);
  }

  // Return the APZC that should be the sibling of other APZCs as we continue
  // moving towards the first child at this depth in the layer tree.
  // If this layer doesn't have a controller, we promote any APZCs in the subtree
  // upwards. Otherwise we fall back to the aNextSibling that was passed in.
  if (controller) {
    return controller;
  }
  if (next) {
    return next;
  }
  return aNextSibling;
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

  // This can be done as part of a tree walk but it's easier to
  // just re-use the Collect method that we need in other places.
  // If this is too slow feel free to change it to a recursive walk.
  nsTArray< nsRefPtr<AsyncPanZoomController> > apzcsToDestroy;
  Collect(mRootApzc, &apzcsToDestroy);
  for (int i = apzcsToDestroy.Length() - 1; i >= 0; i--) {
    apzcsToDestroy[i]->Destroy();
  }
  mRootApzc = nullptr;
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetTargetAPZC(const ScrollableLayerGuid& aGuid)
{
  MonitorAutoLock lock(mTreeLock);
  nsRefPtr<AsyncPanZoomController> target;
  // The root may have siblings, check those too
  for (AsyncPanZoomController* apzc = mRootApzc; apzc; apzc = apzc->GetPrevSibling()) {
    target = FindTargetAPZC(apzc, aGuid);
    if (target) {
      break;
    }
  }
  return target.forget();
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetTargetAPZC(const ScreenPoint& aPoint)
{
  MonitorAutoLock lock(mTreeLock);
  nsRefPtr<AsyncPanZoomController> target;
  // The root may have siblings, so check those too
  gfxPoint point(aPoint.x, aPoint.y);
  for (AsyncPanZoomController* apzc = mRootApzc; apzc; apzc = apzc->GetPrevSibling()) {
    target = GetAPZCAtPoint(apzc, point);
    if (target) {
      break;
    }
  }
  return target.forget();
}

AsyncPanZoomController*
APZCTreeManager::FindTargetAPZC(AsyncPanZoomController* aApzc, const ScrollableLayerGuid& aGuid) {
  // This walks the tree in depth-first, reverse order, so that it encounters
  // APZCs front-to-back on the screen.
  for (AsyncPanZoomController* child = aApzc->GetLastChild(); child; child = child->GetPrevSibling()) {
    AsyncPanZoomController* match = FindTargetAPZC(child, aGuid);
    if (match) {
      return match;
    }
  }

  if (aApzc->Matches(aGuid)) {
    return aApzc;
  }
  return nullptr;
}

AsyncPanZoomController*
APZCTreeManager::GetAPZCAtPoint(AsyncPanZoomController* aApzc, gfxPoint aHitTestPoint)
{
  // This walks the tree in depth-first, reverse order, so that it encounters
  // APZCs front-to-back on the screen.
  ViewTransform apzcTransform = aApzc->GetCurrentAsyncTransform();
  gfxPoint untransformed = gfx3DMatrix(apzcTransform).Inverse().ProjectPoint(aHitTestPoint);
  for (AsyncPanZoomController* child = aApzc->GetLastChild(); child; child = child->GetPrevSibling()) {
    AsyncPanZoomController* match = GetAPZCAtPoint(child, untransformed);
    if (match) {
      return match;
    }
  }
  if (aApzc->VisibleRegionContains(untransformed)) {
    return aApzc;
  }
  return nullptr;
}

}
}

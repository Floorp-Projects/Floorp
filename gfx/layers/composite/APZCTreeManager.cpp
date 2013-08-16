/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManager.h"
#include "AsyncCompositionManager.h"    // for ViewTransform
#include "LayerManagerComposite.h"      // for AsyncCompositionManager.h
#include "Compositor.h"

#define APZC_LOG(...)
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
                                gfx3DMatrix(), nullptr, nullptr,
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
                                             gfx3DMatrix aTransform,
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

        LayerRect visible = container->GetFrameMetrics().mViewport * container->GetFrameMetrics().LayersPixelsPerCSSPixel();
        controller->SetLayerHitTestData(visible, aTransform, aLayer->GetTransform());
        APZC_LOG("Setting rect(%f %f %f %f) as visible region for APZC %p\n", visible.x, visible.y,
                                                                              visible.width, visible.height,
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

  // Accumulate the CSS transform between layers that have an APZC, but exclude any
  // any layers that do have an APZC, and reset the accumulation at those layers.
  if (controller) {
    aTransform = gfx3DMatrix();
  } else {
    // Multiply child layer transforms on the left so they get applied first
    aTransform = aLayer->GetTransform() * aTransform;
  }

  uint64_t childLayersId = (aLayer->AsRefLayer() ? aLayer->AsRefLayer()->GetReferentId() : aLayersId);
  AsyncPanZoomController* next = nullptr;
  for (Layer* child = aLayer->GetLastChild(); child; child = child->GetPrevSibling()) {
    next = UpdatePanZoomControllerTree(aCompositor, child, childLayersId, aTransform, aParent, next,
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

/*static*/ template<class T> void
ApplyTransform(gfx::PointTyped<T>* aPoint, const gfx3DMatrix& aMatrix)
{
  gfxPoint result = aMatrix.Transform(gfxPoint(aPoint->x, aPoint->y));
  aPoint->x = result.x;
  aPoint->y = result.y;
}

/*static*/ template<class T> void
ApplyTransform(gfx::IntPointTyped<T>* aPoint, const gfx3DMatrix& aMatrix)
{
  gfxPoint result = aMatrix.Transform(gfxPoint(aPoint->x, aPoint->y));
  aPoint->x = NS_lround(result.x);
  aPoint->y = NS_lround(result.y);
}

/*static*/ void
ApplyTransform(nsIntPoint* aPoint, const gfx3DMatrix& aMatrix)
{
  gfxPoint result = aMatrix.Transform(gfxPoint(aPoint->x, aPoint->y));
  aPoint->x = NS_lround(result.x);
  aPoint->y = NS_lround(result.y);
}

nsEventStatus
APZCTreeManager::ReceiveInputEvent(const InputData& aEvent)
{
  nsRefPtr<AsyncPanZoomController> apzc;
  gfx3DMatrix transformToApzc;
  gfx3DMatrix transformToScreen;
  switch (aEvent.mInputType) {
    case MULTITOUCH_INPUT: {
      const MultiTouchInput& multiTouchInput = aEvent.AsMultiTouchInput();
      apzc = GetTargetAPZC(ScreenPoint(multiTouchInput.mTouches[0].mScreenPoint),
                           transformToApzc, transformToScreen);
      if (apzc) {
        MultiTouchInput inputForApzc(multiTouchInput);
        for (int i = inputForApzc.mTouches.Length() - 1; i >= 0; i--) {
          ApplyTransform(&(inputForApzc.mTouches[i].mScreenPoint), transformToApzc);
        }
        apzc->ReceiveInputEvent(inputForApzc);
      }
      break;
    } case PINCHGESTURE_INPUT: {
      const PinchGestureInput& pinchInput = aEvent.AsPinchGestureInput();
      apzc = GetTargetAPZC(pinchInput.mFocusPoint, transformToApzc, transformToScreen);
      if (apzc) {
        PinchGestureInput inputForApzc(pinchInput);
        ApplyTransform(&(inputForApzc.mFocusPoint), transformToApzc);
        apzc->ReceiveInputEvent(inputForApzc);
      }
      break;
    } case TAPGESTURE_INPUT: {
      const TapGestureInput& tapInput = aEvent.AsTapGestureInput();
      apzc = GetTargetAPZC(ScreenPoint(tapInput.mPoint), transformToApzc, transformToScreen);
      if (apzc) {
        TapGestureInput inputForApzc(tapInput);
        ApplyTransform(&(inputForApzc.mPoint), transformToApzc);
        apzc->ReceiveInputEvent(inputForApzc);
      }
      break;
    }
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus
APZCTreeManager::ReceiveInputEvent(const nsInputEvent& aEvent,
                                   nsInputEvent* aOutEvent)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<AsyncPanZoomController> apzc;
  gfx3DMatrix transformToApzc;
  gfx3DMatrix transformToScreen;
  switch (aEvent.eventStructType) {
    case NS_TOUCH_EVENT: {
      const nsTouchEvent& touchEvent = static_cast<const nsTouchEvent&>(aEvent);
      if (touchEvent.touches.Length() > 0) {
        nsIntPoint point = touchEvent.touches[0]->mRefPoint;
        apzc = GetTargetAPZC(ScreenPoint::FromUnknownPoint(gfx::Point(point.x, point.y)),
                             transformToApzc, transformToScreen);
        if (apzc) {
          MultiTouchInput inputForApzc(touchEvent);
          for (int i = inputForApzc.mTouches.Length() - 1; i >= 0; i--) {
            ApplyTransform(&(inputForApzc.mTouches[i].mScreenPoint), transformToApzc);
          }

          gfx3DMatrix outTransform = transformToApzc * transformToScreen;
          nsTouchEvent* outEvent = static_cast<nsTouchEvent*>(aOutEvent);
          for (int i = outEvent->touches.Length() - 1; i >= 0; i--) {
            ApplyTransform(&(outEvent->touches[i]->mRefPoint), outTransform);
          }

          return apzc->ReceiveInputEvent(inputForApzc);
        }
      }
      break;
    } case NS_MOUSE_EVENT: {
      const nsMouseEvent& mouseEvent = static_cast<const nsMouseEvent&>(aEvent);
      apzc = GetTargetAPZC(ScreenPoint::FromUnknownPoint(gfx::Point(mouseEvent.refPoint.x,
                                                                    mouseEvent.refPoint.y)),
                           transformToApzc, transformToScreen);
      if (apzc) {
        MultiTouchInput inputForApzc(mouseEvent);
        ApplyTransform(&(inputForApzc.mTouches[0].mScreenPoint), transformToApzc);

        gfx3DMatrix outTransform = transformToApzc * transformToScreen;
        ApplyTransform(&(static_cast<nsMouseEvent*>(aOutEvent)->refPoint), outTransform);

        return apzc->ReceiveInputEvent(inputForApzc);
      }
      break;
    } default: {
      // Ignore other event types
      break;
    }
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

/* This function returns the AsyncPanZoomController instance that hit testing determines
   is under the given ScreenPoint.

   In addition, the aTransformToApzcOut and aTransformToScreenOut out-parameters are filled
   with some useful transformations that input events may need applied. This is best
   illustrated with an example. Consider a chain of layers, L, M, N, O, P, Q, R. Layer L
   is the layer that corresponds to the returned APZC instance, and layer R is the root
   of the layer tree. Layer M is the parent of L, N is the parent of M, and so on.
   When layer L is displayed to the screen by the compositor, the set of transforms that
   are applied to L are (in order from top to bottom):

        L's CSS transform      (hereafter referred to as transform matrix LC)
        L's async transform    (hereafter referred to as transform matrix LA)
        M's CSS transform      (hereafter referred to as transform matrix MC)
        M's async transform    (hereafter referred to as transform matrix MA)
        ...
        R's CSS transform      (hereafter referred to as transform matrix RC)
        R's async transform    (hereafter referred to as transform matrix RA)

   Therefore, if we want user input to modify L's async transform, we have to first convert
   user input from screen space to the coordinate space of L's async transform. Doing this
   involves applying the following transforms (in order from top to bottom):
        RA.Inverse()
        RC.Inverse()
        ...
        MA.Inverse()
        MC.Inverse()
   This combined transformation is returned in the aTransformToApzcOut out-parameter.

   Next, if we want user inputs sent to gecko for event-dispatching, we will need to strip
   out all of the async transforms that are involved in this chain. This is because async
   transforms are stored only in the compositor and gecko does not account for them when
   doing display-list-based hit-testing for event dispatching. Therefore, given a user input
   in screen space, the following transforms need to be applied (in order from top to bottom):
        RA.Inverse()
        RC.Inverse()
        ...
        MA.Inverse()
        MC.Inverse()
        LA.Inverse()
        LC.Inverse()
        LC
        MC
        ...
        RC
   This sequence can be simplified and refactored to the following:
        aTransformToApzcOut
        LA.Inverse()
        MC
        ...
        RC
   Since aTransformToApzcOut is already one of the out-parameters, we set aTransformToScreenOut
   to the remaining transforms (LA.Inverse() * MC * ... * RC), so that the caller code can
   combine it with aTransformToApzcOut to get the final transform required in this case.

   Note that for many of these layers, there will be no AsyncPanZoomController attached, and
   so the async transform will be the identity transform. So, in the example above, if layers
   L and P have APZC instances attached, MA, NA, OA, QA, and RA will be identity transforms.
   Additionally, for space-saving purposes, each APZC instance stores its layers individual
   CSS transform and the accumulation of CSS transforms to its parent APZC. So the APZC for
   layer L would store LC and (MC * NC * OC), and the layer P would store PC and (QC * RC).
   The APZCs also obviously have LA and PA, so all of the above transformation combinations
   required can be generated.

   Note that this function may return null, in which case the matrix out-parameters are
   left unmodified.
 */
already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetTargetAPZC(const ScreenPoint& aPoint,
                               gfx3DMatrix& aTransformToApzcOut,
                               gfx3DMatrix& aTransformToScreenOut)
{
  MonitorAutoLock lock(mTreeLock);
  nsRefPtr<AsyncPanZoomController> target;
  // The root may have siblings, so check those too
  gfxPoint point(aPoint.x, aPoint.y);
  for (AsyncPanZoomController* apzc = mRootApzc; apzc; apzc = apzc->GetPrevSibling()) {
    target = GetAPZCAtPoint(apzc, point, aTransformToApzcOut, aTransformToScreenOut);
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
APZCTreeManager::GetAPZCAtPoint(AsyncPanZoomController* aApzc, const gfxPoint& aHitTestPoint,
                                gfx3DMatrix& aTransformToApzcOut, gfx3DMatrix& aTransformToScreenOut)
{
  // The comments below assume there is a chain of layers L..R with L and P having APZC instances as
  // explained in the comment on GetTargetAPZC. This function will recurse with aApzc at L and P, and the
  // comments explain what values are stored in the variables at these two levels. All the comments
  // use standard matrix notation where the leftmost matrix in a multiplication is applied first.

  // ancestorUntransform is OC.Inverse() * NC.Inverse() * MC.Inverse() at recursion level for L,
  //                    and RC.Inverse() * QC.Inverse()                at recursion level for P.
  gfx3DMatrix ancestorUntransform = aApzc->GetAncestorTransform().Inverse();
  // asyncUntransform is LA.Inverse() at recursion level for L,
  //                 and PA.Inverse() at recursion level for P.
  gfx3DMatrix asyncUntransform = gfx3DMatrix(aApzc->GetCurrentAsyncTransform()).Inverse();
  // untransformSinceLastApzc is OC.Inverse() * NC.Inverse() * MC.Inverse() * LA.Inverse() * LC.Inverse() at L,
  //                         and RC.Inverse() * QC.Inverse() * PA.Inverse() * PC.Inverse()                at P.
  gfx3DMatrix untransformSinceLastApzc = ancestorUntransform * asyncUntransform * aApzc->GetCSSTransform().Inverse();
  // untransformed is the user input in L's layer space at L,
  //                             and in P's layer space at P.
  gfxPoint untransformed = untransformSinceLastApzc.ProjectPoint(aHitTestPoint);
  APZC_LOG("Untransformed %f %f to %f %f for APZC %p\n", aHitTestPoint.x, aHitTestPoint.y, untransformed.x, untransformed.y, aApzc);

  // This walks the tree in depth-first, reverse order, so that it encounters
  // APZCs front-to-back on the screen.
  for (AsyncPanZoomController* child = aApzc->GetLastChild(); child; child = child->GetPrevSibling()) {
    AsyncPanZoomController* match = GetAPZCAtPoint(child, untransformed, aTransformToApzcOut, aTransformToScreenOut);
    if (match) {
      // This code is not run in the recursion at layer L.
      // aTransformToApzcOut is RC.Inverse() * QC.Inverse() * PA.Inverse() * PC.Inverse() * OC.Inverse() * NC.Inverse() * MC.Inverse()
      // at recursion level for P
      aTransformToApzcOut = untransformSinceLastApzc * aTransformToApzcOut;
      // aTransformToScreenOut is LA.Inverse() * MC * NC * OC * PC * QC * RC at recursion level for P
      aTransformToScreenOut = aTransformToScreenOut * aApzc->GetCSSTransform() * aApzc->GetAncestorTransform();
      // The above values for aTransformToApzcOut and aTransformToScreenOut at recursion level for P match
      // the required output as explained in the comment above GetTargetAPZC. Note that any missing terms
      // are async transforms that are guaranteed to be identity transforms.
      return match;
    }
  }
  if (aApzc->VisibleRegionContains(LayerPoint(untransformed.x, untransformed.y))) {
    APZC_LOG("Successfully matched untransformed point %f %f to visible region for APZC %p\n", untransformed.x, untransformed.y, aApzc);
    // This code is not run in the recursion at layer P.
    // aTransformToApzcOut is OC.Inverse() * NC.Inverse() * MC.Inverse() at recursion level for L.
    aTransformToApzcOut = ancestorUntransform;
    // aTransformToScreenOut is LA.Inverse() * MC * NC * OC at recursion level for L.
    aTransformToScreenOut = asyncUntransform * aApzc->GetAncestorTransform();
    return aApzc;
  }
  return nullptr;
}

}
}

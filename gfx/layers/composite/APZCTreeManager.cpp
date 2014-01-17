/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManager.h"
#include "AsyncCompositionManager.h"    // for ViewTransform
#include "Compositor.h"                 // for Compositor
#include "CompositorParent.h"           // for CompositorParent, etc
#include "InputData.h"                  // for InputData, etc
#include "Layers.h"                     // for ContainerLayer, Layer, etc
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "mozilla/dom/Touch.h"          // for Touch
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/layers/AsyncPanZoomController.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/mozalloc.h"           // for operator new
#include "mozilla/TouchEvents.h"
#include "nsDebug.h"                    // for NS_WARNING
#include "nsPoint.h"                    // for nsIntPoint
#include "nsThreadUtils.h"              // for NS_IsMainThread

#include <algorithm>                    // for std::stable_sort

#define APZC_LOG(...)
// #define APZC_LOG(...) printf_stderr("APZC: " __VA_ARGS__)

namespace mozilla {
namespace layers {

float APZCTreeManager::sDPI = 72.0;

APZCTreeManager::APZCTreeManager()
    : mTreeLock("APZCTreeLock"),
      mTouchCount(0)
{
  MOZ_ASSERT(NS_IsMainThread());
  AsyncPanZoomController::InitializeGlobalState();
}

APZCTreeManager::~APZCTreeManager()
{
}

void
APZCTreeManager::GetAllowedTouchBehavior(WidgetInputEvent* aEvent,
                                         nsTArray<TouchBehaviorFlags>& aOutValues)
{
  WidgetTouchEvent *touchEvent = aEvent->AsTouchEvent();

  aOutValues.Clear();

  for (size_t i = 0; i < touchEvent->touches.Length(); i++) {
    // If aEvent wasn't transformed previously we might need to
    // add transforming of the spt here.
    mozilla::ScreenIntPoint spt;
    spt.x = touchEvent->touches[i]->mRefPoint.x;
    spt.y = touchEvent->touches[i]->mRefPoint.y;

    nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(spt);
    aOutValues.AppendElement(apzc
      ? apzc->GetAllowedTouchBehavior(spt)
      : AllowedTouchBehavior::UNKNOWN);
  }
}

void
APZCTreeManager::SetAllowedTouchBehavior(const ScrollableLayerGuid& aGuid,
                                         const nsTArray<TouchBehaviorFlags> &aValues)
{
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (apzc) {
    apzc->SetAllowedTouchBehavior(aValues);
  }
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

  for (size_t i = 0; i < apzcsToDestroy.Length(); i++) {
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
  mTreeLock.AssertCurrentThreadOwns();

  ContainerLayer* container = aLayer->AsContainerLayer();
  AsyncPanZoomController* apzc = nullptr;
  if (container) {
    if (container->GetFrameMetrics().IsScrollable()) {
      const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(aLayersId);
      if (state && state->mController.get()) {
        // If we get here, aLayer is a scrollable container layer and somebody
        // has registered a GeckoContentController for it, so we need to ensure
        // it has an APZC instance to manage its scrolling.

        apzc = container->GetAsyncPanZoomController();

        // If the content represented by the container layer has changed (which may
        // be possible because of DLBI heuristics) then we don't want to keep using
        // the same old APZC for the new content. Null it out so we run through the
        // code to find another one or create one.
        if (apzc && !apzc->Matches(ScrollableLayerGuid(aLayersId, container->GetFrameMetrics()))) {
          apzc = nullptr;
        }

        // If the container doesn't have an APZC already, try to find one of our
        // pre-existing ones that matches. In particular, if we find an APZC whose
        // ScrollableLayerGuid is the same, then we know what happened is that the
        // layout of the page changed causing the layer tree to be rebuilt, but the
        // underlying content for which the APZC was originally created is still
        // there. So it makes sense to pick up that APZC instance again and use it here.
        if (apzc == nullptr) {
          ScrollableLayerGuid target(aLayersId, container->GetFrameMetrics());
          for (size_t i = 0; i < aApzcsToDestroy->Length(); i++) {
            if (aApzcsToDestroy->ElementAt(i)->Matches(target)) {
              apzc = aApzcsToDestroy->ElementAt(i);
              break;
            }
          }
        }

        // The APZC we get off the layer may have been destroyed previously if the layer was inactive
        // or omitted from the layer tree for whatever reason from a layers update. If it later comes
        // back it will have a reference to a destroyed APZC and so we need to throw that out and make
        // a new one.
        bool newApzc = (apzc == nullptr || apzc->IsDestroyed());
        if (newApzc) {
          apzc = new AsyncPanZoomController(aLayersId, this, state->mController,
                                            AsyncPanZoomController::USE_GESTURE_DETECTOR);
          apzc->SetCompositorParent(aCompositor);
          apzc->SetCrossProcessCompositorParent(state->mCrossProcessParent);
        } else {
          // If there was already an APZC for the layer clear the tree pointers
          // so that it doesn't continue pointing to APZCs that should no longer
          // be in the tree. These pointers will get reset properly as we continue
          // building the tree. Also remove it from the set of APZCs that are going
          // to be destroyed, because it's going to remain active.
          aApzcsToDestroy->RemoveElement(apzc);
          apzc->SetPrevSibling(nullptr);
          apzc->SetLastChild(nullptr);
        }
        APZC_LOG("Using APZC %p for layer %p with identifiers %lld %lld\n", apzc, aLayer, aLayersId, container->GetFrameMetrics().mScrollId);

        apzc->NotifyLayersUpdated(container->GetFrameMetrics(),
                                  aIsFirstPaint && (aLayersId == aFirstPaintLayersId));

        ScreenRect visible(container->GetFrameMetrics().mCompositionBounds);
        apzc->SetLayerHitTestData(visible, aTransform, aLayer->GetTransform());
        APZC_LOG("Setting rect(%f %f %f %f) as visible region for APZC %p\n", visible.x, visible.y,
                                                                              visible.width, visible.height,
                                                                              apzc);

        // Bind the APZC instance into the tree of APZCs
        if (aNextSibling) {
          aNextSibling->SetPrevSibling(apzc);
        } else if (aParent) {
          aParent->SetLastChild(apzc);
        } else {
          mRootApzc = apzc;
        }

        // Let this apzc be the parent of other controllers when we recurse downwards
        aParent = apzc;

        if (newApzc) {
          if (apzc->IsRootForLayersId()) {
            // If we just created a new apzc that is the root for its layers ID, then
            // we need to update its zoom constraints which might have arrived before this
            // was created
            ZoomConstraints constraints;
            if (state->mController->GetRootZoomConstraints(&constraints)) {
              apzc->UpdateZoomConstraints(constraints);
            }
          } else {
            // For an apzc that is not the root for its layers ID, we give it the
            // same zoom constraints as its parent. This ensures that if e.g.
            // user-scalable=no was specified, none of the APZCs allow double-tap
            // to zoom.
            apzc->UpdateZoomConstraints(apzc->GetParent()->GetZoomConstraints());
          }
        }
      }
    }

    container->SetAsyncPanZoomController(apzc);
  }

  // Accumulate the CSS transform between layers that have an APZC, but exclude any
  // any layers that do have an APZC, and reset the accumulation at those layers.
  if (apzc) {
    aTransform = gfx3DMatrix();
  } else {
    // Multiply child layer transforms on the left so they get applied first
    aTransform = aLayer->GetTransform() * aTransform;
  }

  uint64_t childLayersId = (aLayer->AsRefLayer() ? aLayer->AsRefLayer()->GetReferentId() : aLayersId);
  // If there's no APZC at this level, any APZCs for our child layers will
  // have our siblings as siblings.
  AsyncPanZoomController* next = apzc ? nullptr : aNextSibling;
  for (Layer* child = aLayer->GetLastChild(); child; child = child->GetPrevSibling()) {
    next = UpdatePanZoomControllerTree(aCompositor, child, childLayersId, aTransform, aParent, next,
                                       aIsFirstPaint, aFirstPaintLayersId, aApzcsToDestroy);
  }

  // Return the APZC that should be the sibling of other APZCs as we continue
  // moving towards the first child at this depth in the layer tree.
  // If this layer doesn't have an APZC, we promote any APZCs in the subtree
  // upwards. Otherwise we fall back to the aNextSibling that was passed in.
  if (apzc) {
    return apzc;
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
APZCTreeManager::ReceiveInputEvent(const InputData& aEvent,
                                   ScrollableLayerGuid* aOutTargetGuid)
{
  nsEventStatus result = nsEventStatus_eIgnore;
  gfx3DMatrix transformToApzc;
  gfx3DMatrix transformToGecko;
  switch (aEvent.mInputType) {
    case MULTITOUCH_INPUT: {
      const MultiTouchInput& multiTouchInput = aEvent.AsMultiTouchInput();
      if (multiTouchInput.mType == MultiTouchInput::MULTITOUCH_START) {
        mTouchCount++;
        mApzcForInputBlock = GetTargetAPZC(ScreenPoint(multiTouchInput.mTouches[0].mScreenPoint));
        if (multiTouchInput.mTouches.Length() == 1) {
          // If we have one touch point, this might be the start of a pan.
          // Prepare for possible overscroll handoff.
          BuildOverscrollHandoffChain(mApzcForInputBlock);
        }
        for (size_t i = 1; i < multiTouchInput.mTouches.Length(); i++) {
          nsRefPtr<AsyncPanZoomController> apzc2 = GetTargetAPZC(ScreenPoint(multiTouchInput.mTouches[i].mScreenPoint));
          mApzcForInputBlock = CommonAncestor(mApzcForInputBlock.get(), apzc2.get());
          APZC_LOG("Using APZC %p as the common ancestor\n", mApzcForInputBlock.get());
          // For now, we only ever want to do pinching on the root APZC for a given layers id. So
          // when we find the common ancestor of multiple points, also walk up to the root APZC.
          mApzcForInputBlock = RootAPZCForLayersId(mApzcForInputBlock);
          APZC_LOG("Using APZC %p as the root APZC for multi-touch\n", mApzcForInputBlock.get());
        }

        if (mApzcForInputBlock) {
          // Cache transformToApzc so it can be used for future events in this block.
          GetInputTransforms(mApzcForInputBlock, transformToApzc, transformToGecko);
          mCachedTransformToApzcForInputBlock = transformToApzc;
        } else {
          // Reset the cached apz transform
          mCachedTransformToApzcForInputBlock = gfx3DMatrix();
        }
      } else if (mApzcForInputBlock) {
        APZC_LOG("Re-using APZC %p as continuation of event block\n", mApzcForInputBlock.get());
      }
      if (mApzcForInputBlock) {
        mApzcForInputBlock->GetGuid(aOutTargetGuid);
        // Use the cached transform to compute the point to send to the APZC.
        // This ensures that the sequence of touch points an APZC sees in an
        // input block are all in the same coordinate space.
        transformToApzc = mCachedTransformToApzcForInputBlock;
        MultiTouchInput inputForApzc(multiTouchInput);
        for (size_t i = 0; i < inputForApzc.mTouches.Length(); i++) {
          ApplyTransform(&(inputForApzc.mTouches[i].mScreenPoint), transformToApzc);
        }
        result = mApzcForInputBlock->ReceiveInputEvent(inputForApzc);
      }
      if (multiTouchInput.mType == MultiTouchInput::MULTITOUCH_CANCEL ||
          multiTouchInput.mType == MultiTouchInput::MULTITOUCH_END) {
        if (mTouchCount >= multiTouchInput.mTouches.Length()) {
          mTouchCount -= multiTouchInput.mTouches.Length();
        } else {
          NS_WARNING("Got an unexpected touchend/touchcancel");
          mTouchCount = 0;
        }
        // If we have an mApzcForInputBlock and it's the end of the touch sequence
        // then null it out so we don't keep a dangling reference and leak things.
        if (mTouchCount == 0) {
          mApzcForInputBlock = nullptr;
          mOverscrollHandoffChain.clear();
        }
      }
      break;
    } case PINCHGESTURE_INPUT: {
      const PinchGestureInput& pinchInput = aEvent.AsPinchGestureInput();
      nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(pinchInput.mFocusPoint);
      if (apzc) {
        apzc->GetGuid(aOutTargetGuid);
        GetInputTransforms(apzc, transformToApzc, transformToGecko);
        PinchGestureInput inputForApzc(pinchInput);
        ApplyTransform(&(inputForApzc.mFocusPoint), transformToApzc);
        result = apzc->ReceiveInputEvent(inputForApzc);
      }
      break;
    } case TAPGESTURE_INPUT: {
      const TapGestureInput& tapInput = aEvent.AsTapGestureInput();
      nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(ScreenPoint(tapInput.mPoint));
      if (apzc) {
        apzc->GetGuid(aOutTargetGuid);
        GetInputTransforms(apzc, transformToApzc, transformToGecko);
        TapGestureInput inputForApzc(tapInput);
        ApplyTransform(&(inputForApzc.mPoint), transformToApzc);
        result = apzc->ReceiveInputEvent(inputForApzc);
      }
      break;
    }
  }
  return result;
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetTouchInputBlockAPZC(const WidgetTouchEvent& aEvent)
{
  ScreenPoint point = ScreenPoint(aEvent.touches[0]->mRefPoint.x, aEvent.touches[0]->mRefPoint.y);
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(point);
  if (aEvent.touches.Length() == 1) {
    // If we have one touch point, this might be the start of a pan.
    // Prepare for possible overscroll handoff.
    BuildOverscrollHandoffChain(apzc);
  }
  for (size_t i = 1; i < aEvent.touches.Length(); i++) {
    point = ScreenPoint(aEvent.touches[i]->mRefPoint.x, aEvent.touches[i]->mRefPoint.y);
    nsRefPtr<AsyncPanZoomController> apzc2 = GetTargetAPZC(point);
    apzc = CommonAncestor(apzc.get(), apzc2.get());
    APZC_LOG("Using APZC %p as the common ancestor\n", apzc.get());
    // For now, we only ever want to do pinching on the root APZC for a given layers id. So
    // when we find the common ancestor of multiple points, also walk up to the root APZC.
    apzc = RootAPZCForLayersId(apzc);
    APZC_LOG("Using APZC %p as the root APZC for multi-touch\n", apzc.get());
  }
  return apzc.forget();
}

nsEventStatus
APZCTreeManager::ProcessTouchEvent(const WidgetTouchEvent& aEvent,
                                   ScrollableLayerGuid* aOutTargetGuid,
                                   WidgetTouchEvent* aOutEvent)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsEventStatus ret = nsEventStatus_eIgnore;
  if (!aEvent.touches.Length()) {
    return ret;
  }
  if (aEvent.message == NS_TOUCH_START) {
    mTouchCount++;
    mApzcForInputBlock = GetTouchInputBlockAPZC(aEvent);
    if (mApzcForInputBlock) {
      // Cache apz transform so it can be used for future events in this block.
      gfx3DMatrix transformToGecko;
      GetInputTransforms(mApzcForInputBlock, mCachedTransformToApzcForInputBlock, transformToGecko);
    } else {
      // Reset the cached apz transform
      mCachedTransformToApzcForInputBlock = gfx3DMatrix();
    }
  }

  if (mApzcForInputBlock) {
    mApzcForInputBlock->GetGuid(aOutTargetGuid);
    // For computing the input for the APZC, used the cached transform.
    // This ensures that the sequence of touch points an APZC sees in an
    // input block are all in the same coordinate space.
    gfx3DMatrix transformToApzc = mCachedTransformToApzcForInputBlock;
    MultiTouchInput inputForApzc(aEvent);
    for (size_t i = 0; i < inputForApzc.mTouches.Length(); i++) {
      ApplyTransform(&(inputForApzc.mTouches[i].mScreenPoint), transformToApzc);
    }
    ret = mApzcForInputBlock->ReceiveInputEvent(inputForApzc);

    // For computing the event to pass back to Gecko, use the up-to-date transforms.
    // This ensures that transformToApzc and transformToGecko are in sync
    // (note that transformToGecko isn't cached).
    gfx3DMatrix transformToGecko;
    GetInputTransforms(mApzcForInputBlock, transformToApzc, transformToGecko);
    gfx3DMatrix outTransform = transformToApzc * transformToGecko;
    for (size_t i = 0; i < aOutEvent->touches.Length(); i++) {
      ApplyTransform(&(aOutEvent->touches[i]->mRefPoint), outTransform);
    }
  }
  // If we have an mApzcForInputBlock and it's the end of the touch sequence
  // then null it out so we don't keep a dangling reference and leak things.
  if (aEvent.message == NS_TOUCH_CANCEL ||
      aEvent.message == NS_TOUCH_END) {
    if (mTouchCount >= aEvent.touches.Length()) {
      mTouchCount -= aEvent.touches.Length();
    } else {
      NS_WARNING("Got an unexpected touchend/touchcancel");
      mTouchCount = 0;
    }
    if (mTouchCount == 0) {
      mApzcForInputBlock = nullptr;
      mOverscrollHandoffChain.clear();
    }
  }
  return ret;
}

void
APZCTreeManager::TransformCoordinateToGecko(const ScreenIntPoint& aPoint,
                                            LayoutDeviceIntPoint* aOutTransformedPoint)
{
  MOZ_ASSERT(aOutTransformedPoint);
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aPoint);
  if (apzc && aOutTransformedPoint) {
    gfx3DMatrix transformToApzc;
    gfx3DMatrix transformToGecko;
    GetInputTransforms(apzc, transformToApzc, transformToGecko);
    gfx3DMatrix outTransform = transformToApzc * transformToGecko;
    aOutTransformedPoint->x = aPoint.x;
    aOutTransformedPoint->y = aPoint.y;
    ApplyTransform(aOutTransformedPoint, outTransform);
  }
}


nsEventStatus
APZCTreeManager::ProcessMouseEvent(const WidgetMouseEvent& aEvent,
                                   ScrollableLayerGuid* aOutTargetGuid,
                                   WidgetMouseEvent* aOutEvent)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(ScreenPoint(aEvent.refPoint.x, aEvent.refPoint.y));
  if (!apzc) {
    return nsEventStatus_eIgnore;
  }
  apzc->GetGuid(aOutTargetGuid);
  gfx3DMatrix transformToApzc;
  gfx3DMatrix transformToGecko;
  GetInputTransforms(apzc, transformToApzc, transformToGecko);
  MultiTouchInput inputForApzc(aEvent);
  ApplyTransform(&(inputForApzc.mTouches[0].mScreenPoint), transformToApzc);
  gfx3DMatrix outTransform = transformToApzc * transformToGecko;
  ApplyTransform(&aOutEvent->refPoint, outTransform);
  return apzc->ReceiveInputEvent(inputForApzc);
}

nsEventStatus
APZCTreeManager::ProcessEvent(const WidgetInputEvent& aEvent,
                              ScrollableLayerGuid* aOutTargetGuid,
                              WidgetInputEvent* aOutEvent)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Transform the refPoint
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(ScreenPoint(aEvent.refPoint.x, aEvent.refPoint.y));
  if (!apzc) {
    return nsEventStatus_eIgnore;
  }
  apzc->GetGuid(aOutTargetGuid);
  gfx3DMatrix transformToApzc;
  gfx3DMatrix transformToGecko;
  GetInputTransforms(apzc, transformToApzc, transformToGecko);
  gfx3DMatrix outTransform = transformToApzc * transformToGecko;
  ApplyTransform(&(aOutEvent->refPoint), outTransform);
  return nsEventStatus_eIgnore;
}

nsEventStatus
APZCTreeManager::ReceiveInputEvent(const WidgetInputEvent& aEvent,
                                   ScrollableLayerGuid* aOutTargetGuid,
                                   WidgetInputEvent* aOutEvent)
{
  MOZ_ASSERT(NS_IsMainThread());

  switch (aEvent.eventStructType) {
    case NS_TOUCH_EVENT: {
      const WidgetTouchEvent& touchEvent = *aEvent.AsTouchEvent();
      return ProcessTouchEvent(touchEvent, aOutTargetGuid, aOutEvent->AsTouchEvent());
    }
    case NS_MOUSE_EVENT: {
      // For b2g emulation
      const WidgetMouseEvent& mouseEvent = *aEvent.AsMouseEvent();
      WidgetMouseEvent* outMouseEvent = aOutEvent->AsMouseEvent();
      return ProcessMouseEvent(mouseEvent, aOutTargetGuid, outMouseEvent);
    }
    default: {
      return ProcessEvent(aEvent, aOutTargetGuid, aOutEvent);
    }
  }
}

nsEventStatus
APZCTreeManager::ReceiveInputEvent(WidgetInputEvent& aEvent,
                                   ScrollableLayerGuid* aOutTargetGuid)
{
  MOZ_ASSERT(NS_IsMainThread());

  switch (aEvent.eventStructType) {
    case NS_TOUCH_EVENT: {
      WidgetTouchEvent& touchEvent = *aEvent.AsTouchEvent();
      return ProcessTouchEvent(touchEvent, aOutTargetGuid, &touchEvent);
    }
    default: {
      return ProcessEvent(aEvent, aOutTargetGuid, &aEvent);
    }
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
                                       const ZoomConstraints& aConstraints)
{
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  // For a given layers id, non-root APZCs inherit the zoom constraints
  // of their root.
  if (apzc && apzc->IsRootForLayersId()) {
    MonitorAutoLock lock(mTreeLock);
    UpdateZoomConstraintsRecursively(apzc.get(), aConstraints);
  }
}

void
APZCTreeManager::UpdateZoomConstraintsRecursively(AsyncPanZoomController* aApzc,
                                                  const ZoomConstraints& aConstraints)
{
  mTreeLock.AssertCurrentThreadOwns();

  aApzc->UpdateZoomConstraints(aConstraints);
  for (AsyncPanZoomController* child = aApzc->GetLastChild(); child; child = child->GetPrevSibling()) {
    // We can have subtrees with their own layers id - leave those alone.
    if (!child->IsRootForLayersId()) {
      UpdateZoomConstraintsRecursively(child, aConstraints);
    }
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
  for (size_t i = 0; i < apzcsToDestroy.Length(); i++) {
    apzcsToDestroy[i]->Destroy();
  }
  mRootApzc = nullptr;
}

void
APZCTreeManager::DispatchScroll(AsyncPanZoomController* aPrev, ScreenPoint aStartPoint, ScreenPoint aEndPoint,
                                uint32_t aOverscrollHandoffChainIndex)
{
  // If we have reached the end of the overscroll handoff chain, there is
  // nothing more to scroll, so we ignore the rest of the pan gesture.
  if (aOverscrollHandoffChainIndex >= mOverscrollHandoffChain.length()) {
    // Nothing more to scroll - ignore the rest of the pan gesture.
    return;
  }

  nsRefPtr<AsyncPanZoomController> next = mOverscrollHandoffChain[aOverscrollHandoffChainIndex];
  if (next == nullptr)
    return;

  // Convert the start and end points from |aPrev|'s coordinate space to
  // |next|'s coordinate space. Since |aPrev| may be the same as |next|
  // (if |aPrev| is the APZC that is initiating the scroll and there is no
  // scroll grabbing to grab the scroll from it), don't bother doing the
  // transformations in that case.
  if (next != aPrev) {
    gfx3DMatrix transformToApzc;
    gfx3DMatrix transformToGecko;  // ignored

    // Convert start and end points to untransformed screen coordinates.
    GetInputTransforms(aPrev, transformToApzc, transformToGecko);
    ApplyTransform(&aStartPoint, transformToApzc.Inverse());
    ApplyTransform(&aEndPoint, transformToApzc.Inverse());

    // Convert start and end points to next's transformed screen coordinates.
    GetInputTransforms(next, transformToApzc, transformToGecko);
    ApplyTransform(&aStartPoint, transformToApzc);
    ApplyTransform(&aEndPoint, transformToApzc);
  }

  // Scroll |next|. If this causes overscroll, it will call DispatchScroll()
  // again with an incremented index.
  next->AttemptScroll(aStartPoint, aEndPoint, aOverscrollHandoffChainIndex);
}

bool
APZCTreeManager::HitTestAPZC(const ScreenIntPoint& aPoint)
{
  MonitorAutoLock lock(mTreeLock);
  nsRefPtr<AsyncPanZoomController> target;
  // The root may have siblings, so check those too
  gfxPoint point(aPoint.x, aPoint.y);
  for (AsyncPanZoomController* apzc = mRootApzc; apzc; apzc = apzc->GetPrevSibling()) {
    target = GetAPZCAtPoint(apzc, point);
    if (target) {
      return true;
    }
  }
  return false;
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

struct CompareByScrollPriority
{
  bool operator()(const nsRefPtr<AsyncPanZoomController>& a, const nsRefPtr<AsyncPanZoomController>& b) {
    return a->HasScrollgrab() && !b->HasScrollgrab();
  }
};

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

void
APZCTreeManager::BuildOverscrollHandoffChain(const nsRefPtr<AsyncPanZoomController>& aInitialTarget)
{
  // Scroll grabbing is a mechanism that allows content to specify that
  // the initial target of a pan should be not the innermost scrollable
  // frame at the touch point (which is what GetTargetAPZC finds), but
  // something higher up in the tree.
  // It's not sufficient to just find the initial target, however, as
  // overscroll can be handed off to another APZC. Without scroll grabbing,
  // handoff just occurs from child to parent. With scroll grabbing, the
  // handoff order can be different, so we build a chain of APZCs in the
  // order in which scroll will be handed off to them.

  mOverscrollHandoffChain.clear();

  // Start with the child -> parent chain.
  for (AsyncPanZoomController* apzc = aInitialTarget; apzc; apzc = apzc->GetParent()) {
    if (!mOverscrollHandoffChain.append(apzc)) {
      NS_WARNING("Vector::append failed");
      mOverscrollHandoffChain.clear();
      return;
    }
  }

  // Now adjust the chain to account for scroll grabbing. Sorting is a bit
  // of an overkill here, but scroll grabbing will likely be generalized
  // to scroll priorities, so we might as well do it this way.
  // The sorting being stable ensures that the relative order between
  // non-scrollgrabbing APZCs remains child -> parent.
  // (The relative order between scrollgrabbing APZCs will also remain
  // child -> parent, though that's just an artefact of the implementation
  // and users of 'scrollgrab' should not rely on this.)
  std::stable_sort(mOverscrollHandoffChain.begin(), mOverscrollHandoffChain.end(),
                   CompareByScrollPriority());
}

AsyncPanZoomController*
APZCTreeManager::FindTargetAPZC(AsyncPanZoomController* aApzc, const ScrollableLayerGuid& aGuid)
{
  mTreeLock.AssertCurrentThreadOwns();

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
APZCTreeManager::GetAPZCAtPoint(AsyncPanZoomController* aApzc, const gfxPoint& aHitTestPoint)
{
  mTreeLock.AssertCurrentThreadOwns();

  // The comments below assume there is a chain of layers L..R with L and P having APZC instances as
  // explained in the comment on GetInputTransforms. This function will recurse with aApzc at L and P, and the
  // comments explain what values are stored in the variables at these two levels. All the comments
  // use standard matrix notation where the leftmost matrix in a multiplication is applied first.

  // ancestorUntransform takes points from aApzc's parent APZC's layer coordinates
  // to aApzc's screen coordinates.
  // It is OC.Inverse() * NC.Inverse() * MC.Inverse() at recursion level for L,
  //   and RC.Inverse() * QC.Inverse()                at recursion level for P.
  gfx3DMatrix ancestorUntransform = aApzc->GetAncestorTransform().Inverse();

  // hitTestTransform takes points from aApzc's parent APZC's layer coordinates to
  // the hit test space (which is aApzc's transient coordinates).
  // It is OC.Inverse() * NC.Inverse() * MC.Inverse() * LC.Inverse() * LN.Inverse() at L,
  //   and RC.Inverse() * QC.Inverse() * PC.Inverse() * PN.Inverse()                at P.
  gfx3DMatrix hitTestTransform = ancestorUntransform
                               * aApzc->GetCSSTransform().Inverse()
                               * aApzc->GetNontransientAsyncTransform().Inverse();
  gfxPoint hitTestPointForThisLayer = hitTestTransform.ProjectPoint(aHitTestPoint);
  APZC_LOG("Untransformed %f %f to transient coordinates %f %f for hit-testing APZC %p\n",
           aHitTestPoint.x, aHitTestPoint.y,
           hitTestPointForThisLayer.x, hitTestPointForThisLayer.y, aApzc);

  // childUntransform takes points from aApzc's parent APZC's layer coordinates
  // to aApzc's layer coordinates (which are aApzc's children's screen coordinates).
  // It is OC.Inverse() * NC.Inverse() * MC.Inverse() * LC.Inverse() * LA.Inverse() at L
  //   and RC.Inverse() * QC.Inverse() * PC.Inverse() * PA.Inverse()                at P.
  gfx3DMatrix childUntransform = ancestorUntransform
                               * aApzc->GetCSSTransform().Inverse()
                               * gfx3DMatrix(aApzc->GetCurrentAsyncTransform()).Inverse();
  gfxPoint hitTestPointForChildLayers = childUntransform.ProjectPoint(aHitTestPoint);
  APZC_LOG("Untransformed %f %f to layer coordinates %f %f for APZC %p\n",
           aHitTestPoint.x, aHitTestPoint.y,
           hitTestPointForChildLayers.x, hitTestPointForChildLayers.y, aApzc);

  // This walks the tree in depth-first, reverse order, so that it encounters
  // APZCs front-to-back on the screen.
  for (AsyncPanZoomController* child = aApzc->GetLastChild(); child; child = child->GetPrevSibling()) {
    AsyncPanZoomController* match = GetAPZCAtPoint(child, hitTestPointForChildLayers);
    if (match) {
      return match;
    }
  }
  if (aApzc->VisibleRegionContains(ScreenPoint(hitTestPointForThisLayer.x, hitTestPointForThisLayer.y))) {
    APZC_LOG("Successfully matched untransformed point %f %f to visible region for APZC %p\n",
             hitTestPointForThisLayer.x, hitTestPointForThisLayer.y, aApzc);
    return aApzc;
  }
  return nullptr;
}

/* This function sets the aTransformToApzcOut and aTransformToGeckoOut out-parameters
   to some useful transformations that input events may need applied. This is best
   illustrated with an example. Consider a chain of layers, L, M, N, O, P, Q, R. Layer L
   is the layer that corresponds to the returned APZC instance, and layer R is the root
   of the layer tree. Layer M is the parent of L, N is the parent of M, and so on.
   When layer L is displayed to the screen by the compositor, the set of transforms that
   are applied to L are (in order from top to bottom):

        L's transient async transform       (hereafter referred to as transform matrix LT)
        L's nontransient async transform    (hereafter referred to as transform matrix LN)
        L's CSS transform                   (hereafter referred to as transform matrix LC)
        M's transient async transform       (hereafter referred to as transform matrix MT)
        M's nontransient async transform    (hereafter referred to as transform matrix MN)
        M's CSS transform                   (hereafter referred to as transform matrix MC)
        ...
        R's transient async transform       (hereafter referred to as transform matrix RT)
        R's nontransient async transform    (hereafter referred to as transform matrix RN)
        R's CSS transform                   (hereafter referred to as transform matrix RC)

   Also, for any layer, the async transform is the combination of its transient and non-transient
   parts. That is, for any layer L:
                  LA === LT * LN
        LA.Inverse() === LN.Inverse() * LT.Inverse()

   If we want user input to modify L's transient async transform, we have to first convert
   user input from screen space to the coordinate space of L's transient async transform. Doing
   this involves applying the following transforms (in order from top to bottom):
        RC.Inverse()
        RN.Inverse()
        RT.Inverse()
        ...
        MC.Inverse()
        MN.Inverse()
        MT.Inverse()
        LC.Inverse()
        LN.Inverse()
   This combined transformation is returned in the aTransformToApzcOut out-parameter.

   Next, if we want user inputs sent to gecko for event-dispatching, we will need to strip
   out all of the async transforms that are involved in this chain. This is because async
   transforms are stored only in the compositor and gecko does not account for them when
   doing display-list-based hit-testing for event dispatching. Therefore, given a user input
   in screen space, the following transforms need to be applied (in order from top to bottom):
        RC.Inverse()
        RN.Inverse()
        RT.Inverse()
        ...
        MC.Inverse()
        MN.Inverse()
        MT.Inverse()
        LC.Inverse()
        LN.Inverse()
        LT.Inverse()
        LC
        MC
        ...
        RC
   This sequence can be simplified and refactored to the following:
        aTransformToApzcOut
        LT.Inverse()
        LC
        MC
        ...
        RC
   Since aTransformToApzcOut is already one of the out-parameters, we set aTransformToGeckoOut
   to the remaining transforms (LT.Inverse() * LC * ... * RC), so that the caller code can
   combine it with aTransformToApzcOut to get the final transform required in this case.

   Note that for many of these layers, there will be no AsyncPanZoomController attached, and
   so the async transform will be the identity transform. So, in the example above, if layers
   L and P have APZC instances attached, MT, MN, NT, NN, OT, ON, QT, QN, RT and RN will be
   identity transforms.
   Additionally, for space-saving purposes, each APZC instance stores its layer's individual
   CSS transform and the accumulation of CSS transforms to its parent APZC. So the APZC for
   layer L would store LC and (MC * NC * OC), and the layer P would store PC and (QC * RC).
   The APZCs also obviously have LT, LN, PT, and PN, so all of the above transformation combinations
   required can be generated.
 */
void
APZCTreeManager::GetInputTransforms(AsyncPanZoomController *aApzc, gfx3DMatrix& aTransformToApzcOut,
                                    gfx3DMatrix& aTransformToGeckoOut)
{
  MonitorAutoLock lock(mTreeLock);

  // The comments below assume there is a chain of layers L..R with L and P having APZC instances as
  // explained in the comment above. This function is called with aApzc at L, and the loop
  // below performs one iteration, where parent is at P. The comments explain what values are stored
  // in the variables at these two levels. All the comments use standard matrix notation where the
  // leftmost matrix in a multiplication is applied first.

  // ancestorUntransform is OC.Inverse() * NC.Inverse() * MC.Inverse()
  gfx3DMatrix ancestorUntransform = aApzc->GetAncestorTransform().Inverse();
  // asyncUntransform is LA.Inverse()
  gfx3DMatrix asyncUntransform = gfx3DMatrix(aApzc->GetCurrentAsyncTransform()).Inverse();
  // nontransientAsyncTransform is LN
  gfx3DMatrix nontransientAsyncTransform = aApzc->GetNontransientAsyncTransform();
  // transientAsyncUntransform is LT.Inverse()
  gfx3DMatrix transientAsyncUntransform = nontransientAsyncTransform * asyncUntransform;

  // aTransformToApzcOut is initialized to OC.Inverse() * NC.Inverse() * MC.Inverse() * LC.Inverse() * LN.Inverse()
  aTransformToApzcOut = ancestorUntransform * aApzc->GetCSSTransform().Inverse() * nontransientAsyncTransform.Inverse();
  // aTransformToGeckoOut is initialized to LT.Inverse() * LC * MC * NC * OC
  aTransformToGeckoOut = transientAsyncUntransform * aApzc->GetCSSTransform() * aApzc->GetAncestorTransform();

  for (AsyncPanZoomController* parent = aApzc->GetParent(); parent; parent = parent->GetParent()) {
    // ancestorUntransform is updated to RC.Inverse() * QC.Inverse() when parent == P
    ancestorUntransform = parent->GetAncestorTransform().Inverse();
    // asyncUntransform is updated to PA.Inverse() when parent == P
    asyncUntransform = gfx3DMatrix(parent->GetCurrentAsyncTransform()).Inverse();
    // untransformSinceLastApzc is RC.Inverse() * QC.Inverse() * PC.Inverse() * PA.Inverse()
    gfx3DMatrix untransformSinceLastApzc = ancestorUntransform * parent->GetCSSTransform().Inverse() * asyncUntransform;

    // aTransformToApzcOut is RC.Inverse() * QC.Inverse() * PC.Inverse() * PA.Inverse() * OC.Inverse() * NC.Inverse() * MC.Inverse() * LC.Inverse() * LN.Inverse()
    aTransformToApzcOut = untransformSinceLastApzc * aTransformToApzcOut;
    // aTransformToGeckoOut is LT.Inverse() * LC * MC * NC * OC * PC * QC * RC
    aTransformToGeckoOut = aTransformToGeckoOut * parent->GetCSSTransform() * parent->GetAncestorTransform();

    // The above values for aTransformToApzcOut and aTransformToGeckoOut when parent == P match
    // the required output as explained in the comment above GetTargetAPZC. Note that any missing terms
    // are async transforms that are guaranteed to be identity transforms.
  }
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::CommonAncestor(AsyncPanZoomController* aApzc1, AsyncPanZoomController* aApzc2)
{
  MonitorAutoLock lock(mTreeLock);
  nsRefPtr<AsyncPanZoomController> ancestor;

  // If either aApzc1 or aApzc2 is null, min(depth1, depth2) will be 0 and this function
  // will return null.

  // Calculate depth of the APZCs in the tree
  int depth1 = 0, depth2 = 0;
  for (AsyncPanZoomController* parent = aApzc1; parent; parent = parent->GetParent()) {
    depth1++;
  }
  for (AsyncPanZoomController* parent = aApzc2; parent; parent = parent->GetParent()) {
    depth2++;
  }

  // At most one of the following two loops will be executed; the deeper APZC pointer
  // will get walked up to the depth of the shallower one.
  int minDepth = depth1 < depth2 ? depth1 : depth2;
  while (depth1 > minDepth) {
    depth1--;
    aApzc1 = aApzc1->GetParent();
  }
  while (depth2 > minDepth) {
    depth2--;
    aApzc2 = aApzc2->GetParent();
  }

  // Walk up the ancestor chains of both APZCs, always staying at the same depth for
  // either APZC, and return the the first common ancestor encountered.
  while (true) {
    if (aApzc1 == aApzc2) {
      ancestor = aApzc1;
      break;
    }
    if (depth1 <= 0) {
      break;
    }
    aApzc1 = aApzc1->GetParent();
    aApzc2 = aApzc2->GetParent();
  }
  return ancestor.forget();
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::RootAPZCForLayersId(AsyncPanZoomController* aApzc)
{
  MonitorAutoLock lock(mTreeLock);
  nsRefPtr<AsyncPanZoomController> apzc = aApzc;
  while (apzc && !apzc->IsRootForLayersId()) {
    apzc = apzc->GetParent();
  }
  return apzc.forget();
}

}
}

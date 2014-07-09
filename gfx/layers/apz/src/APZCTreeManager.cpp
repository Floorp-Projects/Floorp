/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManager.h"
#include "Compositor.h"                 // for Compositor
#include "CompositorParent.h"           // for CompositorParent, etc
#include "InputData.h"                  // for InputData, etc
#include "Layers.h"                     // for ContainerLayer, Layer, etc
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "mozilla/dom/Touch.h"          // for Touch
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/layers/AsyncCompositionManager.h" // for ViewTransform
#include "mozilla/layers/AsyncPanZoomController.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/mozalloc.h"           // for operator new
#include "mozilla/TouchEvents.h"
#include "mozilla/Preferences.h"        // for Preferences
#include "nsDebug.h"                    // for NS_WARNING
#include "nsPoint.h"                    // for nsIntPoint
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "mozilla/gfx/Logging.h"        // for gfx::TreeLog
#include "UnitTransforms.h"             // for ViewAs
#include "gfxPrefs.h"                   // for gfxPrefs

#include <algorithm>                    // for std::stable_sort

#define APZCTM_LOG(...)
// #define APZCTM_LOG(...) printf_stderr("APZCTM: " __VA_ARGS__)

namespace mozilla {
namespace layers {

float APZCTreeManager::sDPI = 160.0;

APZCTreeManager::APZCTreeManager()
    : mTreeLock("APZCTreeLock"),
      mInOverscrolledApzc(false),
      mRetainedTouchIdentifier(-1),
      mTouchCount(0),
      mApzcTreeLog("apzctree")
{
  MOZ_ASSERT(NS_IsMainThread());
  AsyncPanZoomController::InitializeGlobalState();
  mApzcTreeLog.ConditionOnPrefFunction(gfxPrefs::APZPrintTree);
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

    nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(spt, nullptr);
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
APZCTreeManager::UpdatePanZoomControllerTree(CompositorParent* aCompositor,
                                             Layer* aRoot,
                                             bool aIsFirstPaint,
                                             uint64_t aOriginatingLayersId,
                                             uint32_t aPaintSequenceNumber)
{
  if (AsyncPanZoomController::GetThreadAssertionsEnabled()) {
    Compositor::AssertOnCompositorThread();
  }

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

  // For testing purposes, we log some data to the APZTestData associated with
  // the layers id that originated this update.
  APZTestData* testData = nullptr;
  if (gfxPrefs::APZTestLoggingEnabled()) {
    if (CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(aOriginatingLayersId)) {
      testData = &state->mApzTestData;
      testData->StartNewPaint(aPaintSequenceNumber);
    }
  }
  APZPaintLogHelper paintLogger(testData, aPaintSequenceNumber);

  if (aRoot) {
    mApzcTreeLog << "[start]\n";
    UpdatePanZoomControllerTree(aCompositor,
                                aRoot,
                                // aCompositor is null in gtest scenarios
                                aCompositor ? aCompositor->RootLayerTreeId() : 0,
                                gfx3DMatrix(), nullptr, nullptr,
                                aIsFirstPaint, aOriginatingLayersId,
                                paintLogger, &apzcsToDestroy);
    mApzcTreeLog << "[end]\n";
  }

  for (size_t i = 0; i < apzcsToDestroy.Length(); i++) {
    APZCTM_LOG("Destroying APZC at %p\n", apzcsToDestroy[i].get());
    apzcsToDestroy[i]->Destroy();
  }
}

AsyncPanZoomController*
APZCTreeManager::UpdatePanZoomControllerTree(CompositorParent* aCompositor,
                                             Layer* aLayer, uint64_t aLayersId,
                                             gfx3DMatrix aTransform,
                                             AsyncPanZoomController* aParent,
                                             AsyncPanZoomController* aNextSibling,
                                             bool aIsFirstPaint,
                                             uint64_t aOriginatingLayersId,
                                             const APZPaintLogHelper& aPaintLogger,
                                             nsTArray< nsRefPtr<AsyncPanZoomController> >* aApzcsToDestroy)
{
  mTreeLock.AssertCurrentThreadOwns();

  ContainerLayer* container = aLayer->AsContainerLayer();
  AsyncPanZoomController* apzc = nullptr;
  mApzcTreeLog << aLayer->Name() << '\t';
  if (container) {
    const FrameMetrics& metrics = container->GetFrameMetrics();
    if (metrics.IsScrollable()) {
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
        ScrollableLayerGuid guid(aLayersId, metrics);
        if (apzc && !apzc->Matches(guid)) {
          apzc = nullptr;
        }

        // If the container doesn't have an APZC already, try to find one of our
        // pre-existing ones that matches. In particular, if we find an APZC whose
        // ScrollableLayerGuid is the same, then we know what happened is that the
        // layout of the page changed causing the layer tree to be rebuilt, but the
        // underlying content for which the APZC was originally created is still
        // there. So it makes sense to pick up that APZC instance again and use it here.
        if (apzc == nullptr) {
          for (size_t i = 0; i < aApzcsToDestroy->Length(); i++) {
            if (aApzcsToDestroy->ElementAt(i)->Matches(guid)) {
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
          if (state->mCrossProcessParent != nullptr) {
            apzc->ShareFrameMetricsAcrossProcesses();
          }
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
        APZCTM_LOG("Using APZC %p for layer %p with identifiers %lld %lld\n", apzc, aLayer, aLayersId, container->GetFrameMetrics().GetScrollId());

        apzc->NotifyLayersUpdated(metrics,
                                  aIsFirstPaint && (aLayersId == aOriginatingLayersId));
        apzc->SetScrollHandoffParentId(container->GetScrollHandoffParentId());

        // Use the composition bounds as the hit test region.
        // Optionally, the GeckoContentController can provide a touch-sensitive
        // region that constrains all frames associated with the controller.
        // In this case we intersect the composition bounds with that region.
        ParentLayerRect visible(metrics.mCompositionBounds);
        CSSRect touchSensitiveRegion;
        if (state->mController->GetTouchSensitiveRegion(&touchSensitiveRegion)) {
          // Note: we assume here that touchSensitiveRegion is in the CSS pixels
          // of our parent layer, which makes this coordinate conversion
          // correct.
          visible = visible.Intersect(touchSensitiveRegion
                                      * metrics.mDevPixelsPerCSSPixel
                                      * metrics.GetParentResolution());
        }
        gfx3DMatrix transform;
        gfx::To3DMatrix(aLayer->GetTransform(), transform);

        apzc->SetLayerHitTestData(visible, aTransform, transform);
        APZCTM_LOG("Setting rect(%f %f %f %f) as visible region for APZC %p\n", visible.x, visible.y,
                                                                              visible.width, visible.height,
                                                                              apzc);

        mApzcTreeLog << "APZC " << guid
                     << "\tcb=" << visible
                     << "\tsr=" << container->GetFrameMetrics().mScrollableRect
                     << (aLayer->GetVisibleRegion().IsEmpty() ? "\tscrollinfo" : "")
                     << "\t" << container->GetFrameMetrics().GetContentDescription();

        // Bind the APZC instance into the tree of APZCs
        if (aNextSibling) {
          aNextSibling->SetPrevSibling(apzc);
        } else if (aParent) {
          aParent->SetLastChild(apzc);
        } else {
          mRootApzc = apzc;
          apzc->MakeRoot();
        }

        // For testing, log the parent scroll id of every APZC that has a
        // parent. This allows test code to reconstruct the APZC tree.
        // Note that we currently only do this for APZCs in the layer tree
        // that originated the update, because the only identifying information
        // we are logging about APZCs is the scroll id, and otherwise we could
        // confuse APZCs from different layer trees with the same scroll id.
        if (aLayersId == aOriginatingLayersId && apzc->GetParent()) {
          aPaintLogger.LogTestData(metrics.GetScrollId(), "parentScrollId",
              apzc->GetParent()->GetGuid().mScrollId);
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
  mApzcTreeLog << '\n';

  // Accumulate the CSS transform between layers that have an APZC, but exclude any
  // any layers that do have an APZC, and reset the accumulation at those layers.
  if (apzc) {
    aTransform = gfx3DMatrix();
  } else {
    // Multiply child layer transforms on the left so they get applied first
    gfx3DMatrix matrix;
    gfx::To3DMatrix(aLayer->GetTransform(), matrix);
    aTransform = matrix * aTransform;
  }

  uint64_t childLayersId = (aLayer->AsRefLayer() ? aLayer->AsRefLayer()->GetReferentId() : aLayersId);
  // If there's no APZC at this level, any APZCs for our child layers will
  // have our siblings as siblings.
  AsyncPanZoomController* next = apzc ? nullptr : aNextSibling;
  for (Layer* child = aLayer->GetLastChild(); child; child = child->GetPrevSibling()) {
    gfx::TreeAutoIndent indent(mApzcTreeLog);
    next = UpdatePanZoomControllerTree(aCompositor, child, childLayersId, aTransform, aParent, next,
                                       aIsFirstPaint, aOriginatingLayersId,
                                       aPaintLogger, aApzcsToDestroy);
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

/*static*/ template<class T> void
TransformScreenToGecko(T* aPoint, AsyncPanZoomController* aApzc, APZCTreeManager* aApzcTm)
{
  gfx3DMatrix transformToApzc, transformToGecko;
  aApzcTm->GetInputTransforms(aApzc, transformToApzc, transformToGecko);
  ApplyTransform(aPoint, transformToApzc * transformToGecko);
}

nsEventStatus
APZCTreeManager::ReceiveInputEvent(InputData& aEvent,
                                   ScrollableLayerGuid* aOutTargetGuid)
{
  nsEventStatus result = nsEventStatus_eIgnore;
  gfx3DMatrix transformToApzc;
  gfx3DMatrix transformToGecko;
  switch (aEvent.mInputType) {
    case MULTITOUCH_INPUT: {
      MultiTouchInput& touchInput = aEvent.AsMultiTouchInput();
      result = ProcessTouchInput(touchInput, aOutTargetGuid);
      break;
    } case PANGESTURE_INPUT: {
      PanGestureInput& panInput = aEvent.AsPanGestureInput();
      bool inOverscrolledApzc = false;
      nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(panInput.mPanStartPoint,
                                                            &inOverscrolledApzc);
      if (apzc) {
        if (panInput.mType == PanGestureInput::PANGESTURE_START ||
            panInput.mType == PanGestureInput::PANGESTURE_MOMENTUMSTART) {
          BuildOverscrollHandoffChain(apzc);
        }

        // When passing the event to the APZC, we need to apply a different
        // transform than the one in TransformScreenToGecko, so we need to
        // make a copy of the event.
        PanGestureInput inputForApzc(panInput);
        GetInputTransforms(apzc, transformToApzc, transformToGecko);
        ApplyTransform(&(inputForApzc.mPanStartPoint), transformToApzc);
        result = apzc->ReceiveInputEvent(inputForApzc);

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);
        TransformScreenToGecko(&(panInput.mPanStartPoint), apzc, this);

        if (panInput.mType == PanGestureInput::PANGESTURE_END ||
            panInput.mType == PanGestureInput::PANGESTURE_MOMENTUMEND) {
          ClearOverscrollHandoffChain();
        }
      }
      break;
    } case PINCHGESTURE_INPUT: {
      PinchGestureInput& pinchInput = aEvent.AsPinchGestureInput();
      bool inOverscrolledApzc = false;
      nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(pinchInput.mFocusPoint,
                                                            &inOverscrolledApzc);
      if (apzc) {
        // When passing the event to the APZC, we need to apply a different
        // transform than the one in TransformScreenToGecko, so we need to
        // make a copy of the event.
        PinchGestureInput inputForApzc(pinchInput);
        GetInputTransforms(apzc, transformToApzc, transformToGecko);
        ApplyTransform(&(inputForApzc.mFocusPoint), transformToApzc);
        apzc->ReceiveInputEvent(inputForApzc);

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);
        TransformScreenToGecko(&(pinchInput.mFocusPoint), apzc, this);
      }
      result = inOverscrolledApzc ? nsEventStatus_eConsumeNoDefault
             : apzc ? nsEventStatus_eConsumeDoDefault
             : nsEventStatus_eIgnore;
      break;
    } case TAPGESTURE_INPUT: {
      TapGestureInput& tapInput = aEvent.AsTapGestureInput();
      bool inOverscrolledApzc = false;
      nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(ScreenPoint(tapInput.mPoint),
                                                            &inOverscrolledApzc);
      if (apzc) {
        // When passing the event to the APZC, we need to apply a different
        // transform than the one in TransformScreenToGecko, so we need to
        // make a copy of the event.
        TapGestureInput inputForApzc(tapInput);
        GetInputTransforms(apzc, transformToApzc, transformToGecko);
        ApplyTransform(&(inputForApzc.mPoint), transformToApzc);
        apzc->ReceiveInputEvent(inputForApzc);

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);
        TransformScreenToGecko(&(tapInput.mPoint), apzc, this);
      }
      result = inOverscrolledApzc ? nsEventStatus_eConsumeNoDefault
             : apzc ? nsEventStatus_eConsumeDoDefault
             : nsEventStatus_eIgnore;
      break;
    }
  }
  return result;
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetTouchInputBlockAPZC(const MultiTouchInput& aEvent,
                                        bool* aOutInOverscrolledApzc)
{
  nsRefPtr<AsyncPanZoomController> apzc;
  if (aEvent.mTouches.Length() == 0) {
    return apzc.forget();
  }

  apzc = GetTargetAPZC(aEvent.mTouches[0].mScreenPoint, aOutInOverscrolledApzc);
  for (size_t i = 1; i < aEvent.mTouches.Length(); i++) {
    nsRefPtr<AsyncPanZoomController> apzc2 = GetTargetAPZC(aEvent.mTouches[i].mScreenPoint, aOutInOverscrolledApzc);
    apzc = CommonAncestor(apzc.get(), apzc2.get());
    APZCTM_LOG("Using APZC %p as the common ancestor\n", apzc.get());
    // For now, we only ever want to do pinching on the root APZC for a given layers id. So
    // when we find the common ancestor of multiple points, also walk up to the root APZC.
    apzc = RootAPZCForLayersId(apzc);
    APZCTM_LOG("Using APZC %p as the root APZC for multi-touch\n", apzc.get());
  }

  // Prepare for possible overscroll handoff.
  BuildOverscrollHandoffChain(apzc);

  return apzc.forget();
}

nsEventStatus
APZCTreeManager::ProcessTouchInput(MultiTouchInput& aInput,
                                   ScrollableLayerGuid* aOutTargetGuid)
{
  if (aInput.mType == MultiTouchInput::MULTITOUCH_START) {
    // If we are in an overscrolled state and a second finger goes down,
    // ignore that second touch point completely. The touch-start for it is
    // dropped completely; subsequent touch events until the touch-end for it
    // will have this touch point filtered out.
    if (mApzcForInputBlock && mApzcForInputBlock->IsOverscrolled()) {
      if (mRetainedTouchIdentifier == -1) {
        mRetainedTouchIdentifier = mApzcForInputBlock->GetLastTouchIdentifier();
      }
      return nsEventStatus_eConsumeNoDefault;
    }

    // NS_TOUCH_START event contains all active touches of the current
    // session thus resetting mTouchCount.
    mTouchCount = aInput.mTouches.Length();
    mInOverscrolledApzc = false;
    nsRefPtr<AsyncPanZoomController> apzc = GetTouchInputBlockAPZC(aInput, &mInOverscrolledApzc);
    if (apzc != mApzcForInputBlock) {
      // If we're moving to a different APZC as our input target, then send a cancel event
      // to the old one so that it clears its internal state. Otherwise it could get left
      // in the middle of a panning touch block (for example) and not clean up properly.
      if (mApzcForInputBlock) {
        MultiTouchInput cancel(MultiTouchInput::MULTITOUCH_CANCEL, 0, TimeStamp::Now(), 0);
        mApzcForInputBlock->ReceiveInputEvent(cancel);
      }
      mApzcForInputBlock = apzc;
    }

    if (mApzcForInputBlock) {
      // Cache apz transform so it can be used for future events in this block.
      gfx3DMatrix transformToGecko;
      GetInputTransforms(mApzcForInputBlock, mCachedTransformToApzcForInputBlock, transformToGecko);
    } else {
      // Reset the cached apz transform
      mCachedTransformToApzcForInputBlock = gfx3DMatrix();
    }
  } else if (mApzcForInputBlock) {
    APZCTM_LOG("Re-using APZC %p as continuation of event block\n", mApzcForInputBlock.get());
  }

  // If we receive a touch-cancel, it means all touches are finished, so we
  // can stop ignoring any that we were ignoring.
  if (aInput.mType == MultiTouchInput::MULTITOUCH_CANCEL) {
    mRetainedTouchIdentifier = -1;
  }

  // If we are currently ignoring any touch points, filter them out from the
  // set of touch points included in this event. Note that we modify aInput
  // itself, so that the touch points are also filtered out when the caller
  // passes the event on to content.
  if (mRetainedTouchIdentifier != -1) {
    for (size_t j = 0; j < aInput.mTouches.Length(); ++j) {
      if (aInput.mTouches[j].mIdentifier != mRetainedTouchIdentifier) {
        aInput.mTouches.RemoveElementAt(j);
        --j;
      }
    }
    if (aInput.mTouches.IsEmpty()) {
      return nsEventStatus_eConsumeNoDefault;
    }
  }

  if (mApzcForInputBlock) {
    mApzcForInputBlock->GetGuid(aOutTargetGuid);
    // For computing the input for the APZC, used the cached transform.
    // This ensures that the sequence of touch points an APZC sees in an
    // input block are all in the same coordinate space.
    gfx3DMatrix transformToApzc = mCachedTransformToApzcForInputBlock;
    MultiTouchInput inputForApzc(aInput);
    for (size_t i = 0; i < inputForApzc.mTouches.Length(); i++) {
      ApplyTransform(&(inputForApzc.mTouches[i].mScreenPoint), transformToApzc);
    }
    mApzcForInputBlock->ReceiveInputEvent(inputForApzc);

    // For computing the event to pass back to Gecko, use the up-to-date transforms.
    // This ensures that transformToApzc and transformToGecko are in sync
    // (note that transformToGecko isn't cached).
    gfx3DMatrix transformToGecko;
    GetInputTransforms(mApzcForInputBlock, transformToApzc, transformToGecko);
    gfx3DMatrix outTransform = transformToApzc * transformToGecko;
    for (size_t i = 0; i < aInput.mTouches.Length(); i++) {
      ApplyTransform(&(aInput.mTouches[i].mScreenPoint), outTransform);
    }
  }

  nsEventStatus result = mInOverscrolledApzc ? nsEventStatus_eConsumeNoDefault
                       : mApzcForInputBlock ? nsEventStatus_eConsumeDoDefault
                       : nsEventStatus_eIgnore;

  if (aInput.mType == MultiTouchInput::MULTITOUCH_END) {
    if (mTouchCount >= aInput.mTouches.Length()) {
      // NS_TOUCH_END event contains only released touches thus decrementing.
      mTouchCount -= aInput.mTouches.Length();
    } else {
      NS_WARNING("Got an unexpected touchend/touchcancel");
      mTouchCount = 0;
    }
  } else if (aInput.mType == MultiTouchInput::MULTITOUCH_CANCEL) {
    mTouchCount = 0;
  }

  // If it's the end of the touch sequence then clear out variables so we
  // keep dangling references and leak things.
  if (mTouchCount == 0) {
    mApzcForInputBlock = nullptr;
    mInOverscrolledApzc = false;
    mRetainedTouchIdentifier = -1;
    ClearOverscrollHandoffChain();
  }

  return result;
}

void
APZCTreeManager::TransformCoordinateToGecko(const ScreenIntPoint& aPoint,
                                            LayoutDeviceIntPoint* aOutTransformedPoint)
{
  MOZ_ASSERT(aOutTransformedPoint);
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aPoint, nullptr);
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
APZCTreeManager::ProcessEvent(WidgetInputEvent& aEvent,
                              ScrollableLayerGuid* aOutTargetGuid)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Transform the refPoint.
  // If the event hits an overscrolled APZC, instruct the caller to ignore it.
  bool inOverscrolledApzc = false;
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(ScreenPoint(aEvent.refPoint.x, aEvent.refPoint.y),
                                                        &inOverscrolledApzc);
  if (apzc) {
    apzc->GetGuid(aOutTargetGuid);
    gfx3DMatrix transformToApzc;
    gfx3DMatrix transformToGecko;
    GetInputTransforms(apzc, transformToApzc, transformToGecko);
    gfx3DMatrix outTransform = transformToApzc * transformToGecko;
    ApplyTransform(&(aEvent.refPoint), outTransform);
  }
  return inOverscrolledApzc ? nsEventStatus_eConsumeNoDefault
       : apzc ? nsEventStatus_eConsumeDoDefault
       : nsEventStatus_eIgnore;
}

nsEventStatus
APZCTreeManager::ReceiveInputEvent(WidgetInputEvent& aEvent,
                                   ScrollableLayerGuid* aOutTargetGuid)
{
  // This function will be removed as part of bug 930939.
  // In general it is preferable to use the version of ReceiveInputEvent
  // that takes an InputData, as that is usable from off-main-thread.

  MOZ_ASSERT(NS_IsMainThread());

  switch (aEvent.eventStructType) {
    case NS_TOUCH_EVENT: {
      WidgetTouchEvent& touchEvent = *aEvent.AsTouchEvent();
      MultiTouchInput touchInput(touchEvent);
      nsEventStatus result = ProcessTouchInput(touchInput, aOutTargetGuid);
      // touchInput was modified in-place to possibly remove some
      // touch points (if we are overscrolled), and the coordinates were
      // modified using the APZ untransform. We need to copy these changes
      // back into the WidgetInputEvent.
      touchEvent.touches.Clear();
      touchEvent.touches.SetCapacity(touchInput.mTouches.Length());
      for (size_t i = 0; i < touchInput.mTouches.Length(); i++) {
        *touchEvent.touches.AppendElement() = touchInput.mTouches[i].ToNewDOMTouch();
      }
      return result;
    }
    default: {
      return ProcessEvent(aEvent, aOutTargetGuid);
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

/**
 * Transform a displacement from the screen coordinates of a source APZC to
 * the screen coordinates of a target APZC.
 * @param aTreeManager the tree manager for the APZC tree containing |aSource|
 *                     and |aTarget|
 * @param aSource the source APZC
 * @param aTarget the target APZC
 * @param aStartPoint the start point of the displacement
 * @param aEndPoint the end point of the displacement
 */
static void
TransformDisplacement(APZCTreeManager* aTreeManager,
                      AsyncPanZoomController* aSource,
                      AsyncPanZoomController* aTarget,
                      ScreenPoint& aStartPoint,
                      ScreenPoint& aEndPoint) {
  gfx3DMatrix transformToApzc;
  gfx3DMatrix transformToGecko;  // ignored

  // Convert start and end points to untransformed screen coordinates.
  aTreeManager->GetInputTransforms(aSource, transformToApzc, transformToGecko);
  ApplyTransform(&aStartPoint, transformToApzc.Inverse());
  ApplyTransform(&aEndPoint, transformToApzc.Inverse());

  // Convert start and end points to aTarget's transformed screen coordinates.
  aTreeManager->GetInputTransforms(aTarget, transformToApzc, transformToGecko);
  ApplyTransform(&aStartPoint, transformToApzc);
  ApplyTransform(&aEndPoint, transformToApzc);
}

bool
APZCTreeManager::DispatchScroll(AsyncPanZoomController* aPrev, ScreenPoint aStartPoint, ScreenPoint aEndPoint,
                                uint32_t aOverscrollHandoffChainIndex)
{
  nsRefPtr<AsyncPanZoomController> next;
  {
    // Grab tree lock to protect mOverscrollHandoffChain from concurrent
    // access from the input and compositor threads.
    // Release it before calling TransformDisplacement() as that grabs the
    // lock itself.
    MonitorAutoLock lock(mTreeLock);

    // If we have reached the end of the overscroll handoff chain, there is
    // nothing more to scroll, so we ignore the rest of the pan gesture.
    if (aOverscrollHandoffChainIndex >= mOverscrollHandoffChain.length()) {
      // Nothing more to scroll - ignore the rest of the pan gesture.
      return false;
    }

    next = mOverscrollHandoffChain[aOverscrollHandoffChainIndex];
  }

  if (next == nullptr) {
    return false;
  }

  // Convert the start and end points from |aPrev|'s coordinate space to
  // |next|'s coordinate space. Since |aPrev| may be the same as |next|
  // (if |aPrev| is the APZC that is initiating the scroll and there is no
  // scroll grabbing to grab the scroll from it), don't bother doing the
  // transformations in that case.
  if (next != aPrev) {
    TransformDisplacement(this, aPrev, next, aStartPoint, aEndPoint);
  }

  // Scroll |next|. If this causes overscroll, it will call DispatchScroll()
  // again with an incremented index.
  return next->AttemptScroll(aStartPoint, aEndPoint, aOverscrollHandoffChainIndex);
}

bool
APZCTreeManager::HandOffFling(AsyncPanZoomController* aPrev, ScreenPoint aVelocity)
{
  // Build the overscroll handoff chain. This is necessary because it is
  // otherwise built on touch-start and cleared on touch-end, and a fling
  // happens after touch-end. Note that, unlike DispatchScroll() which is
  // called on every touch-move during overscroll panning,
  // HandOffFling() is only called once during a fling handoff,
  // so it's not worth trying to avoid building the handoff chain here.
  BuildOverscrollHandoffChain(aPrev);

  nsRefPtr<AsyncPanZoomController> next;  // will be used outside monitor block
  {
    // Grab tree lock to protect mOverscrollHandoffChain from concurrent
    // access from the input and compositor threads.
    // Release it before calling GetInputTransforms() as that grabs the
    // lock itself.
    MonitorAutoLock lock(mTreeLock);

    // Find |aPrev| in the handoff chain.
    uint32_t i;
    for (i = 0; i < mOverscrollHandoffChain.length(); ++i) {
      if (mOverscrollHandoffChain[i] == aPrev) {
        break;
      }
    }

    // Get the next APZC in the handoff chain, if any.
    if (i + 1 < mOverscrollHandoffChain.length()) {
      next = mOverscrollHandoffChain[i + 1];
    }

    // Clear the handoff chain so we don't maintain references to APZCs
    // unnecessarily.
    mOverscrollHandoffChain.clear();
  }

  // Nothing to hand off fling to.
  if (next == nullptr) {
    return false;
  }

  // The fling's velocity needs to be transformed from the screen coordinates
  // of |aPrev| to the screen coordinates of |next|. To transform a velocity
  // correctly, we need to convert it to a displacement. For now, we do this
  // by anchoring it to a start point of (0, 0).
  // TODO: For this to be correct in the presence of 3D transforms, we should
  // use the end point of the touch that started the fling as the start point
  // rather than (0, 0).
  ScreenPoint startPoint;  // (0, 0)
  ScreenPoint endPoint = startPoint + aVelocity;
  TransformDisplacement(this, aPrev, next, startPoint, endPoint);
  ScreenPoint transformedVelocity = endPoint - startPoint;

  // Tell |next| to start a fling with the transformed velocity.
  return next->TakeOverFling(transformedVelocity);
}

bool
APZCTreeManager::FlushRepaintsForOverscrollHandoffChain()
{
  MonitorAutoLock lock(mTreeLock);  // to access mOverscrollHandoffChain
  if (mOverscrollHandoffChain.length() == 0) {
    return false;
  }
  for (uint32_t i = 0; i < mOverscrollHandoffChain.length(); i++) {
    nsRefPtr<AsyncPanZoomController> item = mOverscrollHandoffChain[i];
    if (item) {
      item->FlushRepaintForOverscrollHandoff();
    }
  }
  return true;
}

bool
APZCTreeManager::CanBePanned(AsyncPanZoomController* aApzc)
{
  MonitorAutoLock lock(mTreeLock);  // to access mOverscrollHandoffChain

  // Find |aApzc| in the handoff chain.
  uint32_t i;
  for (i = 0; i < mOverscrollHandoffChain.length(); ++i) {
    if (mOverscrollHandoffChain[i] == aApzc) {
      break;
    }
  }

  // See whether any APZC in the handoff chain starting from |aApzc|
  // has room to be panned.
  for (uint32_t j = i; j < mOverscrollHandoffChain.length(); ++j) {
    if (mOverscrollHandoffChain[j]->IsPannable()) {
      return true;
    }
  }

  return false;
}

bool
APZCTreeManager::HitTestAPZC(const ScreenIntPoint& aPoint)
{
  nsRefPtr<AsyncPanZoomController> target = GetTargetAPZC(aPoint, nullptr);
  return target != nullptr;
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
APZCTreeManager::GetTargetAPZC(const ScreenPoint& aPoint, bool* aOutInOverscrolledApzc)
{
  MonitorAutoLock lock(mTreeLock);
  nsRefPtr<AsyncPanZoomController> target;
  // The root may have siblings, so check those too
  gfxPoint point(aPoint.x, aPoint.y);
  for (AsyncPanZoomController* apzc = mRootApzc; apzc; apzc = apzc->GetPrevSibling()) {
    target = GetAPZCAtPoint(apzc, point, aOutInOverscrolledApzc);
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

  // Grab tree lock to protect mOverscrollHandoffChain from concurrent
  // access between the input and compositor threads.
  MonitorAutoLock lock(mTreeLock);

  mOverscrollHandoffChain.clear();

  // Build the chain. If there is a scroll parent link, we use that. This is
  // needed to deal with scroll info layers, because they participate in handoff
  // but do not follow the expected layer tree structure. If there are no
  // scroll parent links we just walk up the tree to find the scroll parent.
  AsyncPanZoomController* apzc = aInitialTarget;
  while (apzc != nullptr) {
    if (!mOverscrollHandoffChain.append(apzc)) {
      NS_WARNING("Vector::append failed");
      mOverscrollHandoffChain.clear();
      return;
    }
    APZCTM_LOG("mOverscrollHandoffChain[%d] = %p\n", mOverscrollHandoffChain.length(), apzc);

    if (apzc->GetScrollHandoffParentId() == FrameMetrics::NULL_SCROLL_ID) {
      if (!apzc->IsRootForLayersId()) {
        // This probably indicates a bug or missed case in layout code
        NS_WARNING("Found a non-root APZ with no handoff parent");
      }
      apzc = apzc->GetParent();
      continue;
    }

    // Find the AsyncPanZoomController instance with a matching layersId and
    // the scroll id that matches apzc->GetScrollHandoffParentId(). To do this
    // search the subtree with the same layersId for the apzc with the specified
    // scroll id.
    AsyncPanZoomController* scrollParent = nullptr;
    AsyncPanZoomController* parent = apzc;
    while (!parent->IsRootForLayersId()) {
      parent = parent->GetParent();
      // While walking up to find the root of the subtree, if we encounter the
      // handoff parent, we don't actually need to do the search so we can
      // just abort here.
      if (parent->GetGuid().mScrollId == apzc->GetScrollHandoffParentId()) {
        scrollParent = parent;
        break;
      }
    }
    if (!scrollParent) {
      scrollParent = FindTargetAPZC(parent, apzc->GetScrollHandoffParentId());
    }
    apzc = scrollParent;
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

/* Find the apzc in the subtree rooted at aApzc that has the same layers id as
   aApzc, and that has the given scroll id. Generally this function should be called
   with aApzc being the root of its layers id subtree. */
AsyncPanZoomController*
APZCTreeManager::FindTargetAPZC(AsyncPanZoomController* aApzc, FrameMetrics::ViewID aScrollId)
{
  mTreeLock.AssertCurrentThreadOwns();

  if (aApzc->GetGuid().mScrollId == aScrollId) {
    return aApzc;
  }
  for (AsyncPanZoomController* child = aApzc->GetLastChild(); child; child = child->GetPrevSibling()) {
    if (child->GetGuid().mLayersId != aApzc->GetGuid().mLayersId) {
      continue;
    }
    AsyncPanZoomController* match = FindTargetAPZC(child, aScrollId);
    if (match) {
      return match;
    }
  }

  return nullptr;
}

void
APZCTreeManager::ClearOverscrollHandoffChain()
{
  MonitorAutoLock lock(mTreeLock);
  mOverscrollHandoffChain.clear();
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
APZCTreeManager::GetAPZCAtPoint(AsyncPanZoomController* aApzc,
                                const gfxPoint& aHitTestPoint,
                                bool* aOutInOverscrolledApzc)
{
  mTreeLock.AssertCurrentThreadOwns();

  // The comments below assume there is a chain of layers L..R with L and P having APZC instances as
  // explained in the comment on GetInputTransforms. This function will recurse with aApzc at L and P, and the
  // comments explain what values are stored in the variables at these two levels. All the comments
  // use standard matrix notation where the leftmost matrix in a multiplication is applied first.

  // ancestorUntransform takes points from aApzc's parent APZC's layer coordinates
  // to aApzc's parent layer's layer coordinates.
  // It is OC.Inverse() * NC.Inverse() * MC.Inverse() at recursion level for L,
  //   and RC.Inverse() * QC.Inverse()                at recursion level for P.
  gfx3DMatrix ancestorUntransform = aApzc->GetAncestorTransform().Inverse();

  // Hit testing for this layer takes place in our parent layer coordinates,
  // since the composition bounds (used to initialize the visible rect against
  // which we hit test are in those coordinates).
  gfxPointH3D hitTestPointForThisLayer = ancestorUntransform.ProjectPoint(aHitTestPoint);
  APZCTM_LOG("Untransformed %f %f to transient coordinates %f %f for hit-testing APZC %p\n",
           aHitTestPoint.x, aHitTestPoint.y,
           hitTestPointForThisLayer.x, hitTestPointForThisLayer.y, aApzc);

  // childUntransform takes points from aApzc's parent APZC's layer coordinates
  // to aApzc's layer coordinates (which are aApzc's children's ParentLayer coordinates).
  // It is OC.Inverse() * NC.Inverse() * MC.Inverse() * LC.Inverse() * LA.Inverse() at L
  //   and RC.Inverse() * QC.Inverse() * PC.Inverse() * PA.Inverse()                at P.
  gfx3DMatrix childUntransform = ancestorUntransform
                               * aApzc->GetCSSTransform().Inverse()
                               * gfx3DMatrix(aApzc->GetCurrentAsyncTransform()).Inverse();
  gfxPointH3D hitTestPointForChildLayers = childUntransform.ProjectPoint(aHitTestPoint);
  APZCTM_LOG("Untransformed %f %f to layer coordinates %f %f for APZC %p\n",
           aHitTestPoint.x, aHitTestPoint.y,
           hitTestPointForChildLayers.x, hitTestPointForChildLayers.y, aApzc);

  AsyncPanZoomController* result = nullptr;
  // This walks the tree in depth-first, reverse order, so that it encounters
  // APZCs front-to-back on the screen.
  if (hitTestPointForChildLayers.HasPositiveWCoord()) {
    for (AsyncPanZoomController* child = aApzc->GetLastChild(); child; child = child->GetPrevSibling()) {
      AsyncPanZoomController* match = GetAPZCAtPoint(child, hitTestPointForChildLayers.As2DPoint(), aOutInOverscrolledApzc);
      if (match) {
        result = match;
        break;
      }
    }
  }
  if (!result && hitTestPointForThisLayer.HasPositiveWCoord() &&
      aApzc->VisibleRegionContains(ViewAs<ParentLayerPixel>(hitTestPointForThisLayer.As2DPoint()))) {
    APZCTM_LOG("Successfully matched untransformed point %f %f to visible region for APZC %p\n",
             hitTestPointForThisLayer.x, hitTestPointForThisLayer.y, aApzc);
    result = aApzc;
  }

  // If we are overscrolled, and the point matches us or one of our children,
  // the result is inside an overscrolled APZC, inform our caller of this
  // (callers typically ignore events targeted at overscrolled APZCs).
  if (result && aApzc->IsOverscrolled()) {
    if (aOutInOverscrolledApzc) {
      *aOutInOverscrolledApzc = true;
    }
    result = nullptr;
  }

  return result;
}

/* This function sets the aTransformToApzcOut and aTransformToGeckoOut out-parameters
   to some useful transformations that input events may need applied. This is best
   illustrated with an example. Consider a chain of layers, L, M, N, O, P, Q, R. Layer L
   is the layer that corresponds to the argument |aApzc|, and layer R is the root
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
   doing display-list-based hit-testing for event dispatching.
   Furthermore, because these input events are processed by Gecko in a FIFO queue that
   includes other things (specifically paint requests), it is possible that by time the
   input event reaches gecko, it will have painted something else. Therefore, we need to
   apply another transform to the input events to account for the possible disparity between
   what we know gecko last painted and the last paint request we sent to gecko. Let this
   transform be represented by LD, MD, ... RD.
   Therefore, given a user input in screen space, the following transforms need to be applied
   (in order from top to bottom):
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
        LD
        LC
        MD
        MC
        ...
        RD
        RC
   This sequence can be simplified and refactored to the following:
        aTransformToApzcOut
        LT.Inverse()
        LD
        LC
        MD
        MC
        ...
        RD
        RC
   Since aTransformToApzcOut is already one of the out-parameters, we set aTransformToGeckoOut
   to the remaining transforms (LT.Inverse() * LD * ... * RC), so that the caller code can
   combine it with aTransformToApzcOut to get the final transform required in this case.

   Note that for many of these layers, there will be no AsyncPanZoomController attached, and
   so the async transform will be the identity transform. So, in the example above, if layers
   L and P have APZC instances attached, MT, MN, MD, NT, NN, ND, OT, ON, OD, QT, QN, QD, RT,
   RN and RD will be identity transforms.
   Additionally, for space-saving purposes, each APZC instance stores its layer's individual
   CSS transform and the accumulation of CSS transforms to its parent APZC. So the APZC for
   layer L would store LC and (MC * NC * OC), and the layer P would store PC and (QC * RC).
   The APZC instances track the last dispatched paint request and so are able to calculate LD and
   PD using those internally stored values.
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
  // aTransformToGeckoOut is initialized to LT.Inverse() * LD * LC * MC * NC * OC
  aTransformToGeckoOut = transientAsyncUntransform * aApzc->GetTransformToLastDispatchedPaint() * aApzc->GetCSSTransform() * aApzc->GetAncestorTransform();

  for (AsyncPanZoomController* parent = aApzc->GetParent(); parent; parent = parent->GetParent()) {
    // ancestorUntransform is updated to RC.Inverse() * QC.Inverse() when parent == P
    ancestorUntransform = parent->GetAncestorTransform().Inverse();
    // asyncUntransform is updated to PA.Inverse() when parent == P
    asyncUntransform = gfx3DMatrix(parent->GetCurrentAsyncTransform()).Inverse();
    // untransformSinceLastApzc is RC.Inverse() * QC.Inverse() * PC.Inverse() * PA.Inverse()
    gfx3DMatrix untransformSinceLastApzc = ancestorUntransform * parent->GetCSSTransform().Inverse() * asyncUntransform;

    // aTransformToApzcOut is RC.Inverse() * QC.Inverse() * PC.Inverse() * PA.Inverse() * OC.Inverse() * NC.Inverse() * MC.Inverse() * LC.Inverse() * LN.Inverse()
    aTransformToApzcOut = untransformSinceLastApzc * aTransformToApzcOut;
    // aTransformToGeckoOut is LT.Inverse() * LD * LC * MC * NC * OC * PD * PC * QC * RC
    aTransformToGeckoOut = aTransformToGeckoOut * parent->GetTransformToLastDispatchedPaint() * parent->GetCSSTransform() * parent->GetAncestorTransform();

    // The above values for aTransformToApzcOut and aTransformToGeckoOut when parent == P match
    // the required output as explained in the comment above this method. Note that any missing
    // terms are guaranteed to be identity transforms.
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

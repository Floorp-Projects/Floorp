/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManager.h"
#include "AsyncPanZoomController.h"
#include "Compositor.h"                 // for Compositor
#include "InputBlockState.h"            // for InputBlockState
#include "InputData.h"                  // for InputData, etc
#include "Layers.h"                     // for Layer, etc
#include "mozilla/dom/Touch.h"          // for Touch
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/layers/AsyncCompositionManager.h" // for ViewTransform
#include "mozilla/layers/CompositorParent.h" // for CompositorParent, etc
#include "mozilla/layers/LayerMetricsWrapper.h"
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
#include "OverscrollHandoffState.h"     // for OverscrollHandoffState
#include "LayersLogging.h"              // for Stringify

#define APZCTM_LOG(...)
// #define APZCTM_LOG(...) printf_stderr("APZCTM: " __VA_ARGS__)

namespace mozilla {
namespace layers {

typedef mozilla::gfx::Point Point;
typedef mozilla::gfx::Point4D Point4D;
typedef mozilla::gfx::Matrix4x4 Matrix4x4;

float APZCTreeManager::sDPI = 160.0;

struct APZCTreeManager::TreeBuildingState {
  TreeBuildingState(CompositorParent* aCompositor,
                    bool aIsFirstPaint, uint64_t aOriginatingLayersId,
                    APZTestData* aTestData, uint32_t aPaintSequence)
    : mCompositor(aCompositor)
    , mIsFirstPaint(aIsFirstPaint)
    , mOriginatingLayersId(aOriginatingLayersId)
    , mPaintLogger(aTestData, aPaintSequence)
  {
  }

  // State that doesn't change as we recurse in the tree building
  CompositorParent* const mCompositor;
  const bool mIsFirstPaint;
  const uint64_t mOriginatingLayersId;
  const APZPaintLogHelper mPaintLogger;

  // State that is updated as we perform the tree build
  nsTArray< nsRefPtr<AsyncPanZoomController> > mApzcsToDestroy;
  std::map<ScrollableLayerGuid, AsyncPanZoomController*> mApzcMap;
  nsTArray<EventRegions> mEventRegions;
};

/*static*/ const ScreenMargin
APZCTreeManager::CalculatePendingDisplayPort(
  const FrameMetrics& aFrameMetrics,
  const ParentLayerPoint& aVelocity,
  double aEstimatedPaintDuration)
{
  return AsyncPanZoomController::CalculatePendingDisplayPort(
    aFrameMetrics, aVelocity, aEstimatedPaintDuration);
}

APZCTreeManager::APZCTreeManager()
    : mInputQueue(new InputQueue()),
      mTreeLock("APZCTreeLock"),
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
APZCTreeManager::SetAllowedTouchBehavior(uint64_t aInputBlockId,
                                         const nsTArray<TouchBehaviorFlags> &aValues)
{
  mInputQueue->SetAllowedTouchBehavior(aInputBlockId, aValues);
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

  // For testing purposes, we log some data to the APZTestData associated with
  // the layers id that originated this update.
  APZTestData* testData = nullptr;
  if (gfxPrefs::APZTestLoggingEnabled()) {
    if (CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(aOriginatingLayersId)) {
      testData = &state->mApzTestData;
      testData->StartNewPaint(aPaintSequenceNumber);
    }
  }

  TreeBuildingState state(aCompositor, aIsFirstPaint, aOriginatingLayersId,
                          testData, aPaintSequenceNumber);

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
  Collect(mRootApzc, &state.mApzcsToDestroy);
  mRootApzc = nullptr;

  if (aRoot) {
    mApzcTreeLog << "[start]\n";
    LayerMetricsWrapper root(aRoot);
    UpdatePanZoomControllerTree(state, root,
                                // aCompositor is null in gtest scenarios
                                aCompositor ? aCompositor->RootLayerTreeId() : 0,
                                Matrix4x4(), nullptr, nullptr, nsIntRegion());
    mApzcTreeLog << "[end]\n";
  }
  MOZ_ASSERT(state.mEventRegions.Length() == 0);

  for (size_t i = 0; i < state.mApzcsToDestroy.Length(); i++) {
    APZCTM_LOG("Destroying APZC at %p\n", state.mApzcsToDestroy[i].get());
    state.mApzcsToDestroy[i]->Destroy();
  }
}

static nsIntRegion
ComputeTouchSensitiveRegion(GeckoContentController* aController,
                            const FrameMetrics& aMetrics,
                            const nsIntRegion& aObscured)
{
  // Use the composition bounds as the hit test region.
  // Optionally, the GeckoContentController can provide a touch-sensitive
  // region that constrains all frames associated with the controller.
  // In this case we intersect the composition bounds with that region.
  ParentLayerRect visible(aMetrics.mCompositionBounds);
  CSSRect touchSensitiveRegion;
  if (aController->GetTouchSensitiveRegion(&touchSensitiveRegion)) {
    // Here we assume 'touchSensitiveRegion' is in the CSS pixels of the
    // parent frame. To convert it to ParentLayer pixels, we therefore need
    // the cumulative resolution of the parent frame. We approximate this as
    // the quotient of our cumulative resolution and our pres shell resolution;
    // this approximation may not be accurate in the presence of a css-driven
    // resolution.
    LayoutDeviceToParentLayerScale parentCumulativeResolution =
          aMetrics.mCumulativeResolution
        / ParentLayerToLayerScale(aMetrics.mPresShellResolution);
    visible = visible.Intersect(touchSensitiveRegion
                                * aMetrics.mDevPixelsPerCSSPixel
                                * parentCumulativeResolution);
  }

  // Not sure what rounding option is the most correct here, but if we ever
  // figure it out we can change this. For now I'm rounding in to minimize
  // the chances of getting a complex region.
  nsIntRegion unobscured;
  unobscured.Sub(ParentLayerIntRect::ToUntyped(RoundedIn(visible)), aObscured);
  return unobscured;
}

void
APZCTreeManager::PrintAPZCInfo(const LayerMetricsWrapper& aLayer,
                               const AsyncPanZoomController* apzc)
{
  const FrameMetrics& metrics = aLayer.Metrics();
  mApzcTreeLog << "APZC " << apzc->GetGuid() << "\tcb=" << metrics.mCompositionBounds
               << "\tsr=" << metrics.mScrollableRect
               << (aLayer.IsScrollInfoLayer() ? "\tscrollinfo" : "")
               << (apzc->HasScrollgrab() ? "\tscrollgrab" : "") << "\t"
               << metrics.GetContentDescription().get();
}

AsyncPanZoomController*
APZCTreeManager::PrepareAPZCForLayer(const LayerMetricsWrapper& aLayer,
                                     const FrameMetrics& aMetrics,
                                     uint64_t aLayersId,
                                     const gfx::Matrix4x4& aAncestorTransform,
                                     const nsIntRegion& aObscured,
                                     AsyncPanZoomController* aParent,
                                     AsyncPanZoomController* aNextSibling,
                                     TreeBuildingState& aState)
{
  if (!aMetrics.IsScrollable()) {
    return nullptr;
  }

  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(aLayersId);
  if (!(state && state->mController.get())) {
    return nullptr;
  }

  AsyncPanZoomController* apzc = nullptr;
  // If we get here, aLayer is a scrollable layer and somebody
  // has registered a GeckoContentController for it, so we need to ensure
  // it has an APZC instance to manage its scrolling.

  // aState.mApzcMap allows reusing the exact same APZC instance for different layers
  // with the same FrameMetrics data. This is needed because in some cases content
  // that is supposed to scroll together is split into multiple layers because of
  // e.g. non-scrolling content interleaved in z-index order.
  ScrollableLayerGuid guid(aLayersId, aMetrics);
  auto insertResult = aState.mApzcMap.insert(std::make_pair(guid, static_cast<AsyncPanZoomController*>(nullptr)));
  if (!insertResult.second) {
    apzc = insertResult.first->second;
    PrintAPZCInfo(aLayer, apzc);
  }
  APZCTM_LOG("Found APZC %p for layer %p with identifiers %" PRId64 " %" PRId64 "\n", apzc, aLayer.GetLayer(), guid.mLayersId, guid.mScrollId);

  // If we haven't encountered a layer already with the same metrics, then we need to
  // do the full reuse-or-make-an-APZC algorithm, which is contained inside the block
  // below.
  if (apzc == nullptr) {
    apzc = aLayer.GetApzc();

    // If the content represented by the scrollable layer has changed (which may
    // be possible because of DLBI heuristics) then we don't want to keep using
    // the same old APZC for the new content. Null it out so we run through the
    // code to find another one or create one.
    if (apzc && !apzc->Matches(guid)) {
      apzc = nullptr;
    }

    // If the layer doesn't have an APZC already, try to find one of our
    // pre-existing ones that matches. In particular, if we find an APZC whose
    // ScrollableLayerGuid is the same, then we know what happened is that the
    // layout of the page changed causing the layer tree to be rebuilt, but the
    // underlying content for which the APZC was originally created is still
    // there. So it makes sense to pick up that APZC instance again and use it here.
    if (apzc == nullptr) {
      for (size_t i = 0; i < aState.mApzcsToDestroy.Length(); i++) {
        if (aState.mApzcsToDestroy.ElementAt(i)->Matches(guid)) {
          apzc = aState.mApzcsToDestroy.ElementAt(i);
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
      apzc = new AsyncPanZoomController(aLayersId, this, mInputQueue, state->mController,
                                        AsyncPanZoomController::USE_GESTURE_DETECTOR);
      apzc->SetCompositorParent(aState.mCompositor);
      if (state->mCrossProcessParent != nullptr) {
        apzc->ShareFrameMetricsAcrossProcesses();
      }
    } else {
      // If there was already an APZC for the layer clear the tree pointers
      // so that it doesn't continue pointing to APZCs that should no longer
      // be in the tree. These pointers will get reset properly as we continue
      // building the tree. Also remove it from the set of APZCs that are going
      // to be destroyed, because it's going to remain active.
      aState.mApzcsToDestroy.RemoveElement(apzc);
      apzc->SetPrevSibling(nullptr);
      apzc->SetLastChild(nullptr);
    }
    APZCTM_LOG("Using APZC %p for layer %p with identifiers %" PRId64 " %" PRId64 "\n", apzc, aLayer.GetLayer(), aLayersId, aMetrics.GetScrollId());

    apzc->NotifyLayersUpdated(aMetrics,
        aState.mIsFirstPaint && (aLayersId == aState.mOriginatingLayersId));

    nsIntRegion unobscured;
    if (!gfxPrefs::LayoutEventRegionsEnabled()) {
      unobscured = ComputeTouchSensitiveRegion(state->mController, aMetrics, aObscured);
    }
    apzc->SetLayerHitTestData(EventRegions(unobscured), aAncestorTransform);
    APZCTM_LOG("Setting region %s as visible region for APZC %p\n",
        Stringify(unobscured).c_str(), apzc);

    PrintAPZCInfo(aLayer, apzc);

    // Bind the APZC instance into the tree of APZCs
    if (aNextSibling) {
      aNextSibling->SetPrevSibling(apzc);
    } else if (aParent) {
      aParent->SetLastChild(apzc);
    } else {
      MOZ_ASSERT(!mRootApzc);
      mRootApzc = apzc;
      apzc->MakeRoot();
    }

    // For testing, log the parent scroll id of every APZC that has a
    // parent. This allows test code to reconstruct the APZC tree.
    // Note that we currently only do this for APZCs in the layer tree
    // that originated the update, because the only identifying information
    // we are logging about APZCs is the scroll id, and otherwise we could
    // confuse APZCs from different layer trees with the same scroll id.
    if (aLayersId == aState.mOriginatingLayersId && apzc->GetParent()) {
      aState.mPaintLogger.LogTestData(aMetrics.GetScrollId(), "parentScrollId",
          apzc->GetParent()->GetGuid().mScrollId);
    }

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

    insertResult.first->second = apzc;
  } else {
    // We already built an APZC earlier in this tree walk, but we have another layer
    // now that will also be using that APZC. The hit-test region on the APZC needs
    // to be updated to deal with the new layer's hit region.
    // FIXME: Combining this hit test region to the existing hit test region has a bit
    // of a problem, because it assumes the z-index of this new region is the same as
    // the z-index of the old region (from the previous layer with the same scrollid)
    // when in fact that may not be the case.
    // Consider the case where we have three layers: A, B, and C. A is at the top in
    // z-order and C is at the bottom. A and C share a scrollid and scroll together; but
    // B has a different scrollid and scrolls independently. Depending on how B moves
    // and the async transform on it, a larger/smaller area of C may be unobscured.
    // However, when we combine the hit regions of A and C here we are ignoring the async
    // async transform and so we basically assume the same amount of C is always visible
    // on top of B. Fixing this doesn't appear to be very easy so I'm leaving it for
    // now in the hopes that we won't run into this problem a lot.
    if (!gfxPrefs::LayoutEventRegionsEnabled()) {
      nsIntRegion unobscured = ComputeTouchSensitiveRegion(state->mController, aMetrics, aObscured);
      apzc->AddHitTestRegions(EventRegions(unobscured));
      APZCTM_LOG("Adding region %s to visible region of APZC %p\n", Stringify(unobscured).c_str(), apzc);
    }
  }

  return apzc;
}

AsyncPanZoomController*
APZCTreeManager::UpdatePanZoomControllerTree(TreeBuildingState& aState,
                                             const LayerMetricsWrapper& aLayer,
                                             uint64_t aLayersId,
                                             const gfx::Matrix4x4& aAncestorTransform,
                                             AsyncPanZoomController* aParent,
                                             AsyncPanZoomController* aNextSibling,
                                             const nsIntRegion& aObscured)
{
  mTreeLock.AssertCurrentThreadOwns();

  mApzcTreeLog << aLayer.Name() << '\t';

  AsyncPanZoomController* apzc = PrepareAPZCForLayer(aLayer,
        aLayer.Metrics(), aLayersId, aAncestorTransform,
        aObscured, aParent, aNextSibling, aState);
  aLayer.SetApzc(apzc);

  mApzcTreeLog << '\n';

  // Accumulate the CSS transform between layers that have an APZC.
  // In the terminology of the big comment above APZCTreeManager::GetScreenToApzcTransform, if
  // we are at layer M, then aAncestorTransform is NC * OC * PC, and we left-multiply MC and
  // compute ancestorTransform to be MC * NC * OC * PC. This gets passed down as the ancestor
  // transform to layer L when we recurse into the children below. If we are at a layer
  // with an APZC, such as P, then we reset the ancestorTransform to just PC, to start
  // the new accumulation as we go down.
  Matrix4x4 transform = aLayer.GetTransform();
  Matrix4x4 ancestorTransform = transform;
  if (!apzc) {
    ancestorTransform = ancestorTransform * aAncestorTransform;
  }

  uint64_t childLayersId = (aLayer.AsRefLayer() ? aLayer.AsRefLayer()->GetReferentId() : aLayersId);

  nsIntRegion obscured;
  if (aLayersId == childLayersId) {
    // If the child layer is in the same process, transform
    // aObscured from aLayer's ParentLayerPixels to aLayer's LayerPixels,
    // which are the children layers' ParentLayerPixels.
    // If we cross a process boundary, we assume that we can start with
    // an empty obscured region because nothing in the parent process will
    // obscure the child process. This may be false. However, not doing this
    // definitely runs into a problematic case where the B2G notification
    // bar and the keyboard get merged into a single layer that obscures
    // all child processes, even though visually they do not. We'd probably
    // have to check for mask layers and so on in order to properly handle
    // that case.
    obscured = aObscured;
    obscured.Transform(To3DMatrix(transform).Inverse());
  }

  // If there's no APZC at this level, any APZCs for our child layers will
  // have our siblings as their siblings, and our parent as their parent.
  AsyncPanZoomController* next = aNextSibling;
  if (apzc) {
    // Otherwise, use this APZC as the parent going downwards, and start off
    // with its first child as the next sibling
    aParent = apzc;
    next = apzc->GetFirstChild();
  }

  // In our recursive downward traversal, track event regions for layers once
  // we encounter an APZC. Push a new empty region on the mEventRegions stack
  // which will accumulate the hit area of descendants of aLayer. In general,
  // the mEventRegions stack is used to accumulate event regions from descendant
  // layers because the event regions for a layer don't include those of its
  // children.
  if (gfxPrefs::LayoutEventRegionsEnabled() && (apzc || aState.mEventRegions.Length() > 0)) {
    aState.mEventRegions.AppendElement(EventRegions());
  }

  for (LayerMetricsWrapper child = aLayer.GetLastChild(); child; child = child.GetPrevSibling()) {
    gfx::TreeAutoIndent indent(mApzcTreeLog);
    next = UpdatePanZoomControllerTree(aState, child, childLayersId,
                                       ancestorTransform, aParent, next,
                                       obscured);

    // Each layer obscures its previous siblings, so we augment the obscured
    // region as we loop backwards through the children.
    nsIntRegion childRegion;
    if (gfxPrefs::LayoutEventRegionsEnabled()) {
      childRegion = child.GetEventRegions().mHitRegion;
    } else {
      childRegion = child.GetVisibleRegion();
    }
    childRegion.Transform(gfx::To3DMatrix(child.GetTransform()));
    if (child.GetClipRect()) {
      childRegion.AndWith(*child.GetClipRect());
    }

    obscured.OrWith(childRegion);
  }

  if (gfxPrefs::LayoutEventRegionsEnabled() && aState.mEventRegions.Length() > 0) {
    // At this point in the code, aState.mEventRegions.LastElement() contains
    // the accumulated regions of the non-APZC descendants of |aLayer|. This
    // happened in the loop above while we iterated through the descendants of
    // |aLayer|. Note that it only includes the non-APZC descendants, because
    // if a layer has an APZC, we simply store the regions from that subtree on
    // that APZC and don't propagate them upwards in the tree. Because of the
    // way we do hit-testing (where the deepest matching APZC is used) it should
    // still be ok if we did propagate those regions upwards and included them
    // in all the ancestor APZCs.
    //
    // Also at this point in the code the |obscured| region includes the hit
    // regions of children of |aLayer| as well as the hit regions of |aLayer|'s
    // younger uncles (i.e. the next-sibling chain of |aLayer|'s parent).
    // When we compute the unobscured regions below, we subtract off the
    // |obscured| region, but it would also be ok to do this before the above
    // loop. At that point |obscured| would only have the uncles' hit regions
    // and not the children. The reason this is ok is again because of the way
    // we do hit-testing (where the deepest APZC is used) it doesn't matter if
    // we count the children as obscuring the parent or not.

    EventRegions unobscured;
    unobscured.Sub(aLayer.GetEventRegions(), obscured);
    APZCTM_LOG("Picking up unobscured hit region %s from layer %p\n", Stringify(unobscured).c_str(), aLayer.GetLayer());

    // Take the hit region of the |aLayer|'s subtree (which has already been
    // transformed into the coordinate space of |aLayer|) and...
    EventRegions subtreeEventRegions = aState.mEventRegions.LastElement();
    aState.mEventRegions.RemoveElementAt(aState.mEventRegions.Length() - 1);
    // ... combine it with the hit region for this layer, and then ...
    subtreeEventRegions.OrWith(unobscured);
    // ... transform it up to the parent layer's coordinate space.
    subtreeEventRegions.Transform(To3DMatrix(aLayer.GetTransform()));
    if (aLayer.GetClipRect()) {
      subtreeEventRegions.AndWith(*aLayer.GetClipRect());
    }

    APZCTM_LOG("After processing layer %p the subtree hit region is %s\n", aLayer.GetLayer(), Stringify(subtreeEventRegions).c_str());

    // If we have an APZC at this level, intersect the subtree hit region with
    // the touch-sensitive region and add it to the APZ's hit test regions.
    if (apzc) {
      APZCTM_LOG("Adding region %s to visible region of APZC %p\n", Stringify(subtreeEventRegions).c_str(), apzc);
      const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(aLayersId);
      MOZ_ASSERT(state);
      MOZ_ASSERT(state->mController.get());
      CSSRect touchSensitiveRegion;
      if (state->mController->GetTouchSensitiveRegion(&touchSensitiveRegion)) {
        // Here we assume 'touchSensitiveRegion' is in the CSS pixels of the
        // parent frame. To convert it to ParentLayer pixels, we therefore need
        // the cumulative resolution of the parent frame. We approximate this as
        // the quotient of our cumulative resolution and our pres shell
        // resolution; this approximation may not be accurate in the presence of
        // a css-driven resolution.
        LayoutDeviceToParentLayerScale parentCumulativeResolution =
            aLayer.Metrics().mCumulativeResolution
            / ParentLayerToLayerScale(aLayer.Metrics().mPresShellResolution);
        subtreeEventRegions.AndWith(ParentLayerIntRect::ToUntyped(
            RoundedIn(touchSensitiveRegion
                    * aLayer.Metrics().mDevPixelsPerCSSPixel
                    * parentCumulativeResolution)));
      }
      apzc->AddHitTestRegions(subtreeEventRegions);
    } else {
      // If we don't have an APZC at this level, carry the subtree hit region
      // up to the parent.
      MOZ_ASSERT(aState.mEventRegions.Length() > 0);
      aState.mEventRegions.LastElement().OrWith(subtreeEventRegions);
    }
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

nsEventStatus
APZCTreeManager::ReceiveInputEvent(InputData& aEvent,
                                   ScrollableLayerGuid* aOutTargetGuid,
                                   uint64_t* aOutInputBlockId)
{
  // Initialize aOutInputBlockId to a sane value, and then later we overwrite
  // it if the input event goes into a block.
  if (aOutInputBlockId) {
    *aOutInputBlockId = InputBlockState::NO_BLOCK_ID;
  }
  nsEventStatus result = nsEventStatus_eIgnore;
  Matrix4x4 transformToApzc;
  bool inOverscrolledApzc = false;
  switch (aEvent.mInputType) {
    case MULTITOUCH_INPUT: {
      MultiTouchInput& touchInput = aEvent.AsMultiTouchInput();
      result = ProcessTouchInput(touchInput, aOutTargetGuid, aOutInputBlockId);
      break;
    } case PANGESTURE_INPUT: {
      PanGestureInput& panInput = aEvent.AsPanGestureInput();
      nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(panInput.mPanStartPoint,
                                                            &inOverscrolledApzc);
      if (apzc) {
        transformToApzc = GetScreenToApzcTransform(apzc);
        panInput.mLocalPanStartPoint = TransformTo<ParentLayerPixel>(
            transformToApzc, panInput.mPanStartPoint);
        panInput.mLocalPanDisplacement = TransformVector<ParentLayerPixel>(
            transformToApzc, panInput.mPanDisplacement, panInput.mPanStartPoint);
        result = mInputQueue->ReceiveInputEvent(apzc, panInput, aOutInputBlockId);

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);
        Matrix4x4 transformToGecko = transformToApzc * GetApzcToGeckoTransform(apzc);
        panInput.mPanStartPoint = TransformTo<ScreenPixel>(
            transformToGecko, panInput.mPanStartPoint);
        panInput.mPanDisplacement = TransformVector<ScreenPixel>(
            transformToGecko, panInput.mPanDisplacement, panInput.mPanStartPoint);
      }
      break;
    } case PINCHGESTURE_INPUT: {  // note: no one currently sends these
      PinchGestureInput& pinchInput = aEvent.AsPinchGestureInput();
      nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(pinchInput.mFocusPoint,
                                                            &inOverscrolledApzc);
      if (apzc) {
        transformToApzc = GetScreenToApzcTransform(apzc);
        pinchInput.mLocalFocusPoint = TransformTo<ParentLayerPixel>(
            transformToApzc, pinchInput.mFocusPoint);
        result = mInputQueue->ReceiveInputEvent(apzc, pinchInput, aOutInputBlockId);

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);
        Matrix4x4 outTransform = transformToApzc * GetApzcToGeckoTransform(apzc);
        pinchInput.mFocusPoint = TransformTo<ScreenPixel>(
            outTransform, pinchInput.mFocusPoint);
      }
      break;
    } case TAPGESTURE_INPUT: {  // note: no one currently sends these
      TapGestureInput& tapInput = aEvent.AsTapGestureInput();
      nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(tapInput.mPoint,
                                                            &inOverscrolledApzc);
      if (apzc) {
        transformToApzc = GetScreenToApzcTransform(apzc);
        tapInput.mLocalPoint = TransformTo<ParentLayerPixel>(
            transformToApzc, tapInput.mPoint);
        result = mInputQueue->ReceiveInputEvent(apzc, tapInput, aOutInputBlockId);

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);
        Matrix4x4 outTransform = transformToApzc * GetApzcToGeckoTransform(apzc);
        tapInput.mPoint = TransformTo<ScreenPixel>(outTransform, tapInput.mPoint);
      }
      break;
    }
  }
  if (inOverscrolledApzc) {
    result = nsEventStatus_eConsumeNoDefault;
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

  { // In this block we flush repaint requests for the entire APZ tree. We need to do this
    // at the start of an input block for a number of reasons. One of the reasons is so that
    // after we untransform the event into gecko space, it doesn't end up under something
    // else. Another reason is that if we hit-test this event and end up on a layer's
    // dispatch-to-content region we cannot be sure we actually got the correct layer. We
    // have to fall back to the gecko hit-test to handle this case, but we can't untransform
    // the event we send to gecko because we don't know the layer to untransform with
    // respect to.
    MonitorAutoLock lock(mTreeLock);
    for (AsyncPanZoomController* apzc = mRootApzc; apzc; apzc = apzc->GetPrevSibling()) {
      FlushRepaintsRecursively(apzc);
    }
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

  return apzc.forget();
}

nsEventStatus
APZCTreeManager::ProcessTouchInput(MultiTouchInput& aInput,
                                   ScrollableLayerGuid* aOutTargetGuid,
                                   uint64_t* aOutInputBlockId)
{
  if (aInput.mType == MultiTouchInput::MULTITOUCH_START) {
    // If we are in an overscrolled state and a second finger goes down,
    // ignore that second touch point completely. The touch-start for it is
    // dropped completely; subsequent touch events until the touch-end for it
    // will have this touch point filtered out.
    if (mApzcForInputBlock && BuildOverscrollHandoffChain(mApzcForInputBlock)->HasOverscrolledApzc()) {
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
        mInputQueue->ReceiveInputEvent(mApzcForInputBlock, cancel, nullptr);
      }
      mApzcForInputBlock = apzc;
    }

    if (mApzcForInputBlock) {
      // Cache apz transform so it can be used for future events in this block.
      mCachedTransformToApzcForInputBlock = GetScreenToApzcTransform(mApzcForInputBlock);
    } else {
      // Reset the cached apz transform
      mCachedTransformToApzcForInputBlock = Matrix4x4();
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

  nsEventStatus result = nsEventStatus_eIgnore;
  if (mApzcForInputBlock) {
    mApzcForInputBlock->GetGuid(aOutTargetGuid);
    // For computing the input for the APZC, used the cached transform.
    // This ensures that the sequence of touch points an APZC sees in an
    // input block are all in the same coordinate space.
    Matrix4x4 transformToApzc = mCachedTransformToApzcForInputBlock;
    for (size_t i = 0; i < aInput.mTouches.Length(); i++) {
      SingleTouchData& touchData = aInput.mTouches[i];
      touchData.mLocalScreenPoint = TransformTo<ParentLayerPixel>(
          transformToApzc, ScreenPoint(touchData.mScreenPoint));
    }
    result = mInputQueue->ReceiveInputEvent(mApzcForInputBlock, aInput, aOutInputBlockId);

    // For computing the event to pass back to Gecko, use the up-to-date transforms.
    // This ensures that transformToApzc and transformToGecko are in sync
    // (note that transformToGecko isn't cached).
    transformToApzc = GetScreenToApzcTransform(mApzcForInputBlock);
    Matrix4x4 transformToGecko = GetApzcToGeckoTransform(mApzcForInputBlock);
    Matrix4x4 outTransform = transformToApzc * transformToGecko;
    for (size_t i = 0; i < aInput.mTouches.Length(); i++) {
      SingleTouchData& touchData = aInput.mTouches[i];
      touchData.mScreenPoint = TransformTo<ScreenPixel>(
          outTransform, touchData.mScreenPoint);
    }
  }
  if (mInOverscrolledApzc) {
    result = nsEventStatus_eConsumeNoDefault;
  }

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
  // don't keep dangling references and leak things.
  if (mTouchCount == 0) {
    mApzcForInputBlock = nullptr;
    mInOverscrolledApzc = false;
    mRetainedTouchIdentifier = -1;
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
    Matrix4x4 transformToApzc = GetScreenToApzcTransform(apzc);
    Matrix4x4 transformToGecko = GetApzcToGeckoTransform(apzc);
    Matrix4x4 outTransform = transformToApzc * transformToGecko;
    *aOutTransformedPoint = TransformTo<LayoutDevicePixel>(outTransform, aPoint);
  }
}

nsEventStatus
APZCTreeManager::ProcessEvent(WidgetInputEvent& aEvent,
                              ScrollableLayerGuid* aOutTargetGuid,
                              uint64_t* aOutInputBlockId)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsEventStatus result = nsEventStatus_eIgnore;

  // Transform the refPoint.
  // If the event hits an overscrolled APZC, instruct the caller to ignore it.
  bool inOverscrolledApzc = false;
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(ScreenPoint(aEvent.refPoint.x, aEvent.refPoint.y),
                                                        &inOverscrolledApzc);
  if (apzc) {
    apzc->GetGuid(aOutTargetGuid);
    Matrix4x4 transformToApzc = GetScreenToApzcTransform(apzc);
    Matrix4x4 transformToGecko = GetApzcToGeckoTransform(apzc);
    Matrix4x4 outTransform = transformToApzc * transformToGecko;
    aEvent.refPoint = TransformTo<LayoutDevicePixel>(outTransform, aEvent.refPoint);
  }
  if (inOverscrolledApzc) {
    result = nsEventStatus_eConsumeNoDefault;
  }
  return result;
}

nsEventStatus
APZCTreeManager::ReceiveInputEvent(WidgetInputEvent& aEvent,
                                   ScrollableLayerGuid* aOutTargetGuid,
                                   uint64_t* aOutInputBlockId)
{
  // This function will be removed as part of bug 930939.
  // In general it is preferable to use the version of ReceiveInputEvent
  // that takes an InputData, as that is usable from off-main-thread.

  MOZ_ASSERT(NS_IsMainThread());

  // Initialize aOutInputBlockId to a sane value, and then later we overwrite
  // it if the input event goes into a block.
  if (aOutInputBlockId) {
    *aOutInputBlockId = InputBlockState::NO_BLOCK_ID;
  }

  switch (aEvent.mClass) {
    case eTouchEventClass: {
      WidgetTouchEvent& touchEvent = *aEvent.AsTouchEvent();
      MultiTouchInput touchInput(touchEvent);
      nsEventStatus result = ProcessTouchInput(touchInput, aOutTargetGuid, aOutInputBlockId);
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
      return ProcessEvent(aEvent, aOutTargetGuid, aOutInputBlockId);
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
APZCTreeManager::ContentReceivedTouch(uint64_t aInputBlockId,
                                      bool aPreventDefault)
{
  mInputQueue->ContentReceivedTouch(aInputBlockId, aPreventDefault);
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
APZCTreeManager::FlushRepaintsRecursively(AsyncPanZoomController* aApzc)
{
  mTreeLock.AssertCurrentThreadOwns();

  aApzc->FlushRepaintForNewInputBlock();
  for (AsyncPanZoomController* child = aApzc->GetLastChild(); child; child = child->GetPrevSibling()) {
    FlushRepaintsRecursively(child);
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
 * Transform a displacement from the ParentLayer coordinates of a source APZC
 * to the ParentLayer coordinates of a target APZC.
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
                      ParentLayerPoint& aStartPoint,
                      ParentLayerPoint& aEndPoint) {
  // Convert start and end points to Screen coordinates.
  Matrix4x4 untransformToApzc = aTreeManager->GetScreenToApzcTransform(aSource).Inverse();
  ScreenPoint screenStart = TransformTo<ScreenPixel>(untransformToApzc, aStartPoint);
  ScreenPoint screenEnd = TransformTo<ScreenPixel>(untransformToApzc, aEndPoint);

  // Convert start and end points to aTarget's ParentLayer coordinates.
  Matrix4x4 transformToApzc = aTreeManager->GetScreenToApzcTransform(aTarget);
  aStartPoint = TransformTo<ParentLayerPixel>(transformToApzc, screenStart);
  aEndPoint = TransformTo<ParentLayerPixel>(transformToApzc, screenEnd);
}

bool
APZCTreeManager::DispatchScroll(AsyncPanZoomController* aPrev,
                                ParentLayerPoint aStartPoint,
                                ParentLayerPoint aEndPoint,
                                OverscrollHandoffState& aOverscrollHandoffState)
{
  const OverscrollHandoffChain& overscrollHandoffChain = aOverscrollHandoffState.mChain;
  uint32_t overscrollHandoffChainIndex = aOverscrollHandoffState.mChainIndex;
  nsRefPtr<AsyncPanZoomController> next;
  // If we have reached the end of the overscroll handoff chain, there is
  // nothing more to scroll, so we ignore the rest of the pan gesture.
  if (overscrollHandoffChainIndex >= overscrollHandoffChain.Length()) {
    // Nothing more to scroll - ignore the rest of the pan gesture.
    return false;
  }

  next = overscrollHandoffChain.GetApzcAtIndex(overscrollHandoffChainIndex);

  if (next == nullptr || next->IsDestroyed()) {
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
  return next->AttemptScroll(aStartPoint, aEndPoint, aOverscrollHandoffState);
}

bool
APZCTreeManager::DispatchFling(AsyncPanZoomController* aPrev,
                               ParentLayerPoint aVelocity,
                               nsRefPtr<const OverscrollHandoffChain> aOverscrollHandoffChain,
                               bool aHandoff)
{
  nsRefPtr<AsyncPanZoomController> current;
  uint32_t aOverscrollHandoffChainLength = aOverscrollHandoffChain->Length();
  uint32_t startIndex;

  // The fling's velocity needs to be transformed from the screen coordinates
  // of |aPrev| to the screen coordinates of |next|. To transform a velocity
  // correctly, we need to convert it to a displacement. For now, we do this
  // by anchoring it to a start point of (0, 0).
  // TODO: For this to be correct in the presence of 3D transforms, we should
  // use the end point of the touch that started the fling as the start point
  // rather than (0, 0).
  ParentLayerPoint startPoint;  // (0, 0)
  ParentLayerPoint endPoint;
  ParentLayerPoint transformedVelocity = aVelocity;

  if (aHandoff) {
    startIndex = aOverscrollHandoffChain->IndexOf(aPrev) + 1;

    // IndexOf will return aOverscrollHandoffChain->Length() if
    // |aPrev| is not found.
    if (startIndex >= aOverscrollHandoffChainLength) {
      return false;
    }
  } else {
    startIndex = 0;
  }

  for (; startIndex < aOverscrollHandoffChainLength; startIndex++) {
    current = aOverscrollHandoffChain->GetApzcAtIndex(startIndex);

    // Make sure the apcz about to be handled can be handled
    if (current == nullptr || current->IsDestroyed()) {
      return false;
    }

    endPoint = startPoint + transformedVelocity;

    // Only transform when current apcz can be transformed with previous
    if (startIndex > 0) {
      TransformDisplacement(this,
                            aOverscrollHandoffChain->GetApzcAtIndex(startIndex - 1),
                            current,
                            startPoint,
                            endPoint);
    }

    transformedVelocity = endPoint - startPoint;

    bool handoff = (startIndex < 1) ? aHandoff : true;
    if (current->AttemptFling(transformedVelocity,
                              aOverscrollHandoffChain,
                              handoff)) {
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

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetTargetAPZC(const ScreenPoint& aPoint, bool* aOutInOverscrolledApzc)
{
  MonitorAutoLock lock(mTreeLock);
  nsRefPtr<AsyncPanZoomController> target;
  // The root may have siblings, so check those too
  bool inOverscrolledApzc = false;
  for (AsyncPanZoomController* apzc = mRootApzc; apzc; apzc = apzc->GetPrevSibling()) {
    target = GetAPZCAtPoint(apzc, aPoint.ToUnknownPoint(), &inOverscrolledApzc);
    // If we hit an overscrolled APZC, 'target' will be nullptr but it's still
    // a hit so we don't search further siblings.
    if (target || inOverscrolledApzc) {
      break;
    }
  }
  // If we are in an overscrolled APZC, we should be returning nullptr.
  MOZ_ASSERT(!(target && inOverscrolledApzc));
  if (aOutInOverscrolledApzc) {
    *aOutInOverscrolledApzc = inOverscrolledApzc;
  }
  return target.forget();
}

nsRefPtr<const OverscrollHandoffChain>
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

  // Grab tree lock since we'll be walking the APZC tree.
  MonitorAutoLock lock(mTreeLock);

  // Build the chain. If there is a scroll parent link, we use that. This is
  // needed to deal with scroll info layers, because they participate in handoff
  // but do not follow the expected layer tree structure. If there are no
  // scroll parent links we just walk up the tree to find the scroll parent.
  OverscrollHandoffChain* result = new OverscrollHandoffChain;
  AsyncPanZoomController* apzc = aInitialTarget;
  while (apzc != nullptr) {
    result->Add(apzc);

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
  result->SortByScrollPriority();

  // Print the overscroll chain for debugging.
  for (uint32_t i = 0; i < result->Length(); ++i) {
    APZCTM_LOG("OverscrollHandoffChain[%d] = %p\n", i, result->GetApzcAtIndex(i).get());
  }

  return result;
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
                                const Point& aHitTestPoint,
                                bool* aOutInOverscrolledApzc)
{
  mTreeLock.AssertCurrentThreadOwns();

  // The comments below assume there is a chain of layers L..R with L and P having APZC instances as
  // explained in the comment above GetScreenToApzcTransform. This function will recurse with aApzc at L and P, and the
  // comments explain what values are stored in the variables at these two levels. All the comments
  // use standard matrix notation where the leftmost matrix in a multiplication is applied first.

  // ancestorUntransform takes points from aApzc's parent APZC's CSS-transformed layer coordinates
  // to aApzc's parent layer's layer coordinates.
  // It is PC.Inverse() * OC.Inverse() * NC.Inverse() * MC.Inverse() at recursion level for L,
  //   and RC.Inverse() * QC.Inverse()                               at recursion level for P.
  Matrix4x4 ancestorUntransform = aApzc->GetAncestorTransform().Inverse();

  // Hit testing for this layer takes place in our parent layer coordinates,
  // since the composition bounds (used to initialize the visible rect against
  // which we hit test are in those coordinates).
  Point4D hitTestPointForThisLayer = ancestorUntransform.ProjectPoint(aHitTestPoint);
  APZCTM_LOG("Untransformed %f %f to transient coordinates %f %f for hit-testing APZC %p\n",
           aHitTestPoint.x, aHitTestPoint.y,
           hitTestPointForThisLayer.x, hitTestPointForThisLayer.y, aApzc);

  // childUntransform takes points from aApzc's parent APZC's CSS-transformed layer coordinates
  // to aApzc's CSS-transformed layer coordinates.
  // It is PC.Inverse() * OC.Inverse() * NC.Inverse() * MC.Inverse() * LA.Inverse() at L
  //   and RC.Inverse() * QC.Inverse() * PA.Inverse()                               at P.
  Matrix4x4 childUntransform = ancestorUntransform * Matrix4x4(aApzc->GetCurrentAsyncTransform()).Inverse();
  Point4D hitTestPointForChildLayers = childUntransform.ProjectPoint(aHitTestPoint);
  APZCTM_LOG("Untransformed %f %f to layer coordinates %f %f for APZC %p\n",
           aHitTestPoint.x, aHitTestPoint.y,
           hitTestPointForChildLayers.x, hitTestPointForChildLayers.y, aApzc);

  AsyncPanZoomController* result = nullptr;
  // This walks the tree in depth-first, reverse order, so that it encounters
  // APZCs front-to-back on the screen.
  if (hitTestPointForChildLayers.HasPositiveWCoord()) {
    for (AsyncPanZoomController* child = aApzc->GetLastChild(); child; child = child->GetPrevSibling()) {
      AsyncPanZoomController* match = GetAPZCAtPoint(child, hitTestPointForChildLayers.As2DPoint(), aOutInOverscrolledApzc);
      if (*aOutInOverscrolledApzc) {
        // We matched an overscrolled APZC, abort.
        return nullptr;
      }
      if (match) {
        result = match;
        break;
      }
    }
  }
  if (!result && hitTestPointForThisLayer.HasPositiveWCoord() &&
      aApzc->HitRegionContains(ParentLayerPoint::FromUnknownPoint(hitTestPointForThisLayer.As2DPoint()))) {
    APZCTM_LOG("Successfully matched untransformed point %f %f to visible region for APZC %p\n",
             hitTestPointForThisLayer.x, hitTestPointForThisLayer.y, aApzc);
    result = aApzc;
  }

  // If we are overscrolled, and the point matches us or one of our children,
  // the result is inside an overscrolled APZC, inform our caller of this
  // (callers typically ignore events targeted at overscrolled APZCs).
  if (result && aApzc->IsOverscrolled()) {
    *aOutInOverscrolledApzc = true;
    result = nullptr;
  }

  return result;
}

/* The methods GetScreenToApzcTransform() and GetApzcToGeckoTransform() return
   some useful transformations that input events may need applied. This is best
   illustrated with an example. Consider a chain of layers, L, M, N, O, P, Q, R. Layer L
   is the layer that corresponds to the argument |aApzc|, and layer R is the root
   of the layer tree. Layer M is the parent of L, N is the parent of M, and so on.
   When layer L is displayed to the screen by the compositor, the set of transforms that
   are applied to L are (in order from top to bottom):

        L's CSS transform                   (hereafter referred to as transform matrix LC)
        L's nontransient async transform    (hereafter referred to as transform matrix LN)
        L's transient async transform       (hereafter referred to as transform matrix LT)
        M's CSS transform                   (hereafter referred to as transform matrix MC)
        M's nontransient async transform    (hereafter referred to as transform matrix MN)
        M's transient async transform       (hereafter referred to as transform matrix MT)
        ...
        R's CSS transform                   (hereafter referred to as transform matrix RC)
        R's nontransient async transform    (hereafter referred to as transform matrix RN)
        R's transient async transform       (hereafter referred to as transform matrix RT)

   Also, for any layer, the async transform is the combination of its transient and non-transient
   parts. That is, for any layer L:
                  LA === LN * LT
        LA.Inverse() === LT.Inverse() * LN.Inverse()

   If we want user input to modify L's transient async transform, we have to first convert
   user input from screen space to the coordinate space of L's transient async transform. Doing
   this involves applying the following transforms (in order from top to bottom):
        RT.Inverse()
        RN.Inverse()
        RC.Inverse()
        ...
        MT.Inverse()
        MN.Inverse()
        MC.Inverse()
   This combined transformation is returned by GetScreenToApzcTransform().

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
        RT.Inverse()
        RN.Inverse()
        RC.Inverse()
        ...
        MT.Inverse()
        MN.Inverse()
        MC.Inverse()
        LT.Inverse()
        LN.Inverse()
        LC.Inverse()
        LC
        LD
        MC
        MD
        ...
        RC
        RD
   This sequence can be simplified and refactored to the following:
        GetScreenToApzcTransform()
        LA.Inverse()
        LD
        MC
        MD
        ...
        RC
        RD
   Since GetScreenToApzcTransform() can be obtained by calling that function, GetApzcToGeckoTransform()
   returns the remaining transforms (LA.Inverse() * LD * ... * RD), so that the caller code can
   combine it with GetScreenToApzcTransform() to get the final transform required in this case.

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

/*
 * See the long comment above for a detailed explanation of this function.
 */
Matrix4x4
APZCTreeManager::GetScreenToApzcTransform(const AsyncPanZoomController *aApzc) const
{
  Matrix4x4 result;
  MonitorAutoLock lock(mTreeLock);

  // The comments below assume there is a chain of layers L..R with L and P having APZC instances as
  // explained in the comment above. This function is called with aApzc at L, and the loop
  // below performs one iteration, where parent is at P. The comments explain what values are stored
  // in the variables at these two levels. All the comments use standard matrix notation where the
  // leftmost matrix in a multiplication is applied first.

  // ancestorUntransform is PC.Inverse() * OC.Inverse() * NC.Inverse() * MC.Inverse()
  Matrix4x4 ancestorUntransform = aApzc->GetAncestorTransform().Inverse();

  // result is initialized to PC.Inverse() * OC.Inverse() * NC.Inverse() * MC.Inverse()
  result = ancestorUntransform;

  for (AsyncPanZoomController* parent = aApzc->GetParent(); parent; parent = parent->GetParent()) {
    // ancestorUntransform is updated to RC.Inverse() * QC.Inverse() when parent == P
    ancestorUntransform = parent->GetAncestorTransform().Inverse();
    // asyncUntransform is updated to PA.Inverse() when parent == P
    Matrix4x4 asyncUntransform = Matrix4x4(parent->GetCurrentAsyncTransform()).Inverse();
    // untransformSinceLastApzc is RC.Inverse() * QC.Inverse() * PA.Inverse()
    Matrix4x4 untransformSinceLastApzc = ancestorUntransform * asyncUntransform;

    // result is RC.Inverse() * QC.Inverse() * PA.Inverse() * PC.Inverse() * OC.Inverse() * NC.Inverse() * MC.Inverse()
    result = untransformSinceLastApzc * result;

    // The above value for result when parent == P matches the required output
    // as explained in the comment above this method. Note that any missing
    // terms are guaranteed to be identity transforms.
  }

  return result;
}

/*
 * See the long comment above GetScreenToApzcTransform() for a detailed
 * explanation of this function.
 */
Matrix4x4
APZCTreeManager::GetApzcToGeckoTransform(const AsyncPanZoomController *aApzc) const
{
  Matrix4x4 result;
  MonitorAutoLock lock(mTreeLock);

  // The comments below assume there is a chain of layers L..R with L and P having APZC instances as
  // explained in the comment above. This function is called with aApzc at L, and the loop
  // below performs one iteration, where parent is at P. The comments explain what values are stored
  // in the variables at these two levels. All the comments use standard matrix notation where the
  // leftmost matrix in a multiplication is applied first.

  // asyncUntransform is LA.Inverse()
  Matrix4x4 asyncUntransform = Matrix4x4(aApzc->GetCurrentAsyncTransform()).Inverse();

  // aTransformToGeckoOut is initialized to LA.Inverse() * LD * MC * NC * OC * PC
  result = asyncUntransform * aApzc->GetTransformToLastDispatchedPaint() * aApzc->GetAncestorTransform();

  for (AsyncPanZoomController* parent = aApzc->GetParent(); parent; parent = parent->GetParent()) {
    // aTransformToGeckoOut is LA.Inverse() * LD * MC * NC * OC * PC * PD * QC * RC
    result = result * parent->GetTransformToLastDispatchedPaint() * parent->GetAncestorTransform();

    // The above value for result when parent == P matches the required output
    // as explained in the comment above this method. Note that any missing
    // terms are guaranteed to be identity transforms.
  }

  return result;
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

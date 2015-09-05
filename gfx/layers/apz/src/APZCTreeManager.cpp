/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManager.h"
#include "AsyncPanZoomController.h"
#include "Compositor.h"                 // for Compositor
#include "HitTestingTreeNode.h"         // for HitTestingTreeNode
#include "InputBlockState.h"            // for InputBlockState
#include "InputData.h"                  // for InputData, etc
#include "Layers.h"                     // for Layer, etc
#include "mozilla/dom/Touch.h"          // for Touch
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/layers/APZThreadUtils.h"  // for AssertOnCompositorThread, etc
#include "mozilla/layers/AsyncCompositionManager.h" // for ViewTransform
#include "mozilla/layers/CompositorParent.h" // for CompositorParent, etc
#include "mozilla/layers/LayerMetricsWrapper.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/mozalloc.h"           // for operator new
#include "mozilla/TouchEvents.h"
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/EventStateManager.h"  // for WheelPrefs
#include "nsDebug.h"                    // for NS_WARNING
#include "nsPoint.h"                    // for nsIntPoint
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "mozilla/gfx/Logging.h"        // for gfx::TreeLog
#include "UnitTransforms.h"             // for ViewAs
#include "gfxPrefs.h"                   // for gfxPrefs
#include "OverscrollHandoffState.h"     // for OverscrollHandoffState
#include "TreeTraversal.h"              // for generic tree traveral algorithms
#include "LayersLogging.h"              // for Stringify
#include "Units.h"                      // for ParentlayerPixel

#define ENABLE_APZCTM_LOGGING 0
// #define ENABLE_APZCTM_LOGGING 1

#if ENABLE_APZCTM_LOGGING
#  define APZCTM_LOG(...) printf_stderr("APZCTM: " __VA_ARGS__)
#else
#  define APZCTM_LOG(...)
#endif

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

  // A list of nodes that need to be destroyed at the end of the tree building.
  // This is initialized with all nodes in the old tree, and nodes are removed
  // from it as we reuse them in the new tree.
  nsTArray<nsRefPtr<HitTestingTreeNode>> mNodesToDestroy;

  // This map is populated as we place APZCs into the new tree. Its purpose is
  // to facilitate re-using the same APZC for different layers that scroll
  // together (and thus have the same ScrollableLayerGuid).
  std::map<ScrollableLayerGuid, AsyncPanZoomController*> mApzcMap;
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
      mHitResultForInputBlock(HitNothing),
      mRetainedTouchIdentifier(-1),
      mApzcTreeLog("apzctree")
{
  MOZ_ASSERT(NS_IsMainThread());
  AsyncPanZoomController::InitializeGlobalState();
  mApzcTreeLog.ConditionOnPrefFunction(gfxPrefs::APZPrintTree);
}

APZCTreeManager::~APZCTreeManager()
{
}

AsyncPanZoomController*
APZCTreeManager::MakeAPZCInstance(uint64_t aLayersId,
                                  GeckoContentController* aController)
{
  return new AsyncPanZoomController(aLayersId, this, mInputQueue,
    aController, AsyncPanZoomController::USE_GESTURE_DETECTOR);
}

void
APZCTreeManager::SetAllowedTouchBehavior(uint64_t aInputBlockId,
                                         const nsTArray<TouchBehaviorFlags> &aValues)
{
  mInputQueue->SetAllowedTouchBehavior(aInputBlockId, aValues);
}

/* Flatten the tree of nodes into the given nsTArray */
static void
Collect(HitTestingTreeNode* aNode, nsTArray<nsRefPtr<HitTestingTreeNode>>* aCollection)
{
  if (aNode) {
    aCollection->AppendElement(aNode);
    Collect(aNode->GetLastChild(), aCollection);
    Collect(aNode->GetPrevSibling(), aCollection);
  }
}

void
APZCTreeManager::UpdateHitTestingTree(CompositorParent* aCompositor,
                                      Layer* aRoot,
                                      bool aIsFirstPaint,
                                      uint64_t aOriginatingLayersId,
                                      uint32_t aPaintSequenceNumber)
{
  APZThreadUtils::AssertOnCompositorThread();

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
  Collect(mRootNode, &state.mNodesToDestroy);
  mRootNode = nullptr;

  if (aRoot) {
    mApzcTreeLog << "[start]\n";
    LayerMetricsWrapper root(aRoot);
    UpdateHitTestingTree(state, root,
                         // aCompositor is null in gtest scenarios
                         aCompositor ? aCompositor->RootLayerTreeId() : 0,
                         Matrix4x4(), nullptr, nullptr);
    mApzcTreeLog << "[end]\n";
  }

  for (size_t i = 0; i < state.mNodesToDestroy.Length(); i++) {
    APZCTM_LOG("Destroying node at %p with APZC %p\n",
        state.mNodesToDestroy[i].get(),
        state.mNodesToDestroy[i]->GetApzc());
    state.mNodesToDestroy[i]->Destroy();
  }

#if ENABLE_APZCTM_LOGGING
  // Make the hit-test tree line up with the layer dump
  printf_stderr("APZCTreeManager (%p)\n", this);
  mRootNode->Dump("  ");
#endif
}

// Compute the clip region to be used for a layer with an APZC. This function
// is only called for layers which actually have scrollable metrics and an APZC.
static ParentLayerIntRegion
ComputeClipRegion(GeckoContentController* aController,
                  const LayerMetricsWrapper& aLayer)
{
  ParentLayerIntRegion clipRegion;
  if (aLayer.GetClipRect()) {
    clipRegion = *aLayer.GetClipRect();
  } else {
    // if there is no clip on this layer (which should only happen for the
    // root scrollable layer in a process, or for some of the LayerMetrics
    // expansions of a multi-metrics layer), fall back to using the comp
    // bounds which should be equivalent.
    clipRegion = RoundedToInt(aLayer.Metrics().GetCompositionBounds());
  }

  // Optionally, the GeckoContentController can provide a touch-sensitive
  // region that constrains all frames associated with the controller.
  // In this case we intersect the composition bounds with that region.
  CSSRect touchSensitiveRegion;
  if (aController->GetTouchSensitiveRegion(&touchSensitiveRegion)) {
    // Here we assume 'touchSensitiveRegion' is in the CSS pixels of the
    // parent frame. To convert it to ParentLayer pixels, we therefore need
    // the cumulative resolution of the parent frame. We approximate this as
    // the quotient of our cumulative resolution and our pres shell resolution;
    // this approximation may not be accurate in the presence of a css-driven
    // resolution.
    LayoutDeviceToParentLayerScale2D parentCumulativeResolution =
          aLayer.Metrics().GetCumulativeResolution()
        / ParentLayerToLayerScale(aLayer.Metrics().GetPresShellResolution());
    // Not sure what rounding option is the most correct here, but if we ever
    // figure it out we can change this. For now I'm rounding in to minimize
    // the chances of getting a complex region.
    ParentLayerIntRegion extraClip = RoundedIn(
        touchSensitiveRegion
        * aLayer.Metrics().GetDevPixelsPerCSSPixel()
        * parentCumulativeResolution);
    clipRegion.AndWith(extraClip);
  }

  return clipRegion;
}

void
APZCTreeManager::PrintAPZCInfo(const LayerMetricsWrapper& aLayer,
                               const AsyncPanZoomController* apzc)
{
  const FrameMetrics& metrics = aLayer.Metrics();
  mApzcTreeLog << "APZC " << apzc->GetGuid()
               << "\tcb=" << metrics.GetCompositionBounds()
               << "\tsr=" << metrics.GetScrollableRect()
               << (aLayer.IsScrollInfoLayer() ? "\tscrollinfo" : "")
               << (apzc->HasScrollgrab() ? "\tscrollgrab" : "") << "\t"
               << metrics.GetContentDescription().get();
}

void
APZCTreeManager::AttachNodeToTree(HitTestingTreeNode* aNode,
                                  HitTestingTreeNode* aParent,
                                  HitTestingTreeNode* aNextSibling)
{
  if (aNextSibling) {
    aNextSibling->SetPrevSibling(aNode);
  } else if (aParent) {
    aParent->SetLastChild(aNode);
  } else {
    MOZ_ASSERT(!mRootNode);
    mRootNode = aNode;
    aNode->MakeRoot();
  }
}

static EventRegions
GetEventRegions(const LayerMetricsWrapper& aLayer)
{
  if (aLayer.IsScrollInfoLayer()) {
    return EventRegions(nsIntRegion(ParentLayerIntRect::ToUntyped(
      RoundedToInt(aLayer.Metrics().GetCompositionBounds()))));
  }
  return aLayer.GetEventRegions();
}

already_AddRefed<HitTestingTreeNode>
APZCTreeManager::RecycleOrCreateNode(TreeBuildingState& aState,
                                     AsyncPanZoomController* aApzc,
                                     uint64_t aLayersId)
{
  // Find a node without an APZC and return it. Note that unless the layer tree
  // actually changes, this loop should generally do an early-return on the
  // first iteration, so it should be cheap in the common case.
  for (size_t i = 0; i < aState.mNodesToDestroy.Length(); i++) {
    nsRefPtr<HitTestingTreeNode> node = aState.mNodesToDestroy[i];
    if (!node->IsPrimaryHolder()) {
      aState.mNodesToDestroy.RemoveElement(node);
      node->RecycleWith(aApzc, aLayersId);
      return node.forget();
    }
  }
  nsRefPtr<HitTestingTreeNode> node = new HitTestingTreeNode(aApzc, false, aLayersId);
  return node.forget();
}

static EventRegionsOverride
GetEventRegionsOverride(HitTestingTreeNode* aParent,
                       const LayerMetricsWrapper& aLayer)
{
  // Make it so that if the flag is set on the layer tree, it automatically
  // propagates to all the nodes in the corresponding subtree rooted at that
  // layer in the hit-test tree. This saves having to walk up the tree every
  // we want to see if a hit-test node is affected by this flag.
  EventRegionsOverride result = aLayer.GetEventRegionsOverride();
  if (aParent) {
    result |= aParent->GetEventRegionsOverride();
  }
  return result;
}

HitTestingTreeNode*
APZCTreeManager::PrepareNodeForLayer(const LayerMetricsWrapper& aLayer,
                                     const FrameMetrics& aMetrics,
                                     uint64_t aLayersId,
                                     const gfx::Matrix4x4& aAncestorTransform,
                                     HitTestingTreeNode* aParent,
                                     HitTestingTreeNode* aNextSibling,
                                     TreeBuildingState& aState)
{
  mTreeLock.AssertCurrentThreadOwns();

  bool needsApzc = true;
  if (!aMetrics.IsScrollable()) {
    needsApzc = false;
  }

  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(aLayersId);
  if (!(state && state->mController.get())) {
    needsApzc = false;
  }

  nsRefPtr<HitTestingTreeNode> node = nullptr;
  if (!needsApzc) {
    node = RecycleOrCreateNode(aState, nullptr, aLayersId);
    AttachNodeToTree(node, aParent, aNextSibling);
    node->SetHitTestData(GetEventRegions(aLayer), aLayer.GetTransform(),
        aLayer.GetClipRect() ? Some(ParentLayerIntRegion(*aLayer.GetClipRect())) : Nothing(),
        GetEventRegionsOverride(aParent, aLayer));
    return node;
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
    // the same old APZC for the new content. Also, when reparenting a tab into a
    // new window a layer might get moved to a different layer tree with a
    // different APZCTreeManager. In these cases we don't want to reuse the same
    // APZC, so null it out so we run through the code to find another one or
    // create one.
    if (apzc && (!apzc->Matches(guid) || !apzc->HasTreeManager(this))) {
      apzc = nullptr;
    }

    // See if we can find an APZC from the previous tree that matches the
    // ScrollableLayerGuid from this layer. If there is one, then we know that
    // the layout of the page changed causing the layer tree to be rebuilt, but
    // the underlying content for the APZC is still there somewhere. Therefore,
    // we want to find the APZC instance and continue using it here.
    //
    // We particularly want to find the primary-holder node from the previous
    // tree that matches, because we don't want that node to get destroyed. If
    // it does get destroyed, then the APZC will get destroyed along with it by
    // definition, but we want to keep that APZC around in the new tree.
    // We leave non-primary-holder nodes in the destroy list because we don't
    // care about those nodes getting destroyed.
    for (size_t i = 0; i < aState.mNodesToDestroy.Length(); i++) {
      nsRefPtr<HitTestingTreeNode> n = aState.mNodesToDestroy[i];
      if (n->IsPrimaryHolder() && n->GetApzc() && n->GetApzc()->Matches(guid)) {
        node = n;
        if (apzc != nullptr) {
          // If there is an APZC already then it should match the one from the
          // old primary-holder node
          MOZ_ASSERT(apzc == node->GetApzc());
        }
        apzc = node->GetApzc();
        break;
      }
    }

    // The APZC we get off the layer may have been destroyed previously if the
    // layer was inactive or omitted from the layer tree for whatever reason
    // from a layers update. If it later comes back it will have a reference to
    // a destroyed APZC and so we need to throw that out and make a new one.
    bool newApzc = (apzc == nullptr || apzc->IsDestroyed());
    if (newApzc) {
      apzc = MakeAPZCInstance(aLayersId, state->mController);
      apzc->SetCompositorParent(aState.mCompositor);
      if (state->mCrossProcessParent != nullptr) {
        apzc->ShareFrameMetricsAcrossProcesses();
      }
      MOZ_ASSERT(node == nullptr);
      node = new HitTestingTreeNode(apzc, true, aLayersId);
    } else {
      // If we are re-using a node for this layer clear the tree pointers
      // so that it doesn't continue pointing to nodes that might no longer
      // be in the tree. These pointers will get reset properly as we continue
      // building the tree. Also remove it from the set of nodes that are going
      // to be destroyed, because it's going to remain active.
      aState.mNodesToDestroy.RemoveElement(node);
      node->SetPrevSibling(nullptr);
      node->SetLastChild(nullptr);
    }

    APZCTM_LOG("Using APZC %p for layer %p with identifiers %" PRId64 " %" PRId64 "\n", apzc, aLayer.GetLayer(), aLayersId, aMetrics.GetScrollId());

    apzc->NotifyLayersUpdated(aMetrics,
        aState.mIsFirstPaint && (aLayersId == aState.mOriginatingLayersId));

    // Since this is the first time we are encountering an APZC with this guid,
    // the node holding it must be the primary holder. It may be newly-created
    // or not, depending on whether it went through the newApzc branch above.
    MOZ_ASSERT(node->IsPrimaryHolder() && node->GetApzc() && node->GetApzc()->Matches(guid));

    ParentLayerIntRegion clipRegion = ComputeClipRegion(state->mController, aLayer);
    node->SetHitTestData(GetEventRegions(aLayer), aLayer.GetTransform(), Some(clipRegion),
        GetEventRegionsOverride(aParent, aLayer));
    apzc->SetAncestorTransform(aAncestorTransform);

    PrintAPZCInfo(aLayer, apzc);

    // Bind the APZC instance into the tree of APZCs
    AttachNodeToTree(node, aParent, aNextSibling);

    // For testing, log the parent scroll id of every APZC that has a
    // parent. This allows test code to reconstruct the APZC tree.
    // Note that we currently only do this for APZCs in the layer tree
    // that originated the update, because the only identifying information
    // we are logging about APZCs is the scroll id, and otherwise we could
    // confuse APZCs from different layer trees with the same scroll id.
    if (aLayersId == aState.mOriginatingLayersId) {
      if (apzc->HasNoParentWithSameLayersId()) {
        aState.mPaintLogger.LogTestData(aMetrics.GetScrollId(),
            "hasNoParentWithSameLayersId", true);
      } else {
        MOZ_ASSERT(apzc->GetParent());
        aState.mPaintLogger.LogTestData(aMetrics.GetScrollId(),
            "parentScrollId", apzc->GetParent()->GetGuid().mScrollId);
      }
    }

    if (newApzc) {
      auto it = mZoomConstraints.find(guid);
      if (it != mZoomConstraints.end()) {
        // We have a zoomconstraints for this guid, apply it.
        apzc->UpdateZoomConstraints(it->second);
      } else if (!apzc->HasNoParentWithSameLayersId()) {
        // This is a sub-APZC, so inherit the zoom constraints from its parent.
        // This ensures that if e.g. user-scalable=no was specified, none of the
        // APZCs for that subtree allow double-tap to zoom.
        apzc->UpdateZoomConstraints(apzc->GetParent()->GetZoomConstraints());
      }
      // Otherwise, this is the root of a layers id, but we didn't have a saved
      // zoom constraints. Leave it empty for now.
    }

    // Add a guid -> APZC mapping for the newly created APZC.
    insertResult.first->second = apzc;
  } else {
    // We already built an APZC earlier in this tree walk, but we have another layer
    // now that will also be using that APZC. The hit-test region on the APZC needs
    // to be updated to deal with the new layer's hit region.

    node = RecycleOrCreateNode(aState, apzc, aLayersId);
    AttachNodeToTree(node, aParent, aNextSibling);

    // Even though different layers associated with a given APZC may be at
    // different levels in the layer tree (e.g. one being an uncle of another),
    // we require from Layout that the CSS transforms up to their common
    // ancestor be the same.
    MOZ_ASSERT(aAncestorTransform == apzc->GetAncestorTransform());

    ParentLayerIntRegion clipRegion = ComputeClipRegion(state->mController, aLayer);
    node->SetHitTestData(GetEventRegions(aLayer), aLayer.GetTransform(), Some(clipRegion),
        GetEventRegionsOverride(aParent, aLayer));
  }

  return node;
}

HitTestingTreeNode*
APZCTreeManager::UpdateHitTestingTree(TreeBuildingState& aState,
                                      const LayerMetricsWrapper& aLayer,
                                      uint64_t aLayersId,
                                      const gfx::Matrix4x4& aAncestorTransform,
                                      HitTestingTreeNode* aParent,
                                      HitTestingTreeNode* aNextSibling)
{
  mTreeLock.AssertCurrentThreadOwns();

  mApzcTreeLog << aLayer.Name() << '\t';

  HitTestingTreeNode* node = PrepareNodeForLayer(aLayer,
        aLayer.Metrics(), aLayersId, aAncestorTransform,
        aParent, aNextSibling, aState);
  MOZ_ASSERT(node);
  AsyncPanZoomController* apzc = node->GetApzc();
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

  // Note that |node| at this point will not have any children, otherwise we
  // we would have to set next to node->GetFirstChild().
  MOZ_ASSERT(!node->GetFirstChild());
  aParent = node;
  HitTestingTreeNode* next = nullptr;

  uint64_t childLayersId = (aLayer.AsRefLayer() ? aLayer.AsRefLayer()->GetReferentId() : aLayersId);
  for (LayerMetricsWrapper child = aLayer.GetLastChild(); child; child = child.GetPrevSibling()) {
    gfx::TreeAutoIndent indent(mApzcTreeLog);
    next = UpdateHitTestingTree(aState, child, childLayersId,
                                ancestorTransform, aParent, next);
  }

  return node;
}

// Returns whether or not a wheel event action will be (or was) performed by
// APZ. If this returns true, the event must not perform a synchronous
// scroll.
//
// Even if this returns false, all wheel events in APZ-aware widgets must
// be sent through APZ so they are transformed correctly for TabParent.
static bool
WillHandleWheelEvent(WidgetWheelEvent* aEvent)
{
  return EventStateManager::WheelEventIsScrollAction(aEvent) &&
         (aEvent->deltaMode == nsIDOMWheelEvent::DOM_DELTA_LINE
            || aEvent->deltaMode == nsIDOMWheelEvent::DOM_DELTA_PIXEL) &&
         !EventStateManager::WheelEventNeedsDeltaMultipliers(aEvent);
}

template<typename PanGestureOrScrollWheelInput>
static bool
WillHandleInput(const PanGestureOrScrollWheelInput& aPanInput)
{
  if (!NS_IsMainThread()) {
    return true;
  }

  WidgetWheelEvent wheelEvent = aPanInput.ToWidgetWheelEvent(nullptr);
  return WillHandleWheelEvent(&wheelEvent);
}

void
APZCTreeManager::FlushApzRepaints(uint64_t aLayersId)
{
  APZCTM_LOG("Flushing repaints for layers id %" PRIu64, aLayersId);
  { // scope lock
    MonitorAutoLock lock(mTreeLock);
    FlushPendingRepaintRecursively(mRootNode, aLayersId);
  }
  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(aLayersId);
  MOZ_ASSERT(state && state->mController);
  NS_DispatchToMainThread(NS_NewRunnableMethod(
    state->mController.get(), &GeckoContentController::NotifyFlushComplete));
}

nsEventStatus
APZCTreeManager::ReceiveInputEvent(InputData& aEvent,
                                   ScrollableLayerGuid* aOutTargetGuid,
                                   uint64_t* aOutInputBlockId)
{
  APZThreadUtils::AssertOnControllerThread();

  // Initialize aOutInputBlockId to a sane value, and then later we overwrite
  // it if the input event goes into a block.
  if (aOutInputBlockId) {
    *aOutInputBlockId = InputBlockState::NO_BLOCK_ID;
  }
  nsEventStatus result = nsEventStatus_eIgnore;
  HitTestResult hitResult = HitNothing;
  switch (aEvent.mInputType) {
    case MULTITOUCH_INPUT: {
      MultiTouchInput& touchInput = aEvent.AsMultiTouchInput();
      result = ProcessTouchInput(touchInput, aOutTargetGuid, aOutInputBlockId);
      break;
    } case SCROLLWHEEL_INPUT: {
      FlushRepaintsToClearScreenToGeckoTransform();

      ScrollWheelInput& wheelInput = aEvent.AsScrollWheelInput();

      wheelInput.mHandledByAPZ = WillHandleInput(wheelInput);
      if (!wheelInput.mHandledByAPZ) {
        return result;
      }

      nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(wheelInput.mOrigin,
                                                            &hitResult);
      if (apzc) {
        MOZ_ASSERT(hitResult == HitLayer || hitResult == HitDispatchToContentRegion);

        // For wheel events, the call to ReceiveInputEvent below may result in
        // scrolling, which changes the async transform. However, the event we
        // want to pass to gecko should be the pre-scroll event coordinates,
        // transformed into the gecko space. (pre-scroll because the mouse
        // cursor is stationary during wheel scrolling, unlike touchmove
        // events). Since we just flushed the pending repaints the transform to
        // gecko space should only consist of overscroll-cancelling transforms.
        Matrix4x4 transformToGecko = GetScreenToApzcTransform(apzc)
                                   * GetApzcToGeckoTransform(apzc);
        MOZ_ASSERT(transformToGecko.Is2D());
        ScreenPoint untransformedOrigin = TransformTo<ScreenPixel>(
          transformToGecko, wheelInput.mOrigin);

        result = mInputQueue->ReceiveInputEvent(
          apzc,
          /* aTargetConfirmed = */ hitResult == HitLayer,
          wheelInput, aOutInputBlockId);

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);
        wheelInput.mOrigin = untransformedOrigin;
      }
      break;
    } case PANGESTURE_INPUT: {
      FlushRepaintsToClearScreenToGeckoTransform();

      PanGestureInput& panInput = aEvent.AsPanGestureInput();
      panInput.mHandledByAPZ = WillHandleInput(panInput);
      if (!panInput.mHandledByAPZ) {
        return result;
      }

      nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(panInput.mPanStartPoint,
                                                            &hitResult);
      if (apzc) {
        MOZ_ASSERT(hitResult == HitLayer || hitResult == HitDispatchToContentRegion);

        // For pan gesture events, the call to ReceiveInputEvent below may result in
        // scrolling, which changes the async transform. However, the event we
        // want to pass to gecko should be the pre-scroll event coordinates,
        // transformed into the gecko space. (pre-scroll because the mouse
        // cursor is stationary during pan gesture scrolling, unlike touchmove
        // events). Since we just flushed the pending repaints the transform to
        // gecko space should only consist of overscroll-cancelling transforms.
        Matrix4x4 transformToGecko = GetScreenToApzcTransform(apzc)
                                   * GetApzcToGeckoTransform(apzc);
        MOZ_ASSERT(transformToGecko.Is2D());
        ScreenPoint untransformedStartPoint = TransformTo<ScreenPixel>(
          transformToGecko, panInput.mPanStartPoint);
        ScreenPoint untransformedDisplacement = TransformVector<ScreenPixel>(
            transformToGecko, panInput.mPanDisplacement, panInput.mPanStartPoint);

        result = mInputQueue->ReceiveInputEvent(
            apzc,
            /* aTargetConfirmed = */ hitResult == HitLayer,
            panInput, aOutInputBlockId);

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);
        panInput.mPanStartPoint = untransformedStartPoint;
        panInput.mPanDisplacement = untransformedDisplacement;
      }
      break;
    } case PINCHGESTURE_INPUT: {  // note: no one currently sends these
      PinchGestureInput& pinchInput = aEvent.AsPinchGestureInput();
      nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(pinchInput.mFocusPoint,
                                                            &hitResult);
      if (apzc) {
        MOZ_ASSERT(hitResult == HitLayer || hitResult == HitDispatchToContentRegion);

        result = mInputQueue->ReceiveInputEvent(
            apzc,
            /* aTargetConfirmed = */ hitResult == HitLayer,
            pinchInput, aOutInputBlockId);

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);
        Matrix4x4 outTransform = GetScreenToApzcTransform(apzc)
                               * GetApzcToGeckoTransform(apzc);
        MOZ_ASSERT(outTransform.Is2D());
        pinchInput.mFocusPoint = TransformTo<ScreenPixel>(
            outTransform, pinchInput.mFocusPoint);
      }
      break;
    } case TAPGESTURE_INPUT: {  // note: no one currently sends these
      TapGestureInput& tapInput = aEvent.AsTapGestureInput();
      nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(tapInput.mPoint,
                                                            &hitResult);
      if (apzc) {
        MOZ_ASSERT(hitResult == HitLayer || hitResult == HitDispatchToContentRegion);

        result = mInputQueue->ReceiveInputEvent(
            apzc,
            /* aTargetConfirmed = */ hitResult == HitLayer,
            tapInput, aOutInputBlockId);

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);
        Matrix4x4 outTransform = GetScreenToApzcTransform(apzc)
                               * GetApzcToGeckoTransform(apzc);
        MOZ_ASSERT(outTransform.Is2D());
        tapInput.mPoint = TransformTo<ScreenPixel>(outTransform, tapInput.mPoint);
      }
      break;
    }
  }
  return result;
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetTouchInputBlockAPZC(const MultiTouchInput& aEvent,
                                        HitTestResult* aOutHitResult)
{
  nsRefPtr<AsyncPanZoomController> apzc;
  if (aEvent.mTouches.Length() == 0) {
    return apzc.forget();
  }

  FlushRepaintsToClearScreenToGeckoTransform();

  apzc = GetTargetAPZC(aEvent.mTouches[0].mScreenPoint, aOutHitResult);
  for (size_t i = 1; i < aEvent.mTouches.Length(); i++) {
    nsRefPtr<AsyncPanZoomController> apzc2 = GetTargetAPZC(aEvent.mTouches[i].mScreenPoint, aOutHitResult);
    apzc = GetMultitouchTarget(apzc, apzc2);
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
    // If we are panned into overscroll and a second finger goes down,
    // ignore that second touch point completely. The touch-start for it is
    // dropped completely; subsequent touch events until the touch-end for it
    // will have this touch point filtered out.
    // (By contrast, if we're in overscroll but not panning, such as after
    // putting two fingers down during an overscroll animation, we process the
    // second touch and proceed to pinch.)
    if (mApzcForInputBlock && BuildOverscrollHandoffChain(mApzcForInputBlock)->HasApzcPannedIntoOverscroll()) {
      if (mRetainedTouchIdentifier == -1) {
        mRetainedTouchIdentifier = mApzcForInputBlock->GetLastTouchIdentifier();
      }
      return nsEventStatus_eConsumeNoDefault;
    }

    mHitResultForInputBlock = HitNothing;
    mApzcForInputBlock = GetTouchInputBlockAPZC(aInput, &mHitResultForInputBlock);
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
    MOZ_ASSERT(mHitResultForInputBlock == HitLayer || mHitResultForInputBlock == HitDispatchToContentRegion);

    mApzcForInputBlock->GetGuid(aOutTargetGuid);
    result = mInputQueue->ReceiveInputEvent(mApzcForInputBlock,
        /* aTargetConfirmed = */ mHitResultForInputBlock == HitLayer,
        aInput, aOutInputBlockId);

    // For computing the event to pass back to Gecko, use up-to-date transforms
    // (i.e. not anything cached in an input block).
    // This ensures that transformToApzc and transformToGecko are in sync.
    Matrix4x4 transformToApzc = GetScreenToApzcTransform(mApzcForInputBlock);
    Matrix4x4 transformToGecko = GetApzcToGeckoTransform(mApzcForInputBlock);
    Matrix4x4 outTransform = transformToApzc * transformToGecko;
    MOZ_ASSERT(outTransform.Is2D());
    
    for (size_t i = 0; i < aInput.mTouches.Length(); i++) {
      SingleTouchData& touchData = aInput.mTouches[i];
      touchData.mScreenPoint = TransformTo<ScreenPixel>(
          outTransform, touchData.mScreenPoint);
    }
  }

  mTouchCounter.Update(aInput);

  // If it's the end of the touch sequence then clear out variables so we
  // don't keep dangling references and leak things.
  if (mTouchCounter.GetActiveTouchCount() == 0) {
    mApzcForInputBlock = nullptr;
    mHitResultForInputBlock = HitNothing;
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
    MOZ_ASSERT(outTransform.Is2D());
    *aOutTransformedPoint = TransformTo<LayoutDevicePixel>(outTransform, aPoint);
  }
}

void
APZCTreeManager::UpdateWheelTransaction(WidgetInputEvent& aEvent)
{
  WheelBlockState* txn = mInputQueue->GetCurrentWheelTransaction();
  if (!txn) {
    return;
  }

  // If the transaction has simply timed out, we don't need to do anything
  // else.
  if (txn->MaybeTimeout(TimeStamp::Now())) {
    return;
  }

  switch (aEvent.mMessage) {
   case eMouseMove:
   case eDragOver: {
     WidgetMouseEvent* mouseEvent = aEvent.AsMouseEvent();
     if (!mouseEvent->IsReal()) {
       return;
     }

     ScreenIntPoint point =
       ViewAs<ScreenPixel>(aEvent.refPoint, PixelCastJustification::LayoutDeviceToScreenForUntransformedEvent);
     txn->OnMouseMove(point);
     return;
   }
   case eKeyPress:
   case eKeyUp:
   case eKeyDown:
   case eMouseUp:
   case eMouseDown:
   case eMouseDoubleClick:
   case eMouseClick:
   case eContextMenu:
   case eDrop:
     txn->EndTransaction();
     return;
   default:
     break;
  }
}

nsEventStatus
APZCTreeManager::ProcessEvent(WidgetInputEvent& aEvent,
                              ScrollableLayerGuid* aOutTargetGuid,
                              uint64_t* aOutInputBlockId)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsEventStatus result = nsEventStatus_eIgnore;

  // Note, we call this before having transformed the reference point.
  UpdateWheelTransaction(aEvent);

  // Transform the refPoint.
  // If the event hits an overscrolled APZC, instruct the caller to ignore it.
  HitTestResult hitResult = HitNothing;
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(ScreenPoint(aEvent.refPoint.x, aEvent.refPoint.y),
                                                        &hitResult);
  if (apzc) {
    MOZ_ASSERT(hitResult == HitLayer || hitResult == HitDispatchToContentRegion);
    apzc->GetGuid(aOutTargetGuid);
    Matrix4x4 transformToApzc = GetScreenToApzcTransform(apzc);
    Matrix4x4 transformToGecko = GetApzcToGeckoTransform(apzc);
    Matrix4x4 outTransform = transformToApzc * transformToGecko;
    MOZ_ASSERT(outTransform.Is2D());
    aEvent.refPoint = TransformTo<LayoutDevicePixel>(outTransform, aEvent.refPoint);
  }
  return result;
}

nsEventStatus
APZCTreeManager::ProcessWheelEvent(WidgetWheelEvent& aEvent,
                                   ScrollableLayerGuid* aOutTargetGuid,
                                   uint64_t* aOutInputBlockId)
{
  ScrollWheelInput::ScrollMode scrollMode = ScrollWheelInput::SCROLLMODE_INSTANT;
  if (aEvent.deltaMode == nsIDOMWheelEvent::DOM_DELTA_LINE &&
      gfxPrefs::SmoothScrollEnabled() && gfxPrefs::WheelSmoothScrollEnabled()) {
    scrollMode = ScrollWheelInput::SCROLLMODE_SMOOTH;
  }

  ScreenPoint origin(aEvent.refPoint.x, aEvent.refPoint.y);
  ScrollWheelInput input(aEvent.time, aEvent.timeStamp, 0,
                         scrollMode,
                         ScrollWheelInput::DeltaTypeForDeltaMode(aEvent.deltaMode),
                         origin,
                         aEvent.deltaX,
                         aEvent.deltaY);

  nsEventStatus status = ReceiveInputEvent(input, aOutTargetGuid, aOutInputBlockId);
  aEvent.refPoint.x = input.mOrigin.x;
  aEvent.refPoint.y = input.mOrigin.y;
  aEvent.mFlags.mHandledByAPZ = input.mHandledByAPZ;
  return status;
}

nsEventStatus
APZCTreeManager::ReceiveInputEvent(WidgetInputEvent& aEvent,
                                   ScrollableLayerGuid* aOutTargetGuid,
                                   uint64_t* aOutInputBlockId)
{
  // This function will be removed once metro code is modified to use the
  // InputData version of ReceiveInputEvent.
  // In general it is preferable to use the version of ReceiveInputEvent
  // that takes an InputData, as that is usable from off-main-thread.

  MOZ_ASSERT(NS_IsMainThread());
  APZThreadUtils::AssertOnControllerThread();

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
    case eWheelEventClass: {
      WidgetWheelEvent& wheelEvent = *aEvent.AsWheelEvent();
      if (WillHandleWheelEvent(&wheelEvent)) {
        return ProcessWheelEvent(wheelEvent, aOutTargetGuid, aOutInputBlockId);
      }
      return ProcessEvent(aEvent, aOutTargetGuid, aOutInputBlockId);
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
APZCTreeManager::ContentReceivedInputBlock(uint64_t aInputBlockId, bool aPreventDefault)
{
  APZThreadUtils::AssertOnControllerThread();

  mInputQueue->ContentReceivedInputBlock(aInputBlockId, aPreventDefault);
}

void
APZCTreeManager::SetTargetAPZC(uint64_t aInputBlockId,
                               const nsTArray<ScrollableLayerGuid>& aTargets)
{
  APZThreadUtils::AssertOnControllerThread();

  nsRefPtr<AsyncPanZoomController> target = nullptr;
  if (aTargets.Length() > 0) {
    target = GetTargetAPZC(aTargets[0]);
  }
  for (size_t i = 1; i < aTargets.Length(); i++) {
    nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aTargets[i]);
    target = GetMultitouchTarget(target, apzc);
  }
  mInputQueue->SetConfirmedTargetApzc(aInputBlockId, target);
}

void
APZCTreeManager::SetTargetAPZC(uint64_t aInputBlockId, const ScrollableLayerGuid& aTarget)
{
  APZThreadUtils::AssertOnControllerThread();

  nsRefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aTarget);
  mInputQueue->SetConfirmedTargetApzc(aInputBlockId, apzc);
}

void
APZCTreeManager::UpdateZoomConstraints(const ScrollableLayerGuid& aGuid,
                                       const Maybe<ZoomConstraints>& aConstraints)
{
  MonitorAutoLock lock(mTreeLock);
  nsRefPtr<HitTestingTreeNode> node = GetTargetNode(aGuid, nullptr);
  MOZ_ASSERT(!node || node->GetApzc()); // any node returned must have an APZC

  // Propagate the zoom constraints down to the subtree, stopping at APZCs
  // which have their own zoom constraints or are in a different layers id.
  if (aConstraints) {
    APZCTM_LOG("Recording constraints %s for guid %s\n",
      Stringify(aConstraints.value()).c_str(), Stringify(aGuid).c_str());
    mZoomConstraints[aGuid] = aConstraints.ref();
  } else {
    APZCTM_LOG("Removing constraints for guid %s\n", Stringify(aGuid).c_str());
    mZoomConstraints.erase(aGuid);
  }
  if (node && aConstraints) {
    UpdateZoomConstraintsRecursively(node.get(), aConstraints.ref());
  }
}

void
APZCTreeManager::UpdateZoomConstraintsRecursively(HitTestingTreeNode* aNode,
                                                  const ZoomConstraints& aConstraints)
{
  mTreeLock.AssertCurrentThreadOwns();

  if (aNode->IsPrimaryHolder()) {
    MOZ_ASSERT(aNode->GetApzc());
    aNode->GetApzc()->UpdateZoomConstraints(aConstraints);
  }
  for (HitTestingTreeNode* child = aNode->GetLastChild(); child; child = child->GetPrevSibling()) {
    if (AsyncPanZoomController* childApzc = child->GetApzc()) {
      // We can have subtrees with their own zoom constraints or separate layers
      // id - leave those alone.
      if (childApzc->HasNoParentWithSameLayersId() ||
          mZoomConstraints.find(childApzc->GetGuid()) != mZoomConstraints.end()) {
        continue;
      }
    }
    UpdateZoomConstraintsRecursively(child, aConstraints);
  }
}

void
APZCTreeManager::FlushRepaintsToClearScreenToGeckoTransform()
{
  // As the name implies, we flush repaint requests for the entire APZ tree in
  // order to clear the screen-to-gecko transform (aka the "untransform" applied
  // to incoming input events before they can be passed on to Gecko).
  //
  // The primary reason we do this is to avoid the problem where input events,
  // after being untransformed, end up hit-testing differently in Gecko. This
  // might happen in cases where the input event lands on content that is async-
  // scrolled into view, but Gecko still thinks it is out of view given the
  // visible area of a scrollframe.
  //
  // Another reason we want to clear the untransform is that if our APZ hit-test
  // hits a dispatch-to-content region then that's an ambiguous result and we
  // need to ask Gecko what actually got hit. In order to do this we need to
  // untransform the input event into Gecko space - but to do that we need to
  // know which APZC got hit! This leads to a circular dependency; the only way
  // to get out of it is to make sure that the untransform for all the possible
  // matched APZCs is the same. It is simplest to ensure that by flushing the
  // pending repaint requests, which makes all of the untransforms empty (and
  // therefore equal).
  MonitorAutoLock lock(mTreeLock);
  FlushRepaintsRecursively(mRootNode);
}

void
APZCTreeManager::FlushRepaintsRecursively(HitTestingTreeNode* aNode)
{
  mTreeLock.AssertCurrentThreadOwns();

  for (HitTestingTreeNode* node = aNode; node; node = node->GetPrevSibling()) {
    if (node->IsPrimaryHolder()) {
      MOZ_ASSERT(node->GetApzc());
      node->GetApzc()->FlushRepaintForNewInputBlock();
    }
    FlushRepaintsRecursively(node->GetLastChild());
  }
}

void
APZCTreeManager::FlushPendingRepaintRecursively(HitTestingTreeNode* aNode, uint64_t aLayersId)
{
  mTreeLock.AssertCurrentThreadOwns();

  for (HitTestingTreeNode* node = aNode; node; node = node->GetPrevSibling()) {
    if (node->IsPrimaryHolder()) {
      AsyncPanZoomController* apzc = node->GetApzc();
      MOZ_ASSERT(apzc);
      if (apzc->GetGuid().mLayersId == aLayersId) {
        apzc->FlushRepaintIfPending();
      }
    }
    FlushPendingRepaintRecursively(node->GetLastChild(), aLayersId);
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
  // Ensure that no references to APZCs are alive in any lingering input
  // blocks. This breaks cycles from InputBlockState::mTargetApzc back to
  // the InputQueue.
  APZThreadUtils::RunOnControllerThread(NewRunnableMethod(
    mInputQueue.get(), &InputQueue::Clear));

  MonitorAutoLock lock(mTreeLock);

  // This can be done as part of a tree walk but it's easier to
  // just re-use the Collect method that we need in other places.
  // If this is too slow feel free to change it to a recursive walk.
  nsTArray<nsRefPtr<HitTestingTreeNode>> nodesToDestroy;
  Collect(mRootNode, &nodesToDestroy);
  for (size_t i = 0; i < nodesToDestroy.Length(); i++) {
    nodesToDestroy[i]->Destroy();
  }
  mRootNode = nullptr;
}

nsRefPtr<HitTestingTreeNode>
APZCTreeManager::GetRootNode() const
{
  MonitorAutoLock lock(mTreeLock);
  return mRootNode;
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
 * @return true on success, false if aStartPoint or aEndPoint cannot be transformed into target's coordinate space
 */
static bool
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
  Maybe<ParentLayerPoint> startPoint = UntransformTo<ParentLayerPixel>(transformToApzc, screenStart);
  Maybe<ParentLayerPoint> endPoint = UntransformTo<ParentLayerPixel>(transformToApzc, screenEnd);
  if (!startPoint || !endPoint) {
    return false;
  }
  aEndPoint = *endPoint;
  aStartPoint = *startPoint;

  return true;
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
    if (!TransformDisplacement(this, aPrev, next, aStartPoint, aEndPoint)) {
      return false;
    }
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
      if (!TransformDisplacement(this,
                            aOverscrollHandoffChain->GetApzcAtIndex(startIndex - 1),
                            current,
                            startPoint,
                            endPoint)) {
          return false;
      }
    }

    transformedVelocity = endPoint - startPoint;

    if (current->AttemptFling(transformedVelocity,
                              aOverscrollHandoffChain,
                              aHandoff)) {
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
  nsRefPtr<HitTestingTreeNode> node = GetTargetNode(aGuid, nullptr);
  MOZ_ASSERT(!node || node->GetApzc()); // any node returned must have an APZC
  nsRefPtr<AsyncPanZoomController> apzc = node ? node->GetApzc() : nullptr;
  return apzc.forget();
}

already_AddRefed<HitTestingTreeNode>
APZCTreeManager::GetTargetNode(const ScrollableLayerGuid& aGuid,
                               GuidComparator aComparator)
{
  mTreeLock.AssertCurrentThreadOwns();
  nsRefPtr<HitTestingTreeNode> target = FindTargetNode(mRootNode, aGuid, aComparator);
  return target.forget();
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetTargetAPZC(const ScreenPoint& aPoint, HitTestResult* aOutHitResult)
{
  MonitorAutoLock lock(mTreeLock);
  HitTestResult hitResult = HitNothing;
  ParentLayerPoint point = ViewAs<ParentLayerPixel>(aPoint,
    PixelCastJustification::ScreenIsParentLayerForRoot);
  nsRefPtr<AsyncPanZoomController> target = GetAPZCAtPoint(mRootNode, point, &hitResult);

  if (aOutHitResult) {
    *aOutHitResult = hitResult;
  }
  return target.forget();
}

static bool
GuidComparatorIgnoringPresShell(const ScrollableLayerGuid& aOne, const ScrollableLayerGuid& aTwo)
{
  return aOne.mLayersId == aTwo.mLayersId
      && aOne.mScrollId == aTwo.mScrollId;
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

    // Guard against a possible infinite-loop condition. If we hit this, the
    // layout code that generates the handoff parents did something wrong.
    MOZ_ASSERT(apzc->GetScrollHandoffParentId() != apzc->GetGuid().mScrollId);

    // Find the AsyncPanZoomController instance with a matching layersId and
    // the scroll id that matches apzc->GetScrollHandoffParentId().
    // As an optimization, we start by walking up the APZC tree from 'apzc'
    // until we reach the top of the layer subtree for this layers id.
    AsyncPanZoomController* scrollParent = nullptr;
    AsyncPanZoomController* parent = apzc;
    while (!parent->HasNoParentWithSameLayersId()) {
      parent = parent->GetParent();
      // While walking up to find the root of the subtree, if we encounter the
      // handoff parent, we don't actually need to do the search so we can
      // just abort here.
      if (parent->GetGuid().mScrollId == apzc->GetScrollHandoffParentId()) {
        scrollParent = parent;
        break;
      }
    }
    // If that heuristic didn't turn up the scroll parent, do a full tree search.
    if (!scrollParent) {
      ScrollableLayerGuid guid(parent->GetGuid().mLayersId, 0, apzc->GetScrollHandoffParentId());
      nsRefPtr<HitTestingTreeNode> node = GetTargetNode(guid, &GuidComparatorIgnoringPresShell);
      MOZ_ASSERT(!node || node->GetApzc()); // any node returned must have an APZC
      scrollParent = node ? node->GetApzc() : nullptr;
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

HitTestingTreeNode*
APZCTreeManager::FindTargetNode(HitTestingTreeNode* aNode,
                                const ScrollableLayerGuid& aGuid,
                                GuidComparator aComparator)
{
  mTreeLock.AssertCurrentThreadOwns();

  // This walks the tree in depth-first, reverse order, so that it encounters
  // APZCs front-to-back on the screen.
  for (HitTestingTreeNode* node = aNode; node; node = node->GetPrevSibling()) {
    HitTestingTreeNode* match = FindTargetNode(node->GetLastChild(), aGuid, aComparator);
    if (match) {
      return match;
    }

    bool matches = false;
    if (node->GetApzc()) {
      if (aComparator) {
        matches = aComparator(aGuid, node->GetApzc()->GetGuid());
      } else {
        matches = node->GetApzc()->Matches(aGuid);
      }
    }
    if (matches) {
      return node;
    }
  }
  return nullptr;
}

AsyncPanZoomController*
APZCTreeManager::GetAPZCAtPoint(HitTestingTreeNode* aNode,
                                const ParentLayerPoint& aHitTestPoint,
                                HitTestResult* aOutHitResult)
{
  mTreeLock.AssertCurrentThreadOwns();

  // This walks the tree in depth-first, reverse order, so that it encounters
  // APZCs front-to-back on the screen.
  for (HitTestingTreeNode* node = aNode; node; node = node->GetPrevSibling()) {
    if (node->IsOutsideClip(aHitTestPoint)) {
      // If the point being tested is outside the clip region for this node
      // then we don't need to test against this node or any of its children.
      // Just skip it and move on.
      APZCTM_LOG("Point %f %f outside clip for node %p\n",
        aHitTestPoint.x, aHitTestPoint.y, node);
      continue;
    }

    AsyncPanZoomController* result = nullptr;

    // First check the subtree rooted at this node, because deeper nodes
    // are more "in front".
    Maybe<LayerPoint> hitTestPointForChildLayers = node->Untransform(aHitTestPoint);
    if (hitTestPointForChildLayers) {
      ParentLayerPoint childPoint = ViewAs<ParentLayerPixel>(hitTestPointForChildLayers.ref(),
        PixelCastJustification::MovingDownToChildren);
      result = GetAPZCAtPoint(node->GetLastChild(), childPoint, aOutHitResult);
    }

    // If we didn't match anything in the subtree, check |node|.
    if (*aOutHitResult == HitNothing) {
      APZCTM_LOG("Testing ParentLayer point %s (Layer %s) against node %p\n",
          Stringify(aHitTestPoint).c_str(),
          hitTestPointForChildLayers ? Stringify(hitTestPointForChildLayers.ref()).c_str() : "nil",
          node);
      HitTestResult hitResult = node->HitTest(aHitTestPoint);
      if (hitResult != HitTestResult::HitNothing) {
        result = node->GetNearestContainingApzcWithSameLayersId();
        if (!result) {
          result = FindRootApzcForLayersId(node->GetLayersId());
          MOZ_ASSERT(result);
        }
        APZCTM_LOG("Successfully matched APZC %p via node %p (hit result %d)\n",
             result, node, hitResult);
        MOZ_ASSERT(hitResult == HitLayer || hitResult == HitDispatchToContentRegion);
        // If event regions are disabled, *aOutHitResult will be HitLayer
        *aOutHitResult = hitResult;
      }
    }

    if (*aOutHitResult != HitNothing) {
      return result;
    }
  }

  return nullptr;
}

AsyncPanZoomController*
APZCTreeManager::FindRootApzcForLayersId(uint64_t aLayersId) const
{
  mTreeLock.AssertCurrentThreadOwns();

  const HitTestingTreeNode* resultNode = BreadthFirstSearch(mRootNode.get(),
      [aLayersId](const HitTestingTreeNode* aNode) {
        AsyncPanZoomController* apzc = aNode->GetApzc();
        return apzc
            && apzc->GetLayersId() == aLayersId
            && apzc->IsRootForLayersId();
      });
  return resultNode ? resultNode->GetApzc() : nullptr;
}

AsyncPanZoomController*
APZCTreeManager::FindRootContentApzcForLayersId(uint64_t aLayersId) const
{
  mTreeLock.AssertCurrentThreadOwns();

  const HitTestingTreeNode* resultNode = BreadthFirstSearch(mRootNode.get(),
      [aLayersId](const HitTestingTreeNode* aNode) {
        AsyncPanZoomController* apzc = aNode->GetApzc();
        return apzc
            && apzc->GetLayersId() == aLayersId
            && apzc->IsRootContent();
      });
  return resultNode ? resultNode->GetApzc() : nullptr;
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
    Matrix4x4 asyncUntransform = parent->GetCurrentAsyncTransformWithOverscroll().Inverse();
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
  Matrix4x4 asyncUntransform = aApzc->GetCurrentAsyncTransformWithOverscroll().Inverse();

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
APZCTreeManager::GetMultitouchTarget(AsyncPanZoomController* aApzc1, AsyncPanZoomController* aApzc2) const
{
  MonitorAutoLock lock(mTreeLock);
  nsRefPtr<AsyncPanZoomController> apzc;
  // For now, we only ever want to do pinching on the root-content APZC for
  // a given layers id.
  if (aApzc1 && aApzc2 && aApzc1->GetLayersId() == aApzc2->GetLayersId()) {
    // If the two APZCs have the same layers id, find the root-content APZC
    // for that layers id. Don't call CommonAncestor() because there may not
    // be a common ancestor for the layers id (e.g. if one APZCs is inside a
    // fixed-position element).
    apzc = FindRootContentApzcForLayersId(aApzc1->GetLayersId());
  } else {
    // Otherwise, find the common ancestor (to reach a common layers id), and
    // get the root-content APZC for that layers id.
    apzc = CommonAncestor(aApzc1, aApzc2);
    if (apzc) {
      apzc = FindRootContentApzcForLayersId(apzc->GetLayersId());
    }
  }
  return apzc.forget();
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::CommonAncestor(AsyncPanZoomController* aApzc1, AsyncPanZoomController* aApzc2) const
{
  mTreeLock.AssertCurrentThreadOwns();
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

} // namespace layers
} // namespace mozilla

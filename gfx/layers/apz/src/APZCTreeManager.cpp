/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stack>
#include <unordered_set>
#include "APZCTreeManager.h"
#include "AsyncPanZoomController.h"
#include "Compositor.h"                 // for Compositor
#include "DragTracker.h"                // for DragTracker
#include "gfxPrefs.h"                   // for gfxPrefs
#include "HitTestingTreeNode.h"         // for HitTestingTreeNode
#include "InputBlockState.h"            // for InputBlockState
#include "InputData.h"                  // for InputData, etc
#include "Layers.h"                     // for Layer, etc
#include "mozilla/dom/MouseEventBinding.h" // for MouseEvent constants
#include "mozilla/dom/Touch.h"          // for Touch
#include "mozilla/gfx/gfxVars.h"        // for gfxVars
#include "mozilla/gfx/GPUParent.h"      // for GPUParent
#include "mozilla/gfx/Logging.h"        // for gfx::TreeLog
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/layers/APZSampler.h"  // for APZSampler
#include "mozilla/layers/APZThreadUtils.h"  // for AssertOnControllerThread, etc
#include "mozilla/layers/APZUpdater.h"  // for APZUpdater
#include "mozilla/layers/AsyncCompositionManager.h" // for ViewTransform
#include "mozilla/layers/AsyncDragMetrics.h" // for AsyncDragMetrics
#include "mozilla/layers/CompositorBridgeParent.h" // for CompositorBridgeParent, etc
#include "mozilla/layers/LayerMetricsWrapper.h"
#include "mozilla/layers/WebRenderScrollDataWrapper.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/mozalloc.h"           // for operator new
#include "mozilla/TouchEvents.h"
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/EventStateManager.h"  // for WheelPrefs
#include "mozilla/webrender/WebRenderAPI.h"
#include "nsDebug.h"                    // for NS_WARNING
#include "nsPoint.h"                    // for nsIntPoint
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "OverscrollHandoffState.h"     // for OverscrollHandoffState
#include "TreeTraversal.h"              // for ForEachNode, BreadthFirstSearch, etc
#include "LayersLogging.h"              // for Stringify
#include "Units.h"                      // for ParentlayerPixel
#include "GestureEventListener.h"       // for GestureEventListener::setLongTapEnabled
#include "UnitTransforms.h"             // for ViewAs

#define ENABLE_APZCTM_LOGGING 0
// #define ENABLE_APZCTM_LOGGING 1

#if ENABLE_APZCTM_LOGGING
#  define APZCTM_LOG(...) printf_stderr("APZCTM: " __VA_ARGS__)
#else
#  define APZCTM_LOG(...)
#endif

// #define APZ_KEY_LOG(...) printf_stderr("APZKEY: " __VA_ARGS__)
#define APZ_KEY_LOG(...)

namespace mozilla {
namespace layers {

using mozilla::gfx::CompositorHitTestInfo;

typedef mozilla::gfx::Point Point;
typedef mozilla::gfx::Point4D Point4D;
typedef mozilla::gfx::Matrix4x4 Matrix4x4;

typedef CompositorBridgeParent::LayerTreeState LayerTreeState;

struct APZCTreeManager::TreeBuildingState {
  TreeBuildingState(LayersId aRootLayersId,
                    bool aIsFirstPaint, LayersId aOriginatingLayersId,
                    APZTestData* aTestData, uint32_t aPaintSequence)
    : mIsFirstPaint(aIsFirstPaint)
    , mOriginatingLayersId(aOriginatingLayersId)
    , mPaintLogger(aTestData, aPaintSequence)
  {
    CompositorBridgeParent::CallWithIndirectShadowTree(aRootLayersId,
        [this](LayerTreeState& aState) -> void {
          mCompositorController = aState.GetCompositorController();
          mInProcessSharingController = aState.InProcessSharingController();
        });
  }

  typedef std::unordered_map<AsyncPanZoomController*, gfx::Matrix4x4> DeferredTransformMap;

  // State that doesn't change as we recurse in the tree building
  RefPtr<CompositorController> mCompositorController;
  RefPtr<MetricsSharingController> mInProcessSharingController;
  const bool mIsFirstPaint;
  const LayersId mOriginatingLayersId;
  const APZPaintLogHelper mPaintLogger;

  // State that is updated as we perform the tree build

  // A list of nodes that need to be destroyed at the end of the tree building.
  // This is initialized with all nodes in the old tree, and nodes are removed
  // from it as we reuse them in the new tree.
  nsTArray<RefPtr<HitTestingTreeNode>> mNodesToDestroy;

  // This map is populated as we place APZCs into the new tree. Its purpose is
  // to facilitate re-using the same APZC for different layers that scroll
  // together (and thus have the same ScrollableLayerGuid). The presShellId
  // doesn't matter for this purpose, and we move the map to the APZCTreeManager
  // after we're done building, so it's useful to have the presshell-ignoring
  // map for that.
  std::unordered_map<ScrollableLayerGuid,
                     RefPtr<AsyncPanZoomController>,
                     ScrollableLayerGuid::HashIgnoringPresShellFn,
                     ScrollableLayerGuid::EqualIgnoringPresShellFn> mApzcMap;

  // This is populated with all the HitTestingTreeNodes that are scroll thumbs
  // and have a scrollthumb animation id (which indicates that they need to be
  // sampled for WebRender on the sampler thread).
  std::vector<HitTestingTreeNode*> mScrollThumbs;
  // This is populated with all the scroll target nodes. We use in conjunction
  // with mScrollThumbs to build APZCTreeManager::mScrollThumbInfo.
  std::unordered_map<ScrollableLayerGuid,
                     HitTestingTreeNode*,
                     ScrollableLayerGuid::HashIgnoringPresShellFn,
                     ScrollableLayerGuid::EqualIgnoringPresShellFn> mScrollTargets;

  // As the tree is traversed, the top element of this stack tracks whether
  // the parent scroll node has a perspective transform.
  std::stack<bool> mParentHasPerspective;

  // During the tree building process, the perspective transform component
  // of the ancestor transforms of some APZCs can be "deferred" to their
  // children, meaning they are added to the children's ancestor transforms
  // instead. Those deferred transforms are tracked here.
  DeferredTransformMap mPerspectiveTransformsDeferredToChildren;
};

class APZCTreeManager::CheckerboardFlushObserver : public nsIObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  explicit CheckerboardFlushObserver(APZCTreeManager* aTreeManager)
    : mTreeManager(aTreeManager)
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
    MOZ_ASSERT(obsSvc);
    if (obsSvc) {
      obsSvc->AddObserver(this, "APZ:FlushActiveCheckerboard", false);
    }
  }

  void Unregister()
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
    if (obsSvc) {
      obsSvc->RemoveObserver(this, "APZ:FlushActiveCheckerboard");
    }
    mTreeManager = nullptr;
  }

protected:
  virtual ~CheckerboardFlushObserver() = default;

private:
  RefPtr<APZCTreeManager> mTreeManager;
};

NS_IMPL_ISUPPORTS(APZCTreeManager::CheckerboardFlushObserver, nsIObserver)

NS_IMETHODIMP
APZCTreeManager::CheckerboardFlushObserver::Observe(nsISupports* aSubject,
                                                    const char* aTopic,
                                                    const char16_t*)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mTreeManager.get());

  RecursiveMutexAutoLock lock(mTreeManager->mTreeLock);
  if (mTreeManager->mRootNode) {
    ForEachNode<ReverseIterator>(mTreeManager->mRootNode.get(),
        [](HitTestingTreeNode* aNode)
        {
          if (aNode->IsPrimaryHolder()) {
            MOZ_ASSERT(aNode->GetApzc());
            aNode->GetApzc()->FlushActiveCheckerboardReport();
          }
        });
  }
  if (XRE_IsGPUProcess()) {
    if (gfx::GPUParent* gpu = gfx::GPUParent::GetSingleton()) {
      nsCString topic("APZ:FlushActiveCheckerboard:Done");
      Unused << gpu->SendNotifyUiObservers(topic);
    }
  } else {
    MOZ_ASSERT(XRE_IsParentProcess());
    nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
    if (obsSvc) {
      obsSvc->NotifyObservers(nullptr, "APZ:FlushActiveCheckerboard:Done", nullptr);
    }
  }
  return NS_OK;
}

/**
 * A RAII class used for setting the focus sequence number on input events
 * as they are being processed. Any input event is assumed to be potentially
 * focus changing unless explicitly marked otherwise.
 */
class MOZ_RAII AutoFocusSequenceNumberSetter
{
public:
  AutoFocusSequenceNumberSetter(FocusState& aFocusState, InputData& aEvent)
    : mFocusState(aFocusState)
    , mEvent(aEvent)
    , mMayChangeFocus(true)
  { }

  void MarkAsNonFocusChanging() { mMayChangeFocus = false; }

  ~AutoFocusSequenceNumberSetter()
  {
    if (mMayChangeFocus) {
      mFocusState.ReceiveFocusChangingEvent();

      APZ_KEY_LOG("Marking input with type=%d as focus changing with seq=%" PRIu64 "\n",
                  static_cast<int>(mEvent.mInputType),
                  mFocusState.LastAPZProcessedEvent());
    } else {
      APZ_KEY_LOG("Marking input with type=%d as non focus changing with seq=%" PRIu64 "\n",
                  static_cast<int>(mEvent.mInputType),
                  mFocusState.LastAPZProcessedEvent());
    }

    mEvent.mFocusSequenceNumber = mFocusState.LastAPZProcessedEvent();
  }

private:
  FocusState& mFocusState;
  InputData& mEvent;
  bool mMayChangeFocus;
};

APZCTreeManager::APZCTreeManager(LayersId aRootLayersId)
    : mInputQueue(new InputQueue()),
      mRootLayersId(aRootLayersId),
      mSampler(nullptr),
      mUpdater(nullptr),
      mTreeLock("APZCTreeLock"),
      mMapLock("APZCMapLock"),
      mHitResultForInputBlock(CompositorHitTestInfo::eInvisibleToHitTest),
      mRetainedTouchIdentifier(-1),
      mInScrollbarTouchDrag(false),
      mApzcTreeLog("apzctree"),
      mTestDataLock("APZTestDataLock"),
      mDPI(160.0)
{
  RefPtr<APZCTreeManager> self(this);
  NS_DispatchToMainThread(
    NS_NewRunnableFunction("layers::APZCTreeManager::APZCTreeManager", [self] {
      self->mFlushObserver = new CheckerboardFlushObserver(self);
    }));
  AsyncPanZoomController::InitializeGlobalState();
  mApzcTreeLog.ConditionOnPrefFunction(gfxPrefs::APZPrintTree);
#if defined(MOZ_WIDGET_ANDROID)
  mToolbarAnimator = new AndroidDynamicToolbarAnimator(this);
#endif // (MOZ_WIDGET_ANDROID)
}

APZCTreeManager::~APZCTreeManager() = default;

void
APZCTreeManager::SetSampler(APZSampler* aSampler)
{
  // We're either setting the sampler or clearing it
  MOZ_ASSERT((mSampler == nullptr) != (aSampler == nullptr));
  mSampler = aSampler;
}

void
APZCTreeManager::SetUpdater(APZUpdater* aUpdater)
{
  // We're either setting the updater or clearing it
  MOZ_ASSERT((mUpdater == nullptr) != (aUpdater == nullptr));
  mUpdater = aUpdater;
}

void
APZCTreeManager::NotifyLayerTreeAdopted(LayersId aLayersId,
                                        const RefPtr<APZCTreeManager>& aOldApzcTreeManager)
{
  AssertOnUpdaterThread();

  if (aOldApzcTreeManager) {
    aOldApzcTreeManager->mFocusState.RemoveFocusTarget(aLayersId);
    // While we could move the focus target information from the old APZC tree
    // manager into this one, it's safer to not do that, as we'll probably have
    // that information repopulated soon anyway (on the next layers update).
  }

  UniquePtr<APZTestData> adoptedData;
  if (aOldApzcTreeManager) {
    MutexAutoLock lock(aOldApzcTreeManager->mTestDataLock);
    auto it = aOldApzcTreeManager->mTestData.find(aLayersId);
    if (it != aOldApzcTreeManager->mTestData.end()) {
      adoptedData = Move(it->second);
      aOldApzcTreeManager->mTestData.erase(it);
    }
  }
  if (adoptedData) {
    MutexAutoLock lock(mTestDataLock);
    mTestData[aLayersId] = Move(adoptedData);
  }
}

void
APZCTreeManager::NotifyLayerTreeRemoved(LayersId aLayersId)
{
  AssertOnUpdaterThread();

  mFocusState.RemoveFocusTarget(aLayersId);

  { // scope lock
    MutexAutoLock lock(mTestDataLock);
    mTestData.erase(aLayersId);
  }
}

AsyncPanZoomController*
APZCTreeManager::NewAPZCInstance(LayersId aLayersId,
                                 GeckoContentController* aController)
{
  return new AsyncPanZoomController(aLayersId, this, mInputQueue,
    aController, AsyncPanZoomController::USE_GESTURE_DETECTOR);
}

TimeStamp
APZCTreeManager::GetFrameTime()
{
  return TimeStamp::Now();
}

void
APZCTreeManager::SetAllowedTouchBehavior(uint64_t aInputBlockId,
                                         const nsTArray<TouchBehaviorFlags> &aValues)
{
  APZThreadUtils::AssertOnControllerThread();

  mInputQueue->SetAllowedTouchBehavior(aInputBlockId, aValues);
}

template<class ScrollNode> void // ScrollNode is a LayerMetricsWrapper or a WebRenderScrollDataWrapper
APZCTreeManager::UpdateHitTestingTreeImpl(LayersId aRootLayerTreeId,
                                          const ScrollNode& aRoot,
                                          bool aIsFirstPaint,
                                          LayersId aOriginatingLayersId,
                                          uint32_t aPaintSequenceNumber)
{
  RecursiveMutexAutoLock lock(mTreeLock);

  // For testing purposes, we log some data to the APZTestData associated with
  // the layers id that originated this update.
  APZTestData* testData = nullptr;
  if (gfxPrefs::APZTestLoggingEnabled()) {
    MutexAutoLock lock(mTestDataLock);
    UniquePtr<APZTestData> ptr = MakeUnique<APZTestData>();
    auto result = mTestData.insert(std::make_pair(aOriginatingLayersId, Move(ptr)));
    testData = result.first->second.get();
    testData->StartNewPaint(aPaintSequenceNumber);
  }

  TreeBuildingState state(aRootLayerTreeId, aIsFirstPaint, aOriginatingLayersId,
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
  ForEachNode<ReverseIterator>(mRootNode.get(),
      [&state] (HitTestingTreeNode* aNode)
      {
        state.mNodesToDestroy.AppendElement(aNode);
      });
  mRootNode = nullptr;

  if (aRoot) {
    std::stack<gfx::TreeAutoIndent> indents;
    std::stack<AncestorTransform> ancestorTransforms;
    HitTestingTreeNode* parent = nullptr;
    HitTestingTreeNode* next = nullptr;
    LayersId layersId = aRootLayerTreeId;
    ancestorTransforms.push(AncestorTransform());
    state.mParentHasPerspective.push(false);

    mApzcTreeLog << "[start]\n";
    mTreeLock.AssertCurrentThreadIn();

    ForEachNode<ReverseIterator>(aRoot,
        [&](ScrollNode aLayerMetrics)
        {
          mApzcTreeLog << aLayerMetrics.Name() << '\t';

          HitTestingTreeNode* node = PrepareNodeForLayer(aLayerMetrics,
                aLayerMetrics.Metrics(), layersId, ancestorTransforms.top(),
                parent, next, state);
          MOZ_ASSERT(node);
          AsyncPanZoomController* apzc = node->GetApzc();
          aLayerMetrics.SetApzc(apzc);

          // GetScrollbarAnimationId is only non-zero when webrender is enabled,
          // which limits the extra thumb mapping work to the webrender-enabled
          // case where it is needed.
          // Note also that when webrender is enabled, a "valid" animation id
          // is always nonzero, so we don't need to worry about handling the
          // case where WR is enabled and the animation id is zero.
          if (node->IsScrollThumbNode() && node->GetScrollbarAnimationId()) {
            state.mScrollThumbs.push_back(node);
          }
          if (apzc && node->IsPrimaryHolder()) {
            state.mScrollTargets[apzc->GetGuid()] = node;
          }

          mApzcTreeLog << '\n';

          // Accumulate the CSS transform between layers that have an APZC.
          // In the terminology of the big comment above APZCTreeManager::GetScreenToApzcTransform, if
          // we are at layer M, then aAncestorTransform is NC * OC * PC, and we left-multiply MC and
          // compute ancestorTransform to be MC * NC * OC * PC. This gets passed down as the ancestor
          // transform to layer L when we recurse into the children below. If we are at a layer
          // with an APZC, such as P, then we reset the ancestorTransform to just PC, to start
          // the new accumulation as we go down.
          AncestorTransform currentTransform{aLayerMetrics.GetTransform(),
                                             aLayerMetrics.TransformIsPerspective()};
          if (!apzc) {
            currentTransform = currentTransform * ancestorTransforms.top();
          }
          ancestorTransforms.push(currentTransform);

          // Note that |node| at this point will not have any children, otherwise we
          // we would have to set next to node->GetFirstChild().
          MOZ_ASSERT(!node->GetFirstChild());
          parent = node;
          next = nullptr;

          // Update the layersId if we have a new one
          if (Maybe<LayersId> newLayersId = aLayerMetrics.GetReferentId()) {
            layersId = *newLayersId;
          }

          indents.push(gfx::TreeAutoIndent(mApzcTreeLog));
          state.mParentHasPerspective.push(aLayerMetrics.TransformIsPerspective());
        },
        [&](ScrollNode aLayerMetrics)
        {
          next = parent;
          parent = parent->GetParent();
          layersId = next->GetLayersId();
          ancestorTransforms.pop();
          indents.pop();
          state.mParentHasPerspective.pop();
        });

    mApzcTreeLog << "[end]\n";

    // If we have perspective transforms deferred to children, do another
    // walk of the tree and actually apply them to the children.
    // We can't do this "as we go" in the previous traversal, because by the
    // time we realize we need to defer a perspective transform for an APZC,
    // we may already have processed a previous layer (including children
    // found in its subtree) that shares that APZC.
    if (!state.mPerspectiveTransformsDeferredToChildren.empty()) {
      ForEachNode<ReverseIterator>(mRootNode.get(),
          [&state](HitTestingTreeNode* aNode) {
            AsyncPanZoomController* apzc = aNode->GetApzc();
            if (!apzc) {
              return;
            }
            if (!aNode->IsPrimaryHolder()) {
              return;
            }

            AsyncPanZoomController* parent = apzc->GetParent();
            if (!parent) {
              return;
            }

            auto it = state.mPerspectiveTransformsDeferredToChildren.find(parent);
            if (it != state.mPerspectiveTransformsDeferredToChildren.end()) {
              apzc->SetAncestorTransform(AncestorTransform{
                it->second * apzc->GetAncestorTransform(), false
              });
            }
          });
    }
  }

  // We do not support tree structures where the root node has siblings.
  MOZ_ASSERT(!(mRootNode && mRootNode->GetPrevSibling()));

  { // scope lock and update our mApzcMap before we destroy all the unused
    // APZC instances
    MutexAutoLock lock(mMapLock);
    mApzcMap = Move(state.mApzcMap);
    mScrollThumbInfo.clear();
    // For non-webrender, state.mScrollThumbs will be empty so this will be a
    // no-op.
    for (HitTestingTreeNode* thumb : state.mScrollThumbs) {
      MOZ_ASSERT(thumb->IsScrollThumbNode());
      ScrollableLayerGuid targetGuid(thumb->GetLayersId(), 0, thumb->GetScrollTargetId());
      auto it = state.mScrollTargets.find(targetGuid);
      if (it == state.mScrollTargets.end()) {
        // It could be that |thumb| is a scrollthumb for content which didn't
        // have an APZC, for example if the content isn't layerized. Regardless,
        // we can't async-scroll it so we don't need to worry about putting it
        // in mScrollThumbInfo.
        continue;
      }
      HitTestingTreeNode* target = it->second;
      mScrollThumbInfo.emplace_back(
          thumb->GetScrollbarAnimationId(),
          thumb->GetTransform(),
          thumb->GetScrollbarData(),
          targetGuid,
          target->GetTransform(),
          target->IsAncestorOf(thumb));
    }
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

void
APZCTreeManager::UpdateFocusState(LayersId aRootLayerTreeId,
                                  LayersId aOriginatingLayersId,
                                  const FocusTarget& aFocusTarget)
{
  AssertOnUpdaterThread();

  if (!gfxPrefs::APZKeyboardEnabled()) {
    return;
  }

  mFocusState.Update(aRootLayerTreeId,
                     aOriginatingLayersId,
                     aFocusTarget);
}

void
APZCTreeManager::UpdateHitTestingTree(LayersId aRootLayerTreeId,
                                      Layer* aRoot,
                                      bool aIsFirstPaint,
                                      LayersId aOriginatingLayersId,
                                      uint32_t aPaintSequenceNumber)
{
  AssertOnUpdaterThread();

  LayerMetricsWrapper root(aRoot);
  UpdateHitTestingTreeImpl(aRootLayerTreeId, root, aIsFirstPaint,
                           aOriginatingLayersId, aPaintSequenceNumber);
}

void
APZCTreeManager::UpdateHitTestingTree(LayersId aRootLayerTreeId,
                                      const WebRenderScrollDataWrapper& aScrollWrapper,
                                      bool aIsFirstPaint,
                                      LayersId aOriginatingLayersId,
                                      uint32_t aPaintSequenceNumber)
{
  AssertOnUpdaterThread();

  UpdateHitTestingTreeImpl(aRootLayerTreeId, aScrollWrapper, aIsFirstPaint,
                           aOriginatingLayersId, aPaintSequenceNumber);
}

void
APZCTreeManager::SampleForWebRender(wr::TransactionWrapper& aTxn,
                                    const TimeStamp& aSampleTime)
{
  AssertOnSamplerThread();
  MutexAutoLock lock(mMapLock);

  bool activeAnimations = false;
  for (const auto& mapping : mApzcMap) {
    AsyncPanZoomController* apzc = mapping.second;
    ParentLayerPoint layerTranslation = apzc->GetCurrentAsyncTransform(
        AsyncPanZoomController::eForCompositing).mTranslation;

    // The positive translation means the painted content is supposed to
    // move down (or to the right), and that corresponds to a reduction in
    // the scroll offset. Since we are effectively giving WR the async
    // scroll delta here, we want to negate the translation.
    ParentLayerPoint asyncScrollDelta = -layerTranslation;
    // XXX figure out what zoom-related conversions need to happen here.
    aTxn.UpdateScrollPosition(
        wr::AsPipelineId(apzc->GetGuid().mLayersId),
        apzc->GetGuid().mScrollId,
        wr::ToLayoutPoint(LayoutDevicePoint::FromUnknownPoint(asyncScrollDelta.ToUnknownPoint())));

    apzc->ReportCheckerboard(aSampleTime);
    activeAnimations |= apzc->AdvanceAnimations(aSampleTime);
  }

  // Now collect all the async transforms needed for the scrollthumbs.
  nsTArray<wr::WrTransformProperty> scrollbarTransforms;
  for (const ScrollThumbInfo& info : mScrollThumbInfo) {
    auto it = mApzcMap.find(info.mTargetGuid);
    if (it == mApzcMap.end()) {
      // It could be that |info| is a scrollthumb for content which didn't
      // have an APZC, for example if the content isn't layerized. Regardless,
      // we can't async-scroll it so we don't need to worry about putting it
      // in mScrollThumbInfo.
      continue;
    }
    AsyncPanZoomController* scrollTargetApzc = it->second;
    MOZ_ASSERT(scrollTargetApzc);
    LayerToParentLayerMatrix4x4 transform = scrollTargetApzc->CallWithLastContentPaintMetrics(
        [&](const FrameMetrics& aMetrics) {
            return ComputeTransformForScrollThumb(
                info.mThumbTransform * AsyncTransformMatrix(),
                info.mTargetTransform.ToUnknownMatrix(),
                scrollTargetApzc,
                aMetrics,
                info.mThumbData,
                info.mTargetIsAncestor,
                nullptr);
        });
    scrollbarTransforms.AppendElement(wr::ToWrTransformProperty(
        info.mThumbAnimationId,
        transform));
  }
  aTxn.AppendTransformProperties(scrollbarTransforms);

  if (activeAnimations) {
    RefPtr<CompositorController> controller;
    CompositorBridgeParent::CallWithIndirectShadowTree(mRootLayersId,
      [&](LayerTreeState& aState) -> void {
        controller = aState.GetCompositorController();
      });
    if (controller) {
      controller->ScheduleRenderOnCompositorThread();
    }
  }
}

// Compute the clip region to be used for a layer with an APZC. This function
// is only called for layers which actually have scrollable metrics and an APZC.
template<class ScrollNode> static ParentLayerIntRegion
ComputeClipRegion(const ScrollNode& aLayer)
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

  return clipRegion;
}

template<class ScrollNode> void
APZCTreeManager::PrintAPZCInfo(const ScrollNode& aLayer,
                               const AsyncPanZoomController* apzc)
{
  const FrameMetrics& metrics = aLayer.Metrics();
  mApzcTreeLog << "APZC " << apzc->GetGuid()
               << "\tcb=" << metrics.GetCompositionBounds()
               << "\tsr=" << metrics.GetScrollableRect()
               << (metrics.IsScrollInfoLayer() ? "\tscrollinfo" : "")
               << (apzc->HasScrollgrab() ? "\tscrollgrab" : "") << "\t"
               << aLayer.Metadata().GetContentDescription().get();
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

template<class ScrollNode> static EventRegions
GetEventRegions(const ScrollNode& aLayer)
{
  if (aLayer.Metrics().IsScrollInfoLayer()) {
    ParentLayerIntRect compositionBounds(RoundedToInt(aLayer.Metrics().GetCompositionBounds()));
    nsIntRegion hitRegion(compositionBounds.ToUnknownRect());
    EventRegions eventRegions(hitRegion);
    eventRegions.mDispatchToContentHitRegion = eventRegions.mHitRegion;
    return eventRegions;
  }
  return aLayer.GetEventRegions();
}



already_AddRefed<HitTestingTreeNode>
APZCTreeManager::RecycleOrCreateNode(TreeBuildingState& aState,
                                     AsyncPanZoomController* aApzc,
                                     LayersId aLayersId)
{
  // Find a node without an APZC and return it. Note that unless the layer tree
  // actually changes, this loop should generally do an early-return on the
  // first iteration, so it should be cheap in the common case.
  for (int32_t i = aState.mNodesToDestroy.Length() - 1; i >= 0; i--) {
    RefPtr<HitTestingTreeNode> node = aState.mNodesToDestroy[i];
    if (!node->IsPrimaryHolder()) {
      aState.mNodesToDestroy.RemoveElementAt(i);
      node->RecycleWith(aApzc, aLayersId);
      return node.forget();
    }
  }
  RefPtr<HitTestingTreeNode> node = new HitTestingTreeNode(aApzc, false, aLayersId);
  return node.forget();
}

template<class ScrollNode> static EventRegionsOverride
GetEventRegionsOverride(HitTestingTreeNode* aParent,
                       const ScrollNode& aLayer)
{
  // Make it so that if the flag is set on the layer tree, it automatically
  // propagates to all the nodes in the corresponding subtree rooted at that
  // layer in the hit-test tree. This saves having to walk up the tree every
  // we want to see if a hit-test node is affected by this flag.
  EventRegionsOverride result = aLayer.GetEventRegionsOverride();
  if (result != EventRegionsOverride::NoOverride) {
    // Overrides should only ever get set for ref layers.
    MOZ_ASSERT(aLayer.GetReferentId());
  }
  if (aParent) {
    result |= aParent->GetEventRegionsOverride();
  }
  return result;
}

void
APZCTreeManager::StartScrollbarDrag(const ScrollableLayerGuid& aGuid,
                                    const AsyncDragMetrics& aDragMetrics)
{
  APZThreadUtils::AssertOnControllerThread();

  RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (!apzc) {
    NotifyScrollbarDragRejected(aGuid);
    return;
  }

  uint64_t inputBlockId = aDragMetrics.mDragStartSequenceNumber;
  mInputQueue->ConfirmDragBlock(inputBlockId, apzc, aDragMetrics);
}

bool
APZCTreeManager::StartAutoscroll(const ScrollableLayerGuid& aGuid,
                                 const ScreenPoint& aAnchorLocation)
{
  APZThreadUtils::AssertOnControllerThread();

  RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (!apzc) {
    if (XRE_IsGPUProcess()) {
      // If we're in the compositor process, the "return false" will be
      // ignored because the query comes over the PAPZCTreeManager protocol
      // via an async message. In this case, send an explicit rejection
      // message to content.
      NotifyAutoscrollRejected(aGuid);
    }
    return false;
  }

  apzc->StartAutoscroll(aAnchorLocation);
  return true;
}

void
APZCTreeManager::StopAutoscroll(const ScrollableLayerGuid& aGuid)
{
  APZThreadUtils::AssertOnControllerThread();

  if (RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid)) {
    apzc->StopAutoscroll();
  }
}

void
APZCTreeManager::NotifyScrollbarDragRejected(const ScrollableLayerGuid& aGuid) const
{
  RefPtr<GeckoContentController> controller =
    GetContentController(aGuid.mLayersId);
  MOZ_ASSERT(controller);
  controller->NotifyAsyncScrollbarDragRejected(aGuid.mScrollId);
}

void
APZCTreeManager::NotifyAutoscrollRejected(const ScrollableLayerGuid& aGuid) const
{
  RefPtr<GeckoContentController> controller =
    GetContentController(aGuid.mLayersId);
  MOZ_ASSERT(controller);
  controller->NotifyAsyncAutoscrollRejected(aGuid.mScrollId);
}

template<class ScrollNode> HitTestingTreeNode*
APZCTreeManager::PrepareNodeForLayer(const ScrollNode& aLayer,
                                     const FrameMetrics& aMetrics,
                                     LayersId aLayersId,
                                     const AncestorTransform& aAncestorTransform,
                                     HitTestingTreeNode* aParent,
                                     HitTestingTreeNode* aNextSibling,
                                     TreeBuildingState& aState)
{
  mTreeLock.AssertCurrentThreadIn();

  bool needsApzc = true;
  if (!aMetrics.IsScrollable()) {
    needsApzc = false;
  }

  // XXX: As a future optimization we can probably stick these things on the
  // TreeBuildingState, and update them as we change layers id during the
  // traversal
  RefPtr<GeckoContentController> geckoContentController;
  RefPtr<MetricsSharingController> crossProcessSharingController;
  CompositorBridgeParent::CallWithIndirectShadowTree(aLayersId,
      [&](LayerTreeState& lts) -> void {
        geckoContentController = lts.mController;
        crossProcessSharingController = lts.CrossProcessSharingController();
      });

  if (!geckoContentController) {
    needsApzc = false;
  }

  bool parentHasPerspective = aState.mParentHasPerspective.top();

  RefPtr<HitTestingTreeNode> node = nullptr;
  if (!needsApzc) {
    // Note: if layer properties must be propagated to nodes, RecvUpdate in
    // LayerTransactionParent.cpp must ensure that APZ will be notified
    // when those properties change.
    node = RecycleOrCreateNode(aState, nullptr, aLayersId);
    AttachNodeToTree(node, aParent, aNextSibling);
    node->SetHitTestData(
        GetEventRegions(aLayer),
        aLayer.GetVisibleRegion(),
        aLayer.GetTransformTyped(),
        (!parentHasPerspective && aLayer.GetClipRect())
          ? Some(ParentLayerIntRegion(*aLayer.GetClipRect()))
          : Nothing(),
        GetEventRegionsOverride(aParent, aLayer),
        aLayer.IsBackfaceHidden());
    node->SetScrollbarData(aLayer.GetScrollbarAnimationId(),
                           aLayer.GetScrollbarData());
    node->SetFixedPosData(aLayer.GetFixedPositionScrollContainerId());
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
  APZCTM_LOG("Found APZC %p for layer %p with identifiers %" PRIx64 " %" PRId64 "\n",
      apzc, aLayer.GetLayer(), uint64_t(guid.mLayersId), guid.mScrollId);

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
      RefPtr<HitTestingTreeNode> n = aState.mNodesToDestroy[i];
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
      apzc = NewAPZCInstance(aLayersId, geckoContentController);
      apzc->SetCompositorController(aState.mCompositorController.get());
      if (crossProcessSharingController) {
        apzc->SetMetricsSharingController(crossProcessSharingController);
      } else {
        apzc->SetMetricsSharingController(aState.mInProcessSharingController.get());
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

    APZCTM_LOG("Using APZC %p for layer %p with identifiers %" PRIx64 " %" PRId64 "\n",
        apzc, aLayer.GetLayer(), uint64_t(aLayersId), aMetrics.GetScrollId());

    apzc->NotifyLayersUpdated(aLayer.Metadata(), aState.mIsFirstPaint,
        aLayersId == aState.mOriginatingLayersId);

    // Since this is the first time we are encountering an APZC with this guid,
    // the node holding it must be the primary holder. It may be newly-created
    // or not, depending on whether it went through the newApzc branch above.
    MOZ_ASSERT(node->IsPrimaryHolder() && node->GetApzc() && node->GetApzc()->Matches(guid));

    Maybe<ParentLayerIntRegion> clipRegion = parentHasPerspective
      ? Nothing()
      : Some(ComputeClipRegion(aLayer));
    node->SetHitTestData(
        GetEventRegions(aLayer),
        aLayer.GetVisibleRegion(),
        aLayer.GetTransformTyped(),
        clipRegion,
        GetEventRegionsOverride(aParent, aLayer),
        aLayer.IsBackfaceHidden());
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
      if (aMetrics.IsRootContent()) {
        aState.mPaintLogger.LogTestData(aMetrics.GetScrollId(),
            "isRootContent", true);
      }
      // Note that the async scroll offset is in ParentLayer pixels
      aState.mPaintLogger.LogTestData(aMetrics.GetScrollId(), "asyncScrollOffset",
          apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting));
      aState.mPaintLogger.LogTestData(aMetrics.GetScrollId(), "hasAsyncKeyScrolled",
          apzc->TestHasAsyncKeyScrolled());
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
    // ancestor be roughly the same. There are cases in which the transforms
    // are not exactly the same, for example if the parent is container layer
    // for an opacity, and this container layer has a resolution-induced scale
    // as its base transform and a prescale that is supposed to undo that scale.
    // Due to floating point inaccuracies those transforms can end up not quite
    // canceling each other. That's why we're using a fuzzy comparison here
    // instead of an exact one.
    // In addition, two ancestor transforms are allowed to differ if one of
    // them contains a perspective transform component and the other does not.
    // This represents situations where some content in a scrollable frame
    // is subject to a perspective transform and other content does not.
    // In such cases, go with the one that does not include the perspective
    // component; the perspective transform is remembered and applied to the
    // children instead.
    if (!aAncestorTransform.CombinedTransform().FuzzyEqualsMultiplicative(apzc->GetAncestorTransform())) {
      typedef TreeBuildingState::DeferredTransformMap::value_type PairType;
      if (!aAncestorTransform.ContainsPerspectiveTransform() &&
          !apzc->AncestorTransformContainsPerspective()) {
        MOZ_ASSERT(false, "Two layers that scroll together have different ancestor transforms");
      } else if (!aAncestorTransform.ContainsPerspectiveTransform()) {
        aState.mPerspectiveTransformsDeferredToChildren.insert(
            PairType{apzc, apzc->GetAncestorTransformPerspective()});
        apzc->SetAncestorTransform(aAncestorTransform);
      } else {
        aState.mPerspectiveTransformsDeferredToChildren.insert(
            PairType{apzc, aAncestorTransform.GetPerspectiveTransform()});
      }
    }

    Maybe<ParentLayerIntRegion> clipRegion = parentHasPerspective
      ? Nothing()
      : Some(ComputeClipRegion(aLayer));
    node->SetHitTestData(
        GetEventRegions(aLayer),
        aLayer.GetVisibleRegion(),
        aLayer.GetTransformTyped(),
        clipRegion,
        GetEventRegionsOverride(aParent, aLayer),
        aLayer.IsBackfaceHidden());
  }

  // Note: if layer properties must be propagated to nodes, RecvUpdate in
  // LayerTransactionParent.cpp must ensure that APZ will be notified
  // when those properties change.
  node->SetScrollbarData(aLayer.GetScrollbarAnimationId(),
                         aLayer.GetScrollbarData());
  node->SetFixedPosData(aLayer.GetFixedPositionScrollContainerId());
  return node;
}

template<typename PanGestureOrScrollWheelInput>
static bool
WillHandleInput(const PanGestureOrScrollWheelInput& aPanInput)
{
  if (!XRE_IsParentProcess() || !NS_IsMainThread()) {
    return true;
  }

  WidgetWheelEvent wheelEvent = aPanInput.ToWidgetWheelEvent(nullptr);
  return APZInputBridge::WillHandleWheelEvent(&wheelEvent);
}

void
APZCTreeManager::FlushApzRepaints(LayersId aLayersId)
{
  // Previously, paints were throttled and therefore this method was used to
  // ensure any pending paints were flushed. Now, paints are flushed
  // immediately, so it is safe to simply send a notification now.
  APZCTM_LOG("Flushing repaints for layers id 0x%" PRIx64 "\n", uint64_t(aLayersId));
  RefPtr<GeckoContentController> controller = GetContentController(aLayersId);
  MOZ_ASSERT(controller);
  controller->DispatchToRepaintThread(
    NewRunnableMethod("layers::GeckoContentController::NotifyFlushComplete",
                      controller,
                      &GeckoContentController::NotifyFlushComplete));
}

nsEventStatus
APZCTreeManager::ReceiveInputEvent(InputData& aEvent,
                                   ScrollableLayerGuid* aOutTargetGuid,
                                   uint64_t* aOutInputBlockId)
{
  APZThreadUtils::AssertOnControllerThread();

  // Use a RAII class for updating the focus sequence number of this event
  AutoFocusSequenceNumberSetter focusSetter(mFocusState, aEvent);

#if defined(MOZ_WIDGET_ANDROID)
  MOZ_ASSERT(mToolbarAnimator);
  ScreenPoint scrollOffset;
  {
    RecursiveMutexAutoLock lock(mTreeLock);
    RefPtr<AsyncPanZoomController> apzc = FindRootContentOrRootApzc();
    if (apzc) {
      scrollOffset = ViewAs<ScreenPixel>(apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting),
                                         PixelCastJustification::ScreenIsParentLayerForRoot);
    }
  }
  nsEventStatus isConsumed = mToolbarAnimator->ReceiveInputEvent(aEvent, scrollOffset);
  // Check if the mToolbarAnimator consumed the event.
  if (isConsumed == nsEventStatus_eConsumeNoDefault) {
    APZCTM_LOG("Dynamic toolbar consumed event");
    return isConsumed;
  }
#endif // (MOZ_WIDGET_ANDROID)

  // Initialize aOutInputBlockId to a sane value, and then later we overwrite
  // it if the input event goes into a block.
  if (aOutInputBlockId) {
    *aOutInputBlockId = InputBlockState::NO_BLOCK_ID;
  }
  nsEventStatus result = nsEventStatus_eIgnore;
  CompositorHitTestInfo hitResult = CompositorHitTestInfo::eInvisibleToHitTest;
  switch (aEvent.mInputType) {
    case MULTITOUCH_INPUT: {
      MultiTouchInput& touchInput = aEvent.AsMultiTouchInput();
      result = ProcessTouchInput(touchInput, aOutTargetGuid, aOutInputBlockId);
      break;
    } case MOUSE_INPUT: {
      MouseInput& mouseInput = aEvent.AsMouseInput();
      mouseInput.mHandledByAPZ = true;

      mCurrentMousePosition = mouseInput.mOrigin;

      bool startsDrag = DragTracker::StartsDrag(mouseInput);
      if (startsDrag) {
        // If this is the start of a drag we need to unambiguously know if it's
        // going to land on a scrollbar or not. We can't apply an untransform
        // here without knowing that, so we need to ensure the untransform is
        // a no-op.
        FlushRepaintsToClearScreenToGeckoTransform();
      }

      RefPtr<HitTestingTreeNode> hitScrollbarNode = nullptr;
      RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(mouseInput.mOrigin,
            &hitResult, &hitScrollbarNode);
      bool hitScrollbar = hitScrollbarNode;

      // When the mouse is outside the window we still want to handle dragging
      // but we won't find an APZC. Fallback to root APZC then.
      { // scope lock
        RecursiveMutexAutoLock lock(mTreeLock);
        if (!apzc && mRootNode) {
          apzc = mRootNode->GetApzc();
        }
      }

      if (apzc) {
        if (gfxPrefs::APZTestLoggingEnabled() && mouseInput.mType == MouseInput::MOUSE_HITTEST) {
          ScrollableLayerGuid guid = apzc->GetGuid();

          MutexAutoLock lock(mTestDataLock);
          auto it = mTestData.find(guid.mLayersId);
          MOZ_ASSERT(it != mTestData.end());
          it->second->RecordHitResult(mouseInput.mOrigin, hitResult, guid.mScrollId);
        }

        TargetConfirmationFlags confFlags{hitResult};
        bool apzDragEnabled = gfxPrefs::APZDragEnabled();
        if (apzDragEnabled && hitScrollbar) {
          // If scrollbar dragging is enabled and we hit a scrollbar, wait
          // for the main-thread confirmation because it contains drag metrics
          // that we need.
          confFlags.mTargetConfirmed = false;
        }
        result = mInputQueue->ReceiveInputEvent(
          apzc, confFlags, mouseInput, aOutInputBlockId);

        // If we're starting an async scrollbar drag
        if (apzDragEnabled && startsDrag && hitScrollbarNode &&
            hitScrollbarNode->IsScrollThumbNode() &&
            hitScrollbarNode->GetScrollbarData().mThumbIsAsyncDraggable) {
          SetupScrollbarDrag(mouseInput, hitScrollbarNode.get(), apzc.get());
        }

        if (result == nsEventStatus_eConsumeDoDefault) {
          // This input event is part of a drag block, so whether or not it is
          // directed at a scrollbar depends on whether the drag block started
          // on a scrollbar.
          hitScrollbar = mInputQueue->IsDragOnScrollbar(hitScrollbar);
        }

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);

        if (!hitScrollbar) {
          // The input was not targeted at a scrollbar, so we untransform it
          // like we do for other content. Scrollbars are "special" because they
          // have special handling in AsyncCompositionManager when resolution is
          // applied. TODO: we should find a better way to deal with this.
          ScreenToParentLayerMatrix4x4 transformToApzc = GetScreenToApzcTransform(apzc);
          ParentLayerToScreenMatrix4x4 transformToGecko = GetApzcToGeckoTransform(apzc);
          ScreenToScreenMatrix4x4 outTransform = transformToApzc * transformToGecko;
          Maybe<ScreenPoint> untransformedRefPoint = UntransformBy(
            outTransform, mouseInput.mOrigin);
          if (untransformedRefPoint) {
            mouseInput.mOrigin = *untransformedRefPoint;
          }
        } else {
          // Likewise, if the input was targeted at a scrollbar, we don't want to
          // apply the callback transform in the main thread, so we remove the
          // scrollid from the guid. We need to keep the layersId intact so
          // that the response from the child process doesn't get discarded.
          aOutTargetGuid->mScrollId = FrameMetrics::NULL_SCROLL_ID;
        }
      }
      break;
    } case SCROLLWHEEL_INPUT: {
      FlushRepaintsToClearScreenToGeckoTransform();

      ScrollWheelInput& wheelInput = aEvent.AsScrollWheelInput();
      wheelInput.mHandledByAPZ = WillHandleInput(wheelInput);
      if (!wheelInput.mHandledByAPZ) {
        return result;
      }

      RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(wheelInput.mOrigin,
                                                            &hitResult);
      if (apzc) {
        MOZ_ASSERT(hitResult != CompositorHitTestInfo::eInvisibleToHitTest);

        // For wheel events, the call to ReceiveInputEvent below may result in
        // scrolling, which changes the async transform. However, the event we
        // want to pass to gecko should be the pre-scroll event coordinates,
        // transformed into the gecko space. (pre-scroll because the mouse
        // cursor is stationary during wheel scrolling, unlike touchmove
        // events). Since we just flushed the pending repaints the transform to
        // gecko space should only consist of overscroll-cancelling transforms.
        ScreenToScreenMatrix4x4 transformToGecko = GetScreenToApzcTransform(apzc)
                                                 * GetApzcToGeckoTransform(apzc);
        Maybe<ScreenPoint> untransformedOrigin = UntransformBy(
          transformToGecko, wheelInput.mOrigin);

        if (!untransformedOrigin) {
          return result;
        }

        result = mInputQueue->ReceiveInputEvent(
          apzc,
          TargetConfirmationFlags{hitResult},
          wheelInput, aOutInputBlockId);

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);
        wheelInput.mOrigin = *untransformedOrigin;
      }
      break;
    } case PANGESTURE_INPUT: {
      FlushRepaintsToClearScreenToGeckoTransform();

      PanGestureInput& panInput = aEvent.AsPanGestureInput();
      panInput.mHandledByAPZ = WillHandleInput(panInput);
      if (!panInput.mHandledByAPZ) {
        return result;
      }

      // If/when we enable support for pan inputs off-main-thread, we'll need
      // to duplicate this EventStateManager code or something. See the call to
      // GetUserPrefsForWheelEvent in IAPZCTreeManager.cpp for why these fields
      // are stored separately.
      MOZ_ASSERT(NS_IsMainThread());
      WidgetWheelEvent wheelEvent = panInput.ToWidgetWheelEvent(nullptr);
      EventStateManager::GetUserPrefsForWheelEvent(&wheelEvent,
        &panInput.mUserDeltaMultiplierX,
        &panInput.mUserDeltaMultiplierY);

      RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(panInput.mPanStartPoint,
                                                            &hitResult);
      if (apzc) {
        MOZ_ASSERT(hitResult != CompositorHitTestInfo::eInvisibleToHitTest);

        // For pan gesture events, the call to ReceiveInputEvent below may result in
        // scrolling, which changes the async transform. However, the event we
        // want to pass to gecko should be the pre-scroll event coordinates,
        // transformed into the gecko space. (pre-scroll because the mouse
        // cursor is stationary during pan gesture scrolling, unlike touchmove
        // events). Since we just flushed the pending repaints the transform to
        // gecko space should only consist of overscroll-cancelling transforms.
        ScreenToScreenMatrix4x4 transformToGecko = GetScreenToApzcTransform(apzc)
                                                 * GetApzcToGeckoTransform(apzc);
        Maybe<ScreenPoint> untransformedStartPoint = UntransformBy(
          transformToGecko, panInput.mPanStartPoint);
        Maybe<ScreenPoint> untransformedDisplacement = UntransformVector(
            transformToGecko, panInput.mPanDisplacement, panInput.mPanStartPoint);

        if (!untransformedStartPoint || !untransformedDisplacement) {
          return result;
        }

        result = mInputQueue->ReceiveInputEvent(
            apzc,
            TargetConfirmationFlags{hitResult},
            panInput, aOutInputBlockId);

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);
        panInput.mPanStartPoint = *untransformedStartPoint;
        panInput.mPanDisplacement = *untransformedDisplacement;

        panInput.mOverscrollBehaviorAllowsSwipe =
            apzc->OverscrollBehaviorAllowsSwipe();
      }
      break;
    } case PINCHGESTURE_INPUT: {  // note: no one currently sends these
      PinchGestureInput& pinchInput = aEvent.AsPinchGestureInput();
      RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(pinchInput.mFocusPoint,
                                                            &hitResult);
      if (apzc) {
        MOZ_ASSERT(hitResult != CompositorHitTestInfo::eInvisibleToHitTest);

        ScreenToScreenMatrix4x4 outTransform = GetScreenToApzcTransform(apzc)
                                             * GetApzcToGeckoTransform(apzc);
        Maybe<ScreenPoint> untransformedFocusPoint = UntransformBy(
          outTransform, pinchInput.mFocusPoint);

        if (!untransformedFocusPoint) {
          return result;
        }

        result = mInputQueue->ReceiveInputEvent(
            apzc,
            TargetConfirmationFlags{hitResult},
            pinchInput, aOutInputBlockId);

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);
        pinchInput.mFocusPoint = *untransformedFocusPoint;
      }
      break;
    } case TAPGESTURE_INPUT: {  // note: no one currently sends these
      TapGestureInput& tapInput = aEvent.AsTapGestureInput();
      RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(tapInput.mPoint,
                                                            &hitResult);
      if (apzc) {
        MOZ_ASSERT(hitResult != CompositorHitTestInfo::eInvisibleToHitTest);

        ScreenToScreenMatrix4x4 outTransform = GetScreenToApzcTransform(apzc)
                                             * GetApzcToGeckoTransform(apzc);
        Maybe<ScreenIntPoint> untransformedPoint =
          UntransformBy(outTransform, tapInput.mPoint);

        if (!untransformedPoint) {
          return result;
        }

        result = mInputQueue->ReceiveInputEvent(
            apzc,
            TargetConfirmationFlags{hitResult},
            tapInput, aOutInputBlockId);

        // Update the out-parameters so they are what the caller expects.
        apzc->GetGuid(aOutTargetGuid);
        tapInput.mPoint = *untransformedPoint;
      }
      break;
    } case KEYBOARD_INPUT: {
      // Disable async keyboard scrolling when accessibility.browsewithcaret is enabled
      if (!gfxPrefs::APZKeyboardEnabled() ||
          gfxPrefs::AccessibilityBrowseWithCaret()) {
        APZ_KEY_LOG("Skipping key input from invalid prefs\n");
        return result;
      }

      KeyboardInput& keyInput = aEvent.AsKeyboardInput();

      // Try and find a matching shortcut for this keyboard input
      Maybe<KeyboardShortcut> shortcut = mKeyboardMap.FindMatch(keyInput);

      if (!shortcut) {
        APZ_KEY_LOG("Skipping key input with no shortcut\n");

        // If we don't have a shortcut for this key event, then we can keep our focus
        // only if we know there are no key event listeners for this target
        if (mFocusState.CanIgnoreKeyboardShortcutMisses()) {
          focusSetter.MarkAsNonFocusChanging();
        }
        return result;
      }

      // Check if this shortcut needs to be dispatched to content. Anything matching
      // this is assumed to be able to change focus.
      if (shortcut->mDispatchToContent) {
        APZ_KEY_LOG("Skipping key input with dispatch-to-content shortcut\n");
        return result;
      }

      // We know we have an action to execute on whatever is the current focus target
      const KeyboardScrollAction& action = shortcut->mAction;

      // The current focus target depends on which direction the scroll is to happen
      Maybe<ScrollableLayerGuid> targetGuid;
      switch (action.mType)
      {
        case KeyboardScrollAction::eScrollCharacter: {
          targetGuid = mFocusState.GetHorizontalTarget();
          break;
        }
        case KeyboardScrollAction::eScrollLine:
        case KeyboardScrollAction::eScrollPage:
        case KeyboardScrollAction::eScrollComplete: {
          targetGuid = mFocusState.GetVerticalTarget();
          break;
        }
      }

      // If we don't have a scroll target then either we have a stale focus target,
      // the focused element has event listeners, or the focused element doesn't have a
      // layerized scroll frame. In any case we need to dispatch to content.
      if (!targetGuid) {
        APZ_KEY_LOG("Skipping key input with no current focus target\n");
        return result;
      }

      RefPtr<AsyncPanZoomController> targetApzc = GetTargetAPZC(targetGuid->mLayersId,
                                                                targetGuid->mScrollId);

      if (!targetApzc) {
        APZ_KEY_LOG("Skipping key input with focus target but no APZC\n");
        return result;
      }

      // Attach the keyboard scroll action to the input event for processing
      // by the input queue.
      keyInput.mAction = action;

      APZ_KEY_LOG("Dispatching key input with apzc=%p\n",
                  targetApzc.get());

      // Dispatch the event to the input queue.
      result = mInputQueue->ReceiveInputEvent(
          targetApzc,
          TargetConfirmationFlags{true},
          keyInput, aOutInputBlockId);

      // Any keyboard event that is dispatched to the input queue at this point
      // should have been consumed
      MOZ_ASSERT(result == nsEventStatus_eConsumeDoDefault ||
                 result == nsEventStatus_eConsumeNoDefault);

      keyInput.mHandledByAPZ = true;
      focusSetter.MarkAsNonFocusChanging();

      break;
    }
  }
  return result;
}

static TouchBehaviorFlags
ConvertToTouchBehavior(CompositorHitTestInfo info)
{
  TouchBehaviorFlags result = AllowedTouchBehavior::UNKNOWN;
  if (info == CompositorHitTestInfo::eInvisibleToHitTest) {
    result = AllowedTouchBehavior::NONE;
  } else if (info & CompositorHitTestInfo::eDispatchToContent) {
    result = AllowedTouchBehavior::UNKNOWN;
  } else {
    result = AllowedTouchBehavior::VERTICAL_PAN
           | AllowedTouchBehavior::HORIZONTAL_PAN
           | AllowedTouchBehavior::PINCH_ZOOM
           | AllowedTouchBehavior::DOUBLE_TAP_ZOOM;
    if (info & CompositorHitTestInfo::eTouchActionPanXDisabled) {
      result &= ~AllowedTouchBehavior::HORIZONTAL_PAN;
    }
    if (info & CompositorHitTestInfo::eTouchActionPanYDisabled) {
      result &= ~AllowedTouchBehavior::VERTICAL_PAN;
    }
    if (info & CompositorHitTestInfo::eTouchActionPinchZoomDisabled) {
      result &= ~AllowedTouchBehavior::PINCH_ZOOM;
    }
    if (info & CompositorHitTestInfo::eTouchActionDoubleTapZoomDisabled) {
      result &= ~AllowedTouchBehavior::DOUBLE_TAP_ZOOM;
    }
  }
  return result;
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetTouchInputBlockAPZC(const MultiTouchInput& aEvent,
                                        nsTArray<TouchBehaviorFlags>* aOutTouchBehaviors,
                                        CompositorHitTestInfo* aOutHitResult,
                                        RefPtr<HitTestingTreeNode>* aOutHitScrollbarNode)
{
  RefPtr<AsyncPanZoomController> apzc;
  if (aEvent.mTouches.Length() == 0) {
    return apzc.forget();
  }

  FlushRepaintsToClearScreenToGeckoTransform();

  CompositorHitTestInfo hitResult;
  apzc = GetTargetAPZC(aEvent.mTouches[0].mScreenPoint, &hitResult,
      aOutHitScrollbarNode);
  if (aOutTouchBehaviors) {
    aOutTouchBehaviors->AppendElement(ConvertToTouchBehavior(hitResult));
  }
  for (size_t i = 1; i < aEvent.mTouches.Length(); i++) {
    RefPtr<AsyncPanZoomController> apzc2 = GetTargetAPZC(aEvent.mTouches[i].mScreenPoint, &hitResult);
    if (aOutTouchBehaviors) {
      aOutTouchBehaviors->AppendElement(ConvertToTouchBehavior(hitResult));
    }
    apzc = GetMultitouchTarget(apzc, apzc2);
    APZCTM_LOG("Using APZC %p as the root APZC for multi-touch\n", apzc.get());
    // A multi-touch gesture will not be a scrollbar drag, even if the
    // first touch point happened to hit a scrollbar.
    *aOutHitScrollbarNode = nullptr;
  }

  if (aOutHitResult) {
    // XXX we should probably be combining the hit results from the different
    // touch points somehow, instead of just using the last one.
    *aOutHitResult = hitResult;
  }
  return apzc.forget();
}

nsEventStatus
APZCTreeManager::ProcessTouchInput(MultiTouchInput& aInput,
                                   ScrollableLayerGuid* aOutTargetGuid,
                                   uint64_t* aOutInputBlockId)
{
  aInput.mHandledByAPZ = true;
  nsTArray<TouchBehaviorFlags> touchBehaviors;
  RefPtr<HitTestingTreeNode> hitScrollbarNode = nullptr;
  if (aInput.mType == MultiTouchInput::MULTITOUCH_START) {
    // If we are panned into overscroll and a second finger goes down,
    // ignore that second touch point completely. The touch-start for it is
    // dropped completely; subsequent touch events until the touch-end for it
    // will have this touch point filtered out.
    // (By contrast, if we're in overscroll but not panning, such as after
    // putting two fingers down during an overscroll animation, we process the
    // second touch and proceed to pinch.)
    if (mApzcForInputBlock &&
        mApzcForInputBlock->IsInPanningState() &&
        BuildOverscrollHandoffChain(mApzcForInputBlock)->HasOverscrolledApzc()) {
      if (mRetainedTouchIdentifier == -1) {
        mRetainedTouchIdentifier = mApzcForInputBlock->GetLastTouchIdentifier();
      }
      return nsEventStatus_eConsumeNoDefault;
    }

    mHitResultForInputBlock = CompositorHitTestInfo::eInvisibleToHitTest;
    mApzcForInputBlock = GetTouchInputBlockAPZC(aInput, &touchBehaviors,
        &mHitResultForInputBlock, &hitScrollbarNode);

    // Check if this event starts a scrollbar touch-drag. The conditions
    // checked are similar to the ones we check for MOUSE_INPUT starting
    // a scrollbar mouse-drag.
    mInScrollbarTouchDrag = gfxPrefs::APZDragEnabled() &&
                            gfxPrefs::APZTouchDragEnabled() && hitScrollbarNode &&
                            hitScrollbarNode->IsScrollThumbNode() &&
                            hitScrollbarNode->GetScrollbarData().mThumbIsAsyncDraggable;

    MOZ_ASSERT(touchBehaviors.Length() == aInput.mTouches.Length());
    for (size_t i = 0; i < touchBehaviors.Length(); i++) {
      APZCTM_LOG("Touch point has allowed behaviours 0x%02x\n", touchBehaviors[i]);
      if (touchBehaviors[i] == AllowedTouchBehavior::UNKNOWN) {
        // If there's any unknown items in the list, throw it out and we'll
        // wait for the main thread to send us a notification.
        touchBehaviors.Clear();
        break;
      }
    }
  } else if (mApzcForInputBlock) {
    APZCTM_LOG("Re-using APZC %p as continuation of event block\n", mApzcForInputBlock.get());
  }

  nsEventStatus result = nsEventStatus_eIgnore;

  if (mInScrollbarTouchDrag) {
    result = ProcessTouchInputForScrollbarDrag(aInput, hitScrollbarNode.get(),
        aOutTargetGuid, aOutInputBlockId);
  } else {
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
          if (!touchBehaviors.IsEmpty()) {
            MOZ_ASSERT(touchBehaviors.Length() > j);
            touchBehaviors.RemoveElementAt(j);
          }
          --j;
        }
      }
      if (aInput.mTouches.IsEmpty()) {
        return nsEventStatus_eConsumeNoDefault;
      }
    }

    if (mApzcForInputBlock) {
      MOZ_ASSERT(mHitResultForInputBlock != CompositorHitTestInfo::eInvisibleToHitTest);

      mApzcForInputBlock->GetGuid(aOutTargetGuid);
      uint64_t inputBlockId = 0;
      result = mInputQueue->ReceiveInputEvent(mApzcForInputBlock,
          TargetConfirmationFlags{mHitResultForInputBlock},
          aInput, &inputBlockId);
      if (aOutInputBlockId) {
        *aOutInputBlockId = inputBlockId;
      }
      if (!touchBehaviors.IsEmpty()) {
        mInputQueue->SetAllowedTouchBehavior(inputBlockId, touchBehaviors);
      }

      // For computing the event to pass back to Gecko, use up-to-date transforms
      // (i.e. not anything cached in an input block).
      // This ensures that transformToApzc and transformToGecko are in sync.
      ScreenToParentLayerMatrix4x4 transformToApzc = GetScreenToApzcTransform(mApzcForInputBlock);
      ParentLayerToScreenMatrix4x4 transformToGecko = GetApzcToGeckoTransform(mApzcForInputBlock);
      ScreenToScreenMatrix4x4 outTransform = transformToApzc * transformToGecko;

      for (size_t i = 0; i < aInput.mTouches.Length(); i++) {
        SingleTouchData& touchData = aInput.mTouches[i];
        Maybe<ScreenIntPoint> untransformedScreenPoint = UntransformBy(
            outTransform, touchData.mScreenPoint);
        if (!untransformedScreenPoint) {
          return nsEventStatus_eIgnore;
        }
        touchData.mScreenPoint = *untransformedScreenPoint;
      }
    }
  }

  mTouchCounter.Update(aInput);

  // If it's the end of the touch sequence then clear out variables so we
  // don't keep dangling references and leak things.
  if (mTouchCounter.GetActiveTouchCount() == 0) {
    mApzcForInputBlock = nullptr;
    mHitResultForInputBlock = CompositorHitTestInfo::eInvisibleToHitTest;
    mRetainedTouchIdentifier = -1;
    mInScrollbarTouchDrag = false;
  }

  return result;
}

MouseInput::MouseType
MultiTouchTypeToMouseType(MultiTouchInput::MultiTouchType aType)
{
  switch (aType)
  {
  case MultiTouchInput::MULTITOUCH_START:
    return MouseInput::MOUSE_DOWN;
  case MultiTouchInput::MULTITOUCH_MOVE:
    return MouseInput::MOUSE_MOVE;
  case MultiTouchInput::MULTITOUCH_END:
  case MultiTouchInput::MULTITOUCH_CANCEL:
    return MouseInput::MOUSE_UP;
  }
  MOZ_ASSERT_UNREACHABLE("Invalid multi-touch type");
  return MouseInput::MOUSE_NONE;
}

nsEventStatus
APZCTreeManager::ProcessTouchInputForScrollbarDrag(MultiTouchInput& aTouchInput,
                                                   const HitTestingTreeNode* aScrollThumbNode,
                                                   ScrollableLayerGuid* aOutTargetGuid,
                                                   uint64_t* aOutInputBlockId)
{
  MOZ_ASSERT(mRetainedTouchIdentifier == -1);
  MOZ_ASSERT(mApzcForInputBlock);
  MOZ_ASSERT(aTouchInput.mTouches.Length() == 1);

  // Synthesize a mouse event based on the touch event, so that we can
  // reuse code in InputQueue and APZC for handling scrollbar mouse-drags.
  MouseInput mouseInput{MultiTouchTypeToMouseType(aTouchInput.mType),
                        MouseInput::LEFT_BUTTON,
                        dom::MouseEventBinding::MOZ_SOURCE_TOUCH,
                        WidgetMouseEvent::eLeftButtonFlag,
                        aTouchInput.mTouches[0].mScreenPoint,
                        aTouchInput.mTime,
                        aTouchInput.mTimeStamp,
                        aTouchInput.modifiers};
  mouseInput.mHandledByAPZ = true;

  // The value of |targetConfirmed| passed to InputQueue::ReceiveInputEvent()
  // only matters for the first event, which creates the drag block. For
  // that event, the correct value is false, since the drag block will, at the
  // earliest, be confirmed in the subsequent SetupScrollbarDrag() call.
  TargetConfirmationFlags targetConfirmed{false};

  nsEventStatus result = mInputQueue->ReceiveInputEvent(mApzcForInputBlock,
      targetConfirmed, mouseInput, aOutInputBlockId);

  // |aScrollThumbNode| is non-null iff. this is the event that starts the drag.
  // If so, set up the drag.
  if (aScrollThumbNode) {
    SetupScrollbarDrag(mouseInput, aScrollThumbNode, mApzcForInputBlock.get());
  }

  mApzcForInputBlock->GetGuid(aOutTargetGuid);

  // Since the input was targeted at a scrollbar:
  //    - The original touch event (which will be sent on to content) will
  //      not be untransformed.
  //    - We don't want to apply the callback transform in the main thread,
  //      so we remove the scrollid from the guid.
  // Both of these match the behaviour of mouse events that target a scrollbar;
  // see the code for handling mouse events in ReceiveInputEvent() for
  // additional explanation.
  aOutTargetGuid->mScrollId = FrameMetrics::NULL_SCROLL_ID;

  return result;
}

void
APZCTreeManager::SetupScrollbarDrag(MouseInput& aMouseInput,
                                    const HitTestingTreeNode* aScrollThumbNode,
                                    AsyncPanZoomController* aApzc)
{
  DragBlockState* dragBlock = mInputQueue->GetCurrentDragBlock();
  if (!dragBlock) {
    return;
  }

  const ScrollbarData& thumbData = aScrollThumbNode->GetScrollbarData();
  MOZ_ASSERT(thumbData.mDirection.isSome());

  // Record the thumb's position at the start of the drag.
  // We snap back to this position if, during the drag, the mouse
  // gets sufficiently far away from the scrollbar.
  dragBlock->SetInitialThumbPos(thumbData.mThumbStart);

  // Under some conditions, we can confirm the drag block right away.
  // Otherwise, we have to wait for a main-thread confirmation.
  if (gfxPrefs::APZDragInitiationEnabled() &&
      // check that the scrollbar's target scroll frame is layerized
      aScrollThumbNode->GetScrollTargetId() == aApzc->GetGuid().mScrollId &&
      !aApzc->IsScrollInfoLayer()) {
    uint64_t dragBlockId = dragBlock->GetBlockId();
    // AsyncPanZoomController::HandleInputEvent() will call
    // TransformToLocal() on the event, but we need its mLocalOrigin now
    // to compute a drag start offset for the AsyncDragMetrics.
    aMouseInput.TransformToLocal(aApzc->GetTransformToThis());
    CSSCoord dragStart = aApzc->ConvertScrollbarPoint(
        aMouseInput.mLocalOrigin, thumbData);
    // ConvertScrollbarPoint() got the drag start offset relative to
    // the scroll track. Now get it relative to the thumb.
    // ScrollThumbData::mThumbStart stores the offset of the thumb
    // relative to the scroll track at the time of the last paint.
    // Since that paint, the thumb may have acquired an async transform
    // due to async scrolling, so look that up and apply it.
    LayerToParentLayerMatrix4x4 thumbTransform;
    {
      RecursiveMutexAutoLock lock(mTreeLock);
      thumbTransform = ComputeTransformForNode(aScrollThumbNode);
    }
    // Only consider the translation, since we do not support both
    // zooming and scrollbar dragging on any platform.
    CSSCoord thumbStart = thumbData.mThumbStart
                        + ((*thumbData.mDirection == ScrollDirection::eHorizontal)
                           ? thumbTransform._41 : thumbTransform._42);
    dragStart -= thumbStart;

    // Content can't prevent scrollbar dragging with preventDefault(),
    // so we don't need to wait for a content response. It's important
    // to do this before calling ConfirmDragBlock() since that can
    // potentially process and consume the block.
    dragBlock->SetContentResponse(false);

    mInputQueue->ConfirmDragBlock(
        dragBlockId, aApzc,
        AsyncDragMetrics(aApzc->GetGuid().mScrollId,
                         aApzc->GetGuid().mPresShellId,
                         dragBlockId,
                         dragStart,
                         *thumbData.mDirection));
  }
}

void
APZCTreeManager::UpdateWheelTransaction(LayoutDeviceIntPoint aRefPoint,
                                        EventMessage aEventMessage)
{
  APZThreadUtils::AssertOnControllerThread();

  WheelBlockState* txn = mInputQueue->GetActiveWheelTransaction();
  if (!txn) {
    return;
  }

  // If the transaction has simply timed out, we don't need to do anything
  // else.
  if (txn->MaybeTimeout(TimeStamp::Now())) {
    return;
  }

  switch (aEventMessage) {
   case eMouseMove:
   case eDragOver: {

    ScreenIntPoint point =
     ViewAs<ScreenPixel>(aRefPoint,
       PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent);

    txn->OnMouseMove(point);

    return;
   }
   case eKeyPress:
   case eKeyUp:
   case eKeyDown:
   case eMouseUp:
   case eMouseDown:
   case eMouseDoubleClick:
   case eMouseAuxClick:
   case eMouseClick:
   case eContextMenu:
   case eDrop:
     txn->EndTransaction();
     return;
   default:
     break;
  }
}

void
APZCTreeManager::ProcessUnhandledEvent(LayoutDeviceIntPoint* aRefPoint,
                                        ScrollableLayerGuid*  aOutTargetGuid,
                                        uint64_t*             aOutFocusSequenceNumber)
{
  APZThreadUtils::AssertOnControllerThread();

  // Transform the aRefPoint.
  // If the event hits an overscrolled APZC, instruct the caller to ignore it.
  CompositorHitTestInfo hitResult = CompositorHitTestInfo::eInvisibleToHitTest;
  PixelCastJustification LDIsScreen = PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent;
  ScreenIntPoint refPointAsScreen =
    ViewAs<ScreenPixel>(*aRefPoint, LDIsScreen);
  RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(refPointAsScreen, &hitResult);
  if (apzc) {
    MOZ_ASSERT(hitResult != CompositorHitTestInfo::eInvisibleToHitTest);
    apzc->GetGuid(aOutTargetGuid);
    ScreenToParentLayerMatrix4x4 transformToApzc = GetScreenToApzcTransform(apzc);
    ParentLayerToScreenMatrix4x4 transformToGecko = GetApzcToGeckoTransform(apzc);
    ScreenToScreenMatrix4x4 outTransform = transformToApzc * transformToGecko;
    Maybe<ScreenIntPoint> untransformedRefPoint =
      UntransformBy(outTransform, refPointAsScreen);
    if (untransformedRefPoint) {
      *aRefPoint =
        ViewAs<LayoutDevicePixel>(*untransformedRefPoint, LDIsScreen);
    }
  }

  // Update the focus sequence number and attach it to the event
  mFocusState.ReceiveFocusChangingEvent();
  *aOutFocusSequenceNumber = mFocusState.LastAPZProcessedEvent();
}

void
APZCTreeManager::ProcessTouchVelocity(uint32_t aTimestampMs, float aSpeedY)
{
  if (mApzcForInputBlock) {
    mApzcForInputBlock->HandleTouchVelocity(aTimestampMs, aSpeedY);
  }
}

void
APZCTreeManager::SetKeyboardMap(const KeyboardMap& aKeyboardMap)
{
  APZThreadUtils::AssertOnControllerThread();

  mKeyboardMap = aKeyboardMap;
}

void
APZCTreeManager::ZoomToRect(const ScrollableLayerGuid& aGuid,
                            const CSSRect& aRect,
                            const uint32_t aFlags)
{
  // We could probably move this to run on the updater thread if needed, but
  // either way we should restrict it to a single thread. For now let's use the
  // controller thread.
  APZThreadUtils::AssertOnControllerThread();

  RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (apzc) {
    apzc->ZoomToRect(aRect, aFlags);
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

  RefPtr<AsyncPanZoomController> target = nullptr;
  if (aTargets.Length() > 0) {
    target = GetTargetAPZC(aTargets[0]);
  }
  for (size_t i = 1; i < aTargets.Length(); i++) {
    RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aTargets[i]);
    target = GetMultitouchTarget(target, apzc);
  }
  mInputQueue->SetConfirmedTargetApzc(aInputBlockId, target);
}

void
APZCTreeManager::SetTargetAPZC(uint64_t aInputBlockId, const ScrollableLayerGuid& aTarget)
{
  APZThreadUtils::AssertOnControllerThread();

  RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aTarget);
  mInputQueue->SetConfirmedTargetApzc(aInputBlockId, apzc);
}

void
APZCTreeManager::UpdateZoomConstraints(const ScrollableLayerGuid& aGuid,
                                       const Maybe<ZoomConstraints>& aConstraints)
{
  if (!GetUpdater()->IsUpdaterThread()) {
    // This can happen if we're in the UI process and got a call directly from
    // nsBaseWidget (as opposed to over PAPZCTreeManager). We want this function
    // to run on the updater thread, so bounce it over.
    MOZ_ASSERT(XRE_IsParentProcess());

    GetUpdater()->RunOnUpdaterThread(
        aGuid.mLayersId,
        NewRunnableMethod<ScrollableLayerGuid, Maybe<ZoomConstraints>>(
            "APZCTreeManager::UpdateZoomConstraints",
            this,
            &APZCTreeManager::UpdateZoomConstraints,
            aGuid,
            aConstraints));
    return;
  }

  AssertOnUpdaterThread();

  RecursiveMutexAutoLock lock(mTreeLock);
  RefPtr<HitTestingTreeNode> node = GetTargetNode(aGuid, nullptr);
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
    ForEachNode<ReverseIterator>(node.get(),
        [&aConstraints, &node, this](HitTestingTreeNode* aNode)
        {
          if (aNode != node) {
            if (AsyncPanZoomController* childApzc = aNode->GetApzc()) {
              // We can have subtrees with their own zoom constraints or separate layers
              // id - leave these alone.
              if (childApzc->HasNoParentWithSameLayersId() ||
                  this->mZoomConstraints.find(childApzc->GetGuid()) != this->mZoomConstraints.end()) {
                return TraversalFlag::Skip;
              }
            }
          }
          if (aNode->IsPrimaryHolder()) {
            MOZ_ASSERT(aNode->GetApzc());
            aNode->GetApzc()->UpdateZoomConstraints(aConstraints.ref());
          }
          return TraversalFlag::Continue;
        });
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
  RecursiveMutexAutoLock lock(mTreeLock);

  ForEachNode<ReverseIterator>(mRootNode.get(),
      [](HitTestingTreeNode* aNode)
      {
        if (aNode->IsPrimaryHolder()) {
          MOZ_ASSERT(aNode->GetApzc());
          aNode->GetApzc()->FlushRepaintForNewInputBlock();
        }
      });
}

void
APZCTreeManager::CancelAnimation(const ScrollableLayerGuid &aGuid)
{
  RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (apzc) {
    apzc->CancelAnimation();
  }
}

void
APZCTreeManager::AdjustScrollForSurfaceShift(const ScreenPoint& aShift)
{
  RecursiveMutexAutoLock lock(mTreeLock);
  RefPtr<AsyncPanZoomController> apzc = FindRootContentOrRootApzc();
  if (apzc) {
    apzc->AdjustScrollForSurfaceShift(aShift);
  }
}

void
APZCTreeManager::ClearTree()
{
  AssertOnUpdaterThread();

#if defined(MOZ_WIDGET_ANDROID)
  mToolbarAnimator->ClearTreeManager();
#endif

  // Ensure that no references to APZCs are alive in any lingering input
  // blocks. This breaks cycles from InputBlockState::mTargetApzc back to
  // the InputQueue.
  APZThreadUtils::RunOnControllerThread(NewRunnableMethod(
    "layers::InputQueue::Clear", mInputQueue, &InputQueue::Clear));

  RecursiveMutexAutoLock lock(mTreeLock);

  // Collect the nodes into a list, and then destroy each one.
  // We can't destroy them as we collect them, because ForEachNode()
  // does a pre-order traversal of the tree, and Destroy() nulls out
  // the fields needed to reach the children of the node.
  nsTArray<RefPtr<HitTestingTreeNode>> nodesToDestroy;
  ForEachNode<ReverseIterator>(mRootNode.get(),
      [&nodesToDestroy](HitTestingTreeNode* aNode)
      {
        nodesToDestroy.AppendElement(aNode);
      });

  for (size_t i = 0; i < nodesToDestroy.Length(); i++) {
    nodesToDestroy[i]->Destroy();
  }
  mRootNode = nullptr;

  RefPtr<APZCTreeManager> self(this);
  NS_DispatchToMainThread(
    NS_NewRunnableFunction("layers::APZCTreeManager::ClearTree", [self] {
      self->mFlushObserver->Unregister();
      self->mFlushObserver = nullptr;
    }));
}

RefPtr<HitTestingTreeNode>
APZCTreeManager::GetRootNode() const
{
  RecursiveMutexAutoLock lock(mTreeLock);
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
  if (aSource == aTarget) {
    return true;
  }

  // Convert start and end points to Screen coordinates.
  ParentLayerToScreenMatrix4x4 untransformToApzc = aTreeManager->GetScreenToApzcTransform(aSource).Inverse();
  ScreenPoint screenStart = TransformBy(untransformToApzc, aStartPoint);
  ScreenPoint screenEnd = TransformBy(untransformToApzc, aEndPoint);

  // Convert start and end points to aTarget's ParentLayer coordinates.
  ScreenToParentLayerMatrix4x4 transformToApzc = aTreeManager->GetScreenToApzcTransform(aTarget);
  Maybe<ParentLayerPoint> startPoint = UntransformBy(transformToApzc, screenStart);
  Maybe<ParentLayerPoint> endPoint = UntransformBy(transformToApzc, screenEnd);
  if (!startPoint || !endPoint) {
    return false;
  }
  aEndPoint = *endPoint;
  aStartPoint = *startPoint;

  return true;
}

void
APZCTreeManager::DispatchScroll(AsyncPanZoomController* aPrev,
                                ParentLayerPoint& aStartPoint,
                                ParentLayerPoint& aEndPoint,
                                OverscrollHandoffState& aOverscrollHandoffState)
{
  const OverscrollHandoffChain& overscrollHandoffChain = aOverscrollHandoffState.mChain;
  uint32_t overscrollHandoffChainIndex = aOverscrollHandoffState.mChainIndex;
  RefPtr<AsyncPanZoomController> next;
  // If we have reached the end of the overscroll handoff chain, there is
  // nothing more to scroll, so we ignore the rest of the pan gesture.
  if (overscrollHandoffChainIndex >= overscrollHandoffChain.Length()) {
    // Nothing more to scroll - ignore the rest of the pan gesture.
    return;
  }

  next = overscrollHandoffChain.GetApzcAtIndex(overscrollHandoffChainIndex);

  if (next == nullptr || next->IsDestroyed()) {
    return;
  }

  // Convert the start and end points from |aPrev|'s coordinate space to
  // |next|'s coordinate space.
  if (!TransformDisplacement(this, aPrev, next, aStartPoint, aEndPoint)) {
    return;
  }

  // Scroll |next|. If this causes overscroll, it will call DispatchScroll()
  // again with an incremented index.
  if (!next->AttemptScroll(aStartPoint, aEndPoint, aOverscrollHandoffState)) {
    // Transform |aStartPoint| and |aEndPoint| (which now represent the
    // portion of the displacement that wasn't consumed by APZCs later
    // in the handoff chain) back into |aPrev|'s coordinate space. This
    // allows the caller (which is |aPrev|) to interpret the unconsumed
    // displacement in its own coordinate space, and make use of it
    // (e.g. by going into overscroll).
    if (!TransformDisplacement(this, next, aPrev, aStartPoint, aEndPoint)) {
      NS_WARNING("Failed to untransform scroll points during dispatch");
    }
  }
}

ParentLayerPoint
APZCTreeManager::DispatchFling(AsyncPanZoomController* aPrev,
                               const FlingHandoffState& aHandoffState)
{
  // If immediate handoff is disallowed, do not allow handoff beyond the
  // single APZC that's scrolled by the input block that triggered this fling.
  if (aHandoffState.mIsHandoff &&
      !gfxPrefs::APZAllowImmediateHandoff() &&
      aHandoffState.mScrolledApzc == aPrev) {
    return aHandoffState.mVelocity;
  }

  const OverscrollHandoffChain* chain = aHandoffState.mChain;
  RefPtr<AsyncPanZoomController> current;
  uint32_t overscrollHandoffChainLength = chain->Length();
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

  if (aHandoffState.mIsHandoff) {
    startIndex = chain->IndexOf(aPrev) + 1;

    // IndexOf will return aOverscrollHandoffChain->Length() if
    // |aPrev| is not found.
    if (startIndex >= overscrollHandoffChainLength) {
      return aHandoffState.mVelocity;
    }
  } else {
    startIndex = 0;
  }

  // This will store any velocity left over after the entire handoff.
  ParentLayerPoint finalResidualVelocity = aHandoffState.mVelocity;

  ParentLayerPoint currentVelocity = aHandoffState.mVelocity;
  for (; startIndex < overscrollHandoffChainLength; startIndex++) {
    current = chain->GetApzcAtIndex(startIndex);

    // Make sure the apzc about to be handled can be handled
    if (current == nullptr || current->IsDestroyed()) {
      break;
    }

    endPoint = startPoint + currentVelocity;

    RefPtr<AsyncPanZoomController> prevApzc = (startIndex > 0)
                                            ? chain->GetApzcAtIndex(startIndex - 1)
                                            : nullptr;

    // Only transform when current apzc can be transformed with previous
    if (prevApzc) {
      if (!TransformDisplacement(this,
                                 prevApzc,
                                 current,
                                 startPoint,
                                 endPoint)) {
        break;
      }
    }

    ParentLayerPoint availableVelocity = (endPoint - startPoint);
    ParentLayerPoint residualVelocity;

    FlingHandoffState transformedHandoffState = aHandoffState;
    transformedHandoffState.mVelocity = availableVelocity;

    // Obey overscroll-behavior.
    if (prevApzc) {
      residualVelocity += prevApzc->AdjustHandoffVelocityForOverscrollBehavior(transformedHandoffState.mVelocity);
    }

    residualVelocity += current->AttemptFling(transformedHandoffState);

    // If there's no residual velocity, there's nothing more to hand off.
    if (IsZero(residualVelocity)) {
      return ParentLayerPoint();
    }

    // If any of the velocity available to be handed off was consumed,
    // subtract the proportion of consumed velocity from finalResidualVelocity.
    // Note: it's important to compare |residualVelocity| to |availableVelocity|
    // here and not to |transformedHandoffState.mVelocity|, since the latter
    // may have been modified by AdjustHandoffVelocityForOverscrollBehavior().
    if (!FuzzyEqualsAdditive(availableVelocity.x,
                             residualVelocity.x, COORDINATE_EPSILON)) {
      finalResidualVelocity.x *= (residualVelocity.x / availableVelocity.x);
    }
    if (!FuzzyEqualsAdditive(availableVelocity.y,
                             residualVelocity.y, COORDINATE_EPSILON)) {
      finalResidualVelocity.y *= (residualVelocity.y / availableVelocity.y);
    }

    currentVelocity = residualVelocity;
  }

  // Return any residual velocity left over after the entire handoff process.
  return finalResidualVelocity;
}

bool
APZCTreeManager::HitTestAPZC(const ScreenIntPoint& aPoint)
{
  RefPtr<AsyncPanZoomController> target = GetTargetAPZC(aPoint, nullptr);
  return target != nullptr;
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetTargetAPZC(const ScrollableLayerGuid& aGuid)
{
  RecursiveMutexAutoLock lock(mTreeLock);
  RefPtr<HitTestingTreeNode> node = GetTargetNode(aGuid, nullptr);
  MOZ_ASSERT(!node || node->GetApzc()); // any node returned must have an APZC
  RefPtr<AsyncPanZoomController> apzc = node ? node->GetApzc() : nullptr;
  return apzc.forget();
}

static bool
GuidComparatorIgnoringPresShell(const ScrollableLayerGuid& aOne, const ScrollableLayerGuid& aTwo)
{
  return aOne.mLayersId == aTwo.mLayersId
      && aOne.mScrollId == aTwo.mScrollId;
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetTargetAPZC(const LayersId& aLayersId,
                               const FrameMetrics::ViewID& aScrollId)
{
  MutexAutoLock lock(mMapLock);
  ScrollableLayerGuid guid(aLayersId, 0, aScrollId);
  auto it = mApzcMap.find(guid);
  RefPtr<AsyncPanZoomController> apzc = (it != mApzcMap.end() ? it->second : nullptr);
  return apzc.forget();
}

already_AddRefed<HitTestingTreeNode>
APZCTreeManager::GetTargetNode(const ScrollableLayerGuid& aGuid,
                               GuidComparator aComparator) const
{
  mTreeLock.AssertCurrentThreadIn();
  RefPtr<HitTestingTreeNode> target = DepthFirstSearchPostOrder<ReverseIterator>(mRootNode.get(),
      [&aGuid, &aComparator](HitTestingTreeNode* node)
      {
        bool matches = false;
        if (node->GetApzc()) {
          if (aComparator) {
            matches = aComparator(aGuid, node->GetApzc()->GetGuid());
          } else {
            matches = node->GetApzc()->Matches(aGuid);
          }
        }
        return matches;
      }
  );
  return target.forget();
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetTargetAPZC(const ScreenPoint& aPoint,
                               CompositorHitTestInfo* aOutHitResult,
                               RefPtr<HitTestingTreeNode>* aOutScrollbarNode)
{
  RecursiveMutexAutoLock lock(mTreeLock);

  CompositorHitTestInfo hitResult = CompositorHitTestInfo::eInvisibleToHitTest;
  HitTestingTreeNode* scrollbarNode = nullptr;
  RefPtr<AsyncPanZoomController> target;
  if (gfx::gfxVars::UseWebRender() && gfxPrefs::WebRenderHitTest()) {
    target = GetAPZCAtPointWR(aPoint, &hitResult, &scrollbarNode);
  } else {
    target = GetAPZCAtPoint(mRootNode, aPoint, &hitResult, &scrollbarNode);
  }

  if (aOutHitResult) {
    *aOutHitResult = hitResult;
  }
  if (aOutScrollbarNode) {
    *aOutScrollbarNode = scrollbarNode;
  }
  return target.forget();
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetAPZCAtPointWR(const ScreenPoint& aHitTestPoint,
                                  CompositorHitTestInfo* aOutHitResult,
                                  HitTestingTreeNode** aOutScrollbarNode)
{
  MOZ_ASSERT(aOutHitResult);
  MOZ_ASSERT(aOutScrollbarNode);

  RefPtr<AsyncPanZoomController> result;
  RefPtr<wr::WebRenderAPI> wr = GetWebRenderAPI();
  if (!wr) {
    // If WebRender isn't running, fall back to the root APZC.
    // This is mostly for the benefit of GTests which do not
    // run a WebRender instance, but gracefully falling back
    // here allows those tests which are not specifically
    // testing the hit-test algorithm to still work.
    result = FindRootApzcForLayersId(mRootLayersId);
    *aOutHitResult = CompositorHitTestInfo::eVisibleToHitTest;
    return result.forget();
  }

  wr::WrPipelineId pipelineId;
  FrameMetrics::ViewID scrollId;
  gfx::CompositorHitTestInfo hitInfo;
  bool hitSomething = wr->HitTest(wr::ToWorldPoint(aHitTestPoint),
      pipelineId, scrollId, hitInfo);
  if (!hitSomething) {
    return result.forget();
  }

  LayersId layersId = wr::AsLayersId(pipelineId);
  result = GetTargetAPZC(layersId, scrollId);
  if (!result) {
    // It falls back to the root
    // Re-enable these assertions once bug 1391318 is fixed. For now there are
    // race conditions with the WR hit-testing code that make these assertions
    // fail.
    //MOZ_ASSERT(scrollId == FrameMetrics::NULL_SCROLL_ID);
    result = FindRootApzcForLayersId(layersId);
    //MOZ_ASSERT(result);
  }

  bool isScrollbar = bool(hitInfo & gfx::CompositorHitTestInfo::eScrollbar);
  bool isScrollbarThumb = bool(hitInfo & gfx::CompositorHitTestInfo::eScrollbarThumb);
  ScrollDirection direction = (hitInfo & gfx::CompositorHitTestInfo::eScrollbarVertical)
                            ? ScrollDirection::eVertical
                            : ScrollDirection::eHorizontal;
  if (isScrollbar || isScrollbarThumb) {
    *aOutScrollbarNode = BreadthFirstSearch<ReverseIterator>(mRootNode.get(),
      [&](HitTestingTreeNode* aNode) {
        return (aNode->GetLayersId() == layersId) &&
               (aNode->IsScrollbarNode() == isScrollbar) &&
               (aNode->IsScrollThumbNode() == isScrollbarThumb) &&
               (aNode->GetScrollbarDirection() == direction) &&
               (aNode->GetScrollTargetId() == scrollId);
      });
  }

  *aOutHitResult = hitInfo;
  return result.forget();
}

RefPtr<const OverscrollHandoffChain>
APZCTreeManager::BuildOverscrollHandoffChain(const RefPtr<AsyncPanZoomController>& aInitialTarget)
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
  RecursiveMutexAutoLock lock(mTreeLock);

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
    RefPtr<AsyncPanZoomController> scrollParent =
        GetTargetAPZC(apzc->GetGuid().mLayersId, apzc->GetScrollHandoffParentId());
    apzc = scrollParent.get();
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

void
APZCTreeManager::SetLongTapEnabled(bool aLongTapEnabled)
{
  APZThreadUtils::AssertOnControllerThread();
  GestureEventListener::SetLongTapEnabled(aLongTapEnabled);
}

RefPtr<HitTestingTreeNode>
APZCTreeManager::FindScrollThumbNode(const AsyncDragMetrics& aDragMetrics)
{
  if (!aDragMetrics.mDirection) {
    // The AsyncDragMetrics has not been initialized yet - there will be
    // no matching node, so don't bother searching the tree.
    return RefPtr<HitTestingTreeNode>();
  }

  RecursiveMutexAutoLock lock(mTreeLock);

  return DepthFirstSearch<ReverseIterator>(mRootNode.get(),
      [&aDragMetrics](HitTestingTreeNode* aNode) {
        return aNode->MatchesScrollDragMetrics(aDragMetrics);
      });
}

AsyncPanZoomController*
APZCTreeManager::GetTargetApzcForNode(HitTestingTreeNode* aNode)
{
  for (const HitTestingTreeNode* n = aNode;
       n && n->GetLayersId() == aNode->GetLayersId();
       n = n->GetParent()) {
    if (n->GetApzc()) {
      APZCTM_LOG("Found target %p using ancestor lookup\n", n->GetApzc());
      return n->GetApzc();
    }
    if (n->GetFixedPosTarget() != FrameMetrics::NULL_SCROLL_ID) {
      RefPtr<AsyncPanZoomController> fpTarget =
          GetTargetAPZC(n->GetLayersId(), n->GetFixedPosTarget());
      APZCTM_LOG("Found target APZC %p using fixed-pos lookup on %" PRIu64 "\n", fpTarget.get(), n->GetFixedPosTarget());
      return fpTarget.get();
    }
  }
  return nullptr;
}

AsyncPanZoomController*
APZCTreeManager::GetAPZCAtPoint(HitTestingTreeNode* aNode,
                                const ScreenPoint& aHitTestPoint,
                                CompositorHitTestInfo* aOutHitResult,
                                HitTestingTreeNode** aOutScrollbarNode)
{
  mTreeLock.AssertCurrentThreadIn();

  // This walks the tree in depth-first, reverse order, so that it encounters
  // APZCs front-to-back on the screen.
  HitTestingTreeNode* resultNode;
  HitTestingTreeNode* root = aNode;
  std::stack<LayerPoint> hitTestPoints;
  ParentLayerPoint point = ViewAs<ParentLayerPixel>(aHitTestPoint,
      PixelCastJustification::ScreenIsParentLayerForRoot);
  hitTestPoints.push(ViewAs<LayerPixel>(point,
      PixelCastJustification::MovingDownToChildren));

  ForEachNode<ReverseIterator>(root,
      [&hitTestPoints, this](HitTestingTreeNode* aNode) {
        ParentLayerPoint hitTestPointForParent = ViewAs<ParentLayerPixel>(hitTestPoints.top(),
            PixelCastJustification::MovingDownToChildren);
        if (aNode->IsOutsideClip(hitTestPointForParent)) {
          // If the point being tested is outside the clip region for this node
          // then we don't need to test against this node or any of its children.
          // Just skip it and move on.
          APZCTM_LOG("Point %f %f outside clip for node %p\n",
            hitTestPoints.top().x, hitTestPoints.top().y, aNode);
          return TraversalFlag::Skip;
        }
        // First check the subtree rooted at this node, because deeper nodes
        // are more "in front".
        Maybe<LayerPoint> hitTestPoint = aNode->Untransform(
            hitTestPointForParent, ComputeTransformForNode(aNode));
        APZCTM_LOG("Transformed ParentLayer point %s to layer %s\n",
                Stringify(hitTestPointForParent).c_str(),
                hitTestPoint ? Stringify(hitTestPoint.ref()).c_str() : "nil");
        if (!hitTestPoint) {
          return TraversalFlag::Skip;
        }
        hitTestPoints.push(hitTestPoint.ref());
        return TraversalFlag::Continue;
      },
      [&resultNode, &hitTestPoints, &aOutHitResult](HitTestingTreeNode* aNode) {
        CompositorHitTestInfo hitResult = aNode->HitTest(hitTestPoints.top());
        hitTestPoints.pop();
        APZCTM_LOG("Testing Layer point %s against node %p\n",
                Stringify(hitTestPoints.top()).c_str(), aNode);
        if (hitResult != CompositorHitTestInfo::eInvisibleToHitTest) {
          resultNode = aNode;
          *aOutHitResult = hitResult;
          return TraversalFlag::Abort;
        }
        return TraversalFlag::Continue;
      }
  );

  if (*aOutHitResult != CompositorHitTestInfo::eInvisibleToHitTest) {
    MOZ_ASSERT(resultNode);
    for (HitTestingTreeNode* n = resultNode; n; n = n->GetParent()) {
      if (n->IsScrollbarNode()) {
        *aOutScrollbarNode = n;
        *aOutHitResult |= CompositorHitTestInfo::eScrollbar;
        if (n->IsScrollThumbNode()) {
          *aOutHitResult |= CompositorHitTestInfo::eScrollbarThumb;
        }
        if (n->GetScrollbarDirection() == ScrollDirection::eVertical) {
          *aOutHitResult |= CompositorHitTestInfo::eScrollbarVertical;
        }

        // If we hit a scrollbar, target the APZC for the content scrolled
        // by the scrollbar. (The scrollbar itself doesn't scroll with the
        // scrolled content, so it doesn't carry the scrolled content's
        // scroll metadata).
        RefPtr<AsyncPanZoomController> scrollTarget =
            GetTargetAPZC(n->GetLayersId(), n->GetScrollTargetId());
        if (scrollTarget) {
          return scrollTarget.get();
        }
      }
    }

    AsyncPanZoomController* result = GetTargetApzcForNode(resultNode);
    if (!result) {
      result = FindRootApzcForLayersId(resultNode->GetLayersId());
      MOZ_ASSERT(result);
      APZCTM_LOG("Found target %p using root lookup\n", result);
    }
    APZCTM_LOG("Successfully matched APZC %p via node %p (hit result 0x%x)\n",
        result, resultNode, (int)*aOutHitResult);
    return result;
  }

  return nullptr;
}

AsyncPanZoomController*
APZCTreeManager::FindRootApzcForLayersId(LayersId aLayersId) const
{
  mTreeLock.AssertCurrentThreadIn();

  HitTestingTreeNode* resultNode = BreadthFirstSearch<ReverseIterator>(mRootNode.get(),
      [aLayersId](HitTestingTreeNode* aNode) {
        AsyncPanZoomController* apzc = aNode->GetApzc();
        return apzc
            && apzc->GetLayersId() == aLayersId
            && apzc->IsRootForLayersId();
      });
  return resultNode ? resultNode->GetApzc() : nullptr;
}

AsyncPanZoomController*
APZCTreeManager::FindRootContentApzcForLayersId(LayersId aLayersId) const
{
  mTreeLock.AssertCurrentThreadIn();

  HitTestingTreeNode* resultNode = BreadthFirstSearch<ReverseIterator>(mRootNode.get(),
      [aLayersId](HitTestingTreeNode* aNode) {
        AsyncPanZoomController* apzc = aNode->GetApzc();
        return apzc
            && apzc->GetLayersId() == aLayersId
            && apzc->IsRootContent();
      });
  return resultNode ? resultNode->GetApzc() : nullptr;
}

AsyncPanZoomController*
APZCTreeManager::FindRootContentOrRootApzc() const
{
  mTreeLock.AssertCurrentThreadIn();

  // Note: this is intended to find the same "root" that would be found
  // by AsyncCompositionManager::ApplyAsyncContentTransformToTree inside
  // the MOZ_WIDGET_ANDROID block. That is, it should find the RCD node if there
  // is one, or the root APZC if there is not.
  // Since BreadthFirstSearch is a pre-order search, we first do a search for
  // the RCD, and then if we don't find one, we do a search for the root APZC.
  HitTestingTreeNode* resultNode = BreadthFirstSearch<ReverseIterator>(mRootNode.get(),
      [](HitTestingTreeNode* aNode) {
        AsyncPanZoomController* apzc = aNode->GetApzc();
        return apzc && apzc->IsRootContent();
      });
  if (resultNode) {
    return resultNode->GetApzc();
  }
  resultNode = BreadthFirstSearch<ReverseIterator>(mRootNode.get(),
      [](HitTestingTreeNode* aNode) {
        AsyncPanZoomController* apzc = aNode->GetApzc();
        return (apzc != nullptr);
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
ScreenToParentLayerMatrix4x4
APZCTreeManager::GetScreenToApzcTransform(const AsyncPanZoomController *aApzc) const
{
  Matrix4x4 result;
  RecursiveMutexAutoLock lock(mTreeLock);

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
    Matrix4x4 asyncUntransform = parent->GetCurrentAsyncTransformWithOverscroll(AsyncPanZoomController::eForHitTesting).Inverse().ToUnknownMatrix();
    // untransformSinceLastApzc is RC.Inverse() * QC.Inverse() * PA.Inverse()
    Matrix4x4 untransformSinceLastApzc = ancestorUntransform * asyncUntransform;

    // result is RC.Inverse() * QC.Inverse() * PA.Inverse() * PC.Inverse() * OC.Inverse() * NC.Inverse() * MC.Inverse()
    result = untransformSinceLastApzc * result;

    // The above value for result when parent == P matches the required output
    // as explained in the comment above this method. Note that any missing
    // terms are guaranteed to be identity transforms.
  }

  return ViewAs<ScreenToParentLayerMatrix4x4>(result);
}

/*
 * See the long comment above GetScreenToApzcTransform() for a detailed
 * explanation of this function.
 */
ParentLayerToScreenMatrix4x4
APZCTreeManager::GetApzcToGeckoTransform(const AsyncPanZoomController *aApzc) const
{
  Matrix4x4 result;
  RecursiveMutexAutoLock lock(mTreeLock);

  // The comments below assume there is a chain of layers L..R with L and P having APZC instances as
  // explained in the comment above. This function is called with aApzc at L, and the loop
  // below performs one iteration, where parent is at P. The comments explain what values are stored
  // in the variables at these two levels. All the comments use standard matrix notation where the
  // leftmost matrix in a multiplication is applied first.

  // asyncUntransform is LA.Inverse()
  Matrix4x4 asyncUntransform = aApzc->GetCurrentAsyncTransformWithOverscroll(AsyncPanZoomController::eForHitTesting).Inverse().ToUnknownMatrix();

  // aTransformToGeckoOut is initialized to LA.Inverse() * LD * MC * NC * OC * PC
  result = asyncUntransform * aApzc->GetTransformToLastDispatchedPaint() * aApzc->GetAncestorTransform();

  for (AsyncPanZoomController* parent = aApzc->GetParent(); parent; parent = parent->GetParent()) {
    // aTransformToGeckoOut is LA.Inverse() * LD * MC * NC * OC * PC * PD * QC * RC
    result = result * parent->GetTransformToLastDispatchedPaint() * parent->GetAncestorTransform();

    // The above value for result when parent == P matches the required output
    // as explained in the comment above this method. Note that any missing
    // terms are guaranteed to be identity transforms.
  }

  return ViewAs<ParentLayerToScreenMatrix4x4>(result);
}

ScreenPoint
APZCTreeManager::GetCurrentMousePosition() const
{
  return mCurrentMousePosition;
}

already_AddRefed<AsyncPanZoomController>
APZCTreeManager::GetMultitouchTarget(AsyncPanZoomController* aApzc1, AsyncPanZoomController* aApzc2) const
{
  RecursiveMutexAutoLock lock(mTreeLock);
  RefPtr<AsyncPanZoomController> apzc;
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
  mTreeLock.AssertCurrentThreadIn();
  RefPtr<AsyncPanZoomController> ancestor;

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

LayerToParentLayerMatrix4x4
APZCTreeManager::ComputeTransformForNode(const HitTestingTreeNode* aNode) const
{
  mTreeLock.AssertCurrentThreadIn();
  if (AsyncPanZoomController* apzc = aNode->GetApzc()) {
    // If the node represents scrollable content, apply the async transform
    // from its APZC.
    return aNode->GetTransform() *
        CompleteAsyncTransform(
          apzc->GetCurrentAsyncTransformWithOverscroll(AsyncPanZoomController::eForHitTesting));
  } else if (aNode->IsScrollThumbNode()) {
    // If the node represents a scrollbar thumb, compute and apply the
    // transformation that will be applied to the thumb in
    // AsyncCompositionManager.
    ScrollableLayerGuid guid{aNode->GetLayersId(), 0, aNode->GetScrollTargetId()};
    if (RefPtr<HitTestingTreeNode> scrollTargetNode = GetTargetNode(guid, &GuidComparatorIgnoringPresShell)) {
      AsyncPanZoomController* scrollTargetApzc = scrollTargetNode->GetApzc();
      MOZ_ASSERT(scrollTargetApzc);
      return scrollTargetApzc->CallWithLastContentPaintMetrics(
        [&](const FrameMetrics& aMetrics) {
          return ComputeTransformForScrollThumb(
              aNode->GetTransform() * AsyncTransformMatrix(),
              scrollTargetNode->GetTransform().ToUnknownMatrix(),
              scrollTargetApzc,
              aMetrics,
              aNode->GetScrollbarData(),
              scrollTargetNode->IsAncestorOf(aNode),
              nullptr);
        });
    }
  }
  // Otherwise, the node does not have an async transform.
  return aNode->GetTransform() * AsyncTransformMatrix();
}

already_AddRefed<wr::WebRenderAPI>
APZCTreeManager::GetWebRenderAPI() const
{
  RefPtr<wr::WebRenderAPI> api;
  CompositorBridgeParent::CallWithIndirectShadowTree(mRootLayersId,
    [&](LayerTreeState& aState) -> void {
      if (aState.mWrBridge) {
        api = aState.mWrBridge->GetWebRenderAPI();
      }
    });
  return api.forget();
}

already_AddRefed<GeckoContentController>
APZCTreeManager::GetContentController(LayersId aLayersId) const
{
  RefPtr<GeckoContentController> controller;
  CompositorBridgeParent::CallWithIndirectShadowTree(aLayersId,
    [&](LayerTreeState& aState) -> void {
      controller = aState.mController;
    });
  return controller.forget();
}

bool
APZCTreeManager::GetAPZTestData(LayersId aLayersId,
                                APZTestData* aOutData)
{
  AssertOnUpdaterThread();
  MutexAutoLock lock(mTestDataLock);
  auto it = mTestData.find(aLayersId);
  if (it == mTestData.end()) {
    return false;
  }
  *aOutData = *(it->second);
  return true;
}

/*static*/ LayerToParentLayerMatrix4x4
APZCTreeManager::ComputeTransformForScrollThumb(
    const LayerToParentLayerMatrix4x4& aCurrentTransform,
    const Matrix4x4& aScrollableContentTransform,
    AsyncPanZoomController* aApzc,
    const FrameMetrics& aMetrics,
    const ScrollbarData& aScrollbarData,
    bool aScrollbarIsDescendant,
    AsyncTransformComponentMatrix* aOutClipTransform)
{
  // We only apply the transform if the scroll-target layer has non-container
  // children (i.e. when it has some possibly-visible content). This is to
  // avoid moving scroll-bars in the situation that only a scroll information
  // layer has been built for a scroll frame, as this would result in a
  // disparity between scrollbars and visible content.
  if (aMetrics.IsScrollInfoLayer()) {
    return LayerToParentLayerMatrix4x4{};
  }

  MOZ_RELEASE_ASSERT(aApzc);

  AsyncTransformComponentMatrix asyncTransform =
    aApzc->GetCurrentAsyncTransform(AsyncPanZoomController::eForCompositing);

  // |asyncTransform| represents the amount by which we have scrolled and
  // zoomed since the last paint. Because the scrollbar was sized and positioned based
  // on the painted content, we need to adjust it based on asyncTransform so that
  // it reflects what the user is actually seeing now.
  AsyncTransformComponentMatrix scrollbarTransform;
  if (*aScrollbarData.mDirection == ScrollDirection::eVertical) {
    const ParentLayerCoord asyncScrollY = asyncTransform._42;
    const float asyncZoomY = asyncTransform._22;

    // The scroll thumb needs to be scaled in the direction of scrolling by the
    // inverse of the async zoom. This is because zooming in decreases the
    // fraction of the whole srollable rect that is in view.
    const float yScale = 1.f / asyncZoomY;

    // Note: |metrics.GetZoom()| doesn't yet include the async zoom.
    const CSSToParentLayerScale effectiveZoom(aMetrics.GetZoom().yScale * asyncZoomY);

    // Here we convert the scrollbar thumb ratio into a true unitless ratio by
    // dividing out the conversion factor from the scrollframe's parent's space
    // to the scrollframe's space.
    const float ratio = aScrollbarData.mThumbRatio /
        (aMetrics.GetPresShellResolution() * asyncZoomY);
    // The scroll thumb needs to be translated in opposite direction of the
    // async scroll. This is because scrolling down, which translates the layer
    // content up, should result in moving the scroll thumb down.
    ParentLayerCoord yTranslation = -asyncScrollY * ratio;

    // The scroll thumb additionally needs to be translated to compensate for
    // the scale applied above. The origin with respect to which the scale is
    // applied is the origin of the entire scrollbar, rather than the origin of
    // the scroll thumb (meaning, for a vertical scrollbar it's at the top of
    // the composition bounds). This means that empty space above the thumb
    // is scaled too, effectively translating the thumb. We undo that
    // translation here.
    // (One can think of the adjustment being done to the translation here as
    // a change of basis. We have a method to help with that,
    // Matrix4x4::ChangeBasis(), but it wouldn't necessarily make the code
    // cleaner in this case).
    const CSSCoord thumbOrigin = (aMetrics.GetScrollOffset().y * ratio);
    const CSSCoord thumbOriginScaled = thumbOrigin * yScale;
    const CSSCoord thumbOriginDelta = thumbOriginScaled - thumbOrigin;
    const ParentLayerCoord thumbOriginDeltaPL = thumbOriginDelta * effectiveZoom;
    yTranslation -= thumbOriginDeltaPL;

    if (aMetrics.IsRootContent()) {
      // Scrollbar for the root are painted at the same resolution as the
      // content. Since the coordinate space we apply this transform in includes
      // the resolution, we need to adjust for it as well here. Note that in
      // another metrics.IsRootContent() hunk below we apply a
      // resolution-cancelling transform which ensures the scroll thumb isn't
      // actually rendered at a larger scale.
      yTranslation *= aMetrics.GetPresShellResolution();
    }

    scrollbarTransform.PostScale(1.f, yScale, 1.f);
    scrollbarTransform.PostTranslate(0, yTranslation, 0);
  }
  if (*aScrollbarData.mDirection == ScrollDirection::eHorizontal) {
    // See detailed comments under the VERTICAL case.

    const ParentLayerCoord asyncScrollX = asyncTransform._41;
    const float asyncZoomX = asyncTransform._11;

    const float xScale = 1.f / asyncZoomX;

    const CSSToParentLayerScale effectiveZoom(aMetrics.GetZoom().xScale * asyncZoomX);

    const float ratio = aScrollbarData.mThumbRatio /
        (aMetrics.GetPresShellResolution() * asyncZoomX);
    ParentLayerCoord xTranslation = -asyncScrollX * ratio;

    const CSSCoord thumbOrigin = (aMetrics.GetScrollOffset().x * ratio);
    const CSSCoord thumbOriginScaled = thumbOrigin * xScale;
    const CSSCoord thumbOriginDelta = thumbOriginScaled - thumbOrigin;
    const ParentLayerCoord thumbOriginDeltaPL = thumbOriginDelta * effectiveZoom;
    xTranslation -= thumbOriginDeltaPL;

    if (aMetrics.IsRootContent()) {
      xTranslation *= aMetrics.GetPresShellResolution();
    }

    scrollbarTransform.PostScale(xScale, 1.f, 1.f);
    scrollbarTransform.PostTranslate(xTranslation, 0, 0);
  }

  LayerToParentLayerMatrix4x4 transform =
      aCurrentTransform * scrollbarTransform;

  AsyncTransformComponentMatrix compensation;
  // If the scrollbar layer is for the root then the content's resolution
  // applies to the scrollbar as well. Since we don't actually want the scroll
  // thumb's size to vary with the zoom (other than its length reflecting the
  // fraction of the scrollable length that's in view, which is taken care of
  // above), we apply a transform to cancel out this resolution.
  if (aMetrics.IsRootContent()) {
    compensation =
        AsyncTransformComponentMatrix::Scaling(
            aMetrics.GetPresShellResolution(),
            aMetrics.GetPresShellResolution(),
            1.0f).Inverse();
  }
  // If the scrollbar layer is a child of the content it is a scrollbar for,
  // then we need to adjust for any async transform (including an overscroll
  // transform) on the content. This needs to be cancelled out because layout
  // positions and sizes the scrollbar on the assumption that there is no async
  // transform, and without this adjustment the scrollbar will end up in the
  // wrong place.
  //
  // Note that since the async transform is applied on top of the content's
  // regular transform, we need to make sure to unapply the async transform in
  // the same coordinate space. This requires applying the content transform
  // and then unapplying it after unapplying the async transform.
  if (aScrollbarIsDescendant) {
    AsyncTransformComponentMatrix overscroll =
        aApzc->GetOverscrollTransform(AsyncPanZoomController::eForCompositing);
    Matrix4x4 asyncUntransform = (asyncTransform * overscroll).Inverse().ToUnknownMatrix();
    const Matrix4x4& contentTransform = aScrollableContentTransform;
    Matrix4x4 contentUntransform = contentTransform.Inverse();

    compensation *= ViewAs<AsyncTransformComponentMatrix>(
                        contentTransform
                      * asyncUntransform
                      * contentUntransform);

    // Pass the total compensation out to the caller so that it can use it
    // to transform clip transforms as needed.
    if (aOutClipTransform) {
      *aOutClipTransform = compensation;
    }
  }
  transform = transform * compensation;

  return transform;
}

APZSampler*
APZCTreeManager::GetSampler() const
{
  // We should always have a sampler here, since in practice the sampler
  // is destroyed at the same time that this APZCTreeMAnager instance is.
  MOZ_ASSERT(mSampler);
  return mSampler;
}

void
APZCTreeManager::AssertOnSamplerThread()
{
  GetSampler()->AssertOnSamplerThread();
}

APZUpdater*
APZCTreeManager::GetUpdater() const
{
  // We should always have an updater here, since in practice the updater
  // is destroyed at the same time that this APZCTreeManager instance is.
  MOZ_ASSERT(mUpdater);
  return mUpdater;
}

void
APZCTreeManager::AssertOnUpdaterThread()
{
  GetUpdater()->AssertOnUpdaterThread();
}

void
APZCTreeManager::LockTree()
{
  AssertOnUpdaterThread();
  mTreeLock.Lock();
}

void
APZCTreeManager::UnlockTree()
{
  AssertOnUpdaterThread();
  mTreeLock.Unlock();
}

void
APZCTreeManager::SetDPI(float aDpiValue)
{
  APZThreadUtils::AssertOnControllerThread();
  mDPI = aDpiValue;
}

float
APZCTreeManager::GetDPI() const
{
  APZThreadUtils::AssertOnControllerThread();
  return mDPI;
}

#if defined(MOZ_WIDGET_ANDROID)
AndroidDynamicToolbarAnimator*
APZCTreeManager::GetAndroidDynamicToolbarAnimator()
{
  return mToolbarAnimator;
}
#endif // defined(MOZ_WIDGET_ANDROID)

} // namespace layers
} // namespace mozilla

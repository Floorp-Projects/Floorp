/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stack>
#include <unordered_set>
#include "APZCTreeManager.h"
#include "AsyncPanZoomController.h"
#include "Compositor.h"             // for Compositor
#include "DragTracker.h"            // for DragTracker
#include "GenericFlingAnimation.h"  // for FLING_LOG
#include "HitTestingTreeNode.h"     // for HitTestingTreeNode
#include "InputBlockState.h"        // for InputBlockState
#include "InputData.h"              // for InputData, etc
#include "Layers.h"                 // for Layer, etc
#include "WRHitTester.h"            // for WRHitTester
#include "mozilla/RecursiveMutex.h"
#include "mozilla/dom/MouseEventBinding.h"  // for MouseEvent constants
#include "mozilla/dom/BrowserParent.h"      // for AreRecordReplayTabsActive
#include "mozilla/dom/Touch.h"              // for Touch
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/gfx/LoggingConstants.h"
#include "mozilla/gfx/gfxVars.h"            // for gfxVars
#include "mozilla/gfx/GPUParent.h"          // for GPUParent
#include "mozilla/gfx/Logging.h"            // for gfx::TreeLog
#include "mozilla/gfx/Point.h"              // for Point
#include "mozilla/layers/APZSampler.h"      // for APZSampler
#include "mozilla/layers/APZThreadUtils.h"  // for AssertOnControllerThread, etc
#include "mozilla/layers/APZUpdater.h"      // for APZUpdater
#include "mozilla/layers/APZUtils.h"        // for AsyncTransform
#include "mozilla/layers/AsyncDragMetrics.h"        // for AsyncDragMetrics
#include "mozilla/layers/CompositorBridgeParent.h"  // for CompositorBridgeParent, etc
#include "mozilla/layers/DoubleTapToZoom.h"         // for ZoomTarget
#include "mozilla/layers/MatrixMessage.h"
#include "mozilla/layers/UiCompositorControllerParent.h"
#include "mozilla/layers/WebRenderScrollDataWrapper.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/mozalloc.h"     // for operator new
#include "mozilla/Preferences.h"  // for Preferences
#include "mozilla/StaticPrefs_accessibility.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/ToString.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/EventStateManager.h"  // for WheelPrefs
#include "mozilla/webrender/WebRenderAPI.h"
#include "nsDebug.h"                 // for NS_WARNING
#include "nsPoint.h"                 // for nsIntPoint
#include "nsThreadUtils.h"           // for NS_IsMainThread
#include "ScrollThumbUtils.h"        // for ComputeTransformForScrollThumb
#include "OverscrollHandoffState.h"  // for OverscrollHandoffState
#include "TreeTraversal.h"           // for ForEachNode, BreadthFirstSearch, etc
#include "Units.h"                   // for ParentlayerPixel
#include "GestureEventListener.h"  // for GestureEventListener::setLongTapEnabled
#include "UnitTransforms.h"        // for ViewAs

mozilla::LazyLogModule mozilla::layers::APZCTreeManager::sLog("apz.manager");
#define APZCTM_LOG(...) \
  MOZ_LOG(APZCTreeManager::sLog, LogLevel::Debug, (__VA_ARGS__))

static mozilla::LazyLogModule sApzKeyLog("apz.key");
#define APZ_KEY_LOG(...) MOZ_LOG(sApzKeyLog, LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {
namespace layers {

using mozilla::gfx::CompositorHitTestFlags;
using mozilla::gfx::CompositorHitTestInfo;
using mozilla::gfx::CompositorHitTestInvisibleToHit;
using mozilla::gfx::LOG_DEFAULT;

typedef mozilla::gfx::Point Point;
typedef mozilla::gfx::Point4D Point4D;
typedef mozilla::gfx::Matrix4x4 Matrix4x4;

typedef CompositorBridgeParent::LayerTreeState LayerTreeState;

struct APZCTreeManager::TreeBuildingState {
  TreeBuildingState(LayersId aRootLayersId, bool aIsFirstPaint,
                    LayersId aOriginatingLayersId, APZTestData* aTestData,
                    uint32_t aPaintSequence)
      : mIsFirstPaint(aIsFirstPaint),
        mOriginatingLayersId(aOriginatingLayersId),
        mPaintLogger(aTestData, aPaintSequence) {
    CompositorBridgeParent::CallWithIndirectShadowTree(
        aRootLayersId, [this](LayerTreeState& aState) -> void {
          mCompositorController = aState.GetCompositorController();
        });
  }

  typedef std::unordered_map<AsyncPanZoomController*, gfx::Matrix4x4>
      DeferredTransformMap;

  // State that doesn't change as we recurse in the tree building
  RefPtr<CompositorController> mCompositorController;
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
  std::unordered_map<ScrollableLayerGuid, ApzcMapData,
                     ScrollableLayerGuid::HashIgnoringPresShellFn,
                     ScrollableLayerGuid::EqualIgnoringPresShellFn>
      mApzcMap;

  // This is populated with all the HitTestingTreeNodes that are scroll thumbs
  // and have a scrollthumb animation id (which indicates that they need to be
  // sampled for WebRender on the sampler thread).
  std::vector<HitTestingTreeNode*> mScrollThumbs;
  // This is populated with all the scroll target nodes. We use in conjunction
  // with mScrollThumbs to build APZCTreeManager::mScrollThumbInfo.
  std::unordered_map<ScrollableLayerGuid, HitTestingTreeNode*,
                     ScrollableLayerGuid::HashIgnoringPresShellFn,
                     ScrollableLayerGuid::EqualIgnoringPresShellFn>
      mScrollTargets;

  // During the tree building process, the perspective transform component
  // of the ancestor transforms of some APZCs can be "deferred" to their
  // children, meaning they are added to the children's ancestor transforms
  // instead. Those deferred transforms are tracked here.
  DeferredTransformMap mPerspectiveTransformsDeferredToChildren;

  // As we recurse down through the tree, this picks up the zoom animation id
  // from a node in the layer tree, and propagates it downwards to the nearest
  // APZC instance that is for an RCD node. Generally it will be set on the
  // root node of the layers (sub-)tree, which may not be same as the RCD node
  // for the subtree, and so we need this mechanism to ensure it gets propagated
  // to the RCD's APZC instance. Once it is set on the APZC instance, the value
  // is cleared back to Nothing(). Note that this is only used in the WebRender
  // codepath.
  Maybe<uint64_t> mZoomAnimationId;

  // See corresponding members of APZCTreeManager. These are the same thing, but
  // on the tree-walking state. They are populated while walking the tree in
  // a layers update, and then moved into APZCTreeManager.
  std::vector<FixedPositionInfo> mFixedPositionInfo;
  std::vector<RootScrollbarInfo> mRootScrollbarInfo;
  std::vector<StickyPositionInfo> mStickyPositionInfo;

  // As we recurse down through reflayers in the tree, this picks up the
  // cumulative EventRegionsOverride flags from the reflayers, and is used to
  // apply them to descendant layers.
  std::stack<EventRegionsOverride> mOverrideFlags;
};

class APZCTreeManager::CheckerboardFlushObserver : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  explicit CheckerboardFlushObserver(APZCTreeManager* aTreeManager)
      : mTreeManager(aTreeManager) {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
    MOZ_ASSERT(obsSvc);
    if (obsSvc) {
      obsSvc->AddObserver(this, "APZ:FlushActiveCheckerboard", false);
    }
  }

  void Unregister() {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
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
                                                    const char16_t*) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mTreeManager.get());

  RecursiveMutexAutoLock lock(mTreeManager->mTreeLock);
  if (mTreeManager->mRootNode) {
    ForEachNode<ReverseIterator>(
        mTreeManager->mRootNode.get(), [](HitTestingTreeNode* aNode) {
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
    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
    if (obsSvc) {
      obsSvc->NotifyObservers(nullptr, "APZ:FlushActiveCheckerboard:Done",
                              nullptr);
    }
  }
  return NS_OK;
}

/**
 * A RAII class used for setting the focus sequence number on input events
 * as they are being processed. Any input event is assumed to be potentially
 * focus changing unless explicitly marked otherwise.
 */
class MOZ_RAII AutoFocusSequenceNumberSetter {
 public:
  AutoFocusSequenceNumberSetter(FocusState& aFocusState, InputData& aEvent)
      : mFocusState(aFocusState), mEvent(aEvent), mMayChangeFocus(true) {}

  void MarkAsNonFocusChanging() { mMayChangeFocus = false; }

  ~AutoFocusSequenceNumberSetter() {
    if (mMayChangeFocus) {
      mFocusState.ReceiveFocusChangingEvent();

      APZ_KEY_LOG(
          "Marking input with type=%d as focus changing with seq=%" PRIu64 "\n",
          static_cast<int>(mEvent.mInputType),
          mFocusState.LastAPZProcessedEvent());
    } else {
      APZ_KEY_LOG(
          "Marking input with type=%d as non focus changing with seq=%" PRIu64
          "\n",
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

APZCTreeManager::APZCTreeManager(LayersId aRootLayersId,
                                 UniquePtr<IAPZHitTester> aHitTester)
    : mTestSampleTime(Nothing(), "APZCTreeManager::mTestSampleTime"),
      mInputQueue(new InputQueue()),
      mRootLayersId(aRootLayersId),
      mSampler(nullptr),
      mUpdater(nullptr),
      mTreeLock("APZCTreeLock"),
      mMapLock("APZCMapLock"),
      mRetainedTouchIdentifier(-1),
      mInScrollbarTouchDrag(false),
      mCurrentMousePosition(ScreenPoint(),
                            "APZCTreeManager::mCurrentMousePosition"),
      mApzcTreeLog("apzctree"),
      mTestDataLock("APZTestDataLock"),
      mDPI(160.0),
      mHitTester(std::move(aHitTester)) {
  RefPtr<APZCTreeManager> self(this);
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "layers::APZCTreeManager::APZCTreeManager",
      [self] { self->mFlushObserver = new CheckerboardFlushObserver(self); }));
  AsyncPanZoomController::InitializeGlobalState();
  mApzcTreeLog.ConditionOnPrefFunction(StaticPrefs::apz_printtree);

  if (!mHitTester) {
    mHitTester = MakeUnique<WRHitTester>();
  }
  mHitTester->Initialize(this);
}

APZCTreeManager::~APZCTreeManager() = default;

void APZCTreeManager::SetSampler(APZSampler* aSampler) {
  // We're either setting the sampler or clearing it
  MOZ_ASSERT((mSampler == nullptr) != (aSampler == nullptr));
  mSampler = aSampler;
}

void APZCTreeManager::SetUpdater(APZUpdater* aUpdater) {
  // We're either setting the updater or clearing it
  MOZ_ASSERT((mUpdater == nullptr) != (aUpdater == nullptr));
  mUpdater = aUpdater;
}

void APZCTreeManager::NotifyLayerTreeAdopted(
    LayersId aLayersId, const RefPtr<APZCTreeManager>& aOldApzcTreeManager) {
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
      adoptedData = std::move(it->second);
      aOldApzcTreeManager->mTestData.erase(it);
    }
  }
  if (adoptedData) {
    MutexAutoLock lock(mTestDataLock);
    mTestData[aLayersId] = std::move(adoptedData);
  }
}

void APZCTreeManager::NotifyLayerTreeRemoved(LayersId aLayersId) {
  AssertOnUpdaterThread();

  mFocusState.RemoveFocusTarget(aLayersId);

  {  // scope lock
    MutexAutoLock lock(mTestDataLock);
    mTestData.erase(aLayersId);
  }
}

AsyncPanZoomController* APZCTreeManager::NewAPZCInstance(
    LayersId aLayersId, GeckoContentController* aController) {
  return new AsyncPanZoomController(
      aLayersId, this, mInputQueue, aController,
      AsyncPanZoomController::USE_GESTURE_DETECTOR);
}

void APZCTreeManager::SetTestSampleTime(const Maybe<TimeStamp>& aTime) {
  auto testSampleTime = mTestSampleTime.Lock();
  testSampleTime.ref() = aTime;
}

SampleTime APZCTreeManager::GetFrameTime() {
  auto testSampleTime = mTestSampleTime.Lock();
  if (testSampleTime.ref()) {
    return SampleTime::FromTest(*testSampleTime.ref());
  }
  return SampleTime::FromNow();
}

void APZCTreeManager::SetAllowedTouchBehavior(
    uint64_t aInputBlockId, const nsTArray<TouchBehaviorFlags>& aValues) {
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(
        NewRunnableMethod<uint64_t,
                          StoreCopyPassByLRef<nsTArray<TouchBehaviorFlags>>>(
            "layers::APZCTreeManager::SetAllowedTouchBehavior", this,
            &APZCTreeManager::SetAllowedTouchBehavior, aInputBlockId,
            aValues.Clone()));
    return;
  }

  APZThreadUtils::AssertOnControllerThread();

  mInputQueue->SetAllowedTouchBehavior(aInputBlockId, aValues);
}

void APZCTreeManager::UpdateHitTestingTree(
    const WebRenderScrollDataWrapper& aRoot, bool aIsFirstPaint,
    LayersId aOriginatingLayersId, uint32_t aPaintSequenceNumber) {
  AssertOnUpdaterThread();

  RecursiveMutexAutoLock lock(mTreeLock);

  // For testing purposes, we log some data to the APZTestData associated with
  // the layers id that originated this update.
  APZTestData* testData = nullptr;
  if (StaticPrefs::apz_test_logging_enabled()) {
    MutexAutoLock lock(mTestDataLock);
    UniquePtr<APZTestData> ptr = MakeUnique<APZTestData>();
    auto result =
        mTestData.insert(std::make_pair(aOriginatingLayersId, std::move(ptr)));
    testData = result.first->second.get();
    testData->StartNewPaint(aPaintSequenceNumber);
  }

  TreeBuildingState state(mRootLayersId, aIsFirstPaint, aOriginatingLayersId,
                          testData, aPaintSequenceNumber);

  // We do this business with collecting the entire tree into an array because
  // otherwise it's very hard to determine which APZC instances need to be
  // destroyed. In the worst case, there are two scenarios: (a) a layer with an
  // APZC is removed from the layer tree and (b) a layer with an APZC is moved
  // in the layer tree from one place to a completely different place. In
  // scenario (a) we would want to destroy the APZC while walking the layer tree
  // and noticing that the layer/APZC is no longer there. But if we do that then
  // we run into a problem in scenario (b) because we might encounter that layer
  // later during the walk. To handle both of these we have to 'remember' that
  // the layer was not found, and then do the destroy only at the end of the
  // tree walk after we are sure that the layer was removed and not just
  // transplanted elsewhere. Doing that as part of a recursive tree walk is hard
  // and so maintaining a list and removing APZCs that are still alive is much
  // simpler.
  ForEachNode<ReverseIterator>(mRootNode.get(),
                               [&state](HitTestingTreeNode* aNode) {
                                 state.mNodesToDestroy.AppendElement(aNode);
                               });
  mRootNode = nullptr;
  mAsyncZoomContainerSubtree = Nothing();
  int asyncZoomContainerNestingDepth = 0;
  bool haveNestedAsyncZoomContainers = false;
  nsTArray<LayersId> subtreesWithRootContentOutsideAsyncZoomContainer;

  if (aRoot) {
    std::unordered_set<LayersId, LayersId::HashFn> seenLayersIds;
    std::stack<gfx::TreeAutoIndent<LOG_DEFAULT>> indents;
    std::stack<AncestorTransform> ancestorTransforms;
    HitTestingTreeNode* parent = nullptr;
    HitTestingTreeNode* next = nullptr;
    LayersId layersId = mRootLayersId;
    seenLayersIds.insert(mRootLayersId);
    ancestorTransforms.push(AncestorTransform());
    state.mOverrideFlags.push(EventRegionsOverride::NoOverride);
    nsTArray<Maybe<ZoomConstraints>> zoomConstraintsStack;

    // push a nothing to be used for anything outside an async zoom container
    zoomConstraintsStack.AppendElement(Nothing());

    mApzcTreeLog << "[start]\n";
    mTreeLock.AssertCurrentThreadIn();

    ForEachNode<ReverseIterator>(
        aRoot,
        [&](ScrollNode aLayerMetrics) {
          mApzcTreeLog << aLayerMetrics.Name() << '\t';

          if (auto asyncZoomContainerId =
                  aLayerMetrics.GetAsyncZoomContainerId()) {
            if (asyncZoomContainerNestingDepth > 0) {
              haveNestedAsyncZoomContainers = true;
            }
            mAsyncZoomContainerSubtree = Some(layersId);
            ++asyncZoomContainerNestingDepth;

            auto it = mZoomConstraints.find(
                ScrollableLayerGuid(layersId, 0, *asyncZoomContainerId));
            if (it != mZoomConstraints.end()) {
              zoomConstraintsStack.AppendElement(Some(it->second));
            } else {
              zoomConstraintsStack.AppendElement(Nothing());
            }
          }

          if (aLayerMetrics.Metrics().IsRootContent()) {
            MutexAutoLock lock(mMapLock);
            mGeckoFixedLayerMargins =
                aLayerMetrics.Metrics().GetFixedLayerMargins();
          } else {
            MOZ_ASSERT(aLayerMetrics.Metrics().GetFixedLayerMargins() ==
                           ScreenMargin(),
                       "fixed-layer-margins should be 0 on non-root layer");
          }

          // Note that this check happens after the potential increment of
          // asyncZoomContainerNestingDepth, to allow the root content
          // metadata to be on the same node as the async zoom container.
          if (aLayerMetrics.Metrics().IsRootContent() &&
              asyncZoomContainerNestingDepth == 0) {
            subtreesWithRootContentOutsideAsyncZoomContainer.AppendElement(
                layersId);
          }

          HitTestingTreeNode* node = PrepareNodeForLayer(
              lock, aLayerMetrics, aLayerMetrics.Metrics(), layersId,
              zoomConstraintsStack.LastElement(), ancestorTransforms.top(),
              parent, next, state);
          MOZ_ASSERT(node);
          AsyncPanZoomController* apzc = node->GetApzc();
          aLayerMetrics.SetApzc(apzc);

          // GetScrollbarAnimationId is only set when webrender is enabled,
          // which limits the extra thumb mapping work to the webrender-enabled
          // case where it is needed.
          // Note also that when webrender is enabled, a "valid" animation id
          // is always nonzero, so we don't need to worry about handling the
          // case where WR is enabled and the animation id is zero.
          if (node->GetScrollbarAnimationId()) {
            if (node->IsScrollThumbNode()) {
              state.mScrollThumbs.push_back(node);
            } else if (node->IsScrollbarContainerNode()) {
              // Only scrollbar containers for the root have an animation id.
              state.mRootScrollbarInfo.emplace_back(
                  *(node->GetScrollbarAnimationId()),
                  node->GetScrollbarDirection());
            }
          }

          // GetFixedPositionAnimationId is only set when webrender is enabled.
          if (node->GetFixedPositionAnimationId().isSome()) {
            state.mFixedPositionInfo.emplace_back(node);
          }
          // GetStickyPositionAnimationId is only set when webrender is enabled.
          if (node->GetStickyPositionAnimationId().isSome()) {
            state.mStickyPositionInfo.emplace_back(node);
          }
          if (apzc && node->IsPrimaryHolder()) {
            state.mScrollTargets[apzc->GetGuid()] = node;
          }

          mApzcTreeLog << '\n';

          // Accumulate the CSS transform between layers that have an APZC.
          // In the terminology of the big comment above
          // APZCTreeManager::GetScreenToApzcTransform, if we are at layer M,
          // then aAncestorTransform is NC * OC * PC, and we left-multiply MC
          // and compute ancestorTransform to be MC * NC * OC * PC. This gets
          // passed down as the ancestor transform to layer L when we recurse
          // into the children below. If we are at a layer with an APZC, such as
          // P, then we reset the ancestorTransform to just PC, to start the new
          // accumulation as we go down.
          AncestorTransform currentTransform{
              aLayerMetrics.GetTransform(),
              aLayerMetrics.TransformIsPerspective()};
          if (!apzc) {
            currentTransform = currentTransform * ancestorTransforms.top();
          }
          ancestorTransforms.push(currentTransform);

          // Note that |node| at this point will not have any children,
          // otherwise we we would have to set next to node->GetFirstChild().
          MOZ_ASSERT(!node->GetFirstChild());
          parent = node;
          next = nullptr;

          // Update the layersId if we have a new one
          if (Maybe<LayersId> newLayersId = aLayerMetrics.GetReferentId()) {
            layersId = *newLayersId;
            seenLayersIds.insert(layersId);

            // Propagate any event region override flags down into all
            // descendant nodes from the reflayer that has the flag. This is an
            // optimization to avoid having to walk up the tree to check the
            // override flags. Note that we don't keep the flags on the reflayer
            // itself, because the semantics of the flags are that they apply
            // to all content in the layer subtree being referenced. This
            // matters with the WR hit-test codepath, because this reflayer may
            // be just one of many nodes associated with a particular APZC, and
            // calling GetTargetNode with a guid may return any one of the
            // nodes. If different nodes have different flags on them that can
            // make the WR hit-test result incorrect, but being strict about
            // only putting the flags on descendant layers avoids this problem.
            state.mOverrideFlags.push(state.mOverrideFlags.top() |
                                      aLayerMetrics.GetEventRegionsOverride());
          }

          indents.push(gfx::TreeAutoIndent<LOG_DEFAULT>(mApzcTreeLog));
        },
        [&](ScrollNode aLayerMetrics) {
          if (aLayerMetrics.GetAsyncZoomContainerId()) {
            --asyncZoomContainerNestingDepth;
            zoomConstraintsStack.RemoveLastElement();
          }
          if (aLayerMetrics.GetReferentId()) {
            state.mOverrideFlags.pop();
          }

          next = parent;
          parent = parent->GetParent();
          layersId = next->GetLayersId();
          ancestorTransforms.pop();
          indents.pop();
        });

    mApzcTreeLog << "[end]\n";

    MOZ_ASSERT(
        !mAsyncZoomContainerSubtree ||
            !subtreesWithRootContentOutsideAsyncZoomContainer.Contains(
                *mAsyncZoomContainerSubtree),
        "If there is an async zoom container, all scroll nodes with root "
        "content scroll metadata should be inside it");
    MOZ_ASSERT(!haveNestedAsyncZoomContainers,
               "Should not have nested async zoom container");

    // If we have perspective transforms deferred to children, do another
    // walk of the tree and actually apply them to the children.
    // We can't do this "as we go" in the previous traversal, because by the
    // time we realize we need to defer a perspective transform for an APZC,
    // we may already have processed a previous layer (including children
    // found in its subtree) that shares that APZC.
    if (!state.mPerspectiveTransformsDeferredToChildren.empty()) {
      ForEachNode<ReverseIterator>(
          mRootNode.get(), [&state](HitTestingTreeNode* aNode) {
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

            auto it =
                state.mPerspectiveTransformsDeferredToChildren.find(parent);
            if (it != state.mPerspectiveTransformsDeferredToChildren.end()) {
              apzc->SetAncestorTransform(AncestorTransform{
                  it->second * apzc->GetAncestorTransform(), false});
            }
          });
    }

    // Remove any layers ids for which we no longer have content from
    // mDetachedLayersIds.
    for (auto iter = mDetachedLayersIds.begin();
         iter != mDetachedLayersIds.end();) {
      // unordered_set::erase() invalidates the iterator pointing to the
      // element being erased, but returns an iterator to the next element.
      if (seenLayersIds.find(*iter) == seenLayersIds.end()) {
        iter = mDetachedLayersIds.erase(iter);
      } else {
        ++iter;
      }
    }
  }

  // We do not support tree structures where the root node has siblings.
  MOZ_ASSERT(!(mRootNode && mRootNode->GetPrevSibling()));

  {  // scope lock and update our mApzcMap before we destroy all the unused
    // APZC instances
    MutexAutoLock lock(mMapLock);
    mApzcMap = std::move(state.mApzcMap);

    for (auto& mapping : mApzcMap) {
      AsyncPanZoomController* parent = mapping.second.apzc->GetParent();
      mapping.second.parent = parent ? Some(parent->GetGuid()) : Nothing();
    }

    mScrollThumbInfo.clear();
    // For non-webrender, state.mScrollThumbs will be empty so this will be a
    // no-op.
    for (HitTestingTreeNode* thumb : state.mScrollThumbs) {
      MOZ_ASSERT(thumb->IsScrollThumbNode());
      ScrollableLayerGuid targetGuid(thumb->GetLayersId(), 0,
                                     thumb->GetScrollTargetId());
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
          *(thumb->GetScrollbarAnimationId()), thumb->GetTransform(),
          thumb->GetScrollbarData(), targetGuid, target->GetTransform(),
          target->IsAncestorOf(thumb));
    }

    mRootScrollbarInfo = std::move(state.mRootScrollbarInfo);
    mFixedPositionInfo = std::move(state.mFixedPositionInfo);
    mStickyPositionInfo = std::move(state.mStickyPositionInfo);
  }

  for (size_t i = 0; i < state.mNodesToDestroy.Length(); i++) {
    APZCTM_LOG("Destroying node at %p with APZC %p\n",
               state.mNodesToDestroy[i].get(),
               state.mNodesToDestroy[i]->GetApzc());
    state.mNodesToDestroy[i]->Destroy();
  }

  APZCTM_LOG("APZCTreeManager (%p)\n", this);
  if (mRootNode && MOZ_LOG_TEST(sLog, LogLevel::Debug)) {
    mRootNode->Dump("  ");
  }
  SendSubtreeTransformsToChromeMainThread(nullptr);
}

void APZCTreeManager::UpdateFocusState(LayersId aRootLayerTreeId,
                                       LayersId aOriginatingLayersId,
                                       const FocusTarget& aFocusTarget) {
  AssertOnUpdaterThread();

  if (!StaticPrefs::apz_keyboard_enabled_AtStartup()) {
    return;
  }

  mFocusState.Update(aRootLayerTreeId, aOriginatingLayersId, aFocusTarget);
}

void APZCTreeManager::SampleForWebRender(const Maybe<VsyncId>& aVsyncId,
                                         wr::TransactionWrapper& aTxn,
                                         const SampleTime& aSampleTime) {
  AssertOnSamplerThread();
  MutexAutoLock lock(mMapLock);

  RefPtr<WebRenderBridgeParent> wrBridgeParent;
  RefPtr<CompositorController> controller;
  CompositorBridgeParent::CallWithIndirectShadowTree(
      mRootLayersId, [&](LayerTreeState& aState) -> void {
        controller = aState.GetCompositorController();
        wrBridgeParent = aState.mWrBridge;
      });

  bool activeAnimations = AdvanceAnimationsInternal(lock, aSampleTime);
  if (activeAnimations && controller) {
    controller->ScheduleRenderOnCompositorThread(
        wr::RenderReasons::ANIMATED_PROPERTY);
  }

  nsTArray<wr::WrTransformProperty> transforms;

  // Sample async transforms on scrollable layers.
  for (const auto& mapping : mApzcMap) {
    AsyncPanZoomController* apzc = mapping.second.apzc;

    const AsyncTransformComponents asyncTransformComponents =
        apzc->GetZoomAnimationId()
            ? AsyncTransformComponents{AsyncTransformComponent::eLayout}
            : LayoutAndVisual;
    ParentLayerPoint layerTranslation =
        apzc->GetCurrentAsyncTransformWithOverscroll(
                AsyncPanZoomController::eForCompositing,
                asyncTransformComponents)
            .TransformPoint(ParentLayerPoint(0, 0));

    if (Maybe<CompositionPayload> payload = apzc->NotifyScrollSampling()) {
      if (wrBridgeParent && aVsyncId) {
        wrBridgeParent->AddPendingScrollPayload(*payload, *aVsyncId);
      }
    }

    if (StaticPrefs::apz_test_logging_enabled()) {
      MutexAutoLock lock(mTestDataLock);

      ScrollableLayerGuid guid = apzc->GetGuid();
      auto it = mTestData.find(guid.mLayersId);
      if (it != mTestData.end()) {
        it->second->RecordSampledResult(
            apzc->GetCurrentAsyncScrollOffsetInCssPixels(
                AsyncPanZoomController::eForCompositing),
            (aSampleTime.Time() - TimeStamp::ProcessCreation(nullptr))
                .ToMicroseconds(),
            guid.mLayersId, guid.mScrollId);
      }
    }

    if (Maybe<uint64_t> zoomAnimationId = apzc->GetZoomAnimationId()) {
      // for now we only support zooming on root content APZCs
      MOZ_ASSERT(apzc->IsRootContent());

      LayoutDeviceToParentLayerScale zoom = apzc->GetCurrentPinchZoomScale(
          AsyncPanZoomController::eForCompositing);

      AsyncTransform asyncVisualTransform = apzc->GetCurrentAsyncTransform(
          AsyncPanZoomController::eForCompositing,
          AsyncTransformComponents{AsyncTransformComponent::eVisual});

      transforms.AppendElement(wr::ToWrTransformProperty(
          *zoomAnimationId, LayoutDeviceToParentLayerMatrix4x4::Scaling(
                                zoom.scale, zoom.scale, 1.0f) *
                                AsyncTransformComponentMatrix::Translation(
                                    asyncVisualTransform.mTranslation) *
                                apzc->GetOverscrollTransform(
                                    AsyncPanZoomController::eForCompositing)));

      aTxn.UpdateIsTransformAsyncZooming(*zoomAnimationId,
                                         apzc->IsAsyncZooming());
    }

    // If layerTranslation includes only the layout component of the async
    // transform then it has not been scaled by the async zoom, so we want to
    // divide it by the resolution. If layerTranslation includes the visual
    // component, then we should use the pinch zoom scale, which includes the
    // async zoom. However, we only use LayoutAndVisual for non-zoomable APZCs,
    // so it makes no difference.
    LayoutDeviceToParentLayerScale resolution =
        apzc->GetCumulativeResolution() * LayerToParentLayerScale(1.0f);
    // The positive translation means the painted content is supposed to
    // move down (or to the right), and that corresponds to a reduction in
    // the scroll offset. Since we are effectively giving WR the async
    // scroll delta here, we want to negate the translation.
    LayoutDevicePoint asyncScrollDelta = -layerTranslation / resolution;
    aTxn.UpdateScrollPosition(wr::AsPipelineId(apzc->GetGuid().mLayersId),
                              apzc->GetGuid().mScrollId,
                              wr::ToLayoutPoint(asyncScrollDelta));

#if defined(MOZ_WIDGET_ANDROID)
    // Send the root frame metrics to java through the UIController
    RefPtr<UiCompositorControllerParent> uiController =
        UiCompositorControllerParent::GetFromRootLayerTreeId(mRootLayersId);
    if (uiController &&
        apzc->UpdateRootFrameMetricsIfChanged(mLastRootMetrics)) {
      uiController->NotifyUpdateScreenMetrics(mLastRootMetrics);
    }
#endif
  }

  // Now collect all the async transforms needed for the scrollthumbs.
  for (const ScrollThumbInfo& info : mScrollThumbInfo) {
    auto it = mApzcMap.find(info.mTargetGuid);
    if (it == mApzcMap.end()) {
      // It could be that |info| is a scrollthumb for content which didn't
      // have an APZC, for example if the content isn't layerized. Regardless,
      // we can't async-scroll it so we don't need to worry about putting it
      // in mScrollThumbInfo.
      continue;
    }
    AsyncPanZoomController* scrollTargetApzc = it->second.apzc;
    MOZ_ASSERT(scrollTargetApzc);
    LayerToParentLayerMatrix4x4 transform =
        scrollTargetApzc->CallWithLastContentPaintMetrics(
            [&](const FrameMetrics& aMetrics) {
              return ComputeTransformForScrollThumb(
                  info.mThumbTransform * AsyncTransformMatrix(),
                  info.mTargetTransform.ToUnknownMatrix(), scrollTargetApzc,
                  aMetrics, info.mThumbData, info.mTargetIsAncestor);
            });
    transforms.AppendElement(
        wr::ToWrTransformProperty(info.mThumbAnimationId, transform));
  }

  // Move the root scrollbar in response to the dynamic toolbar transition.
  for (const RootScrollbarInfo& info : mRootScrollbarInfo) {
    // We only care about the horizontal scrollbar.
    if (info.mScrollDirection == ScrollDirection::eHorizontal) {
      ScreenPoint translation =
          apz::ComputeFixedMarginsOffset(GetCompositorFixedLayerMargins(lock),
                                         SideBits::eBottom, ScreenMargin());

      LayerToParentLayerMatrix4x4 transform =
          LayerToParentLayerMatrix4x4::Translation(ViewAs<ParentLayerPixel>(
              translation, PixelCastJustification::ScreenIsParentLayerForRoot));

      transforms.AppendElement(
          wr::ToWrTransformProperty(info.mScrollbarAnimationId, transform));
    }
  }

  for (const FixedPositionInfo& info : mFixedPositionInfo) {
    MOZ_ASSERT(info.mFixedPositionAnimationId.isSome());
    if (!IsFixedToRootContent(info, lock)) {
      continue;
    }

    ScreenPoint translation = apz::ComputeFixedMarginsOffset(
        GetCompositorFixedLayerMargins(lock), info.mFixedPosSides,
        mGeckoFixedLayerMargins);

    LayerToParentLayerMatrix4x4 transform =
        LayerToParentLayerMatrix4x4::Translation(ViewAs<ParentLayerPixel>(
            translation, PixelCastJustification::ScreenIsParentLayerForRoot));

    transforms.AppendElement(
        wr::ToWrTransformProperty(*info.mFixedPositionAnimationId, transform));
  }

  for (const StickyPositionInfo& info : mStickyPositionInfo) {
    MOZ_ASSERT(info.mStickyPositionAnimationId.isSome());
    SideBits sides = SidesStuckToRootContent(info, lock);
    if (sides == SideBits::eNone) {
      continue;
    }

    ScreenPoint translation = apz::ComputeFixedMarginsOffset(
        GetCompositorFixedLayerMargins(lock), sides,
        // For sticky layers, we don't need to factor
        // mGeckoFixedLayerMargins because Gecko doesn't shift the
        // position of sticky elements for dynamic toolbar movements.
        ScreenMargin());

    LayerToParentLayerMatrix4x4 transform =
        LayerToParentLayerMatrix4x4::Translation(ViewAs<ParentLayerPixel>(
            translation, PixelCastJustification::ScreenIsParentLayerForRoot));

    transforms.AppendElement(
        wr::ToWrTransformProperty(*info.mStickyPositionAnimationId, transform));
  }

  aTxn.AppendTransformProperties(transforms);
}

ParentLayerRect APZCTreeManager::ComputeClippedCompositionBounds(
    const MutexAutoLock& aProofOfMapLock, ClippedCompositionBoundsMap& aDestMap,
    ScrollableLayerGuid aGuid) {
  if (auto iter = aDestMap.find(aGuid); iter != aDestMap.end()) {
    // We already computed it for this one, early-exit. This might happen
    // because on a later iteration of mApzcMap we might encounter an ancestor
    // of an APZC that we processed on an earlier iteration. In this case we
    // would have computed the ancestor's clipped composition bounds when
    // recursing up on the earlier iteration.
    return iter->second;
  }

  ParentLayerRect bounds = mApzcMap[aGuid].apzc->GetCompositionBounds();
  const auto& mapEntry = mApzcMap.find(aGuid);
  MOZ_ASSERT(mapEntry != mApzcMap.end());
  if (mapEntry->second.parent.isNothing()) {
    // Recursion base case, where the APZC with guid `aGuid` has no parent.
    // In this case, we don't need to clip `bounds` any further and can just
    // early exit.
    aDestMap.emplace(aGuid, bounds);
    return bounds;
  }

  ScrollableLayerGuid parentGuid = mapEntry->second.parent.value();
  auto parentBoundsEntry = aDestMap.find(parentGuid);
  // If aDestMap doesn't contain the parent entry yet, we recurse to compute
  // that one first.
  ParentLayerRect parentClippedBounds =
      (parentBoundsEntry == aDestMap.end())
          ? ComputeClippedCompositionBounds(aProofOfMapLock, aDestMap,
                                            parentGuid)
          : parentBoundsEntry->second;

  // The parent layer's async transform applies to the current layer to take
  // `bounds` into the same coordinate space as `parentClippedBounds`. However,
  // we're going to do the inverse operation and unapply this transform to
  // `parentClippedBounds` to bring it into the same coordinate space as
  // `bounds`.
  AsyncTransform appliesToLayer =
      mApzcMap[parentGuid].apzc->GetCurrentAsyncTransform(
          AsyncPanZoomController::eForCompositing);

  // Do the unapplication
  LayerRect parentClippedBoundsInParentLayerSpace =
      (parentClippedBounds - appliesToLayer.mTranslation) /
      appliesToLayer.mScale;

  // And then clip `bounds` by the parent's comp bounds in the current space.
  bounds = bounds.Intersect(
      ViewAs<ParentLayerPixel>(parentClippedBoundsInParentLayerSpace,
                               PixelCastJustification::MovingDownToChildren));

  // Done!
  aDestMap.emplace(aGuid, bounds);
  return bounds;
}

bool APZCTreeManager::AdvanceAnimationsInternal(
    const MutexAutoLock& aProofOfMapLock, const SampleTime& aSampleTime) {
  ClippedCompositionBoundsMap clippedCompBounds;
  bool activeAnimations = false;
  for (const auto& mapping : mApzcMap) {
    AsyncPanZoomController* apzc = mapping.second.apzc;
    // Note that this call is recursive, but it early-exits if called again
    // with the same guid. So this loop is still amortized O(n) with respect to
    // the number of APZCs.
    ParentLayerRect clippedBounds = ComputeClippedCompositionBounds(
        aProofOfMapLock, clippedCompBounds, mapping.first);

    apzc->ReportCheckerboard(aSampleTime, clippedBounds);
    activeAnimations |= apzc->AdvanceAnimations(aSampleTime);
  }
  return activeAnimations;
}

void APZCTreeManager::PrintAPZCInfo(const ScrollNode& aLayer,
                                    const AsyncPanZoomController* apzc) {
  const FrameMetrics& metrics = aLayer.Metrics();
  std::stringstream guidStr;
  guidStr << apzc->GetGuid();
  mApzcTreeLog << "APZC " << guidStr.str()
               << "\tcb=" << metrics.GetCompositionBounds()
               << "\tsr=" << metrics.GetScrollableRect()
               << (metrics.IsScrollInfoLayer() ? "\tscrollinfo" : "")
               << (apzc->HasScrollgrab() ? "\tscrollgrab" : "") << "\t"
               << aLayer.Metadata().GetContentDescription().get();
}

void APZCTreeManager::AttachNodeToTree(HitTestingTreeNode* aNode,
                                       HitTestingTreeNode* aParent,
                                       HitTestingTreeNode* aNextSibling) {
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

already_AddRefed<HitTestingTreeNode> APZCTreeManager::RecycleOrCreateNode(
    const RecursiveMutexAutoLock& aProofOfTreeLock, TreeBuildingState& aState,
    AsyncPanZoomController* aApzc, LayersId aLayersId) {
  // Find a node without an APZC and return it. Note that unless the layer tree
  // actually changes, this loop should generally do an early-return on the
  // first iteration, so it should be cheap in the common case.
  for (int32_t i = aState.mNodesToDestroy.Length() - 1; i >= 0; i--) {
    RefPtr<HitTestingTreeNode> node = aState.mNodesToDestroy[i];
    if (node->IsRecyclable(aProofOfTreeLock)) {
      aState.mNodesToDestroy.RemoveElementAt(i);
      node->RecycleWith(aProofOfTreeLock, aApzc, aLayersId);
      return node.forget();
    }
  }
  RefPtr<HitTestingTreeNode> node =
      new HitTestingTreeNode(aApzc, false, aLayersId);
  return node.forget();
}

void APZCTreeManager::StartScrollbarDrag(const ScrollableLayerGuid& aGuid,
                                         const AsyncDragMetrics& aDragMetrics) {
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(
        NewRunnableMethod<ScrollableLayerGuid, AsyncDragMetrics>(
            "layers::APZCTreeManager::StartScrollbarDrag", this,
            &APZCTreeManager::StartScrollbarDrag, aGuid, aDragMetrics));
    return;
  }

  APZThreadUtils::AssertOnControllerThread();

  RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (!apzc) {
    NotifyScrollbarDragRejected(aGuid);
    return;
  }

  uint64_t inputBlockId = aDragMetrics.mDragStartSequenceNumber;
  mInputQueue->ConfirmDragBlock(inputBlockId, apzc, aDragMetrics);
}

bool APZCTreeManager::StartAutoscroll(const ScrollableLayerGuid& aGuid,
                                      const ScreenPoint& aAnchorLocation) {
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

void APZCTreeManager::StopAutoscroll(const ScrollableLayerGuid& aGuid) {
  APZThreadUtils::AssertOnControllerThread();

  if (RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid)) {
    apzc->StopAutoscroll();
  }
}

void APZCTreeManager::NotifyScrollbarDragInitiated(
    uint64_t aDragBlockId, const ScrollableLayerGuid& aGuid,
    ScrollDirection aDirection) const {
  RefPtr<GeckoContentController> controller =
      GetContentController(aGuid.mLayersId);
  if (controller) {
    controller->NotifyAsyncScrollbarDragInitiated(aDragBlockId, aGuid.mScrollId,
                                                  aDirection);
  }
}

void APZCTreeManager::NotifyScrollbarDragRejected(
    const ScrollableLayerGuid& aGuid) const {
  RefPtr<GeckoContentController> controller =
      GetContentController(aGuid.mLayersId);
  if (controller) {
    controller->NotifyAsyncScrollbarDragRejected(aGuid.mScrollId);
  }
}

void APZCTreeManager::NotifyAutoscrollRejected(
    const ScrollableLayerGuid& aGuid) const {
  RefPtr<GeckoContentController> controller =
      GetContentController(aGuid.mLayersId);
  MOZ_ASSERT(controller);
  controller->NotifyAsyncAutoscrollRejected(aGuid.mScrollId);
}

void SetHitTestData(HitTestingTreeNode* aNode,
                    const WebRenderScrollDataWrapper& aLayer,
                    const EventRegionsOverride& aOverrideFlags) {
  aNode->SetHitTestData(aLayer.GetVisibleRegion(),
                        aLayer.GetRemoteDocumentSize(),
                        aLayer.GetTransformTyped(), aOverrideFlags,
                        aLayer.GetAsyncZoomContainerId());
}

HitTestingTreeNode* APZCTreeManager::PrepareNodeForLayer(
    const RecursiveMutexAutoLock& aProofOfTreeLock, const ScrollNode& aLayer,
    const FrameMetrics& aMetrics, LayersId aLayersId,
    const Maybe<ZoomConstraints>& aZoomConstraints,
    const AncestorTransform& aAncestorTransform, HitTestingTreeNode* aParent,
    HitTestingTreeNode* aNextSibling, TreeBuildingState& aState) {
  bool needsApzc = true;
  if (!aMetrics.IsScrollable()) {
    needsApzc = false;
  }

  // XXX: As a future optimization we can probably stick these things on the
  // TreeBuildingState, and update them as we change layers id during the
  // traversal
  RefPtr<GeckoContentController> geckoContentController;
  CompositorBridgeParent::CallWithIndirectShadowTree(
      aLayersId, [&](LayerTreeState& lts) -> void {
        geckoContentController = lts.mController;
      });

  if (!geckoContentController) {
    needsApzc = false;
  }

  if (Maybe<uint64_t> zoomAnimationId = aLayer.GetZoomAnimationId()) {
    aState.mZoomAnimationId = zoomAnimationId;
  }

  RefPtr<HitTestingTreeNode> node = nullptr;
  if (!needsApzc) {
    // Note: if layer properties must be propagated to nodes, RecvUpdate in
    // LayerTransactionParent.cpp must ensure that APZ will be notified
    // when those properties change.
    node = RecycleOrCreateNode(aProofOfTreeLock, aState, nullptr, aLayersId);
    AttachNodeToTree(node, aParent, aNextSibling);
    SetHitTestData(node, aLayer, aState.mOverrideFlags.top());
    node->SetScrollbarData(aLayer.GetScrollbarAnimationId(),
                           aLayer.GetScrollbarData());
    node->SetFixedPosData(aLayer.GetFixedPositionScrollContainerId(),
                          aLayer.GetFixedPositionSides(),
                          aLayer.GetFixedPositionAnimationId());
    node->SetStickyPosData(aLayer.GetStickyScrollContainerId(),
                           aLayer.GetStickyScrollRangeOuter(),
                           aLayer.GetStickyScrollRangeInner(),
                           aLayer.GetStickyPositionAnimationId());
    return node;
  }

  AsyncPanZoomController* apzc = nullptr;
  // If we get here, aLayer is a scrollable layer and somebody
  // has registered a GeckoContentController for it, so we need to ensure
  // it has an APZC instance to manage its scrolling.

  // aState.mApzcMap allows reusing the exact same APZC instance for different
  // layers with the same FrameMetrics data. This is needed because in some
  // cases content that is supposed to scroll together is split into multiple
  // layers because of e.g. non-scrolling content interleaved in z-index order.
  ScrollableLayerGuid guid(aLayersId, aMetrics.GetPresShellId(),
                           aMetrics.GetScrollId());
  auto insertResult = aState.mApzcMap.insert(std::make_pair(
      guid,
      ApzcMapData{static_cast<AsyncPanZoomController*>(nullptr), Nothing()}));
  if (!insertResult.second) {
    apzc = insertResult.first->second.apzc;
    PrintAPZCInfo(aLayer, apzc);
  }
  APZCTM_LOG("Found APZC %p for layer %p with identifiers %" PRIx64 " %" PRId64
             "\n",
             apzc, aLayer.GetLayer(), uint64_t(guid.mLayersId), guid.mScrollId);

  // If we haven't encountered a layer already with the same metrics, then we
  // need to do the full reuse-or-make-an-APZC algorithm, which is contained
  // inside the block below.
  if (apzc == nullptr) {
    apzc = aLayer.GetApzc();

    // If the content represented by the scrollable layer has changed (which may
    // be possible because of DLBI heuristics) then we don't want to keep using
    // the same old APZC for the new content. Also, when reparenting a tab into
    // a new window a layer might get moved to a different layer tree with a
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

    if (aMetrics.IsRootContent()) {
      apzc->SetZoomAnimationId(aState.mZoomAnimationId);
      aState.mZoomAnimationId = Nothing();
    }

    APZCTM_LOG(
        "Using APZC %p for layer %p with identifiers %" PRIx64 " %" PRId64 "\n",
        apzc, aLayer.GetLayer(), uint64_t(aLayersId), aMetrics.GetScrollId());

    apzc->NotifyLayersUpdated(aLayer.Metadata(), aState.mIsFirstPaint,
                              aLayersId == aState.mOriginatingLayersId);

    // Since this is the first time we are encountering an APZC with this guid,
    // the node holding it must be the primary holder. It may be newly-created
    // or not, depending on whether it went through the newApzc branch above.
    MOZ_ASSERT(node->IsPrimaryHolder() && node->GetApzc() &&
               node->GetApzc()->Matches(guid));

    SetHitTestData(node, aLayer, aState.mOverrideFlags.top());
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
                                        "parentScrollId",
                                        apzc->GetParent()->GetGuid().mScrollId);
      }
      if (aMetrics.IsRootContent()) {
        aState.mPaintLogger.LogTestData(aMetrics.GetScrollId(), "isRootContent",
                                        true);
      }
      // Note that the async scroll offset is in ParentLayer pixels
      aState.mPaintLogger.LogTestData(
          aMetrics.GetScrollId(), "asyncScrollOffset",
          apzc->GetCurrentAsyncScrollOffset(
              AsyncPanZoomController::eForHitTesting));
      aState.mPaintLogger.LogTestData(aMetrics.GetScrollId(),
                                      "hasAsyncKeyScrolled",
                                      apzc->TestHasAsyncKeyScrolled());
    }

    // We must update the zoom constraints even if the apzc isn't new because it
    // might have moved.
    if (node->IsPrimaryHolder()) {
      if (aZoomConstraints) {
        apzc->UpdateZoomConstraints(*aZoomConstraints);

#ifdef DEBUG
        auto it = mZoomConstraints.find(guid);
        if (it != mZoomConstraints.end()) {
          MOZ_ASSERT(it->second == *aZoomConstraints);
        }
      } else {
        // We'd like to assert these things (if the first doesn't hold then at
        // least the second) but neither are not true because xul root content
        // gets zoomable zoom constraints, but which is not zoomable because it
        // doesn't have a root scroll frame.
        // clang-format off
        // MOZ_ASSERT(mZoomConstraints.find(guid) == mZoomConstraints.end());
        // auto it = mZoomConstraints.find(guid);
        // if (it != mZoomConstraints.end()) {
        //   MOZ_ASSERT(!it->second.mAllowZoom && !it->second.mAllowDoubleTapZoom);
        // }
        // clang-format on
#endif
      }
    }

    // Add a guid -> APZC mapping for the newly created APZC.
    insertResult.first->second.apzc = apzc;
  } else {
    // We already built an APZC earlier in this tree walk, but we have another
    // layer now that will also be using that APZC. The hit-test region on the
    // APZC needs to be updated to deal with the new layer's hit region.

    node = RecycleOrCreateNode(aProofOfTreeLock, aState, apzc, aLayersId);
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
    if (!aAncestorTransform.CombinedTransform().FuzzyEqualsMultiplicative(
            apzc->GetAncestorTransform())) {
      typedef TreeBuildingState::DeferredTransformMap::value_type PairType;
      if (!aAncestorTransform.ContainsPerspectiveTransform() &&
          !apzc->AncestorTransformContainsPerspective()) {
        MOZ_ASSERT(false,
                   "Two layers that scroll together have different ancestor "
                   "transforms");
      } else if (!aAncestorTransform.ContainsPerspectiveTransform()) {
        aState.mPerspectiveTransformsDeferredToChildren.insert(
            PairType{apzc, apzc->GetAncestorTransformPerspective()});
        apzc->SetAncestorTransform(aAncestorTransform);
      } else {
        aState.mPerspectiveTransformsDeferredToChildren.insert(
            PairType{apzc, aAncestorTransform.GetPerspectiveTransform()});
      }
    }

    SetHitTestData(node, aLayer, aState.mOverrideFlags.top());
  }

  // Note: if layer properties must be propagated to nodes, RecvUpdate in
  // LayerTransactionParent.cpp must ensure that APZ will be notified
  // when those properties change.
  node->SetScrollbarData(aLayer.GetScrollbarAnimationId(),
                         aLayer.GetScrollbarData());
  node->SetFixedPosData(aLayer.GetFixedPositionScrollContainerId(),
                        aLayer.GetFixedPositionSides(),
                        aLayer.GetFixedPositionAnimationId());
  node->SetStickyPosData(aLayer.GetStickyScrollContainerId(),
                         aLayer.GetStickyScrollRangeOuter(),
                         aLayer.GetStickyScrollRangeInner(),
                         aLayer.GetStickyPositionAnimationId());
  return node;
}

template <typename PanGestureOrScrollWheelInput>
static bool WillHandleInput(const PanGestureOrScrollWheelInput& aPanInput) {
  if (!XRE_IsParentProcess() || !NS_IsMainThread()) {
    return true;
  }

  WidgetWheelEvent wheelEvent = aPanInput.ToWidgetEvent(nullptr);
  return APZInputBridge::ActionForWheelEvent(&wheelEvent).isSome();
}

/*static*/
void APZCTreeManager::FlushApzRepaints(LayersId aLayersId) {
  // Previously, paints were throttled and therefore this method was used to
  // ensure any pending paints were flushed. Now, paints are flushed
  // immediately, so it is safe to simply send a notification now.
  APZCTM_LOG("Flushing repaints for layers id 0x%" PRIx64 "\n",
             uint64_t(aLayersId));
  RefPtr<GeckoContentController> controller = GetContentController(aLayersId);
#ifndef MOZ_WIDGET_ANDROID
  // On Android, this code is run in production and may actually get a nullptr
  // controller here. On other platforms this code is test-only and should never
  // get a nullptr.
  MOZ_ASSERT(controller);
#endif
  if (controller) {
    controller->DispatchToRepaintThread(NewRunnableMethod(
        "layers::GeckoContentController::NotifyFlushComplete", controller,
        &GeckoContentController::NotifyFlushComplete));
  }
}

void APZCTreeManager::MarkAsDetached(LayersId aLayersId) {
  RecursiveMutexAutoLock lock(mTreeLock);
  mDetachedLayersIds.insert(aLayersId);
}

static bool HasNonLockModifier(Modifiers aModifiers) {
  return (aModifiers & (MODIFIER_ALT | MODIFIER_ALTGRAPH | MODIFIER_CONTROL |
                        MODIFIER_FN | MODIFIER_META | MODIFIER_SHIFT |
                        MODIFIER_SYMBOL | MODIFIER_OS)) != 0;
}

APZEventResult APZCTreeManager::ReceiveInputEvent(InputData& aEvent) {
  APZThreadUtils::AssertOnControllerThread();
  InputHandlingState state{aEvent};

  // Use a RAII class for updating the focus sequence number of this event
  AutoFocusSequenceNumberSetter focusSetter(mFocusState, aEvent);

  switch (aEvent.mInputType) {
    case MULTITOUCH_INPUT: {
      MultiTouchInput& touchInput = aEvent.AsMultiTouchInput();
      ProcessTouchInput(state, touchInput);
      break;
    }
    case MOUSE_INPUT: {
      MouseInput& mouseInput = aEvent.AsMouseInput();
      mouseInput.mHandledByAPZ = true;

      SetCurrentMousePosition(mouseInput.mOrigin);

      bool startsDrag = DragTracker::StartsDrag(mouseInput);
      if (startsDrag) {
        // If this is the start of a drag we need to unambiguously know if it's
        // going to land on a scrollbar or not. We can't apply an untransform
        // here without knowing that, so we need to ensure the untransform is
        // a no-op.
        FlushRepaintsToClearScreenToGeckoTransform();
      }

      state.mHit = GetTargetAPZC(mouseInput.mOrigin);
      bool hitScrollbar = (bool)state.mHit.mScrollbarNode;

      // When the mouse is outside the window we still want to handle dragging
      // but we won't find an APZC. Fallback to root APZC then.
      {  // scope lock
        RecursiveMutexAutoLock lock(mTreeLock);
        if (!state.mHit.mTargetApzc && mRootNode) {
          state.mHit.mTargetApzc = mRootNode->GetApzc();
        }
      }

      if (state.mHit.mTargetApzc) {
        if (StaticPrefs::apz_test_logging_enabled() &&
            mouseInput.mType == MouseInput::MOUSE_HITTEST) {
          ScrollableLayerGuid guid = state.mHit.mTargetApzc->GetGuid();

          MutexAutoLock lock(mTestDataLock);
          auto it = mTestData.find(guid.mLayersId);
          MOZ_ASSERT(it != mTestData.end());
          it->second->RecordHitResult(mouseInput.mOrigin, state.mHit.mHitResult,
                                      guid.mLayersId, guid.mScrollId);
        }

        TargetConfirmationFlags confFlags{state.mHit.mHitResult};
        state.mResult = mInputQueue->ReceiveInputEvent(state.mHit.mTargetApzc,
                                                       confFlags, mouseInput);

        // If we're starting an async scrollbar drag
        bool apzDragEnabled = StaticPrefs::apz_drag_enabled();
        if (apzDragEnabled && startsDrag && state.mHit.mScrollbarNode &&
            state.mHit.mScrollbarNode->IsScrollThumbNode() &&
            state.mHit.mScrollbarNode->GetScrollbarData()
                .mThumbIsAsyncDraggable) {
          SetupScrollbarDrag(mouseInput, state.mHit.mScrollbarNode,
                             state.mHit.mTargetApzc.get());
        }

        if (state.mResult.GetStatus() == nsEventStatus_eConsumeDoDefault) {
          // This input event is part of a drag block, so whether or not it is
          // directed at a scrollbar depends on whether the drag block started
          // on a scrollbar.
          hitScrollbar = mInputQueue->IsDragOnScrollbar(hitScrollbar);
        }

        if (!hitScrollbar) {
          // The input was not targeted at a scrollbar, so we untransform it
          // like we do for other content. Scrollbars are "special" because they
          // have special handling in AsyncCompositionManager when resolution is
          // applied. TODO: we should find a better way to deal with this.
          ScreenToParentLayerMatrix4x4 transformToApzc =
              GetScreenToApzcTransform(state.mHit.mTargetApzc);
          ParentLayerToScreenMatrix4x4 transformToGecko =
              GetApzcToGeckoTransform(state.mHit.mTargetApzc);
          ScreenToScreenMatrix4x4 outTransform =
              transformToApzc * transformToGecko;
          Maybe<ScreenPoint> untransformedRefPoint =
              UntransformBy(outTransform, mouseInput.mOrigin);
          if (untransformedRefPoint) {
            mouseInput.mOrigin = *untransformedRefPoint;
          }
        } else {
          // Likewise, if the input was targeted at a scrollbar, we don't want
          // to apply the callback transform in the main thread, so we remove
          // the scrollid from the guid. We need to keep the layersId intact so
          // that the response from the child process doesn't get discarded.
          state.mResult.mTargetGuid.mScrollId =
              ScrollableLayerGuid::NULL_SCROLL_ID;
        }
      }
      break;
    }
    case SCROLLWHEEL_INPUT: {
      FlushRepaintsToClearScreenToGeckoTransform();

      // Do this before early return for Fission hit testing.
      ScrollWheelInput& wheelInput = aEvent.AsScrollWheelInput();
      state.mHit = GetTargetAPZC(wheelInput.mOrigin);

      wheelInput.mHandledByAPZ = WillHandleInput(wheelInput);
      if (!wheelInput.mHandledByAPZ) {
        return state.Finish();
      }

      if (state.mHit.mTargetApzc) {
        MOZ_ASSERT(state.mHit.mHitResult != CompositorHitTestInvisibleToHit);

        if (wheelInput.mAPZAction == APZWheelAction::PinchZoom) {
          // The mousewheel may have hit a subframe, but we want to send the
          // pinch-zoom events to the root-content APZC.
          {
            RecursiveMutexAutoLock lock(mTreeLock);
            state.mHit.mTargetApzc = FindRootContentApzcForLayersId(
                state.mHit.mTargetApzc->GetLayersId());
          }
          if (state.mHit.mTargetApzc) {
            SynthesizePinchGestureFromMouseWheel(wheelInput,
                                                 state.mHit.mTargetApzc);
          }
          state.mResult.SetStatusAsConsumeNoDefault();
          return state.Finish();
        }

        MOZ_ASSERT(wheelInput.mAPZAction == APZWheelAction::Scroll);

        // For wheel events, the call to ReceiveInputEvent below may result in
        // scrolling, which changes the async transform. However, the event we
        // want to pass to gecko should be the pre-scroll event coordinates,
        // transformed into the gecko space. (pre-scroll because the mouse
        // cursor is stationary during wheel scrolling, unlike touchmove
        // events). Since we just flushed the pending repaints the transform to
        // gecko space should only consist of overscroll-cancelling transforms.
        ScreenToScreenMatrix4x4 transformToGecko =
            GetScreenToApzcTransform(state.mHit.mTargetApzc) *
            GetApzcToGeckoTransform(state.mHit.mTargetApzc);
        Maybe<ScreenPoint> untransformedOrigin =
            UntransformBy(transformToGecko, wheelInput.mOrigin);

        if (!untransformedOrigin) {
          return state.Finish();
        }

        state.mResult = mInputQueue->ReceiveInputEvent(
            state.mHit.mTargetApzc,
            TargetConfirmationFlags{state.mHit.mHitResult}, wheelInput);

        // Update the out-parameters so they are what the caller expects.
        wheelInput.mOrigin = *untransformedOrigin;
      }
      break;
    }
    case PANGESTURE_INPUT: {
      FlushRepaintsToClearScreenToGeckoTransform();

      // Do this before early return for Fission hit testing.
      PanGestureInput& panInput = aEvent.AsPanGestureInput();
      state.mHit = GetTargetAPZC(panInput.mPanStartPoint);

      panInput.mHandledByAPZ = WillHandleInput(panInput);
      if (!panInput.mHandledByAPZ) {
        if (mInputQueue->GetCurrentPanGestureBlock()) {
          if (state.mHit.mTargetApzc &&
              (panInput.mType == PanGestureInput::PANGESTURE_END ||
               panInput.mType == PanGestureInput::PANGESTURE_CANCELLED)) {
            // If we've already been processing a pan gesture in an APZC but
            // fall into this _if_ branch, which means this pan-end or
            // pan-cancelled event will not be proccessed in the APZC, send a
            // pan-interrupted event to stop any on-going work for the pan
            // gesture, otherwise we will get stuck at an intermidiate state
            // becasue we might no longer receive any events which will be
            // handled by the APZC.
            PanGestureInput panInterrupted(
                PanGestureInput::PANGESTURE_INTERRUPTED, panInput.mTime,
                panInput.mTimeStamp, panInput.mPanStartPoint,
                panInput.mPanDisplacement, panInput.modifiers);
            Unused << mInputQueue->ReceiveInputEvent(
                state.mHit.mTargetApzc,
                TargetConfirmationFlags{state.mHit.mHitResult}, panInterrupted);
          }
        }
        return state.Finish();
      }

      // If/when we enable support for pan inputs off-main-thread, we'll need
      // to duplicate this EventStateManager code or something. See the call to
      // GetUserPrefsForWheelEvent in APZInputBridge.cpp for why these fields
      // are stored separately.
      MOZ_ASSERT(NS_IsMainThread());
      WidgetWheelEvent wheelEvent = panInput.ToWidgetEvent(nullptr);
      EventStateManager::GetUserPrefsForWheelEvent(
          &wheelEvent, &panInput.mUserDeltaMultiplierX,
          &panInput.mUserDeltaMultiplierY);

      if (state.mHit.mTargetApzc) {
        MOZ_ASSERT(state.mHit.mHitResult != CompositorHitTestInvisibleToHit);

        // For pan gesture events, the call to ReceiveInputEvent below may
        // result in scrolling, which changes the async transform. However, the
        // event we want to pass to gecko should be the pre-scroll event
        // coordinates, transformed into the gecko space. (pre-scroll because
        // the mouse cursor is stationary during pan gesture scrolling, unlike
        // touchmove events). Since we just flushed the pending repaints the
        // transform to gecko space should only consist of overscroll-cancelling
        // transforms.
        ScreenToScreenMatrix4x4 transformToGecko =
            GetScreenToApzcTransform(state.mHit.mTargetApzc) *
            GetApzcToGeckoTransform(state.mHit.mTargetApzc);
        Maybe<ScreenPoint> untransformedStartPoint =
            UntransformBy(transformToGecko, panInput.mPanStartPoint);
        Maybe<ScreenPoint> untransformedDisplacement =
            UntransformVector(transformToGecko, panInput.mPanDisplacement,
                              panInput.mPanStartPoint);

        if (!untransformedStartPoint || !untransformedDisplacement) {
          return state.Finish();
        }

        state.mResult = mInputQueue->ReceiveInputEvent(
            state.mHit.mTargetApzc,
            TargetConfirmationFlags{state.mHit.mHitResult}, panInput);

        // Update the out-parameters so they are what the caller expects.
        panInput.mPanStartPoint = *untransformedStartPoint;
        panInput.mPanDisplacement = *untransformedDisplacement;

        panInput.mOverscrollBehaviorAllowsSwipe =
            state.mHit.mTargetApzc->OverscrollBehaviorAllowsSwipe();
      }
      break;
    }
    case PINCHGESTURE_INPUT: {
      PinchGestureInput& pinchInput = aEvent.AsPinchGestureInput();
      if (HasNonLockModifier(pinchInput.modifiers)) {
        APZCTM_LOG("Discarding pinch input due to modifiers 0x%x\n",
                   pinchInput.modifiers);
        return state.Finish();
      }

      state.mHit = GetTargetAPZC(pinchInput.mFocusPoint);

      // We always handle pinch gestures as pinch zooms.
      pinchInput.mHandledByAPZ = true;

      if (state.mHit.mTargetApzc) {
        MOZ_ASSERT(state.mHit.mHitResult != CompositorHitTestInvisibleToHit);

        if (!state.mHit.mTargetApzc->IsRootContent()) {
          state.mHit.mTargetApzc = FindZoomableApzc(state.mHit.mTargetApzc);
        }
      }

      if (state.mHit.mTargetApzc) {
        ScreenToScreenMatrix4x4 outTransform =
            GetScreenToApzcTransform(state.mHit.mTargetApzc) *
            GetApzcToGeckoTransform(state.mHit.mTargetApzc);
        Maybe<ScreenPoint> untransformedFocusPoint =
            UntransformBy(outTransform, pinchInput.mFocusPoint);

        if (!untransformedFocusPoint) {
          return state.Finish();
        }

        state.mResult = mInputQueue->ReceiveInputEvent(
            state.mHit.mTargetApzc,
            TargetConfirmationFlags{state.mHit.mHitResult}, pinchInput);

        // Update the out-parameters so they are what the caller expects.
        pinchInput.mFocusPoint = *untransformedFocusPoint;
      }
      break;
    }
    case TAPGESTURE_INPUT: {  // note: no one currently sends these
      TapGestureInput& tapInput = aEvent.AsTapGestureInput();
      state.mHit = GetTargetAPZC(tapInput.mPoint);

      if (state.mHit.mTargetApzc) {
        MOZ_ASSERT(state.mHit.mHitResult != CompositorHitTestInvisibleToHit);

        ScreenToScreenMatrix4x4 outTransform =
            GetScreenToApzcTransform(state.mHit.mTargetApzc) *
            GetApzcToGeckoTransform(state.mHit.mTargetApzc);
        Maybe<ScreenIntPoint> untransformedPoint =
            UntransformBy(outTransform, tapInput.mPoint);

        if (!untransformedPoint) {
          return state.Finish();
        }

        state.mResult = mInputQueue->ReceiveInputEvent(
            state.mHit.mTargetApzc,
            TargetConfirmationFlags{state.mHit.mHitResult}, tapInput);

        // Update the out-parameters so they are what the caller expects.
        tapInput.mPoint = *untransformedPoint;
      }
      break;
    }
    case KEYBOARD_INPUT: {
      // Disable async keyboard scrolling when accessibility.browsewithcaret is
      // enabled
      if (!StaticPrefs::apz_keyboard_enabled_AtStartup() ||
          StaticPrefs::accessibility_browsewithcaret()) {
        APZ_KEY_LOG("Skipping key input from invalid prefs\n");
        return state.Finish();
      }

      KeyboardInput& keyInput = aEvent.AsKeyboardInput();

      // Try and find a matching shortcut for this keyboard input
      Maybe<KeyboardShortcut> shortcut = mKeyboardMap.FindMatch(keyInput);

      if (!shortcut) {
        APZ_KEY_LOG("Skipping key input with no shortcut\n");

        // If we don't have a shortcut for this key event, then we can keep our
        // focus only if we know there are no key event listeners for this
        // target
        if (mFocusState.CanIgnoreKeyboardShortcutMisses()) {
          focusSetter.MarkAsNonFocusChanging();
        }
        return state.Finish();
      }

      // Check if this shortcut needs to be dispatched to content. Anything
      // matching this is assumed to be able to change focus.
      if (shortcut->mDispatchToContent) {
        APZ_KEY_LOG("Skipping key input with dispatch-to-content shortcut\n");
        return state.Finish();
      }

      // We know we have an action to execute on whatever is the current focus
      // target
      const KeyboardScrollAction& action = shortcut->mAction;

      // The current focus target depends on which direction the scroll is to
      // happen
      Maybe<ScrollableLayerGuid> targetGuid;
      switch (action.mType) {
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

      // If we don't have a scroll target then either we have a stale focus
      // target, the focused element has event listeners, or the focused element
      // doesn't have a layerized scroll frame. In any case we need to dispatch
      // to content.
      if (!targetGuid) {
        APZ_KEY_LOG("Skipping key input with no current focus target\n");
        return state.Finish();
      }

      RefPtr<AsyncPanZoomController> targetApzc =
          GetTargetAPZC(targetGuid->mLayersId, targetGuid->mScrollId);

      if (!targetApzc) {
        APZ_KEY_LOG("Skipping key input with focus target but no APZC\n");
        return state.Finish();
      }

      // Attach the keyboard scroll action to the input event for processing
      // by the input queue.
      keyInput.mAction = action;

      APZ_KEY_LOG("Dispatching key input with apzc=%p\n", targetApzc.get());

      // Dispatch the event to the input queue.
      state.mResult = mInputQueue->ReceiveInputEvent(
          targetApzc, TargetConfirmationFlags{true}, keyInput);

      // Any keyboard event that is dispatched to the input queue at this point
      // should have been consumed
      MOZ_ASSERT(state.mResult.GetStatus() == nsEventStatus_eConsumeDoDefault ||
                 state.mResult.GetStatus() == nsEventStatus_eConsumeNoDefault);

      keyInput.mHandledByAPZ = true;
      focusSetter.MarkAsNonFocusChanging();

      break;
    }
  }
  return state.Finish();
}

static TouchBehaviorFlags ConvertToTouchBehavior(
    const CompositorHitTestInfo& info) {
  TouchBehaviorFlags result = AllowedTouchBehavior::UNKNOWN;
  if (info == CompositorHitTestInvisibleToHit) {
    result = AllowedTouchBehavior::NONE;
  } else if (info.contains(CompositorHitTestFlags::eIrregularArea)) {
    // Note that eApzAwareListeners and eInactiveScrollframe are similar
    // to eIrregularArea in some respects, but are not relevant for the
    // purposes of this function, which deals specifically with touch-action.
    result = AllowedTouchBehavior::UNKNOWN;
  } else {
    result = AllowedTouchBehavior::VERTICAL_PAN |
             AllowedTouchBehavior::HORIZONTAL_PAN |
             AllowedTouchBehavior::PINCH_ZOOM |
             AllowedTouchBehavior::DOUBLE_TAP_ZOOM;
    if (info.contains(CompositorHitTestFlags::eTouchActionPanXDisabled)) {
      result &= ~AllowedTouchBehavior::HORIZONTAL_PAN;
    }
    if (info.contains(CompositorHitTestFlags::eTouchActionPanYDisabled)) {
      result &= ~AllowedTouchBehavior::VERTICAL_PAN;
    }
    if (info.contains(CompositorHitTestFlags::eTouchActionPinchZoomDisabled)) {
      result &= ~AllowedTouchBehavior::PINCH_ZOOM;
    }
    if (info.contains(
            CompositorHitTestFlags::eTouchActionDoubleTapZoomDisabled)) {
      result &= ~AllowedTouchBehavior::DOUBLE_TAP_ZOOM;
    }
  }
  return result;
}

APZCTreeManager::HitTestResult APZCTreeManager::GetTouchInputBlockAPZC(
    const MultiTouchInput& aEvent,
    nsTArray<TouchBehaviorFlags>* aOutTouchBehaviors) {
  HitTestResult hit;
  if (aEvent.mTouches.Length() == 0) {
    return hit;
  }

  FlushRepaintsToClearScreenToGeckoTransform();

  hit = GetTargetAPZC(aEvent.mTouches[0].mScreenPoint);
  // Don't set a layers id on multi-touch events.
  if (aEvent.mTouches.Length() != 1) {
    hit.mLayersId = LayersId{0};
  }

  if (aOutTouchBehaviors) {
    aOutTouchBehaviors->AppendElement(ConvertToTouchBehavior(hit.mHitResult));
  }
  for (size_t i = 1; i < aEvent.mTouches.Length(); i++) {
    HitTestResult hit2 = GetTargetAPZC(aEvent.mTouches[i].mScreenPoint);
    if (aOutTouchBehaviors) {
      aOutTouchBehaviors->AppendElement(
          ConvertToTouchBehavior(hit2.mHitResult));
    }
    hit.mTargetApzc = GetZoomableTarget(hit.mTargetApzc, hit2.mTargetApzc);
    APZCTM_LOG("Using APZC %p as the root APZC for multi-touch\n",
               hit.mTargetApzc.get());
    // A multi-touch gesture will not be a scrollbar drag, even if the
    // first touch point happened to hit a scrollbar.
    hit.mScrollbarNode.Clear();

    // XXX we should probably be combining the hit results from the different
    // touch points somehow, instead of just using the last one.
    hit.mHitResult = hit2.mHitResult;
  }

  return hit;
}

APZEventResult APZCTreeManager::InputHandlingState::Finish() {
  // The validity check here handles both the case where mHit was
  // never populated (because this event did not trigger a hit-test),
  // and the case where it was populated with an invalid LayersId
  // (which can happen e.g. for multi-touch events).
  if (mHit.mLayersId.IsValid()) {
    mEvent.mLayersId = mHit.mLayersId;
  }

  // If the event is over an overscroll gutter, do not dispatch it to Gecko.
  if (mHit.mHitOverscrollGutter) {
    mResult.SetStatusAsConsumeNoDefault();
  }

  return mResult;
}

void APZCTreeManager::ProcessTouchInput(InputHandlingState& aState,
                                        MultiTouchInput& aInput) {
  aInput.mHandledByAPZ = true;
  nsTArray<TouchBehaviorFlags> touchBehaviors;
  HitTestingTreeNodeAutoLock hitScrollbarNode;
  if (aInput.mType == MultiTouchInput::MULTITOUCH_START) {
    // If we are panned into overscroll and a second finger goes down,
    // ignore that second touch point completely. The touch-start for it is
    // dropped completely; subsequent touch events until the touch-end for it
    // will have this touch point filtered out.
    // (By contrast, if we're in overscroll but not panning, such as after
    // putting two fingers down during an overscroll animation, we process the
    // second touch and proceed to pinch.)
    if (mTouchBlockHitResult.mTargetApzc &&
        mTouchBlockHitResult.mTargetApzc->IsInPanningState() &&
        BuildOverscrollHandoffChain(mTouchBlockHitResult.mTargetApzc)
            ->HasOverscrolledApzc()) {
      if (mRetainedTouchIdentifier == -1) {
        mRetainedTouchIdentifier =
            mTouchBlockHitResult.mTargetApzc->GetLastTouchIdentifier();
      }

      aState.mResult.SetStatusAsConsumeNoDefault();
      return;
    }

    aState.mHit = GetTouchInputBlockAPZC(aInput, &touchBehaviors);
    // Repopulate mTouchBlockHitResult with the fields we care about.
    mTouchBlockHitResult = aState.mHit.CopyWithoutScrollbarNode();
    hitScrollbarNode = std::move(aState.mHit.mScrollbarNode);

    // Check if this event starts a scrollbar touch-drag. The conditions
    // checked are similar to the ones we check for MOUSE_INPUT starting
    // a scrollbar mouse-drag.
    mInScrollbarTouchDrag =
        StaticPrefs::apz_drag_enabled() &&
        StaticPrefs::apz_drag_touch_enabled() && hitScrollbarNode &&
        hitScrollbarNode->IsScrollThumbNode() &&
        hitScrollbarNode->GetScrollbarData().mThumbIsAsyncDraggable;

    MOZ_ASSERT(touchBehaviors.Length() == aInput.mTouches.Length());
    for (size_t i = 0; i < touchBehaviors.Length(); i++) {
      APZCTM_LOG("Touch point has allowed behaviours 0x%02x\n",
                 touchBehaviors[i]);
      if (touchBehaviors[i] == AllowedTouchBehavior::UNKNOWN) {
        // If there's any unknown items in the list, throw it out and we'll
        // wait for the main thread to send us a notification.
        touchBehaviors.Clear();
        break;
      }
    }
  } else if (mTouchBlockHitResult.mTargetApzc) {
    APZCTM_LOG("Re-using APZC %p as continuation of event block\n",
               mTouchBlockHitResult.mTargetApzc.get());
    aState.mHit = mTouchBlockHitResult.CopyWithoutScrollbarNode();
  }

  if (mInScrollbarTouchDrag) {
    aState.mResult = ProcessTouchInputForScrollbarDrag(
        aInput, hitScrollbarNode, mTouchBlockHitResult.mHitResult);
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
        aState.mResult.SetStatusAsConsumeNoDefault();
        return;
      }
    }

    if (mTouchBlockHitResult.mTargetApzc) {
      MOZ_ASSERT(mTouchBlockHitResult.mHitResult !=
                 CompositorHitTestInvisibleToHit);

      aState.mResult = mInputQueue->ReceiveInputEvent(
          mTouchBlockHitResult.mTargetApzc,
          TargetConfirmationFlags{mTouchBlockHitResult.mHitResult}, aInput,
          touchBehaviors.IsEmpty() ? Nothing()
                                   : Some(std::move(touchBehaviors)));

      // For computing the event to pass back to Gecko, use up-to-date
      // transforms (i.e. not anything cached in an input block). This ensures
      // that transformToApzc and transformToGecko are in sync.
      // Note: we are not using ConvertToGecko() here, because we don't
      // want to multiply transformToApzc and transformToGecko once
      // for each touch point.
      ScreenToParentLayerMatrix4x4 transformToApzc =
          GetScreenToApzcTransform(mTouchBlockHitResult.mTargetApzc);
      ParentLayerToScreenMatrix4x4 transformToGecko =
          GetApzcToGeckoTransform(mTouchBlockHitResult.mTargetApzc);
      ScreenToScreenMatrix4x4 outTransform = transformToApzc * transformToGecko;

      for (size_t i = 0; i < aInput.mTouches.Length(); i++) {
        SingleTouchData& touchData = aInput.mTouches[i];
        Maybe<ScreenIntPoint> untransformedScreenPoint =
            UntransformBy(outTransform, touchData.mScreenPoint);
        if (!untransformedScreenPoint) {
          aState.mResult.SetStatusAsIgnore();
          return;
        }
        touchData.mScreenPoint = *untransformedScreenPoint;
        if (mTouchBlockHitResult.mFixedPosSides != SideBits::eNone) {
          MutexAutoLock lock(mMapLock);
          touchData.mScreenPoint -= RoundedToInt(apz::ComputeFixedMarginsOffset(
              GetCompositorFixedLayerMargins(lock),
              mTouchBlockHitResult.mFixedPosSides, mGeckoFixedLayerMargins));
        }
      }
    }
  }

  mTouchCounter.Update(aInput);

  // If it's the end of the touch sequence then clear out variables so we
  // don't keep dangling references and leak things.
  if (mTouchCounter.GetActiveTouchCount() == 0) {
    mTouchBlockHitResult = HitTestResult();
    mRetainedTouchIdentifier = -1;
    mInScrollbarTouchDrag = false;
  }
}

static MouseInput::MouseType MultiTouchTypeToMouseType(
    MultiTouchInput::MultiTouchType aType) {
  switch (aType) {
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

APZEventResult APZCTreeManager::ProcessTouchInputForScrollbarDrag(
    MultiTouchInput& aTouchInput,
    const HitTestingTreeNodeAutoLock& aScrollThumbNode,
    const gfx::CompositorHitTestInfo& aHitInfo) {
  MOZ_ASSERT(mRetainedTouchIdentifier == -1);
  MOZ_ASSERT(mTouchBlockHitResult.mTargetApzc);
  MOZ_ASSERT(aTouchInput.mTouches.Length() == 1);

  // Synthesize a mouse event based on the touch event, so that we can
  // reuse code in InputQueue and APZC for handling scrollbar mouse-drags.
  MouseInput mouseInput{MultiTouchTypeToMouseType(aTouchInput.mType),
                        MouseInput::PRIMARY_BUTTON,
                        dom::MouseEvent_Binding::MOZ_SOURCE_TOUCH,
                        MouseButtonsFlag::ePrimaryFlag,
                        aTouchInput.mTouches[0].mScreenPoint,
                        aTouchInput.mTime,
                        aTouchInput.mTimeStamp,
                        aTouchInput.modifiers};
  mouseInput.mHandledByAPZ = true;

  TargetConfirmationFlags targetConfirmed{aHitInfo};
  APZEventResult result;
  result = mInputQueue->ReceiveInputEvent(mTouchBlockHitResult.mTargetApzc,
                                          targetConfirmed, mouseInput);

  // |aScrollThumbNode| is non-null iff. this is the event that starts the drag.
  // If so, set up the drag.
  if (aScrollThumbNode) {
    SetupScrollbarDrag(mouseInput, aScrollThumbNode,
                       mTouchBlockHitResult.mTargetApzc.get());
  }

  // Since the input was targeted at a scrollbar:
  //    - The original touch event (which will be sent on to content) will
  //      not be untransformed.
  //    - We don't want to apply the callback transform in the main thread,
  //      so we remove the scrollid from the guid.
  // Both of these match the behaviour of mouse events that target a scrollbar;
  // see the code for handling mouse events in ReceiveInputEvent() for
  // additional explanation.
  result.mTargetGuid.mScrollId = ScrollableLayerGuid::NULL_SCROLL_ID;

  return result;
}

void APZCTreeManager::SetupScrollbarDrag(
    MouseInput& aMouseInput, const HitTestingTreeNodeAutoLock& aScrollThumbNode,
    AsyncPanZoomController* aApzc) {
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
  if (StaticPrefs::apz_drag_initial_enabled() &&
      // check that the scrollbar's target scroll frame is layerized
      aScrollThumbNode->GetScrollTargetId() == aApzc->GetGuid().mScrollId &&
      !aApzc->IsScrollInfoLayer()) {
    uint64_t dragBlockId = dragBlock->GetBlockId();
    // AsyncPanZoomController::HandleInputEvent() will call
    // TransformToLocal() on the event, but we need its mLocalOrigin now
    // to compute a drag start offset for the AsyncDragMetrics.
    aMouseInput.TransformToLocal(aApzc->GetTransformToThis());
    CSSCoord dragStart =
        aApzc->ConvertScrollbarPoint(aMouseInput.mLocalOrigin, thumbData);
    // ConvertScrollbarPoint() got the drag start offset relative to
    // the scroll track. Now get it relative to the thumb.
    // ScrollThumbData::mThumbStart stores the offset of the thumb
    // relative to the scroll track at the time of the last paint.
    // Since that paint, the thumb may have acquired an async transform
    // due to async scrolling, so look that up and apply it.
    LayerToParentLayerMatrix4x4 thumbTransform;
    {
      RecursiveMutexAutoLock lock(mTreeLock);
      thumbTransform = ComputeTransformForNode(aScrollThumbNode.Get(lock));
    }
    // Only consider the translation, since we do not support both
    // zooming and scrollbar dragging on any platform.
    CSSCoord thumbStart =
        thumbData.mThumbStart +
        ((*thumbData.mDirection == ScrollDirection::eHorizontal)
             ? thumbTransform._41
             : thumbTransform._42);
    dragStart -= thumbStart;

    // Content can't prevent scrollbar dragging with preventDefault(),
    // so we don't need to wait for a content response. It's important
    // to do this before calling ConfirmDragBlock() since that can
    // potentially process and consume the block.
    dragBlock->SetContentResponse(false);

    NotifyScrollbarDragInitiated(dragBlockId, aApzc->GetGuid(),
                                 *thumbData.mDirection);

    mInputQueue->ConfirmDragBlock(
        dragBlockId, aApzc,
        AsyncDragMetrics(aApzc->GetGuid().mScrollId,
                         aApzc->GetGuid().mPresShellId, dragBlockId, dragStart,
                         *thumbData.mDirection));
  }
}

void APZCTreeManager::SynthesizePinchGestureFromMouseWheel(
    const ScrollWheelInput& aWheelInput,
    const RefPtr<AsyncPanZoomController>& aTarget) {
  MOZ_ASSERT(aTarget);

  ScreenPoint focusPoint = aWheelInput.mOrigin;

  // Compute span values based on the wheel delta.
  ScreenCoord oldSpan = 100;
  ScreenCoord newSpan = oldSpan + aWheelInput.mDeltaY;

  // There's no ambiguity as to the target for pinch gesture events.
  TargetConfirmationFlags confFlags{true};

  PinchGestureInput pinchStart{PinchGestureInput::PINCHGESTURE_START,
                               PinchGestureInput::MOUSEWHEEL,
                               aWheelInput.mTime,
                               aWheelInput.mTimeStamp,
                               ExternalPoint(0, 0),
                               focusPoint,
                               oldSpan,
                               oldSpan,
                               aWheelInput.modifiers};
  PinchGestureInput pinchScale1{PinchGestureInput::PINCHGESTURE_SCALE,
                                PinchGestureInput::MOUSEWHEEL,
                                aWheelInput.mTime,
                                aWheelInput.mTimeStamp,
                                ExternalPoint(0, 0),
                                focusPoint,
                                oldSpan,
                                oldSpan,
                                aWheelInput.modifiers};
  PinchGestureInput pinchScale2{PinchGestureInput::PINCHGESTURE_SCALE,
                                PinchGestureInput::MOUSEWHEEL,
                                aWheelInput.mTime,
                                aWheelInput.mTimeStamp,
                                ExternalPoint(0, 0),
                                focusPoint,
                                oldSpan,
                                newSpan,
                                aWheelInput.modifiers};
  PinchGestureInput pinchEnd{PinchGestureInput::PINCHGESTURE_END,
                             PinchGestureInput::MOUSEWHEEL,
                             aWheelInput.mTime,
                             aWheelInput.mTimeStamp,
                             ExternalPoint(0, 0),
                             focusPoint,
                             newSpan,
                             newSpan,
                             aWheelInput.modifiers};

  mInputQueue->ReceiveInputEvent(aTarget, confFlags, pinchStart);
  mInputQueue->ReceiveInputEvent(aTarget, confFlags, pinchScale1);
  mInputQueue->ReceiveInputEvent(aTarget, confFlags, pinchScale2);
  mInputQueue->ReceiveInputEvent(aTarget, confFlags, pinchEnd);
}

void APZCTreeManager::UpdateWheelTransaction(
    LayoutDeviceIntPoint aRefPoint, EventMessage aEventMessage,
    const Maybe<ScrollableLayerGuid>& aTargetGuid) {
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
      ScreenIntPoint point = ViewAs<ScreenPixel>(
          aRefPoint,
          PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent);

      txn->OnMouseMove(point, aTargetGuid);

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

void APZCTreeManager::ProcessUnhandledEvent(LayoutDeviceIntPoint* aRefPoint,
                                            ScrollableLayerGuid* aOutTargetGuid,
                                            uint64_t* aOutFocusSequenceNumber,
                                            LayersId* aOutLayersId) {
  APZThreadUtils::AssertOnControllerThread();

  // Transform the aRefPoint.
  // If the event hits an overscrolled APZC, instruct the caller to ignore it.
  PixelCastJustification LDIsScreen =
      PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent;
  ScreenIntPoint refPointAsScreen = ViewAs<ScreenPixel>(*aRefPoint, LDIsScreen);
  HitTestResult hit = GetTargetAPZC(refPointAsScreen);
  if (aOutLayersId) {
    *aOutLayersId = hit.mLayersId;
  }
  if (hit.mTargetApzc) {
    MOZ_ASSERT(hit.mHitResult != CompositorHitTestInvisibleToHit);
    hit.mTargetApzc->GetGuid(aOutTargetGuid);
    ScreenToParentLayerMatrix4x4 transformToApzc =
        GetScreenToApzcTransform(hit.mTargetApzc);
    ParentLayerToScreenMatrix4x4 transformToGecko =
        GetApzcToGeckoTransform(hit.mTargetApzc);
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

void APZCTreeManager::SetKeyboardMap(const KeyboardMap& aKeyboardMap) {
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod<KeyboardMap>(
        "layers::APZCTreeManager::SetKeyboardMap", this,
        &APZCTreeManager::SetKeyboardMap, aKeyboardMap));
    return;
  }

  APZThreadUtils::AssertOnControllerThread();

  mKeyboardMap = aKeyboardMap;
}

void APZCTreeManager::ZoomToRect(const ScrollableLayerGuid& aGuid,
                                 const ZoomTarget& aZoomTarget,
                                 const uint32_t aFlags) {
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(
        NewRunnableMethod<ScrollableLayerGuid, ZoomTarget, uint32_t>(
            "layers::APZCTreeManager::ZoomToRect", this,
            &APZCTreeManager::ZoomToRect, aGuid, aZoomTarget, aFlags));
    return;
  }

  // We could probably move this to run on the updater thread if needed, but
  // either way we should restrict it to a single thread. For now let's use the
  // controller thread.
  APZThreadUtils::AssertOnControllerThread();

  RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aGuid);
  if (apzc) {
    apzc->ZoomToRect(aZoomTarget, aFlags);
  }
}

void APZCTreeManager::ContentReceivedInputBlock(uint64_t aInputBlockId,
                                                bool aPreventDefault) {
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod<uint64_t, bool>(
        "layers::APZCTreeManager::ContentReceivedInputBlock", this,
        &APZCTreeManager::ContentReceivedInputBlock, aInputBlockId,
        aPreventDefault));
    return;
  }

  APZThreadUtils::AssertOnControllerThread();

  mInputQueue->ContentReceivedInputBlock(aInputBlockId, aPreventDefault);
}

void APZCTreeManager::SetTargetAPZC(
    uint64_t aInputBlockId, const nsTArray<ScrollableLayerGuid>& aTargets) {
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(
        NewRunnableMethod<uint64_t,
                          StoreCopyPassByRRef<nsTArray<ScrollableLayerGuid>>>(
            "layers::APZCTreeManager::SetTargetAPZC", this,
            &layers::APZCTreeManager::SetTargetAPZC, aInputBlockId,
            aTargets.Clone()));
    return;
  }

  RefPtr<AsyncPanZoomController> target = nullptr;
  if (aTargets.Length() > 0) {
    target = GetTargetAPZC(aTargets[0]);
  }
  for (size_t i = 1; i < aTargets.Length(); i++) {
    RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aTargets[i]);
    target = GetZoomableTarget(target, apzc);
  }
  if (InputBlockState* block = mInputQueue->GetBlockForId(aInputBlockId)) {
    if (block->AsPinchGestureBlock() && aTargets.Length() == 1) {
      target = FindZoomableApzc(target);
    }
  }
  mInputQueue->SetConfirmedTargetApzc(aInputBlockId, target);
}

void APZCTreeManager::UpdateZoomConstraints(
    const ScrollableLayerGuid& aGuid,
    const Maybe<ZoomConstraints>& aConstraints) {
  if (!GetUpdater()->IsUpdaterThread()) {
    // This can happen if we're in the UI process and got a call directly from
    // nsBaseWidget or from a content process over PAPZCTreeManager. In that
    // case we get this call on the compositor thread, which may be different
    // from the updater thread. It can also happen in the GPU process if that is
    // enabled, since the call will go over PAPZCTreeManager and arrive on the
    // compositor thread in the GPU process.
    GetUpdater()->RunOnUpdaterThread(
        aGuid.mLayersId,
        NewRunnableMethod<ScrollableLayerGuid, Maybe<ZoomConstraints>>(
            "APZCTreeManager::UpdateZoomConstraints", this,
            &APZCTreeManager::UpdateZoomConstraints, aGuid, aConstraints));
    return;
  }

  AssertOnUpdaterThread();

  // Propagate the zoom constraints down to the subtree, stopping at APZCs
  // which have their own zoom constraints or are in a different layers id.
  if (aConstraints) {
    APZCTM_LOG("Recording constraints %s for guid %s\n",
               ToString(aConstraints.value()).c_str(), ToString(aGuid).c_str());
    mZoomConstraints[aGuid] = aConstraints.ref();
  } else {
    APZCTM_LOG("Removing constraints for guid %s\n", ToString(aGuid).c_str());
    mZoomConstraints.erase(aGuid);
  }

  RecursiveMutexAutoLock lock(mTreeLock);
  RefPtr<HitTestingTreeNode> node = DepthFirstSearchPostOrder<ReverseIterator>(
      mRootNode.get(), [&aGuid](HitTestingTreeNode* aNode) {
        bool matches = false;
        if (auto zoomId = aNode->GetAsyncZoomContainerId()) {
          matches = ScrollableLayerGuid::EqualsIgnoringPresShell(
              aGuid, ScrollableLayerGuid(aNode->GetLayersId(), 0, *zoomId));
        }
        return matches;
      });

  MOZ_ASSERT(!node ||
             !node->GetApzc());  // any node returned must not have an APZC

  // This does not hold because we can get zoom constraints updates before the
  // layer tree update with the async zoom container (I assume).
  // clang-format off
  // MOZ_ASSERT(node || aConstraints.isNothing() ||
  //           (!aConstraints->mAllowZoom && !aConstraints->mAllowDoubleTapZoom));
  // clang-format on

  // If there is no async zoom container then the zoom constraints should not
  // allow zooming and building the HTT should have handled clearing the zoom
  // constraints from all nodes so we don't have to handle doing anything in
  // case there is no async zoom container.

  if (node && aConstraints) {
    ForEachNode<ReverseIterator>(node.get(), [&aConstraints, &node, &aGuid,
                                              this](HitTestingTreeNode* aNode) {
      if (aNode != node) {
        // don't go into other async zoom containers
        if (auto zoomId = aNode->GetAsyncZoomContainerId()) {
          MOZ_ASSERT(!ScrollableLayerGuid::EqualsIgnoringPresShell(
              aGuid, ScrollableLayerGuid(aNode->GetLayersId(), 0, *zoomId)));
          return TraversalFlag::Skip;
        }
        if (AsyncPanZoomController* childApzc = aNode->GetApzc()) {
          if (!ScrollableLayerGuid::EqualsIgnoringPresShell(
                  aGuid, childApzc->GetGuid())) {
            // We can have subtrees with their own zoom constraints - leave
            // these alone.
            if (this->mZoomConstraints.find(childApzc->GetGuid()) !=
                this->mZoomConstraints.end()) {
              return TraversalFlag::Skip;
            }
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

void APZCTreeManager::FlushRepaintsToClearScreenToGeckoTransform() {
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

  ForEachNode<ReverseIterator>(mRootNode.get(), [](HitTestingTreeNode* aNode) {
    if (aNode->IsPrimaryHolder()) {
      MOZ_ASSERT(aNode->GetApzc());
      aNode->GetApzc()->FlushRepaintForNewInputBlock();
    }
  });
}

void APZCTreeManager::ClearTree() {
  AssertOnUpdaterThread();

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
                               [&nodesToDestroy](HitTestingTreeNode* aNode) {
                                 nodesToDestroy.AppendElement(aNode);
                               });

  for (size_t i = 0; i < nodesToDestroy.Length(); i++) {
    nodesToDestroy[i]->Destroy();
  }
  mRootNode = nullptr;

  {
    // Also remove references to APZC instances in the map
    MutexAutoLock lock(mMapLock);
    mApzcMap.clear();
  }

  RefPtr<APZCTreeManager> self(this);
  NS_DispatchToMainThread(
      NS_NewRunnableFunction("layers::APZCTreeManager::ClearTree", [self] {
        self->mFlushObserver->Unregister();
        self->mFlushObserver = nullptr;
      }));
}

RefPtr<HitTestingTreeNode> APZCTreeManager::GetRootNode() const {
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
 * @return true on success, false if aStartPoint or aEndPoint cannot be
 * transformed into target's coordinate space
 */
static bool TransformDisplacement(APZCTreeManager* aTreeManager,
                                  AsyncPanZoomController* aSource,
                                  AsyncPanZoomController* aTarget,
                                  ParentLayerPoint& aStartPoint,
                                  ParentLayerPoint& aEndPoint) {
  if (aSource == aTarget) {
    return true;
  }

  // Convert start and end points to Screen coordinates.
  ParentLayerToScreenMatrix4x4 untransformToApzc =
      aTreeManager->GetScreenToApzcTransform(aSource).Inverse();
  ScreenPoint screenStart = TransformBy(untransformToApzc, aStartPoint);
  ScreenPoint screenEnd = TransformBy(untransformToApzc, aEndPoint);

  // Convert start and end points to aTarget's ParentLayer coordinates.
  ScreenToParentLayerMatrix4x4 transformToApzc =
      aTreeManager->GetScreenToApzcTransform(aTarget);
  Maybe<ParentLayerPoint> startPoint =
      UntransformBy(transformToApzc, screenStart);
  Maybe<ParentLayerPoint> endPoint = UntransformBy(transformToApzc, screenEnd);
  if (!startPoint || !endPoint) {
    return false;
  }
  aEndPoint = *endPoint;
  aStartPoint = *startPoint;

  return true;
}

bool APZCTreeManager::DispatchScroll(
    AsyncPanZoomController* aPrev, ParentLayerPoint& aStartPoint,
    ParentLayerPoint& aEndPoint,
    OverscrollHandoffState& aOverscrollHandoffState) {
  const OverscrollHandoffChain& overscrollHandoffChain =
      aOverscrollHandoffState.mChain;
  uint32_t overscrollHandoffChainIndex = aOverscrollHandoffState.mChainIndex;
  RefPtr<AsyncPanZoomController> next;
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
  // |next|'s coordinate space.
  if (!TransformDisplacement(this, aPrev, next, aStartPoint, aEndPoint)) {
    return false;
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
    return false;
  }

  // Return true to indicate the scroll was consumed entirely.
  return true;
}

ParentLayerPoint APZCTreeManager::DispatchFling(
    AsyncPanZoomController* aPrev, const FlingHandoffState& aHandoffState) {
  // If immediate handoff is disallowed, do not allow handoff beyond the
  // single APZC that's scrolled by the input block that triggered this fling.
  if (aHandoffState.mIsHandoff && !StaticPrefs::apz_allow_immediate_handoff() &&
      aHandoffState.mScrolledApzc == aPrev) {
    FLING_LOG("APZCTM dropping handoff due to disallowed immediate handoff\n");
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

    RefPtr<AsyncPanZoomController> prevApzc =
        (startIndex > 0) ? chain->GetApzcAtIndex(startIndex - 1) : nullptr;

    // Only transform when current apzc can be transformed with previous
    if (prevApzc) {
      if (!TransformDisplacement(this, prevApzc, current, startPoint,
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
      residualVelocity += prevApzc->AdjustHandoffVelocityForOverscrollBehavior(
          transformedHandoffState.mVelocity);
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
    if (!FuzzyEqualsAdditive(availableVelocity.x, residualVelocity.x,
                             COORDINATE_EPSILON)) {
      finalResidualVelocity.x *= (residualVelocity.x / availableVelocity.x);
    }
    if (!FuzzyEqualsAdditive(availableVelocity.y, residualVelocity.y,
                             COORDINATE_EPSILON)) {
      finalResidualVelocity.y *= (residualVelocity.y / availableVelocity.y);
    }

    currentVelocity = residualVelocity;
  }

  // Return any residual velocity left over after the entire handoff process.
  return finalResidualVelocity;
}

already_AddRefed<AsyncPanZoomController> APZCTreeManager::GetTargetAPZC(
    const ScrollableLayerGuid& aGuid) {
  RecursiveMutexAutoLock lock(mTreeLock);
  RefPtr<HitTestingTreeNode> node = GetTargetNode(aGuid, nullptr);
  MOZ_ASSERT(!node || node->GetApzc());  // any node returned must have an APZC
  RefPtr<AsyncPanZoomController> apzc = node ? node->GetApzc() : nullptr;
  return apzc.forget();
}

already_AddRefed<AsyncPanZoomController> APZCTreeManager::GetTargetAPZC(
    const LayersId& aLayersId,
    const ScrollableLayerGuid::ViewID& aScrollId) const {
  MutexAutoLock lock(mMapLock);
  ScrollableLayerGuid guid(aLayersId, 0, aScrollId);
  auto it = mApzcMap.find(guid);
  RefPtr<AsyncPanZoomController> apzc =
      (it != mApzcMap.end() ? it->second.apzc : nullptr);
  return apzc.forget();
}

already_AddRefed<HitTestingTreeNode> APZCTreeManager::GetTargetNode(
    const ScrollableLayerGuid& aGuid, GuidComparator aComparator) const {
  mTreeLock.AssertCurrentThreadIn();
  RefPtr<HitTestingTreeNode> target =
      DepthFirstSearchPostOrder<ReverseIterator>(
          mRootNode.get(), [&aGuid, &aComparator](HitTestingTreeNode* node) {
            bool matches = false;
            if (node->GetApzc()) {
              if (aComparator) {
                matches = aComparator(aGuid, node->GetApzc()->GetGuid());
              } else {
                matches = node->GetApzc()->Matches(aGuid);
              }
            }
            return matches;
          });
  return target.forget();
}

APZCTreeManager::HitTestResult APZCTreeManager::GetTargetAPZC(
    const ScreenPoint& aPoint) {
  RecursiveMutexAutoLock lock(mTreeLock);
  MOZ_ASSERT(mHitTester);
  return mHitTester->GetAPZCAtPoint(aPoint, lock);
}

AsyncPanZoomController* APZCTreeManager::FindHandoffParent(
    const AsyncPanZoomController* aApzc) {
  RefPtr<HitTestingTreeNode> node = GetTargetNode(aApzc->GetGuid(), nullptr);
  while (node) {
    if (auto* apzc = GetTargetApzcForNode(node->GetParent())) {
      // avoid infinite recursion in the overscroll handoff chain.
      if (apzc != aApzc) {
        return apzc;
      }
    }
    node = node->GetParent();
  }

  return nullptr;
}

RefPtr<const OverscrollHandoffChain>
APZCTreeManager::BuildOverscrollHandoffChain(
    const RefPtr<AsyncPanZoomController>& aInitialTarget) {
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

    if (apzc->GetScrollHandoffParentId() ==
        ScrollableLayerGuid::NULL_SCROLL_ID) {
      if (!apzc->IsRootForLayersId()) {
        // This probably indicates a bug or missed case in layout code
        NS_WARNING("Found a non-root APZ with no handoff parent");
      }

      // Find the parent APZC by using HitTestingTree (i.e. by using
      // GetTargetApzcForNode) so that we can properly find the parent APZC for
      // cases where cross-process iframes are inside position:fixed subtree.
      apzc = FindHandoffParent(apzc);
      continue;
    }

    // Guard against a possible infinite-loop condition. If we hit this, the
    // layout code that generates the handoff parents did something wrong.
    MOZ_ASSERT(apzc->GetScrollHandoffParentId() != apzc->GetGuid().mScrollId);
    RefPtr<AsyncPanZoomController> scrollParent = GetTargetAPZC(
        apzc->GetGuid().mLayersId, apzc->GetScrollHandoffParentId());
    apzc = scrollParent.get();
  }

  // Now adjust the chain to account for scroll grabbing. Sorting is a bit
  // of an overkill here, but scroll grabbing will likely be generalized
  // to scroll priorities, so we might as well do it this way.
  result->SortByScrollPriority();

  // Print the overscroll chain for debugging.
  for (uint32_t i = 0; i < result->Length(); ++i) {
    APZCTM_LOG("OverscrollHandoffChain[%d] = %p\n", i,
               result->GetApzcAtIndex(i).get());
  }

  return result;
}

void APZCTreeManager::SetLongTapEnabled(bool aLongTapEnabled) {
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod<bool>(
        "layers::APZCTreeManager::SetLongTapEnabled", this,
        &APZCTreeManager::SetLongTapEnabled, aLongTapEnabled));
    return;
  }

  APZThreadUtils::AssertOnControllerThread();
  GestureEventListener::SetLongTapEnabled(aLongTapEnabled);
}

void APZCTreeManager::AddInputBlockCallback(uint64_t aInputBlockId,
                                            InputBlockCallback&& aCallback) {
  APZThreadUtils::AssertOnControllerThread();
  mInputQueue->AddInputBlockCallback(aInputBlockId, std::move(aCallback));
}

void APZCTreeManager::FindScrollThumbNode(
    const AsyncDragMetrics& aDragMetrics, LayersId aLayersId,
    HitTestingTreeNodeAutoLock& aOutThumbNode) {
  if (!aDragMetrics.mDirection) {
    // The AsyncDragMetrics has not been initialized yet - there will be
    // no matching node, so don't bother searching the tree.
    return;
  }

  RecursiveMutexAutoLock lock(mTreeLock);

  RefPtr<HitTestingTreeNode> result = DepthFirstSearch<ReverseIterator>(
      mRootNode.get(), [&aDragMetrics, &aLayersId](HitTestingTreeNode* aNode) {
        return aNode->MatchesScrollDragMetrics(aDragMetrics, aLayersId);
      });
  if (result) {
    aOutThumbNode.Initialize(lock, result.forget(), mTreeLock);
  }
}

AsyncPanZoomController* APZCTreeManager::GetTargetApzcForNode(
    const HitTestingTreeNode* aNode) {
  for (const HitTestingTreeNode* n = aNode;
       n && n->GetLayersId() == aNode->GetLayersId(); n = n->GetParent()) {
    if (n->GetApzc()) {
      APZCTM_LOG("Found target %p using ancestor lookup\n", n->GetApzc());
      return n->GetApzc();
    }
    if (n->GetFixedPosTarget() != ScrollableLayerGuid::NULL_SCROLL_ID) {
      RefPtr<AsyncPanZoomController> fpTarget =
          GetTargetAPZC(n->GetLayersId(), n->GetFixedPosTarget());
      APZCTM_LOG("Found target APZC %p using fixed-pos lookup on %" PRIu64 "\n",
                 fpTarget.get(), n->GetFixedPosTarget());
      return fpTarget.get();
    }
  }
  return nullptr;
}

HitTestingTreeNode* APZCTreeManager::FindRootNodeForLayersId(
    LayersId aLayersId) const {
  mTreeLock.AssertCurrentThreadIn();

  HitTestingTreeNode* resultNode = BreadthFirstSearch<ReverseIterator>(
      mRootNode.get(), [aLayersId](HitTestingTreeNode* aNode) {
        AsyncPanZoomController* apzc = aNode->GetApzc();
        return apzc && apzc->GetLayersId() == aLayersId &&
               apzc->IsRootForLayersId();
      });
  return resultNode;
}

already_AddRefed<AsyncPanZoomController> APZCTreeManager::FindZoomableApzc(
    AsyncPanZoomController* aStart) const {
  return GetZoomableTarget(aStart, aStart);
}

ScreenMargin APZCTreeManager::GetGeckoFixedLayerMargins() const {
  RecursiveMutexAutoLock lock(mTreeLock);
  return mGeckoFixedLayerMargins;
}

ScreenMargin APZCTreeManager::GetCompositorFixedLayerMargins() const {
  RecursiveMutexAutoLock lock(mTreeLock);
  return mCompositorFixedLayerMargins;
}

AsyncPanZoomController* APZCTreeManager::FindRootContentApzcForLayersId(
    LayersId aLayersId) const {
  mTreeLock.AssertCurrentThreadIn();

  HitTestingTreeNode* resultNode = BreadthFirstSearch<ReverseIterator>(
      mRootNode.get(), [aLayersId](HitTestingTreeNode* aNode) {
        AsyncPanZoomController* apzc = aNode->GetApzc();
        return apzc && apzc->GetLayersId() == aLayersId &&
               apzc->IsRootContent();
      });
  return resultNode ? resultNode->GetApzc() : nullptr;
}

// clang-format off
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
// clang-format on

/*
 * See the long comment above for a detailed explanation of this function.
 */
ScreenToParentLayerMatrix4x4 APZCTreeManager::GetScreenToApzcTransform(
    const AsyncPanZoomController* aApzc) const {
  Matrix4x4 result;
  RecursiveMutexAutoLock lock(mTreeLock);

  // The comments below assume there is a chain of layers L..R with L and P
  // having APZC instances as explained in the comment above. This function is
  // called with aApzc at L, and the loop below performs one iteration, where
  // parent is at P. The comments explain what values are stored in the
  // variables at these two levels. All the comments use standard matrix
  // notation where the leftmost matrix in a multiplication is applied first.

  // ancestorUntransform is PC.Inverse() * OC.Inverse() * NC.Inverse() *
  // MC.Inverse()
  Matrix4x4 ancestorUntransform = aApzc->GetAncestorTransform().Inverse();

  // result is initialized to PC.Inverse() * OC.Inverse() * NC.Inverse() *
  // MC.Inverse()
  result = ancestorUntransform;

  for (AsyncPanZoomController* parent = aApzc->GetParent(); parent;
       parent = parent->GetParent()) {
    // ancestorUntransform is updated to RC.Inverse() * QC.Inverse() when parent
    // == P
    ancestorUntransform = parent->GetAncestorTransform().Inverse();
    // asyncUntransform is updated to PA.Inverse() when parent == P
    Matrix4x4 asyncUntransform = parent
                                     ->GetCurrentAsyncTransformWithOverscroll(
                                         AsyncPanZoomController::eForHitTesting)
                                     .Inverse()
                                     .ToUnknownMatrix();
    // untransformSinceLastApzc is RC.Inverse() * QC.Inverse() * PA.Inverse()
    Matrix4x4 untransformSinceLastApzc = ancestorUntransform * asyncUntransform;

    // result is RC.Inverse() * QC.Inverse() * PA.Inverse() * PC.Inverse() *
    // OC.Inverse() * NC.Inverse() * MC.Inverse()
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
ParentLayerToScreenMatrix4x4 APZCTreeManager::GetApzcToGeckoTransform(
    const AsyncPanZoomController* aApzc) const {
  Matrix4x4 result;
  RecursiveMutexAutoLock lock(mTreeLock);

  // The comments below assume there is a chain of layers L..R with L and P
  // having APZC instances as explained in the comment above. This function is
  // called with aApzc at L, and the loop below performs one iteration, where
  // parent is at P. The comments explain what values are stored in the
  // variables at these two levels. All the comments use standard matrix
  // notation where the leftmost matrix in a multiplication is applied first.

  // asyncUntransform is LA.Inverse()
  Matrix4x4 asyncUntransform = aApzc
                                   ->GetCurrentAsyncTransformWithOverscroll(
                                       AsyncPanZoomController::eForHitTesting)
                                   .Inverse()
                                   .ToUnknownMatrix();

  // aTransformToGeckoOut is initialized to LA.Inverse() * LD * MC * NC * OC *
  // PC
  result = asyncUntransform * aApzc->GetTransformToLastDispatchedPaint() *
           aApzc->GetAncestorTransform();

  for (AsyncPanZoomController* parent = aApzc->GetParent(); parent;
       parent = parent->GetParent()) {
    // aTransformToGeckoOut is LA.Inverse() * LD * MC * NC * OC * PC * PD * QC *
    // RC
    result = result * parent->GetTransformToLastDispatchedPaint() *
             parent->GetAncestorTransform();

    // The above value for result when parent == P matches the required output
    // as explained in the comment above this method. Note that any missing
    // terms are guaranteed to be identity transforms.
  }

  return ViewAs<ParentLayerToScreenMatrix4x4>(result);
}

ScreenPoint APZCTreeManager::GetCurrentMousePosition() const {
  auto pos = mCurrentMousePosition.Lock();
  return pos.ref();
}

void APZCTreeManager::SetCurrentMousePosition(const ScreenPoint& aNewPos) {
  auto pos = mCurrentMousePosition.Lock();
  pos.ref() = aNewPos;
}

static AsyncPanZoomController* GetApzcWithDifferentLayersIdByWalkingParents(
    AsyncPanZoomController* aApzc) {
  if (!aApzc) {
    return nullptr;
  }
  AsyncPanZoomController* parent = aApzc->GetParent();
  while (parent && (parent->GetLayersId() == aApzc->GetLayersId())) {
    parent = parent->GetParent();
  }
  return parent;
}

already_AddRefed<AsyncPanZoomController> APZCTreeManager::GetZoomableTarget(
    AsyncPanZoomController* aApzc1, AsyncPanZoomController* aApzc2) const {
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
    if (apzc) {
      return apzc.forget();
    }
  }

  // Otherwise, find the common ancestor (to reach a common layers id), and then
  // walk up the apzc tree until we find a root-content APZC.
  apzc = CommonAncestor(aApzc1, aApzc2);
  RefPtr<AsyncPanZoomController> zoomable;
  while (apzc && !zoomable) {
    zoomable = FindRootContentApzcForLayersId(apzc->GetLayersId());
    apzc = GetApzcWithDifferentLayersIdByWalkingParents(apzc);
  }

  return zoomable.forget();
}

Maybe<ScreenIntPoint> APZCTreeManager::ConvertToGecko(
    const ScreenIntPoint& aPoint, AsyncPanZoomController* aApzc) {
  RecursiveMutexAutoLock lock(mTreeLock);
  ScreenToScreenMatrix4x4 transformScreenToGecko =
      GetScreenToApzcTransform(aApzc) * GetApzcToGeckoTransform(aApzc);
  Maybe<ScreenIntPoint> geckoPoint =
      UntransformBy(transformScreenToGecko, aPoint);
  if (geckoPoint) {
    if (mTouchBlockHitResult.mFixedPosSides != SideBits::eNone) {
      MutexAutoLock mapLock(mMapLock);
      *geckoPoint -= RoundedToInt(apz::ComputeFixedMarginsOffset(
          GetCompositorFixedLayerMargins(mapLock),
          mTouchBlockHitResult.mFixedPosSides, mGeckoFixedLayerMargins));
    }
  }
  return geckoPoint;
}

already_AddRefed<AsyncPanZoomController> APZCTreeManager::CommonAncestor(
    AsyncPanZoomController* aApzc1, AsyncPanZoomController* aApzc2) const {
  mTreeLock.AssertCurrentThreadIn();
  RefPtr<AsyncPanZoomController> ancestor;

  // If either aApzc1 or aApzc2 is null, min(depth1, depth2) will be 0 and this
  // function will return null.

  // Calculate depth of the APZCs in the tree
  int depth1 = 0, depth2 = 0;
  for (AsyncPanZoomController* parent = aApzc1; parent;
       parent = parent->GetParent()) {
    depth1++;
  }
  for (AsyncPanZoomController* parent = aApzc2; parent;
       parent = parent->GetParent()) {
    depth2++;
  }

  // At most one of the following two loops will be executed; the deeper APZC
  // pointer will get walked up to the depth of the shallower one.
  int minDepth = depth1 < depth2 ? depth1 : depth2;
  while (depth1 > minDepth) {
    depth1--;
    aApzc1 = aApzc1->GetParent();
  }
  while (depth2 > minDepth) {
    depth2--;
    aApzc2 = aApzc2->GetParent();
  }

  // Walk up the ancestor chains of both APZCs, always staying at the same depth
  // for either APZC, and return the the first common ancestor encountered.
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

bool APZCTreeManager::IsFixedToRootContent(
    const HitTestingTreeNode* aNode) const {
  MutexAutoLock lock(mMapLock);
  return IsFixedToRootContent(FixedPositionInfo(aNode), lock);
}

bool APZCTreeManager::IsFixedToRootContent(
    const FixedPositionInfo& aFixedInfo,
    const MutexAutoLock& aProofOfMapLock) const {
  ScrollableLayerGuid::ViewID fixedTarget = aFixedInfo.mFixedPosTarget;
  if (fixedTarget == ScrollableLayerGuid::NULL_SCROLL_ID) {
    return false;
  }
  auto it =
      mApzcMap.find(ScrollableLayerGuid(aFixedInfo.mLayersId, 0, fixedTarget));
  if (it == mApzcMap.end()) {
    return false;
  }
  RefPtr<AsyncPanZoomController> targetApzc = it->second.apzc;
  return targetApzc && targetApzc->IsRootContent();
}

SideBits APZCTreeManager::SidesStuckToRootContent(
    const HitTestingTreeNode* aNode) const {
  MutexAutoLock lock(mMapLock);
  return SidesStuckToRootContent(StickyPositionInfo(aNode), lock);
}

SideBits APZCTreeManager::SidesStuckToRootContent(
    const StickyPositionInfo& aStickyInfo,
    const MutexAutoLock& aProofOfMapLock) const {
  SideBits result = SideBits::eNone;

  ScrollableLayerGuid::ViewID stickyTarget = aStickyInfo.mStickyPosTarget;
  if (stickyTarget == ScrollableLayerGuid::NULL_SCROLL_ID) {
    return result;
  }

  // We support the dynamic toolbar at top and bottom.
  if ((aStickyInfo.mFixedPosSides & SideBits::eTopBottom) == SideBits::eNone) {
    return result;
  }

  auto it = mApzcMap.find(
      ScrollableLayerGuid(aStickyInfo.mLayersId, 0, stickyTarget));
  if (it == mApzcMap.end()) {
    return result;
  }
  RefPtr<AsyncPanZoomController> stickyTargetApzc = it->second.apzc;
  if (!stickyTargetApzc || !stickyTargetApzc->IsRootContent()) {
    return result;
  }

  ParentLayerPoint translation =
      stickyTargetApzc
          ->GetCurrentAsyncTransform(
              AsyncPanZoomController::eForHitTesting,
              AsyncTransformComponents{AsyncTransformComponent::eLayout})
          .mTranslation;

  if (apz::IsStuckAtTop(translation.y, aStickyInfo.mStickyScrollRangeInner,
                        aStickyInfo.mStickyScrollRangeOuter)) {
    result |= SideBits::eTop;
  }
  if (apz::IsStuckAtBottom(translation.y, aStickyInfo.mStickyScrollRangeInner,
                           aStickyInfo.mStickyScrollRangeOuter)) {
    result |= SideBits::eBottom;
  }
  return result;
}

LayerToParentLayerMatrix4x4 APZCTreeManager::ComputeTransformForNode(
    const HitTestingTreeNode* aNode) const {
  mTreeLock.AssertCurrentThreadIn();
  // The async transforms applied here for hit-testing purposes, are intended
  // to match the ones AsyncCompositionManager (or equivalent WebRender code)
  // applies for rendering purposes.
  // Note that with containerless scrolling, the layer structure looks like
  // this:
  //
  //   root container layer
  //     async zoom container layer
  //       scrollable content layers (with scroll metadata)
  //       fixed content layers (no scroll metadta, annotated isFixedPosition)
  //     scrollbar layers
  //
  // The intended async transforms in this case are:
  //  * On the async zoom container layer, the "visual" portion of the root
  //    content APZC's async transform (which includes the zoom, and async
  //    scrolling of the visual viewport relative to the layout viewport).
  //  * On the scrollable layers bearing the root content APZC's scroll
  //    metadata, the "layout" portion of the root content APZC's async
  //    transform (which includes async scrolling of the layout viewport
  //    relative to the scrollable rect origin).
  if (AsyncPanZoomController* apzc = aNode->GetApzc()) {
    // If the node represents scrollable content, apply the async transform
    // from its APZC.
    bool visualTransformIsInheritedFromAncestor =
        /* we're the APZC whose visual transform might be on the async
           zoom container */
        apzc->IsRootContent() &&
        /* there is an async zoom container on this subtree */
        mAsyncZoomContainerSubtree == Some(aNode->GetLayersId()) &&
        /* it's not us */
        !aNode->GetAsyncZoomContainerId();
    AsyncTransformComponents components =
        visualTransformIsInheritedFromAncestor
            ? AsyncTransformComponents{AsyncTransformComponent::eLayout}
            : LayoutAndVisual;
    return aNode->GetTransform() *
           CompleteAsyncTransform(apzc->GetCurrentAsyncTransformWithOverscroll(
               AsyncPanZoomController::eForHitTesting, components));
  } else if (aNode->GetAsyncZoomContainerId()) {
    if (AsyncPanZoomController* rootContent =
            FindRootContentApzcForLayersId(aNode->GetLayersId())) {
      return aNode->GetTransform() *
             CompleteAsyncTransform(
                 rootContent->GetCurrentAsyncTransformWithOverscroll(
                     AsyncPanZoomController::eForHitTesting,
                     {AsyncTransformComponent::eVisual}));
    }
  } else if (aNode->IsScrollThumbNode()) {
    // If the node represents a scrollbar thumb, compute and apply the
    // transformation that will be applied to the thumb in
    // AsyncCompositionManager.
    ScrollableLayerGuid guid{aNode->GetLayersId(), 0,
                             aNode->GetScrollTargetId()};
    if (RefPtr<HitTestingTreeNode> scrollTargetNode = GetTargetNode(
            guid, &ScrollableLayerGuid::EqualsIgnoringPresShell)) {
      AsyncPanZoomController* scrollTargetApzc = scrollTargetNode->GetApzc();
      MOZ_ASSERT(scrollTargetApzc);
      return scrollTargetApzc->CallWithLastContentPaintMetrics(
          [&](const FrameMetrics& aMetrics) {
            return ComputeTransformForScrollThumb(
                aNode->GetTransform() * AsyncTransformMatrix(),
                scrollTargetNode->GetTransform().ToUnknownMatrix(),
                scrollTargetApzc, aMetrics, aNode->GetScrollbarData(),
                scrollTargetNode->IsAncestorOf(aNode));
          });
    }
  } else if (IsFixedToRootContent(aNode)) {
    ParentLayerPoint translation;
    {
      MutexAutoLock mapLock(mMapLock);
      translation = ViewAs<ParentLayerPixel>(
          apz::ComputeFixedMarginsOffset(
              GetCompositorFixedLayerMargins(mapLock),
              aNode->GetFixedPosSides(), mGeckoFixedLayerMargins),
          PixelCastJustification::ScreenIsParentLayerForRoot);
    }
    return aNode->GetTransform() *
           CompleteAsyncTransform(
               AsyncTransformComponentMatrix::Translation(translation));
  }
  SideBits sides = SidesStuckToRootContent(aNode);
  if (sides != SideBits::eNone) {
    ParentLayerPoint translation;
    {
      MutexAutoLock mapLock(mMapLock);
      translation = ViewAs<ParentLayerPixel>(
          apz::ComputeFixedMarginsOffset(
              GetCompositorFixedLayerMargins(mapLock), sides,
              // For sticky layers, we don't need to factor
              // mGeckoFixedLayerMargins because Gecko doesn't shift the
              // position of sticky elements for dynamic toolbar movements.
              ScreenMargin()),
          PixelCastJustification::ScreenIsParentLayerForRoot);
    }
    return aNode->GetTransform() *
           CompleteAsyncTransform(
               AsyncTransformComponentMatrix::Translation(translation));
  }
  // Otherwise, the node does not have an async transform.
  return aNode->GetTransform() * AsyncTransformMatrix();
}

already_AddRefed<wr::WebRenderAPI> APZCTreeManager::GetWebRenderAPI() const {
  RefPtr<wr::WebRenderAPI> api;
  CompositorBridgeParent::CallWithIndirectShadowTree(
      mRootLayersId, [&](LayerTreeState& aState) -> void {
        if (aState.mWrBridge) {
          api = aState.mWrBridge->GetWebRenderAPI();
        }
      });
  return api.forget();
}

/*static*/
already_AddRefed<GeckoContentController> APZCTreeManager::GetContentController(
    LayersId aLayersId) {
  RefPtr<GeckoContentController> controller;
  CompositorBridgeParent::CallWithIndirectShadowTree(
      aLayersId,
      [&](LayerTreeState& aState) -> void { controller = aState.mController; });
  return controller.forget();
}

ScreenMargin APZCTreeManager::GetCompositorFixedLayerMargins(
    const MutexAutoLock& aProofOfMapLock) const {
  ScreenMargin result = mCompositorFixedLayerMargins;
  if (StaticPrefs::apz_fixed_margin_override_enabled()) {
    result.top = StaticPrefs::apz_fixed_margin_override_top();
    result.bottom = StaticPrefs::apz_fixed_margin_override_bottom();
  }
  return result;
}

bool APZCTreeManager::GetAPZTestData(LayersId aLayersId,
                                     APZTestData* aOutData) {
  AssertOnUpdaterThread();

  {  // copy the relevant test data into aOutData while holding the
     // mTestDataLock
    MutexAutoLock lock(mTestDataLock);
    auto it = mTestData.find(aLayersId);
    if (it == mTestData.end()) {
      return false;
    }
    *aOutData = *(it->second);
  }

  {  // add some additional "current state" into the returned APZTestData
    MutexAutoLock mapLock(mMapLock);

    ClippedCompositionBoundsMap clippedCompBounds;
    for (const auto& mapping : mApzcMap) {
      if (mapping.first.mLayersId != aLayersId) {
        continue;
      }

      ParentLayerRect clippedBounds = ComputeClippedCompositionBounds(
          mapLock, clippedCompBounds, mapping.first);
      AsyncPanZoomController* apzc = mapping.second.apzc;
      std::string viewId = std::to_string(mapping.first.mScrollId);
      std::string apzcState;
      if (apzc->GetCheckerboardMagnitude(clippedBounds)) {
        apzcState += "checkerboarding,";
      }
      aOutData->RecordAdditionalData(viewId, apzcState);
    }
  }
  return true;
}

void APZCTreeManager::SendSubtreeTransformsToChromeMainThread(
    const AsyncPanZoomController* aAncestor) {
  RefPtr<GeckoContentController> controller =
      GetContentController(mRootLayersId);
  if (!controller) {
    return;
  }
  nsTArray<MatrixMessage> messages;
  bool underAncestor = (aAncestor == nullptr);
  bool shouldNotify = false;
  {
    RecursiveMutexAutoLock lock(mTreeLock);
    if (!mRootNode) {
      // Event dispatched during shutdown, after ClearTree().
      // Note, mRootNode needs to be checked with mTreeLock held.
      return;
    }
    // This formulation duplicates matrix multiplications closer
    // to the root of the tree. For now, aiming for separation
    // of concerns rather than minimum number of multiplications.
    ForEachNode<ReverseIterator>(
        mRootNode.get(),
        [&](HitTestingTreeNode* aNode) {
          bool atAncestor = (aAncestor && aNode->GetApzc() == aAncestor);
          MOZ_ASSERT(!(underAncestor && atAncestor));
          underAncestor |= atAncestor;
          if (!underAncestor) {
            return;
          }
          LayersId layersId = aNode->GetLayersId();
          HitTestingTreeNode* parent = aNode->GetParent();
          if (!parent) {
            messages.AppendElement(MatrixMessage(Some(LayerToScreenMatrix4x4()),
                                                 ScreenRect(), layersId));
          } else if (layersId != parent->GetLayersId()) {
            if (mDetachedLayersIds.find(layersId) != mDetachedLayersIds.end()) {
              messages.AppendElement(
                  MatrixMessage(Nothing(), ScreenRect(), layersId));
            } else {
              messages.AppendElement(MatrixMessage(
                  Some(parent->GetTransformToGecko()),
                  parent->GetRemoteDocumentScreenRect(), layersId));
            }
          }
        },
        [&](HitTestingTreeNode* aNode) {
          bool atAncestor = (aAncestor && aNode->GetApzc() == aAncestor);
          if (atAncestor) {
            MOZ_ASSERT(underAncestor);
            underAncestor = false;
          }
        });
    if (messages != mLastMessages) {
      mLastMessages = messages;
      shouldNotify = true;
    }
  }
  if (shouldNotify) {
    controller->NotifyLayerTransforms(std::move(messages));
  }
}

void APZCTreeManager::SetFixedLayerMargins(ScreenIntCoord aTop,
                                           ScreenIntCoord aBottom) {
  MutexAutoLock lock(mMapLock);
  mCompositorFixedLayerMargins.top = aTop;
  mCompositorFixedLayerMargins.bottom = aBottom;
}

/*static*/
LayerToParentLayerMatrix4x4 APZCTreeManager::ComputeTransformForScrollThumb(
    const LayerToParentLayerMatrix4x4& aCurrentTransform,
    const Matrix4x4& aScrollableContentTransform, AsyncPanZoomController* aApzc,
    const FrameMetrics& aMetrics, const ScrollbarData& aScrollbarData,
    bool aScrollbarIsDescendant) {
  return apz::ComputeTransformForScrollThumb(
      aCurrentTransform, aScrollableContentTransform, aApzc, aMetrics,
      aScrollbarData, aScrollbarIsDescendant);
}

APZSampler* APZCTreeManager::GetSampler() const {
  // We should always have a sampler here, since in practice the sampler
  // is destroyed at the same time that this APZCTreeMAnager instance is.
  MOZ_ASSERT(mSampler);
  return mSampler;
}

void APZCTreeManager::AssertOnSamplerThread() {
  GetSampler()->AssertOnSamplerThread();
}

APZUpdater* APZCTreeManager::GetUpdater() const {
  // We should always have an updater here, since in practice the updater
  // is destroyed at the same time that this APZCTreeManager instance is.
  MOZ_ASSERT(mUpdater);
  return mUpdater;
}

void APZCTreeManager::AssertOnUpdaterThread() {
  GetUpdater()->AssertOnUpdaterThread();
}

void APZCTreeManager::LockTree() {
  AssertOnUpdaterThread();
  mTreeLock.Lock();
}

void APZCTreeManager::UnlockTree() {
  AssertOnUpdaterThread();
  mTreeLock.Unlock();
}

void APZCTreeManager::SetDPI(float aDpiValue) {
  if (!APZThreadUtils::IsControllerThread()) {
    APZThreadUtils::RunOnControllerThread(
        NewRunnableMethod<float>("layers::APZCTreeManager::SetDPI", this,
                                 &APZCTreeManager::SetDPI, aDpiValue));
    return;
  }

  APZThreadUtils::AssertOnControllerThread();
  mDPI = aDpiValue;
}

float APZCTreeManager::GetDPI() const {
  APZThreadUtils::AssertOnControllerThread();
  return mDPI;
}

APZCTreeManager::FixedPositionInfo::FixedPositionInfo(
    const HitTestingTreeNode* aNode) {
  mFixedPositionAnimationId = aNode->GetFixedPositionAnimationId();
  mFixedPosSides = aNode->GetFixedPosSides();
  mFixedPosTarget = aNode->GetFixedPosTarget();
  mLayersId = aNode->GetLayersId();
}

APZCTreeManager::StickyPositionInfo::StickyPositionInfo(
    const HitTestingTreeNode* aNode) {
  mStickyPositionAnimationId = aNode->GetStickyPositionAnimationId();
  mFixedPosSides = aNode->GetFixedPosSides();
  mStickyPosTarget = aNode->GetStickyPosTarget();
  mLayersId = aNode->GetLayersId();
  mStickyScrollRangeInner = aNode->GetStickyScrollRangeInner();
  mStickyScrollRangeOuter = aNode->GetStickyScrollRangeOuter();
}

}  // namespace layers
}  // namespace mozilla

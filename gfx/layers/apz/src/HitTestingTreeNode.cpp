/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HitTestingTreeNode.h"
#include <stack>

#include "AsyncPanZoomController.h"  // for AsyncPanZoomController
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/gfx/Point.h"        // for Point4D
#include "mozilla/layers/APZUtils.h"  // for AsyncTransform, CompleteAsyncTransform
#include "mozilla/layers/AsyncDragMetrics.h"  // for AsyncDragMetrics
#include "mozilla/ToString.h"                 // for ToString
#include "nsPrintfCString.h"                  // for nsPrintfCString
#include "UnitTransforms.h"                   // for ViewAs

static mozilla::LazyLogModule sApzMgrLog("apz.manager");

namespace mozilla {
namespace layers {

using gfx::CompositorHitTestInfo;

HitTestingTreeNode::HitTestingTreeNode(AsyncPanZoomController* aApzc,
                                       bool aIsPrimaryHolder,
                                       LayersId aLayersId)
    : mApzc(aApzc),
      mIsPrimaryApzcHolder(aIsPrimaryHolder),
      mLockCount(0),
      mLayersId(aLayersId),
      mFixedPosTarget(ScrollableLayerGuid::NULL_SCROLL_ID),
      mStickyPosTarget(ScrollableLayerGuid::NULL_SCROLL_ID),
      mOverride(EventRegionsOverride::NoOverride) {
  if (mIsPrimaryApzcHolder) {
    MOZ_ASSERT(mApzc);
  }
  MOZ_ASSERT(!mApzc || mApzc->GetLayersId() == mLayersId);
}

void HitTestingTreeNode::RecycleWith(
    const RecursiveMutexAutoLock& aProofOfTreeLock,
    AsyncPanZoomController* aApzc, LayersId aLayersId) {
  MOZ_ASSERT(IsRecyclable(aProofOfTreeLock));
  Destroy();  // clear out tree pointers
  mApzc = aApzc;
  mLayersId = aLayersId;
  MOZ_ASSERT(!mApzc || mApzc->GetLayersId() == mLayersId);
  // The caller is expected to call appropriate setters (SetHitTestData,
  // SetScrollbarData, SetFixedPosData, SetStickyPosData, etc.) to repopulate
  // all the data fields before using this node for "real work". Otherwise
  // those data fields may contain stale information from the previous use
  // of this node object.
}

HitTestingTreeNode::~HitTestingTreeNode() = default;

void HitTestingTreeNode::Destroy() {
  // This runs on the updater thread, it's not worth passing around extra raw
  // pointers just to assert it.

  mPrevSibling = nullptr;
  mLastChild = nullptr;
  mParent = nullptr;

  if (mApzc) {
    if (mIsPrimaryApzcHolder) {
      mApzc->Destroy();
    }
    mApzc = nullptr;
  }
}

bool HitTestingTreeNode::IsRecyclable(
    const RecursiveMutexAutoLock& aProofOfTreeLock) {
  return !(IsPrimaryHolder() || (mLockCount > 0));
}

void HitTestingTreeNode::SetLastChild(HitTestingTreeNode* aChild) {
  mLastChild = aChild;
  if (aChild) {
    aChild->mParent = this;

    if (aChild->GetApzc()) {
      AsyncPanZoomController* parent = GetNearestContainingApzc();
      // We assume that HitTestingTreeNodes with an ancestor/descendant
      // relationship cannot both point to the same APZC instance. This
      // assertion only covers a subset of cases in which that might occur,
      // but it's better than nothing.
      MOZ_ASSERT(aChild->GetApzc() != parent);
      aChild->SetApzcParent(parent);
    }
  }
}

void HitTestingTreeNode::SetScrollbarData(
    const Maybe<uint64_t>& aScrollbarAnimationId,
    const ScrollbarData& aScrollbarData) {
  mScrollbarAnimationId = aScrollbarAnimationId;
  mScrollbarData = aScrollbarData;
}

bool HitTestingTreeNode::MatchesScrollDragMetrics(
    const AsyncDragMetrics& aDragMetrics, LayersId aLayersId) const {
  return IsScrollThumbNode() && mLayersId == aLayersId &&
         mScrollbarData.mDirection == aDragMetrics.mDirection &&
         mScrollbarData.mTargetViewId == aDragMetrics.mViewId;
}

bool HitTestingTreeNode::IsScrollThumbNode() const {
  return mScrollbarData.mScrollbarLayerType ==
         layers::ScrollbarLayerType::Thumb;
}

bool HitTestingTreeNode::IsScrollbarNode() const {
  return mScrollbarData.mScrollbarLayerType != layers::ScrollbarLayerType::None;
}

bool HitTestingTreeNode::IsScrollbarContainerNode() const {
  return mScrollbarData.mScrollbarLayerType ==
         layers::ScrollbarLayerType::Container;
}

ScrollDirection HitTestingTreeNode::GetScrollbarDirection() const {
  MOZ_ASSERT(IsScrollbarNode());
  MOZ_ASSERT(mScrollbarData.mDirection.isSome());
  return *mScrollbarData.mDirection;
}

ScrollableLayerGuid::ViewID HitTestingTreeNode::GetScrollTargetId() const {
  return mScrollbarData.mTargetViewId;
}

Maybe<uint64_t> HitTestingTreeNode::GetScrollbarAnimationId() const {
  return mScrollbarAnimationId;
}

const ScrollbarData& HitTestingTreeNode::GetScrollbarData() const {
  return mScrollbarData;
}

void HitTestingTreeNode::SetFixedPosData(
    ScrollableLayerGuid::ViewID aFixedPosTarget, SideBits aFixedPosSides,
    const Maybe<uint64_t>& aFixedPositionAnimationId) {
  mFixedPosTarget = aFixedPosTarget;
  mFixedPosSides = aFixedPosSides;
  mFixedPositionAnimationId = aFixedPositionAnimationId;
}

ScrollableLayerGuid::ViewID HitTestingTreeNode::GetFixedPosTarget() const {
  return mFixedPosTarget;
}

SideBits HitTestingTreeNode::GetFixedPosSides() const { return mFixedPosSides; }

Maybe<uint64_t> HitTestingTreeNode::GetFixedPositionAnimationId() const {
  return mFixedPositionAnimationId;
}

void HitTestingTreeNode::SetPrevSibling(HitTestingTreeNode* aSibling) {
  mPrevSibling = aSibling;
  if (aSibling) {
    aSibling->mParent = mParent;

    if (aSibling->GetApzc()) {
      AsyncPanZoomController* parent =
          mParent ? mParent->GetNearestContainingApzc() : nullptr;
      aSibling->SetApzcParent(parent);
    }
  }
}

void HitTestingTreeNode::SetStickyPosData(
    ScrollableLayerGuid::ViewID aStickyPosTarget,
    const LayerRectAbsolute& aScrollRangeOuter,
    const LayerRectAbsolute& aScrollRangeInner,
    const Maybe<uint64_t>& aStickyPositionAnimationId) {
  mStickyPosTarget = aStickyPosTarget;
  mStickyScrollRangeOuter = aScrollRangeOuter;
  mStickyScrollRangeInner = aScrollRangeInner;
  mStickyPositionAnimationId = aStickyPositionAnimationId;
}

ScrollableLayerGuid::ViewID HitTestingTreeNode::GetStickyPosTarget() const {
  return mStickyPosTarget;
}

const LayerRectAbsolute& HitTestingTreeNode::GetStickyScrollRangeOuter() const {
  return mStickyScrollRangeOuter;
}

const LayerRectAbsolute& HitTestingTreeNode::GetStickyScrollRangeInner() const {
  return mStickyScrollRangeInner;
}

Maybe<uint64_t> HitTestingTreeNode::GetStickyPositionAnimationId() const {
  return mStickyPositionAnimationId;
}

void HitTestingTreeNode::MakeRoot() {
  mParent = nullptr;

  if (GetApzc()) {
    SetApzcParent(nullptr);
  }
}

HitTestingTreeNode* HitTestingTreeNode::GetFirstChild() const {
  HitTestingTreeNode* child = GetLastChild();
  while (child && child->GetPrevSibling()) {
    child = child->GetPrevSibling();
  }
  return child;
}

HitTestingTreeNode* HitTestingTreeNode::GetLastChild() const {
  return mLastChild;
}

HitTestingTreeNode* HitTestingTreeNode::GetPrevSibling() const {
  return mPrevSibling;
}

HitTestingTreeNode* HitTestingTreeNode::GetParent() const { return mParent; }

bool HitTestingTreeNode::IsAncestorOf(const HitTestingTreeNode* aOther) const {
  for (const HitTestingTreeNode* cur = aOther; cur; cur = cur->GetParent()) {
    if (cur == this) {
      return true;
    }
  }
  return false;
}

AsyncPanZoomController* HitTestingTreeNode::GetApzc() const { return mApzc; }

AsyncPanZoomController* HitTestingTreeNode::GetNearestContainingApzc() const {
  for (const HitTestingTreeNode* n = this; n; n = n->GetParent()) {
    if (n->GetApzc()) {
      return n->GetApzc();
    }
  }
  return nullptr;
}

bool HitTestingTreeNode::IsPrimaryHolder() const {
  return mIsPrimaryApzcHolder;
}

LayersId HitTestingTreeNode::GetLayersId() const { return mLayersId; }

void HitTestingTreeNode::SetHitTestData(
    const LayerIntRegion& aVisibleRegion,
    const LayerIntSize& aRemoteDocumentSize,
    const CSSTransformMatrix& aTransform, const EventRegionsOverride& aOverride,
    const Maybe<ScrollableLayerGuid::ViewID>& aAsyncZoomContainerId) {
  mVisibleRegion = aVisibleRegion;
  mRemoteDocumentSize = aRemoteDocumentSize;
  mTransform = aTransform;
  mOverride = aOverride;
  mAsyncZoomContainerId = aAsyncZoomContainerId;
}

EventRegionsOverride HitTestingTreeNode::GetEventRegionsOverride() const {
  return mOverride;
}

const CSSTransformMatrix& HitTestingTreeNode::GetTransform() const {
  return mTransform;
}

LayerToScreenMatrix4x4 HitTestingTreeNode::GetTransformToGecko(
    LayersId aRemoteLayersId) const {
  if (mParent) {
    LayerToParentLayerMatrix4x4 thisToParent =
        mTransform * AsyncTransformMatrix();
    if (mApzc) {
      thisToParent =
          thisToParent * ViewAs<AsyncTransformComponentMatrix>(
                             mApzc->GetTransformToLastDispatchedPaint(
                                 LayoutAndVisual, aRemoteLayersId));
    }
    ParentLayerToScreenMatrix4x4 parentToRoot =
        ViewAs<ParentLayerToScreenMatrix4x4>(
            mParent->GetTransformToGecko(aRemoteLayersId),
            PixelCastJustification::MovingDownToChildren);
    return thisToParent * parentToRoot;
  }

  return ViewAs<LayerToScreenMatrix4x4>(
      mTransform * AsyncTransformMatrix(),
      PixelCastJustification::ScreenIsParentLayerForRoot);
}

const LayerIntRegion& HitTestingTreeNode::GetVisibleRegion() const {
  return mVisibleRegion;
}

ScreenRect HitTestingTreeNode::GetRemoteDocumentScreenRect(
    LayersId aRemoteDocumentLayersId) const {
  ScreenRect result = TransformBy(
      GetTransformToGecko(aRemoteDocumentLayersId),
      IntRectToRect(LayerIntRect(LayerIntPoint(), mRemoteDocumentSize)));

  for (const HitTestingTreeNode* node = this; node; node = node->GetParent()) {
    if (!node->GetApzc()) {
      continue;
    }

    ParentLayerRect compositionBounds = node->GetApzc()->GetCompositionBounds();
    if (compositionBounds.IsEmpty()) {
      return ScreenRect();
    }

    ScreenRect scrollPortOnScreenCoordinate = TransformBy(
        node->GetParent()
            ? node->GetParent()->GetTransformToGecko(node->GetLayersId())
            : LayerToScreenMatrix4x4(),
        ViewAs<LayerPixel>(compositionBounds,
                           PixelCastJustification::MovingDownToChildren));
    if (scrollPortOnScreenCoordinate.IsEmpty()) {
      return ScreenRect();
    }

    result = result.Intersect(scrollPortOnScreenCoordinate);
    if (result.IsEmpty()) {
      return ScreenRect();
    }
  }
  return result;
}

Maybe<ScrollableLayerGuid::ViewID> HitTestingTreeNode::GetAsyncZoomContainerId()
    const {
  return mAsyncZoomContainerId;
}

void HitTestingTreeNode::Dump(const char* aPrefix) const {
  MOZ_LOG(
      sApzMgrLog, LogLevel::Debug,
      ("%sHitTestingTreeNode (%p) APZC (%p) g=(%s) %s%s%s t=(%s) "
       "%s%s\n",
       aPrefix, this, mApzc.get(),
       mApzc ? ToString(mApzc->GetGuid()).c_str()
             : nsPrintfCString("l=0x%" PRIx64, uint64_t(mLayersId)).get(),
       (mOverride & EventRegionsOverride::ForceDispatchToContent) ? "fdtc "
                                                                  : "",
       (mOverride & EventRegionsOverride::ForceEmptyHitRegion) ? "fehr " : "",
       (mFixedPosTarget != ScrollableLayerGuid::NULL_SCROLL_ID)
           ? nsPrintfCString("fixed=%" PRIu64 " ", mFixedPosTarget).get()
           : "",
       ToString(mTransform).c_str(),
       mScrollbarData.mDirection.isSome() ? " scrollbar" : "",
       IsScrollThumbNode() ? " scrollthumb" : ""));

  if (!mLastChild) {
    return;
  }

  // Dump the children in order from first child to last child
  std::stack<HitTestingTreeNode*> children;
  for (HitTestingTreeNode* child = mLastChild.get(); child;
       child = child->mPrevSibling) {
    children.push(child);
  }
  nsPrintfCString childPrefix("%s  ", aPrefix);
  while (!children.empty()) {
    children.top()->Dump(childPrefix.get());
    children.pop();
  }
}

void HitTestingTreeNode::SetApzcParent(AsyncPanZoomController* aParent) {
  // precondition: GetApzc() is non-null
  MOZ_ASSERT(GetApzc() != nullptr);
  if (IsPrimaryHolder()) {
    GetApzc()->SetParent(aParent);
  } else {
    MOZ_ASSERT(GetApzc()->GetParent() == aParent);
  }
}

void HitTestingTreeNode::Lock(const RecursiveMutexAutoLock& aProofOfTreeLock) {
  mLockCount++;
}

void HitTestingTreeNode::Unlock(
    const RecursiveMutexAutoLock& aProofOfTreeLock) {
  MOZ_ASSERT(mLockCount > 0);
  mLockCount--;
}

HitTestingTreeNodeAutoLock::HitTestingTreeNodeAutoLock()
    : mTreeMutex(nullptr) {}

HitTestingTreeNodeAutoLock::~HitTestingTreeNodeAutoLock() { Clear(); }

void HitTestingTreeNodeAutoLock::Initialize(
    const RecursiveMutexAutoLock& aProofOfTreeLock,
    already_AddRefed<HitTestingTreeNode> aNode, RecursiveMutex& aTreeMutex) {
  MOZ_ASSERT(!mNode);

  mNode = aNode;
  mTreeMutex = &aTreeMutex;

  mNode->Lock(aProofOfTreeLock);
}

void HitTestingTreeNodeAutoLock::Clear() {
  if (!mNode) {
    return;
  }
  MOZ_ASSERT(mTreeMutex);

  {  // scope lock
    RecursiveMutexAutoLock lock(*mTreeMutex);
    mNode->Unlock(lock);
  }
  mNode = nullptr;
  mTreeMutex = nullptr;
}

}  // namespace layers
}  // namespace mozilla

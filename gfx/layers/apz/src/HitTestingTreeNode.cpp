/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HitTestingTreeNode.h"

#include "AsyncPanZoomController.h"                     // for AsyncPanZoomController
#include "gfxPrefs.h"
#include "LayersLogging.h"                              // for Stringify
#include "mozilla/gfx/Point.h"                          // for Point4D
#include "mozilla/layers/APZUtils.h"                    // for CompleteAsyncTransform
#include "mozilla/layers/AsyncCompositionManager.h"     // for ViewTransform::operator Matrix4x4()
#include "mozilla/layers/AsyncDragMetrics.h"            // for AsyncDragMetrics
#include "nsPrintfCString.h"                            // for nsPrintfCString
#include "UnitTransforms.h"                             // for ViewAs

namespace mozilla {
namespace layers {

using gfx::CompositorHitTestInfo;

HitTestingTreeNode::HitTestingTreeNode(AsyncPanZoomController* aApzc,
                                       bool aIsPrimaryHolder,
                                       LayersId aLayersId)
  : mApzc(aApzc)
  , mIsPrimaryApzcHolder(aIsPrimaryHolder)
  , mLockCount(0)
  , mLayersId(aLayersId)
  , mScrollbarAnimationId(0)
  , mFixedPosTarget(FrameMetrics::NULL_SCROLL_ID)
  , mIsBackfaceHidden(false)
  , mOverride(EventRegionsOverride::NoOverride)
{
  if (mIsPrimaryApzcHolder) {
    MOZ_ASSERT(mApzc);
  }
  MOZ_ASSERT(!mApzc || mApzc->GetLayersId() == mLayersId);
}

void
HitTestingTreeNode::RecycleWith(const RecursiveMutexAutoLock& aProofOfTreeLock,
                                AsyncPanZoomController* aApzc,
                                LayersId aLayersId)
{
  MOZ_ASSERT(IsRecyclable(aProofOfTreeLock));
  Destroy(); // clear out tree pointers
  mApzc = aApzc;
  mLayersId = aLayersId;
  MOZ_ASSERT(!mApzc || mApzc->GetLayersId() == mLayersId);
  // The caller is expected to call SetHitTestData to repopulate the hit-test
  // fields.
}

HitTestingTreeNode::~HitTestingTreeNode() = default;

void
HitTestingTreeNode::Destroy()
{
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

bool
HitTestingTreeNode::IsRecyclable(const RecursiveMutexAutoLock& aProofOfTreeLock)
{
  return !(IsPrimaryHolder() || (mLockCount > 0));
}

void
HitTestingTreeNode::SetLastChild(HitTestingTreeNode* aChild)
{
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

void
HitTestingTreeNode::SetScrollbarData(const uint64_t& aScrollbarAnimationId,
                                     const ScrollbarData& aScrollbarData)
{
  mScrollbarAnimationId = aScrollbarAnimationId;
  mScrollbarData = aScrollbarData;
}

bool
HitTestingTreeNode::MatchesScrollDragMetrics(const AsyncDragMetrics& aDragMetrics) const
{
  return IsScrollThumbNode() &&
         mScrollbarData.mDirection == aDragMetrics.mDirection &&
         mScrollbarData.mTargetViewId == aDragMetrics.mViewId;
}

bool
HitTestingTreeNode::IsScrollThumbNode() const
{
  return mScrollbarData.mScrollbarLayerType == layers::ScrollbarLayerType::Thumb;
}

bool
HitTestingTreeNode::IsScrollbarNode() const
{
  return mScrollbarData.mScrollbarLayerType != layers::ScrollbarLayerType::None;
}

ScrollDirection
HitTestingTreeNode::GetScrollbarDirection() const
{
  MOZ_ASSERT(IsScrollbarNode());
  MOZ_ASSERT(mScrollbarData.mDirection.isSome());
  return *mScrollbarData.mDirection;
}

FrameMetrics::ViewID
HitTestingTreeNode::GetScrollTargetId() const
{
  return mScrollbarData.mTargetViewId;
}

const uint64_t&
HitTestingTreeNode::GetScrollbarAnimationId() const
{
  return mScrollbarAnimationId;
}

const ScrollbarData&
HitTestingTreeNode::GetScrollbarData() const
{
  return mScrollbarData;
}

void
HitTestingTreeNode::SetFixedPosData(FrameMetrics::ViewID aFixedPosTarget)
{
  mFixedPosTarget = aFixedPosTarget;
}

FrameMetrics::ViewID
HitTestingTreeNode::GetFixedPosTarget() const
{
  return mFixedPosTarget;
}

void
HitTestingTreeNode::SetPrevSibling(HitTestingTreeNode* aSibling)
{
  mPrevSibling = aSibling;
  if (aSibling) {
    aSibling->mParent = mParent;

    if (aSibling->GetApzc()) {
      AsyncPanZoomController* parent = mParent ? mParent->GetNearestContainingApzc() : nullptr;
      aSibling->SetApzcParent(parent);
    }
  }
}

void
HitTestingTreeNode::MakeRoot()
{
  mParent = nullptr;

  if (GetApzc()) {
    SetApzcParent(nullptr);
  }
}

HitTestingTreeNode*
HitTestingTreeNode::GetFirstChild() const
{
  HitTestingTreeNode* child = GetLastChild();
  while (child && child->GetPrevSibling()) {
    child = child->GetPrevSibling();
  }
  return child;
}

HitTestingTreeNode*
HitTestingTreeNode::GetLastChild() const
{
  return mLastChild;
}

HitTestingTreeNode*
HitTestingTreeNode::GetPrevSibling() const
{
  return mPrevSibling;
}

HitTestingTreeNode*
HitTestingTreeNode::GetParent() const
{
  return mParent;
}

bool
HitTestingTreeNode::IsAncestorOf(const HitTestingTreeNode* aOther) const
{
  for (const HitTestingTreeNode* cur = aOther; cur; cur = cur->GetParent()) {
    if (cur == this) {
      return true;
    }
  }
  return false;
}

AsyncPanZoomController*
HitTestingTreeNode::GetApzc() const
{
  return mApzc;
}

AsyncPanZoomController*
HitTestingTreeNode::GetNearestContainingApzc() const
{
  for (const HitTestingTreeNode* n = this; n; n = n->GetParent()) {
    if (n->GetApzc()) {
      return n->GetApzc();
    }
  }
  return nullptr;
}

bool
HitTestingTreeNode::IsPrimaryHolder() const
{
  return mIsPrimaryApzcHolder;
}

LayersId
HitTestingTreeNode::GetLayersId() const
{
  return mLayersId;
}

void
HitTestingTreeNode::SetHitTestData(const EventRegions& aRegions,
                                   const LayerIntRegion& aVisibleRegion,
                                   const CSSTransformMatrix& aTransform,
                                   const Maybe<ParentLayerIntRegion>& aClipRegion,
                                   const EventRegionsOverride& aOverride,
                                   bool aIsBackfaceHidden)
{
  mEventRegions = aRegions;
  mVisibleRegion = aVisibleRegion;
  mTransform = aTransform;
  mClipRegion = aClipRegion;
  mOverride = aOverride;
  mIsBackfaceHidden = aIsBackfaceHidden;
}

bool
HitTestingTreeNode::IsOutsideClip(const ParentLayerPoint& aPoint) const
{
  // test against clip rect in ParentLayer coordinate space
  return (mClipRegion.isSome() && !mClipRegion->Contains(aPoint.x, aPoint.y));
}

Maybe<LayerPoint>
HitTestingTreeNode::Untransform(const ParentLayerPoint& aPoint,
                                const LayerToParentLayerMatrix4x4& aTransform) const
{
  Maybe<ParentLayerToLayerMatrix4x4> inverse = aTransform.MaybeInverse();
  if (inverse) {
    return UntransformBy(inverse.ref(), aPoint);
  }
  return Nothing();
}

CompositorHitTestInfo
HitTestingTreeNode::HitTest(const LayerPoint& aPoint) const
{
  CompositorHitTestInfo result = CompositorHitTestInfo::eInvisibleToHitTest;

  if (mOverride & EventRegionsOverride::ForceEmptyHitRegion) {
    return result;
  }

  auto point = LayerIntPoint::Round(aPoint);

  // If the layer's backface is showing and it's hidden, don't hit it.
  // This matches the behavior of main-thread hit testing in
  // nsDisplayTransform::HitTest().
  if (mIsBackfaceHidden) {
    return result;
  }

  // test against event regions in Layer coordinate space
  if (!mEventRegions.mHitRegion.Contains(point.x, point.y)) {
    return result;
  }

  result |= CompositorHitTestInfo::eVisibleToHitTest;

  if ((mOverride & EventRegionsOverride::ForceDispatchToContent) ||
      mEventRegions.mDispatchToContentHitRegion.Contains(point.x, point.y))
  {
    result |= CompositorHitTestInfo::eDispatchToContent;
    if (mEventRegions.mDTCRequiresTargetConfirmation) {
      result |= CompositorHitTestInfo::eRequiresTargetConfirmation;
    }
  } else if (gfxPrefs::TouchActionEnabled()) {
    if (mEventRegions.mNoActionRegion.Contains(point.x, point.y)) {
      // set all the touch-action flags as disabled
      result |= CompositorHitTestInfo::eTouchActionMask;
    } else {
      bool panX = mEventRegions.mHorizontalPanRegion.Contains(point.x, point.y);
      bool panY = mEventRegions.mVerticalPanRegion.Contains(point.x, point.y);
      if (panX && panY) {
        // touch-action: pan-x pan-y
        result |= CompositorHitTestInfo::eTouchActionDoubleTapZoomDisabled
                | CompositorHitTestInfo::eTouchActionPinchZoomDisabled;
      } else if (panX) {
        // touch-action: pan-x
        result |= CompositorHitTestInfo::eTouchActionPanYDisabled
                | CompositorHitTestInfo::eTouchActionPinchZoomDisabled
                | CompositorHitTestInfo::eTouchActionDoubleTapZoomDisabled;
      } else if (panY) {
        // touch-action: pan-y
        result |= CompositorHitTestInfo::eTouchActionPanXDisabled
                | CompositorHitTestInfo::eTouchActionPinchZoomDisabled
                | CompositorHitTestInfo::eTouchActionDoubleTapZoomDisabled;
      } // else we're in the touch-action: auto or touch-action: manipulation
        // cases and we'll allow all actions. Technically we shouldn't allow
        // double-tap zooming in the manipulation case but apparently this has
        // been broken since the dawn of time.
    }
  }

  // The scrollbar flags are set at the call site in GetAPZCAtPoint, because
  // those require walking up the tree to see if we are contained inside a
  // scrollbar or scrollthumb, and we do that there anyway to get the scrollbar
  // node.

  return result;
}

EventRegionsOverride
HitTestingTreeNode::GetEventRegionsOverride() const
{
  return mOverride;
}

const CSSTransformMatrix&
HitTestingTreeNode::GetTransform() const
{
  return mTransform;
}

const LayerIntRegion&
HitTestingTreeNode::GetVisibleRegion() const
{
  return mVisibleRegion;
}

void
HitTestingTreeNode::Dump(const char* aPrefix) const
{
  if (mPrevSibling) {
    mPrevSibling->Dump(aPrefix);
  }
  printf_stderr("%sHitTestingTreeNode (%p) APZC (%p) g=(%s) %s%s%sr=(%s) t=(%s) c=(%s)%s%s\n",
    aPrefix, this, mApzc.get(),
    mApzc ? Stringify(mApzc->GetGuid()).c_str() : nsPrintfCString("l=0x%" PRIx64, uint64_t(mLayersId)).get(),
    (mOverride & EventRegionsOverride::ForceDispatchToContent) ? "fdtc " : "",
    (mOverride & EventRegionsOverride::ForceEmptyHitRegion) ? "fehr " : "",
    (mFixedPosTarget != FrameMetrics::NULL_SCROLL_ID) ? nsPrintfCString("fixed=%" PRIu64 " ", mFixedPosTarget).get() : "",
    Stringify(mEventRegions).c_str(), Stringify(mTransform).c_str(),
    mClipRegion ? Stringify(mClipRegion.ref()).c_str() : "none",
    mScrollbarData.mDirection.isSome() ? " scrollbar" : "",
    IsScrollThumbNode() ? " scrollthumb" : "");
  if (mLastChild) {
    mLastChild->Dump(nsPrintfCString("%s  ", aPrefix).get());
  }
}

void
HitTestingTreeNode::SetApzcParent(AsyncPanZoomController* aParent)
{
  // precondition: GetApzc() is non-null
  MOZ_ASSERT(GetApzc() != nullptr);
  if (IsPrimaryHolder()) {
    GetApzc()->SetParent(aParent);
  } else {
    MOZ_ASSERT(GetApzc()->GetParent() == aParent);
  }
}

void
HitTestingTreeNode::Lock(const RecursiveMutexAutoLock& aProofOfTreeLock)
{
  mLockCount++;
}

void
HitTestingTreeNode::Unlock(const RecursiveMutexAutoLock& aProofOfTreeLock)
{
  MOZ_ASSERT(mLockCount > 0);
  mLockCount--;
}

HitTestingTreeNodeAutoLock::HitTestingTreeNodeAutoLock()
  : mTreeMutex(nullptr)
{
}

HitTestingTreeNodeAutoLock::~HitTestingTreeNodeAutoLock()
{
  Clear();
}

void
HitTestingTreeNodeAutoLock::Initialize(const RecursiveMutexAutoLock& aProofOfTreeLock,
                                       already_AddRefed<HitTestingTreeNode> aNode,
                                       RecursiveMutex& aTreeMutex)
{
  MOZ_ASSERT(!mNode);

  mNode = aNode;
  mTreeMutex = &aTreeMutex;

  mNode->Lock(aProofOfTreeLock);
}

void
HitTestingTreeNodeAutoLock::Clear()
{
  if (!mNode) {
    return;
  }
  MOZ_ASSERT(mTreeMutex);

  { // scope lock
    RecursiveMutexAutoLock lock(*mTreeMutex);
    mNode->Unlock(lock);
  }
  mNode = nullptr;
  mTreeMutex = nullptr;
}

} // namespace layers
} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HitTestingTreeNode.h"

#include "AsyncPanZoomController.h"                     // for AsyncPanZoomController
#include "LayersLogging.h"                              // for Stringify
#include "mozilla/gfx/Point.h"                          // for Point4D
#include "mozilla/layers/APZThreadUtils.h"              // for AssertOnCompositorThread
#include "mozilla/layers/AsyncCompositionManager.h"     // for ViewTransform::operator Matrix4x4()
#include "nsPrintfCString.h"                            // for nsPrintfCString
#include "UnitTransforms.h"                             // for ViewAs

namespace mozilla {
namespace layers {

HitTestingTreeNode::HitTestingTreeNode(AsyncPanZoomController* aApzc,
                                       bool aIsPrimaryHolder,
                                       uint64_t aLayersId)
  : mApzc(aApzc)
  , mIsPrimaryApzcHolder(aIsPrimaryHolder)
  , mLayersId(aLayersId)
  , mOverride(EventRegionsOverride::NoOverride)
{
  if (mIsPrimaryApzcHolder) {
    MOZ_ASSERT(mApzc);
  }
  MOZ_ASSERT(!mApzc || mApzc->GetLayersId() == mLayersId);
}

void
HitTestingTreeNode::RecycleWith(AsyncPanZoomController* aApzc,
                                uint64_t aLayersId)
{
  MOZ_ASSERT(!mIsPrimaryApzcHolder);
  Destroy(); // clear out tree pointers
  mApzc = aApzc;
  mLayersId = aLayersId;
  MOZ_ASSERT(!mApzc || mApzc->GetLayersId() == mLayersId);
  // The caller is expected to call SetHitTestData to repopulate the hit-test
  // fields.
}

HitTestingTreeNode::~HitTestingTreeNode()
{
}

void
HitTestingTreeNode::Destroy()
{
  APZThreadUtils::AssertOnCompositorThread();

  mPrevSibling = nullptr;
  mLastChild = nullptr;
  mParent = nullptr;

  if (mApzc) {
    if (mIsPrimaryApzcHolder) {
      mApzc->Destroy();
    }
    mApzc = nullptr;
  }

  mLayersId = 0;
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
HitTestingTreeNode::SetScrollbarData(FrameMetrics::ViewID aScrollViewId, Layer::ScrollDirection aDir, int32_t aScrollSize)
{
  mScrollViewId = aScrollViewId;
  mScrollDir = aDir;
  mScrollSize = aScrollSize;;
}

bool
HitTestingTreeNode::MatchesScrollDragMetrics(const AsyncDragMetrics& aDragMetrics) const
{
  return ((mScrollDir == Layer::HORIZONTAL &&
           aDragMetrics.mDirection == AsyncDragMetrics::HORIZONTAL) ||
          (mScrollDir == Layer::VERTICAL &&
           aDragMetrics.mDirection == AsyncDragMetrics::VERTICAL)) &&
         mScrollViewId == aDragMetrics.mViewId;
}

int32_t
HitTestingTreeNode::GetScrollSize() const
{
  return mScrollSize;
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

AsyncPanZoomController*
HitTestingTreeNode::GetNearestContainingApzcWithSameLayersId() const
{
  for (const HitTestingTreeNode* n = this;
       n && n->mLayersId == mLayersId;
       n = n->GetParent()) {
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

uint64_t
HitTestingTreeNode::GetLayersId() const
{
  return mLayersId;
}

void
HitTestingTreeNode::SetHitTestData(const EventRegions& aRegions,
                                   const gfx::Matrix4x4& aTransform,
                                   const Maybe<ParentLayerIntRegion>& aClipRegion,
                                   const EventRegionsOverride& aOverride)
{
  mEventRegions = aRegions;
  mTransform = aTransform;
  mClipRegion = aClipRegion;
  mOverride = aOverride;
}

bool
HitTestingTreeNode::IsOutsideClip(const ParentLayerPoint& aPoint) const
{
  // test against clip rect in ParentLayer coordinate space
  return (mClipRegion.isSome() && !mClipRegion->Contains(aPoint.x, aPoint.y));
}

Maybe<LayerPoint>
HitTestingTreeNode::Untransform(const ParentLayerPoint& aPoint) const
{
  // convert into Layer coordinate space
  gfx::Matrix4x4 localTransform = mTransform;
  if (mApzc) {
    localTransform = localTransform * mApzc->GetCurrentAsyncTransformWithOverscroll();
  }
  return UntransformBy(
      ViewAs<LayerToParentLayerMatrix4x4>(localTransform).Inverse(), aPoint);
}

HitTestResult
HitTestingTreeNode::HitTest(const ParentLayerPoint& aPoint) const
{
  // This should only ever get called if the point is inside the clip region
  // for this node.
  MOZ_ASSERT(!IsOutsideClip(aPoint));

  if (mOverride & EventRegionsOverride::ForceEmptyHitRegion) {
    return HitTestResult::HitNothing;
  }

  // convert into Layer coordinate space
  Maybe<LayerPoint> pointInLayerPixels = Untransform(aPoint);
  if (!pointInLayerPixels) {
    return HitTestResult::HitNothing;
  }
  LayerIntPoint point = RoundedToInt(pointInLayerPixels.ref());

  // test against event regions in Layer coordinate space
  if (!mEventRegions.mHitRegion.Contains(point.x, point.y)) {
    return HitTestResult::HitNothing;
  }
  if ((mOverride & EventRegionsOverride::ForceDispatchToContent) ||
      mEventRegions.mDispatchToContentHitRegion.Contains(point.x, point.y))
  {
    return HitTestResult::HitDispatchToContentRegion;
  }
  return HitTestResult::HitLayer;
}

EventRegionsOverride
HitTestingTreeNode::GetEventRegionsOverride() const
{
  return mOverride;
}

void
HitTestingTreeNode::Dump(const char* aPrefix) const
{
  if (mPrevSibling) {
    mPrevSibling->Dump(aPrefix);
  }
  printf_stderr("%sHitTestingTreeNode (%p) APZC (%p) g=(%s) %s%sr=(%s) t=(%s) c=(%s)\n",
    aPrefix, this, mApzc.get(),
    mApzc ? Stringify(mApzc->GetGuid()).c_str() : nsPrintfCString("l=%" PRIu64, mLayersId).get(),
    (mOverride & EventRegionsOverride::ForceDispatchToContent) ? "fdtc " : "",
    (mOverride & EventRegionsOverride::ForceEmptyHitRegion) ? "fehr " : "",
    Stringify(mEventRegions).c_str(), Stringify(mTransform).c_str(),
    mClipRegion ? Stringify(mClipRegion.ref()).c_str() : "none");
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

} // namespace layers
} // namespace mozilla

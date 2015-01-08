/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HitTestingTreeNode.h"

#include "AsyncPanZoomController.h"                     // for AsyncPanZoomController
#include "LayersLogging.h"                              // for Stringify
#include "mozilla/gfx/Point.h"                          // for Point4D
#include "mozilla/layers/AsyncCompositionManager.h"     // for ViewTransform::operator Matrix4x4()
#include "nsPrintfCString.h"                            // for nsPrintfCString
#include "UnitTransforms.h"                             // for ViewAs

namespace mozilla {
namespace layers {

HitTestingTreeNode::HitTestingTreeNode(AsyncPanZoomController* aApzc,
                                       bool aIsPrimaryHolder)
  : mApzc(aApzc)
  , mIsPrimaryApzcHolder(aIsPrimaryHolder)
{
  MOZ_ASSERT(mApzc);
}

HitTestingTreeNode::~HitTestingTreeNode()
{
}

void
HitTestingTreeNode::Destroy()
{
  AsyncPanZoomController::AssertOnCompositorThread();

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

void
HitTestingTreeNode::SetLastChild(HitTestingTreeNode* aChild)
{
  mLastChild = aChild;
  if (aChild) {
    // We assume that HitTestingTreeNodes with an ancestor/descendant
    // relationship cannot both point to the same APZC instance. This assertion
    // only covers a subset of cases in which that might occur, but it's better
    // than nothing.
    MOZ_ASSERT(aChild->Apzc() != Apzc());

    aChild->mParent = this;
    aChild->Apzc()->SetParent(Apzc());
  }
}

void
HitTestingTreeNode::SetPrevSibling(HitTestingTreeNode* aSibling)
{
  mPrevSibling = aSibling;
  if (aSibling) {
    aSibling->mParent = mParent;
    aSibling->Apzc()->SetParent(mParent ? mParent->Apzc() : nullptr);
  }
}

void
HitTestingTreeNode::MakeRoot()
{
  mParent = nullptr;
  Apzc()->SetParent(nullptr);
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
HitTestingTreeNode::Apzc() const
{
  return mApzc;
}

bool
HitTestingTreeNode::IsPrimaryHolder() const
{
  return mIsPrimaryApzcHolder;
}

void
HitTestingTreeNode::SetHitTestData(const EventRegions& aRegions,
                                   const gfx::Matrix4x4& aTransform,
                                   const nsIntRegion& aClipRegion)
{
  mEventRegions = aRegions;
  mTransform = aTransform;
  mClipRegion = aClipRegion;
}

HitTestResult
HitTestingTreeNode::HitTest(const ParentLayerPoint& aPoint) const
{
  // When event regions are disabled, we are actually storing the
  // touch-sensitive section of the composition bounds in the clip rect, and we
  // don't need to use mTransform or mEventRegions.
  if (!gfxPrefs::LayoutEventRegionsEnabled()) {
    MOZ_ASSERT(mEventRegions == EventRegions());
    MOZ_ASSERT(mTransform == gfx::Matrix4x4());
    return mClipRegion.Contains(aPoint.x, aPoint.y)
        ? HitTestResult::ApzcHitRegion
        : HitTestResult::NoApzcHit;
  }

  // test against clip rect in ParentLayer coordinate space
  if (!mClipRegion.Contains(aPoint.x, aPoint.y)) {
    return HitTestResult::NoApzcHit;
  }

  // convert into Layer coordinate space
  gfx::Matrix4x4 localTransform = mTransform * gfx::Matrix4x4(mApzc->GetCurrentAsyncTransform());
  gfx::Point4D pointInLayerPixels = localTransform.Inverse().ProjectPoint(aPoint.ToUnknownPoint());
  if (!pointInLayerPixels.HasPositiveWCoord()) {
    return HitTestResult::NoApzcHit;
  }
  LayerIntPoint point = RoundedToInt(ViewAs<LayerPixel>(pointInLayerPixels.As2DPoint()));

  // test against event regions in Layer coordinate space
  if (!mEventRegions.mHitRegion.Contains(point.x, point.y)) {
    return HitTestResult::NoApzcHit;
  }
  if (mEventRegions.mDispatchToContentHitRegion.Contains(point.x, point.y)) {
    return HitTestResult::ApzcContentRegion;
  }
  return HitTestResult::ApzcHitRegion;
}

void
HitTestingTreeNode::Dump(const char* aPrefix) const
{
  if (mPrevSibling) {
    mPrevSibling->Dump(aPrefix);
  }
  printf_stderr("%sHitTestingTreeNode (%p) APZC (%p) g=(%s) r=(%s) t=(%s) c=(%s)\n",
    aPrefix, this, mApzc.get(), Stringify(mApzc->GetGuid()).c_str(),
    Stringify(mEventRegions).c_str(), Stringify(mTransform).c_str(),
    Stringify(mClipRegion).c_str());
  if (mLastChild) {
    mLastChild->Dump(nsPrintfCString("%s  ", aPrefix).get());
  }
}

} // namespace layers
} // namespace mozilla

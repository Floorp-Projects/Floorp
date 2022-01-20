/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IAPZHitTester.h"
#include "APZCTreeManager.h"
#include "AsyncPanZoomController.h"

namespace mozilla {
namespace layers {

IAPZHitTester::HitTestResult
IAPZHitTester::HitTestResult::CopyWithoutScrollbarNode() const {
  HitTestResult result;
  result.mTargetApzc = mTargetApzc;
  result.mHitResult = mHitResult;
  result.mLayersId = mLayersId;
  result.mFixedPosSides = mFixedPosSides;
  result.mHitOverscrollGutter = mHitOverscrollGutter;
  return result;
}

LayersId IAPZHitTester::GetRootLayersId() const {
  return mTreeManager->mRootLayersId;
}

HitTestingTreeNode* IAPZHitTester::GetRootNode() const {
  mTreeManager->mTreeLock.AssertCurrentThreadIn();
  return mTreeManager->mRootNode;
}

HitTestingTreeNode* IAPZHitTester::FindRootNodeForLayersId(
    LayersId aLayersId) const {
  return mTreeManager->FindRootNodeForLayersId(aLayersId);
}

AsyncPanZoomController* IAPZHitTester::FindRootApzcForLayersId(
    LayersId aLayersId) const {
  HitTestingTreeNode* resultNode = FindRootNodeForLayersId(aLayersId);
  return resultNode ? resultNode->GetApzc() : nullptr;
}

already_AddRefed<HitTestingTreeNode> IAPZHitTester::GetTargetNode(
    const ScrollableLayerGuid& aGuid,
    ScrollableLayerGuid::Comparator aComparator) {
  // Acquire the tree lock so that derived classes can call this from
  // methods other than GetAPZCAtPoint().
  RecursiveMutexAutoLock lock(mTreeManager->mTreeLock);
  return mTreeManager->GetTargetNode(aGuid, aComparator);
}

void IAPZHitTester::InitializeHitTestingTreeNodeAutoLock(
    HitTestingTreeNodeAutoLock& aAutoLock,
    const RecursiveMutexAutoLock& aProofOfTreeLock,
    RefPtr<HitTestingTreeNode>& aNode) {
  aAutoLock.Initialize(aProofOfTreeLock, aNode.forget(),
                       mTreeManager->mTreeLock);
}

}  // namespace layers
}  // namespace mozilla

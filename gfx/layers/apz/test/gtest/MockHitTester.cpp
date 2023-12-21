/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MockHitTester.h"
#include "TreeTraversal.h"
#include "apz/src/APZCTreeManager.h"
#include "apz/src/AsyncPanZoomController.h"
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/layers/ScrollableLayerGuid.h"

namespace mozilla::layers {

IAPZHitTester::HitTestResult MockHitTester::GetAPZCAtPoint(
    const ScreenPoint& aHitTestPoint,
    const RecursiveMutexAutoLock& aProofOfTreeLock) {
  MOZ_ASSERT(!mQueuedResults.empty());
  HitTestResult result = std::move(mQueuedResults.front());
  mQueuedResults.pop();
  return result;
}

void MockHitTester::QueueHitResult(ScrollableLayerGuid::ViewID aScrollId,
                                   gfx::CompositorHitTestInfo aHitInfo) {
  LayersId layersId = GetRootLayersId();  // currently this is all the tests use
  RefPtr<HitTestingTreeNode> node =
      GetTargetNode(ScrollableLayerGuid(layersId, 0, aScrollId),
                    ScrollableLayerGuid::EqualsIgnoringPresShell);
  MOZ_ASSERT(node);
  AsyncPanZoomController* apzc = node->GetApzc();
  MOZ_ASSERT(apzc);
  HitTestResult result;
  result.mTargetApzc = apzc;
  result.mHitResult = aHitInfo;
  result.mLayersId = layersId;
  mQueuedResults.push(std::move(result));
}

void MockHitTester::QueueScrollbarThumbHitResult(
    ScrollableLayerGuid::ViewID aScrollId, ScrollDirection aDirection) {
  RecursiveMutexAutoLock lock(GetTreeLock());
  LayersId layersId = GetRootLayersId();  // currently this is all the tests use
  // First find the scrolalble node, to get the APZC.
  RefPtr<HitTestingTreeNode> scrollableNode =
      GetTargetNode(ScrollableLayerGuid(layersId, 0, aScrollId),
                    ScrollableLayerGuid::EqualsIgnoringPresShell);
  MOZ_ASSERT(scrollableNode);
  AsyncPanZoomController* apzc = scrollableNode->GetApzc();
  MOZ_ASSERT(apzc);

  // Now find the scroll thumb node.
  RefPtr<HitTestingTreeNode> scrollThumbNode =
      BreadthFirstSearch<ReverseIterator>(
          GetRootNode(), [&](HitTestingTreeNode* aNode) {
            return aNode->GetLayersId() == layersId &&
                   aNode->IsScrollThumbNode() &&
                   aNode->GetScrollbarDirection() == aDirection &&
                   aNode->GetScrollTargetId() == aScrollId;
          });
  MOZ_ASSERT(scrollThumbNode);

  HitTestResult result;
  result.mTargetApzc = apzc;
  result.mHitResult = {gfx::CompositorHitTestFlags::eVisibleToHitTest,
                       gfx::CompositorHitTestFlags::eScrollbarThumb};
  if (aDirection == ScrollDirection::eVertical) {
    result.mHitResult += gfx::CompositorHitTestFlags::eScrollbarVertical;
  }
  InitializeHitTestingTreeNodeAutoLock(result.mScrollbarNode, lock,
                                       scrollThumbNode);
  mQueuedResults.push(std::move(result));
}

}  // namespace mozilla::layers

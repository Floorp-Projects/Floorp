/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InternalHitTester.h"
#include "TreeTraversal.h"  // for ForEachNode
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "apz/src/APZCTreeManager.h"
#include "apz/src/AsyncPanZoomController.h"

#define APZCTM_LOG(...) \
  MOZ_LOG(APZCTreeManager::sLog, LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {
namespace layers {

using mozilla::gfx::CompositorHitTestFlags;
using mozilla::gfx::CompositorHitTestInfo;
using mozilla::gfx::CompositorHitTestInvisibleToHit;

IAPZHitTester::HitTestResult InternalHitTester::GetAPZCAtPoint(
    const ScreenPoint& aHitTestPoint,
    const RecursiveMutexAutoLock& aProofOfTreeLock) {
  HitTestResult hit;
  // This walks the tree in depth-first, reverse order, so that it encounters
  // APZCs front-to-back on the screen.
  HitTestingTreeNode* resultNode = nullptr;
  HitTestingTreeNode* root = GetRootNode();
  HitTestingTreeNode* scrollbarNode = nullptr;
  std::stack<LayerPoint> hitTestPoints;
  ParentLayerPoint point = ViewAs<ParentLayerPixel>(
      aHitTestPoint, PixelCastJustification::ScreenIsParentLayerForRoot);
  hitTestPoints.push(
      ViewAs<LayerPixel>(point, PixelCastJustification::MovingDownToChildren));

  ForEachNode<ReverseIterator>(
      root,
      [&resultNode, &hitTestPoints, &hit, this](HitTestingTreeNode* aNode) {
        ParentLayerPoint hitTestPointForParent = ViewAs<ParentLayerPixel>(
            hitTestPoints.top(), PixelCastJustification::MovingDownToChildren);
        if (aNode->IsOutsideClip(hitTestPointForParent)) {
          // If the point being tested is outside the clip region for this node
          // then we don't need to test against this node or any of its
          // children. Just skip it and move on.
          APZCTM_LOG("Point %s outside clip for node %p\n",
                     ToString(hitTestPointForParent).c_str(), aNode);
          return TraversalFlag::Skip;
        }
        // If this node has a transform that includes an overscroll transform,
        // check if the point is inside the corresponding APZC's overscroll
        // gutter. We do this here in the pre-action because we need the
        // hit-test point in ParentLayer coordinates for this check. If the
        // point is in the gutter, we can abort the search and target this node.
        // (Note that no descendant node would be a match if we're in the
        // gutter.)
        const AsyncPanZoomController* sourceOfOverscrollTransform = nullptr;
        auto transform =
            this->ComputeTransformForNode(aNode, &sourceOfOverscrollTransform);
        if (sourceOfOverscrollTransform &&
            sourceOfOverscrollTransform->IsInOverscrollGutter(
                hitTestPointForParent)) {
          APZCTM_LOG(
              "ParentLayer point %s in overscroll gutter of APZC %p (node "
              "%p)\n",
              ToString(hitTestPointForParent).c_str(), aNode->GetApzc(), aNode);
          resultNode = aNode;
          // We want to target the overscrolled APZC, but if we're over the
          // gutter then we're not over its hit or DTC regions. Use
          // {eVisibleToHitTest} as the hit result because the event won't be
          // sent to gecko (so DTC flags are irrelevant), and we do want
          // browser default actions to work (e.g. scrolling to relieve the
          // overscroll).
          hit.mHitResult = {CompositorHitTestFlags::eVisibleToHitTest};
          hit.mHitOverscrollGutter = true;
          return TraversalFlag::Abort;
        }
        // First check the subtree rooted at this node, because deeper nodes
        // are more "in front".
        Maybe<LayerPoint> hitTestPoint =
            aNode->Untransform(hitTestPointForParent, transform);
        APZCTM_LOG("Transformed ParentLayer point %s to layer %s\n",
                   ToString(hitTestPointForParent).c_str(),
                   hitTestPoint ? ToString(hitTestPoint.ref()).c_str() : "nil");
        if (!hitTestPoint) {
          return TraversalFlag::Skip;
        }
        hitTestPoints.push(hitTestPoint.ref());
        return TraversalFlag::Continue;
      },
      [&resultNode, &hitTestPoints, &hit](HitTestingTreeNode* aNode) {
        CompositorHitTestInfo hitResult = aNode->HitTest(hitTestPoints.top());
        hitTestPoints.pop();
        APZCTM_LOG("Testing Layer point %s against node %p\n",
                   ToString(hitTestPoints.top()).c_str(), aNode);
        if (hitResult != CompositorHitTestInvisibleToHit) {
          resultNode = aNode;
          hit.mHitResult = hitResult;
          return TraversalFlag::Abort;
        }
        return TraversalFlag::Continue;
      });

  if (hit.mHitResult != CompositorHitTestInvisibleToHit) {
    MOZ_ASSERT(resultNode);
    for (HitTestingTreeNode* n = resultNode; n; n = n->GetParent()) {
      if (n->IsScrollbarNode()) {
        scrollbarNode = n;
        hit.mHitResult += CompositorHitTestFlags::eScrollbar;
        if (n->IsScrollThumbNode()) {
          hit.mHitResult += CompositorHitTestFlags::eScrollbarThumb;
        }
        if (n->GetScrollbarDirection() == ScrollDirection::eVertical) {
          hit.mHitResult += CompositorHitTestFlags::eScrollbarVertical;
        }

        // If we hit a scrollbar, target the APZC for the content scrolled
        // by the scrollbar. (The scrollbar itself doesn't scroll with the
        // scrolled content, so it doesn't carry the scrolled content's
        // scroll metadata).
        RefPtr<AsyncPanZoomController> scrollTarget =
            mTreeManager->GetTargetAPZC(n->GetLayersId(),
                                        n->GetScrollTargetId());
        if (scrollTarget) {
          hit.mLayersId = n->GetLayersId();
          hit.mTargetApzc = std::move(scrollTarget);
          RefPtr<HitTestingTreeNode> scrollbarRef = scrollbarNode;
          InitializeHitTestingTreeNodeAutoLock(hit.mScrollbarNode,
                                               aProofOfTreeLock, scrollbarRef);
          return hit;
        }
      } else if (IsFixedToRootContent(n)) {
        hit.mFixedPosSides = n->GetFixedPosSides();
      }
    }

    hit.mTargetApzc = GetTargetApzcForNode(resultNode);
    if (!hit.mTargetApzc) {
      hit.mTargetApzc = FindRootApzcForLayersId(resultNode->GetLayersId());
      MOZ_ASSERT(hit.mTargetApzc);
      APZCTM_LOG("Found target %p using root lookup\n", hit.mTargetApzc.get());
    }
    APZCTM_LOG("Successfully matched APZC %p via node %p (hit result 0x%x)\n",
               hit.mTargetApzc.get(), resultNode, hit.mHitResult.serialize());
    hit.mLayersId = resultNode->GetLayersId();
  }

  // If we found an APZC that wasn't directly on the result node, we haven't
  // checked if we're in its overscroll gutter, so check now.
  if (hit.mTargetApzc && resultNode &&
      (hit.mTargetApzc != resultNode->GetApzc())) {
    hit.mHitOverscrollGutter =
        hit.mTargetApzc->IsInOverscrollGutter(aHitTestPoint);
  }

  return hit;
}

}  // namespace layers
}  // namespace mozilla

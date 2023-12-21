/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_IAPZHitTester_h
#define mozilla_layers_IAPZHitTester_h

#include "HitTestingTreeNode.h"  // for HitTestingTreeNodeAutoLock
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/layers/LayersTypes.h"

namespace mozilla {
namespace layers {

class AsyncPanZoomController;
class APZCTreeManager;

class IAPZHitTester {
 public:
  virtual ~IAPZHitTester() = default;

  // Not a constructor because we want external code to be able to pass a hit
  // tester to the APZCTreeManager constructor, which will then initialize it.
  void Initialize(APZCTreeManager* aTreeManager) {
    mTreeManager = aTreeManager;
  }

  // Represents the results of an APZ hit test.
  struct HitTestResult {
    // The APZC targeted by the hit test.
    RefPtr<AsyncPanZoomController> mTargetApzc;
    // The applicable hit test flags.
    gfx::CompositorHitTestInfo mHitResult;
    // The layers id of the content that was hit.
    // This effectively identifiers the process that was hit for
    // Fission purposes.
    LayersId mLayersId;
    // If a scrollbar was hit, this will be populated with the
    // scrollbar node. The AutoLock allows accessing the scrollbar
    // node without having to hold the tree lock.
    HitTestingTreeNodeAutoLock mScrollbarNode;
    // If content that is fixed to the root-content APZC was hit,
    // the sides of the viewport to which the content is fixed.
    SideBits mFixedPosSides = SideBits::eNone;
    // If a fixed/sticky position element was hit, this will be populated with
    // the hit-testing tree node. The AutoLock allows accessing the node
    // without having to hold the tree lock.
    HitTestingTreeNodeAutoLock mNode;
    // This is set to true If mTargetApzc is overscrolled and the
    // event targeted the gap space ("gutter") created by the overscroll.
    bool mHitOverscrollGutter = false;

    HitTestResult() = default;
    // Make it move-only.
    HitTestResult(HitTestResult&&) = default;
    HitTestResult& operator=(HitTestResult&&) = default;
  };

  virtual HitTestResult GetAPZCAtPoint(
      const ScreenPoint& aHitTestPoint,
      const RecursiveMutexAutoLock& aProofOfTreeLock) = 0;

  HitTestResult CloneHitTestResult(RecursiveMutexAutoLock& aProofOfTreeLock,
                                   const HitTestResult& aHitTestResult) const;

 protected:
  APZCTreeManager* mTreeManager = nullptr;

  // We are a friend of APZCTreeManager but our derived classes
  // are not. Wrap a few private members of APZCTreeManager for
  // use by derived classes.
  RecursiveMutex& GetTreeLock();
  LayersId GetRootLayersId() const;
  HitTestingTreeNode* GetRootNode() const;
  HitTestingTreeNode* FindRootNodeForLayersId(LayersId aLayersId) const;
  AsyncPanZoomController* FindRootApzcForLayersId(LayersId aLayersId) const;
  already_AddRefed<HitTestingTreeNode> GetTargetNode(
      const ScrollableLayerGuid& aGuid,
      ScrollableLayerGuid::Comparator aComparator);
  void InitializeHitTestingTreeNodeAutoLock(
      HitTestingTreeNodeAutoLock& aAutoLock,
      const RecursiveMutexAutoLock& aProofOfTreeLock,
      RefPtr<HitTestingTreeNode>& aNode) const;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_IAPZHitTester_h

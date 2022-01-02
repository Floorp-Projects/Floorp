/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "InputUtils.h"

class APZCTreeManagerGenericTester : public APZCTreeManagerTester {
 protected:
  void CreateSimpleScrollingLayer() {
    const char* treeShape = "x";
    nsIntRegion layerVisibleRegion[] = {
        nsIntRegion(IntRect(0, 0, 200, 200)),
    };
    CreateScrollData(treeShape, layerVisibleRegion);
    SetScrollableFrameMetrics(layers[0], ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 500, 500));
  }

  void CreateSimpleMultiLayerTree() {
    const char* treeShape = "x(xx)";
    // LayerID               0 12
    nsIntRegion layerVisibleRegion[] = {
        nsIntRegion(IntRect(0, 0, 100, 100)),
        nsIntRegion(IntRect(0, 0, 100, 50)),
        nsIntRegion(IntRect(0, 50, 100, 50)),
    };
    CreateScrollData(treeShape, layerVisibleRegion);
  }

  void CreatePotentiallyLeakingTree() {
    const char* treeShape = "x(x(x(x))x(x(x)))";
    // LayerID               0 1 2 3  4 5 6
    CreateScrollData(treeShape);
    SetScrollableFrameMetrics(layers[0], ScrollableLayerGuid::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[2],
                              ScrollableLayerGuid::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[5],
                              ScrollableLayerGuid::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[3],
                              ScrollableLayerGuid::START_SCROLL_ID + 2);
    SetScrollableFrameMetrics(layers[6],
                              ScrollableLayerGuid::START_SCROLL_ID + 3);
  }

  void CreateTwoLayerTree(int32_t aRootContentLayerIndex) {
    const char* treeShape = "x(x)";
    // LayerID               0 1
    nsIntRegion layerVisibleRegion[] = {
        nsIntRegion(IntRect(0, 0, 100, 100)),
        nsIntRegion(IntRect(0, 0, 100, 100)),
    };
    CreateScrollData(treeShape, layerVisibleRegion);
    SetScrollableFrameMetrics(layers[0], ScrollableLayerGuid::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[1],
                              ScrollableLayerGuid::START_SCROLL_ID + 1);
    SetScrollHandoff(layers[1], layers[0]);

    // Make layers[aRootContentLayerIndex] the root content
    ModifyFrameMetrics(layers[aRootContentLayerIndex],
                       [](ScrollMetadata& sm, FrameMetrics& fm) {
                         fm.SetIsRootContent(true);
                       });
  }
};

TEST_F(APZCTreeManagerGenericTester, ScrollablePaintedLayers) {
  CreateSimpleMultiLayerTree();
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);

  // both layers have the same scrollId
  SetScrollableFrameMetrics(layers[1], ScrollableLayerGuid::START_SCROLL_ID);
  SetScrollableFrameMetrics(layers[2], ScrollableLayerGuid::START_SCROLL_ID);
  UpdateHitTestingTree();

  TestAsyncPanZoomController* nullAPZC = nullptr;
  // so they should have the same APZC
  EXPECT_FALSE(HasScrollableFrameMetrics(layers[0]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[1]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[2]));
  EXPECT_EQ(ApzcOf(layers[1]), ApzcOf(layers[2]));
}

TEST_F(APZCTreeManagerGenericTester, Bug1068268) {
  CreatePotentiallyLeakingTree();
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);

  UpdateHitTestingTree();
  RefPtr<HitTestingTreeNode> root = manager->GetRootNode();
  RefPtr<HitTestingTreeNode> node2 = root->GetFirstChild()->GetFirstChild();
  RefPtr<HitTestingTreeNode> node5 = root->GetLastChild()->GetLastChild();

  EXPECT_EQ(ApzcOf(layers[2]), node5->GetApzc());
  EXPECT_EQ(ApzcOf(layers[2]), node2->GetApzc());
  EXPECT_EQ(ApzcOf(layers[0]), ApzcOf(layers[2])->GetParent());
  EXPECT_EQ(ApzcOf(layers[2]), ApzcOf(layers[5]));

  EXPECT_EQ(node2->GetFirstChild(), node2->GetLastChild());
  EXPECT_EQ(ApzcOf(layers[3]), node2->GetLastChild()->GetApzc());
  EXPECT_EQ(node5->GetFirstChild(), node5->GetLastChild());
  EXPECT_EQ(ApzcOf(layers[6]), node5->GetLastChild()->GetApzc());
  EXPECT_EQ(ApzcOf(layers[2]), ApzcOf(layers[3])->GetParent());
  EXPECT_EQ(ApzcOf(layers[5]), ApzcOf(layers[6])->GetParent());
}

class APZCTreeManagerGenericTesterMock : public APZCTreeManagerGenericTester {
 public:
  APZCTreeManagerGenericTesterMock() { CreateMockHitTester(); }
};

TEST_F(APZCTreeManagerGenericTesterMock, Bug1194876) {
  // Create a layer tree with parent and child scrollable layers, with the
  // child being the root content.
  CreateTwoLayerTree(1);
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();

  uint64_t blockId;
  nsTArray<ScrollableLayerGuid> targets;

  // First touch goes down, APZCTM will hit layers[1] because it is on top of
  // layers[0], but we tell it the real target APZC is layers[0].
  MultiTouchInput mti;
  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  mti.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(25, 50), ScreenSize(0, 0), 0, 0));
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1,
                     {CompositorHitTestFlags::eVisibleToHitTest,
                      CompositorHitTestFlags::eIrregularArea});
  blockId = manager->ReceiveInputEvent(mti).mInputBlockId;
  manager->ContentReceivedInputBlock(blockId, false);
  targets.AppendElement(ApzcOf(layers[0])->GetGuid());
  manager->SetTargetAPZC(blockId, targets);

  // Around here, the above touch will get processed by ApzcOf(layers[0])

  // Second touch goes down (first touch remains down), APZCTM will again hit
  // layers[1]. Again we tell it both touches landed on layers[0], but because
  // layers[1] is the RCD layer, it will end up being the multitouch target.
  mti.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(75, 50), ScreenSize(0, 0), 0, 0));
  // Each touch will get hit-tested, so queue two hit-test results.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1,
                     {CompositorHitTestFlags::eVisibleToHitTest,
                      CompositorHitTestFlags::eIrregularArea});
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1,
                     {CompositorHitTestFlags::eVisibleToHitTest,
                      CompositorHitTestFlags::eIrregularArea});
  blockId = manager->ReceiveInputEvent(mti).mInputBlockId;
  manager->ContentReceivedInputBlock(blockId, false);
  targets.AppendElement(ApzcOf(layers[0])->GetGuid());
  manager->SetTargetAPZC(blockId, targets);

  // Around here, the above multi-touch will get processed by ApzcOf(layers[1]).
  // We want to ensure that ApzcOf(layers[0]) has had its state cleared, because
  // otherwise it will do things like dispatch spurious long-tap events.

  EXPECT_CALL(*mcc, HandleTap(TapType::eLongTap, _, _, _, _)).Times(0);
}

TEST_F(APZCTreeManagerGenericTesterMock, TargetChangesMidGesture_Bug1570559) {
  // Create a layer tree with parent and child scrollable layers, with the
  // parent being the root content.
  CreateTwoLayerTree(0);
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();

  uint64_t blockId;
  nsTArray<ScrollableLayerGuid> targets;

  // First touch goes down. APZCTM hits the child layer because it is on top
  // (and we confirm this target), but do not prevent-default the event, causing
  // the child APZC's gesture detector to start a long-tap timeout task.
  MultiTouchInput mti =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  mti.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(25, 50), ScreenSize(0, 0), 0, 0));
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1,
                     {CompositorHitTestFlags::eVisibleToHitTest,
                      CompositorHitTestFlags::eIrregularArea});
  blockId = manager->ReceiveInputEvent(mti).mInputBlockId;
  manager->ContentReceivedInputBlock(blockId, /* default prevented = */ false);
  targets.AppendElement(ApzcOf(layers[1])->GetGuid());
  manager->SetTargetAPZC(blockId, targets);

  // Second touch goes down (first touch remains down). APZCTM again hits the
  // child and we confirm this, but multi-touch events are routed to the root
  // content APZC which is the parent. This event is prevent-defaulted, so we
  // clear the parent's gesture state. The bug is that we fail to clear the
  // child's gesture state.
  mti.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(75, 50), ScreenSize(0, 0), 0, 0));
  // Each touch will get hit-tested, so queue two hit-test results.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1,
                     {CompositorHitTestFlags::eVisibleToHitTest,
                      CompositorHitTestFlags::eIrregularArea});
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1,
                     {CompositorHitTestFlags::eVisibleToHitTest,
                      CompositorHitTestFlags::eIrregularArea});
  blockId = manager->ReceiveInputEvent(mti).mInputBlockId;
  manager->ContentReceivedInputBlock(blockId, /* default prevented = */ true);
  targets.AppendElement(ApzcOf(layers[1])->GetGuid());
  manager->SetTargetAPZC(blockId, targets);

  // If we've failed to clear the child's gesture state, then the long tap
  // timeout task will fire in TearDown() and a long-tap will be dispatched.
  EXPECT_CALL(*mcc, HandleTap(TapType::eLongTap, _, _, _, _)).Times(0);
}

TEST_F(APZCTreeManagerGenericTesterMock, Bug1198900) {
  // This is just a test that cancels a wheel event to make sure it doesn't
  // crash.
  CreateSimpleScrollingLayer();
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();

  ScreenPoint origin(100, 50);
  ScrollWheelInput swi(MillisecondsSinceStartup(mcc->Time()), mcc->Time(), 0,
                       ScrollWheelInput::SCROLLMODE_INSTANT,
                       ScrollWheelInput::SCROLLDELTA_PIXEL, origin, 0, 10,
                       false, WheelDeltaAdjustmentStrategy::eNone);
  uint64_t blockId;
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID,
                     {CompositorHitTestFlags::eVisibleToHitTest,
                      CompositorHitTestFlags::eIrregularArea});
  blockId = manager->ReceiveInputEvent(swi).mInputBlockId;
  manager->ContentReceivedInputBlock(blockId, /* preventDefault= */ true);
}

// The next two tests check that APZ clamps the scroll offset it composites even
// if the main thread fails to do so. (The main thread will always clamp its
// scroll offset internally, but it may not send APZ the clamped version for
// scroll offset synchronization reasons.)
TEST_F(APZCTreeManagerTester, Bug1551582) {
  // The simple layer tree has a scrollable rect of 500x500 and a composition
  // bounds of 200x200, leading to a scroll range of (0,0,300,300).
  CreateSimpleScrollingLayer();
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();

  // Simulate the main thread scrolling to the end of the scroll range.
  ModifyFrameMetrics(root, [](ScrollMetadata& aSm, FrameMetrics& aMetrics) {
    aMetrics.SetLayoutScrollOffset(CSSPoint(300, 300));
    nsTArray<ScrollPositionUpdate> scrollUpdates;
    scrollUpdates.AppendElement(ScrollPositionUpdate::NewScroll(
        ScrollOrigin::Other, CSSPoint::ToAppUnits(CSSPoint(300, 300))));
    aSm.SetScrollUpdates(scrollUpdates);
    aMetrics.SetScrollGeneration(scrollUpdates.LastElement().GetGeneration());
  });
  UpdateHitTestingTree();

  // Sanity check.
  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);
  CSSPoint compositedScrollOffset = apzc->GetCompositedScrollOffset();
  EXPECT_EQ(CSSPoint(300, 300), compositedScrollOffset);

  // Simulate the main thread shrinking the scrollable rect to 400x400 (and
  // thereby the scroll range to (0,0,200,200) without sending a new scroll
  // offset update for the clamped scroll position (200,200).
  ModifyFrameMetrics(root, [](ScrollMetadata& aSm, FrameMetrics& aMetrics) {
    aMetrics.SetScrollableRect(CSSRect(0, 0, 400, 400));
  });
  UpdateHitTestingTree();

  // Check that APZ has clamped the scroll offset to (200,200) for us.
  compositedScrollOffset = apzc->GetCompositedScrollOffset();
  EXPECT_EQ(CSSPoint(200, 200), compositedScrollOffset);
}
TEST_F(APZCTreeManagerTester, Bug1557424) {
  // The simple layer tree has a scrollable rect of 500x500 and a composition
  // bounds of 200x200, leading to a scroll range of (0,0,300,300).
  CreateSimpleScrollingLayer();
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();

  // Simulate the main thread scrolling to the end of the scroll range.
  ModifyFrameMetrics(root, [](ScrollMetadata& aSm, FrameMetrics& aMetrics) {
    aMetrics.SetLayoutScrollOffset(CSSPoint(300, 300));
    nsTArray<ScrollPositionUpdate> scrollUpdates;
    scrollUpdates.AppendElement(ScrollPositionUpdate::NewScroll(
        ScrollOrigin::Other, CSSPoint::ToAppUnits(CSSPoint(300, 300))));
    aSm.SetScrollUpdates(scrollUpdates);
    aMetrics.SetScrollGeneration(scrollUpdates.LastElement().GetGeneration());
  });
  UpdateHitTestingTree();

  // Sanity check.
  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);
  CSSPoint compositedScrollOffset = apzc->GetCompositedScrollOffset();
  EXPECT_EQ(CSSPoint(300, 300), compositedScrollOffset);

  // Simulate the main thread expanding the composition bounds to 300x300 (and
  // thereby shrinking the scroll range to (0,0,200,200) without sending a new
  // scroll offset update for the clamped scroll position (200,200).
  ModifyFrameMetrics(root, [](ScrollMetadata& aSm, FrameMetrics& aMetrics) {
    aMetrics.SetCompositionBounds(ParentLayerRect(0, 0, 300, 300));
  });
  UpdateHitTestingTree();

  // Check that APZ has clamped the scroll offset to (200,200) for us.
  compositedScrollOffset = apzc->GetCompositedScrollOffset();
  EXPECT_EQ(CSSPoint(200, 200), compositedScrollOffset);
}

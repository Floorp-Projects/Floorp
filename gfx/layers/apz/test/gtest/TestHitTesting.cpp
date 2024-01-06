/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"

#include "InputUtils.h"

class APZHitTestingTester : public APZCTreeManagerTester {
 protected:
  ScreenToParentLayerMatrix4x4 transformToApzc;
  ParentLayerToScreenMatrix4x4 transformToGecko;

  already_AddRefed<AsyncPanZoomController> GetTargetAPZC(
      const ScreenPoint& aPoint) {
    RefPtr<AsyncPanZoomController> hit =
        manager->GetTargetAPZC(aPoint).mTargetApzc;
    if (hit) {
      transformToApzc = manager->GetScreenToApzcTransform(hit.get());
      transformToGecko =
          manager->GetApzcToGeckoTransform(hit.get(), LayoutAndVisual);
    }
    return hit.forget();
  }

 protected:
  void DisableApzOn(WebRenderLayerScrollData* aLayer) {
    ModifyFrameMetrics(aLayer, [](ScrollMetadata& aSm, FrameMetrics&) {
      aSm.SetForceDisableApz(true);
    });
  }

  void CreateComplexMultiLayerTree() {
    const char* treeShape = "x(xx(x)xx(x(x)xx))";
    // LayerID               0 12 3 45 6 7 89
    LayerIntRect layerVisibleRect[] = {
        LayerIntRect(0, 0, 300, 400),    // root(0)
        LayerIntRect(0, 0, 100, 100),    // layer(1) in top-left
        LayerIntRect(50, 50, 200, 300),  // layer(2) centered in root(0)
        LayerIntRect(50, 50, 200,
                     300),  // layer(3) fully occupying parent layer(2)
        LayerIntRect(0, 200, 100, 100),  // layer(4) in bottom-left
        LayerIntRect(200, 0, 100,
                     400),  // layer(5) along the right 100px of root(0)
        LayerIntRect(200, 0, 100, 200),  // layer(6) taking up the top
                                         // half of parent layer(5)
        LayerIntRect(200, 0, 100,
                     200),  // layer(7) fully occupying parent layer(6)
        LayerIntRect(200, 200, 100,
                     100),  // layer(8) in bottom-right (below (6))
        LayerIntRect(200, 300, 100,
                     100),  // layer(9) in bottom-right (below (8))
    };
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(layers[1], ScrollableLayerGuid::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[2], ScrollableLayerGuid::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[4],
                              ScrollableLayerGuid::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[6],
                              ScrollableLayerGuid::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[7],
                              ScrollableLayerGuid::START_SCROLL_ID + 2);
    SetScrollableFrameMetrics(layers[8],
                              ScrollableLayerGuid::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[9],
                              ScrollableLayerGuid::START_SCROLL_ID + 3);
  }

  void CreateBug1148350LayerTree() {
    const char* treeShape = "x(x)";
    // LayerID               0 1
    LayerIntRect layerVisibleRect[] = {
        LayerIntRect(0, 0, 200, 200),
        LayerIntRect(0, 0, 200, 200),
    };
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(layers[1], ScrollableLayerGuid::START_SCROLL_ID);
  }
};

TEST_F(APZHitTestingTester, ComplexMultiLayerTree) {
  CreateComplexMultiLayerTree();
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();

  /* The layer tree looks like this:

                0
        |----|--+--|----|
        1    2     4    5
             |         /|\
             3        6 8 9
                      |
                      7

     Layers 1,2 have the same APZC
     Layers 4,6,8 have the same APZC
     Layer 7 has an APZC
     Layer 9 has an APZC
  */

  TestAsyncPanZoomController* nullAPZC = nullptr;
  // Ensure all the scrollable layers have an APZC

  EXPECT_FALSE(HasScrollableFrameMetrics(layers[0]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[1]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[2]));
  EXPECT_FALSE(HasScrollableFrameMetrics(layers[3]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[4]));
  EXPECT_FALSE(HasScrollableFrameMetrics(layers[5]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[6]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[7]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[8]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[9]));
  // Ensure those that scroll together have the same APZCs
  EXPECT_EQ(ApzcOf(layers[1]), ApzcOf(layers[2]));
  EXPECT_EQ(ApzcOf(layers[4]), ApzcOf(layers[6]));
  EXPECT_EQ(ApzcOf(layers[8]), ApzcOf(layers[6]));
  // Ensure those that don't scroll together have different APZCs
  EXPECT_NE(ApzcOf(layers[1]), ApzcOf(layers[4]));
  EXPECT_NE(ApzcOf(layers[1]), ApzcOf(layers[7]));
  EXPECT_NE(ApzcOf(layers[1]), ApzcOf(layers[9]));
  EXPECT_NE(ApzcOf(layers[4]), ApzcOf(layers[7]));
  EXPECT_NE(ApzcOf(layers[4]), ApzcOf(layers[9]));
  EXPECT_NE(ApzcOf(layers[7]), ApzcOf(layers[9]));
  // Ensure the APZC parent chains are set up correctly
  TestAsyncPanZoomController* layers1_2 = ApzcOf(layers[1]);
  TestAsyncPanZoomController* layers4_6_8 = ApzcOf(layers[4]);
  TestAsyncPanZoomController* layer7 = ApzcOf(layers[7]);
  TestAsyncPanZoomController* layer9 = ApzcOf(layers[9]);
  EXPECT_EQ(nullptr, layers1_2->GetParent());
  EXPECT_EQ(nullptr, layers4_6_8->GetParent());
  EXPECT_EQ(layers4_6_8, layer7->GetParent());
  EXPECT_EQ(nullptr, layer9->GetParent());
  // Ensure the hit-testing tree looks like the layer tree
  RefPtr<HitTestingTreeNode> root = manager->GetRootNode();
  RefPtr<HitTestingTreeNode> node5 = root->GetLastChild();
  RefPtr<HitTestingTreeNode> node4 = node5->GetPrevSibling();
  RefPtr<HitTestingTreeNode> node2 = node4->GetPrevSibling();
  RefPtr<HitTestingTreeNode> node1 = node2->GetPrevSibling();
  RefPtr<HitTestingTreeNode> node3 = node2->GetLastChild();
  RefPtr<HitTestingTreeNode> node9 = node5->GetLastChild();
  RefPtr<HitTestingTreeNode> node8 = node9->GetPrevSibling();
  RefPtr<HitTestingTreeNode> node6 = node8->GetPrevSibling();
  RefPtr<HitTestingTreeNode> node7 = node6->GetLastChild();
  EXPECT_EQ(nullptr, node1->GetPrevSibling());
  EXPECT_EQ(nullptr, node3->GetPrevSibling());
  EXPECT_EQ(nullptr, node6->GetPrevSibling());
  EXPECT_EQ(nullptr, node7->GetPrevSibling());
  EXPECT_EQ(nullptr, node1->GetLastChild());
  EXPECT_EQ(nullptr, node3->GetLastChild());
  EXPECT_EQ(nullptr, node4->GetLastChild());
  EXPECT_EQ(nullptr, node7->GetLastChild());
  EXPECT_EQ(nullptr, node8->GetLastChild());
  EXPECT_EQ(nullptr, node9->GetLastChild());

  // Assertions about hit-testing have been ported to mochitest,
  // in helper_hittest_bug1730606-4.html.
}

TEST_F(APZHitTestingTester, TestRepaintFlushOnNewInputBlock) {
  // The main purpose of this test is to verify that touch-start events (or
  // anything that starts a new input block) don't ever get untransformed. This
  // should always hold because the APZ code should flush repaints when we start
  // a new input block and the transform to gecko space should be empty.

  CreateSimpleScrollingLayer();
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();
  RefPtr<TestAsyncPanZoomController> apzcroot = ApzcOf(root);

  // At this point, the following holds (all coordinates in screen pixels):
  // layers[0] has content from (0,0)-(500,500), clipped by composition bounds
  // (0,0)-(200,200)

  MockFunction<void(std::string checkPointName)> check;

  {
    InSequence s;

    EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(AtLeast(1));
    EXPECT_CALL(check, Call("post-first-touch-start"));
    EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(AtLeast(1));
    EXPECT_CALL(check, Call("post-second-fling"));
    EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(AtLeast(1));
    EXPECT_CALL(check, Call("post-second-touch-start"));
  }

  // This first pan will move the APZC by 50 pixels, and dispatch a paint
  // request.
  Pan(apzcroot, 100, 50, PanOptions::NoFling);

  // Verify that a touch start doesn't get untransformed
  ScreenIntPoint touchPoint(50, 50);
  MultiTouchInput mti =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  mti.mTouches.AppendElement(
      SingleTouchData(0, touchPoint, ScreenSize(0, 0), 0, 0));

  EXPECT_EQ(nsEventStatus_eConsumeDoDefault,
            manager->ReceiveInputEvent(mti).GetStatus());
  EXPECT_EQ(touchPoint, mti.mTouches[0].mScreenPoint);
  check.Call("post-first-touch-start");

  // Send a touchend to clear state
  mti.mType = MultiTouchInput::MULTITOUCH_END;
  manager->ReceiveInputEvent(mti);

  mcc->AdvanceByMillis(1000);

  // Now do two pans. The first of these will dispatch a repaint request, as
  // above. The second will get stuck in the paint throttler because the first
  // one doesn't get marked as "completed", so this will result in a non-empty
  // LD transform. (Note that any outstanding repaint requests from the first
  // half of this test don't impact this half because we advance the time by 1
  // second, which will trigger the max-wait-exceeded codepath in the paint
  // throttler).
  Pan(apzcroot, 100, 50, PanOptions::NoFling);
  check.Call("post-second-fling");
  Pan(apzcroot, 100, 50, PanOptions::NoFling);

  // Ensure that a touch start again doesn't get untransformed by flushing
  // a repaint
  mti.mType = MultiTouchInput::MULTITOUCH_START;
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault,
            manager->ReceiveInputEvent(mti).GetStatus());
  EXPECT_EQ(touchPoint, mti.mTouches[0].mScreenPoint);
  check.Call("post-second-touch-start");

  mti.mType = MultiTouchInput::MULTITOUCH_END;
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault,
            manager->ReceiveInputEvent(mti).GetStatus());
  EXPECT_EQ(touchPoint, mti.mTouches[0].mScreenPoint);
}

TEST_F(APZHitTestingTester, TestRepaintFlushOnWheelEvents) {
  // The purpose of this test is to ensure that wheel events trigger a repaint
  // flush as per bug 1166871, and that the wheel event untransform is a no-op.

  CreateSimpleScrollingLayer();
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();
  TestAsyncPanZoomController* apzcroot = ApzcOf(root);

  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(AtLeast(3));
  ScreenPoint origin(100, 50);
  for (int i = 0; i < 3; i++) {
    ScrollWheelInput swi(mcc->Time(), 0, ScrollWheelInput::SCROLLMODE_INSTANT,
                         ScrollWheelInput::SCROLLDELTA_PIXEL, origin, 0, 10,
                         false, WheelDeltaAdjustmentStrategy::eNone);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault,
              manager->ReceiveInputEvent(swi).GetStatus());
    EXPECT_EQ(origin, swi.mOrigin);

    AsyncTransform viewTransform;
    ParentLayerPoint point;
    apzcroot->SampleContentTransformForFrame(&viewTransform, point);
    EXPECT_EQ(0, point.x);
    EXPECT_EQ((i + 1) * 10, point.y);
    EXPECT_EQ(0, viewTransform.mTranslation.x);
    EXPECT_EQ((i + 1) * -10, viewTransform.mTranslation.y);

    mcc->AdvanceByMillis(5);
  }
}

TEST_F(APZHitTestingTester, TestForceDisableApz) {
  CreateSimpleScrollingLayer();
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();
  DisableApzOn(root);
  TestAsyncPanZoomController* apzcroot = ApzcOf(root);

  ScreenPoint origin(100, 50);
  ScrollWheelInput swi(mcc->Time(), 0, ScrollWheelInput::SCROLLMODE_INSTANT,
                       ScrollWheelInput::SCROLLDELTA_PIXEL, origin, 0, 10,
                       false, WheelDeltaAdjustmentStrategy::eNone);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault,
            manager->ReceiveInputEvent(swi).GetStatus());
  EXPECT_EQ(origin, swi.mOrigin);

  AsyncTransform viewTransform;
  ParentLayerPoint point;
  apzcroot->SampleContentTransformForFrame(&viewTransform, point);
  // Since APZ is force-disabled, we expect to see the async transform via
  // the NORMAL AsyncMode, but not via the RESPECT_FORCE_DISABLE AsyncMode.
  EXPECT_EQ(0, point.x);
  EXPECT_EQ(10, point.y);
  EXPECT_EQ(0, viewTransform.mTranslation.x);
  EXPECT_EQ(-10, viewTransform.mTranslation.y);
  viewTransform = apzcroot->GetCurrentAsyncTransform(
      AsyncPanZoomController::eForCompositing);
  point = apzcroot->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForCompositing);
  EXPECT_EQ(0, point.x);
  EXPECT_EQ(0, point.y);
  EXPECT_EQ(0, viewTransform.mTranslation.x);
  EXPECT_EQ(0, viewTransform.mTranslation.y);

  mcc->AdvanceByMillis(10);

  // With untransforming events we should get normal behaviour (in this case,
  // no noticeable untransform, because the repaint request already got
  // flushed).
  swi = ScrollWheelInput(mcc->Time(), 0, ScrollWheelInput::SCROLLMODE_INSTANT,
                         ScrollWheelInput::SCROLLDELTA_PIXEL, origin, 0, 0,
                         false, WheelDeltaAdjustmentStrategy::eNone);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault,
            manager->ReceiveInputEvent(swi).GetStatus());
  EXPECT_EQ(origin, swi.mOrigin);
}

TEST_F(APZHitTestingTester, Bug1148350) {
  CreateBug1148350LayerTree();
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();

  MockFunction<void(std::string checkPointName)> check;
  {
    InSequence s;
    EXPECT_CALL(*mcc,
                HandleTap(TapType::eSingleTap, LayoutDevicePoint(100, 100), 0,
                          ApzcOf(layers[1])->GetGuid(), _, _))
        .Times(1);
    EXPECT_CALL(check, Call("Tapped without transform"));
    EXPECT_CALL(*mcc,
                HandleTap(TapType::eSingleTap, LayoutDevicePoint(100, 100), 0,
                          ApzcOf(layers[1])->GetGuid(), _, _))
        .Times(1);
    EXPECT_CALL(check, Call("Tapped with interleaved transform"));
  }

  Tap(manager, ScreenIntPoint(100, 100), TimeDuration::FromMilliseconds(100));
  mcc->RunThroughDelayedTasks();
  check.Call("Tapped without transform");

  uint64_t blockId =
      TouchDown(manager, ScreenIntPoint(100, 100), mcc->Time()).mInputBlockId;
  SetDefaultAllowedTouchBehavior(manager, blockId);
  mcc->AdvanceByMillis(100);

  layers[0]->SetVisibleRect(LayerIntRect(0, 50, 200, 150));
  layers[0]->SetTransform(Matrix4x4::Translation(0, 50, 0));
  UpdateHitTestingTree();

  TouchUp(manager, ScreenIntPoint(100, 100), mcc->Time());
  mcc->RunThroughDelayedTasks();
  check.Call("Tapped with interleaved transform");
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
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

  already_AddRefed<AsyncPanZoomController> GetTargetAPZC(const ScreenPoint& aPoint) {
    RefPtr<AsyncPanZoomController> hit = manager->GetTargetAPZC(aPoint, nullptr);
    if (hit) {
      transformToApzc = manager->GetScreenToApzcTransform(hit.get());
      transformToGecko = manager->GetApzcToGeckoTransform(hit.get());
    }
    return hit.forget();
  }

protected:
  void CreateHitTesting1LayerTree() {
    const char* layerTreeSyntax = "c(tttt)";
    // LayerID                     0 1234
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0,0,100,100)),
      nsIntRegion(IntRect(0,0,100,100)),
      nsIntRegion(IntRect(10,10,20,20)),
      nsIntRegion(IntRect(10,10,20,20)),
      nsIntRegion(IntRect(5,5,20,20)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
  }

  void CreateHitTesting2LayerTree() {
    const char* layerTreeSyntax = "c(tc(t))";
    // LayerID                     0 12 3
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0,0,100,100)),
      nsIntRegion(IntRect(10,10,40,40)),
      nsIntRegion(IntRect(10,60,40,40)),
      nsIntRegion(IntRect(10,60,40,40)),
    };
    Matrix4x4 transforms[] = {
      Matrix4x4(),
      Matrix4x4(),
      Matrix4x4::Scaling(2, 1, 1),
      Matrix4x4(),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, transforms, lm, layers);

    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 200, 200));
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 80, 80));
    SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 2, CSSRect(0, 0, 80, 80));
  }

  void DisableApzOn(Layer* aLayer) {
    ScrollMetadata m = aLayer->GetScrollMetadata(0);
    m.SetForceDisableApz(true);
    aLayer->SetScrollMetadata(m);
  }

  void CreateComplexMultiLayerTree() {
    const char* layerTreeSyntax = "c(tc(t)tc(c(t)tt))";
    // LayerID                     0 12 3 45 6 7 89
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0,0,300,400)),      // root(0)
      nsIntRegion(IntRect(0,0,100,100)),      // thebes(1) in top-left
      nsIntRegion(IntRect(50,50,200,300)),    // container(2) centered in root(0)
      nsIntRegion(IntRect(50,50,200,300)),    // thebes(3) fully occupying parent container(2)
      nsIntRegion(IntRect(0,200,100,100)),    // thebes(4) in bottom-left
      nsIntRegion(IntRect(200,0,100,400)),    // container(5) along the right 100px of root(0)
      nsIntRegion(IntRect(200,0,100,200)),    // container(6) taking up the top half of parent container(5)
      nsIntRegion(IntRect(200,0,100,200)),    // thebes(7) fully occupying parent container(6)
      nsIntRegion(IntRect(200,200,100,100)),  // thebes(8) in bottom-right (below (6))
      nsIntRegion(IntRect(200,300,100,100)),  // thebes(9) in bottom-right (below (8))
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[4], FrameMetrics::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[6], FrameMetrics::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[7], FrameMetrics::START_SCROLL_ID + 2);
    SetScrollableFrameMetrics(layers[8], FrameMetrics::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[9], FrameMetrics::START_SCROLL_ID + 3);
  }

  void CreateBug1148350LayerTree() {
    const char* layerTreeSyntax = "c(t)";
    // LayerID                     0 1
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0,0,200,200)),
      nsIntRegion(IntRect(0,0,200,200)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID);
  }
};

// A simple hit testing test that doesn't involve any transforms on layers.
TEST_F(APZHitTestingTester, HitTesting1) {
  CreateHitTesting1LayerTree();
  ScopedLayerTreeRegistration registration(manager, 0, root, mcc);

  // No APZC attached so hit testing will return no APZC at (20,20)
  RefPtr<AsyncPanZoomController> hit = GetTargetAPZC(ScreenPoint(20, 20));
  TestAsyncPanZoomController* nullAPZC = nullptr;
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(ScreenToParentLayerMatrix4x4(), transformToApzc);
  EXPECT_EQ(ParentLayerToScreenMatrix4x4(), transformToGecko);

  uint32_t paintSequenceNumber = 0;

  // Now we have a root APZC that will match the page
  SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, paintSequenceNumber++);
  hit = GetTargetAPZC(ScreenPoint(15, 15));
  EXPECT_EQ(ApzcOf(root), hit.get());
  // expect hit point at LayerIntPoint(15, 15)
  EXPECT_EQ(ParentLayerPoint(15, 15), transformToApzc * ScreenPoint(15, 15));
  EXPECT_EQ(ScreenPoint(15, 15), transformToGecko * ParentLayerPoint(15, 15));

  // Now we have a sub APZC with a better fit
  SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 1);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, paintSequenceNumber++);
  EXPECT_NE(ApzcOf(root), ApzcOf(layers[3]));
  hit = GetTargetAPZC(ScreenPoint(25, 25));
  EXPECT_EQ(ApzcOf(layers[3]), hit.get());
  // expect hit point at LayerIntPoint(25, 25)
  EXPECT_EQ(ParentLayerPoint(25, 25), transformToApzc * ScreenPoint(25, 25));
  EXPECT_EQ(ScreenPoint(25, 25), transformToGecko * ParentLayerPoint(25, 25));

  // At this point, layers[4] obscures layers[3] at the point (15, 15) so
  // hitting there should hit the root APZC
  hit = GetTargetAPZC(ScreenPoint(15, 15));
  EXPECT_EQ(ApzcOf(root), hit.get());

  // Now test hit testing when we have two scrollable layers
  SetScrollableFrameMetrics(layers[4], FrameMetrics::START_SCROLL_ID + 2);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, paintSequenceNumber++);
  hit = GetTargetAPZC(ScreenPoint(15, 15));
  EXPECT_EQ(ApzcOf(layers[4]), hit.get());
  // expect hit point at LayerIntPoint(15, 15)
  EXPECT_EQ(ParentLayerPoint(15, 15), transformToApzc * ScreenPoint(15, 15));
  EXPECT_EQ(ScreenPoint(15, 15), transformToGecko * ParentLayerPoint(15, 15));

  // Hit test ouside the reach of layer[3,4] but inside root
  hit = GetTargetAPZC(ScreenPoint(90, 90));
  EXPECT_EQ(ApzcOf(root), hit.get());
  // expect hit point at LayerIntPoint(90, 90)
  EXPECT_EQ(ParentLayerPoint(90, 90), transformToApzc * ScreenPoint(90, 90));
  EXPECT_EQ(ScreenPoint(90, 90), transformToGecko * ParentLayerPoint(90, 90));

  // Hit test ouside the reach of any layer
  hit = GetTargetAPZC(ScreenPoint(1000, 10));
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(ScreenToParentLayerMatrix4x4(), transformToApzc);
  EXPECT_EQ(ParentLayerToScreenMatrix4x4(), transformToGecko);
  hit = GetTargetAPZC(ScreenPoint(-1000, 10));
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(ScreenToParentLayerMatrix4x4(), transformToApzc);
  EXPECT_EQ(ParentLayerToScreenMatrix4x4(), transformToGecko);
}

// A more involved hit testing test that involves css and async transforms.
TEST_F(APZHitTestingTester, HitTesting2) {
  SCOPED_GFX_PREF(APZVelocityBias, float, 0.0); // Velocity bias can cause extra repaint requests

  CreateHitTesting2LayerTree();
  ScopedLayerTreeRegistration registration(manager, 0, root, mcc);

  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);

  // At this point, the following holds (all coordinates in screen pixels):
  // layers[0] has content from (0,0)-(200,200), clipped by composition bounds (0,0)-(100,100)
  // layers[1] has content from (10,10)-(90,90), clipped by composition bounds (10,10)-(50,50)
  // layers[2] has content from (20,60)-(100,100). no clipping as it's not a scrollable layer
  // layers[3] has content from (20,60)-(180,140), clipped by composition bounds (20,60)-(100,100)

  TestAsyncPanZoomController* apzcroot = ApzcOf(root);
  TestAsyncPanZoomController* apzc1 = ApzcOf(layers[1]);
  TestAsyncPanZoomController* apzc3 = ApzcOf(layers[3]);

  // Hit an area that's clearly on the root layer but not any of the child layers.
  RefPtr<AsyncPanZoomController> hit = GetTargetAPZC(ScreenPoint(75, 25));
  EXPECT_EQ(apzcroot, hit.get());
  EXPECT_EQ(ParentLayerPoint(75, 25), transformToApzc * ScreenPoint(75, 25));
  EXPECT_EQ(ScreenPoint(75, 25), transformToGecko * ParentLayerPoint(75, 25));

  // Hit an area on the root that would be on layers[3] if layers[2]
  // weren't transformed.
  // Note that if layers[2] were scrollable, then this would hit layers[2]
  // because its composition bounds would be at (10,60)-(50,100) (and the
  // scale-only transform that we set on layers[2] would be invalid because
  // it would place the layer into overscroll, as its composition bounds
  // start at x=10 but its content at x=20).
  hit = GetTargetAPZC(ScreenPoint(15, 75));
  EXPECT_EQ(apzcroot, hit.get());
  EXPECT_EQ(ParentLayerPoint(15, 75), transformToApzc * ScreenPoint(15, 75));
  EXPECT_EQ(ScreenPoint(15, 75), transformToGecko * ParentLayerPoint(15, 75));

  // Hit an area on layers[1].
  hit = GetTargetAPZC(ScreenPoint(25, 25));
  EXPECT_EQ(apzc1, hit.get());
  EXPECT_EQ(ParentLayerPoint(25, 25), transformToApzc * ScreenPoint(25, 25));
  EXPECT_EQ(ScreenPoint(25, 25), transformToGecko * ParentLayerPoint(25, 25));

  // Hit an area on layers[3].
  hit = GetTargetAPZC(ScreenPoint(25, 75));
  EXPECT_EQ(apzc3, hit.get());
  // transformToApzc should unapply layers[2]'s transform
  EXPECT_EQ(ParentLayerPoint(12.5, 75), transformToApzc * ScreenPoint(25, 75));
  // and transformToGecko should reapply it
  EXPECT_EQ(ScreenPoint(25, 75), transformToGecko * ParentLayerPoint(12.5, 75));

  // Hit an area on layers[3] that would be on the root if layers[2]
  // weren't transformed.
  hit = GetTargetAPZC(ScreenPoint(75, 75));
  EXPECT_EQ(apzc3, hit.get());
  // transformToApzc should unapply layers[2]'s transform
  EXPECT_EQ(ParentLayerPoint(37.5, 75), transformToApzc * ScreenPoint(75, 75));
  // and transformToGecko should reapply it
  EXPECT_EQ(ScreenPoint(75, 75), transformToGecko * ParentLayerPoint(37.5, 75));

  // Pan the root layer upward by 50 pixels.
  // This causes layers[1] to scroll out of view, and an async transform
  // of -50 to be set on the root layer.
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  // This first pan will move the APZC by 50 pixels, and dispatch a paint request.
  // Since this paint request is in the queue to Gecko, transformToGecko will
  // take it into account.
  ApzcPanNoFling(apzcroot, 100, 50);

  // Hit where layers[3] used to be. It should now hit the root.
  hit = GetTargetAPZC(ScreenPoint(75, 75));
  EXPECT_EQ(apzcroot, hit.get());
  // transformToApzc doesn't unapply the root's own async transform
  EXPECT_EQ(ParentLayerPoint(75, 75), transformToApzc * ScreenPoint(75, 75));
  // and transformToGecko unapplies it and then reapplies it, because by the
  // time the event being transformed reaches Gecko the new paint request will
  // have been handled.
  EXPECT_EQ(ScreenPoint(75, 75), transformToGecko * ParentLayerPoint(75, 75));

  // Hit where layers[1] used to be and where layers[3] should now be.
  hit = GetTargetAPZC(ScreenPoint(25, 25));
  EXPECT_EQ(apzc3, hit.get());
  // transformToApzc unapplies both layers[2]'s css transform and the root's
  // async transform
  EXPECT_EQ(ParentLayerPoint(12.5, 75), transformToApzc * ScreenPoint(25, 25));
  // transformToGecko reapplies both the css transform and the async transform
  // because we have already issued a paint request with it.
  EXPECT_EQ(ScreenPoint(25, 25), transformToGecko * ParentLayerPoint(12.5, 75));

  // This second pan will move the APZC by another 50 pixels.
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);
  ApzcPanNoFling(apzcroot, 100, 50);

  // Hit where layers[3] used to be. It should now hit the root.
  hit = GetTargetAPZC(ScreenPoint(75, 75));
  EXPECT_EQ(apzcroot, hit.get());
  // transformToApzc doesn't unapply the root's own async transform
  EXPECT_EQ(ParentLayerPoint(75, 75), transformToApzc * ScreenPoint(75, 75));
  // transformToGecko unapplies the full async transform of -100 pixels
  EXPECT_EQ(ScreenPoint(75, 75), transformToGecko * ParentLayerPoint(75, 75));

  // Hit where layers[1] used to be. It should now hit the root.
  hit = GetTargetAPZC(ScreenPoint(25, 25));
  EXPECT_EQ(apzcroot, hit.get());
  // transformToApzc doesn't unapply the root's own async transform
  EXPECT_EQ(ParentLayerPoint(25, 25), transformToApzc * ScreenPoint(25, 25));
  // transformToGecko unapplies the full async transform of -100 pixels
  EXPECT_EQ(ScreenPoint(25, 25), transformToGecko * ParentLayerPoint(25, 25));
}

TEST_F(APZHitTestingTester, ComplexMultiLayerTree) {
  CreateComplexMultiLayerTree();
  ScopedLayerTreeRegistration registration(manager, 0, root, mcc);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);

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
  EXPECT_FALSE(layers[0]->HasScrollableFrameMetrics());
  EXPECT_NE(nullAPZC, ApzcOf(layers[1]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[2]));
  EXPECT_FALSE(layers[3]->HasScrollableFrameMetrics());
  EXPECT_NE(nullAPZC, ApzcOf(layers[4]));
  EXPECT_FALSE(layers[5]->HasScrollableFrameMetrics());
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

  RefPtr<AsyncPanZoomController> hit = GetTargetAPZC(ScreenPoint(25, 25));
  EXPECT_EQ(ApzcOf(layers[1]), hit.get());
  hit = GetTargetAPZC(ScreenPoint(275, 375));
  EXPECT_EQ(ApzcOf(layers[9]), hit.get());
  hit = GetTargetAPZC(ScreenPoint(250, 100));
  EXPECT_EQ(ApzcOf(layers[7]), hit.get());
}

TEST_F(APZHitTestingTester, TestRepaintFlushOnNewInputBlock) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);

  // The main purpose of this test is to verify that touch-start events (or anything
  // that starts a new input block) don't ever get untransformed. This should always
  // hold because the APZ code should flush repaints when we start a new input block
  // and the transform to gecko space should be empty.

  CreateSimpleScrollingLayer();
  ScopedLayerTreeRegistration registration(manager, 0, root, mcc);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
  TestAsyncPanZoomController* apzcroot = ApzcOf(root);

  // At this point, the following holds (all coordinates in screen pixels):
  // layers[0] has content from (0,0)-(500,500), clipped by composition bounds (0,0)-(200,200)

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

  // This first pan will move the APZC by 50 pixels, and dispatch a paint request.
  ApzcPanNoFling(apzcroot, 100, 50);

  // Verify that a touch start doesn't get untransformed
  ScreenIntPoint touchPoint(50, 50);
  MultiTouchInput mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  mti.mTouches.AppendElement(SingleTouchData(0, touchPoint, ScreenSize(0, 0), 0, 0));

  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, manager->ReceiveInputEvent(mti, nullptr, nullptr));
  EXPECT_EQ(touchPoint, mti.mTouches[0].mScreenPoint);
  check.Call("post-first-touch-start");

  // Send a touchend to clear state
  mti.mType = MultiTouchInput::MULTITOUCH_END;
  manager->ReceiveInputEvent(mti, nullptr, nullptr);

  mcc->AdvanceByMillis(1000);

  // Now do two pans. The first of these will dispatch a repaint request, as above.
  // The second will get stuck in the paint throttler because the first one doesn't
  // get marked as "completed", so this will result in a non-empty LD transform.
  // (Note that any outstanding repaint requests from the first half of this test
  // don't impact this half because we advance the time by 1 second, which will trigger
  // the max-wait-exceeded codepath in the paint throttler).
  ApzcPanNoFling(apzcroot, 100, 50);
  check.Call("post-second-fling");
  ApzcPanNoFling(apzcroot, 100, 50);

  // Ensure that a touch start again doesn't get untransformed by flushing
  // a repaint
  mti.mType = MultiTouchInput::MULTITOUCH_START;
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, manager->ReceiveInputEvent(mti, nullptr, nullptr));
  EXPECT_EQ(touchPoint, mti.mTouches[0].mScreenPoint);
  check.Call("post-second-touch-start");

  mti.mType = MultiTouchInput::MULTITOUCH_END;
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, manager->ReceiveInputEvent(mti, nullptr, nullptr));
  EXPECT_EQ(touchPoint, mti.mTouches[0].mScreenPoint);
}

TEST_F(APZHitTestingTester, TestRepaintFlushOnWheelEvents) {
  // The purpose of this test is to ensure that wheel events trigger a repaint
  // flush as per bug 1166871, and that the wheel event untransform is a no-op.

  CreateSimpleScrollingLayer();
  ScopedLayerTreeRegistration registration(manager, 0, root, mcc);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
  TestAsyncPanZoomController* apzcroot = ApzcOf(root);

  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(AtLeast(3));
  ScreenPoint origin(100, 50);
  for (int i = 0; i < 3; i++) {
    ScrollWheelInput swi(MillisecondsSinceStartup(mcc->Time()), mcc->Time(), 0,
      ScrollWheelInput::SCROLLMODE_INSTANT, ScrollWheelInput::SCROLLDELTA_PIXEL,
      origin, 0, 10, false);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, manager->ReceiveInputEvent(swi, nullptr, nullptr));
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
  DisableApzOn(root);
  ScopedLayerTreeRegistration registration(manager, 0, root, mcc);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
  TestAsyncPanZoomController* apzcroot = ApzcOf(root);

  ScreenPoint origin(100, 50);
  ScrollWheelInput swi(MillisecondsSinceStartup(mcc->Time()), mcc->Time(), 0,
    ScrollWheelInput::SCROLLMODE_INSTANT, ScrollWheelInput::SCROLLDELTA_PIXEL,
    origin, 0, 10, false);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, manager->ReceiveInputEvent(swi, nullptr, nullptr));
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
  viewTransform = apzcroot->GetCurrentAsyncTransform(AsyncPanZoomController::RESPECT_FORCE_DISABLE);
  point = apzcroot->GetCurrentAsyncScrollOffset(AsyncPanZoomController::RESPECT_FORCE_DISABLE);
  EXPECT_EQ(0, point.x);
  EXPECT_EQ(0, point.y);
  EXPECT_EQ(0, viewTransform.mTranslation.x);
  EXPECT_EQ(0, viewTransform.mTranslation.y);

  mcc->AdvanceByMillis(10);

  // With untransforming events we should get normal behaviour (in this case,
  // no noticeable untransform, because the repaint request already got
  // flushed).
  swi = ScrollWheelInput(MillisecondsSinceStartup(mcc->Time()), mcc->Time(), 0,
    ScrollWheelInput::SCROLLMODE_INSTANT, ScrollWheelInput::SCROLLDELTA_PIXEL,
    origin, 0, 0, false);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, manager->ReceiveInputEvent(swi, nullptr, nullptr));
  EXPECT_EQ(origin, swi.mOrigin);
}

TEST_F(APZHitTestingTester, Bug1148350) {
  CreateBug1148350LayerTree();
  ScopedLayerTreeRegistration registration(manager, 0, root, mcc);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);

  MockFunction<void(std::string checkPointName)> check;
  {
    InSequence s;
    EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(100, 100), 0, ApzcOf(layers[1])->GetGuid())).Times(1);
    EXPECT_CALL(check, Call("Tapped without transform"));
    EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(100, 100), 0, ApzcOf(layers[1])->GetGuid())).Times(1);
    EXPECT_CALL(check, Call("Tapped with interleaved transform"));
  }

  Tap(manager, ScreenIntPoint(100, 100), TimeDuration::FromMilliseconds(100));
  mcc->RunThroughDelayedTasks();
  check.Call("Tapped without transform");

  uint64_t blockId;
  TouchDown(manager, ScreenIntPoint(100, 100), mcc->Time(), &blockId);
  if (gfxPrefs::TouchActionEnabled()) {
    SetDefaultAllowedTouchBehavior(manager, blockId);
  }
  mcc->AdvanceByMillis(100);

  layers[0]->SetVisibleRegion(LayerIntRegion(LayerIntRect(0,50,200,150)));
  layers[0]->SetBaseTransform(Matrix4x4::Translation(0, 50, 0));
  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);

  TouchUp(manager, ScreenIntPoint(100, 100), mcc->Time());
  mcc->RunThroughDelayedTasks();
  check.Call("Tapped with interleaved transform");
}

TEST_F(APZHitTestingTester, HitTestingRespectsScrollClip_Bug1257288) {
  // Create the layer tree.
  const char* layerTreeSyntax = "c(tt)";
  // LayerID                     0 12
  nsIntRegion layerVisibleRegion[] = {
    nsIntRegion(IntRect(0,0,200,200)),
    nsIntRegion(IntRect(0,0,200,200)),
    nsIntRegion(IntRect(0,0,200,100))
  };
  root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);

  // Add root scroll metadata to the first painted layer.
  SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID, CSSRect(0,0,200,200));

  // Add root and subframe scroll metadata to the second painted layer.
  // Give the subframe metadata a scroll clip corresponding to the subframe's
  // composition bounds.
  // Importantly, give the layer a layer clip which leaks outside of the
  // subframe's composition bounds.
  ScrollMetadata rootMetadata = BuildScrollMetadata(
      FrameMetrics::START_SCROLL_ID, CSSRect(0,0,200,200),
      ParentLayerRect(0,0,200,200));
  ScrollMetadata subframeMetadata = BuildScrollMetadata(
      FrameMetrics::START_SCROLL_ID + 1, CSSRect(0,0,200,200),
      ParentLayerRect(0,0,200,100));
  subframeMetadata.SetClipRect(Some(ParentLayerIntRect(0,0,200,100)));
  layers[2]->SetScrollMetadata({subframeMetadata, rootMetadata});
  layers[2]->SetClipRect(Some(ParentLayerIntRect(0,0,200,200)));
  SetEventRegionsBasedOnBottommostMetrics(layers[2]);

  // Build the hit testing tree.
  ScopedLayerTreeRegistration registration(manager, 0, root, mcc);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);

  // Pan on a region that's inside layers[2]'s layer clip, but outside
  // its subframe metadata's scroll clip.
  Pan(manager, 120, 110);

  // Test that the subframe hasn't scrolled.
  EXPECT_EQ(CSSPoint(0,0), ApzcOf(layers[2], 0)->GetFrameMetrics().GetScrollOffset());
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "InputUtils.h"

class APZScrollHandoffTester : public APZCTreeManagerTester {
 protected:
  UniquePtr<ScopedLayerTreeRegistration> registration;
  TestAsyncPanZoomController* rootApzc;

  void CreateScrollHandoffLayerTree1() {
    const char* treeShape = "x(x)";
    LayerIntRect layerVisibleRect[] = {LayerIntRect(0, 0, 100, 100),
                                       LayerIntRect(0, 50, 100, 50)};
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 200, 200));
    SetScrollableFrameMetrics(layers[1],
                              ScrollableLayerGuid::START_SCROLL_ID + 1,
                              CSSRect(0, 0, 100, 100));
    SetScrollHandoff(layers[1], root);
    registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
    UpdateHitTestingTree();
    rootApzc = ApzcOf(root);
    rootApzc->GetFrameMetrics().SetIsRootContent(
        true);  // make root APZC zoomable
  }

  void CreateScrollHandoffLayerTree2() {
    const char* treeShape = "x(x(x))";
    LayerIntRect layerVisibleRect[] = {LayerIntRect(0, 0, 100, 100),
                                       LayerIntRect(0, 0, 100, 100),
                                       LayerIntRect(0, 50, 100, 50)};
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 200, 200));
    SetScrollableFrameMetrics(layers[1],
                              ScrollableLayerGuid::START_SCROLL_ID + 2,
                              CSSRect(-100, -100, 200, 200));
    SetScrollableFrameMetrics(layers[2],
                              ScrollableLayerGuid::START_SCROLL_ID + 1,
                              CSSRect(0, 0, 100, 100));
    SetScrollHandoff(layers[1], root);
    SetScrollHandoff(layers[2], layers[1]);
    // No ScopedLayerTreeRegistration as that just needs to be done once per
    // test and this is the second layer tree for a particular test.
    MOZ_ASSERT(registration);
    UpdateHitTestingTree();
    rootApzc = ApzcOf(root);
  }

  void CreateScrollHandoffLayerTree3() {
    const char* treeShape = "x(x(x)x(x))";
    LayerIntRect layerVisibleRect[] = {
        LayerIntRect(0, 0, 100, 100),  // root
        LayerIntRect(0, 0, 100, 50),   // scrolling parent 1
        LayerIntRect(0, 0, 100, 50),   // scrolling child 1
        LayerIntRect(0, 50, 100, 50),  // scrolling parent 2
        LayerIntRect(0, 50, 100, 50)   // scrolling child 2
    };
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(layers[0], ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 100, 100));
    SetScrollableFrameMetrics(layers[1],
                              ScrollableLayerGuid::START_SCROLL_ID + 1,
                              CSSRect(0, 0, 100, 100));
    SetScrollableFrameMetrics(layers[2],
                              ScrollableLayerGuid::START_SCROLL_ID + 2,
                              CSSRect(0, 0, 100, 100));
    SetScrollableFrameMetrics(layers[3],
                              ScrollableLayerGuid::START_SCROLL_ID + 3,
                              CSSRect(0, 50, 100, 100));
    SetScrollableFrameMetrics(layers[4],
                              ScrollableLayerGuid::START_SCROLL_ID + 4,
                              CSSRect(0, 50, 100, 100));
    SetScrollHandoff(layers[1], layers[0]);
    SetScrollHandoff(layers[3], layers[0]);
    SetScrollHandoff(layers[2], layers[1]);
    SetScrollHandoff(layers[4], layers[3]);
    registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
    UpdateHitTestingTree();
  }

  // Creates a layer tree with a parent layer that is only scrollable
  // horizontally, and a child layer that is only scrollable vertically.
  void CreateScrollHandoffLayerTree4() {
    const char* treeShape = "x(x)";
    LayerIntRect layerVisibleRect[] = {LayerIntRect(0, 0, 100, 100),
                                       LayerIntRect(0, 0, 100, 100)};
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 200, 100));
    SetScrollableFrameMetrics(layers[1],
                              ScrollableLayerGuid::START_SCROLL_ID + 1,
                              CSSRect(0, 0, 100, 200));
    SetScrollHandoff(layers[1], root);
    registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
    UpdateHitTestingTree();
    rootApzc = ApzcOf(root);
  }

  // Creates a layer tree with a parent layer that is not scrollable, and a
  // child layer that is only scrollable vertically.
  void CreateScrollHandoffLayerTree5() {
    const char* treeShape = "x(x)";
    LayerIntRect layerVisibleRect[] = {
        LayerIntRect(0, 0, 100, 100),  // scrolling parent
        LayerIntRect(0, 50, 100, 50)   // scrolling child
    };
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 100, 100));
    SetScrollableFrameMetrics(layers[1],
                              ScrollableLayerGuid::START_SCROLL_ID + 1,
                              CSSRect(0, 0, 100, 200));
    SetScrollHandoff(layers[1], root);
    registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
    UpdateHitTestingTree();
    rootApzc = ApzcOf(root);
  }

  void CreateScrollgrabLayerTree(bool makeParentScrollable = true) {
    const char* treeShape = "x(x)";
    LayerIntRect layerVisibleRect[] = {
        LayerIntRect(0, 0, 100, 100),  // scroll-grabbing parent
        LayerIntRect(0, 20, 100, 80)   // child
    };
    CreateScrollData(treeShape, layerVisibleRect);
    float parentHeight = makeParentScrollable ? 120 : 100;
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 100, parentHeight));
    SetScrollableFrameMetrics(layers[1],
                              ScrollableLayerGuid::START_SCROLL_ID + 1,
                              CSSRect(0, 0, 100, 800));
    SetScrollHandoff(layers[1], root);
    registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
    UpdateHitTestingTree();
    rootApzc = ApzcOf(root);
    rootApzc->GetScrollMetadata().SetHasScrollgrab(true);
  }

  void TestFlingAcceleration() {
    // Jack up the fling acceleration multiplier so we can easily determine
    // whether acceleration occured.
    const float kAcceleration = 100.0f;
    SCOPED_GFX_PREF_FLOAT("apz.fling_accel_base_mult", kAcceleration);
    SCOPED_GFX_PREF_FLOAT("apz.fling_accel_min_fling_velocity", 0.0);
    SCOPED_GFX_PREF_FLOAT("apz.fling_accel_min_pan_velocity", 0.0);

    RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

    // Pan once, enough to fully scroll the scrollgrab parent and then scroll
    // and fling the child.
    QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
    Pan(manager, 70, 40);

    // Give the fling animation a chance to start.
    SampleAnimationsOnce();

    float childVelocityAfterFling1 = childApzc->GetVelocityVector().y;

    // Pan again.
    QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
    Pan(manager, 70, 40);

    // Give the fling animation a chance to start.
    // This time it should be accelerated.
    SampleAnimationsOnce();

    float childVelocityAfterFling2 = childApzc->GetVelocityVector().y;

    // We should have accelerated once.
    // The division by 2 is to account for friction.
    EXPECT_GT(childVelocityAfterFling2,
              childVelocityAfterFling1 * kAcceleration / 2);

    // We should not have accelerated twice.
    // The division by 4 is to account for friction.
    EXPECT_LE(childVelocityAfterFling2,
              childVelocityAfterFling1 * kAcceleration * kAcceleration / 4);
  }

  void TestCrossApzcAxisLock() {
    SCOPED_GFX_PREF_INT("apz.axis_lock.mode", 1);

    CreateScrollHandoffLayerTree1();

    RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);
    Pan(childApzc, ScreenIntPoint(10, 60), ScreenIntPoint(15, 90),
        PanOptions::KeepFingerDown | PanOptions::ExactCoordinates);

    childApzc->AssertAxisLocked(ScrollDirection::eVertical);
    childApzc->AssertStateIsPanningLockedY();
  }
};

class APZScrollHandoffTesterMock : public APZScrollHandoffTester {
 public:
  APZScrollHandoffTesterMock() { CreateMockHitTester(); }
};

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
// Here we test that if the processing of a touch block is deferred while we
// wait for content to send a prevent-default message, overscroll is still
// handed off correctly when the block is processed.
TEST_F(APZScrollHandoffTester, DeferredInputEventProcessing) {
  SCOPED_GFX_PREF_BOOL("apz.allow_immediate_handoff", true);

  // Set up the APZC tree.
  CreateScrollHandoffLayerTree1();

  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  // Enable touch-listeners so that we can separate the queueing of input
  // events from them being processed.
  childApzc->SetWaitForMainThread();

  // Queue input events for a pan.
  uint64_t blockId = 0;
  Pan(childApzc, 90, 30, PanOptions::NoFling, nullptr, nullptr, &blockId);

  // Allow the pan to be processed.
  childApzc->ContentReceivedInputBlock(blockId, false);
  childApzc->ConfirmTarget(blockId);

  // Make sure overscroll was handed off correctly.
  EXPECT_EQ(50, childApzc->GetFrameMetrics().GetVisualScrollOffset().y);
  EXPECT_EQ(10, rootApzc->GetFrameMetrics().GetVisualScrollOffset().y);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
// Here we test that if the layer structure changes in between two input
// blocks being queued, and the first block is only processed after the second
// one has been queued, overscroll handoff for the first block follows
// the original layer structure while overscroll handoff for the second block
// follows the new layer structure.
TEST_F(APZScrollHandoffTester, LayerStructureChangesWhileEventsArePending) {
  SCOPED_GFX_PREF_BOOL("apz.allow_immediate_handoff", true);

  // Set up an initial APZC tree.
  CreateScrollHandoffLayerTree1();

  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  // Enable touch-listeners so that we can separate the queueing of input
  // events from them being processed.
  childApzc->SetWaitForMainThread();

  // Queue input events for a pan.
  uint64_t blockId = 0;
  Pan(childApzc, 90, 30, PanOptions::NoFling, nullptr, nullptr, &blockId);

  // Modify the APZC tree to insert a new APZC 'middle' into the handoff chain
  // between the child and the root.
  CreateScrollHandoffLayerTree2();
  WebRenderLayerScrollData* middle = layers[1];
  childApzc->SetWaitForMainThread();
  TestAsyncPanZoomController* middleApzc = ApzcOf(middle);

  // Queue input events for another pan.
  uint64_t secondBlockId = 0;
  Pan(childApzc, 30, 90, PanOptions::NoFling, nullptr, nullptr, &secondBlockId);

  // Allow the first pan to be processed.
  childApzc->ContentReceivedInputBlock(blockId, false);
  childApzc->ConfirmTarget(blockId);

  // Make sure things have scrolled according to the handoff chain in
  // place at the time the touch-start of the first pan was queued.
  EXPECT_EQ(50, childApzc->GetFrameMetrics().GetVisualScrollOffset().y);
  EXPECT_EQ(10, rootApzc->GetFrameMetrics().GetVisualScrollOffset().y);
  EXPECT_EQ(0, middleApzc->GetFrameMetrics().GetVisualScrollOffset().y);

  // Allow the second pan to be processed.
  childApzc->ContentReceivedInputBlock(secondBlockId, false);
  childApzc->ConfirmTarget(secondBlockId);

  // Make sure things have scrolled according to the handoff chain in
  // place at the time the touch-start of the second pan was queued.
  EXPECT_EQ(0, childApzc->GetFrameMetrics().GetVisualScrollOffset().y);
  EXPECT_EQ(10, rootApzc->GetFrameMetrics().GetVisualScrollOffset().y);
  EXPECT_EQ(-10, middleApzc->GetFrameMetrics().GetVisualScrollOffset().y);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
// Test that putting a second finger down on an APZC while a down-chain APZC
// is overscrolled doesn't result in being stuck in overscroll.
TEST_F(APZScrollHandoffTesterMock, StuckInOverscroll_Bug1073250) {
  // Enable overscrolling.
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);

  CreateScrollHandoffLayerTree1();

  TestAsyncPanZoomController* child = ApzcOf(layers[1]);

  // Pan, causing the parent APZC to overscroll.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  Pan(manager, 10, 40, PanOptions::KeepFingerDown);
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_TRUE(rootApzc->IsOverscrolled());

  // Put a second finger down.
  MultiTouchInput secondFingerDown =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  // Use the same touch identifier for the first touch (0) as Pan(). (A bit
  // hacky.)
  secondFingerDown.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(10, 40), ScreenSize(0, 0), 0, 0));
  secondFingerDown.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(30, 20), ScreenSize(0, 0), 0, 0));
  manager->ReceiveInputEvent(secondFingerDown);

  // Release the fingers.
  MultiTouchInput fingersUp = secondFingerDown;
  fingersUp.mType = MultiTouchInput::MULTITOUCH_END;
  manager->ReceiveInputEvent(fingersUp);

  // Allow any animations to run their course.
  child->AdvanceAnimationsUntilEnd();
  rootApzc->AdvanceAnimationsUntilEnd();

  // Make sure nothing is overscrolled.
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_FALSE(rootApzc->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
// This is almost exactly like StuckInOverscroll_Bug1073250, except the
// APZC receiving the input events for the first touch block is the child
// (and thus not the same APZC that overscrolls, which is the parent).
TEST_F(APZScrollHandoffTesterMock, StuckInOverscroll_Bug1231228) {
  // Enable overscrolling.
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);

  CreateScrollHandoffLayerTree1();

  TestAsyncPanZoomController* child = ApzcOf(layers[1]);

  // Pan, causing the parent APZC to overscroll.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  Pan(manager, 60, 90, PanOptions::KeepFingerDown);
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_TRUE(rootApzc->IsOverscrolled());

  // Put a second finger down.
  MultiTouchInput secondFingerDown =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  // Use the same touch identifier for the first touch (0) as Pan(). (A bit
  // hacky.)
  secondFingerDown.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(10, 40), ScreenSize(0, 0), 0, 0));
  secondFingerDown.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(30, 20), ScreenSize(0, 0), 0, 0));
  manager->ReceiveInputEvent(secondFingerDown);

  // Release the fingers.
  MultiTouchInput fingersUp = secondFingerDown;
  fingersUp.mType = MultiTouchInput::MULTITOUCH_END;
  manager->ReceiveInputEvent(fingersUp);

  // Allow any animations to run their course.
  child->AdvanceAnimationsUntilEnd();
  rootApzc->AdvanceAnimationsUntilEnd();

  // Make sure nothing is overscrolled.
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_FALSE(rootApzc->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZScrollHandoffTester, StuckInOverscroll_Bug1240202a) {
  // Enable overscrolling.
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  CreateScrollHandoffLayerTree1();

  TestAsyncPanZoomController* child = ApzcOf(layers[1]);

  // Pan, causing the parent APZC to overscroll.
  Pan(manager, 60, 90, PanOptions::KeepFingerDown);
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_TRUE(rootApzc->IsOverscrolled());

  // Lift the finger, triggering an overscroll animation
  // (but don't allow it to run).
  TouchUp(manager, ScreenIntPoint(10, 90), mcc->Time());

  // Put the finger down again, interrupting the animation
  // and entering the TOUCHING state.
  TouchDown(manager, ScreenIntPoint(10, 90), mcc->Time());

  // Lift the finger once again.
  TouchUp(manager, ScreenIntPoint(10, 90), mcc->Time());

  // Allow any animations to run their course.
  child->AdvanceAnimationsUntilEnd();
  rootApzc->AdvanceAnimationsUntilEnd();

  // Make sure nothing is overscrolled.
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_FALSE(rootApzc->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZScrollHandoffTesterMock, StuckInOverscroll_Bug1240202b) {
  // Enable overscrolling.
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  CreateScrollHandoffLayerTree1();

  TestAsyncPanZoomController* child = ApzcOf(layers[1]);

  // Pan, causing the parent APZC to overscroll.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  Pan(manager, 60, 90, PanOptions::KeepFingerDown);
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_TRUE(rootApzc->IsOverscrolled());

  // Lift the finger, triggering an overscroll animation
  // (but don't allow it to run).
  TouchUp(manager, ScreenIntPoint(10, 90), mcc->Time());

  // Put the finger down again, interrupting the animation
  // and entering the TOUCHING state.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  TouchDown(manager, ScreenIntPoint(10, 90), mcc->Time());

  // Put a second finger down. Since we're in the TOUCHING state,
  // the "are we panned into overscroll" check will fail and we
  // will not ignore the second finger, instead entering the
  // PINCHING state.
  MultiTouchInput secondFingerDown(MultiTouchInput::MULTITOUCH_START, 0,
                                   TimeStamp(), 0);
  // Use the same touch identifier for the first touch (0) as TouchDown(). (A
  // bit hacky.)
  secondFingerDown.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(10, 90), ScreenSize(0, 0), 0, 0));
  secondFingerDown.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(10, 80), ScreenSize(0, 0), 0, 0));
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  manager->ReceiveInputEvent(secondFingerDown);

  // Release the fingers.
  MultiTouchInput fingersUp = secondFingerDown;
  fingersUp.mType = MultiTouchInput::MULTITOUCH_END;
  manager->ReceiveInputEvent(fingersUp);

  // Allow any animations to run their course.
  child->AdvanceAnimationsUntilEnd();
  rootApzc->AdvanceAnimationsUntilEnd();

  // Make sure nothing is overscrolled.
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_FALSE(rootApzc->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZScrollHandoffTester, OpposingConstrainedAxes_Bug1201098) {
  // Enable overscrolling.
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  CreateScrollHandoffLayerTree4();

  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  // Pan, causing the child APZC to overscroll.
  Pan(childApzc, 50, 60);

  // Make sure only the child is overscrolled.
  EXPECT_TRUE(childApzc->IsOverscrolled());
  EXPECT_FALSE(rootApzc->IsOverscrolled());
}
#endif

// Test that flinging in a direction where one component of the fling goes into
// overscroll but the other doesn't, results in just the one component being
// handed off to the parent, while the original APZC continues flinging in the
// other direction.
TEST_F(APZScrollHandoffTesterMock, PartialFlingHandoff) {
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);

  CreateScrollHandoffLayerTree1();

  // Fling up and to the left. The child APZC has room to scroll up, but not
  // to the left, so the horizontal component of the fling should be handed
  // off to the parent APZC.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  Pan(manager, ScreenIntPoint(90, 90), ScreenIntPoint(55, 55));

  RefPtr<TestAsyncPanZoomController> parent = ApzcOf(layers[0]);
  RefPtr<TestAsyncPanZoomController> child = ApzcOf(layers[1]);

  // Advance the child's fling animation once to give the partial handoff
  // a chance to occur.
  mcc->AdvanceByMillis(10);
  child->AdvanceAnimations(mcc->GetSampleTime());

  // Assert that partial handoff has occurred.
  child->AssertStateIsFling();
  parent->AssertStateIsFling();
}

// Here we test that if two flings are happening simultaneously, overscroll
// is handed off correctly for each.
TEST_F(APZScrollHandoffTester, SimultaneousFlings) {
  SCOPED_GFX_PREF_BOOL("apz.allow_immediate_handoff", true);
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);

  // Set up an initial APZC tree.
  CreateScrollHandoffLayerTree3();

  RefPtr<TestAsyncPanZoomController> parent1 = ApzcOf(layers[1]);
  RefPtr<TestAsyncPanZoomController> child1 = ApzcOf(layers[2]);
  RefPtr<TestAsyncPanZoomController> parent2 = ApzcOf(layers[3]);
  RefPtr<TestAsyncPanZoomController> child2 = ApzcOf(layers[4]);

  // Pan on the lower child.
  Pan(child2, 45, 5);

  // Pan on the upper child.
  Pan(child1, 95, 55);

  // Check that child1 and child2 are in a FLING state.
  child1->AssertStateIsFling();
  child2->AssertStateIsFling();

  // Advance the animations on child1 and child2 until their end.
  child1->AdvanceAnimationsUntilEnd();
  child2->AdvanceAnimationsUntilEnd();

  // Check that the flings have been handed off to the parents.
  child1->AssertStateIsReset();
  parent1->AssertStateIsFling();
  child2->AssertStateIsReset();
  parent2->AssertStateIsFling();
}

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZScrollHandoffTester, Scrollgrab) {
  SCOPED_GFX_PREF_BOOL("apz.allow_immediate_handoff", true);

  // Set up the layer tree
  CreateScrollgrabLayerTree();

  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  // Pan on the child, enough to fully scroll the scrollgrab parent (20 px)
  // and leave some more (another 15 px) for the child.
  Pan(childApzc, 80, 45);

  // Check that the parent and child have scrolled as much as we expect.
  EXPECT_EQ(20, rootApzc->GetFrameMetrics().GetVisualScrollOffset().y);
  EXPECT_EQ(15, childApzc->GetFrameMetrics().GetVisualScrollOffset().y);
}
#endif

TEST_F(APZScrollHandoffTester, ScrollgrabFling) {
  SCOPED_GFX_PREF_BOOL("apz.allow_immediate_handoff", true);
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);

  // Set up the layer tree
  CreateScrollgrabLayerTree();

  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  // Pan on the child, not enough to fully scroll the scrollgrab parent.
  Pan(childApzc, 80, 70);

  // Check that it is the scrollgrab parent that's in a fling, not the child.
  rootApzc->AssertStateIsFling();
  childApzc->AssertStateIsReset();
}

TEST_F(APZScrollHandoffTesterMock, ScrollgrabFlingAcceleration1) {
  SCOPED_GFX_PREF_BOOL("apz.allow_immediate_handoff", true);
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);
  CreateScrollgrabLayerTree(true /* make parent scrollable */);

  // Note: Usually, fling acceleration does not work across handoff, because our
  // fling acceleration code does not propagate the "fling cancel velocity"
  // across handoff. However, this test sets apz.fling_min_velocity_threshold to
  // zero, so the "fling cancel velocity" is allowed to be zero, and fling
  // acceleration succeeds, almost by accident.
  TestFlingAcceleration();
}

TEST_F(APZScrollHandoffTesterMock, ScrollgrabFlingAcceleration2) {
  SCOPED_GFX_PREF_BOOL("apz.allow_immediate_handoff", true);
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);
  CreateScrollgrabLayerTree(false /* do not make parent scrollable */);
  TestFlingAcceleration();
}

TEST_F(APZScrollHandoffTester, ImmediateHandoffDisallowed_Pan) {
  SCOPED_GFX_PREF_BOOL("apz.allow_immediate_handoff", false);

  CreateScrollHandoffLayerTree1();

  RefPtr<TestAsyncPanZoomController> parentApzc = ApzcOf(layers[0]);
  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  // Pan on the child, enough to scroll it to its end and have scroll
  // left to hand off. Since immediate handoff is disallowed, we expect
  // the leftover scroll not to be handed off.
  Pan(childApzc, 60, 5);

  // Verify that the parent has not scrolled.
  EXPECT_EQ(50, childApzc->GetFrameMetrics().GetVisualScrollOffset().y);
  EXPECT_EQ(0, parentApzc->GetFrameMetrics().GetVisualScrollOffset().y);

  // Pan again on the child. This time, since the child was scrolled to
  // its end when the gesture began, we expect the scroll to be handed off.
  Pan(childApzc, 60, 50);

  // Verify that the parent scrolled.
  EXPECT_EQ(10, parentApzc->GetFrameMetrics().GetVisualScrollOffset().y);
}

TEST_F(APZScrollHandoffTester, ImmediateHandoffDisallowed_Fling) {
  SCOPED_GFX_PREF_BOOL("apz.allow_immediate_handoff", false);
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);

  CreateScrollHandoffLayerTree1();

  RefPtr<TestAsyncPanZoomController> parentApzc = ApzcOf(layers[0]);
  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  // Pan on the child, enough to get very close to the end, so that the
  // subsequent fling reaches the end and has leftover velocity to hand off.
  Pan(childApzc, 60, 2);

  // Allow the fling to run its course.
  childApzc->AdvanceAnimationsUntilEnd();
  parentApzc->AdvanceAnimationsUntilEnd();

  // Verify that the parent has not scrolled.
  // The first comparison needs to be an ASSERT_NEAR because the fling
  // computations are such that the final scroll position can be within
  // COORDINATE_EPSILON of the end rather than right at the end.
  ASSERT_NEAR(50, childApzc->GetFrameMetrics().GetVisualScrollOffset().y,
              COORDINATE_EPSILON);
  EXPECT_EQ(0, parentApzc->GetFrameMetrics().GetVisualScrollOffset().y);

  // Pan again on the child. This time, since the child was scrolled to
  // its end when the gesture began, we expect the scroll to be handed off.
  Pan(childApzc, 60, 40);

  // Allow the fling to run its course. The fling should also be handed off.
  childApzc->AdvanceAnimationsUntilEnd();
  parentApzc->AdvanceAnimationsUntilEnd();

  // Verify that the parent scrolled from the fling.
  EXPECT_GT(parentApzc->GetFrameMetrics().GetVisualScrollOffset().y, 10);
}

TEST_F(APZScrollHandoffTester, CrossApzcAxisLock_TouchAction) {
  TestCrossApzcAxisLock();
}

TEST_F(APZScrollHandoffTesterMock, WheelHandoffAfterDirectionReversal) {
  // Explicitly set the wheel transaction timeout pref because the test relies
  // on its value.
  SCOPED_GFX_PREF_INT("mousewheel.transaction.timeout", 1500);

  // Set up a basic scroll handoff layer tree.
  CreateScrollHandoffLayerTree1();

  rootApzc = ApzcOf(layers[0]);
  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);
  FrameMetrics& rootMetrics = rootApzc->GetFrameMetrics();
  FrameMetrics& childMetrics = childApzc->GetFrameMetrics();
  CSSRect childScrollRange = childMetrics.CalculateScrollRange();

  EXPECT_EQ(0, rootMetrics.GetVisualScrollOffset().y);
  EXPECT_EQ(0, childMetrics.GetVisualScrollOffset().y);

  ScreenIntPoint cursorLocation(10, 60);  // positioned to hit the subframe
  ScreenPoint upwardDelta(0, -10);
  ScreenPoint downwardDelta(0, 10);

  // First wheel upwards. This will have no effect because we're already
  // scrolled to the top.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  Wheel(manager, cursorLocation, upwardDelta, mcc->Time());
  EXPECT_EQ(0, rootMetrics.GetVisualScrollOffset().y);
  EXPECT_EQ(0, childMetrics.GetVisualScrollOffset().y);

  // Now wheel downwards 6 times. This should scroll the child, and get it
  // to the bottom of its 50px scroll range.
  for (size_t i = 0; i < 6; ++i) {
    mcc->AdvanceByMillis(100);
    QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
    Wheel(manager, cursorLocation, downwardDelta, mcc->Time());
  }
  EXPECT_EQ(0, rootMetrics.GetVisualScrollOffset().y);
  EXPECT_EQ(childScrollRange.YMost(), childMetrics.GetVisualScrollOffset().y);

  // Wheel downwards an additional 16 times, with 100ms increments.
  // This should be enough to overcome the 1500ms wheel transaction timeout
  // and start scrolling the root.
  for (size_t i = 0; i < 16; ++i) {
    mcc->AdvanceByMillis(100);
    QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
    Wheel(manager, cursorLocation, downwardDelta, mcc->Time());
  }
  EXPECT_EQ(childScrollRange.YMost(), childMetrics.GetVisualScrollOffset().y);
  EXPECT_GT(rootMetrics.GetVisualScrollOffset().y, 0);
}

TEST_F(APZScrollHandoffTesterMock, WheelHandoffNonscrollable) {
  // Set up a basic scroll layer tree.
  CreateScrollHandoffLayerTree5();

  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);
  FrameMetrics& childMetrics = childApzc->GetFrameMetrics();

  EXPECT_EQ(0, childMetrics.GetVisualScrollOffset().y);

  ScreenPoint downwardDelta(0, 10);
  // Positioned to hit the nonscrollable parent frame
  ScreenIntPoint nonscrollableLocation(40, 10);
  // Positioned to hit the scrollable subframe
  ScreenIntPoint scrollableLocation(40, 60);

  // Start the wheel transaction on a nonscrollable parent frame.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  Wheel(manager, nonscrollableLocation, downwardDelta, mcc->Time());
  EXPECT_EQ(0, childMetrics.GetVisualScrollOffset().y);

  // Mouse moves to a scrollable subframe. This should end the transaction.
  mcc->AdvanceByMillis(100);
  MouseInput mouseInput(MouseInput::MOUSE_MOVE,
                        MouseInput::ButtonType::PRIMARY_BUTTON, 0, 0,
                        scrollableLocation, mcc->Time(), 0);
  WidgetMouseEvent mouseEvent = mouseInput.ToWidgetEvent(nullptr);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  ((APZInputBridge*)manager.get())->ReceiveInputEvent(mouseEvent);

  // Wheel downward should scroll the subframe.
  mcc->AdvanceByMillis(100);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  Wheel(manager, scrollableLocation, downwardDelta, mcc->Time());
  EXPECT_GT(childMetrics.GetVisualScrollOffset().y, 0);
}

TEST_F(APZScrollHandoffTesterMock, ChildCloseToEndOfScrollRange) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  CreateScrollHandoffLayerTree1();

  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  FrameMetrics& rootMetrics = rootApzc->GetFrameMetrics();
  FrameMetrics& childMetrics = childApzc->GetFrameMetrics();

  // Zoom the page in by 3x. This needs to be reflected in the zoom level
  // and composition bounds of both APZCs.
  rootMetrics.SetZoom(CSSToParentLayerScale(3.0));
  rootMetrics.SetCompositionBounds(ParentLayerRect(0, 0, 300, 300));
  childMetrics.SetZoom(CSSToParentLayerScale(3.0));
  childMetrics.SetCompositionBounds(ParentLayerRect(0, 150, 300, 150));

  // Scroll the child APZC very close to the end of the scroll range.
  // The scroll offset is chosen such that in CSS pixels it has 0.01 pixels
  // room to scroll (less than COORDINATE_EPSILON = 0.02), but in ParentLayer
  // pixels it has 0.03 pixels room (greater than COORDINATE_EPSILON).
  childMetrics.SetVisualScrollOffset(CSSPoint(0, 49.99));

  EXPECT_FALSE(childApzc->IsOverscrolled());

  CSSPoint childBefore = childApzc->GetFrameMetrics().GetVisualScrollOffset();
  CSSPoint parentBefore = rootApzc->GetFrameMetrics().GetVisualScrollOffset();

  // Synthesize a pan gesture that tries to scroll the child further down.
  PanGesture(PanGestureInput::PANGESTURE_START, childApzc,
             ScreenIntPoint(10, 20), ScreenPoint(0, 40), mcc->Time());
  mcc->AdvanceByMillis(5);
  childApzc->AdvanceAnimations(mcc->GetSampleTime());

  PanGesture(PanGestureInput::PANGESTURE_END, childApzc, ScreenIntPoint(10, 21),
             ScreenPoint(0, 0), mcc->Time());

  CSSPoint childAfter = childApzc->GetFrameMetrics().GetVisualScrollOffset();
  CSSPoint parentAfter = rootApzc->GetFrameMetrics().GetVisualScrollOffset();

  bool childScrolled = (childBefore != childAfter);
  bool parentScrolled = (parentBefore != parentAfter);

  // Check that either the child or the parent scrolled.
  // (With the current implementation of comparing quantities to
  // COORDINATE_EPSILON in CSS units, it will be the parent, but the important
  // thing is that at least one of the child or parent scroll, i.e. we're not
  // stuck in a situation where no scroll offset is changing).
  EXPECT_TRUE(childScrolled || parentScrolled);
}

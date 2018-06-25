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
    const char* layerTreeSyntax = "c(t)";
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0, 0, 100, 100)),
      nsIntRegion(IntRect(0, 50, 100, 50))
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 200, 200));
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 100, 100));
    SetScrollHandoff(layers[1], root);
    registration = MakeUnique<ScopedLayerTreeRegistration>(manager, LayersId{0}, root, mcc);
    manager->UpdateHitTestingTree(LayersId{0}, root, false, LayersId{0}, 0);
    rootApzc = ApzcOf(root);
    rootApzc->GetFrameMetrics().SetIsRootContent(true);  // make root APZC zoomable
  }

  void CreateScrollHandoffLayerTree2() {
    const char* layerTreeSyntax = "c(c(t))";
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0, 0, 100, 100)),
      nsIntRegion(IntRect(0, 0, 100, 100)),
      nsIntRegion(IntRect(0, 50, 100, 50))
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 200, 200));
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 2, CSSRect(-100, -100, 200, 200));
    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 100, 100));
    SetScrollHandoff(layers[1], root);
    SetScrollHandoff(layers[2], layers[1]);
    // No ScopedLayerTreeRegistration as that just needs to be done once per test
    // and this is the second layer tree for a particular test.
    MOZ_ASSERT(registration);
    manager->UpdateHitTestingTree(LayersId{0}, root, false, LayersId{0}, 0);
    rootApzc = ApzcOf(root);
  }

  void CreateScrollHandoffLayerTree3() {
    const char* layerTreeSyntax = "c(c(t)c(t))";
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0, 0, 100, 100)),  // root
      nsIntRegion(IntRect(0, 0, 100, 50)),   // scrolling parent 1
      nsIntRegion(IntRect(0, 0, 100, 50)),   // scrolling child 1
      nsIntRegion(IntRect(0, 50, 100, 50)),  // scrolling parent 2
      nsIntRegion(IntRect(0, 50, 100, 50))   // scrolling child 2
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(layers[0], FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 100, 100));
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 100, 100));
    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 2, CSSRect(0, 0, 100, 100));
    SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 3, CSSRect(0, 50, 100, 100));
    SetScrollableFrameMetrics(layers[4], FrameMetrics::START_SCROLL_ID + 4, CSSRect(0, 50, 100, 100));
    SetScrollHandoff(layers[1], layers[0]);
    SetScrollHandoff(layers[3], layers[0]);
    SetScrollHandoff(layers[2], layers[1]);
    SetScrollHandoff(layers[4], layers[3]);
    registration = MakeUnique<ScopedLayerTreeRegistration>(manager, LayersId{0}, root, mcc);
    manager->UpdateHitTestingTree(LayersId{0}, root, false, LayersId{0}, 0);
  }

  // Creates a layer tree with a parent layer that is only scrollable
  // horizontally, and a child layer that is only scrollable vertically.
  void CreateScrollHandoffLayerTree4() {
    const char* layerTreeSyntax = "c(t)";
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0, 0, 100, 100)),
      nsIntRegion(IntRect(0, 0, 100, 100))
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 200, 100));
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 100, 200));
    SetScrollHandoff(layers[1], root);
    registration = MakeUnique<ScopedLayerTreeRegistration>(manager, LayersId{0}, root, mcc);
    manager->UpdateHitTestingTree(LayersId{0}, root, false, LayersId{0}, 0);
    rootApzc = ApzcOf(root);
  }

  void CreateScrollgrabLayerTree(bool makeParentScrollable = true) {
    const char* layerTreeSyntax = "c(t)";
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0, 0, 100, 100)),  // scroll-grabbing parent
      nsIntRegion(IntRect(0, 20, 100, 80))   // child
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    float parentHeight = makeParentScrollable ? 120 : 100;
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 100, parentHeight));
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 100, 200));
    SetScrollHandoff(layers[1], root);
    registration = MakeUnique<ScopedLayerTreeRegistration>(manager, LayersId{0}, root, mcc);
    manager->UpdateHitTestingTree(LayersId{0}, root, false, LayersId{0}, 0);
    rootApzc = ApzcOf(root);
    rootApzc->GetScrollMetadata().SetHasScrollgrab(true);
  }

  void TestFlingAcceleration() {
    // Jack up the fling acceleration multiplier so we can easily determine
    // whether acceleration occured.
    const float kAcceleration = 100.0f;
    SCOPED_GFX_PREF(APZFlingAccelBaseMultiplier, float, kAcceleration);
    SCOPED_GFX_PREF(APZFlingAccelMinVelocity, float, 0.0);

    RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

    // Pan once, enough to fully scroll the scrollgrab parent and then scroll
    // and fling the child.
    Pan(manager, 70, 40);

    // Give the fling animation a chance to start.
    SampleAnimationsOnce();

    float childVelocityAfterFling1 = childApzc->GetVelocityVector().y;

    // Pan again.
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
    SCOPED_GFX_PREF(APZAxisLockMode, int32_t, 1);

    CreateScrollHandoffLayerTree1();

    RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);
    Pan(childApzc, ScreenIntPoint(10, 60), ScreenIntPoint(15, 90),
        PanOptions::KeepFingerDown | PanOptions::ExactCoordinates);

    childApzc->AssertAxisLocked(ScrollDirection::eVertical);
  }
};

// Here we test that if the processing of a touch block is deferred while we
// wait for content to send a prevent-default message, overscroll is still
// handed off correctly when the block is processed.
TEST_F(APZScrollHandoffTester, DeferredInputEventProcessing) {
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
  EXPECT_EQ(50, childApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(10, rootApzc->GetFrameMetrics().GetScrollOffset().y);
}

// Here we test that if the layer structure changes in between two input
// blocks being queued, and the first block is only processed after the second
// one has been queued, overscroll handoff for the first block follows
// the original layer structure while overscroll handoff for the second block
// follows the new layer structure.
TEST_F(APZScrollHandoffTester, LayerStructureChangesWhileEventsArePending) {
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
  RefPtr<Layer> middle = layers[1];
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
  EXPECT_EQ(50, childApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(10, rootApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(0, middleApzc->GetFrameMetrics().GetScrollOffset().y);

  // Allow the second pan to be processed.
  childApzc->ContentReceivedInputBlock(secondBlockId, false);
  childApzc->ConfirmTarget(secondBlockId);

  // Make sure things have scrolled according to the handoff chain in
  // place at the time the touch-start of the second pan was queued.
  EXPECT_EQ(0, childApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(10, rootApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(-10, middleApzc->GetFrameMetrics().GetScrollOffset().y);
}

// Test that putting a second finger down on an APZC while a down-chain APZC
// is overscrolled doesn't result in being stuck in overscroll.
TEST_F(APZScrollHandoffTester, StuckInOverscroll_Bug1073250) {
  // Enable overscrolling.
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);
  SCOPED_GFX_PREF(APZFlingMinVelocityThreshold, float, 0.0f);
  SCOPED_GFX_VAR(UseWebRender, bool, false);

  CreateScrollHandoffLayerTree1();

  TestAsyncPanZoomController* child = ApzcOf(layers[1]);

  // Pan, causing the parent APZC to overscroll.
  Pan(manager, 10, 40, PanOptions::KeepFingerDown);
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_TRUE(rootApzc->IsOverscrolled());

  // Put a second finger down.
  MultiTouchInput secondFingerDown(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0);
  // Use the same touch identifier for the first touch (0) as Pan(). (A bit hacky.)
  secondFingerDown.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, 40), ScreenSize(0, 0), 0, 0));
  secondFingerDown.mTouches.AppendElement(SingleTouchData(1, ScreenIntPoint(30, 20), ScreenSize(0, 0), 0, 0));
  manager->ReceiveInputEvent(secondFingerDown, nullptr, nullptr);

  // Release the fingers.
  MultiTouchInput fingersUp = secondFingerDown;
  fingersUp.mType = MultiTouchInput::MULTITOUCH_END;
  manager->ReceiveInputEvent(fingersUp, nullptr, nullptr);

  // Allow any animations to run their course.
  child->AdvanceAnimationsUntilEnd();
  rootApzc->AdvanceAnimationsUntilEnd();

  // Make sure nothing is overscrolled.
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_FALSE(rootApzc->IsOverscrolled());
}

// This is almost exactly like StuckInOverscroll_Bug1073250, except the
// APZC receiving the input events for the first touch block is the child
// (and thus not the same APZC that overscrolls, which is the parent).
TEST_F(APZScrollHandoffTester, StuckInOverscroll_Bug1231228) {
  // Enable overscrolling.
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);
  SCOPED_GFX_PREF(APZFlingMinVelocityThreshold, float, 0.0f);
  SCOPED_GFX_VAR(UseWebRender, bool, false);

  CreateScrollHandoffLayerTree1();

  TestAsyncPanZoomController* child = ApzcOf(layers[1]);

  // Pan, causing the parent APZC to overscroll.
  Pan(manager, 60, 90, PanOptions::KeepFingerDown);
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_TRUE(rootApzc->IsOverscrolled());

  // Put a second finger down.
  MultiTouchInput secondFingerDown(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0);
  // Use the same touch identifier for the first touch (0) as Pan(). (A bit hacky.)
  secondFingerDown.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, 40), ScreenSize(0, 0), 0, 0));
  secondFingerDown.mTouches.AppendElement(SingleTouchData(1, ScreenIntPoint(30, 20), ScreenSize(0, 0), 0, 0));
  manager->ReceiveInputEvent(secondFingerDown, nullptr, nullptr);

  // Release the fingers.
  MultiTouchInput fingersUp = secondFingerDown;
  fingersUp.mType = MultiTouchInput::MULTITOUCH_END;
  manager->ReceiveInputEvent(fingersUp, nullptr, nullptr);

  // Allow any animations to run their course.
  child->AdvanceAnimationsUntilEnd();
  rootApzc->AdvanceAnimationsUntilEnd();

  // Make sure nothing is overscrolled.
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_FALSE(rootApzc->IsOverscrolled());
}

TEST_F(APZScrollHandoffTester, StuckInOverscroll_Bug1240202a) {
  // Enable overscrolling.
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

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

TEST_F(APZScrollHandoffTester, StuckInOverscroll_Bug1240202b) {
  // Enable overscrolling.
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);
  SCOPED_GFX_VAR(UseWebRender, bool, false);

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

  // Put a second finger down. Since we're in the TOUCHING state,
  // the "are we panned into overscroll" check will fail and we
  // will not ignore the second finger, instead entering the
  // PINCHING state.
  MultiTouchInput secondFingerDown(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0);
  // Use the same touch identifier for the first touch (0) as TouchDown(). (A bit hacky.)
  secondFingerDown.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, 90), ScreenSize(0, 0), 0, 0));
  secondFingerDown.mTouches.AppendElement(SingleTouchData(1, ScreenIntPoint(10, 80), ScreenSize(0, 0), 0, 0));
  manager->ReceiveInputEvent(secondFingerDown, nullptr, nullptr);

  // Release the fingers.
  MultiTouchInput fingersUp = secondFingerDown;
  fingersUp.mType = MultiTouchInput::MULTITOUCH_END;
  manager->ReceiveInputEvent(fingersUp, nullptr, nullptr);

  // Allow any animations to run their course.
  child->AdvanceAnimationsUntilEnd();
  rootApzc->AdvanceAnimationsUntilEnd();

  // Make sure nothing is overscrolled.
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_FALSE(rootApzc->IsOverscrolled());
}

TEST_F(APZScrollHandoffTester, OpposingConstrainedAxes_Bug1201098) {
  // Enable overscrolling.
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  CreateScrollHandoffLayerTree4();

  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  // Pan, causing the child APZC to overscroll.
  Pan(childApzc, 50, 60);

  //Make sure only the child is overscrolled.
  EXPECT_TRUE(childApzc->IsOverscrolled());
  EXPECT_FALSE(rootApzc->IsOverscrolled());
}

// Test that flinging in a direction where one component of the fling goes into
// overscroll but the other doesn't, results in just the one component being
// handed off to the parent, while the original APZC continues flinging in the
// other direction.
TEST_F(APZScrollHandoffTester, PartialFlingHandoff) {
  SCOPED_GFX_VAR(UseWebRender, bool, false);

  CreateScrollHandoffLayerTree1();

  // Fling up and to the left. The child APZC has room to scroll up, but not
  // to the left, so the horizontal component of the fling should be handed
  // off to the parent APZC.
  Pan(manager, ScreenIntPoint(90, 90), ScreenIntPoint(55, 55));

  RefPtr<TestAsyncPanZoomController> parent = ApzcOf(root);
  RefPtr<TestAsyncPanZoomController> child = ApzcOf(layers[1]);

  // Advance the child's fling animation once to give the partial handoff
  // a chance to occur.
  mcc->AdvanceByMillis(10);
  child->AdvanceAnimations(mcc->Time());

  // Assert that partial handoff has occurred.
  child->AssertStateIsFling();
  parent->AssertStateIsFling();
}

// Here we test that if two flings are happening simultaneously, overscroll
// is handed off correctly for each.
TEST_F(APZScrollHandoffTester, SimultaneousFlings) {
  SCOPED_GFX_PREF(APZFlingMinVelocityThreshold, float, 0.0f);

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

TEST_F(APZScrollHandoffTester, Scrollgrab) {
  // Set up the layer tree
  CreateScrollgrabLayerTree();

  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  // Pan on the child, enough to fully scroll the scrollgrab parent (20 px)
  // and leave some more (another 15 px) for the child.
  Pan(childApzc, 80, 45);

  // Check that the parent and child have scrolled as much as we expect.
  EXPECT_EQ(20, rootApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(15, childApzc->GetFrameMetrics().GetScrollOffset().y);
}

TEST_F(APZScrollHandoffTester, ScrollgrabFling) {
  SCOPED_GFX_PREF(APZFlingMinVelocityThreshold, float, 0.0f);
  // Set up the layer tree
  CreateScrollgrabLayerTree();

  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  // Pan on the child, not enough to fully scroll the scrollgrab parent.
  Pan(childApzc, 80, 70);

  // Check that it is the scrollgrab parent that's in a fling, not the child.
  rootApzc->AssertStateIsFling();
  childApzc->AssertStateIsReset();
}

TEST_F(APZScrollHandoffTester, ScrollgrabFlingAcceleration1) {
  SCOPED_GFX_PREF(APZFlingMinVelocityThreshold, float, 0.0f);
  SCOPED_GFX_VAR(UseWebRender, bool, false);
  CreateScrollgrabLayerTree(true /* make parent scrollable */);
  TestFlingAcceleration();
}

TEST_F(APZScrollHandoffTester, ScrollgrabFlingAcceleration2) {
  SCOPED_GFX_PREF(APZFlingMinVelocityThreshold, float, 0.0f);
  SCOPED_GFX_VAR(UseWebRender, bool, false);
  CreateScrollgrabLayerTree(false /* do not make parent scrollable */);
  TestFlingAcceleration();
}

TEST_F(APZScrollHandoffTester, ImmediateHandoffDisallowed_Pan) {
  SCOPED_GFX_PREF(APZAllowImmediateHandoff, bool, false);

  CreateScrollHandoffLayerTree1();

  RefPtr<TestAsyncPanZoomController> parentApzc = ApzcOf(root);
  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  // Pan on the child, enough to scroll it to its end and have scroll
  // left to hand off. Since immediate handoff is disallowed, we expect
  // the leftover scroll not to be handed off.
  Pan(childApzc, 60, 5);

  // Verify that the parent has not scrolled.
  EXPECT_EQ(50, childApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(0, parentApzc->GetFrameMetrics().GetScrollOffset().y);

  // Pan again on the child. This time, since the child was scrolled to
  // its end when the gesture began, we expect the scroll to be handed off.
  Pan(childApzc, 60, 50);

  // Verify that the parent scrolled.
  EXPECT_EQ(10, parentApzc->GetFrameMetrics().GetScrollOffset().y);
}

TEST_F(APZScrollHandoffTester, ImmediateHandoffDisallowed_Fling) {
  SCOPED_GFX_PREF(APZAllowImmediateHandoff, bool, false);
  SCOPED_GFX_PREF(APZFlingMinVelocityThreshold, float, 0.0f);

  CreateScrollHandoffLayerTree1();

  RefPtr<TestAsyncPanZoomController> parentApzc = ApzcOf(root);
  RefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  // Pan on the child, enough to get very close to the end, so that the
  // subsequent fling reaches the end and has leftover velocity to hand off.
  Pan(childApzc, 60, 12);

  // Allow the fling to run its course.
  childApzc->AdvanceAnimationsUntilEnd();
  parentApzc->AdvanceAnimationsUntilEnd();

  // Verify that the parent has not scrolled.
  // The first comparison needs to be an ASSERT_NEAR because the fling
  // computations are such that the final scroll position can be within
  // COORDINATE_EPSILON of the end rather than right at the end.
  ASSERT_NEAR(50, childApzc->GetFrameMetrics().GetScrollOffset().y, COORDINATE_EPSILON);
  EXPECT_EQ(0, parentApzc->GetFrameMetrics().GetScrollOffset().y);

  // Pan again on the child. This time, since the child was scrolled to
  // its end when the gesture began, we expect the scroll to be handed off.
  Pan(childApzc, 60, 50);

  // Allow the fling to run its course. The fling should also be handed off.
  childApzc->AdvanceAnimationsUntilEnd();
  parentApzc->AdvanceAnimationsUntilEnd();

  // Verify that the parent scrolled from the fling.
  EXPECT_GT(parentApzc->GetFrameMetrics().GetScrollOffset().y, 10);
}

TEST_F(APZScrollHandoffTester, CrossApzcAxisLock_NoTouchAction) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);
  TestCrossApzcAxisLock();
}

TEST_F(APZScrollHandoffTester, CrossApzcAxisLock_TouchAction) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  TestCrossApzcAxisLock();
}

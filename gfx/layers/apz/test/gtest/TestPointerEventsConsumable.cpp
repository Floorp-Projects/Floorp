/* -*- Mode: C+; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "InputUtils.h"
#include "apz/src/AsyncPanZoomController.h"
#include "apz/src/InputBlockState.h"
#include "apz/src/OverscrollHandoffState.h"
#include "mozilla/layers/IAPZCTreeManager.h"

class APZCArePointerEventsConsumable : public APZCTreeManagerTester {
 public:
  APZCArePointerEventsConsumable() { CreateMockHitTester(); }

  void CreateSingleElementTree() {
    const char* treeShape = "x";
    LayerIntRect layerVisibleRect[] = {
        LayerIntRect(0, 0, 100, 100),
    };
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 500, 500));

    registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);

    UpdateHitTestingTree();

    ApzcOf(root)->GetFrameMetrics().SetIsRootContent(true);
  }

  void CreateScrollHandoffTree() {
    const char* treeShape = "x(x)";
    LayerIntRect layerVisibleRect[] = {LayerIntRect(0, 0, 200, 200),
                                       LayerIntRect(50, 50, 100, 100)};
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 300, 300));
    SetScrollableFrameMetrics(layers[1],
                              ScrollableLayerGuid::START_SCROLL_ID + 1,
                              CSSRect(0, 0, 200, 200));
    SetScrollHandoff(layers[1], root);
    registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
    UpdateHitTestingTree();

    ApzcOf(root)->GetFrameMetrics().SetIsRootContent(true);
  }

  RefPtr<TouchBlockState> CreateTouchBlockStateForApzc(
      const RefPtr<TestAsyncPanZoomController>& aApzc) {
    TouchCounter counter{};
    TargetConfirmationFlags flags{true};

    return new TouchBlockState(aApzc, flags, counter);
  }

  void UpdateOverscrollBehavior(ScrollableLayerGuid::ViewID aScrollId,
                                OverscrollBehavior aX, OverscrollBehavior aY) {
    auto* layer = layers[aScrollId - ScrollableLayerGuid::START_SCROLL_ID];
    ModifyFrameMetrics(layer, [aX, aY](ScrollMetadata& sm, FrameMetrics& _) {
      OverscrollBehaviorInfo overscroll;
      overscroll.mBehaviorX = aX;
      overscroll.mBehaviorY = aY;
      sm.SetOverscrollBehavior(overscroll);
    });
    UpdateHitTestingTree();
  }

  UniquePtr<ScopedLayerTreeRegistration> registration;
};

TEST_F(APZCArePointerEventsConsumable, EmptyInput) {
  CreateSingleElementTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);
  RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);

  MultiTouchInput touchInput =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());

  const PointerEventsConsumableFlags expected{false, false};
  const PointerEventsConsumableFlags actual =
      apzc->ArePointerEventsConsumable(blockState, touchInput);
  EXPECT_EQ(expected, actual);
}

TEST_F(APZCArePointerEventsConsumable, ScrollHorizontally) {
  CreateSingleElementTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);
  RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);

  // Create touch with horizontal 20 unit scroll
  MultiTouchInput touchStart =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  touchStart.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(10, 10), ScreenSize(0, 0), 0, 0));

  MultiTouchInput touchMove =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  touchMove.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(30, 10), ScreenSize(0, 0), 0, 0));

  blockState->UpdateSlopState(touchStart, false);

  PointerEventsConsumableFlags actual{};
  PointerEventsConsumableFlags expected{};

  // Scroll area 500x500, room to pan x, room to pan y
  expected = {true, true};
  actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);

  // Scroll area 100x100, no room to pan x, no room to pan y
  apzc->GetFrameMetrics().SetScrollableRect(CSSRect{0, 0, 100, 100});
  expected = {false, true};
  actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);

  // Scroll area 500x100, room to pan x, no room to pan y
  apzc->GetFrameMetrics().SetScrollableRect(CSSRect{0, 0, 500, 100});
  expected = {true, true};
  actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);

  // Scroll area 100x500, no room to pan x, room to pan y
  apzc->GetFrameMetrics().SetScrollableRect(CSSRect{0, 0, 100, 500});
  expected = {false, true};
  actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);
}

TEST_F(APZCArePointerEventsConsumable, ScrollVertically) {
  CreateSingleElementTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);
  RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);

  // Create touch with vertical 20 unit scroll
  MultiTouchInput touchStart =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  touchStart.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(10, 10), ScreenSize(0, 0), 0, 0));

  MultiTouchInput touchMove =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  touchMove.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(10, 30), ScreenSize(0, 0), 0, 0));

  blockState->UpdateSlopState(touchStart, false);

  PointerEventsConsumableFlags actual{};
  PointerEventsConsumableFlags expected{};

  // Scroll area 500x500, room to pan x, room to pan y
  expected = {true, true};
  actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);

  // Scroll area 100x100, no room to pan x, no room to pan y
  apzc->GetFrameMetrics().SetScrollableRect(CSSRect{0, 0, 100, 100});
  expected = {false, true};
  actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);

  // Scroll area 500x100, room to pan x, no room to pan y
  apzc->GetFrameMetrics().SetScrollableRect(CSSRect{0, 0, 500, 100});
  expected = {false, true};
  actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);

  // Scroll area 100x500, no room to pan x, room to pan y
  apzc->GetFrameMetrics().SetScrollableRect(CSSRect{0, 0, 100, 500});
  expected = {true, true};
  actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);
}

TEST_F(APZCArePointerEventsConsumable, NestedElementCanScroll) {
  CreateScrollHandoffTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(layers[1]);
  RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);

  // Create touch with vertical 20 unit scroll
  MultiTouchInput touchStart =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  touchStart.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 60), ScreenSize(0, 0), 0, 0));

  MultiTouchInput touchMove =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  touchMove.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 80), ScreenSize(0, 0), 0, 0));

  blockState->UpdateSlopState(touchStart, false);

  const PointerEventsConsumableFlags expected{true, true};
  const PointerEventsConsumableFlags actual =
      apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);
}

TEST_F(APZCArePointerEventsConsumable, NestedElementCannotScroll) {
  CreateScrollHandoffTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(layers[1]);
  RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);

  // Create touch with vertical 20 unit scroll
  MultiTouchInput touchStart =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  touchStart.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 60), ScreenSize(0, 0), 0, 0));

  MultiTouchInput touchMove =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  touchMove.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 80), ScreenSize(0, 0), 0, 0));

  blockState->UpdateSlopState(touchStart, false);

  PointerEventsConsumableFlags actual{};
  PointerEventsConsumableFlags expected{};

  // Set the nested element to have no room to scroll.
  // Because of the overscroll handoff, we still have room to scroll
  // in the parent element.
  apzc->GetFrameMetrics().SetScrollableRect(CSSRect{0, 0, 100, 100});
  expected = {true, true};
  actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);

  // Set overscroll handoff for the nested element to none.
  // Because no handoff will happen, we are not able to use the parent's
  // room to scroll.
  // Bug 1814886: Once fixed, change expected value to {false, true}.
  UpdateOverscrollBehavior(ScrollableLayerGuid::START_SCROLL_ID + 1,
                           OverscrollBehavior::None, OverscrollBehavior::None);
  expected = {true, true};
  actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);
}

TEST_F(APZCArePointerEventsConsumable, NotScrollableButZoomable) {
  CreateSingleElementTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);
  RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);

  // Create touch with vertical 20 unit scroll
  MultiTouchInput touchStart =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  touchStart.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 60), ScreenSize(0, 0), 0, 0));

  MultiTouchInput touchMove =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  touchMove.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 80), ScreenSize(0, 0), 0, 0));

  blockState->UpdateSlopState(touchStart, false);

  // Make the root have no room to scroll
  apzc->GetFrameMetrics().SetScrollableRect(CSSRect{0, 0, 100, 100});

  // Make zoomable
  apzc->UpdateZoomConstraints(ZoomConstraints(
      true, true, CSSToParentLayerScale(0.25f), CSSToParentLayerScale(4.0f)));

  PointerEventsConsumableFlags actual{};
  PointerEventsConsumableFlags expected{};

  expected = {false, true};
  actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);

  // Add a second touch point and therefore make the APZC consider
  // zoom use cases as well.
  touchMove.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 90), ScreenSize(0, 0), 0, 0));

  expected = {true, true};
  actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);
}

TEST_F(APZCArePointerEventsConsumable, TouchActionsProhibitAll) {
  CreateSingleElementTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  // Create touch with vertical 20 unit scroll
  MultiTouchInput touchStart =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  touchStart.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 60), ScreenSize(0, 0), 0, 0));

  MultiTouchInput touchMove =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  touchMove.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 80), ScreenSize(0, 0), 0, 0));

  PointerEventsConsumableFlags expected{};
  PointerEventsConsumableFlags actual{};

  {
    RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);
    blockState->UpdateSlopState(touchStart, false);

    blockState->SetAllowedTouchBehaviors({AllowedTouchBehavior::NONE});
    expected = {true, false};
    actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
    EXPECT_EQ(expected, actual);
  }

  // Convert touch input to two-finger pinch
  touchStart.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(80, 80), ScreenSize(0, 0), 0, 0));
  touchMove.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(90, 90), ScreenSize(0, 0), 0, 0));

  {
    RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);
    blockState->UpdateSlopState(touchStart, false);

    blockState->SetAllowedTouchBehaviors({AllowedTouchBehavior::NONE});
    expected = {true, false};
    actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(APZCArePointerEventsConsumable, TouchActionsAllowVerticalScrolling) {
  CreateSingleElementTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  // Create touch with vertical 20 unit scroll
  MultiTouchInput touchStart =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  touchStart.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 60), ScreenSize(0, 0), 0, 0));

  MultiTouchInput touchMove =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  touchMove.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 80), ScreenSize(0, 0), 0, 0));

  PointerEventsConsumableFlags expected{};
  PointerEventsConsumableFlags actual{};

  {
    RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);
    blockState->UpdateSlopState(touchStart, false);

    blockState->SetAllowedTouchBehaviors({AllowedTouchBehavior::NONE});
    expected = {true, false};
    actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
    EXPECT_EQ(expected, actual);
  }

  {
    RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);
    blockState->UpdateSlopState(touchStart, false);

    blockState->SetAllowedTouchBehaviors({AllowedTouchBehavior::VERTICAL_PAN});
    expected = {true, true};
    actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(APZCArePointerEventsConsumable, TouchActionsAllowHorizontalScrolling) {
  CreateSingleElementTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  // Create touch with horizontal 20 unit scroll
  MultiTouchInput touchStart =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  touchStart.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 60), ScreenSize(0, 0), 0, 0));

  MultiTouchInput touchMove =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  touchMove.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(80, 60), ScreenSize(0, 0), 0, 0));

  PointerEventsConsumableFlags expected{};
  PointerEventsConsumableFlags actual{};

  {
    RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);
    blockState->UpdateSlopState(touchStart, false);

    blockState->SetAllowedTouchBehaviors({AllowedTouchBehavior::NONE});
    expected = {true, false};
    actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
    EXPECT_EQ(expected, actual);
  }

  {
    RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);
    blockState->UpdateSlopState(touchStart, false);

    blockState->SetAllowedTouchBehaviors(
        {AllowedTouchBehavior::HORIZONTAL_PAN});
    expected = {true, true};
    actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(APZCArePointerEventsConsumable, TouchActionsAllowPinchZoom) {
  CreateSingleElementTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  // Create two-finger pinch
  MultiTouchInput touchStart =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  touchStart.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 60), ScreenSize(0, 0), 0, 0));
  touchStart.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(80, 80), ScreenSize(0, 0), 0, 0));

  MultiTouchInput touchMove =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  touchMove.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(50, 50), ScreenSize(0, 0), 0, 0));
  touchMove.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(90, 90), ScreenSize(0, 0), 0, 0));

  PointerEventsConsumableFlags expected{};
  PointerEventsConsumableFlags actual{};

  {
    RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);
    blockState->UpdateSlopState(touchStart, false);

    blockState->SetAllowedTouchBehaviors({AllowedTouchBehavior::NONE});
    expected = {true, false};
    actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
    EXPECT_EQ(expected, actual);
  }

  {
    RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);
    blockState->UpdateSlopState(touchStart, false);

    blockState->SetAllowedTouchBehaviors({AllowedTouchBehavior::PINCH_ZOOM});
    expected = {true, true};
    actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(APZCArePointerEventsConsumable, DynamicToolbar) {
  CreateSingleElementTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);
  RefPtr<TouchBlockState> blockState = CreateTouchBlockStateForApzc(apzc);

  // Create touch with vertical 20 unit scroll
  MultiTouchInput touchStart =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  touchStart.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 30), ScreenSize(0, 0), 0, 0));

  MultiTouchInput touchMove =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  touchMove.mTouches.AppendElement(
      SingleTouchData(0, ScreenIntPoint(60, 40), ScreenSize(0, 0), 0, 0));

  blockState->UpdateSlopState(touchStart, false);

  // Restrict size of scrollable area: No room to pan X, no room to pan Y
  apzc->GetFrameMetrics().SetScrollableRect(CSSRect{0, 0, 100, 100});

  PointerEventsConsumableFlags actual{};
  PointerEventsConsumableFlags expected{};

  expected = {false, true};
  actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);

  apzc->GetFrameMetrics().SetCompositionSizeWithoutDynamicToolbar(
      ParentLayerSize{100, 90});
  UpdateHitTestingTree();

  expected = {true, true};
  actual = apzc->ArePointerEventsConsumable(blockState, touchMove);
  EXPECT_EQ(expected, actual);
}

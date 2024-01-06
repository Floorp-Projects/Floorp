/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "InputUtils.h"
#include "mozilla/layers/LayersTypes.h"

class APZEventRegionsTester : public APZCTreeManagerTester {
 protected:
  UniquePtr<ScopedLayerTreeRegistration> registration;
  TestAsyncPanZoomController* rootApzc;

  void CreateEventRegionsLayerTree1() {
    const char* treeShape = "x(xx)";
    LayerIntRect layerVisibleRects[] = {
        LayerIntRect(0, 0, 200, 200),    // root
        LayerIntRect(0, 0, 100, 200),    // left half
        LayerIntRect(0, 100, 200, 100),  // bottom half
    };
    CreateScrollData(treeShape, layerVisibleRects);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[1],
                              ScrollableLayerGuid::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[2],
                              ScrollableLayerGuid::START_SCROLL_ID + 2);
    SetScrollHandoff(layers[1], root);
    SetScrollHandoff(layers[2], root);

    registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
    UpdateHitTestingTree();
    rootApzc = ApzcOf(root);
  }

  void CreateEventRegionsLayerTree2() {
    const char* treeShape = "x(x)";
    LayerIntRect layerVisibleRects[] = {
        LayerIntRect(0, 0, 100, 500),
        LayerIntRect(0, 150, 100, 100),
    };
    CreateScrollData(treeShape, layerVisibleRects);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID);

    registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
    UpdateHitTestingTree();
    rootApzc = ApzcOf(root);
  }

  void CreateBug1117712LayerTree() {
    const char* treeShape = "x(x(x)x)";
    // LayerID               0 1 2 3
    // 0 is the root
    // 1 is a container layer whose sole purpose to make a non-empty ancestor
    //   transform for 2, so that 2's screen-to-apzc and apzc-to-gecko
    //   transforms are different from 3's.
    // 2 is a small layer that is the actual target
    // 3 is a big layer obscuring 2 with a dispatch-to-content region
    LayerIntRect layerVisibleRects[] = {
        LayerIntRect(0, 0, 100, 100),
        LayerIntRect(0, 0, 0, 0),
        LayerIntRect(0, 0, 10, 10),
        LayerIntRect(0, 0, 100, 100),
    };
    Matrix4x4 layerTransforms[] = {
        Matrix4x4(),
        Matrix4x4::Translation(50, 0, 0),
        Matrix4x4(),
        Matrix4x4(),
    };
    CreateScrollData(treeShape, layerVisibleRects, layerTransforms);

    SetScrollableFrameMetrics(layers[2], ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 10, 10));
    SetScrollableFrameMetrics(layers[3],
                              ScrollableLayerGuid::START_SCROLL_ID + 1,
                              CSSRect(0, 0, 100, 100));
    SetScrollHandoff(layers[3], layers[2]);

    registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
    UpdateHitTestingTree();
  }
};

class APZEventRegionsTesterMock : public APZEventRegionsTester {
 public:
  APZEventRegionsTesterMock() { CreateMockHitTester(); }
};

TEST_F(APZEventRegionsTesterMock, HitRegionImmediateResponse) {
  CreateEventRegionsLayerTree1();

  TestAsyncPanZoomController* root = ApzcOf(layers[0]);
  TestAsyncPanZoomController* left = ApzcOf(layers[1]);
  TestAsyncPanZoomController* bottom = ApzcOf(layers[2]);

  MockFunction<void(std::string checkPointName)> check;
  {
    InSequence s;
    EXPECT_CALL(*mcc,
                HandleTap(TapType::eSingleTap, _, _, left->GetGuid(), _, _))
        .Times(1);
    EXPECT_CALL(check, Call("Tapped on left"));
    EXPECT_CALL(*mcc,
                HandleTap(TapType::eSingleTap, _, _, bottom->GetGuid(), _, _))
        .Times(1);
    EXPECT_CALL(check, Call("Tapped on bottom"));
    EXPECT_CALL(*mcc,
                HandleTap(TapType::eSingleTap, _, _, root->GetGuid(), _, _))
        .Times(1);
    EXPECT_CALL(check, Call("Tapped on root"));
    EXPECT_CALL(check, Call("Tap pending on d-t-c region"));
    EXPECT_CALL(*mcc,
                HandleTap(TapType::eSingleTap, _, _, bottom->GetGuid(), _, _))
        .Times(1);
    EXPECT_CALL(check, Call("Tapped on bottom again"));
    EXPECT_CALL(*mcc,
                HandleTap(TapType::eSingleTap, _, _, left->GetGuid(), _, _))
        .Times(1);
    EXPECT_CALL(check, Call("Tapped on left this time"));
  }

  TimeDuration tapDuration = TimeDuration::FromMilliseconds(100);

  // Tap in the exposed hit regions of each of the layers once and ensure
  // the clicks are dispatched right away
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  Tap(manager, ScreenIntPoint(10, 10), tapDuration);
  mcc->RunThroughDelayedTasks();  // this runs the tap event
  check.Call("Tapped on left");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 2);
  Tap(manager, ScreenIntPoint(110, 110), tapDuration);
  mcc->RunThroughDelayedTasks();  // this runs the tap event
  check.Call("Tapped on bottom");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  Tap(manager, ScreenIntPoint(110, 10), tapDuration);
  mcc->RunThroughDelayedTasks();  // this runs the tap event
  check.Call("Tapped on root");

  // Now tap on the dispatch-to-content region where the layers overlap
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 2,
                     {CompositorHitTestFlags::eVisibleToHitTest,
                      CompositorHitTestFlags::eIrregularArea});
  Tap(manager, ScreenIntPoint(10, 110), tapDuration);
  mcc->RunThroughDelayedTasks();  // this runs the main-thread timeout
  check.Call("Tap pending on d-t-c region");
  mcc->RunThroughDelayedTasks();  // this runs the tap event
  check.Call("Tapped on bottom again");

  // Now let's do that again, but simulate a main-thread response
  uint64_t inputBlockId = 0;
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 2,
                     {CompositorHitTestFlags::eVisibleToHitTest,
                      CompositorHitTestFlags::eIrregularArea});
  Tap(manager, ScreenIntPoint(10, 110), tapDuration, nullptr, &inputBlockId);
  nsTArray<ScrollableLayerGuid> targets;
  targets.AppendElement(left->GetGuid());
  manager->SetTargetAPZC(inputBlockId, targets);
  while (mcc->RunThroughDelayedTasks())
    ;  // this runs the tap event
  check.Call("Tapped on left this time");
}

TEST_F(APZEventRegionsTesterMock, HitRegionAccumulatesChildren) {
  CreateEventRegionsLayerTree2();

  // Tap in the area of the child layer that's not directly included in the
  // parent layer's hit region. Verify that it comes out of the APZC's
  // content controller, which indicates the input events got routed correctly
  // to the APZC.
  EXPECT_CALL(*mcc,
              HandleTap(TapType::eSingleTap, _, _, rootApzc->GetGuid(), _, _))
      .Times(1);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  Tap(manager, ScreenIntPoint(10, 160), TimeDuration::FromMilliseconds(100));
}

TEST_F(APZEventRegionsTesterMock, Bug1117712) {
  CreateBug1117712LayerTree();

  TestAsyncPanZoomController* apzc2 = ApzcOf(layers[2]);

  // These touch events should hit the dispatch-to-content region of layers[3]
  // and so get queued with that APZC as the tentative target.
  uint64_t inputBlockId = 0;
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1,
                     {CompositorHitTestFlags::eVisibleToHitTest,
                      CompositorHitTestFlags::eIrregularArea});
  Tap(manager, ScreenIntPoint(55, 5), TimeDuration::FromMilliseconds(100),
      nullptr, &inputBlockId);
  // But now we tell the APZ that really it hit layers[2], and expect the tap
  // to be delivered at the correct coordinates.
  EXPECT_CALL(*mcc, HandleTap(TapType::eSingleTap, LayoutDevicePoint(55, 5), 0,
                              apzc2->GetGuid(), _, _))
      .Times(1);

  nsTArray<ScrollableLayerGuid> targets;
  targets.AppendElement(apzc2->GetGuid());
  manager->SetTargetAPZC(inputBlockId, targets);
}

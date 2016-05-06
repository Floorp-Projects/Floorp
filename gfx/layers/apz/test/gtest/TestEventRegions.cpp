/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "InputUtils.h"

class APZEventRegionsTester : public APZCTreeManagerTester {
protected:
  UniquePtr<ScopedLayerTreeRegistration> registration;
  TestAsyncPanZoomController* rootApzc;

  void CreateEventRegionsLayerTree1() {
    const char* layerTreeSyntax = "c(tt)";
    nsIntRegion layerVisibleRegions[] = {
      nsIntRegion(IntRect(0, 0, 200, 200)),     // root
      nsIntRegion(IntRect(0, 0, 100, 200)),     // left half
      nsIntRegion(IntRect(0, 100, 200, 100)),   // bottom half
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegions, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 2);
    SetScrollHandoff(layers[1], root);
    SetScrollHandoff(layers[2], root);

    // Set up the event regions over a 200x200 area. The root layer has the
    // whole 200x200 as the hit region; layers[1] has the left half and
    // layers[2] has the bottom half. The bottom-left 100x100 area is also
    // in the d-t-c region for both layers[1] and layers[2] (but layers[2] is
    // on top so it gets the events by default if the main thread doesn't
    // respond).
    EventRegions regions(nsIntRegion(IntRect(0, 0, 200, 200)));
    root->SetEventRegions(regions);
    regions.mDispatchToContentHitRegion = nsIntRegion(IntRect(0, 100, 100, 100));
    regions.mHitRegion = nsIntRegion(IntRect(0, 0, 100, 200));
    layers[1]->SetEventRegions(regions);
    regions.mHitRegion = nsIntRegion(IntRect(0, 100, 200, 100));
    layers[2]->SetEventRegions(regions);

    registration = MakeUnique<ScopedLayerTreeRegistration>(manager, 0, root, mcc);
    manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
    rootApzc = ApzcOf(root);
  }

  void CreateEventRegionsLayerTree2() {
    const char* layerTreeSyntax = "c(t)";
    nsIntRegion layerVisibleRegions[] = {
      nsIntRegion(IntRect(0, 0, 100, 500)),
      nsIntRegion(IntRect(0, 150, 100, 100)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegions, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID);

    // Set up the event regions so that the child thebes layer is positioned far
    // away from the scrolling container layer.
    EventRegions regions(nsIntRegion(IntRect(0, 0, 100, 100)));
    root->SetEventRegions(regions);
    regions.mHitRegion = nsIntRegion(IntRect(0, 150, 100, 100));
    layers[1]->SetEventRegions(regions);

    registration = MakeUnique<ScopedLayerTreeRegistration>(manager, 0, root, mcc);
    manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
    rootApzc = ApzcOf(root);
  }

  void CreateObscuringLayerTree() {
    const char* layerTreeSyntax = "c(c(t)t)";
    // LayerID                     0 1 2 3
    // 0 is the root.
    // 1 is a parent scrollable layer.
    // 2 is a child scrollable layer.
    // 3 is the Obscurer, who ruins everything.
    nsIntRegion layerVisibleRegions[] = {
        // x coordinates are uninteresting
        nsIntRegion(IntRect(0,   0, 200, 200)),  // [0, 200]
        nsIntRegion(IntRect(0,   0, 200, 200)),  // [0, 200]
        nsIntRegion(IntRect(0, 100, 200,  50)),  // [100, 150]
        nsIntRegion(IntRect(0, 100, 200, 100))   // [100, 200]
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegions, nullptr, lm, layers);

    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 200, 200));
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 200, 300));
    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 2, CSSRect(0, 0, 200, 100));
    SetScrollHandoff(layers[2], layers[1]);
    SetScrollHandoff(layers[1], root);

    EventRegions regions(nsIntRegion(IntRect(0, 0, 200, 200)));
    root->SetEventRegions(regions);
    regions.mHitRegion = nsIntRegion(IntRect(0, 0, 200, 300));
    layers[1]->SetEventRegions(regions);
    regions.mHitRegion = nsIntRegion(IntRect(0, 100, 200, 100));
    layers[2]->SetEventRegions(regions);

    registration = MakeUnique<ScopedLayerTreeRegistration>(manager, 0, root, mcc);
    manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
    rootApzc = ApzcOf(root);
  }

  void CreateBug1119497LayerTree() {
    const char* layerTreeSyntax = "c(tt)";
    // LayerID                     0 12
    // 0 is the root and has an APZC
    // 1 is behind 2 and has an APZC
    // 2 entirely covers 1 and should take all the input events, but has no APZC
    // so hits to 2 should go to to the root APZC
    nsIntRegion layerVisibleRegions[] = {
      nsIntRegion(IntRect(0, 0, 100, 100)),
      nsIntRegion(IntRect(0, 0, 100, 100)),
      nsIntRegion(IntRect(0, 0, 100, 100)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegions, nullptr, lm, layers);

    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1);

    registration = MakeUnique<ScopedLayerTreeRegistration>(manager, 0, root, mcc);
    manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
  }

  void CreateBug1117712LayerTree() {
    const char* layerTreeSyntax = "c(c(t)t)";
    // LayerID                     0 1 2 3
    // 0 is the root
    // 1 is a container layer whose sole purpose to make a non-empty ancestor
    //   transform for 2, so that 2's screen-to-apzc and apzc-to-gecko
    //   transforms are different from 3's.
    // 2 is a small layer that is the actual target
    // 3 is a big layer obscuring 2 with a dispatch-to-content region
    nsIntRegion layerVisibleRegions[] = {
      nsIntRegion(IntRect(0, 0, 100, 100)),
      nsIntRegion(IntRect(0, 0, 0, 0)),
      nsIntRegion(IntRect(0, 0, 10, 10)),
      nsIntRegion(IntRect(0, 0, 100, 100)),
    };
    Matrix4x4 layerTransforms[] = {
      Matrix4x4(),
      Matrix4x4::Translation(50, 0, 0),
      Matrix4x4(),
      Matrix4x4(),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegions, layerTransforms, lm, layers);

    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 10, 10));
    SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 2, CSSRect(0, 0, 100, 100));

    EventRegions regions(nsIntRegion(IntRect(0, 0, 10, 10)));
    layers[2]->SetEventRegions(regions);
    regions.mHitRegion = nsIntRegion(IntRect(0, 0, 100, 100));
    regions.mDispatchToContentHitRegion = nsIntRegion(IntRect(0, 0, 100, 100));
    layers[3]->SetEventRegions(regions);

    registration = MakeUnique<ScopedLayerTreeRegistration>(manager, 0, root, mcc);
    manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
  }
};

TEST_F(APZEventRegionsTester, HitRegionImmediateResponse) {
  CreateEventRegionsLayerTree1();

  TestAsyncPanZoomController* root = ApzcOf(layers[0]);
  TestAsyncPanZoomController* left = ApzcOf(layers[1]);
  TestAsyncPanZoomController* bottom = ApzcOf(layers[2]);

  MockFunction<void(std::string checkPointName)> check;
  {
    InSequence s;
    EXPECT_CALL(*mcc, HandleSingleTap(_, _, left->GetGuid())).Times(1);
    EXPECT_CALL(check, Call("Tapped on left"));
    EXPECT_CALL(*mcc, HandleSingleTap(_, _, bottom->GetGuid())).Times(1);
    EXPECT_CALL(check, Call("Tapped on bottom"));
    EXPECT_CALL(*mcc, HandleSingleTap(_, _, root->GetGuid())).Times(1);
    EXPECT_CALL(check, Call("Tapped on root"));
    EXPECT_CALL(check, Call("Tap pending on d-t-c region"));
    EXPECT_CALL(*mcc, HandleSingleTap(_, _, bottom->GetGuid())).Times(1);
    EXPECT_CALL(check, Call("Tapped on bottom again"));
    EXPECT_CALL(*mcc, HandleSingleTap(_, _, left->GetGuid())).Times(1);
    EXPECT_CALL(check, Call("Tapped on left this time"));
  }

  TimeDuration tapDuration = TimeDuration::FromMilliseconds(100);

  // Tap in the exposed hit regions of each of the layers once and ensure
  // the clicks are dispatched right away
  Tap(manager, ScreenIntPoint(10, 10), tapDuration);
  mcc->RunThroughDelayedTasks();    // this runs the tap event
  check.Call("Tapped on left");
  Tap(manager, ScreenIntPoint(110, 110), tapDuration);
  mcc->RunThroughDelayedTasks();    // this runs the tap event
  check.Call("Tapped on bottom");
  Tap(manager, ScreenIntPoint(110, 10), tapDuration);
  mcc->RunThroughDelayedTasks();    // this runs the tap event
  check.Call("Tapped on root");

  // Now tap on the dispatch-to-content region where the layers overlap
  Tap(manager, ScreenIntPoint(10, 110), tapDuration);
  mcc->RunThroughDelayedTasks();    // this runs the main-thread timeout
  check.Call("Tap pending on d-t-c region");
  mcc->RunThroughDelayedTasks();    // this runs the tap event
  check.Call("Tapped on bottom again");

  // Now let's do that again, but simulate a main-thread response
  uint64_t inputBlockId = 0;
  Tap(manager, ScreenIntPoint(10, 110), tapDuration, nullptr, &inputBlockId);
  nsTArray<ScrollableLayerGuid> targets;
  targets.AppendElement(left->GetGuid());
  manager->SetTargetAPZC(inputBlockId, targets);
  while (mcc->RunThroughDelayedTasks());    // this runs the tap event
  check.Call("Tapped on left this time");
}

TEST_F(APZEventRegionsTester, HitRegionAccumulatesChildren) {
  CreateEventRegionsLayerTree2();

  // Tap in the area of the child layer that's not directly included in the
  // parent layer's hit region. Verify that it comes out of the APZC's
  // content controller, which indicates the input events got routed correctly
  // to the APZC.
  EXPECT_CALL(*mcc, HandleSingleTap(_, _, rootApzc->GetGuid())).Times(1);
  Tap(manager, ScreenIntPoint(10, 160), TimeDuration::FromMilliseconds(100));
}

TEST_F(APZEventRegionsTester, Obscuration) {
  CreateObscuringLayerTree();
  ScopedLayerTreeRegistration registration(manager, 0, root, mcc);

  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);

  TestAsyncPanZoomController* parent = ApzcOf(layers[1]);
  TestAsyncPanZoomController* child = ApzcOf(layers[2]);

  ApzcPanNoFling(parent, 75, 25);

  HitTestResult result;
  RefPtr<AsyncPanZoomController> hit = manager->GetTargetAPZC(ScreenPoint(50, 75), &result);
  EXPECT_EQ(child, hit.get());
  EXPECT_EQ(HitTestResult::HitLayer, result);
}

TEST_F(APZEventRegionsTester, Bug1119497) {
  CreateBug1119497LayerTree();

  HitTestResult result;
  RefPtr<AsyncPanZoomController> hit = manager->GetTargetAPZC(ScreenPoint(50, 50), &result);
  // We should hit layers[2], so |result| will be HitLayer but there's no
  // actual APZC on layers[2], so it will be the APZC of the root layer.
  EXPECT_EQ(ApzcOf(layers[0]), hit.get());
  EXPECT_EQ(HitTestResult::HitLayer, result);
}

TEST_F(APZEventRegionsTester, Bug1117712) {
  CreateBug1117712LayerTree();

  TestAsyncPanZoomController* apzc2 = ApzcOf(layers[2]);

  // These touch events should hit the dispatch-to-content region of layers[3]
  // and so get queued with that APZC as the tentative target.
  uint64_t inputBlockId = 0;
  Tap(manager, ScreenIntPoint(55, 5), TimeDuration::FromMilliseconds(100), nullptr, &inputBlockId);
  // But now we tell the APZ that really it hit layers[2], and expect the tap
  // to be delivered at the correct coordinates.
  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(55, 5), 0, apzc2->GetGuid())).Times(1);

  nsTArray<ScrollableLayerGuid> targets;
  targets.AppendElement(apzc2->GetGuid());
  manager->SetTargetAPZC(inputBlockId, targets);
}

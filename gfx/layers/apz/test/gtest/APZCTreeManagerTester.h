/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZCTreeManagerTester_h
#define mozilla_layers_APZCTreeManagerTester_h

/**
 * Defines a test fixture used for testing multiple APZCs interacting in
 * an APZCTreeManager.
 */

#include "APZTestCommon.h"

class APZCTreeManagerTester : public ::testing::Test {
protected:
  virtual void SetUp() {
    gfxPrefs::GetSingleton();
    APZThreadUtils::SetThreadAssertionsEnabled(false);
    APZThreadUtils::SetControllerThread(MessageLoop::current());

    mcc = new NiceMock<MockContentControllerDelayed>();
    manager = new TestAPZCTreeManager(mcc);
  }

  virtual void TearDown() {
    while (mcc->RunThroughDelayedTasks());
    manager->ClearTree();
  }

  /**
   * Sample animations once for all APZCs, 1 ms later than the last sample.
   */
  void SampleAnimationsOnce() {
    const TimeDuration increment = TimeDuration::FromMilliseconds(1);
    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;
    mcc->AdvanceBy(increment);

    for (const RefPtr<Layer>& layer : layers) {
      if (TestAsyncPanZoomController* apzc = ApzcOf(layer)) {
        apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
      }
    }
  }

  RefPtr<MockContentControllerDelayed> mcc;

  nsTArray<RefPtr<Layer> > layers;
  RefPtr<LayerManager> lm;
  RefPtr<Layer> root;

  RefPtr<TestAPZCTreeManager> manager;

protected:
  static void SetScrollableFrameMetrics(Layer* aLayer, FrameMetrics::ViewID aScrollId,
                                        CSSRect aScrollableRect = CSSRect(-1, -1, -1, -1)) {
    FrameMetrics metrics;
    metrics.SetScrollId(aScrollId);
    // By convention in this test file, START_SCROLL_ID is the root, so mark it as such.
    if (aScrollId == FrameMetrics::START_SCROLL_ID) {
      metrics.SetIsLayersIdRoot(true);
    }
    IntRect layerBound = aLayer->GetVisibleRegion().ToUnknownRegion().GetBounds();
    metrics.SetCompositionBounds(ParentLayerRect(layerBound.x, layerBound.y,
                                                 layerBound.width, layerBound.height));
    metrics.SetScrollableRect(aScrollableRect);
    metrics.SetScrollOffset(CSSPoint(0, 0));
    metrics.SetPageScrollAmount(LayoutDeviceIntSize(50, 100));
    metrics.SetAllowVerticalScrollWithWheel(true);
    aLayer->SetFrameMetrics(metrics);
    aLayer->SetClipRect(Some(ViewAs<ParentLayerPixel>(layerBound)));
    if (!aScrollableRect.IsEqualEdges(CSSRect(-1, -1, -1, -1))) {
      // The purpose of this is to roughly mimic what layout would do in the
      // case of a scrollable frame with the event regions and clip. This lets
      // us exercise the hit-testing code in APZCTreeManager
      EventRegions er = aLayer->GetEventRegions();
      IntRect scrollRect = RoundedToInt(aScrollableRect * metrics.LayersPixelsPerCSSPixel()).ToUnknownRect();
      er.mHitRegion = nsIntRegion(IntRect(layerBound.TopLeft(), scrollRect.Size()));
      aLayer->SetEventRegions(er);
    }
  }

  void SetScrollHandoff(Layer* aChild, Layer* aParent) {
    FrameMetrics metrics = aChild->GetFrameMetrics(0);
    metrics.SetScrollParentId(aParent->GetFrameMetrics(0).GetScrollId());
    aChild->SetFrameMetrics(metrics);
  }

  static TestAsyncPanZoomController* ApzcOf(Layer* aLayer) {
    EXPECT_EQ(1u, aLayer->GetFrameMetricsCount());
    return (TestAsyncPanZoomController*)aLayer->GetAsyncPanZoomController(0);
  }

  void CreateSimpleScrollingLayer() {
    const char* layerTreeSyntax = "t";
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0,0,200,200)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 500, 500));
  }

  void CreateSimpleDTCScrollingLayer() {
    const char* layerTreeSyntax = "t";
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0,0,200,200)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 500, 500));

    EventRegions regions;
    regions.mHitRegion = nsIntRegion(IntRect(0, 0, 200, 200));
    regions.mDispatchToContentHitRegion = regions.mHitRegion;
    layers[0]->SetEventRegions(regions);
  }

  void CreateSimpleMultiLayerTree() {
    const char* layerTreeSyntax = "c(tt)";
    // LayerID                     0 12
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0,0,100,100)),
      nsIntRegion(IntRect(0,0,100,50)),
      nsIntRegion(IntRect(0,50,100,50)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
  }

  void CreatePotentiallyLeakingTree() {
    const char* layerTreeSyntax = "c(c(c(t))c(c(t)))";
    // LayerID                     0 1 2 3  4 5 6
    root = CreateLayerTree(layerTreeSyntax, nullptr, nullptr, lm, layers);
    SetScrollableFrameMetrics(layers[0], FrameMetrics::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[5], FrameMetrics::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 2);
    SetScrollableFrameMetrics(layers[6], FrameMetrics::START_SCROLL_ID + 3);
  }

  void CreateBug1194876Tree() {
    const char* layerTreeSyntax = "c(t)";
    // LayerID                     0 1
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0,0,100,100)),
      nsIntRegion(IntRect(0,0,100,100)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(layers[0], FrameMetrics::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1);

    // Make layers[1] the root content
    FrameMetrics childMetrics = layers[1]->GetFrameMetrics(0);
    childMetrics.SetIsRootContent(true);
    layers[1]->SetFrameMetrics(childMetrics);

    // Both layers are fully dispatch-to-content
    EventRegions regions;
    regions.mHitRegion = nsIntRegion(IntRect(0, 0, 100, 100));
    regions.mDispatchToContentHitRegion = regions.mHitRegion;
    layers[0]->SetEventRegions(regions);
    layers[1]->SetEventRegions(regions);
  }
};

#endif // mozilla_layers_APZCTreeManagerTester_h

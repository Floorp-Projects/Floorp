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
#include "gfxPlatform.h"

class APZCTreeManagerTester : public APZCTesterBase {
protected:
  virtual void SetUp() {
    gfxPrefs::GetSingleton();
    gfxPlatform::GetPlatform();
    APZThreadUtils::SetThreadAssertionsEnabled(false);
    APZThreadUtils::SetControllerThread(MessageLoop::current());

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

  nsTArray<RefPtr<Layer> > layers;
  RefPtr<LayerManager> lm;
  RefPtr<Layer> root;

  RefPtr<TestAPZCTreeManager> manager;

protected:
  static ScrollMetadata BuildScrollMetadata(FrameMetrics::ViewID aScrollId,
                                            const CSSRect& aScrollableRect,
                                            const ParentLayerRect& aCompositionBounds)
  {
    ScrollMetadata metadata;
    FrameMetrics& metrics = metadata.GetMetrics();
    metrics.SetScrollId(aScrollId);
    // By convention in this test file, START_SCROLL_ID is the root, so mark it as such.
    if (aScrollId == FrameMetrics::START_SCROLL_ID) {
      metadata.SetIsLayersIdRoot(true);
    }
    metrics.SetCompositionBounds(aCompositionBounds);
    metrics.SetScrollableRect(aScrollableRect);
    metrics.SetScrollOffset(CSSPoint(0, 0));
    metadata.SetPageScrollAmount(LayoutDeviceIntSize(50, 100));
    metadata.SetLineScrollAmount(LayoutDeviceIntSize(5, 10));
    metadata.SetAllowVerticalScrollWithWheel(true);
    return metadata;
  }

  static void SetEventRegionsBasedOnBottommostMetrics(Layer* aLayer)
  {
    const FrameMetrics& metrics = aLayer->GetScrollMetadata(0).GetMetrics();
    CSSRect scrollableRect = metrics.GetScrollableRect();
    if (!scrollableRect.IsEqualEdges(CSSRect(-1, -1, -1, -1))) {
      // The purpose of this is to roughly mimic what layout would do in the
      // case of a scrollable frame with the event regions and clip. This lets
      // us exercise the hit-testing code in APZCTreeManager
      EventRegions er = aLayer->GetEventRegions();
      IntRect scrollRect = RoundedToInt(
          scrollableRect * metrics.LayersPixelsPerCSSPixel()).ToUnknownRect();
      er.mHitRegion = nsIntRegion(IntRect(
          RoundedToInt(metrics.GetCompositionBounds().TopLeft().ToUnknownPoint()),
          scrollRect.Size()));
      aLayer->SetEventRegions(er);
    }
  }

  static void SetScrollableFrameMetrics(Layer* aLayer, FrameMetrics::ViewID aScrollId,
                                        CSSRect aScrollableRect = CSSRect(-1, -1, -1, -1)) {
    ParentLayerIntRect compositionBounds = ViewAs<ParentLayerPixel>(
        aLayer->GetVisibleRegion().ToUnknownRegion().GetBounds());
    ScrollMetadata metadata = BuildScrollMetadata(aScrollId, aScrollableRect,
        ParentLayerRect(compositionBounds));
    aLayer->SetScrollMetadata(metadata);
    aLayer->SetClipRect(Some(compositionBounds));
    SetEventRegionsBasedOnBottommostMetrics(aLayer);
  }

  void SetScrollHandoff(Layer* aChild, Layer* aParent) {
    ScrollMetadata metadata = aChild->GetScrollMetadata(0);
    metadata.SetScrollParentId(aParent->GetFrameMetrics(0).GetScrollId());
    aChild->SetScrollMetadata(metadata);
  }

  static TestAsyncPanZoomController* ApzcOf(Layer* aLayer) {
    EXPECT_EQ(1u, aLayer->GetScrollMetadataCount());
    return (TestAsyncPanZoomController*)aLayer->GetAsyncPanZoomController(0);
  }

  static TestAsyncPanZoomController* ApzcOf(Layer* aLayer, uint32_t aIndex) {
    EXPECT_LT(aIndex, aLayer->GetScrollMetadataCount());
    return (TestAsyncPanZoomController*)aLayer->GetAsyncPanZoomController(aIndex);
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
    ScrollMetadata childMetadata = layers[1]->GetScrollMetadata(0);
    childMetadata.GetMetrics().SetIsRootContent(true);
    layers[1]->SetScrollMetadata(childMetadata);

    // Both layers are fully dispatch-to-content
    EventRegions regions;
    regions.mHitRegion = nsIntRegion(IntRect(0, 0, 100, 100));
    regions.mDispatchToContentHitRegion = regions.mHitRegion;
    layers[0]->SetEventRegions(regions);
    layers[1]->SetEventRegions(regions);
  }
};

#endif // mozilla_layers_APZCTreeManagerTester_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

#include "mozilla/layers/APZSampler.h"
#include "mozilla/layers/APZUpdater.h"

class APZCTreeManagerTester : public APZCTesterBase {
 protected:
  virtual void SetUp() {
    gfxPlatform::GetPlatform();
    APZThreadUtils::SetThreadAssertionsEnabled(false);
    APZThreadUtils::SetControllerThread(NS_GetCurrentThread());

    manager = new TestAPZCTreeManager(mcc);
    updater = new APZUpdater(manager, false);
    sampler = new APZSampler(manager, false);
  }

  virtual void TearDown() {
    while (mcc->RunThroughDelayedTasks())
      ;
    manager->ClearTree();
    manager->ClearContentController();
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

  // A convenience function for letting a test modify the frame metrics
  // stored on a particular layer. The layer doesn't let us modify it in-place,
  // so we take care of the copying in this function.
  template <typename Callback>
  void ModifyFrameMetrics(Layer* aLayer, Callback aCallback) {
    ScrollMetadata metadata = aLayer->GetScrollMetadata(0);
    aCallback(metadata.GetMetrics());
    aLayer->SetScrollMetadata(metadata);
  }

  // A convenience wrapper for manager->UpdateHitTestingTree().
  void UpdateHitTestingTree(uint32_t aPaintSequenceNumber = 0) {
    manager->UpdateHitTestingTree(root, /* is first paint = */ false,
                                  LayersId{0}, aPaintSequenceNumber);
  }

  nsTArray<RefPtr<Layer> > layers;
  RefPtr<LayerManager> lm;
  RefPtr<Layer> root;

  RefPtr<TestAPZCTreeManager> manager;
  RefPtr<APZSampler> sampler;
  RefPtr<APZUpdater> updater;

 protected:
  static ScrollMetadata BuildScrollMetadata(
      ScrollableLayerGuid::ViewID aScrollId, const CSSRect& aScrollableRect,
      const ParentLayerRect& aCompositionBounds) {
    ScrollMetadata metadata;
    FrameMetrics& metrics = metadata.GetMetrics();
    metrics.SetScrollId(aScrollId);
    // By convention in this test file, START_SCROLL_ID is the root, so mark it
    // as such.
    if (aScrollId == ScrollableLayerGuid::START_SCROLL_ID) {
      metadata.SetIsLayersIdRoot(true);
    }
    metrics.SetCompositionBounds(aCompositionBounds);
    metrics.SetScrollableRect(aScrollableRect);
    metrics.SetScrollOffset(CSSPoint(0, 0));
    metadata.SetPageScrollAmount(LayoutDeviceIntSize(50, 100));
    metadata.SetLineScrollAmount(LayoutDeviceIntSize(5, 10));
    return metadata;
  }

  static void SetEventRegionsBasedOnBottommostMetrics(Layer* aLayer) {
    const FrameMetrics& metrics = aLayer->GetScrollMetadata(0).GetMetrics();
    CSSRect scrollableRect = metrics.GetScrollableRect();
    if (!scrollableRect.IsEqualEdges(CSSRect(-1, -1, -1, -1))) {
      // The purpose of this is to roughly mimic what layout would do in the
      // case of a scrollable frame with the event regions and clip. This lets
      // us exercise the hit-testing code in APZCTreeManager
      EventRegions er = aLayer->GetEventRegions();
      IntRect scrollRect =
          RoundedToInt(scrollableRect * metrics.LayersPixelsPerCSSPixel())
              .ToUnknownRect();
      er.mHitRegion = nsIntRegion(IntRect(
          RoundedToInt(
              metrics.GetCompositionBounds().TopLeft().ToUnknownPoint()),
          scrollRect.Size()));
      aLayer->SetEventRegions(er);
    }
  }

  static void SetScrollableFrameMetrics(
      Layer* aLayer, ScrollableLayerGuid::ViewID aScrollId,
      CSSRect aScrollableRect = CSSRect(-1, -1, -1, -1)) {
    ParentLayerIntRect compositionBounds =
        RoundedToInt(aLayer->GetLocalTransformTyped().TransformBounds(
            LayerRect(aLayer->GetVisibleRegion().GetBounds())));
    ScrollMetadata metadata = BuildScrollMetadata(
        aScrollId, aScrollableRect, ParentLayerRect(compositionBounds));
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
    return (TestAsyncPanZoomController*)aLayer->GetAsyncPanZoomController(
        aIndex);
  }

  void CreateSimpleScrollingLayer() {
    const char* layerTreeSyntax = "t";
    nsIntRegion layerVisibleRegion[] = {
        nsIntRegion(IntRect(0, 0, 200, 200)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm,
                           layers);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 500, 500));
  }
};

#endif  // mozilla_layers_APZCTreeManagerTester_h

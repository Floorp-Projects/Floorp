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

#include "APZTestAccess.h"
#include "APZTestCommon.h"
#include "gfxPlatform.h"
#include "MockHitTester.h"
#include "apz/src/WRHitTester.h"

#include "mozilla/layers/APZSampler.h"
#include "mozilla/layers/APZUpdater.h"
#include "mozilla/layers/WebRenderScrollDataWrapper.h"

class APZCTreeManagerTester : public APZCTesterBase {
 protected:
  APZCTreeManagerTester() : mHitTester(MakeUnique<WRHitTester>()) {}

  virtual void SetUp() {
    APZCTesterBase::SetUp();

    APZThreadUtils::SetThreadAssertionsEnabled(false);
    APZThreadUtils::SetControllerThread(NS_GetCurrentThread());

    manager = new TestAPZCTreeManager(mcc, std::move(mHitTester));
    updater = new APZUpdater(manager, false);
    sampler = new APZSampler(manager, false);
  }

  virtual void TearDown() {
    while (mcc->RunThroughDelayedTasks())
      ;
    manager->ClearTree();
    manager->ClearContentController();

    APZCTesterBase::TearDown();
  }

  /**
   * Sample animations once for all APZCs, 1 ms later than the last sample and
   * return whether there is still any active animations or not.
   */
  bool SampleAnimationsOnce() {
    const TimeDuration increment = TimeDuration::FromMilliseconds(1);
    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;
    mcc->AdvanceBy(increment);

    bool activeAnimations = false;

    for (size_t i = 0; i < layers.GetLayerCount(); ++i) {
      if (TestAsyncPanZoomController* apzc = ApzcOf(layers[i])) {
        activeAnimations |=
            apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
      }
    }

    return activeAnimations;
  }

  // A convenience function for letting a test modify the frame metrics
  // stored on a particular layer.
  template <typename Callback>
  void ModifyFrameMetrics(WebRenderLayerScrollData* aLayer,
                          Callback aCallback) {
    MOZ_ASSERT(aLayer->GetScrollMetadataCount() == 1);
    ScrollMetadata& metadataRef =
        APZTestAccess::GetScrollMetadataMut(*aLayer, layers, 0);
    aCallback(metadataRef, metadataRef.GetMetrics());
  }

  // A convenience wrapper for manager->UpdateHitTestingTree().
  void UpdateHitTestingTree(uint32_t aPaintSequenceNumber = 0) {
    manager->UpdateHitTestingTree(WebRenderScrollDataWrapper{*updater, &layers},
                                  /* is first paint = */ false, LayersId{0},
                                  aPaintSequenceNumber);
  }

  void CreateScrollData(const char* aTreeShape,
                        const LayerIntRect* aVisibleRects = nullptr,
                        const gfx::Matrix4x4* aTransforms = nullptr) {
    layers = TestWRScrollData::Create(aTreeShape, *updater, aVisibleRects,
                                      aTransforms);
    root = layers[0];
  }

  void CreateMockHitTester() {
    mHitTester = MakeUnique<MockHitTester>();
    // Save a pointer in a separate variable, because SetUp() will
    // move the value out of mHitTester.
    mMockHitTester = static_cast<MockHitTester*>(mHitTester.get());
  }
  void QueueMockHitResult(ScrollableLayerGuid::ViewID aScrollId,
                          gfx::CompositorHitTestInfo aHitInfo =
                              gfx::CompositorHitTestFlags::eVisibleToHitTest) {
    MOZ_ASSERT(mMockHitTester);
    mMockHitTester->QueueHitResult(aScrollId, aHitInfo);
  }

  RefPtr<TestAPZCTreeManager> manager;
  RefPtr<APZSampler> sampler;
  RefPtr<APZUpdater> updater;
  TestWRScrollData layers;
  WebRenderLayerScrollData* root = nullptr;

  UniquePtr<IAPZHitTester> mHitTester;
  MockHitTester* mMockHitTester = nullptr;

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
    metrics.SetLayoutScrollOffset(CSSPoint(0, 0));
    metadata.SetPageScrollAmount(LayoutDeviceIntSize(50, 100));
    metadata.SetLineScrollAmount(LayoutDeviceIntSize(5, 10));
    return metadata;
  }

  void SetScrollMetadata(WebRenderLayerScrollData* aLayer,
                         const ScrollMetadata& aMetadata) {
    MOZ_ASSERT(aLayer->GetScrollMetadataCount() <= 1,
               "This function does not support multiple ScrollMetadata on a "
               "single layer");
    if (aLayer->GetScrollMetadataCount() == 0) {
      // Add new metrics
      aLayer->AppendScrollMetadata(layers, aMetadata);
    } else {
      // Overwrite existing metrics
      ModifyFrameMetrics(
          aLayer, [&](ScrollMetadata& aSm, FrameMetrics&) { aSm = aMetadata; });
    }
  }

  void SetScrollMetadata(WebRenderLayerScrollData* aLayer,
                         const nsTArray<ScrollMetadata>& aMetadata) {
    // The reason for this restriction is that WebRenderLayerScrollData does not
    // have an API to *remove* previous metadata.
    MOZ_ASSERT(aLayer->GetScrollMetadataCount() == 0,
               "This function can only be used on layers which do not yet have "
               "scroll metadata");
    for (const ScrollMetadata& metadata : aMetadata) {
      aLayer->AppendScrollMetadata(layers, metadata);
    }
  }

  void SetScrollableFrameMetrics(WebRenderLayerScrollData* aLayer,
                                 ScrollableLayerGuid::ViewID aScrollId,
                                 CSSRect aScrollableRect = CSSRect(-1, -1, -1,
                                                                   -1)) {
    auto localTransform = aLayer->GetTransformTyped() * AsyncTransformMatrix();
    ParentLayerIntRect compositionBounds = RoundedToInt(
        localTransform.TransformBounds(LayerRect(aLayer->GetVisibleRect())));
    ScrollMetadata metadata = BuildScrollMetadata(
        aScrollId, aScrollableRect, ParentLayerRect(compositionBounds));
    SetScrollMetadata(aLayer, metadata);
  }

  bool HasScrollableFrameMetrics(const WebRenderLayerScrollData* aLayer) const {
    for (uint32_t i = 0; i < aLayer->GetScrollMetadataCount(); i++) {
      if (aLayer->GetScrollMetadata(layers, i).GetMetrics().IsScrollable()) {
        return true;
      }
    }
    return false;
  }

  void SetScrollHandoff(WebRenderLayerScrollData* aChild,
                        WebRenderLayerScrollData* aParent) {
    ModifyFrameMetrics(aChild, [&](ScrollMetadata& aSm, FrameMetrics&) {
      aSm.SetScrollParentId(
          aParent->GetScrollMetadata(layers, 0).GetMetrics().GetScrollId());
    });
  }

  TestAsyncPanZoomController* ApzcOf(WebRenderLayerScrollData* aLayer) {
    EXPECT_EQ(1u, aLayer->GetScrollMetadataCount());
    return ApzcOf(aLayer, 0);
  }

  TestAsyncPanZoomController* ApzcOf(WebRenderLayerScrollData* aLayer,
                                     uint32_t aIndex) {
    EXPECT_LT(aIndex, aLayer->GetScrollMetadataCount());
    // Unlike Layer, WebRenderLayerScrollData does not store the associated
    // APZCs, so look it up using the tree manager instead.
    RefPtr<AsyncPanZoomController> apzc = manager->GetTargetAPZC(
        LayersId{0},
        aLayer->GetScrollMetadata(layers, aIndex).GetMetrics().GetScrollId());
    return (TestAsyncPanZoomController*)apzc.get();
  }

  void CreateSimpleScrollingLayer() {
    const char* treeShape = "x";
    LayerIntRect layerVisibleRect[] = {
        LayerIntRect(0, 0, 200, 200),
    };
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(layers[0], ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 500, 500));
  }
};

#endif  // mozilla_layers_APZCTreeManagerTester_h

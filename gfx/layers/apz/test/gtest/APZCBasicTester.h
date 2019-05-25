/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZCBasicTester_h
#define mozilla_layers_APZCBasicTester_h

/**
 * Defines a test fixture used for testing a single APZC.
 */

#include "APZTestCommon.h"
#include "gfxPrefs.h"
#include "mozilla/layers/APZSampler.h"
#include "mozilla/layers/APZUpdater.h"

class APZCBasicTester : public APZCTesterBase {
 public:
  explicit APZCBasicTester(
      AsyncPanZoomController::GestureBehavior aGestureBehavior =
          AsyncPanZoomController::DEFAULT_GESTURES)
      : mGestureBehavior(aGestureBehavior) {}

 protected:
  virtual void SetUp() {
    gfxPrefs::GetSingleton();
    APZThreadUtils::SetThreadAssertionsEnabled(false);
    APZThreadUtils::SetControllerThread(MessageLoop::current());

    tm = new TestAPZCTreeManager(mcc);
    updater = new APZUpdater(tm, false);
    sampler = new APZSampler(tm, false);
    apzc = new TestAsyncPanZoomController(
        LayersId{0}, mcc, tm, wr::RenderRoot::Default, mGestureBehavior);
    apzc->SetFrameMetrics(TestFrameMetrics());
    apzc->GetScrollMetadata().SetIsLayersIdRoot(true);
  }

  /**
   * Get the APZC's scroll range in CSS pixels.
   */
  CSSRect GetScrollRange() const {
    const FrameMetrics& metrics = apzc->GetFrameMetrics();
    return CSSRect(metrics.GetScrollableRect().TopLeft(),
                   metrics.GetScrollableRect().Size() -
                       metrics.CalculateCompositedSizeInCssPixels());
  }

  virtual void TearDown() {
    while (mcc->RunThroughDelayedTasks())
      ;
    apzc->Destroy();
    tm->ClearTree();
    tm->ClearContentController();
  }

  void MakeApzcWaitForMainThread() { apzc->SetWaitForMainThread(); }

  void MakeApzcZoomable() {
    apzc->UpdateZoomConstraints(ZoomConstraints(
        true, true, CSSToParentLayerScale(0.25f), CSSToParentLayerScale(4.0f)));
  }

  void MakeApzcUnzoomable() {
    apzc->UpdateZoomConstraints(ZoomConstraints(false, false,
                                                CSSToParentLayerScale(1.0f),
                                                CSSToParentLayerScale(1.0f)));
  }

  void PanIntoOverscroll();

  /**
   * Sample animations once, 1 ms later than the last sample.
   */
  void SampleAnimationOnce() {
    const TimeDuration increment = TimeDuration::FromMilliseconds(1);
    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;
    mcc->AdvanceBy(increment);
    apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
  }

  /**
   * Sample animations until we recover from overscroll.
   * @param aExpectedScrollOffset the expected reported scroll offset
   *                              throughout the animation
   */
  void SampleAnimationUntilRecoveredFromOverscroll(
      const ParentLayerPoint& aExpectedScrollOffset) {
    const TimeDuration increment = TimeDuration::FromMilliseconds(1);
    bool recoveredFromOverscroll = false;
    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;
    while (apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut)) {
      // The reported scroll offset should be the same throughout.
      EXPECT_EQ(aExpectedScrollOffset, pointOut);

      // Trigger computation of the overscroll tranform, to make sure
      // no assetions fire during the calculation.
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);

      if (!apzc->IsOverscrolled()) {
        recoveredFromOverscroll = true;
      }

      mcc->AdvanceBy(increment);
    }
    EXPECT_TRUE(recoveredFromOverscroll);
    apzc->AssertStateIsReset();
  }

  void TestOverscroll();

  AsyncPanZoomController::GestureBehavior mGestureBehavior;
  RefPtr<TestAPZCTreeManager> tm;
  RefPtr<APZSampler> sampler;
  RefPtr<APZUpdater> updater;
  RefPtr<TestAsyncPanZoomController> apzc;
};

#endif  // mozilla_layers_APZCBasicTester_h

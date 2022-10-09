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
    APZCTesterBase::SetUp();
    APZThreadUtils::SetThreadAssertionsEnabled(false);
    APZThreadUtils::SetControllerThread(NS_GetCurrentThread());

    tm = new TestAPZCTreeManager(mcc);
    updater = new APZUpdater(tm, false);
    sampler = new APZSampler(tm, false);
    apzc =
        new TestAsyncPanZoomController(LayersId{0}, mcc, tm, mGestureBehavior);
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

    APZCTesterBase::TearDown();
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
   * Sample animations one frame, 17 ms later than the last sample.
   */
  void SampleAnimationOneFrame() {
    const TimeDuration increment = TimeDuration::FromMilliseconds(17);
    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;
    mcc->AdvanceBy(increment);
    apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
  }

  AsyncPanZoomController::GestureBehavior mGestureBehavior;
  RefPtr<TestAPZCTreeManager> tm;
  RefPtr<APZSampler> sampler;
  RefPtr<APZUpdater> updater;
  RefPtr<TestAsyncPanZoomController> apzc;
};

#endif  // mozilla_layers_APZCBasicTester_h

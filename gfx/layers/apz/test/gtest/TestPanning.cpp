/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCBasicTester.h"
#include "APZTestCommon.h"
#include "InputUtils.h"

class APZCPanningTester : public APZCBasicTester {
protected:
  void DoPanTest(bool aShouldTriggerScroll, bool aShouldBeConsumed, uint32_t aBehavior)
  {
    if (aShouldTriggerScroll) {
      // One repaint request for each pan.
      EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(2);
    } else {
      EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(0);
    }

    int touchStart = 50;
    int touchEnd = 10;
    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;

    nsTArray<uint32_t> allowedTouchBehaviors;
    allowedTouchBehaviors.AppendElement(aBehavior);

    // Pan down
    PanAndCheckStatus(apzc, touchStart, touchEnd, aShouldBeConsumed, &allowedTouchBehaviors);
    apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);

    if (aShouldTriggerScroll) {
      EXPECT_EQ(ParentLayerPoint(0, -(touchEnd-touchStart)), pointOut);
      EXPECT_NE(AsyncTransform(), viewTransformOut);
    } else {
      EXPECT_EQ(ParentLayerPoint(), pointOut);
      EXPECT_EQ(AsyncTransform(), viewTransformOut);
    }

    // Clear the fling from the previous pan, or stopping it will
    // consume the next touchstart
    apzc->CancelAnimation();

    // Pan back
    PanAndCheckStatus(apzc, touchEnd, touchStart, aShouldBeConsumed, &allowedTouchBehaviors);
    apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);

    EXPECT_EQ(ParentLayerPoint(), pointOut);
    EXPECT_EQ(AsyncTransform(), viewTransformOut);
  }

  void DoPanWithPreventDefaultTest()
  {
    MakeApzcWaitForMainThread();

    int touchStart = 50;
    int touchEnd = 10;
    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;
    uint64_t blockId = 0;

    // Pan down
    nsTArray<uint32_t> allowedTouchBehaviors;
    allowedTouchBehaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
    PanAndCheckStatus(apzc, touchStart, touchEnd, true, &allowedTouchBehaviors, &blockId);

    // Send the signal that content has handled and preventDefaulted the touch
    // events. This flushes the event queue.
    apzc->ContentReceivedInputBlock(blockId, true);

    apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
    EXPECT_EQ(ParentLayerPoint(), pointOut);
    EXPECT_EQ(AsyncTransform(), viewTransformOut);

    apzc->AssertStateIsReset();
  }
};

TEST_F(APZCPanningTester, Pan) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);
  SCOPED_GFX_PREF(APZVelocityBias, float, 0.0); // Velocity bias can cause extra repaint requests
  DoPanTest(true, true, mozilla::layers::AllowedTouchBehavior::NONE);
}

// In the each of the following 4 pan tests we are performing two pan gestures: vertical pan from top
// to bottom and back - from bottom to top.
// According to the pointer-events/touch-action spec AUTO and PAN_Y touch-action values allow vertical
// scrolling while NONE and PAN_X forbid it. The first parameter of DoPanTest method specifies this
// behavior.
// However, the events will be marked as consumed even if the behavior in PAN_X, because the user could
// move their finger horizontally too - APZ has no way of knowing beforehand and so must consume the
// events.
TEST_F(APZCPanningTester, PanWithTouchActionAuto) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  SCOPED_GFX_PREF(APZVelocityBias, float, 0.0); // Velocity bias can cause extra repaint requests
  DoPanTest(true, true, mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN
                      | mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
}

TEST_F(APZCPanningTester, PanWithTouchActionNone) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  SCOPED_GFX_PREF(APZVelocityBias, float, 0.0); // Velocity bias can cause extra repaint requests
  DoPanTest(false, false, 0);
}

TEST_F(APZCPanningTester, PanWithTouchActionPanX) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  SCOPED_GFX_PREF(APZVelocityBias, float, 0.0); // Velocity bias can cause extra repaint requests
  DoPanTest(false, true, mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN);
}

TEST_F(APZCPanningTester, PanWithTouchActionPanY) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  SCOPED_GFX_PREF(APZVelocityBias, float, 0.0); // Velocity bias can cause extra repaint requests
  DoPanTest(true, true, mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
}

TEST_F(APZCPanningTester, PanWithPreventDefaultAndTouchAction) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  DoPanWithPreventDefaultTest();
}

TEST_F(APZCPanningTester, PanWithPreventDefault) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);
  DoPanWithPreventDefaultTest();
}

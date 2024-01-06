/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <initializer_list>
#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "InputUtils.h"

class APZCFlingAccelerationTester : public APZCTreeManagerTester {
 protected:
  void SetUp() {
    APZCTreeManagerTester::SetUp();
    const char* treeShape = "x";
    LayerIntRect layerVisibleRect[] = {
        LayerIntRect(0, 0, 800, 1000),
    };
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 800, 50000));
    // Scroll somewhere into the middle of the scroll range, so that we have
    // lots of space to scroll in both directions.
    ModifyFrameMetrics(root, [](ScrollMetadata& aSm, FrameMetrics& aMetrics) {
      aMetrics.SetVisualScrollUpdateType(
          FrameMetrics::ScrollOffsetUpdateType::eMainThread);
      aMetrics.SetVisualDestination(CSSPoint(0, 25000));
    });

    registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
    UpdateHitTestingTree();

    apzc = ApzcOf(root);
  }

  void ExecutePanGesture100Hz(const ScreenIntPoint& aStartPoint,
                              std::initializer_list<int32_t> aYDeltas) {
    APZEventResult result = TouchDown(apzc, aStartPoint, mcc->Time());

    // Allowed touch behaviours must be set after sending touch-start.
    if (result.GetStatus() != nsEventStatus_eConsumeNoDefault) {
      SetDefaultAllowedTouchBehavior(apzc, result.mInputBlockId);
    }

    const TimeDuration kTouchTimeDelta100Hz =
        TimeDuration::FromMilliseconds(10);

    ScreenIntPoint currentLocation = aStartPoint;
    for (int32_t delta : aYDeltas) {
      mcc->AdvanceBy(kTouchTimeDelta100Hz);
      if (delta != 0) {
        currentLocation.y += delta;
        Unused << TouchMove(apzc, currentLocation, mcc->Time());
      }
    }

    Unused << TouchUp(apzc, currentLocation, mcc->Time());
  }

  void ExecuteWait(const TimeDuration& aDuration) {
    TimeDuration remaining = aDuration;
    const TimeDuration TIME_BETWEEN_FRAMES =
        TimeDuration::FromSeconds(1) / int64_t(60);
    while (remaining.ToMilliseconds() > 0) {
      mcc->AdvanceBy(TIME_BETWEEN_FRAMES);
      apzc->AdvanceAnimations(mcc->GetSampleTime());
      remaining -= TIME_BETWEEN_FRAMES;
    }
  }

  RefPtr<TestAsyncPanZoomController> apzc;
  UniquePtr<ScopedLayerTreeRegistration> registration;
};

enum class UpOrDown : uint8_t { Up, Down };

// This is a macro so that the assertions print useful line numbers.
#define CHECK_VELOCITY(aUpOrDown, aLowerBound, aUpperBound) \
  do {                                                      \
    auto vel = apzc->GetVelocityVector();                   \
    if (UpOrDown::aUpOrDown == UpOrDown::Up) {              \
      EXPECT_LT(vel.y, 0.0);                                \
    } else {                                                \
      EXPECT_GT(vel.y, 0.0);                                \
    }                                                       \
    EXPECT_GE(vel.Length(), aLowerBound);                   \
    EXPECT_LE(vel.Length(), aUpperBound);                   \
  } while (0)

// These tests have the following pattern: Two flings are executed, with a bit
// of wait time in between. The deltas in each pan gesture have been captured
// from a real phone, from touch events triggered by real fingers.
// We check the velocity at the end to detect whether the fling was accelerated
// or not. As an additional safety precaution, we also check the velocities for
// the first fling, so that changes in behavior are easier to analyze.
// One added challenge of this test is the fact that it has to work with on
// multiple platforms, and we use different velocity estimation strategies and
// different fling physics depending on the platform.
// The upper and lower bounds for the velocities were chosen in such a way that
// the test passes on all platforms. At the time of writing, we usually end up
// with higher velocities on Android than on Desktop, so the observed velocities
// on Android became the upper bounds and the observed velocities on Desktop
// becaume the lower bounds, each rounded out to a multiple of 0.1.

TEST_F(APZCFlingAccelerationTester, TwoNormalFlingsShouldAccelerate) {
  ExecutePanGesture100Hz(ScreenIntPoint{665, 1244},
                         {0, 0, -21, -44, -52, -55, -53, -49, -46, -47});
  CHECK_VELOCITY(Down, 4.5, 6.8);

  ExecuteWait(TimeDuration::FromMilliseconds(375));
  CHECK_VELOCITY(Down, 2.2, 5.1);

  ExecutePanGesture100Hz(ScreenIntPoint{623, 1211},
                         {-6, -51, -55, 0, -53, -57, -60, -60, -56});
  CHECK_VELOCITY(Down, 9.0, 14.0);
}

TEST_F(APZCFlingAccelerationTester, TwoFastFlingsShouldAccelerate) {
  ExecutePanGesture100Hz(ScreenIntPoint{764, 714},
                         {9, 30, 49, 60, 64, 64, 62, 59, 51});
  CHECK_VELOCITY(Up, 5.0, 7.5);

  ExecuteWait(TimeDuration::FromMilliseconds(447));
  CHECK_VELOCITY(Up, 2.3, 5.2);

  ExecutePanGesture100Hz(ScreenIntPoint{743, 739},
                         {7, 0, 38, 66, 75, 146, 0, 119});
  CHECK_VELOCITY(Up, 13.0, 20.0);
}

TEST_F(APZCFlingAccelerationTester,
       FlingsInOppositeDirectionShouldNotAccelerate) {
  ExecutePanGesture100Hz(ScreenIntPoint{728, 1381},
                         {0, 0, 0, -12, -24, -32, -43, -46, 0});
  CHECK_VELOCITY(Down, 2.9, 5.3);

  ExecuteWait(TimeDuration::FromMilliseconds(153));
  CHECK_VELOCITY(Down, 2.1, 4.8);

  ExecutePanGesture100Hz(ScreenIntPoint{698, 1059},
                         {0, 0, 14, 61, 41, 0, 45, 35});
  CHECK_VELOCITY(Up, 3.2, 4.3);
}

TEST_F(APZCFlingAccelerationTester,
       ShouldNotAccelerateWhenPreviousFlingHasSlowedDown) {
  ExecutePanGesture100Hz(ScreenIntPoint{748, 1046},
                         {0, 9, 15, 23, 31, 30, 0, 34, 31, 29, 28, 24, 24, 11});
  CHECK_VELOCITY(Up, 2.2, 3.0);
  ExecuteWait(TimeDuration::FromMilliseconds(498));
  CHECK_VELOCITY(Up, 0.5, 1.0);
  ExecutePanGesture100Hz(ScreenIntPoint{745, 1056},
                         {0, 10, 17, 29, 29, 33, 33, 0, 31, 27, 13});
  CHECK_VELOCITY(Up, 1.8, 2.7);
}

TEST_F(APZCFlingAccelerationTester, ShouldNotAccelerateWhenPausedAtStartOfPan) {
  ExecutePanGesture100Hz(
      ScreenIntPoint{711, 1468},
      {0, 0, 0, 0, -8, 0, -18, -32, -50, -57, -66, -68, -63, -60});
  CHECK_VELOCITY(Down, 6.2, 8.6);

  ExecuteWait(TimeDuration::FromMilliseconds(285));
  CHECK_VELOCITY(Down, 3.4, 7.4);

  ExecutePanGesture100Hz(
      ScreenIntPoint{658, 1352},
      {0, 0, 0, 0, 0, 0,  0,   0,   0,   0,   0,   0,   0,
       0, 0, 0, 0, 0, -8, -18, -34, -53, -70, -75, -75, -64});
  CHECK_VELOCITY(Down, 6.7, 9.1);
}

TEST_F(APZCFlingAccelerationTester, ShouldNotAccelerateWhenPausedDuringPan) {
  ExecutePanGesture100Hz(
      ScreenIntPoint{732, 1423},
      {0, 0, 0, -5, 0, -15, -41, -71, -90, -93, -85, -64, -44});
  CHECK_VELOCITY(Down, 7.5, 10.1);

  ExecuteWait(TimeDuration::FromMilliseconds(204));
  CHECK_VELOCITY(Down, 4.8, 9.4);

  ExecutePanGesture100Hz(
      ScreenIntPoint{651, 1372},
      {0,   0,   0,  -6, 0,  -16, -26, -41, -49, -65, -66, -61, -50, -35, -24,
       -17, -11, -8, -6, -5, -4,  -3,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,
       -3,  -4,  -5, -7, -9, -10, -10, -12, -18, -25, -23, -28, -30, -24});
  CHECK_VELOCITY(Down, 2.5, 3.4);
}

TEST_F(APZCFlingAccelerationTester,
       ShouldNotAccelerateWhenOppositeDirectionDuringPan) {
  ExecutePanGesture100Hz(ScreenIntPoint{663, 1371},
                         {0, 0, 0, -5, -18, -31, -49, -56, -61, -54, -55});
  CHECK_VELOCITY(Down, 5.4, 7.1);

  ExecuteWait(TimeDuration::FromMilliseconds(255));
  CHECK_VELOCITY(Down, 3.1, 6.0);

  ExecutePanGesture100Hz(
      ScreenIntPoint{726, 930},
      {0,  0,   0,   0,   30,  0,   19,  24,  32,  30, 37, 33,
       33, 32,  25,  23,  23,  18,  13,  9,   5,   3,  1,  0,
       -7, -19, -38, -53, -68, -79, -85, -73, -64, -54});
  CHECK_VELOCITY(Down, 7.0, 10.0);
}

TEST_F(APZCFlingAccelerationTester,
       ShouldAccelerateAfterLongWaitIfVelocityStillHigh) {
  // Reduce friction with the "Desktop" fling physics a little, so that it
  // behaves more similarly to the Android fling physics, and has enough
  // velocity after the wait time to allow for acceleration.
  SCOPED_GFX_PREF_FLOAT("apz.fling_friction", 0.0012);

  ExecutePanGesture100Hz(ScreenIntPoint{739, 1424},
                         {0, 0, -5, -10, -20, 0, -110, -86, 0, -102, -105});
  CHECK_VELOCITY(Down, 6.3, 9.4);

  ExecuteWait(TimeDuration::FromMilliseconds(1117));
  CHECK_VELOCITY(Down, 1.6, 3.3);

  ExecutePanGesture100Hz(ScreenIntPoint{726, 1380},
                         {0, -8, 0, -30, -60, -87, -104, -111});
  CHECK_VELOCITY(Down, 13.0, 23.0);
}

TEST_F(APZCFlingAccelerationTester, ShouldNotAccelerateAfterCanceledWithTap) {
  // First, build up a lot of speed.
  ExecutePanGesture100Hz(ScreenIntPoint{569, 710},
                         {11, 2, 107, 18, 148, 57, 133, 159, 21});
  ExecuteWait(TimeDuration::FromMilliseconds(154));
  ExecutePanGesture100Hz(ScreenIntPoint{581, 650},
                         {12, 68, 0, 162, 78, 140, 167});
  ExecuteWait(TimeDuration::FromMilliseconds(123));
  ExecutePanGesture100Hz(ScreenIntPoint{568, 723}, {11, 0, 79, 91, 131, 171});
  ExecuteWait(TimeDuration::FromMilliseconds(123));
  ExecutePanGesture100Hz(ScreenIntPoint{598, 678},
                         {8, 55, 22, 87, 117, 220, 54});
  ExecuteWait(TimeDuration::FromMilliseconds(134));
  ExecutePanGesture100Hz(ScreenIntPoint{585, 854}, {45, 137, 107, 102, 79});
  ExecuteWait(TimeDuration::FromMilliseconds(246));

  // Then, interrupt with a tap.
  ExecutePanGesture100Hz(ScreenIntPoint{566, 812}, {0, 0, 0, 0});
  ExecuteWait(TimeDuration::FromMilliseconds(869));

  // Then do a regular fling.
  ExecutePanGesture100Hz(ScreenIntPoint{599, 819},
                         {0, 0, 8, 35, 8, 38, 29, 37});

  CHECK_VELOCITY(Up, 2.8, 4.2);
}

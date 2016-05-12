/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncPanZoomController.h"
#include "FlingOverScrollerAnimation.h"
#include "GeneratedJNIWrappers.h"
#include "gfxPrefs.h"
#include "OverscrollHandoffState.h"
#include "OverScroller.h"

namespace mozilla {
namespace layers {

// Value used in boundary detection. Due to round off error,
// assume getting within 5 pixels of the boundary is close enough.
static const float BOUNDS_EPSILON = 5.0f;
// Maximum number of times the animator can calculate the same offset
// before the animation is aborted. This is due to a bug in the Android
// OverScroller class where under certain conditions the OverScroller
// will overflow some internal value and begin scrolling beyond the bounds
// of the page. Since we are clamping the results from the OverScroller,
// if the offset does not change over the past 30 frames, we assume the
// OverScroller has overflowed.
static const int32_t MAX_OVERSCROLL_COUNT = 30;

FlingOverScrollerAnimation::FlingOverScrollerAnimation(AsyncPanZoomController& aApzc,
                                                       widget::sdk::OverScroller::GlobalRef aOverScroller,
                                                       const RefPtr<const OverscrollHandoffChain>& aOverscrollHandoffChain,
                                                       const RefPtr<const AsyncPanZoomController>& aScrolledApzc)
  : mApzc(aApzc)
  , mOverScroller(aOverScroller)
  , mOverscrollHandoffChain(aOverscrollHandoffChain)
  , mScrolledApzc(aScrolledApzc)
  , mSentBounceX(false)
  , mSentBounceY(false)
  , mOverScrollCount(0)
{
  MOZ_ASSERT(mOverscrollHandoffChain);
  MOZ_ASSERT(mOverScroller);

  // Drop any velocity on axes where we don't have room to scroll anyways
  // (in this APZC, or an APZC further in the handoff chain).
  // This ensures that we don't take the 'overscroll' path in Sample()
  // on account of one axis which can't scroll having a velocity.
  if (!mOverscrollHandoffChain->CanScrollInDirection(&mApzc, Layer::HORIZONTAL)) {
    ReentrantMonitorAutoEnter lock(mApzc.mMonitor);
    mApzc.mX.SetVelocity(0);
  }
  if (!mOverscrollHandoffChain->CanScrollInDirection(&mApzc, Layer::VERTICAL)) {
    ReentrantMonitorAutoEnter lock(mApzc.mMonitor);
    mApzc.mY.SetVelocity(0);
  }

  ParentLayerPoint velocity = mApzc.GetVelocityVector();
  float length = velocity.Length();
  if (length > 0.0f) {
    mFlingDirection = velocity / length;
  }

  mStartOffset.x = mPreviousOffset.x = mApzc.mX.GetOrigin().value;
  mStartOffset.y = mPreviousOffset.y = mApzc.mY.GetOrigin().value;
  mOverScroller->Fling((int32_t)mStartOffset.x, (int32_t)mStartOffset.y,
                       // Android needs the velocity in pixels per second and it is in pixels per ms.
                       (int32_t)(velocity.x * 1000.0f), (int32_t)(velocity.y * 1000.0f),
                       (int32_t)mApzc.mX.GetPageStart().value, (int32_t)(mApzc.mX.GetPageEnd() - mApzc.mX.GetCompositionLength()).value,
                       (int32_t)mApzc.mY.GetPageStart().value, (int32_t)(mApzc.mY.GetPageEnd() - mApzc.mY.GetCompositionLength()).value,
                       0, 0);
}

/**
 * Advances a fling by an interpolated amount based on the Android OverScroller.
 * This should be called whenever sampling the content transform for this
 * frame. Returns true if the fling animation should be advanced by one frame,
 * or false if there is no fling or the fling has ended.
 */
bool
FlingOverScrollerAnimation::DoSample(FrameMetrics& aFrameMetrics,
                                     const TimeDuration& aDelta)
{
  bool shouldContinueFling = true;
  mOverScroller->ComputeScrollOffset(&shouldContinueFling);

  float speed = 0.0f;
  mOverScroller->GetCurrVelocity(&speed);
  speed = speed * 0.001f; // convert from pixels/sec to pixels/ms

  // gfxPrefs::APZFlingStoppedThreshold is only used in tests.
  if (!shouldContinueFling || (speed < gfxPrefs::APZFlingStoppedThreshold())) {
    if (shouldContinueFling) {
      // The OverScroller thinks it should continue but the speed is below
      // the stopping threshold so abort the animation.
      mOverScroller->AbortAnimation();
    }
    mApzc.mX.SetVelocity(0);
    mApzc.mY.SetVelocity(0);
    return false;
  }

  int32_t currentX = 0;
  int32_t currentY = 0;
  mOverScroller->GetCurrX(&currentX);
  mOverScroller->GetCurrY(&currentY);
  ParentLayerPoint offset((float)currentX, (float)currentY);
  ParentLayerPoint velocity = mFlingDirection * speed;

  bool hitBoundX = CheckBounds(mApzc.mX, offset.x, &(offset.x));
  bool hitBoundY = CheckBounds(mApzc.mY, offset.y, &(offset.y));

  if (IsZero(mPreviousOffset - offset)) {
    mOverScrollCount++;
  } else {
    mOverScrollCount = 0;
  }

  // If the offset hasn't changed in over MAX_OVERSCROLL_COUNT we have overflowed
  // the OverScroller and it needs to be aborted.
  if (mOverScrollCount > MAX_OVERSCROLL_COUNT) {
    velocity.x = velocity.y = 0.0f;
    mOverScroller->AbortAnimation();
  }

  mPreviousOffset = offset;

  mApzc.SetVelocityVector(velocity);
  aFrameMetrics.SetScrollOffset(offset / aFrameMetrics.GetZoom());

  if (hitBoundX || hitBoundY) {
    ParentLayerPoint bounceVelocity = mFlingDirection * speed;

    if (!mSentBounceX && hitBoundX && fabsf(offset.x - mStartOffset.x) > BOUNDS_EPSILON) {
      mSentBounceX = true;
    } else {
      bounceVelocity.x = 0.0f;
    }

    if (!mSentBounceY && hitBoundY && fabsf(offset.y - mStartOffset.y) > BOUNDS_EPSILON) {
      mSentBounceY = true;
    } else {
      bounceVelocity.y = 0.0f;
    }
    if (!IsZero(bounceVelocity)) {
      mDeferredTasks.AppendElement(
          NewRunnableMethod<ParentLayerPoint,
                            RefPtr<const OverscrollHandoffChain>,
                            RefPtr<const AsyncPanZoomController>>(&mApzc,
                                                                  &AsyncPanZoomController::HandleFlingOverscroll,
                                                                  bounceVelocity,
                                                                  mOverscrollHandoffChain,
                                                                  mScrolledApzc));
    }
  }

  return true;
}

bool
FlingOverScrollerAnimation::CheckBounds(Axis& aAxis, float aValue, float* aClamped)
{
  bool result = false;
  if ((aValue - BOUNDS_EPSILON) <= aAxis.GetPageStart().value) {
    result = true;
    if (aClamped) {
      *aClamped = aAxis.GetPageStart().value;
    }
  } else if ((aValue + BOUNDS_EPSILON) >= (aAxis.GetPageEnd() - aAxis.GetCompositionLength()).value) {
    result = true;
    if (aClamped) {
      *aClamped = (aAxis.GetPageEnd() - aAxis.GetCompositionLength()).value;
    }
  }
  return result;
}

} // namespace layers
} // namespace mozilla

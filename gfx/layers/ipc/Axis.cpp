/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Axis.h"
#include "AsyncPanZoomController.h"

namespace mozilla {
namespace layers {

static const float EPSILON = 0.0001f;

/**
 * Maximum acceleration that can happen between two frames. Velocity is
 * throttled if it's above this. This may happen if a time delta is very low,
 * or we get a touch point very far away from the previous position for some
 * reason.
 */
static const float MAX_EVENT_ACCELERATION = 0.5f;

/**
 * Amount of friction applied during flings when going above
 * VELOCITY_THRESHOLD.
 */
static const float FLING_FRICTION_FAST = 0.0025f;

/**
 * Amount of friction applied during flings when going below
 * VELOCITY_THRESHOLD.
 */
static const float FLING_FRICTION_SLOW = 0.0015f;

/**
 * Maximum velocity before fling friction increases.
 */
static const float VELOCITY_THRESHOLD = 1.0f;

/**
 * When flinging, if the velocity goes below this number, we just stop the
 * animation completely. This is to prevent asymptotically approaching 0
 * velocity and rerendering unnecessarily.
 */
static const float FLING_STOPPED_THRESHOLD = 0.01f;

Axis::Axis(AsyncPanZoomController* aAsyncPanZoomController)
  : mPos(0.0f),
    mVelocity(0.0f),
    mAsyncPanZoomController(aAsyncPanZoomController),
    mLockPanning(false)
{

}

void Axis::UpdateWithTouchAtDevicePoint(PRInt32 aPos, const TimeDuration& aTimeDelta) {
  if (mLockPanning) {
    return;
  }

  float newVelocity = (mPos - aPos) / aTimeDelta.ToMilliseconds();

  bool curVelocityIsLow = fabsf(newVelocity) < 0.01f;
  bool directionChange = (mVelocity > 0) != (newVelocity != 0);

  // If a direction change has happened, or the current velocity due to this new
  // touch is relatively low, then just apply it. If not, throttle it.
  if (curVelocityIsLow || (directionChange && fabs(newVelocity) - EPSILON <= 0.0f)) {
    mVelocity = newVelocity;
  } else {
    float maxChange = fabsf(mVelocity * aTimeDelta.ToMilliseconds() * MAX_EVENT_ACCELERATION);
    mVelocity = NS_MIN(mVelocity + maxChange, NS_MAX(mVelocity - maxChange, newVelocity));
  }

  mVelocity = newVelocity;
  mPos = aPos;
}

void Axis::StartTouch(PRInt32 aPos) {
  mStartPos = aPos;
  mPos = aPos;
  mVelocity = 0.0f;
  mLockPanning = false;
}

PRInt32 Axis::GetDisplacementForDuration(float aScale, const TimeDuration& aDelta) {
  PRInt32 displacement = NS_lround(mVelocity * aScale * aDelta.ToMilliseconds());
  // If this displacement will cause an overscroll, throttle it. Can potentially
  // bring it to 0 even if the velocity is high.
  if (DisplacementWillOverscroll(displacement) != OVERSCROLL_NONE) {
    displacement -= DisplacementWillOverscrollAmount(displacement);
  }
  return displacement;
}

float Axis::PanDistance() {
  return fabsf(mPos - mStartPos);
}

void Axis::StopTouch() {
  mVelocity = 0.0f;
}

void Axis::LockPanning() {
  mLockPanning = true;
}

bool Axis::FlingApplyFrictionOrCancel(const TimeDuration& aDelta) {
  if (fabsf(mVelocity) <= FLING_STOPPED_THRESHOLD) {
    // If the velocity is very low, just set it to 0 and stop the fling,
    // otherwise we'll just asymptotically approach 0 and the user won't
    // actually see any changes.
    mVelocity = 0.0f;
    return false;
  } else if (fabsf(mVelocity) >= VELOCITY_THRESHOLD) {
    mVelocity *= NS_MAX(1.0f - FLING_FRICTION_FAST * aDelta.ToMilliseconds(), 0.0);
  } else {
    mVelocity *= NS_MAX(1.0f - FLING_FRICTION_SLOW * aDelta.ToMilliseconds(), 0.0);
  }
  return true;
}

Axis::Overscroll Axis::GetOverscroll() {
  // If the current pan takes the viewport to the left of or above the current
  // page rect.
  bool minus = GetOrigin() < GetPageStart();
  // If the current pan takes the viewport to the right of or below the current
  // page rect.
  bool plus = GetViewportEnd() > GetPageEnd();
  if (minus && plus) {
    return OVERSCROLL_BOTH;
  }
  if (minus) {
    return OVERSCROLL_MINUS;
  }
  if (plus) {
    return OVERSCROLL_PLUS;
  }
  return OVERSCROLL_NONE;
}

PRInt32 Axis::GetExcess() {
  switch (GetOverscroll()) {
  case OVERSCROLL_MINUS: return GetOrigin() - GetPageStart();
  case OVERSCROLL_PLUS: return GetViewportEnd() - GetPageEnd();
  case OVERSCROLL_BOTH: return (GetViewportEnd() - GetPageEnd()) + (GetPageStart() - GetOrigin());
  default: return 0;
  }
}

Axis::Overscroll Axis::DisplacementWillOverscroll(PRInt32 aDisplacement) {
  // If the current pan plus a displacement takes the viewport to the left of or
  // above the current page rect.
  bool minus = GetOrigin() + aDisplacement < GetPageStart();
  // If the current pan plus a displacement takes the viewport to the right of or
  // below the current page rect.
  bool plus = GetViewportEnd() + aDisplacement > GetPageEnd();
  if (minus && plus) {
    return OVERSCROLL_BOTH;
  }
  if (minus) {
    return OVERSCROLL_MINUS;
  }
  if (plus) {
    return OVERSCROLL_PLUS;
  }
  return OVERSCROLL_NONE;
}

PRInt32 Axis::DisplacementWillOverscrollAmount(PRInt32 aDisplacement) {
  switch (DisplacementWillOverscroll(aDisplacement)) {
  case OVERSCROLL_MINUS: return (GetOrigin() + aDisplacement) - GetPageStart();
  case OVERSCROLL_PLUS: return (GetViewportEnd() + aDisplacement) - GetPageEnd();
  // Don't handle overscrolled in both directions; a displacement can't cause
  // this, it must have already been zoomed out too far.
  default: return 0;
  }
}

Axis::Overscroll Axis::ScaleWillOverscroll(float aScale, PRInt32 aFocus) {
  PRInt32 originAfterScale = NS_lround((GetOrigin() + aFocus) * aScale - aFocus);

  bool both = ScaleWillOverscrollBothSides(aScale);
  bool minus = originAfterScale < NS_lround(GetPageStart() * aScale);
  bool plus = (originAfterScale + GetViewportLength()) > NS_lround(GetPageEnd() * aScale);

  if ((minus && plus) || both) {
    return OVERSCROLL_BOTH;
  }
  if (minus) {
    return OVERSCROLL_MINUS;
  }
  if (plus) {
    return OVERSCROLL_PLUS;
  }
  return OVERSCROLL_NONE;
}

PRInt32 Axis::ScaleWillOverscrollAmount(float aScale, PRInt32 aFocus) {
  PRInt32 originAfterScale = NS_lround((GetOrigin() + aFocus) * aScale - aFocus);
  switch (ScaleWillOverscroll(aScale, aFocus)) {
  case OVERSCROLL_MINUS: return originAfterScale - NS_lround(GetPageStart() * aScale);
  case OVERSCROLL_PLUS: return (originAfterScale + GetViewportLength()) - NS_lround(GetPageEnd() * aScale);
  // Don't handle OVERSCROLL_BOTH. Client code is expected to deal with it.
  default: return 0;
  }
}

float Axis::GetVelocity() {
  return mVelocity;
}

PRInt32 Axis::GetViewportEnd() {
  return GetOrigin() + GetViewportLength();
}

PRInt32 Axis::GetPageEnd() {
  return GetPageStart() + GetPageLength();
}

PRInt32 Axis::GetOrigin() {
  nsIntPoint origin = mAsyncPanZoomController->GetFrameMetrics().mViewportScrollOffset;
  return GetPointOffset(origin);
}

PRInt32 Axis::GetViewportLength() {
  nsIntRect viewport = mAsyncPanZoomController->GetFrameMetrics().mViewport;
  gfx::Rect scaledViewport = gfx::Rect(viewport.x, viewport.y, viewport.width, viewport.height);
  scaledViewport.ScaleRoundIn(1 / mAsyncPanZoomController->GetFrameMetrics().mResolution.width);
  return GetRectLength(scaledViewport);
}

PRInt32 Axis::GetPageStart() {
  gfx::Rect pageRect = mAsyncPanZoomController->GetFrameMetrics().mCSSContentRect;
  return GetRectOffset(pageRect);
}

PRInt32 Axis::GetPageLength() {
  gfx::Rect pageRect = mAsyncPanZoomController->GetFrameMetrics().mCSSContentRect;
  return GetRectLength(pageRect);
}

bool Axis::ScaleWillOverscrollBothSides(float aScale) {
  const FrameMetrics& metrics = mAsyncPanZoomController->GetFrameMetrics();

  gfx::Rect cssContentRect = metrics.mCSSContentRect;

  float currentScale = metrics.mResolution.width;
  gfx::Rect viewport = gfx::Rect(metrics.mViewport.x,
                                 metrics.mViewport.y,
                                 metrics.mViewport.width,
                                 metrics.mViewport.height);
  viewport.ScaleRoundIn(1 / (currentScale * aScale));

  return GetRectLength(cssContentRect) < GetRectLength(viewport);
}

AxisX::AxisX(AsyncPanZoomController* aAsyncPanZoomController)
  : Axis(aAsyncPanZoomController)
{

}

PRInt32 AxisX::GetPointOffset(const nsIntPoint& aPoint)
{
  return aPoint.x;
}

PRInt32 AxisX::GetRectLength(const gfx::Rect& aRect)
{
  return NS_lround(aRect.width);
}

PRInt32 AxisX::GetRectOffset(const gfx::Rect& aRect)
{
  return NS_lround(aRect.x);
}

AxisY::AxisY(AsyncPanZoomController* aAsyncPanZoomController)
  : Axis(aAsyncPanZoomController)
{

}

PRInt32 AxisY::GetPointOffset(const nsIntPoint& aPoint)
{
  return aPoint.y;
}

PRInt32 AxisY::GetRectLength(const gfx::Rect& aRect)
{
  return NS_lround(aRect.height);
}

PRInt32 AxisY::GetRectOffset(const gfx::Rect& aRect)
{
  return NS_lround(aRect.y);
}

}
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Axis.h"
#include "AsyncPanZoomController.h"

namespace mozilla {
namespace layers {

static const float EPSILON = 0.0001;

/**
 * Milliseconds per frame, used to judge how much displacement should have
 * happened every frame based on the velocity calculated from touch events.
 */
static const float MS_PER_FRAME = 1000.0f / 60.0f;

/**
 * Maximum acceleration that can happen between two frames. Velocity is
 * throttled if it's above this. This may happen if a time delta is very low,
 * or we get a touch point very far away from the previous position for some
 * reason.
 */
static const float MAX_EVENT_ACCELERATION = 12;

/**
 * Amount of friction applied during flings when going above
 * VELOCITY_THRESHOLD.
 */
static const float FLING_FRICTION_FAST = 0.010;

/**
 * Amount of friction applied during flings when going below
 * VELOCITY_THRESHOLD.
 */
static const float FLING_FRICTION_SLOW = 0.008;

/**
 * Maximum velocity before fling friction increases.
 */
static const float VELOCITY_THRESHOLD = 10;

/**
 * When flinging, if the velocity goes below this number, we just stop the
 * animation completely. This is to prevent asymptotically approaching 0
 * velocity and rerendering unnecessarily.
 */
static const float FLING_STOPPED_THRESHOLD = 0.1f;

Axis::Axis(AsyncPanZoomController* aAsyncPanZoomController)
  : mPos(0.0f),
    mVelocity(0.0f),
    mAsyncPanZoomController(aAsyncPanZoomController)
{

}

void Axis::UpdateWithTouchAtDevicePoint(PRInt32 aPos, PRInt32 aTimeDelta) {
  float newVelocity = MS_PER_FRAME * (mPos - aPos) / aTimeDelta;

  bool curVelocityIsLow = fabsf(newVelocity) < 1.0f;
  bool directionChange = (mVelocity > 0) != (newVelocity != 0);

  // If a direction change has happened, or the current velocity due to this new
  // touch is relatively low, then just apply it. If not, throttle it.
  if (curVelocityIsLow || (directionChange && fabs(newVelocity) - EPSILON <= 0.0f)) {
    mVelocity = newVelocity;
  } else {
    float maxChange = fabsf(mVelocity * aTimeDelta * MAX_EVENT_ACCELERATION);
    mVelocity = NS_MIN(mVelocity + maxChange, NS_MAX(mVelocity - maxChange, newVelocity));
  }

  mVelocity = newVelocity;
  mPos = aPos;
}

void Axis::StartTouch(PRInt32 aPos) {
  mStartPos = aPos;
  mPos = aPos;
  mVelocity = 0.0f;
}

PRInt32 Axis::UpdateAndGetDisplacement(float aScale) {
  PRInt32 displacement = NS_lround(mVelocity * aScale);
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

bool Axis::FlingApplyFrictionOrCancel(const TimeDuration& aDelta) {
  if (fabsf(mVelocity) <= FLING_STOPPED_THRESHOLD) {
    // If the velocity is very low, just set it to 0 and stop the fling,
    // otherwise we'll just asymptotically approach 0 and the user won't
    // actually see any changes.
    mVelocity = 0.0f;
    return false;
  } else if (fabsf(mVelocity) >= VELOCITY_THRESHOLD) {
    mVelocity *= 1.0f - FLING_FRICTION_FAST * aDelta.ToMilliseconds();
  } else {
    mVelocity *= 1.0f - FLING_FRICTION_SLOW * aDelta.ToMilliseconds();
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
  return GetRectLength(viewport);
}

PRInt32 Axis::GetPageStart() {
  nsIntRect pageRect = mAsyncPanZoomController->GetFrameMetrics().mContentRect;
  return GetRectOffset(pageRect);
}

PRInt32 Axis::GetPageLength() {
  nsIntRect pageRect = mAsyncPanZoomController->GetFrameMetrics().mContentRect;
  return GetRectLength(pageRect);
}

bool Axis::ScaleWillOverscrollBothSides(float aScale) {
  const FrameMetrics& metrics = mAsyncPanZoomController->GetFrameMetrics();

  float currentScale = metrics.mResolution.width;
  gfx::Rect cssContentRect = metrics.mCSSContentRect;
  cssContentRect.ScaleRoundIn(currentScale * aScale);

  nsIntRect contentRect = nsIntRect(cssContentRect.x,
                                    cssContentRect.y,
                                    cssContentRect.width,
                                    cssContentRect.height);

  return GetRectLength(contentRect) < GetRectLength(metrics.mViewport);
}

AxisX::AxisX(AsyncPanZoomController* aAsyncPanZoomController)
  : Axis(aAsyncPanZoomController)
{

}

PRInt32 AxisX::GetPointOffset(const nsIntPoint& aPoint)
{
  return aPoint.x;
}

PRInt32 AxisX::GetRectLength(const nsIntRect& aRect)
{
  return aRect.width;
}

PRInt32 AxisX::GetRectOffset(const nsIntRect& aRect)
{
  return aRect.x;
}

AxisY::AxisY(AsyncPanZoomController* aAsyncPanZoomController)
  : Axis(aAsyncPanZoomController)
{

}

PRInt32 AxisY::GetPointOffset(const nsIntPoint& aPoint)
{
  return aPoint.y;
}

PRInt32 AxisY::GetRectLength(const nsIntRect& aRect)
{
  return aRect.height;
}

PRInt32 AxisY::GetRectOffset(const nsIntRect& aRect)
{
  return aRect.y;
}

}
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Axis.h"
#include "AsyncPanZoomController.h"
#include "mozilla/Preferences.h"
#include "nsThreadUtils.h"
#include <algorithm>

namespace mozilla {
namespace layers {

static const float EPSILON = 0.0001f;

/**
 * Maximum acceleration that can happen between two frames. Velocity is
 * throttled if it's above this. This may happen if a time delta is very low,
 * or we get a touch point very far away from the previous position for some
 * reason.
 */
static float gMaxEventAcceleration = 999.0f;

/**
 * Amount of friction applied during flings.
 */
static float gFlingFriction = 0.006f;

/**
 * Threshold for velocity beneath which we turn off any acceleration we had
 * during repeated flings.
 */
static float gVelocityThreshold = 0.14f;

/**
 * Amount of acceleration we multiply in each time the user flings in one
 * direction. Every time they let go of the screen, we increase the acceleration
 * by this amount raised to the power of the amount of times they have let go,
 * times two (to make the curve steeper).  This stops if the user lets go and we
 * slow down enough, or if they put their finger down without moving it for a
 * moment (or in the opposite direction).
 */
static float gAccelerationMultiplier = 1.125f;

/**
 * When flinging, if the velocity goes below this number, we just stop the
 * animation completely. This is to prevent asymptotically approaching 0
 * velocity and rerendering unnecessarily.
 */
static float gFlingStoppedThreshold = 0.01f;

/**
 * Maximum size of velocity queue. The queue contains last N velocity records.
 * On touch end we calculate the average velocity in order to compensate
 * touch/mouse drivers misbehaviour.
 */
static int gMaxVelocityQueueSize = 5;

static void ReadAxisPrefs()
{
  Preferences::AddFloatVarCache(&gMaxEventAcceleration, "gfx.axis.max_event_acceleration", gMaxEventAcceleration);
  Preferences::AddFloatVarCache(&gFlingFriction, "gfx.axis.fling_friction", gFlingFriction);
  Preferences::AddFloatVarCache(&gVelocityThreshold, "gfx.axis.velocity_threshold", gVelocityThreshold);
  Preferences::AddFloatVarCache(&gAccelerationMultiplier, "gfx.axis.acceleration_multiplier", gAccelerationMultiplier);
  Preferences::AddFloatVarCache(&gFlingStoppedThreshold, "gfx.axis.fling_stopped_threshold", gFlingStoppedThreshold);
  Preferences::AddIntVarCache(&gMaxVelocityQueueSize, "gfx.axis.max_velocity_queue_size", gMaxVelocityQueueSize);
}

class ReadAxisPref MOZ_FINAL : public nsRunnable {
public:
  NS_IMETHOD Run()
  {
    ReadAxisPrefs();
    return NS_OK;
  }
};

static void InitAxisPrefs()
{
  static bool sInitialized = false;
  if (sInitialized)
    return;

  sInitialized = true;
  if (NS_IsMainThread()) {
    ReadAxisPrefs();
  } else {
    // We have to dispatch an event to the main thread to read the pref.
    NS_DispatchToMainThread(new ReadAxisPref());
  }
}

Axis::Axis(AsyncPanZoomController* aAsyncPanZoomController)
  : mPos(0),
    mVelocity(0.0f),
    mAcceleration(0),
    mAsyncPanZoomController(aAsyncPanZoomController)
{
  InitAxisPrefs();
}

void Axis::UpdateWithTouchAtDevicePoint(int32_t aPos, const TimeDuration& aTimeDelta) {
  float newVelocity = (mPos - aPos) / aTimeDelta.ToMilliseconds();

  bool curVelocityBelowThreshold = fabsf(newVelocity) < gVelocityThreshold;
  bool directionChange = (mVelocity > 0) != (newVelocity > 0);

  // If we've changed directions, or the current velocity threshold, stop any
  // acceleration we've accumulated.
  if (directionChange || curVelocityBelowThreshold) {
    mAcceleration = 0;
  }

  mVelocity = newVelocity;
  mPos = aPos;

  // Keep last gMaxVelocityQueueSize or less velocities in the queue.
  mVelocityQueue.AppendElement(mVelocity);
  if (mVelocityQueue.Length() > gMaxVelocityQueueSize) {
    mVelocityQueue.RemoveElementAt(0);
  }
}

void Axis::StartTouch(int32_t aPos) {
  mStartPos = aPos;
  mPos = aPos;
}

float Axis::GetDisplacementForDuration(float aScale, const TimeDuration& aDelta) {
  if (fabsf(mVelocity) < gVelocityThreshold) {
    mAcceleration = 0;
  }

  float accelerationFactor = GetAccelerationFactor();
  float displacement = mVelocity * aScale * aDelta.ToMilliseconds() * accelerationFactor;
  // If this displacement will cause an overscroll, throttle it. Can potentially
  // bring it to 0 even if the velocity is high.
  if (DisplacementWillOverscroll(displacement) != OVERSCROLL_NONE) {
    // No need to have a velocity along this axis anymore; it won't take us
    // anywhere, so we're just spinning needlessly.
    mVelocity = 0.0f;
    mAcceleration = 0;
    displacement -= DisplacementWillOverscrollAmount(displacement);
  }
  return displacement;
}

float Axis::PanDistance() {
  return fabsf(mPos - mStartPos);
}

void Axis::EndTouch() {
  mAcceleration++;

  // Calculate the mean velocity and empty the queue.
  int count = mVelocityQueue.Length();
  if (count) {
    mVelocity = 0;
    while (!mVelocityQueue.IsEmpty()) {
      mVelocity += mVelocityQueue[0];
      mVelocityQueue.RemoveElementAt(0);
    }
    mVelocity /= count;
  }
}

void Axis::CancelTouch() {
  mVelocity = 0.0f;
  mAcceleration = 0;
  while (!mVelocityQueue.IsEmpty()) {
    mVelocityQueue.RemoveElementAt(0);
  }
}

bool Axis::FlingApplyFrictionOrCancel(const TimeDuration& aDelta) {
  if (fabsf(mVelocity) <= gFlingStoppedThreshold) {
    // If the velocity is very low, just set it to 0 and stop the fling,
    // otherwise we'll just asymptotically approach 0 and the user won't
    // actually see any changes.
    mVelocity = 0.0f;
    return false;
  } else {
    mVelocity *= pow(1.0f - gFlingFriction, float(aDelta.ToMilliseconds()));
  }
  return true;
}

Axis::Overscroll Axis::GetOverscroll() {
  // If the current pan takes the window to the left of or above the current
  // page rect.
  bool minus = GetOrigin() < GetPageStart();
  // If the current pan takes the window to the right of or below the current
  // page rect.
  bool plus = GetCompositionEnd() > GetPageEnd();
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

float Axis::GetExcess() {
  switch (GetOverscroll()) {
  case OVERSCROLL_MINUS: return GetOrigin() - GetPageStart();
  case OVERSCROLL_PLUS: return GetCompositionEnd() - GetPageEnd();
  case OVERSCROLL_BOTH: return (GetCompositionEnd() - GetPageEnd()) +
                               (GetPageStart() - GetOrigin());
  default: return 0;
  }
}

Axis::Overscroll Axis::DisplacementWillOverscroll(float aDisplacement) {
  // If the current pan plus a displacement takes the window to the left of or
  // above the current page rect.
  bool minus = GetOrigin() + aDisplacement < GetPageStart();
  // If the current pan plus a displacement takes the window to the right of or
  // below the current page rect.
  bool plus = GetCompositionEnd() + aDisplacement > GetPageEnd();
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

float Axis::DisplacementWillOverscrollAmount(float aDisplacement) {
  switch (DisplacementWillOverscroll(aDisplacement)) {
  case OVERSCROLL_MINUS: return (GetOrigin() + aDisplacement) - GetPageStart();
  case OVERSCROLL_PLUS: return (GetCompositionEnd() + aDisplacement) - GetPageEnd();
  // Don't handle overscrolled in both directions; a displacement can't cause
  // this, it must have already been zoomed out too far.
  default: return 0;
  }
}

Axis::Overscroll Axis::ScaleWillOverscroll(float aScale, int32_t aFocus) {
  float originAfterScale = (GetOrigin() + aFocus) * aScale - aFocus;

  bool both = ScaleWillOverscrollBothSides(aScale);
  bool minus = originAfterScale < GetPageStart() * aScale;
  bool plus = (originAfterScale + GetCompositionLength()) > GetPageEnd() * aScale;

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

float Axis::ScaleWillOverscrollAmount(float aScale, int32_t aFocus) {
  float originAfterScale = (GetOrigin() + aFocus) * aScale - aFocus;
  switch (ScaleWillOverscroll(aScale, aFocus)) {
  case OVERSCROLL_MINUS: return originAfterScale - GetPageStart() * aScale;
  case OVERSCROLL_PLUS: return (originAfterScale + GetCompositionLength()) -
                               NS_lround(GetPageEnd() * aScale);
  // Don't handle OVERSCROLL_BOTH. Client code is expected to deal with it.
  default: return 0;
  }
}

float Axis::GetVelocity() {
  return mVelocity;
}

float Axis::GetAccelerationFactor() {
  return powf(gAccelerationMultiplier, std::max(0, (mAcceleration - 4) * 3));
}

float Axis::GetCompositionEnd() {
  return GetOrigin() + GetCompositionLength();
}

float Axis::GetPageEnd() {
  return GetPageStart() + GetPageLength();
}

float Axis::GetOrigin() {
  CSSPoint origin = mAsyncPanZoomController->GetFrameMetrics().mScrollOffset;
  return GetPointOffset(origin);
}

float Axis::GetCompositionLength() {
  const FrameMetrics& metrics = mAsyncPanZoomController->GetFrameMetrics();
  CSSRect cssCompositedRect =
    AsyncPanZoomController::CalculateCompositedRectInCssPixels(metrics);
  return GetRectLength(cssCompositedRect);
}

float Axis::GetPageStart() {
  CSSRect pageRect = mAsyncPanZoomController->GetFrameMetrics().mScrollableRect;
  return GetRectOffset(pageRect);
}

float Axis::GetPageLength() {
  CSSRect pageRect = mAsyncPanZoomController->GetFrameMetrics().mScrollableRect;
  return GetRectLength(pageRect);
}

bool Axis::ScaleWillOverscrollBothSides(float aScale) {
  const FrameMetrics& metrics = mAsyncPanZoomController->GetFrameMetrics();

  CSSRect cssContentRect = metrics.mScrollableRect;

  float scale = metrics.mZoom.width * aScale;
  CSSIntRect cssCompositionBounds = LayerIntRect::ToCSSIntRectRoundIn(
    metrics.mCompositionBounds, scale, scale);

  return GetRectLength(cssContentRect) < GetRectLength(CSSRect(cssCompositionBounds));
}

AxisX::AxisX(AsyncPanZoomController* aAsyncPanZoomController)
  : Axis(aAsyncPanZoomController)
{

}

float AxisX::GetPointOffset(const CSSPoint& aPoint)
{
  return aPoint.x;
}

float AxisX::GetRectLength(const CSSRect& aRect)
{
  return aRect.width;
}

float AxisX::GetRectOffset(const CSSRect& aRect)
{
  return aRect.x;
}

AxisY::AxisY(AsyncPanZoomController* aAsyncPanZoomController)
  : Axis(aAsyncPanZoomController)
{

}

float AxisY::GetPointOffset(const CSSPoint& aPoint)
{
  return aPoint.y;
}

float AxisY::GetRectLength(const CSSRect& aRect)
{
  return aRect.height;
}

float AxisY::GetRectOffset(const CSSRect& aRect)
{
  return aRect.y;
}

}
}

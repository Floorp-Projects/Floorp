/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Axis.h"
#include <math.h>                       // for fabsf, pow, powf
#include <algorithm>                    // for max
#include "AsyncPanZoomController.h"     // for AsyncPanZoomController
#include "mozilla/dom/KeyframeEffect.h" // for ComputedTimingFunction
#include "mozilla/layers/APZCTreeManager.h" // for APZCTreeManager
#include "mozilla/layers/APZThreadUtils.h" // for AssertOnControllerThread
#include "FrameMetrics.h"               // for FrameMetrics
#include "mozilla/Attributes.h"         // for final
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/gfx/Rect.h"           // for RoundedIn
#include "mozilla/mozalloc.h"           // for operator new
#include "mozilla/FloatingPoint.h"      // for FuzzyEqualsAdditive
#include "mozilla/StaticPtr.h"          // for StaticAutoPtr
#include "nsMathUtils.h"                // for NS_lround
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "nsThreadUtils.h"              // for NS_DispatchToMainThread, etc
#include "nscore.h"                     // for NS_IMETHOD
#include "gfxPrefs.h"                   // for the preferences

#define AXIS_LOG(...)
// #define AXIS_LOG(...) printf_stderr("AXIS: " __VA_ARGS__)

namespace mozilla {
namespace layers {

bool FuzzyEqualsCoordinate(float aValue1, float aValue2)
{
  return FuzzyEqualsAdditive(aValue1, aValue2, COORDINATE_EPSILON)
      || FuzzyEqualsMultiplicative(aValue1, aValue2);
}

extern StaticAutoPtr<ComputedTimingFunction> gVelocityCurveFunction;

Axis::Axis(AsyncPanZoomController* aAsyncPanZoomController)
  : mPos(0),
    mPosTimeMs(0),
    mVelocity(0.0f),
    mAxisLocked(false),
    mAsyncPanZoomController(aAsyncPanZoomController),
    mOverscroll(0),
    mFirstOverscrollAnimationSample(0),
    mLastOverscrollPeak(0),
    mOverscrollScale(1.0f)
{
}

float Axis::ToLocalVelocity(float aVelocityInchesPerMs) const {
  ScreenPoint velocity = MakePoint(aVelocityInchesPerMs * APZCTreeManager::GetDPI());
  // Use ToScreenCoordinates() to convert a point rather than a vector by
  // treating the point as a vector, and using (0, 0) as the anchor.
  ScreenPoint panStart = mAsyncPanZoomController->ToScreenCoordinates(
      mAsyncPanZoomController->PanStart(),
      ParentLayerPoint());
  ParentLayerPoint localVelocity =
      mAsyncPanZoomController->ToParentLayerCoordinates(velocity, panStart);
  return localVelocity.Length();
}

void Axis::UpdateWithTouchAtDevicePoint(ParentLayerCoord aPos, ParentLayerCoord aAdditionalDelta, uint32_t aTimestampMs) {
  // mVelocityQueue is controller-thread only
  APZThreadUtils::AssertOnControllerThread();

  if (aTimestampMs == mPosTimeMs) {
    // This could be a duplicate event, or it could be a legitimate event
    // on some platforms that generate events really fast. As a compromise
    // update mPos so we don't run into problems like bug 1042734, even though
    // that means the velocity will be stale. Better than doing a divide-by-zero.
    mPos = aPos;
    return;
  }

  float newVelocity = mAxisLocked ? 0.0f : (float)(mPos - aPos - aAdditionalDelta) / (float)(aTimestampMs - mPosTimeMs);
  if (gfxPrefs::APZMaxVelocity() > 0.0f) {
    bool velocityIsNegative = (newVelocity < 0);
    newVelocity = fabs(newVelocity);

    float maxVelocity = ToLocalVelocity(gfxPrefs::APZMaxVelocity());
    newVelocity = std::min(newVelocity, maxVelocity);

    if (gfxPrefs::APZCurveThreshold() > 0.0f && gfxPrefs::APZCurveThreshold() < gfxPrefs::APZMaxVelocity()) {
      float curveThreshold = ToLocalVelocity(gfxPrefs::APZCurveThreshold());
      if (newVelocity > curveThreshold) {
        // here, 0 < curveThreshold < newVelocity <= maxVelocity, so we apply the curve
        float scale = maxVelocity - curveThreshold;
        float funcInput = (newVelocity - curveThreshold) / scale;
        float funcOutput = gVelocityCurveFunction->GetValue(funcInput);
        float curvedVelocity = (funcOutput * scale) + curveThreshold;
        AXIS_LOG("%p|%s curving up velocity from %f to %f\n",
          mAsyncPanZoomController, Name(), newVelocity, curvedVelocity);
        newVelocity = curvedVelocity;
      }
    }

    if (velocityIsNegative) {
      newVelocity = -newVelocity;
    }
  }

  AXIS_LOG("%p|%s updating velocity to %f with touch\n",
    mAsyncPanZoomController, Name(), newVelocity);
  mVelocity = newVelocity;
  mPos = aPos;
  mPosTimeMs = aTimestampMs;

  // Limit queue size pased on pref
  mVelocityQueue.AppendElement(std::make_pair(aTimestampMs, mVelocity));
  if (mVelocityQueue.Length() > gfxPrefs::APZMaxVelocityQueueSize()) {
    mVelocityQueue.RemoveElementAt(0);
  }
}

void Axis::StartTouch(ParentLayerCoord aPos, uint32_t aTimestampMs) {
  mStartPos = aPos;
  mPos = aPos;
  mPosTimeMs = aTimestampMs;
  mAxisLocked = false;
}

bool Axis::AdjustDisplacement(ParentLayerCoord aDisplacement,
                              /* ParentLayerCoord */ float& aDisplacementOut,
                              /* ParentLayerCoord */ float& aOverscrollAmountOut,
                              bool aForceOverscroll /* = false */)
{
  if (mAxisLocked) {
    aOverscrollAmountOut = 0;
    aDisplacementOut = 0;
    return false;
  }
  if (aForceOverscroll) {
    aOverscrollAmountOut = aDisplacement;
    aDisplacementOut = 0;
    return false;
  }

  EndOverscrollAnimation();

  ParentLayerCoord displacement = aDisplacement;

  // First consume any overscroll in the opposite direction along this axis.
  ParentLayerCoord consumedOverscroll = 0;
  if (mOverscroll > 0 && aDisplacement < 0) {
    consumedOverscroll = std::min(mOverscroll, -aDisplacement);
  } else if (mOverscroll < 0 && aDisplacement > 0) {
    consumedOverscroll = 0.f - std::min(-mOverscroll, aDisplacement);
  }
  mOverscroll -= consumedOverscroll;
  displacement += consumedOverscroll;

  // Split the requested displacement into an allowed displacement that does
  // not overscroll, and an overscroll amount.
  aOverscrollAmountOut = DisplacementWillOverscrollAmount(displacement);
  if (aOverscrollAmountOut != 0.0f) {
    // No need to have a velocity along this axis anymore; it won't take us
    // anywhere, so we're just spinning needlessly.
    AXIS_LOG("%p|%s has overscrolled, clearing velocity\n",
      mAsyncPanZoomController, Name());
    mVelocity = 0.0f;
    displacement -= aOverscrollAmountOut;
  }
  aDisplacementOut = displacement;
  return fabsf(consumedOverscroll) > EPSILON;
}

ParentLayerCoord Axis::ApplyResistance(ParentLayerCoord aRequestedOverscroll) const {
  // 'resistanceFactor' is a value between 0 and 1, which:
  //   - tends to 1 as the existing overscroll tends to 0
  //   - tends to 0 as the existing overscroll tends to the composition length
  // The actual overscroll is the requested overscroll multiplied by this
  // factor; this should prevent overscrolling by more than the composition
  // length.
  float resistanceFactor = 1 - fabsf(GetOverscroll()) / GetCompositionLength();
  return resistanceFactor < 0 ? ParentLayerCoord(0) : aRequestedOverscroll * resistanceFactor;
}

void Axis::OverscrollBy(ParentLayerCoord aOverscroll) {
  MOZ_ASSERT(CanScroll());
  // We can get some spurious calls to OverscrollBy() with near-zero values
  // due to rounding error. Ignore those (they might trip the asserts below.)
  if (FuzzyEqualsAdditive(aOverscroll.value, 0.0f, COORDINATE_EPSILON)) {
    return;
  }
  EndOverscrollAnimation();
  aOverscroll = ApplyResistance(aOverscroll);
  if (aOverscroll > 0) {
#ifdef DEBUG
    if (!FuzzyEqualsCoordinate(GetCompositionEnd().value, GetPageEnd().value)) {
      nsPrintfCString message("composition end (%f) is not equal (within error) to page end (%f)\n",
                              GetCompositionEnd().value, GetPageEnd().value);
      NS_ASSERTION(false, message.get());
      MOZ_CRASH();
    }
#endif
    MOZ_ASSERT(mOverscroll >= 0);
  } else if (aOverscroll < 0) {
#ifdef DEBUG
    if (!FuzzyEqualsCoordinate(GetOrigin().value, GetPageStart().value)) {
      nsPrintfCString message("composition origin (%f) is not equal (within error) to page origin (%f)\n",
                              GetOrigin().value, GetPageStart().value);
      NS_ASSERTION(false, message.get());
      MOZ_CRASH();
    }
#endif
    MOZ_ASSERT(mOverscroll <= 0);
  }
  mOverscroll += aOverscroll;
}

ParentLayerCoord Axis::GetOverscroll() const {
  ParentLayerCoord result = (mOverscroll - mLastOverscrollPeak) / mOverscrollScale;

  // Assert that we return overscroll in the correct direction
#ifdef DEBUG
  if ((result.value * mFirstOverscrollAnimationSample.value) < 0.0f) {
    nsPrintfCString message("GetOverscroll() (%f) and first overscroll animation sample (%f) have different signs\n",
                            result.value, mFirstOverscrollAnimationSample.value);
    NS_ASSERTION(false, message.get());
    MOZ_CRASH();
  }
#endif

  return result;
}

void Axis::StartOverscrollAnimation(float aVelocity) {
  // Make sure any state from a previous animation has been cleared.
  MOZ_ASSERT(mFirstOverscrollAnimationSample == 0 &&
             mLastOverscrollPeak == 0 &&
             mOverscrollScale == 1);

  SetVelocity(aVelocity);
}

void Axis::EndOverscrollAnimation() {
  ParentLayerCoord overscroll = GetOverscroll();
  mFirstOverscrollAnimationSample = 0;
  mLastOverscrollPeak = 0;
  mOverscrollScale = 1.0f;
  mOverscroll = overscroll;
}

void Axis::StepOverscrollAnimation(double aStepDurationMilliseconds) {
  // Apply spring physics to the overscroll as time goes on.
  // Note: this method of sampling isn't perfectly smooth, as it assumes
  // a constant velocity over 'aDelta', instead of an accelerating velocity.
  // (The way we applying friction to flings has the same issue.)
  // Hooke's law with damping:
  //   F = -kx - bv
  // where
  //   k is a constant related to the stiffness of the spring
  //     The larger the constant, the stiffer the spring.
  //   x is the displacement of the end of the spring from its equilibrium
  //     In our scenario, it's the amount of overscroll on the axis.
  //   b is a constant that provides damping (friction)
  //   v is the velocity of the point at the end of the spring
  // See http://gafferongames.com/game-physics/spring-physics/
  const float kSpringStiffness = gfxPrefs::APZOverscrollSpringStiffness();
  const float kSpringFriction = gfxPrefs::APZOverscrollSpringFriction();

  // Apply spring force.
  float springForce = -1 * kSpringStiffness * mOverscroll;
  // Assume unit mass, so force = acceleration.
  float oldVelocity = mVelocity;
  mVelocity += springForce * aStepDurationMilliseconds;

  // Apply dampening.
  mVelocity *= pow(double(1 - kSpringFriction), aStepDurationMilliseconds);
  AXIS_LOG("%p|%s sampled overscroll animation, leaving velocity at %f\n",
    mAsyncPanZoomController, Name(), mVelocity);

  // At the peak of each oscillation, record new offset and scaling factors for
  // overscroll, to ensure that GetOverscroll always returns a value of the
  // same sign, and that this value is correctly adjusted as the spring is
  // dampened.
  // To handle the case where one of the velocity samples is exaclty zero,
  // consider a sign change to have occurred when the outgoing velocity is zero.
  bool velocitySignChange = (oldVelocity * mVelocity) < 0 || mVelocity == 0;
  if (mFirstOverscrollAnimationSample == 0.0f) {
    mFirstOverscrollAnimationSample = mOverscroll;

    // It's possible to start sampling overscroll with velocity == 0, or
    // velocity in the opposite direction of overscroll, so make sure we
    // correctly record the peak in this case.
    if (mOverscroll != 0 && ((mOverscroll > 0 ? oldVelocity : -oldVelocity) <= 0.0f)) {
      velocitySignChange = true;
    }
  }
  if (velocitySignChange) {
    bool oddOscillation = (mOverscroll.value * mFirstOverscrollAnimationSample.value) < 0.0f;
    mLastOverscrollPeak = oddOscillation ? mOverscroll : -mOverscroll;
    mOverscrollScale = 2.0f;
  }

  // Adjust the amount of overscroll based on the velocity.
  // Note that we allow for oscillations.
  mOverscroll += (mVelocity * aStepDurationMilliseconds);

  // Our mechanism for translating a set of mOverscroll values that oscillate
  // around zero to a set of GetOverscroll() values that have the same sign
  // (so content is always stretched, never compressed) assumes that
  // mOverscroll does not exceed mLastOverscrollPeak in magnitude. If our
  // calculations were exact, this would be the case, as a dampened spring
  // should never attain a displacement greater in magnitude than a previous
  // peak. In our approximation calculations, however, this may not hold
  // exactly. To ensure the assumption is not violated, we clamp the magnitude
  // of mOverscroll.
  if (mLastOverscrollPeak != 0 && fabs(mOverscroll) > fabs(mLastOverscrollPeak)) {
    mOverscroll = (mOverscroll >= 0) ? fabs(mLastOverscrollPeak) : -fabs(mLastOverscrollPeak);
  }
}

bool Axis::SampleOverscrollAnimation(const TimeDuration& aDelta) {
  // Short-circuit early rather than running through all the sampling code.
  if (mVelocity == 0.0f && mOverscroll == 0.0f) {
    return false;
  }

  // We approximate the curve traced out by the velocity of the spring
  // over time by breaking up the curve into small segments over which we
  // consider the velocity to be constant. If the animation is sampled
  // sufficiently often, then treating |aDelta| as a single segment of this
  // sort would be fine, but the frequency at which the animation is sampled
  // can be affected by external factors, and as the segment size grows larger,
  // the approximation gets worse and the approximated curve can even diverge
  // (i.e. oscillate forever, with displacements of increasing absolute value)!
  // To avoid this, we break up |aDelta| into smaller segments of length 1 ms
  // each, and a segment of any remaining fractional milliseconds.
  double milliseconds = aDelta.ToMilliseconds();
  int wholeMilliseconds = (int) aDelta.ToMilliseconds();
  double fractionalMilliseconds = milliseconds - wholeMilliseconds;
  for (int i = 0; i < wholeMilliseconds; ++i) {
    StepOverscrollAnimation(1);
  }
  StepOverscrollAnimation(fractionalMilliseconds);

  // If both the velocity and the displacement fall below a threshold, stop
  // the animation so we don't continue doing tiny oscillations that aren't
  // noticeable.
  if (fabs(mOverscroll) < gfxPrefs::APZOverscrollStopDistanceThreshold() &&
      fabs(mVelocity) < gfxPrefs::APZOverscrollStopVelocityThreshold()) {
    // "Jump" to the at-rest state. The jump shouldn't be noticeable as the
    // velocity and overscroll are already low.
    AXIS_LOG("%p|%s oscillation dropped below threshold, going to rest\n",
      mAsyncPanZoomController, Name());
    ClearOverscroll();
    mVelocity = 0;
    return false;
  }

  // Otherwise, continue the animation.
  return true;
}

bool Axis::IsOverscrolled() const {
  return mOverscroll != 0.f;
}

void Axis::ClearOverscroll() {
  EndOverscrollAnimation();
  mOverscroll = 0;
}

ParentLayerCoord Axis::PanStart() const {
  return mStartPos;
}

ParentLayerCoord Axis::PanDistance() const {
  return fabs(mPos - mStartPos);
}

ParentLayerCoord Axis::PanDistance(ParentLayerCoord aPos) const {
  return fabs(aPos - mStartPos);
}

void Axis::EndTouch(uint32_t aTimestampMs) {
  // mVelocityQueue is controller-thread only
  APZThreadUtils::AssertOnControllerThread();

  mVelocity = 0;
  int count = 0;
  while (!mVelocityQueue.IsEmpty()) {
    uint32_t timeDelta = (aTimestampMs - mVelocityQueue[0].first);
    if (timeDelta < gfxPrefs::APZVelocityRelevanceTime()) {
      count++;
      mVelocity += mVelocityQueue[0].second;
    }
    mVelocityQueue.RemoveElementAt(0);
  }
  if (count > 1) {
    mVelocity /= count;
  }
  AXIS_LOG("%p|%s ending touch, computed velocity %f\n",
    mAsyncPanZoomController, Name(), mVelocity);
}

void Axis::CancelGesture() {
  // mVelocityQueue is controller-thread only
  APZThreadUtils::AssertOnControllerThread();

  AXIS_LOG("%p|%s cancelling touch, clearing velocity queue\n",
    mAsyncPanZoomController, Name());
  mVelocity = 0.0f;
  while (!mVelocityQueue.IsEmpty()) {
    mVelocityQueue.RemoveElementAt(0);
  }
}

bool Axis::CanScroll() const {
  return GetPageLength() - GetCompositionLength() > COORDINATE_EPSILON;
}

bool Axis::CanScroll(ParentLayerCoord aDelta) const
{
  if (!CanScroll() || mAxisLocked) {
    return false;
  }

  return DisplacementWillOverscrollAmount(aDelta) != aDelta;
}

CSSCoord Axis::ClampOriginToScrollableRect(CSSCoord aOrigin) const
{
  CSSToParentLayerScale zoom = GetScaleForAxis(GetFrameMetrics().GetZoom());
  ParentLayerCoord origin = aOrigin * zoom;

  ParentLayerCoord result;
  if (origin < GetPageStart()) {
    result = GetPageStart();
  } else if (origin + GetCompositionLength() > GetPageEnd()) {
    result = GetPageEnd() - GetCompositionLength();
  } else {
    result = origin;
  }

  return result / zoom;
}

bool Axis::CanScrollNow() const {
  return !mAxisLocked && CanScroll();
}

bool Axis::FlingApplyFrictionOrCancel(const TimeDuration& aDelta,
                                      float aFriction,
                                      float aThreshold) {
  if (fabsf(mVelocity) <= aThreshold) {
    // If the velocity is very low, just set it to 0 and stop the fling,
    // otherwise we'll just asymptotically approach 0 and the user won't
    // actually see any changes.
    mVelocity = 0.0f;
    return false;
  } else {
    mVelocity *= pow(1.0f - aFriction, float(aDelta.ToMilliseconds()));
  }
  AXIS_LOG("%p|%s reduced velocity to %f due to friction\n",
    mAsyncPanZoomController, Name(), mVelocity);
  return true;
}

ParentLayerCoord Axis::DisplacementWillOverscrollAmount(ParentLayerCoord aDisplacement) const {
  ParentLayerCoord newOrigin = GetOrigin() + aDisplacement;
  ParentLayerCoord newCompositionEnd = GetCompositionEnd() + aDisplacement;
  // If the current pan plus a displacement takes the window to the left of or
  // above the current page rect.
  bool minus = newOrigin < GetPageStart();
  // If the current pan plus a displacement takes the window to the right of or
  // below the current page rect.
  bool plus = newCompositionEnd > GetPageEnd();
  if (minus && plus) {
    // Don't handle overscrolled in both directions; a displacement can't cause
    // this, it must have already been zoomed out too far.
    return 0;
  }
  if (minus) {
    return newOrigin - GetPageStart();
  }
  if (plus) {
    return newCompositionEnd - GetPageEnd();
  }
  return 0;
}

CSSCoord Axis::ScaleWillOverscrollAmount(float aScale, CSSCoord aFocus) const {
  // Internally, do computations in ParentLayer coordinates *before* the scale
  // is applied.
  CSSToParentLayerScale zoom = GetFrameMetrics().GetZoom().ToScaleFactor();
  ParentLayerCoord focus = aFocus * zoom;
  ParentLayerCoord originAfterScale = (GetOrigin() + focus) - (focus / aScale);

  bool both = ScaleWillOverscrollBothSides(aScale);
  bool minus = GetPageStart() - originAfterScale > COORDINATE_EPSILON;
  bool plus = (originAfterScale + (GetCompositionLength() / aScale)) - GetPageEnd() > COORDINATE_EPSILON;

  if ((minus && plus) || both) {
    // If we ever reach here it's a bug in the client code.
    MOZ_ASSERT(false, "In an OVERSCROLL_BOTH condition in ScaleWillOverscrollAmount");
    return 0;
  }
  if (minus) {
    return (originAfterScale - GetPageStart()) / zoom;
  }
  if (plus) {
    return (originAfterScale + (GetCompositionLength() / aScale) - GetPageEnd()) / zoom;
  }
  return 0;
}

bool Axis::IsAxisLocked() const {
  return mAxisLocked;
}

float Axis::GetVelocity() const {
  return mAxisLocked ? 0 : mVelocity;
}

void Axis::SetVelocity(float aVelocity) {
  AXIS_LOG("%p|%s direct-setting velocity to %f\n",
    mAsyncPanZoomController, Name(), aVelocity);
  mVelocity = aVelocity;
}

ParentLayerCoord Axis::GetCompositionEnd() const {
  return GetOrigin() + GetCompositionLength();
}

ParentLayerCoord Axis::GetPageEnd() const {
  return GetPageStart() + GetPageLength();
}

ParentLayerCoord Axis::GetOrigin() const {
  ParentLayerPoint origin = GetFrameMetrics().GetScrollOffset() * GetFrameMetrics().GetZoom();
  return GetPointOffset(origin);
}

ParentLayerCoord Axis::GetCompositionLength() const {
  return GetRectLength(GetFrameMetrics().GetCompositionBounds());
}

ParentLayerCoord Axis::GetPageStart() const {
  ParentLayerRect pageRect = GetFrameMetrics().GetExpandedScrollableRect() * GetFrameMetrics().GetZoom();
  return GetRectOffset(pageRect);
}

ParentLayerCoord Axis::GetPageLength() const {
  ParentLayerRect pageRect = GetFrameMetrics().GetExpandedScrollableRect() * GetFrameMetrics().GetZoom();
  return GetRectLength(pageRect);
}

bool Axis::ScaleWillOverscrollBothSides(float aScale) const {
  const FrameMetrics& metrics = GetFrameMetrics();
  ParentLayerRect screenCompositionBounds = metrics.GetCompositionBounds()
                                          / ParentLayerToParentLayerScale(aScale);
  return GetRectLength(screenCompositionBounds) - GetPageLength() > COORDINATE_EPSILON;
}

const FrameMetrics& Axis::GetFrameMetrics() const {
  return mAsyncPanZoomController->GetFrameMetrics();
}


AxisX::AxisX(AsyncPanZoomController* aAsyncPanZoomController)
  : Axis(aAsyncPanZoomController)
{

}

ParentLayerCoord AxisX::GetPointOffset(const ParentLayerPoint& aPoint) const
{
  return aPoint.x;
}

ParentLayerCoord AxisX::GetRectLength(const ParentLayerRect& aRect) const
{
  return aRect.width;
}

ParentLayerCoord AxisX::GetRectOffset(const ParentLayerRect& aRect) const
{
  return aRect.x;
}

CSSToParentLayerScale AxisX::GetScaleForAxis(const CSSToParentLayerScale2D& aScale) const
{
  return CSSToParentLayerScale(aScale.xScale);
}

ScreenPoint AxisX::MakePoint(ScreenCoord aCoord) const
{
  return ScreenPoint(aCoord, 0);
}

const char* AxisX::Name() const
{
  return "X";
}

AxisY::AxisY(AsyncPanZoomController* aAsyncPanZoomController)
  : Axis(aAsyncPanZoomController)
{

}

ParentLayerCoord AxisY::GetPointOffset(const ParentLayerPoint& aPoint) const
{
  return aPoint.y;
}

ParentLayerCoord AxisY::GetRectLength(const ParentLayerRect& aRect) const
{
  return aRect.height;
}

ParentLayerCoord AxisY::GetRectOffset(const ParentLayerRect& aRect) const
{
  return aRect.y;
}

CSSToParentLayerScale AxisY::GetScaleForAxis(const CSSToParentLayerScale2D& aScale) const
{
  return CSSToParentLayerScale(aScale.yScale);
}

ScreenPoint AxisY::MakePoint(ScreenCoord aCoord) const
{
  return ScreenPoint(0, aCoord);
}

const char* AxisY::Name() const
{
  return "Y";
}

} // namespace layers
} // namespace mozilla

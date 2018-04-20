/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Axis.h"

#include <math.h>                       // for fabsf, pow, powf
#include <algorithm>                    // for max

#include "APZCTreeManager.h"            // for APZCTreeManager
#include "AsyncPanZoomController.h"     // for AsyncPanZoomController
#include "mozilla/layers/APZThreadUtils.h" // for AssertOnControllerThread
#include "FrameMetrics.h"               // for FrameMetrics
#include "mozilla/Attributes.h"         // for final
#include "mozilla/ComputedTimingFunction.h" // for ComputedTimingFunction
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

// When we compute the velocity we do so by taking two input events and
// dividing the distance delta over the time delta. In some cases the time
// delta can be really small, which can make the velocity computation very
// volatile. To avoid this we impose a minimum time delta below which we do
// not recompute the velocity.
const uint32_t MIN_VELOCITY_SAMPLE_TIME_MS = 5;

bool FuzzyEqualsCoordinate(float aValue1, float aValue2)
{
  return FuzzyEqualsAdditive(aValue1, aValue2, COORDINATE_EPSILON)
      || FuzzyEqualsMultiplicative(aValue1, aValue2);
}

extern StaticAutoPtr<ComputedTimingFunction> gVelocityCurveFunction;

Axis::Axis(AsyncPanZoomController* aAsyncPanZoomController)
  : mPos(0),
    mVelocitySampleTimeMs(0),
    mVelocitySamplePos(0),
    mVelocity(0.0f),
    mAxisLocked(false),
    mAsyncPanZoomController(aAsyncPanZoomController),
    mOverscroll(0),
    mMSDModel(0.0, 0.0, 0.0, 400.0, 1.2)
{
}

float Axis::ToLocalVelocity(float aVelocityInchesPerMs) const {
  ScreenPoint velocity = MakePoint(aVelocityInchesPerMs * mAsyncPanZoomController->GetDPI());
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

  if (aTimestampMs <= mVelocitySampleTimeMs + MIN_VELOCITY_SAMPLE_TIME_MS) {
    // See also the comment on MIN_VELOCITY_SAMPLE_TIME_MS.
    // We still update mPos so that the positioning is correct (and we don't run
    // into problems like bug 1042734) but the velocity will remain where it was.
    // In particular we don't update either mVelocitySampleTimeMs or
    // mVelocitySamplePos so that eventually when we do get an event with the
    // required time delta we use the corresponding distance delta as well.
    AXIS_LOG("%p|%s skipping velocity computation for small time delta %dms\n",
        mAsyncPanZoomController, Name(), (aTimestampMs - mVelocitySampleTimeMs));
    mPos = aPos;
    return;
  }

  float newVelocity = mAxisLocked ? 0.0f : (float)(mVelocitySamplePos - aPos + aAdditionalDelta) / (float)(aTimestampMs - mVelocitySampleTimeMs);

  newVelocity = ApplyFlingCurveToVelocity(newVelocity);

  AXIS_LOG("%p|%s updating velocity to %f with touch\n",
    mAsyncPanZoomController, Name(), newVelocity);
  mVelocity = newVelocity;
  mPos = aPos;
  mVelocitySampleTimeMs = aTimestampMs;
  mVelocitySamplePos = aPos;

  AddVelocityToQueue(aTimestampMs, mVelocity);
}

float Axis::ApplyFlingCurveToVelocity(float aVelocity) const {
  float newVelocity = aVelocity;
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
        float funcOutput =
          gVelocityCurveFunction->GetValue(funcInput,
            ComputedTimingFunction::BeforeFlag::Unset);
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

  return newVelocity;
}

void Axis::AddVelocityToQueue(uint32_t aTimestampMs, float aVelocity) {
  mVelocityQueue.AppendElement(std::make_pair(aTimestampMs, aVelocity));
  if (mVelocityQueue.Length() > gfxPrefs::APZMaxVelocityQueueSize()) {
    mVelocityQueue.RemoveElementAt(0);
  }
}

void Axis::HandleTouchVelocity(uint32_t aTimestampMs, float aSpeed) {
  // mVelocityQueue is controller-thread only
  APZThreadUtils::AssertOnControllerThread();

  mVelocity = ApplyFlingCurveToVelocity(aSpeed);
  mVelocitySampleTimeMs = aTimestampMs;

  AddVelocityToQueue(aTimestampMs, mVelocity);
}

void Axis::StartTouch(ParentLayerCoord aPos, uint32_t aTimestampMs) {
  mStartPos = aPos;
  mPos = aPos;
  mVelocitySampleTimeMs = aTimestampMs;
  mVelocitySamplePos = aPos;
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
  // 'resistanceFactor' is a value between 0 and 1/16, which:
  //   - tends to 1/16 as the existing overscroll tends to 0
  //   - tends to 0 as the existing overscroll tends to the composition length
  // The actual overscroll is the requested overscroll multiplied by this
  // factor.
  float resistanceFactor = (1 - fabsf(GetOverscroll()) / GetCompositionLength()) / 16;
  float result = resistanceFactor < 0 ? ParentLayerCoord(0) : aRequestedOverscroll * resistanceFactor;
  result = clamped(result, -8.0f, 8.0f);
  return result;
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
      MOZ_CRASH("GFX: Overscroll issue > 0");
    }
#endif
    MOZ_ASSERT(mOverscroll >= 0);
  } else if (aOverscroll < 0) {
#ifdef DEBUG
    if (!FuzzyEqualsCoordinate(GetOrigin().value, GetPageStart().value)) {
      nsPrintfCString message("composition origin (%f) is not equal (within error) to page origin (%f)\n",
                              GetOrigin().value, GetPageStart().value);
      NS_ASSERTION(false, message.get());
      MOZ_CRASH("GFX: Overscroll issue < 0");
    }
#endif
    MOZ_ASSERT(mOverscroll <= 0);
  }
  mOverscroll += aOverscroll;
}

ParentLayerCoord Axis::GetOverscroll() const {
  return mOverscroll;
}

void Axis::StartOverscrollAnimation(float aVelocity) {
  aVelocity = clamped(aVelocity / 2.0f, -20.0f, 20.0f);
  SetVelocity(aVelocity);
  mMSDModel.SetPosition(mOverscroll);
  // Convert velocity from ParentLayerCoords/millisecond to ParentLayerCoords/second.
  mMSDModel.SetVelocity(mVelocity * 1000.0);
}

void Axis::EndOverscrollAnimation() {
  mMSDModel.SetPosition(0.0);
  mMSDModel.SetVelocity(0.0);
}

bool Axis::SampleOverscrollAnimation(const TimeDuration& aDelta) {
  mMSDModel.Simulate(aDelta);
  mOverscroll = mMSDModel.GetPosition();

  if (mMSDModel.IsFinished(1.0)) {
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

  mAxisLocked = false;
  mVelocity = 0;
  int count = 0;
  for (const auto& e : mVelocityQueue) {
    uint32_t timeDelta = (aTimestampMs - e.first);
    if (timeDelta < gfxPrefs::APZVelocityRelevanceTime()) {
      count++;
      mVelocity += e.second;
    }
  }
  mVelocityQueue.Clear();
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
  mVelocityQueue.Clear();
}

bool Axis::CanScroll() const {
  return GetPageLength() - GetCompositionLength() > COORDINATE_EPSILON;
}

bool Axis::CanScroll(ParentLayerCoord aDelta) const
{
  if (!CanScroll() || mAxisLocked) {
    return false;
  }

  return fabs(DisplacementWillOverscrollAmount(aDelta) - aDelta) > COORDINATE_EPSILON;
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
    return aOrigin;
  }

  return result / zoom;
}

bool Axis::CanScrollNow() const {
  return !mAxisLocked && CanScroll();
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

ParentLayerCoord Axis::GetScrollRangeEnd() const {
  return GetPageEnd() - GetCompositionLength();
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

const ScrollMetadata& Axis::GetScrollMetadata() const {
  return mAsyncPanZoomController->GetScrollMetadata();
}

bool Axis::OverscrollBehaviorAllowsHandoff() const {
  // Scroll handoff is a "non-local" overscroll behavior, so it's allowed
  // with "auto" and disallowed with "contain" and "none".
  return GetOverscrollBehavior() == OverscrollBehavior::Auto;
}

bool Axis::OverscrollBehaviorAllowsOverscrollEffect() const {
  // An overscroll effect is a "local" overscroll behavior, so it's allowed
  // with "auto" and "contain" and disallowed with "none".
  return GetOverscrollBehavior() != OverscrollBehavior::None;
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
  return aRect.Width();
}

ParentLayerCoord AxisX::GetRectOffset(const ParentLayerRect& aRect) const
{
  return aRect.X();
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

bool AxisX::CanScrollTo(Side aSide) const
{
  switch (aSide) {
    case eSideLeft:
      return CanScroll(-COORDINATE_EPSILON * 2);
    case eSideRight:
      return CanScroll(COORDINATE_EPSILON * 2);
    default:
      MOZ_ASSERT_UNREACHABLE("aSide is out of valid values");
      return false;
  }
}

OverscrollBehavior AxisX::GetOverscrollBehavior() const
{
  return GetScrollMetadata().GetOverscrollBehavior().mBehaviorX;
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
  return aRect.Height();
}

ParentLayerCoord AxisY::GetRectOffset(const ParentLayerRect& aRect) const
{
  return aRect.Y();
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

bool AxisY::CanScrollTo(Side aSide) const
{
  switch (aSide) {
    case eSideTop:
      return CanScroll(-COORDINATE_EPSILON * 2);
    case eSideBottom:
      return CanScroll(COORDINATE_EPSILON * 2);
    default:
      MOZ_ASSERT_UNREACHABLE("aSide is out of valid values");
      return false;
  }
}

OverscrollBehavior AxisY::GetOverscrollBehavior() const
{
  return GetScrollMetadata().GetOverscrollBehavior().mBehaviorY;
}

} // namespace layers
} // namespace mozilla

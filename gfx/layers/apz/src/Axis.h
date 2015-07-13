/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_Axis_h
#define mozilla_layers_Axis_h

#include <sys/types.h>                  // for int32_t
#include "APZUtils.h"
#include "Units.h"
#include "mozilla/TimeStamp.h"          // for TimeDuration
#include "nsTArray.h"                   // for nsTArray

namespace mozilla {
namespace layers {

const float EPSILON = 0.0001f;

/**
 * Compare two coordinates for equality, accounting for rounding error.
 * Use both FuzzyEqualsAdditive() with COORDINATE_EPISLON, which accounts for
 * things like the error introduced by rounding during a round-trip to app
 * units, and FuzzyEqualsMultiplicative(), which accounts for accumulated error
 * due to floating-point operations (which can be larger than COORDINATE_EPISLON
 * for sufficiently large coordinate values).
 */
bool FuzzyEqualsCoordinate(float aValue1, float aValue2);

struct FrameMetrics;
class AsyncPanZoomController;

/**
 * Helper class to maintain each axis of movement (X,Y) for panning and zooming.
 * Note that everything here is specific to one axis; that is, the X axis knows
 * nothing about the Y axis and vice versa.
 */
class Axis {
public:
  explicit Axis(AsyncPanZoomController* aAsyncPanZoomController);

  /**
   * Notify this Axis that a new touch has been received, including a timestamp
   * for when the touch was received. This triggers a recalculation of velocity.
   */
  void UpdateWithTouchAtDevicePoint(ParentLayerCoord aPos, uint32_t aTimestampMs);

  /**
   * Notify this Axis that a touch has begun, i.e. the user has put their finger
   * on the screen but has not yet tried to pan.
   */
  void StartTouch(ParentLayerCoord aPos, uint32_t aTimestampMs);

  /**
   * Notify this Axis that a touch has ended gracefully. This may perform
   * recalculations of the axis velocity.
   */
  void EndTouch(uint32_t aTimestampMs);

  /**
   * Notify this Axis that a touch has ended forcefully. Useful for stopping
   * flings when a user puts their finger down in the middle of one (i.e. to
   * stop a previous touch including its fling so that a new one can take its
   * place).
   */
  void CancelTouch();

  /**
   * Takes a requested displacement to the position of this axis, and adjusts it
   * to account for overscroll (which might decrease the displacement; this is
   * to prevent the viewport from overscrolling the page rect), and axis locking
   * (which might prevent any displacement from happening). If overscroll
   * ocurred, its amount is written to |aOverscrollAmountOut|.
   * The |aDisplacementOut| parameter is set to the adjusted
   * displacement, and the function returns true iff internal overscroll amounts
   * were changed.
   */
  bool AdjustDisplacement(ParentLayerCoord aDisplacement,
                          /* ParentLayerCoord */ float& aDisplacementOut,
                          /* ParentLayerCoord */ float& aOverscrollAmountOut,
                          bool aForceOverscroll = false);

  /**
   * Overscrolls this axis by the requested amount in the requested direction.
   * The axis must be at the end of its scroll range in this direction.
   */
  void OverscrollBy(ParentLayerCoord aOverscroll);

  /**
   * Return the amount of overscroll on this axis, in ParentLayer pixels.
   *
   * If this amount is nonzero, the relevant component of
   * mAsyncPanZoomController->mFrameMetrics.mScrollOffset must be at its
   * extreme allowed value in the relevant direction (that is, it must be at
   * its maximum value if we are overscrolled at our composition length, and
   * at its minimum value if we are overscrolled at the origin).
   */
  ParentLayerCoord GetOverscroll() const;

  /**
   * Start an overscroll animation with the given initial velocity.
   */
  void StartOverscrollAnimation(float aVelocity);

  /**
   * Sample the snap-back animation to relieve overscroll.
   * |aDelta| is the time since the last sample.
   */
  bool SampleOverscrollAnimation(const TimeDuration& aDelta);

  /**
   * Stop an overscroll animation.
   */
  void EndOverscrollAnimation();

  /**
   * Return whether this axis is overscrolled in either direction.
   */
  bool IsOverscrolled() const;

  /**
   * Clear any overscroll amount on this axis.
   */
  void ClearOverscroll();

  /**
   * Gets the starting position of the touch supplied in StartTouch().
   */
  ParentLayerCoord PanStart() const;

  /**
   * Gets the distance between the starting position of the touch supplied in
   * StartTouch() and the current touch from the last
   * UpdateWithTouchAtDevicePoint().
   */
  ParentLayerCoord PanDistance() const;

  /**
   * Gets the distance between the starting position of the touch supplied in
   * StartTouch() and the supplied position.
   */
  ParentLayerCoord PanDistance(ParentLayerCoord aPos) const;

  /**
   * Applies friction during a fling, or cancels the fling if the velocity is
   * too low. Returns true if the fling should continue to another frame, or
   * false if it should end.
   * |aDelta| is the amount of time that has passed since the last time
   * friction was applied.
   * |aFriction| is the amount of friction to apply.
   * |aThreshold| is the velocity below which the fling is cancelled.
   */
  bool FlingApplyFrictionOrCancel(const TimeDuration& aDelta,
                                  float aFriction,
                                  float aThreshold);

  /**
   * Returns true if the page has room to be scrolled along this axis.
   */
  bool CanScroll() const;

  /**
   * Returns whether this axis can scroll any more in a particular direction.
   */
  bool CanScroll(double aDelta) const;

  /**
   * Returns true if the page has room to be scrolled along this axis
   * and this axis is not scroll-locked.
   */
  bool CanScrollNow() const;

  /**
   * Clamp a point to the page's scrollable bounds. That is, a scroll
   * destination to the returned point will not contain any overscroll.
   */
  CSSCoord ClampOriginToScrollableRect(CSSCoord aOrigin) const;

  void SetAxisLocked(bool aAxisLocked) { mAxisLocked = aAxisLocked; }

  /**
   * Gets the raw velocity of this axis at this moment.
   */
  float GetVelocity() const;

  /**
   * Sets the raw velocity of this axis at this moment.
   * Intended to be called only when the axis "takes over" a velocity from
   * another APZC, in which case there are no touch points available to call
   * UpdateWithTouchAtDevicePoint. In other circumstances,
   * UpdateWithTouchAtDevicePoint should be used and the velocity calculated
   * there.
   */
  void SetVelocity(float aVelocity);

  /**
   * If a displacement will overscroll the axis, this returns the amount and in
   * what direction.
   */
  ParentLayerCoord DisplacementWillOverscrollAmount(ParentLayerCoord aDisplacement) const;

  /**
   * If a scale will overscroll the axis, this returns the amount and in what
   * direction.
   *
   * |aFocus| is the point at which the scale is focused at. We will offset the
   * scroll offset in such a way that it remains in the same place on the page
   * relative.
   *
   * Note: Unlike most other functions in Axis, this functions operates in
   *       CSS coordinates so there is no confusion as to whether the ParentLayer
   *       coordinates it operates in are before or after the scale is applied.
   */
  CSSCoord ScaleWillOverscrollAmount(float aScale, CSSCoord aFocus) const;

  /**
   * Checks if an axis will overscroll in both directions by computing the
   * content rect and checking that its height/width (depending on the axis)
   * does not overextend past the viewport.
   *
   * This gets called by ScaleWillOverscroll().
   */
  bool ScaleWillOverscrollBothSides(float aScale) const;

  ParentLayerCoord GetOrigin() const;
  ParentLayerCoord GetCompositionLength() const;
  ParentLayerCoord GetPageStart() const;
  ParentLayerCoord GetPageLength() const;
  ParentLayerCoord GetCompositionEnd() const;
  ParentLayerCoord GetPageEnd() const;

  ParentLayerCoord GetPos() const { return mPos; }

  virtual ParentLayerCoord GetPointOffset(const ParentLayerPoint& aPoint) const = 0;
  virtual ParentLayerCoord GetRectLength(const ParentLayerRect& aRect) const = 0;
  virtual ParentLayerCoord GetRectOffset(const ParentLayerRect& aRect) const = 0;
  virtual CSSToParentLayerScale GetScaleForAxis(const CSSToParentLayerScale2D& aScale) const = 0;

  virtual ScreenPoint MakePoint(ScreenCoord aCoord) const = 0;

  virtual const char* Name() const = 0;

protected:
  ParentLayerCoord mPos;
  uint32_t mPosTimeMs;
  ParentLayerCoord mStartPos;
  float mVelocity;      // Units: ParentLayerCoords per millisecond
  bool mAxisLocked;     // Whether movement on this axis is locked.
  AsyncPanZoomController* mAsyncPanZoomController;

  // mOverscroll is the displacement of an oscillating spring from its resting
  // state. The resting state moves as the overscroll animation progresses.
  ParentLayerCoord mOverscroll;
  // Used to record the initial overscroll when we start sampling for animation.
  ParentLayerCoord mFirstOverscrollAnimationSample;
  // These two variables are used in combination to make sure that
  // GetOverscroll() never changes sign during animation. This is necessary,
  // as mOverscroll itself oscillates around zero during animation.
  // If we're not sampling overscroll animation, mOverscrollScale will be 1.0
  // and mLastOverscrollPeak will be zero.
  // If we are animating, after the overscroll reaches its peak,
  // mOverscrollScale will be 2.0 and mLastOverscrollPeak will store the amount
  // of overscroll at the last peak of the oscillation. Together, these values
  // guarantee that the result of GetOverscroll() never changes sign.
  ParentLayerCoord mLastOverscrollPeak;
  float mOverscrollScale;

  // A queue of (timestamp, velocity) pairs; these are the historical
  // velocities at the given timestamps. Timestamps are in milliseconds,
  // velocities are in screen pixels per ms. This member can only be
  // accessed on the controller/UI thread.
  nsTArray<std::pair<uint32_t, float> > mVelocityQueue;

  const FrameMetrics& GetFrameMetrics() const;

  // Adjust a requested overscroll amount for resistance, yielding a smaller
  // actual overscroll amount.
  ParentLayerCoord ApplyResistance(ParentLayerCoord aOverscroll) const;

  // Clear the state associated with an overscroll animation.
  void ClearOverscrollAnimationState();

  // Helper function for SampleOverscrollAnimation().
  void StepOverscrollAnimation(double aStepDurationMilliseconds);

  // Convert a velocity from global inches/ms into ParentLayerCoords/ms.
  float ToLocalVelocity(float aVelocityInchesPerMs) const;
};

class AxisX : public Axis {
public:
  explicit AxisX(AsyncPanZoomController* mAsyncPanZoomController);
  virtual ParentLayerCoord GetPointOffset(const ParentLayerPoint& aPoint) const override;
  virtual ParentLayerCoord GetRectLength(const ParentLayerRect& aRect) const override;
  virtual ParentLayerCoord GetRectOffset(const ParentLayerRect& aRect) const override;
  virtual CSSToParentLayerScale GetScaleForAxis(const CSSToParentLayerScale2D& aScale) const override;
  virtual ScreenPoint MakePoint(ScreenCoord aCoord) const override;
  virtual const char* Name() const override;
};

class AxisY : public Axis {
public:
  explicit AxisY(AsyncPanZoomController* mAsyncPanZoomController);
  virtual ParentLayerCoord GetPointOffset(const ParentLayerPoint& aPoint) const override;
  virtual ParentLayerCoord GetRectLength(const ParentLayerRect& aRect) const override;
  virtual ParentLayerCoord GetRectOffset(const ParentLayerRect& aRect) const override;
  virtual CSSToParentLayerScale GetScaleForAxis(const CSSToParentLayerScale2D& aScale) const override;
  virtual ScreenPoint MakePoint(ScreenCoord aCoord) const override;
  virtual const char* Name() const override;
};

} // namespace layers
} // namespace mozilla

#endif

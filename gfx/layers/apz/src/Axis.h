/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_Axis_h
#define mozilla_layers_Axis_h

#include <sys/types.h>                  // for int32_t
#include "Units.h"
#include "mozilla/TimeStamp.h"          // for TimeDuration
#include "nsTArray.h"                   // for nsTArray

namespace mozilla {
namespace layers {

const float EPSILON = 0.0001f;

// Epsilon to be used when comparing 'float' coordinate values
// with FuzzyEqualsAdditive. The rationale is that 'float' has 7 decimal
// digits of precision, and coordinate values should be no larger than in the
// ten thousands. Note also that the smallest legitimate difference in page
// coordinates is 1 app unit, which is 1/60 of a (CSS pixel), so this epsilon
// isn't too large.
const float COORDINATE_EPSILON = 0.01f;

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
  void UpdateWithTouchAtDevicePoint(ScreenCoord aPos, uint32_t aTimestampMs);

  /**
   * Notify this Axis that a touch has begun, i.e. the user has put their finger
   * on the screen but has not yet tried to pan.
   */
  void StartTouch(ScreenCoord aPos, uint32_t aTimestampMs);

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
  bool AdjustDisplacement(ScreenCoord aDisplacement,
                          /* ScreenCoord */ float& aDisplacementOut,
                          /* ScreenCoord */ float& aOverscrollAmountOut);

  /**
   * Overscrolls this axis by the requested amount in the requested direction.
   * The axis must be at the end of its scroll range in this direction.
   */
  void OverscrollBy(ScreenCoord aOverscroll);

  /**
   * Return the amount of overscroll on this axis, in Screen pixels.
   */
  ScreenCoord GetOverscroll() const;

  /**
   * Return whether the axis is in underscroll. See |mInUnderscroll|
   * for a detailed description.
   */
  bool IsInUnderscroll() const;

  /**
   * Sample the snap-back animation to relieve overscroll.
   * |aDelta| is the time since the last sample.
   */
  bool SampleOverscrollAnimation(const TimeDuration& aDelta);

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
  ScreenCoord PanStart() const;

  /**
   * Gets the distance between the starting position of the touch supplied in
   * StartTouch() and the current touch from the last
   * UpdateWithTouchAtDevicePoint().
   */
  ScreenCoord PanDistance() const;

  /**
   * Gets the distance between the starting position of the touch supplied in
   * StartTouch() and the supplied position.
   */
  ScreenCoord PanDistance(ScreenCoord aPos) const;

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
   * Returns true if the page has room to be scrolled along this axis
   * and this axis is not scroll-locked.
   */
  bool CanScrollNow() const;

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
  ScreenCoord DisplacementWillOverscrollAmount(ScreenCoord aDisplacement) const;

  /**
   * If a scale will overscroll the axis, this returns the amount and in what
   * direction.
   *
   * |aFocus| is the point at which the scale is focused at. We will offset the
   * scroll offset in such a way that it remains in the same place on the page
   * relative.
   *
   * Note: Unlike most other functions in Axis, this functions operates in
   *       CSS coordinates so there is no confusion as to whether the Screen
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

  ScreenCoord GetOrigin() const;
  ScreenCoord GetCompositionLength() const;
  ScreenCoord GetPageStart() const;
  ScreenCoord GetPageLength() const;
  ScreenCoord GetCompositionEnd() const;
  ScreenCoord GetPageEnd() const;

  ScreenCoord GetPos() const { return mPos; }

  virtual ScreenCoord GetPointOffset(const ScreenPoint& aPoint) const = 0;
  virtual ScreenCoord GetRectLength(const ScreenRect& aRect) const = 0;
  virtual ScreenCoord GetRectOffset(const ScreenRect& aRect) const = 0;

  virtual ScreenPoint MakePoint(ScreenCoord aCoord) const = 0;

protected:
  ScreenCoord mPos;
  uint32_t mPosTimeMs;
  ScreenCoord mStartPos;
  float mVelocity;      // Units: ScreenCoords per millisecond
  bool mAxisLocked;     // Whether movement on this axis is locked.
  AsyncPanZoomController* mAsyncPanZoomController;
  // The amount by which this axis is in overscroll, in Screen coordinates.
  // If this amount is nonzero, the relevant component of
  // mAsyncPanZoomController->mFrameMetrics.mScrollOffset must be at its
  // extreme allowed value in the relevant direction (that is, it must be at
  // its maximum value if we are overscrolled at our composition length, and
  // at its minimum value if we are overscrolled at the origin).
  // Note that if |mInUnderscroll| is true, the interpretation of this field
  // changes slightly (see below).
  ScreenCoord mOverscroll;
  // This flag is set when an overscroll animation is in a state where the
  // spring physics caused a snap-back movement to "overshoot" its target and
  // as a result the spring is stretched in a direction opposite to the one
  // when we were in overscroll. We call this situation "underscroll". When in
  // underscroll, mOverscroll can be nonzero, but rather than being
  // interpreted as overscroll (stretch) at the other end of the composition
  // bounds, it's interpeted as an "underscroll" (compression) at the same end.
  // This table summarizes what the possible combinations of mOverscroll and
  // mInUnderscroll mean:
  //
  //      mOverscroll  |  mInUnderscroll  |    Description
  //---------------------------------------------------------------------------
  //       negative    |      false       | The axis is overscrolled at
  //                   |                  | its origin. A stretch is applied
  //                   |                  | with the content fixed in place
  //                   |                  | at the origin.
  //---------------------------------------------------------------------------
  //       positive    |      false       | The axis is overscrolled at its
  //                   |                  | composition end. A stretch is
  //                   |                  | applied with the content fixed in
  //                   |                  | place at the composition end.
  //---------------------------------------------------------------------------
  //       positive    |      true        | The axis is underscrolled at its
  //                   |                  | origin. A compression is applied
  //                   |                  | with the content fixed in place
  //                   |                  | at the origin.
  //---------------------------------------------------------------------------
  //       negative    |      true        | The axis is underscrolled at its
  //                   |                  | composition end. A compression is
  //                   |                  | applied with the content fixed in
  //                   |                  | place at the composition end.
  //---------------------------------------------------------------------------
  bool mInUnderscroll;
  // A queue of (timestamp, velocity) pairs; these are the historical
  // velocities at the given timestamps. Timestamps are in milliseconds,
  // velocities are in screen pixels per ms. This member can only be
  // accessed on the controller/UI thread.
  nsTArray<std::pair<uint32_t, float> > mVelocityQueue;

  const FrameMetrics& GetFrameMetrics() const;

  // Adjust a requested overscroll amount for resistance, yielding a smaller
  // actual overscroll amount.
  ScreenCoord ApplyResistance(ScreenCoord aOverscroll) const;

  // Convert a velocity from global inches/ms into local ScreenCoords per ms
  float ToLocalVelocity(float aVelocityInchesPerMs);
};

class AxisX : public Axis {
public:
  explicit AxisX(AsyncPanZoomController* mAsyncPanZoomController);
  virtual ScreenCoord GetPointOffset(const ScreenPoint& aPoint) const MOZ_OVERRIDE;
  virtual ScreenCoord GetRectLength(const ScreenRect& aRect) const MOZ_OVERRIDE;
  virtual ScreenCoord GetRectOffset(const ScreenRect& aRect) const MOZ_OVERRIDE;
  virtual ScreenPoint MakePoint(ScreenCoord aCoord) const MOZ_OVERRIDE;
};

class AxisY : public Axis {
public:
  explicit AxisY(AsyncPanZoomController* mAsyncPanZoomController);
  virtual ScreenCoord GetPointOffset(const ScreenPoint& aPoint) const MOZ_OVERRIDE;
  virtual ScreenCoord GetRectLength(const ScreenRect& aRect) const MOZ_OVERRIDE;
  virtual ScreenCoord GetRectOffset(const ScreenRect& aRect) const MOZ_OVERRIDE;
  virtual ScreenPoint MakePoint(ScreenCoord aCoord) const MOZ_OVERRIDE;
};

}
}

#endif

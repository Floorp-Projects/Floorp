/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_Axis_h
#define mozilla_layers_Axis_h

#include <sys/types.h>                  // for int32_t
#include "Units.h"                      // for CSSRect, CSSPoint
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

  enum Overscroll {
    // Overscroll is not happening at all.
    OVERSCROLL_NONE = 0,
    // Overscroll is happening in the negative direction. This means either to
    // the left or to the top depending on the axis.
    OVERSCROLL_MINUS,
    // Overscroll is happening in the positive direction. This means either to
    // the right or to the bottom depending on the axis.
    OVERSCROLL_PLUS,
    // Overscroll is happening both ways. This only means something when the
    // page is scaled out to a smaller size than the viewport.
    OVERSCROLL_BOTH
  };

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
  bool AdjustDisplacement(CSSCoord aDisplacement,
                          CSSCoord& aDisplacementOut,
                          CSSCoord& aOverscrollAmountOut);

  /**
   * Overscrolls this axis by the requested amount in the requested direction.
   * The axis must be at the end of its scroll range in this direction.
   */
  void OverscrollBy(CSSCoord aOverscroll);

  /**
   * Return the amount of overscroll on this axis, in CSS pixels.
   */
  CSSCoord GetOverscroll() const;

  /**
   * Sample the snap-back animation to relieve overscroll.
   * |aDelta| is the time since the last sample.
   */
  bool SampleSnapBack(const TimeDuration& aDelta);

  /**
   * Return whether this axis is overscrolled in either direction.
   */
  bool IsOverscrolled() const;

  /**
   * Clear any overscroll amount on this axis.
   */
  void ClearOverscroll();

  /**
   * Gets the distance between the starting position of the touch supplied in
   * startTouch() and the current touch from the last
   * updateWithTouchAtDevicePoint().
   */
  float PanDistance();

  /**
   * Gets the distance between the starting position of the touch supplied in
   * startTouch() and the supplied position.
   */
  float PanDistance(ScreenCoord aPos);

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
  float GetVelocity();

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
   * Gets the overscroll state of the axis given an additional displacement.
   * That is to say, if the given displacement is applied, this will tell you
   * whether or not it will overscroll, and in what direction.
   */
  Overscroll DisplacementWillOverscroll(CSSCoord aDisplacement);

  /**
   * If a displacement will overscroll the axis, this returns the amount and in
   * what direction. Similar to GetExcess() but takes a displacement to apply.
   */
  CSSCoord DisplacementWillOverscrollAmount(CSSCoord aDisplacement);

  /**
   * If a scale will overscroll the axis, this returns the amount and in what
   * direction. Similar to GetExcess() but takes a displacement to apply.
   *
   * |aFocus| is the point at which the scale is focused at. We will offset the
   * scroll offset in such a way that it remains in the same place on the page
   * relative.
   */
  CSSCoord ScaleWillOverscrollAmount(float aScale, CSSCoord aFocus);

  /**
   * Checks if an axis will overscroll in both directions by computing the
   * content rect and checking that its height/width (depending on the axis)
   * does not overextend past the viewport.
   *
   * This gets called by ScaleWillOverscroll().
   */
  bool ScaleWillOverscrollBothSides(float aScale);

  CSSCoord GetOrigin() const;
  CSSCoord GetCompositionLength() const;
  CSSCoord GetPageStart() const;
  CSSCoord GetPageLength() const;
  CSSCoord GetCompositionEnd() const;
  CSSCoord GetPageEnd() const;

  ScreenCoord GetPos() const { return mPos; }

  virtual CSSCoord GetPointOffset(const CSSPoint& aPoint) const = 0;
  virtual CSSCoord GetRectLength(const CSSRect& aRect) const = 0;
  virtual CSSCoord GetRectOffset(const CSSRect& aRect) const = 0;

protected:
  ScreenCoord mPos;
  uint32_t mPosTimeMs;
  ScreenCoord mStartPos;
  float mVelocity;
  bool mAxisLocked;     // Whether movement on this axis is locked.
  AsyncPanZoomController* mAsyncPanZoomController;
  // The amount by which this axis is in overscroll, in CSS coordinates.
  // If this amount is nonzero, the relevant component of
  // mAsyncPanZoomController->mFrameMetrics.mScrollOffset must be at its
  // extreme allowed value in the relevant direction (that is, it must be at
  // its maximum value if mOverscroll is positive, and at its minimum value
  // if mOverscroll is negative).
  CSSCoord mOverscroll;
  // A queue of (timestamp, velocity) pairs; these are the historical
  // velocities at the given timestamps. Timestamps are in milliseconds,
  // velocities are in screen pixels per ms. This member can only be
  // accessed on the controller/UI thread.
  nsTArray<std::pair<uint32_t, float> > mVelocityQueue;

  const FrameMetrics& GetFrameMetrics() const;

  // Adjust a requested overscroll amount for resistance, yielding a smaller
  // actual overscroll amount.
  CSSCoord ApplyResistance(CSSCoord aOverscroll) const;
};

class AxisX : public Axis {
public:
  explicit AxisX(AsyncPanZoomController* mAsyncPanZoomController);
  virtual CSSCoord GetPointOffset(const CSSPoint& aPoint) const;
  virtual CSSCoord GetRectLength(const CSSRect& aRect) const;
  virtual CSSCoord GetRectOffset(const CSSRect& aRect) const;
};

class AxisY : public Axis {
public:
  explicit AxisY(AsyncPanZoomController* mAsyncPanZoomController);
  virtual CSSCoord GetPointOffset(const CSSPoint& aPoint) const;
  virtual CSSCoord GetRectLength(const CSSRect& aRect) const;
  virtual CSSCoord GetRectOffset(const CSSRect& aRect) const;
};

}
}

#endif

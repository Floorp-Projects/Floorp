/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_Axis_h
#define mozilla_layers_Axis_h

#include "nsGUIEvent.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/gfx/2D.h"
#include "nsTArray.h"
#include "Units.h"

namespace mozilla {
namespace layers {

class AsyncPanZoomController;

/**
 * Helper class to maintain each axis of movement (X,Y) for panning and zooming.
 * Note that everything here is specific to one axis; that is, the X axis knows
 * nothing about the Y axis and vice versa.
 */
class Axis {
public:
  Axis(AsyncPanZoomController* aAsyncPanZoomController);

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
   * Notify this Axis that a new touch has been received, including a time delta
   * indicating how long it has been since the previous one. This triggers a
   * recalculation of velocity.
   */
  void UpdateWithTouchAtDevicePoint(int32_t aPos, const TimeDuration& aTimeDelta);

  /**
   * Notify this Axis that a touch has begun, i.e. the user has put their finger
   * on the screen but has not yet tried to pan.
   */
  void StartTouch(int32_t aPos);

  /**
   * Notify this Axis that a touch has ended gracefully. This may perform
   * recalculations of the axis velocity.
   */
  void EndTouch();

  /**
   * Notify this Axis that a touch has ended forcefully. Useful for stopping
   * flings when a user puts their finger down in the middle of one (i.e. to
   * stop a previous touch including its fling so that a new one can take its
   * place).
   */
  void CancelTouch();

  /**
   * Gets displacement that should have happened since the previous touch.
   * Note: Does not reset the displacement. It gets recalculated on the next
   * UpdateWithTouchAtDevicePoint(), however it is not safe to assume this will
   * be the same on every call. This also checks for page boundaries and will
   * return an adjusted displacement to prevent the viewport from overscrolling
   * the page rect. An example of where this might matter is when you call it,
   * apply a displacement that takes you to the boundary of the page, then call
   * it again. The result will be different in this case.
   */
  float GetDisplacementForDuration(float aScale, const TimeDuration& aDelta);

  /**
   * Gets the distance between the starting position of the touch supplied in
   * startTouch() and the current touch from the last
   * updateWithTouchAtDevicePoint().
   */
  float PanDistance();

  /**
   * Applies friction during a fling, or cancels the fling if the velocity is
   * too low. Returns true if the fling should continue to another frame, or
   * false if it should end. |aDelta| is the amount of time that has passed
   * since the last time friction was applied.
   */
  bool FlingApplyFrictionOrCancel(const TimeDuration& aDelta);

  /**
   * Gets the overscroll state of the axis in its current position.
   */
  Overscroll GetOverscroll();

  /**
   * If there is overscroll, returns the amount. Sign depends on in what
   * direction it is overscrolling. Positive excess means that it is
   * overscrolling in the positive direction, whereas negative excess means
   * that it is overscrolling in the negative direction. If there is overscroll
   * in both directions, this returns 0; it assumes that you check
   * GetOverscroll() first.
   */
  float GetExcess();

  /**
   * Gets the factor of acceleration applied to the velocity, based on the
   * amount of flings that have been done successively.
   */
  float GetAccelerationFactor();

  /**
   * Gets the raw velocity of this axis at this moment.
   */
  float GetVelocity();

  /**
   * Gets the overscroll state of the axis given an additional displacement.
   * That is to say, if the given displacement is applied, this will tell you
   * whether or not it will overscroll, and in what direction.
   */
  Overscroll DisplacementWillOverscroll(float aDisplacement);

  /**
   * If a displacement will overscroll the axis, this returns the amount and in
   * what direction. Similar to getExcess() but takes a displacement to apply.
   */
  float DisplacementWillOverscrollAmount(float aDisplacement);

  /**
   * Gets the overscroll state of the axis given a scaling of the page. That is
   * to say, if the given scale is applied, this will tell you whether or not
   * it will overscroll, and in what direction.
   *
   * |aFocus| is the point at which the scale is focused at. We will offset the
   * scroll offset in such a way that it remains in the same place on the page
   * relative.
   */
  Overscroll ScaleWillOverscroll(float aScale, float aFocus);

  /**
   * If a scale will overscroll the axis, this returns the amount and in what
   * direction. Similar to getExcess() but takes a displacement to apply.
   *
   * |aFocus| is the point at which the scale is focused at. We will offset the
   * scroll offset in such a way that it remains in the same place on the page
   * relative.
   */
  float ScaleWillOverscrollAmount(float aScale, float aFocus);

  /**
   * Checks if an axis will overscroll in both directions by computing the
   * content rect and checking that its height/width (depending on the axis)
   * does not overextend past the viewport.
   *
   * This gets called by ScaleWillOverscroll().
   */
  bool ScaleWillOverscrollBothSides(float aScale);

  float GetOrigin();
  float GetCompositionLength();
  float GetPageStart();
  float GetPageLength();
  float GetCompositionEnd();
  float GetPageEnd();

  virtual float GetPointOffset(const CSSPoint& aPoint) = 0;
  virtual float GetRectLength(const CSSRect& aRect) = 0;
  virtual float GetRectOffset(const CSSRect& aRect) = 0;

protected:
  int32_t mPos;
  int32_t mStartPos;
  float mVelocity;
  // Acceleration is represented by an int, which is the power we raise a
  // constant to and then multiply the velocity by whenever it is sampled. We do
  // this only when we detect that the user wants to do a fast fling; that is,
  // they are flinging multiple times in a row very quickly, probably trying to
  // reach one of the extremes of the page.
  int32_t mAcceleration;
  AsyncPanZoomController* mAsyncPanZoomController;
  nsTArray<float> mVelocityQueue;
};

class AxisX : public Axis {
public:
  AxisX(AsyncPanZoomController* mAsyncPanZoomController);
  virtual float GetPointOffset(const CSSPoint& aPoint);
  virtual float GetRectLength(const CSSRect& aRect);
  virtual float GetRectOffset(const CSSRect& aRect);
};

class AxisY : public Axis {
public:
  AxisY(AsyncPanZoomController* mAsyncPanZoomController);
  virtual float GetPointOffset(const CSSPoint& aPoint);
  virtual float GetRectLength(const CSSRect& aRect);
  virtual float GetRectOffset(const CSSRect& aRect);
};

}
}

#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_layers_AutoDirWheelDeltaAdjuster_h__
#define __mozilla_layers_AutoDirWheelDeltaAdjuster_h__

#include "Axis.h"                         // for AxisX, AxisY, Side
#include "mozilla/WheelHandlingHelper.h"  // for AutoDirWheelDeltaAdjuster

namespace mozilla {
namespace layers {

/**
 * About AutoDirWheelDeltaAdjuster:
 * For an AutoDir wheel scroll, there's some situations where we should adjust a
 * wheel event's delta values. AutoDirWheelDeltaAdjuster converts delta values
 * for AutoDir scrolling. An AutoDir wheel scroll lets the user scroll a frame
 * with only one scrollbar, using either a vertical or a horzizontal wheel.
 * For more detail about the concept of AutoDir scrolling, see the comments in
 * AutoDirWheelDeltaAdjuster.
 *
 * This is the APZ implementation of AutoDirWheelDeltaAdjuster.
 */
class MOZ_STACK_CLASS APZAutoDirWheelDeltaAdjuster final
    : public AutoDirWheelDeltaAdjuster {
 public:
  /**
   * @param aDeltaX            DeltaX for a wheel event whose delta values will
   *                           be adjusted upon calling adjust() when
   *                           ShouldBeAdjusted() returns true.
   * @param aDeltaY            DeltaY for a wheel event, like DeltaX.
   * @param aAxisX             The X axis information provider for the current
   *                           frame, such as whether the frame can be scrolled
   *                           horizontally, leftwards or rightwards.
   * @param aAxisY             The Y axis information provider for the current
   *                           frame, such as whether the frame can be scrolled
   *                           vertically, upwards or downwards.
   * @param aIsHorizontalContentRightToLeft
   *                           Indicates whether the horizontal content starts
   *                           at rightside. This value will decide which edge
   *                           the adjusted scroll goes towards, in other words,
   *                           it will decide the sign of the adjusted delta
   *                           values). For detailed information, see
   *                           IsHorizontalContentRightToLeft() in
   *                           the base class AutoDirWheelDeltaAdjuster.
   */
  APZAutoDirWheelDeltaAdjuster(double& aDeltaX, double& aDeltaY,
                               const AxisX& aAxisX, const AxisY& aAxisY,
                               bool aIsHorizontalContentRightToLeft)
      : AutoDirWheelDeltaAdjuster(aDeltaX, aDeltaY),
        mAxisX(aAxisX),
        mAxisY(aAxisY),
        mIsHorizontalContentRightToLeft(aIsHorizontalContentRightToLeft) {}

 private:
  virtual bool CanScrollAlongXAxis() const override {
    return mAxisX.CanScroll();
  }
  virtual bool CanScrollAlongYAxis() const override {
    return mAxisY.CanScroll();
  }
  virtual bool CanScrollUpwards() const override {
    return mAxisY.CanScrollTo(eSideTop);
  }
  virtual bool CanScrollDownwards() const override {
    return mAxisY.CanScrollTo(eSideBottom);
  }
  virtual bool CanScrollLeftwards() const override {
    return mAxisX.CanScrollTo(eSideLeft);
  }
  virtual bool CanScrollRightwards() const override {
    return mAxisX.CanScrollTo(eSideRight);
  }
  virtual bool IsHorizontalContentRightToLeft() const override {
    return mIsHorizontalContentRightToLeft;
  }

  const AxisX& mAxisX;
  const AxisY& mAxisY;
  bool mIsHorizontalContentRightToLeft;
};

}  // namespace layers
}  // namespace mozilla

#endif  // __mozilla_layers_AutoDirWheelDeltaAdjuster_h__

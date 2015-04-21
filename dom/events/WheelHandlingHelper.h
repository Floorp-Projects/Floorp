/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WheelHandlingHelper_h_
#define mozilla_WheelHandlingHelper_h_

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsCoord.h"
#include "nsIFrame.h"
#include "nsPoint.h"

class nsIScrollableFrame;
class nsITimer;

namespace mozilla {

class EventStateManager;

/**
 * DeltaValues stores two delta values which are along X and Y axis.  This is
 * useful for arguments and results of some methods.
 */

struct DeltaValues
{
  DeltaValues()
    : deltaX(0.0)
    , deltaY(0.0)
  {
  }

  DeltaValues(double aDeltaX, double aDeltaY)
    : deltaX(aDeltaX)
    , deltaY(aDeltaY)
  {
  }

  explicit DeltaValues(WidgetWheelEvent* aEvent);

  double deltaX;
  double deltaY;
};

/**
 * WheelHandlingUtils provides some static methods which are useful at handling
 * wheel events.
 */

class WheelHandlingUtils
{
public:
  /**
   * Returns true if the scrollable frame can be scrolled to either aDirectionX
   * or aDirectionY along each axis.  Otherwise, false.
   */
  static bool CanScrollOn(nsIScrollableFrame* aScrollFrame,
                          double aDirectionX, double aDirectionY);

private:
  static bool CanScrollInRange(nscoord aMin, nscoord aValue, nscoord aMax,
                               double aDirection);
};

/**
 * ScrollbarsForWheel manages scrollbars state during wheel operation.
 * E.g., on some platforms, scrollbars should show only while user attempts to
 * scroll.  At that time, scrollbars which may be possible to scroll by
 * operation of wheel at the point should show temporarily.
 */

class ScrollbarsForWheel
{
public:
  static void PrepareToScrollText(EventStateManager* aESM,
                                  nsIFrame* aTargetFrame,
                                  WidgetWheelEvent* aEvent);
  static void SetActiveScrollTarget(nsIScrollableFrame* aScrollTarget);
  // Hide all scrollbars (both mActiveOwner's and mActivatedScrollTargets')
  static void MayInactivate();
  static void Inactivate();
  static bool IsActive();
  static void OwnWheelTransaction(bool aOwn);

protected:
  static const size_t kNumberOfTargets = 4;
  static const DeltaValues directions[kNumberOfTargets];
  static nsWeakFrame sActiveOwner;
  static nsWeakFrame sActivatedScrollTargets[kNumberOfTargets];
  static bool sHadWheelStart;
  static bool sOwnWheelTransaction;


  /**
   * These two methods are called upon NS_WHEEL_START/NS_WHEEL_STOP events
   * to show/hide the right scrollbars.
   */
  static void TemporarilyActivateAllPossibleScrollTargets(
                EventStateManager* aESM,
                nsIFrame* aTargetFrame,
                WidgetWheelEvent* aEvent);
  static void DeactivateAllTemporarilyActivatedScrollTargets();
};

/**
 * WheelTransaction manages a series of wheel events as a transaction.
 * While in a transaction, every wheel event should scroll the same scrollable
 * element even if a different scrollable element is under the mouse cursor.
 *
 * Additionally, this class also manages wheel scroll speed acceleration.
 */

class WheelTransaction
{
public:
  static nsIFrame* GetTargetFrame() { return sTargetFrame; }
  static void BeginTransaction(nsIFrame* aTargetFrame,
                               WidgetWheelEvent* aEvent);
  // Be careful, UpdateTransaction may fire a DOM event, therefore, the target
  // frame might be destroyed in the event handler.
  static bool UpdateTransaction(WidgetWheelEvent* aEvent);
  static void MayEndTransaction();
  static void EndTransaction();
  static void OnEvent(WidgetEvent* aEvent);
  static void Shutdown();
  static uint32_t GetTimeoutTime();

  static void OwnScrollbars(bool aOwn);

  static DeltaValues AccelerateWheelDelta(WidgetWheelEvent* aEvent,
                                          bool aAllowScrollSpeedOverride);

protected:
  static const uint32_t kScrollSeriesTimeout = 80; // in milliseconds
  static nsIntPoint GetScreenPoint(WidgetGUIEvent* aEvent);
  static void OnFailToScrollTarget();
  static void OnTimeout(nsITimer* aTimer, void* aClosure);
  static void SetTimeout();
  static uint32_t GetIgnoreMoveDelayTime();
  static int32_t GetAccelerationStart();
  static int32_t GetAccelerationFactor();
  static DeltaValues OverrideSystemScrollSpeed(WidgetWheelEvent* aEvent);
  static double ComputeAcceleratedWheelDelta(double aDelta, int32_t aFactor);
  static bool OutOfTime(uint32_t aBaseTime, uint32_t aThreshold);

  static nsWeakFrame sTargetFrame;
  static uint32_t sTime; // in milliseconds
  static uint32_t sMouseMoved; // in milliseconds
  static nsITimer* sTimer;
  static int32_t sScrollSeriesCounter;
  static bool sOwnScrollbars;
};

} // namespace mozilla

#endif // mozilla_WheelHandlingHelper_h_

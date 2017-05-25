/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
   * Returns true if aFrame is a scrollable frame and it can be scrolled to
   * either aDirectionX or aDirectionY along each axis.  Or if aFrame is a
   * plugin frame (in this case, aDirectionX and aDirectionY are ignored).
   * Otherwise, false.
   */
  static bool CanScrollOn(nsIFrame* aFrame,
                          double aDirectionX, double aDirectionY);
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
  static AutoWeakFrame sActiveOwner;
  static AutoWeakFrame sActivatedScrollTargets[kNumberOfTargets];
  static bool sHadWheelStart;
  static bool sOwnWheelTransaction;


  /**
   * These two methods are called upon eWheelOperationStart/eWheelOperationEnd
   * events to show/hide the right scrollbars.
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
  static void EndTransaction();
  /**
   * WillHandleDefaultAction() is called before handling aWheelEvent on
   * aTargetFrame.
   *
   * @return    false if the caller cannot continue to handle the default
   *            action.  Otherwise, true.
   */ 
  static bool WillHandleDefaultAction(WidgetWheelEvent* aWheelEvent,
                                      AutoWeakFrame& aTargetWeakFrame);
  static bool WillHandleDefaultAction(WidgetWheelEvent* aWheelEvent,
                                      nsIFrame* aTargetFrame)
  {
    AutoWeakFrame targetWeakFrame(aTargetFrame);
    return WillHandleDefaultAction(aWheelEvent, targetWeakFrame);
  }
  static void OnEvent(WidgetEvent* aEvent);
  static void Shutdown();
  static uint32_t GetTimeoutTime()
  {
    return Prefs::sMouseWheelTransactionTimeout;
  }

  static void OwnScrollbars(bool aOwn);

  static DeltaValues AccelerateWheelDelta(WidgetWheelEvent* aEvent,
                                          bool aAllowScrollSpeedOverride);
  static void InitializeStatics()
  {
    Prefs::InitializeStatics();
  }

protected:
  static void BeginTransaction(nsIFrame* aTargetFrame,
                               WidgetWheelEvent* aEvent);
  // Be careful, UpdateTransaction may fire a DOM event, therefore, the target
  // frame might be destroyed in the event handler.
  static bool UpdateTransaction(WidgetWheelEvent* aEvent);
  static void MayEndTransaction();

  static LayoutDeviceIntPoint GetScreenPoint(WidgetGUIEvent* aEvent);
  static void OnFailToScrollTarget();
  static void OnTimeout(nsITimer* aTimer, void* aClosure);
  static void SetTimeout();
  static uint32_t GetIgnoreMoveDelayTime()
  {
    return Prefs::sMouseWheelTransactionIgnoreMoveDelay;
  }
  static int32_t GetAccelerationStart()
  {
    return Prefs::sMouseWheelAccelerationStart;
  }
  static int32_t GetAccelerationFactor()
  {
    return Prefs::sMouseWheelAccelerationFactor;
  }
  static DeltaValues OverrideSystemScrollSpeed(WidgetWheelEvent* aEvent);
  static double ComputeAcceleratedWheelDelta(double aDelta, int32_t aFactor);
  static bool OutOfTime(uint32_t aBaseTime, uint32_t aThreshold);

  static AutoWeakFrame sTargetFrame;
  static uint32_t sTime; // in milliseconds
  static uint32_t sMouseMoved; // in milliseconds
  static nsITimer* sTimer;
  static int32_t sScrollSeriesCounter;
  static bool sOwnScrollbars;

  class Prefs
  {
  public:
    static void InitializeStatics();
    static int32_t sMouseWheelAccelerationStart;
    static int32_t sMouseWheelAccelerationFactor;
    static uint32_t sMouseWheelTransactionTimeout;
    static uint32_t sMouseWheelTransactionIgnoreMoveDelay;
    static bool sTestMouseScroll;
  };
};

} // namespace mozilla

#endif // mozilla_WheelHandlingHelper_h_

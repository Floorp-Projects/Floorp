/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WheelHandlingHelper.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIScrollableFrame.h"
#include "nsITimer.h"
#include "nsPresContext.h"
#include "prtime.h"
#include "Units.h"

namespace mozilla {

/******************************************************************/
/* mozilla::DeltaValues                                           */
/******************************************************************/

DeltaValues::DeltaValues(WidgetWheelEvent* aEvent)
  : deltaX(aEvent->deltaX)
  , deltaY(aEvent->deltaY)
{
}

/******************************************************************/
/* mozilla::WheelHandlingUtils                                    */
/******************************************************************/

/* static */ bool
WheelHandlingUtils::CanScrollInRange(nscoord aMin, nscoord aValue, nscoord aMax,
                                     double aDirection)
{
  return aDirection > 0.0 ? aValue < static_cast<double>(aMax) :
                            static_cast<double>(aMin) < aValue;
}

/* static */ bool
WheelHandlingUtils::CanScrollOn(nsIScrollableFrame* aScrollFrame,
                                double aDirectionX, double aDirectionY)
{
  MOZ_ASSERT(aScrollFrame);
  NS_ASSERTION(aDirectionX || aDirectionY,
               "One of the delta values must be non-zero at least");

  nsPoint scrollPt = aScrollFrame->GetScrollPosition();
  nsRect scrollRange = aScrollFrame->GetScrollRange();
  uint32_t directions = aScrollFrame->GetPerceivedScrollingDirections();

  return (aDirectionX && (directions & nsIScrollableFrame::HORIZONTAL) &&
          CanScrollInRange(scrollRange.x, scrollPt.x,
                           scrollRange.XMost(), aDirectionX)) ||
         (aDirectionY && (directions & nsIScrollableFrame::VERTICAL) &&
          CanScrollInRange(scrollRange.y, scrollPt.y,
                           scrollRange.YMost(), aDirectionY));
}

/******************************************************************/
/* mozilla::WheelTransaction                                      */
/******************************************************************/

nsWeakFrame WheelTransaction::sTargetFrame(nullptr);
uint32_t WheelTransaction::sTime = 0;
uint32_t WheelTransaction::sMouseMoved = 0;
nsITimer* WheelTransaction::sTimer = nullptr;
int32_t WheelTransaction::sScrollSeriesCounter = 0;
bool WheelTransaction::sOwnScrollbars = false;

/* static */ bool
WheelTransaction::OutOfTime(uint32_t aBaseTime, uint32_t aThreshold)
{
  uint32_t now = PR_IntervalToMilliseconds(PR_IntervalNow());
  return (now - aBaseTime > aThreshold);
}

/* static */ void
WheelTransaction::OwnScrollbars(bool aOwn)
{
  sOwnScrollbars = aOwn;
}

/* static */ void
WheelTransaction::BeginTransaction(nsIFrame* aTargetFrame,
                                   WidgetWheelEvent* aEvent)
{
  NS_ASSERTION(!sTargetFrame, "previous transaction is not finished!");
  MOZ_ASSERT(aEvent->mMessage == eWheel,
             "Transaction must be started with a wheel event");
  ScrollbarsForWheel::OwnWheelTransaction(false);
  sTargetFrame = aTargetFrame;
  sScrollSeriesCounter = 0;
  if (!UpdateTransaction(aEvent)) {
    NS_ERROR("BeginTransaction is called even cannot scroll the frame");
    EndTransaction();
  }
}

/* static */ bool
WheelTransaction::UpdateTransaction(WidgetWheelEvent* aEvent)
{
  nsIScrollableFrame* sf = GetTargetFrame()->GetScrollTargetFrame();
  NS_ENSURE_TRUE(sf, false);

  if (!WheelHandlingUtils::CanScrollOn(sf, aEvent->deltaX, aEvent->deltaY)) {
    OnFailToScrollTarget();
    // We should not modify the transaction state when the view will not be
    // scrolled actually.
    return false;
  }

  SetTimeout();

  if (sScrollSeriesCounter != 0 && OutOfTime(sTime, kScrollSeriesTimeout)) {
    sScrollSeriesCounter = 0;
  }
  sScrollSeriesCounter++;

  // We should use current time instead of WidgetEvent.time.
  // 1. Some events doesn't have the correct creation time.
  // 2. If the computer runs slowly by other processes eating the CPU resource,
  //    the event creation time doesn't keep real time.
  sTime = PR_IntervalToMilliseconds(PR_IntervalNow());
  sMouseMoved = 0;
  return true;
}

/* static */ void
WheelTransaction::MayEndTransaction()
{
  if (!sOwnScrollbars && ScrollbarsForWheel::IsActive()) {
    ScrollbarsForWheel::OwnWheelTransaction(true);
  } else {
    EndTransaction();
  }
}

/* static */ void
WheelTransaction::EndTransaction()
{
  if (sTimer) {
    sTimer->Cancel();
  }
  sTargetFrame = nullptr;
  sScrollSeriesCounter = 0;
  if (sOwnScrollbars) {
    sOwnScrollbars = false;
    ScrollbarsForWheel::OwnWheelTransaction(false);
    ScrollbarsForWheel::Inactivate();
  }
}

/* static */ void
WheelTransaction::OnEvent(WidgetEvent* aEvent)
{
  if (!sTargetFrame) {
    return;
  }

  if (OutOfTime(sTime, GetTimeoutTime())) {
    // Even if the scroll event which is handled after timeout, but onTimeout
    // was not fired by timer, then the scroll event will scroll old frame,
    // therefore, we should call OnTimeout here and ensure to finish the old
    // transaction.
    OnTimeout(nullptr, nullptr);
    return;
  }

  switch (aEvent->mMessage) {
    case eWheel:
      if (sMouseMoved != 0 &&
          OutOfTime(sMouseMoved, GetIgnoreMoveDelayTime())) {
        // Terminate the current mousewheel transaction if the mouse moved more
        // than ignoremovedelay milliseconds ago
        EndTransaction();
      }
      return;
    case eMouseMove:
    case eDragOver: {
      WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
      if (mouseEvent->IsReal()) {
        // If the cursor is moving to be outside the frame,
        // terminate the scrollwheel transaction.
        nsIntPoint pt = GetScreenPoint(mouseEvent);
        nsIntRect r = sTargetFrame->GetScreenRectExternal();
        if (!r.Contains(pt)) {
          EndTransaction();
          return;
        }

        // If the cursor is moving inside the frame, and it is less than
        // ignoremovedelay milliseconds since the last scroll operation, ignore
        // the mouse move; otherwise, record the current mouse move time to be
        // checked later
        if (!sMouseMoved && OutOfTime(sTime, GetIgnoreMoveDelayTime())) {
          sMouseMoved = PR_IntervalToMilliseconds(PR_IntervalNow());
        }
      }
      return;
    }
    case eKeyPress:
    case eKeyUp:
    case eKeyDown:
    case eMouseUp:
    case eMouseDown:
    case eMouseDoubleClick:
    case eMouseClick:
    case eContextMenu:
    case eDrop:
      EndTransaction();
      return;
    default:
      break;
  }
}

/* static */ void
WheelTransaction::Shutdown()
{
  NS_IF_RELEASE(sTimer);
}

/* static */ void
WheelTransaction::OnFailToScrollTarget()
{
  NS_PRECONDITION(sTargetFrame, "We don't have mouse scrolling transaction");

  if (Preferences::GetBool("test.mousescroll", false)) {
    // This event is used for automated tests, see bug 442774.
    nsContentUtils::DispatchTrustedEvent(
                      sTargetFrame->GetContent()->OwnerDoc(),
                      sTargetFrame->GetContent(),
                      NS_LITERAL_STRING("MozMouseScrollFailed"),
                      true, true);
  }
  // The target frame might be destroyed in the event handler, at that time,
  // we need to finish the current transaction
  if (!sTargetFrame) {
    EndTransaction();
  }
}

/* static */ void
WheelTransaction::OnTimeout(nsITimer* aTimer, void* aClosure)
{
  if (!sTargetFrame) {
    // The transaction target was destroyed already
    EndTransaction();
    return;
  }
  // Store the sTargetFrame, the variable becomes null in EndTransaction.
  nsIFrame* frame = sTargetFrame;
  // We need to finish current transaction before DOM event firing. Because
  // the next DOM event might create strange situation for us.
  MayEndTransaction();

  if (Preferences::GetBool("test.mousescroll", false)) {
    // This event is used for automated tests, see bug 442774.
    nsContentUtils::DispatchTrustedEvent(
                      frame->GetContent()->OwnerDoc(),
                      frame->GetContent(),
                      NS_LITERAL_STRING("MozMouseScrollTransactionTimeout"),
                      true, true);
  }
}

/* static */ void
WheelTransaction::SetTimeout()
{
  if (!sTimer) {
    nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (!timer) {
      return;
    }
    timer.swap(sTimer);
  }
  sTimer->Cancel();
  DebugOnly<nsresult> rv =
    sTimer->InitWithFuncCallback(OnTimeout, nullptr, GetTimeoutTime(),
                                 nsITimer::TYPE_ONE_SHOT);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "nsITimer::InitWithFuncCallback failed");
}

/* static */ nsIntPoint
WheelTransaction::GetScreenPoint(WidgetGUIEvent* aEvent)
{
  NS_ASSERTION(aEvent, "aEvent is null");
  NS_ASSERTION(aEvent->widget, "aEvent-widget is null");
  return LayoutDeviceIntPoint::ToUntyped(aEvent->refPoint +
           aEvent->widget->WidgetToScreenOffset());
}

/* static */ uint32_t
WheelTransaction::GetTimeoutTime()
{
  return Preferences::GetUint("mousewheel.transaction.timeout", 1500);
}

/* static */ uint32_t
WheelTransaction::GetIgnoreMoveDelayTime()
{
  return Preferences::GetUint("mousewheel.transaction.ignoremovedelay", 100);
}

/* static */ DeltaValues
WheelTransaction::AccelerateWheelDelta(WidgetWheelEvent* aEvent,
                                       bool aAllowScrollSpeedOverride)
{
  DeltaValues result(aEvent);

  // Don't accelerate the delta values if the event isn't line scrolling.
  if (aEvent->deltaMode != nsIDOMWheelEvent::DOM_DELTA_LINE) {
    return result;
  }

  if (aAllowScrollSpeedOverride) {
    result = OverrideSystemScrollSpeed(aEvent);
  }

  // Accelerate by the sScrollSeriesCounter
  int32_t start = GetAccelerationStart();
  if (start >= 0 && sScrollSeriesCounter >= start) {
    int32_t factor = GetAccelerationFactor();
    if (factor > 0) {
      result.deltaX = ComputeAcceleratedWheelDelta(result.deltaX, factor);
      result.deltaY = ComputeAcceleratedWheelDelta(result.deltaY, factor);
    }
  }

  return result;
}

/* static */ double
WheelTransaction::ComputeAcceleratedWheelDelta(double aDelta,
                                               int32_t aFactor)
{
  if (aDelta == 0.0) {
    return 0;
  }

  return (aDelta * sScrollSeriesCounter * (double)aFactor / 10);
}

/* static */ int32_t
WheelTransaction::GetAccelerationStart()
{
  return Preferences::GetInt("mousewheel.acceleration.start", -1);
}

/* static */ int32_t
WheelTransaction::GetAccelerationFactor()
{
  return Preferences::GetInt("mousewheel.acceleration.factor", -1);
}

/* static */ DeltaValues
WheelTransaction::OverrideSystemScrollSpeed(WidgetWheelEvent* aEvent)
{
  MOZ_ASSERT(sTargetFrame, "We don't have mouse scrolling transaction");
  MOZ_ASSERT(aEvent->deltaMode == nsIDOMWheelEvent::DOM_DELTA_LINE);

  // If the event doesn't scroll to both X and Y, we don't need to do anything
  // here.
  if (!aEvent->deltaX && !aEvent->deltaY) {
    return DeltaValues(aEvent);
  }

  // We shouldn't override the scrolling speed on non root scroll frame.
  if (sTargetFrame !=
        sTargetFrame->PresContext()->PresShell()->GetRootScrollFrame()) {
    return DeltaValues(aEvent);
  }

  // Compute the overridden speed to nsIWidget.  The widget can check the
  // conditions (e.g., checking the prefs, and also whether the user customized
  // the system settings of the mouse wheel scrolling or not), and can limit
  // the speed for preventing the unexpected high speed scrolling.
  nsCOMPtr<nsIWidget> widget(sTargetFrame->GetNearestWidget());
  NS_ENSURE_TRUE(widget, DeltaValues(aEvent));
  DeltaValues overriddenDeltaValues(0.0, 0.0);
  nsresult rv =
    widget->OverrideSystemMouseScrollSpeed(aEvent->deltaX, aEvent->deltaY,
                                           overriddenDeltaValues.deltaX,
                                           overriddenDeltaValues.deltaY);
  return NS_FAILED(rv) ? DeltaValues(aEvent) : overriddenDeltaValues;
}

/******************************************************************/
/* mozilla::ScrollbarsForWheel                                    */
/******************************************************************/

const DeltaValues ScrollbarsForWheel::directions[kNumberOfTargets] = {
  DeltaValues(-1, 0), DeltaValues(+1, 0), DeltaValues(0, -1), DeltaValues(0, +1)
};

nsWeakFrame ScrollbarsForWheel::sActiveOwner = nullptr;
nsWeakFrame ScrollbarsForWheel::sActivatedScrollTargets[kNumberOfTargets] = {
  nullptr, nullptr, nullptr, nullptr
};

bool ScrollbarsForWheel::sHadWheelStart = false;
bool ScrollbarsForWheel::sOwnWheelTransaction = false;

/* static */ void
ScrollbarsForWheel::PrepareToScrollText(EventStateManager* aESM,
                                        nsIFrame* aTargetFrame,
                                        WidgetWheelEvent* aEvent)
{
  if (aEvent->mMessage == eWheelOperationStart) {
    WheelTransaction::OwnScrollbars(false);
    if (!IsActive()) {
      TemporarilyActivateAllPossibleScrollTargets(aESM, aTargetFrame, aEvent);
      sHadWheelStart = true;
    }
  } else {
    DeactivateAllTemporarilyActivatedScrollTargets();
  }
}

/* static */ void
ScrollbarsForWheel::SetActiveScrollTarget(nsIScrollableFrame* aScrollTarget)
{
  if (!sHadWheelStart) {
    return;
  }
  nsIScrollbarMediator* scrollbarMediator = do_QueryFrame(aScrollTarget);
  if (!scrollbarMediator) {
    return;
  }
  sHadWheelStart = false;
  sActiveOwner = do_QueryFrame(aScrollTarget);
  scrollbarMediator->ScrollbarActivityStarted();
}

/* static */ void
ScrollbarsForWheel::MayInactivate()
{
  if (!sOwnWheelTransaction && WheelTransaction::GetTargetFrame()) {
    WheelTransaction::OwnScrollbars(true);
  } else {
    Inactivate();
  }
}

/* static */ void
ScrollbarsForWheel::Inactivate()
{
  nsIScrollbarMediator* scrollbarMediator = do_QueryFrame(sActiveOwner);
  if (scrollbarMediator) {
    scrollbarMediator->ScrollbarActivityStopped();
  }
  sActiveOwner = nullptr;
  DeactivateAllTemporarilyActivatedScrollTargets();
  if (sOwnWheelTransaction) {
    sOwnWheelTransaction = false;
    WheelTransaction::OwnScrollbars(false);
    WheelTransaction::EndTransaction();
  }
}

/* static */ bool
ScrollbarsForWheel::IsActive()
{
  if (sActiveOwner) {
    return true;
  }
  for (size_t i = 0; i < kNumberOfTargets; ++i) {
    if (sActivatedScrollTargets[i]) {
      return true;
    }
  }
  return false;
}

/* static */ void
ScrollbarsForWheel::OwnWheelTransaction(bool aOwn)
{
  sOwnWheelTransaction = aOwn;
}

/* static */ void
ScrollbarsForWheel::TemporarilyActivateAllPossibleScrollTargets(
                      EventStateManager* aESM,
                      nsIFrame* aTargetFrame,
                      WidgetWheelEvent* aEvent)
{
  for (size_t i = 0; i < kNumberOfTargets; i++) {
    const DeltaValues *dir = &directions[i];
    nsWeakFrame* scrollTarget = &sActivatedScrollTargets[i];
    MOZ_ASSERT(!*scrollTarget, "scroll target still temporarily activated!");
    nsIScrollableFrame* target =
      aESM->ComputeScrollTarget(aTargetFrame, dir->deltaX, dir->deltaY, aEvent,
              EventStateManager::COMPUTE_DEFAULT_ACTION_TARGET);
    nsIScrollbarMediator* scrollbarMediator = do_QueryFrame(target);
    if (scrollbarMediator) {
      nsIFrame* targetFrame = do_QueryFrame(target);
      *scrollTarget = targetFrame;
      scrollbarMediator->ScrollbarActivityStarted();
    }
  }
}

/* static */ void
ScrollbarsForWheel::DeactivateAllTemporarilyActivatedScrollTargets()
{
  for (size_t i = 0; i < kNumberOfTargets; i++) {
    nsWeakFrame* scrollTarget = &sActivatedScrollTargets[i];
    if (*scrollTarget) {
      nsIScrollbarMediator* scrollbarMediator = do_QueryFrame(*scrollTarget);
      if (scrollbarMediator) {
        scrollbarMediator->ScrollbarActivityStopped();
      }
      *scrollTarget = nullptr;
    }
  }
}

} // namespace mozilla

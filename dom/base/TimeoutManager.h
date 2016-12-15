/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TimeoutManager_h__
#define mozilla_dom_TimeoutManager_h__

#include "mozilla/dom/Timeout.h"

class nsITimeoutHandler;
class nsGlobalWindow;

namespace mozilla {

class ThrottledEventQueue;

namespace dom {

// This class manages the timeouts in a Window's setTimeout/setInterval pool.
class TimeoutManager final
{
public:
  explicit TimeoutManager(nsGlobalWindow& aWindow);
  TimeoutManager(const TimeoutManager& rhs) = delete;
  void operator=(const TimeoutManager& rhs) = delete;

  bool IsRunningTimeout() const { return mTimeoutFiringDepth > 0; }

  static uint32_t GetNestingLevel() { return sNestingLevel; }
  static void SetNestingLevel(uint32_t aLevel) { sNestingLevel = aLevel; }

  bool HasTimeouts() const { return !mTimeouts.IsEmpty(); }

  nsresult SetTimeout(nsITimeoutHandler* aHandler,
                      int32_t interval, bool aIsInterval,
                      mozilla::dom::Timeout::Reason aReason,
                      int32_t* aReturn);
  void ClearTimeout(int32_t aTimerId,
                    mozilla::dom::Timeout::Reason aReason);

  // The timeout implementation functions.
  void RunTimeout(mozilla::dom::Timeout* aTimeout);
  // Return true if |aTimeout| needs to be reinserted into the timeout list.
  bool RescheduleTimeout(mozilla::dom::Timeout* aTimeout, const TimeStamp& now,
                         bool aRunningPendingTimeouts);

  void ClearAllTimeouts();
  uint32_t GetTimeoutId(mozilla::dom::Timeout::Reason aReason);

  // Apply back pressure to the window if the TabGroup ThrottledEventQueue
  // exists and has too many runnables waiting to run.  For example, increase
  // the minimum timer delay, etc.
  void MaybeApplyBackPressure();

  // Check the current ThrottledEventQueue depth and update the back pressure
  // state.  If the queue has drained back pressure may be canceled.
  void CancelOrUpdateBackPressure(nsGlobalWindow* aWindow);

  // When timers are being throttled and we reduce the thottle delay we must
  // reschedule.  The amount of the old throttle delay must be provided in
  // order to bound how many timers must be examined.
  nsresult ResetTimersForThrottleReduction();

  int32_t DOMMinTimeoutValue() const;

  // aTimeout is the timeout that we're about to start running.  This function
  // returns the current timeout.
  mozilla::dom::Timeout* BeginRunningTimeout(mozilla::dom::Timeout* aTimeout);
  // aTimeout is the last running timeout.
  void EndRunningTimeout(mozilla::dom::Timeout* aTimeout);

  void UnmarkGrayTimers();

  // These four methods are intended to be called from the corresponding methods
  // on nsGlobalWindow.
  void Suspend();
  void Resume();
  void Freeze();
  void Thaw();

  // Initialize TimeoutManager before the first time it is accessed.
  static void Initialize();

  // Run some code for each Timeout in our list.
  template <class Callable>
  void ForEachTimeout(Callable c)
  {
    mTimeouts.ForEach(c);
  }

  // Run some code for each Timeout in our list, but let the callback cancel
  // the iteration by returning true.
  template <class Callable>
  void ForEachTimeoutAbortable(Callable c)
  {
    mTimeouts.ForEachAbortable(c);
  }

private:
  nsresult ResetTimersForThrottleReduction(int32_t aPreviousThrottleDelayMS);

private:
  typedef mozilla::LinkedList<mozilla::dom::Timeout> TimeoutList;
  struct Timeouts {
    Timeouts()
      : mTimeoutInsertionPoint(nullptr)
    {
    }

    // Insert aTimeout into the list, before all timeouts that would
    // fire after it, but no earlier than mTimeoutInsertionPoint, if any.
    enum class SortBy
    {
      TimeRemaining,
      TimeWhen
    };
    void Insert(mozilla::dom::Timeout* aTimeout, SortBy aSortBy);
    nsresult ResetTimersForThrottleReduction(int32_t aPreviousThrottleDelayMS,
                                             int32_t aMinTimeoutValueMS,
                                             SortBy aSortBy,
                                             mozilla::ThrottledEventQueue* aQueue);

    const Timeout* GetFirst() const { return mTimeoutList.getFirst(); }
    Timeout* GetFirst() { return mTimeoutList.getFirst(); }
    const Timeout* GetLast() const { return mTimeoutList.getLast(); }
    Timeout* GetLast() { return mTimeoutList.getLast(); }
    bool IsEmpty() const { return mTimeoutList.isEmpty(); }
    void InsertFront(Timeout* aTimeout) { mTimeoutList.insertFront(aTimeout); }
    void Clear() { mTimeoutList.clear(); }

    void SetInsertionPoint(Timeout* aTimeout)
    {
      mTimeoutInsertionPoint = aTimeout;
    }
    Timeout* InsertionPoint()
    {
      return mTimeoutInsertionPoint;
    }

    template <class Callable>
    void ForEach(Callable c)
    {
      for (Timeout* timeout = GetFirst();
           timeout;
           timeout = timeout->getNext()) {
        c(timeout);
      }
    }

    template <class Callable>
    void ForEachAbortable(Callable c)
    {
      for (Timeout* timeout = GetFirst();
           timeout;
           timeout = timeout->getNext()) {
        if (c(timeout)) {
          break;
        }
      }
    }

  private:
    // mTimeoutList is generally sorted by mWhen, unless mTimeoutInsertionPoint is
    // non-null.  In that case, the dummy timeout pointed to by
    // mTimeoutInsertionPoint may have a later mWhen than some of the timeouts
    // that come after it.
    TimeoutList               mTimeoutList;
    // If mTimeoutInsertionPoint is non-null, insertions should happen after it.
    // This is a dummy timeout at the moment; if that ever changes, the logic in
    // ResetTimersForThrottleReduction needs to change.
    mozilla::dom::Timeout*    mTimeoutInsertionPoint;
  };

  // Each nsGlobalWindow object has a TimeoutManager member.  This reference
  // points to that holder object.
  nsGlobalWindow&             mWindow;
  Timeouts                    mTimeouts;
  uint32_t                    mTimeoutIdCounter;
  uint32_t                    mTimeoutFiringDepth;
  mozilla::dom::Timeout*      mRunningTimeout;

   // The current idle request callback timeout handle
  uint32_t                    mIdleCallbackTimeoutCounter;

  int32_t                     mBackPressureDelayMS;

  static uint32_t             sNestingLevel;
};

}
}

#endif

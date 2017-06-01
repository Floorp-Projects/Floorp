/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TimeoutManager_h__
#define mozilla_dom_TimeoutManager_h__

#include "mozilla/dom/Timeout.h"
#include "nsTArray.h"

class nsIEventTarget;
class nsITimeoutHandler;
class nsGlobalWindow;

namespace mozilla {
namespace dom {

class OrderedTimeoutIterator;

// This class manages the timeouts in a Window's setTimeout/setInterval pool.
class TimeoutManager final
{
public:
  explicit TimeoutManager(nsGlobalWindow& aWindow);
  ~TimeoutManager();
  TimeoutManager(const TimeoutManager& rhs) = delete;
  void operator=(const TimeoutManager& rhs) = delete;

  bool IsRunningTimeout() const;

  static uint32_t GetNestingLevel() { return sNestingLevel; }
  static void SetNestingLevel(uint32_t aLevel) { sNestingLevel = aLevel; }

  bool HasTimeouts() const
  {
    return !mNormalTimeouts.IsEmpty() ||
           !mTrackingTimeouts.IsEmpty();
  }

  nsresult SetTimeout(nsITimeoutHandler* aHandler,
                      int32_t interval, bool aIsInterval,
                      mozilla::dom::Timeout::Reason aReason,
                      int32_t* aReturn);
  void ClearTimeout(int32_t aTimerId,
                    mozilla::dom::Timeout::Reason aReason);

  // The timeout implementation functions.
  void RunTimeout(const TimeStamp& aTargetDeadline);
  // Return true if |aTimeout| needs to be reinserted into the timeout list.
  bool RescheduleTimeout(mozilla::dom::Timeout* aTimeout, const TimeStamp& now);

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

  int32_t DOMMinTimeoutValue(bool aIsTracking) const;

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

  // Exposed only for testing
  bool IsTimeoutTracking(uint32_t aTimeoutId);

  // The document finished loading
  void OnDocumentLoaded();
  void StartThrottlingTrackingTimeouts();

  // Run some code for each Timeout in our list.  Note that this function
  // doesn't guarantee that Timeouts are iterated in any particular order.
  template <class Callable>
  void ForEachUnorderedTimeout(Callable c)
  {
    mNormalTimeouts.ForEach(c);
    mTrackingTimeouts.ForEach(c);
  }

  // Run some code for each Timeout in our list, but let the callback cancel the
  // iteration by returning true.  Note that this function doesn't guarantee
  // that Timeouts are iterated in any particular order.
  template <class Callable>
  void ForEachUnorderedTimeoutAbortable(Callable c)
  {
    if (!mNormalTimeouts.ForEachAbortable(c)) {
      mTrackingTimeouts.ForEachAbortable(c);
    }
  }

  void BeginSyncOperation();
  void EndSyncOperation();

  nsIEventTarget*
  EventTarget();

  static const uint32_t InvalidFiringId;

private:
  nsresult ResetTimersForThrottleReduction(int32_t aPreviousThrottleDelayMS);
  void MaybeStartThrottleTrackingTimout();

  bool IsBackground() const;

  uint32_t
  CreateFiringId();

  void
  DestroyFiringId(uint32_t aFiringId);

  bool
  IsInvalidFiringId(uint32_t aFiringId) const;

private:
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
                                             const TimeoutManager& aTimeoutManager,
                                             SortBy aSortBy,
                                             nsIEventTarget* aQueue);

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

    // Returns true when a callback aborts iteration.
    template <class Callable>
    bool ForEachAbortable(Callable c)
    {
      for (Timeout* timeout = GetFirst();
           timeout;
           timeout = timeout->getNext()) {
        if (c(timeout)) {
          return true;
        }
      }
      return false;
    }

    friend class OrderedTimeoutIterator;

  private:
    typedef mozilla::LinkedList<RefPtr<Timeout>> TimeoutList;

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

  friend class OrderedTimeoutIterator;

  // Each nsGlobalWindow object has a TimeoutManager member.  This reference
  // points to that holder object.
  nsGlobalWindow&             mWindow;
  // The list of timeouts coming from non-tracking scripts.
  Timeouts                    mNormalTimeouts;
  // The list of timeouts coming from scripts on the tracking protection list.
  Timeouts                    mTrackingTimeouts;
  uint32_t                    mTimeoutIdCounter;
  uint32_t                    mNextFiringId;
  AutoTArray<uint32_t, 2>     mFiringIdStack;
  mozilla::dom::Timeout*      mRunningTimeout;

   // The current idle request callback timeout handle
  uint32_t                    mIdleCallbackTimeoutCounter;

  int32_t                     mBackPressureDelayMS;

  nsCOMPtr<nsITimer>          mThrottleTrackingTimeoutsTimer;
  bool                        mThrottleTrackingTimeouts;

  static uint32_t             sNestingLevel;
};

}
}

#endif

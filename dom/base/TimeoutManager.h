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
class TimeoutExecutor;

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
  void RunTimeout(const TimeStamp& aNow, const TimeStamp& aTargetDeadline);
  // Return true if |aTimeout| needs to be reinserted into the timeout list.
  bool RescheduleTimeout(mozilla::dom::Timeout* aTimeout, const TimeStamp& now);

  void ClearAllTimeouts();
  uint32_t GetTimeoutId(mozilla::dom::Timeout::Reason aReason);

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

  // This should be called by nsGlobalWindow when the window might have moved
  // to the background or foreground.
  void UpdateBackgroundState();

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
  IsValidFiringId(uint32_t aFiringId) const;

  bool
  IsInvalidFiringId(uint32_t aFiringId) const;

  TimeDuration
  MinSchedulingDelay() const;

private:
  struct Timeouts {
    explicit Timeouts(const TimeoutManager& aManager)
      : mManager(aManager)
    {
    }

    // Insert aTimeout into the list, before all timeouts that would
    // fire after it, but no earlier than the last Timeout with a
    // valid FiringId.
    enum class SortBy
    {
      TimeRemaining,
      TimeWhen
    };
    void Insert(mozilla::dom::Timeout* aTimeout, SortBy aSortBy);
    nsresult ResetTimersForThrottleReduction(int32_t aPreviousThrottleDelayMS,
                                             const TimeoutManager& aTimeoutManager,
                                             SortBy aSortBy);

    const Timeout* GetFirst() const { return mTimeoutList.getFirst(); }
    Timeout* GetFirst() { return mTimeoutList.getFirst(); }
    const Timeout* GetLast() const { return mTimeoutList.getLast(); }
    Timeout* GetLast() { return mTimeoutList.getLast(); }
    bool IsEmpty() const { return mTimeoutList.isEmpty(); }
    void InsertFront(Timeout* aTimeout) { mTimeoutList.insertFront(aTimeout); }
    void Clear() { mTimeoutList.clear(); }

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
    // The TimeoutManager that owns this Timeouts structure.  This is
    // mainly used to call state inspecting methods like IsValidFiringId().
    const TimeoutManager&     mManager;

    typedef mozilla::LinkedList<RefPtr<Timeout>> TimeoutList;

    // mTimeoutList is generally sorted by mWhen, but new values are always
    // inserted after any Timeouts with a valid FiringId.
    TimeoutList               mTimeoutList;
  };

  friend class OrderedTimeoutIterator;

  // Each nsGlobalWindow object has a TimeoutManager member.  This reference
  // points to that holder object.
  nsGlobalWindow&             mWindow;
  // The executor is specific to the nsGlobalWindow/TimeoutManager, but it
  // can live past the destruction of the window if its scheduled.  Therefore
  // it must be a separate ref-counted object.
  RefPtr<TimeoutExecutor>     mExecutor;
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

  nsCOMPtr<nsITimer>          mThrottleTrackingTimeoutsTimer;
  bool                        mThrottleTrackingTimeouts;

  static uint32_t             sNestingLevel;
};

}
}

#endif

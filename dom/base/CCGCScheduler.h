/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/SliceBudget.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/MainThreadIdlePeriod.h"
#include "nsCycleCollector.h"

static const TimeDuration kOneMinute = TimeDuration::FromSeconds(60.0f);

// The amount of time we wait between a request to CC (after GC ran)
// and doing the actual CC.
static const TimeDuration kCCDelay = TimeDuration::FromSeconds(6);

static const TimeDuration kCCSkippableDelay =
    TimeDuration::FromMilliseconds(250);

// In case the cycle collector isn't run at all, we don't want forget skippables
// to run too often. So limit the forget skippable cycle to start at earliest 2
// seconds after the end of the previous cycle.
static const TimeDuration kTimeBetweenForgetSkippableCycles =
    TimeDuration::FromSeconds(2);

// ForgetSkippable is usually fast, so we can use small budgets.
// This isn't a real budget but a hint to IdleTaskRunner whether there
// is enough time to call ForgetSkippable.
static const TimeDuration kForgetSkippableSliceDuration =
    TimeDuration::FromMilliseconds(2);

// Maximum amount of time that should elapse between incremental CC slices
static const TimeDuration kICCIntersliceDelay =
    TimeDuration::FromMilliseconds(64);

// Time budget for an incremental CC slice when using timer to run it.
static const TimeDuration kICCSliceBudget = TimeDuration::FromMilliseconds(3);
// Minimum budget for an incremental CC slice when using idle time to run it.
static const TimeDuration kIdleICCSliceBudget =
    TimeDuration::FromMilliseconds(2);

// Maximum total duration for an ICC
static const TimeDuration kMaxICCDuration = TimeDuration::FromSeconds(2);

// Force a CC after this long if there's more than NS_CC_FORCED_PURPLE_LIMIT
// objects in the purple buffer.
static const TimeDuration kCCForced = kOneMinute * 2;
static const uint32_t kCCForcedPurpleLimit = 10;

// Don't allow an incremental GC to lock out the CC for too long.
static const TimeDuration kMaxCCLockedoutTime = TimeDuration::FromSeconds(30);

// Trigger a CC if the purple buffer exceeds this size when we check it.
static const uint32_t kCCPurpleLimit = 200;

// if you add statics here, add them to the list in StartupJSEnvironment

namespace mozilla {

class CCGCScheduler {
 public:
  void SetActiveIntersliceGCBudget(TimeDuration aDuration) {
    mActiveIntersliceGCBudget = aDuration;
  }

  // Returns an IsNull() TimeStamp if CC is not currently locked out.
  TimeStamp GetCCLockedOutTime() const {
    return mCCLockedOut ? mCCLockedOutTime : TimeStamp();
  }

  bool InIncrementalGC() const { return mCCLockedOut; }

  void NoteGCBegin() { mCCLockedOut = true; }

  void NoteGCEnd() { mCCLockedOut = false; }

  TimeDuration ComputeInterSliceGCBudget(TimeStamp aDeadline) const {
    // We use longer budgets when the CC has been locked out but the CC has
    // tried to run since that means we may have a significant amount of
    // garbage to collect and it's better to GC in several longer slices than
    // in a very long one.
    TimeDuration budget = aDeadline.IsNull() ? mActiveIntersliceGCBudget * 2
                                             : aDeadline - TimeStamp::Now();
    TimeStamp CCLockedOutTime = GetCCLockedOutTime();
    if (CCLockedOutTime.IsNull()) {
      return budget;
    }

    TimeDuration blockedTime = TimeStamp::Now() - CCLockedOutTime;
    TimeDuration maxSliceGCBudget = mActiveIntersliceGCBudget * 10;
    double percentOfBlockedTime =
        std::min(blockedTime / kMaxCCLockedoutTime, 1.0);
    return std::max(budget, maxSliceGCBudget.MultDouble(percentOfBlockedTime));
  }

  // The CC is locked out because an incremental GC has started.
  bool mCCLockedOut = false;

  // When the CC started actually waiting for the GC to finish. This will be
  // set to non-null at a later time than mCCLockedOut.
  TimeStamp mCCLockedOutTime;

  // Configuration parameters

  TimeDuration mActiveIntersliceGCBudget = TimeDuration::FromMilliseconds(5);
};

}  // namespace mozilla

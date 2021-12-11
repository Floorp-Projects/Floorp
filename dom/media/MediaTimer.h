/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaTimer_h_)
#  define MediaTimer_h_

#  include <queue>

#  include "mozilla/AbstractThread.h"
#  include "mozilla/IntegerPrintfMacros.h"
#  include "mozilla/Monitor.h"
#  include "mozilla/MozPromise.h"
#  include "mozilla/RefPtr.h"
#  include "mozilla/TimeStamp.h"
#  include "mozilla/Unused.h"
#  include "nsITimer.h"

namespace mozilla {

extern LazyLogModule gMediaTimerLog;

#  define TIMER_LOG(x, ...)                                    \
    MOZ_ASSERT(gMediaTimerLog);                                \
    MOZ_LOG(gMediaTimerLog, LogLevel::Debug,                   \
            ("[MediaTimer=%p relative_t=%" PRId64 "]" x, this, \
             RelativeMicroseconds(TimeStamp::Now()), ##__VA_ARGS__))

// This promise type is only exclusive because so far there isn't a reason for
// it not to be. Feel free to change that.
typedef MozPromise<bool, bool, /* IsExclusive = */ true> MediaTimerPromise;

// Timers only know how to fire at a given thread, which creates an impedence
// mismatch with code that operates with TaskQueues. This class solves
// that mismatch with a dedicated (but shared) thread and a nice MozPromise-y
// interface.
class MediaTimer {
 public:
  explicit MediaTimer(bool aFuzzy = false);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DESTROY(MediaTimer,
                                                     DispatchDestroy());

  RefPtr<MediaTimerPromise> WaitFor(const TimeDuration& aDuration,
                                    const char* aCallSite);
  RefPtr<MediaTimerPromise> WaitUntil(const TimeStamp& aTimeStamp,
                                      const char* aCallSite);
  void Cancel();  // Cancel and reject any unresolved promises with false.

 private:
  virtual ~MediaTimer() { MOZ_ASSERT(OnMediaTimerThread()); }

  void DispatchDestroy();  // Invoked by Release on an arbitrary thread.
  void Destroy();          // Runs on the timer thread.

  bool OnMediaTimerThread();
  void ScheduleUpdate();
  void Update();
  void UpdateLocked();
  bool IsExpired(const TimeStamp& aTarget, const TimeStamp& aNow);
  void Reject();

  static void TimerCallback(nsITimer* aTimer, void* aClosure);
  void TimerFired();
  void ArmTimer(const TimeStamp& aTarget, const TimeStamp& aNow);

  bool TimerIsArmed() { return !mCurrentTimerTarget.IsNull(); }

  void CancelTimerIfArmed() {
    MOZ_ASSERT(OnMediaTimerThread());
    if (TimerIsArmed()) {
      TIMER_LOG("MediaTimer::CancelTimerIfArmed canceling timer");
      mTimer->Cancel();
      mCurrentTimerTarget = TimeStamp();
    }
  }

  struct Entry {
    TimeStamp mTimeStamp;
    RefPtr<MediaTimerPromise::Private> mPromise;

    explicit Entry(const TimeStamp& aTimeStamp, const char* aCallSite)
        : mTimeStamp(aTimeStamp),
          mPromise(new MediaTimerPromise::Private(aCallSite)) {}

    // Define a < overload that reverses ordering because std::priority_queue
    // provides access to the largest element, and we want the smallest
    // (i.e. the soonest).
    bool operator<(const Entry& aOther) const {
      return mTimeStamp > aOther.mTimeStamp;
    }
  };

  nsCOMPtr<nsIEventTarget> mThread;
  std::priority_queue<Entry> mEntries;
  Monitor mMonitor;
  nsCOMPtr<nsITimer> mTimer;
  TimeStamp mCurrentTimerTarget;

  // Timestamps only have relative meaning, so we need a base timestamp for
  // logging purposes.
  TimeStamp mCreationTimeStamp;
  int64_t RelativeMicroseconds(const TimeStamp& aTimeStamp) {
    return (int64_t)(aTimeStamp - mCreationTimeStamp).ToMicroseconds();
  }

  bool mUpdateScheduled;
  const bool mFuzzy;
};

// Class for managing delayed dispatches on target thread.
class DelayedScheduler {
 public:
  explicit DelayedScheduler(nsISerialEventTarget* aTargetThread,
                            bool aFuzzy = false)
      : mTargetThread(aTargetThread), mMediaTimer(new MediaTimer(aFuzzy)) {
    MOZ_ASSERT(mTargetThread);
  }

  bool IsScheduled() const { return !mTarget.IsNull(); }

  void Reset() {
    MOZ_ASSERT(mTargetThread->IsOnCurrentThread(),
               "Must be on target thread to disconnect");
    mRequest.DisconnectIfExists();
    mTarget = TimeStamp();
  }

  template <typename ResolveFunc, typename RejectFunc>
  void Ensure(mozilla::TimeStamp& aTarget, ResolveFunc&& aResolver,
              RejectFunc&& aRejector) {
    MOZ_ASSERT(mTargetThread->IsOnCurrentThread());
    if (IsScheduled() && mTarget <= aTarget) {
      return;
    }
    Reset();
    mTarget = aTarget;
    mMediaTimer->WaitUntil(mTarget, __func__)
        ->Then(mTargetThread, __func__, std::forward<ResolveFunc>(aResolver),
               std::forward<RejectFunc>(aRejector))
        ->Track(mRequest);
  }

  void CompleteRequest() {
    MOZ_ASSERT(mTargetThread->IsOnCurrentThread());
    mRequest.Complete();
    mTarget = TimeStamp();
  }

 private:
  nsCOMPtr<nsISerialEventTarget> mTargetThread;
  RefPtr<MediaTimer> mMediaTimer;
  MozPromiseRequestHolder<mozilla::MediaTimerPromise> mRequest;
  TimeStamp mTarget;
};

}  // namespace mozilla

#endif

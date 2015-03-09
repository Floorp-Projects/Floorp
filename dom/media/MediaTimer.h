/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaTimer_h_)
#define MediaTimer_h_

#include "MediaPromise.h"

#include <queue>

#include "nsITimer.h"
#include "nsRefPtr.h"

#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {

// This promise type is only exclusive because so far there isn't a reason for
// it not to be. Feel free to change that.
typedef MediaPromise<bool, bool, /* IsExclusive = */ true> MediaTimerPromise;

// Timers only know how to fire at a given thread, which creates an impedence
// mismatch with code that operates with MediaTaskQueues. This class solves
// that mismatch with a dedicated (but shared) thread and a nice MediaPromise-y
// interface.
class MediaTimer
{
public:
  MediaTimer();

  // We use a release with a custom Destroy().
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);
  NS_IMETHOD_(MozExternalRefCountType) Release(void);

  nsRefPtr<MediaTimerPromise> WaitUntil(const TimeStamp& aTimeStamp, const char* aCallSite);

private:
  virtual ~MediaTimer() { MOZ_ASSERT(OnMediaTimerThread()); }

  void DispatchDestroy(); // Invoked by Release on an arbitrary thread.
  void Destroy(); // Runs on the timer thread.

  bool OnMediaTimerThread();
  void ScheduleUpdate();
  void Update();
  void UpdateLocked();

  static void TimerCallback(nsITimer* aTimer, void* aClosure);
  void TimerFired();
  void ArmTimer(const TimeStamp& aTarget, const TimeStamp& aNow);

  bool TimerIsArmed()
  {
    return !mCurrentTimerTarget.IsNull();
  }

  void CancelTimerIfArmed()
  {
    MOZ_ASSERT(OnMediaTimerThread());
    if (TimerIsArmed()) {
      mTimer->Cancel();
      mCurrentTimerTarget = TimeStamp();
    }
  }


  struct Entry
  {
    TimeStamp mTimeStamp;
    nsRefPtr<MediaTimerPromise::Private> mPromise;

    explicit Entry(const TimeStamp& aTimeStamp, const char* aCallSite)
      : mTimeStamp(aTimeStamp)
      , mPromise(new MediaTimerPromise::Private(aCallSite))
    {}

    bool operator<(const Entry& aOther) const
    {
      return mTimeStamp < aOther.mTimeStamp;
    }
  };

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
  nsCOMPtr<nsIEventTarget> mThread;
  std::priority_queue<Entry> mEntries;
  Monitor mMonitor;
  nsCOMPtr<nsITimer> mTimer;
  TimeStamp mCurrentTimerTarget;
  bool mUpdateScheduled;
};

} // namespace mozilla

#endif

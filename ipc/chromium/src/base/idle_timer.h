// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IdleTimer is a recurring Timer which runs only when the system is idle.
// System Idle time is defined as not having any user keyboard or mouse
// activity for some period of time.  Because the timer is user dependant, it
// is possible for the timer to never fire.
//
// Usage should be for low-priority work, and may look like this:
//
//   class MyIdleTimer : public IdleTimer {
//    public:
//     // This timer will run repeatedly after 5 seconds of idle time
//     MyIdleTimer() : IdleTimer(TimeDelta::FromSeconds(5), true) {};
//     virtual void OnIdle() { do something };
//   }
//
//   MyIdleTimer *timer = new MyIdleTimer();
//   timer->Start();
//
//   // As with all Timers, the caller must dispose the object.
//   delete timer;  // Will Stop the timer and cleanup.
//
// NOTE: An IdleTimer can only be used on a thread that processes UI events.
// Such a thread should be running a MessageLoopForUI.

#ifndef BASE_IDLE_TIMER_H_
#define BASE_IDLE_TIMER_H_

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/basictypes.h"
#include "base/task.h"
#include "base/timer.h"

namespace base {

// Function prototype - Get the number of milliseconds that the user has been
// idle.
typedef bool (*IdleTimeSource)(int32_t *milliseconds_interval_since_last_event);

class IdleTimer {
 public:
  // Create an IdleTimer.
  // idle_time: idle time required before this timer can run.
  // repeat: true if the timer should fire multiple times per idle,
  //         false to fire once per idle.
  IdleTimer(TimeDelta idle_time, bool repeat);

  // On destruction, the IdleTimer will Stop itself.
  virtual ~IdleTimer();

  // Start the IdleTimer.
  void Start();

  // Stop the IdleTimer.
  void Stop();

  // The method to run when the timer elapses.
  virtual void OnIdle() = 0;

 protected:
  // Override the IdleTimeSource.
  void set_idle_time_source(IdleTimeSource idle_time_source) {
    idle_time_source_ = idle_time_source;
  }

 private:
  // Called when timer_ expires.
  void Run();

  // Start the timer.
  void StartTimer();

  // Gets the number of milliseconds since the last input event.
  TimeDelta CurrentIdleTime();

  // Compute time until idle.  Returns 0 if we are now idle.
  TimeDelta TimeUntilIdle();

  TimeDelta idle_interval_;
  bool repeat_;
  Time last_time_fired_;  // The last time the idle timer fired.
                          // will be 0 until the timer fires the first time.
  OneShotTimer<IdleTimer> timer_;

  IdleTimeSource idle_time_source_;

  DISALLOW_COPY_AND_ASSIGN(IdleTimer);
};

}  // namespace base

#endif  // BASE_IDLE_TIMER_H_

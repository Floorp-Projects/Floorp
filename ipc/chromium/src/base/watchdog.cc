// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/watchdog.h"

#include "base/compiler_specific.h"
#include "base/platform_thread.h"

using base::TimeDelta;
using base::TimeTicks;

//------------------------------------------------------------------------------
// Public API methods.

// Start thread running in a Disarmed state.
Watchdog::Watchdog(const TimeDelta& duration,
                   const std::string& thread_watched_name,
                   bool enabled)
  : init_successful_(false),
    lock_(),
    condition_variable_(&lock_),
    state_(DISARMED),
    duration_(duration),
    thread_watched_name_(thread_watched_name),
    ALLOW_THIS_IN_INITIALIZER_LIST(delegate_(this)) {
  if (!enabled)
    return;  // Don't start thread, or doing anything really.
  init_successful_ = PlatformThread::Create(0,  // Default stack size.
                                            &delegate_,
                                            &handle_);
  DCHECK(init_successful_);
}

// Notify watchdog thread, and wait for it to finish up.
Watchdog::~Watchdog() {
  if (!init_successful_)
    return;
  {
    AutoLock lock(lock_);
    state_ = SHUTDOWN;
  }
  condition_variable_.Signal();
  PlatformThread::Join(handle_);
}

void Watchdog::Arm() {
  ArmAtStartTime(TimeTicks::Now());
}

void Watchdog::ArmSomeTimeDeltaAgo(const TimeDelta& time_delta) {
  ArmAtStartTime(TimeTicks::Now() - time_delta);
}

// Start clock for watchdog.
void Watchdog::ArmAtStartTime(const TimeTicks start_time) {
  {
    AutoLock lock(lock_);
    start_time_ = start_time;
    state_ = ARMED;
  }
  // Force watchdog to wake up, and go to sleep with the timer ticking with the
  // proper duration.
  condition_variable_.Signal();
}

// Disable watchdog so that it won't do anything when time expires.
void Watchdog::Disarm() {
  AutoLock lock(lock_);
  state_ = DISARMED;
  // We don't need to signal, as the watchdog will eventually wake up, and it
  // will check its state and time, and act accordingly.
}

//------------------------------------------------------------------------------
// Internal private methods that the watchdog thread uses.

void Watchdog::ThreadDelegate::ThreadMain() {
  SetThreadName();
  TimeDelta remaining_duration;
  while (1) {
    AutoLock lock(watchdog_->lock_);
    while (DISARMED == watchdog_->state_)
      watchdog_->condition_variable_.Wait();
    if (SHUTDOWN == watchdog_->state_)
      return;
    DCHECK(ARMED == watchdog_->state_);
    remaining_duration = watchdog_->duration_ -
        (TimeTicks::Now() - watchdog_->start_time_);
    if (remaining_duration.InMilliseconds() > 0) {
      // Spurios wake?  Timer drifts?  Go back to sleep for remaining time.
      watchdog_->condition_variable_.TimedWait(remaining_duration);
    } else {
      // We overslept, so this seems like a real alarm.
      // Watch out for a user that stopped the debugger on a different alarm!
      {
        AutoLock static_lock(static_lock_);
        if (last_debugged_alarm_time_ > watchdog_->start_time_) {
          // False alarm: we started our clock before the debugger break (last
          // alarm time).
          watchdog_->start_time_ += last_debugged_alarm_delay_;
          if (last_debugged_alarm_time_ > watchdog_->start_time_)
            // Too many alarms must have taken place.
            watchdog_->state_ = DISARMED;
          continue;
        }
      }
      watchdog_->state_ = DISARMED;  // Only alarm at most once.
      TimeTicks last_alarm_time = TimeTicks::Now();
      watchdog_->Alarm();  // Set a break point here to debug on alarms.
      TimeDelta last_alarm_delay = TimeTicks::Now() - last_alarm_time;
      if (last_alarm_delay > TimeDelta::FromMilliseconds(2)) {
        // Ignore race of two alarms/breaks going off at roughly the same time.
        AutoLock static_lock(static_lock_);
        // This was a real debugger break.
        last_debugged_alarm_time_ = last_alarm_time;
        last_debugged_alarm_delay_ = last_alarm_delay;
      }
    }
  }
}

void Watchdog::ThreadDelegate::SetThreadName() const {
  std::string name = watchdog_->thread_watched_name_ + " Watchdog";
  PlatformThread::SetName(name.c_str());
  DLOG(INFO) << "Watchdog active: " << name;
}

// static
Lock Watchdog::static_lock_;  // Lock for access of static data...
// static
TimeTicks Watchdog::last_debugged_alarm_time_ = TimeTicks();
// static
TimeDelta Watchdog::last_debugged_alarm_delay_;

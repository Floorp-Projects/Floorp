/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_default.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_nsautorelease_pool.h"
#include "GeckoProfiler.h"

#include "mozilla/BackgroundHangMonitor.h"

namespace base {

MessagePumpDefault::MessagePumpDefault()
    : keep_running_(true),
      event_(false, false) {
}

void MessagePumpDefault::Run(Delegate* delegate) {
  DCHECK(keep_running_) << "Quit must have been called outside of Run!";

  const MessageLoop* const loop = MessageLoop::current();
  mozilla::BackgroundHangMonitor hangMonitor(
    loop->thread_name().c_str(),
    loop->transient_hang_timeout(),
    loop->permanent_hang_timeout());

  for (;;) {
    ScopedNSAutoreleasePool autorelease_pool;

    hangMonitor.NotifyActivity();
    bool did_work = delegate->DoWork();
    if (!keep_running_)
      break;

    hangMonitor.NotifyActivity();
    did_work |= delegate->DoDelayedWork(&delayed_work_time_);
    if (!keep_running_)
      break;

    if (did_work)
      continue;

    hangMonitor.NotifyActivity();
    did_work = delegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (did_work)
      continue;

    if (delayed_work_time_.is_null()) {
      hangMonitor.NotifyWait();
      PROFILER_LABEL("MessagePump", "Wait",
        js::ProfileEntry::Category::OTHER);
      {
        mozilla::AutoProfilerThreadSleep sleep;
        event_.Wait();
      }
    } else {
      TimeDelta delay = delayed_work_time_ - TimeTicks::Now();
      if (delay > TimeDelta()) {
        hangMonitor.NotifyWait();
        PROFILER_LABEL("MessagePump", "Wait",
          js::ProfileEntry::Category::OTHER);
        {
          mozilla::AutoProfilerThreadSleep sleep;
          event_.TimedWait(delay);
        }
      } else {
        // It looks like delayed_work_time_ indicates a time in the past, so we
        // need to call DoDelayedWork now.
        delayed_work_time_ = TimeTicks();
      }
    }
    // Since event_ is auto-reset, we don't need to do anything special here
    // other than service each delegate method.
  }

  keep_running_ = true;
}

void MessagePumpDefault::Quit() {
  keep_running_ = false;
}

void MessagePumpDefault::ScheduleWork() {
  // Since this can be called on any thread, we need to ensure that our Run
  // loop wakes up.
  event_.Signal();
}

void MessagePumpDefault::ScheduleDelayedWork(
    const TimeTicks& delayed_work_time) {
  // We know that we can't be blocked on Wait right now since this method can
  // only be called on the same thread as Run, so we only need to update our
  // record of how long to sleep when we do sleep.
  delayed_work_time_ = delayed_work_time;
}

}  // namespace base

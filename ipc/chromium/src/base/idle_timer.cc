// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/idle_timer.h"

// We may not want to port idle_timer to Linux, but we have implemented it
// anyway.  Define this to 1 to enable the Linux idle timer and then add the
// libs that need to be linked (Xss).
#define ENABLE_XSS_SUPPORT 0

#if defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif

#if defined(OS_LINUX) && ENABLE_XSS_SUPPORT
// We may not want to port idle_timer to Linux, but we have implemented it
// anyway.  Remove the 0 above if we want it.
#include <gdk/gdkx.h>
#include <X11/extensions/scrnsaver.h>
#include "base/lazy_instance.h"
#include "base/thread_local.h"
#endif

#include "base/message_loop.h"
#include "base/time.h"

namespace base {

#if defined(OS_WIN)
bool OSIdleTimeSource(int32 *milliseconds_interval_since_last_event) {
  LASTINPUTINFO lastInputInfo;
  lastInputInfo.cbSize = sizeof(lastInputInfo);
  if (GetLastInputInfo(&lastInputInfo) == 0) {
    return false;
  }
  int32 last_input_time = lastInputInfo.dwTime;

  // Note: On Windows GetLastInputInfo returns a 32bit value which rolls over
  // ~49days.
  int32 current_time = GetTickCount();
  int32 delta = current_time - last_input_time;
  // delta will go negative if we've been idle for 2GB of ticks.
  if (delta < 0)
    delta = -delta;
  *milliseconds_interval_since_last_event = delta;
  return true;
}
#elif defined(OS_MACOSX)
bool OSIdleTimeSource(int32 *milliseconds_interval_since_last_event) {
  *milliseconds_interval_since_last_event =
      CGEventSourceSecondsSinceLastEventType(
          kCGEventSourceStateCombinedSessionState,
          kCGAnyInputEventType) * 1000.0;
  return true;
}
#elif defined(OS_LINUX) && ENABLE_XSS_SUPPORT
class IdleState {
 public:
  IdleState() {
    int event_base, error_base;
    have_idle_info_ = XScreenSaverQueryExtension(GDK_DISPLAY(), &event_base,
                                                 &error_base);
    if (have_idle_info_)
      idle_info_.Set(XScreenSaverAllocInfo());
  }

  ~IdleState() {
    if (idle_info_.Get()) {
      XFree(idle_info_.Get());
      idle_info_.~ThreadLocalPointer();
    }
  }

  int32 IdleTime() {
    if (have_idle_info_ && idle_info_.Get()) {
      XScreenSaverQueryInfo(GDK_DISPLAY(), GDK_ROOT_WINDOW(),
                            idle_info_.Get());
      return idle_info_.Get()->idle;
    }
    return -1;
  }

 private:
  bool have_idle_info_;
  ThreadLocalPointer<XScreenSaverInfo> idle_info_;

  DISALLOW_COPY_AND_ASSIGN(IdleState);
};

bool OSIdleTimeSource(int32* milliseconds_interval_since_last_event) {
  static LazyInstance<IdleState> state_instance(base::LINKER_INITIALIZED);
  IdleState* state = state_instance.Pointer();
  int32 idle_time = state->IdleTime();
  if (0 < idle_time) {
    *milliseconds_interval_since_last_event = idle_time;
    return true;
  }
  return false;
}
#endif

IdleTimer::IdleTimer(TimeDelta idle_time, bool repeat)
    : idle_interval_(idle_time),
      repeat_(repeat),
      idle_time_source_(OSIdleTimeSource) {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type()) <<
      "Requires a thread that processes Windows UI events";
}

IdleTimer::~IdleTimer() {
  Stop();
}

void IdleTimer::Start() {
  StartTimer();
}

void IdleTimer::Stop() {
  timer_.Stop();
}

void IdleTimer::Run() {
  // Verify we can fire the idle timer.
  if (TimeUntilIdle().InMilliseconds() <= 0) {
    OnIdle();
    last_time_fired_ = Time::Now();
  }
  Stop();
  StartTimer();  // Restart the timer for next run.
}

void IdleTimer::StartTimer() {
  DCHECK(!timer_.IsRunning());
  TimeDelta delay = TimeUntilIdle();
  if (delay.InMilliseconds() < 0)
    delay = TimeDelta();
  timer_.Start(delay, this, &IdleTimer::Run);
}

TimeDelta IdleTimer::CurrentIdleTime() {
  int32 interval = 0;
  if (idle_time_source_(&interval)) {
    return TimeDelta::FromMilliseconds(interval);
  }
  NOTREACHED();
  return TimeDelta::FromMilliseconds(0);
}

TimeDelta IdleTimer::TimeUntilIdle() {
  TimeDelta time_since_last_fire = Time::Now() - last_time_fired_;
  TimeDelta current_idle_time = CurrentIdleTime();
  if (current_idle_time > time_since_last_fire) {
    if (repeat_)
      return idle_interval_ - time_since_last_fire;
    return idle_interval_;
  }
  return idle_interval_ - current_idle_time;
}

}  // namespace base

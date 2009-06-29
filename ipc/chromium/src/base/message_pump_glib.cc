// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_glib.h"

#include <fcntl.h>
#include <math.h>

#include "base/eintr_wrapper.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/platform_thread.h"

namespace {

// We send a byte across a pipe to wakeup the event loop.
const char kWorkScheduled = '\0';

// Return a timeout suitable for the glib loop, -1 to block forever,
// 0 to return right away, or a timeout in milliseconds from now.
int GetTimeIntervalMilliseconds(base::Time from) {
  if (from.is_null())
    return -1;

  // Be careful here.  TimeDelta has a precision of microseconds, but we want a
  // value in milliseconds.  If there are 5.5ms left, should the delay be 5 or
  // 6?  It should be 6 to avoid executing delayed work too early.
  int delay = static_cast<int>(
      ceil((from - base::Time::Now()).InMillisecondsF()));

  // If this value is negative, then we need to run delayed work soon.
  return delay < 0 ? 0 : delay;
}

// A brief refresher on GLib:
//     GLib sources have four callbacks: Prepare, Check, Dispatch and Finalize.
// On each iteration of the GLib pump, it calls each source's Prepare function.
// This function should return TRUE if it wants GLib to call its Dispatch, and
// FALSE otherwise.  It can also set a timeout in this case for the next time
// Prepare should be called again (it may be called sooner).
//     After the Prepare calls, GLib does a poll to check for events from the
// system.  File descriptors can be attached to the sources.  The poll may block
// if none of the Prepare calls returned TRUE.  It will block indefinitely, or
// by the minimum time returned by a source in Prepare.
//     After the poll, GLib calls Check for each source that returned FALSE
// from Prepare.  The return value of Check has the same meaning as for Prepare,
// making Check a second chance to tell GLib we are ready for Dispatch.
//     Finally, GLib calls Dispatch for each source that is ready.  If Dispatch
// returns FALSE, GLib will destroy the source.  Dispatch calls may be recursive
// (i.e., you can call Run from them), but Prepare and Check cannot.
//     Finalize is called when the source is destroyed.

struct WorkSource : public GSource {
  base::MessagePumpForUI* pump;
};

gboolean WorkSourcePrepare(GSource* source,
                           gint* timeout_ms) {
  *timeout_ms = static_cast<WorkSource*>(source)->pump->HandlePrepare();
  // We always return FALSE, so that our timeout is honored.  If we were
  // to return TRUE, the timeout would be considered to be 0 and the poll
  // would never block.  Once the poll is finished, Check will be called.
  return FALSE;
}

gboolean WorkSourceCheck(GSource* source) {
  // Always return TRUE, and Dispatch will be called.
  return TRUE;
}

gboolean WorkSourceDispatch(GSource* source,
                            GSourceFunc unused_func,
                            gpointer unused_data) {

  static_cast<WorkSource*>(source)->pump->HandleDispatch();
  // Always return TRUE so our source stays registered.
  return TRUE;
}

// I wish these could be const, but g_source_new wants non-const.
GSourceFuncs WorkSourceFuncs = {
  WorkSourcePrepare,
  WorkSourceCheck,
  WorkSourceDispatch,
  NULL
};

}  // namespace


namespace base {

MessagePumpForUI::MessagePumpForUI()
    : state_(NULL),
      context_(g_main_context_default()) {
  // Create our wakeup pipe, which is used to flag when work was scheduled.
  int fds[2];
  CHECK(pipe(fds) == 0);
  wakeup_pipe_read_  = fds[0];
  wakeup_pipe_write_ = fds[1];
  wakeup_gpollfd_.fd = wakeup_pipe_read_;
  wakeup_gpollfd_.events = G_IO_IN;

  work_source_ = g_source_new(&WorkSourceFuncs, sizeof(WorkSource));
  static_cast<WorkSource*>(work_source_)->pump = this;
  g_source_add_poll(work_source_, &wakeup_gpollfd_);
  // Use a low priority so that we let other events in the queue go first.
  g_source_set_priority(work_source_, G_PRIORITY_DEFAULT_IDLE);
  // This is needed to allow Run calls inside Dispatch.
  g_source_set_can_recurse(work_source_, TRUE);
  g_source_attach(work_source_, context_);
}

MessagePumpForUI::~MessagePumpForUI() {
  g_source_destroy(work_source_);
  g_source_unref(work_source_);
  close(wakeup_pipe_read_);
  close(wakeup_pipe_write_);
}

void MessagePumpForUI::Run(Delegate* delegate) {
#ifndef NDEBUG
  // Make sure we only run this on one thread.  GTK only has one message pump
  // so we can only have one UI loop per process.
  static PlatformThreadId thread_id = PlatformThread::CurrentId();
  DCHECK(thread_id == PlatformThread::CurrentId()) <<
      "Running MessagePumpForUI on two different threads; "
      "this is unsupported by GLib!";
#endif

  RunState state;
  state.delegate = delegate;
  state.should_quit = false;
  state.run_depth = state_ ? state_->run_depth + 1 : 1;
  // We really only do a single task for each iteration of the loop.  If we
  // have done something, assume there is likely something more to do.  This
  // will mean that we don't block on the message pump until there was nothing
  // more to do.  We also set this to true to make sure not to block on the
  // first iteration of the loop, so RunAllPending() works correctly.
  state.more_work_is_plausible = true;

  RunState* previous_state = state_;
  state_ = &state;

  // We run our own loop instead of using g_main_loop_quit in one of the
  // callbacks.  This is so we only quit our own loops, and we don't quit
  // nested loops run by others.  TODO(deanm): Is this what we want?
  while (!state_->should_quit)
    g_main_context_iteration(context_, true);

  state_ = previous_state;
}

// Return the timeout we want passed to poll.
int MessagePumpForUI::HandlePrepare() {
  // If it's likely that we have more work, don't let the pump
  // block so that we can do some processing.
  if (state_->more_work_is_plausible)
    return 0;

  // Work wasn't plausible, so we'll block.  In the case where glib fires
  // our Dispatch(), |more_work_is_plausible| will be reset to whatever it
  // should be.  However, so we don't get starved by more important work,
  // we set |more_work_is_plausible| to true.  This means if we come back
  // here without having been through Dispatch(), we will get a chance to be
  // fired and properly do our work in Dispatch().
  state_->more_work_is_plausible = true;

  // We don't think we have work to do, but make sure not to block
  // longer than the next time we need to run delayed work.
  return GetTimeIntervalMilliseconds(delayed_work_time_);
}

void MessagePumpForUI::HandleDispatch() {
  // We should only ever have a single message on the wakeup pipe, since we
  // are only signaled when the queue went from empty to non-empty.  The glib
  // poll will tell us whether there was data, so this read shouldn't block.
  if (wakeup_gpollfd_.revents & G_IO_IN) {
    char msg;
    if (HANDLE_EINTR(read(wakeup_pipe_read_, &msg, 1)) != 1 || msg != '!') {
      NOTREACHED() << "Error reading from the wakeup pipe.";
    }
  }

  if (state_->should_quit)
    return;

  state_->more_work_is_plausible = false;

  if (state_->delegate->DoWork())
    state_->more_work_is_plausible = true;

  if (state_->should_quit)
    return;

  if (state_->delegate->DoDelayedWork(&delayed_work_time_))
    state_->more_work_is_plausible = true;
  if (state_->should_quit)
    return;

  // Don't do idle work if we think there are more important things
  // that we could be doing.
  if (state_->more_work_is_plausible)
    return;

  if (state_->delegate->DoIdleWork())
    state_->more_work_is_plausible = true;
  if (state_->should_quit)
    return;
}

void MessagePumpForUI::Quit() {
  if (state_) {
    state_->should_quit = true;
  } else {
    NOTREACHED() << "Quit called outside Run!";
  }
}

void MessagePumpForUI::ScheduleWork() {
  // This can be called on any thread, so we don't want to touch any state
  // variables as we would then need locks all over.  This ensures that if
  // we are sleeping in a poll that we will wake up.
  char msg = '!';
  if (HANDLE_EINTR(write(wakeup_pipe_write_, &msg, 1)) != 1) {
    NOTREACHED() << "Could not write to the UI message loop wakeup pipe!";
  }
}

void MessagePumpForUI::ScheduleDelayedWork(const Time& delayed_work_time) {
  // We need to wake up the loop in case the poll timeout needs to be
  // adjusted.  This will cause us to try to do work, but that's ok.
  delayed_work_time_ = delayed_work_time;
  ScheduleWork();
}

}  // namespace base

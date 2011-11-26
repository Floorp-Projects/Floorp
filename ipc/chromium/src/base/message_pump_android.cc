// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_android.h"

#include <fcntl.h>
#include <math.h>

#include "base/eintr_wrapper.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/platform_thread.h"

namespace mozilla {
bool ProcessNextEvent();
void NotifyEvent();
}

namespace base {

MessagePumpForUI::MessagePumpForUI()
  : state_(NULL)
  , pump(*this)
{
}

MessagePumpForUI::~MessagePumpForUI() {
}

MessagePumpAndroid::MessagePumpAndroid(MessagePumpForUI &aPump)
  : pump(aPump)
{
}

MessagePumpAndroid::~MessagePumpAndroid()
{
}

void MessagePumpForUI::Run(Delegate* delegate) {
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

  while (!state_->should_quit) {
    mozilla::ProcessNextEvent();
    if (work_scheduled) {
      work_scheduled = false;
      HandleDispatch();
    }
  }

  state_ = previous_state;
}

void MessagePumpForUI::HandleDispatch() {
  // We should only ever have a single message on the wakeup pipe, since we
  // are only signaled when the queue went from empty to non-empty.  The qApp
  // poll will tell us whether there was data, so this read shouldn't block.
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
  work_scheduled = true;
  mozilla::NotifyEvent();
}

void MessagePumpForUI::ScheduleDelayedWork(const Time& delayed_work_time) {
  // We need to wake up the loop in case the poll timeout needs to be
  // adjusted.  This will cause us to try to do work, but that's ok.
  delayed_work_time_ = delayed_work_time;
  ScheduleWork();
}

}  // namespace base

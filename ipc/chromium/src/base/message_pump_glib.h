// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_GLIB_H_
#define BASE_MESSAGE_PUMP_GLIB_H_

#include <glib.h>

#include "base/message_pump.h"
#include "base/time.h"

namespace base {

// This class implements a MessagePump needed for TYPE_UI MessageLoops on
// OS_LINUX platforms using GLib.
class MessagePumpForUI : public MessagePump {
 public:
  MessagePumpForUI();
  ~MessagePumpForUI();

  virtual void Run(Delegate* delegate);
  virtual void Quit();
  virtual void ScheduleWork();
  virtual void ScheduleDelayedWork(const Time& delayed_work_time);

  // Internal methods used for processing the pump callbacks.  They are
  // public for simplicity but should not be used directly.  HandlePrepare
  // is called during the prepare step of glib, and returns a timeout that
  // will be passed to the poll.  HandleDispatch is called after the poll
  // has completed.
  int HandlePrepare();
  void HandleDispatch();

 private:
  // We may make recursive calls to Run, so we save state that needs to be
  // separate between them in this structure type.
  struct RunState {
    Delegate* delegate;

    // Used to flag that the current Run() invocation should return ASAP.
    bool should_quit;

    // Used to count how many Run() invocations are on the stack.
    int run_depth;

    // Used internally for controlling whether we want a message pump
    // iteration to be blocking or not.
    bool more_work_is_plausible;
  };

  RunState* state_;

  // This is a GLib structure that we can add event sources to.  We use the
  // default GLib context, which is the one to which all GTK events are
  // dispatched.
  GMainContext* context_;

  // This is the time when we need to do delayed work.
  Time delayed_work_time_;

  // The work source.  It is shared by all calls to Run and destroyed when
  // the message pump is destroyed.
  GSource* work_source_;

  // We use a wakeup pipe to make sure we'll get out of the glib polling phase
  // when another thread has scheduled us to do some work.  There is a glib
  // mechanism g_main_context_wakeup, but this won't guarantee that our event's
  // Dispatch() will be called.
  int wakeup_pipe_read_;
  int wakeup_pipe_write_;
  GPollFD wakeup_gpollfd_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpForUI);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_GLIB_H_

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_ANDROID_H_
#define BASE_MESSAGE_PUMP_ANDROID_H_

#include "base/message_pump.h"
#include "base/time.h"

namespace base {

class MessagePumpForUI;

class MessagePumpAndroid {

 public:
  MessagePumpAndroid(MessagePumpForUI &pump);
  ~MessagePumpAndroid();

 private:
  base::MessagePumpForUI &pump;
};

// This class implements a MessagePump needed for TYPE_UI MessageLoops on
// Android
class MessagePumpForUI : public MessagePump {

 public:
  MessagePumpForUI();
  ~MessagePumpForUI();

  virtual void Run(Delegate* delegate);
  virtual void Quit();
  virtual void ScheduleWork();
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time);

  // Internal methods used for processing the pump callbacks.  They are
  // public for simplicity but should not be used directly.
  // HandleDispatch is called after the poll has completed.
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

  // This is the time when we need to do delayed work.
  TimeTicks delayed_work_time_;

  bool work_scheduled;

  // MessagePump implementation for Android based on the GLib implement.
  MessagePumpAndroid pump;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpForUI);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_ANDROID_H_

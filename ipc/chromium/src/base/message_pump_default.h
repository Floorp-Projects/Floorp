// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_DEFAULT_H_
#define BASE_MESSAGE_PUMP_DEFAULT_H_

#include "base/message_pump.h"
#include "base/time.h"
#include "base/waitable_event.h"

namespace base {

class MessagePumpDefault : public MessagePump {
 public:
  MessagePumpDefault();
  ~MessagePumpDefault() {}

  // MessagePump methods:
  virtual void Run(Delegate* delegate);
  virtual void Quit();
  virtual void ScheduleWork();
  virtual void ScheduleDelayedWork(const Time& delayed_work_time);

 private:
  // This flag is set to false when Run should return.
  bool keep_running_;

  // Used to sleep until there is more work to do.
  WaitableEvent event_;

  // The time at which we should call DoDelayedWork.
  Time delayed_work_time_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpDefault);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_DEFAULT_H_

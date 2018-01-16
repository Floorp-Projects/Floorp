/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
  virtual void Run(Delegate* delegate) override;
  virtual void Quit() override;
  virtual void ScheduleWork() override;
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;

 protected:
  // This flag is set to false when Run should return.
  bool keep_running_;

  // Used to sleep until there is more work to do.
  WaitableEvent event_;

  // The time at which we should call DoDelayedWork.
  TimeTicks delayed_work_time_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MessagePumpDefault);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_DEFAULT_H_

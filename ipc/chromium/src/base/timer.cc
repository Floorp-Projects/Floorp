/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer.h"

#include "base/message_loop.h"

namespace base {

void BaseTimer_Helper::OrphanDelayedTask() {
  if (delayed_task_) {
    delayed_task_->timer_ = NULL;
    delayed_task_ = NULL;
  }
}

void BaseTimer_Helper::InitiateDelayedTask(TimerTask* timer_task) {
  OrphanDelayedTask();

  delayed_task_ = timer_task;
  delayed_task_->timer_ = this;
  RefPtr<TimerTask> addrefedTask = timer_task;
  MessageLoop::current()->PostDelayedTask(
      addrefedTask.forget(),
      static_cast<int>(timer_task->delay_.InMilliseconds()));
}

}  // namespace base

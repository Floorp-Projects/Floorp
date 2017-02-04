// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/condition_variable.h"

#include "base/lock.h"
#include "base/logging.h"
#include "base/time.h"

ConditionVariable::ConditionVariable(Lock* user_lock)
    : srwlock_(user_lock->lock_.native_handle())
{
  DCHECK(user_lock);
  InitializeConditionVariable(&cv_);
}

ConditionVariable::~ConditionVariable() = default;

void ConditionVariable::Wait() {
  TimedWait(base::TimeDelta::FromMilliseconds(INFINITE));
}

void ConditionVariable::TimedWait(const base::TimeDelta& max_time) {
  DWORD timeout = static_cast<DWORD>(max_time.InMilliseconds());

  if (!SleepConditionVariableSRW(&cv_, srwlock_, timeout, 0)) {
    // On failure, we only expect the CV to timeout. Any other error value means
    // that we've unexpectedly woken up.
    // Note that WAIT_TIMEOUT != ERROR_TIMEOUT. WAIT_TIMEOUT is used with the
    // WaitFor* family of functions as a direct return value. ERROR_TIMEOUT is
    // used with GetLastError().
    DCHECK_EQ(static_cast<DWORD>(ERROR_TIMEOUT), GetLastError());
  }
}

void ConditionVariable::Broadcast() {
  WakeAllConditionVariable(&cv_);
}

void ConditionVariable::Signal() {
  WakeConditionVariable(&cv_);
}

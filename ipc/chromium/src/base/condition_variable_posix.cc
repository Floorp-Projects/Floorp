// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/condition_variable.h"

#include <errno.h>
#include <sys/time.h>

#include "base/lock.h"
#include "base/lock_impl.h"
#include "base/logging.h"
#include "base/time.h"

using base::Time;
using base::TimeDelta;

ConditionVariable::ConditionVariable(Lock* user_lock)
    : user_mutex_(user_lock->lock_impl()->os_lock()) {
  int rv = pthread_cond_init(&condition_, NULL);
  DCHECK(rv == 0);
}

ConditionVariable::~ConditionVariable() {
  int rv = pthread_cond_destroy(&condition_);
  DCHECK(rv == 0);
}

void ConditionVariable::Wait() {
  int rv = pthread_cond_wait(&condition_, user_mutex_);
  DCHECK(rv == 0);
}

void ConditionVariable::TimedWait(const TimeDelta& max_time) {
  int64 usecs = max_time.InMicroseconds();

  // The timeout argument to pthread_cond_timedwait is in absolute time.
  struct timeval now;
  gettimeofday(&now, NULL);

  struct timespec abstime;
  abstime.tv_sec = now.tv_sec + (usecs / Time::kMicrosecondsPerSecond);
  abstime.tv_nsec = (now.tv_usec + (usecs % Time::kMicrosecondsPerSecond)) *
                    Time::kNanosecondsPerMicrosecond;
  abstime.tv_sec += abstime.tv_nsec / Time::kNanosecondsPerSecond;
  abstime.tv_nsec %= Time::kNanosecondsPerSecond;
  DCHECK(abstime.tv_sec >= now.tv_sec);  // Overflow paranoia

  int rv = pthread_cond_timedwait(&condition_, user_mutex_, &abstime);
  DCHECK(rv == 0 || rv == ETIMEDOUT);
}

void ConditionVariable::Broadcast() {
  int rv = pthread_cond_broadcast(&condition_);
  DCHECK(rv == 0);
}

void ConditionVariable::Signal() {
  int rv = pthread_cond_signal(&condition_);
  DCHECK(rv == 0);
}

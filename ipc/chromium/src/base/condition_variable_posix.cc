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
  int rv = 0;
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  pthread_condattr_t attrs;
  rv = pthread_condattr_init(&attrs);
  DCHECK_EQ(0, rv);
  pthread_condattr_setclock(&attrs, CLOCK_MONOTONIC);
  rv = pthread_cond_init(&condition_, &attrs);
  pthread_condattr_destroy(&attrs);
#else
  rv = pthread_cond_init(&condition_, NULL);
#endif
  DCHECK_EQ(0, rv);
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
  int64_t usecs = max_time.InMicroseconds();

  struct timespec relative_time;
  relative_time.tv_sec = usecs / Time::kMicrosecondsPerSecond;
  relative_time.tv_nsec =
      (usecs % Time::kMicrosecondsPerSecond) * Time::kNanosecondsPerMicrosecond;

#if defined(OS_MACOSX)
  int rv = pthread_cond_timedwait_relative_np(
      &condition_, user_mutex_, &relative_time);
#else
  // The timeout argument to pthread_cond_timedwait is in absolute time.
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  struct timespec absolute_time;
  absolute_time.tv_sec = now.tv_sec;
  absolute_time.tv_nsec = now.tv_nsec;
  absolute_time.tv_sec += relative_time.tv_sec;
  absolute_time.tv_nsec += relative_time.tv_nsec;
  absolute_time.tv_sec += absolute_time.tv_nsec / Time::kNanosecondsPerSecond;
  absolute_time.tv_nsec %= Time::kNanosecondsPerSecond;
  DCHECK_GE(absolute_time.tv_sec, now.tv_sec);  // Overflow paranoia

#if defined(OS_ANDROID)
  int rv = pthread_cond_timedwait_monotonic_np(
      &condition_, user_mutex_, &absolute_time);
#else
  int rv = pthread_cond_timedwait(&condition_, user_mutex_, &absolute_time);
#endif  // OS_ANDROID
#endif  // OS_MACOSX

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

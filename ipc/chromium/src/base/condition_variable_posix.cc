/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/condition_variable.h"

#include <errno.h>
#include <stdint.h>
#include <sys/time.h>

#include "base/lock.h"
#include "base/logging.h"
#include "base/time.h"

ConditionVariable::ConditionVariable(Lock* user_lock)
    : user_mutex_(user_lock->lock_.native_handle()) {
  int rv = 0;
  // http://crbug.com/293736
  // On older Android platform versions, monotonic clock based absolute
  // deadlines are supported through the non-standard
  // pthread_cond_timedwait_monotonic_np. Newer platform versions have
  // pthread_condattr_setclock.
  // Mac can use relative time deadlines.
#if !defined(XP_DARWIN) && \
    !(defined(ANDROID) && defined(HAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC))
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
#if defined(XP_DARWIN)
  // This hack is necessary to avoid a fatal pthreads subsystem bug in the
  // Darwin kernel. http://crbug.com/517681.
  {
    Lock lock;
    AutoLock l(lock);
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1;
    pthread_cond_timedwait_relative_np(&condition_, lock.lock_.native_handle(),
                                       &ts);
  }
#endif

  int rv = pthread_cond_destroy(&condition_);
  DCHECK_EQ(0, rv);
}

void ConditionVariable::Wait() {
  int rv = pthread_cond_wait(&condition_, user_mutex_);
  DCHECK_EQ(0, rv);
}

void ConditionVariable::TimedWait(const base::TimeDelta& max_time) {
  int64_t usecs = max_time.InMicroseconds();
  struct timespec relative_time;
  relative_time.tv_sec = usecs / base::Time::kMicrosecondsPerSecond;
  relative_time.tv_nsec = (usecs % base::Time::kMicrosecondsPerSecond) *
                          base::Time::kNanosecondsPerMicrosecond;

#if defined(XP_DARWIN)
  int rv = pthread_cond_timedwait_relative_np(&condition_, user_mutex_,
                                              &relative_time);
#else
  // The timeout argument to pthread_cond_timedwait is in absolute time.
  struct timespec absolute_time;
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  absolute_time.tv_sec = now.tv_sec;
  absolute_time.tv_nsec = now.tv_nsec;

  absolute_time.tv_sec += relative_time.tv_sec;
  absolute_time.tv_nsec += relative_time.tv_nsec;
  absolute_time.tv_sec +=
      absolute_time.tv_nsec / base::Time::kNanosecondsPerSecond;
  absolute_time.tv_nsec %= base::Time::kNanosecondsPerSecond;
  DCHECK_GE(absolute_time.tv_sec, now.tv_sec);  // Overflow paranoia

#  if defined(ANDROID) && defined(HAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC)
  int rv = pthread_cond_timedwait_monotonic_np(&condition_, user_mutex_,
                                               &absolute_time);
#  else
  int rv = pthread_cond_timedwait(&condition_, user_mutex_, &absolute_time);
#  endif  // ANDROID && HAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC
#endif    // XP_DARWIN

  // On failure, we only expect the CV to timeout. Any other error value means
  // that we've unexpectedly woken up.
  DCHECK(rv == 0 || rv == ETIMEDOUT);
}

void ConditionVariable::Broadcast() {
  int rv = pthread_cond_broadcast(&condition_);
  DCHECK_EQ(0, rv);
}

void ConditionVariable::Signal() {
  int rv = pthread_cond_signal(&condition_);
  DCHECK_EQ(0, rv);
}

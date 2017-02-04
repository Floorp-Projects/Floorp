// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/condition_variable.h"

#include <errno.h>
#include <stdint.h>
#include <sys/time.h>

#include "base/lock.h"
#include "base/time.h"
#include "build/build_config.h"

ConditionVariable::ConditionVariable(Lock* user_lock)
    : user_mutex_(user_lock->lock_.native_handle())
{
  int rv = 0;
  // http://crbug.com/293736
  // NaCl doesn't support monotonic clock based absolute deadlines.
  // On older Android platform versions, it's supported through the
  // non-standard pthread_cond_timedwait_monotonic_np. Newer platform
  // versions have pthread_condattr_setclock.
  // Mac can use relative time deadlines.
#if !defined(OS_MACOSX) && !defined(OS_NACL) && \
      !(defined(OS_ANDROID) && defined(HAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC))
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
#if defined(OS_MACOSX)
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
  relative_time.tv_nsec =
    (usecs % base::Time::kMicrosecondsPerSecond) * base::Time::kNanosecondsPerMicrosecond;

#if defined(OS_MACOSX)
  int rv = pthread_cond_timedwait_relative_np(
      &condition_, user_mutex_, &relative_time);
#else
  // The timeout argument to pthread_cond_timedwait is in absolute time.
  struct timespec absolute_time;
#if defined(OS_NACL)
  // See comment in constructor for why this is different in NaCl.
  struct timeval now;
  gettimeofday(&now, NULL);
  absolute_time.tv_sec = now.tv_sec;
  absolute_time.tv_nsec = now.tv_usec * base::Time::kNanosecondsPerMicrosecond;
#else
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  absolute_time.tv_sec = now.tv_sec;
  absolute_time.tv_nsec = now.tv_nsec;
#endif

  absolute_time.tv_sec += relative_time.tv_sec;
  absolute_time.tv_nsec += relative_time.tv_nsec;
  absolute_time.tv_sec += absolute_time.tv_nsec / base::Time::kNanosecondsPerSecond;
  absolute_time.tv_nsec %= base::Time::kNanosecondsPerSecond;
  DCHECK_GE(absolute_time.tv_sec, now.tv_sec);  // Overflow paranoia

#if defined(OS_ANDROID) && defined(HAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC)
  int rv = pthread_cond_timedwait_monotonic_np(
      &condition_, user_mutex_, &absolute_time);
#else
  int rv = pthread_cond_timedwait(&condition_, user_mutex_, &absolute_time);
#endif  // OS_ANDROID && HAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC
#endif  // OS_MACOSX

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

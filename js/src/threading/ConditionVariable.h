/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef threading_ConditionVariable_h
#define threading_ConditionVariable_h

#include "mozilla/Attributes.h"
#include "mozilla/Move.h"
#include "mozilla/TimeStamp.h"

#include <stdint.h>
#ifndef XP_WIN
# include <pthread.h>
#endif

#include "threading/LockGuard.h"
#include "threading/Mutex.h"

namespace js {

template <typename T> using UniqueLock = LockGuard<T>;

enum class CVStatus {
  NoTimeout,
  Timeout
};

// A poly-fill for std::condition_variable.
class ConditionVariable
{
public:
  struct PlatformData;

  ConditionVariable();
  ~ConditionVariable();

  void notify_one();
  void notify_all();

  void wait(UniqueLock<Mutex>& lock);

  template <typename Predicate>
  void wait(UniqueLock<Mutex>& lock, Predicate pred) {
    while (!pred()) {
      wait(lock);
    }
  }

  CVStatus wait_until(UniqueLock<Mutex>& lock,
                      const mozilla::TimeStamp& abs_time);

  template <typename Predicate>
  bool wait_until(UniqueLock<Mutex>& lock, const mozilla::TimeStamp& abs_time,
                  Predicate pred) {
    while (!pred()) {
      if (wait_until(lock, abs_time) == CVStatus::Timeout) {
        return pred();
      }
    }
    return true;
  }

  CVStatus wait_for(UniqueLock<Mutex>& lock,
                    const mozilla::TimeDuration& rel_time);

  template <typename Predicate>
  bool wait_for(UniqueLock<Mutex>& lock, const mozilla::TimeDuration& rel_time,
                Predicate pred) {
    return wait_until(lock, mozilla::TimeStamp::Now() + rel_time,
                      mozilla::Move(pred));
  }


private:
  ConditionVariable(const ConditionVariable&) = delete;
  ConditionVariable& operator=(const ConditionVariable&) = delete;

  PlatformData* platformData();

#ifndef XP_WIN
  void* platformData_[sizeof(pthread_cond_t) / sizeof(void*)];
  static_assert(sizeof(pthread_cond_t) / sizeof(void*) != 0 &&
                sizeof(pthread_cond_t) % sizeof(void*) == 0,
                "pthread_cond_t must have pointer alignment");
#else
  void* platformData_[4];
#endif
};

} // namespace js

#endif // threading_ConditionVariable_h

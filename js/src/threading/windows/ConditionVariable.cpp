/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"

#include <float.h>
#include <intrin.h>
#include <stdlib.h>
#include <windows.h>

#include "threading/ConditionVariable.h"
#include "threading/Mutex.h"
#include "threading/windows/MutexPlatformData.h"

// Some versions of the Windows SDK have a bug where some interlocked functions
// are not redefined as compiler intrinsics. Fix that for the interlocked
// functions that are used in this file.
#if defined(_MSC_VER) && !defined(InterlockedExchangeAdd)
#define InterlockedExchangeAdd(addend, value)                                  \
  _InterlockedExchangeAdd((volatile long*)(addend), (long)(value))
#endif

#if defined(_MSC_VER) && !defined(InterlockedIncrement)
#define InterlockedIncrement(addend)                                           \
  _InterlockedIncrement((volatile long*)(addend))
#endif

// Wrapper for native condition variable APIs.
struct js::ConditionVariable::PlatformData
{
  CONDITION_VARIABLE cv_;
};

js::ConditionVariable::ConditionVariable()
{
  InitializeConditionVariable(&platformData()->cv_);
}

void
js::ConditionVariable::notify_one()
{
  WakeConditionVariable(&platformData()->cv_);
}

void
js::ConditionVariable::notify_all()
{
  WakeAllConditionVariable(&platformData()->cv_);
}

void
js::ConditionVariable::wait(UniqueLock<Mutex>& lock)
{
  CRITICAL_SECTION* cs = &lock.lock.platformData()->criticalSection;
  bool r = SleepConditionVariableCS(&platformData()->cv_, cs, INFINITE);
  MOZ_RELEASE_ASSERT(r);
}

js::CVStatus
js::ConditionVariable::wait_until(UniqueLock<Mutex>& lock,
                                  const mozilla::TimeStamp& abs_time)
{
  return wait_for(lock, abs_time - mozilla::TimeStamp::Now());
}

js::CVStatus
js::ConditionVariable::wait_for(UniqueLock<Mutex>& lock,
                                const mozilla::TimeDuration& rel_time)
{
  CRITICAL_SECTION* cs = &lock.lock.platformData()->criticalSection;

  // Note that DWORD is unsigned, so we have to be careful to clamp at 0.
  // If rel_time is Forever, then ToMilliseconds is +inf, which evaluates as
  // greater than UINT32_MAX, resulting in the correct INFINITE wait.
  double msecd = rel_time.ToMilliseconds();
  DWORD msec = msecd < 0.0
               ? 0
               : msecd > UINT32_MAX
                 ? INFINITE
                 : static_cast<DWORD>(msecd);

  BOOL r = SleepConditionVariableCS(&platformData()->cv_, cs, msec);
  if (r)
    return CVStatus::NoTimeout;
  MOZ_RELEASE_ASSERT(GetLastError() == ERROR_TIMEOUT);
  return CVStatus::Timeout;
}

js::ConditionVariable::~ConditionVariable()
{
  // Native condition variables don't require cleanup.
}

inline js::ConditionVariable::PlatformData*
js::ConditionVariable::platformData()
{
  static_assert(sizeof platformData_ >= sizeof(PlatformData),
                "platformData_ is too small");
  return reinterpret_cast<PlatformData*>(platformData_);
}

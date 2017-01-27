/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"

#include "jswin.h"

#include "threading/Mutex.h"
#include "threading/windows/MutexPlatformData.h"

js::detail::MutexImpl::MutexImpl()
{
  // This number was adopted from NSPR.
  const static DWORD LockSpinCount = 1500;
  // Vista and later automatically allocate and subsequently leak a debug info
  // object for each critical section that we allocate unless we tell the
  // system not to do that.
  BOOL r = InitializeCriticalSectionEx(&platformData()->criticalSection,
                                       LockSpinCount,
                                       CRITICAL_SECTION_NO_DEBUG_INFO);
  MOZ_RELEASE_ASSERT(r);
}

js::detail::MutexImpl::~MutexImpl()
{
  DeleteCriticalSection(&platformData()->criticalSection);
}

void
js::detail::MutexImpl::lock()
{
  EnterCriticalSection(&platformData()->criticalSection);
}

void
js::detail::MutexImpl::unlock()
{
  LeaveCriticalSection(&platformData()->criticalSection);
}

js::detail::MutexImpl::PlatformData*
js::detail::MutexImpl::platformData()
{
  static_assert(sizeof(platformData_) >= sizeof(PlatformData),
                "platformData_ is too small");
  return reinterpret_cast<PlatformData*>(platformData_);
}

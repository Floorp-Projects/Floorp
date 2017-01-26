/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

#include "threading/Mutex.h"
#include "threading/posix/MutexPlatformData.h"

#define TRY_CALL_PTHREADS(call, msg)            \
  {                                             \
    int result = (call);                        \
    if (result != 0) {                          \
      errno = result;                           \
      perror(msg);                              \
      MOZ_CRASH(msg);                           \
    }                                           \
  }

js::detail::MutexImpl::MutexImpl()
{
  pthread_mutexattr_t* attrp = nullptr;

#ifdef DEBUG
  pthread_mutexattr_t attr;

  TRY_CALL_PTHREADS(pthread_mutexattr_init(&attr),
                    "js::detail::MutexImpl::MutexImpl: pthread_mutexattr_init failed");

  TRY_CALL_PTHREADS(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK),
                    "js::detail::MutexImpl::MutexImpl: pthread_mutexattr_settype failed");

  attrp = &attr;
#endif

  TRY_CALL_PTHREADS(pthread_mutex_init(&platformData()->ptMutex, attrp),
                    "js::detail::MutexImpl::MutexImpl: pthread_mutex_init failed");

#ifdef DEBUG
  TRY_CALL_PTHREADS(pthread_mutexattr_destroy(&attr),
                    "js::detail::MutexImpl::MutexImpl: pthread_mutexattr_destroy failed");
#endif
}

js::detail::MutexImpl::~MutexImpl()
{
  TRY_CALL_PTHREADS(pthread_mutex_destroy(&platformData()->ptMutex),
                    "js::detail::MutexImpl::~MutexImpl: pthread_mutex_destroy failed");
}

void
js::detail::MutexImpl::lock()
{
  TRY_CALL_PTHREADS(pthread_mutex_lock(&platformData()->ptMutex),
                    "js::detail::MutexImpl::lock: pthread_mutex_lock failed");
}

void
js::detail::MutexImpl::unlock()
{
  TRY_CALL_PTHREADS(pthread_mutex_unlock(&platformData()->ptMutex),
                    "js::detail::MutexImpl::unlock: pthread_mutex_unlock failed");
}

#undef TRY_CALL_PTHREADS

js::detail::MutexImpl::PlatformData*
js::detail::MutexImpl::platformData()
{
  static_assert(sizeof(platformData_) >= sizeof(PlatformData),
                "platformData_ is too small");
  return reinterpret_cast<PlatformData*>(platformData_);
}

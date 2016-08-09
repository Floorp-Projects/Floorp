/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

#include "js/Utility.h"

#include "threading/Mutex.h"
#include "threading/posix/MutexPlatformData.h"

#define TRY_CALL_PTHREADS(call, msg)            \
  {                                             \
    int result = (call);                        \
    if (result != 0) {                          \
      MOZ_ASSERT(!errno);                       \
      errno = result;                           \
      perror(msg);                              \
      MOZ_CRASH(msg);                           \
    }                                           \
  }

js::Mutex::Mutex()
{
  AutoEnterOOMUnsafeRegion oom;
  platformData_ = js_new<PlatformData>();
  if (!platformData_)
    oom.crash("js::Mutex::Mutex");

  pthread_mutexattr_t* attrp = nullptr;

#ifdef DEBUG
  pthread_mutexattr_t attr;

  TRY_CALL_PTHREADS(pthread_mutexattr_init(&attr),
                    "js::Mutex::Mutex: pthread_mutexattr_init failed");

  TRY_CALL_PTHREADS(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK),
                    "js::Mutex::Mutex: pthread_mutexattr_settype failed");

  attrp = &attr;
#endif

  TRY_CALL_PTHREADS(pthread_mutex_init(&platformData()->ptMutex, attrp),
                    "js::Mutex::Mutex: pthread_mutex_init failed");

#ifdef DEBUG
  TRY_CALL_PTHREADS(pthread_mutexattr_destroy(&attr),
                    "js::Mutex::Mutex: pthread_mutexattr_destroy failed");
#endif
}

js::Mutex::~Mutex()
{
  if (!platformData_)
    return;

  TRY_CALL_PTHREADS(pthread_mutex_destroy(&platformData()->ptMutex),
                    "js::Mutex::~Mutex: pthread_mutex_destroy failed");

  js_delete(platformData());
}

void
js::Mutex::lock()
{
  TRY_CALL_PTHREADS(pthread_mutex_lock(&platformData()->ptMutex),
                    "js::Mutex::lock: pthread_mutex_lock failed");
}

void
js::Mutex::unlock()
{
  TRY_CALL_PTHREADS(pthread_mutex_unlock(&platformData()->ptMutex),
                    "js::Mutex::unlock: pthread_mutex_unlock failed");
}

#undef TRY_CALL_PTHREADS

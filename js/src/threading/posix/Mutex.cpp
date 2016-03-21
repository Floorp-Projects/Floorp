/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <pthread.h>

#include "js/Utility.h"

#include "threading/Mutex.h"
#include "threading/posix/MutexPlatformData.h"

js::Mutex::Mutex()
{
  AutoEnterOOMUnsafeRegion oom;
  platformData_ = js_new<PlatformData>();
  if (!platformData_)
    oom.crash("js::Mutex::Mutex");

  int r = pthread_mutex_init(&platformData()->ptMutex, NULL);
  MOZ_RELEASE_ASSERT(r == 0);
}

js::Mutex::~Mutex()
{
  if (!platformData_)
    return;

  int r = pthread_mutex_destroy(&platformData()->ptMutex);
  MOZ_RELEASE_ASSERT(r == 0);
  js_delete(platformData());
}

void
js::Mutex::lock()
{
  int r = pthread_mutex_lock(&platformData()->ptMutex);
  MOZ_RELEASE_ASSERT(r == 0);
}

void
js::Mutex::unlock()
{
  int r = pthread_mutex_unlock(&platformData()->ptMutex);
  MOZ_RELEASE_ASSERT(r == 0);
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"

#include <pthread.h>

#include "threading/Mutex.h"
#include "threading/posix/MutexPlatformData.h"

js::Mutex::Mutex()
{
  int r = pthread_mutex_init(&platformData()->ptMutex, NULL);
  MOZ_RELEASE_ASSERT(r == 0);
}

js::Mutex::~Mutex()
{
  int r = pthread_mutex_destroy(&platformData()->ptMutex);
  MOZ_RELEASE_ASSERT(r == 0);
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

js::Mutex::PlatformData*
js::Mutex::platformData()
{
  static_assert(sizeof(platformData_) >= sizeof(PlatformData),
                "platformData_ is too small");
  return reinterpret_cast<PlatformData*>(platformData_);
}

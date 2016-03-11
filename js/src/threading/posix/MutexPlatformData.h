/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef platform_win_MutexPlatformData_h
#define platform_win_MutexPlatformData_h

#include <pthread.h>

#include "threading/Mutex.h"

namespace js {

struct Mutex::PlatformData
{
  pthread_mutex_t ptMutex;
};

} // namespace js

#endif // platform_win_MutexPlatformData_h

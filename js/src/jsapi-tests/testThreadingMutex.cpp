/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"
#include "threading/LockGuard.h"
#include "vm/MutexIDs.h"

BEGIN_TEST(testThreadingMutex)
{
    js::Mutex mutex(js::mutexid::TestMutex);
    mutex.lock();
    mutex.unlock();
    return true;
}
END_TEST(testThreadingMutex)

BEGIN_TEST(testThreadingLockGuard)
{
    js::Mutex mutex(js::mutexid::TestMutex);
    js::LockGuard<js::Mutex> guard(mutex);
    return true;
}
END_TEST(testThreadingLockGuard)

BEGIN_TEST(testThreadingUnlockGuard)
{
    js::Mutex mutex(js::mutexid::TestMutex);
    js::LockGuard<js::Mutex> guard(mutex);
    js::UnlockGuard<js::Mutex> unguard(guard);
    return true;
}
END_TEST(testThreadingUnlockGuard)

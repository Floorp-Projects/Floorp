/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"
#include "threading/LockGuard.h"
#include "vm/MutexIDs.h"

#ifdef DEBUG
#  define DEBUG_CHECK(x) CHECK(x)
#else
#  define DEBUG_CHECK(x)
#endif

BEGIN_TEST(testThreadingMutex) {
  js::Mutex mutex MOZ_UNANNOTATED(js::mutexid::TestMutex);
  DEBUG_CHECK(!mutex.isOwnedByCurrentThread());
  mutex.lock();
  DEBUG_CHECK(mutex.isOwnedByCurrentThread());
  mutex.unlock();
  DEBUG_CHECK(!mutex.isOwnedByCurrentThread());
  return true;
}
END_TEST(testThreadingMutex)

BEGIN_TEST(testThreadingLockGuard) {
  js::Mutex mutex MOZ_UNANNOTATED(js::mutexid::TestMutex);
  DEBUG_CHECK(!mutex.isOwnedByCurrentThread());
  js::LockGuard guard(mutex);
  DEBUG_CHECK(mutex.isOwnedByCurrentThread());
  return true;
}
END_TEST(testThreadingLockGuard)

BEGIN_TEST(testThreadingUnlockGuard) {
  js::Mutex mutex MOZ_UNANNOTATED(js::mutexid::TestMutex);
  DEBUG_CHECK(!mutex.isOwnedByCurrentThread());
  js::LockGuard guard(mutex);
  DEBUG_CHECK(mutex.isOwnedByCurrentThread());
  js::UnlockGuard unguard(guard);
  DEBUG_CHECK(!mutex.isOwnedByCurrentThread());
  return true;
}
END_TEST(testThreadingUnlockGuard)

#undef DEBUG_CHECK

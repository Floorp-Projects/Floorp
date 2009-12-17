// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LOCK_IMPL_H_
#define BASE_LOCK_IMPL_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX)
#include <pthread.h>
#endif

#include "base/basictypes.h"
#include "base/platform_thread.h"

// This class implements the underlying platform-specific spin-lock mechanism
// used for the Lock class.  Most users should not use LockImpl directly, but
// should instead use Lock.
class LockImpl {
 public:
#if defined(OS_WIN)
  typedef CRITICAL_SECTION OSLockType;
#elif defined(OS_POSIX)
  typedef pthread_mutex_t OSLockType;
#endif

  LockImpl();
  ~LockImpl();

  // If the lock is not held, take it and return true.  If the lock is already
  // held by something else, immediately return false.
  bool Try();

  // Take the lock, blocking until it is available if necessary.
  void Lock();

  // Release the lock.  This must only be called by the lock's holder: after
  // a successful call to Try, or a call to Lock.
  void Unlock();

  // Debug-only method that will DCHECK() if the lock is not acquired by the
  // current thread.  In non-debug builds, no check is performed.
  // Because linux and mac condition variables modify the underlyning lock
  // through the os_lock() method, runtime assertions can not be done on those
  // builds.
#if defined(NDEBUG) || !defined(OS_WIN)
  void AssertAcquired() const {}
#else
  void AssertAcquired() const;
#endif

  // Return the native underlying lock.  Not supported for Windows builds.
  // TODO(awalker): refactor lock and condition variables so that this is
  // unnecessary.
#if !defined(OS_WIN)
  OSLockType* os_lock() { return &os_lock_; }
#endif

 private:
  OSLockType os_lock_;

#if !defined(NDEBUG) && defined(OS_WIN)
  // All private data is implicitly protected by lock_.
  // Be VERY careful to only access members under that lock.
  PlatformThreadId owning_thread_id_;
  int32 recursion_count_shadow_;
  bool recursion_used_;      // Allow debugging to continued after a DCHECK().
#endif  // NDEBUG

  DISALLOW_COPY_AND_ASSIGN(LockImpl);
};


#endif  // BASE_LOCK_IMPL_H_

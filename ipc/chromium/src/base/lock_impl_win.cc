/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lock_impl.h"
#include "base/logging.h"

// NOTE: Although windows critical sections support recursive locks, we do not
// allow this, and we will commonly fire a DCHECK() if a thread attempts to
// acquire the lock a second time (while already holding it).

LockImpl::LockImpl() {
#ifndef NDEBUG
  recursion_count_shadow_ = 0;
  recursion_used_ = false;
  owning_thread_id_ = 0;
#endif  // NDEBUG
  // The second parameter is the spin count, for short-held locks it avoid the
  // contending thread from going to sleep which helps performance greatly.
  ::InitializeCriticalSectionAndSpinCount(&os_lock_, 2000);
}

LockImpl::~LockImpl() {
  ::DeleteCriticalSection(&os_lock_);
}

bool LockImpl::Try() {
  if (::TryEnterCriticalSection(&os_lock_) != FALSE) {
#ifndef NDEBUG
    // ONLY access data after locking.
    owning_thread_id_ = PlatformThread::CurrentId();
    DCHECK_NE(owning_thread_id_, 0);
    recursion_count_shadow_++;
    if (2 == recursion_count_shadow_ && !recursion_used_) {
      recursion_used_ = true;
      DCHECK(false);  // Catch accidental redundant lock acquisition.
    }
#endif
    return true;
  }
  return false;
}

void LockImpl::Lock() {
  ::EnterCriticalSection(&os_lock_);
#ifndef NDEBUG
  // ONLY access data after locking.
  owning_thread_id_ = PlatformThread::CurrentId();
  DCHECK_NE(owning_thread_id_, 0);
  recursion_count_shadow_++;
  if (2 == recursion_count_shadow_ && !recursion_used_) {
    recursion_used_ = true;
    DCHECK(false);  // Catch accidental redundant lock acquisition.
  }
#endif  // NDEBUG
}

void LockImpl::Unlock() {
#ifndef NDEBUG
  --recursion_count_shadow_;  // ONLY access while lock is still held.
  DCHECK(0 <= recursion_count_shadow_);
  owning_thread_id_ = 0;
#endif  // NDEBUG
  ::LeaveCriticalSection(&os_lock_);
}

// In non-debug builds, this method is declared as an empty inline method.
#ifndef NDEBUG
void LockImpl::AssertAcquired() const {
  DCHECK(recursion_count_shadow_ > 0);
  DCHECK_EQ(owning_thread_id_, PlatformThread::CurrentId());
}
#endif

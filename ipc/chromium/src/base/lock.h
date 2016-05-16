/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LOCK_H_
#define BASE_LOCK_H_

#include "base/lock_impl.h"

// A convenient wrapper for an OS specific critical section.

class Lock {
 public:
  Lock() : lock_() {}
  ~Lock() {}
  void Acquire() { lock_.Lock(); }
  void Release() { lock_.Unlock(); }
  // If the lock is not held, take it and return true. If the lock is already
  // held by another thread, immediately return false.
  bool Try() { return lock_.Try(); }

  // In debug builds this method checks that the lock has been acquired by the
  // calling thread.  If the lock has not been acquired, then the method
  // will DCHECK().  In non-debug builds, the LockImpl's implementation of
  // AssertAcquired() is an empty inline method.
  void AssertAcquired() const { return lock_.AssertAcquired(); }

  // Return the underlying lock implementation.
  // TODO(awalker): refactor lock and condition variables so that this is
  // unnecessary.
  LockImpl* lock_impl() { return &lock_; }

 private:
  LockImpl lock_;  // Platform specific underlying lock implementation.

  DISALLOW_COPY_AND_ASSIGN(Lock);
};

// A helper class that acquires the given Lock while the AutoLock is in scope.
class AutoLock {
 public:
  explicit AutoLock(Lock& lock) : lock_(lock) {
    lock_.Acquire();
  }

  ~AutoLock() {
    lock_.AssertAcquired();
    lock_.Release();
  }

 private:
  Lock& lock_;
  DISALLOW_COPY_AND_ASSIGN(AutoLock);
};

// AutoUnlock is a helper that will Release() the |lock| argument in the
// constructor, and re-Acquire() it in the destructor.
class AutoUnlock {
 public:
  explicit AutoUnlock(Lock& lock) : lock_(lock) {
    // We require our caller to have the lock.
    lock_.AssertAcquired();
    lock_.Release();
  }

  ~AutoUnlock() {
    lock_.Acquire();
  }

 private:
  Lock& lock_;
  DISALLOW_COPY_AND_ASSIGN(AutoUnlock);
};

#endif  // BASE_LOCK_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lock_impl.h"

namespace base {
namespace internal {

LockImpl::LockImpl() : native_handle_(SRWLOCK_INIT) {}

LockImpl::~LockImpl() = default;

bool LockImpl::Try() {
  return !!::TryAcquireSRWLockExclusive(&native_handle_);
}

void LockImpl::Lock() {
  ::AcquireSRWLockExclusive(&native_handle_);
}

void LockImpl::Unlock() {
  ::ReleaseSRWLockExclusive(&native_handle_);
}

}  // namespace internal
}  // namespace base

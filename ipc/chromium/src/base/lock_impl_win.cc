/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

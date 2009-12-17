// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/non_thread_safe.h"

// These checks are only done in debug builds.
#ifndef NDEBUG

#include "base/logging.h"

NonThreadSafe::NonThreadSafe()
    : valid_thread_id_(PlatformThread::CurrentId()) {
}

bool NonThreadSafe::CalledOnValidThread() const {
  return valid_thread_id_ == PlatformThread::CurrentId();
}

NonThreadSafe::~NonThreadSafe() {
  DCHECK(CalledOnValidThread());
}

#endif  // NDEBUG

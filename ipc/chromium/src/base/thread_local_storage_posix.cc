// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread_local_storage.h"

#include "base/logging.h"

ThreadLocalStorage::Slot::Slot(TLSDestructorFunc destructor)
    : initialized_(false) {
  Initialize(destructor);
}

bool ThreadLocalStorage::Slot::Initialize(TLSDestructorFunc destructor) {
  DCHECK(!initialized_);
  int error = pthread_key_create(&key_, destructor);
  if (error) {
    NOTREACHED();
    return false;
  }

  initialized_ = true;
  return true;
}

void ThreadLocalStorage::Slot::Free() {
  DCHECK(initialized_);
  int error = pthread_key_delete(key_);
  if (error)
    NOTREACHED();
  initialized_ = false;
}

void* ThreadLocalStorage::Slot::Get() const {
  DCHECK(initialized_);
  return pthread_getspecific(key_);
}

void ThreadLocalStorage::Slot::Set(void* value) {
  DCHECK(initialized_);
  int error = pthread_setspecific(key_, value);
  if (error)
    NOTREACHED();
}

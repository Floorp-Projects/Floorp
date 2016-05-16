/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread_local.h"

#include <pthread.h>

#include "base/logging.h"

namespace base {

// static
void ThreadLocalPlatform::AllocateSlot(SlotType& slot) {
  int error = pthread_key_create(&slot, NULL);
  CHECK(error == 0);
}

// static
void ThreadLocalPlatform::FreeSlot(SlotType& slot) {
  int error = pthread_key_delete(slot);
  DCHECK(error == 0);
}

// static
void* ThreadLocalPlatform::GetValueFromSlot(SlotType& slot) {
  return pthread_getspecific(slot);
}

// static
void ThreadLocalPlatform::SetValueInSlot(SlotType& slot, void* value) {
  int error = pthread_setspecific(slot, value);
  CHECK(error == 0);
}

}  // namespace base

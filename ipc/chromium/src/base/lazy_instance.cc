// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"

#include "base/at_exit.h"
#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/platform_thread.h"

namespace base {

void LazyInstanceHelper::EnsureInstance(void* instance,
                                        void (*ctor)(void*),
                                        void (*dtor)(void*)) {
  // Try to create the instance, if we're the first, will go from EMPTY
  // to CREATING, otherwise we've already been beaten here.
  if (base::subtle::Acquire_CompareAndSwap(
          &state_, STATE_EMPTY, STATE_CREATING) == STATE_EMPTY) {
    // Created the instance in the space provided by |instance|.
    ctor(instance);
    // Instance is created, go from CREATING to CREATED.
    base::subtle::Release_Store(&state_, STATE_CREATED);
    // Register the destructor callback with AtExitManager.
    base::AtExitManager::RegisterCallback(dtor, instance);
  } else {
    // It's either in the process of being created, or already created.  Spin.
    while (base::subtle::NoBarrier_Load(&state_) != STATE_CREATED)
      PlatformThread::YieldCurrentThread();
  }
}

}  // namespace base

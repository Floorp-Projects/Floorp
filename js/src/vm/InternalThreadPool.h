/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * An internal thread pool, used for the shell and when
 * JS::SetHelperThreadTaskCallback not called.
 */

#ifndef vm_InternalThreadPool_h
#define vm_InternalThreadPool_h

#include "js/AllocPolicy.h"
#include "js/UniquePtr.h"
#include "js/Vector.h"
#include "threading/ConditionVariable.h"
#include "threading/ProtectedData.h"

// We want our default stack size limit to be approximately 2MB, to be safe, but
// expect most threads to use much less. On Linux, however, requesting a stack
// of 2MB or larger risks the kernel allocating an entire 2MB huge page for it
// on first access, which we do not want. To avoid this possibility, we subtract
// 2 standard VM page sizes from our default.
static const uint32_t kDefaultHelperStackSize = 2048 * 1024 - 2 * 4096;
static const uint32_t kDefaultHelperStackQuota = 1800 * 1024;

// TSan enforces a minimum stack size that's just slightly larger than our
// default helper stack size.  It does this to store blobs of TSan-specific
// data on each thread's stack.  Unfortunately, that means that even though
// we'll actually receive a larger stack than we requested, the effective
// usable space of that stack is significantly less than what we expect.
// To offset TSan stealing our stack space from underneath us, double the
// default.
//
// Note that we don't need this for ASan/MOZ_ASAN because ASan doesn't
// require all the thread-specific state that TSan does.
#if defined(MOZ_TSAN)
static const uint32_t HELPER_STACK_SIZE = 2 * kDefaultHelperStackSize;
static const uint32_t HELPER_STACK_QUOTA = 2 * kDefaultHelperStackQuota;
#else
static const uint32_t HELPER_STACK_SIZE = kDefaultHelperStackSize;
static const uint32_t HELPER_STACK_QUOTA = kDefaultHelperStackQuota;
#endif

namespace js {

class AutoLockHelperThreadState;
class HelperThread;

using HelperThreadVector =
    Vector<UniquePtr<HelperThread>, 0, SystemAllocPolicy>;

class InternalThreadPool {
 public:
  static bool Initialize(size_t threadCount, AutoLockHelperThreadState& lock);
  static void ShutDown(AutoLockHelperThreadState& lock);

  static bool IsInitialized() { return Instance; }
  static InternalThreadPool& Get();

  bool ensureThreadCount(size_t threadCount, AutoLockHelperThreadState& lock);
  size_t threadCount(const AutoLockHelperThreadState& lock);

  void dispatchTask(const AutoLockHelperThreadState& lock);

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                             const AutoLockHelperThreadState& lock) const;

 private:
  void shutDown(AutoLockHelperThreadState& lock);

  HelperThreadVector& threads(const AutoLockHelperThreadState& lock);
  const HelperThreadVector& threads(
      const AutoLockHelperThreadState& lock) const;

  void notifyOne(const AutoLockHelperThreadState& lock);
  void notifyAll(const AutoLockHelperThreadState& lock);
  void wait(AutoLockHelperThreadState& lock);
  friend class HelperThread;

  static InternalThreadPool* Instance;

  HelperThreadLockData<HelperThreadVector> threads_;

  js::ConditionVariable wakeup;

  HelperThreadLockData<bool> terminating;
};

}  // namespace js

#endif /* vm_InternalThreadPool_h */

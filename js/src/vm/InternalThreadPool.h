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

namespace JS {
enum class DispatchReason;
class HelperThreadTask;
};

namespace js {

class AutoLockHelperThreadState;
class HelperThread;
using JS::HelperThreadTask;

using HelperThreadVector =
    Vector<UniquePtr<HelperThread>, 0, SystemAllocPolicy>;

using HelperTaskVector = Vector<HelperThreadTask*, 0, SystemAllocPolicy>;

class InternalThreadPool {
 public:
  static bool Initialize(size_t threadCount, AutoLockHelperThreadState& lock);
  static void ShutDown(AutoLockHelperThreadState& lock);

  static bool IsInitialized() { return Instance; }
  static InternalThreadPool& Get();

  bool ensureThreadCount(size_t threadCount, AutoLockHelperThreadState& lock);
  size_t threadCount(const AutoLockHelperThreadState& lock);

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                             const AutoLockHelperThreadState& lock) const;

 private:
  static void DispatchTask(HelperThreadTask* task, JS::DispatchReason reason);

  void dispatchTask(HelperThreadTask* task, JS::DispatchReason reason);
  void shutDown(AutoLockHelperThreadState& lock);

  HelperThreadVector& threads(const AutoLockHelperThreadState& lock);
  const HelperThreadVector& threads(
      const AutoLockHelperThreadState& lock) const;

  HelperTaskVector& tasks(const AutoLockHelperThreadState& lock);

  void notifyAll(const AutoLockHelperThreadState& lock);
  void wait(AutoLockHelperThreadState& lock);
  friend class HelperThread;

  static InternalThreadPool* Instance;

  HelperThreadLockData<HelperThreadVector> threads_;

  HelperThreadLockData<HelperTaskVector> tasks_;

  js::ConditionVariable wakeup;

  HelperThreadLockData<bool> terminating;
};

}  // namespace js

#endif /* vm_InternalThreadPool_h */

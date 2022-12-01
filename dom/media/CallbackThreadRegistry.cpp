/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CallbackThreadRegistry.h"
#include "mozilla/ClearOnShutdown.h"

namespace mozilla {
struct CallbackThreadRegistrySingleton {
  CallbackThreadRegistrySingleton()
      : mRegistry(MakeUnique<CallbackThreadRegistry>()) {
    NS_DispatchToMainThread(
        NS_NewRunnableFunction(__func__, [registry = &mRegistry] {
          const auto phase = ShutdownPhase::XPCOMShutdownFinal;
          MOZ_DIAGNOSTIC_ASSERT(!PastShutdownPhase(phase));
          ClearOnShutdown(registry, phase);
        }));
  }

  UniquePtr<CallbackThreadRegistry> mRegistry;
};

CallbackThreadRegistry::CallbackThreadRegistry()
    : mThreadIds("CallbackThreadRegistry::mThreadIds") {}

/* static */
CallbackThreadRegistry* CallbackThreadRegistry::Get() {
  static CallbackThreadRegistrySingleton sSingleton;
  return sSingleton.mRegistry.get();
}

static bool CanLeak() {
#ifdef NS_BUILD_REFCNT_LOGGING
  static const bool logging =
      getenv("XPCOM_MEM_LEAK_LOG") || getenv("XPCOM_MEM_BLOAT_LOG") ||
      getenv("XPCOM_MEM_REFCNT_LOG") || getenv("XPCOM_MEM_ALLOC_LOG") ||
      getenv("XPCOM_MEM_COMPTR_LOG");
  return logging;
#else
  return false;
#endif
}

void CallbackThreadRegistry::Register(ProfilerThreadId aThreadId,
                                      const char* aName) {
  if (!aThreadId.IsSpecified()) {
    // profiler_current_thread_id is unspecified on unsupported platforms.
    return;
  }

  if (CanLeak()) {
    NS_WARNING(
        "Not registering callback thread due to refcount logging; it may show "
        "up as a leak of the TLS-backed nsThread wrapper if the thread "
        "outlives xpcom shutdown.");
    return;
  }

  auto threadIds = mThreadIds.Lock();
  for (uint32_t i = 0; i < threadIds->Length(); i++) {
    if ((*threadIds)[i].mId == aThreadId) {
      (*threadIds)[i].mUserCount++;
      return;
    }
  }
  ThreadUserCount tuc;
  tuc.mId = aThreadId;
  tuc.mUserCount = 1;
  threadIds->AppendElement(tuc);
  PROFILER_REGISTER_THREAD(aName);
}

void CallbackThreadRegistry::Unregister(ProfilerThreadId aThreadId) {
  if (!aThreadId.IsSpecified()) {
    // profiler_current_thread_id is unspedified on unsupported platforms.
    return;
  }

  if (CanLeak()) {
    return;
  }

  auto threadIds = mThreadIds.Lock();
  for (uint32_t i = 0; i < threadIds->Length(); i++) {
    if ((*threadIds)[i].mId == aThreadId) {
      MOZ_ASSERT((*threadIds)[i].mUserCount > 0);
      (*threadIds)[i].mUserCount--;

      if ((*threadIds)[i].mUserCount == 0) {
        PROFILER_UNREGISTER_THREAD();
        threadIds->RemoveElementAt(i);
      }
      return;
    }
  }
  MOZ_ASSERT_UNREACHABLE("Current thread was not registered");
}

}  // namespace mozilla

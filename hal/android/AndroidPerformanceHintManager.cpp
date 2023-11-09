/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalLog.h"
#include "HalTypes.h"

#include "AndroidBuild.h"

#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct APerformanceHintManager APerformanceHintManager;
typedef struct APerformanceHintSession APerformanceHintSession;

namespace mozilla {
namespace hal_impl {

#define LOAD_FN(api, lib, name)                                      \
  do {                                                               \
    api->m##name = reinterpret_cast<Fn##name>(dlsym(handle, #name)); \
    if (!api->m##name) {                                             \
      HAL_ERR("Failed to load %s", #name);                           \
      return nullptr;                                                \
    }                                                                \
  } while (false)

class PerformanceHintManagerApi final {
 public:
  static PerformanceHintManagerApi* Get() {
    // C++ guarantees local static variable initialization is thread safe
    static UniquePtr<PerformanceHintManagerApi> api = Create();
    return api.get();
  }

  APerformanceHintManager* APerformanceHint_getManager() const {
    return mAPerformanceHint_getManager();
  }

  APerformanceHintSession* APerformanceHint_createSession(
      APerformanceHintManager* manager, const int32_t* threadIds, size_t size,
      int64_t initialTargetWorkDurationNanos) const {
    return mAPerformanceHint_createSession(manager, threadIds, size,
                                           initialTargetWorkDurationNanos);
  }

  int APerformanceHint_updateTargetWorkDuration(
      APerformanceHintSession* session, int64_t targetDurationNanos) const {
    return mAPerformanceHint_updateTargetWorkDuration(session,
                                                      targetDurationNanos);
  }

  int APerformanceHint_reportActualWorkDuration(
      APerformanceHintSession* session, int64_t actualDurationNanos) const {
    return mAPerformanceHint_reportActualWorkDuration(session,
                                                      actualDurationNanos);
  }

  void APerformanceHint_closeSession(APerformanceHintSession* session) const {
    mAPerformanceHint_closeSession(session);
  }

 private:
  PerformanceHintManagerApi() = default;

  static UniquePtr<PerformanceHintManagerApi> Create() {
    if (mozilla::jni::GetAPIVersion() < __ANDROID_API_T__) {
      return nullptr;
    }

    void* const handle = dlopen("libandroid.so", RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
      HAL_ERR("Failed to open libandroid.so");
      return nullptr;
    }

    auto api = WrapUnique(new PerformanceHintManagerApi());
    LOAD_FN(api, handle, APerformanceHint_getManager);
    LOAD_FN(api, handle, APerformanceHint_createSession);
    LOAD_FN(api, handle, APerformanceHint_updateTargetWorkDuration);
    LOAD_FN(api, handle, APerformanceHint_reportActualWorkDuration);
    LOAD_FN(api, handle, APerformanceHint_closeSession);

    return api;
  }

  using FnAPerformanceHint_getManager = APerformanceHintManager* (*)();
  using FnAPerformanceHint_createSession =
      APerformanceHintSession* (*)(APerformanceHintManager* manager,
                                   const int32_t* threadIds, size_t size,
                                   int64_t initialTargetWorkDurationNanos);
  using FnAPerformanceHint_updateTargetWorkDuration =
      int (*)(APerformanceHintSession* session, int64_t targetDurationNanos);
  using FnAPerformanceHint_reportActualWorkDuration =
      int (*)(APerformanceHintSession* session, int64_t actualDurationNanos);
  using FnAPerformanceHint_closeSession =
      void (*)(APerformanceHintSession* session);

  FnAPerformanceHint_getManager mAPerformanceHint_getManager = nullptr;
  FnAPerformanceHint_createSession mAPerformanceHint_createSession = nullptr;
  FnAPerformanceHint_updateTargetWorkDuration
      mAPerformanceHint_updateTargetWorkDuration = nullptr;
  FnAPerformanceHint_reportActualWorkDuration
      mAPerformanceHint_reportActualWorkDuration = nullptr;
  FnAPerformanceHint_closeSession mAPerformanceHint_closeSession = nullptr;
};

class AndroidPerformanceHintSession final : public hal::PerformanceHintSession {
 public:
  // Creates a PerformanceHintSession wrapping the provided NDK
  // APerformanceHintSession instance. This assumes ownership of aSession,
  // therefore the caller must not close the session itself.
  explicit AndroidPerformanceHintSession(APerformanceHintSession* aSession)
      : mSession(aSession) {}

  AndroidPerformanceHintSession(AndroidPerformanceHintSession& aOther) = delete;
  AndroidPerformanceHintSession(AndroidPerformanceHintSession&& aOther) {
    mSession = aOther.mSession;
    aOther.mSession = nullptr;
  }

  ~AndroidPerformanceHintSession() {
    if (mSession) {
      PerformanceHintManagerApi::Get()->APerformanceHint_closeSession(mSession);
    }
  }

  void UpdateTargetWorkDuration(TimeDuration aDuration) override {
    PerformanceHintManagerApi::Get()->APerformanceHint_updateTargetWorkDuration(
        mSession, aDuration.ToMicroseconds() * 1000);
  }

  void ReportActualWorkDuration(TimeDuration aDuration) override {
    PerformanceHintManagerApi::Get()->APerformanceHint_reportActualWorkDuration(
        mSession, aDuration.ToMicroseconds() * 1000);
  }

 private:
  APerformanceHintSession* mSession;
};

static APerformanceHintManager* InitManager() {
  const auto* api = PerformanceHintManagerApi::Get();
  if (!api) {
    return nullptr;
  }

  // At the time of writing we are only aware of PerformanceHintManager being
  // implemented on Tensor devices (Pixel 6 and 7 families). On most devices
  // createSession() will simply return null. However, on some devices
  // createSession() does return a session but scheduling does not appear to be
  // affected in any way. Rather than pretending to the caller that
  // PerformanceHintManager is available on such devices, return null allowing
  // them to use another means of achieving the performance they require.
  const auto socManufacturer = java::sdk::Build::SOC_MANUFACTURER()->ToString();
  if (!socManufacturer.EqualsASCII("Google")) {
    return nullptr;
  }

  return api->APerformanceHint_getManager();
}

UniquePtr<hal::PerformanceHintSession> CreatePerformanceHintSession(
    const nsTArray<PlatformThreadHandle>& aThreads,
    mozilla::TimeDuration aTargetWorkDuration) {
  // C++ guarantees local static variable initialization is thread safe
  static APerformanceHintManager* manager = InitManager();

  if (!manager) {
    return nullptr;
  }

  const auto* api = PerformanceHintManagerApi::Get();

  nsTArray<pid_t> tids(aThreads.Length());
  std::transform(aThreads.cbegin(), aThreads.cend(), MakeBackInserter(tids),
                 [](pthread_t handle) { return pthread_gettid_np(handle); });

  APerformanceHintSession* session = api->APerformanceHint_createSession(
      manager, tids.Elements(), tids.Length(),
      aTargetWorkDuration.ToMicroseconds() * 1000);
  if (!session) {
    return nullptr;
  }

  return MakeUnique<AndroidPerformanceHintSession>(session);
}

}  // namespace hal_impl
}  // namespace mozilla

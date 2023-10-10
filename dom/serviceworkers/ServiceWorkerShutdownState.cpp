/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerShutdownState.h"

#include <array>
#include <type_traits>

#include "MainThreadUtils.h"
#include "ServiceWorkerUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/RemoteWorkerService.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

namespace mozilla::dom {

using Progress = ServiceWorkerShutdownState::Progress;

namespace {

constexpr inline auto UnderlyingProgressValue(Progress aProgress) {
  return std::underlying_type_t<Progress>(aProgress);
}

constexpr std::array<const char*, UnderlyingProgressValue(Progress::EndGuard_)>
    gProgressStrings = {{
        // clang-format off
      "parent process main thread",
      "parent process IPDL background thread",
      "content process worker launcher thread",
      "content process main thread",
      "shutdown completed"
        // clang-format on
    }};

}  // anonymous namespace

ServiceWorkerShutdownState::ServiceWorkerShutdownState()
    : mProgress(Progress::ParentProcessMainThread) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

ServiceWorkerShutdownState::~ServiceWorkerShutdownState() {
  Unused << NS_WARN_IF(mProgress != Progress::ShutdownCompleted);
}

const char* ServiceWorkerShutdownState::GetProgressString() const {
  return gProgressStrings[UnderlyingProgressValue(mProgress)];
}

void ServiceWorkerShutdownState::SetProgress(Progress aProgress) {
  MOZ_ASSERT(aProgress != Progress::EndGuard_);
  MOZ_RELEASE_ASSERT(UnderlyingProgressValue(mProgress) + 1 ==
                     UnderlyingProgressValue(aProgress));

  mProgress = aProgress;
}

namespace {

void ReportProgressToServiceWorkerManager(uint32_t aShutdownStateId,
                                          Progress aProgress) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_RELEASE_ASSERT(swm, "ServiceWorkers should shutdown before SWM.");

  swm->ReportServiceWorkerShutdownProgress(aShutdownStateId, aProgress);
}

void ReportProgressToParentProcess(uint32_t aShutdownStateId,
                                   Progress aProgress) {
  MOZ_ASSERT(XRE_IsContentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  ContentChild* contentChild = ContentChild::GetSingleton();
  MOZ_ASSERT(contentChild);

  contentChild->SendReportServiceWorkerShutdownProgress(aShutdownStateId,
                                                        aProgress);
}

void ReportServiceWorkerShutdownProgress(uint32_t aShutdownStateId,
                                         Progress aProgress) {
  MOZ_ASSERT(UnderlyingProgressValue(Progress::ParentProcessMainThread) <
             UnderlyingProgressValue(aProgress));
  MOZ_ASSERT(UnderlyingProgressValue(aProgress) <
             UnderlyingProgressValue(Progress::EndGuard_));

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [shutdownStateId = aShutdownStateId, progress = aProgress] {
        if (XRE_IsParentProcess()) {
          ReportProgressToServiceWorkerManager(shutdownStateId, progress);
        } else {
          ReportProgressToParentProcess(shutdownStateId, progress);
        }
      });

  if (NS_IsMainThread()) {
    MOZ_ALWAYS_SUCCEEDS(r->Run());
  } else {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(r.forget()));
  }
}

void ReportServiceWorkerShutdownProgress(uint32_t aShutdownStateId) {
  Progress progress = Progress::EndGuard_;

  if (XRE_IsParentProcess()) {
    mozilla::ipc::AssertIsOnBackgroundThread();

    progress = Progress::ParentProcessIpdlBackgroundThread;
  } else {
    if (NS_IsMainThread()) {
      progress = Progress::ContentProcessMainThread;
    } else {
      MOZ_ASSERT(RemoteWorkerService::Thread()->IsOnCurrentThread());
      progress = Progress::ContentProcessWorkerLauncherThread;
    }
  }

  ReportServiceWorkerShutdownProgress(aShutdownStateId, progress);
}

}  // anonymous namespace

void MaybeReportServiceWorkerShutdownProgress(const ServiceWorkerOpArgs& aArgs,
                                              bool aShutdownCompleted) {
  if (XRE_IsParentProcess() && !XRE_IsE10sParentProcess()) {
    return;
  }

  if (aShutdownCompleted) {
    MOZ_ASSERT(aArgs.type() ==
               ServiceWorkerOpArgs::TServiceWorkerTerminateWorkerOpArgs);

    ReportServiceWorkerShutdownProgress(
        aArgs.get_ServiceWorkerTerminateWorkerOpArgs().shutdownStateId(),
        Progress::ShutdownCompleted);

    return;
  }

  if (aArgs.type() ==
      ServiceWorkerOpArgs::TServiceWorkerTerminateWorkerOpArgs) {
    ReportServiceWorkerShutdownProgress(
        aArgs.get_ServiceWorkerTerminateWorkerOpArgs().shutdownStateId());
  }
}

}  // namespace mozilla::dom

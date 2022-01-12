/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/ipc/UtilityProcessParent.h"
#include "mozilla/ipc/UtilityProcessManager.h"

#if defined(XP_WIN)
#  include <dwrite.h>
#  include <process.h>
#  include "mozilla/WinDllServices.h"
#endif

#include "mozilla/ipc/ProcessChild.h"

namespace mozilla::ipc {

static std::atomic<UtilityProcessParent*> sUtilityProcessParent;

UtilityProcessParent::UtilityProcessParent(UtilityProcessHost* aHost)
    : mHost(aHost) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mHost);
  sUtilityProcessParent = this;
}

UtilityProcessParent::~UtilityProcessParent() = default;

/* static */
UtilityProcessParent* UtilityProcessParent::GetSingleton() {
  MOZ_DIAGNOSTIC_ASSERT(sUtilityProcessParent);
  return sUtilityProcessParent;
}

bool UtilityProcessParent::SendRequestMemoryReport(
    const uint32_t& aGeneration, const bool& aAnonymize,
    const bool& aMinimizeMemoryUsage, const Maybe<FileDescriptor>& aDMDFile) {
  mMemoryReportRequest = MakeUnique<MemoryReportRequestHost>(aGeneration);

  PUtilityProcessParent::SendRequestMemoryReport(
      aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile,
      [&](const uint32_t& aGeneration2) {
        if (RefPtr<UtilityProcessManager> utilitypm =
                UtilityProcessManager::GetSingleton()) {
          if (UtilityProcessParent* child = utilitypm->GetProcessParent()) {
            if (child->mMemoryReportRequest) {
              child->mMemoryReportRequest->Finish(aGeneration2);
              child->mMemoryReportRequest = nullptr;
            }
          }
        }
      },
      [&](mozilla::ipc::ResponseRejectReason) {
        if (RefPtr<UtilityProcessManager> utilitypm =
                UtilityProcessManager::GetSingleton()) {
          if (UtilityProcessParent* child = utilitypm->GetProcessParent()) {
            child->mMemoryReportRequest = nullptr;
          }
        }
      });

  return true;
}

mozilla::ipc::IPCResult UtilityProcessParent::RecvAddMemoryReport(
    const MemoryReport& aReport) {
  if (mMemoryReportRequest) {
    mMemoryReportRequest->RecvReport(aReport);
  }
  return IPC_OK();
}

void UtilityProcessParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (aWhy == AbnormalShutdown) {
    GenerateCrashReport(OtherPid());
  }

  mHost->OnChannelClosed();
}

// To ensure that IPDL is finished before UtilityParent gets deleted.
class DeferredDeleteUtilityProcessParent : public Runnable {
 public:
  explicit DeferredDeleteUtilityProcessParent(
      RefPtr<UtilityProcessParent> aParent)
      : Runnable("ipc::glue::DeferredDeleteUtilityProcessParent"),
        mParent(std::move(aParent)) {}

  NS_IMETHODIMP Run() override { return NS_OK; }

 private:
  RefPtr<UtilityProcessParent> mParent;
};

/* static */
void UtilityProcessParent::Destroy(RefPtr<UtilityProcessParent> aParent) {
  NS_DispatchToMainThread(
      new DeferredDeleteUtilityProcessParent(std::move(aParent)));
}

}  // namespace mozilla::ipc

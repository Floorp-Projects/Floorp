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
#include "mozilla/FOGIPC.h"

#include "nsHashPropertyBag.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"

namespace mozilla::ipc {

UtilityProcessParent::UtilityProcessParent(UtilityProcessHost* aHost)
    : mHost(aHost) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mHost);
}

UtilityProcessParent::~UtilityProcessParent() = default;

bool UtilityProcessParent::SendRequestMemoryReport(
    const uint32_t& aGeneration, const bool& aAnonymize,
    const bool& aMinimizeMemoryUsage, const Maybe<FileDescriptor>& aDMDFile) {
  mMemoryReportRequest = MakeUnique<MemoryReportRequestHost>(aGeneration);

  PUtilityProcessParent::SendRequestMemoryReport(
      aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile,
      [self = RefPtr{this}](const uint32_t& aGeneration2) {
        if (self->mMemoryReportRequest) {
          self->mMemoryReportRequest->Finish(aGeneration2);
          self->mMemoryReportRequest = nullptr;
        }
      },
      [self = RefPtr{this}](mozilla::ipc::ResponseRejectReason) {
        self->mMemoryReportRequest = nullptr;
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

mozilla::ipc::IPCResult UtilityProcessParent::RecvFOGData(ByteBuf&& aBuf) {
  glean::FOGData(std::move(aBuf));
  return IPC_OK();
}

mozilla::ipc::IPCResult UtilityProcessParent::RecvInitCompleted() {
  MOZ_ASSERT(mHost);
  mHost->ResolvePromise();
  return IPC_OK();
}

void UtilityProcessParent::ActorDestroy(ActorDestroyReason aWhy) {
  RefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();

  if (aWhy == AbnormalShutdown) {
    nsAutoString dumpID;

    if (mCrashReporter) {
#if defined(MOZ_SANDBOX)
      mCrashReporter->AddAnnotation(
          CrashReporter::Annotation::UtilityProcessSandboxingKind,
          (unsigned int)mHost->mSandbox);
#endif
    }

    GenerateCrashReport(OtherPid(), &dumpID);

    // It's okay for dumpID to be empty if there was no minidump generated
    // tests like ipc/glue/test/browser/browser_utility_crashReporter.js are
    // there to verify this
    if (!dumpID.IsEmpty()) {
      props->SetPropertyAsAString(u"dumpID"_ns, dumpID);
    }
  }

  nsAutoString pid;
  pid.AppendInt(static_cast<uint64_t>(OtherPid()));

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers((nsIPropertyBag2*)props, "ipc:utility-shutdown",
                         pid.get());
  } else {
    NS_WARNING("Could not get a nsIObserverService, ipc:utility-shutdown skip");
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

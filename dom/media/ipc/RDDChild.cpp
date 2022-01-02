/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RDDChild.h"

#include "mozilla/RDDProcessManager.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/MemoryReportRequest.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/ipc/CrashReporterHost.h"
#include "mozilla/ipc/Endpoint.h"

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxBroker.h"
#  include "mozilla/SandboxBrokerPolicyFactory.h"
#endif

#include "mozilla/Telemetry.h"

#if defined(XP_WIN)
#  include "mozilla/WinDllServices.h"
#endif

#include "ProfilerParent.h"
#include "RDDProcessHost.h"

namespace mozilla {

using namespace layers;
using namespace gfx;

RDDChild::RDDChild(RDDProcessHost* aHost) : mHost(aHost) {
  MOZ_COUNT_CTOR(RDDChild);
}

RDDChild::~RDDChild() { MOZ_COUNT_DTOR(RDDChild); }

bool RDDChild::Init() {
  Maybe<FileDescriptor> brokerFd;

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  auto policy = SandboxBrokerPolicyFactory::GetRDDPolicy(OtherPid());
  if (policy != nullptr) {
    brokerFd = Some(FileDescriptor());
    mSandboxBroker =
        SandboxBroker::Create(std::move(policy), OtherPid(), brokerFd.ref());
    // This is unlikely to fail and probably indicates OS resource
    // exhaustion, but we can at least try to recover.
    if (NS_WARN_IF(mSandboxBroker == nullptr)) {
      return false;
    }
    MOZ_ASSERT(brokerFd.ref().IsValid());
  }
#endif  // XP_LINUX && MOZ_SANDBOX

  nsTArray<GfxVarUpdate> updates = gfxVars::FetchNonDefaultVars();

  SendInit(updates, brokerFd, Telemetry::CanRecordReleaseData());

  Unused << SendInitProfiler(ProfilerParent::CreateForProcess(OtherPid()));

  gfxVars::AddReceiver(this);
  auto* gpm = gfx::GPUProcessManager::Get();
  if (gpm) {
    gpm->AddListener(this);
  }

  return true;
}

bool RDDChild::SendRequestMemoryReport(const uint32_t& aGeneration,
                                       const bool& aAnonymize,
                                       const bool& aMinimizeMemoryUsage,
                                       const Maybe<FileDescriptor>& aDMDFile) {
  mMemoryReportRequest = MakeUnique<MemoryReportRequestHost>(aGeneration);

  PRDDChild::SendRequestMemoryReport(
      aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile,
      [&](const uint32_t& aGeneration2) {
        if (RDDProcessManager* rddpm = RDDProcessManager::Get()) {
          if (RDDChild* child = rddpm->GetRDDChild()) {
            if (child->mMemoryReportRequest) {
              child->mMemoryReportRequest->Finish(aGeneration2);
              child->mMemoryReportRequest = nullptr;
            }
          }
        }
      },
      [&](mozilla::ipc::ResponseRejectReason) {
        if (RDDProcessManager* rddpm = RDDProcessManager::Get()) {
          if (RDDChild* child = rddpm->GetRDDChild()) {
            child->mMemoryReportRequest = nullptr;
          }
        }
      });

  return true;
}

void RDDChild::OnCompositorUnexpectedShutdown() {
  auto* rddm = RDDProcessManager::Get();
  if (rddm) {
    rddm->CreateVideoBridge();
  }
}

void RDDChild::OnVarChanged(const GfxVarUpdate& aVar) { SendUpdateVar(aVar); }

mozilla::ipc::IPCResult RDDChild::RecvAddMemoryReport(
    const MemoryReport& aReport) {
  if (mMemoryReportRequest) {
    mMemoryReportRequest->RecvReport(aReport);
  }
  return IPC_OK();
}

#if defined(XP_WIN)
mozilla::ipc::IPCResult RDDChild::RecvGetModulesTrust(
    ModulePaths&& aModPaths, bool aRunAtNormalPriority,
    GetModulesTrustResolver&& aResolver) {
  RefPtr<DllServices> dllSvc(DllServices::Get());
  dllSvc->GetModulesTrust(std::move(aModPaths), aRunAtNormalPriority)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [aResolver](ModulesMapResult&& aResult) {
            aResolver(Some(ModulesMapResult(std::move(aResult))));
          },
          [aResolver](nsresult aRv) { aResolver(Nothing()); });
  return IPC_OK();
}
#endif  // defined(XP_WIN)

mozilla::ipc::IPCResult RDDChild::RecvUpdateMediaCodecsSupported(
    const PDMFactory::MediaCodecsSupported& aSupported) {
  dom::ContentParent::BroadcastMediaCodecsSupportedUpdate(
      RemoteDecodeIn::RddProcess, aSupported);
  return IPC_OK();
}

void RDDChild::ActorDestroy(ActorDestroyReason aWhy) {
  if (aWhy == AbnormalShutdown) {
    GenerateCrashReport(OtherPid());
  }

  auto* gpm = gfx::GPUProcessManager::Get();
  if (gpm) {
    // Note: the manager could have shutdown already.
    gpm->RemoveListener(this);
  }

  gfxVars::RemoveReceiver(this);
  mHost->OnChannelClosed();
}

class DeferredDeleteRDDChild : public Runnable {
 public:
  explicit DeferredDeleteRDDChild(UniquePtr<RDDChild>&& aChild)
      : Runnable("gfx::DeferredDeleteRDDChild"), mChild(std::move(aChild)) {}

  NS_IMETHODIMP Run() override { return NS_OK; }

 private:
  UniquePtr<RDDChild> mChild;
};

/* static */
void RDDChild::Destroy(UniquePtr<RDDChild>&& aChild) {
  NS_DispatchToMainThread(new DeferredDeleteRDDChild(std::move(aChild)));
}

}  // namespace mozilla

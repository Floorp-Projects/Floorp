/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "UtilityProcessChild.h"

#include "mozilla/ipc/UtilityProcessManager.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/MemoryReportRequest.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/Preferences.h"

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/Sandbox.h"
#endif

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
#  include "mozilla/SandboxTestingChild.h"
#endif

#include "mozilla/Telemetry.h"

#if defined(XP_WIN)
#  include "mozilla/WinDllServices.h"
#endif

#include "nsDebugImpl.h"
#include "nsIXULRuntime.h"
#include "nsThreadManager.h"
#include "GeckoProfiler.h"

#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/FOGIPC.h"
#include "mozilla/glean/GleanMetrics.h"

namespace mozilla::ipc {

using namespace layers;

static StaticMutex sUtilityProcessChildMutex;
static StaticRefPtr<UtilityProcessChild> sUtilityProcessChild;

UtilityProcessChild::UtilityProcessChild() {
  nsDebugImpl::SetMultiprocessMode("Utility");
  StaticMutexAutoLock lock(sUtilityProcessChildMutex);
  sUtilityProcessChild = this;
}

UtilityProcessChild::~UtilityProcessChild() = default;

/* static */
RefPtr<UtilityProcessChild> UtilityProcessChild::GetSingleton() {
  StaticMutexAutoLock lock(sUtilityProcessChildMutex);
  if (!sUtilityProcessChild) {
    sUtilityProcessChild = new UtilityProcessChild();
  }
  return sUtilityProcessChild;
}

/* static */
RefPtr<UtilityProcessChild> UtilityProcessChild::Get() {
  StaticMutexAutoLock lock(sUtilityProcessChildMutex);
  return sUtilityProcessChild;
}

bool UtilityProcessChild::Init(mozilla::ipc::UntypedEndpoint&& aEndpoint,
                               const nsCString& aParentBuildID,
                               uint64_t aSandboxingKind) {
  MOZ_ASSERT(NS_IsMainThread());

  // Initialize the thread manager before starting IPC. Otherwise, messages
  // may be posted to the main thread and we won't be able to process them.
  if (NS_WARN_IF(NS_FAILED(nsThreadManager::get().Init()))) {
    return false;
  }

  // Now it's safe to start IPC.
  if (NS_WARN_IF(!aEndpoint.Bind(this))) {
    return false;
  }

  // This must be checked before any IPDL message, which may hit sentinel
  // errors due to parent and content processes having different
  // versions.
  MessageChannel* channel = GetIPCChannel();
  if (channel && !channel->SendBuildIDsMatchMessage(aParentBuildID.get())) {
    // We need to quit this process if the buildID doesn't match the parent's.
    // This can occur when an update occurred in the background.
    ipc::ProcessChild::QuickExit();
  }

  // Init crash reporter support.
  ipc::CrashReporterClient::InitSingleton(this);

  if (NS_FAILED(NS_InitMinimalXPCOM())) {
    return false;
  }

  mSandbox = (SandboxingKind)aSandboxingKind;

  profiler_set_process_name(nsCString("Utility Process"));

  // Notify the parent process that we have finished our init and that it can
  // now resolve the pending promise of process startup
  SendInitCompleted();

  return true;
}

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
extern "C" {
void CGSShutdownServerConnections();
};
#endif

mozilla::ipc::IPCResult UtilityProcessChild::RecvInit(
    const Maybe<FileDescriptor>& aBrokerFd,
    const bool& aCanRecordReleaseTelemetry) {
  // Do this now (before closing WindowServer on macOS) to avoid risking
  // blocking in GetCurrentProcess() called on that platform
  mozilla::ipc::SetThisProcessName("Utility Process");

#if defined(MOZ_SANDBOX)
#  if defined(XP_MACOSX)
  // Close all current connections to the WindowServer. This ensures that the
  // Activity Monitor will not label the content process as "Not responding"
  // because it's not running a native event loop. See bug 1384336.
  CGSShutdownServerConnections();

#  elif defined(XP_LINUX)
  int fd = -1;
  if (aBrokerFd.isSome()) {
    fd = aBrokerFd.value().ClonePlatformHandle().release();
  }

  SetUtilitySandbox(fd, mSandbox);

#  endif  // XP_MACOSX/XP_LINUX
#endif    // MOZ_SANDBOX

#if defined(XP_WIN)
  if (aCanRecordReleaseTelemetry) {
    RefPtr<DllServices> dllSvc(DllServices::Get());
    dllSvc->StartUntrustedModulesProcessor(false);
  }
#endif  // defined(XP_WIN)
  return IPC_OK();
}

mozilla::ipc::IPCResult UtilityProcessChild::RecvPreferenceUpdate(
    const Pref& aPref) {
  Preferences::SetPreference(aPref);
  return IPC_OK();
}

mozilla::ipc::IPCResult UtilityProcessChild::RecvInitProfiler(
    Endpoint<PProfilerChild>&& aEndpoint) {
  mProfilerController = ChildProfilerController::Create(std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult UtilityProcessChild::RecvRequestMemoryReport(
    const uint32_t& aGeneration, const bool& aAnonymize,
    const bool& aMinimizeMemoryUsage, const Maybe<FileDescriptor>& aDMDFile,
    const RequestMemoryReportResolver& aResolver) {
  nsPrintfCString processName("Utility (pid: %" PRIPID
                              ", sandboxingKind: %" PRIu64 ")",
                              base::GetCurrentProcId(), mSandbox);

  mozilla::dom::MemoryReportRequestClient::Start(
      aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile, processName,
      [&](const MemoryReport& aReport) {
        Unused << GetSingleton()->SendAddMemoryReport(aReport);
      },
      aResolver);
  return IPC_OK();
}

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
mozilla::ipc::IPCResult UtilityProcessChild::RecvInitSandboxTesting(
    Endpoint<PSandboxTestingChild>&& aEndpoint) {
  if (!SandboxTestingChild::Initialize(std::move(aEndpoint))) {
    return IPC_FAIL(
        this, "InitSandboxTesting failed to initialise the child process.");
  }
  return IPC_OK();
}
#endif

mozilla::ipc::IPCResult UtilityProcessChild::RecvFlushFOGData(
    FlushFOGDataResolver&& aResolver) {
  glean::FlushFOGData(std::move(aResolver));
  return IPC_OK();
}

mozilla::ipc::IPCResult UtilityProcessChild::RecvTestTriggerMetrics(
    TestTriggerMetricsResolver&& aResolve) {
  mozilla::glean::test_only_ipc::a_counter.Add(
      nsIXULRuntime::PROCESS_TYPE_UTILITY);
  aResolve(true);
  return IPC_OK();
}

mozilla::ipc::IPCResult
UtilityProcessChild::RecvStartUtilityAudioDecoderService(
    Endpoint<PUtilityAudioDecoderParent>&& aEndpoint) {
  mUtilityAudioDecoderInstance = new UtilityAudioDecoderParent();
  if (!mUtilityAudioDecoderInstance) {
    return IPC_FAIL(this, "Failing to create UtilityAudioDecoderParent");
  }

  mUtilityAudioDecoderInstance->Start(std::move(aEndpoint));
  return IPC_OK();
}

void UtilityProcessChild::ActorDestroy(ActorDestroyReason aWhy) {
  if (AbnormalShutdown == aWhy) {
    NS_WARNING("Shutting down Utility process early due to a crash!");
    ipc::ProcessChild::QuickExit();
  }

  // Send the last bits of Glean data over to the main process.
  glean::FlushFOGData(
      [](ByteBuf&& aBuf) { glean::SendFOGData(std::move(aBuf)); });

#ifndef NS_FREE_PERMANENT_DATA
  ProcessChild::QuickExit();
#else

  if (mProfilerController) {
    mProfilerController->Shutdown();
    mProfilerController = nullptr;
  }

  // Wait until all RemoteDecoderManagerParent have closed.
  //
  // FIXME: Should move from using AsyncBlockers to proper
  // nsIAsyncShutdownService once it is not JS, see bug 1760855
  mShutdownBlockers.WaitUntilClear(10 * 1000 /* 10s timeout*/)
      ->Then(GetCurrentSerialEventTarget(), __func__, [&]() {
#  ifdef XP_WIN
        {
          RefPtr<DllServices> dllSvc(DllServices::Get());
          dllSvc->DisableFull();
        }
#  endif  // defined(XP_WIN)

        {
          StaticMutexAutoLock lock(sUtilityProcessChildMutex);
          if (sUtilityProcessChild) {
            sUtilityProcessChild = nullptr;
          }
        }

        ipc::CrashReporterClient::DestroySingleton();
        XRE_ShutdownChildProcess();
      });
#endif    // NS_FREE_PERMANENT_DATA
}

}  // namespace mozilla::ipc

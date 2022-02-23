/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_ipc_glue_UtilityProcessChild_h_
#define _include_ipc_glue_UtilityProcessChild_h_
#include "mozilla/ipc/PUtilityProcessChild.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"
#include "mozilla/UniquePtr.h"
#include "ChildProfilerController.h"

namespace mozilla::ipc {

class UtilityProcessHost;

class UtilityProcessChild final : public PUtilityProcessChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UtilityProcessChild, override);

  UtilityProcessChild();

  static RefPtr<UtilityProcessChild> GetSingleton();
  static RefPtr<UtilityProcessChild> Get();

  SandboxingKind mSandbox{};

  bool Init(base::ProcessId aParentPid, const nsCString& aParentBuildID,
            uint64_t aSandboxingKind, mozilla::ipc::ScopedPort aPort);

  mozilla::ipc::IPCResult RecvInit(const Maybe<ipc::FileDescriptor>& aBrokerFd,
                                   const bool& aCanRecordReleaseTelemetry);
  mozilla::ipc::IPCResult RecvInitProfiler(
      Endpoint<PProfilerChild>&& aEndpoint);

  mozilla::ipc::IPCResult RecvPreferenceUpdate(const Pref& pref);

  mozilla::ipc::IPCResult RecvRequestMemoryReport(
      const uint32_t& generation, const bool& anonymize,
      const bool& minimizeMemoryUsage,
      const Maybe<ipc::FileDescriptor>& DMDFile,
      const RequestMemoryReportResolver& aResolver);

  mozilla::ipc::IPCResult RecvFlushFOGData(FlushFOGDataResolver&& aResolver);

  mozilla::ipc::IPCResult RecvTestTriggerMetrics(
      TestTriggerMetricsResolver&& aResolve);

  void ActorDestroy(ActorDestroyReason aWhy) override;

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
  mozilla::ipc::IPCResult RecvInitSandboxTesting(
      Endpoint<PSandboxTestingChild>&& aEndpoint);
#endif

 protected:
  friend class UtilityProcessImpl;
  ~UtilityProcessChild();

 private:
  RefPtr<ChildProfilerController> mProfilerController;
};

}  // namespace mozilla::ipc

#endif  // _include_ipc_glue_UtilityProcessChild_h_

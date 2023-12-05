/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_dom_media_ipc_RDDParent_h__
#define _include_dom_media_ipc_RDDParent_h__
#include "mozilla/PRDDParent.h"

#include "mozilla/RefPtr.h"
#include "mozilla/ipc/AsyncBlockers.h"

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
#  include "mozilla/PSandboxTestingChild.h"
#endif

namespace mozilla {

class TimeStamp;
class ChildProfilerController;

class RDDParent final : public PRDDParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(RDDParent, final)

  RDDParent();

  static RDDParent* GetSingleton();

  ipc::AsyncBlockers& AsyncShutdownService() { return mShutdownBlockers; }

  bool Init(mozilla::ipc::UntypedEndpoint&& aEndpoint,
            const char* aParentBuildID);

  mozilla::ipc::IPCResult RecvInit(nsTArray<GfxVarUpdate>&& vars,
                                   const Maybe<ipc::FileDescriptor>& aBrokerFd,
                                   const bool& aCanRecordReleaseTelemetry,
                                   const bool& aIsReadyForBackgroundProcessing);
  mozilla::ipc::IPCResult RecvInitProfiler(
      Endpoint<PProfilerChild>&& aEndpoint);

  mozilla::ipc::IPCResult RecvNewContentRemoteDecoderManager(
      Endpoint<PRemoteDecoderManagerParent>&& aEndpoint,
      const ContentParentId& aParentId);
  mozilla::ipc::IPCResult RecvInitVideoBridge(
      Endpoint<PVideoBridgeChild>&& aEndpoint,
      const bool& aCreateHardwareDevice,
      const ContentDeviceData& aContentDeviceData);
  mozilla::ipc::IPCResult RecvRequestMemoryReport(
      const uint32_t& generation, const bool& anonymize,
      const bool& minimizeMemoryUsage,
      const Maybe<ipc::FileDescriptor>& DMDFile,
      const RequestMemoryReportResolver& aResolver);
#if defined(XP_WIN)
  mozilla::ipc::IPCResult RecvGetUntrustedModulesData(
      GetUntrustedModulesDataResolver&& aResolver);
  mozilla::ipc::IPCResult RecvUnblockUntrustedModulesThread();
#endif  // defined(XP_WIN)
  mozilla::ipc::IPCResult RecvPreferenceUpdate(const Pref& pref);
  mozilla::ipc::IPCResult RecvUpdateVar(const GfxVarUpdate& pref);

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
  mozilla::ipc::IPCResult RecvInitSandboxTesting(
      Endpoint<PSandboxTestingChild>&& aEndpoint);
#endif
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvFlushFOGData(FlushFOGDataResolver&& aResolver);

  mozilla::ipc::IPCResult RecvTestTriggerMetrics(
      TestTriggerMetricsResolver&& aResolve);

  mozilla::ipc::IPCResult RecvTestTelemetryProbes();

 private:
  ~RDDParent();

  const TimeStamp mLaunchTime;
  RefPtr<ChildProfilerController> mProfilerController;
  ipc::AsyncBlockers mShutdownBlockers;
};

}  // namespace mozilla

#endif  // _include_dom_media_ipc_RDDParent_h__

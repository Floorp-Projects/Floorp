/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_dom_media_ipc_RDDChild_h_
#define _include_dom_media_ipc_RDDChild_h_
#include "mozilla/PRDDChild.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/GPUProcessListener.h"
#include "mozilla/gfx/gfxVarReceiver.h"
#include "mozilla/ipc/CrashReporterHelper.h"

namespace mozilla {

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
class SandboxBroker;
#endif

namespace dom {
class MemoryReportRequestHost;
}  // namespace dom

class RDDProcessHost;

class RDDChild final : public PRDDChild,
                       public ipc::CrashReporterHelper<GeckoProcessType_RDD>,
                       public gfx::gfxVarReceiver,
                       public gfx::GPUProcessListener {
  typedef mozilla::dom::MemoryReportRequestHost MemoryReportRequestHost;

 public:
  explicit RDDChild(RDDProcessHost* aHost);
  ~RDDChild();

  bool Init();

  void OnCompositorUnexpectedShutdown() override;
  void OnVarChanged(const GfxVarUpdate& aVar) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvAddMemoryReport(const MemoryReport& aReport);
#if defined(XP_WIN)
  mozilla::ipc::IPCResult RecvGetModulesTrust(
      ModulePaths&& aModPaths, bool aRunAtNormalPriority,
      GetModulesTrustResolver&& aResolver);
#endif  // defined(XP_WIN)
  mozilla::ipc::IPCResult RecvUpdateMediaCodecsSupported(
      const media::MediaCodecsSupported& aSupported);
  mozilla::ipc::IPCResult RecvFOGData(ByteBuf&& aBuf);

  bool SendRequestMemoryReport(const uint32_t& aGeneration,
                               const bool& aAnonymize,
                               const bool& aMinimizeMemoryUsage,
                               const Maybe<ipc::FileDescriptor>& aDMDFile);

  static void Destroy(UniquePtr<RDDChild>&& aChild);

 private:
  RDDProcessHost* mHost;
  UniquePtr<MemoryReportRequestHost> mMemoryReportRequest;
#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  UniquePtr<SandboxBroker> mSandboxBroker;
#endif
};

}  // namespace mozilla

#endif  // _include_dom_media_ipc_RDDChild_h_

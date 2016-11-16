/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_mozilla_gfx_ipc_GPUChild_h_
#define _include_mozilla_gfx_ipc_GPUChild_h_

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/PGPUChild.h"
#include "mozilla/gfx/gfxVarReceiver.h"

namespace mozilla {
namespace ipc {
class CrashReporterHost;
} // namespace
namespace gfx {

class GPUProcessHost;

class GPUChild final
  : public PGPUChild,
    public gfxVarReceiver
{
public:
  explicit GPUChild(GPUProcessHost* aHost);
  ~GPUChild();

  void Init();

  void EnsureGPUReady();

  // gfxVarReceiver overrides.
  void OnVarChanged(const GfxVarUpdate& aVar) override;

  // PGPUChild overrides.
  mozilla::ipc::IPCResult RecvInitComplete(const GPUDeviceData& aData) override;
  mozilla::ipc::IPCResult RecvReportCheckerboard(const uint32_t& aSeverity, const nsCString& aLog) override;
  mozilla::ipc::IPCResult RecvInitCrashReporter(Shmem&& shmem) override;
  mozilla::ipc::IPCResult RecvAccumulateChildHistogram(InfallibleTArray<Accumulation>&& aAccumulations) override;
  mozilla::ipc::IPCResult RecvAccumulateChildKeyedHistogram(InfallibleTArray<KeyedAccumulation>&& aAccumulations) override;
  void ActorDestroy(ActorDestroyReason aWhy) override;
  mozilla::ipc::IPCResult RecvGraphicsError(const nsCString& aError) override;
  mozilla::ipc::IPCResult RecvNotifyUiObservers(const nsCString& aTopic) override;
  mozilla::ipc::IPCResult RecvNotifyDeviceReset() override;

  static void Destroy(UniquePtr<GPUChild>&& aChild);

private:
  GPUProcessHost* mHost;
  UniquePtr<ipc::CrashReporterHost> mCrashReporter;
  bool mGPUReady;
};

} // namespace gfx
} // namespace mozilla

#endif // _include_mozilla_gfx_ipc_GPUChild_h_

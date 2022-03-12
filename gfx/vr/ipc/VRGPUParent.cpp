/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRGPUParent.h"
#include "VRPuppetCommandBuffer.h"

#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/StaticPrefs_dom.h"

namespace mozilla {
namespace gfx {

using namespace ipc;

VRGPUParent::VRGPUParent(ProcessId aChildProcessId) : mClosed(false) {
  MOZ_COUNT_CTOR(VRGPUParent);
  MOZ_ASSERT(NS_IsMainThread());

  SetOtherProcessId(aChildProcessId);
}

VRGPUParent::~VRGPUParent() { MOZ_COUNT_DTOR(VRGPUParent); }

void VRGPUParent::ActorDestroy(ActorDestroyReason aWhy) {
#if !defined(MOZ_WIDGET_ANDROID)
  if (mVRService) {
    mVRService->Stop();
    mVRService = nullptr;
  }
#endif

  mClosed = true;
  GetCurrentSerialEventTarget()->Dispatch(
      NewRunnableMethod("gfx::VRGPUParent::DeferredDestroy", this,
                        &VRGPUParent::DeferredDestroy));
}

void VRGPUParent::DeferredDestroy() { mSelfRef = nullptr; }

/* static */
RefPtr<VRGPUParent> VRGPUParent::CreateForGPU(
    Endpoint<PVRGPUParent>&& aEndpoint) {
  if (!StaticPrefs::dom_vr_enabled() && !StaticPrefs::dom_vr_webxr_enabled()) {
    return nullptr;
  }

  RefPtr<VRGPUParent> vcp = new VRGPUParent(aEndpoint.OtherPid());
  GetCurrentSerialEventTarget()->Dispatch(
      NewRunnableMethod<Endpoint<PVRGPUParent>&&>("gfx::VRGPUParent::Bind", vcp,
                                                  &VRGPUParent::Bind,
                                                  std::move(aEndpoint)));

  return vcp;
}

void VRGPUParent::Bind(Endpoint<PVRGPUParent>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    return;
  }

  mSelfRef = this;
}

mozilla::ipc::IPCResult VRGPUParent::RecvStartVRService() {
#if !defined(MOZ_WIDGET_ANDROID)
  mVRService = VRService::Create();
  MOZ_ASSERT(mVRService);

  mVRService->Start();
#endif

  return IPC_OK();
}

mozilla::ipc::IPCResult VRGPUParent::RecvStopVRService() {
#if !defined(MOZ_WIDGET_ANDROID)
  if (mVRService) {
    mVRService->Stop();
    mVRService = nullptr;
  }
#endif

  return IPC_OK();
}

mozilla::ipc::IPCResult VRGPUParent::RecvPuppetReset() {
#if !defined(MOZ_WIDGET_ANDROID)
  VRPuppetCommandBuffer::Get().Reset();
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult VRGPUParent::RecvPuppetSubmit(
    const nsTArray<uint64_t>& aBuffer) {
#if !defined(MOZ_WIDGET_ANDROID)
  VRPuppetCommandBuffer::Get().Submit(aBuffer);
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult VRGPUParent::RecvPuppetCheckForCompletion() {
#if !defined(MOZ_WIDGET_ANDROID)
  if (VRPuppetCommandBuffer::Get().HasEnded()) {
    Unused << SendNotifyPuppetComplete();
  }
#endif
  return IPC_OK();
}

bool VRGPUParent::IsClosed() { return mClosed; }

}  // namespace gfx
}  // namespace mozilla

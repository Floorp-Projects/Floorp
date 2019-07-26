/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRServiceHost.h"
#include "VRGPUChild.h"
#include "VRManager.h"
#include "VRPuppetCommandBuffer.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/gfx/GPUParent.h"
#include "service/VRService.h"

namespace mozilla {
namespace gfx {

static StaticRefPtr<VRServiceHost> sVRServiceHostSingleton;

/* static */
void VRServiceHost::Init(bool aEnableVRProcess) {
  MOZ_ASSERT(NS_IsMainThread());

  if (sVRServiceHostSingleton == nullptr) {
    sVRServiceHostSingleton = new VRServiceHost(aEnableVRProcess);
    ClearOnShutdown(&sVRServiceHostSingleton);
  }
}

/* static */
VRServiceHost* VRServiceHost::Get() {
  MOZ_ASSERT(sVRServiceHostSingleton != nullptr);
  return sVRServiceHostSingleton;
}

VRServiceHost::VRServiceHost(bool aEnableVRProcess)
    : mPuppetActive(false)
#if !defined(MOZ_WIDGET_ANDROID)
      ,
      mVRService(nullptr),
      mVRProcessEnabled(aEnableVRProcess),
      mVRProcessStarted(false),
      mVRServiceRequested(false)

#endif
{
  MOZ_COUNT_CTOR(VRServiceHost);
}

VRServiceHost::~VRServiceHost() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_DTOR(VRServiceHost);
}

void VRServiceHost::StartService() {
  mVRServiceRequested = true;
  if (mVRProcessEnabled) {
    // VRService in the VR process
    RefreshVRProcess();
  } else if (mVRService) {
    // VRService in the GPU process if enabled, or
    // the parent process if the GPU process is not enabled.
    mVRService->Start();
  }
}

void VRServiceHost::StopService() {
  mVRServiceRequested = false;
  if (mVRProcessEnabled) {
    // VRService in the VR process
    RefreshVRProcess();
  } else if (mVRService) {
    // VRService in the GPU process if enabled, or
    // the parent process if the GPU process is not enabled.
    mVRService->Stop();
  }
}

void VRServiceHost::Shutdown() {
  PuppetReset();
  StopService();
  mVRService = nullptr;
}

void VRServiceHost::Refresh() {
  if (mVRService) {
    mVRService->Refresh();
  }
}

#if !defined(MOZ_WIDGET_ANDROID)

void VRServiceHost::CreateService(volatile VRExternalShmem* aShmem) {
  MOZ_ASSERT(!mVRProcessEnabled);
  mVRService = VRService::Create(aShmem);
}

bool VRServiceHost::NeedVRProcess() {
  if (!mVRProcessEnabled) {
    return false;
  }
  if (mVRServiceRequested) {
    return true;
  }
  if (mPuppetActive) {
    return true;
  }
  return false;
}

void VRServiceHost::RefreshVRProcess() {
  // Start or stop the VR process if needed
  if (NeedVRProcess()) {
    if (!mVRProcessStarted) {
      CreateVRProcess();
    }
  } else {
    if (mVRProcessStarted) {
      ShutdownVRProcess();
    }
  }
}

void VRServiceHost::CreateVRProcess() {
  // This is only allowed to run in the main thread of the GPU process
  if (!XRE_IsGPUProcess()) {
    return;
  }
  // Forward this to the main thread if not already there
  if (!NS_IsMainThread()) {
    RefPtr<Runnable> task = NS_NewRunnableFunction(
        "VRServiceHost::CreateVRProcess",
        []() -> void { VRServiceHost::Get()->CreateVRProcess(); });
    NS_DispatchToMainThread(task.forget());
    return;
  }
  if (mVRProcessStarted) {
    return;
  }

  mVRProcessStarted = true;
  // Using PGPU channel to tell the main process
  // to create the VR process.
  gfx::GPUParent* gpu = GPUParent::GetSingleton();
  MOZ_ASSERT(gpu);
  Unused << gpu->SendCreateVRProcess();
}

void VRServiceHost::ShutdownVRProcess() {
  // This is only allowed to run in the main thread of the GPU process
  if (!XRE_IsGPUProcess()) {
    return;
  }
  // Forward this to the main thread if not already there
  if (!NS_IsMainThread()) {
    RefPtr<Runnable> task = NS_NewRunnableFunction(
        "VRServiceHost::ShutdownVRProcess",
        []() -> void { VRServiceHost::Get()->ShutdownVRProcess(); });
    NS_DispatchToMainThread(task.forget());
    return;
  }
  if (VRGPUChild::IsCreated()) {
    VRGPUChild* vrGPUChild = VRGPUChild::Get();
    vrGPUChild->SendStopVRService();
    if (!vrGPUChild->IsClosed()) {
      vrGPUChild->Close();
    }
    VRGPUChild::Shutdown();
  }
  if (!mVRProcessStarted) {
    return;
  }
  // Using PGPU channel to tell the main process
  // to shutdown VR process.
  gfx::GPUParent* gpu = GPUParent::GetSingleton();
  MOZ_ASSERT(gpu);
  Unused << gpu->SendShutdownVRProcess();
  mVRProcessStarted = false;
}

#endif  // !defined(MOZ_WIDGET_ANDROID)

void VRServiceHost::PuppetSubmit(const nsTArray<uint64_t>& aBuffer) {
  if (mVRProcessEnabled) {
    mPuppetActive = true;
    // TODO - Implement VR puppet support for VR process (Bug 1555188)
    MOZ_ASSERT(false);  // Not implemented
  } else {
    VRPuppetCommandBuffer::Get().Submit(aBuffer);
  }
}

void VRServiceHost::PuppetReset() {
  if (mVRProcessEnabled) {
    mPuppetActive = false;
    if (!mVRProcessStarted) {
      // Process is stopped, so puppet state is already clear
      return;
    }
    // TODO - Implement VR puppet support for VR process (Bug 1555188)
    MOZ_ASSERT(false);  // Not implemented
  } else {
    VRPuppetCommandBuffer::Get().Reset();
  }
}

bool VRServiceHost::PuppetHasEnded() {
  if (mVRProcessEnabled) {
    if (!mVRProcessStarted) {
      // The VR process will be kept alive as long
      // as there is a queue in the puppet command
      // buffer.  If the process is stopped, we can
      // infer that the queue has been cleared and
      // puppet state is reset.
      return true;
    }
    // TODO - Implement VR puppet support for VR process (Bug 1555188)
    MOZ_ASSERT(false);  // Not implemented
    return false;
  }
  return VRPuppetCommandBuffer::Get().HasEnded();
}

}  // namespace gfx
}  // namespace mozilla

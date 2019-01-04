/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRServiceManager.h"
#include "VRGPUChild.h"
#include "mozilla/gfx/GPUParent.h"

namespace mozilla {
namespace gfx {

VRServiceManager::VRServiceManager()
#if !defined(MOZ_WIDGET_ANDROID)
    : mVRService(nullptr)
#endif
{
}

VRServiceManager& VRServiceManager::Get() {
  static VRServiceManager instance;
  return instance;
}

void VRServiceManager::CreateVRProcess() {
  // Using PGPU channel to tell the main process
  // to create VR process.
  RefPtr<Runnable> task =
      NS_NewRunnableFunction("GPUParent::SendCreateVRProcess", []() -> void {
        gfx::GPUParent* gpu = GPUParent::GetSingleton();
        MOZ_ASSERT(gpu);
        Unused << gpu->SendCreateVRProcess();
      });

  NS_DispatchToMainThread(task.forget());
}

void VRServiceManager::ShutdownVRProcess() {
  if (VRGPUChild::IsCreated()) {
    VRGPUChild* vrGPUChild = VRGPUChild::Get();
    vrGPUChild->SendStopVRService();
    vrGPUChild->Close();
    VRGPUChild::Shutdown();
  }
  if (gfxPrefs::VRProcessEnabled()) {
    // Using PGPU channel to tell the main process
    // to shutdown VR process.
    gfx::GPUParent* gpu = GPUParent::GetSingleton();
    MOZ_ASSERT(gpu);
    Unused << gpu->SendShutdownVRProcess();
  }
}

void VRServiceManager::CreateService() {
  if (!gfxPrefs::VRProcessEnabled()) {
    mVRService = VRService::Create();
  }
}

void VRServiceManager::Start() {
  if (mVRService) {
    mVRService->Start();
  }
}

void VRServiceManager::Stop() {
  if (mVRService) {
    mVRService->Stop();
  }
}

void VRServiceManager::Shutdown() {
  Stop();
  mVRService = nullptr;
}

void VRServiceManager::Refresh() {
  if (mVRService) {
    mVRService->Refresh();
  }
}

bool VRServiceManager::IsServiceValid() { return (mVRService != nullptr); }

VRExternalShmem* VRServiceManager::GetAPIShmem() {
#if !defined(MOZ_WIDGET_ANDROID)
  return mVRService->GetAPIShmem();
#endif
  return nullptr;
}

}  // namespace gfx
}  // namespace mozilla
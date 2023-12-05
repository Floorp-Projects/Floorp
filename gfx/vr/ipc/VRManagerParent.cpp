/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRManagerParent.h"

#include "ipc/VRLayerParent.h"
#include "mozilla/gfx/PVRManagerParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/ProtocolTypes.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/ipc/ProtocolUtils.h"  // for IToplevelProtocol
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "mozilla/Unused.h"
#include "VRManager.h"
#include "VRThread.h"

using mozilla::dom::GamepadHandle;

namespace mozilla {
using namespace layers;
namespace gfx {

// See VRManagerChild.cpp
void ReleaseVRManagerParentSingleton();

VRManagerParent::VRManagerParent(ProcessId aChildProcessId,
                                 dom::ContentParentId aChildId,
                                 bool aIsContentChild)
    : mChildId(aChildId),
      mHaveEventListener(false),
      mHaveControllerListener(false),
      mIsContentChild(aIsContentChild),
      mVRActiveStatus(false) {
  MOZ_COUNT_CTOR(VRManagerParent);
  MOZ_ASSERT(NS_IsMainThread());

  SetOtherProcessId(aChildProcessId);
}

VRManagerParent::~VRManagerParent() {
  MOZ_ASSERT(!mVRManagerHolder);

  MOZ_COUNT_DTOR(VRManagerParent);
}

PVRLayerParent* VRManagerParent::AllocPVRLayerParent(const uint32_t& aDisplayID,
                                                     const uint32_t& aGroup) {
  if (!StaticPrefs::dom_vr_enabled() && !StaticPrefs::dom_vr_webxr_enabled()) {
    return nullptr;
  }

  RefPtr<VRLayerParent> layer;
  layer = new VRLayerParent(aDisplayID, aGroup);
  VRManager* vm = VRManager::Get();
  vm->AddLayer(layer);
  return layer.forget().take();
}

bool VRManagerParent::DeallocPVRLayerParent(PVRLayerParent* actor) {
  delete actor;
  return true;
}

bool VRManagerParent::IsSameProcess() const {
  return OtherPid() == base::GetCurrentProcId();
}

void VRManagerParent::RegisterWithManager() {
  VRManager* vm = VRManager::Get();
  vm->AddVRManagerParent(this);
  mVRManagerHolder = vm;
}

void VRManagerParent::UnregisterFromManager() {
  VRManager* vm = VRManager::Get();
  vm->RemoveVRManagerParent(this);
  mVRManagerHolder = nullptr;
}

/* static */
bool VRManagerParent::CreateForContent(Endpoint<PVRManagerParent>&& aEndpoint,
                                       dom::ContentParentId aChildId) {
  if (!CompositorThread()) {
    return false;
  }

  RefPtr<VRManagerParent> vmp =
      new VRManagerParent(aEndpoint.OtherPid(), aChildId, true);
  CompositorThread()->Dispatch(NewRunnableMethod<Endpoint<PVRManagerParent>&&>(
      "gfx::VRManagerParent::Bind", vmp, &VRManagerParent::Bind,
      std::move(aEndpoint)));

  return true;
}

void VRManagerParent::Bind(Endpoint<PVRManagerParent>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    return;
  }
  mCompositorThreadHolder = CompositorThreadHolder::GetSingleton();

  RegisterWithManager();
}

/*static*/
void VRManagerParent::RegisterVRManagerInCompositorThread(
    VRManagerParent* aVRManager) {
  aVRManager->RegisterWithManager();
}

/*static*/
already_AddRefed<VRManagerParent> VRManagerParent::CreateSameProcess() {
  RefPtr<VRManagerParent> vmp = new VRManagerParent(
      base::GetCurrentProcId(), dom::ContentParentId(), false);
  vmp->mCompositorThreadHolder = CompositorThreadHolder::GetSingleton();
  CompositorThread()->Dispatch(
      NewRunnableFunction("RegisterVRManagerIncompositorThreadRunnable",
                          RegisterVRManagerInCompositorThread, vmp.get()));
  return vmp.forget();
}

bool VRManagerParent::CreateForGPUProcess(
    Endpoint<PVRManagerParent>&& aEndpoint) {
  RefPtr<VRManagerParent> vmp =
      new VRManagerParent(aEndpoint.OtherPid(), dom::ContentParentId(), false);
  vmp->mCompositorThreadHolder = CompositorThreadHolder::GetSingleton();
  CompositorThread()->Dispatch(NewRunnableMethod<Endpoint<PVRManagerParent>&&>(
      "gfx::VRManagerParent::Bind", vmp, &VRManagerParent::Bind,
      std::move(aEndpoint)));
  return true;
}

/*static*/
void VRManagerParent::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(
      CompositorThread(),
      "Shutdown() must gets called before the compositor thread is shutdown");
  ReleaseVRManagerParentSingleton();
  CompositorThread()->Dispatch(NS_NewRunnableFunction(
      "VRManagerParent::Shutdown",
      [vm = RefPtr<VRManager>(VRManager::MaybeGet())]() -> void {
        if (!vm) {
          return;
        }
        vm->ShutdownVRManagerParents();
      }));
}

void VRManagerParent::ActorDestroy(ActorDestroyReason why) {
  UnregisterFromManager();
  mCompositorThreadHolder = nullptr;
}

mozilla::ipc::IPCResult VRManagerParent::RecvDetectRuntimes() {
  // Detect runtime capabilities. This will return the presense of VR and/or AR
  // runtime software, without enumerating or activating any hardware devices.
  // UpdateDisplayInfo will be sent to VRManagerChild with the results of the
  // detection.
  VRManager* vm = VRManager::Get();
  vm->DetectRuntimes();

  return IPC_OK();
}

mozilla::ipc::IPCResult VRManagerParent::RecvRefreshDisplays() {
  // This is called to activate the VR runtimes, detecting the
  // presence and capabilities of XR hardware.
  // UpdateDisplayInfo will be sent to VRManagerChild with the results of the
  // enumerated hardware.
  VRManager* vm = VRManager::Get();
  vm->EnumerateDevices();

  return IPC_OK();
}

mozilla::ipc::IPCResult VRManagerParent::RecvSetGroupMask(
    const uint32_t& aDisplayID, const uint32_t& aGroupMask) {
  VRManager* vm = VRManager::Get();
  vm->SetGroupMask(aGroupMask);
  return IPC_OK();
}

bool VRManagerParent::HaveEventListener() { return mHaveEventListener; }

bool VRManagerParent::HaveControllerListener() {
  return mHaveControllerListener;
}

bool VRManagerParent::GetVRActiveStatus() { return mVRActiveStatus; }

mozilla::ipc::IPCResult VRManagerParent::RecvSetHaveEventListener(
    const bool& aHaveEventListener) {
  mHaveEventListener = aHaveEventListener;
  return IPC_OK();
}

mozilla::ipc::IPCResult VRManagerParent::RecvControllerListenerAdded() {
  // Force update the available controllers for GamepadManager,
  VRManager* vm = VRManager::Get();
  vm->StopAllHaptics();
  mHaveControllerListener = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult VRManagerParent::RecvControllerListenerRemoved() {
  mHaveControllerListener = false;
  VRManager* vm = VRManager::Get();
  vm->StopAllHaptics();
  return IPC_OK();
}

mozilla::ipc::IPCResult VRManagerParent::RecvRunPuppet(
    const nsTArray<uint64_t>& aBuffer) {
#if defined(MOZ_WIDGET_ANDROID)
  // Not yet implemented for Android / GeckoView
  // See Bug 1555192
  Unused << SendNotifyPuppetCommandBufferCompleted(false);
#else
  VRManager* vm = VRManager::Get();
  if (!vm->RunPuppet(aBuffer, this)) {
    // We have immediately failed, need to resolve the
    // promise right away
    Unused << SendNotifyPuppetCommandBufferCompleted(false);
  }
#endif  // defined(MOZ_WIDGET_ANDROID)
  return IPC_OK();
}

mozilla::ipc::IPCResult VRManagerParent::RecvResetPuppet() {
#if defined(MOZ_WIDGET_ANDROID)
  // Not yet implemented for Android / GeckoView
  // See Bug 1555192
#else
  VRManager* vm = VRManager::Get();
  vm->ResetPuppet(this);
#endif  // defined(MOZ_WIDGET_ANDROID)
  return IPC_OK();
}

mozilla::ipc::IPCResult VRManagerParent::RecvVibrateHaptic(
    const mozilla::dom::GamepadHandle& aGamepadHandle,
    const uint32_t& aHapticIndex, const double& aIntensity,
    const double& aDuration, const uint32_t& aPromiseID) {
  VRManager* vm = VRManager::Get();
  VRManagerPromise promise(this, aPromiseID);

  vm->VibrateHaptic(aGamepadHandle, aHapticIndex, aIntensity, aDuration,
                    promise);
  return IPC_OK();
}

mozilla::ipc::IPCResult VRManagerParent::RecvStopVibrateHaptic(
    const mozilla::dom::GamepadHandle& aGamepadHandle) {
  VRManager* vm = VRManager::Get();
  vm->StopVibrateHaptic(aGamepadHandle);
  return IPC_OK();
}

bool VRManagerParent::SendReplyGamepadVibrateHaptic(
    const uint32_t& aPromiseID) {
  // GamepadManager only exists at the content process
  // or the same process in non-e10s mode.
  if (mHaveControllerListener && (mIsContentChild || IsSameProcess())) {
    return PVRManagerParent::SendReplyGamepadVibrateHaptic(aPromiseID);
  }

  return true;
}

mozilla::ipc::IPCResult VRManagerParent::RecvStartVRNavigation(
    const uint32_t& aDeviceID) {
  VRManager* vm = VRManager::Get();
  vm->StartVRNavigation(aDeviceID);
  return IPC_OK();
}

mozilla::ipc::IPCResult VRManagerParent::RecvStopVRNavigation(
    const uint32_t& aDeviceID, const TimeDuration& aTimeout) {
  VRManager* vm = VRManager::Get();
  vm->StopVRNavigation(aDeviceID, aTimeout);
  return IPC_OK();
}

mozilla::ipc::IPCResult VRManagerParent::RecvStartActivity() {
  mVRActiveStatus = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult VRManagerParent::RecvStopActivity() {
  mVRActiveStatus = false;
  return IPC_OK();
}

}  // namespace gfx
}  // namespace mozilla

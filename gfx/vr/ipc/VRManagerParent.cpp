/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRManagerParent.h"

#include "ipc/VRLayerParent.h"
#include "mozilla/gfx/PVRManagerParent.h"
#include "mozilla/ipc/ProtocolTypes.h"
#include "mozilla/ipc/ProtocolUtils.h"  // for IToplevelProtocol
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "mozilla/Unused.h"
#include "VRManager.h"
#include "VRThread.h"

namespace mozilla {
using namespace layers;
namespace gfx {

VRManagerParent::VRManagerParent(ProcessId aChildProcessId,
                                 bool aIsContentChild)
    : mHaveEventListener(false),
      mHaveControllerListener(false),
      mIsContentChild(aIsContentChild),
      mVRActiveStatus(true) {
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
bool VRManagerParent::CreateForContent(Endpoint<PVRManagerParent>&& aEndpoint) {
  MessageLoop* loop = CompositorThreadHolder::Loop();
  if (!loop) {
    return false;
  }

  RefPtr<VRManagerParent> vmp = new VRManagerParent(aEndpoint.OtherPid(), true);
  loop->PostTask(NewRunnableMethod<Endpoint<PVRManagerParent>&&>(
      "gfx::VRManagerParent::Bind", vmp, &VRManagerParent::Bind,
      std::move(aEndpoint)));

  return true;
}

void VRManagerParent::Bind(Endpoint<PVRManagerParent>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    return;
  }
  mSelfRef = this;

  RegisterWithManager();
}

/*static*/
void VRManagerParent::RegisterVRManagerInCompositorThread(
    VRManagerParent* aVRManager) {
  aVRManager->RegisterWithManager();
}

/*static*/
VRManagerParent* VRManagerParent::CreateSameProcess() {
  MessageLoop* loop = CompositorThreadHolder::Loop();
  RefPtr<VRManagerParent> vmp =
      new VRManagerParent(base::GetCurrentProcId(), false);
  vmp->mCompositorThreadHolder = CompositorThreadHolder::GetSingleton();
  vmp->mSelfRef = vmp;
  loop->PostTask(
      NewRunnableFunction("RegisterVRManagerIncompositorThreadRunnable",
                          RegisterVRManagerInCompositorThread, vmp.get()));
  return vmp.get();
}

bool VRManagerParent::CreateForGPUProcess(
    Endpoint<PVRManagerParent>&& aEndpoint) {
  MessageLoop* loop = CompositorThreadHolder::Loop();

  RefPtr<VRManagerParent> vmp =
      new VRManagerParent(aEndpoint.OtherPid(), false);
  vmp->mCompositorThreadHolder = CompositorThreadHolder::GetSingleton();
  vmp->mSelfRef = vmp;
  loop->PostTask(NewRunnableMethod<Endpoint<PVRManagerParent>&&>(
      "gfx::VRManagerParent::Bind", vmp, &VRManagerParent::Bind,
      std::move(aEndpoint)));
  return true;
}

void VRManagerParent::DeferredDestroy() {
  mCompositorThreadHolder = nullptr;
  mSelfRef = nullptr;
}

void VRManagerParent::ActorDestroy(ActorDestroyReason why) {
  UnregisterFromManager();
  MessageLoop::current()->PostTask(
      NewRunnableMethod("gfx::VRManagerParent::DeferredDestroy", this,
                        &VRManagerParent::DeferredDestroy));
}

void VRManagerParent::OnChannelConnected(int32_t aPid) {
  mCompositorThreadHolder = CompositorThreadHolder::GetSingleton();
}

mozilla::ipc::IPCResult VRManagerParent::RecvRefreshDisplays() {
  // This is called to refresh the VR Displays for Navigator.GetVRDevices().
  // We must pass "true" to VRManager::RefreshVRDisplays()
  // to ensure that the promise returned by Navigator.GetVRDevices
  // can resolve even if there are no changes to the VR Displays.
  VRManager* vm = VRManager::Get();
  vm->RefreshVRDisplays(true);

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
    const uint32_t& aControllerIdx, const uint32_t& aHapticIndex,
    const double& aIntensity, const double& aDuration,
    const uint32_t& aPromiseID) {
  VRManager* vm = VRManager::Get();
  VRManagerPromise promise(this, aPromiseID);

  vm->VibrateHaptic(aControllerIdx, aHapticIndex, aIntensity, aDuration,
                    promise);
  return IPC_OK();
}

mozilla::ipc::IPCResult VRManagerParent::RecvStopVibrateHaptic(
    const uint32_t& aControllerIdx) {
  VRManager* vm = VRManager::Get();
  vm->StopVibrateHaptic(aControllerIdx);
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

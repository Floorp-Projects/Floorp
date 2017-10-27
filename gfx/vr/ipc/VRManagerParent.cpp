/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRManagerParent.h"
#include "ipc/VRLayerParent.h"
#include "mozilla/gfx/PVRManagerParent.h"
#include "mozilla/ipc/ProtocolTypes.h"
#include "mozilla/ipc/ProtocolUtils.h"       // for IToplevelProtocol
#include "mozilla/TimeStamp.h"               // for TimeStamp
#include "mozilla/Unused.h"
#include "VRManager.h"
#include "VRThread.h"
#include "gfxVRPuppet.h"

namespace mozilla {
using namespace layers;
namespace gfx {

VRManagerParent::VRManagerParent(ProcessId aChildProcessId, bool aIsContentChild)
  : mControllerTestID(1)
  , mHaveEventListener(false)
  , mHaveControllerListener(false)
  , mIsContentChild(aIsContentChild)
{
  MOZ_COUNT_CTOR(VRManagerParent);
  MOZ_ASSERT(NS_IsMainThread());

  SetOtherProcessId(aChildProcessId);
}

VRManagerParent::~VRManagerParent()
{
  MOZ_ASSERT(!mVRManagerHolder);

  MOZ_COUNT_DTOR(VRManagerParent);
}

PVRLayerParent*
VRManagerParent::AllocPVRLayerParent(const uint32_t& aDisplayID,
                                     const uint32_t& aGroup)
{
  RefPtr<VRLayerParent> layer;
  layer = new VRLayerParent(aDisplayID,
                            aGroup);
  VRManager* vm = VRManager::Get();
  RefPtr<gfx::VRDisplayHost> display = vm->GetDisplay(aDisplayID);
  if (display) {
    display->AddLayer(layer);
  }
  return layer.forget().take();
}

bool
VRManagerParent::DeallocPVRLayerParent(PVRLayerParent* actor)
{
  delete actor;
  return true;
}

bool
VRManagerParent::IsSameProcess() const
{
  return OtherPid() == base::GetCurrentProcId();
}

void
VRManagerParent::RegisterWithManager()
{
  VRManager* vm = VRManager::Get();
  vm->AddVRManagerParent(this);
  mVRManagerHolder = vm;
}

void
VRManagerParent::UnregisterFromManager()
{
  VRManager* vm = VRManager::Get();
  vm->RemoveVRManagerParent(this);
  mVRManagerHolder = nullptr;
}

/* static */ bool
VRManagerParent::CreateForContent(Endpoint<PVRManagerParent>&& aEndpoint)
{
  MessageLoop* loop = VRListenerThreadHolder::Loop();

  RefPtr<VRManagerParent> vmp = new VRManagerParent(aEndpoint.OtherPid(), true);
  loop->PostTask(NewRunnableMethod<Endpoint<PVRManagerParent>&&>(
    "gfx::VRManagerParent::Bind",
    vmp,
    &VRManagerParent::Bind,
    Move(aEndpoint)));

  return true;
}

void
VRManagerParent::Bind(Endpoint<PVRManagerParent>&& aEndpoint)
{
  if (!aEndpoint.Bind(this)) {
    return;
  }
  mSelfRef = this;

  RegisterWithManager();
}

/*static*/ void
VRManagerParent::RegisterVRManagerInVRListenerThread(VRManagerParent* aVRManager)
{
  aVRManager->RegisterWithManager();
}

/*static*/ VRManagerParent*
VRManagerParent::CreateSameProcess()
{
  MessageLoop* loop = VRListenerThreadHolder::Loop();
  RefPtr<VRManagerParent> vmp = new VRManagerParent(base::GetCurrentProcId(), false);
  vmp->mVRListenerThreadHolder = VRListenerThreadHolder::GetSingleton();
  vmp->mSelfRef = vmp;
  loop->PostTask(NewRunnableFunction("RegisterVRManagerInVRListenerThreadRunnable",
                                     RegisterVRManagerInVRListenerThread, vmp.get()));
  return vmp.get();
}

bool
VRManagerParent::CreateForGPUProcess(Endpoint<PVRManagerParent>&& aEndpoint)
{
  MessageLoop* loop = VRListenerThreadHolder::Loop();

  RefPtr<VRManagerParent> vmp = new VRManagerParent(aEndpoint.OtherPid(), false);
  vmp->mVRListenerThreadHolder = VRListenerThreadHolder::GetSingleton();
  loop->PostTask(NewRunnableMethod<Endpoint<PVRManagerParent>&&>(
    "gfx::VRManagerParent::Bind",
    vmp,
    &VRManagerParent::Bind,
    Move(aEndpoint)));
  return true;
}

void
VRManagerParent::DeferredDestroy()
{
  mVRListenerThreadHolder = nullptr;
  mSelfRef = nullptr;
}

void
VRManagerParent::ActorDestroy(ActorDestroyReason why)
{
  UnregisterFromManager();
  MessageLoop::current()->PostTask(
    NewRunnableMethod("gfx::VRManagerParent::DeferredDestroy",
                      this,
                      &VRManagerParent::DeferredDestroy));
}

void
VRManagerParent::OnChannelConnected(int32_t aPid)
{
  mVRListenerThreadHolder = VRListenerThreadHolder::GetSingleton();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvRefreshDisplays()
{
  // TODO: Bug 1406327, Launch VR listener thread here.
  MOZ_ASSERT(VRListenerThreadHolder::IsInVRListenerThread());

  // This is called to refresh the VR Displays for Navigator.GetVRDevices().
  // We must pass "true" to VRManager::RefreshVRDisplays()
  // to ensure that the promise returned by Navigator.GetVRDevices
  // can resolve even if there are no changes to the VR Displays.
  VRManager* vm = VRManager::Get();
  vm->RefreshVRDisplays(true);

  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvResetSensor(const uint32_t& aDisplayID)
{
  VRManager* vm = VRManager::Get();
  RefPtr<gfx::VRDisplayHost> display = vm->GetDisplay(aDisplayID);
  if (display != nullptr) {
    display->ZeroSensor();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvSetGroupMask(const uint32_t& aDisplayID, const uint32_t& aGroupMask)
{
  VRManager* vm = VRManager::Get();
  RefPtr<gfx::VRDisplayHost> display = vm->GetDisplay(aDisplayID);
  if (display != nullptr) {
    display->SetGroupMask(aGroupMask);
  }
  return IPC_OK();
}

bool
VRManagerParent::HaveEventListener()
{
  return mHaveEventListener;
}

bool
VRManagerParent::HaveControllerListener()
{
  return mHaveControllerListener;
}

mozilla::ipc::IPCResult
VRManagerParent::RecvSetHaveEventListener(const bool& aHaveEventListener)
{
  mHaveEventListener = aHaveEventListener;
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvControllerListenerAdded()
{
  // Force update the available controllers for GamepadManager,
  // remove the existing controllers and sync them by NotifyVsync().
  VRManager* vm = VRManager::Get();
  vm->RemoveControllers();
  mHaveControllerListener = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvControllerListenerRemoved()
{
  mHaveControllerListener = false;
  VRManager* vm = VRManager::Get();
  vm->RemoveControllers();
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvCreateVRTestSystem()
{
  VRManager* vm = VRManager::Get();
  vm->CreateVRTestSystem();
  // The mControllerTestID is 1 based
  mControllerTestID = 1;
  mVRControllerTests.Clear();
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvCreateVRServiceTestDisplay(const nsCString& aID, const uint32_t& aPromiseID)
{
  VRManager* vm = VRManager::Get();
  VRSystemManagerPuppet* puppetManager = vm->GetPuppetManager();
  uint32_t deviceID = puppetManager->CreateTestDisplay();

  if (SendReplyCreateVRServiceTestDisplay(aID, aPromiseID, deviceID)) {
    return IPC_OK();
  }

  return IPC_FAIL(this, "SendReplyCreateVRServiceTestController fail");
}

mozilla::ipc::IPCResult
VRManagerParent::RecvCreateVRServiceTestController(const nsCString& aID, const uint32_t& aPromiseID)
{
  uint32_t controllerIdx = 1; // ID's are 1 based
  nsTArray<VRControllerInfo> controllerInfoArray;
  impl::VRControllerPuppet* controllerPuppet = nullptr;
  VRManager* vm = VRManager::Get();

  /**
   * When running headless mochitests on some of our automated test
   * infrastructure, 2d display vsyncs are not always generated.
   * In this case, the test controllers can't be created immediately
   * after the VR display was created as the state of the VR displays
   * are updated during vsync.
   * To workaround, we produce a vsync manually.
   */
  vm->NotifyVsync(TimeStamp::Now());

  // Get VRControllerPuppet from VRManager
  vm->GetVRControllerInfo(controllerInfoArray);
  for (auto& controllerInfo : controllerInfoArray) {
    if (controllerInfo.GetType() == VRDeviceType::Puppet) {
      if (controllerIdx == mControllerTestID) {
        controllerPuppet = static_cast<impl::VRControllerPuppet*>(
                           vm->GetController(controllerInfo.GetControllerID()).get());
        break;
      }
      ++controllerIdx;
    }
  }

  // We might not have a controllerPuppet if the test did
  // not create a VR display first.
  if (!controllerPuppet) {
    // We send a device ID of "0" to indicate failure
    if (SendReplyCreateVRServiceTestController(aID, aPromiseID, 0)) {
      return IPC_OK();
    }
  } else {
    if (!mVRControllerTests.Get(mControllerTestID, nullptr)) {
      mVRControllerTests.Put(mControllerTestID, controllerPuppet);
    }

    if (SendReplyCreateVRServiceTestController(aID, aPromiseID, mControllerTestID)) {
      ++mControllerTestID;
      return IPC_OK();
    }
  }

  return IPC_FAIL(this, "SendReplyCreateVRServiceTestController fail");
}

mozilla::ipc::IPCResult
VRManagerParent::RecvSetDisplayInfoToMockDisplay(const uint32_t& aDeviceID,
                                                 const VRDisplayInfo& aDisplayInfo)
{
  VRManager* vm = VRManager::Get();
  VRSystemManagerPuppet* puppetManager = vm->GetPuppetManager();
  puppetManager->SetPuppetDisplayInfo(aDeviceID, aDisplayInfo);
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvSetSensorStateToMockDisplay(const uint32_t& aDeviceID,
                                                 const VRHMDSensorState& aSensorState)
{
  VRManager* vm = VRManager::Get();
  VRSystemManagerPuppet* puppetManager = vm->GetPuppetManager();
  puppetManager->SetPuppetDisplaySensorState(aDeviceID, aSensorState);
  return IPC_OK();
}

already_AddRefed<impl::VRControllerPuppet>
VRManagerParent::GetControllerPuppet(uint32_t aDeviceID)
{
  // aDeviceID for controllers start at 1 and are
  // used as a key to mVRControllerTests
  MOZ_ASSERT(aDeviceID > 0);
  RefPtr<impl::VRControllerPuppet> controllerPuppet;
  mVRControllerTests.Get(aDeviceID,
                         getter_AddRefs(controllerPuppet));
  MOZ_ASSERT(controllerPuppet);
  return controllerPuppet.forget();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvNewButtonEventToMockController(const uint32_t& aDeviceID, const long& aButton,
                                                    const bool& aPressed)
{
  RefPtr<impl::VRControllerPuppet> controllerPuppet = GetControllerPuppet(aDeviceID);
  if (controllerPuppet) {
    controllerPuppet->SetButtonPressState(aButton, aPressed);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvNewAxisMoveEventToMockController(const uint32_t& aDeviceID, const long& aAxis,
                                                      const double& aValue)
{
  RefPtr<impl::VRControllerPuppet> controllerPuppet = GetControllerPuppet(aDeviceID);
  if (controllerPuppet) {
    controllerPuppet->SetAxisMoveState(aAxis, aValue);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvNewPoseMoveToMockController(const uint32_t& aDeviceID,
                                                 const GamepadPoseState& pose)
{
  RefPtr<impl::VRControllerPuppet> controllerPuppet = GetControllerPuppet(aDeviceID);
  if (controllerPuppet) {
    controllerPuppet->SetPoseMoveState(pose);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvVibrateHaptic(const uint32_t& aControllerIdx,
                                   const uint32_t& aHapticIndex,
                                   const double& aIntensity,
                                   const double& aDuration,
                                   const uint32_t& aPromiseID)
{
  VRManager* vm = VRManager::Get();
  VRManagerPromise promise(this, aPromiseID);

  vm->VibrateHaptic(aControllerIdx, aHapticIndex, aIntensity,
                    aDuration, promise);
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvStopVibrateHaptic(const uint32_t& aControllerIdx)
{
  VRManager* vm = VRManager::Get();
  vm->StopVibrateHaptic(aControllerIdx);
  return IPC_OK();
}

bool
VRManagerParent::SendGamepadUpdate(const GamepadChangeEvent& aGamepadEvent)
{
  // GamepadManager only exists at the content process
  // or the same process in non-e10s mode.
  if (mHaveControllerListener &&
      (mIsContentChild || IsSameProcess())) {
    return PVRManagerParent::SendGamepadUpdate(aGamepadEvent);
  }

  return true;
}

bool
VRManagerParent::SendReplyGamepadVibrateHaptic(const uint32_t& aPromiseID)
{
  // GamepadManager only exists at the content process
  // or the same process in non-e10s mode.
  if (mHaveControllerListener &&
      (mIsContentChild || IsSameProcess())) {
    return PVRManagerParent::SendReplyGamepadVibrateHaptic(aPromiseID);
  }

  return true;
}

} // namespace gfx
} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRManagerParent.h"
#include "ipc/VRLayerParent.h"
#include "mozilla/gfx/PVRManagerParent.h"
#include "mozilla/ipc/ProtocolTypes.h"
#include "mozilla/ipc/ProtocolUtils.h"       // for IToplevelProtocol
#include "mozilla/TimeStamp.h"               // for TimeStamp
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/Unused.h"
#include "VRManager.h"
#include "gfxVRPuppet.h"

namespace mozilla {
using namespace layers;
namespace gfx {

VRManagerParent::VRManagerParent(ProcessId aChildProcessId, bool aIsContentChild)
  : HostIPCAllocator()
  , mDisplayTestID(0)
  , mControllerTestID(0)
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

PTextureParent*
VRManagerParent::AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                     const LayersBackend& aLayersBackend,
                                     const TextureFlags& aFlags,
                                     const uint64_t& aSerial)
{
  return layers::TextureHost::CreateIPDLActor(this, aSharedData, aLayersBackend, aFlags, aSerial, Nothing());
}

bool
VRManagerParent::DeallocPTextureParent(PTextureParent* actor)
{
  return layers::TextureHost::DestroyIPDLActor(actor);
}

PVRLayerParent*
VRManagerParent::AllocPVRLayerParent(const uint32_t& aDisplayID,
                                     const float& aLeftEyeX,
                                     const float& aLeftEyeY,
                                     const float& aLeftEyeWidth,
                                     const float& aLeftEyeHeight,
                                     const float& aRightEyeX,
                                     const float& aRightEyeY,
                                     const float& aRightEyeWidth,
                                     const float& aRightEyeHeight,
                                     const uint32_t& aGroup)
{
  RefPtr<VRLayerParent> layer;
  layer = new VRLayerParent(aDisplayID,
                            Rect(aLeftEyeX, aLeftEyeY, aLeftEyeWidth, aLeftEyeHeight),
                            Rect(aRightEyeX, aRightEyeY, aRightEyeWidth, aRightEyeHeight),
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
VRManagerParent::AllocShmem(size_t aSize,
  ipc::SharedMemory::SharedMemoryType aType,
  ipc::Shmem* aShmem)
{
  return PVRManagerParent::AllocShmem(aSize, aType, aShmem);
}

bool
VRManagerParent::AllocUnsafeShmem(size_t aSize,
  ipc::SharedMemory::SharedMemoryType aType,
  ipc::Shmem* aShmem)
{
  return PVRManagerParent::AllocUnsafeShmem(aSize, aType, aShmem);
}

void
VRManagerParent::DeallocShmem(ipc::Shmem& aShmem)
{
  PVRManagerParent::DeallocShmem(aShmem);
}

bool
VRManagerParent::IsSameProcess() const
{
  return OtherPid() == base::GetCurrentProcId();
}

void
VRManagerParent::NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

void
VRManagerParent::SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

base::ProcessId
VRManagerParent::GetChildProcessId()
{
  return OtherPid();
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
  MessageLoop* loop = layers::CompositorThreadHolder::Loop();

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
VRManagerParent::RegisterVRManagerInCompositorThread(VRManagerParent* aVRManager)
{
  aVRManager->RegisterWithManager();
}

/*static*/ VRManagerParent*
VRManagerParent::CreateSameProcess()
{
  MessageLoop* loop = mozilla::layers::CompositorThreadHolder::Loop();
  RefPtr<VRManagerParent> vmp = new VRManagerParent(base::GetCurrentProcId(), false);
  vmp->mCompositorThreadHolder = layers::CompositorThreadHolder::GetSingleton();
  vmp->mSelfRef = vmp;
  loop->PostTask(NewRunnableFunction(RegisterVRManagerInCompositorThread, vmp.get()));
  return vmp.get();
}

bool
VRManagerParent::CreateForGPUProcess(Endpoint<PVRManagerParent>&& aEndpoint)
{
  MessageLoop* loop = mozilla::layers::CompositorThreadHolder::Loop();

  RefPtr<VRManagerParent> vmp = new VRManagerParent(aEndpoint.OtherPid(), false);
  vmp->mCompositorThreadHolder = layers::CompositorThreadHolder::GetSingleton();
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
  mCompositorThreadHolder = nullptr;
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
  mCompositorThreadHolder = layers::CompositorThreadHolder::GetSingleton();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvRefreshDisplays()
{
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
  VRManager* vm = VRManager::Get();
  mHaveControllerListener = true;
  // Ask the connected gamepads to be added to GamepadManager
  vm->ScanForControllers();
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvControllerListenerRemoved()
{
  VRManager* vm = VRManager::Get();
  mHaveControllerListener = false;
  vm->RemoveControllers();
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvCreateVRTestSystem()
{
  VRManager* vm = VRManager::Get();
  vm->CreateVRTestSystem();
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvCreateVRServiceTestDisplay(const nsCString& aID, const uint32_t& aPromiseID)
{
  nsTArray<VRDisplayInfo> displayInfoArray;
  impl::VRDisplayPuppet* displayPuppet = nullptr;
  VRManager* vm = VRManager::Get();
  vm->RefreshVRDisplays();

  // Get VRDisplayPuppet from VRManager
  vm->GetVRDisplayInfo(displayInfoArray);
  for (auto& displayInfo : displayInfoArray) {
    if (displayInfo.GetType() == VRDeviceType::Puppet) {
        displayPuppet = static_cast<impl::VRDisplayPuppet*>(
                        vm->GetDisplay(displayInfo.GetDisplayID()).get());
        break;
    }
  }

  MOZ_ASSERT(displayPuppet);
  MOZ_ASSERT(!mDisplayTestID); // We have only one display in VRSystemManagerPuppet.

  if (!mVRDisplayTests.Get(mDisplayTestID, nullptr)) {
    mVRDisplayTests.Put(mDisplayTestID, displayPuppet);
  }

  if (SendReplyCreateVRServiceTestDisplay(aID, aPromiseID, mDisplayTestID)) {
    return IPC_OK();
  }

  return IPC_FAIL(this, "SendReplyCreateVRServiceTestController fail");
}

mozilla::ipc::IPCResult
VRManagerParent::RecvCreateVRServiceTestController(const nsCString& aID, const uint32_t& aPromiseID)
{
  uint32_t controllerIdx = 0;
  nsTArray<VRControllerInfo> controllerInfoArray;
  impl::VRControllerPuppet* controllerPuppet = nullptr;
  VRManager* vm = VRManager::Get();

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

  MOZ_ASSERT(controllerPuppet);
  MOZ_ASSERT(mControllerTestID < 2); // We have only two controllers in VRSystemManagerPuppet.

  if (!mVRControllerTests.Get(mControllerTestID, nullptr)) {
    mVRControllerTests.Put(mControllerTestID, controllerPuppet);
  }

  if (SendReplyCreateVRServiceTestController(aID, aPromiseID, mControllerTestID)) {
    ++mControllerTestID;
    return IPC_OK();
  }

  return IPC_FAIL(this, "SendReplyCreateVRServiceTestController fail");
}

mozilla::ipc::IPCResult
VRManagerParent::RecvSetDisplayInfoToMockDisplay(const uint32_t& aDeviceID,
                                                 const VRDisplayInfo& aDisplayInfo)
{
  RefPtr<impl::VRDisplayPuppet> displayPuppet;
  mVRDisplayTests.Get(mDisplayTestID,
                      getter_AddRefs(displayPuppet));
  MOZ_ASSERT(displayPuppet);
  displayPuppet->SetDisplayInfo(aDisplayInfo);
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvSetSensorStateToMockDisplay(const uint32_t& aDeviceID,
                                                 const VRHMDSensorState& aSensorState)
{
  RefPtr<impl::VRDisplayPuppet> displayPuppet;
  mVRDisplayTests.Get(mDisplayTestID,
                      getter_AddRefs(displayPuppet));
  MOZ_ASSERT(displayPuppet);
  displayPuppet->SetSensorState(aSensorState);
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvNewButtonEventToMockController(const uint32_t& aDeviceID, const long& aButton,
                                                    const bool& aPressed)
{
  RefPtr<impl::VRControllerPuppet> controllerPuppet;
  mVRControllerTests.Get(mControllerTestID,
                         getter_AddRefs(controllerPuppet));
  MOZ_ASSERT(controllerPuppet);
  controllerPuppet->SetButtonPressState(aButton, aPressed);
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvNewAxisMoveEventToMockController(const uint32_t& aDeviceID, const long& aAxis,
                                                      const double& aValue)
{
  RefPtr<impl::VRControllerPuppet> controllerPuppet;
  mVRControllerTests.Get(mControllerTestID,
                         getter_AddRefs(controllerPuppet));
  MOZ_ASSERT(controllerPuppet);
  controllerPuppet->SetAxisMoveState(aAxis, aValue);
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvNewPoseMoveToMockController(const uint32_t& aDeviceID,
                                                 const GamepadPoseState& pose)
{
  RefPtr<impl::VRControllerPuppet> controllerPuppet;
  mVRControllerTests.Get(mControllerTestID,
                         getter_AddRefs(controllerPuppet));
  MOZ_ASSERT(controllerPuppet);
  controllerPuppet->SetPoseMoveState(pose);
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
  vm->VibrateHaptic(aControllerIdx, aHapticIndex, aIntensity,
                    aDuration, aPromiseID);
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
  } else {
    return true;
  }
}

bool
VRManagerParent::SendReplyGamepadVibrateHaptic(const uint32_t& aPromiseID)
{
  // GamepadManager only exists at the content process
  // or the same process in non-e10s mode.
  if (mHaveControllerListener &&
      (mIsContentChild || IsSameProcess())) {
    return PVRManagerParent::SendReplyGamepadVibrateHaptic(aPromiseID);
  } else {
    return true;
  }
}

} // namespace gfx
} // namespace mozilla

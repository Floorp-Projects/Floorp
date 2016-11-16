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

namespace mozilla {
using namespace layers;
namespace gfx {

VRManagerParent::VRManagerParent(ProcessId aChildProcessId)
  : HostIPCAllocator()
  , mHaveEventListener(false)
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
  return layers::TextureHost::CreateIPDLActor(this, aSharedData, aLayersBackend, aFlags, aSerial);
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
                                     const float& aRightEyeHeight)
{
  RefPtr<VRLayerParent> layer;
  layer = new VRLayerParent(aDisplayID,
                            Rect(aLeftEyeX, aLeftEyeY, aLeftEyeWidth, aLeftEyeHeight),
                            Rect(aRightEyeX, aRightEyeY, aRightEyeWidth, aRightEyeHeight));
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
  gfx::VRLayerParent* layer = static_cast<gfx::VRLayerParent*>(actor);

  VRManager* vm = VRManager::Get();
  RefPtr<gfx::VRDisplayHost> display = vm->GetDisplay(layer->GetDisplayID());
  if (display) {
    display->RemoveLayer(layer);
  }

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

  RefPtr<VRManagerParent> vmp = new VRManagerParent(aEndpoint.OtherPid());
  loop->PostTask(NewRunnableMethod<Endpoint<PVRManagerParent>&&>(
    vmp, &VRManagerParent::Bind, Move(aEndpoint)));

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
  RefPtr<VRManagerParent> vmp = new VRManagerParent(base::GetCurrentProcId());
  vmp->mCompositorThreadHolder = layers::CompositorThreadHolder::GetSingleton();
  vmp->mSelfRef = vmp;
  loop->PostTask(NewRunnableFunction(RegisterVRManagerInCompositorThread, vmp.get()));
  return vmp.get();
}

bool
VRManagerParent::CreateForGPUProcess(Endpoint<PVRManagerParent>&& aEndpoint)
{
  MessageLoop* loop = mozilla::layers::CompositorThreadHolder::Loop();

  RefPtr<VRManagerParent> vmp = new VRManagerParent(aEndpoint.OtherPid());
  vmp->mCompositorThreadHolder = layers::CompositorThreadHolder::GetSingleton();
  loop->PostTask(NewRunnableMethod<Endpoint<PVRManagerParent>&&>(
    vmp, &VRManagerParent::Bind, Move(aEndpoint)));
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
  MessageLoop::current()->PostTask(NewRunnableMethod(this, &VRManagerParent::DeferredDestroy));
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
VRManagerParent::RecvGetDisplays(nsTArray<VRDisplayInfo> *aDisplays)
{
  VRManager* vm = VRManager::Get();
  vm->GetVRDisplayInfo(*aDisplays);
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
VRManagerParent::RecvGetSensorState(const uint32_t& aDisplayID, VRHMDSensorState* aState)
{
  VRManager* vm = VRManager::Get();
  RefPtr<gfx::VRDisplayHost> display = vm->GetDisplay(aDisplayID);
  if (display != nullptr) {
    *aState = display->GetSensorState();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvGetImmediateSensorState(const uint32_t& aDisplayID, VRHMDSensorState* aState)
{
  VRManager* vm = VRManager::Get();
  RefPtr<gfx::VRDisplayHost> display = vm->GetDisplay(aDisplayID);
  if (display != nullptr) {
    *aState = display->GetImmediateSensorState();
  }
  return IPC_OK();
}

bool
VRManagerParent::HaveEventListener()
{
  return mHaveEventListener;
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
  // Ask the connected gamepads to be added to GamepadManager
  vm->ScanForDevices();

  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvControllerListenerRemoved()
{
  return IPC_OK();
}

mozilla::ipc::IPCResult
VRManagerParent::RecvGetControllers(nsTArray<VRControllerInfo> *aControllers)
{
  VRManager* vm = VRManager::Get();
  vm->GetVRControllerInfo(*aControllers);
  return IPC_OK();
}

} // namespace gfx
} // namespace mozilla

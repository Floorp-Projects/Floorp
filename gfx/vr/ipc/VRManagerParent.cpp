/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRManagerParent.h"
#include "mozilla/gfx/PVRManagerParent.h"
#include "mozilla/ipc/ProtocolTypes.h"
#include "mozilla/ipc/ProtocolUtils.h"       // for IToplevelProtocol
#include "mozilla/TimeStamp.h"               // for TimeStamp
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/unused.h"
#include "VRManager.h"

namespace mozilla {
namespace gfx {

VRManagerParent::VRManagerParent(ProcessId aChildProcessId)
{
  MOZ_COUNT_CTOR(VRManagerParent);
  MOZ_ASSERT(NS_IsMainThread());

  SetOtherProcessId(aChildProcessId);
}

VRManagerParent::~VRManagerParent()
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!mVRManagerHolder);

  MOZ_COUNT_DTOR(VRManagerParent);
}

void VRManagerParent::RegisterWithManager()
{
  VRManager* vm = VRManager::Get();
  vm->AddVRManagerParent(this);
  mVRManagerHolder = vm;
}

void VRManagerParent::UnregisterFromManager()
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
  if (!aEndpoint.Bind(this, nullptr)) {
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

mozilla::ipc::IToplevelProtocol*
VRManagerParent::CloneToplevel(const InfallibleTArray<mozilla::ipc::ProtocolFdMapping>& aFds,
                               base::ProcessHandle aPeerProcess,
                               mozilla::ipc::ProtocolCloneContext* aCtx)
{
  MOZ_ASSERT_UNREACHABLE("Not supported");
  return nullptr;
}

void
VRManagerParent::OnChannelConnected(int32_t aPid)
{
  mCompositorThreadHolder = layers::CompositorThreadHolder::GetSingleton();
}

bool
VRManagerParent::RecvRefreshDevices()
{
  VRManager* vm = VRManager::Get();
  vm->RefreshVRDevices();

  return true;
}

bool
VRManagerParent::RecvResetSensor(const uint32_t& aDeviceID)
{
  VRManager* vm = VRManager::Get();
  RefPtr<gfx::VRHMDInfo> device = vm->GetDevice(aDeviceID);
  if (device != nullptr) {
    device->ZeroSensor();
  }

  return true;
}

bool
VRManagerParent::RecvKeepSensorTracking(const uint32_t& aDeviceID)
{
  VRManager* vm = VRManager::Get();
  RefPtr<gfx::VRHMDInfo> device = vm->GetDevice(aDeviceID);
  if (device != nullptr) {
    Unused << device->KeepSensorTracking();
  }
  return true;
}

bool
VRManagerParent::RecvSetFOV(const uint32_t& aDeviceID,
                            const VRFieldOfView& aFOVLeft,
                            const VRFieldOfView& aFOVRight,
                            const double& zNear,
                            const double& zFar)
{
  VRManager* vm = VRManager::Get();
  RefPtr<gfx::VRHMDInfo> device = vm->GetDevice(aDeviceID);
  if (device != nullptr) {
    device->SetFOV(aFOVLeft, aFOVRight, zNear, zFar);
  }
  return true;
}

} // namespace gfx
} // namespace mozilla

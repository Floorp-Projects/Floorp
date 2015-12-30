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
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/unused.h"
#include "VRManager.h"

namespace mozilla {
namespace layers {

// defined in CompositorParent.cpp
CompositorThreadHolder* GetCompositorThreadHolder();

} // namespace layers

namespace gfx {

VRManagerParent::VRManagerParent(MessageLoop* aLoop,
                                 Transport* aTransport,
                                 ProcessId aChildProcessId)
{
  MOZ_COUNT_CTOR(VRManagerParent);
  MOZ_ASSERT(NS_IsMainThread());

  SetTransport(aTransport);
  SetOtherProcessId(aChildProcessId);
}

VRManagerParent::~VRManagerParent()
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!mVRManagerHolder);

  Transport* trans = GetTransport();
  if (trans) {
    MOZ_ASSERT(XRE_GetIOMessageLoop());
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                     new DeleteTask<Transport>(trans));
  }
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

/*static*/ void
VRManagerParent::ConnectVRManagerInParentProcess(VRManagerParent* aVRManager,
                                ipc::Transport* aTransport,
                                base::ProcessId aOtherPid)
{
  aVRManager->Open(aTransport, aOtherPid, XRE_GetIOMessageLoop(), ipc::ParentSide);
  aVRManager->RegisterWithManager();
}

/*static*/ VRManagerParent*
VRManagerParent::CreateCrossProcess(Transport* aTransport, ProcessId aChildProcessId)
{
  MessageLoop* loop = mozilla::layers::CompositorParent::CompositorLoop();
  RefPtr<VRManagerParent> vmp = new VRManagerParent(loop, aTransport, aChildProcessId);
  vmp->mSelfRef = vmp;
  loop->PostTask(FROM_HERE,
                 NewRunnableFunction(ConnectVRManagerInParentProcess,
                                     vmp.get(), aTransport, aChildProcessId));
  return vmp.get();
}

/*static*/ void
VRManagerParent::RegisterVRManagerInCompositorThread(VRManagerParent* aVRManager)
{
  aVRManager->RegisterWithManager();
}

/*static*/ VRManagerParent*
VRManagerParent::CreateSameProcess()
{
  MessageLoop* loop = mozilla::layers::CompositorParent::CompositorLoop();
  RefPtr<VRManagerParent> vmp = new VRManagerParent(loop, nullptr, base::GetCurrentProcId());
  vmp->mCompositorThreadHolder = layers::GetCompositorThreadHolder();
  vmp->mSelfRef = vmp;
  loop->PostTask(FROM_HERE,
                 NewRunnableFunction(RegisterVRManagerInCompositorThread, vmp.get()));
  return vmp.get();
}

void
VRManagerParent::DeferredDestroy()
{
  MOZ_ASSERT(mCompositorThreadHolder);
  mCompositorThreadHolder = nullptr;
  mSelfRef = nullptr;
}

void
VRManagerParent::ActorDestroy(ActorDestroyReason why)
{
  UnregisterFromManager();
  MessageLoop::current()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &VRManagerParent::DeferredDestroy));
}

mozilla::ipc::IToplevelProtocol*
VRManagerParent::CloneToplevel(const InfallibleTArray<mozilla::ipc::ProtocolFdMapping>& aFds,
                               base::ProcessHandle aPeerProcess,
                               mozilla::ipc::ProtocolCloneContext* aCtx)
{
  for (unsigned int i = 0; i < aFds.Length(); i++) {
    if (aFds[i].protocolId() == unsigned(GetProtocolId())) {
      Transport* transport = OpenDescriptor(aFds[i].fd(),
                                            Transport::MODE_SERVER);
      PVRManagerParent* vm = CreateCrossProcess(transport, base::GetProcId(aPeerProcess));
      vm->CloneManagees(this, aCtx);
      vm->IToplevelProtocol::SetTransport(transport);
      // The reference to the compositor thread is held in OnChannelConnected().
      // We need to do this for cloned actors, too.
      vm->OnChannelConnected(base::GetProcId(aPeerProcess));
      return vm;
    }
  }
  return nullptr;
}

void
VRManagerParent::OnChannelConnected(int32_t aPid)
{
  mCompositorThreadHolder = layers::GetCompositorThreadHolder();
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

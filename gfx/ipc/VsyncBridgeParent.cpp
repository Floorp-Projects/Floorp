/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VsyncBridgeParent.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"

using mozilla::layers::CompositorBridgeParent;
using mozilla::layers::CompositorThreadHolder;

namespace mozilla {
namespace gfx {

RefPtr<VsyncBridgeParent>
VsyncBridgeParent::Start(Endpoint<PVsyncBridgeParent>&& aEndpoint)
{
  RefPtr<VsyncBridgeParent> parent = new VsyncBridgeParent();

  RefPtr<Runnable> task = NewRunnableMethod<Endpoint<PVsyncBridgeParent>&&>(
    "gfx::VsyncBridgeParent::Open",
    parent,
    &VsyncBridgeParent::Open,
    std::move(aEndpoint));
  CompositorThreadHolder::Loop()->PostTask(task.forget());

  return parent;
}

VsyncBridgeParent::VsyncBridgeParent()
 : mOpen(false)
{
  MOZ_COUNT_CTOR(VsyncBridgeParent);
  mCompositorThreadRef = CompositorThreadHolder::GetSingleton();
}

VsyncBridgeParent::~VsyncBridgeParent()
{
  MOZ_COUNT_DTOR(VsyncBridgeParent);
}

void
VsyncBridgeParent::Open(Endpoint<PVsyncBridgeParent>&& aEndpoint)
{
  if (!aEndpoint.Bind(this)) {
    // We can't recover from this.
    MOZ_CRASH("Failed to bind VsyncBridgeParent to endpoint");
  }
  AddRef();
  mOpen = true;
}

mozilla::ipc::IPCResult
VsyncBridgeParent::RecvNotifyVsync(const TimeStamp& aTimeStamp, const LayersId& aLayersId)
{
  CompositorBridgeParent::NotifyVsync(aTimeStamp, aLayersId);
  return IPC_OK();
}

void
VsyncBridgeParent::Shutdown()
{
  MessageLoop* ccloop = CompositorThreadHolder::Loop();
  if (MessageLoop::current() != ccloop) {
    ccloop->PostTask(NewRunnableMethod("gfx::VsyncBridgeParent::ShutdownImpl",
                                       this,
                                       &VsyncBridgeParent::ShutdownImpl));
    return;
  }

  ShutdownImpl();
}

void
VsyncBridgeParent::ShutdownImpl()
{
  if (mOpen) {
    Close();
    mOpen = false;
  }
}

void
VsyncBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mOpen = false;
  mCompositorThreadRef = nullptr;
}

void
VsyncBridgeParent::DeallocPVsyncBridgeParent()
{
  Release();
}

} // namespace gfx
} // namespace mozilla

/* vim: set ts=2 sw=2 et tw=80: */
/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositorManagerParent.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"

namespace mozilla {
namespace layers {

StaticRefPtr<CompositorManagerParent> CompositorManagerParent::sInstance;
StaticMutex CompositorManagerParent::sMutex;

/* static */ already_AddRefed<CompositorManagerParent>
CompositorManagerParent::CreateSameProcess()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  StaticMutexAutoLock lock(sMutex);

  // We are creating a manager for the UI process, inside the combined GPU/UI
  // process. It is created more-or-less the same but we retain a reference to
  // the parent to access state.
  if (NS_WARN_IF(sInstance)) {
    MOZ_ASSERT_UNREACHABLE("Already initialized");
    return nullptr;
  }

  // The child is responsible for setting up the IPC channel in the same
  // process case because if we open from the child perspective, we can do it
  // on the main thread and complete before we return the manager handles.
  RefPtr<CompositorManagerParent> parent = new CompositorManagerParent();
  parent->SetOtherProcessId(base::GetCurrentProcId());

  // CompositorManagerParent::Bind would normally add a reference for IPDL but
  // we don't use that in the same process case.
  parent.get()->AddRef();
  sInstance = parent;
  return parent.forget();
}

/* static */ void
CompositorManagerParent::Create(Endpoint<PCompositorManagerParent>&& aEndpoint)
{
  MOZ_ASSERT(NS_IsMainThread());

  // We are creating a manager for the another process, inside the GPU process
  // (or UI process if it subsumbed the GPU process).
  MOZ_ASSERT(aEndpoint.OtherPid() != base::GetCurrentProcId());

  RefPtr<CompositorManagerParent> bridge = new CompositorManagerParent();

  RefPtr<Runnable> runnable = NewRunnableMethod<Endpoint<PCompositorManagerParent>&&>(
    bridge,
    &CompositorManagerParent::Bind,
    Move(aEndpoint));
  CompositorThreadHolder::Loop()->PostTask(runnable.forget());
}

/* static */ already_AddRefed<CompositorBridgeParent>
CompositorManagerParent::CreateSameProcessWidgetCompositorBridge(CSSToLayoutDeviceScale aScale,
                                                                 const CompositorOptions& aOptions,
                                                                 bool aUseExternalSurfaceSize,
                                                                 const gfx::IntSize& aSurfaceSize)
{
  return nullptr;
}

CompositorManagerParent::CompositorManagerParent()
  : mCompositorThreadHolder(CompositorThreadHolder::GetSingleton())
{
}

CompositorManagerParent::~CompositorManagerParent()
{
}

void
CompositorManagerParent::Bind(Endpoint<PCompositorManagerParent>&& aEndpoint)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (NS_WARN_IF(!aEndpoint.Bind(this))) {
    return;
  }

  // Add the IPDL reference to ourself, so we can't get freed until IPDL is
  // done with us.
  AddRef();
}

void
CompositorManagerParent::ActorDestroy(ActorDestroyReason aReason)
{
}

void
CompositorManagerParent::DeallocPCompositorManagerParent()
{
  StaticMutexAutoLock lock(sMutex);
  if (sInstance == this) {
    sInstance = nullptr;
  }
  Release();
}

} // namespace layers
} // namespace mozilla

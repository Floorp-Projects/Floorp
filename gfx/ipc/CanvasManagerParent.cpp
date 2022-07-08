/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasManagerParent.h"
#include "mozilla/dom/WebGLParent.h"
#include "mozilla/gfx/CanvasRenderThread.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/webgpu/WebGPUParent.h"
#include "mozilla/webrender/RenderThread.h"
#include "nsIThread.h"

namespace mozilla::gfx {

CanvasManagerParent::ManagerSet CanvasManagerParent::sManagers;

/* static */ void CanvasManagerParent::Init(
    Endpoint<PCanvasManagerParent>&& aEndpoint) {
  MOZ_ASSERT(layers::CompositorThreadHolder::IsInCompositorThread());

  auto manager = MakeRefPtr<CanvasManagerParent>();

  if (!gfxVars::SupportsThreadsafeGL()) {
    nsCOMPtr<nsIThread> owningThread;
    owningThread = wr::RenderThread::GetRenderThread();
    MOZ_ASSERT(owningThread);

    owningThread->Dispatch(NewRunnableMethod<Endpoint<PCanvasManagerParent>&&>(
        "CanvasManagerParent::Bind", manager, &CanvasManagerParent::Bind,
        std::move(aEndpoint)));
  } else if (gfxVars::UseCanvasRenderThread()) {
    nsCOMPtr<nsIThread> owningThread;
    owningThread = gfx::CanvasRenderThread::GetCanvasRenderThread();
    MOZ_ASSERT(owningThread);

    owningThread->Dispatch(NewRunnableMethod<Endpoint<PCanvasManagerParent>&&>(
        "CanvasManagerParent::Bind", manager, &CanvasManagerParent::Bind,
        std::move(aEndpoint)));
  } else {
    manager->Bind(std::move(aEndpoint));
  }
}

/* static */ void CanvasManagerParent::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsISerialEventTarget> owningThread;
  if (!gfxVars::SupportsThreadsafeGL()) {
    owningThread = wr::RenderThread::GetRenderThread();
  } else if (gfxVars::UseCanvasRenderThread()) {
    owningThread = gfx::CanvasRenderThread::GetCanvasRenderThread();
  } else {
    owningThread = layers::CompositorThread();
  }
  if (!owningThread) {
    return;
  }

  owningThread->Dispatch(
      NS_NewRunnableFunction(
          "CanvasManagerParent::Shutdown",
          []() -> void { CanvasManagerParent::ShutdownInternal(); }),
      NS_DISPATCH_SYNC);
}

/* static */ void CanvasManagerParent::ShutdownInternal() {
  nsTArray<RefPtr<CanvasManagerParent>> actors(sManagers.Count());
  // Do a copy since Close will remove the entry from the set.
  for (const auto& actor : sManagers) {
    actors.AppendElement(actor);
  }

  for (auto const& actor : actors) {
    actor->Close();
  }
}

CanvasManagerParent::CanvasManagerParent() = default;
CanvasManagerParent::~CanvasManagerParent() = default;

void CanvasManagerParent::Bind(Endpoint<PCanvasManagerParent>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    NS_WARNING("Failed to bind CanvasManagerParent!");
    return;
  }

  sManagers.Insert(this);
}

void CanvasManagerParent::ActorDestroy(ActorDestroyReason aWhy) {
  sManagers.Remove(this);
}

already_AddRefed<dom::PWebGLParent> CanvasManagerParent::AllocPWebGLParent() {
  return MakeAndAddRef<dom::WebGLParent>();
}

already_AddRefed<webgpu::PWebGPUParent>
CanvasManagerParent::AllocPWebGPUParent() {
  if (!gfxVars::AllowWebGPU()) {
    return nullptr;
  }

  return MakeAndAddRef<webgpu::WebGPUParent>();
}

mozilla::ipc::IPCResult CanvasManagerParent::RecvInitialize(
    const uint32_t& aId) {
  if (!aId) {
    return IPC_FAIL(this, "invalid id");
  }
  if (mId) {
    return IPC_FAIL(this, "already initialized");
  }
  mId = aId;
  return IPC_OK();
}

mozilla::ipc::IPCResult CanvasManagerParent::RecvGetSnapshot(
    const uint32_t& aManagerId, const int32_t& aProtocolId,
    const CompositableHandle& aHandle, webgl::FrontBufferSnapshotIpc* aResult) {
  if (!aManagerId) {
    return IPC_FAIL(this, "invalid id");
  }

  IProtocol* actor = nullptr;
  for (CanvasManagerParent* i : sManagers) {
    if (i->OtherPidMaybeInvalid() == OtherPidMaybeInvalid() &&
        i->mId == aManagerId) {
      actor = i->Lookup(aProtocolId);
      break;
    }
  }

  if (!actor) {
    return IPC_FAIL(this, "invalid actor");
  }

  if (actor->GetSide() != mozilla::ipc::Side::ParentSide) {
    return IPC_FAIL(this, "unsupported actor");
  }

  webgl::FrontBufferSnapshotIpc buffer;
  switch (actor->GetProtocolId()) {
    case ProtocolId::PWebGLMsgStart: {
      RefPtr<dom::WebGLParent> webgl = static_cast<dom::WebGLParent*>(actor);
      mozilla::ipc::IPCResult rv = webgl->GetFrontBufferSnapshot(&buffer, this);
      if (!rv) {
        return rv;
      }
    } break;
    case ProtocolId::PWebGPUMsgStart: {
      RefPtr<webgpu::WebGPUParent> webgpu =
          static_cast<webgpu::WebGPUParent*>(actor);
      IntSize size;
      mozilla::ipc::IPCResult rv =
          webgpu->GetFrontBufferSnapshot(this, aHandle, buffer.shmem, size);
      if (!rv) {
        return rv;
      }
      buffer.surfSize.x = static_cast<uint32_t>(size.width);
      buffer.surfSize.y = static_cast<uint32_t>(size.height);
    } break;
    default:
      return IPC_FAIL(this, "unsupported protocol");
  }

  *aResult = std::move(buffer);
  return IPC_OK();
}

}  // namespace mozilla::gfx

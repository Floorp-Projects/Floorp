/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasManagerParent.h"
#include "mozilla/dom/WebGLParent.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/webrender/RenderThread.h"
#include "nsIThread.h"

namespace mozilla::gfx {

CanvasManagerParent::ManagerSet CanvasManagerParent::sManagers;

/* static */ void CanvasManagerParent::Init(
    Endpoint<PCanvasManagerParent>&& aEndpoint) {
  MOZ_ASSERT(layers::CompositorThreadHolder::IsInCompositorThread());

  auto manager = MakeRefPtr<CanvasManagerParent>();

  if (gfxVars::SupportsThreadsafeGL()) {
    manager->Bind(std::move(aEndpoint));
  } else {
    nsCOMPtr<nsIThread> owningThread;
    owningThread = wr::RenderThread::GetRenderThread();
    MOZ_ASSERT(owningThread);

    owningThread->Dispatch(NewRunnableMethod<Endpoint<PCanvasManagerParent>&&>(
        "CanvasManagerParent::Bind", manager, &CanvasManagerParent::Bind,
        std::move(aEndpoint)));
  }
}

/* static */ void CanvasManagerParent::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsISerialEventTarget> owningThread;
  if (gfxVars::SupportsThreadsafeGL()) {
    owningThread = layers::CompositorThread();
  } else {
    owningThread = wr::RenderThread::GetRenderThread();
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
    webgl::FrontBufferSnapshotIpc* aResult) {
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

  if (actor->GetProtocolId() != ProtocolId::PWebGLMsgStart ||
      actor->GetSide() != mozilla::ipc::Side::ParentSide) {
    return IPC_FAIL(this, "unsupported actor");
  }

  RefPtr<dom::WebGLParent> webgl = static_cast<dom::WebGLParent*>(actor);
  webgl::FrontBufferSnapshotIpc buffer;
  mozilla::ipc::IPCResult rv = webgl->GetFrontBufferSnapshot(&buffer, this);
  if (!rv) {
    return rv;
  }

  *aResult = std::move(buffer);
  return IPC_OK();
}

}  // namespace mozilla::gfx

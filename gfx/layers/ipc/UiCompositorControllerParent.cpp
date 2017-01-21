/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "UiCompositorControllerParent.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"

namespace mozilla {
namespace layers {

RefPtr<UiCompositorControllerParent>
UiCompositorControllerParent::Start(Endpoint<PUiCompositorControllerParent>&& aEndpoint)
{
  RefPtr<UiCompositorControllerParent> parent = new UiCompositorControllerParent();

  RefPtr<Runnable> task = NewRunnableMethod<Endpoint<PUiCompositorControllerParent>&&>(
    parent, &UiCompositorControllerParent::Open, Move(aEndpoint));
  CompositorThreadHolder::Loop()->PostTask(task.forget());

  return parent;
}

UiCompositorControllerParent::UiCompositorControllerParent()
{
  MOZ_COUNT_CTOR(UiCompositorControllerParent);
}

UiCompositorControllerParent::~UiCompositorControllerParent()
{
  MOZ_COUNT_DTOR(UiCompositorControllerParent);
}

void
UiCompositorControllerParent::Open(Endpoint<PUiCompositorControllerParent>&& aEndpoint)
{
  if (!aEndpoint.Bind(this)) {
    // We can't recover from this.
    MOZ_CRASH("Failed to bind UiCompositorControllerParent to endpoint");
  }
  AddRef();
}

mozilla::ipc::IPCResult
UiCompositorControllerParent::RecvPause(const uint64_t& aLayersId)
{
  CompositorBridgeParent* parent = CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(aLayersId);
  if (parent) {
    parent->PauseComposition();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
UiCompositorControllerParent::RecvResume(const uint64_t& aLayersId)
{
  CompositorBridgeParent* parent = CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(aLayersId);
  if (parent) {
    parent->ResumeComposition();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
UiCompositorControllerParent::RecvResumeAndResize(const uint64_t& aLayersId,
                                                  const int32_t& aHeight,
                                                  const int32_t& aWidth)
{
  CompositorBridgeParent* parent = CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(aLayersId);
  if (parent) {
    parent->ResumeCompositionAndResize(aHeight, aWidth);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
UiCompositorControllerParent::RecvInvalidateAndRender(const uint64_t& aLayersId)
{
  CompositorBridgeParent* parent = CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(aLayersId);
  if (parent) {
    parent->Invalidate();
    parent->ScheduleComposition();
  }
  return IPC_OK();
}

void
UiCompositorControllerParent::Shutdown()
{
  MessageLoop* ccloop = CompositorThreadHolder::Loop();
  if (MessageLoop::current() != ccloop) {
    ccloop->PostTask(NewRunnableMethod(this, &UiCompositorControllerParent::ShutdownImpl));
    return;
  }

  ShutdownImpl();
}

void
UiCompositorControllerParent::ShutdownImpl()
{
  Close();
}

void
UiCompositorControllerParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

void
UiCompositorControllerParent::DeallocPUiCompositorControllerParent()
{
  Release();
}

} // namespace layers
} // namespace mozilla

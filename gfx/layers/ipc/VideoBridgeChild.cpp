/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoBridgeChild.h"
#include "VideoBridgeParent.h"
#include "CompositorThread.h"
#include "mozilla/dom/ContentChild.h"
#include "mtransport/runnable_utils.h"
#include "SynchronousTask.h"

namespace mozilla {
namespace layers {

StaticRefPtr<VideoBridgeChild> sVideoBridge;

/* static */
void VideoBridgeChild::StartupForGPUProcess() {
  ipc::Endpoint<PVideoBridgeParent> parentPipe;
  ipc::Endpoint<PVideoBridgeChild> childPipe;

  PVideoBridge::CreateEndpoints(base::GetCurrentProcId(),
                                base::GetCurrentProcId(), &parentPipe,
                                &childPipe);

  VideoBridgeChild::Open(std::move(childPipe));
  VideoBridgeParent::Open(std::move(parentPipe), VideoBridgeSource::GpuProcess);
}

void VideoBridgeChild::Open(Endpoint<PVideoBridgeChild>&& aEndpoint) {
  MOZ_ASSERT(!sVideoBridge || !sVideoBridge->CanSend());
  sVideoBridge = new VideoBridgeChild();

  if (!aEndpoint.Bind(sVideoBridge)) {
    // We can't recover from this.
    MOZ_CRASH("Failed to bind VideoBridgeChild to endpoint");
  }
}

/* static */
void VideoBridgeChild::Shutdown() {
  if (sVideoBridge) {
    sVideoBridge->Close();
    sVideoBridge = nullptr;
  }
}

VideoBridgeChild::VideoBridgeChild()
    : mIPDLSelfRef(this), mThread(NS_GetCurrentThread()), mCanSend(true) {}

VideoBridgeChild::~VideoBridgeChild() = default;

VideoBridgeChild* VideoBridgeChild::GetSingleton() { return sVideoBridge; }

bool VideoBridgeChild::AllocUnsafeShmem(
    size_t aSize, ipc::SharedMemory::SharedMemoryType aType,
    ipc::Shmem* aShmem) {
  if (!mThread->IsOnCurrentThread()) {
    return DispatchAllocShmemInternal(aSize, aType, aShmem,
                                      true);  // true: unsafe
  }

  if (!CanSend()) {
    return false;
  }

  return PVideoBridgeChild::AllocUnsafeShmem(aSize, aType, aShmem);
}

bool VideoBridgeChild::AllocShmem(size_t aSize,
                                  ipc::SharedMemory::SharedMemoryType aType,
                                  ipc::Shmem* aShmem) {
  MOZ_ASSERT(CanSend());
  return PVideoBridgeChild::AllocShmem(aSize, aType, aShmem);
}

void VideoBridgeChild::ProxyAllocShmemNow(SynchronousTask* aTask, size_t aSize,
                                          SharedMemory::SharedMemoryType aType,
                                          ipc::Shmem* aShmem, bool aUnsafe,
                                          bool* aSuccess) {
  AutoCompleteTask complete(aTask);

  if (!CanSend()) {
    return;
  }

  bool ok = false;
  if (aUnsafe) {
    ok = AllocUnsafeShmem(aSize, aType, aShmem);
  } else {
    ok = AllocShmem(aSize, aType, aShmem);
  }
  *aSuccess = ok;
}

bool VideoBridgeChild::DispatchAllocShmemInternal(
    size_t aSize, SharedMemory::SharedMemoryType aType, ipc::Shmem* aShmem,
    bool aUnsafe) {
  SynchronousTask task("AllocatorProxy alloc");

  bool success = false;
  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<VideoBridgeChild>(this), &VideoBridgeChild::ProxyAllocShmemNow,
      &task, aSize, aType, aShmem, aUnsafe, &success);
  GetThread()->Dispatch(runnable.forget());

  task.Wait();

  return success;
}

void VideoBridgeChild::ProxyDeallocShmemNow(SynchronousTask* aTask,
                                            ipc::Shmem* aShmem, bool* aResult) {
  AutoCompleteTask complete(aTask);

  if (!CanSend()) {
    return;
  }
  *aResult = DeallocShmem(*aShmem);
}

bool VideoBridgeChild::DeallocShmem(ipc::Shmem& aShmem) {
  if (GetThread()->IsOnCurrentThread()) {
    if (!CanSend()) {
      return false;
    }
    return PVideoBridgeChild::DeallocShmem(aShmem);
  }

  SynchronousTask task("AllocatorProxy Dealloc");
  bool result = false;

  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<VideoBridgeChild>(this), &VideoBridgeChild::ProxyDeallocShmemNow,
      &task, &aShmem, &result);
  GetThread()->Dispatch(runnable.forget());

  task.Wait();
  return result;
}

PTextureChild* VideoBridgeChild::AllocPTextureChild(const SurfaceDescriptor&,
                                                    const ReadLockDescriptor&,
                                                    const LayersBackend&,
                                                    const TextureFlags&,
                                                    const uint64_t& aSerial) {
  MOZ_ASSERT(CanSend());
  return TextureClient::CreateIPDLActor();
}

bool VideoBridgeChild::DeallocPTextureChild(PTextureChild* actor) {
  return TextureClient::DestroyIPDLActor(actor);
}

void VideoBridgeChild::ActorDestroy(ActorDestroyReason aWhy) {
  mCanSend = false;
}

void VideoBridgeChild::ActorDealloc() { mIPDLSelfRef = nullptr; }

PTextureChild* VideoBridgeChild::CreateTexture(
    const SurfaceDescriptor& aSharedData, const ReadLockDescriptor& aReadLock,
    LayersBackend aLayersBackend, TextureFlags aFlags, uint64_t aSerial,
    wr::MaybeExternalImageId& aExternalImageId, nsIEventTarget* aTarget) {
  MOZ_ASSERT(CanSend());
  return SendPTextureConstructor(aSharedData, aReadLock, aLayersBackend, aFlags,
                                 aSerial);
}

bool VideoBridgeChild::IsSameProcess() const {
  return OtherPid() == base::GetCurrentProcId();
}

void VideoBridgeChild::HandleFatalError(const char* aMsg) const {
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aMsg, OtherPid());
}

}  // namespace layers
}  // namespace mozilla

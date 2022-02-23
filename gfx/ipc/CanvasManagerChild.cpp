/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasManagerChild.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/CompositorManagerChild.h"

using namespace mozilla::dom;
using namespace mozilla::layers;

namespace mozilla::gfx {

// The IPDL actor holds a strong reference to CanvasManagerChild which we use
// to keep it alive. The owning thread will tell us to close when it is
// shutdown, either via CanvasManagerChild::Shutdown for the main thread, or
// via a shutdown callback from IPCWorkerRef for worker threads.
MOZ_THREAD_LOCAL(CanvasManagerChild*) CanvasManagerChild::sLocalManager;

Atomic<uint32_t> CanvasManagerChild::sNextId(1);

CanvasManagerChild::CanvasManagerChild(uint32_t aId) : mId(aId) {}
CanvasManagerChild::~CanvasManagerChild() = default;

void CanvasManagerChild::ActorDestroy(ActorDestroyReason aReason) {
  if (sLocalManager.get() == this) {
    sLocalManager.set(nullptr);
  }
}

void CanvasManagerChild::Destroy() {
  // The caller has a strong reference. ActorDestroy will clear sLocalManager.
  Close();
  mWorkerRef = nullptr;
}

/* static */ void CanvasManagerChild::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  // The worker threads should destroy their own CanvasManagerChild instances
  // during their shutdown sequence. We just need to take care of the main
  // thread. We need to init here because we may have never created a
  // CanvasManagerChild for the main thread in the first place.
  if (sLocalManager.init()) {
    RefPtr<CanvasManagerChild> manager = sLocalManager.get();
    if (manager) {
      manager->Destroy();
    }
  }
}

/* static */ bool CanvasManagerChild::CreateParent(
    ipc::Endpoint<PCanvasManagerParent>&& aEndpoint) {
  MOZ_ASSERT(NS_IsMainThread());

  CompositorManagerChild* manager = CompositorManagerChild::GetInstance();
  if (!manager || !manager->CanSend()) {
    return false;
  }

  return manager->SendInitCanvasManager(std::move(aEndpoint));
}

/* static */ CanvasManagerChild* CanvasManagerChild::Get() {
  if (NS_WARN_IF(!sLocalManager.init())) {
    return nullptr;
  }

  CanvasManagerChild* managerWeak = sLocalManager.get();
  if (managerWeak) {
    return managerWeak;
  }

  // We are only used on the main thread, or on worker threads.
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT_IF(!worker, NS_IsMainThread());

  ipc::Endpoint<PCanvasManagerParent> parentEndpoint;
  ipc::Endpoint<PCanvasManagerChild> childEndpoint;

  auto compositorPid = CompositorManagerChild::GetOtherPid();
  if (NS_WARN_IF(!compositorPid)) {
    return nullptr;
  }

  nsresult rv = PCanvasManager::CreateEndpoints(
      compositorPid, base::GetCurrentProcId(), &parentEndpoint, &childEndpoint);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  auto manager = MakeRefPtr<CanvasManagerChild>(sNextId++);

  if (worker) {
    // The IPCWorkerRef will let us know when the worker is shutting down. This
    // will let us clear our threadlocal reference and close the actor. We rely
    // upon an explicit shutdown for the main thread.
    manager->mWorkerRef = IPCWorkerRef::Create(
        worker, "CanvasManager", [manager]() { manager->Destroy(); });
    if (NS_WARN_IF(!manager->mWorkerRef)) {
      return nullptr;
    }
  }

  if (NS_WARN_IF(!childEndpoint.Bind(manager))) {
    return nullptr;
  }

  // We can't talk to the compositor process directly from worker threads, but
  // the main thread can via CompositorManagerChild.
  if (worker) {
    worker->DispatchToMainThread(NS_NewRunnableFunction(
        "CanvasManagerChild::CreateParent",
        [parentEndpoint = std::move(parentEndpoint)]() {
          CreateParent(
              std::move(const_cast<ipc::Endpoint<PCanvasManagerParent>&&>(
                  parentEndpoint)));
        }));
  } else if (NS_WARN_IF(!CreateParent(std::move(parentEndpoint)))) {
    return nullptr;
  }

  manager->SendInitialize(manager->Id());
  sLocalManager.set(manager);
  return manager;
}

already_AddRefed<DataSourceSurface> CanvasManagerChild::GetSnapshot(
    uint32_t aManagerId, int32_t aProtocolId, bool aHasAlpha) {
  if (!CanSend()) {
    return nullptr;
  }

  webgl::FrontBufferSnapshotIpc res;
  if (!SendGetSnapshot(aManagerId, aProtocolId, &res)) {
    return nullptr;
  }

  if (!res.shmem || !res.shmem->IsReadable()) {
    return nullptr;
  }

  auto guard = MakeScopeExit([&] { DeallocShmem(res.shmem.ref()); });

  if (!res.surfSize.x || !res.surfSize.y || res.surfSize.x > INT32_MAX ||
      res.surfSize.y > INT32_MAX) {
    return nullptr;
  }

  IntSize size(res.surfSize.x, res.surfSize.y);
  CheckedInt32 stride = CheckedInt32(size.width) * sizeof(uint32_t);
  if (!stride.isValid()) {
    return nullptr;
  }

  CheckedInt32 length = stride * size.height;
  if (!length.isValid() ||
      size_t(length.value()) != res.shmem->Size<uint8_t>()) {
    return nullptr;
  }

  RefPtr<DataSourceSurface> surface =
      Factory::CreateDataSourceSurfaceWithStride(size, SurfaceFormat::B8G8R8A8,
                                                 stride.value(),
                                                 /* aZero */ false);
  if (!surface) {
    return nullptr;
  }

  gfx::DataSourceSurface::ScopedMap map(surface,
                                        gfx::DataSourceSurface::READ_WRITE);
  if (!map.IsMapped()) {
    return nullptr;
  }

  memcpy(map.GetData(), res.shmem->get<uint8_t>(), res.shmem->Size<uint8_t>());
  return surface.forget();
}

}  // namespace mozilla::gfx

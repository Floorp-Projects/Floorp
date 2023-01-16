/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfacesParent.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/StaticPrefs_image.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/layers/SharedSurfacesMemoryReport.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/webrender/RenderSharedSurfaceTextureHost.h"
#include "mozilla/webrender/RenderThread.h"
#include "nsThreadUtils.h"  // for GetCurrentSerialEventTarget

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

StaticMonitor SharedSurfacesParent::sMonitor;
StaticAutoPtr<SharedSurfacesParent> SharedSurfacesParent::sInstance;

// Short wait to allow for a surface to be added, where the consumer has a
// different thread route.
static const TimeDuration kGetTimeout = TimeDuration::FromMilliseconds(50);

void SharedSurfacesParent::MappingTracker::NotifyExpiredLocked(
    SourceSurfaceSharedDataWrapper* aSurface,
    const StaticMonitorAutoLock& aAutoLock) {
  RemoveObjectLocked(aSurface, aAutoLock);
  mExpired.AppendElement(aSurface);
}

void SharedSurfacesParent::MappingTracker::TakeExpired(
    nsTArray<RefPtr<gfx::SourceSurfaceSharedDataWrapper>>& aExpired,
    const StaticMonitorAutoLock& aAutoLock) {
  aExpired = std::move(mExpired);
}

void SharedSurfacesParent::MappingTracker::NotifyHandlerEnd() {
  nsTArray<RefPtr<gfx::SourceSurfaceSharedDataWrapper>> expired;
  {
    StaticMonitorAutoLock lock(sMonitor);
    TakeExpired(expired, lock);
  }

  SharedSurfacesParent::ExpireMap(expired);
}

SharedSurfacesParent::SharedSurfacesParent()
    : mTracker(
          StaticPrefs::image_mem_shared_unmap_min_expiration_ms_AtStartup(),
          mozilla::GetCurrentSerialEventTarget()) {}

/* static */
void SharedSurfacesParent::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());
  StaticMonitorAutoLock lock(sMonitor);
  if (!sInstance) {
    sInstance = new SharedSurfacesParent();
  }
}

/* static */
void SharedSurfacesParent::ShutdownRenderThread() {
  // The main thread should blocked on waiting for the render thread to
  // complete so this should be safe to release off the main thread.
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());
  StaticMonitorAutoLock lock(sMonitor);
  MOZ_ASSERT(sInstance);

  for (const auto& key : sInstance->mSurfaces.Keys()) {
    // There may be lingering consumers of the surfaces that didn't get shutdown
    // yet but since we are here, we know the render thread is finished and we
    // can unregister everything.
    wr::RenderThread::Get()->UnregisterExternalImageDuringShutdown(
        wr::ToExternalImageId(key));
  }
}

/* static */
void SharedSurfacesParent::Shutdown() {
  // The compositor thread and render threads are shutdown, so this is the last
  // thread that could use it. The expiration tracker needs to be freed on the
  // main thread.
  MOZ_ASSERT(NS_IsMainThread());
  StaticMonitorAutoLock lock(sMonitor);
  sInstance = nullptr;
}

/* static */
already_AddRefed<DataSourceSurface> SharedSurfacesParent::Get(
    const wr::ExternalImageId& aId) {
  StaticMonitorAutoLock lock(sMonitor);
  if (!sInstance) {
    gfxCriticalNote << "SSP:Get " << wr::AsUint64(aId) << " shtd";
    return nullptr;
  }

  RefPtr<SourceSurfaceSharedDataWrapper> surface;
  while (
      !sInstance->mSurfaces.Get(wr::AsUint64(aId), getter_AddRefs(surface))) {
    CVStatus status = lock.Wait(kGetTimeout);
    if (status == CVStatus::Timeout) {
      return nullptr;
    }
  }
  return surface.forget();
}

/* static */
already_AddRefed<DataSourceSurface> SharedSurfacesParent::Acquire(
    const wr::ExternalImageId& aId) {
  StaticMonitorAutoLock lock(sMonitor);
  if (!sInstance) {
    gfxCriticalNote << "SSP:Acq " << wr::AsUint64(aId) << " shtd";
    return nullptr;
  }

  RefPtr<SourceSurfaceSharedDataWrapper> surface;
  sInstance->mSurfaces.Get(wr::AsUint64(aId), getter_AddRefs(surface));

  if (surface) {
    DebugOnly<bool> rv = surface->AddConsumer();
    MOZ_ASSERT(!rv);
  }
  return surface.forget();
}

/* static */
bool SharedSurfacesParent::Release(const wr::ExternalImageId& aId,
                                   bool aForCreator) {
  StaticMonitorAutoLock lock(sMonitor);
  if (!sInstance) {
    return false;
  }

  uint64_t id = wr::AsUint64(aId);
  RefPtr<SourceSurfaceSharedDataWrapper> surface;
  sInstance->mSurfaces.Get(wr::AsUint64(aId), getter_AddRefs(surface));
  if (!surface) {
    return false;
  }

  if (surface->RemoveConsumer(aForCreator)) {
    RemoveTrackingLocked(surface, lock);
    wr::RenderThread::Get()->UnregisterExternalImage(wr::ToExternalImageId(id));
    sInstance->mSurfaces.Remove(id);
  }

  return true;
}

/* static */
void SharedSurfacesParent::AddSameProcess(const wr::ExternalImageId& aId,
                                          SourceSurfaceSharedData* aSurface) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  StaticMonitorAutoLock lock(sMonitor);
  if (!sInstance) {
    gfxCriticalNote << "SSP:Ads " << wr::AsUint64(aId) << " shtd";
    return;
  }

  // If the child bridge detects it is in the combined UI/GPU process, then it
  // will insert a wrapper surface holding the shared memory buffer directly.
  // This is good because we avoid mapping the same shared memory twice, but
  // still allow the original surface to be freed and remove the wrapper from
  // the table when it is no longer needed.
  RefPtr<SourceSurfaceSharedDataWrapper> surface =
      new SourceSurfaceSharedDataWrapper();
  surface->Init(aSurface);

  uint64_t id = wr::AsUint64(aId);
  MOZ_ASSERT(!sInstance->mSurfaces.Contains(id));

  auto texture = MakeRefPtr<wr::RenderSharedSurfaceTextureHost>(surface);
  wr::RenderThread::Get()->RegisterExternalImage(aId, texture.forget());

  surface->AddConsumer();
  sInstance->mSurfaces.InsertOrUpdate(id, std::move(surface));
  lock.NotifyAll();
}

/* static */
void SharedSurfacesParent::DestroyProcess(base::ProcessId aPid) {
  StaticMonitorAutoLock lock(sMonitor);
  if (!sInstance) {
    return;
  }

  // Note that the destruction of a parent may not be cheap if it still has a
  // lot of surfaces still bound that require unmapping.
  for (auto i = sInstance->mSurfaces.Iter(); !i.Done(); i.Next()) {
    SourceSurfaceSharedDataWrapper* surface = i.Data();
    if (surface->GetCreatorPid() == aPid && surface->HasCreatorRef() &&
        surface->RemoveConsumer(/* aForCreator */ true)) {
      RemoveTrackingLocked(surface, lock);
      wr::RenderThread::Get()->UnregisterExternalImage(
          wr::ToExternalImageId(i.Key()));
      i.Remove();
    }
  }
}

/* static */
void SharedSurfacesParent::Add(const wr::ExternalImageId& aId,
                               SurfaceDescriptorShared&& aDesc,
                               base::ProcessId aPid) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(aPid != base::GetCurrentProcId());

  RefPtr<SourceSurfaceSharedDataWrapper> surface =
      new SourceSurfaceSharedDataWrapper();

  // We preferentially map in new surfaces when they are initially received
  // because we are likely to reference them in a display list soon. The unmap
  // will ensure we add the surface to the expiration tracker. We do it outside
  // the mutex to ensure we always lock the surface mutex first, and our mutex
  // second, to avoid deadlock.
  //
  // Note that the surface wrapper maps in the given handle as read only.
  surface->Init(aDesc.size(), aDesc.stride(), aDesc.format(),
                std::move(aDesc.handle()), aPid);

  StaticMonitorAutoLock lock(sMonitor);
  if (!sInstance) {
    gfxCriticalNote << "SSP:Add " << wr::AsUint64(aId) << " shtd";
    return;
  }

  uint64_t id = wr::AsUint64(aId);
  MOZ_ASSERT(!sInstance->mSurfaces.Contains(id));

  auto texture = MakeRefPtr<wr::RenderSharedSurfaceTextureHost>(surface);
  wr::RenderThread::Get()->RegisterExternalImage(aId, texture.forget());

  surface->AddConsumer();
  sInstance->mSurfaces.InsertOrUpdate(id, std::move(surface));
  lock.NotifyAll();
}

/* static */
void SharedSurfacesParent::Remove(const wr::ExternalImageId& aId) {
  DebugOnly<bool> rv = Release(aId, /* aForCreator */ true);
  MOZ_ASSERT(rv);
}

/* static */
void SharedSurfacesParent::AddTrackingLocked(
    SourceSurfaceSharedDataWrapper* aSurface,
    const StaticMonitorAutoLock& aAutoLock) {
  MOZ_ASSERT(!aSurface->GetExpirationState()->IsTracked());
  sInstance->mTracker.AddObjectLocked(aSurface, aAutoLock);
}

/* static */
void SharedSurfacesParent::AddTracking(
    SourceSurfaceSharedDataWrapper* aSurface) {
  StaticMonitorAutoLock lock(sMonitor);
  if (!sInstance) {
    return;
  }

  AddTrackingLocked(aSurface, lock);
}

/* static */
void SharedSurfacesParent::RemoveTrackingLocked(
    SourceSurfaceSharedDataWrapper* aSurface,
    const StaticMonitorAutoLock& aAutoLock) {
  if (!aSurface->GetExpirationState()->IsTracked()) {
    return;
  }

  sInstance->mTracker.RemoveObjectLocked(aSurface, aAutoLock);
}

/* static */
void SharedSurfacesParent::RemoveTracking(
    SourceSurfaceSharedDataWrapper* aSurface) {
  StaticMonitorAutoLock lock(sMonitor);
  if (!sInstance) {
    return;
  }

  RemoveTrackingLocked(aSurface, lock);
}

/* static */
bool SharedSurfacesParent::AgeOneGenerationLocked(
    nsTArray<RefPtr<SourceSurfaceSharedDataWrapper>>& aExpired,
    const StaticMonitorAutoLock& aAutoLock) {
  if (sInstance->mTracker.IsEmptyLocked(aAutoLock)) {
    return false;
  }

  sInstance->mTracker.AgeOneGenerationLocked(aAutoLock);
  sInstance->mTracker.TakeExpired(aExpired, aAutoLock);
  return true;
}

/* static */
bool SharedSurfacesParent::AgeOneGeneration(
    nsTArray<RefPtr<SourceSurfaceSharedDataWrapper>>& aExpired) {
  StaticMonitorAutoLock lock(sMonitor);
  if (!sInstance) {
    return false;
  }

  return AgeOneGenerationLocked(aExpired, lock);
}

/* static */
bool SharedSurfacesParent::AgeAndExpireOneGeneration() {
  nsTArray<RefPtr<SourceSurfaceSharedDataWrapper>> expired;
  bool aged = AgeOneGeneration(expired);
  ExpireMap(expired);
  return aged;
}

/* static */
void SharedSurfacesParent::ExpireMap(
    nsTArray<RefPtr<SourceSurfaceSharedDataWrapper>>& aExpired) {
  for (auto& surface : aExpired) {
    surface->ExpireMap();
  }
}

/* static */
void SharedSurfacesParent::AccumulateMemoryReport(
    base::ProcessId aPid, SharedSurfacesMemoryReport& aReport) {
  StaticMonitorAutoLock lock(sMonitor);
  if (!sInstance) {
    return;
  }

  for (const auto& entry : sInstance->mSurfaces) {
    SourceSurfaceSharedDataWrapper* surface = entry.GetData();
    if (surface->GetCreatorPid() == aPid) {
      aReport.mSurfaces.insert(std::make_pair(
          entry.GetKey(),
          SharedSurfacesMemoryReport::SurfaceEntry{
              aPid, surface->GetSize(), surface->Stride(),
              surface->GetConsumers(), surface->HasCreatorRef()}));
    }
  }
}

/* static */
bool SharedSurfacesParent::AccumulateMemoryReport(
    SharedSurfacesMemoryReport& aReport) {
  if (XRE_IsParentProcess()) {
    GPUProcessManager* gpm = GPUProcessManager::Get();
    if (!gpm || gpm->GPUProcessPid() != base::kInvalidProcessId) {
      return false;
    }
  } else if (!XRE_IsGPUProcess()) {
    return false;
  }

  StaticMonitorAutoLock lock(sMonitor);
  if (!sInstance) {
    return true;
  }

  for (const auto& entry : sInstance->mSurfaces) {
    SourceSurfaceSharedDataWrapper* surface = entry.GetData();
    aReport.mSurfaces.insert(std::make_pair(
        entry.GetKey(),
        SharedSurfacesMemoryReport::SurfaceEntry{
            surface->GetCreatorPid(), surface->GetSize(), surface->Stride(),
            surface->GetConsumers(), surface->HasCreatorRef()}));
  }

  return true;
}

}  // namespace layers
}  // namespace mozilla

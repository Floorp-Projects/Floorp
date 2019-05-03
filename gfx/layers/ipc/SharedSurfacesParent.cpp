/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfacesParent.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/layers/SharedSurfacesMemoryReport.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/webrender/RenderSharedSurfaceTextureHost.h"
#include "mozilla/webrender/RenderThread.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

StaticMutex SharedSurfacesParent::sMutex;
StaticAutoPtr<SharedSurfacesParent> SharedSurfacesParent::sInstance;

SharedSurfacesParent::SharedSurfacesParent() {}

SharedSurfacesParent::~SharedSurfacesParent() {
  for (auto i = mSurfaces.Iter(); !i.Done(); i.Next()) {
    // There may be lingering consumers of the surfaces that didn't get shutdown
    // yet but since we are here, we know the render thread is finished and we
    // can unregister everything.
    wr::RenderThread::Get()->UnregisterExternalImageDuringShutdown(i.Key());
  }
}

/* static */
void SharedSurfacesParent::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());
  StaticMutexAutoLock lock(sMutex);
  if (!sInstance) {
    sInstance = new SharedSurfacesParent();
  }
}

/* static */
void SharedSurfacesParent::Shutdown() {
  // The main thread should blocked on waiting for the render thread to
  // complete so this should be safe to release off the main thread.
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());
  StaticMutexAutoLock lock(sMutex);
  sInstance = nullptr;
}

/* static */
already_AddRefed<DataSourceSurface> SharedSurfacesParent::Get(
    const wr::ExternalImageId& aId) {
  StaticMutexAutoLock lock(sMutex);
  if (!sInstance) {
    gfxCriticalNote << "SSP:Get " << aId.mHandle << " shtd";
    return nullptr;
  }

  RefPtr<SourceSurfaceSharedDataWrapper> surface;
  sInstance->mSurfaces.Get(wr::AsUint64(aId), getter_AddRefs(surface));
  return surface.forget();
}

/* static */
already_AddRefed<DataSourceSurface> SharedSurfacesParent::Acquire(
    const wr::ExternalImageId& aId) {
  StaticMutexAutoLock lock(sMutex);
  if (!sInstance) {
    gfxCriticalNote << "SSP:Acq " << aId.mHandle << " shtd";
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
  StaticMutexAutoLock lock(sMutex);
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
    wr::RenderThread::Get()->UnregisterExternalImage(id);
    sInstance->mSurfaces.Remove(id);
  }

  return true;
}

/* static */
void SharedSurfacesParent::AddSameProcess(const wr::ExternalImageId& aId,
                                          SourceSurfaceSharedData* aSurface) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  StaticMutexAutoLock lock(sMutex);
  if (!sInstance) {
    gfxCriticalNote << "SSP:Ads " << aId.mHandle << " shtd";
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

  RefPtr<wr::RenderSharedSurfaceTextureHost> texture =
      new wr::RenderSharedSurfaceTextureHost(surface);
  wr::RenderThread::Get()->RegisterExternalImage(id, texture.forget());

  surface->AddConsumer();
  sInstance->mSurfaces.Put(id, surface);
}

/* static */
void SharedSurfacesParent::RemoveSameProcess(const wr::ExternalImageId& aId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  Release(aId, /* aForCreator */ true);
}

/* static */
void SharedSurfacesParent::DestroyProcess(base::ProcessId aPid) {
  StaticMutexAutoLock lock(sMutex);
  if (!sInstance) {
    return;
  }

  // Note that the destruction of a parent may not be cheap if it still has a
  // lot of surfaces still bound that require unmapping.
  for (auto i = sInstance->mSurfaces.Iter(); !i.Done(); i.Next()) {
    SourceSurfaceSharedDataWrapper* surface = i.Data();
    if (surface->GetCreatorPid() == aPid && surface->HasCreatorRef() &&
        surface->RemoveConsumer(/* aForCreator */ true)) {
      wr::RenderThread::Get()->UnregisterExternalImage(i.Key());
      i.Remove();
    }
  }
}

/* static */
void SharedSurfacesParent::Add(const wr::ExternalImageId& aId,
                               const SurfaceDescriptorShared& aDesc,
                               base::ProcessId aPid) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(aPid != base::GetCurrentProcId());
  StaticMutexAutoLock lock(sMutex);
  if (!sInstance) {
    gfxCriticalNote << "SSP:Add " << aId.mHandle << " shtd";
    return;
  }

  // Note that the surface wrapper maps in the given handle as read only.
  RefPtr<SourceSurfaceSharedDataWrapper> surface =
      new SourceSurfaceSharedDataWrapper();
  if (NS_WARN_IF(!surface->Init(aDesc.size(), aDesc.stride(), aDesc.format(),
                                aDesc.handle(), aPid))) {
    gfxCriticalNote << "SSP:Add " << aId.mHandle << " init";
    return;
  }

  uint64_t id = wr::AsUint64(aId);
  MOZ_ASSERT(!sInstance->mSurfaces.Contains(id));

  RefPtr<wr::RenderSharedSurfaceTextureHost> texture =
      new wr::RenderSharedSurfaceTextureHost(surface);
  wr::RenderThread::Get()->RegisterExternalImage(id, texture.forget());

  surface->AddConsumer();
  sInstance->mSurfaces.Put(id, surface.forget());
}

/* static */
void SharedSurfacesParent::Remove(const wr::ExternalImageId& aId) {
  DebugOnly<bool> rv = Release(aId, /* aForCreator */ true);
  MOZ_ASSERT(rv);
}

/* static */
void SharedSurfacesParent::AccumulateMemoryReport(
    base::ProcessId aPid, SharedSurfacesMemoryReport& aReport) {
  StaticMutexAutoLock lock(sMutex);
  if (!sInstance) {
    return;
  }

  for (auto i = sInstance->mSurfaces.ConstIter(); !i.Done(); i.Next()) {
    SourceSurfaceSharedDataWrapper* surface = i.Data();
    if (surface->GetCreatorPid() == aPid) {
      aReport.mSurfaces.insert(std::make_pair(
          i.Key(), SharedSurfacesMemoryReport::SurfaceEntry{
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
    if (!gpm || gpm->GPUProcessPid() != -1) {
      return false;
    }
  } else if (!XRE_IsGPUProcess()) {
    return false;
  }

  StaticMutexAutoLock lock(sMutex);
  if (!sInstance) {
    return true;
  }

  for (auto i = sInstance->mSurfaces.ConstIter(); !i.Done(); i.Next()) {
    SourceSurfaceSharedDataWrapper* surface = i.Data();
    aReport.mSurfaces.insert(std::make_pair(
        i.Key(),
        SharedSurfacesMemoryReport::SurfaceEntry{
            surface->GetCreatorPid(), surface->GetSize(), surface->Stride(),
            surface->GetConsumers(), surface->HasCreatorRef()}));
  }

  return true;
}

}  // namespace layers
}  // namespace mozilla

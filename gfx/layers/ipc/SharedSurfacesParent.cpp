/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfacesParent.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/webrender/RenderSharedSurfaceTextureHost.h"
#include "mozilla/webrender/RenderThread.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

StaticAutoPtr<SharedSurfacesParent> SharedSurfacesParent::sInstance;

SharedSurfacesParent::SharedSurfacesParent()
{
}

SharedSurfacesParent::~SharedSurfacesParent()
{
}

/* static */ void
SharedSurfacesParent::Initialize()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    sInstance = new SharedSurfacesParent();
  }
}

/* static */ void
SharedSurfacesParent::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  sInstance = nullptr;
}

/* static */ already_AddRefed<DataSourceSurface>
SharedSurfacesParent::Get(const wr::ExternalImageId& aId)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!sInstance) {
    return nullptr;
  }

  RefPtr<SourceSurfaceSharedDataWrapper> surface;
  sInstance->mSurfaces.Get(wr::AsUint64(aId), getter_AddRefs(surface));
  return surface.forget();
}

/* static */ already_AddRefed<DataSourceSurface>
SharedSurfacesParent::Acquire(const wr::ExternalImageId& aId)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!sInstance) {
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

/* static */ bool
SharedSurfacesParent::Release(const wr::ExternalImageId& aId)
{
  //MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!sInstance) {
    return false;
  }

  uint64_t id = wr::AsUint64(aId);
  RefPtr<SourceSurfaceSharedDataWrapper> surface;
  sInstance->mSurfaces.Get(wr::AsUint64(aId), getter_AddRefs(surface));
  if (!surface) {
    return false;
  }

  if (surface->RemoveConsumer()) {
    sInstance->mSurfaces.Remove(id);
  }

  return true;
}

/* static */ void
SharedSurfacesParent::AddSameProcess(const wr::ExternalImageId& aId,
                                     SourceSurfaceSharedData* aSurface)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  // If the child bridge detects it is in the combined UI/GPU process, then it
  // will insert a wrapper surface holding the shared memory buffer directly.
  // This is good because we avoid mapping the same shared memory twice, but
  // still allow the original surface to be freed and remove the wrapper from
  // the table when it is no longer needed.
  RefPtr<SourceSurfaceSharedDataWrapper> surface =
    new SourceSurfaceSharedDataWrapper();
  surface->Init(aSurface);

  uint64_t id = wr::AsUint64(aId);
  RefPtr<Runnable> task = NS_NewRunnableFunction(
    "layers::SharedSurfacesParent::AddSameProcess",
    [surface, id]() -> void {
      if (!sInstance) {
        return;
      }

      MOZ_ASSERT(!sInstance->mSurfaces.Contains(id));

      RefPtr<wr::RenderSharedSurfaceTextureHost> texture =
        new wr::RenderSharedSurfaceTextureHost(surface);
      wr::RenderThread::Get()->RegisterExternalImage(id, texture.forget());

      sInstance->mSurfaces.Put(id, surface);
    });

  CompositorThreadHolder::Loop()->PostTask(task.forget());
}

/* static */ void
SharedSurfacesParent::RemoveSameProcess(const wr::ExternalImageId& aId)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  const wr::ExternalImageId id(aId);
  RefPtr<Runnable> task = NS_NewRunnableFunction(
    "layers::SharedSurfacesParent::RemoveSameProcess",
    [id]() -> void {
      Remove(id);
    });

  CompositorThreadHolder::Loop()->PostTask(task.forget());
}

/* static */ void
SharedSurfacesParent::DestroyProcess(base::ProcessId aPid)
{
  if (!sInstance) {
    return;
  }

  // Note that the destruction of a parent may not be cheap if it still has a
  // lot of surfaces still bound that require unmapping.
  for (auto i = sInstance->mSurfaces.Iter(); !i.Done(); i.Next()) {
    if (i.Data()->GetCreatorPid() == aPid) {
      wr::RenderThread::Get()->UnregisterExternalImage(i.Key());
      i.Remove();
    }
  }
}

/* static */ void
SharedSurfacesParent::Add(const wr::ExternalImageId& aId,
                          const SurfaceDescriptorShared& aDesc,
                          base::ProcessId aPid)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(aPid != base::GetCurrentProcId());
  if (!sInstance) {
    return;
  }

  // Note that the surface wrapper maps in the given handle as read only.
  RefPtr<SourceSurfaceSharedDataWrapper> surface =
    new SourceSurfaceSharedDataWrapper();
  if (NS_WARN_IF(!surface->Init(aDesc.size(), aDesc.stride(),
                                aDesc.format(), aDesc.handle(),
                                aPid))) {
    return;
  }

  uint64_t id = wr::AsUint64(aId);
  MOZ_ASSERT(!sInstance->mSurfaces.Contains(id));

  RefPtr<wr::RenderSharedSurfaceTextureHost> texture =
    new wr::RenderSharedSurfaceTextureHost(surface);
  wr::RenderThread::Get()->RegisterExternalImage(id, texture.forget());

  sInstance->mSurfaces.Put(id, surface.forget());
}

/* static */ void
SharedSurfacesParent::Remove(const wr::ExternalImageId& aId)
{
  //MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  DebugOnly<bool> rv = Release(aId);
  MOZ_ASSERT(rv);
}

} // namespace layers
} // namespace mozilla

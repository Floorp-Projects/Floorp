/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayerTransaction.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/CompositorTypes.h"

#include "gfxPlatform.h"

#include "gfxSharedQuartzSurface.h"

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

bool
ISurfaceAllocator::PlatformAllocSurfaceDescriptor(const gfxIntSize& aSize,
                                                  gfxASurface::gfxContentType aContent,
                                                  uint32_t aCaps,
                                                  SurfaceDescriptor* aBuffer)
{
  return false;
}

/*static*/ already_AddRefed<gfxASurface>
ShadowLayerForwarder::PlatformOpenDescriptor(OpenMode aMode,
                                             const SurfaceDescriptor& aSurface)
{
  if (SurfaceDescriptor::TShmem != aSurface.type()) {
    return nullptr;
  }
  return gfxSharedQuartzSurface::Open(aSurface.get_Shmem());
}

/*static*/ bool
ShadowLayerForwarder::PlatformCloseDescriptor(const SurfaceDescriptor& aDescriptor)
{
  return false;
}

/*static*/ bool
ShadowLayerForwarder::PlatformGetDescriptorSurfaceContentType(
  const SurfaceDescriptor& aDescriptor, OpenMode aMode,
  gfxContentType* aContent,
  gfxASurface** aSurface)
{
  return false;
}

/*static*/ bool
ShadowLayerForwarder::PlatformGetDescriptorSurfaceSize(
  const SurfaceDescriptor& aDescriptor, OpenMode aMode,
  gfxIntSize* aSize,
  gfxASurface** aSurface)
{
  return false;
}

bool
ShadowLayerForwarder::PlatformDestroySharedSurface(SurfaceDescriptor* aSurface)
{
  return false;
}

/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
}

/*static*/ void
LayerManagerComposite::PlatformSyncBeforeReplyUpdate()
{
}

bool
ISurfaceAllocator::PlatformDestroySharedSurface(SurfaceDescriptor*)
{
  return false;
}

/*static*/ already_AddRefed<TextureImage>
LayerManagerComposite::OpenDescriptorForDirectTexturing(GLContext*,
                                                        const SurfaceDescriptor&,
                                                        GLenum)
{
  return nullptr;
}

/*static*/ bool
LayerManagerComposite::SupportsDirectTexturing()
{
  return false;
}


} // namespace layers
} // namespace mozilla

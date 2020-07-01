/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceDMABUF.h"

#include "gfxPlatformGtk.h"
#include "GLContextEGL.h"
#include "MozFramebuffer.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc

namespace mozilla::gl {

/*static*/
UniquePtr<SharedSurface_DMABUF> SharedSurface_DMABUF::Create(
    const SharedSurfaceDesc& desc) {
  const auto flags = static_cast<DMABufSurfaceFlags>(
      DMABUF_TEXTURE | DMABUF_USE_MODIFIERS | DMABUF_ALPHA);
  const RefPtr<DMABufSurface> surface = DMABufSurfaceRGBA::CreateDMABufSurface(
      desc.size.width, desc.size.height, flags);
  if (!surface || !surface->CreateTexture(desc.gl)) {
    return nullptr;
  }

  const auto tex = surface->GetTexture();
  auto fb = MozFramebuffer::CreateForBacking(desc.gl, desc.size, 0, false,
                                             LOCAL_GL_TEXTURE_2D, tex);
  if (!fb) return nullptr;

  return AsUnique(new SharedSurface_DMABUF(desc, std::move(fb), surface));
}

SharedSurface_DMABUF::SharedSurface_DMABUF(const SharedSurfaceDesc& desc,
                                           UniquePtr<MozFramebuffer> fb,
                                           const RefPtr<DMABufSurface> surface)
    : SharedSurface(desc, std::move(fb)), mSurface(surface) {}

SharedSurface_DMABUF::~SharedSurface_DMABUF() {
  const auto& gl = mDesc.gl;
  if (!gl || !gl->MakeCurrent()) {
    return;
  }
  mSurface->ReleaseTextures();
}

void SharedSurface_DMABUF::ProducerReleaseImpl() { mSurface->FenceSet(); }

Maybe<layers::SurfaceDescriptor> SharedSurface_DMABUF::ToSurfaceDescriptor() {
  layers::SurfaceDescriptor desc;
  if (!mSurface->Serialize(desc)) return {};
  return Some(desc);
}

/*static*/
UniquePtr<SurfaceFactory_DMABUF> SurfaceFactory_DMABUF::Create(GLContext& gl) {
  if (!gfxPlatformGtk::GetPlatform()->UseDMABufWebGL()) {
    return nullptr;
  }

  auto dmabufFactory = MakeUnique<SurfaceFactory_DMABUF>(gl);
  if (dmabufFactory->CanCreateSurface()) {
    return dmabufFactory;
  }

  gfxPlatformGtk::GetPlatform()->DisableDMABufWebGL();
  return nullptr;
}

SurfaceFactory_DMABUF::SurfaceFactory_DMABUF(GLContext& gl)
    : SurfaceFactory({&gl, SharedSurfaceType::EGLSurfaceDMABUF,
                      layers::TextureType::DMABUF, true}) {}

}  // namespace mozilla::gl

/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceDMABUF.h"
#include "GLContextEGL.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc

namespace mozilla::gl {

/*static*/
UniquePtr<SharedSurface_DMABUF> SharedSurface_DMABUF::Create(
    GLContext* prodGL, const GLFormats& formats, const gfx::IntSize& size,
    bool hasAlpha) {
  auto flags = static_cast<WaylandDMABufSurfaceFlags>(DMABUF_TEXTURE |
                                                      DMABUF_USE_MODIFIERS);
  if (hasAlpha) {
    flags = static_cast<WaylandDMABufSurfaceFlags>(flags | DMABUF_ALPHA);
  }

  RefPtr<WaylandDMABufSurface> surface =
      WaylandDMABufSurface::CreateDMABufSurface(size.width, size.height, flags);
  if (!surface || !surface->CreateEGLImage(prodGL)) {
    return nullptr;
  }

  UniquePtr<SharedSurface_DMABUF> ret;
  ret.reset(new SharedSurface_DMABUF(prodGL, size, hasAlpha, surface));
  return ret;
}

SharedSurface_DMABUF::SharedSurface_DMABUF(
    GLContext* gl, const gfx::IntSize& size, bool hasAlpha,
    RefPtr<WaylandDMABufSurface> aSurface)
    : SharedSurface(SharedSurfaceType::EGLSurfaceDMABUF,
                    AttachmentType::GLTexture, gl, size, hasAlpha, true),
      mSurface(aSurface) {}

SharedSurface_DMABUF::~SharedSurface_DMABUF() {
  if (!mGL || !mGL->MakeCurrent()) {
    return;
  }
  mSurface->ReleaseEGLImage();
}

void SharedSurface_DMABUF::ProducerReleaseImpl() {
  mGL->MakeCurrent();
  // We don't have a better sync mechanism here so use glFinish() at least.
  mGL->fFinish();
}

bool SharedSurface_DMABUF::ToSurfaceDescriptor(
    layers::SurfaceDescriptor* const out_descriptor) {
  MOZ_ASSERT(mSurface);
  return mSurface->Serialize(*out_descriptor);
}

}  // namespace mozilla::gl

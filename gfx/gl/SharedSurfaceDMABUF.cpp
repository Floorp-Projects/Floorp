/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceDMABUF.h"

#include "gfxPlatform.h"
#include "GLContextEGL.h"
#include "MozFramebuffer.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc

namespace mozilla::gl {

static bool HasDmaBufExtensions(const GLContextEGL* gl) {
  const auto& egl = *(gl->mEgl);
  return egl.IsExtensionSupported(EGLExtension::EXT_image_dma_buf_import) &&
         egl.IsExtensionSupported(
             EGLExtension::EXT_image_dma_buf_import_modifiers) &&
         egl.IsExtensionSupported(EGLExtension::MESA_image_dma_buf_export);
}

/*static*/
UniquePtr<SharedSurface_DMABUF> SharedSurface_DMABUF::Create(
    const SharedSurfaceDesc& desc) {
  const auto& gle = GLContextEGL::Cast(desc.gl);
  const auto& context = gle->mContext;
  const auto& egl = *(gle->mEgl);

  if (!HasDmaBufExtensions(gle)) {
    return nullptr;
  }

  auto fb = MozFramebuffer::Create(desc.gl, desc.size, 0, false);
  if (!fb) return nullptr;

  const auto buffer = reinterpret_cast<EGLClientBuffer>(fb->ColorTex());
  const auto image =
      egl.fCreateImage(context, LOCAL_EGL_GL_TEXTURE_2D, buffer, nullptr);
  if (!image) return nullptr;

  const RefPtr<DMABufSurface> surface = DMABufSurfaceRGBA::CreateDMABufSurface(
      desc.gl, image, desc.size.width, desc.size.height);
  if (!surface) return nullptr;

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

void SharedSurface_DMABUF::WaitForBufferOwnership() { mSurface->FenceWait(); }

Maybe<layers::SurfaceDescriptor> SharedSurface_DMABUF::ToSurfaceDescriptor() {
  layers::SurfaceDescriptor desc;
  if (!mSurface->Serialize(desc)) return {};
  return Some(desc);
}

/*static*/
UniquePtr<SurfaceFactory_DMABUF> SurfaceFactory_DMABUF::Create(GLContext& gl) {
  if (!widget::GetDMABufDevice()->IsDMABufWebGLEnabled()) {
    return nullptr;
  }

  auto dmabufFactory = MakeUnique<SurfaceFactory_DMABUF>(gl);
  if (dmabufFactory->CanCreateSurface(gl)) {
    return dmabufFactory;
  }

  LOGDMABUF(
      ("SurfaceFactory_DMABUF::Create() failed, fallback to SW buffers.\n"));
  widget::GetDMABufDevice()->DisableDMABufWebGL();
  return nullptr;
}

bool SurfaceFactory_DMABUF::CanCreateSurface(GLContext& gl) {
  UniquePtr<SharedSurface> test =
      CreateShared(gfx::IntSize(1, 1), gfx::ColorSpace2::SRGB);
  if (!test) {
    LOGDMABUF((
        "SurfaceFactory_DMABUF::CanCreateSurface() failed to create surface."));
    return false;
  }
  auto desc = test->ToSurfaceDescriptor();
  if (!desc) {
    LOGDMABUF(
        ("SurfaceFactory_DMABUF::CanCreateSurface() failed to serialize "
         "surface."));
    return false;
  }
  RefPtr<DMABufSurface> importedSurface =
      DMABufSurface::CreateDMABufSurface(*desc);
  if (!importedSurface) {
    LOGDMABUF((
        "SurfaceFactory_DMABUF::CanCreateSurface() failed to import surface."));
    return false;
  }
  if (!importedSurface->CreateTexture(&gl)) {
    LOGDMABUF(
        ("SurfaceFactory_DMABUF::CanCreateSurface() failed to create texture "
         "over surface."));
    return false;
  }
  return true;
}

SurfaceFactory_DMABUF::SurfaceFactory_DMABUF(GLContext& gl)
    : SurfaceFactory({&gl, SharedSurfaceType::EGLSurfaceDMABUF,
                      layers::TextureType::DMABUF, true}) {}
}  // namespace mozilla::gl

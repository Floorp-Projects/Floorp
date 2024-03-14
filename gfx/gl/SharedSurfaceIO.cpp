/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceIO.h"

#include "GLContextCGL.h"
#include "MozFramebuffer.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/LayersTypes.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace gl {

// -
// Factory

SurfaceFactory_IOSurface::SurfaceFactory_IOSurface(GLContext& gl)
    : SurfaceFactory({&gl, SharedSurfaceType::IOSurface,
                      layers::TextureType::MacIOSurface, true}),
      mMaxDims(gfx::IntSize::Truncate(MacIOSurface::GetMaxWidth(),
                                      MacIOSurface::GetMaxHeight())) {}

// -
// Surface

static bool BackTextureWithIOSurf(GLContext* const gl, const GLuint tex,
                                  MacIOSurface* const ioSurf) {
  MOZ_ASSERT(gl->IsCurrent());

  ScopedBindTexture texture(gl, tex, LOCAL_GL_TEXTURE_RECTANGLE_ARB);

  gl->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                     LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                     LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_TEXTURE_WRAP_S,
                     LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_TEXTURE_WRAP_T,
                     LOCAL_GL_CLAMP_TO_EDGE);

  return ioSurf->BindTexImage(gl, 0);
}

/*static*/
UniquePtr<SharedSurface_IOSurface> SharedSurface_IOSurface::Create(
    const SharedSurfaceDesc& desc) {
  const auto& size = desc.size;
  const RefPtr<MacIOSurface> ioSurf =
      MacIOSurface::CreateIOSurface(size.width, size.height, true);
  if (!ioSurf) {
    NS_WARNING("Failed to create MacIOSurface.");
    return nullptr;
  }

  ioSurf->SetColorSpace(desc.colorSpace);

  // -

  auto tex = MakeUnique<Texture>(*desc.gl);
  if (!BackTextureWithIOSurf(desc.gl, tex->name, ioSurf)) {
    return nullptr;
  }

  const GLenum target = LOCAL_GL_TEXTURE_RECTANGLE;
  auto fb = MozFramebuffer::CreateForBacking(desc.gl, desc.size, 0, false,
                                             target, tex->name);
  if (!fb) return nullptr;

  return AsUnique(
      new SharedSurface_IOSurface(desc, std::move(fb), std::move(tex), ioSurf));
}

SharedSurface_IOSurface::SharedSurface_IOSurface(
    const SharedSurfaceDesc& desc, UniquePtr<MozFramebuffer> fb,
    UniquePtr<Texture> tex, const RefPtr<MacIOSurface>& ioSurf)
    : SharedSurface(desc, std::move(fb)),
      mTex(std::move(tex)),
      mIOSurf(ioSurf) {}

SharedSurface_IOSurface::~SharedSurface_IOSurface() = default;

void SharedSurface_IOSurface::ProducerReleaseImpl() {
  const auto& gl = mDesc.gl;
  if (!gl) return;
  gl->MakeCurrent();
  gl->fFlush();
}

Maybe<layers::SurfaceDescriptor>
SharedSurface_IOSurface::ToSurfaceDescriptor() {
  const bool isOpaque = false;  // RGBA
  return Some(layers::SurfaceDescriptorMacIOSurface(
      mIOSurf->GetIOSurfaceID(), isOpaque, mIOSurf->GetYUVColorSpace()));
}

}  // namespace gl
}  // namespace mozilla

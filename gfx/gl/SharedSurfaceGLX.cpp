/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceGLX.h"
#include "gfxXlibSurface.h"
#include "GLXLibrary.h"
#include "GLContextProvider.h"
#include "GLContextGLX.h"
#include "GLScreenBuffer.h"
#include "MozFramebuffer.h"
#include "mozilla/gfx/SourceSurfaceCairo.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/ShadowLayerUtilsX11.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/TextureForwarder.h"
#include "mozilla/X11Util.h"

namespace mozilla::gl {

UniquePtr<SharedSurface> SurfaceFactory_GLXDrawable::CreateSharedImpl(
    const SharedSurfaceDesc& desc) {
  return SharedSurface_GLXDrawable::Create(desc);
}

/* static */
UniquePtr<SharedSurface_GLXDrawable> SharedSurface_GLXDrawable::Create(
    const SharedSurfaceDesc& desc) {
  Display* display = DefaultXDisplay();
  Screen* screen = XDefaultScreenOfDisplay(display);
  Visual* visual =
      gfxXlibSurface::FindVisual(screen, gfx::SurfaceFormat::A8R8G8B8_UINT32);

  const RefPtr<gfxXlibSurface> surf =
      gfxXlibSurface::Create(screen, visual, desc.size);
  surf->ReleasePixmap();

  return AsUnique(new SharedSurface_GLXDrawable(desc, surf));
}

SharedSurface_GLXDrawable::SharedSurface_GLXDrawable(
    const SharedSurfaceDesc& desc, const RefPtr<gfxXlibSurface>& xlibSurface)
    : SharedSurface(desc, nullptr), mXlibSurface(xlibSurface) {}

SharedSurface_GLXDrawable::~SharedSurface_GLXDrawable() = default;

void SharedSurface_GLXDrawable::ProducerReleaseImpl() {
  mDesc.gl->MakeCurrent();
  mDesc.gl->fFlush();
}

void SharedSurface_GLXDrawable::LockProdImpl() {
  GLContextGLX::Cast(mDesc.gl)->OverrideDrawable(mXlibSurface->GetGLXPixmap());
}

void SharedSurface_GLXDrawable::UnlockProdImpl() {
  GLContextGLX::Cast(mDesc.gl)->RestoreDrawable();
}

Maybe<layers::SurfaceDescriptor>
SharedSurface_GLXDrawable::ToSurfaceDescriptor() {
  if (!mXlibSurface) return {};
  const bool sameProcess = false;
  return Some(layers::SurfaceDescriptorX11(mXlibSurface, sameProcess));
}

SurfaceFactory_GLXDrawable::SurfaceFactory_GLXDrawable(GLContext& gl)
    : SurfaceFactory({&gl, SharedSurfaceType::GLXDrawable,
                      layers::TextureType::X11, true}) {}

}  // namespace mozilla::gl

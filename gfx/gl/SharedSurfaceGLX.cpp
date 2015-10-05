/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceGLX.h"
#include "gfxXlibSurface.h"
#include "GLXLibrary.h"
#include "GLContextProvider.h"
#include "GLContextGLX.h"
#include "GLScreenBuffer.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/ShadowLayerUtilsX11.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/X11Util.h"

namespace mozilla {
namespace gl {

/* static */
UniquePtr<SharedSurface_GLXDrawable>
SharedSurface_GLXDrawable::Create(GLContext* prodGL,
                                  const SurfaceCaps& caps,
                                  const gfx::IntSize& size,
                                  bool deallocateClient,
                                  bool inSameProcess)
{
    UniquePtr<SharedSurface_GLXDrawable> ret;
    Display* display = DefaultXDisplay();
    Screen* screen = XDefaultScreenOfDisplay(display);
    Visual* visual = gfxXlibSurface::FindVisual(screen, gfxImageFormat::ARGB32);

    RefPtr<gfxXlibSurface> surf = gfxXlibSurface::Create(screen, visual, size);
    if (!deallocateClient)
        surf->ReleasePixmap();

    ret.reset(new SharedSurface_GLXDrawable(prodGL, size, inSameProcess, surf));
    return Move(ret);
}


SharedSurface_GLXDrawable::SharedSurface_GLXDrawable(GLContext* gl,
                                                     const gfx::IntSize& size,
                                                     bool inSameProcess,
                                                     const RefPtr<gfxXlibSurface>& xlibSurface)
    : SharedSurface(SharedSurfaceType::GLXDrawable,
                    AttachmentType::Screen,
                    gl,
                    size,
                    true,
                    true)
    , mXlibSurface(xlibSurface)
    , mInSameProcess(inSameProcess)
{}

void
SharedSurface_GLXDrawable::Fence()
{
    mGL->MakeCurrent();
    mGL->fFlush();
}

void
SharedSurface_GLXDrawable::LockProdImpl()
{
    mGL->Screen()->SetReadBuffer(LOCAL_GL_FRONT);
    GLContextGLX::Cast(mGL)->OverrideDrawable(mXlibSurface->GetGLXPixmap());
}

void
SharedSurface_GLXDrawable::UnlockProdImpl()
{
    GLContextGLX::Cast(mGL)->RestoreDrawable();
}

bool
SharedSurface_GLXDrawable::ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor)
{
  if (!mXlibSurface)
      return false;

   *out_descriptor = layers::SurfaceDescriptorX11(mXlibSurface, mInSameProcess);
   return true;
}

/* static */
UniquePtr<SurfaceFactory_GLXDrawable>
SurfaceFactory_GLXDrawable::Create(GLContext* prodGL,
                                   const SurfaceCaps& caps,
                                   const RefPtr<layers::ISurfaceAllocator>& allocator,
                                   const layers::TextureFlags& flags)
{
    MOZ_ASSERT(caps.alpha, "GLX surfaces require an alpha channel!");

    typedef SurfaceFactory_GLXDrawable ptrT;
    UniquePtr<ptrT> ret(new ptrT(prodGL, caps, allocator,
                                 flags & ~layers::TextureFlags::ORIGIN_BOTTOM_LEFT));
    return Move(ret);
}

UniquePtr<SharedSurface>
SurfaceFactory_GLXDrawable::CreateShared(const gfx::IntSize& size)
{
    bool deallocateClient = !!(mFlags & layers::TextureFlags::DEALLOCATE_CLIENT);
    return SharedSurface_GLXDrawable::Create(mGL, mCaps, size, deallocateClient,
                                             mAllocator->IsSameProcess());
}

} // namespace gl
} // namespace mozilla

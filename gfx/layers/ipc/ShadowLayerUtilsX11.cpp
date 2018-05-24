/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShadowLayerUtilsX11.h"
#include <X11/X.h>                      // for Drawable, XID
#include <X11/Xlib.h>                   // for Display, Visual, etc
#include <X11/extensions/Xrender.h>     // for XRenderPictFormat, etc
#include <X11/extensions/render.h>      // for PictFormat
#include "cairo-xlib.h"
#include "X11UndefineNone.h"
#include <stdint.h>                     // for uint32_t
#include "GLDefs.h"                     // for GLenum
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxXlibSurface.h"             // for gfxXlibSurface
#include "gfx2DGlue.h"                  // for Moz2D transistion helpers
#include "mozilla/X11Util.h"            // for DefaultXDisplay, FinishX, etc
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/CompositorTypes.h"  // for OpenMode
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator, etc
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/ShadowLayers.h"  // for ShadowLayerForwarder, etc
#include "mozilla/mozalloc.h"           // for operator new
#include "gfxEnv.h"
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ERROR

using namespace mozilla::gl;

namespace mozilla {
namespace gl {
class GLContext;
class TextureImage;
}

namespace layers {

// Return true if we're likely compositing using X and so should use
// Xlib surfaces in shadow layers.
static bool
UsingXCompositing()
{
  if (!gfxEnv::LayersEnableXlibSurfaces()) {
      return false;
  }
  return (gfxSurfaceType::Xlib ==
          gfxPlatform::GetPlatform()->ScreenReferenceSurface()->GetType());
}

// LookReturn a pointer to |aFormat| that lives in the Xrender library.
// All code using render formats assumes it doesn't need to copy.
static XRenderPictFormat*
GetXRenderPictFormatFromId(Display* aDisplay, PictFormat aFormatId)
{
  XRenderPictFormat tmplate;
  tmplate.id = aFormatId;
  return XRenderFindFormat(aDisplay, PictFormatID, &tmplate, 0);
}

SurfaceDescriptorX11::SurfaceDescriptorX11(gfxXlibSurface* aSurf,
                                           bool aForwardGLX)
  : mId(aSurf->XDrawable())
  , mSize(aSurf->GetSize())
  , mGLXPixmap(X11None)
{
  const XRenderPictFormat *pictFormat = aSurf->XRenderFormat();
  if (pictFormat) {
    mFormat = pictFormat->id;
  } else {
    mFormat = cairo_xlib_surface_get_visual(aSurf->CairoSurface())->visualid;
  }

  if (aForwardGLX) {
    mGLXPixmap = aSurf->GetGLXPixmap();
  }
}

SurfaceDescriptorX11::SurfaceDescriptorX11(Drawable aDrawable, XID aFormatID,
                                           const gfx::IntSize& aSize)
  : mId(aDrawable)
  , mFormat(aFormatID)
  , mSize(aSize)
  , mGLXPixmap(X11None)
{ }

already_AddRefed<gfxXlibSurface>
SurfaceDescriptorX11::OpenForeign() const
{
  Display* display = DefaultXDisplay();
  Screen* screen = DefaultScreenOfDisplay(display);

  RefPtr<gfxXlibSurface> surf;
  XRenderPictFormat* pictFormat = GetXRenderPictFormatFromId(display, mFormat);
  if (pictFormat) {
    surf = new gfxXlibSurface(screen, mId, pictFormat, mSize);
  } else {
    Visual* visual;
    int depth;
    FindVisualAndDepth(display, mFormat, &visual, &depth);
    if (!visual)
      return nullptr;

    surf = new gfxXlibSurface(display, mId, visual, mSize);
  }

  if (mGLXPixmap)
    surf->BindGLXPixmap(mGLXPixmap);

  return surf->CairoStatus() ? nullptr : surf.forget();
}

/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
  if (UsingXCompositing()) {
    // If we're using X surfaces, then we need to finish all pending
    // operations on the back buffers before handing them to the
    // parent, otherwise the surface might be used by the parent's
    // Display in between two operations queued by our Display.
    FinishX(DefaultXDisplay());
  }
}

/*static*/ void
LayerManagerComposite::PlatformSyncBeforeReplyUpdate()
{
  if (UsingXCompositing()) {
    // If we're using X surfaces, we need to finish all pending
    // operations on the *front buffers* before handing them back to
    // the child, even though they will be read operations.
    // Otherwise, the child might start scribbling on new back buffers
    // that are still participating in requests as old front buffers.
    FinishX(DefaultXDisplay());
  }
}

/*static*/ bool
LayerManagerComposite::SupportsDirectTexturing()
{
  return false;
}

} // namespace layers
} // namespace mozilla

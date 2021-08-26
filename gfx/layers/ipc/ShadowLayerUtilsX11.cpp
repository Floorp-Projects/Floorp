/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShadowLayerUtilsX11.h"
#include <X11/X.h>                   // for Drawable, XID
#include <X11/Xlib.h>                // for Display, Visual, etc
#include <X11/extensions/Xrender.h>  // for XRenderPictFormat, etc
#include <X11/extensions/render.h>   // for PictFormat
#include "cairo-xlib.h"
#include "X11UndefineNone.h"
#include <stdint.h>             // for uint32_t
#include "GLDefs.h"             // for GLenum
#include "gfxPlatform.h"        // for gfxPlatform
#include "gfxXlibSurface.h"     // for gfxXlibSurface
#include "gfx2DGlue.h"          // for Moz2D transistion helpers
#include "mozilla/X11Util.h"    // for DefaultXDisplay, FinishX, etc
#include "mozilla/gfx/Point.h"  // for IntSize
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/CompositorTypes.h"    // for OpenMode
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator, etc
#include "mozilla/layers/LayersMessageUtils.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/mozalloc.h"               // for operator new
#include "gfxEnv.h"
#include "nsCOMPtr.h"  // for already_AddRefed
#include "nsDebug.h"   // for NS_ERROR

using namespace mozilla::gl;

namespace mozilla {
namespace gl {
class GLContext;
class TextureImage;
}  // namespace gl

namespace layers {

// LookReturn a pointer to |aFormat| that lives in the Xrender library.
// All code using render formats assumes it doesn't need to copy.
static XRenderPictFormat* GetXRenderPictFormatFromId(Display* aDisplay,
                                                     PictFormat aFormatId) {
  XRenderPictFormat tmplate;
  tmplate.id = aFormatId;
  return XRenderFindFormat(aDisplay, PictFormatID, &tmplate, 0);
}

SurfaceDescriptorX11::SurfaceDescriptorX11(gfxXlibSurface* aSurf,
                                           bool aForwardGLX)
    : mId(aSurf->XDrawable()), mSize(aSurf->GetSize()), mGLXPixmap(X11None) {
  const XRenderPictFormat* pictFormat = aSurf->XRenderFormat();
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
    : mId(aDrawable), mFormat(aFormatID), mSize(aSize), mGLXPixmap(X11None) {}

already_AddRefed<gfxXlibSurface> SurfaceDescriptorX11::OpenForeign() const {
  Display* display = DefaultXDisplay();
  if (!display) {
    return nullptr;
  }
  Screen* screen = DefaultScreenOfDisplay(display);

  RefPtr<gfxXlibSurface> surf;
  XRenderPictFormat* pictFormat = GetXRenderPictFormatFromId(display, mFormat);
  if (pictFormat) {
    surf = new gfxXlibSurface(screen, mId, pictFormat, mSize);
  } else {
    Visual* visual;
    int depth;
    FindVisualAndDepth(display, mFormat, &visual, &depth);
    if (!visual) return nullptr;

    surf = new gfxXlibSurface(display, mId, visual, mSize);
  }

  if (mGLXPixmap) surf->BindGLXPixmap(mGLXPixmap);

  return surf->CairoStatus() ? nullptr : surf.forget();
}

}  // namespace layers
}  // namespace mozilla

namespace IPC {

void ParamTraits<mozilla::layers::SurfaceDescriptorX11>::Write(
    Message* aMsg, const paramType& aParam) {
  WriteParam(aMsg, aParam.mId);
  WriteParam(aMsg, aParam.mSize);
  WriteParam(aMsg, aParam.mFormat);
  WriteParam(aMsg, aParam.mGLXPixmap);
}

bool ParamTraits<mozilla::layers::SurfaceDescriptorX11>::Read(
    const Message* aMsg, PickleIterator* aIter, paramType* aResult) {
  return (ReadParam(aMsg, aIter, &aResult->mId) &&
          ReadParam(aMsg, aIter, &aResult->mSize) &&
          ReadParam(aMsg, aIter, &aResult->mFormat) &&
          ReadParam(aMsg, aIter, &aResult->mGLXPixmap));
}

}  // namespace IPC

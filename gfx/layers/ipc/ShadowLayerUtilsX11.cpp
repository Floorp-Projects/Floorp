/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayers.h"
#include "mozilla/layers/ShadowLayers.h"
 
#include "gfxPlatform.h"

#include "gfxXlibSurface.h"
#include "mozilla/X11Util.h"
#include "cairo-xlib.h"

namespace mozilla {
namespace layers {

// Return true if we're likely compositing using X and so should use
// Xlib surfaces in shadow layers.
static bool
UsingXCompositing()
{
  return (gfxASurface::SurfaceTypeXlib ==
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

static bool
TakeAndDestroyXlibSurface(SurfaceDescriptor* aSurface)
{
  nsRefPtr<gfxXlibSurface> surf =
    aSurface->get_SurfaceDescriptorX11().OpenForeign();
  surf->TakePixmap();
  *aSurface = SurfaceDescriptor();
  // the Pixmap is destroyed when |surf| goes out of scope
  return true;
}

SurfaceDescriptorX11::SurfaceDescriptorX11(gfxXlibSurface* aSurf)
  : mId(aSurf->XDrawable())
  , mSize(aSurf->GetSize())
{
  const XRenderPictFormat *pictFormat = aSurf->XRenderFormat();
  if (pictFormat) {
    mFormat = pictFormat->id;
  } else {
    mFormat = cairo_xlib_surface_get_visual(aSurf->CairoSurface())->visualid;
  }
}

SurfaceDescriptorX11::SurfaceDescriptorX11(Drawable aDrawable, XID aFormatID,
                                           const gfxIntSize& aSize)
  : mId(aDrawable)
  , mFormat(aFormatID)
  , mSize(aSize)
{ }

already_AddRefed<gfxXlibSurface>
SurfaceDescriptorX11::OpenForeign() const
{
  Display* display = DefaultXDisplay();
  Screen* screen = DefaultScreenOfDisplay(display);

  nsRefPtr<gfxXlibSurface> surf;
  XRenderPictFormat* pictFormat = GetXRenderPictFormatFromId(display, mFormat);
  if (pictFormat) {
    surf = new gfxXlibSurface(screen, mId, pictFormat, mSize);
  } else {
    Visual* visual = NULL;
    unsigned int depth;
    XVisualIDToInfo(display, mFormat, &visual, &depth);
    if (!visual)
      return nsnull;

    surf = new gfxXlibSurface(display, mId, visual, mSize);
  }
  return surf->CairoStatus() ? nsnull : surf.forget();
}

bool
ShadowLayerForwarder::PlatformAllocBuffer(const gfxIntSize& aSize,
                                          gfxASurface::gfxContentType aContent,
                                          uint32_t aCaps,
                                          SurfaceDescriptor* aBuffer)
{
  if (!UsingXCompositing()) {
    // If we're not using X compositing, we're probably compositing on
    // the client side, in which case X surfaces would just slow
    // things down.  Use Shmem instead.
    return false;
  }
  if (MAP_AS_IMAGE_SURFACE & aCaps) {
    // We can't efficiently map pixmaps as gfxImageSurface, in
    // general.  Fall back on Shmem.
    return false;
  }

  gfxPlatform* platform = gfxPlatform::GetPlatform();
  nsRefPtr<gfxASurface> buffer = platform->CreateOffscreenSurface(aSize, aContent);
  if (!buffer ||
      buffer->GetType() != gfxASurface::SurfaceTypeXlib) {
    NS_ERROR("creating Xlib front/back surfaces failed!");
    return false;
  }

  gfxXlibSurface* bufferX = static_cast<gfxXlibSurface*>(buffer.get());
  // Release Pixmap ownership to the layers model
  bufferX->ReleasePixmap();

  *aBuffer = SurfaceDescriptorX11(bufferX);
  return true;
}

/*static*/ already_AddRefed<gfxASurface>
ShadowLayerForwarder::PlatformOpenDescriptor(OpenMode aMode,
                                             const SurfaceDescriptor& aSurface)
{
  if (SurfaceDescriptor::TSurfaceDescriptorX11 != aSurface.type()) {
    return nsnull;
  }
  return aSurface.get_SurfaceDescriptorX11().OpenForeign();
}

/*static*/ bool
ShadowLayerForwarder::PlatformCloseDescriptor(const SurfaceDescriptor& aDescriptor)
{
  // XIDs don't need to be "closed".
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
  if (SurfaceDescriptor::TSurfaceDescriptorX11 != aSurface->type()) {
    return false;
  }
  return TakeAndDestroyXlibSurface(aSurface);
}

/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
  if (UsingXCompositing()) {
    // If we're using X surfaces, then we need to finish all pending
    // operations on the back buffers before handing them to the
    // parent, otherwise the surface might be used by the parent's
    // Display in between two operations queued by our Display.
    XSync(DefaultXDisplay(), False);
  }
}

/*static*/ void
ShadowLayerManager::PlatformSyncBeforeReplyUpdate()
{
  if (UsingXCompositing()) {
    // If we're using X surfaces, we need to finish all pending
    // operations on the *front buffers* before handing them back to
    // the child, even though they will be read operations.
    // Otherwise, the child might start scribbling on new back buffers
    // that are still participating in requests as old front buffers.
    XSync(DefaultXDisplay(), False);
  }
}

bool
ShadowLayerManager::PlatformDestroySharedSurface(SurfaceDescriptor* aSurface)
{
  if (SurfaceDescriptor::TSurfaceDescriptorX11 != aSurface->type()) {
    return false;
  }
  return TakeAndDestroyXlibSurface(aSurface);
}

} // namespace layers
} // namespace mozilla

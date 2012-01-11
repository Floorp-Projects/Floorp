/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
ShadowLayerForwarder::PlatformAllocDoubleBuffer(const gfxIntSize& aSize,
                                                gfxASurface::gfxContentType aContent,
                                                SurfaceDescriptor* aFrontBuffer,
                                                SurfaceDescriptor* aBackBuffer)
{
  return (PlatformAllocBuffer(aSize, aContent, aFrontBuffer) &&
          PlatformAllocBuffer(aSize, aContent, aBackBuffer));
}

bool
ShadowLayerForwarder::PlatformAllocBuffer(const gfxIntSize& aSize,
                                          gfxASurface::gfxContentType aContent,
                                          SurfaceDescriptor* aBuffer)
{
  if (!UsingXCompositing()) {
    // If we're not using X compositing, we're probably compositing on
    // the client side, in which case X surfaces would just slow
    // things down.  Use Shmem instead.
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
ShadowLayerForwarder::PlatformOpenDescriptor(const SurfaceDescriptor& aSurface)
{
  if (SurfaceDescriptor::TSurfaceDescriptorX11 != aSurface.type()) {
    return nsnull;
  }
  return aSurface.get_SurfaceDescriptorX11().OpenForeign();
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

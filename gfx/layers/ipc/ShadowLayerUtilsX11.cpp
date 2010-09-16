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

namespace mozilla {
namespace layers {

// LookReturn a pointer to |aFormat| that lives in the Xrender library.
// All code using render formats assumes it doesn't need to copy.
static XRenderPictFormat*
GetXRenderPictFormatFromId(Display* aDisplay, PictFormat aFormatId)
{
  XRenderPictFormat tmplate;
  tmplate.id = aFormatId;
  return XRenderFindFormat(aDisplay, PictFormatID, &tmplate, 0);
}

// FIXME/bug 594921: stopgap until we have better cairo_xlib_surface
// APIs for working with Xlib surfaces across processes
static already_AddRefed<gfxXlibSurface>
CreateSimilar(gfxXlibSurface* aReference,
              gfxASurface::gfxContentType aType, const gfxIntSize& aSize)
{
  Display* display = aReference->XDisplay();
  XRenderPictFormat* xrenderFormat = nsnull;

  // Try to re-use aReference's render format if it's compatible
  if (aReference->GetContentType() == aType) {
    xrenderFormat = aReference->XRenderFormat();
  } else {
    // Couldn't use aReference's directly.  Use a standard format then.
    gfxASurface::gfxImageFormat format;
    switch (aType) {
    case gfxASurface::CONTENT_COLOR:
      // FIXME/bug 593175: investigate 16bpp
      format = gfxASurface::ImageFormatRGB24; break;
    case gfxASurface::CONTENT_ALPHA:
      format = gfxASurface::ImageFormatA8; break;
    default:
      NS_NOTREACHED("unknown gfxContentType");
    case gfxASurface::CONTENT_COLOR_ALPHA:
      format = gfxASurface::ImageFormatARGB32; break;
    }
    xrenderFormat = gfxXlibSurface::FindRenderFormat(display, format);
  }

  if (!xrenderformat) {
    NS_WARNING("couldn't find suitable render format");
    return nsnull;
  }

  return gfxXlibSurface::Create(aReference->XScreen(), xrenderFormat,
                                aSize, aReference->XDrawable());
}

static PRBool
TakeAndDestroyXlibSurface(SurfaceDescriptor* aSurface)
{
  nsRefPtr<gfxXlibSurface> surf =
    aSurface->get_SurfaceDescriptorX11().OpenForeign();
  surf->TakePixmap();
  *aSurface = SurfaceDescriptor();
  // the Pixmap is destroyed when |surf| goes out of scope
  return PR_TRUE;
}

SurfaceDescriptorX11::SurfaceDescriptorX11(gfxXlibSurface* aSurf)
  : mId(aSurf->XDrawable())
  , mSize(aSurf->GetSize())
  , mFormat(aSurf->XRenderFormat()->id)
{ }

already_AddRefed<gfxXlibSurface>
SurfaceDescriptorX11::OpenForeign() const
{
  Display* display = DefaultXDisplay();
  Screen* screen = DefaultScreenOfDisplay(display);

  XRenderPictFormat* format = GetXRenderPictFormatFromId(display, mFormat);
  nsRefPtr<gfxXlibSurface> surf =
    new gfxXlibSurface(screen, mId, format, mSize);
  return surf->CairoStatus() ? nsnull : surf.forget();
}

PRBool
ShadowLayerForwarder::PlatformAllocDoubleBuffer(const gfxIntSize& aSize,
                                                gfxASurface::gfxContentType aContent,
                                                SurfaceDescriptor* aFrontBuffer,
                                                SurfaceDescriptor* aBackBuffer)
{
  gfxASurface* reference = gfxPlatform::GetPlatform()->ScreenReferenceSurface();
  NS_ABORT_IF_FALSE(reference->GetType() == gfxASurface::SurfaceTypeXlib,
                    "can't alloc an Xlib surface on non-X11");
  gfxXlibSurface* xlibRef = static_cast<gfxXlibSurface*>(reference);

  nsRefPtr<gfxXlibSurface> front = CreateSimilar(xlibRef, aContent, aSize);
  nsRefPtr<gfxXlibSurface> back = CreateSimilar(xlibRef, aContent, aSize);
  if (!front || !back) {
    NS_ERROR("creating Xlib front/back surfaces failed!");
    return PR_FALSE;
  }

  // Release Pixmap ownership to the layers model
  front->ReleasePixmap();
  back->ReleasePixmap();

  *aFrontBuffer = SurfaceDescriptorX11(front);
  *aBackBuffer = SurfaceDescriptorX11(back);
  return PR_TRUE;
}

/*static*/ already_AddRefed<gfxASurface>
ShadowLayerForwarder::PlatformOpenDescriptor(const SurfaceDescriptor& aSurface)
{
  if (SurfaceDescriptor::TSurfaceDescriptorX11 != aSurface.type()) {
    return nsnull;
  }
  return aSurface.get_SurfaceDescriptorX11().OpenForeign();
}

PRBool
ShadowLayerForwarder::PlatformDestroySharedSurface(SurfaceDescriptor* aSurface)
{
  if (SurfaceDescriptor::TSurfaceDescriptorX11 != aSurface->type()) {
    return PR_FALSE;
  }
  return TakeAndDestroyXlibSurface(aSurface);
}

/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
  XSync(DefaultXDisplay(), False);
}

/*static*/ void
ShadowLayerManager::PlatformSyncBeforeReplyUpdate()
{
  XSync(DefaultXDisplay(), False);
}

PRBool
ShadowLayerManager::PlatformDestroySharedSurface(SurfaceDescriptor* aSurface)
{
  if (SurfaceDescriptor::TSurfaceDescriptorX11 != aSurface->type()) {
    return PR_FALSE;
  }
  return TakeAndDestroyXlibSurface(aSurface);
}

} // namespace layers
} // namespace mozilla

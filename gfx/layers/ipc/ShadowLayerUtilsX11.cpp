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
  gfxPlatform* platform = gfxPlatform::GetPlatform();

  nsRefPtr<gfxASurface> front = platform->CreateOffscreenSurface(aSize, aContent);
  nsRefPtr<gfxASurface> back = platform->CreateOffscreenSurface(aSize, aContent);
  if (!front || !back ||
      front->GetType() != gfxASurface::SurfaceTypeXlib ||
      back->GetType() != gfxASurface::SurfaceTypeXlib) {
    NS_ERROR("creating Xlib front/back surfaces failed!");
    return PR_FALSE;
  }

  gfxXlibSurface* frontX = static_cast<gfxXlibSurface*>(front.get());
  gfxXlibSurface* backX = static_cast<gfxXlibSurface*>(back.get());
  // Release Pixmap ownership to the layers model
  frontX->ReleasePixmap();
  backX->ReleasePixmap();

  *aFrontBuffer = SurfaceDescriptorX11(frontX);
  *aBackBuffer = SurfaceDescriptorX11(backX);
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

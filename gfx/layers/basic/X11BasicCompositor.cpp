/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "X11BasicCompositor.h"
#include "gfxPlatform.h"
#include "gfx2DGlue.h"
#include "gfxXlibSurface.h"
#include "gfxImageSurface.h"
#include "mozilla/X11Util.h"

namespace mozilla {
using namespace mozilla::gfx;

namespace layers {

bool
X11DataTextureSourceBasic::Update(gfx::DataSourceSurface* aSurface,
                                  nsIntRegion* aDestRegion,
                                  gfx::IntPoint* aSrcOffset)
{
  // Reallocate our internal X11 surface if we don't have a DrawTarget yet,
  // or if we changed surface size or format since last update.
  if (!mBufferDrawTarget ||
      (aSurface->GetSize() != mBufferDrawTarget->GetSize()) ||
      (aSurface->GetFormat() != mBufferDrawTarget->GetFormat())) {

    nsRefPtr<gfxASurface> surf;
    gfxImageFormat imageFormat = SurfaceFormatToImageFormat(aSurface->GetFormat());
    Display *display = DefaultXDisplay();
    Screen *screen = DefaultScreenOfDisplay(display);
    XRenderPictFormat *xrenderFormat =
      gfxXlibSurface::FindRenderFormat(display, imageFormat);

    if (xrenderFormat) {
      surf = gfxXlibSurface::Create(screen, xrenderFormat,
                                    aSurface->GetSize());
    }

    if (!surf) {
      NS_WARNING("Couldn't create native surface, fallback to image surface");
      surf = new gfxImageSurface(aSurface->GetSize(), imageFormat);
    }

    mBufferDrawTarget = gfxPlatform::GetPlatform()->
      CreateDrawTargetForSurface(surf, aSurface->GetSize());
  }

  // Image contents have changed, upload to our DrawTarget
  // If aDestRegion is null, means we're updating the whole surface
  // Note : Incremental update with a source offset is only used on Mac.
  NS_ASSERTION(!aSrcOffset, "SrcOffset should not be used with linux OMTC basic");

  if (aDestRegion) {
    nsIntRegionRectIterator iter(*aDestRegion);
    while (const IntRect* iterRect = iter.Next()) {
      IntRect srcRect(iterRect->x, iterRect->y, iterRect->width, iterRect->height);
      IntPoint dstPoint(iterRect->x, iterRect->y);

      // We're uploading regions to our buffer, so let's just copy contents over
      mBufferDrawTarget->CopySurface(aSurface, srcRect, dstPoint);
    }
  } else {
    // We're uploading the whole buffer, so let's just copy the full surface
    IntSize size = aSurface->GetSize();
    mBufferDrawTarget->CopySurface(aSurface, IntRect(0, 0, size.width, size.height),
                                   IntPoint(0, 0));
  }

  return true;
}

TextureSourceBasic*
X11DataTextureSourceBasic::AsSourceBasic()
{
  return this;
}

IntSize
X11DataTextureSourceBasic::GetSize() const
{
  if (!mBufferDrawTarget) {
    NS_WARNING("Trying to query the size of an uninitialized TextureSource");
    return IntSize(0, 0);
  } else {
    return mBufferDrawTarget->GetSize();
  }
}

gfx::SurfaceFormat
X11DataTextureSourceBasic::GetFormat() const
{
  if (!mBufferDrawTarget) {
    NS_WARNING("Trying to query the format of an uninitialized TextureSource");
    return gfx::SurfaceFormat::UNKNOWN;
  } else {
    return mBufferDrawTarget->GetFormat();
  }
}

SourceSurface*
X11DataTextureSourceBasic::GetSurface(DrawTarget* aTarget)
{
  RefPtr<gfx::SourceSurface> surface;
  if (mBufferDrawTarget) {
    surface = mBufferDrawTarget->Snapshot();
    return surface.get();
  } else {
    return nullptr;
  }
}

void
X11DataTextureSourceBasic::DeallocateDeviceData()
{
  mBufferDrawTarget = nullptr;
}

TemporaryRef<DataTextureSource>
X11BasicCompositor::CreateDataTextureSource(TextureFlags aFlags)
{
  RefPtr<DataTextureSource> result =
    new X11DataTextureSourceBasic();
  return result.forget();
}

void
X11BasicCompositor::EndFrame()
{
  BasicCompositor::EndFrame();
  XFlush(DefaultXDisplay());
}

} // namespace layers
} // namespace mozilla

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxBlur.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"

#include "mozilla/gfx/Blur.h"

using namespace mozilla::gfx;

gfxAlphaBoxBlur::gfxAlphaBoxBlur()
 : mBlur(nullptr)
{
}

gfxAlphaBoxBlur::~gfxAlphaBoxBlur()
{
  mContext = nullptr;
  mImageSurface = nullptr;
  delete mBlur;
}

gfxContext*
gfxAlphaBoxBlur::Init(const gfxRect& aRect,
                      const gfxIntSize& aSpreadRadius,
                      const gfxIntSize& aBlurRadius,
                      const gfxRect* aDirtyRect,
                      const gfxRect* aSkipRect)
{
    mozilla::gfx::Rect rect(Float(aRect.x), Float(aRect.y),
                            Float(aRect.width), Float(aRect.height));
    IntSize spreadRadius(aSpreadRadius.width, aSpreadRadius.height);
    IntSize blurRadius(aBlurRadius.width, aBlurRadius.height);
    nsAutoPtr<mozilla::gfx::Rect> dirtyRect;
    if (aDirtyRect) {
      dirtyRect = new mozilla::gfx::Rect(Float(aDirtyRect->x),
                                         Float(aDirtyRect->y),
                                         Float(aDirtyRect->width),
                                         Float(aDirtyRect->height));
    }
    nsAutoPtr<mozilla::gfx::Rect> skipRect;
    if (aSkipRect) {
      skipRect = new mozilla::gfx::Rect(Float(aSkipRect->x),
                                        Float(aSkipRect->y),
                                        Float(aSkipRect->width),
                                        Float(aSkipRect->height));
    }

    mBlur = new AlphaBoxBlur(rect, spreadRadius, blurRadius, dirtyRect, skipRect);
    int32_t blurDataSize = mBlur->GetSurfaceAllocationSize();
    if (blurDataSize <= 0)
        return nullptr;

    IntSize size = mBlur->GetSize();

    // Make an alpha-only surface to draw on. We will play with the data after
    // everything is drawn to create a blur effect.
    mImageSurface = new gfxImageSurface(gfxIntSize(size.width, size.height),
                                        gfxImageFormatA8,
                                        mBlur->GetStride(),
                                        blurDataSize,
                                        true);
    if (mImageSurface->CairoStatus())
        return nullptr;

    IntRect irect = mBlur->GetRect();
    gfxPoint topleft(irect.TopLeft().x, irect.TopLeft().y);

    // Use a device offset so callers don't need to worry about translating
    // coordinates, they can draw as if this was part of the destination context
    // at the coordinates of rect.
    mImageSurface->SetDeviceOffset(-topleft);

    mContext = new gfxContext(mImageSurface);

    return mContext;
}

void
gfxAlphaBoxBlur::Paint(gfxContext* aDestinationCtx)
{
    if (!mContext)
        return;

    mBlur->Blur(mImageSurface->Data());

    mozilla::gfx::Rect* dirtyrect = mBlur->GetDirtyRect();

    // Avoid a semi-expensive clip operation if we can, otherwise
    // clip to the dirty rect
    if (dirtyrect) {
        aDestinationCtx->Save();
        aDestinationCtx->NewPath();
        gfxRect dirty(dirtyrect->x, dirtyrect->y, dirtyrect->width, dirtyrect->height);
        gfxRect imageRect(-mImageSurface->GetDeviceOffset(), mImageSurface->GetSize());
        dirty.IntersectRect(dirty, imageRect);
        aDestinationCtx->Rectangle(dirty);
        aDestinationCtx->Clip();
        aDestinationCtx->Mask(mImageSurface, gfxPoint());
        aDestinationCtx->Restore();
    } else {
        aDestinationCtx->Mask(mImageSurface, gfxPoint());
    }
}

gfxIntSize gfxAlphaBoxBlur::CalculateBlurRadius(const gfxPoint& aStd)
{
    mozilla::gfx::Point std(Float(aStd.x), Float(aStd.y));
    IntSize size = AlphaBoxBlur::CalculateBlurRadius(std);
    return gfxIntSize(size.width, size.height);
}

/* static */ void
gfxAlphaBoxBlur::BlurRectangle(gfxContext *aDestinationCtx,
                               const gfxRect& aRect,
                               gfxCornerSizes* aCornerRadii,
                               const gfxPoint& aBlurStdDev,
                               const gfxRGBA& aShadowColor,
                               const gfxRect& aDirtyRect,
                               const gfxRect& aSkipRect)
{
  gfxIntSize blurRadius = CalculateBlurRadius(aBlurStdDev);

  // Create the temporary surface for blurring
  gfxAlphaBoxBlur blur;
  gfxContext *dest = blur.Init(aRect, gfxIntSize(), blurRadius, &aDirtyRect, &aSkipRect);

  if (!dest) {
    return;
  }

  gfxRect shadowGfxRect = aRect;
  shadowGfxRect.Round();

  dest->NewPath();
  if (aCornerRadii) {
    dest->RoundedRectangle(shadowGfxRect, *aCornerRadii);
  } else {
    dest->Rectangle(shadowGfxRect);
  }
  dest->Fill();

  aDestinationCtx->SetColor(aShadowColor);
  blur.Paint(aDestinationCtx);
}

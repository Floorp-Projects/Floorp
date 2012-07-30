/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxBlur.h"

#include "mozilla/gfx/Blur.h"

using namespace mozilla::gfx;

gfxAlphaBoxBlur::gfxAlphaBoxBlur()
 : mBlur(nullptr)
{
}

gfxAlphaBoxBlur::~gfxAlphaBoxBlur()
{
  delete mBlur;
}

gfxContext*
gfxAlphaBoxBlur::Init(const gfxRect& aRect,
                      const gfxIntSize& aSpreadRadius,
                      const gfxIntSize& aBlurRadius,
                      const gfxRect* aDirtyRect,
                      const gfxRect* aSkipRect)
{
    mozilla::gfx::Rect rect(aRect.x, aRect.y, aRect.width, aRect.height);
    IntSize spreadRadius(aSpreadRadius.width, aSpreadRadius.height);
    IntSize blurRadius(aBlurRadius.width, aBlurRadius.height);
    nsAutoPtr<mozilla::gfx::Rect> dirtyRect;
    if (aDirtyRect) {
      dirtyRect = new mozilla::gfx::Rect(aDirtyRect->x, aDirtyRect->y, aDirtyRect->width, aDirtyRect->height);
    }
    nsAutoPtr<mozilla::gfx::Rect> skipRect;
    if (aSkipRect) {
      skipRect = new mozilla::gfx::Rect(aSkipRect->x, aSkipRect->y, aSkipRect->width, aSkipRect->height);
    }

    mBlur = new AlphaBoxBlur(rect, spreadRadius, blurRadius, dirtyRect, skipRect);

    unsigned char* data = mBlur->GetData();
    if (!data)
      return nullptr;

    IntSize size = mBlur->GetSize();
    // Make an alpha-only surface to draw on. We will play with the data after
    // everything is drawn to create a blur effect.
    mImageSurface = new gfxImageSurface(data, gfxIntSize(size.width, size.height),
                                        mBlur->GetStride(),
                                        gfxASurface::ImageFormatA8);
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
gfxAlphaBoxBlur::Paint(gfxContext* aDestinationCtx, const gfxPoint& offset)
{
    if (!mContext)
        return;

    mBlur->Blur();

    mozilla::gfx::Rect* dirtyrect = mBlur->GetDirtyRect();

    // Avoid a semi-expensive clip operation if we can, otherwise
    // clip to the dirty rect
    if (dirtyrect) {
        aDestinationCtx->Save();
        aDestinationCtx->NewPath();
        gfxRect dirty(dirtyrect->x, dirtyrect->y, dirtyrect->width, dirtyrect->height);
        gfxRect imageRect(offset - mImageSurface->GetDeviceOffset(), mImageSurface->GetSize());
        dirty.IntersectRect(dirty, imageRect);
        aDestinationCtx->Rectangle(dirty);
        aDestinationCtx->Clip();
        aDestinationCtx->Mask(mImageSurface, offset);
        aDestinationCtx->Restore();
    } else {
        aDestinationCtx->Mask(mImageSurface, offset);
    }
}

gfxIntSize gfxAlphaBoxBlur::CalculateBlurRadius(const gfxPoint& aStd)
{
    mozilla::gfx::Point std(aStd.x, aStd.y);
    IntSize size = AlphaBoxBlur::CalculateBlurRadius(std);
    return gfxIntSize(size.width, size.height);
}

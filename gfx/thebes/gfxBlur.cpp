/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is gfx thebes code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Eric Butler <zantifon@gmail.com>
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

#include "gfxBlur.h"

#include "mozilla/gfx/Blur.h"

using namespace mozilla::gfx;

gfxAlphaBoxBlur::gfxAlphaBoxBlur()
 : mBlur(nsnull)
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
    Rect rect(aRect.x, aRect.y, aRect.width, aRect.height);
    IntSize spreadRadius(aSpreadRadius.width, aSpreadRadius.height);
    IntSize blurRadius(aBlurRadius.width, aBlurRadius.height);
    nsAutoPtr<Rect> dirtyRect;
    if (aDirtyRect) {
      dirtyRect = new Rect(aDirtyRect->x, aDirtyRect->y, aDirtyRect->width, aDirtyRect->height);
    }
    nsAutoPtr<Rect> skipRect;
    if (aSkipRect) {
      skipRect = new Rect(aSkipRect->x, aSkipRect->y, aSkipRect->width, aSkipRect->height);
    }

    mBlur = new AlphaBoxBlur(rect, spreadRadius, blurRadius, dirtyRect, skipRect);

    unsigned char* data = mBlur->GetData();
    if (!data)
      return nsnull;

    IntSize size = mBlur->GetSize();
    // Make an alpha-only surface to draw on. We will play with the data after
    // everything is drawn to create a blur effect.
    mImageSurface = new gfxImageSurface(data, gfxIntSize(size.width, size.height),
                                        mBlur->GetStride(),
                                        gfxASurface::ImageFormatA8);
    if (mImageSurface->CairoStatus())
        return nsnull;

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

    Rect* dirtyrect = mBlur->GetDirtyRect();

    // Avoid a semi-expensive clip operation if we can, otherwise
    // clip to the dirty rect
    if (dirtyrect) {
        aDestinationCtx->Save();
        aDestinationCtx->NewPath();
        gfxRect dirty(dirtyrect->x, dirtyrect->y, dirtyrect->width, dirtyrect->height);
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
    Point std(aStd.x, aStd.y);
    IntSize size = AlphaBoxBlur::CalculateBlurRadius(std);
    return gfxIntSize(size.width, size.height);
}

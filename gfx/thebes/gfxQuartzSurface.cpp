/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "gfxQuartzSurface.h"
#include "gfxContext.h"

#include "cairo-quartz.h"

void
gfxQuartzSurface::MakeInvalid()
{
    mSize = gfxIntSize(-1, -1);    
}

gfxQuartzSurface::gfxQuartzSurface(const gfxSize& desiredSize, gfxImageFormat format,
                                   bool aForPrinting)
    : mCGContext(NULL), mSize(desiredSize), mForPrinting(aForPrinting)
{
    gfxIntSize size((unsigned int) floor(desiredSize.width),
                    (unsigned int) floor(desiredSize.height));
    if (!CheckSurfaceSize(size))
        MakeInvalid();

    unsigned int width = static_cast<unsigned int>(mSize.width);
    unsigned int height = static_cast<unsigned int>(mSize.height);

    cairo_surface_t *surf = cairo_quartz_surface_create
        ((cairo_format_t) format, width, height);

    mCGContext = cairo_quartz_surface_get_cg_context (surf);

    CGContextRetain(mCGContext);

    Init(surf);
}

gfxQuartzSurface::gfxQuartzSurface(CGContextRef context,
                                   const gfxSize& desiredSize,
                                   bool aForPrinting)
    : mCGContext(context), mSize(desiredSize), mForPrinting(aForPrinting)
{
    gfxIntSize size((unsigned int) floor(desiredSize.width),
                    (unsigned int) floor(desiredSize.height));
    if (!CheckSurfaceSize(size))
        MakeInvalid();

    unsigned int width = static_cast<unsigned int>(mSize.width);
    unsigned int height = static_cast<unsigned int>(mSize.height);

    cairo_surface_t *surf = 
        cairo_quartz_surface_create_for_cg_context(context,
                                                   width, height);

    CGContextRetain(mCGContext);

    Init(surf);
}

gfxQuartzSurface::gfxQuartzSurface(cairo_surface_t *csurf,
                                   bool aForPrinting) :
    mSize(-1.0, -1.0), mForPrinting(aForPrinting)
{
    mCGContext = cairo_quartz_surface_get_cg_context (csurf);
    CGContextRetain (mCGContext);

    Init(csurf, PR_TRUE);
}

gfxQuartzSurface::gfxQuartzSurface(unsigned char *data,
                                   const gfxSize& desiredSize,
                                   long stride,
                                   gfxImageFormat format,
                                   bool aForPrinting)
    : mCGContext(nsnull), mSize(desiredSize), mForPrinting(aForPrinting)
{
    gfxIntSize size((unsigned int) floor(desiredSize.width),
                    (unsigned int) floor(desiredSize.height));
    if (!CheckSurfaceSize(size))
        MakeInvalid();

    unsigned int width = static_cast<unsigned int>(mSize.width);
    unsigned int height = static_cast<unsigned int>(mSize.height);

    cairo_surface_t *surf = cairo_quartz_surface_create_for_data
        (data, (cairo_format_t) format, width, height, stride);

    mCGContext = cairo_quartz_surface_get_cg_context (surf);

    CGContextRetain(mCGContext);

    Init(surf);
}

already_AddRefed<gfxASurface>
gfxQuartzSurface::CreateSimilarSurface(gfxContentType aType,
                                       const gfxIntSize& aSize)
{
    cairo_surface_t *surface =
        cairo_quartz_surface_create_cg_layer(mSurface, (cairo_content_t)aType,
                                             aSize.width, aSize.height);
    if (cairo_surface_status(surface)) {
        cairo_surface_destroy(surface);
        return nsnull;
    }

    nsRefPtr<gfxASurface> result = Wrap(surface);
    cairo_surface_destroy(surface);
    return result.forget();
}

CGContextRef
gfxQuartzSurface::GetCGContextWithClip(gfxContext *ctx)
{
	return cairo_quartz_get_cg_context_with_clip(ctx->GetCairo());
}

PRInt32 gfxQuartzSurface::GetDefaultContextFlags() const
{
    if (mForPrinting)
        return gfxContext::FLAG_DISABLE_SNAPPING |
               gfxContext::FLAG_DISABLE_COPY_BACKGROUND;

    return 0;
}

already_AddRefed<gfxImageSurface> gfxQuartzSurface::GetAsImageSurface()
{
    cairo_surface_t *surface = cairo_quartz_surface_get_image(mSurface);
    if (!surface)
        return nsnull;

    nsRefPtr<gfxASurface> img = Wrap(surface);

    // cairo_quartz_surface_get_image returns a referenced image, and thebes
    // shares the refcounts of Cairo surfaces. However, Wrap also adds a
    // reference to the image. We need to remove one of these references
    // explicitly so we don't leak.
    gfxImageSurface* imgSurface = static_cast<gfxImageSurface*> (img.forget().get());
    imgSurface->Release();

    return imgSurface;
}

gfxQuartzSurface::~gfxQuartzSurface()
{
    CGContextRelease(mCGContext);
}

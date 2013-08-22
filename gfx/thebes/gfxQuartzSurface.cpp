/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    : mCGContext(nullptr), mSize(desiredSize), mForPrinting(aForPrinting)
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
    if (mSurfaceValid) {
      RecordMemoryUsed(mSize.height * 4 + sizeof(gfxQuartzSurface));
    }
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
    if (mSurfaceValid) {
      RecordMemoryUsed(mSize.height * 4 + sizeof(gfxQuartzSurface));
    }
}

gfxQuartzSurface::gfxQuartzSurface(CGContextRef context,
                                   const gfxIntSize& size,
                                   bool aForPrinting)
    : mCGContext(context), mSize(size), mForPrinting(aForPrinting)
{
    if (!CheckSurfaceSize(size))
        MakeInvalid();

    unsigned int width = static_cast<unsigned int>(mSize.width);
    unsigned int height = static_cast<unsigned int>(mSize.height);

    cairo_surface_t *surf = 
        cairo_quartz_surface_create_for_cg_context(context,
                                                   width, height);

    CGContextRetain(mCGContext);

    Init(surf);
    if (mSurfaceValid) {
      RecordMemoryUsed(mSize.height * 4 + sizeof(gfxQuartzSurface));
    }
}

gfxQuartzSurface::gfxQuartzSurface(cairo_surface_t *csurf,
                                   bool aForPrinting) :
    mSize(-1.0, -1.0), mForPrinting(aForPrinting)
{
    mCGContext = cairo_quartz_surface_get_cg_context (csurf);
    CGContextRetain (mCGContext);

    Init(csurf, true);
}

gfxQuartzSurface::gfxQuartzSurface(unsigned char *data,
                                   const gfxSize& desiredSize,
                                   long stride,
                                   gfxImageFormat format,
                                   bool aForPrinting)
    : mCGContext(nullptr), mSize(desiredSize), mForPrinting(aForPrinting)
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
    if (mSurfaceValid) {
      RecordMemoryUsed(mSize.height * stride + sizeof(gfxQuartzSurface));
    }
}

gfxQuartzSurface::gfxQuartzSurface(unsigned char *data,
                                   const gfxIntSize& aSize,
                                   long stride,
                                   gfxImageFormat format,
                                   bool aForPrinting)
    : mCGContext(nullptr), mSize(aSize.width, aSize.height), mForPrinting(aForPrinting)
{
    if (!CheckSurfaceSize(aSize))
        MakeInvalid();

    cairo_surface_t *surf = cairo_quartz_surface_create_for_data
        (data, (cairo_format_t) format, aSize.width, aSize.height, stride);

    mCGContext = cairo_quartz_surface_get_cg_context (surf);

    CGContextRetain(mCGContext);

    Init(surf);
    if (mSurfaceValid) {
      RecordMemoryUsed(mSize.height * stride + sizeof(gfxQuartzSurface));
    }
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
        return nullptr;
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

int32_t gfxQuartzSurface::GetDefaultContextFlags() const
{
    if (mForPrinting)
        return gfxContext::FLAG_DISABLE_SNAPPING |
               gfxContext::FLAG_DISABLE_COPY_BACKGROUND;

    return 0;
}

already_AddRefed<gfxImageSurface> gfxQuartzSurface::GetAsImageSurface()
{
    cairo_surface_t *surface = cairo_quartz_surface_get_image(mSurface);
    if (!surface || cairo_surface_status(surface))
        return nullptr;

    nsRefPtr<gfxASurface> img = Wrap(surface);

    // cairo_quartz_surface_get_image returns a referenced image, and thebes
    // shares the refcounts of Cairo surfaces. However, Wrap also adds a
    // reference to the image. We need to remove one of these references
    // explicitly so we don't leak.
    img->Release();

    return img.forget().downcast<gfxImageSurface>();
}

gfxQuartzSurface::~gfxQuartzSurface()
{
    CGContextRelease(mCGContext);
}

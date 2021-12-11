/* vim:set ts=8 sw=4 noet cin: */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Christian Biesinger <cbiesinger@web.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Christian Biesinger
 * <cbiesinger@web.de>
 *
 * Contributor(s):
 */

// This is a C++ file in order to use the C++ BeOS API

#include "cairoint.h"

#include "cairo-beos.h"

#include "cairo-error-private.h"
#include "cairo-image-surface-inline.h"

#include <new>

#include <Bitmap.h>
#include <Region.h>
#if 0
#include <DirectWindow.h>
#endif
#include <Screen.h>
#include <Window.h>
#include <Locker.h>

/**
 * SECTION:beos-surface
 * @Title: BeOS Surfaces
 * @Short_Description: BeOS surface support
 * @See_Also: #cairo_surface_t
 *
 * The BeOS surface is used to render cairo graphics to BeOS views 
 * and bitmaps.
 **/

#define CAIRO_INT_STATUS_SUCCESS (cairo_int_status_t)(CAIRO_STATUS_SUCCESS)

struct cairo_beos_surface_t {
    cairo_surface_t base;

    cairo_region_t *clip_region;

    BView* view;

    /*
     * A view is either attached to a bitmap, a window, or unattached.
     * If it is attached to a window, we can copy data out of it using BScreen.
     * If it is attached to a bitmap, we can read the bitmap data.
     * If it is not attached, it doesn't draw anything, we need not bother.
     *
     * Since there doesn't seem to be a way to get the bitmap from a view if it
     * is attached to one, we have to use a special surface creation function.
     */

    BBitmap* bitmap;

    // If true, surface and view should be deleted when this surface is
    // destroyed
    bool owns_bitmap_view;
};

class AutoLockView {
    public:
	AutoLockView(BView* view) : mView(view) {
	    mOK = mView->LockLooper();
	}

	~AutoLockView() {
	    if (mOK)
		mView->UnlockLooper();
	}

	operator bool() {
	    return mOK;
	}

    private:
	BView* mView;
	bool   mOK;
};

static cairo_surface_t *
_cairo_beos_surface_create_internal (BView*   view,
				     BBitmap* bmp,
				     bool     owns_bitmap_view = false);

static inline BRect
_cairo_rectangle_to_brect (const cairo_rectangle_int_t* rect)
{
    // A BRect is one pixel wider than you'd think
    return BRect (rect->x, rect->y,
		  rect->x + rect->width - 1,
		  rect->y + rect->height - 1);
}

static inline cairo_rectangle_int_t
_brect_to_cairo_rectangle (const BRect &rect)
{
    cairo_rectangle_int_t retval;
    retval.x = floor (rect.left);
    retval.y = floor (rect.top);
    retval.width = ceil (rect.right) - retval.x + 1;
    retval.height = ceil (rect.bottom) - rectval.y + 1;
    return retval;
}

static inline rgb_color
_cairo_color_to_be_color (const cairo_color_t *color)
{
    // This factor ensures a uniform distribution of numbers
    const float factor = 256 - 1e-5;
    // Using doubles to have non-premultiplied colors
    rgb_color be_color = { uint8(color->red * factor),
			   uint8(color->green * factor),
			   uint8(color->blue * factor),
			   uint8(color->alpha * factor) };

    return be_color;
}

enum ViewCopyStatus {
    OK,
    NOT_VISIBLE, // The view or the interest rect is not visible on screen
    ERROR        // The view was visible, but the rect could not be copied. Probably OOM
};

/**
 * _cairo_beos_view_to_bitmap:
 * @bitmap: [out] The resulting bitmap.
 * @rect: [out] The rectangle that was copied, in the view's coordinate system
 * @interestRect: If non-null, only this part of the view will be copied (view's coord system).
 *
 * Gets the contents of the view as a BBitmap*. Caller must delete the bitmap.
 **/
static ViewCopyStatus
_cairo_beos_view_to_bitmap (BView*       view,
			    BBitmap**    bitmap,
			    BRect*       rect = NULL,
			    const BRect* interestRect = NULL)
{
    *bitmap = NULL;

    BWindow* wnd = view->Window();
    // If we have no window, can't do anything
    if (!wnd)
	return NOT_VISIBLE;

    view->Sync();
    wnd->Sync();

#if 0
    // Is it a direct window?
    BDirectWindow* directWnd = dynamic_cast<BDirectWindow*>(wnd);
    if (directWnd) {
	// WRITEME
    }
#endif

    // Is it visible? If so, we can copy the content off the screen
    if (wnd->IsHidden())
	return NOT_VISIBLE;

    BRect rectToCopy(view->Bounds());
    if (interestRect)
	rectToCopy = rectToCopy & *interestRect;

    if (!rectToCopy.IsValid())
	return NOT_VISIBLE;

    BScreen screen(wnd);
    BRect screenRect(view->ConvertToScreen(rectToCopy));
    screenRect = screenRect & screen.Frame();

    if (!screen.IsValid())
	return NOT_VISIBLE;

    if (rect)
	*rect = view->ConvertFromScreen(screenRect);

    if (screen.GetBitmap(bitmap, false, &screenRect) == B_OK)
	return OK;

    return ERROR;
}

static void
unpremultiply_bgra (unsigned char* data,
		    int            width,
		    int            height,
		    int            stride,
		    unsigned char* retdata)
{
    unsigned char* end = data + stride * height;
    for (unsigned char* in = data, *out = retdata;
	 in < end;
	 in += stride, out += stride)
    {
	for (int i = 0; i < width; i ++) {
	    uint8_t *b = &out[4*i];
	    uint32_t pixel;
	    uint8_t  alpha;

	    memcpy (&pixel, &data[4*i], sizeof (uint32_t));
	    alpha = pixel & 0xff;
	    if (alpha == 0) {
		b[0] = b[1] = b[2] = b[3] = 0;
	    } else {
		b[0] = (((pixel >> 24) & 0xff) * 255 + alpha / 2) / alpha;
		b[1] = (((pixel >> 16) & 0xff) * 255 + alpha / 2) / alpha;
		b[2] = (((pixel >>  8) & 0xff) * 255 + alpha / 2) / alpha;
		b[3] = alpha;
	    }
	}
    }
}

static inline int
multiply_alpha (int alpha, int color)
{
    int temp = (alpha * color) + 0x80;
    return ((temp + (temp >> 8)) >> 8);
}

static unsigned char*
premultiply_bgra (unsigned char* data,
		  int            width,
		  int            height,
		  int            stride)
{
    uint8_t * retdata = reinterpret_cast<unsigned char*>(_cairo_malloc_ab(height, stride));
    if (!retdata)
	return NULL;

    uint8_t * end = data + stride * height;
    for (uint8_t * in = data, *out = retdata;
	 in < end;
	 in += stride, out += stride)
    {
	for (int i = 0; i < width; i ++) {
	    uint8_t *base  = &in[4*i];
	    uint8_t  alpha = base[3];
	    uint32_t p;

	    if (alpha == 0) {
		p = 0;
	    } else {
		uint8_t  blue  = base[0];
		uint8_t  green = base[1];
		uint8_t  red   = base[2];

		if (alpha != 0xff) {
		    blue  = multiply_alpha (alpha, blue);
		    green = multiply_alpha (alpha, green);
		    red   = multiply_alpha (alpha, red);
		}
		p = (alpha << 0) | (red << 8) | (green << 16) | ((uint32_t)blue << 24);
	    }
	    memcpy (&out[4*i], &p, sizeof (uint32_t));
	}
    }
    return retdata;
}

static cairo_int_status_t
_cairo_beos_surface_set_clip_region (cairo_beos_surface_t *surface,
				     cairo_region_t	*region)
{
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							abstract_surface);
    AutoLockView locker(surface->view);
    assert (locker);

    if (region == surface->clip_region)
	return CAIRO_INT_STATUS_SUCCESS;

    cairo_region_destroy (surface->clip_region);
    surface->clip_region = cairo_region_reference (region);

    if (region == NULL) {
	// No clipping
	surface->view->ConstrainClippingRegion(NULL);
	return CAIRO_INT_STATUS_SUCCESS;
    }

    int count = cairo_region_num_rectangles (region);
    BRegion bregion;
    for (int i = 0; i < count; ++i) {
	cairo_rectangle_int_t rect;

	cairo_region_get_rectangle (region, i, &rect);
	// Have to subtract one, because for pixman, the second coordinate
	// lies outside the rectangle.
	bregion.Include (_cairo_rectangle_to_brect (&rect));
    }
    surface->view->ConstrainClippingRegion(&bregion);
    return CAIRO_INT_STATUS_SUCCESS;
}


/**
 * _cairo_beos_bitmap_to_surface:
 *
 * Returns an addrefed image surface for a BBitmap. The bitmap need not outlive
 * the surface.
 **/
static cairo_image_surface_t*
_cairo_beos_bitmap_to_surface (BBitmap* bitmap)
{
    color_space format = bitmap->ColorSpace();
    if (format != B_RGB32 && format != B_RGBA32) {
	BBitmap bmp(bitmap->Bounds(), B_RGB32, true);
	BView view(bitmap->Bounds(), "Cairo bitmap drawing view",
		   B_FOLLOW_ALL_SIDES, 0);
	bmp.AddChild(&view);

	view.LockLooper();

	view.DrawBitmap(bitmap, BPoint(0.0, 0.0));
	view.Sync();

	cairo_image_surface_t* imgsurf = _cairo_beos_bitmap_to_surface(&bmp);

	view.UnlockLooper();
	bmp.RemoveChild(&view);
	return imgsurf;
    }

    cairo_format_t cformat = format == B_RGB32 ?
	                     CAIRO_FORMAT_RGB24 : CAIRO_FORMAT_ARGB32;

    BRect bounds(bitmap->Bounds());
    unsigned char* bits = reinterpret_cast<unsigned char*>(bitmap->Bits());
    int width = bounds.IntegerWidth() + 1;
    int height = bounds.IntegerHeight() + 1;
    unsigned char* premultiplied;
    if (cformat == CAIRO_FORMAT_ARGB32) {
       premultiplied = premultiply_bgra (bits, width, height,
					 bitmap->BytesPerRow());
    } else {
	premultiplied = reinterpret_cast<unsigned char*>(
					_cairo_malloc_ab(bitmap->BytesPerRow(), height));
	if (premultiplied)
	    memcpy(premultiplied, bits, bitmap->BytesPerRow() * height);
    }
    if (!premultiplied)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    cairo_image_surface_t* surf = reinterpret_cast<cairo_image_surface_t*>
	(cairo_image_surface_create_for_data(premultiplied,
					     cformat,
					     width,
					     height,
					     bitmap->BytesPerRow()));
    if (surf->base.status)
	free(premultiplied);
    else
	_cairo_image_surface_assume_ownership_of_data(surf);
    return surf;
}

/**
 * _cairo_image_surface_to_bitmap:
 *
 * Converts an image surface to a BBitmap. The return value must be freed with
 * delete.
 **/
static BBitmap*
_cairo_image_surface_to_bitmap (cairo_image_surface_t* surface)
{
    BRect size(0.0, 0.0, surface->width - 1, surface->height - 1);
    switch (surface->format) {
	case CAIRO_FORMAT_ARGB32: {
	    BBitmap* data = new BBitmap(size, B_RGBA32);
	    unpremultiply_bgra (surface->data,
				surface->width,
				surface->height,
				surface->stride,
				reinterpret_cast<unsigned char*>(data->Bits()));
	    return data;
        }
	case CAIRO_FORMAT_RGB24: {
	    BBitmap* data = new BBitmap(size, B_RGB32);
	    memcpy(data->Bits(), surface->data, surface->height * surface->stride);
	    return data;
	}
	default:
	    assert(0);
	    return NULL;
    }
}

/**
 * _cairo_op_to_be_op:
 *
 * Converts a cairo drawing operator to a beos drawing_mode. Returns true if
 * the operator could be converted, false otherwise.
 **/
static bool
_cairo_op_to_be_op (cairo_operator_t cairo_op,
		    drawing_mode*    beos_op)
{
    switch (cairo_op) {
    case CAIRO_OPERATOR_SOURCE:
	*beos_op = B_OP_COPY;
	return true;
    case CAIRO_OPERATOR_OVER:
	*beos_op = B_OP_ALPHA;
	return true;

    case CAIRO_OPERATOR_ADD:
	// Does not actually work
	// XXX This is a fundamental compositing operator, it has to work!
#if 1
	return false;
#else
	*beos_op = B_OP_ADD;
	return true;
#endif

    case CAIRO_OPERATOR_CLEAR:
	// Does not map to B_OP_ERASE - it replaces the dest with the low
	// color, instead of transparency; could be done by setting low
	// color appropriately.

    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_ATOP:

    case CAIRO_OPERATOR_DEST:
    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_DEST_ATOP:

    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_SATURATE:

    default:
	return false;
    }
}

static cairo_surface_t *
_cairo_beos_surface_create_similar (void            *abstract_surface,
				    cairo_content_t  content,
				    int              width,
				    int              height)
{
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							abstract_surface);

    if (width <= 0)
	width = 1;
    if (height <= 0)
	height = 1;

    BRect rect(0.0, 0.0, width - 1, height - 1);
    BBitmap* bmp;
    switch (content) {
	case CAIRO_CONTENT_ALPHA:
	    return NULL;
	case CAIRO_CONTENT_COLOR_ALPHA:
	    bmp = new BBitmap(rect, B_RGBA32, true);
	    break;
	case CAIRO_CONTENT_COLOR:
	    // Match the color depth
	    if (surface->bitmap) {
		color_space space = surface->bitmap->ColorSpace();
		// No alpha was requested -> make sure not to return
		// a surface with alpha
		if (space == B_RGBA32)
		    space = B_RGB32;
		if (space == B_RGBA15)
		    space = B_RGB15;
		bmp = new BBitmap(rect, space, true);
	    } else {
		BScreen scr(surface->view->Window());
		color_space space = B_RGB32;
		if (scr.IsValid())
		    space = scr.ColorSpace();
		bmp = new BBitmap(rect, space, true);
	    }
	    break;
	default:
	    ASSERT_NOT_REACHED;
	    return NULL;
    }
    BView* view = new BView(rect, "Cairo bitmap view", B_FOLLOW_ALL_SIDES, 0);
    bmp->AddChild(view);
    return _cairo_beos_surface_create_internal(view, bmp, true);
}

static cairo_status_t
_cairo_beos_surface_finish (void *abstract_surface)
{
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							abstract_surface);
    if (surface->owns_bitmap_view) {
	if (surface->bitmap)
	    surface->bitmap->RemoveChild(surface->view);

	delete surface->view;
	delete surface->bitmap;

	surface->view = NULL;
	surface->bitmap = NULL;
    }

    cairo_region_destroy (surface->clip_region);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_beos_surface_acquire_source_image (void                   *abstract_surface,
					  cairo_image_surface_t **image_out,
					  void                  **image_extra)
{
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							abstract_surface);
    AutoLockView locker(surface->view);
    if (!locker)
	return CAIRO_STATUS_NO_MEMORY; /// XXX not exactly right, but what can we do?


    surface->view->Sync();

    if (surface->bitmap) {
	*image_out = _cairo_beos_bitmap_to_surface (surface->bitmap);
	if (unlikely ((*image_out)->base.status))
	    return (*image_out)->base.status;

	*image_extra = NULL;
	return CAIRO_STATUS_SUCCESS;
    }

    BBitmap* bmp;
    if (_cairo_beos_view_to_bitmap(surface->view, &bmp) != OK)
	return CAIRO_STATUS_NO_MEMORY; /// XXX incorrect if the error was NOT_VISIBLE

    *image_out = _cairo_beos_bitmap_to_surface (bmp);
    if (unlikely ((*image_out)->base.status)) {
	delete bmp;
	return (*image_out)->base.status;
    }
    *image_extra = bmp;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_beos_surface_release_source_image (void                  *abstract_surface,
					  cairo_image_surface_t *image,
					  void                  *image_extra)
{
    cairo_surface_destroy (&image->base);

    if (image_extra != NULL) {
	BBitmap* bmp = static_cast<BBitmap*>(image_extra);
	delete bmp;
    }
}

static cairo_status_t
_cairo_beos_surface_acquire_dest_image (void			 *abstract_surface,
                                        cairo_rectangle_int_t	 *interest_rect,
                                        cairo_image_surface_t	**image_out,
                                        cairo_rectangle_int_t	 *image_rect,
                                        void			**image_extra)
{
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							abstract_surface);

    AutoLockView locker(surface->view);
    if (!locker) {
	*image_out = NULL;
	*image_extra = NULL;
	return (cairo_status_t) CAIRO_INT_STATUS_NOTHING_TO_DO;
    }

    if (surface->bitmap) {
	surface->view->Sync();
	*image_out = _cairo_beos_bitmap_to_surface(surface->bitmap);
	if (unlikely ((*image_out)->base.status))
	    return (*image_out)->base.status;

	image_rect->x = 0;
	image_rect->y = 0;
	image_rect->width = (*image_out)->width;
	image_rect->height = (*image_out)->height;

	*image_extra = NULL;
	return CAIRO_STATUS_SUCCESS;
    }

    BRect b_interest_rect (_cairo_rectangle_to_brect (interest_rect));

    BRect rect;
    BBitmap* bitmap;
    ViewCopyStatus status = _cairo_beos_view_to_bitmap(surface->view, &bitmap,
	                                               &rect, &b_interest_rect);
    if (status == NOT_VISIBLE) {
	*image_out = NULL;
	*image_extra = NULL;
	return CAIRO_STATUS_SUCCESS;
    }
    if (status == ERROR)
	return CAIRO_STATUS_NO_MEMORY;

    *image_rect = _brect_to_cairo_rectangle(rect);
    *image_out = _cairo_beos_bitmap_to_surface(bitmap);
    delete bitmap;
    if (unlikely ((*image_out)->base.status))
	return (*image_out)->base.status;

    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}


static void
_cairo_beos_surface_release_dest_image (void			*abstract_surface,
                                        cairo_rectangle_int_t	*intersect_rect,
                                        cairo_image_surface_t	*image,
                                        cairo_rectangle_int_t	*image_rect,
                                        void			*image_extra)
{
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							abstract_surface);

    AutoLockView locker(surface->view);
    if (!locker)
	return;

    BBitmap* bitmap_to_draw = _cairo_image_surface_to_bitmap(image);
    surface->view->PushState();

	surface->view->SetDrawingMode(B_OP_COPY);

	surface->view->DrawBitmap (bitmap_to_draw,
				   _cairo_rectangle_to_brect (image_rect));

    surface->view->PopState();

    delete bitmap_to_draw;
    cairo_surface_destroy(&image->base);
}

static cairo_int_status_t
_cairo_beos_surface_composite (cairo_operator_t		op,
			       cairo_pattern_t	       *src,
			       cairo_pattern_t	       *mask,
			       void		       *dst,
			       int			src_x,
			       int			src_y,
			       int			mask_x,
			       int			mask_y,
			       int			dst_x,
			       int			dst_y,
			       unsigned int		width,
			       unsigned int		height,
			       cairo_region_t		*clip_region)
{
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							dst);
    cairo_int_status_t status;
    AutoLockView locker(surface->view);
    if (!locker)
	return CAIRO_INT_STATUS_SUCCESS;

    drawing_mode mode;
    if (!_cairo_op_to_be_op(op, &mode))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    // XXX Masks are not yet supported
    if (mask)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    // XXX should eventually support the others
    if (src->type != CAIRO_PATTERN_TYPE_SURFACE ||
	src->extend != CAIRO_EXTEND_NONE)
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    // Can we maybe support other matrices as well? (scale? if the filter is right)
    int itx, ity;
    if (!_cairo_matrix_is_integer_translation(&src->matrix, &itx, &ity))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_beos_surface_set_clip_region (surface, clip_region);
    if (unlikely (status))
	return status;

    BRect srcRect(src_x + itx,
		  src_y + ity,
		  src_x + itx + width - 1,
		  src_y + ity + height - 1);
    BRect dstRect(dst_x, dst_y, dst_x + width - 1, dst_y + height - 1);

    cairo_surface_t* src_surface = reinterpret_cast<cairo_surface_pattern_t*>(src)->
					surface;

    // Get a bitmap
    BBitmap* bmp = NULL;
    bool free_bmp = false;
    if (_cairo_surface_is_image(src_surface)) {
	cairo_image_surface_t* img_surface =
	    reinterpret_cast<cairo_image_surface_t*>(src_surface);

	bmp = _cairo_image_surface_to_bitmap(img_surface);
	free_bmp = true;
    } else if (src_surface->backend == surface->base.backend) {
	cairo_beos_surface_t *beos_surface =
	    reinterpret_cast<cairo_beos_surface_t*>(src_surface);
	if (beos_surface->bitmap) {
	    AutoLockView locker(beos_surface->view);
	    if (locker)
		beos_surface->view->Sync();
	    bmp = beos_surface->bitmap;
	} else {
	    _cairo_beos_view_to_bitmap(surface->view, &bmp);
	    free_bmp = true;
	}
    }

    if (!bmp)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    // So, BeOS seems to screw up painting an opaque bitmap onto a
    // translucent one (it makes them partly transparent). Just return
    // unsupported.
    if (bmp->ColorSpace() == B_RGB32 && surface->bitmap &&
	surface->bitmap->ColorSpace() == B_RGBA32 &&
	(mode == B_OP_COPY || mode == B_OP_ALPHA))
    {
	if (free_bmp)
	    delete bmp;
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    // Draw it on screen.
    surface->view->PushState();

	// If our image rect is only a subrect of the desired size, and we
	// aren't using B_OP_ALPHA, then we need to fill the rect first.
	if (mode == B_OP_COPY && !bmp->Bounds().Contains(srcRect)) {
	    rgb_color black = { 0, 0, 0, 0 };

	    surface->view->SetDrawingMode(mode);
	    surface->view->SetHighColor(black);
	    surface->view->FillRect(dstRect);
	}

	if (mode == B_OP_ALPHA && bmp->ColorSpace() == B_RGB32) {
	    mode = B_OP_COPY;
	}
	surface->view->SetDrawingMode(mode);

	if (surface->bitmap && surface->bitmap->ColorSpace() == B_RGBA32)
	    surface->view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
	else
	    surface->view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

	surface->view->DrawBitmap(bmp, srcRect, dstRect);

    surface->view->PopState();

    if (free_bmp)
	delete bmp;

    return CAIRO_INT_STATUS_SUCCESS;
}


static cairo_int_status_t
_cairo_beos_surface_fill_rectangles (void			*abstract_surface,
				     cairo_operator_t		 op,
				     const cairo_color_t	*color,
				     cairo_rectangle_int_t	*rects,
				     int			 num_rects,
				     cairo_region_t		*clip_region)
{
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							abstract_surface);
    cairo_int_status_t status;

    if (num_rects <= 0)
	return CAIRO_INT_STATUS_SUCCESS;

    AutoLockView locker(surface->view);
    if (!locker)
	return CAIRO_INT_STATUS_SUCCESS;

    drawing_mode mode;
    if (!_cairo_op_to_be_op(op, &mode))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_beos_surface_set_clip_region (surface, clip_region);
    if (unlikely (status))
	return status;

    rgb_color be_color = _cairo_color_to_be_color(color);

    if (mode == B_OP_ALPHA && be_color.alpha == 0xFF)
	mode = B_OP_COPY;

    // For CAIRO_OPERATOR_SOURCE, cairo expects us to use the premultiplied
    // color info. This is only relevant when drawing into an rgb24 buffer
    // (as for others, we can convert when asked for the image)
    if (mode == B_OP_COPY && be_color.alpha != 0xFF &&
	(!surface->bitmap || surface->bitmap->ColorSpace() != B_RGBA32))
    {
	be_color.red   = color->red_short  >> 8;
	be_color.green = color->green_short >> 8;
	be_color.blue  = color->blue_short  >> 8;
    }

    surface->view->PushState();

	surface->view->SetDrawingMode(mode);
	surface->view->SetHighColor(be_color);
	if (surface->bitmap && surface->bitmap->ColorSpace() == B_RGBA32)
	    surface->view->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_COMPOSITE);
	else
	    surface->view->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);

	for (int i = 0; i < num_rects; ++i)
	    surface->view->FillRect (_cairo_rectangle_to_brect (&rects[i]));

    surface->view->PopState();

    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_beos_surface_get_extents (void				*abstract_surface,
				 cairo_rectangle_int_t	*rectangle)
{
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							abstract_surface);
    AutoLockView locker(surface->view);
    if (!locker)
	return FALSE;

    *rectangle = _brect_to_cairo_rectangle (surface->view->Bounds());
    return TRUE;
}

static const struct _cairo_surface_backend cairo_beos_surface_backend = {
    CAIRO_SURFACE_TYPE_BEOS,
    _cairo_beos_surface_create_similar,
    _cairo_beos_surface_finish,
    _cairo_beos_surface_acquire_source_image,
    _cairo_beos_surface_release_source_image,
    _cairo_beos_surface_acquire_dest_image,
    _cairo_beos_surface_release_dest_image,
    NULL, /* clone_similar */
    _cairo_beos_surface_composite, /* composite */
    _cairo_beos_surface_fill_rectangles,
    NULL, /* composite_trapezoids */
    NULL, /* create_span_renderer */
    NULL, /* check_span_renderer */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_beos_surface_get_extents,
    NULL,  /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
    NULL  /* show_glyphs */
};

static cairo_surface_t *
_cairo_beos_surface_create_internal (BView*   view,
				     BBitmap* bmp,
				     bool     owns_bitmap_view)
{
    // Must use malloc, because cairo code will use free() on the surface
    cairo_beos_surface_t *surface = static_cast<cairo_beos_surface_t*>(
					malloc(sizeof(cairo_beos_surface_t)));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
        return const_cast<cairo_surface_t*>(&_cairo_surface_nil);
    }

    cairo_content_t content = CAIRO_CONTENT_COLOR;
    if (bmp && (bmp->ColorSpace() == B_RGBA32 || bmp->ColorSpace() == B_RGBA15))
	content = CAIRO_CONTENT_COLOR_ALPHA;
    _cairo_surface_init (&surface->base,
			 &cairo_beos_surface_backend,
			 NULL, /* device */
			 content);

    surface->view = view;
    surface->bitmap = bmp;
    surface->owns_bitmap_view = owns_bitmap_view;

    surface->clip_region = NULL;

    return &surface->base;
}

/**
 * cairo_beos_surface_create:
 * @view: The view to draw on
 *
 * Creates a Cairo surface that draws onto a BeOS BView.
 * The caller must ensure that the view does not get deleted before the surface.
 * If the view is attached to a bitmap rather than an on-screen window, use
 * cairo_beos_surface_create_for_bitmap() instead of this function.
 *
 * Since: TBD
 **/
cairo_surface_t *
cairo_beos_surface_create (BView* view)
{
    return cairo_beos_surface_create_for_bitmap(view, NULL);
}

/**
 * cairo_beos_surface_create_for_bitmap:
 * @view: The view to draw on
 * @bmp: The bitmap to which the view is attached
 *
 * Creates a Cairo surface that draws onto a BeOS BView which is attached to a
 * BBitmap.
 * The caller must ensure that the view and the bitmap do not get deleted
 * before the surface.
 *
 * For views that draw to a bitmap (as opposed to a screen), use this function
 * rather than cairo_beos_surface_create(). Not using this function WILL lead to
 * incorrect behaviour.
 *
 * For now, only views that draw to the entire area of bmp are supported.
 * The view must already be attached to the bitmap.
 *
 * Since: TBD
 **/
cairo_surface_t *
cairo_beos_surface_create_for_bitmap (BView*   view,
				      BBitmap* bmp)
{
    return _cairo_beos_surface_create_internal(view, bmp);
}

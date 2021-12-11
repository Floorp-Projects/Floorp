/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc.
 * Copyright © 2012 Intel Corporation
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Owen Taylor <otaylor@redhat.com>
 *	Stuart Parmenter <stuart@mozilla.com>
 *	Vladimir Vukicevic <vladimir@pobox.com>
 */

#define WIN32_LEAN_AND_MEAN
/* We require Windows 2000 features such as ETO_PDY */
#if !defined(WINVER) || (WINVER < 0x0500)
# define WINVER 0x0500
#endif
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0500)
# define _WIN32_WINNT 0x0500
#endif

#include "cairoint.h"

#include "cairo-clip-private.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-compositor-private.h"
#include "cairo-damage-private.h"
#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-inline.h"
#include "cairo-paginated-private.h"
#include "cairo-pattern-private.h"
#include "cairo-win32-private.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-surface-fallback-private.h"
#include "cairo-surface-backend-private.h"

#include <wchar.h>
#include <windows.h>

#if defined(__MINGW32__) && !defined(ETO_PDY)
# define ETO_PDY 0x2000
#endif

#define PELS_72DPI  ((LONG)(72. / 0.0254))

/**
 * SECTION:cairo-win32
 * @Title: Win32 Surfaces
 * @Short_Description: Microsoft Windows surface support
 * @See_Also: #cairo_surface_t
 *
 * The Microsoft Windows surface is used to render cairo graphics to
 * Microsoft Windows windows, bitmaps, and printing device contexts.
 *
 * The surface returned by cairo_win32_printing_surface_create() is of surface
 * type %CAIRO_SURFACE_TYPE_WIN32_PRINTING and is a multi-page vector surface
 * type.
 *
 * The surface returned by the other win32 constructors is of surface type
 * %CAIRO_SURFACE_TYPE_WIN32 and is a raster surface type.
 **/

/**
 * CAIRO_HAS_WIN32_SURFACE:
 *
 * Defined if the Microsoft Windows surface backend is available.
 * This macro can be used to conditionally compile backend-specific code.
 *
 * Since: 1.0
 **/

static const cairo_surface_backend_t cairo_win32_display_surface_backend;

static cairo_status_t
_create_dc_and_bitmap (cairo_win32_display_surface_t *surface,
		       HDC                    original_dc,
		       cairo_format_t         format,
		       int                    width,
		       int                    height,
		       unsigned char        **bits_out,
		       int                   *rowstride_out)
{
    cairo_status_t status;

    BITMAPINFO *bitmap_info = NULL;
    struct {
	BITMAPINFOHEADER bmiHeader;
	RGBQUAD bmiColors[2];
    } bmi_stack;
    void *bits;

    int num_palette = 0;	/* Quiet GCC */
    int i;

    surface->win32.dc = NULL;
    surface->bitmap = NULL;
    surface->is_dib = FALSE;

    switch (format) {
    default:
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_RGB30:
	return _cairo_error (CAIRO_STATUS_INVALID_FORMAT);
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
	num_palette = 0;
	break;

    case CAIRO_FORMAT_A8:
	num_palette = 256;
	break;

    case CAIRO_FORMAT_A1:
	num_palette = 2;
	break;
    }

    if (num_palette > 2) {
	bitmap_info = _cairo_malloc_ab_plus_c (num_palette, sizeof(RGBQUAD), sizeof(BITMAPINFOHEADER));
	if (!bitmap_info)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    } else {
	bitmap_info = (BITMAPINFO *)&bmi_stack;
    }

    bitmap_info->bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bitmap_info->bmiHeader.biWidth = width == 0 ? 1 : width;
    bitmap_info->bmiHeader.biHeight = height == 0 ? -1 : - height; /* top-down */
    bitmap_info->bmiHeader.biSizeImage = 0;
    bitmap_info->bmiHeader.biXPelsPerMeter = PELS_72DPI; /* unused here */
    bitmap_info->bmiHeader.biYPelsPerMeter = PELS_72DPI; /* unused here */
    bitmap_info->bmiHeader.biPlanes = 1;

    switch (format) {
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_RGB30:
	ASSERT_NOT_REACHED;
    /* We can't create real RGB24 bitmaps because something seems to
     * break if we do, especially if we don't set up an image
     * fallback.  It could be a bug with using a 24bpp pixman image
     * (and creating one with masks).  So treat them like 32bpp.
     * Note: This causes problems when using BitBlt/AlphaBlend/etc!
     * see end of file.
     */
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
	bitmap_info->bmiHeader.biBitCount = 32;
	bitmap_info->bmiHeader.biCompression = BI_RGB;
	bitmap_info->bmiHeader.biClrUsed = 0;	/* unused */
	bitmap_info->bmiHeader.biClrImportant = 0;
	break;

    case CAIRO_FORMAT_A8:
	bitmap_info->bmiHeader.biBitCount = 8;
	bitmap_info->bmiHeader.biCompression = BI_RGB;
	bitmap_info->bmiHeader.biClrUsed = 256;
	bitmap_info->bmiHeader.biClrImportant = 0;

	for (i = 0; i < 256; i++) {
	    bitmap_info->bmiColors[i].rgbBlue = i;
	    bitmap_info->bmiColors[i].rgbGreen = i;
	    bitmap_info->bmiColors[i].rgbRed = i;
	    bitmap_info->bmiColors[i].rgbReserved = 0;
	}
	break;

    case CAIRO_FORMAT_A1:
	bitmap_info->bmiHeader.biBitCount = 1;
	bitmap_info->bmiHeader.biCompression = BI_RGB;
	bitmap_info->bmiHeader.biClrUsed = 2;
	bitmap_info->bmiHeader.biClrImportant = 0;

	for (i = 0; i < 2; i++) {
	    bitmap_info->bmiColors[i].rgbBlue = i * 255;
	    bitmap_info->bmiColors[i].rgbGreen = i * 255;
	    bitmap_info->bmiColors[i].rgbRed = i * 255;
	    bitmap_info->bmiColors[i].rgbReserved = 0;
	}
	break;
    }

    surface->win32.dc = CreateCompatibleDC (original_dc);
    if (!surface->win32.dc)
	goto FAIL;

    surface->bitmap = CreateDIBSection (surface->win32.dc,
					bitmap_info,
					DIB_RGB_COLORS,
					&bits,
					NULL, 0);
    if (!surface->bitmap)
	goto FAIL;

    surface->is_dib = TRUE;

    GdiFlush();

    surface->saved_dc_bitmap = SelectObject (surface->win32.dc,
					     surface->bitmap);
    if (!surface->saved_dc_bitmap)
	goto FAIL;

    if (bitmap_info && num_palette > 2)
	free (bitmap_info);

    if (bits_out)
	*bits_out = bits;

    if (rowstride_out) {
	/* Windows bitmaps are padded to 32-bit (dword) boundaries */
	switch (format) {
	case CAIRO_FORMAT_INVALID:
	case CAIRO_FORMAT_RGB16_565:
	case CAIRO_FORMAT_RGB30:
	    ASSERT_NOT_REACHED;
	case CAIRO_FORMAT_ARGB32:
	case CAIRO_FORMAT_RGB24:
	    *rowstride_out = 4 * width;
	    break;

	case CAIRO_FORMAT_A8:
	    *rowstride_out = (width + 3) & ~3;
	    break;

	case CAIRO_FORMAT_A1:
	    *rowstride_out = ((width + 31) & ~31) / 8;
	    break;
	}
    }

    surface->win32.flags = _cairo_win32_flags_for_dc (surface->win32.dc, format);

    return CAIRO_STATUS_SUCCESS;

 FAIL:
    status = _cairo_win32_print_gdi_error (__FUNCTION__);

    if (bitmap_info && num_palette > 2)
	free (bitmap_info);

    if (surface->saved_dc_bitmap) {
	SelectObject (surface->win32.dc, surface->saved_dc_bitmap);
	surface->saved_dc_bitmap = NULL;
    }

    if (surface->bitmap) {
	DeleteObject (surface->bitmap);
	surface->bitmap = NULL;
    }

    if (surface->win32.dc) {
	DeleteDC (surface->win32.dc);
	surface->win32.dc = NULL;
    }

    return status;
}

static cairo_surface_t *
_cairo_win32_display_surface_create_for_dc (HDC             original_dc,
					    cairo_format_t  format,
					    int		    width,
					    int		    height)
{
    cairo_status_t status;
    cairo_device_t *device;
    cairo_win32_display_surface_t *surface;
    unsigned char *bits;
    int rowstride;

    surface = _cairo_malloc (sizeof (*surface));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    surface->fallback = NULL;

    status = _create_dc_and_bitmap (surface, original_dc, format,
				    width, height,
				    &bits, &rowstride);
    if (status)
	goto FAIL;

    surface->image = cairo_image_surface_create_for_data (bits, format,
							  width, height, rowstride);
    status = surface->image->status;
    if (status)
	goto FAIL;

    _cairo_image_surface_set_parent (to_image_surface(surface->image),
				     &surface->win32.base);

    surface->win32.format = format;

    surface->win32.extents.x = 0;
    surface->win32.extents.y = 0;
    surface->win32.extents.width = width;
    surface->win32.extents.height = height;
    surface->win32.x_ofs = 0;
    surface->win32.y_ofs = 0;

    surface->initial_clip_rgn = NULL;
    surface->had_simple_clip = FALSE;

    device = _cairo_win32_device_get ();

    _cairo_surface_init (&surface->win32.base,
			 &cairo_win32_display_surface_backend,
			 device,
			 _cairo_content_from_format (format),
			 FALSE); /* is_vector */

    cairo_device_destroy (device);

    return &surface->win32.base;

 FAIL:
    if (surface->bitmap) {
	SelectObject (surface->win32.dc, surface->saved_dc_bitmap);
	DeleteObject (surface->bitmap);
	DeleteDC (surface->win32.dc);
    }
    free (surface);

    return _cairo_surface_create_in_error (status);
}

static cairo_surface_t *
_cairo_win32_display_surface_create_similar (void	    *abstract_src,
					     cairo_content_t content,
					     int	     width,
					     int	     height)
{
    cairo_win32_display_surface_t *src = abstract_src;
    cairo_format_t format = _cairo_format_from_content (content);
    cairo_surface_t *new_surf = NULL;

    /* We force a DIB always if:
     * - we need alpha; or
     * - the parent is a DIB; or
     * - the parent is for printing (because we don't care about the
     *   bit depth at that point)
     *
     * We also might end up with a DIB even if a DDB is requested if
     * DDB creation failed due to out of memory.
     */
    if (!(src->is_dib || content & CAIRO_CONTENT_ALPHA)) {
	/* try to create a ddb */
	new_surf = cairo_win32_surface_create_with_ddb (src->win32.dc, CAIRO_FORMAT_RGB24, width, height);

	if (new_surf->status)
	    new_surf = NULL;
    }

    if (new_surf == NULL) {
	new_surf = _cairo_win32_display_surface_create_for_dc (src->win32.dc, format, width, height);
    }

    return new_surf;
}

static cairo_surface_t *
_cairo_win32_display_surface_create_similar_image (void	    *abstract_other,
						   cairo_format_t format,
						   int	     width,
						   int	     height)
{
    cairo_win32_display_surface_t *surface = abstract_other;
    cairo_image_surface_t *image;

    surface = (cairo_win32_display_surface_t *)
	_cairo_win32_display_surface_create_for_dc (surface->win32.dc,
						    format, width, height);
    if (surface->win32.base.status)
	return &surface->win32.base;

    /* And clear in order to comply with our user API semantics */
    image = (cairo_image_surface_t *) surface->image;
    if (! image->base.is_clear) {
	memset (image->data, 0, image->stride * height);
	image->base.is_clear = TRUE;
    }

    return &image->base;
}

static cairo_status_t
_cairo_win32_display_surface_finish (void *abstract_surface)
{
    cairo_win32_display_surface_t *surface = abstract_surface;

    if (surface->image && to_image_surface(surface->image)->parent) {
	assert (to_image_surface(surface->image)->parent == &surface->win32.base);
	/* Unhook ourselves first to avoid the double-unref from the image */
	to_image_surface(surface->image)->parent = NULL;
	cairo_surface_finish (surface->image);
	cairo_surface_destroy (surface->image);
    }

    /* If we created the Bitmap and DC, destroy them */
    if (surface->bitmap) {
	SelectObject (surface->win32.dc, surface->saved_dc_bitmap);
	DeleteObject (surface->bitmap);
	DeleteDC (surface->win32.dc);
    }

    _cairo_win32_display_surface_discard_fallback (surface);

    if (surface->initial_clip_rgn)
	DeleteObject (surface->initial_clip_rgn);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_image_surface_t *
_cairo_win32_display_surface_map_to_image (void                    *abstract_surface,
					   const cairo_rectangle_int_t   *extents)
{
    cairo_win32_display_surface_t *surface = abstract_surface;
    cairo_status_t status;

    TRACE ((stderr, "%s (surface=%d)\n",
	    __FUNCTION__, surface->win32.base.unique_id));

    if (surface->image)
	goto done;

    if (surface->fallback == NULL) {
	surface->fallback =
	    _cairo_win32_display_surface_create_for_dc (surface->win32.dc,
							surface->win32.format,
							surface->win32.extents.x + surface->win32.extents.width,
							surface->win32.extents.y + surface->win32.extents.height);
	if (unlikely (status = surface->fallback->status))
	    goto err;

	if (!BitBlt (to_win32_surface(surface->fallback)->dc,
		     surface->win32.extents.x, surface->win32.extents.y,
		     surface->win32.extents.width,
		     surface->win32.extents.height,
		     surface->win32.dc,
		     surface->win32.extents.x + surface->win32.x_ofs, /* Handling multi-monitor... */
		     surface->win32.extents.y + surface->win32.y_ofs, /* ... setup on Win32 */
		     SRCCOPY)) {
	    status = _cairo_error (CAIRO_STATUS_DEVICE_ERROR);
	    goto err;
	}
    }

    surface = to_win32_display_surface (surface->fallback);
done:
    GdiFlush();
    return _cairo_surface_map_to_image (surface->image, extents);

err:
    cairo_surface_destroy (surface->fallback);
    surface->fallback = NULL;

    return _cairo_image_surface_create_in_error (status);
}

static cairo_int_status_t
_cairo_win32_display_surface_unmap_image (void                    *abstract_surface,
					  cairo_image_surface_t   *image)
{
    cairo_win32_display_surface_t *surface = abstract_surface;

    /* Delay the download until the next flush, which means we also need
     * to make sure our sources rare flushed.
     */
    TRACE ((stderr, "%s (surface=%d)\n",
	    __FUNCTION__, to_win32_surface(surface)->base.unique_id));

    if (surface->fallback) {
	cairo_rectangle_int_t r;

	r.x = image->base.device_transform_inverse.x0;
	r.y = image->base.device_transform_inverse.y0;
	r.width  = image->width;
	r.height = image->height;

	TRACE ((stderr, "%s: adding damage (%d,%d)x(%d,%d)\n",
		__FUNCTION__, r.x, r.y, r.width, r.height));
	surface->fallback->damage =
	    _cairo_damage_add_rectangle (surface->fallback->damage, &r);
	surface = to_win32_display_surface (surface->fallback);
    }

    return _cairo_surface_unmap_image (surface->image, image);
}

static cairo_status_t
_cairo_win32_display_surface_flush (void *abstract_surface, unsigned flags)
{
    cairo_win32_display_surface_t *surface = abstract_surface;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (flags)
	return CAIRO_STATUS_SUCCESS;

    TRACE ((stderr, "%s (surface=%d)\n",
	    __FUNCTION__, surface->win32.base.unique_id));
    if (surface->fallback == NULL)
	return CAIRO_STATUS_SUCCESS;

    if (surface->fallback->damage) {
	cairo_win32_display_surface_t *fallback;
	cairo_damage_t *damage;

	damage = _cairo_damage_reduce (surface->fallback->damage);
	surface->fallback->damage = NULL;

	fallback = to_win32_display_surface (surface->fallback);
	assert (fallback->image);

	TRACE ((stderr, "%s: flushing damage x %d\n", __FUNCTION__,
		damage->region ? cairo_region_num_rectangles (damage->region) : 0));

	if (damage->status) {
	    if (!BitBlt (surface->win32.dc,
			 surface->win32.extents.x + surface->win32.x_ofs, /* Handling multi-monitor... */
			 surface->win32.extents.y + surface->win32.y_ofs, /* ... setup on Win32 */
			 surface->win32.extents.width,
			 surface->win32.extents.height,
			 fallback->win32.dc,
			 surface->win32.extents.x, surface->win32.extents.y,
			 SRCCOPY))
		status = _cairo_win32_print_gdi_error (__FUNCTION__);
	} else if (damage->region) {
	    int n = cairo_region_num_rectangles (damage->region), i;
	    for (i = 0; i < n; i++) {
		cairo_rectangle_int_t rect;

		cairo_region_get_rectangle (damage->region, i, &rect);
		TRACE ((stderr, "%s: damage (%d,%d)x(%d,%d)\n", __FUNCTION__,
			rect.x, rect.y,
			rect.width, rect.height));
		if (!BitBlt (surface->win32.dc,
			     rect.x + surface->win32.x_ofs, /* Handling multi-monitor... */
			     rect.y + surface->win32.y_ofs, /* ... setup on Win32 */
			     rect.width, rect.height,
			     fallback->win32.dc,
			     rect.x, rect.y,
			     SRCCOPY)) {
		    status = _cairo_win32_print_gdi_error (__FUNCTION__);
		    break;
		}
	    }
	}
	_cairo_damage_destroy (damage);
    } else {
	cairo_surface_destroy (surface->fallback);
	surface->fallback = NULL;
    }

    return status;
}

static cairo_status_t
_cairo_win32_display_surface_mark_dirty (void *abstract_surface,
					 int x, int y, int width, int height)
{
    _cairo_win32_display_surface_discard_fallback (abstract_surface);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_win32_save_initial_clip (HDC hdc, cairo_win32_display_surface_t *surface)
{
    RECT rect;
    int clipBoxType;
    int gm;
    XFORM saved_xform;

    /* GetClipBox/GetClipRgn and friends interact badly with a world transform
     * set.  GetClipBox returns values in logical (transformed) coordinates;
     * it's unclear what GetClipRgn returns, because the region is empty in the
     * case of a SIMPLEREGION clip, but I assume device (untransformed) coordinates.
     * Similarly, IntersectClipRect works in logical units, whereas SelectClipRgn
     * works in device units.
     *
     * So, avoid the whole mess and get rid of the world transform
     * while we store our initial data and when we restore initial coordinates.
     *
     * XXX we may need to modify x/y by the ViewportOrg or WindowOrg
     * here in GM_COMPATIBLE; unclear.
     */
    gm = GetGraphicsMode (hdc);
    if (gm == GM_ADVANCED) {
	GetWorldTransform (hdc, &saved_xform);
	ModifyWorldTransform (hdc, NULL, MWT_IDENTITY);
    }

    clipBoxType = GetClipBox (hdc, &rect);
    if (clipBoxType == ERROR) {
	_cairo_win32_print_gdi_error (__FUNCTION__);
	SetGraphicsMode (hdc, gm);
	/* XXX: Can we make a more reasonable guess at the error cause here? */
	return _cairo_error (CAIRO_STATUS_DEVICE_ERROR);
    }

    surface->win32.extents.x = rect.left;
    surface->win32.extents.y = rect.top;
    surface->win32.extents.width = rect.right - rect.left;
    surface->win32.extents.height = rect.bottom - rect.top;

    /* On multi-monitor setup, under Windows, the primary monitor always
     * have origin (0,0).  Any monitors that extends to the left or above
     * will have coordinates in the negative range.  Take this into
     * account, by forcing our Win32 surface to start at extent (0,0) and
     * using a device offset.  Cairo does not handle extents with negative
     * offsets.
     */
    surface->win32.x_ofs = 0;
    surface->win32.y_ofs = 0;
    if ((surface->win32.extents.x < 0) ||
	(surface->win32.extents.y < 0)) {
	/* Negative offsets occurs for (and ONLY for) the desktop DC (virtual
	 * desktop), when a monitor extend to the left or above the primary
	 * monitor.
	 *
	 * More info @ https://www.microsoft.com/msj/0697/monitor/monitor.aspx
	 *
	 * Note that any other DC, including memory DC created with
	 * CreateCompatibleDC(<virtual desktop DC>) will have extents in the
	 * positive range.  This will be taken into account later when we perform
	 * raster operations between the DC (may have to perform offset
	 * translation).
	 */
	surface->win32.x_ofs = surface->win32.extents.x;
	surface->win32.y_ofs = surface->win32.extents.y;
	surface->win32.extents.x = 0;
	surface->win32.extents.y = 0;
    }

    surface->initial_clip_rgn = NULL;
    surface->had_simple_clip = FALSE;

    if (clipBoxType == COMPLEXREGION) {
	surface->initial_clip_rgn = CreateRectRgn (0, 0, 0, 0);
	if (GetClipRgn (hdc, surface->initial_clip_rgn) <= 0) {
	    DeleteObject(surface->initial_clip_rgn);
	    surface->initial_clip_rgn = NULL;
	}
    } else if (clipBoxType == SIMPLEREGION) {
	surface->had_simple_clip = TRUE;
    }

    if (gm == GM_ADVANCED)
	SetWorldTransform (hdc, &saved_xform);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_win32_display_surface_set_clip (cairo_win32_display_surface_t *surface,
				       cairo_clip_t *clip)
{
    char stack[512];
    cairo_rectangle_int_t extents;
    int num_rects;
    RGNDATA *data;
    size_t data_size;
    RECT *rects;
    int i;
    HRGN gdi_region;
    cairo_status_t status;
    cairo_region_t *region;

    /* The semantics we want is that any clip set by cairo combines
     * is intersected with the clip on device context that the
     * surface was created for. To implement this, we need to
     * save the original clip when first setting a clip on surface.
     */

    assert (_cairo_clip_is_region (clip));
    region = _cairo_clip_get_region (clip);
    if (region == NULL)
	return CAIRO_STATUS_SUCCESS;

    cairo_region_get_extents (region, &extents);
    num_rects = cairo_region_num_rectangles (region);

    /* XXX see notes in _cairo_win32_save_initial_clip --
     * this code will interact badly with a HDC which had an initial
     * world transform -- we should probably manually transform the
     * region rects, because SelectClipRgn takes device units, not
     * logical units (unlike IntersectClipRect).
     */

    data_size = sizeof (RGNDATAHEADER) + num_rects * sizeof (RECT);
    if (data_size > sizeof (stack)) {
	data = _cairo_malloc (data_size);
	if (!data)
	    return _cairo_error(CAIRO_STATUS_NO_MEMORY);
    } else
	data = (RGNDATA *)stack;

    data->rdh.dwSize = sizeof (RGNDATAHEADER);
    data->rdh.iType = RDH_RECTANGLES;
    data->rdh.nCount = num_rects;
    data->rdh.nRgnSize = num_rects * sizeof (RECT);
    data->rdh.rcBound.left = extents.x;
    data->rdh.rcBound.top = extents.y;
    data->rdh.rcBound.right = extents.x + extents.width;
    data->rdh.rcBound.bottom = extents.y + extents.height;

    rects = (RECT *)data->Buffer;
    for (i = 0; i < num_rects; i++) {
	cairo_rectangle_int_t rect;

	cairo_region_get_rectangle (region, i, &rect);

	rects[i].left   = rect.x;
	rects[i].top    = rect.y;
	rects[i].right  = rect.x + rect.width;
	rects[i].bottom = rect.y + rect.height;
    }

    gdi_region = ExtCreateRegion (NULL, data_size, data);
    if ((char *)data != stack)
	free (data);

    if (!gdi_region)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    /* AND the new region into our DC */
    status = CAIRO_STATUS_SUCCESS;
    if (ExtSelectClipRgn (surface->win32.dc, gdi_region, RGN_AND) == ERROR)
	status = _cairo_win32_print_gdi_error (__FUNCTION__);

    DeleteObject (gdi_region);

    return status;
}

void
_cairo_win32_display_surface_unset_clip (cairo_win32_display_surface_t *surface)
{
    XFORM saved_xform;
    int gm = GetGraphicsMode (surface->win32.dc);
    if (gm == GM_ADVANCED) {
	GetWorldTransform (surface->win32.dc, &saved_xform);
	ModifyWorldTransform (surface->win32.dc, NULL, MWT_IDENTITY);
    }

    /* initial_clip_rgn will either be a real region or NULL (which means reset to no clip region) */
    SelectClipRgn (surface->win32.dc, surface->initial_clip_rgn);

    if (surface->had_simple_clip) {
	/* then if we had a simple clip, intersect */
	IntersectClipRect (surface->win32.dc,
			   surface->win32.extents.x,
			   surface->win32.extents.y,
			   surface->win32.extents.x + surface->win32.extents.width,
			   surface->win32.extents.y + surface->win32.extents.height);
    }

    if (gm == GM_ADVANCED)
	SetWorldTransform (surface->win32.dc, &saved_xform);
}

void
_cairo_win32_display_surface_discard_fallback (cairo_win32_display_surface_t *surface)
{
    if (surface->fallback) {
	TRACE ((stderr, "%s (surface=%d)\n",
		__FUNCTION__, surface->win32.base.unique_id));

	cairo_surface_finish (surface->fallback);
	cairo_surface_destroy (surface->fallback);
	surface->fallback = NULL;
    }
}

static cairo_int_status_t
_cairo_win32_display_surface_paint (void			*surface,
				    cairo_operator_t		 op,
				    const cairo_pattern_t	*source,
				    const cairo_clip_t		*clip)
{
    cairo_win32_device_t *device = to_win32_device_from_surface (surface);

    TRACE ((stderr, "%s (surface=%d)\n",
	    __FUNCTION__, to_win32_surface(surface)->base.unique_id));

    if (clip == NULL &&
	(op == CAIRO_OPERATOR_SOURCE || op == CAIRO_OPERATOR_CLEAR))
	_cairo_win32_display_surface_discard_fallback (surface);

    return _cairo_compositor_paint (device->compositor,
				    surface, op, source, clip);
}

static cairo_int_status_t
_cairo_win32_display_surface_mask (void				*surface,
				   cairo_operator_t		 op,
				   const cairo_pattern_t	*source,
				   const cairo_pattern_t	*mask,
				   const cairo_clip_t		*clip)
{
    cairo_win32_device_t *device = to_win32_device_from_surface (surface);

    TRACE ((stderr, "%s (surface=%d)\n",
	    __FUNCTION__, to_win32_surface(surface)->base.unique_id));

    if (clip == NULL && op == CAIRO_OPERATOR_SOURCE)
	_cairo_win32_display_surface_discard_fallback (surface);

    return _cairo_compositor_mask (device->compositor,
				   surface, op, source, mask, clip);
}

static cairo_int_status_t
_cairo_win32_display_surface_stroke (void			*surface,
				     cairo_operator_t		 op,
				     const cairo_pattern_t	*source,
				     const cairo_path_fixed_t	*path,
				     const cairo_stroke_style_t	*style,
				     const cairo_matrix_t	*ctm,
				     const cairo_matrix_t	*ctm_inverse,
				     double			 tolerance,
				     cairo_antialias_t		 antialias,
				     const cairo_clip_t		*clip)
{
    cairo_win32_device_t *device = to_win32_device_from_surface (surface);

    TRACE ((stderr, "%s (surface=%d)\n",
	    __FUNCTION__, to_win32_surface(surface)->base.unique_id));

    return _cairo_compositor_stroke (device->compositor, surface,
				     op, source, path,
				     style, ctm, ctm_inverse,
				     tolerance, antialias, clip);
}

static cairo_int_status_t
_cairo_win32_display_surface_fill (void				*surface,
				   cairo_operator_t		 op,
				   const cairo_pattern_t	*source,
				   const cairo_path_fixed_t	*path,
				   cairo_fill_rule_t		 fill_rule,
				   double			 tolerance,
				   cairo_antialias_t		 antialias,
				   const cairo_clip_t		*clip)
{
    cairo_win32_device_t *device = to_win32_device_from_surface (surface);

    TRACE ((stderr, "%s (surface=%d)\n",
	    __FUNCTION__, to_win32_surface(surface)->base.unique_id));

    return _cairo_compositor_fill (device->compositor, surface,
				   op, source, path,
				   fill_rule, tolerance, antialias,
				   clip);
}

static cairo_int_status_t
_cairo_win32_display_surface_glyphs (void			*surface,
				     cairo_operator_t		 op,
				     const cairo_pattern_t	*source,
				     cairo_glyph_t		*glyphs,
				     int			 num_glyphs,
				     cairo_scaled_font_t	*scaled_font,
				     const cairo_clip_t		*clip)
{
    cairo_win32_device_t *device = to_win32_device_from_surface (surface);

    TRACE ((stderr, "%s (surface=%d)\n",
	    __FUNCTION__, to_win32_surface(surface)->base.unique_id));

    return _cairo_compositor_glyphs (device->compositor, surface,
				     op, source,
				     glyphs, num_glyphs, scaled_font,
				     clip);
}

static const cairo_surface_backend_t cairo_win32_display_surface_backend = {
    CAIRO_SURFACE_TYPE_WIN32,
    _cairo_win32_display_surface_finish,

    _cairo_default_context_create,

    _cairo_win32_display_surface_create_similar,
    _cairo_win32_display_surface_create_similar_image,
    _cairo_win32_display_surface_map_to_image,
    _cairo_win32_display_surface_unmap_image,

    _cairo_surface_default_source,
    _cairo_surface_default_acquire_source_image,
    _cairo_surface_default_release_source_image,
    NULL,  /* snapshot */

    NULL, /* copy_page */
    NULL, /* show_page */

    _cairo_win32_surface_get_extents,
    NULL, /* get_font_options */

    _cairo_win32_display_surface_flush,
    _cairo_win32_display_surface_mark_dirty,

    _cairo_win32_display_surface_paint,
    _cairo_win32_display_surface_mask,
    _cairo_win32_display_surface_stroke,
    _cairo_win32_display_surface_fill,
    NULL, /* fill/stroke */
    _cairo_win32_display_surface_glyphs,
};

/* Notes:
 *
 * Win32 alpha-understanding functions
 *
 * BitBlt - will copy full 32 bits from a 32bpp DIB to result
 *          (so it's safe to use for ARGB32->ARGB32 SOURCE blits)
 *          (but not safe going RGB24->ARGB32, if RGB24 is also represented
 *           as a 32bpp DIB, since the alpha isn't discarded!)
 *
 * AlphaBlend - if both the source and dest have alpha, even if AC_SRC_ALPHA isn't set,
 *              it will still copy over the src alpha, because the SCA value (255) will be
 *              multiplied by all the src components.
 */

/**
 * cairo_win32_surface_create_with_format:
 * @hdc: the DC to create a surface for
 * @format: format of pixels in the surface to create
 *
 * Creates a cairo surface that targets the given DC.  The DC will be
 * queried for its initial clip extents, and this will be used as the
 * size of the cairo surface.
 *
 * Supported formats are:
 * %CAIRO_FORMAT_ARGB32
 * %CAIRO_FORMAT_RGB24
 *
 * Note: @format only tells cairo how to draw on the surface, not what
 * the format of the surface is. Namely, cairo does not (and cannot)
 * check that @hdc actually supports alpha-transparency.
 *
 * Return value: the newly created surface, NULL on failure
 *
 * Since: 1.14
 **/
cairo_surface_t *
cairo_win32_surface_create_with_format (HDC hdc, cairo_format_t format)
{
    cairo_win32_display_surface_t *surface;

    cairo_status_t status;
    cairo_device_t *device;

    switch (format) {
    default:
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
	break;
    }

    surface = _cairo_malloc (sizeof (*surface));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    status = _cairo_win32_save_initial_clip (hdc, surface);
    if (status) {
	free (surface);
	return _cairo_surface_create_in_error (status);
    }

    surface->image = NULL;
    surface->fallback = NULL;
    surface->win32.format = format;

    surface->win32.dc = hdc;
    surface->bitmap = NULL;
    surface->is_dib = FALSE;
    surface->saved_dc_bitmap = NULL;

    surface->win32.flags = _cairo_win32_flags_for_dc (surface->win32.dc, format);

    device = _cairo_win32_device_get ();

    _cairo_surface_init (&surface->win32.base,
			 &cairo_win32_display_surface_backend,
			 device,
			 _cairo_content_from_format (format),
			 FALSE); /* is_vector */

    cairo_device_destroy (device);

    return &surface->win32.base;
}

/**
 * cairo_win32_surface_create:
 * @hdc: the DC to create a surface for
 *
 * Creates a cairo surface that targets the given DC.  The DC will be
 * queried for its initial clip extents, and this will be used as the
 * size of the cairo surface.  The resulting surface will always be of
 * format %CAIRO_FORMAT_RGB24; should you need another surface format,
 * you will need to create one through
 * cairo_win32_surface_create_with_format() or
 * cairo_win32_surface_create_with_dib().
 *
 * Return value: the newly created surface, NULL on failure
 *
 * Since: 1.0
 **/
cairo_surface_t *
cairo_win32_surface_create (HDC hdc)
{
    return cairo_win32_surface_create_with_format (hdc, CAIRO_FORMAT_RGB24);
}

/**
 * cairo_win32_surface_create_with_dib:
 * @format: format of pixels in the surface to create
 * @width: width of the surface, in pixels
 * @height: height of the surface, in pixels
 *
 * Creates a device-independent-bitmap surface not associated with
 * any particular existing surface or device context. The created
 * bitmap will be uninitialized.
 *
 * Return value: the newly created surface
 *
 * Since: 1.2
 **/
cairo_surface_t *
cairo_win32_surface_create_with_dib (cairo_format_t format,
				     int	    width,
				     int	    height)
{
    if (! CAIRO_FORMAT_VALID (format))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));

    return _cairo_win32_display_surface_create_for_dc (NULL, format, width, height);
}

/**
 * cairo_win32_surface_create_with_ddb:
 * @hdc: a DC compatible with the surface to create
 * @format: format of pixels in the surface to create
 * @width: width of the surface, in pixels
 * @height: height of the surface, in pixels
 *
 * Creates a device-dependent-bitmap surface not associated with
 * any particular existing surface or device context. The created
 * bitmap will be uninitialized.
 *
 * Return value: the newly created surface
 *
 * Since: 1.4
 **/
cairo_surface_t *
cairo_win32_surface_create_with_ddb (HDC hdc,
				     cairo_format_t format,
				     int width,
				     int height)
{
    cairo_win32_display_surface_t *new_surf;
    HBITMAP ddb;
    HDC screen_dc, ddb_dc;
    HBITMAP saved_dc_bitmap;

    switch (format) {
    default:
/* XXX handle these eventually */
    case CAIRO_FORMAT_A8:
    case CAIRO_FORMAT_A1:
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
	break;
    }

    if (!hdc) {
	screen_dc = GetDC (NULL);
	hdc = screen_dc;
    } else {
	screen_dc = NULL;
    }

    ddb_dc = CreateCompatibleDC (hdc);
    if (ddb_dc == NULL) {
	new_surf = (cairo_win32_display_surface_t*) _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	goto FINISH;
    }

    ddb = CreateCompatibleBitmap (hdc, width, height);
    if (ddb == NULL) {
	DeleteDC (ddb_dc);

	/* Note that if an app actually does hit this out of memory
	 * condition, it's going to have lots of other issues, as
	 * video memory is probably exhausted.  However, it can often
	 * continue using DIBs instead of DDBs.
	 */
	new_surf = (cairo_win32_display_surface_t*) _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	goto FINISH;
    }

    saved_dc_bitmap = SelectObject (ddb_dc, ddb);

    new_surf = (cairo_win32_display_surface_t*) cairo_win32_surface_create (ddb_dc);
    new_surf->bitmap = ddb;
    new_surf->saved_dc_bitmap = saved_dc_bitmap;
    new_surf->is_dib = FALSE;

FINISH:
    if (screen_dc)
	ReleaseDC (NULL, screen_dc);

    return &new_surf->win32.base;
}

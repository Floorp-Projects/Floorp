/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc.
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
#include "cairo-error-private.h"
#include "cairo-paginated-private.h"
#include "cairo-win32-private.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-surface-fallback-private.h"
#include "cairo-surface-clipper-private.h"
#include "cairo-gstate-private.h"
#include "cairo-private.h"
#include <wchar.h>
#include <windows.h>

#if defined(__MINGW32__) && !defined(ETO_PDY)
# define ETO_PDY 0x2000
#endif

#undef DEBUG_COMPOSITE

/* for older SDKs */
#ifndef SHADEBLENDCAPS
#define SHADEBLENDCAPS  120
#endif
#ifndef SB_NONE
#define SB_NONE         0x00000000
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
 */

/**
 * CAIRO_HAS_WIN32_SURFACE:
 *
 * Defined if the Microsoft Windows surface backend is available.
 * This macro can be used to conditionally compile backend-specific code.
 */

static const cairo_surface_backend_t cairo_win32_surface_backend;

/**
 * _cairo_win32_print_gdi_error:
 * @context: context string to display along with the error
 *
 * Helper function to dump out a human readable form of the
 * current error code.
 *
 * Return value: A cairo status code for the error code
 **/
cairo_status_t
_cairo_win32_print_gdi_error (const char *context)
{
    void *lpMsgBuf;
    DWORD last_error = GetLastError ();

    if (!FormatMessageW (FORMAT_MESSAGE_ALLOCATE_BUFFER |
			 FORMAT_MESSAGE_FROM_SYSTEM,
			 NULL,
			 last_error,
			 MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
			 (LPWSTR) &lpMsgBuf,
			 0, NULL)) {
	fprintf (stderr, "%s: Unknown GDI error", context);
    } else {
	fwprintf (stderr, L"%s: %S", context, (wchar_t *)lpMsgBuf);

	LocalFree (lpMsgBuf);
    }
    fflush(stderr);

    /* We should switch off of last_status, but we'd either return
     * CAIRO_STATUS_NO_MEMORY or CAIRO_STATUS_UNKNOWN_ERROR and there
     * is no CAIRO_STATUS_UNKNOWN_ERROR.
     */

    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
}

uint32_t
_cairo_win32_flags_for_dc (HDC dc)
{
    uint32_t flags = 0;

    if (GetDeviceCaps(dc, TECHNOLOGY) == DT_RASDISPLAY) {
	flags |= CAIRO_WIN32_SURFACE_IS_DISPLAY;

	/* These will always be possible, but the actual GetDeviceCaps
	 * calls will return whether they're accelerated or not.
	 * We may want to use our own (pixman) routines sometimes
	 * if they're eventually faster, but for now have GDI do
	 * everything.
	 */
	flags |= CAIRO_WIN32_SURFACE_CAN_BITBLT;
	flags |= CAIRO_WIN32_SURFACE_CAN_ALPHABLEND;
	flags |= CAIRO_WIN32_SURFACE_CAN_STRETCHBLT;
	flags |= CAIRO_WIN32_SURFACE_CAN_STRETCHDIB;
    } else {
	int cap;

	cap = GetDeviceCaps(dc, SHADEBLENDCAPS);
	if (cap != SB_NONE)
	    flags |= CAIRO_WIN32_SURFACE_CAN_ALPHABLEND;

	cap = GetDeviceCaps(dc, RASTERCAPS);
	if (cap & RC_BITBLT)
	    flags |= CAIRO_WIN32_SURFACE_CAN_BITBLT;
	if (cap & RC_STRETCHBLT)
	    flags |= CAIRO_WIN32_SURFACE_CAN_STRETCHBLT;
	if (cap & RC_STRETCHDIB)
	    flags |= CAIRO_WIN32_SURFACE_CAN_STRETCHDIB;
    }

    return flags;
}

static cairo_status_t
_create_dc_and_bitmap (cairo_win32_surface_t *surface,
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

    surface->dc = NULL;
    surface->bitmap = NULL;
    surface->is_dib = FALSE;

    switch (format) {
    default:
    case CAIRO_FORMAT_INVALID:
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

    surface->dc = CreateCompatibleDC (original_dc);
    if (!surface->dc)
	goto FAIL;

    surface->bitmap = CreateDIBSection (surface->dc,
			                bitmap_info,
			                DIB_RGB_COLORS,
			                &bits,
			                NULL, 0);
    if (!surface->bitmap)
	goto FAIL;

    surface->is_dib = TRUE;

    GdiFlush();

    surface->saved_dc_bitmap = SelectObject (surface->dc,
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

    surface->flags = _cairo_win32_flags_for_dc (surface->dc);

    return CAIRO_STATUS_SUCCESS;

 FAIL:
    status = _cairo_win32_print_gdi_error ("_create_dc_and_bitmap");

    if (bitmap_info && num_palette > 2)
	free (bitmap_info);

    if (surface->saved_dc_bitmap) {
	SelectObject (surface->dc, surface->saved_dc_bitmap);
	surface->saved_dc_bitmap = NULL;
    }

    if (surface->bitmap) {
	DeleteObject (surface->bitmap);
	surface->bitmap = NULL;
    }

    if (surface->dc) {
 	DeleteDC (surface->dc);
	surface->dc = NULL;
    }

    return status;
}

static cairo_surface_t *
_cairo_win32_surface_create_for_dc (HDC             original_dc,
				    cairo_format_t  format,
				    int	            width,
				    int	            height)
{
    cairo_status_t status;
    cairo_win32_surface_t *surface;
    unsigned char *bits;
    int rowstride;

    if (! CAIRO_FORMAT_VALID (format))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));

    surface = malloc (sizeof (cairo_win32_surface_t));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    surface->clip_region = NULL;

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

    surface->format = format;

    surface->clip_rect.x = 0;
    surface->clip_rect.y = 0;
    surface->clip_rect.width = width;
    surface->clip_rect.height = height;

    surface->initial_clip_rgn = NULL;
    surface->had_simple_clip = FALSE;

    surface->extents = surface->clip_rect;
    surface->font_subsets = NULL;

    _cairo_surface_init (&surface->base,
			 &cairo_win32_surface_backend,
			 NULL, /* device */
			 _cairo_content_from_format (format));

    return &surface->base;

 FAIL:
    if (surface->bitmap) {
	SelectObject (surface->dc, surface->saved_dc_bitmap);
	DeleteObject (surface->bitmap);
	DeleteDC (surface->dc);
    }
    free (surface);

    return _cairo_surface_create_in_error (status);
}

static cairo_surface_t *
_cairo_win32_surface_create_similar_internal (void	    *abstract_src,
					      cairo_content_t content,
					      int	     width,
					      int	     height,
					      cairo_bool_t   force_dib)
{
    cairo_win32_surface_t *src = abstract_src;
    cairo_format_t format = _cairo_format_from_content (content);
    cairo_surface_t *new_surf = NULL;

    /* We force a DIB always if:
     * - we need alpha; or
     * - the parent is a DIB; or
     * - the parent is for printing (because we don't care about the bit depth at that point)
     *
     * We also might end up with a DIB even if a DDB is requested if DDB creation failed
     * due to out of memory.
     */
    if (src->is_dib ||
	(content & CAIRO_CONTENT_ALPHA) ||
	src->base.backend->type == CAIRO_SURFACE_TYPE_WIN32_PRINTING)
    {
	force_dib = TRUE;
    }

    if (!force_dib) {
	/* try to create a ddb */
	new_surf = cairo_win32_surface_create_with_ddb (src->dc, CAIRO_FORMAT_RGB24, width, height);

	if (new_surf->status != CAIRO_STATUS_SUCCESS)
	    new_surf = NULL;
    }

    if (new_surf == NULL) {
	new_surf = _cairo_win32_surface_create_for_dc (src->dc, format, width, height);
    }

    return new_surf;
}

cairo_surface_t *
_cairo_win32_surface_create_similar (void	    *abstract_src,
				     cairo_content_t content,
				     int	     width,
				     int	     height)
{
    return _cairo_win32_surface_create_similar_internal (abstract_src, content, width, height, FALSE);
}

cairo_status_t
_cairo_win32_surface_finish (void *abstract_surface)
{
    cairo_win32_surface_t *surface = abstract_surface;

    if (surface->image)
	cairo_surface_destroy (surface->image);

    /* If we created the Bitmap and DC, destroy them */
    if (surface->bitmap) {
	SelectObject (surface->dc, surface->saved_dc_bitmap);
	DeleteObject (surface->bitmap);
	DeleteDC (surface->dc);
    } else {
	_cairo_win32_restore_initial_clip (surface);
    }

    if (surface->initial_clip_rgn)
	DeleteObject (surface->initial_clip_rgn);

    if (surface->font_subsets != NULL)
	_cairo_scaled_font_subsets_destroy (surface->font_subsets);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_surface_get_subimage (cairo_win32_surface_t  *surface,
				   int                     x,
				   int                     y,
				   int                     width,
				   int                     height,
				   cairo_win32_surface_t **local_out)
{
    cairo_win32_surface_t *local;
    cairo_int_status_t status;
    cairo_content_t content = _cairo_content_from_format (surface->format);

    local =
	(cairo_win32_surface_t *) _cairo_win32_surface_create_similar_internal
	(surface, content, width, height, TRUE);
    if (local == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    if (local->base.status)
	return local->base.status;

    status = CAIRO_INT_STATUS_UNSUPPORTED;

    /* Only BitBlt if the source surface supports it. */
    if ((surface->flags & CAIRO_WIN32_SURFACE_CAN_BITBLT) &&
	BitBlt (local->dc,
		0, 0,
		width, height,
		surface->dc,
		x, y,
		SRCCOPY))
    {
	status = CAIRO_STATUS_SUCCESS;
    }

    if (status) {
	/* If we failed here, most likely the source or dest doesn't
	 * support BitBlt/AlphaBlend (e.g. a printer).
	 * You can't reliably get bits from a printer DC, so just fill in
	 * the surface as white (common case for printing).
	 */

	RECT r;
	r.left = r.top = 0;
	r.right = width;
	r.bottom = height;
	FillRect(local->dc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
    }

    *local_out = local;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_win32_convert_ddb_to_dib (cairo_win32_surface_t *surface)
{
    cairo_win32_surface_t *new_surface;
    int width  = surface->extents.width  + surface->extents.x;
    int height = surface->extents.height + surface->extents.y;

    BOOL ok;
    HBITMAP oldbitmap;

    new_surface = (cairo_win32_surface_t*)
	_cairo_win32_surface_create_for_dc (surface->dc,
					    surface->format,
					    width,
					    height);

    if (new_surface->base.status)
	return;

    /* DDB can't be 32bpp, so BitBlt is safe */
    ok = BitBlt (new_surface->dc,
		 0, 0, width, height,
		 surface->dc,
		 0, 0, SRCCOPY);

    if (!ok)
	goto out;

    /* Now swap around new_surface and surface's internal bitmap
     * pointers. */
    DeleteDC (new_surface->dc);
    new_surface->dc = NULL;

    oldbitmap = SelectObject (surface->dc, new_surface->bitmap);
    DeleteObject (oldbitmap);

    surface->image = new_surface->image;
    surface->is_dib = new_surface->is_dib;
    surface->bitmap = new_surface->bitmap;

    new_surface->bitmap = NULL;
    new_surface->image = NULL;

    /* Finally update flags */
    surface->flags = _cairo_win32_flags_for_dc (surface->dc);

  out:
    cairo_surface_destroy ((cairo_surface_t*)new_surface);
}

static cairo_status_t
_cairo_win32_surface_acquire_source_image (void                    *abstract_surface,
					   cairo_image_surface_t  **image_out,
					   void                   **image_extra)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_win32_surface_t *local;
    cairo_status_t status;

    if (!surface->image && !surface->is_dib && surface->bitmap &&
	(surface->flags & CAIRO_WIN32_SURFACE_CAN_CONVERT_TO_DIB) != 0)
    {
	/* This is a DDB, and we're being asked to use it as a source for
	 * something that we couldn't support natively.  So turn it into
	 * a DIB, so that we have an equivalent image surface, as long
	 * as we're allowed to via flags.
	 */
	_cairo_win32_convert_ddb_to_dib (surface);
    }

    if (surface->image) {
	*image_out = (cairo_image_surface_t *)surface->image;
	*image_extra = NULL;
	return CAIRO_STATUS_SUCCESS;
    }

    status = _cairo_win32_surface_get_subimage (abstract_surface, 0, 0,
						surface->extents.width,
						surface->extents.height, &local);
    if (status)
	return status;

    *image_out = (cairo_image_surface_t *)local->image;
    *image_extra = local;
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_win32_surface_release_source_image (void                   *abstract_surface,
					   cairo_image_surface_t  *image,
					   void                   *image_extra)
{
    cairo_win32_surface_t *local = image_extra;

    if (local)
	cairo_surface_destroy ((cairo_surface_t *)local);
}

static cairo_status_t
_cairo_win32_surface_acquire_dest_image (void                    *abstract_surface,
					 cairo_rectangle_int_t   *interest_rect,
					 cairo_image_surface_t  **image_out,
					 cairo_rectangle_int_t   *image_rect,
					 void                   **image_extra)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_win32_surface_t *local = NULL;
    cairo_status_t status;

    if (surface->image) {
	GdiFlush();

	*image_out = (cairo_image_surface_t *) surface->image;
	*image_extra = NULL;
	*image_rect = surface->extents;
	return CAIRO_STATUS_SUCCESS;
    }

    status = _cairo_win32_surface_get_subimage (abstract_surface,
						interest_rect->x,
						interest_rect->y,
						interest_rect->width,
						interest_rect->height,
						&local);
    if (status)
	return status;

    *image_out = (cairo_image_surface_t *) local->image;
    *image_extra = local;
    *image_rect = *interest_rect;
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_win32_surface_release_dest_image (void                    *abstract_surface,
					 cairo_rectangle_int_t   *interest_rect,
					 cairo_image_surface_t   *image,
					 cairo_rectangle_int_t   *image_rect,
					 void                    *image_extra)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_win32_surface_t *local = image_extra;

    if (!local)
	return;

    /* clear any clip that's currently set on the surface
       so that we can blit uninhibited. */
    _cairo_win32_surface_set_clip_region (surface, NULL);

    if (!BitBlt (surface->dc,
		 image_rect->x, image_rect->y,
		 image_rect->width, image_rect->height,
		 local->dc,
		 0, 0,
		 SRCCOPY))
	_cairo_win32_print_gdi_error ("_cairo_win32_surface_release_dest_image");

    cairo_surface_destroy ((cairo_surface_t *)local);
}

cairo_status_t
_cairo_win32_surface_set_clip_region (void           *abstract_surface,
				      cairo_region_t *region)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (surface->clip_region == region)
	return CAIRO_STATUS_SUCCESS;

    cairo_region_destroy (surface->clip_region);
    surface->clip_region = cairo_region_reference (region);

    /* The semantics we want is that any clip set by cairo combines
     * is intersected with the clip on device context that the
     * surface was created for. To implement this, we need to
     * save the original clip when first setting a clip on surface.
     */

    /* Clear any clip set by cairo, return to the original first */
    status = _cairo_win32_restore_initial_clip (surface);

    /* Then combine any new region with it */
    if (region) {
	cairo_rectangle_int_t extents;
	int num_rects;
	RGNDATA *data;
	size_t data_size;
	RECT *rects;
	int i;
	HRGN gdi_region;

	/* Create a GDI region for the cairo region */

	cairo_region_get_extents (region, &extents);
	num_rects = cairo_region_num_rectangles (region);
	/* XXX see notes in _cairo_win32_save_initial_clip --
	 * this code will interact badly with a HDC which had an initial
	 * world transform -- we should probably manually transform the
	 * region rects, because SelectClipRgn takes device units, not
	 * logical units (unlike IntersectClipRect).
	 */

	data_size = sizeof (RGNDATAHEADER) + num_rects * sizeof (RECT);
	data = malloc (data_size);
	if (!data)
	    return _cairo_error(CAIRO_STATUS_NO_MEMORY);
	rects = (RECT *)data->Buffer;

	data->rdh.dwSize = sizeof (RGNDATAHEADER);
	data->rdh.iType = RDH_RECTANGLES;
	data->rdh.nCount = num_rects;
	data->rdh.nRgnSize = num_rects * sizeof (RECT);
	data->rdh.rcBound.left = extents.x;
	data->rdh.rcBound.top = extents.y;
	data->rdh.rcBound.right = extents.x + extents.width;
	data->rdh.rcBound.bottom = extents.y + extents.height;

	for (i = 0; i < num_rects; i++) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (region, i, &rect);

	    rects[i].left   = rect.x;
	    rects[i].top    = rect.y;
	    rects[i].right  = rect.x + rect.width;
	    rects[i].bottom = rect.y + rect.height;
	}

	gdi_region = ExtCreateRegion (NULL, data_size, data);
	free (data);

	if (!gdi_region)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	/* AND the new region into our DC */
	if (ExtSelectClipRgn (surface->dc, gdi_region, RGN_AND) == ERROR)
	    status = _cairo_win32_print_gdi_error ("_cairo_win32_surface_set_clip_region");

	DeleteObject (gdi_region);
    }

    return status;
}

#if !defined(AC_SRC_OVER)
#define AC_SRC_OVER                 0x00
#pragma pack(1)
typedef struct {
    BYTE   BlendOp;
    BYTE   BlendFlags;
    BYTE   SourceConstantAlpha;
    BYTE   AlphaFormat;
}BLENDFUNCTION;
#pragma pack()
#endif

/* for compatibility with VC++ 6 */
#ifndef AC_SRC_ALPHA
#define AC_SRC_ALPHA                0x01
#endif

typedef BOOL (WINAPI *cairo_alpha_blend_func_t) (HDC hdcDest,
						 int nXOriginDest,
						 int nYOriginDest,
						 int nWidthDest,
						 int hHeightDest,
						 HDC hdcSrc,
						 int nXOriginSrc,
						 int nYOriginSrc,
						 int nWidthSrc,
						 int nHeightSrc,
						 BLENDFUNCTION blendFunction);

static cairo_int_status_t
_composite_alpha_blend (cairo_win32_surface_t *dst,
			cairo_win32_surface_t *src,
			int                    alpha,
			int                    src_x,
			int                    src_y,
			int                    src_w,
			int                    src_h,
			int                    dst_x,
			int                    dst_y,
			int                    dst_w,
			int                    dst_h)
{
    static unsigned alpha_blend_checked = FALSE;
    static cairo_alpha_blend_func_t alpha_blend = NULL;

    BLENDFUNCTION blend_function;

    /* Check for AlphaBlend dynamically to allow compiling on
     * MSVC 6 and use on older windows versions
     */
    if (!alpha_blend_checked) {
	OSVERSIONINFO os;

	os.dwOSVersionInfoSize = sizeof (os);
	GetVersionEx (&os);

	/* If running on Win98, disable using AlphaBlend()
	 * to avoid Win98 AlphaBlend() bug */
	if (VER_PLATFORM_WIN32_WINDOWS != os.dwPlatformId ||
	    os.dwMajorVersion != 4 || os.dwMinorVersion != 10)
	{
	    HMODULE msimg32_dll = LoadLibraryW (L"msimg32");

	    if (msimg32_dll != NULL)
		alpha_blend = (cairo_alpha_blend_func_t)GetProcAddress (msimg32_dll,
									"AlphaBlend");
	}

	alpha_blend_checked = TRUE;
    }

    if (alpha_blend == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    if (!(dst->flags & CAIRO_WIN32_SURFACE_CAN_ALPHABLEND))
	return CAIRO_INT_STATUS_UNSUPPORTED;
    if (src->format == CAIRO_FORMAT_RGB24 && dst->format == CAIRO_FORMAT_ARGB32)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    blend_function.BlendOp = AC_SRC_OVER;
    blend_function.BlendFlags = 0;
    blend_function.SourceConstantAlpha = alpha;
    blend_function.AlphaFormat = (src->format == CAIRO_FORMAT_ARGB32) ? AC_SRC_ALPHA : 0;

    if (!alpha_blend (dst->dc,
		      dst_x, dst_y,
		      dst_w, dst_h,
		      src->dc,
		      src_x, src_y,
		      src_w, src_h,
		      blend_function))
	return _cairo_win32_print_gdi_error ("_cairo_win32_surface_composite(AlphaBlend)");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_win32_surface_composite_inner (cairo_win32_surface_t *src,
				      cairo_image_surface_t *src_image,
				      cairo_win32_surface_t *dst,
				      cairo_rectangle_int_t src_extents,
				      cairo_rectangle_int_t src_r,
				      cairo_rectangle_int_t dst_r,
				      int alpha,
				      cairo_bool_t needs_alpha,
				      cairo_bool_t needs_scale)
{
    /* Then do BitBlt, StretchDIBits, StretchBlt, AlphaBlend, or MaskBlt */
    if (src_image) {
	if (needs_alpha || needs_scale)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (dst->flags & CAIRO_WIN32_SURFACE_CAN_STRETCHBLT) {
	    BITMAPINFO bi;
	    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	    bi.bmiHeader.biWidth = src_image->width;
	    bi.bmiHeader.biHeight = - src_image->height;
	    bi.bmiHeader.biSizeImage = 0;
	    bi.bmiHeader.biXPelsPerMeter = PELS_72DPI;
	    bi.bmiHeader.biYPelsPerMeter = PELS_72DPI;
	    bi.bmiHeader.biPlanes = 1;
	    bi.bmiHeader.biBitCount = 32;
	    bi.bmiHeader.biCompression = BI_RGB;
	    bi.bmiHeader.biClrUsed = 0;
	    bi.bmiHeader.biClrImportant = 0;

	    /* StretchDIBits is broken with top-down dibs; you need to do some
	     * special munging to make the coordinate space work (basically,
	     * need to address everything based on the bottom left, instead of top left,
	     * and need to tell it to flip the resulting image.
	     *
	     * See http://blog.vlad1.com/archives/2006/10/26/134/ and comments.
	     */
	    if (!StretchDIBits (dst->dc,
				/* dst x,y,w,h */
				dst_r.x, dst_r.y + dst_r.height - 1,
				dst_r.width, - (int) dst_r.height,
				/* src x,y,w,h */
				src_r.x, src_extents.height - src_r.y + 1,
				src_r.width, - (int) src_r.height,
				src_image->data,
				&bi,
				DIB_RGB_COLORS,
				SRCCOPY))
		return _cairo_win32_print_gdi_error ("_cairo_win32_surface_composite(StretchDIBits)");
	}
    } else if (!needs_alpha) {
	/* BitBlt or StretchBlt? */
	if (!needs_scale && (dst->flags & CAIRO_WIN32_SURFACE_CAN_BITBLT)) {
            if (!BitBlt (dst->dc,
			 dst_r.x, dst_r.y,
			 dst_r.width, dst_r.height,
			 src->dc,
			 src_r.x, src_r.y,
			 SRCCOPY))
		return _cairo_win32_print_gdi_error ("_cairo_win32_surface_composite(BitBlt)");
	} else if (dst->flags & CAIRO_WIN32_SURFACE_CAN_STRETCHBLT) {
	    /* StretchBlt? */
	    /* XXX check if we want HALFTONE, based on the src filter */
	    BOOL success;
	    int oldmode = SetStretchBltMode(dst->dc, HALFTONE);
	    success = StretchBlt(dst->dc,
				 dst_r.x, dst_r.y,
				 dst_r.width, dst_r.height,
				 src->dc,
				 src_r.x, src_r.y,
				 src_r.width, src_r.height,
				 SRCCOPY);
	    SetStretchBltMode(dst->dc, oldmode);

	    if (!success)
		return _cairo_win32_print_gdi_error ("StretchBlt");
	}
    } else if (needs_alpha && !needs_scale) {
	RECT r = {0, 0, 5000, 5000};
        //FillRect(dst->dc, &r, GetStockObject(DKGRAY_BRUSH));
  	return _composite_alpha_blend (dst, src, alpha,
				       src_r.x, src_r.y, src_r.width, src_r.height,
				       dst_r.x, dst_r.y, dst_r.width, dst_r.height);
    }

    return CAIRO_STATUS_SUCCESS;
}

/* from pixman-private.h */
#define MOD(a,b) ((a) < 0 ? ((b) - ((-(a) - 1) % (b))) - 1 : (a) % (b))

static cairo_int_status_t
_cairo_win32_surface_composite (cairo_operator_t	op,
				const cairo_pattern_t	*pattern,
				const cairo_pattern_t	*mask_pattern,
				void			*abstract_dst,
				int			src_x,
				int			src_y,
				int			mask_x,
				int			mask_y,
				int			dst_x,
				int			dst_y,
				unsigned int		width,
				unsigned int		height,
				cairo_region_t	       *clip_region)
{
    cairo_win32_surface_t *dst = abstract_dst;
    cairo_win32_surface_t *src;
    cairo_surface_pattern_t *src_surface_pattern;
    int alpha;
    double scalex, scaley;
    cairo_fixed_t x0_fixed, y0_fixed;
    cairo_int_status_t status;

    cairo_bool_t needs_alpha, needs_scale, needs_repeat;
    cairo_image_surface_t *src_image = NULL;

    cairo_format_t src_format;
    cairo_rectangle_int_t src_extents;

    cairo_rectangle_int_t src_r = { src_x, src_y, width, height };
    cairo_rectangle_int_t dst_r = { dst_x, dst_y, width, height };

#ifdef DEBUG_COMPOSITE
    fprintf (stderr, "+++ composite: %d %p %p %p [%d %d] [%d %d] [%d %d] %dx%d\n",
	     op, pattern, mask_pattern, abstract_dst, src_x, src_y, mask_x, mask_y, dst_x, dst_y, width, height);
#endif

    /* If the destination can't do any of these, then
     * we may as well give up, since this is what we'll
     * look to for optimization.
     */
    if ((dst->flags & (CAIRO_WIN32_SURFACE_CAN_BITBLT |
		       CAIRO_WIN32_SURFACE_CAN_ALPHABLEND |
		       CAIRO_WIN32_SURFACE_CAN_STRETCHBLT |
		       CAIRO_WIN32_SURFACE_CAN_STRETCHDIB))
	== 0)
    {
	goto UNSUPPORTED;
    }

    if (pattern->type != CAIRO_PATTERN_TYPE_SURFACE)
	goto UNSUPPORTED;

    if (pattern->extend != CAIRO_EXTEND_NONE &&
	pattern->extend != CAIRO_EXTEND_REPEAT)
	goto UNSUPPORTED;

    if (mask_pattern) {
	/* FIXME: When we fully support RENDER style 4-channel
	 * masks we need to check r/g/b != 1.0.
	 */
	if (mask_pattern->type != CAIRO_PATTERN_TYPE_SOLID)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	alpha = ((cairo_solid_pattern_t *)mask_pattern)->color.alpha_short >> 8;
    } else {
	alpha = 255;
    }

    src_surface_pattern = (cairo_surface_pattern_t *)pattern;
    src = (cairo_win32_surface_t *)src_surface_pattern->surface;

    if (src->base.type == CAIRO_SURFACE_TYPE_IMAGE &&
	dst->flags & (CAIRO_WIN32_SURFACE_CAN_STRETCHDIB))
    {
	/* In some very limited cases, we can use StretchDIBits to draw
	 * an image surface directly:
	 *  - source is CAIRO_FORMAT_ARGB32
	 *  - dest is CAIRO_FORMAT_ARGB32
	 *  - alpha is 255
	 *  - operator is SOURCE or OVER
	 *  - image stride is 4*width
	 */
	src_image = (cairo_image_surface_t*) src;

	if (src_image->format != CAIRO_FORMAT_RGB24 ||
	    dst->format != CAIRO_FORMAT_RGB24 ||
	    alpha != 255 ||
	    (op != CAIRO_OPERATOR_SOURCE && op != CAIRO_OPERATOR_OVER) ||
	    src_image->stride != (src_image->width * 4))
	{
	    goto UNSUPPORTED;
	}

	src_format = src_image->format;
	src_extents.x = 0;
	src_extents.y = 0;
	src_extents.width = src_image->width;
	src_extents.height = src_image->height;
    } else if (src->base.backend != dst->base.backend) {
	goto UNSUPPORTED;
    } else {
	src_format = src->format;
	src_extents = src->extents;
    }


#ifdef DEBUG_COMPOSITE
    fprintf (stderr, "Before check: src size: (%d %d) xy [%d %d] -> dst [%d %d %d %d] {srcmat %f %f %f %f}\n",
	     src_extents.width, src_extents.height,
	     src_x, src_y,
	     dst_x, dst_y, width, height,
	     pattern->matrix.x0, pattern->matrix.y0, pattern->matrix.xx, pattern->matrix.yy);
#endif

    /* We can only use GDI functions if the source and destination rectangles
     * are on integer pixel boundaries.  Figure that out here.
     */
    x0_fixed = _cairo_fixed_from_double(pattern->matrix.x0 / pattern->matrix.xx);
    y0_fixed = _cairo_fixed_from_double(pattern->matrix.y0 / pattern->matrix.yy);

    if (pattern->matrix.yx != 0.0 ||
	pattern->matrix.xy != 0.0 ||
	!_cairo_fixed_is_integer(x0_fixed) ||
	!_cairo_fixed_is_integer(y0_fixed))
    {
	goto UNSUPPORTED;
    }

    scalex = pattern->matrix.xx;
    scaley = pattern->matrix.yy;

    src_r.x += _cairo_fixed_integer_part(x0_fixed);
    src_r.y += _cairo_fixed_integer_part(y0_fixed);

    /* Success, right? */
    if (scalex == 0.0 || scaley == 0.0)
	return CAIRO_STATUS_SUCCESS;

    if (scalex != 1.0 || scaley != 1.0)
	goto UNSUPPORTED;

    /* If the src coordinates are outside of the source surface bounds,
     * we have to fix them up, because this is an error for the GDI
     * functions.
     */

#ifdef DEBUG_COMPOSITE
    fprintf (stderr, "before: [%d %d %d %d] -> [%d %d %d %d]\n",
	     src_r.x, src_r.y, src_r.width, src_r.height,
	     dst_r.x, dst_r.y, dst_r.width, dst_r.height);
    fflush (stderr);
#endif

    /* If the src rectangle doesn't wholly lie within the src extents,
     * fudge things.  We really need to do fixup on the unpainted
     * region -- e.g. the SOURCE operator is broken for areas outside
     * of the extents, because it won't clear that area to transparent
     * black.
     */

    if (pattern->extend != CAIRO_EXTEND_REPEAT) {
	needs_repeat = FALSE;

	/* If the src rect and the extents of the source image don't overlap at all,
	 * we can't do anything useful here.
	 */
	if (src_r.x > src_extents.width || src_r.y > src_extents.height ||
	    (src_r.x + src_r.width) < 0 || (src_r.y + src_r.height) < 0)
	{
	    if (op == CAIRO_OPERATOR_OVER)
		return CAIRO_STATUS_SUCCESS;
	    goto UNSUPPORTED;
	}

	if (src_r.x < 0) {
	    src_r.width += src_r.x;

	    dst_r.width += src_r.x;
	    dst_r.x -= src_r.x;

            src_r.x = 0;
	}

	if (src_r.y < 0) {
	    src_r.height += src_r.y;

	    dst_r.height += src_r.y;
	    dst_r.y -= src_r.y;
	    
            src_r.y = 0;
	}

	if (src_r.x + src_r.width > src_extents.width) {
	    src_r.width = src_extents.width - src_r.x;
	    dst_r.width = src_r.width;
	}

	if (src_r.y + src_r.height > src_extents.height) {
	    src_r.height = src_extents.height - src_r.y;
	    dst_r.height = src_r.height;
	}
    } else {
	needs_repeat = TRUE;
    }

    /*
     * Operations that we can do:
     *
     *  RGB OVER  RGB -> BitBlt (same as SOURCE)
     *  RGB OVER ARGB -> UNSUPPORTED (AlphaBlend treats this as a BitBlt, even with SCA 255 and no AC_SRC_ALPHA)
     * ARGB OVER ARGB -> AlphaBlend, with AC_SRC_ALPHA
     * ARGB OVER  RGB -> AlphaBlend, with AC_SRC_ALPHA; we'll have junk in the dst A byte
     * 
     *  RGB OVER  RGB + mask -> AlphaBlend, no AC_SRC_ALPHA
     *  RGB OVER ARGB + mask -> UNSUPPORTED
     * ARGB OVER ARGB + mask -> AlphaBlend, with AC_SRC_ALPHA
     * ARGB OVER  RGB + mask -> AlphaBlend, with AC_SRC_ALPHA; junk in the dst A byte
     * 
     *  RGB SOURCE  RGB -> BitBlt
     *  RGB SOURCE ARGB -> UNSUPPORTED (AlphaBlend treats this as a BitBlt, even with SCA 255 and no AC_SRC_ALPHA)
     * ARGB SOURCE ARGB -> BitBlt
     * ARGB SOURCE  RGB -> BitBlt
     * 
     *  RGB SOURCE  RGB + mask -> unsupported
     *  RGB SOURCE ARGB + mask -> unsupported
     * ARGB SOURCE ARGB + mask -> unsupported
     * ARGB SOURCE  RGB + mask -> unsupported
     */

    /*
     * Figure out what action to take.
     */
    if (op == CAIRO_OPERATOR_OVER) {
	if (alpha == 0)
	    return CAIRO_STATUS_SUCCESS;

	if (src_format == dst->format) {
	    if (alpha == 255 && src_format == CAIRO_FORMAT_RGB24) {
		needs_alpha = FALSE;
	    } else {
		needs_alpha = TRUE;
	    }
	} else if (src_format == CAIRO_FORMAT_ARGB32 &&
		   dst->format == CAIRO_FORMAT_RGB24)
	{
	    needs_alpha = TRUE;
	} else {
	    goto UNSUPPORTED;
	}
    } else if (alpha == 255 && op == CAIRO_OPERATOR_SOURCE) {
	if ((src_format == dst->format) ||
	    (src_format == CAIRO_FORMAT_ARGB32 && dst->format == CAIRO_FORMAT_RGB24))
	{
	    needs_alpha = FALSE;
	} else {
	    goto UNSUPPORTED;
	}
    } else {
	goto UNSUPPORTED;
    }

    if (scalex == 1.0 && scaley == 1.0) {
	needs_scale = FALSE;
    } else {
	/* Should never be reached until we turn StretchBlt back on */
	needs_scale = TRUE;
    }

#ifdef DEBUG_COMPOSITE
    fprintf (stderr, "action: [%d %d %d %d] -> [%d %d %d %d]\n",
	     src_r.x, src_r.y, src_r.width, src_r.height,
	     dst_r.x, dst_r.y, dst_r.width, dst_r.height);
    fflush (stderr);
#endif

    status = _cairo_win32_surface_set_clip_region (dst, clip_region);
    if (status)
	return status;

    /* If we need to repeat, we turn the repeated blit into
     * a bunch of piece-by-piece blits.
     */
    if (needs_repeat) {
	cairo_rectangle_int_t piece_src_r, piece_dst_r;
	uint32_t rendered_width = 0, rendered_height = 0;
	uint32_t to_render_height, to_render_width;
	int32_t piece_x, piece_y;
	int32_t src_start_x = MOD(src_r.x, src_extents.width);
	int32_t src_start_y = MOD(src_r.y, src_extents.height);

	if (needs_scale)
	    goto UNSUPPORTED;

	/* If both the src and dest have an image, we may as well fall
	 * back, because it will be faster than our separate blits.
	 * Our blit code will be fastest when the src is a DDB and the
	 * destination is a DDB.
	 */
	if ((src_image || src->image) && dst->image)
	    goto UNSUPPORTED;

	/* If the src is not a bitmap but an on-screen (or unknown)
	 * DC, chances are that fallback will be faster.
	 */
	if (src->bitmap == NULL)
	    goto UNSUPPORTED;

	/* If we can use PatBlt, just do so */
	if (!src_image && !needs_alpha)
	{
	    HBRUSH brush;
	    HGDIOBJ old_brush;
	    POINT old_brush_origin;

	    /* Set up the brush with our bitmap */
	    brush = CreatePatternBrush (src->bitmap);

	    /* SetBrushOrgEx sets the coordinates in the destination DC of where the
	     * pattern should start.
	     */
	    SetBrushOrgEx (dst->dc, dst_r.x - src_start_x,
			   dst_r.y - src_start_y, &old_brush_origin);

	    old_brush = SelectObject (dst->dc, brush);

	    PatBlt (dst->dc, dst_r.x, dst_r.y, dst_r.width, dst_r.height, PATCOPY);

	    /* Restore the old brush and pen */
	    SetBrushOrgEx (dst->dc, old_brush_origin.x, old_brush_origin.y, NULL);
	    SelectObject (dst->dc, old_brush);
	    DeleteObject (brush);

	    return CAIRO_STATUS_SUCCESS;
	}

	/* If we were not able to use PatBlt, then manually expand out the blit */

	/* Arbitrary factor; we think that going through
	 * fallback will be faster if we have to do more
	 * than this amount of blits in either direction.
	 */
	if (dst_r.width / src_extents.width > 5 ||
	    dst_r.height / src_extents.height > 5)
	    goto UNSUPPORTED;

	for (rendered_height = 0;
	     rendered_height < dst_r.height;
	     rendered_height += to_render_height)
	{
	    piece_y = (src_start_y + rendered_height) % src_extents.height;
	    to_render_height = src_extents.height - piece_y;

	    if (rendered_height + to_render_height > dst_r.height)
		to_render_height = dst_r.height - rendered_height;

	    for (rendered_width = 0;
		 rendered_width < dst_r.width;
		 rendered_width += to_render_width)
	    {
		piece_x = (src_start_x + rendered_width) % src_extents.width;
		to_render_width = src_extents.width - piece_x;

		if (rendered_width + to_render_width > dst_r.width)
		    to_render_width = dst_r.width - rendered_width;

		piece_src_r.x = piece_x;
		piece_src_r.y = piece_y;
		piece_src_r.width = to_render_width;
		piece_src_r.height = to_render_height;

		piece_dst_r.x = dst_r.x + rendered_width;
		piece_dst_r.y = dst_r.y + rendered_height;
		piece_dst_r.width = to_render_width;
		piece_dst_r.height = to_render_height;

		status = _cairo_win32_surface_composite_inner (src, src_image, dst,
							       src_extents, piece_src_r, piece_dst_r,
							       alpha, needs_alpha, needs_scale);
		if (status != CAIRO_STATUS_SUCCESS) {
		    /* Uh oh.  If something failed, and it's the first
		     * piece, then we can jump to UNSUPPORTED. 
		     * Otherwise, this is bad times, because part of the
		     * rendering was already done. */
		    if (rendered_width == 0 &&
			rendered_height == 0)
		    {
			goto UNSUPPORTED;
		    }

		    return status;
		}
	    }
	}
    } else {
	status = _cairo_win32_surface_composite_inner (src, src_image, dst,
						       src_extents, src_r, dst_r,
						       alpha, needs_alpha, needs_scale);
    }

    if (status == CAIRO_STATUS_SUCCESS)
	return status;

UNSUPPORTED:
    /* Fall back to image surface directly, if this is a DIB surface */
    if (dst->image) {
	GdiFlush();

	return dst->image->backend->composite (op, pattern, mask_pattern,
					       dst->image,
					       src_x, src_y,
					       mask_x, mask_y,
					       dst_x, dst_y,
					       width, height,
					       clip_region);
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

/* This big function tells us how to optimize operators for the
 * case of solid destination and constant-alpha source
 *
 * Note: This function needs revisiting if we add support for
 *       super-luminescent colors (a == 0, r,g,b > 0)
 */
static enum { DO_CLEAR, DO_SOURCE, DO_NOTHING, DO_UNSUPPORTED }
categorize_solid_dest_operator (cairo_operator_t op,
				unsigned short   alpha)
{
    enum { SOURCE_TRANSPARENT, SOURCE_LIGHT, SOURCE_SOLID, SOURCE_OTHER } source;

    if (alpha >= 0xff00)
	source = SOURCE_SOLID;
    else if (alpha < 0x100)
	source = SOURCE_TRANSPARENT;
    else
	source = SOURCE_OTHER;

    switch (op) {
    case CAIRO_OPERATOR_CLEAR:    /* 0                 0 */
    case CAIRO_OPERATOR_OUT:      /* 1 - Ab            0 */
	return DO_CLEAR;
	break;

    case CAIRO_OPERATOR_SOURCE:   /* 1                 0 */
    case CAIRO_OPERATOR_IN:       /* Ab                0 */
	return DO_SOURCE;
	break;

    case CAIRO_OPERATOR_OVER:     /* 1            1 - Aa */
    case CAIRO_OPERATOR_ATOP:     /* Ab           1 - Aa */
	if (source == SOURCE_SOLID)
	    return DO_SOURCE;
	else if (source == SOURCE_TRANSPARENT)
	    return DO_NOTHING;
	else
	    return DO_UNSUPPORTED;
	break;

    case CAIRO_OPERATOR_DEST_OUT: /* 0            1 - Aa */
    case CAIRO_OPERATOR_XOR:      /* 1 - Ab       1 - Aa */
	if (source == SOURCE_SOLID)
	    return DO_CLEAR;
	else if (source == SOURCE_TRANSPARENT)
	    return DO_NOTHING;
	else
	    return DO_UNSUPPORTED;
    	break;

    case CAIRO_OPERATOR_DEST:     /* 0                 1 */
    case CAIRO_OPERATOR_DEST_OVER:/* 1 - Ab            1 */
    case CAIRO_OPERATOR_SATURATE: /* min(1,(1-Ab)/Aa)  1 */
	return DO_NOTHING;
	break;

    case CAIRO_OPERATOR_DEST_IN:  /* 0                Aa */
    case CAIRO_OPERATOR_DEST_ATOP:/* 1 - Ab           Aa */
	if (source == SOURCE_SOLID)
	    return DO_NOTHING;
	else if (source == SOURCE_TRANSPARENT)
	    return DO_CLEAR;
	else
	    return DO_UNSUPPORTED;
	break;

    case CAIRO_OPERATOR_ADD:	  /* 1                1 */
	if (source == SOURCE_TRANSPARENT)
	    return DO_NOTHING;
	else
	    return DO_UNSUPPORTED;
	break;
    }

    ASSERT_NOT_REACHED;
    return DO_UNSUPPORTED;
}

static cairo_status_t
_cairo_win32_surface_fill_rectangles_stretchdib (HDC                   dc,
                                                 const cairo_color_t   *color,
                                                 cairo_rectangle_int_t *rect,
                                                 int                   num_rects)
{
    BITMAPINFO bi;
    int pixel = ((color->alpha_short >> 8) << 24) |
                ((color->red_short >> 8) << 16) |
                ((color->green_short >> 8) << 8) |
                (color->blue_short >> 8);
    int i;

    /* Experiments suggest that it's impossible to use FillRect to set the alpha value
       of a Win32 HDC for a transparent window. So instead we use StretchDIBits of a single
       pixel, which does work. */
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = 1;
    bi.bmiHeader.biHeight = 1;
    bi.bmiHeader.biSizeImage = 0;
    bi.bmiHeader.biXPelsPerMeter = PELS_72DPI;
    bi.bmiHeader.biYPelsPerMeter = PELS_72DPI;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biClrUsed = 0;
    bi.bmiHeader.biClrImportant = 0;

    for (i = 0; i < num_rects; i++) {
        if (!StretchDIBits (dc,
                            /* dst x,y,w,h */
                            rect[i].x, rect[i].y, rect[i].width, rect[i].height,
                            /* src x,y,w,h */
                            0, 0, 1, 1,
                            &pixel,
                            &bi,
                            DIB_RGB_COLORS,
                            SRCCOPY))
            return _cairo_win32_print_gdi_error ("_cairo_win32_surface_fill_rectangles_stretchdib(StretchDIBits)");
    }
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_win32_surface_fill_rectangles (void			*abstract_surface,
				      cairo_operator_t		op,
				      const cairo_color_t	*color,
				      cairo_rectangle_int_t	*rects,
				      int			num_rects)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_status_t status;
    COLORREF new_color;
    HBRUSH new_brush;
    int i;

    status = _cairo_win32_surface_set_clip_region (surface, NULL);
    if (status)
        return status;

    if (surface->format == CAIRO_FORMAT_ARGB32 &&
        (surface->flags & CAIRO_WIN32_SURFACE_CAN_STRETCHDIB) &&
        (op == CAIRO_OPERATOR_SOURCE ||
         (op == CAIRO_OPERATOR_OVER && color->alpha_short >= 0xff00))) {
        return _cairo_win32_surface_fill_rectangles_stretchdib (surface->dc,
                                                                color, rects, num_rects);
    }
    if (surface->format != CAIRO_FORMAT_RGB24)
        return CAIRO_INT_STATUS_UNSUPPORTED;

    /* Optimize for no destination alpha (surface->pixman_image is non-NULL for all
     * surfaces with alpha.)
     */
    switch (categorize_solid_dest_operator (op, color->alpha_short)) {
    case DO_CLEAR:
	new_color = RGB (0, 0, 0);
	break;
    case DO_SOURCE:
	new_color = RGB (color->red_short >> 8, color->green_short >> 8, color->blue_short >> 8);
	break;
    case DO_NOTHING:
	return CAIRO_STATUS_SUCCESS;
    case DO_UNSUPPORTED:
    default:
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    new_brush = CreateSolidBrush (new_color);
    if (!new_brush)
	return _cairo_win32_print_gdi_error ("_cairo_win32_surface_fill_rectangles");

    for (i = 0; i < num_rects; i++) {
	RECT rect;

	rect.left = rects[i].x;
	rect.top = rects[i].y;
	rect.right = rects[i].x + rects[i].width;
	rect.bottom = rects[i].y + rects[i].height;

	if (!FillRect (surface->dc, &rect, new_brush))
	    goto FAIL;
    }

    DeleteObject (new_brush);

    return CAIRO_STATUS_SUCCESS;

 FAIL:
    status = _cairo_win32_print_gdi_error ("_cairo_win32_surface_fill_rectangles");

    DeleteObject (new_brush);

    return status;
}

cairo_bool_t
_cairo_win32_surface_get_extents (void		          *abstract_surface,
				  cairo_rectangle_int_t   *rectangle)
{
    cairo_win32_surface_t *surface = abstract_surface;

    *rectangle = surface->extents;
    return TRUE;
}

static cairo_status_t
_cairo_win32_surface_flush (void *abstract_surface)
{
    return _cairo_win32_surface_set_clip_region (abstract_surface, NULL);
}

#define STACK_GLYPH_SIZE 256

cairo_int_status_t
_cairo_win32_surface_show_glyphs_internal (void			*surface,
					   cairo_operator_t	 op,
					   const cairo_pattern_t	*source,
					   cairo_glyph_t		*glyphs,
					   int			 num_glyphs,
					   cairo_scaled_font_t	*scaled_font,
					   cairo_clip_t		*clip,
					   int			*remaining_glyphs,
					   cairo_bool_t           glyph_indexing)
{
#ifdef CAIRO_HAS_WIN32_FONT
    if (scaled_font->backend->type == CAIRO_FONT_TYPE_DWRITE) {
#ifdef CAIRO_HAS_DWRITE_FONT
        return _cairo_dwrite_show_glyphs_on_surface(surface, op, source, glyphs, num_glyphs, scaled_font, clip);
#endif
    } else {
	cairo_win32_surface_t *dst = surface;
        
	WORD glyph_buf_stack[STACK_GLYPH_SIZE];
	WORD *glyph_buf = glyph_buf_stack;
	int dxy_buf_stack[2 * STACK_GLYPH_SIZE];
	int *dxy_buf = dxy_buf_stack;

	BOOL win_result = 0;
	int i, j;

	cairo_solid_pattern_t *solid_pattern;
	COLORREF color;

	cairo_matrix_t device_to_logical;

	int start_x, start_y;
	double user_x, user_y;
	int logical_x, logical_y;
	unsigned int glyph_index_option;

	/* We can only handle win32 fonts */
	if (cairo_scaled_font_get_type (scaled_font) != CAIRO_FONT_TYPE_WIN32)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	/* We can only handle opaque solid color sources */
	if (!_cairo_pattern_is_opaque_solid(source))
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	/* We can only handle operator SOURCE or OVER with the destination
	 * having no alpha */
	if ((op != CAIRO_OPERATOR_SOURCE && op != CAIRO_OPERATOR_OVER) ||
	    (dst->format != CAIRO_FORMAT_RGB24))
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	/* If we have a fallback mask clip set on the dst, we have
	 * to go through the fallback path, but only if we're not
	 * doing this for printing */
	if (clip != NULL) {
	    if ((dst->flags & CAIRO_WIN32_SURFACE_FOR_PRINTING) == 0) {
		cairo_region_t *clip_region;
		cairo_status_t status;

		status = _cairo_clip_get_region (clip, &clip_region);
		assert (status != CAIRO_INT_STATUS_NOTHING_TO_DO);
		if (status)
		    return status;

		_cairo_win32_surface_set_clip_region (surface, clip_region);
	    }
	} else {
	    _cairo_win32_surface_set_clip_region (surface, NULL);
	}

	solid_pattern = (cairo_solid_pattern_t *)source;
	color = RGB(((int)solid_pattern->color.red_short) >> 8,
		    ((int)solid_pattern->color.green_short) >> 8,
		    ((int)solid_pattern->color.blue_short) >> 8);

	cairo_win32_scaled_font_get_device_to_logical(scaled_font, &device_to_logical);

	SaveDC(dst->dc);

	cairo_win32_scaled_font_select_font(scaled_font, dst->dc);
	SetTextColor(dst->dc, color);
	SetTextAlign(dst->dc, TA_BASELINE | TA_LEFT);
	SetBkMode(dst->dc, TRANSPARENT);

	if (num_glyphs > STACK_GLYPH_SIZE) {
	    glyph_buf = (WORD *) _cairo_malloc_ab (num_glyphs, sizeof(WORD));
	    dxy_buf = (int *) _cairo_malloc_abc (num_glyphs, sizeof(int), 2);
	}

	/* It is vital that dx values for dxy_buf are calculated from the delta of
	 * _logical_ x coordinates (not user x coordinates) or else the sum of all
	 * previous dx values may start to diverge from the current glyph's x
	 * coordinate due to accumulated rounding error. As a result strings could
	 * be painted shorter or longer than expected. */

	user_x = glyphs[0].x;
	user_y = glyphs[0].y;

	cairo_matrix_transform_point(&device_to_logical,
				     &user_x, &user_y);

	logical_x = _cairo_lround (user_x);
	logical_y = _cairo_lround (user_y);

	start_x = logical_x;
	start_y = logical_y;

	for (i = 0, j = 0; i < num_glyphs; ++i, j = 2 * i) {
	    glyph_buf[i] = (WORD) glyphs[i].index;
	    if (i == num_glyphs - 1) {
		dxy_buf[j] = 0;
		dxy_buf[j+1] = 0;
	    } else {
		double next_user_x = glyphs[i+1].x;
		double next_user_y = glyphs[i+1].y;
		int next_logical_x, next_logical_y;

		cairo_matrix_transform_point(&device_to_logical,
					     &next_user_x, &next_user_y);

		next_logical_x = _cairo_lround (next_user_x);
		next_logical_y = _cairo_lround (next_user_y);

		dxy_buf[j] = _cairo_lround (next_logical_x - logical_x);
		dxy_buf[j+1] = _cairo_lround (logical_y - next_logical_y);
		    /* note that GDI coordinate system is inverted */

		logical_x = next_logical_x;
		logical_y = next_logical_y;
	    }
	}

	if (glyph_indexing)
	    glyph_index_option = ETO_GLYPH_INDEX;
	else
	    glyph_index_option = 0;

	win_result = ExtTextOutW(dst->dc,
				 start_x,
				 start_y,
				 glyph_index_option | ETO_PDY,
				 NULL,
				 glyph_buf,
				 num_glyphs,
				 dxy_buf);
	if (!win_result) {
	    _cairo_win32_print_gdi_error("_cairo_win32_surface_show_glyphs(ExtTextOutW failed)");
	}

	RestoreDC(dst->dc, -1);

	if (glyph_buf != glyph_buf_stack) {
	    free(glyph_buf);
	    free(dxy_buf);
	}
	return (win_result) ? CAIRO_STATUS_SUCCESS : CAIRO_INT_STATUS_UNSUPPORTED;
    }
#else
    return CAIRO_INT_STATUS_UNSUPPORTED;
#endif
}

#undef STACK_GLYPH_SIZE

cairo_int_status_t
_cairo_win32_surface_show_glyphs (void			*surface,
 				  cairo_operator_t	 op,
 				  const cairo_pattern_t *source,
 				  cairo_glyph_t	 	*glyphs,
 				  int			 num_glyphs,
 				  cairo_scaled_font_t	*scaled_font,
 				  cairo_clip_t          *clip,
 				  int		      	*remaining_glyphs)
{
    return _cairo_win32_surface_show_glyphs_internal (surface,
 						      op,
 						      source,
 						      glyphs,
 						      num_glyphs,
 						      scaled_font,
 						      clip,
 						      remaining_glyphs,
 						      TRUE);
}

static cairo_surface_t *
cairo_win32_surface_create_internal (HDC hdc, cairo_format_t format)
{
    cairo_win32_surface_t *surface;

    RECT rect;

    surface = malloc (sizeof (cairo_win32_surface_t));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    if (_cairo_win32_save_initial_clip (hdc, surface) != CAIRO_STATUS_SUCCESS) {
	free (surface);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    surface->clip_region = NULL;
    surface->image = NULL;
    surface->format = format;

    surface->dc = hdc;
    surface->bitmap = NULL;
    surface->is_dib = FALSE;
    surface->saved_dc_bitmap = NULL;
    surface->brush = NULL;
    surface->old_brush = NULL;
    surface->font_subsets = NULL;

    GetClipBox(hdc, &rect);
    surface->extents.x = rect.left;
    surface->extents.y = rect.top;
    surface->extents.width = rect.right - rect.left;
    surface->extents.height = rect.bottom - rect.top;

    surface->flags = _cairo_win32_flags_for_dc (surface->dc);

    _cairo_surface_init (&surface->base,
			 &cairo_win32_surface_backend,
			 NULL, /* device */
			 _cairo_content_from_format (format));

    return &surface->base;
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
 * cairo_win32_surface_create_with_dib() or call
 * cairo_win32_surface_create_with_alpha.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_win32_surface_create (HDC hdc)
{
    /* Assume everything comes in as RGB24 */
    return cairo_win32_surface_create_internal(hdc, CAIRO_FORMAT_RGB24);
}

/**
 * cairo_win32_surface_create_with_alpha:
 * @hdc: the DC to create a surface for
 *
 * Creates a cairo surface that targets the given DC.  The DC will be
 * queried for its initial clip extents, and this will be used as the
 * size of the cairo surface.  The resulting surface will always be of
 * format %CAIRO_FORMAT_ARGB32; this format is used when drawing into
 * transparent windows.
 *
 * Return value: the newly created surface
 *
 * Since: 1.10
 **/
cairo_surface_t *
cairo_win32_surface_create_with_alpha (HDC hdc)
{
    return cairo_win32_surface_create_internal(hdc, CAIRO_FORMAT_ARGB32);
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
    return _cairo_win32_surface_create_for_dc (NULL, format, width, height);
}

/**
 * cairo_win32_surface_create_with_ddb:
 * @hdc: the DC to create a surface for
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
 * Since: 1.4
 **/
cairo_surface_t *
cairo_win32_surface_create_with_ddb (HDC hdc,
				     cairo_format_t format,
				     int width,
				     int height)
{
    cairo_win32_surface_t *new_surf;
    HBITMAP ddb;
    HDC screen_dc, ddb_dc;
    HBITMAP saved_dc_bitmap;

    if (format != CAIRO_FORMAT_RGB24)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
/* XXX handle these eventually
	format != CAIRO_FORMAT_A8 ||
	format != CAIRO_FORMAT_A1)
*/

    if (!hdc) {
	screen_dc = GetDC (NULL);
	hdc = screen_dc;
    } else {
	screen_dc = NULL;
    }

    ddb_dc = CreateCompatibleDC (hdc);
    if (ddb_dc == NULL) {
	new_surf = (cairo_win32_surface_t*) _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
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
	new_surf = (cairo_win32_surface_t*) _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	goto FINISH;
    }

    saved_dc_bitmap = SelectObject (ddb_dc, ddb);

    new_surf = (cairo_win32_surface_t*) cairo_win32_surface_create (ddb_dc);
    new_surf->bitmap = ddb;
    new_surf->saved_dc_bitmap = saved_dc_bitmap;
    new_surf->is_dib = FALSE;

FINISH:
    if (screen_dc)
	ReleaseDC (NULL, screen_dc);

    return (cairo_surface_t*) new_surf;
}

/**
 * _cairo_surface_is_win32:
 * @surface: a #cairo_surface_t
 *
 * Checks if a surface is a win32 surface.  This will
 * return False if this is a win32 printing surface; use
 * _cairo_surface_is_win32_printing() to check for that.
 *
 * Return value: True if the surface is an win32 surface
 **/
int
_cairo_surface_is_win32 (cairo_surface_t *surface)
{
    return surface->backend == &cairo_win32_surface_backend;
}

/**
 * cairo_win32_surface_get_dc
 * @surface: a #cairo_surface_t
 *
 * Returns the HDC associated with this surface, or %NULL if none.
 * Also returns %NULL if the surface is not a win32 surface.
 *
 * Return value: HDC or %NULL if no HDC available.
 *
 * Since: 1.2
 **/
HDC
cairo_win32_surface_get_dc (cairo_surface_t *surface)
{
    cairo_win32_surface_t *winsurf;

    if (_cairo_surface_is_win32 (surface)){
	winsurf = (cairo_win32_surface_t *) surface;

	return winsurf->dc;
    }

    if (_cairo_surface_is_paginated (surface)) {
	cairo_surface_t *target;

	target = _cairo_paginated_surface_get_target (surface);

#ifndef CAIRO_OMIT_WIN32_PRINTING
	if (_cairo_surface_is_win32_printing (target)) {
	    winsurf = (cairo_win32_surface_t *) target;
	    return winsurf->dc;
	}
#endif
    }

    return NULL;
}


HDC
cairo_win32_get_dc_with_clip (cairo_t *cr)
{
    cairo_surface_t *surface = cr->gstate->target;
    cairo_clip_t clip;
    _cairo_clip_init_copy(&clip, &cr->gstate->clip);

    if (_cairo_surface_is_win32 (surface)){
	cairo_win32_surface_t *winsurf = (cairo_win32_surface_t *) surface;
	cairo_region_t *clip_region = NULL;
	cairo_status_t status;

	if (clip.path) {
	    status = _cairo_clip_get_region (&clip, &clip_region);
	    assert (status != CAIRO_INT_STATUS_NOTHING_TO_DO);
	    if (status) {
		_cairo_clip_fini(&clip);
		return NULL;
	    }
	}
	_cairo_win32_surface_set_clip_region (winsurf, clip_region);

	_cairo_clip_fini(&clip);
	return winsurf->dc;
    }

    if (_cairo_surface_is_paginated (surface)) {
	cairo_surface_t *target;

	target = _cairo_paginated_surface_get_target (surface);

#ifndef CAIRO_OMIT_WIN32_PRINTING
	if (_cairo_surface_is_win32_printing (target)) {
	    cairo_status_t status;
	    cairo_win32_surface_t *winsurf = (cairo_win32_surface_t *) target;

	    status = _cairo_surface_clipper_set_clip (&winsurf->clipper, &clip);

	    _cairo_clip_fini(&clip);

	    if (status)
		return NULL;

	    return winsurf->dc;
	}
#endif
    }

    _cairo_clip_fini(&clip);
    return NULL;
}



/**
 * cairo_win32_surface_get_image
 * @surface: a #cairo_surface_t
 *
 * Returns a #cairo_surface_t image surface that refers to the same bits
 * as the DIB of the Win32 surface.  If the passed-in win32 surface
 * is not a DIB surface, %NULL is returned.
 *
 * Return value: a #cairo_surface_t (owned by the win32 #cairo_surface_t),
 * or %NULL if the win32 surface is not a DIB.
 *
 * Since: 1.4
 */
cairo_surface_t *
cairo_win32_surface_get_image (cairo_surface_t *surface)
{
    if (!_cairo_surface_is_win32(surface))
	return NULL;

    return ((cairo_win32_surface_t*)surface)->image;
}

static cairo_bool_t
_cairo_win32_surface_is_similar (void *surface_a,
	                         void *surface_b)
{
    cairo_win32_surface_t *a = surface_a;
    cairo_win32_surface_t *b = surface_b;

    return a->dc == b->dc;
}

typedef struct _cairo_win32_surface_span_renderer {
    cairo_span_renderer_t base;

    cairo_operator_t op;
    const cairo_pattern_t *pattern;
    cairo_antialias_t antialias;

    uint8_t *mask_data;
    uint32_t mask_stride;

    cairo_image_surface_t *mask;
    cairo_win32_surface_t *dst;
    cairo_region_t *clip_region;

    cairo_composite_rectangles_t composite_rectangles;
} cairo_win32_surface_span_renderer_t;

static cairo_status_t
_cairo_win32_surface_span_renderer_render_rows (
    void				*abstract_renderer,
    int					 y,
    int					 height,
    const cairo_half_open_span_t	*spans,
    unsigned				 num_spans)
{
    cairo_win32_surface_span_renderer_t *renderer = abstract_renderer;
    while (height--)
	_cairo_image_surface_span_render_row (y++, spans, num_spans, renderer->mask_data, renderer->mask_stride);
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_win32_surface_span_renderer_destroy (void *abstract_renderer)
{
    cairo_win32_surface_span_renderer_t *renderer = abstract_renderer;
    if (!renderer) return;

    if (renderer->mask != NULL)
	cairo_surface_destroy (&renderer->mask->base);

    free (renderer);
}

static cairo_status_t
_cairo_win32_surface_span_renderer_finish (void *abstract_renderer)
{
    cairo_win32_surface_span_renderer_t *renderer = abstract_renderer;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (renderer->pattern == NULL || renderer->mask == NULL)
	return CAIRO_STATUS_SUCCESS;

    status = cairo_surface_status (&renderer->mask->base);
    if (status == CAIRO_STATUS_SUCCESS) {
	cairo_composite_rectangles_t *rects = &renderer->composite_rectangles;
	cairo_win32_surface_t *dst = renderer->dst;
	cairo_pattern_t *mask_pattern = cairo_pattern_create_for_surface (&renderer->mask->base);
	/* composite onto the image surface directly if we can */
	if (dst->image) {
	    GdiFlush(); /* XXX: I'm not sure if this needed or not */

	    status = dst->image->backend->composite (renderer->op,
		    renderer->pattern, mask_pattern, dst->image,
		    rects->bounded.x, rects->bounded.y,
		    0, 0,		/* mask.x, mask.y */
		    rects->bounded.x, rects->bounded.y,
		    rects->bounded.width, rects->bounded.height,
		    renderer->clip_region);
	} else {
	    /* otherwise go through the fallback_composite path which
	     * will do the appropriate surface acquisition */
	    status = _cairo_surface_fallback_composite (
		    renderer->op,
		    renderer->pattern, mask_pattern, &dst->base,
		    rects->bounded.x, rects->bounded.y,
		    0, 0,		/* mask.x, mask.y */
		    rects->bounded.x, rects->bounded.y,
		    rects->bounded.width, rects->bounded.height,
		    renderer->clip_region);
	}
	cairo_pattern_destroy (mask_pattern);

    }
    if (status != CAIRO_STATUS_SUCCESS)
	return _cairo_span_renderer_set_error (abstract_renderer,
					       status);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_win32_surface_check_span_renderer (cairo_operator_t	  op,
					  const cairo_pattern_t  *pattern,
					  void			 *abstract_dst,
					  cairo_antialias_t	  antialias)
{
    (void) op;
    (void) pattern;
    (void) abstract_dst;
    (void) antialias;
    return TRUE;
}

static cairo_span_renderer_t *
_cairo_win32_surface_create_span_renderer (cairo_operator_t	 op,
					   const cairo_pattern_t  *pattern,
					   void			*abstract_dst,
					   cairo_antialias_t	 antialias,
					   const cairo_composite_rectangles_t *rects,
					   cairo_region_t	*clip_region)
{
    cairo_win32_surface_t *dst = abstract_dst;
    cairo_win32_surface_span_renderer_t *renderer;
    cairo_status_t status;
    int width = rects->bounded.width;
    int height = rects->bounded.height;

    renderer = calloc(1, sizeof(*renderer));
    if (renderer == NULL)
	return _cairo_span_renderer_create_in_error (CAIRO_STATUS_NO_MEMORY);

    renderer->base.destroy = _cairo_win32_surface_span_renderer_destroy;
    renderer->base.finish = _cairo_win32_surface_span_renderer_finish;
    renderer->base.render_rows = _cairo_win32_surface_span_renderer_render_rows;
    renderer->op = op;
    renderer->pattern = pattern;
    renderer->antialias = antialias;
    renderer->dst = dst;
    renderer->clip_region = clip_region;

    renderer->composite_rectangles = *rects;

    /* TODO: support rendering to A1 surfaces (or: go add span
     * compositing to pixman.) */
    renderer->mask = (cairo_image_surface_t *)
	cairo_image_surface_create (CAIRO_FORMAT_A8,
				    width, height);

    status = cairo_surface_status (&renderer->mask->base);

    if (status != CAIRO_STATUS_SUCCESS) {
	_cairo_win32_surface_span_renderer_destroy (renderer);
	return _cairo_span_renderer_create_in_error (status);
    }

    renderer->mask_data = renderer->mask->data - rects->bounded.x - rects->bounded.y * renderer->mask->stride;
    renderer->mask_stride = renderer->mask->stride;
    return &renderer->base;
}


static cairo_int_status_t
_cairo_win32_surface_paint (void			*abstract_surface,
			    cairo_operator_t		 op,
			    const cairo_pattern_t	*source,
			    cairo_clip_t		*clip)
{
    cairo_win32_surface_t *surface = abstract_surface;

    if (surface->image) {
	return _cairo_surface_paint (surface->image, op, source, clip);
    }
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_win32_surface_mask (void				*abstract_surface,
			   cairo_operator_t		 op,
			   const cairo_pattern_t	*source,
			   const cairo_pattern_t	*mask,
			   cairo_clip_t			*clip)
{
    cairo_win32_surface_t *surface = abstract_surface;

    if (surface->image) {
	return _cairo_surface_mask (surface->image, op, source, mask, clip);
    }
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_win32_surface_stroke (void			*abstract_surface,
			     cairo_operator_t		 op,
			     const cairo_pattern_t	*source,
			     cairo_path_fixed_t		*path,
			     const cairo_stroke_style_t	*style,
			     const cairo_matrix_t	*ctm,
			     const cairo_matrix_t	*ctm_inverse,
			     double			 tolerance,
			     cairo_antialias_t		 antialias,
			     cairo_clip_t		*clip)
{
    cairo_win32_surface_t *surface = abstract_surface;

    if (surface->image) {
	return _cairo_surface_stroke (surface->image, op, source,
					        path, style,
					        ctm, ctm_inverse,
					        tolerance, antialias,
					        clip);

    }
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_win32_surface_fill (void				*abstract_surface,
			   cairo_operator_t		 op,
			   const cairo_pattern_t	*source,
			   cairo_path_fixed_t		*path,
			   cairo_fill_rule_t		 fill_rule,
			   double			 tolerance,
			   cairo_antialias_t		 antialias,
			   cairo_clip_t			*clip)
{
    cairo_win32_surface_t *surface = abstract_surface;

    if (surface->image) {
	return _cairo_surface_fill (surface->image, op, source,
					 path, fill_rule,
					 tolerance, antialias,
					 clip);
    }
    return CAIRO_INT_STATUS_UNSUPPORTED;
}


#include "cairoint.h"

#include "cairo-boxes-private.h"
#include "cairo-clip-private.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-error-private.h"
#include "cairo-region-private.h"
#include "cairo-spans-private.h"
#include "cairo-surface-fallback-private.h"

typedef struct {
    cairo_surface_t *dst;
    cairo_rectangle_int_t extents;
    cairo_image_surface_t *image;
    cairo_rectangle_int_t image_rect;
    void *image_extra;
} fallback_state_t;

/**
 * _fallback_init:
 *
 * Acquire destination image surface needed for an image-based
 * fallback.
 *
 * Return value: %CAIRO_INT_STATUS_NOTHING_TO_DO if the extents are not
 * visible, %CAIRO_STATUS_SUCCESS if some portion is visible and all
 * went well, or some error status otherwise.
 **/
static cairo_int_status_t
_fallback_init (fallback_state_t *state,
		cairo_surface_t  *dst,
		int               x,
		int               y,
		int               width,
		int               height)
{
    cairo_status_t status;

    state->extents.x = x;
    state->extents.y = y;
    state->extents.width = width;
    state->extents.height = height;

    state->dst = dst;

    status = _cairo_surface_acquire_dest_image (dst, &state->extents,
						&state->image, &state->image_rect,
						&state->image_extra);
    if (unlikely (status))
	return status;


    /* XXX: This NULL value tucked away in state->image is a rather
     * ugly interface. Cleaner would be to push the
     * CAIRO_INT_STATUS_NOTHING_TO_DO value down into
     * _cairo_surface_acquire_dest_image and its backend
     * counterparts. */
    assert (state->image != NULL);

    return CAIRO_STATUS_SUCCESS;
}

static void
_fallback_fini (fallback_state_t *state)
{
    _cairo_surface_release_dest_image (state->dst, &state->extents,
				       state->image, &state->image_rect,
				       state->image_extra);
}

typedef cairo_status_t
(*cairo_draw_func_t) (void                          *closure,
		      cairo_operator_t               op,
		      const cairo_pattern_t         *src,
		      cairo_surface_t               *dst,
		      int                            dst_x,
		      int                            dst_y,
		      const cairo_rectangle_int_t   *extents,
		      cairo_region_t		    *clip_region);

static cairo_status_t
_create_composite_mask_pattern (cairo_surface_pattern_t       *mask_pattern,
				cairo_clip_t                  *clip,
				cairo_draw_func_t              draw_func,
				void                          *draw_closure,
				cairo_surface_t               *dst,
				const cairo_rectangle_int_t   *extents)
{
    cairo_surface_t *mask;
    cairo_region_t *clip_region = NULL, *fallback_region = NULL;
    cairo_status_t status;
    cairo_bool_t clip_surface = FALSE;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	if (unlikely (_cairo_status_is_error (status) ||
		      status == CAIRO_INT_STATUS_NOTHING_TO_DO))
	{
	    return status;
	}

	clip_surface = status == CAIRO_INT_STATUS_UNSUPPORTED;
    }

    /* We need to use solid here, because to use CAIRO_OPERATOR_SOURCE with
     * a mask (as called via _cairo_surface_mask) triggers assertion failures.
     */
    mask = _cairo_surface_create_similar_solid (dst,
						CAIRO_CONTENT_ALPHA,
						extents->width,
						extents->height,
						CAIRO_COLOR_TRANSPARENT,
						TRUE);
    if (unlikely (mask->status))
	return mask->status;

    if (clip_region && (extents->x || extents->y)) {
	fallback_region = cairo_region_copy (clip_region);
	status = fallback_region->status;
	if (unlikely (status))
	    goto CLEANUP_SURFACE;

	cairo_region_translate (fallback_region,
				-extents->x,
				-extents->y);
	clip_region = fallback_region;
    }

    status = draw_func (draw_closure, CAIRO_OPERATOR_ADD,
			&_cairo_pattern_white.base, mask,
			extents->x, extents->y,
			extents,
			clip_region);
    if (unlikely (status))
	goto CLEANUP_SURFACE;

    if (clip_surface)
	status = _cairo_clip_combine_with_surface (clip, mask, extents->x, extents->y);

    _cairo_pattern_init_for_surface (mask_pattern, mask);

 CLEANUP_SURFACE:
    if (fallback_region)
        cairo_region_destroy (fallback_region);
    cairo_surface_destroy (mask);

    return status;
}

/* Handles compositing with a clip surface when the operator allows
 * us to combine the clip with the mask
 */
static cairo_status_t
_clip_and_composite_with_mask (cairo_clip_t                  *clip,
			       cairo_operator_t               op,
			       const cairo_pattern_t         *src,
			       cairo_draw_func_t              draw_func,
			       void                          *draw_closure,
			       cairo_surface_t               *dst,
			       const cairo_rectangle_int_t   *extents)
{
    cairo_surface_pattern_t mask_pattern;
    cairo_status_t status;

    status = _create_composite_mask_pattern (&mask_pattern,
					     clip,
					     draw_func, draw_closure,
					     dst, extents);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	status = _cairo_surface_composite (op,
					   src, &mask_pattern.base, dst,
					   extents->x,     extents->y,
					   0,              0,
					   extents->x,     extents->y,
					   extents->width, extents->height,
					   NULL);

	_cairo_pattern_fini (&mask_pattern.base);
    }

    return status;
}

/* Handles compositing with a clip surface when we have to do the operation
 * in two pieces and combine them together.
 */
static cairo_status_t
_clip_and_composite_combine (cairo_clip_t                  *clip,
			     cairo_operator_t               op,
			     const cairo_pattern_t         *src,
			     cairo_draw_func_t              draw_func,
			     void                          *draw_closure,
			     cairo_surface_t               *dst,
			     const cairo_rectangle_int_t   *extents)
{
    cairo_surface_t *intermediate;
    cairo_surface_pattern_t pattern;
    cairo_surface_pattern_t clip_pattern;
    cairo_surface_t *clip_surface;
    int clip_x, clip_y;
    cairo_status_t status;

    /* We'd be better off here creating a surface identical in format
     * to dst, but we have no way of getting that information. Instead
     * we ask the backend to create a similar surface of identical content,
     * in the belief that the backend will do something useful - like use
     * an identical format. For example, the xlib backend will endeavor to
     * use a compatible depth to enable core protocol routines.
     */
    intermediate =
	_cairo_surface_create_similar_scratch (dst, dst->content,
					       extents->width,
					       extents->height);
    if (intermediate == NULL) {
	intermediate =
	    _cairo_image_surface_create_with_content (dst->content,
						      extents->width,
						      extents->width);
    }
    if (unlikely (intermediate->status))
	return intermediate->status;

    /* Initialize the intermediate surface from the destination surface */
    _cairo_pattern_init_for_surface (&pattern, dst);
    status = _cairo_surface_composite (CAIRO_OPERATOR_SOURCE,
				       &pattern.base, NULL, intermediate,
				       extents->x,     extents->y,
				       0,              0,
				       0,              0,
				       extents->width, extents->height,
				       NULL);
    _cairo_pattern_fini (&pattern.base);
    if (unlikely (status))
	goto CLEANUP_SURFACE;

    status = (*draw_func) (draw_closure, op,
			   src, intermediate,
			   extents->x, extents->y,
			   extents,
			   NULL);
    if (unlikely (status))
	goto CLEANUP_SURFACE;

    assert (clip->path != NULL);
    clip_surface = _cairo_clip_get_surface (clip, dst, &clip_x, &clip_y);
    if (unlikely (clip_surface->status))
	goto CLEANUP_SURFACE;

    _cairo_pattern_init_for_surface (&clip_pattern, clip_surface);

    /* Combine that with the clip */
    status = _cairo_surface_composite (CAIRO_OPERATOR_DEST_IN,
				       &clip_pattern.base, NULL, intermediate,
				       extents->x - clip_x,
				       extents->y - clip_y,
				       0, 0,
				       0, 0,
				       extents->width, extents->height,
				       NULL);
    if (unlikely (status))
	goto CLEANUP_CLIP;

    /* Punch the clip out of the destination */
    status = _cairo_surface_composite (CAIRO_OPERATOR_DEST_OUT,
				       &clip_pattern.base, NULL, dst,
				       extents->x - clip_x,
				       extents->y - clip_y,
				       0, 0,
				       extents->x, extents->y,
				       extents->width, extents->height,
				       NULL);
    if (unlikely (status))
	goto CLEANUP_CLIP;

    /* Now add the two results together */
    _cairo_pattern_init_for_surface (&pattern, intermediate);
    status = _cairo_surface_composite (CAIRO_OPERATOR_ADD,
				       &pattern.base, NULL, dst,
				       0,              0,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height,
				       NULL);
    _cairo_pattern_fini (&pattern.base);

 CLEANUP_CLIP:
    _cairo_pattern_fini (&clip_pattern.base);
 CLEANUP_SURFACE:
    cairo_surface_destroy (intermediate);

    return status;
}

/* Handles compositing for %CAIRO_OPERATOR_SOURCE, which is special; it's
 * defined as (src IN mask IN clip) ADD (dst OUT (mask IN clip))
 */
static cairo_status_t
_clip_and_composite_source (cairo_clip_t                  *clip,
			    const cairo_pattern_t         *src,
			    cairo_draw_func_t              draw_func,
			    void                          *draw_closure,
			    cairo_surface_t               *dst,
			    const cairo_rectangle_int_t   *extents)
{
    cairo_surface_pattern_t mask_pattern;
    cairo_region_t *clip_region = NULL;
    cairo_status_t status;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	if (unlikely (_cairo_status_is_error (status) ||
		      status == CAIRO_INT_STATUS_NOTHING_TO_DO))
	{
	    return status;
	}
    }

    /* Create a surface that is mask IN clip */
    status = _create_composite_mask_pattern (&mask_pattern,
					     clip,
					     draw_func, draw_closure,
					     dst, extents);
    if (unlikely (status))
	return status;

    /* Compute dest' = dest OUT (mask IN clip) */
    status = _cairo_surface_composite (CAIRO_OPERATOR_DEST_OUT,
				       &mask_pattern.base, NULL, dst,
				       0,              0,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height,
				       clip_region);

    if (unlikely (status))
	goto CLEANUP_MASK_PATTERN;

    /* Now compute (src IN (mask IN clip)) ADD dest' */
    status = _cairo_surface_composite (CAIRO_OPERATOR_ADD,
				       src, &mask_pattern.base, dst,
				       extents->x,     extents->y,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height,
				       clip_region);

 CLEANUP_MASK_PATTERN:
    _cairo_pattern_fini (&mask_pattern.base);
    return status;
}

static int
_cairo_rectangle_empty (const cairo_rectangle_int_t *rect)
{
    return rect->width == 0 || rect->height == 0;
}

/**
 * _clip_and_composite:
 * @clip: a #cairo_clip_t
 * @op: the operator to draw with
 * @src: source pattern
 * @draw_func: function that can be called to draw with the mask onto a surface.
 * @draw_closure: data to pass to @draw_func.
 * @dst: destination surface
 * @extents: rectangle holding a bounding box for the operation; this
 *           rectangle will be used as the size for the temporary
 *           surface.
 *
 * When there is a surface clip, we typically need to create an intermediate
 * surface. This function handles the logic of creating a temporary surface
 * drawing to it, then compositing the result onto the target surface.
 *
 * @draw_func is to called to draw the mask; it will be called no more
 * than once.
 *
 * Return value: %CAIRO_STATUS_SUCCESS if the drawing succeeded.
 **/
static cairo_status_t
_clip_and_composite (cairo_clip_t                  *clip,
		     cairo_operator_t               op,
		     const cairo_pattern_t         *src,
		     cairo_draw_func_t              draw_func,
		     void                          *draw_closure,
		     cairo_surface_t               *dst,
		     const cairo_rectangle_int_t   *extents)
{
    cairo_status_t status;

    if (_cairo_rectangle_empty (extents))
	/* Nothing to do */
	return CAIRO_STATUS_SUCCESS;

    if (op == CAIRO_OPERATOR_CLEAR) {
	src = &_cairo_pattern_white.base;
	op = CAIRO_OPERATOR_DEST_OUT;
    }

    if (op == CAIRO_OPERATOR_SOURCE) {
	status = _clip_and_composite_source (clip,
					     src,
					     draw_func, draw_closure,
					     dst, extents);
    } else {
	cairo_bool_t clip_surface = FALSE;
	cairo_region_t *clip_region = NULL;

	if (clip != NULL) {
	    status = _cairo_clip_get_region (clip, &clip_region);
	    if (unlikely (_cairo_status_is_error (status) ||
			  status == CAIRO_INT_STATUS_NOTHING_TO_DO))
	    {
		return status;
	    }

	    clip_surface = status == CAIRO_INT_STATUS_UNSUPPORTED;
	}

	if (clip_surface) {
	    if (_cairo_operator_bounded_by_mask (op)) {
		status = _clip_and_composite_with_mask (clip, op,
							src,
							draw_func, draw_closure,
							dst, extents);
	    } else {
		status = _clip_and_composite_combine (clip, op,
						      src,
						      draw_func, draw_closure,
						      dst, extents);
	    }
	} else {
	    status = draw_func (draw_closure, op,
				src, dst,
				0, 0,
				extents,
				clip_region);
	}
    }

    return status;
}

/* Composites a region representing a set of trapezoids.
 */
static cairo_status_t
_composite_trap_region (cairo_clip_t            *clip,
			const cairo_pattern_t	*src,
			cairo_operator_t         op,
			cairo_surface_t         *dst,
			cairo_region_t          *trap_region,
			const cairo_rectangle_int_t   *extents)
{
    cairo_status_t status;
    cairo_surface_pattern_t mask_pattern;
    cairo_pattern_t *mask = NULL;
    int mask_x = 0, mask_y =0;

    if (clip != NULL) {
	cairo_surface_t *clip_surface = NULL;
	int clip_x, clip_y;

	clip_surface = _cairo_clip_get_surface (clip, dst, &clip_x, &clip_y);
	if (unlikely (clip_surface->status))
	    return clip_surface->status;

	if (op == CAIRO_OPERATOR_CLEAR) {
	    src = &_cairo_pattern_white.base;
	    op = CAIRO_OPERATOR_DEST_OUT;
	}

	_cairo_pattern_init_for_surface (&mask_pattern, clip_surface);
	mask_x = extents->x - clip_x;
	mask_y = extents->y - clip_y;
	mask = &mask_pattern.base;
    }

    status = _cairo_surface_composite (op, src, mask, dst,
				       extents->x, extents->y,
				       mask_x, mask_y,
				       extents->x, extents->y,
				       extents->width, extents->height,
				       trap_region);

    if (mask != NULL)
      _cairo_pattern_fini (mask);

    return status;
}

typedef struct {
    cairo_traps_t *traps;
    cairo_antialias_t antialias;
} cairo_composite_traps_info_t;

static cairo_status_t
_composite_traps_draw_func (void                          *closure,
			    cairo_operator_t               op,
			    const cairo_pattern_t         *src,
			    cairo_surface_t               *dst,
			    int                            dst_x,
			    int                            dst_y,
			    const cairo_rectangle_int_t   *extents,
			    cairo_region_t		  *clip_region)
{
    cairo_composite_traps_info_t *info = closure;
    cairo_status_t status;
    cairo_region_t *extents_region = NULL;

    if (dst_x != 0 || dst_y != 0)
	_cairo_traps_translate (info->traps, - dst_x, - dst_y);

    if (clip_region == NULL &&
        !_cairo_operator_bounded_by_source (op)) {
        extents_region = cairo_region_create_rectangle (extents);
        if (unlikely (extents_region->status))
            return extents_region->status;
        cairo_region_translate (extents_region, -dst_x, -dst_y);
        clip_region = extents_region;
    }

    status = _cairo_surface_composite_trapezoids (op,
                                                  src, dst, info->antialias,
                                                  extents->x,         extents->y,
                                                  extents->x - dst_x, extents->y - dst_y,
                                                  extents->width,     extents->height,
                                                  info->traps->traps,
                                                  info->traps->num_traps,
                                                  clip_region);

    if (extents_region)
        cairo_region_destroy (extents_region);

    return status;
}

enum {
    HAS_CLEAR_REGION = 0x1,
};

static cairo_status_t
_clip_and_composite_region (const cairo_pattern_t *src,
			    cairo_operator_t op,
			    cairo_surface_t *dst,
			    cairo_region_t *trap_region,
			    cairo_clip_t *clip,
			    cairo_rectangle_int_t *extents)
{
    cairo_region_t clear_region;
    unsigned int has_region = 0;
    cairo_status_t status;

    if (! _cairo_operator_bounded_by_mask (op) && clip == NULL) {
	/* If we optimize drawing with an unbounded operator to
	 * _cairo_surface_fill_rectangles() or to drawing with a
	 * clip region, then we have an additional region to clear.
	 */
	_cairo_region_init_rectangle (&clear_region, extents);
	status = cairo_region_subtract (&clear_region, trap_region);
	if (unlikely (status))
	    return status;

	if (! cairo_region_is_empty (&clear_region))
	    has_region |= HAS_CLEAR_REGION;
    }

    if ((src->type == CAIRO_PATTERN_TYPE_SOLID || op == CAIRO_OPERATOR_CLEAR) &&
	clip == NULL)
    {
	const cairo_color_t *color;

	if (op == CAIRO_OPERATOR_CLEAR)
	    color = CAIRO_COLOR_TRANSPARENT;
	else
	    color = &((cairo_solid_pattern_t *)src)->color;

	/* Solid rectangles special case */
	status = _cairo_surface_fill_region (dst, op, color, trap_region);
    } else {
	/* For a simple rectangle, we can just use composite(), for more
	 * rectangles, we have to set a clip region. The cost of rasterizing
	 * trapezoids is pretty high for most backends currently, so it's
	 * worthwhile even if a region is needed.
	 *
	 * If we have a clip surface, we set it as the mask; this only works
	 * for bounded operators other than SOURCE; for unbounded operators,
	 * clip and mask cannot be interchanged. For SOURCE, the operator
	 * as implemented by the backends is different in its handling
	 * of the mask then what we want.
	 *
	 * CAIRO_INT_STATUS_UNSUPPORTED will be returned if the region has
	 * more than rectangle and the destination doesn't support clip
	 * regions. In that case, we fall through.
	 */
	status = _composite_trap_region (clip, src, op, dst,
					 trap_region, extents);
    }

    if (has_region & HAS_CLEAR_REGION) {
	if (status == CAIRO_STATUS_SUCCESS) {
	    status = _cairo_surface_fill_region (dst,
						 CAIRO_OPERATOR_CLEAR,
						 CAIRO_COLOR_TRANSPARENT,
						 &clear_region);
	}
	_cairo_region_fini (&clear_region);
    }

    return status;
}

/* avoid using region code to re-validate boxes */
static cairo_status_t
_fill_rectangles (cairo_surface_t *dst,
		  cairo_operator_t op,
		  const cairo_pattern_t *src,
		  cairo_traps_t *traps,
		  cairo_clip_t *clip)
{
    const cairo_color_t *color;
    cairo_rectangle_int_t stack_rects[CAIRO_STACK_ARRAY_LENGTH (cairo_rectangle_int_t)];
    cairo_rectangle_int_t *rects = stack_rects;
    cairo_status_t status;
    int i;

    if (! traps->is_rectilinear || ! traps->maybe_region)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* XXX: convert clip region to geometric boxes? */
    if (clip != NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* XXX: fallback for the region_subtract() operation */
    if (! _cairo_operator_bounded_by_mask (op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! (src->type == CAIRO_PATTERN_TYPE_SOLID || op == CAIRO_OPERATOR_CLEAR))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (traps->has_intersections) {
	if (traps->is_rectangular) {
	    status = _cairo_bentley_ottmann_tessellate_rectangular_traps (traps, CAIRO_FILL_RULE_WINDING);
	} else {
	    status = _cairo_bentley_ottmann_tessellate_rectilinear_traps (traps, CAIRO_FILL_RULE_WINDING);
	}
	if (unlikely (status))
	    return status;
    }

    for (i = 0; i < traps->num_traps; i++) {
	if (! _cairo_fixed_is_integer (traps->traps[i].top)          ||
	    ! _cairo_fixed_is_integer (traps->traps[i].bottom)       ||
	    ! _cairo_fixed_is_integer (traps->traps[i].left.p1.x)    ||
	    ! _cairo_fixed_is_integer (traps->traps[i].right.p1.x))
	{
	    traps->maybe_region = FALSE;
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    }

    if (traps->num_traps > ARRAY_LENGTH (stack_rects)) {
	rects = _cairo_malloc_ab (traps->num_traps,
				  sizeof (cairo_rectangle_int_t));
	if (unlikely (rects == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    for (i = 0; i < traps->num_traps; i++) {
	int x1 = _cairo_fixed_integer_part (traps->traps[i].left.p1.x);
	int y1 = _cairo_fixed_integer_part (traps->traps[i].top);
	int x2 = _cairo_fixed_integer_part (traps->traps[i].right.p1.x);
	int y2 = _cairo_fixed_integer_part (traps->traps[i].bottom);

	rects[i].x = x1;
	rects[i].y = y1;
	rects[i].width = x2 - x1;
	rects[i].height = y2 - y1;
    }

    if (op == CAIRO_OPERATOR_CLEAR)
	color = CAIRO_COLOR_TRANSPARENT;
    else
	color = &((cairo_solid_pattern_t *)src)->color;

    status =  _cairo_surface_fill_rectangles (dst, op, color, rects, i);

    if (rects != stack_rects)
	free (rects);

    return status;
}

/* fast-path for very common composite of a single rectangle */
static cairo_status_t
_composite_rectangle (cairo_surface_t *dst,
		      cairo_operator_t op,
		      const cairo_pattern_t *src,
		      cairo_traps_t *traps,
		      cairo_clip_t *clip)
{
    cairo_rectangle_int_t rect;

    if (clip != NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (traps->num_traps > 1 || ! traps->is_rectilinear || ! traps->maybe_region)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _cairo_fixed_is_integer (traps->traps[0].top)          ||
	! _cairo_fixed_is_integer (traps->traps[0].bottom)       ||
	! _cairo_fixed_is_integer (traps->traps[0].left.p1.x)    ||
	! _cairo_fixed_is_integer (traps->traps[0].right.p1.x))
    {
	traps->maybe_region = FALSE;
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    rect.x = _cairo_fixed_integer_part (traps->traps[0].left.p1.x);
    rect.y = _cairo_fixed_integer_part (traps->traps[0].top);
    rect.width  = _cairo_fixed_integer_part (traps->traps[0].right.p1.x) - rect.x;
    rect.height = _cairo_fixed_integer_part (traps->traps[0].bottom) - rect.y;

    return _cairo_surface_composite (op, src, NULL, dst,
				     rect.x, rect.y,
				     0, 0,
				     rect.x, rect.y,
				     rect.width, rect.height,
				     NULL);
}

/* Warning: This call modifies the coordinates of traps */
static cairo_status_t
_clip_and_composite_trapezoids (const cairo_pattern_t *src,
				cairo_operator_t op,
				cairo_surface_t *dst,
				cairo_traps_t *traps,
				cairo_antialias_t antialias,
				cairo_clip_t *clip,
				cairo_rectangle_int_t *extents)
{
    cairo_composite_traps_info_t traps_info;
    cairo_region_t *clip_region = NULL;
    cairo_bool_t clip_surface = FALSE;
    cairo_status_t status;

    if (traps->num_traps == 0 && _cairo_operator_bounded_by_mask (op))
	return CAIRO_STATUS_SUCCESS;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	if (unlikely (_cairo_status_is_error (status)))
	    return status;
	if (unlikely (status == CAIRO_INT_STATUS_NOTHING_TO_DO))
	    return CAIRO_STATUS_SUCCESS;

	clip_surface = status == CAIRO_INT_STATUS_UNSUPPORTED;
    }

    /* Use a fast path if the trapezoids consist of a simple region,
     * but we can only do this if we do not have a clip surface, or can
     * substitute the mask with the clip.
     */
    if (! clip_surface ||
	(_cairo_operator_bounded_by_mask (op) && op != CAIRO_OPERATOR_SOURCE))
    {
	cairo_region_t *trap_region = NULL;

        if (_cairo_operator_bounded_by_source (op)) {
            status = _fill_rectangles (dst, op, src, traps, clip);
            if (status != CAIRO_INT_STATUS_UNSUPPORTED)
                return status;

            status = _composite_rectangle (dst, op, src, traps, clip);
            if (status != CAIRO_INT_STATUS_UNSUPPORTED)
                return status;
        }

	status = _cairo_traps_extract_region (traps, &trap_region);
	if (unlikely (_cairo_status_is_error (status)))
	    return status;

	if (trap_region != NULL) {
	    status = cairo_region_intersect_rectangle (trap_region, extents);
	    if (unlikely (status)) {
		cairo_region_destroy (trap_region);
		return status;
	    }

	    if (clip_region != NULL) {
		status = cairo_region_intersect (trap_region, clip_region);
		if (unlikely (status)) {
		    cairo_region_destroy (trap_region);
		    return status;
		}
	    }

	    if (_cairo_operator_bounded_by_mask (op)) {
		cairo_rectangle_int_t trap_extents;

		cairo_region_get_extents (trap_region, &trap_extents);
		if (! _cairo_rectangle_intersect (extents, &trap_extents)) {
		    cairo_region_destroy (trap_region);
		    return CAIRO_STATUS_SUCCESS;
		}
	    }

	    status = _clip_and_composite_region (src, op, dst,
						 trap_region,
						 clip_surface ? clip : NULL,
						 extents);
	    cairo_region_destroy (trap_region);

	    if (likely (status != CAIRO_INT_STATUS_UNSUPPORTED))
		return status;
	}
    }

    /* No fast path, exclude self-intersections and clip trapezoids. */
    if (traps->has_intersections) {
	if (traps->is_rectangular)
	    status = _cairo_bentley_ottmann_tessellate_rectangular_traps (traps, CAIRO_FILL_RULE_WINDING);
	else if (traps->is_rectilinear)
	    status = _cairo_bentley_ottmann_tessellate_rectilinear_traps (traps, CAIRO_FILL_RULE_WINDING);
	else
	    status = _cairo_bentley_ottmann_tessellate_traps (traps, CAIRO_FILL_RULE_WINDING);
	if (unlikely (status))
	    return status;
    }

    /* Otherwise render the trapezoids to a mask and composite in the usual
     * fashion.
     */
    traps_info.traps = traps;
    traps_info.antialias = antialias;

    return _clip_and_composite (clip, op, src,
				_composite_traps_draw_func,
				&traps_info, dst, extents);
}

typedef struct {
    cairo_polygon_t		*polygon;
    cairo_fill_rule_t		 fill_rule;
    cairo_antialias_t		 antialias;
} cairo_composite_spans_info_t;

static cairo_status_t
_composite_spans_draw_func (void                          *closure,
			    cairo_operator_t               op,
			    const cairo_pattern_t         *src,
			    cairo_surface_t               *dst,
			    int                            dst_x,
			    int                            dst_y,
			    const cairo_rectangle_int_t   *extents,
			    cairo_region_t		  *clip_region)
{
    cairo_composite_rectangles_t rects;
    cairo_composite_spans_info_t *info = closure;

    rects.source = *extents;
    rects.mask = *extents;
    rects.bounded = *extents;
    /* The incoming dst_x/y are where we're pretending the origin of
     * the dst surface is -- *not* the offset of a rectangle where
     * we'd like to place the result. */
    rects.bounded.x -= dst_x;
    rects.bounded.y -= dst_y;
    rects.unbounded = rects.bounded;
    rects.is_bounded = _cairo_operator_bounded_by_either (op);

    return _cairo_surface_composite_polygon (dst, op, src,
					     info->fill_rule,
					     info->antialias,
					     &rects,
					     info->polygon,
					     clip_region);
}

static cairo_status_t
_cairo_win32_surface_span_renderer_composite
                 (void                          *closure,
		  cairo_operator_t               op,
		  const cairo_pattern_t         *src,
                  cairo_image_surface_t         *mask,
		  cairo_win32_surface_t         *dst,
		  int                            dst_x,
		  int                            dst_y,
		  const cairo_rectangle_int_t   *extents,
		  cairo_region_t		*clip_region)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (src == NULL || mask == NULL)
	return CAIRO_STATUS_SUCCESS;

    status = cairo_surface_status (&mask->base);
    if (status == CAIRO_STATUS_SUCCESS) {
	cairo_pattern_t *mask_pattern = cairo_pattern_create_for_surface (&mask->base);
	/* composite onto the image surface directly if we can */
	if (dst->image) {
	    GdiFlush(); /* XXX: I'm not sure if this needed or not */

	    status = dst->image->backend->composite (op,
		    src, mask_pattern, dst->image,
                    extents->x, extents->y,
		    0, 0,		/* mask.x, mask.y */
                    extents->x - dst_x, extents->y - dst_y,
                    extents->width, extents->height,
		    clip_region);
	} else {
	    /* otherwise go through the fallback_composite path which
	     * will do the appropriate surface acquisition */
	    status = _cairo_surface_fallback_composite (
		    op,
		    src, mask_pattern, &dst->base,
                    extents->x, extents->y,
		    0, 0,		/* mask.x, mask.y */
                    extents->x - dst_x, extents->y - dst_y,
                    extents->width, extents->height,
		    clip_region);
	}
	cairo_pattern_destroy (mask_pattern);

    }
    return status;
}
#if 0
static cairo_span_renderer_t *
_cairo_win32_surface_create_span_renderer (cairo_operator_t	 op,
					   const cairo_pattern_t  *pattern,
					   void			*abstract_dst,
					   cairo_antialias_t	 antialias,
					   const cairo_composite_rectangles_t *rects,
					   cairo_region_t	*clip_region)
{
    cairo_win32_surface_t *dst = abstract_dst;
    cairo_win32_surface_span_renderer_t *renderer;
    cairo_status_t status;
    int width = rects->bounded.width;
    int height = rects->bounded.height;

    renderer = calloc(1, sizeof(*renderer));
    if (renderer == NULL)
	return _cairo_span_renderer_create_in_error (CAIRO_STATUS_NO_MEMORY);

    renderer->base.destroy = _cairo_win32_surface_span_renderer_destroy;
    renderer->base.finish = _cairo_win32_surface_span_renderer_finish;
    renderer->base.render_rows = _cairo_win32_surface_span_renderer_render_rows;
    renderer->op = op;
    renderer->pattern = pattern;
    renderer->antialias = antialias;
    renderer->dst = dst;
    renderer->clip_region = clip_region;

    renderer->composite_rectangles = *rects;

    /* TODO: support rendering to A1 surfaces (or: go add span
     * compositing to pixman.) */
    renderer->mask = (cairo_image_surface_t *)
	cairo_image_surface_create (CAIRO_FORMAT_A8,
				    width, height);

    status = cairo_surface_status (&renderer->mask->base);

    if (status != CAIRO_STATUS_SUCCESS) {
	_cairo_win32_surface_span_renderer_destroy (renderer);
	return _cairo_span_renderer_create_in_error (status);
    }

    renderer->mask_data = renderer->mask->data - rects->bounded.x - rects->bounded.y * renderer->mask->stride;
    renderer->mask_stride = renderer->mask->stride;
    return &renderer->base;
}
#endif

typedef struct _cairo_image_surface_span_renderer {
    cairo_span_renderer_t base;

    uint8_t *mask_data;
    uint32_t mask_stride;
} cairo_image_surface_span_renderer_t;

cairo_status_t
_cairo_image_surface_span (void *abstract_renderer,
			   int y, int height,
			   const cairo_half_open_span_t *spans,
			   unsigned num_spans);

static cairo_status_t
_composite_spans (void                          *closure,
		  cairo_operator_t               op,
		  const cairo_pattern_t         *src,
		  cairo_surface_t               *dst,
		  int                            dst_x,
		  int                            dst_y,
		  const cairo_rectangle_int_t   *extents,
		  cairo_region_t		*clip_region)
{
    cairo_composite_spans_info_t *info = closure;
    cairo_image_surface_span_renderer_t renderer;
    cairo_scan_converter_t *converter;
    cairo_image_surface_t *mask;
    cairo_status_t status;

    converter = _cairo_tor_scan_converter_create (extents->x, extents->y,
						  extents->x + extents->width,
						  extents->y + extents->height,
						  info->fill_rule);
    status = converter->add_polygon (converter, info->polygon);
    if (unlikely (status))
	goto CLEANUP_CONVERTER;

    /* TODO: support rendering to A1 surfaces (or: go add span
     * compositing to pixman.) */

    {
	mask = cairo_image_surface_create (CAIRO_FORMAT_A8,
				           extents->width,
                                           extents->height);

	if (cairo_surface_status(&mask->base)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto CLEANUP_CONVERTER;
	}
    }

    renderer.base.render_rows = _cairo_image_surface_span;
    renderer.mask_stride = cairo_image_surface_get_stride (mask);
    renderer.mask_data = cairo_image_surface_get_data (mask);
    renderer.mask_data -= extents->y * renderer.mask_stride + extents->x;

    status = converter->generate (converter, &renderer.base);
    if (unlikely (status))
	goto CLEANUP_RENDERER;

    _cairo_win32_surface_span_renderer_composite
                 (closure,
		  op,
		  src,
                  mask,
		  (cairo_win32_surface_t*)dst, // ewwww
		  dst_x,
		  dst_y,
		  extents,
		  clip_region);
#if 0
    {
	pixman_image_t *src;
	int src_x, src_y;

	src = _pixman_image_for_pattern (pattern, FALSE, extents, &src_x, &src_y);
	if (unlikely (src == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto CLEANUP_RENDERER;
	}

	pixman_image_composite32 (_pixman_operator (op), src, mask, dst,
                                  extents->x + src_x, extents->y + src_y,
                                  0, 0, /* mask.x, mask.y */
                                  extents->x - dst_x, extents->y - dst_y,
                                  extents->width, extents->height);
	pixman_image_unref (src);
    }
#endif
 CLEANUP_RENDERER:
    cairo_surface_destroy (mask);
 CLEANUP_CONVERTER:
    converter->destroy (converter);
    return status;
}



cairo_status_t
_cairo_win32_surface_fallback_paint (cairo_surface_t		*surface,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*source,
			       cairo_clip_t		*clip)
{
    cairo_composite_rectangles_t extents;
    cairo_rectangle_int_t rect;
    cairo_clip_path_t *clip_path = clip ? clip->path : NULL;
    cairo_box_t boxes_stack[32], *clip_boxes = boxes_stack;
    cairo_boxes_t  boxes;
    int num_boxes = ARRAY_LENGTH (boxes_stack);
    cairo_status_t status;
    cairo_traps_t traps;

    if (!_cairo_surface_get_extents (surface, &rect))
        ASSERT_NOT_REACHED;

    status = _cairo_composite_rectangles_init_for_paint (&extents,
							 &rect,
							 op, source,
							 clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    status = _cairo_clip_to_boxes (&clip, &extents, &clip_boxes, &num_boxes);
    if (unlikely (status))
	return status;

    /* If the clip cannot be reduced to a set of boxes, we will need to
     * use a clipmask. Paint is special as it is the only operation that
     * does not implicitly use a mask, so we may be able to reduce this
     * operation to a fill...
     */
    if (clip != NULL && clip_path->prev == NULL &&
	_cairo_operator_bounded_by_mask (op))
    {
	return _cairo_surface_fill (surface, op, source,
				    &clip_path->path,
				    clip_path->fill_rule,
				    clip_path->tolerance,
				    clip_path->antialias,
				    NULL);
    }

    /* meh, surface-fallback is dying anyway... */
    _cairo_boxes_init_for_array (&boxes, clip_boxes, num_boxes);
    status = _cairo_traps_init_boxes (&traps, &boxes);
    if (unlikely (status))
	goto CLEANUP_BOXES;

    status = _clip_and_composite_trapezoids (source, op, surface,
					     &traps, CAIRO_ANTIALIAS_DEFAULT,
					     clip,
                                             extents.is_bounded ? &extents.bounded : &extents.unbounded);
    _cairo_traps_fini (&traps);

CLEANUP_BOXES:
    if (clip_boxes != boxes_stack)
	free (clip_boxes);

    return status;
}

static cairo_status_t
_cairo_surface_mask_draw_func (void                        *closure,
			       cairo_operator_t             op,
			       const cairo_pattern_t       *src,
			       cairo_surface_t             *dst,
			       int                          dst_x,
			       int                          dst_y,
			       const cairo_rectangle_int_t *extents,
			       cairo_region_t		   *clip_region)
{
    cairo_pattern_t *mask = closure;
    cairo_status_t status;
    cairo_region_t *extents_region = NULL;

    if (clip_region == NULL &&
        !_cairo_operator_bounded_by_source (op)) {
        extents_region = cairo_region_create_rectangle (extents);
        if (unlikely (extents_region->status))
            return extents_region->status;
        cairo_region_translate (extents_region, -dst_x, -dst_y);
        clip_region = extents_region;
    }

    if (src) {
	status = _cairo_surface_composite (op,
                                           src, mask, dst,
                                           extents->x,         extents->y,
                                           extents->x,         extents->y,
                                           extents->x - dst_x, extents->y - dst_y,
                                           extents->width,     extents->height,
                                           clip_region);
    } else {
	status = _cairo_surface_composite (op,
                                           mask, NULL, dst,
                                           extents->x,         extents->y,
                                           0,                  0, /* unused */
                                           extents->x - dst_x, extents->y - dst_y,
                                           extents->width,     extents->height,
                                           clip_region);
    }

    if (extents_region)
        cairo_region_destroy (extents_region);

    return status;
}

cairo_status_t
_cairo_win32_surface_fallback_mask (cairo_surface_t		*surface,
			      cairo_operator_t		 op,
			      const cairo_pattern_t	*source,
			      const cairo_pattern_t	*mask,
			      cairo_clip_t		*clip)
{
    cairo_composite_rectangles_t extents;
    cairo_rectangle_int_t rect;
    cairo_status_t status;

    if (!_cairo_surface_get_extents (surface, &rect))
        ASSERT_NOT_REACHED;

    status = _cairo_composite_rectangles_init_for_mask (&extents,
							&rect,
							op, source, mask, clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    if (clip != NULL && extents.is_bounded) {
	status = _cairo_clip_rectangle (clip, &extents.bounded);
	if (unlikely (status))
	    return status;
    }

    return _clip_and_composite (clip, op, source,
				_cairo_surface_mask_draw_func,
				(void *) mask,
				surface,
                                extents.is_bounded ? &extents.bounded : &extents.unbounded);
}

cairo_status_t
_cairo_win32_surface_fallback_stroke (cairo_surface_t		*surface,
				cairo_operator_t	 op,
				const cairo_pattern_t	*source,
				cairo_path_fixed_t	*path,
				const cairo_stroke_style_t	*stroke_style,
				const cairo_matrix_t		*ctm,
				const cairo_matrix_t		*ctm_inverse,
				double			 tolerance,
				cairo_antialias_t	 antialias,
				cairo_clip_t		*clip)
{
    cairo_polygon_t polygon;
    cairo_traps_t traps;
    cairo_box_t boxes_stack[32], *clip_boxes = boxes_stack;
    int num_boxes = ARRAY_LENGTH (boxes_stack);
    cairo_composite_rectangles_t extents;
    cairo_rectangle_int_t rect;
    cairo_status_t status;

    if (!_cairo_surface_get_extents (surface, &rect))
        ASSERT_NOT_REACHED;

    status = _cairo_composite_rectangles_init_for_stroke (&extents,
							  &rect,
							  op, source,
							  path, stroke_style, ctm,
							  clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    status = _cairo_clip_to_boxes (&clip, &extents, &clip_boxes, &num_boxes);
    if (unlikely (status))
	return status;

    _cairo_polygon_init (&polygon);
    _cairo_polygon_limit (&polygon, clip_boxes, num_boxes);

    _cairo_traps_init (&traps);
    _cairo_traps_limit (&traps, clip_boxes, num_boxes);

    if (path->is_rectilinear) {
	status = _cairo_path_fixed_stroke_rectilinear_to_traps (path,
								stroke_style,
								ctm,
								&traps);
	if (likely (status == CAIRO_STATUS_SUCCESS))
	    goto DO_TRAPS;

	if (_cairo_status_is_error (status))
	    goto CLEANUP;
    }

    status = _cairo_path_fixed_stroke_to_polygon (path,
						  stroke_style,
						  ctm, ctm_inverse,
						  tolerance,
						  &polygon);
    if (unlikely (status))
	goto CLEANUP;

    if (polygon.num_edges == 0)
	goto DO_TRAPS;

    if (_cairo_operator_bounded_by_mask (op)) {
	_cairo_box_round_to_rectangle (&polygon.extents, &extents.mask);
	if (! _cairo_rectangle_intersect (&extents.bounded, &extents.mask))
	    goto CLEANUP;
    }

    if (_cairo_surface_check_span_renderer (op, source, surface, antialias)) {
	cairo_composite_spans_info_t info;

	info.polygon = &polygon;
	info.fill_rule = CAIRO_FILL_RULE_WINDING;
	info.antialias = antialias;

	status = _clip_and_composite (clip, op, source,
				      _composite_spans,
				      &info, surface,
                                      extents.is_bounded ? &extents.bounded : &extents.unbounded);
	goto CLEANUP;
    }

    /* Fall back to trapezoid fills. */
    status = _cairo_bentley_ottmann_tessellate_polygon (&traps,
							&polygon,
							CAIRO_FILL_RULE_WINDING);
    if (unlikely (status))
	goto CLEANUP;

  DO_TRAPS:
    status = _clip_and_composite_trapezoids (source, op, surface,
					     &traps, antialias,
					     clip,
                                             extents.is_bounded ? &extents.bounded : &extents.unbounded);
  CLEANUP:
    _cairo_traps_fini (&traps);
    _cairo_polygon_fini (&polygon);
    if (clip_boxes != boxes_stack)
	free (clip_boxes);

    return status;
}

cairo_status_t
_cairo_win32_surface_fallback_fill (cairo_surface_t		*surface,
			      cairo_operator_t		 op,
			      const cairo_pattern_t	*source,
			      cairo_path_fixed_t	*path,
			      cairo_fill_rule_t		 fill_rule,
			      double			 tolerance,
			      cairo_antialias_t		 antialias,
			      cairo_clip_t		*clip)
{
    cairo_polygon_t polygon;
    cairo_traps_t traps;
    cairo_box_t boxes_stack[32], *clip_boxes = boxes_stack;
    int num_boxes = ARRAY_LENGTH (boxes_stack);
    cairo_bool_t is_rectilinear;
    cairo_composite_rectangles_t extents;
    cairo_rectangle_int_t rect;
    cairo_status_t status;

    if (!_cairo_surface_get_extents (surface, &rect))
        ASSERT_NOT_REACHED;

    status = _cairo_composite_rectangles_init_for_fill (&extents,
							&rect,
							op, source, path,
							clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    status = _cairo_clip_to_boxes (&clip, &extents, &clip_boxes, &num_boxes);
    if (unlikely (status))
	return status;

    _cairo_traps_init (&traps);
    _cairo_traps_limit (&traps, clip_boxes, num_boxes);

    _cairo_polygon_init (&polygon);
    _cairo_polygon_limit (&polygon, clip_boxes, num_boxes);

    if (_cairo_path_fixed_fill_is_empty (path))
	goto DO_TRAPS;

    is_rectilinear = _cairo_path_fixed_is_rectilinear_fill (path);
    if (is_rectilinear) {
	status = _cairo_path_fixed_fill_rectilinear_to_traps (path,
							      fill_rule,
							      &traps);
	if (likely (status == CAIRO_STATUS_SUCCESS))
	    goto DO_TRAPS;

	if (_cairo_status_is_error (status))
	    goto CLEANUP;
    }

    status = _cairo_path_fixed_fill_to_polygon (path, tolerance, &polygon);
    if (unlikely (status))
	goto CLEANUP;

    if (polygon.num_edges == 0)
	goto DO_TRAPS;

    if (_cairo_operator_bounded_by_mask (op)) {
	_cairo_box_round_to_rectangle (&polygon.extents, &extents.mask);
	if (! _cairo_rectangle_intersect (&extents.bounded, &extents.mask))
	    goto CLEANUP;
    }

    if (is_rectilinear) {
	status = _cairo_bentley_ottmann_tessellate_rectilinear_polygon (&traps,
									&polygon,
									fill_rule);
	if (likely (status == CAIRO_STATUS_SUCCESS))
	    goto DO_TRAPS;

	if (unlikely (_cairo_status_is_error (status)))
	    goto CLEANUP;
    }


    if (_cairo_surface_check_span_renderer (op, source, surface, antialias)) {
	cairo_composite_spans_info_t info;

	info.polygon = &polygon;
	info.fill_rule = fill_rule;
	info.antialias = antialias;

	status = _clip_and_composite (clip, op, source,
				      _composite_spans,
				      &info, surface,
                                      extents.is_bounded ? &extents.bounded : &extents.unbounded);
	goto CLEANUP;
    }

    /* Fall back to trapezoid fills. */
    status = _cairo_bentley_ottmann_tessellate_polygon (&traps,
							&polygon,
							fill_rule);
    if (unlikely (status))
	goto CLEANUP;

  DO_TRAPS:
    status = _clip_and_composite_trapezoids (source, op, surface,
					     &traps, antialias,
					     clip,
                                             extents.is_bounded ? &extents.bounded : &extents.unbounded);
  CLEANUP:
    _cairo_traps_fini (&traps);
    _cairo_polygon_fini (&polygon);
    if (clip_boxes != boxes_stack)
	free (clip_boxes);

    return status;
}

typedef struct {
    cairo_scaled_font_t *font;
    cairo_glyph_t *glyphs;
    int num_glyphs;
} cairo_show_glyphs_info_t;

static cairo_status_t
_cairo_surface_old_show_glyphs_draw_func (void                          *closure,
					  cairo_operator_t               op,
					  const cairo_pattern_t         *src,
					  cairo_surface_t               *dst,
					  int                            dst_x,
					  int                            dst_y,
					  const cairo_rectangle_int_t	*extents,
					  cairo_region_t		*clip_region)
{
    cairo_show_glyphs_info_t *glyph_info = closure;
    cairo_status_t status;
    cairo_region_t *extents_region = NULL;

    if (clip_region == NULL &&
        !_cairo_operator_bounded_by_source (op)) {
        extents_region = cairo_region_create_rectangle (extents);
        if (unlikely (extents_region->status))
            return extents_region->status;
        cairo_region_translate (extents_region, -dst_x, -dst_y);
        clip_region = extents_region;
    }

    /* Modifying the glyph array is fine because we know that this function
     * will be called only once, and we've already made a copy of the
     * glyphs in the wrapper.
     */
    if (dst_x != 0 || dst_y != 0) {
	int i;

	for (i = 0; i < glyph_info->num_glyphs; ++i) {
	    ((cairo_glyph_t *) glyph_info->glyphs)[i].x -= dst_x;
	    ((cairo_glyph_t *) glyph_info->glyphs)[i].y -= dst_y;
	}
    }

    status = _cairo_surface_old_show_glyphs (glyph_info->font, op, src,
					     dst,
					     extents->x, extents->y,
					     extents->x - dst_x,
					     extents->y - dst_y,
					     extents->width,
					     extents->height,
					     glyph_info->glyphs,
					     glyph_info->num_glyphs,
					     clip_region);

    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	status = _cairo_scaled_font_show_glyphs (glyph_info->font,
                                                 op,
                                                 src, dst,
                                                 extents->x,         extents->y,
                                                 extents->x - dst_x,
                                                 extents->y - dst_y,
                                                 extents->width,     extents->height,
                                                 glyph_info->glyphs,
                                                 glyph_info->num_glyphs,
                                                 clip_region);
    }

    if (extents_region)
        cairo_region_destroy (extents_region);

    return status;
}

cairo_status_t
_cairo_win32_surface_fallback_show_glyphs (cairo_surface_t		*surface,
				     cairo_operator_t		 op,
				     const cairo_pattern_t	*source,
				     cairo_glyph_t		*glyphs,
				     int			 num_glyphs,
				     cairo_scaled_font_t	*scaled_font,
				     cairo_clip_t		*clip)
{
    cairo_show_glyphs_info_t glyph_info;
    cairo_composite_rectangles_t extents;
    cairo_rectangle_int_t rect;
    cairo_status_t status;

    if (!_cairo_surface_get_extents (surface, &rect))
        ASSERT_NOT_REACHED;

    status = _cairo_composite_rectangles_init_for_glyphs (&extents,
							  &rect,
							  op, source,
							  scaled_font,
							  glyphs, num_glyphs,
							  clip,
							  NULL);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_rectangle (clip, &extents.mask))
	clip = NULL;

    if (clip != NULL && extents.is_bounded) {
	status = _cairo_clip_rectangle (clip, &extents.bounded);
	if (unlikely (status))
	    return status;
    }

    glyph_info.font = scaled_font;
    glyph_info.glyphs = glyphs;
    glyph_info.num_glyphs = num_glyphs;

    return _clip_and_composite (clip, op, source,
				_cairo_surface_old_show_glyphs_draw_func,
				&glyph_info,
				surface,
                                extents.is_bounded ? &extents.bounded : &extents.unbounded);
}

cairo_surface_t *
_cairo_win32_surface_fallback_snapshot (cairo_surface_t *surface)
{
    cairo_surface_t *snapshot;
    cairo_status_t status;
    cairo_format_t format;
    cairo_surface_pattern_t pattern;
    cairo_image_surface_t *image;
    void *image_extra;

    status = _cairo_surface_acquire_source_image (surface,
						  &image, &image_extra);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    format = image->format;
    if (format == CAIRO_FORMAT_INVALID) {
	/* Non-standard images formats can be generated when retrieving
	 * images from unusual xservers, for example.
	 */
	format = _cairo_format_from_content (image->base.content);
    }
    snapshot = cairo_image_surface_create (format,
					   image->width,
					   image->height);
    if (cairo_surface_status (snapshot)) {
	_cairo_surface_release_source_image (surface, image, image_extra);
	return snapshot;
    }

    _cairo_pattern_init_for_surface (&pattern, &image->base);
    status = _cairo_surface_paint (snapshot,
				   CAIRO_OPERATOR_SOURCE,
				   &pattern.base,
				   NULL);
    _cairo_pattern_fini (&pattern.base);
    _cairo_surface_release_source_image (surface, image, image_extra);
    if (unlikely (status)) {
	cairo_surface_destroy (snapshot);
	return _cairo_surface_create_in_error (status);
    }

    return snapshot;
}

cairo_status_t
_cairo_win32_surface_fallback_composite (cairo_operator_t		 op,
				   const cairo_pattern_t	*src,
				   const cairo_pattern_t	*mask,
				   cairo_surface_t		*dst,
				   int				 src_x,
				   int				 src_y,
				   int				 mask_x,
				   int				 mask_y,
				   int				 dst_x,
				   int				 dst_y,
				   unsigned int			 width,
				   unsigned int			 height,
				   cairo_region_t		*clip_region)
{
    fallback_state_t state;
    cairo_region_t *fallback_region = NULL;
    cairo_status_t status;

    status = _fallback_init (&state, dst, dst_x, dst_y, width, height);
    if (unlikely (status))
	return status;

    /* We know this will never fail with the image backend; but
     * instead of calling into it directly, we call
     * _cairo_surface_composite so that we get the correct device
     * offset handling.
     */

    if (clip_region != NULL && (state.image_rect.x || state.image_rect.y)) {
	fallback_region = cairo_region_copy (clip_region);
	status = fallback_region->status;
	if (unlikely (status))
	    goto FAIL;

	cairo_region_translate (fallback_region,
				-state.image_rect.x,
				-state.image_rect.y);
	clip_region = fallback_region;
    }

    status = _cairo_surface_composite (op, src, mask,
				       &state.image->base,
				       src_x, src_y, mask_x, mask_y,
				       dst_x - state.image_rect.x,
				       dst_y - state.image_rect.y,
				       width, height,
				       clip_region);
  FAIL:
    if (fallback_region != NULL)
	cairo_region_destroy (fallback_region);
    _fallback_fini (&state);

    return status;
}

cairo_status_t
_cairo_win32_surface_fallback_fill_rectangles (cairo_surface_t         *surface,
					 cairo_operator_t	  op,
					 const cairo_color_t	 *color,
					 cairo_rectangle_int_t   *rects,
					 int			  num_rects)
{
    fallback_state_t state;
    cairo_rectangle_int_t *offset_rects = NULL;
    cairo_status_t status;
    int x1, y1, x2, y2;
    int i;

    assert (surface->snapshot_of == NULL);

    if (num_rects <= 0)
	return CAIRO_STATUS_SUCCESS;

    /* Compute the bounds of the rectangles, so that we know what area of the
     * destination surface to fetch
     */
    x1 = rects[0].x;
    y1 = rects[0].y;
    x2 = rects[0].x + rects[0].width;
    y2 = rects[0].y + rects[0].height;

    for (i = 1; i < num_rects; i++) {
	if (rects[i].x < x1)
	    x1 = rects[i].x;
	if (rects[i].y < y1)
	    y1 = rects[i].y;

	if ((int) (rects[i].x + rects[i].width) > x2)
	    x2 = rects[i].x + rects[i].width;
	if ((int) (rects[i].y + rects[i].height) > y2)
	    y2 = rects[i].y + rects[i].height;
    }

    status = _fallback_init (&state, surface, x1, y1, x2 - x1, y2 - y1);
    if (unlikely (status))
	return status;

    /* If the fetched image isn't at 0,0, we need to offset the rectangles */

    if (state.image_rect.x != 0 || state.image_rect.y != 0) {
	offset_rects = _cairo_malloc_ab (num_rects, sizeof (cairo_rectangle_int_t));
	if (unlikely (offset_rects == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto DONE;
	}

	for (i = 0; i < num_rects; i++) {
	    offset_rects[i].x = rects[i].x - state.image_rect.x;
	    offset_rects[i].y = rects[i].y - state.image_rect.y;
	    offset_rects[i].width = rects[i].width;
	    offset_rects[i].height = rects[i].height;
	}

	rects = offset_rects;
    }

    status = _cairo_surface_fill_rectangles (&state.image->base,
					     op, color,
					     rects, num_rects);

    free (offset_rects);

 DONE:
    _fallback_fini (&state);

    return status;
}

cairo_status_t
_cairo_win32_surface_fallback_composite_trapezoids (cairo_operator_t		op,
					      const cairo_pattern_t    *pattern,
					      cairo_surface_t	       *dst,
					      cairo_antialias_t		antialias,
					      int			src_x,
					      int			src_y,
					      int			dst_x,
					      int			dst_y,
					      unsigned int		width,
					      unsigned int		height,
					      cairo_trapezoid_t	       *traps,
					      int			num_traps,
					      cairo_region_t		*clip_region)
{
    fallback_state_t state;
    cairo_region_t *fallback_region = NULL;
    cairo_trapezoid_t *offset_traps = NULL;
    cairo_status_t status;

    status = _fallback_init (&state, dst, dst_x, dst_y, width, height);
    if (unlikely (status))
	return status;

    /* If the destination image isn't at 0,0, we need to offset the trapezoids */

    if (state.image_rect.x != 0 || state.image_rect.y != 0) {
	offset_traps = _cairo_malloc_ab (num_traps, sizeof (cairo_trapezoid_t));
	if (offset_traps == NULL) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto FAIL;
	}

	_cairo_trapezoid_array_translate_and_scale (offset_traps, traps, num_traps,
                                                    - state.image_rect.x, - state.image_rect.y,
                                                    1.0, 1.0);
	traps = offset_traps;

	/* similarly we need to adjust the region */
	if (clip_region != NULL) {
	    fallback_region = cairo_region_copy (clip_region);
	    status = fallback_region->status;
	    if (unlikely (status))
		goto FAIL;

	    cairo_region_translate (fallback_region,
				    -state.image_rect.x,
				    -state.image_rect.y);
	    clip_region = fallback_region;
	}
    }

    status = _cairo_surface_composite_trapezoids (op, pattern,
					          &state.image->base,
						  antialias,
						  src_x, src_y,
						  dst_x - state.image_rect.x,
						  dst_y - state.image_rect.y,
						  width, height,
						  traps, num_traps,
						  clip_region);
 FAIL:
    if (offset_traps != NULL)
	free (offset_traps);

    if (fallback_region != NULL)
	cairo_region_destroy (fallback_region);

    _fallback_fini (&state);

    return status;
}

cairo_status_t
_cairo_win32_surface_fallback_clone_similar (cairo_surface_t	*surface,
				       cairo_surface_t	*src,
				       int		 src_x,
				       int		 src_y,
				       int		 width,
				       int		 height,
				       int		*clone_offset_x,
				       int		*clone_offset_y,
				       cairo_surface_t **clone_out)
{
    cairo_surface_t *new_surface;
    cairo_surface_pattern_t pattern;
    cairo_status_t status;

    new_surface = _cairo_surface_create_similar_scratch (surface,
							 src->content,
							 width, height);
    if (new_surface == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    if (unlikely (new_surface->status))
	return new_surface->status;

    /* We have to copy these here, so that the coordinate spaces are correct */
    new_surface->device_transform = src->device_transform;
    new_surface->device_transform_inverse = src->device_transform_inverse;

    _cairo_pattern_init_for_surface (&pattern, src);
    cairo_matrix_init_translate (&pattern.base.matrix, src_x, src_y);
    pattern.base.filter = CAIRO_FILTER_NEAREST;

    status = _cairo_surface_paint (new_surface,
				   CAIRO_OPERATOR_SOURCE,
				   &pattern.base,
				   NULL);
    _cairo_pattern_fini (&pattern.base);

    if (unlikely (status)) {
	cairo_surface_destroy (new_surface);
	return status;
    }

    *clone_offset_x = src_x;
    *clone_offset_y = src_y;
    *clone_out = new_surface;
    return CAIRO_STATUS_SUCCESS;
}
static const cairo_surface_backend_t cairo_win32_surface_backend = {
    CAIRO_SURFACE_TYPE_WIN32,
    _cairo_win32_surface_create_similar,
    _cairo_win32_surface_finish,
    _cairo_win32_surface_acquire_source_image,
    _cairo_win32_surface_release_source_image,
    _cairo_win32_surface_acquire_dest_image,
    _cairo_win32_surface_release_dest_image,
    NULL, /* clone similar */
    _cairo_win32_surface_composite,
    _cairo_win32_surface_fill_rectangles,
    NULL, /* composite_trapezoids */
    _cairo_win32_surface_create_span_renderer,
    _cairo_win32_surface_check_span_renderer,
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_win32_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    _cairo_win32_surface_flush,
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    _cairo_win32_surface_fallback_paint,
    _cairo_win32_surface_fallback_mask,
    _cairo_win32_surface_fallback_stroke,
    _cairo_win32_surface_fallback_fill,
    _cairo_win32_surface_show_glyphs,

    NULL,  /* snapshot */
    _cairo_win32_surface_is_similar,
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


cairo_int_status_t
_cairo_win32_save_initial_clip (HDC hdc, cairo_win32_surface_t *surface)
{
    RECT rect;
    int clipBoxType;
    int gm;
    XFORM saved_xform;

    /* GetClipBox/GetClipRgn and friends interact badly with a world transform
     * set.  GetClipBox returns values in logical (transformed) coordinates;
     * it's unclear what GetClipRgn returns, because the region is empty in the
     * case of a SIMPLEREGION clip, but I assume device (untransformed) coordinates.
     * Similarily, IntersectClipRect works in logical units, whereas SelectClipRgn
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
	_cairo_win32_print_gdi_error ("cairo_win32_surface_create");
	SetGraphicsMode (hdc, gm);
	/* XXX: Can we make a more reasonable guess at the error cause here? */
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    surface->clip_rect.x = rect.left;
    surface->clip_rect.y = rect.top;
    surface->clip_rect.width = rect.right - rect.left;
    surface->clip_rect.height = rect.bottom - rect.top;

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

cairo_int_status_t
_cairo_win32_restore_initial_clip (cairo_win32_surface_t *surface)
{
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;

    XFORM saved_xform;
    int gm = GetGraphicsMode (surface->dc);
    if (gm == GM_ADVANCED) {
	GetWorldTransform (surface->dc, &saved_xform);
	ModifyWorldTransform (surface->dc, NULL, MWT_IDENTITY);
    }

    /* initial_clip_rgn will either be a real region or NULL (which means reset to no clip region) */
    SelectClipRgn (surface->dc, surface->initial_clip_rgn);

    if (surface->had_simple_clip) {
	/* then if we had a simple clip, intersect */
	IntersectClipRect (surface->dc,
			   surface->clip_rect.x,
			   surface->clip_rect.y,
			   surface->clip_rect.x + surface->clip_rect.width,
			   surface->clip_rect.y + surface->clip_rect.height);
    }

    if (gm == GM_ADVANCED)
	SetWorldTransform (surface->dc, &saved_xform);

    return status;
}

void
_cairo_win32_debug_dump_hrgn (HRGN rgn, char *header)
{
    RGNDATA *rd;
    unsigned int z;

    if (header)
	fprintf (stderr, "%s\n", header);

    if (rgn == NULL) {
	fprintf (stderr, " NULL\n");
    }

    z = GetRegionData(rgn, 0, NULL);
    rd = (RGNDATA*) malloc(z);
    z = GetRegionData(rgn, z, rd);

    fprintf (stderr, " %ld rects, bounds: %ld %ld %ld %ld\n",
	     rd->rdh.nCount,
	     rd->rdh.rcBound.left,
	     rd->rdh.rcBound.top,
	     rd->rdh.rcBound.right - rd->rdh.rcBound.left,
	     rd->rdh.rcBound.bottom - rd->rdh.rcBound.top);

    for (z = 0; z < rd->rdh.nCount; z++) {
	RECT r = ((RECT*)rd->Buffer)[z];
	fprintf (stderr, " [%d]: [%ld %ld %ld %ld]\n",
		 z, r.left, r.top, r.right - r.left, r.bottom - r.top);
    }

    free(rd);
    fflush (stderr);
}

/**
 * cairo_win32_surface_set_can_convert_to_dib
 * @surface: a #cairo_surface_t
 * @can_convert: a #cairo_bool_t indicating whether this surface can
 *               be coverted to a DIB if necessary
 *
 * A DDB surface with this flag set can be converted to a DIB if it's
 * used as a source in a way that GDI can't natively handle; for
 * example, drawing a RGB24 DDB onto an ARGB32 DIB.  Doing this
 * conversion results in a significant speed optimization, because we
 * can call on pixman to perform the operation natively, instead of
 * reading the data from the DC each time.
 *
 * Return value: %CAIRO_STATUS_SUCCESS if the flag was successfully
 * changed, or an error otherwise.
 * 
 */
cairo_status_t
cairo_win32_surface_set_can_convert_to_dib (cairo_surface_t *asurface, cairo_bool_t can_convert)
{
    cairo_win32_surface_t *surface = (cairo_win32_surface_t*) asurface;
    if (surface->base.type != CAIRO_SURFACE_TYPE_WIN32)
	return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;

    if (surface->bitmap) {
	if (can_convert)
	    surface->flags |= CAIRO_WIN32_SURFACE_CAN_CONVERT_TO_DIB;
	else
	    surface->flags &= ~CAIRO_WIN32_SURFACE_CAN_CONVERT_TO_DIB;
    }

    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_win32_surface_get_can_convert_to_dib
 * @surface: a #cairo_surface_t
 * @can_convert: a #cairo_bool_t* that receives the return value
 *
 * Returns the value of the flag indicating whether the surface can be
 * converted to a DIB if necessary, as set by
 * cairo_win32_surface_set_can_convert_to_dib.
 *
 * Return value: %CAIRO_STATUS_SUCCESS if the flag was successfully
 * retreived, or an error otherwise.
 * 
 */
cairo_status_t
cairo_win32_surface_get_can_convert_to_dib (cairo_surface_t *asurface, cairo_bool_t *can_convert)
{
    cairo_win32_surface_t *surface = (cairo_win32_surface_t*) asurface;
    if (surface->base.type != CAIRO_SURFACE_TYPE_WIN32)
	return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;

    *can_convert = ((surface->flags & CAIRO_WIN32_SURFACE_CAN_CONVERT_TO_DIB) != 0);
    return CAIRO_STATUS_SUCCESS;
}

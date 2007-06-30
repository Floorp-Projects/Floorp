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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
#include <windows.h>

#include <stdio.h>
#include "cairoint.h"
#include "cairo-clip-private.h"
#include "cairo-win32-private.h"

#undef DEBUG_COMPOSITE

/* for older SDKs */
#ifndef SHADEBLENDCAPS
#define SHADEBLENDCAPS  120
#endif
#ifndef SB_NONE
#define SB_NONE         0x00000000
#endif

#define PELS_72DPI  ((LONG)(72. / 0.0254))
#define NIL_SURFACE ((cairo_surface_t*)&_cairo_surface_nil)

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

    if (!FormatMessageA (FORMAT_MESSAGE_ALLOCATE_BUFFER |
			 FORMAT_MESSAGE_FROM_SYSTEM,
			 NULL,
			 last_error,
			 MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
			 (LPTSTR) &lpMsgBuf,
			 0, NULL)) {
	fprintf (stderr, "%s: Unknown GDI error", context);
    } else {
	fprintf (stderr, "%s: %s", context, (char *)lpMsgBuf);

	LocalFree (lpMsgBuf);
    }

    /* We should switch off of last_status, but we'd either return
     * CAIRO_STATUS_NO_MEMORY or CAIRO_STATUS_UNKNOWN_ERROR and there
     * is no CAIRO_STATUS_UNKNOWN_ERROR.
     */

    return CAIRO_STATUS_NO_MEMORY;
}

static uint32_t
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
		       char                 **bits_out,
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
	bitmap_info = malloc (sizeof (BITMAPINFOHEADER) + num_palette * sizeof (RGBQUAD));
	if (!bitmap_info)
	    return CAIRO_STATUS_NO_MEMORY;
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
     * NOTE: This causes problems when using BitBlt/AlphaBlend/etc!
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
    char *bits;
    int rowstride;

    _cairo_win32_initialize ();

    surface = malloc (sizeof (cairo_win32_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return NIL_SURFACE;
    }

    status = _create_dc_and_bitmap (surface, original_dc, format,
				    width, height,
				    &bits, &rowstride);
    if (status)
	goto FAIL;

    surface->image = cairo_image_surface_create_for_data (bits, format,
							  width, height, rowstride);
    if (surface->image->status) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto FAIL;
    }

    surface->format = format;

    surface->clip_rect.x = 0;
    surface->clip_rect.y = 0;
    surface->clip_rect.width = width;
    surface->clip_rect.height = height;

    surface->saved_clip = CreateRectRgn (0, 0, 0, 0);
    if (GetClipRgn (surface->dc, surface->saved_clip) == 0) {
	DeleteObject(surface->saved_clip);
	surface->saved_clip = NULL;
    }

    surface->extents = surface->clip_rect;

    _cairo_surface_init (&surface->base, &cairo_win32_surface_backend,
			 _cairo_content_from_format (format));

    return (cairo_surface_t *)surface;

 FAIL:
    if (surface->bitmap) {
	SelectObject (surface->dc, surface->saved_dc_bitmap);
	DeleteObject (surface->bitmap);
	DeleteDC (surface->dc);
    }
    if (surface)
	free (surface);

    if (status == CAIRO_STATUS_NO_MEMORY) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return NIL_SURFACE;
    } else {
	_cairo_error (status);
	return NIL_SURFACE;
    }
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
    cairo_win32_surface_t *new_surf;

    /* if the parent is a DIB or if we need alpha, then
     * we have to create a dib */
    if (force_dib || src->is_dib || (content & CAIRO_CONTENT_ALPHA)) {
	new_surf = (cairo_win32_surface_t*)
	    _cairo_win32_surface_create_for_dc (src->dc, format, width, height);
    } else {
	/* otherwise, create a ddb */
	HBITMAP ddb = CreateCompatibleBitmap (src->dc, width, height);
	HDC ddb_dc = CreateCompatibleDC (src->dc);
	HRGN crgn = CreateRectRgn (0, 0, width, height);
	HBITMAP saved_dc_bitmap;

	saved_dc_bitmap = SelectObject (ddb_dc, ddb);
	SelectClipRgn (ddb_dc, crgn);

	DeleteObject (crgn);

	new_surf = (cairo_win32_surface_t*) cairo_win32_surface_create (ddb_dc);
	new_surf->bitmap = ddb;
	new_surf->saved_dc_bitmap = saved_dc_bitmap;
	new_surf->is_dib = FALSE;
    }

    return (cairo_surface_t*) new_surf;
}

static cairo_surface_t *
_cairo_win32_surface_create_similar (void	    *abstract_src,
				     cairo_content_t content,
				     int	     width,
				     int	     height)
{
    return _cairo_win32_surface_create_similar_internal (abstract_src, content, width, height, FALSE);
}

static cairo_status_t
_cairo_win32_surface_finish (void *abstract_surface)
{
    cairo_win32_surface_t *surface = abstract_surface;

    if (surface->image)
	cairo_surface_destroy (surface->image);

    if (surface->saved_clip)
	DeleteObject (surface->saved_clip);

    /* If we created the Bitmap and DC, destroy them */
    if (surface->bitmap) {
	SelectObject (surface->dc, surface->saved_dc_bitmap);
	DeleteObject (surface->bitmap);
	DeleteDC (surface->dc);
    }

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
    if (local->base.status)
	return CAIRO_STATUS_NO_MEMORY;

    status = CAIRO_INT_STATUS_UNSUPPORTED;

    /* Check for SURFACE_IS_DISPLAY here, because there are a lot
     * of printer drivers that lie and say they can BitBlt, but
     * just spit out black instead.
     */
    if ((local->flags & CAIRO_WIN32_SURFACE_IS_DISPLAY) &&
	(local->flags & CAIRO_WIN32_SURFACE_CAN_BITBLT) &&
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

static cairo_status_t
_cairo_win32_surface_acquire_source_image (void                    *abstract_surface,
					   cairo_image_surface_t  **image_out,
					   void                   **image_extra)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_win32_surface_t *local = NULL;
    cairo_status_t status;

    if (surface->image) {
	*image_out = (cairo_image_surface_t *)surface->image;
	*image_extra = NULL;

	return CAIRO_STATUS_SUCCESS;
    }

    status = _cairo_win32_surface_get_subimage (abstract_surface, 0, 0,
						surface->clip_rect.width,
						surface->clip_rect.height, &local);
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
					 cairo_rectangle_int16_t *interest_rect,
					 cairo_image_surface_t  **image_out,
					 cairo_rectangle_int16_t *image_rect,
					 void                   **image_extra)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_win32_surface_t *local = NULL;
    cairo_status_t status;
    RECT clip_box;
    int x1, y1, x2, y2;

    if (surface->image) {
	GdiFlush();

	image_rect->x = 0;
	image_rect->y = 0;
	image_rect->width = surface->clip_rect.width;
	image_rect->height = surface->clip_rect.height;

	*image_out = (cairo_image_surface_t *)surface->image;
	*image_extra = NULL;

	return CAIRO_STATUS_SUCCESS;
    }

    if (GetClipBox (surface->dc, &clip_box) == ERROR)
	return _cairo_win32_print_gdi_error ("_cairo_win3_surface_acquire_dest_image");

    x1 = clip_box.left;
    x2 = clip_box.right;
    y1 = clip_box.top;
    y2 = clip_box.bottom;

    if (interest_rect->x > x1)
	x1 = interest_rect->x;
    if (interest_rect->y > y1)
	y1 = interest_rect->y;
    if (interest_rect->x + interest_rect->width < x2)
	x2 = interest_rect->x + interest_rect->width;
    if (interest_rect->y + interest_rect->height < y2)
	y2 = interest_rect->y + interest_rect->height;

    if (x1 >= x2 || y1 >= y2) {
	*image_out = NULL;
	*image_extra = NULL;

	return CAIRO_STATUS_SUCCESS;
    }

    status = _cairo_win32_surface_get_subimage (abstract_surface,
						x1, y1, x2 - x1, y2 - y1,
						&local);
    if (status)
	return status;

    *image_out = (cairo_image_surface_t *)local->image;
    *image_extra = local;

    image_rect->x = x1;
    image_rect->y = y1;
    image_rect->width = x2 - x1;
    image_rect->height = y2 - y1;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_win32_surface_release_dest_image (void                    *abstract_surface,
					 cairo_rectangle_int16_t *interest_rect,
					 cairo_image_surface_t   *image,
					 cairo_rectangle_int16_t *image_rect,
					 void                    *image_extra)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_win32_surface_t *local = image_extra;

    if (!local)
	return;

    if (!BitBlt (surface->dc,
		 image_rect->x, image_rect->y,
		 image_rect->width, image_rect->height,
		 local->dc,
		 0, 0,
		 SRCCOPY))
	_cairo_win32_print_gdi_error ("_cairo_win32_surface_release_dest_image");

    cairo_surface_destroy ((cairo_surface_t *)local);
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
	    HMODULE msimg32_dll = LoadLibrary ("msimg32");

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
				      cairo_rectangle_int16_t src_extents,
				      cairo_rectangle_int32_t src_r,
				      cairo_rectangle_int32_t dst_r,
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
  	return _composite_alpha_blend (dst, src, alpha,
				       src_r.x, src_r.y, src_r.width, src_r.height,
				       dst_r.x, dst_r.y, dst_r.width, dst_r.height);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_win32_surface_composite (cairo_operator_t	op,
				cairo_pattern_t       	*pattern,
				cairo_pattern_t		*mask_pattern,
				void			*abstract_dst,
				int			src_x,
				int			src_y,
				int			mask_x,
				int			mask_y,
				int			dst_x,
				int			dst_y,
				unsigned int		width,
				unsigned int		height)
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
    cairo_rectangle_int16_t src_extents;

    cairo_rectangle_int32_t src_r = { src_x, src_y, width, height };
    cairo_rectangle_int32_t dst_r = { dst_x, dst_y, width, height };

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

    /* If the src recangle doesn't wholly lie within the src extents,
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
	    src_r.x = 0;

	    dst_r.width += src_r.x;
	    dst_r.x -= src_r.x;
	}

	if (src_r.y < 0) {
	    src_r.height += src_r.y;
	    src_r.y = 0;

	    dst_r.height += dst_r.y;
	    dst_r.y -= src_r.y;
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

    /* If we need to repeat, we turn the repeated blit into
     * a bunch of piece-by-piece blits.
     */
    if (needs_repeat) {
	cairo_rectangle_int32_t piece_src_r, piece_dst_r;
	uint32_t rendered_width = 0, rendered_height = 0;
	uint32_t to_render_height, to_render_width;
	int32_t piece_x, piece_y;
	int32_t src_start_x = src_r.x % src_extents.width;
	int32_t src_start_y = src_r.y % src_extents.height;

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
					       width, height);
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

/* This big function tells us how to optimize operators for the
 * case of solid destination and constant-alpha source
 *
 * NOTE: This function needs revisiting if we add support for
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

static cairo_int_status_t
_cairo_win32_surface_fill_rectangles (void			*abstract_surface,
				      cairo_operator_t		op,
				      const cairo_color_t	*color,
				      cairo_rectangle_int16_t		*rects,
				      int			num_rects)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_status_t status;
    COLORREF new_color;
    HBRUSH new_brush;
    int i;

    /* XXXperf If it's not RGB24, we need to do a little more checking
     * to figure out when we can use GDI.  We don't have that checking
     * anywhere at the moment, so just bail and use the fallback
     * paths. */
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

static cairo_int_status_t
_cairo_win32_surface_set_clip_region (void              *abstract_surface,
				      pixman_region16_t *region)
{
    cairo_win32_surface_t *surface = abstract_surface;
    cairo_status_t status;

    /* If we are in-memory, then we set the clip on the image surface
     * as well as on the underlying GDI surface.
     */
    if (surface->image) {
	unsigned int serial;

	serial = _cairo_surface_allocate_clip_serial (surface->image);
	_cairo_surface_set_clip_region (surface->image, region, serial);
    }

    /* The semantics we want is that any clip set by cairo combines
     * is intersected with the clip on device context that the
     * surface was created for. To implement this, we need to
     * save the original clip when first setting a clip on surface.
     */

    if (region == NULL) {
	/* Clear any clip set by cairo, return to the original */
	if (SelectClipRgn (surface->dc, surface->saved_clip) == ERROR)
	    return _cairo_win32_print_gdi_error ("_cairo_win32_surface_set_clip_region (reset)");

	return CAIRO_STATUS_SUCCESS;

    } else {
	pixman_box16_t *boxes = pixman_region_rects (region);
	int num_boxes = pixman_region_num_rects (region);
	pixman_box16_t *extents = pixman_region_extents (region);
	RGNDATA *data;
	size_t data_size;
	RECT *rects;
	int i;
	HRGN gdi_region;

	/* Create a GDI region for the cairo region */

	data_size = sizeof (RGNDATAHEADER) + num_boxes * sizeof (RECT);
	data = malloc (data_size);
	if (!data)
	    return CAIRO_STATUS_NO_MEMORY;
	rects = (RECT *)data->Buffer;

	data->rdh.dwSize = sizeof (RGNDATAHEADER);
	data->rdh.iType = RDH_RECTANGLES;
	data->rdh.nCount = num_boxes;
	data->rdh.nRgnSize = num_boxes * sizeof (RECT);
	data->rdh.rcBound.left = extents->x1;
	data->rdh.rcBound.top = extents->y1;
	data->rdh.rcBound.right = extents->x2;
	data->rdh.rcBound.bottom = extents->y2;

	for (i = 0; i < num_boxes; i++) {
	    rects[i].left = boxes[i].x1;
	    rects[i].top = boxes[i].y1;
	    rects[i].right = boxes[i].x2;
	    rects[i].bottom = boxes[i].y2;
	}

	gdi_region = ExtCreateRegion (NULL, data_size, data);
	free (data);

	if (!gdi_region)
	    return CAIRO_STATUS_NO_MEMORY;

	/* Combine the new region with the original clip */

	if (surface->saved_clip) {
	    if (CombineRgn (gdi_region, gdi_region, surface->saved_clip, RGN_AND) == ERROR)
		goto FAIL;
	}

	if (SelectClipRgn (surface->dc, gdi_region) == ERROR)
	    goto FAIL;

	DeleteObject (gdi_region);
	return CAIRO_STATUS_SUCCESS;

    FAIL:
	status = _cairo_win32_print_gdi_error ("_cairo_win32_surface_set_clip_region");
	DeleteObject (gdi_region);
	return status;
    }
}

static cairo_int_status_t
_cairo_win32_surface_get_extents (void		          *abstract_surface,
				  cairo_rectangle_int16_t *rectangle)
{
    cairo_win32_surface_t *surface = abstract_surface;

    *rectangle = surface->extents;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_win32_surface_flush (void *abstract_surface)
{
    return _cairo_surface_reset_clip (abstract_surface);
}

#define STACK_GLYPH_SIZE 256

static cairo_int_status_t
_cairo_win32_surface_show_glyphs (void			*surface,
				  cairo_operator_t	 op,
				  cairo_pattern_t	*source,
				  cairo_glyph_t		*glyphs,
				  int			 num_glyphs,
				  cairo_scaled_font_t	*scaled_font)
{
#if CAIRO_HAS_WIN32_FONT
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
     * to go through the fallback path */
    if (dst->base.clip &&
	(dst->base.clip->mode != CAIRO_CLIP_MODE_REGION ||
	 dst->base.clip->surface != NULL))
	return CAIRO_INT_STATUS_UNSUPPORTED;

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
	glyph_buf = (WORD *)malloc(num_glyphs * sizeof(WORD));
        dxy_buf = (int *)malloc(num_glyphs * 2 * sizeof(int));
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
            dxy_buf[j+1] = _cairo_lround (next_logical_y - logical_y);

            logical_x = next_logical_x;
            logical_y = next_logical_y;
        }
    }

    win_result = ExtTextOutW(dst->dc,
                             start_x,
                             start_y,
                             ETO_GLYPH_INDEX | ETO_PDY,
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
#else
    return CAIRO_INT_STATUS_UNSUPPORTED;
#endif
}

#undef STACK_GLYPH_SIZE

/**
 * cairo_win32_surface_create:
 * @hdc: the DC to create a surface for
 *
 * Creates a cairo surface that targets the given DC.  The DC will be
 * queried for its initial clip extents, and this will be used as the
 * size of the cairo surface.  Also, if the DC is a raster DC, it will
 * be queried for its pixel format and the cairo surface format will
 * be set appropriately.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_win32_surface_create (HDC hdc)
{
    cairo_win32_surface_t *surface;
    RECT rect;
    int depth;
    cairo_format_t format;

    _cairo_win32_initialize ();

    /* Try to figure out the drawing bounds for the Device context
     */
    if (GetClipBox (hdc, &rect) == ERROR) {
	_cairo_win32_print_gdi_error ("cairo_win32_surface_create");
	/* XXX: Can we make a more reasonable guess at the error cause here? */
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return NIL_SURFACE;
    }

    if (GetDeviceCaps(hdc, TECHNOLOGY) == DT_RASDISPLAY) {
	depth = GetDeviceCaps(hdc, BITSPIXEL);
	if (depth == 32)
	    format = CAIRO_FORMAT_RGB24;
	else if (depth == 24)
	    format = CAIRO_FORMAT_RGB24;
	else if (depth == 16)
	    format = CAIRO_FORMAT_RGB24;
	else if (depth == 8)
	    format = CAIRO_FORMAT_A8;
	else if (depth == 1)
	    format = CAIRO_FORMAT_A1;
	else {
	    _cairo_win32_print_gdi_error("cairo_win32_surface_create(bad BITSPIXEL)");
	    _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    return NIL_SURFACE;
	}
    } else {
	format = CAIRO_FORMAT_RGB24;
    }

    surface = malloc (sizeof (cairo_win32_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return NIL_SURFACE;
    }

    surface->image = NULL;
    surface->format = format;

    surface->dc = hdc;
    surface->bitmap = NULL;
    surface->is_dib = FALSE;
    surface->saved_dc_bitmap = NULL;

    surface->clip_rect.x = (int16_t) rect.left;
    surface->clip_rect.y = (int16_t) rect.top;
    surface->clip_rect.width = (uint16_t) (rect.right - rect.left);
    surface->clip_rect.height = (uint16_t) (rect.bottom - rect.top);

    if (surface->clip_rect.width == 0 ||
	surface->clip_rect.height == 0)
    {
	surface->saved_clip = NULL;
    } else {
	surface->saved_clip = CreateRectRgn (0, 0, 0, 0);
	if (GetClipRgn (hdc, surface->saved_clip) == 0) {
	    DeleteObject(surface->saved_clip);
	    surface->saved_clip = NULL;
	}
    }

    surface->extents = surface->clip_rect;

    surface->flags = _cairo_win32_flags_for_dc (surface->dc);

    _cairo_surface_init (&surface->base, &cairo_win32_surface_backend,
			 _cairo_content_from_format (format));

    return (cairo_surface_t *)surface;
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
    HRGN crgn;
    HBITMAP saved_dc_bitmap;

    if (format != CAIRO_FORMAT_RGB24)
	return NULL;
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

    ddb = CreateCompatibleBitmap (hdc, width, height);
    ddb_dc = CreateCompatibleDC (hdc);

    saved_dc_bitmap = SelectObject (ddb_dc, ddb);

    crgn = CreateRectRgn (0, 0, width, height);
    SelectClipRgn (ddb_dc, crgn);
    DeleteObject (crgn);

    new_surf = (cairo_win32_surface_t*) cairo_win32_surface_create (ddb_dc);
    new_surf->bitmap = ddb;
    new_surf->saved_dc_bitmap = saved_dc_bitmap;
    new_surf->is_dib = FALSE;

    if (screen_dc)
	ReleaseDC (NULL, screen_dc);

    return (cairo_surface_t*) new_surf;
}

/**
 * _cairo_surface_is_win32:
 * @surface: a #cairo_surface_t
 *
 * Checks if a surface is an #cairo_win32_surface_t
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
 * Returns the HDC associated with this surface, or NULL if none.
 * Also returns NULL if the surface is not a win32 surface.
 *
 * Return value: HDC or NULL if no HDC available.
 *
 * Since: 1.2
 **/
HDC
cairo_win32_surface_get_dc (cairo_surface_t *surface)
{
    cairo_win32_surface_t *winsurf;

    if (surface == NULL)
	return NULL;

    if (!_cairo_surface_is_win32(surface))
	return NULL;

    winsurf = (cairo_win32_surface_t *) surface;

    return winsurf->dc;
}

/**
 * cairo_win32_surface_get_image
 * @surface: a #cairo_surface_t
 *
 * Returns a #cairo_surface_t image surface that refers to the same bits
 * as the DIB of the Win32 surface.  If the passed-in win32 surface
 * is not a DIB surface, NULL is returned.
 *
 * Return value: a #cairo_surface_t (owned by the win32 cairo_surface_t),
 * or NULL if the win32 surface is not a DIB.
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

static const cairo_surface_backend_t cairo_win32_surface_backend = {
    CAIRO_SURFACE_TYPE_WIN32,
    _cairo_win32_surface_create_similar,
    _cairo_win32_surface_finish,
    _cairo_win32_surface_acquire_source_image,
    _cairo_win32_surface_release_source_image,
    _cairo_win32_surface_acquire_dest_image,
    _cairo_win32_surface_release_dest_image,
    NULL, /* clone_similar */
    _cairo_win32_surface_composite,
    _cairo_win32_surface_fill_rectangles,
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_win32_surface_set_clip_region,
    NULL, /* intersect_clip_path */
    _cairo_win32_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    _cairo_win32_surface_flush,
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
    _cairo_win32_surface_show_glyphs,

    NULL  /* snapshot */
};

/*
 * Without pthread, on win32 we need to initialize all the 'mutex'es
 * before use. It is guaranteed that DllMain will get called single
 * threaded before any other function.
 * Initializing more than finally needed should not matter much.
 */
#if !defined(HAVE_PTHREAD_H) 

CRITICAL_SECTION _cairo_scaled_font_map_mutex;
#ifdef CAIRO_HAS_FT_FONT
CRITICAL_SECTION _cairo_ft_unscaled_font_map_mutex;
#endif
CRITICAL_SECTION _cairo_font_face_mutex;

static int _cairo_win32_initialized = 0;

void
_cairo_win32_initialize (void) {
    if (_cairo_win32_initialized)
	return;

    /* every 'mutex' from CAIRO_MUTEX_DECALRE needs to be initialized here */
    InitializeCriticalSection (&_cairo_scaled_font_map_mutex);
#ifdef CAIRO_HAS_FT_FONT
    InitializeCriticalSection (&_cairo_ft_unscaled_font_map_mutex);
#endif
    InitializeCriticalSection (&_cairo_font_face_mutex);

    _cairo_win32_initialized = 1;
}

#if !defined(CAIRO_WIN32_STATIC_BUILD)
BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
	 DWORD     fdwReason,
	 LPVOID    lpvReserved)
{
  switch (fdwReason)
  {
  case DLL_PROCESS_ATTACH:
    _cairo_win32_initialize();
    break;
  case DLL_PROCESS_DETACH:
    DeleteCriticalSection (&_cairo_scaled_font_map_mutex);
#ifdef CAIRO_HAS_FT_FONT
    DeleteCriticalSection (&_cairo_ft_unscaled_font_map_mutex);
#endif
    DeleteCriticalSection (&_cairo_font_face_mutex);
    break;
  }
  return TRUE;
}
#endif
#else
/* Need a function definition here too since it's called outside of ifdefs */
void
_cairo_win32_initialize (void)
{
}
#endif

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

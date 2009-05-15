/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 NVIDIA Corporation
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
 * The Initial Developer of the Original Code is NVIDIA Corporation.
 *
 * Contributor(s):
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#if CAIRO_HAS_DDRAW_SURFACE

#include "cairo-clip-private.h"
#include "cairo-ddraw-private.h"
#include "cairo-region-private.h"

#include <windows.h>
#include <ddraw.h>

#ifndef DDLOCK_WAITNOTBUSY
#error Your DirectDraw header is too old.  Copy in ddraw.h from the WM6 SDK over your SDK's ddraw.h.
#error Otherwise, you can fix this code to use IDirectDraw4 and IDirectDrawSurface5.
#endif

/* DirectDraw helper macros */

#define DDCALL0(fn,p)           ((p)->lpVtbl->fn((p)))
#define DDCALL1(fn,p,a)         ((p)->lpVtbl->fn((p),(a)))
#define DDCALL2(fn,p,a,b)       ((p)->lpVtbl->fn((p),(a),(b)))
#define DDCALL3(fn,p,a,b,c)     ((p)->lpVtbl->fn((p),(a),(b),(c)))
#define DDCALL4(fn,p,a,b,c,d)   ((p)->lpVtbl->fn((p),(a),(b),(c),(d)))
#define DDCALL5(fn,p,a,b,c,d,e) ((p)->lpVtbl->fn((p),(a),(b),(c),(d),(e)))

#define IURelease(p)                  DDCALL0(Release,p)

#define IDDCreateClipper(p,a,b,c)     DDCALL3(CreateClipper,p,a,b,c)
#define IDDCreateSurface(p,a,b,c)     DDCALL3(CreateSurface,p,a,b,c)

#define IDDCSetClipList(p,a,b)        DDCALL2(SetClipList,p,a,b)

#define IDDSBlt(p,a,b,c,d,e)          DDCALL5(Blt,p,a,b,c,d,e)
#define IDDSGetDDInterface(p,a)       DDCALL1(GetDDInterface,p,a)
#define IDDSLock(p,a,b,c,d)           DDCALL4(Lock,p,a,b,c,d)
#define IDDSSetClipper(p,a)           DDCALL1(SetClipper,p,a)
#define IDDSUnlock(p,a)               DDCALL1(Unlock,p,a)
#define IDDSGetPixelFormat(p,a)       DDCALL1(GetPixelFormat,p,a)

static const cairo_surface_backend_t cairo_ddraw_surface_backend;

/**
 * _cairo_ddraw_print_ddraw_error:
 * @context: context string to display along with the error
 * @hr: HRESULT code
 *
 * Helper function to dump out a human readable form of the
 * current error code.
 *
 * Return value: A cairo status code for the error code
 **/
static cairo_status_t
_cairo_ddraw_print_ddraw_error (const char *context, HRESULT hr)
{
    /*XXX make me pretty */
    fprintf(stderr, "%s: DirectDraw error 0x%08x\n", context, hr);

    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
}

static cairo_status_t
_cairo_ddraw_surface_set_image_clip (cairo_ddraw_surface_t *surface)
{
    if (surface->image_clip_invalid) {
	surface->image_clip_invalid = FALSE;
	if (surface->has_clip_region) {
	    unsigned int serial =
		_cairo_surface_allocate_clip_serial (surface->image);
	    surface->has_image_clip = TRUE;
	    return _cairo_surface_set_clip_region (surface->image,
						   &surface->clip_region,
						   serial);
	} else {
	    surface->has_image_clip = FALSE;
	    return _cairo_surface_set_clip_region (surface->image,
						   NULL, 0);
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ddraw_surface_set_clip_list (cairo_ddraw_surface_t * surface)
{
    DWORD stack_data[CAIRO_STACK_BUFFER_SIZE / sizeof (DWORD)];
    cairo_rectangle_int_t extents;
    int num_boxes;
    RGNDATA * rgn;
    DWORD size;
    cairo_status_t status;
    cairo_point_int_t offset;
    HRESULT hr;
    RECT * prect;
    int i;

    if (!surface->clip_list_invalid)
	return CAIRO_STATUS_SUCCESS;

    surface->clip_list_invalid = FALSE;

    if (!surface->has_clip_region) {
	surface->has_clip_list = FALSE;
	return CAIRO_STATUS_SUCCESS;
    }

    surface->has_clip_list = TRUE;
	
    _cairo_region_get_extents (&surface->clip_region, &extents);
    
    rgn = (RGNDATA *) stack_data;
    num_boxes = _cairo_region_num_boxes (&surface->clip_region);
    
    size = sizeof (RGNDATAHEADER) + sizeof (RECT) * num_boxes;
    if (size > sizeof (stack_data)) {
	if ((rgn = (RGNDATA *) malloc (size)) == 0)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    offset.x = MAX (0, surface->origin.x);
    offset.y = MAX (0, surface->origin.y);
    
    rgn->rdh.dwSize = sizeof(RGNDATAHEADER);
    rgn->rdh.iType = RDH_RECTANGLES;
    rgn->rdh.nCount = num_boxes;
    rgn->rdh.nRgnSize = num_boxes * sizeof (RECT);
    rgn->rdh.rcBound.left = extents.x + offset.x;
    rgn->rdh.rcBound.top = extents.y + offset.y;
    rgn->rdh.rcBound.right = extents.x + extents.width + offset.x;
    rgn->rdh.rcBound.bottom = extents.y + extents.height + offset.y;
    
    prect = (RECT *) &rgn->Buffer;
    for (i = 0; i < num_boxes; ++i) {
	cairo_box_int_t box;
	
	_cairo_region_get_box (&surface->clip_region, i, &box);
	
	prect->left = box.p1.x + offset.x;
	prect->top = box.p1.y + offset.y;
	prect->right = box.p2.x + offset.x;
	prect->bottom = box.p2.y + offset.y;
	++prect;
    }
    
    status = CAIRO_STATUS_SUCCESS;
    
    if (FAILED(hr = IDDCSetClipList (surface->lpddc, rgn, 0)))
	status = _cairo_ddraw_print_ddraw_error ("_set_clip_region", hr);
    
    if (rgn != (RGNDATA *) stack_data)
	free (rgn);
    
    return status;
}

static cairo_status_t
_cairo_ddraw_surface_lock (cairo_ddraw_surface_t *surface)
{
    HRESULT hr;
    DDSURFACEDESC ddsd;
    cairo_image_surface_t * img = (cairo_image_surface_t *) surface->image;

    if (surface->alias) {
	cairo_ddraw_surface_t * base =
	    (cairo_ddraw_surface_t *) surface->alias;
	cairo_image_surface_t * base_img;

	if (!base->locked) {
	    cairo_status_t status =
		_cairo_ddraw_surface_lock (base);
	    if (status)
		return status;
	}

	base_img = (cairo_image_surface_t *) base->image;

	if (img) {
	    if (img->data == base_img->data + surface->data_offset &&
		img->stride == base_img->stride)
		return CAIRO_STATUS_SUCCESS;

	    cairo_surface_destroy (surface->image);
	}

	surface->data_offset =
	    MAX (0, surface->origin.y) * base_img->stride +
	    MAX (0, surface->origin.x) * 4;
	surface->image =
	    cairo_image_surface_create_for_data (base_img->data +
						 surface->data_offset,
						 surface->format,
						 surface->acquirable_rect.width,
						 surface->acquirable_rect.height,
						 base_img->stride);
	if (surface->image->status)
	    return surface->image->status;

	surface->has_image_clip = FALSE;
	surface->image_clip_invalid = surface->has_clip_region;

	return _cairo_ddraw_surface_set_image_clip (surface);
    }

    if (surface->locked)
	return CAIRO_STATUS_SUCCESS;

    ddsd.dwSize = sizeof (ddsd);
    if (FAILED(hr = IDDSLock (surface->lpdds, NULL, &ddsd,
			      DDLOCK_WAITNOTBUSY, NULL)))
	return _cairo_ddraw_print_ddraw_error ("_lock", hr);

    surface->locked = TRUE;

    if (img) {
	if (img->data == ddsd.lpSurface && img->stride == ddsd.lPitch)
	    return CAIRO_STATUS_SUCCESS;

	cairo_surface_destroy (surface->image);
    }

    surface->image =
	cairo_image_surface_create_for_data (ddsd.lpSurface,
					     surface->format,
					     ddsd.dwWidth,
					     ddsd.dwHeight,
					     ddsd.lPitch);

    if (surface->image->status)
	return surface->image->status;

    surface->has_image_clip = FALSE;
    surface->image_clip_invalid = surface->has_clip_region;

    return _cairo_ddraw_surface_set_image_clip (surface);
}

static cairo_status_t
_cairo_ddraw_surface_unlock (cairo_ddraw_surface_t *surface)
{
    HRESULT hr;

    if (surface->alias)
	surface = (cairo_ddraw_surface_t *) surface->alias;

    if (!surface->locked)
	return CAIRO_STATUS_SUCCESS;

    surface->locked = FALSE;

    if (FAILED(hr = IDDSUnlock (surface->lpdds, NULL)))
	return _cairo_ddraw_print_ddraw_error ("_unlock", hr);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_ddraw_surface_is_locked (cairo_ddraw_surface_t *surface)
{
    if (surface->alias)
	surface = (cairo_ddraw_surface_t *) surface->alias;
    return surface->locked;
}

static cairo_int_status_t
_cairo_ddraw_surface_set_clipper (cairo_ddraw_surface_t *surface)
{
    LPDIRECTDRAWCLIPPER myclip =
	surface->has_clip_list ? surface->lpddc : NULL;

    if (surface->alias)
	surface = (cairo_ddraw_surface_t *) surface->alias;

    if (surface->installed_clipper != myclip) {
	HRESULT hr;
	if (FAILED(hr = IDDSSetClipper(surface->lpdds, myclip)))
	    return _cairo_ddraw_print_ddraw_error ("_set_clipper", hr);
	surface->installed_clipper = myclip;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ddraw_surface_reset_clipper (cairo_ddraw_surface_t *surface)
{
    if (surface->alias)
	surface = (cairo_ddraw_surface_t *) surface->alias;

    if (surface->installed_clipper != NULL) {
	HRESULT hr;
	if (FAILED(hr = IDDSSetClipper(surface->lpdds, NULL)))
	    return _cairo_ddraw_print_ddraw_error ("_reset_clipper", hr);
	surface->installed_clipper = NULL;
    }

    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t
_cairo_ddraw_surface_flush (void *abstract_surface)
{
    cairo_ddraw_surface_t *surface = abstract_surface;

    return _cairo_ddraw_surface_unlock (surface);
}

cairo_status_t
_cairo_ddraw_surface_reset (cairo_surface_t *surface)
{
    return _cairo_surface_reset_clip (surface);
}

static cairo_status_t
_cairo_ddraw_surface_finish (void *abstract_surface)
{
    cairo_ddraw_surface_t *surface = abstract_surface;

    if (surface->image)
	cairo_surface_destroy (surface->image);

    if (surface->lpddc)
	IURelease(surface->lpddc);

    _cairo_region_fini (&surface->clip_region);

    if (surface->alias)
	cairo_surface_destroy (surface->alias);
    else if (surface->lpdds)
	IURelease (surface->lpdds);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ddraw_surface_acquire_source_image (void                    *abstract_surface,
					   cairo_image_surface_t  **image_out,
					   void                   **image_extra)
{
    cairo_ddraw_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (surface->alias)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if ((status = _cairo_ddraw_surface_lock (surface)))
	return status;

    *image_out = (cairo_image_surface_t *) surface->image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ddraw_surface_acquire_dest_image (void                    *abstract_surface,
					 cairo_rectangle_int_t   *interest_rect,
					 cairo_image_surface_t  **image_out,
					 cairo_rectangle_int_t   *image_rect,
					 void                   **image_extra)
{
    cairo_ddraw_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if ((status = _cairo_ddraw_surface_lock (surface)))
	return status;

    if ((status = _cairo_ddraw_surface_set_image_clip (surface)))
	return status;

    *image_out = (cairo_image_surface_t *) surface->image;
    *image_rect = surface->acquirable_rect;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ddraw_surface_get_extents (void		          *abstract_surface,
				  cairo_rectangle_int_t   *rectangle)
{
    cairo_ddraw_surface_t *surface = abstract_surface;

    *rectangle = surface->extents;

    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_ddraw_surface_set_clip_region (void *abstract_surface,
				      cairo_region_t *region)
{
    cairo_status_t status;
    cairo_ddraw_surface_t * surface =
	(cairo_ddraw_surface_t *) abstract_surface;

    if (region) {
	surface->has_clip_region = TRUE;
	surface->image_clip_invalid = TRUE;
	surface->clip_list_invalid = TRUE;

	status = _cairo_region_copy (&surface->clip_region, region);
	if (status)
	    return status;

	if (surface->origin.x < 0 || surface->origin.y < 0)
	    _cairo_region_translate (&surface->clip_region,
		MIN (0, surface->origin.x),
		MIN (0, surface->origin.y));
    } else {
	surface->has_clip_region = FALSE;
	surface->image_clip_invalid = surface->has_image_clip;
	surface->clip_list_invalid = surface->has_clip_list;
    }
    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_ddraw_surface_create:
 * @lpdd: pointer to a DirectDraw object
 * @format: format of pixels in the surface to create
 * @width: width of the surface, in pixels
 * @height: height of the surface, in pixels
 *
 * Creates a DirectDraw surface not associated with any particular existing
 * surface or The created surface will be uninitialized.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_ddraw_surface_create (LPDIRECTDRAW lpdd,
			    cairo_format_t format,
			    int	    width,
			    int	    height)
{
    cairo_ddraw_surface_t *surface;
    HRESULT hr;
    DDSURFACEDESC ddsd;

    if (format != CAIRO_FORMAT_ARGB32 &&
	format != CAIRO_FORMAT_RGB24)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));

    surface = malloc (sizeof (cairo_ddraw_surface_t));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    memset (&ddsd, 0, sizeof (ddsd));
    ddsd.dwSize = sizeof (ddsd);
    ddsd.dwFlags = DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    ddsd.dwHeight = height ? height : 1;
    ddsd.dwWidth = width ? width : 1;
    ddsd.ddpfPixelFormat.dwSize = sizeof (ddsd.ddpfPixelFormat);
    ddsd.ddpfPixelFormat.dwRGBBitCount = 32;
    ddsd.ddpfPixelFormat.dwRBitMask = 0x00ff0000;
    ddsd.ddpfPixelFormat.dwGBitMask = 0x0000ff00;
    ddsd.ddpfPixelFormat.dwBBitMask = 0x000000ff;
    if (format == CAIRO_FORMAT_ARGB32) {
	ddsd.ddpfPixelFormat.dwFlags =
	    DDPF_ALPHAPIXELS | DDPF_ALPHAPREMULT | DDPF_RGB;
	ddsd.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
    } else {
	ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
    }

    if (FAILED(hr = IDDCreateSurface (lpdd, &ddsd, &surface->lpdds, NULL))) {
	cairo_status_t status = _cairo_ddraw_print_ddraw_error ("_surface_create", hr);
	free (surface);
	return _cairo_surface_create_in_error (status);
    }

    if (FAILED(hr = IDDCreateClipper (lpdd, 0, &surface->lpddc, NULL))) {
	cairo_status_t status = _cairo_ddraw_print_ddraw_error ("_surface_create", hr);
	IURelease(surface->lpdds);
	free (surface);
	return _cairo_surface_create_in_error (status);
    }

    _cairo_region_init (&surface->clip_region);

    surface->locked = FALSE;
    surface->has_clip_region = FALSE;
    surface->has_image_clip = FALSE;
    surface->has_clip_list = FALSE;
    surface->image_clip_invalid = FALSE;
    surface->clip_list_invalid = FALSE;

    surface->format = format;
    surface->installed_clipper = NULL;
    surface->image = NULL;
    surface->alias = NULL;
    surface->data_offset = 0;

    surface->extents.x = 0;
    surface->extents.y = 0;
    surface->extents.width = ddsd.dwWidth;
    surface->extents.height = ddsd.dwHeight;

    surface->origin.x = 0;
    surface->origin.y = 0;
    
    surface->acquirable_rect.x = 0;
    surface->acquirable_rect.y = 0;
    surface->acquirable_rect.width = ddsd.dwWidth;
    surface->acquirable_rect.height = ddsd.dwHeight;

    _cairo_surface_init (&surface->base, &cairo_ddraw_surface_backend,
			 _cairo_content_from_format (format));

    return &surface->base;
}

/**
 * cairo_ddraw_surface_create_alias:
 * @base_surface: pointer to a DirectDraw surface
 * @x: x-origin of sub-surface
 * @y: y-origin of sub-surface
 * @width: width of sub-surface, in pixels
 * @height: height of the sub-surface, in pixels
 *
 * Creates an alias to an existing DirectDraw surface.  The alias is a
 * sub-surface which occupies a portion of the main surface.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_ddraw_surface_create_alias (cairo_surface_t *base_surface,
				  int x,
				  int y,
				  int width,
				  int height)
{
    cairo_ddraw_surface_t * base = (cairo_ddraw_surface_t *) base_surface;
    cairo_ddraw_surface_t * surface;
    LPDIRECTDRAW lpdd;
    HRESULT hr;

    if (base_surface->type != CAIRO_SURFACE_TYPE_DDRAW)
	_cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));

    if (FAILED(hr = IDDSGetDDInterface (base->lpdds, &lpdd))) {
	cairo_status_t status = _cairo_ddraw_print_ddraw_error ("_create_alias", hr);
	return _cairo_surface_create_in_error (status);
    }

    if (base->alias) {
	x += base->origin.x;
	y += base->origin.y;
	base = (cairo_ddraw_surface_t *) base->alias;
    }

    surface = malloc (sizeof (cairo_ddraw_surface_t));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    if (FAILED(hr = IDDCreateClipper (lpdd, 0, &surface->lpddc, NULL))) {
	cairo_status_t status = _cairo_ddraw_print_ddraw_error ("_surface_create", hr);
	free (surface);
	return _cairo_surface_create_in_error (status);
    }

    _cairo_region_init (&surface->clip_region);

    surface->locked = FALSE;
    surface->has_clip_region = FALSE;
    surface->has_image_clip = FALSE;
    surface->has_clip_list = FALSE;
    surface->image_clip_invalid = FALSE;
    surface->clip_list_invalid = FALSE;

    surface->format = base->format;
    surface->lpdds = base->lpdds;
    surface->installed_clipper = NULL;
    surface->image = NULL;
    surface->alias = cairo_surface_reference (base_surface);
    surface->data_offset = 0;

    surface->extents.x = 0;
    surface->extents.y = 0;
    surface->extents.width = width;
    surface->extents.height = height;

    surface->origin.x = x;
    surface->origin.y = y;

    surface->acquirable_rect.x = MAX (0, -x);
    surface->acquirable_rect.y = MAX (0, -y);
    surface->acquirable_rect.width =
	MIN (x + width, (int)base->extents.width) - MAX (x, 0);
    surface->acquirable_rect.height =
	MIN (y + height, (int)base->extents.height) - MAX (y, 0);

    _cairo_surface_init (&surface->base, &cairo_ddraw_surface_backend,
			 base_surface->content);

    return &surface->base;
}

/**
 * cairo_ddraw_surface_get_ddraw_surface:
 * @surface: pointer to a DirectDraw surface
 *
 * Gets a pointer to the DirectDraw Surface.
 *
 * Return value: the DirectDraw surface pointer
 **/
LPDIRECTDRAWSURFACE
cairo_ddraw_surface_get_ddraw_surface (cairo_surface_t *surface)
{
    cairo_ddraw_surface_t * dd_surf = (cairo_ddraw_surface_t *) surface;

    if (surface->type != CAIRO_SURFACE_TYPE_DDRAW)
	return NULL;

    _cairo_ddraw_surface_reset_clipper (dd_surf);
    _cairo_ddraw_surface_unlock (dd_surf);

    return dd_surf->lpdds;
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

static cairo_int_status_t
_cairo_ddraw_surface_fill_rectangles (void			*abstract_surface,
				      cairo_operator_t		op,
				      const cairo_color_t	*color,
				      cairo_rectangle_int_t	*rects,
				      int			num_rects)
{
    DDBLTFX ddbltfx;
    COLORREF new_color;
    cairo_ddraw_surface_t *surface = abstract_surface;
    cairo_status_t status;
    int i;

    /* XXXperf If it's not RGB24, we need to do a little more checking
     * to figure out when we can use GDI.  We don't have that checking
     * anywhere at the moment, so just bail and use the fallback
     * paths. */
    if (surface->format != CAIRO_FORMAT_RGB24)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    switch (categorize_solid_dest_operator (op, color->alpha_short)) {
    case DO_CLEAR:
        new_color = 0x00000000u;
	break;
    case DO_SOURCE:
        new_color = 0xff000000u |
            ((color->red_short & 0xff00u) << 8) |
            (color->green_short & 0xff00u) |
            (color->blue_short >> 8);
	break;
    case DO_NOTHING:
	return CAIRO_STATUS_SUCCESS;
    case DO_UNSUPPORTED:
    default:
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (_cairo_ddraw_surface_is_locked (surface)) {
	uint32_t sum = 0;

	/* check to see if we have enough work to do */
	for (i = 0; i < num_rects; ++i) {
	    sum += rects[i].width * rects[i].height;
	    if (sum >= FILL_THRESHOLD)
		break;
	}

	if (sum < FILL_THRESHOLD)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if ((status = _cairo_ddraw_surface_unlock (surface)))
	    return status;
    }

    if ((status = _cairo_ddraw_surface_unlock (surface)))
	return status;

    if ((status = _cairo_ddraw_surface_set_clip_list (surface)))
	return status;

    if ((status = _cairo_ddraw_surface_set_clipper (surface)))
	return status;

    memset (&ddbltfx, 0, sizeof (ddbltfx));
    ddbltfx.dwSize = sizeof (ddbltfx);
    ddbltfx.dwFillColor = new_color;

    for (i = 0; i < num_rects; i++) {
	RECT rect;
	HRESULT hr;

	rect.left = rects[i].x + surface->origin.x;
	rect.top = rects[i].y + surface->origin.y;
	rect.right = rects[i].x + rects[i].width + surface->origin.x;
	rect.bottom = rects[i].y + rects[i].height + surface->origin.y;

	if (FAILED (hr = IDDSBlt (surface->lpdds, &rect,
				 NULL, NULL,
				 DDBLT_COLORFILL | DDBLT_WAITNOTBUSY,
				 &ddbltfx)))
	    return _cairo_ddraw_print_ddraw_error ("_fill_rectangles", hr);
    }

    return CAIRO_STATUS_SUCCESS;
}


static const cairo_surface_backend_t cairo_ddraw_surface_backend = {
    CAIRO_SURFACE_TYPE_DDRAW,
    NULL, /* create_similar */
    _cairo_ddraw_surface_finish,
    _cairo_ddraw_surface_acquire_source_image,
    NULL, /* release_source_image */
    _cairo_ddraw_surface_acquire_dest_image,
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    _cairo_ddraw_surface_fill_rectangles,
    NULL, /* composite_trapezoids */
    NULL, /* create_span_renderer */
    NULL, /* check_span_renderer */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_ddraw_surface_set_clip_region,
    NULL, /* intersect_clip_path */
    _cairo_ddraw_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    _cairo_ddraw_surface_flush,
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
    NULL, /* show_glyphs */

    NULL,  /* snapshot */
    NULL, /* is_similar */

    _cairo_ddraw_surface_reset,
};

#endif /* CAIRO_HAS_DDRAW_SURFACE */

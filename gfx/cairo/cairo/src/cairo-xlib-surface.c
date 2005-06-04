/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include "cairoint.h"
#include "cairo-xlib.h"
#include "cairo-xlib-xrender.h"
#include "cairo-xlib-test.h"
#include <X11/extensions/Xrender.h>

/* Xlib doesn't define a typedef, so define one ourselves */
typedef int (*cairo_xlib_error_func_t) (Display     *display,
					XErrorEvent *event);

typedef struct _cairo_xlib_surface cairo_xlib_surface_t;

static void _cairo_xlib_surface_ensure_gc (cairo_xlib_surface_t *surface);

/*
 * Instead of taking two round trips for each blending request,
 * assume that if a particular drawable fails GetImage that it will
 * fail for a "while"; use temporary pixmaps to avoid the errors
 */

#define CAIRO_ASSUME_PIXMAP	20

struct _cairo_xlib_surface {
    cairo_surface_t base;

    Display *dpy;
    GC gc;
    Drawable drawable;
    cairo_bool_t owns_pixmap;
    Visual *visual;
    
    int use_pixmap;

    int render_major;
    int render_minor;

    int width;
    int height;
    int depth;

    Picture picture;
    XRenderPictFormat *format;
};

#define CAIRO_SURFACE_RENDER_AT_LEAST(surface, major, minor)	\
	(((surface)->render_major > major) ||			\
	 (((surface)->render_major == major) && ((surface)->render_minor >= minor)))

#define CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 0)
#define CAIRO_SURFACE_RENDER_HAS_COMPOSITE(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 0)
#define CAIRO_SURFACE_RENDER_HAS_COMPOSITE_TEXT(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 0)


#define CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLE(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 1)
#define CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLES(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 1)

#define CAIRO_SURFACE_RENDER_HAS_DISJOINT(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 2)
#define CAIRO_SURFACE_RENDER_HAS_CONJOINT(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 2)

#define CAIRO_SURFACE_RENDER_HAS_TRAPEZOIDS(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRIANGLES(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRISTRIP(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRIFAN(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)

#define CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 6)
#define CAIRO_SURFACE_RENDER_HAS_FILTERS(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 6)

static cairo_bool_t cairo_xlib_render_disabled = FALSE;

/**
 * cairo_test_xlib_disable_render:
 *
 * Disables the use of the RENDER extension.
 * 
 * <note>
 * This function is <emphasis>only</emphasis> intended for internal
 * testing use within the cairo distribution. It is not installed in
 * any public header file.
 * </note>
 **/
void
cairo_test_xlib_disable_render (void)
{
    cairo_xlib_render_disabled = TRUE;
}

static int
_CAIRO_FORMAT_DEPTH (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_A1:
	return 1;
    case CAIRO_FORMAT_A8:
	return 8;
    case CAIRO_FORMAT_RGB24:
	return 24;
    case CAIRO_FORMAT_ARGB32:
    default:
	return 32;
    }
}

static XRenderPictFormat *
_CAIRO_FORMAT_XRENDER_FORMAT(Display *dpy, cairo_format_t format)
{
    int	pict_format;
    switch (format) {
    case CAIRO_FORMAT_A1:
	pict_format = PictStandardA1; break;
    case CAIRO_FORMAT_A8:
	pict_format = PictStandardA8; break;
    case CAIRO_FORMAT_RGB24:
	pict_format = PictStandardRGB24; break;
    case CAIRO_FORMAT_ARGB32:
    default:
	pict_format = PictStandardARGB32; break;
    }
    return XRenderFindStandardFormat (dpy, pict_format);
}

static cairo_surface_t *
_cairo_xlib_surface_create_similar (void		*abstract_src,
				    cairo_format_t	format,
				    int			drawable,
				    int			width,
				    int			height)
{
    cairo_xlib_surface_t *src = abstract_src;
    Display *dpy = src->dpy;
    int scr;
    Pixmap pix;
    cairo_xlib_surface_t *surface;
    int depth = _CAIRO_FORMAT_DEPTH (format);
    XRenderPictFormat *xrender_format = _CAIRO_FORMAT_XRENDER_FORMAT (dpy, 
								      format);

    /* As a good first approximation, if the display doesn't have COMPOSITE,
     * we're better off using image surfaces for all temporary operations
     */
    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE(src)) {
	return cairo_image_surface_create (format, width, height);
    }
    
    scr = DefaultScreen (dpy);

    pix = XCreatePixmap (dpy, DefaultRootWindow (dpy),
			 width <= 0 ? 1 : width, height <= 0 ? 1 : height,
			 depth);
    
    surface = (cairo_xlib_surface_t *)
	cairo_xlib_surface_create_with_xrender_format (dpy, pix,
						       xrender_format,
						       width, height);
				 
    surface->owns_pixmap = TRUE;

    return &surface->base;
}

static cairo_status_t
_cairo_xlib_surface_finish (void *abstract_surface)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    if (surface->picture)
	XRenderFreePicture (surface->dpy, surface->picture);

    if (surface->owns_pixmap)
	XFreePixmap (surface->dpy, surface->drawable);

    if (surface->gc)
	XFreeGC (surface->dpy, surface->gc);

    surface->dpy = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static int
_noop_error_handler (Display     *display,
		     XErrorEvent *event)
{
    return False;		/* return value is ignored */
}

static cairo_bool_t
_CAIRO_MASK_FORMAT (cairo_format_masks_t *masks, cairo_format_t *format)
{
    switch (masks->bpp) {
    case 32:
	if (masks->alpha_mask == 0xff000000 &&
	    masks->red_mask == 0x00ff0000 &&
	    masks->green_mask == 0x0000ff00 &&
	    masks->blue_mask == 0x000000ff)
	{
	    *format = CAIRO_FORMAT_ARGB32;
	    return True;
	}
	if (masks->alpha_mask == 0x00000000 &&
	    masks->red_mask == 0x00ff0000 &&
	    masks->green_mask == 0x0000ff00 &&
	    masks->blue_mask == 0x000000ff)
	{
	    *format = CAIRO_FORMAT_RGB24;
	    return True;
	}
	break;
    case 8:
	if (masks->alpha_mask == 0xff)
	{
	    *format = CAIRO_FORMAT_A8;
	    return True;
	}
	break;
    case 1:
	if (masks->alpha_mask == 0x1)
	{
	    *format = CAIRO_FORMAT_A1;
	    return True;
	}
	break;
    }
    return False;
}

static cairo_status_t
_get_image_surface (cairo_xlib_surface_t   *surface,
		    cairo_rectangle_t      *interest_rect,
		    cairo_image_surface_t **image_out,
		    cairo_rectangle_t      *image_rect)
{
    cairo_image_surface_t *image;
    XImage *ximage;
    int x1, y1, x2, y2;
    cairo_format_masks_t masks;
    cairo_format_t format;

    x1 = 0;
    y1 = 0;
    x2 = surface->width;
    y2 = surface->height;

    if (interest_rect) {
	cairo_rectangle_t rect;
	
	rect.x = interest_rect->x;
	rect.y = interest_rect->y;
	rect.width = interest_rect->width;
	rect.height = interest_rect->width;
    
	if (rect.x > x1)
	    x1 = rect.x;
	if (rect.y > y1)
	    y1 = rect.y;
	if (rect.x + rect.width < x2)
	    x2 = rect.x + rect.width;
	if (rect.y + rect.height < y2)
	    y2 = rect.y + rect.height;

	if (x1 >= x2 || y1 >= y2) {
	    *image_out = NULL;
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    if (image_rect) {
	image_rect->x = x1;
	image_rect->y = y1;
	image_rect->width = x2 - x1;
	image_rect->height = y2 - y1;
    }

    /* XXX: This should try to use the XShm extension if available */

    if (surface->use_pixmap == 0)
    {
	cairo_xlib_error_func_t old_handler;

	old_handler = XSetErrorHandler (_noop_error_handler);
							
	ximage = XGetImage (surface->dpy,
			    surface->drawable,
			    x1, y1,
			    x2 - x1, y2 - y1,
			    AllPlanes, ZPixmap);

	XSetErrorHandler (old_handler);
	
	/* If we get an error, the surface must have been a window,
	 * so retry with the safe code path.
	 */
	if (!ximage)
	    surface->use_pixmap = CAIRO_ASSUME_PIXMAP;
    }
    else
    {
	surface->use_pixmap--;
	ximage = 0;
    }
    
    if (!ximage)
    {

	/* XGetImage from a window is dangerous because it can
	 * produce errors if the window is unmapped or partially
	 * outside the screen. We could check for errors and
	 * retry, but to keep things simple, we just create a
	 * temporary pixmap
	 */
	Pixmap pixmap = XCreatePixmap (surface->dpy,
				       surface->drawable,
				       x2 - x1, y2 - y1,
				       surface->depth);
	_cairo_xlib_surface_ensure_gc (surface);

	XCopyArea (surface->dpy, surface->drawable, pixmap, surface->gc,
		   x1, y1, x2 - x1, y2 - y1, 0, 0);
	
	ximage = XGetImage (surface->dpy,
			    pixmap,
			    0, 0,
			    x2 - x1, y2 - y1,
			    AllPlanes, ZPixmap);
	
	XFreePixmap (surface->dpy, pixmap);
    }
    if (!ximage)
	return CAIRO_STATUS_NO_MEMORY;
					
    /*
     * Compute the pixel format masks from either a visual or a 
     * XRenderFormat, failing we assume the drawable is an
     * alpha-only pixmap as it could only have been created
     * that way through the cairo_xlib_surface_create_for_bitmap
     * function.
     */
    if (surface->visual) {
	masks.bpp = ximage->bits_per_pixel;
	masks.alpha_mask = 0;
	masks.red_mask = surface->visual->red_mask;
	masks.green_mask = surface->visual->green_mask;
	masks.blue_mask = surface->visual->blue_mask;
    } else if (surface->format) {
	masks.bpp = ximage->bits_per_pixel;
	masks.red_mask = surface->format->direct.redMask << surface->format->direct.red;
	masks.green_mask = surface->format->direct.greenMask << surface->format->direct.green;
	masks.blue_mask = surface->format->direct.blueMask << surface->format->direct.blue;
	masks.alpha_mask = surface->format->direct.alphaMask << surface->format->direct.alpha;
    } else {
	masks.bpp = ximage->bits_per_pixel;
	masks.red_mask = 0;
	masks.green_mask = 0;
	masks.blue_mask = 0;
	if (surface->depth < 32)
	    masks.alpha_mask = (1 << surface->depth) - 1;
	else
	    masks.alpha_mask = 0xffffffff;
    }

    /*
     * Prefer to use a standard pixman format instead of the
     * general masks case.
     */
    if (_CAIRO_MASK_FORMAT (&masks, &format))
    {
	image = (cairo_image_surface_t *) cairo_image_surface_create_for_data ((unsigned char *) ximage->data,
									       format,
									       ximage->width, 
									       ximage->height,
									       ximage->bytes_per_line);
    }
    else
    {
	/* 
	 * XXX This can't work.  We must convert the data to one of the 
	 * supported pixman formats.  Pixman needs another function
	 * which takes data in an arbitrary format and converts it
	 * to something supported by that library.
	 */
	image = _cairo_image_surface_create_with_masks ((unsigned char *) ximage->data,
							&masks,
							ximage->width, 
							ximage->height,
							ximage->bytes_per_line);
    }

    /* Let the surface take ownership of the data */
    /* XXX: Can probably come up with a cleaner API here. */
    _cairo_image_surface_assume_ownership_of_data (image);
    ximage->data = NULL;
    XDestroyImage (ximage);
     
    _cairo_image_surface_set_repeat (image, surface->base.repeat);
    _cairo_image_surface_set_matrix (image, &(surface->base.matrix));

    *image_out = image;
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xlib_surface_ensure_gc (cairo_xlib_surface_t *surface)
{
    XGCValues gcv;

    if (surface->gc)
	return;

    gcv.graphics_exposures = False;
    surface->gc = XCreateGC (surface->dpy, surface->drawable,
			     GCGraphicsExposures, &gcv);
}

static cairo_status_t
_draw_image_surface (cairo_xlib_surface_t   *surface,
		     cairo_image_surface_t  *image,
		     int                    dst_x,
		     int                    dst_y)
{
    XImage *ximage;
    unsigned bitmap_pad;

    /* XXX this is wrong */
    if (image->depth > 16)
	bitmap_pad = 32;
    else if (image->depth > 8)
	bitmap_pad = 16;
    else
	bitmap_pad = 8;

    ximage = XCreateImage (surface->dpy,
			   DefaultVisual(surface->dpy, DefaultScreen(surface->dpy)),
			   image->depth,
			   ZPixmap,
			   0,
			   (char *) image->data,
			   image->width,
			   image->height,
			   bitmap_pad,
			   image->stride);
    if (ximage == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    _cairo_xlib_surface_ensure_gc (surface);
    XPutImage(surface->dpy, surface->drawable, surface->gc,
	      ximage, 0, 0, dst_x, dst_y,
	      image->width, image->height);

    /* Foolish XDestroyImage thinks it can free my data, but I won't
       stand for it. */
    ximage->data = NULL;
    XDestroyImage (ximage);

    return CAIRO_STATUS_SUCCESS;

}

static cairo_status_t
_cairo_xlib_surface_acquire_source_image (void                    *abstract_surface,
					  cairo_image_surface_t  **image_out,
					  void                   **image_extra)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    cairo_status_t status;

    status = _get_image_surface (surface, NULL, &image, NULL);
    if (status == CAIRO_STATUS_SUCCESS)
	*image_out = image;

    return status;
}

static void
_cairo_xlib_surface_release_source_image (void                   *abstract_surface,
					  cairo_image_surface_t  *image,
					  void                   *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_xlib_surface_acquire_dest_image (void                    *abstract_surface,
					cairo_rectangle_t       *interest_rect,
					cairo_image_surface_t  **image_out,
					cairo_rectangle_t       *image_rect_out,
					void                   **image_extra)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    cairo_status_t status;

    status = _get_image_surface (surface, interest_rect, &image, image_rect_out);
    if (status == CAIRO_STATUS_SUCCESS)
	*image_out = image;

    return status;
}

static void
_cairo_xlib_surface_release_dest_image (void                   *abstract_surface,
					cairo_rectangle_t      *interest_rect,
					cairo_image_surface_t  *image,
					cairo_rectangle_t      *image_rect,
					void                   *image_extra)
{
    cairo_xlib_surface_t *surface = abstract_surface;

    /* ignore errors */
    _draw_image_surface (surface, image, image_rect->x, image_rect->y);

    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_xlib_surface_clone_similar (void			*abstract_surface,
				   cairo_surface_t	*src,
				   cairo_surface_t     **clone_out)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_xlib_surface_t *clone;

    if (src->backend == surface->base.backend ) {
	cairo_xlib_surface_t *xlib_src = (cairo_xlib_surface_t *)src;

	if (xlib_src->dpy == surface->dpy) {
	    *clone_out = src;
	    cairo_surface_reference (src);
	    
	    return CAIRO_STATUS_SUCCESS;
	}
    } else if (_cairo_surface_is_image (src)) {
	cairo_image_surface_t *image_src = (cairo_image_surface_t *)src;
    
	clone = (cairo_xlib_surface_t *)
	    _cairo_xlib_surface_create_similar (surface, image_src->format, 0,
						image_src->width, image_src->height);
	if (clone == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
	
	_draw_image_surface (clone, image_src, 0, 0);
	
	*clone_out = &clone->base;

	return CAIRO_STATUS_SUCCESS;
    }
    
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_status_t
_cairo_xlib_surface_set_matrix (cairo_xlib_surface_t *surface,
				cairo_matrix_t	     *matrix)
{
    XTransform xtransform;

    if (!surface->picture)
	return CAIRO_STATUS_SUCCESS;
    
    xtransform.matrix[0][0] = _cairo_fixed_from_double (matrix->xx);
    xtransform.matrix[0][1] = _cairo_fixed_from_double (matrix->xy);
    xtransform.matrix[0][2] = _cairo_fixed_from_double (matrix->x0);

    xtransform.matrix[1][0] = _cairo_fixed_from_double (matrix->yx);
    xtransform.matrix[1][1] = _cairo_fixed_from_double (matrix->yy);
    xtransform.matrix[1][2] = _cairo_fixed_from_double (matrix->y0);

    xtransform.matrix[2][0] = 0;
    xtransform.matrix[2][1] = 0;
    xtransform.matrix[2][2] = _cairo_fixed_from_double (1);

    if (!CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM (surface))
    {
	static const XTransform identity = { {
	    { 1 << 16, 0x00000, 0x00000 },
	    { 0x00000, 1 << 16, 0x00000 },
	    { 0x00000, 0x00000, 1 << 16 },
	} };

	if (memcmp (&xtransform, &identity, sizeof (XTransform)) == 0)
	    return CAIRO_STATUS_SUCCESS;
	
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    XRenderSetPictureTransform (surface->dpy, surface->picture, &xtransform);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_filter (cairo_xlib_surface_t *surface,
				cairo_filter_t	     filter)
{
    char *render_filter;

    if (!surface->picture)
	return CAIRO_STATUS_SUCCESS;

    if (!CAIRO_SURFACE_RENDER_HAS_FILTERS (surface))
    {
	if (filter == CAIRO_FILTER_FAST || filter == CAIRO_FILTER_NEAREST)
	    return CAIRO_STATUS_SUCCESS;
	
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    
    switch (filter) {
    case CAIRO_FILTER_FAST:
	render_filter = FilterFast;
	break;
    case CAIRO_FILTER_GOOD:
	render_filter = FilterGood;
	break;
    case CAIRO_FILTER_BEST:
	render_filter = FilterBest;
	break;
    case CAIRO_FILTER_NEAREST:
	render_filter = FilterNearest;
	break;
    case CAIRO_FILTER_BILINEAR:
	render_filter = FilterBilinear;
	break;
    default:
	render_filter = FilterBest;
	break;
    }

    XRenderSetPictureFilter (surface->dpy, surface->picture,
			     render_filter, NULL, 0);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_repeat (cairo_xlib_surface_t *surface, int repeat)
{
    XRenderPictureAttributes pa;
    unsigned long	     mask;

    if (!surface->picture)
	return CAIRO_STATUS_SUCCESS;
    
    mask = CPRepeat;
    pa.repeat = repeat;

    XRenderChangePicture (surface->dpy, surface->picture, mask, &pa);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_set_attributes (cairo_xlib_surface_t	  *surface,
				       cairo_surface_attributes_t *attributes)
{
    cairo_int_status_t status;

    status = _cairo_xlib_surface_set_matrix (surface, &attributes->matrix);
    if (status)
	return status;
    
    switch (attributes->extend) {
    case CAIRO_EXTEND_NONE:
	_cairo_xlib_surface_set_repeat (surface, 0);
	break;
    case CAIRO_EXTEND_REPEAT:
	_cairo_xlib_surface_set_repeat (surface, 1);
	break;
    case CAIRO_EXTEND_REFLECT:
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    status = _cairo_xlib_surface_set_filter (surface, attributes->filter);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static int
_render_operator (cairo_operator_t operator)
{
    switch (operator) {
    case CAIRO_OPERATOR_CLEAR:
	return PictOpClear;

    case CAIRO_OPERATOR_SOURCE:
	return PictOpSrc;
    case CAIRO_OPERATOR_OVER:
	return PictOpOver;
    case CAIRO_OPERATOR_IN:
	return PictOpIn;
    case CAIRO_OPERATOR_OUT:
	return PictOpOut;
    case CAIRO_OPERATOR_ATOP:
	return PictOpAtop;

    case CAIRO_OPERATOR_DEST:
	return PictOpDst;
    case CAIRO_OPERATOR_DEST_OVER:
	return PictOpOverReverse;
    case CAIRO_OPERATOR_DEST_IN:
	return PictOpInReverse;
    case CAIRO_OPERATOR_DEST_OUT:
	return PictOpOutReverse;
    case CAIRO_OPERATOR_DEST_ATOP:
	return PictOpAtopReverse;

    case CAIRO_OPERATOR_XOR:
	return PictOpXor;
    case CAIRO_OPERATOR_ADD:
	return PictOpAdd;
    case CAIRO_OPERATOR_SATURATE:
	return PictOpSaturate;
    default:
	return PictOpOver;
    }
}

static cairo_int_status_t
_cairo_xlib_surface_composite (cairo_operator_t		operator,
			       cairo_pattern_t		*src_pattern,
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
    cairo_surface_attributes_t	src_attr, mask_attr;
    cairo_xlib_surface_t	*dst = abstract_dst;
    cairo_xlib_surface_t	*src;
    cairo_xlib_surface_t	*mask;
    cairo_int_status_t		status;

    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE (dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_pattern_acquire_surfaces (src_pattern, mask_pattern,
					      &dst->base,
					      src_x, src_y,
					      mask_x, mask_y,
					      width, height,
					      (cairo_surface_t **) &src,
					      (cairo_surface_t **) &mask,
					      &src_attr, &mask_attr);
    if (status)
	return status;
    
    status = _cairo_xlib_surface_set_attributes (src, &src_attr);
    if (CAIRO_OK (status)) {
	if (mask) {
	    status = _cairo_xlib_surface_set_attributes (mask, &mask_attr);
	    if (CAIRO_OK (status))
		XRenderComposite (dst->dpy,
				  _render_operator (operator),
				  src->picture,
				  mask->picture,
				  dst->picture,
				  src_x + src_attr.x_offset,
				  src_y + src_attr.y_offset,
				  mask_x + mask_attr.x_offset,
				  mask_y + mask_attr.y_offset,
				  dst_x, dst_y,
				  width, height);
	} else {
	    XRenderComposite (dst->dpy,
			      _render_operator (operator),
			      src->picture,
			      0,
			      dst->picture,
			      src_x + src_attr.x_offset,
			      src_y + src_attr.y_offset,
			      0, 0,
			      dst_x, dst_y,
			      width, height);
	}
    }

    if (mask)
	_cairo_pattern_release_surface (&dst->base, &mask->base, &mask_attr);
    
    _cairo_pattern_release_surface (&dst->base, &src->base, &src_attr);

    return status;
}

static cairo_int_status_t
_cairo_xlib_surface_fill_rectangles (void			*abstract_surface,
				     cairo_operator_t		operator,
				     const cairo_color_t	*color,
				     cairo_rectangle_t		*rects,
				     int			num_rects)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    XRenderColor render_color;

    if (!CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLE (surface))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    render_color.red   = color->red_short;
    render_color.green = color->green_short;
    render_color.blue  = color->blue_short;
    render_color.alpha = color->alpha_short;

    /* XXX: This XRectangle cast is evil... it needs to go away somehow. */
    XRenderFillRectangles (surface->dpy,
			   _render_operator (operator),
			   surface->picture,
			   &render_color, (XRectangle *) rects, num_rects);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_composite_trapezoids (cairo_operator_t	operator,
					  cairo_pattern_t	*pattern,
					  void			*abstract_dst,
					  int			src_x,
					  int			src_y,
					  int			dst_x,
					  int			dst_y,
					  unsigned int		width,
					  unsigned int		height,
					  cairo_trapezoid_t	*traps,
					  int			num_traps)
{
    cairo_surface_attributes_t	attributes;
    cairo_xlib_surface_t	*dst = abstract_dst;
    cairo_xlib_surface_t	*src;
    cairo_int_status_t		status;
    int				render_reference_x, render_reference_y;
    int				render_src_x, render_src_y;

    if (!CAIRO_SURFACE_RENDER_HAS_TRAPEZOIDS (dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;
    
    status = _cairo_pattern_acquire_surface (pattern, &dst->base,
					     src_x, src_y, width, height,
					     (cairo_surface_t **) &src,
					     &attributes);
    if (status)
	return status;
    
    if (traps[0].left.p1.y < traps[0].left.p2.y) {
	render_reference_x = _cairo_fixed_integer_floor (traps[0].left.p1.x);
	render_reference_y = _cairo_fixed_integer_floor (traps[0].left.p1.y);
    } else {
	render_reference_x = _cairo_fixed_integer_floor (traps[0].left.p2.x);
	render_reference_y = _cairo_fixed_integer_floor (traps[0].left.p2.y);
    }

    render_src_x = src_x + render_reference_x - dst_x;
    render_src_y = src_y + render_reference_y - dst_y;

    /* XXX: The XTrapezoid cast is evil and needs to go away somehow. */
    status = _cairo_xlib_surface_set_attributes (src, &attributes);
    if (CAIRO_OK (status))
	XRenderCompositeTrapezoids (dst->dpy,
				    _render_operator (operator),
				    src->picture, dst->picture,
				    XRenderFindStandardFormat (dst->dpy, PictStandardA8),
				    render_src_x + attributes.x_offset, 
				    render_src_y + attributes.y_offset,
				    (XTrapezoid *) traps, num_traps);

    _cairo_pattern_release_surface (&dst->base, &src->base, &attributes);

    return status;
}

static cairo_int_status_t
_cairo_xlib_surface_set_clip_region (void              *abstract_surface,
				     pixman_region16_t *region)
{
    cairo_xlib_surface_t *surface = (cairo_xlib_surface_t *) abstract_surface;

    if (region == NULL) {
	if (surface->gc)
	    XSetClipMask (surface->dpy, surface->gc, None);
	
	if (surface->picture) {
	    XRenderPictureAttributes pa;
	    pa.clip_mask = None;
	    XRenderChangePicture (surface->dpy, surface->picture,
				  CPClipMask, &pa);
	}
    } else {
	pixman_box16_t *boxes;
	XRectangle *rects = NULL;
	int n_boxes, i;
	
	n_boxes = pixman_region_num_rects (region);
	if (n_boxes > 0) {
	    rects = malloc (sizeof(XRectangle) * n_boxes);
	    if (rects == NULL)
		return CAIRO_STATUS_NO_MEMORY;
	} else {
	    rects = NULL;
	}

	boxes = pixman_region_rects (region);
	
	for (i = 0; i < n_boxes; i++) {
	    rects[i].x = boxes[i].x1;
	    rects[i].y = boxes[i].y1;
	    rects[i].width = boxes[i].x2 - boxes[i].x1;
	    rects[i].height = boxes[i].y2 - boxes[i].y1;
	}

	if (surface->gc)
	    XSetClipRectangles(surface->dpy, surface->gc,
			       0, 0, rects, n_boxes, YXSorted);
	if (surface->picture)
	    XRenderSetPictureClipRectangles (surface->dpy, surface->picture,
					     0, 0, rects, n_boxes);

	if (rects)
	    free (rects);
    }
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_get_extents (void		   *abstract_surface,
				 cairo_rectangle_t *rectangle)
{
    cairo_xlib_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;

    rectangle->width  = surface->width;
    rectangle->height = surface->height;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_show_glyphs (cairo_scaled_font_t    *scaled_font,
				 cairo_operator_t       operator,
				 cairo_pattern_t	*pattern,
				 void			*abstract_surface,
				 int                    source_x,
				 int                    source_y,
				 int			dest_x,
				 int			dest_y,
				 unsigned int		width,
				 unsigned int		height,
				 const cairo_glyph_t    *glyphs,
				 int                    num_glyphs);

static const cairo_surface_backend_t cairo_xlib_surface_backend = {
    _cairo_xlib_surface_create_similar,
    _cairo_xlib_surface_finish,
    _cairo_xlib_surface_acquire_source_image,
    _cairo_xlib_surface_release_source_image,
    _cairo_xlib_surface_acquire_dest_image,
    _cairo_xlib_surface_release_dest_image,
    _cairo_xlib_surface_clone_similar,
    _cairo_xlib_surface_composite,
    _cairo_xlib_surface_fill_rectangles,
    _cairo_xlib_surface_composite_trapezoids,
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_xlib_surface_set_clip_region,
    _cairo_xlib_surface_get_extents,
    _cairo_xlib_surface_show_glyphs
};

/**
 * _cairo_surface_is_xlib:
 * @surface: a #cairo_surface_t
 * 
 * Checks if a surface is a #cairo_xlib_surface_t
 * 
 * Return value: True if the surface is an xlib surface
 **/
static cairo_bool_t
_cairo_surface_is_xlib (cairo_surface_t *surface)
{
    return surface->backend == &cairo_xlib_surface_backend;
}

static cairo_surface_t *
_cairo_xlib_surface_create_internal (Display		       *dpy,
				     Drawable		        drawable,
				     Visual		       *visual,
				     XRenderPictFormat	       *format,
				     int			width,
				     int			height,
				     int			depth)
{
    cairo_xlib_surface_t *surface;

    surface = malloc (sizeof (cairo_xlib_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (&surface->base, &cairo_xlib_surface_backend);

    surface->dpy = dpy;

    surface->gc = NULL;
    surface->drawable = drawable;
    surface->owns_pixmap = FALSE;
    surface->visual = visual;
    surface->format = format;
    surface->use_pixmap = 0;
    surface->width = width;
    surface->height = height;
    surface->depth = depth;
    
    if (format) {
	surface->depth = format->depth;
    } else if (visual) {
	int i, j, k;

	/* This is ugly, but we have to walk over all visuals
	 * for the display to find the depth.
	 */
	for (i = 0; i < ScreenCount (dpy); i++) {
	    Screen *screen = ScreenOfDisplay (dpy, i);
	    for (j = 0; j < screen->ndepths; j++) {
		Depth *depth = &screen->depths[j];
		for (k = 0; k < depth->nvisuals; k++) {
		    if (&depth->visuals[k] == visual) {
			surface->depth = depth->depth;
			goto found;
		    }
		}
	    }
	}
    found:
	;
    }

    if (cairo_xlib_render_disabled ||
	! XRenderQueryVersion (dpy, &surface->render_major, &surface->render_minor)) {
	surface->render_major = -1;
	surface->render_minor = -1;
    }

    surface->picture = None;

    if (CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE (surface)) {

	if (!format) {
	    if (visual) {
		format = XRenderFindVisualFormat (dpy, visual);
	    } else if (depth == 1)
		format = XRenderFindStandardFormat (dpy, PictStandardA1);
	}

	if (format)
	    surface->picture = XRenderCreatePicture (dpy, drawable,
						     format, 0, NULL);
    }

    return (cairo_surface_t *) surface;
}

/**
 * cairo_xlib_surface_create:
 * @dpy: an X Display
 * @drawable: an X Drawable, (a Pixmap or a Window)
 * @visual: the visual to use for drawing to @drawable. The depth
 *          of the visual must match the depth of the drawable.
 *          Currently, only TrueColor visuals are fully supported.
 * @width: the current width of @drawable.
 * @height: the current height of @drawable.
 *
 * Creates an Xlib surface that draws to the given drawable.
 * The way that colors are represented in the drawable is specified
 * by the provided visual.
 *
 * NOTE: If @drawable is a Window, then the function
 * cairo_xlib_surface_set_size must be called whenever the size of the
 * window changes.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_xlib_surface_create (Display     *dpy,
			   Drawable	drawable,
			   Visual      *visual,
			   int		width,
			   int		height)
{
    return _cairo_xlib_surface_create_internal (dpy, drawable,
						visual, NULL, width, height, 0);
}

/**
 * cairo_xlib_surface_create_for_bitmap:
 * @dpy: an X Display
 * @bitmap: an X Drawable, (a depth-1 Pixmap)
 * @width: the current width of @bitmap.
 * @height: the current height of @bitmap.
 * 
 * Creates an Xlib surface that draws to the given bitmap.
 * This will be drawn to as a CAIRO_FORMAT_A1 object.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_xlib_surface_create_for_bitmap (Display  *dpy,
				      Pixmap	bitmap,
				      int	width,
				      int	height)
{
    return _cairo_xlib_surface_create_internal (dpy, bitmap,
						NULL, NULL, width, height, 1);
}

/**
 * cairo_xlib_surface_create_with_xrender_format:
 * @dpy: an X Display
 * @drawable: an X Drawable, (a Pixmap or a Window)
 * @format: the picture format to use for drawing to @drawable. The depth
 *          of @format must match the depth of the drawable.
 * @width: the current width of @drawable.
 * @height: the current height of @drawable.
 *
 * Creates an Xlib surface that draws to the given drawable.
 * The way that colors are represented in the drawable is specified
 * by the provided picture format.
 *
 * NOTE: If @drawable is a Window, then the function
 * cairo_xlib_surface_set_size must be called whenever the size of the
 * window changes.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_xlib_surface_create_with_xrender_format (Display		    *dpy,
					       Drawable		    drawable,
					       XRenderPictFormat    *format,
					       int		    width,
					       int		    height)
{
    return _cairo_xlib_surface_create_internal (dpy, drawable,
						NULL, format, width, height, 0);
}

/**
 * cairo_xlib_surface_set_size:
 * @surface: a #cairo_surface_t for the XLib backend
 * @width: the new width of the surface
 * @height: the new height of the surface
 * 
 * Informs cairo of the new size of the X Drawable underlying the
 * surface. For a surface created for a Window (rather than a Pixmap),
 * this function must be called each time the size of the window
 * changes. (For a subwindow, you are normally resizing the window
 * yourself, but for a toplevel window, it is necessary to listen for
 * ConfigureNotify events.)
 *
 * A Pixmap can never change size, so it is never necessary to call
 * this function on a surface created for a Pixmap.
 **/
void
cairo_xlib_surface_set_size (cairo_surface_t *surface,
			     int              width,
			     int              height)
{
    cairo_xlib_surface_t *xlib_surface = (cairo_xlib_surface_t *)surface;

    /* XXX: How do we want to handle this error case? */
    if (! _cairo_surface_is_xlib (surface))
	return;

    xlib_surface->width = width;
    xlib_surface->height = height;
}

/* RENDER glyphset cache code */

typedef struct glyphset_cache {
    cairo_cache_t base;
    struct glyphset_cache *next;
    Display *display;
    XRenderPictFormat *a8_pict_format;
    GlyphSet glyphset;
    Glyph counter;
} glyphset_cache_t;

typedef struct {
    cairo_glyph_cache_key_t key;
    Glyph glyph;
    XGlyphInfo info;
    int refcount;
} glyphset_cache_entry_t;

static Glyph
_next_xlib_glyph (glyphset_cache_t *cache)
{
    return ++(cache->counter);
}


static cairo_status_t 
_xlib_glyphset_cache_create_entry (void *cache,
				   void *key,
				   void **return_entry)
{
    glyphset_cache_t *g = (glyphset_cache_t *) cache;
    cairo_glyph_cache_key_t *k = (cairo_glyph_cache_key_t *)key;
    glyphset_cache_entry_t *v;

    cairo_status_t status;

    cairo_cache_t *im_cache;
    cairo_image_glyph_cache_entry_t *im;

    v = malloc (sizeof (glyphset_cache_entry_t));
    _cairo_lock_global_image_glyph_cache ();
    im_cache = _cairo_get_global_image_glyph_cache ();

    if (g == NULL || v == NULL || im_cache == NULL) {
	_cairo_unlock_global_image_glyph_cache ();
	return CAIRO_STATUS_NO_MEMORY;
    }

    status = _cairo_cache_lookup (im_cache, key, (void **) (&im), NULL);
    if (status != CAIRO_STATUS_SUCCESS || im == NULL) {
	_cairo_unlock_global_image_glyph_cache ();
	return CAIRO_STATUS_NO_MEMORY;
    }

    v->refcount = 1;
    v->key = *k;
    _cairo_unscaled_font_reference (v->key.unscaled);

    v->glyph = _next_xlib_glyph (g);

    v->info.width = im->image ? im->image->stride : im->size.width;
    v->info.height = im->size.height;

    /*
     *  Most of the font rendering system thinks of glyph tiles as having
     *  an origin at (0,0) and an x and y bounding box "offset" which
     *  extends possibly off into negative coordinates, like so:
     *
     *     
     *       (x,y) <-- probably negative numbers
     *         +----------------+
     *         |      .         |
     *         |      .         |
     *         |......(0,0)     |
     *         |                |
     *         |                |
     *         +----------------+
     *                  (width+x,height+y)
     *
     *  This is a postscript-y model, where each glyph has its own
     *  coordinate space, so it's what we expose in terms of metrics. It's
     *  apparantly what everyone's expecting. Everyone except the Render
     *  extension. Render wants to see a glyph tile starting at (0,0), with
     *  an origin offset inside, like this:
     *
     *       (0,0)
     *         +---------------+
     *         |      .        |
     *         |      .        |
     *         |......(x,y)    |
     *         |               |
     *         |               |
     *         +---------------+
     *                   (width,height)
     *
     *  Luckily, this is just the negation of the numbers we already have
     *  sitting around for x and y. 
     */

    v->info.x = -im->size.x;
    v->info.y = -im->size.y;
    v->info.xOff = 0;
    v->info.yOff = 0;

    XRenderAddGlyphs (g->display, g->glyphset,
		      &(v->glyph), &(v->info), 1,
		      im->image ? (char *) im->image->data : NULL,
		      im->image ? v->info.height * v->info.width : 0);

    v->key.base.memory = im->image ? im->image->width * im->image->stride : 0;
    *return_entry = v;
    _cairo_unlock_global_image_glyph_cache ();
    return CAIRO_STATUS_SUCCESS;
}

static void
_glyphset_cache_entry_reference (glyphset_cache_entry_t *e)
{
    e->refcount++;
}

static void 
_xlib_glyphset_cache_destroy_cache (void *cache)
{
    /* FIXME: will we ever release glyphset caches? */
}

static void 
_xlib_glyphset_cache_destroy_entry (void *cache, void *entry)
{
    glyphset_cache_t *g;
    glyphset_cache_entry_t *v;

    g = (glyphset_cache_t *) cache;
    v = (glyphset_cache_entry_t *) entry;

    if (--v->refcount > 0)
	return;

    _cairo_unscaled_font_destroy (v->key.unscaled);
    XRenderFreeGlyphs (g->display, g->glyphset, &(v->glyph), 1);
    free (v);	
}

static const cairo_cache_backend_t _xlib_glyphset_cache_backend = {
    _cairo_glyph_cache_hash,
    _cairo_glyph_cache_keys_equal,
    _xlib_glyphset_cache_create_entry,
    _xlib_glyphset_cache_destroy_entry,
    _xlib_glyphset_cache_destroy_cache
};


static glyphset_cache_t *
_xlib_glyphset_caches = NULL;

static void
_lock_xlib_glyphset_caches (void)
{
    /* FIXME: implement locking */
}

static void
_unlock_xlib_glyphset_caches (void)
{
    /* FIXME: implement locking */
}

static glyphset_cache_t *
_get_glyphset_cache (Display *d)
{
    /* 
     * There should usually only be one, or a very small number, of
     * displays. So we just do a linear scan. 
     */

    glyphset_cache_t *g;

    for (g = _xlib_glyphset_caches; g != NULL; g = g->next) {
	if (g->display == d)
	    return g;
    }

    g = malloc (sizeof (glyphset_cache_t));
    if (g == NULL) 
	goto ERR;

    g->counter = 0;
    g->display = d;
    g->a8_pict_format = XRenderFindStandardFormat (d, PictStandardA8);
    if (g->a8_pict_format == NULL)
	goto ERR;
    
    if (_cairo_cache_init (&g->base,
			   &_xlib_glyphset_cache_backend,
			   CAIRO_XLIB_GLYPH_CACHE_MEMORY_DEFAULT))
	goto FREE_GLYPHSET_CACHE;
    
    g->glyphset = XRenderCreateGlyphSet (d, g->a8_pict_format);
    g->next = _xlib_glyphset_caches;
    _xlib_glyphset_caches = g;
    return g;
    
 FREE_GLYPHSET_CACHE:
    free (g);
    
 ERR:
    return NULL;
}

#define N_STACK_BUF 1024

static cairo_status_t
_cairo_xlib_surface_show_glyphs32 (cairo_scaled_font_t    *scaled_font,
				   cairo_operator_t       operator,
				   glyphset_cache_t 	  *g,
				   cairo_glyph_cache_key_t *key,
				   cairo_xlib_surface_t   *src,
				   cairo_xlib_surface_t   *self,
				   int                    source_x,
				   int                    source_y,
				   const cairo_glyph_t    *glyphs,
				   glyphset_cache_entry_t **entries,
				   int                    num_glyphs)
{
    XGlyphElt32 *elts = NULL;
    XGlyphElt32 stack_elts [N_STACK_BUF];

    unsigned int *chars = NULL;
    unsigned int stack_chars [N_STACK_BUF];

    int i;    
    int thisX, thisY;
    int lastX = 0, lastY = 0;

    /* Acquire arrays of suitable sizes. */
    if (num_glyphs < N_STACK_BUF) {
	elts = stack_elts;
	chars = stack_chars;

    } else {
	elts = malloc (num_glyphs * sizeof (XGlyphElt32));
	if (elts == NULL)
	    goto FAIL;

	chars = malloc (num_glyphs * sizeof (unsigned int));
	if (chars == NULL)
	    goto FREE_ELTS;

    }

    for (i = 0; i < num_glyphs; ++i) {
	chars[i] = entries[i]->glyph;
	elts[i].chars = &(chars[i]);
	elts[i].nchars = 1;
	elts[i].glyphset = g->glyphset;
	thisX = (int) floor (glyphs[i].x + 0.5);
	thisY = (int) floor (glyphs[i].y + 0.5);
	elts[i].xOff = thisX - lastX;
	elts[i].yOff = thisY - lastY;
	lastX = thisX;
	lastY = thisY;
    }

    XRenderCompositeText32 (self->dpy,
			    _render_operator (operator),
			    src->picture,
			    self->picture,
			    g->a8_pict_format,
			    source_x, source_y,
			    0, 0,
			    elts, num_glyphs);

    if (num_glyphs >= N_STACK_BUF) {
	free (chars);
	free (elts); 
    }
    
    return CAIRO_STATUS_SUCCESS;
    
 FREE_ELTS:
    if (num_glyphs >= N_STACK_BUF)
	free (elts); 

 FAIL:
    return CAIRO_STATUS_NO_MEMORY;
}


static cairo_status_t
_cairo_xlib_surface_show_glyphs16 (cairo_scaled_font_t    *scaled_font,
				   cairo_operator_t       operator,
				   glyphset_cache_t 	  *g,
				   cairo_glyph_cache_key_t *key,
				   cairo_xlib_surface_t   *src,
				   cairo_xlib_surface_t   *self,
				   int                    source_x,
				   int                    source_y,
				   const cairo_glyph_t    *glyphs,
				   glyphset_cache_entry_t **entries,
				   int                    num_glyphs)
{
    XGlyphElt16 *elts = NULL;
    XGlyphElt16 stack_elts [N_STACK_BUF];

    unsigned short *chars = NULL;
    unsigned short stack_chars [N_STACK_BUF];

    int i;
    int thisX, thisY;
    int lastX = 0, lastY = 0;

    /* Acquire arrays of suitable sizes. */
    if (num_glyphs < N_STACK_BUF) {
	elts = stack_elts;
	chars = stack_chars;

    } else {
	elts = malloc (num_glyphs * sizeof (XGlyphElt16));
	if (elts == NULL)
	    goto FAIL;

	chars = malloc (num_glyphs * sizeof (unsigned short));
	if (chars == NULL)
	    goto FREE_ELTS;

    }

    for (i = 0; i < num_glyphs; ++i) {
	chars[i] = entries[i]->glyph;
	elts[i].chars = &(chars[i]);
	elts[i].nchars = 1;
	elts[i].glyphset = g->glyphset;
	thisX = (int) floor (glyphs[i].x + 0.5);
	thisY = (int) floor (glyphs[i].y + 0.5);
	elts[i].xOff = thisX - lastX;
	elts[i].yOff = thisY - lastY;
	lastX = thisX;
	lastY = thisY;
    }

    XRenderCompositeText16 (self->dpy,
			    _render_operator (operator),
			    src->picture,
			    self->picture,
			    g->a8_pict_format,
			    source_x, source_y,
			    0, 0,
			    elts, num_glyphs);

    if (num_glyphs >= N_STACK_BUF) {
	free (chars);
	free (elts); 
    }
    
    return CAIRO_STATUS_SUCCESS;

 FREE_ELTS:
    if (num_glyphs >= N_STACK_BUF)
	free (elts); 

 FAIL:
    return CAIRO_STATUS_NO_MEMORY;
}

static cairo_status_t
_cairo_xlib_surface_show_glyphs8 (cairo_scaled_font_t    *scaled_font,
				  cairo_operator_t       operator,
				  glyphset_cache_t 	 *g,
				  cairo_glyph_cache_key_t *key,
				  cairo_xlib_surface_t   *src,
				  cairo_xlib_surface_t   *self,
				  int                    source_x,
				  int                    source_y,
				  const cairo_glyph_t    *glyphs,
				  glyphset_cache_entry_t **entries,
				  int                    num_glyphs)
{
    XGlyphElt8 *elts = NULL;
    XGlyphElt8 stack_elts [N_STACK_BUF];

    char *chars = NULL;
    char stack_chars [N_STACK_BUF];

    int i;
    int thisX, thisY;
    int lastX = 0, lastY = 0;

    /* Acquire arrays of suitable sizes. */
    if (num_glyphs < N_STACK_BUF) {
	elts = stack_elts;
	chars = stack_chars;

    } else {
	elts = malloc (num_glyphs * sizeof (XGlyphElt8));
	if (elts == NULL)
	    goto FAIL;

	chars = malloc (num_glyphs * sizeof (char));
	if (chars == NULL)
	    goto FREE_ELTS;

    }

    for (i = 0; i < num_glyphs; ++i) {
	chars[i] = entries[i]->glyph;
	elts[i].chars = &(chars[i]);
	elts[i].nchars = 1;
	elts[i].glyphset = g->glyphset;
	thisX = (int) floor (glyphs[i].x + 0.5);
	thisY = (int) floor (glyphs[i].y + 0.5);
	elts[i].xOff = thisX - lastX;
	elts[i].yOff = thisY - lastY;
	lastX = thisX;
	lastY = thisY;
    }

    XRenderCompositeText8 (self->dpy,
			   _render_operator (operator),
			   src->picture,
			   self->picture,
			   g->a8_pict_format,
			   source_x, source_y,
			   0, 0,
			   elts, num_glyphs);
    
    if (num_glyphs >= N_STACK_BUF) {
	free (chars);
	free (elts); 
    }
    
    return CAIRO_STATUS_SUCCESS;
    
 FREE_ELTS:
    if (num_glyphs >= N_STACK_BUF)
	free (elts); 

 FAIL:
    return CAIRO_STATUS_NO_MEMORY;
}


static cairo_int_status_t
_cairo_xlib_surface_show_glyphs (cairo_scaled_font_t    *scaled_font,
				 cairo_operator_t       operator,
				 cairo_pattern_t        *pattern,
				 void		        *abstract_surface,
				 int                    source_x,
				 int                    source_y,
				 int			dest_x,
				 int			dest_y,
				 unsigned int		width,
				 unsigned int		height,
				 const cairo_glyph_t    *glyphs,
				 int                    num_glyphs)
{
    cairo_surface_attributes_t	attributes;
    cairo_int_status_t		status;
    unsigned int elt_size;
    cairo_xlib_surface_t *self = abstract_surface;
    cairo_xlib_surface_t *src;
    glyphset_cache_t *g;
    cairo_glyph_cache_key_t key;
    glyphset_cache_entry_t **entries;
    glyphset_cache_entry_t *stack_entries [N_STACK_BUF];
    int i;

    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE_TEXT (self))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_pattern_acquire_surface (pattern, &self->base,
					     source_x, source_y, width, height,
					     (cairo_surface_t **) &src,
					     &attributes);
    if (status)
	return status;

    status = _cairo_xlib_surface_set_attributes (src, &attributes);
    if (status)
	goto FAIL;
    
    /* Acquire an entry array of suitable size. */
    if (num_glyphs < N_STACK_BUF) {
	entries = stack_entries;

    } else {
	entries = malloc (num_glyphs * sizeof (glyphset_cache_entry_t *));
	if (entries == NULL)
	    goto FAIL;
    }

    _lock_xlib_glyphset_caches ();
    g = _get_glyphset_cache (self->dpy);
    if (g == NULL)
	goto UNLOCK;

    /* Work out the index size to use. */
    elt_size = 8;
    _cairo_scaled_font_get_glyph_cache_key (scaled_font, &key);

    for (i = 0; i < num_glyphs; ++i) {
	key.index = glyphs[i].index;
	status = _cairo_cache_lookup (&g->base, &key, (void **) (&entries[i]), NULL);
	if (status != CAIRO_STATUS_SUCCESS || entries[i] == NULL) 
	    goto UNLOCK;

	/* Referencing the glyph entries we use prevents them from
	 * being freed if lookup of later entries causes them to
	 * be ejected from the cache. It would be more efficient
	 * (though more complex) to prevent them from being ejected
	 * from the cache at all, so they could get reused later
	 * in the same string.
	 */
	_glyphset_cache_entry_reference (entries[i]);

	if (elt_size == 8 && entries[i]->glyph > 0xff)
	    elt_size = 16;
	if (elt_size == 16 && entries[i]->glyph > 0xffff) {
	    elt_size = 32;
	    break;
	}
    }

    /* Call the appropriate sub-function. */

    if (elt_size == 8)
	status = _cairo_xlib_surface_show_glyphs8 (scaled_font, operator, g, &key, src, self,
						   source_x + attributes.x_offset,
						   source_y + attributes.y_offset, 
						   glyphs, entries, num_glyphs);
    else if (elt_size == 16)
	status = _cairo_xlib_surface_show_glyphs16 (scaled_font, operator, g, &key, src, self,
						    source_x + attributes.x_offset,
						    source_y + attributes.y_offset, 
						    glyphs, entries, num_glyphs);
    else 
	status = _cairo_xlib_surface_show_glyphs32 (scaled_font, operator, g, &key, src, self,
						    source_x + attributes.x_offset,
						    source_y + attributes.y_offset, 
						    glyphs, entries, num_glyphs);

    for (i = 0; i < num_glyphs; ++i)
	_xlib_glyphset_cache_destroy_entry (g, entries[i]);

 UNLOCK:
    _unlock_xlib_glyphset_caches ();

    if (num_glyphs >= N_STACK_BUF)
	free (entries); 

 FAIL:
    _cairo_pattern_release_surface (&self->base, &src->base, &attributes);
    
    return status;
}

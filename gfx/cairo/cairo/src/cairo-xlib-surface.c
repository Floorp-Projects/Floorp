/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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
#include "cairo-xlib-private.h"
#include <X11/extensions/Xrender.h>

/* Xlib doesn't define a typedef, so define one ourselves */
typedef int (*cairo_xlib_error_func_t) (Display     *display,
					XErrorEvent *event);

typedef struct _cairo_xlib_surface cairo_xlib_surface_t;

static void
_cairo_xlib_surface_ensure_gc (cairo_xlib_surface_t *surface);

static void 
_cairo_xlib_surface_ensure_src_picture (cairo_xlib_surface_t *surface);

static void 
_cairo_xlib_surface_ensure_dst_picture (cairo_xlib_surface_t *surface);

static cairo_bool_t
_cairo_surface_is_xlib (cairo_surface_t *surface);

/*
 * Instead of taking two round trips for each blending request,
 * assume that if a particular drawable fails GetImage that it will
 * fail for a "while"; use temporary pixmaps to avoid the errors
 */

#define CAIRO_ASSUME_PIXMAP	20

struct _cairo_xlib_surface {
    cairo_surface_t base;

    Display *dpy;
    cairo_xlib_screen_info_t *screen_info;
  
    GC gc;
    Drawable drawable;
    Screen *screen;
    cairo_bool_t owns_pixmap;
    Visual *visual;
    
    int use_pixmap;

    int render_major;
    int render_minor;

    /* TRUE if the server has a bug with repeating pictures
     *
     *  https://bugs.freedesktop.org/show_bug.cgi?id=3566
     *
     * We can't test for this because it depends on whether the
     * picture is in video memory or not.
     *
     * We also use this variable as a guard against a second
     * independent bug with transformed repeating pictures:
     *
     * http://lists.freedesktop.org/archives/cairo/2004-September/001839.html
     *
     * Both are fixed in xorg >= 6.9 and hopefully in > 6.8.2, so
     * we can reuse the test for now.
     */ 
    cairo_bool_t buggy_repeat;

    int width;
    int height;
    int depth;

    Picture dst_picture, src_picture;

    cairo_bool_t have_clip_rects;
    XRectangle *clip_rects;
    int num_clip_rects;

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
_cairo_xlib_surface_create_similar (void	       *abstract_src,
				    cairo_content_t	content,
				    int			width,
				    int			height)
{
    cairo_xlib_surface_t *src = abstract_src;
    Display *dpy = src->dpy;
    Pixmap pix;
    cairo_xlib_surface_t *surface;
    cairo_format_t format = _cairo_format_from_content (content);
    int depth = _CAIRO_FORMAT_DEPTH (format);
    XRenderPictFormat *xrender_format = _CAIRO_FORMAT_XRENDER_FORMAT (dpy, 
								      format);

    /* As a good first approximation, if the display doesn't have COMPOSITE,
     * we're better off using image surfaces for all temporary operations
     */
    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE(src)) {
	return cairo_image_surface_create (format, width, height);
    }
    
    pix = XCreatePixmap (dpy, RootWindowOfScreen (src->screen),
			 width <= 0 ? 1 : width, height <= 0 ? 1 : height,
			 depth);
    
    surface = (cairo_xlib_surface_t *)
	cairo_xlib_surface_create_with_xrender_format (dpy, pix, src->screen,
						       xrender_format,
						       width, height);
    if (surface->base.status) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }
				 
    surface->owns_pixmap = TRUE;

    return &surface->base;
}

static cairo_status_t
_cairo_xlib_surface_finish (void *abstract_surface)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    if (surface->dst_picture)
	XRenderFreePicture (surface->dpy, surface->dst_picture);
    
    if (surface->src_picture)
	XRenderFreePicture (surface->dpy, surface->src_picture);

    if (surface->owns_pixmap)
	XFreePixmap (surface->dpy, surface->drawable);

    if (surface->gc)
	XFreeGC (surface->dpy, surface->gc);

    if (surface->clip_rects)
	free (surface->clip_rects);

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
	rect.height = interest_rect->height;
    
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
	masks.red_mask = (unsigned long)surface->format->direct.redMask << surface->format->direct.red;
	masks.green_mask = (unsigned long)surface->format->direct.greenMask << surface->format->direct.green;
	masks.blue_mask = (unsigned long)surface->format->direct.blueMask << surface->format->direct.blue;
	masks.alpha_mask = (unsigned long)surface->format->direct.alphaMask << surface->format->direct.alpha;
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
	image = (cairo_image_surface_t*)
	    cairo_image_surface_create_for_data ((unsigned char *) ximage->data,
						 format,
						 ximage->width, 
						 ximage->height,
						 ximage->bytes_per_line);
	if (image->base.status)
	    goto FAIL;
    }
    else
    {
	/* 
	 * XXX This can't work.  We must convert the data to one of the 
	 * supported pixman formats.  Pixman needs another function
	 * which takes data in an arbitrary format and converts it
	 * to something supported by that library.
	 */
	image = (cairo_image_surface_t*)
	    _cairo_image_surface_create_with_masks ((unsigned char *) ximage->data,
						    &masks,
						    ximage->width, 
						    ximage->height,
						    ximage->bytes_per_line);
	if (image->base.status)
	    goto FAIL;
    }

    /* Let the surface take ownership of the data */
    _cairo_image_surface_assume_ownership_of_data (image);
    ximage->data = NULL;
    XDestroyImage (ximage);
     
    *image_out = image;
    return CAIRO_STATUS_SUCCESS;

 FAIL:
    XDestroyImage (ximage);
    return CAIRO_STATUS_NO_MEMORY;
}

static void
_cairo_xlib_surface_ensure_src_picture (cairo_xlib_surface_t    *surface)
{
    if (!surface->src_picture)
	surface->src_picture = XRenderCreatePicture (surface->dpy, 
						     surface->drawable, 
						     surface->format,
						     0, NULL);
}
	
static void
_cairo_xlib_surface_set_picture_clip_rects (cairo_xlib_surface_t *surface)
{
    if (surface->have_clip_rects)
	XRenderSetPictureClipRectangles (surface->dpy, surface->dst_picture,
					 0, 0,
					 surface->clip_rects,
					 surface->num_clip_rects);
}

static void
_cairo_xlib_surface_set_gc_clip_rects (cairo_xlib_surface_t *surface)
{
    if (surface->have_clip_rects)
	XSetClipRectangles(surface->dpy, surface->gc,
			   0, 0,
			   surface->clip_rects,
			   surface->num_clip_rects, YXSorted);
}
    
static void
_cairo_xlib_surface_ensure_dst_picture (cairo_xlib_surface_t    *surface)
{
    if (!surface->dst_picture) {
	surface->dst_picture = XRenderCreatePicture (surface->dpy, 
						     surface->drawable, 
						     surface->format,
						     0, NULL);
	_cairo_xlib_surface_set_picture_clip_rects (surface);
    }
	
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
    _cairo_xlib_surface_set_gc_clip_rects (surface);
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
    if (status)
	return status;

    *image_out = image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
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
    if (status)
	return status;

    *image_out = image;
    *image_extra = NULL;
    
    return CAIRO_STATUS_SUCCESS;
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

/*
 * Return whether two xlib surfaces share the same
 * screen.  Both core and Render drawing require this
 * when using multiple drawables in an operation.
 */
static cairo_bool_t
_cairo_xlib_surface_same_screen (cairo_xlib_surface_t *dst,
				 cairo_xlib_surface_t *src)
{
    return dst->dpy == src->dpy && dst->screen == src->screen;
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

	if (_cairo_xlib_surface_same_screen (surface, xlib_src)) {
	    *clone_out = cairo_surface_reference (src);
	    
	    return CAIRO_STATUS_SUCCESS;
	}
    } else if (_cairo_surface_is_image (src)) {
	cairo_image_surface_t *image_src = (cairo_image_surface_t *)src;
	cairo_content_t content = _cairo_content_from_format (image_src->format);
    
	clone = (cairo_xlib_surface_t *)
	    _cairo_xlib_surface_create_similar (surface, content,
						image_src->width, image_src->height);
	if (clone->base.status)
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

    if (!surface->src_picture)
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

    XRenderSetPictureTransform (surface->dpy, surface->src_picture, &xtransform);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_filter (cairo_xlib_surface_t *surface,
				cairo_filter_t	     filter)
{
    char *render_filter;

    if (!surface->src_picture)
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

    XRenderSetPictureFilter (surface->dpy, surface->src_picture,
			     render_filter, NULL, 0);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_repeat (cairo_xlib_surface_t *surface, int repeat)
{
    XRenderPictureAttributes pa;
    unsigned long	     mask;

    if (!surface->src_picture)
	return CAIRO_STATUS_SUCCESS;
    
    mask = CPRepeat;
    pa.repeat = repeat;

    XRenderChangePicture (surface->dpy, surface->src_picture, mask, &pa);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_set_attributes (cairo_xlib_surface_t	  *surface,
				       cairo_surface_attributes_t *attributes)
{
    cairo_int_status_t status;

    _cairo_xlib_surface_ensure_src_picture (surface);

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

/* Checks whether we can can directly draw from src to dst with
 * the core protocol: either with CopyArea or using src as a
 * a tile in a GC.
 */
static cairo_bool_t
_surfaces_compatible (cairo_xlib_surface_t *dst,
		      cairo_xlib_surface_t *src)
{
    /* same screen */
    if (!_cairo_xlib_surface_same_screen (dst, src))
	return FALSE;
    
    /* same depth (for core) */
    if (src->depth != dst->depth)
	return FALSE;

    /* if Render is supported, match picture formats */
    if (src->format != NULL && src->format == dst->format)
	return TRUE;
    
    /* Without Render, match visuals instead */
    if (src->visual == dst->visual)
	return TRUE;

    return FALSE;
}

static cairo_bool_t
_surface_has_alpha (cairo_xlib_surface_t *surface)
{
    if (surface->format) {
	if (surface->format->type == PictTypeDirect &&
	    surface->format->direct.alphaMask != 0)
	    return TRUE;
	else
	    return FALSE;
    } else {
	
	/* In the no-render case, we never have alpha */
	return FALSE;
    }
}

/* Returns true if the given operator and source-alpha combination
 * requires alpha compositing to complete.
 */
static cairo_bool_t
_operator_needs_alpha_composite (cairo_operator_t operator,
				 cairo_bool_t     surface_has_alpha)
{
    if (operator == CAIRO_OPERATOR_SOURCE ||
	(!surface_has_alpha &&
	 (operator == CAIRO_OPERATOR_OVER ||
	  operator == CAIRO_OPERATOR_ATOP ||
	  operator == CAIRO_OPERATOR_IN)))
	return FALSE;

    return TRUE;
}

/* There is a bug in most older X servers with compositing using a
 * untransformed repeating source pattern when the source is in off-screen
 * video memory, and another with repeated transformed images using a
 * general tranform matrix. When these bugs could be triggered, we need a
 * fallback: in the common case where we have no transformation and the
 * source and destination have the same format/visual, we can do the
 * operation using the core protocol for the first bug, otherwise, we need
 * a software fallback.
 *
 * We can also often optimize a compositing operation by calling XCopyArea
 * for some common cases where there is no alpha compositing to be done.
 * We figure that out here as well.
 */
typedef enum {
    DO_RENDER,		/* use render */
    DO_XCOPYAREA,	/* core protocol XCopyArea optimization/fallback */
    DO_XTILE,		/* core protocol XSetTile optimization/fallback */
    DO_UNSUPPORTED	/* software fallback */
} composite_operation_t;

/* Initial check for the render bugs; we need to recheck for the
 * offscreen-memory bug after we turn patterns into surfaces, since that
 * may introduce a repeating pattern for gradient patterns.  We don't need
 * to check for the repeat+transform bug because gradient surfaces aren't
 * transformed.
 *
 * All we do here is reject cases where we *know* are going to
 * hit the bug and won't be able to use a core protocol fallback.
 */
static composite_operation_t
_categorize_composite_operation (cairo_xlib_surface_t *dst,
				 cairo_operator_t      operator,
				 cairo_pattern_t      *src_pattern,
				 cairo_bool_t	       have_mask)
			      
{
    if (!dst->buggy_repeat)
	return DO_RENDER;

    if (src_pattern->type == CAIRO_PATTERN_SURFACE)
    {
	cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *)src_pattern;
	
	if (_cairo_matrix_is_integer_translation (&src_pattern->matrix, NULL, NULL) &&
	    src_pattern->extend == CAIRO_EXTEND_REPEAT)
	{
	    /* This is the case where we have the bug involving
	     * untransformed repeating source patterns with off-screen
	     * video memory; reject some cases where a core protocol
	     * fallback is impossible.
	     */
	    if (have_mask ||
		!(operator == CAIRO_OPERATOR_SOURCE || operator == CAIRO_OPERATOR_OVER))
		return DO_UNSUPPORTED;

	    if (_cairo_surface_is_xlib (surface_pattern->surface)) {
		cairo_xlib_surface_t *src = (cairo_xlib_surface_t *)surface_pattern->surface;

		if (operator == CAIRO_OPERATOR_OVER && _surface_has_alpha (src))
		    return DO_UNSUPPORTED;
		
		/* If these are on the same screen but otherwise incompatible,
		 * make a copy as core drawing can't cross depths and doesn't
		 * work rightacross visuals of the same depth
		 */
		if (_cairo_xlib_surface_same_screen (dst, src) && 
		    !_surfaces_compatible (dst, src))
		    return DO_UNSUPPORTED;
	    }
	}

	/* Check for the other bug involving repeat patterns with general
	 * transforms. */
	if (!_cairo_matrix_is_integer_translation (&src_pattern->matrix, NULL, NULL) &&
	    src_pattern->extend == CAIRO_EXTEND_REPEAT)
	    return DO_UNSUPPORTED;
    }

    return DO_RENDER;
}

/* Recheck for composite-repeat once we've turned patterns into Xlib surfaces
 * If we end up returning DO_UNSUPPORTED here, we're throwing away work we
 * did to turn gradients into a pattern, but most of the time we can handle
 * that case with core protocol fallback.
 *
 * Also check here if we can just use XCopyArea, instead of going through
 * Render.
 */
static composite_operation_t
_recategorize_composite_operation (cairo_xlib_surface_t	      *dst,
				   cairo_operator_t	       operator,
				   cairo_xlib_surface_t	      *src,
				   cairo_surface_attributes_t *src_attr,
				   cairo_bool_t		       have_mask)
{
    cairo_bool_t is_integer_translation =
	_cairo_matrix_is_integer_translation (&src_attr->matrix, NULL, NULL);
    cairo_bool_t needs_alpha_composite =
	_operator_needs_alpha_composite (operator, _surface_has_alpha (src));

    if (!have_mask &&
	is_integer_translation &&
	src_attr->extend == CAIRO_EXTEND_NONE &&
	!needs_alpha_composite &&
	_surfaces_compatible(src, dst))
    {
	return DO_XCOPYAREA;
    }

    if (!dst->buggy_repeat)
	return DO_RENDER;
    
    if (is_integer_translation &&
	src_attr->extend == CAIRO_EXTEND_REPEAT &&
	(src->width != 1 || src->height != 1))
    {
	if (!have_mask &&
	    !needs_alpha_composite &&
	    _surfaces_compatible (dst, src))
	{
	    return DO_XTILE;
	}
	
	return DO_UNSUPPORTED;
    }
    
    return DO_RENDER;
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
    composite_operation_t       operation;
    int				itx, ity;

    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE (dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    operation = _categorize_composite_operation (dst, operator, src_pattern,
						 mask_pattern != NULL);
    if (operation == DO_UNSUPPORTED)
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
    
    operation = _recategorize_composite_operation (dst, operator, src, &src_attr,
						   mask_pattern != NULL);
    if (operation == DO_UNSUPPORTED) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	goto FAIL;
    }
	
    status = _cairo_xlib_surface_set_attributes (src, &src_attr);
    if (status)
	goto FAIL;

    switch (operation)
    {
    case DO_RENDER:
	_cairo_xlib_surface_ensure_dst_picture (dst);
	if (mask) {
	    status = _cairo_xlib_surface_set_attributes (mask, &mask_attr);
	    if (status)
		goto FAIL;
	    
	    XRenderComposite (dst->dpy,
			      _render_operator (operator),
			      src->src_picture,
			      mask->src_picture,
			      dst->dst_picture,
			      src_x + src_attr.x_offset,
			      src_y + src_attr.y_offset,
			      mask_x + mask_attr.x_offset,
			      mask_y + mask_attr.y_offset,
			      dst_x, dst_y,
			      width, height);
	} else {
	    XRenderComposite (dst->dpy,
			      _render_operator (operator),
			      src->src_picture,
			      0,
			      dst->dst_picture,
			      src_x + src_attr.x_offset,
			      src_y + src_attr.y_offset,
			      0, 0,
			      dst_x, dst_y,
			      width, height);
	}

	if (!_cairo_operator_bounded (operator))
	    status = _cairo_surface_composite_fixup_unbounded (&dst->base,
							       &src_attr, src->width, src->height,
							       mask ? &mask_attr : NULL,
							       mask ? mask->width : 0,
							       mask ? mask->height : 0,
							       src_x, src_y,
							       mask_x, mask_y,
							       dst_x, dst_y, width, height);
	break;

    case DO_XCOPYAREA:
	_cairo_xlib_surface_ensure_gc (dst);
	XCopyArea (dst->dpy,
		   src->drawable,
		   dst->drawable,
		   dst->gc,
		   src_x + src_attr.x_offset,
		   src_y + src_attr.y_offset,
		   width, height,
		   dst_x, dst_y);
	break;

    case DO_XTILE:
	/* This case is only used for bug fallbacks, though it is theoretically
	 * applicable to the case where we don't have the RENDER extension as
	 * well.
	 *
	 * We've checked that we have a repeating unscaled source in
	 * _recategorize_composite_operation.
	 */

	_cairo_xlib_surface_ensure_gc (dst);
	_cairo_matrix_is_integer_translation (&src_attr.matrix, &itx, &ity);

	XSetTSOrigin (dst->dpy, dst->gc,
		      - (itx + src_attr.x_offset), - (ity + src_attr.y_offset));
	XSetTile (dst->dpy, dst->gc, src->drawable);
	XSetFillStyle (dst->dpy, dst->gc, FillTiled);

	XFillRectangle (dst->dpy, dst->drawable, dst->gc,
			dst_x, dst_y, width, height);
	break;

    default:
	ASSERT_NOT_REACHED;
    }

 FAIL:

    if (mask)
	_cairo_pattern_release_surface (mask_pattern, &mask->base, &mask_attr);
    
    _cairo_pattern_release_surface (src_pattern, &src->base, &src_attr);

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
    _cairo_xlib_surface_ensure_dst_picture (surface);
    XRenderFillRectangles (surface->dpy,
			   _render_operator (operator),
			   surface->dst_picture,
			   &render_color, (XRectangle *) rects, num_rects);

    return CAIRO_STATUS_SUCCESS;
}

/* Creates an A8 picture of size @width x @height, initialized with @color
 */
static Picture
_create_a8_picture (cairo_xlib_surface_t *surface,
		    XRenderColor         *color,
		    int                   width,
		    int                   height,
		    cairo_bool_t          repeat)
{
    XRenderPictureAttributes pa;
    unsigned long mask = 0;

    Pixmap pixmap = XCreatePixmap (surface->dpy, surface->drawable,
				   width, height,
				   8);
    Picture picture;

    if (repeat) {
	pa.repeat = TRUE;
	mask = CPRepeat;
    }
    
    picture = XRenderCreatePicture (surface->dpy, pixmap,
				    XRenderFindStandardFormat (surface->dpy, PictStandardA8),
				    mask, &pa);
    XRenderFillRectangle (surface->dpy, PictOpSrc, picture, color,
			  0, 0, width, height);
    XFreePixmap (surface->dpy, pixmap);
    
    return picture;
}

/* Creates a temporary mask for the trapezoids covering the area
 * [@dst_x, @dst_y, @width, @height] of the destination surface.
 */
static Picture
_create_trapezoid_mask (cairo_xlib_surface_t *dst,
			cairo_trapezoid_t    *traps,
			int                   num_traps,
			int                   dst_x,
			int                   dst_y,
			int                   width,
			int                   height,
			XRenderPictFormat     *pict_format)
{
    XRenderColor transparent = { 0, 0, 0, 0 };
    XRenderColor solid = { 0xffff, 0xffff, 0xffff, 0xffff };
    Picture mask_picture, solid_picture;
    XTrapezoid *offset_traps;
    int i;

    /* This would be considerably simpler using XRenderAddTraps(), but since
     * we are only using this in the unbounded-operator case, we stick with
     * XRenderCompositeTrapezoids, which is available on older versions
     * of RENDER rather than conditionalizing. We should still hit an
     * optimization that avoids creating another intermediate surface on
     * the servers that have XRenderAddTraps().
     */
    mask_picture = _create_a8_picture (dst, &transparent, width, height, FALSE);
    solid_picture = _create_a8_picture (dst, &solid, width, height, TRUE);

    offset_traps = malloc (sizeof (XTrapezoid) * num_traps);
    if (!offset_traps)
	return None;

    for (i = 0; i < num_traps; i++) {
	offset_traps[i].top = traps[i].top - 0x10000 * dst_y;
	offset_traps[i].bottom = traps[i].bottom - 0x10000 * dst_y;
	offset_traps[i].left.p1.x = traps[i].left.p1.x - 0x10000 * dst_x;
	offset_traps[i].left.p1.y = traps[i].left.p1.y - 0x10000 * dst_y;
	offset_traps[i].left.p2.x = traps[i].left.p2.x - 0x10000 * dst_x;
	offset_traps[i].left.p2.y = traps[i].left.p2.y - 0x10000 * dst_y;
	offset_traps[i].right.p1.x = traps[i].right.p1.x - 0x10000 * dst_x;
	offset_traps[i].right.p1.y = traps[i].right.p1.y - 0x10000 * dst_y;
	offset_traps[i].right.p2.x = traps[i].right.p2.x - 0x10000 * dst_x;
	offset_traps[i].right.p2.y = traps[i].right.p2.y - 0x10000 * dst_y;
    }

    XRenderCompositeTrapezoids (dst->dpy, PictOpAdd,
				solid_picture, mask_picture,
				pict_format,
				0, 0,
				offset_traps, num_traps);
    
    XRenderFreePicture (dst->dpy, solid_picture);
    free (offset_traps);

    return mask_picture;
}

static cairo_int_status_t
_cairo_xlib_surface_composite_trapezoids (cairo_operator_t	operator,
					  cairo_pattern_t	*pattern,
					  void			*abstract_dst,
					  cairo_antialias_t	antialias,
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
    composite_operation_t       operation;
    int				render_reference_x, render_reference_y;
    int				render_src_x, render_src_y;
    XRenderPictFormat		*pict_format;

    if (!CAIRO_SURFACE_RENDER_HAS_TRAPEZOIDS (dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    operation = _categorize_composite_operation (dst, operator, pattern, TRUE);
    if (operation == DO_UNSUPPORTED)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    
    status = _cairo_pattern_acquire_surface (pattern, &dst->base,
					     src_x, src_y, width, height,
					     (cairo_surface_t **) &src,
					     &attributes);
    if (status)
	return status;

    operation = _recategorize_composite_operation (dst, operator, src, &attributes, TRUE);
    if (operation == DO_UNSUPPORTED) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	goto FAIL;
    }

    switch (antialias) {
    case CAIRO_ANTIALIAS_NONE:
	pict_format = XRenderFindStandardFormat (dst->dpy, PictStandardA1);
	break;
    default:
	pict_format = XRenderFindStandardFormat (dst->dpy, PictStandardA8);
	break;
    }
	
    if (traps[0].left.p1.y < traps[0].left.p2.y) {
	render_reference_x = _cairo_fixed_integer_floor (traps[0].left.p1.x);
	render_reference_y = _cairo_fixed_integer_floor (traps[0].left.p1.y);
    } else {
	render_reference_x = _cairo_fixed_integer_floor (traps[0].left.p2.x);
	render_reference_y = _cairo_fixed_integer_floor (traps[0].left.p2.y);
    }

    render_src_x = src_x + render_reference_x - dst_x;
    render_src_y = src_y + render_reference_y - dst_y;

    _cairo_xlib_surface_ensure_dst_picture (dst);
    status = _cairo_xlib_surface_set_attributes (src, &attributes);
    if (status)
	goto FAIL;

    if (!_cairo_operator_bounded (operator)) {
	/* XRenderCompositeTrapezoids() creates a mask only large enough for the
	 * trapezoids themselves, but if the operator is unbounded, then we need
	 * to actually composite all the way out to the bounds, so we create
	 * the mask and composite ourselves. There actually would
	 * be benefit to doing this in all cases, since RENDER implementations
	 * will frequently create a too temporary big mask, ignoring destination
	 * bounds and clip. (XRenderAddTraps() could be used to make creating
	 * the mask somewhat cheaper.)
	 */
	Picture mask_picture = _create_trapezoid_mask (dst, traps, num_traps,
						       dst_x, dst_y, width, height,
						       pict_format);
	if (!mask_picture) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto FAIL;
	}

	XRenderComposite (dst->dpy,
			  _render_operator (operator),
			  src->src_picture,
			  mask_picture,
			  dst->dst_picture,
			  src_x + attributes.x_offset,
			  src_y + attributes.y_offset,
			  0, 0,
			  dst_x, dst_y,
			  width, height);
	
	XRenderFreePicture (dst->dpy, mask_picture);
	
	status = _cairo_surface_composite_shape_fixup_unbounded (&dst->base,
								 &attributes, src->width, src->height,
								 width, height,
								 src_x, src_y,
								 0, 0,
								 dst_x, dst_y, width, height);

	
    } else {
	/* XXX: The XTrapezoid cast is evil and needs to go away somehow. */
	XRenderCompositeTrapezoids (dst->dpy,
				    _render_operator (operator),
				    src->src_picture, dst->dst_picture,
				    pict_format,
				    render_src_x + attributes.x_offset, 
				    render_src_y + attributes.y_offset,
				    (XTrapezoid *) traps, num_traps);
    }

 FAIL:
    _cairo_pattern_release_surface (pattern, &src->base, &attributes);

    return status;
}

static cairo_int_status_t
_cairo_xlib_surface_set_clip_region (void              *abstract_surface,
				     pixman_region16_t *region)
{
    cairo_xlib_surface_t *surface = (cairo_xlib_surface_t *) abstract_surface;

    if (surface->clip_rects) {
	free (surface->clip_rects);
	surface->clip_rects = NULL;
    }

    surface->have_clip_rects = FALSE;
    surface->num_clip_rects = 0;

    if (region == NULL) {
	if (surface->gc)
	    XSetClipMask (surface->dpy, surface->gc, None);
	
	if (surface->format && surface->dst_picture) {
	    XRenderPictureAttributes pa;
	    pa.clip_mask = None;
	    XRenderChangePicture (surface->dpy, surface->dst_picture,
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

	surface->have_clip_rects = TRUE;
	surface->clip_rects = rects;
	surface->num_clip_rects = n_boxes;

	if (surface->gc)
	    _cairo_xlib_surface_set_gc_clip_rects (surface);

	if (surface->dst_picture)
	    _cairo_xlib_surface_set_picture_clip_rects (surface);
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

static void
_cairo_xlib_surface_get_font_options (void                  *abstract_surface,
				      cairo_font_options_t  *options)
{
    cairo_xlib_surface_t *surface = abstract_surface;
  
    *options = surface->screen_info->font_options;
    
    if (_surface_has_alpha (surface) && options->antialias == CAIRO_ANTIALIAS_SUBPIXEL)
	options->antialias = CAIRO_ANTIALIAS_GRAY;
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
    NULL, /* intersect_clip_path */
    _cairo_xlib_surface_get_extents,
    _cairo_xlib_surface_show_glyphs,
    NULL, /* fill_path */
    _cairo_xlib_surface_get_font_options
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
				     Screen		       *screen,
				     Visual		       *visual,
				     XRenderPictFormat	       *format,
				     int			width,
				     int			height,
				     int			depth)
{
    cairo_xlib_surface_t *surface;
    cairo_xlib_screen_info_t *screen_info;

    screen_info = _cairo_xlib_screen_info_get (dpy, screen);
    if (screen_info == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    surface = malloc (sizeof (cairo_xlib_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&surface->base, &cairo_xlib_surface_backend);

    surface->dpy = dpy;
    surface->screen_info = screen_info;

    surface->gc = NULL;
    surface->drawable = drawable;
    surface->screen = screen;
    surface->owns_pixmap = FALSE;
    surface->use_pixmap = 0;
    surface->width = width;
    surface->height = height;
    
    if (format) {
	depth = format->depth;
    } else if (visual) {
	int j, k;

	/* This is ugly, but we have to walk over all visuals
	 * for the display to find the depth.
	 */
	for (j = 0; j < screen->ndepths; j++) {
	    Depth *d = &screen->depths[j];
	    for (k = 0; k < d->nvisuals; k++) {
		if (&d->visuals[k] == visual) {
		    depth = d->depth;
		    goto found;
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

    surface->buggy_repeat = FALSE;
    if (strcmp (ServerVendor (dpy), "The X.Org Foundation") == 0) {
	if (VendorRelease (dpy) <= 60802000)
	    surface->buggy_repeat = TRUE;
    } else if (strcmp (ServerVendor (dpy), "The XFree86 Project, Inc") == 0) {
	if (VendorRelease (dpy) <= 40400000)
	    surface->buggy_repeat = TRUE;
    }

    surface->dst_picture = None;
    surface->src_picture = None;

    if (CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE (surface)) {
	if (!format) {
	    if (visual)
		format = XRenderFindVisualFormat (dpy, visual);
	    else if (depth == 1)
		format = XRenderFindStandardFormat (dpy, PictStandardA1);
	}
    } else {
	format = NULL;
    }

    surface->visual = visual;
    surface->format = format;
    surface->depth = depth;

    surface->have_clip_rects = FALSE;
    surface->clip_rects = NULL;
    surface->num_clip_rects = 0;

    return (cairo_surface_t *) surface;
}

static Screen *
_cairo_xlib_screen_from_visual (Display *dpy, Visual *visual)
{
    int	    s;
    int	    d;
    int	    v;
    Screen *screen;
    Depth  *depth;

    for (s = 0; s < ScreenCount (dpy); s++) {
	screen = ScreenOfDisplay (dpy, s);
	if (visual == DefaultVisualOfScreen (screen))
	    return screen;
	for (d = 0; d < screen->ndepths; d++) {
	    depth = &screen->depths[d];
	    for (v = 0; v < depth->nvisuals; v++)
		if (visual == &depth->visuals[v])
		    return screen;
	}
    }
    return NULL;
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
    Screen *screen = _cairo_xlib_screen_from_visual (dpy, visual);

    if (screen == NULL) {
	_cairo_error (CAIRO_STATUS_INVALID_VISUAL);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }
    
    return _cairo_xlib_surface_create_internal (dpy, drawable, screen,
						visual, NULL, width, height, 0);
}

/**
 * cairo_xlib_surface_create_for_bitmap:
 * @dpy: an X Display
 * @bitmap: an X Drawable, (a depth-1 Pixmap)
 * @screen: the X Screen associated with @bitmap
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
				      Screen   *screen,
				      int	width,
				      int	height)
{
    return _cairo_xlib_surface_create_internal (dpy, bitmap, screen,
						NULL, NULL, width, height, 1);
}

/**
 * cairo_xlib_surface_create_with_xrender_format:
 * @dpy: an X Display
 * @drawable: an X Drawable, (a Pixmap or a Window)
 * @screen: the X Screen associated with @drawable
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
					       Screen		    *screen,
					       XRenderPictFormat    *format,
					       int		    width,
					       int		    height)
{
    return _cairo_xlib_surface_create_internal (dpy, drawable, screen,
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
/**
 * cairo_xlib_surface_set_drawable:
 * @surface: a #cairo_surface_t for the XLib backend
 * @drawable: the new drawable for the surface
 * 
 * Informs cairo of a new X Drawable underlying the
 * surface. The drawable must match the display, screen
 * and format of the existing drawable or the application
 * will get X protocol errors and will probably terminate.
 * No checks are done by this function to ensure this
 * compatibility.
 **/
void
cairo_xlib_surface_set_drawable (cairo_surface_t   *abstract_surface,
				 Drawable	    drawable,
				 int		    width,
				 int		    height)
{
    cairo_xlib_surface_t *surface = (cairo_xlib_surface_t *)abstract_surface;

    /* XXX: How do we want to handle this error case? */
    if (! _cairo_surface_is_xlib (abstract_surface))
	return;

    /* XXX: and what about this case? */
    if (surface->owns_pixmap)
	return;
    
    if (surface->drawable != drawable) {
	if (surface->dst_picture)
	    XRenderFreePicture (surface->dpy, surface->dst_picture);
	
	if (surface->src_picture)
	    XRenderFreePicture (surface->dpy, surface->src_picture);
    
	surface->dst_picture = None;
	surface->src_picture = None;
    
	surface->drawable = drawable;
    }
    surface->width = width;
    surface->height = height;
}

/* RENDER glyphset cache code */

typedef struct glyphset_cache {
    cairo_cache_t base;

    Display *display;
    Glyph counter;

    XRenderPictFormat *a1_pict_format;
    GlyphSet a1_glyphset;

    XRenderPictFormat *a8_pict_format;
    GlyphSet a8_glyphset;

    XRenderPictFormat *argb32_pict_format;
    GlyphSet argb32_glyphset;

    struct glyphset_cache *next;
} glyphset_cache_t;

typedef struct {
    cairo_glyph_cache_key_t key;
    GlyphSet glyphset;
    Glyph glyph;
    cairo_glyph_size_t size;
} glyphset_cache_entry_t;

static Glyph
_next_xlib_glyph (glyphset_cache_t *cache)
{
    return ++(cache->counter);
}

static cairo_bool_t
_native_byte_order_lsb (void)
{
    int	x = 1;

    return *((char *) &x) == 1;
}

static cairo_status_t 
_xlib_glyphset_cache_create_entry (void *abstract_cache,
				   void *abstract_key,
				   void **return_entry)
{
    glyphset_cache_t *cache = abstract_cache;
    cairo_glyph_cache_key_t *key = abstract_key;
    glyphset_cache_entry_t *entry;
    XGlyphInfo glyph_info;
    unsigned char *data;

    cairo_status_t status;

    cairo_cache_t *im_cache;
    cairo_image_glyph_cache_entry_t *im;

    entry = malloc (sizeof (glyphset_cache_entry_t));
    _cairo_lock_global_image_glyph_cache ();
    im_cache = _cairo_get_global_image_glyph_cache ();

    if (cache == NULL || entry == NULL || im_cache == NULL) {
	_cairo_unlock_global_image_glyph_cache ();
	if (entry)
	    free (entry);
	return CAIRO_STATUS_NO_MEMORY;
    }

    status = _cairo_cache_lookup (im_cache, key, (void **) (&im), NULL);
    if (status != CAIRO_STATUS_SUCCESS || im == NULL) {
	_cairo_unlock_global_image_glyph_cache ();
	free (entry);
	return CAIRO_STATUS_NO_MEMORY;
    }

    entry->key = *key;
    _cairo_unscaled_font_reference (entry->key.unscaled);

    if (!im->image) {
	entry->glyph = None;
	entry->glyphset = None;
	entry->key.base.memory = 0;
	entry->size.x = entry->size.y = entry->size.width = entry->size.height = 0;
	
	goto out;
    }
    
    entry->glyph = _next_xlib_glyph (cache);

    data = im->image->data;

    entry->size = im->size;

    glyph_info.width = im->size.width;
    glyph_info.height = im->size.height;

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

    glyph_info.x = -im->size.x;
    glyph_info.y = -im->size.y;
    glyph_info.xOff = 0;
    glyph_info.yOff = 0;

    switch (im->image->format) {
    case CAIRO_FORMAT_A1:
	/* local bitmaps are always stored with bit == byte */
	if (_native_byte_order_lsb() != 
	    (BitmapBitOrder (cache->display) == LSBFirst)) 
	{
	    int		    c = im->image->stride * im->size.height;
	    unsigned char   *d;
	    unsigned char   *new, *n;
	    
	    new = malloc (c);
	    if (!new)
		return CAIRO_STATUS_NO_MEMORY;
	    n = new;
	    d = data;
	    while (c--)
	    {
		char	b = *d++;
		b = ((b << 1) & 0xaa) | ((b >> 1) & 0x55);
		b = ((b << 2) & 0xcc) | ((b >> 2) & 0x33);
		b = ((b << 4) & 0xf0) | ((b >> 4) & 0x0f);
		*n++ = b;
	    }
	    data = new;
	}
	entry->glyphset = cache->a1_glyphset;
	break;
    case CAIRO_FORMAT_A8:
	entry->glyphset = cache->a8_glyphset;
	break;
    case CAIRO_FORMAT_ARGB32:
	if (_native_byte_order_lsb() != 
	    (ImageByteOrder (cache->display) == LSBFirst)) 
	{
	    int		    c = im->image->stride * im->size.height;
	    unsigned char   *d;
	    unsigned char   *new, *n;
	    
	    new = malloc (c);
	    if (!new)
		return CAIRO_STATUS_NO_MEMORY;
	    n = new;
	    d = data;
	    while ((c -= 4) >= 0)
	    {
		n[3] = d[0];
		n[2] = d[1];
		n[1] = d[2];
		n[0] = d[3];
		d += 4;
		n += 4;
	    }
	    data = new;
	}
	entry->glyphset = cache->argb32_glyphset;
	break;
    case CAIRO_FORMAT_RGB24:
    default:
	ASSERT_NOT_REACHED;
	break;
    }
    /* XXX assume X server wants pixman padding. Xft assumes this as well */

    XRenderAddGlyphs (cache->display, entry->glyphset,
		      &(entry->glyph), &(glyph_info), 1,
		      (char *) data,
		      im->image->stride * glyph_info.height);

    if (data != im->image->data)
	free (data);
    
    entry->key.base.memory = im->image->height * im->image->stride;

 out:
    *return_entry = entry;
    _cairo_unlock_global_image_glyph_cache ();
    
    return CAIRO_STATUS_SUCCESS;
}

static void 
_xlib_glyphset_cache_destroy_cache (void *cache)
{
    /* FIXME: will we ever release glyphset caches? */
}

static void 
_xlib_glyphset_cache_destroy_entry (void *abstract_cache,
				    void *abstract_entry)
{
    glyphset_cache_t *cache = abstract_cache;
    glyphset_cache_entry_t *entry = abstract_entry;

    _cairo_unscaled_font_destroy (entry->key.unscaled);
    if (entry->glyph)
	XRenderFreeGlyphs (cache->display, entry->glyphset,
			   &(entry->glyph), 1);
    free (entry);	
}

static const cairo_cache_backend_t _xlib_glyphset_cache_backend = {
    _cairo_glyph_cache_hash,
    _cairo_glyph_cache_keys_equal,
    _xlib_glyphset_cache_create_entry,
    _xlib_glyphset_cache_destroy_entry,
    _xlib_glyphset_cache_destroy_cache
};

CAIRO_MUTEX_DECLARE(_xlib_glyphset_caches_mutex);

static glyphset_cache_t *
_xlib_glyphset_caches = NULL;

static void
_lock_xlib_glyphset_caches (void)
{
    CAIRO_MUTEX_LOCK(_xlib_glyphset_caches_mutex);
}

static void
_unlock_xlib_glyphset_caches (glyphset_cache_t *cache)
{
    if (cache) {
	_cairo_cache_shrink_to (&cache->base,
				CAIRO_XLIB_GLYPH_CACHE_MEMORY_DEFAULT);
    }
    CAIRO_MUTEX_UNLOCK(_xlib_glyphset_caches_mutex);
}

static glyphset_cache_t *
_get_glyphset_cache (Display *d)
{
    /* 
     * There should usually only be one, or a very small number, of
     * displays. So we just do a linear scan. 
     */
    glyphset_cache_t *cache;

    /* XXX: This is not thread-safe. Xft has example code to get
     * per-display data via Xlib extension mechanisms. */
    for (cache = _xlib_glyphset_caches; cache != NULL; cache = cache->next) {
	if (cache->display == d)
	    return cache;
    }

    cache = malloc (sizeof (glyphset_cache_t));
    if (cache == NULL) 
	goto ERR;

    if (_cairo_cache_init (&cache->base,
			   &_xlib_glyphset_cache_backend, 0))
	goto FREE_GLYPHSET_CACHE;

    cache->display = d;
    cache->counter = 0;

    cache->a1_pict_format = XRenderFindStandardFormat (d, PictStandardA1);
    cache->a1_glyphset = XRenderCreateGlyphSet (d, cache->a1_pict_format);

    cache->a8_pict_format = XRenderFindStandardFormat (d, PictStandardA8);
    cache->a8_glyphset = XRenderCreateGlyphSet (d, cache->a8_pict_format);

    cache->argb32_pict_format = XRenderFindStandardFormat (d, PictStandardARGB32);
    cache->argb32_glyphset = XRenderCreateGlyphSet (d, cache->argb32_pict_format);;
    
    cache->next = _xlib_glyphset_caches;
    _xlib_glyphset_caches = cache;

    return cache;
    
 FREE_GLYPHSET_CACHE:
    free (cache);
    
 ERR:
    return NULL;
}

#define N_STACK_BUF 1024

static XRenderPictFormat *
_select_text_mask_format (glyphset_cache_t *cache,
			  cairo_bool_t	    have_a1_glyphs,
			  cairo_bool_t 	    have_a8_glyphs,
			  cairo_bool_t 	    have_argb32_glyphs)
{
    if (have_a8_glyphs)
	return cache->a8_pict_format;

    if (have_a1_glyphs && have_argb32_glyphs)
	return cache->a8_pict_format;

    if (have_a1_glyphs)
	return cache->a1_pict_format;

    if (have_argb32_glyphs)
	return cache->argb32_pict_format;

    /* when there are no glyphs to draw, just pick something */
    return cache->a8_pict_format;
}

static cairo_status_t
_cairo_xlib_surface_show_glyphs32 (cairo_scaled_font_t    *scaled_font,
				   cairo_operator_t       operator,
				   glyphset_cache_t 	  *cache,
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

    int i, count;    
    int thisX, thisY;
    int lastX = 0, lastY = 0;

    cairo_bool_t have_a1, have_a8, have_argb32;
    XRenderPictFormat *mask_format;

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

    have_a1 = FALSE;
    have_a8 = FALSE;
    have_argb32 = FALSE;
    count = 0;

    for (i = 0; i < num_glyphs; ++i) {
	GlyphSet glyphset;
	
	if (!entries[i]->glyph)
	    continue;

	glyphset = entries[i]->glyphset;

	if (glyphset == cache->a1_glyphset)
	    have_a1 = TRUE;
	else if (glyphset == cache->a8_glyphset)
	    have_a8 = TRUE;
	else if (glyphset == cache->argb32_glyphset)
	    have_argb32 = TRUE;

	chars[count] = entries[i]->glyph;
	elts[count].chars = &(chars[count]);
	elts[count].nchars = 1;
	elts[count].glyphset = glyphset;
	thisX = (int) floor (glyphs[i].x + 0.5);
	thisY = (int) floor (glyphs[i].y + 0.5);
	elts[count].xOff = thisX - lastX;
	elts[count].yOff = thisY - lastY;
	lastX = thisX;
	lastY = thisY;
	count++;
    }

    mask_format = _select_text_mask_format (cache,
					    have_a1, have_a8, have_argb32);

    XRenderCompositeText32 (self->dpy,
			    _render_operator (operator),
			    src->src_picture,
			    self->dst_picture,
			    mask_format,
			    source_x, source_y,
			    0, 0,
			    elts, count);

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
				   glyphset_cache_t 	  *cache,
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

    int i, count;
    int thisX, thisY;
    int lastX = 0, lastY = 0;

    cairo_bool_t have_a1, have_a8, have_argb32;
    XRenderPictFormat *mask_format;

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

    have_a1 = FALSE;
    have_a8 = FALSE;
    have_argb32 = FALSE;
    count = 0;

    for (i = 0; i < num_glyphs; ++i) {
	GlyphSet glyphset;
	
	if (!entries[i]->glyph)
	    continue;

	glyphset = entries[i]->glyphset;

	if (glyphset == cache->a1_glyphset)
	    have_a1 = TRUE;
	else if (glyphset == cache->a8_glyphset)
	    have_a8 = TRUE;
	else if (glyphset == cache->argb32_glyphset)
	    have_argb32 = TRUE;

	chars[count] = entries[i]->glyph;
	elts[count].chars = &(chars[count]);
	elts[count].nchars = 1;
	elts[count].glyphset = glyphset;
	thisX = (int) floor (glyphs[i].x + 0.5);
	thisY = (int) floor (glyphs[i].y + 0.5);
	elts[count].xOff = thisX - lastX;
	elts[count].yOff = thisY - lastY;
	lastX = thisX;
	lastY = thisY;
	count++;
    }

    mask_format = _select_text_mask_format (cache,
					    have_a1, have_a8, have_argb32);

    XRenderCompositeText16 (self->dpy,
			    _render_operator (operator),
			    src->src_picture,
			    self->dst_picture,
			    mask_format,
			    source_x, source_y,
			    0, 0,
			    elts, count);

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
				  glyphset_cache_t 	 *cache,
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

    int i, count;
    int thisX, thisY;
    int lastX = 0, lastY = 0;

    cairo_bool_t have_a1, have_a8, have_argb32;
    XRenderPictFormat *mask_format;

    if (num_glyphs == 0)
	return CAIRO_STATUS_SUCCESS;

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

    have_a1 = FALSE;
    have_a8 = FALSE;
    have_argb32 = FALSE;
    count = 0;

    for (i = 0; i < num_glyphs; ++i) {
	GlyphSet glyphset;
	
	if (!entries[i]->glyph)
	    continue;

	glyphset = entries[i]->glyphset;

	if (glyphset == cache->a1_glyphset)
	    have_a1 = TRUE;
	else if (glyphset == cache->a8_glyphset)
	    have_a8 = TRUE;
	else if (glyphset == cache->argb32_glyphset)
	    have_argb32 = TRUE;

	chars[count] = entries[i]->glyph;
	elts[count].chars = &(chars[count]);
	elts[count].nchars = 1;
	elts[count].glyphset = glyphset;
	thisX = (int) floor (glyphs[i].x + 0.5);
	thisY = (int) floor (glyphs[i].y + 0.5);
	elts[count].xOff = thisX - lastX;
	elts[count].yOff = thisY - lastY;
	lastX = thisX;
	lastY = thisY;
	count++;
    }

    mask_format = _select_text_mask_format (cache,
					    have_a1, have_a8, have_argb32);

    XRenderCompositeText8 (self->dpy,
			   _render_operator (operator),
			   src->src_picture,
			   self->dst_picture,
			   mask_format,
			   source_x, source_y,
			   0, 0,
			   elts, count);
    
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

/* Handles clearing the regions that are outside of the temporary
 * mask created by XRenderCompositeText[N] but should be affected
 * by an unbounded operator like CAIRO_OPERATOR_SOURCE.
 */
static cairo_status_t
_show_glyphs_fixup_unbounded (cairo_xlib_surface_t       *self,
			      cairo_surface_attributes_t *src_attr,
			      cairo_xlib_surface_t       *src,
			      const cairo_glyph_t        *glyphs,
			      glyphset_cache_entry_t    **entries,
			      int                         num_glyphs,
			      int                         src_x,
			      int                         src_y,
			      int                         dst_x,
			      int                         dst_y,
			      int                         width,
			      int                         height)
{
    int x1 = INT_MAX;
    int x2 = INT_MIN;
    int y1 = INT_MAX;
    int y2 = INT_MIN;
    int i;

    /* Compute the size of the glyph mask as the bounding box
     * of all the glyphs.
     */
    for (i = 0; i < num_glyphs; ++i) {
	int thisX, thisY;
	
	if (entries[i] == NULL || !entries[i]->glyph) 
	    continue;
	
	thisX = (int) floor (glyphs[i].x + 0.5);
	thisY = (int) floor (glyphs[i].y + 0.5);
	
	if (thisX + entries[i]->size.x < x1)
	    x1 = thisX + entries[i]->size.x;
	if (thisX + entries[i]->size.x + entries[i]->size.width > x2)
	    x2 = thisX + entries[i]->size.x + entries[i]->size.width;
	if (thisY + entries[i]->size.y < y1)
	    y1 = thisY + entries[i]->size.y;
	if (thisY + entries[i]->size.y + entries[i]->size.height > y2)
	    y2 = thisY + entries[i]->size.y + entries[i]->size.height;
    }

    if (x1 >= x2 || y1 >= y2)
	x1 = x2 = y1 = y2 = 0;

    return _cairo_surface_composite_shape_fixup_unbounded (&self->base,
							   src_attr, src->width, src->height,
							   x2 - x1, y2 - y1,
							   src_x, src_y,
							   dst_x - x1, dst_y - y1,
							   dst_x, dst_y, width, height);
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
    glyphset_cache_t *cache;
    cairo_glyph_cache_key_t key;
    glyphset_cache_entry_t **entries;
    glyphset_cache_entry_t *stack_entries [N_STACK_BUF];
    composite_operation_t operation;
    int i;

    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE_TEXT (self) || !self->format)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    operation = _categorize_composite_operation (self, operator, pattern, TRUE);
    if (operation == DO_UNSUPPORTED)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    
    status = _cairo_pattern_acquire_surface (pattern, &self->base,
					     source_x, source_y, width, height,
					     (cairo_surface_t **) &src,
					     &attributes);
    if (status)
	return status;

    operation = _recategorize_composite_operation (self, operator, src, &attributes, TRUE);
    if (operation == DO_UNSUPPORTED) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	goto FAIL;
    }
	
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
    cache = _get_glyphset_cache (self->dpy);
    if (cache == NULL)
	goto UNLOCK;

    /* Work out the index size to use. */
    elt_size = 8;
    status = _cairo_scaled_font_get_glyph_cache_key (scaled_font, &key);
    if (status)
	goto UNLOCK;

    for (i = 0; i < num_glyphs; ++i) {
	key.index = glyphs[i].index;
	status = _cairo_cache_lookup (&cache->base, &key, (void **) (&entries[i]), NULL);
	if (status != CAIRO_STATUS_SUCCESS || entries[i] == NULL) 
	    goto UNLOCK;

	if (elt_size == 8 && entries[i]->glyph > 0xff)
	    elt_size = 16;
	if (elt_size == 16 && entries[i]->glyph > 0xffff) {
	    elt_size = 32;
	    break;
	}
    }

    /* Call the appropriate sub-function. */

    _cairo_xlib_surface_ensure_dst_picture (self);
    if (elt_size == 8)
    {
	status = _cairo_xlib_surface_show_glyphs8 (scaled_font, operator, cache, &key, src, self,
						   source_x + attributes.x_offset,
						   source_y + attributes.y_offset, 
						   glyphs, entries, num_glyphs);
    }
    else if (elt_size == 16)
    {
	status = _cairo_xlib_surface_show_glyphs16 (scaled_font, operator, cache, &key, src, self,
						    source_x + attributes.x_offset,
						    source_y + attributes.y_offset, 
						    glyphs, entries, num_glyphs);
    }
    else 
    {
	status = _cairo_xlib_surface_show_glyphs32 (scaled_font, operator, cache, &key, src, self,
						    source_x + attributes.x_offset,
						    source_y + attributes.y_offset, 
						    glyphs, entries, num_glyphs);
    }

    if (status == CAIRO_STATUS_SUCCESS &&
	!_cairo_operator_bounded (operator))
	status = _show_glyphs_fixup_unbounded (self,
					       &attributes, src,
					       glyphs, entries, num_glyphs,
					       source_x, source_y,
					       dest_x, dest_y, width, height);

 UNLOCK:
    _unlock_xlib_glyphset_caches (cache);

    if (num_glyphs >= N_STACK_BUF)
	free (entries); 

 FAIL:
    _cairo_pattern_release_surface (pattern, &src->base, &attributes);
    
    return status;
}

static void
_destroy_glyphset_cache_recurse (glyphset_cache_t *cache)
{
    if (cache == NULL)
	return;

    _destroy_glyphset_cache_recurse (cache->next);
    _cairo_cache_destroy (&cache->base);
    free (cache);
}

void
_cairo_xlib_surface_reset_static_data (void)
{
    _lock_xlib_glyphset_caches ();
    _destroy_glyphset_cache_recurse (_xlib_glyphset_caches);
    _xlib_glyphset_caches = NULL;
    _unlock_xlib_glyphset_caches (NULL);
}

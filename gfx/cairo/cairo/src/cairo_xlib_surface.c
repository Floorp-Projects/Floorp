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
 *	Carl D. Worth <cworth@isi.edu>
 */

#include "cairoint.h"
#include "cairo-xlib.h"

void
cairo_set_target_drawable (cairo_t	*cr,
			   Display	*dpy,
			   Drawable	drawable)
{
    cairo_surface_t *surface;

    if (cr->status && cr->status != CAIRO_STATUS_NO_TARGET_SURFACE)
	    return;

    surface = cairo_xlib_surface_create (dpy, drawable,
					 DefaultVisual (dpy, DefaultScreen (dpy)),
					 0,
					 DefaultColormap (dpy, DefaultScreen (dpy)));
    if (surface == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	return;
    }

    cairo_set_target_surface (cr, surface);

    /* cairo_set_target_surface takes a reference, so we must destroy ours */
    cairo_surface_destroy (surface);
}

typedef struct _cairo_xlib_surface {
    cairo_surface_t base;

    Display *dpy;
    GC gc;
    Drawable drawable;
    int owns_pixmap;
    Visual *visual;
    cairo_format_t format;

    int render_major;
    int render_minor;

    int width;
    int height;

    Picture picture;
} cairo_xlib_surface_t;

#define CAIRO_SURFACE_RENDER_AT_LEAST(surface, major, minor)	\
	(((surface)->render_major > major) ||			\
	 (((surface)->render_major == major) && ((surface)->render_minor >= minor)))

#define CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 0)
#define CAIRO_SURFACE_RENDER_HAS_COMPOSITE(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 0)

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

static cairo_surface_t *
_cairo_xlib_surface_create_with_size (Display		*dpy,
				      Drawable		drawable,
				      Visual		*visual,
				      cairo_format_t	format,
				      Colormap		colormap,
				      int               width,
				      int               height);


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

    /* XXX: There's a pretty lame heuristic here. This assumes that
     * all non-Render X servers do not support depth-32 pixmaps, (and
     * that they do support depths 1, 8, and 24). Obviously, it would
     * be much better to check the depths that are actually
     * supported. */
    if (!dpy
	|| (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE (src)
	    && format == CAIRO_FORMAT_ARGB32))
    {
	return NULL;
    }

    scr = DefaultScreen (dpy);

    pix = XCreatePixmap (dpy, DefaultRootWindow (dpy),
			 width <= 0 ? 1 : width, height <= 0 ? 1 : height,
			 _CAIRO_FORMAT_DEPTH (format));
    
    surface = (cairo_xlib_surface_t *)
	_cairo_xlib_surface_create_with_size (dpy, pix, NULL, format,
					      DefaultColormap (dpy, scr),
					      width, height);
    surface->owns_pixmap = 1;

    surface->width = width;
    surface->height = height;

    return &surface->base;
}

static void
_cairo_xlib_surface_destroy (void *abstract_surface)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    if (surface->picture)
	XRenderFreePicture (surface->dpy, surface->picture);

    if (surface->owns_pixmap)
	XFreePixmap (surface->dpy, surface->drawable);

    if (surface->gc)
	XFreeGC (surface->dpy, surface->gc);

    surface->dpy = 0;

    free (surface);
}

static double
_cairo_xlib_surface_pixels_per_inch (void *abstract_surface)
{
    /* XXX: We should really get this value from somewhere like Xft.dpy */
    return 96.0;
}

static cairo_image_surface_t *
_cairo_xlib_surface_get_image (void *abstract_surface)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;

    XImage *ximage;
    Window root_ignore;
    int x_ignore, y_ignore, bwidth_ignore, depth_ignore;

    XGetGeometry (surface->dpy, 
		  surface->drawable, 
		  &root_ignore, &x_ignore, &y_ignore,
		  &surface->width, &surface->height,
		  &bwidth_ignore, &depth_ignore);

    /* XXX: This should try to use the XShm extension if availible */
    ximage = XGetImage (surface->dpy,
			surface->drawable,
			0, 0,
			surface->width, surface->height,
			AllPlanes, ZPixmap);

    if (surface->visual) {
	cairo_format_masks_t masks;

	/* XXX: Add support here for pictures with external alpha? */

	masks.bpp = ximage->bits_per_pixel;
	masks.alpha_mask = 0;
	masks.red_mask = surface->visual->red_mask;
	masks.green_mask = surface->visual->green_mask;
	masks.blue_mask = surface->visual->blue_mask;

	image = _cairo_image_surface_create_with_masks (ximage->data,
							&masks,
							ximage->width, 
							ximage->height,
							ximage->bytes_per_line);
    } else {
	image = (cairo_image_surface_t *)
	    cairo_image_surface_create_for_data (ximage->data,
						 surface->format,
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

    return image;
}

static void
_cairo_xlib_surface_ensure_gc (cairo_xlib_surface_t *surface)
{
    if (surface->gc)
	return;

    surface->gc = XCreateGC (surface->dpy, surface->drawable, 0, NULL);
}

static cairo_status_t
_cairo_xlib_surface_set_image (void			*abstract_surface,
			       cairo_image_surface_t	*image)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    XImage *ximage;
    unsigned bitmap_pad;

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
			   image->data,
			   image->width,
			   image->height,
			   bitmap_pad,
			   image->stride);
    if (ximage == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    _cairo_xlib_surface_ensure_gc (surface);
    XPutImage(surface->dpy, surface->drawable, surface->gc,
	      ximage, 0, 0, 0, 0,
	      surface->width,
	      surface->height);

    /* Foolish XDestroyImage thinks it can free my data, but I won't
       stand for it. */
    ximage->data = NULL;
    XDestroyImage (ximage);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_matrix (void *abstract_surface, cairo_matrix_t *matrix)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    XTransform xtransform;

    if (!surface->picture)
	return CAIRO_STATUS_SUCCESS;

    xtransform.matrix[0][0] = _cairo_fixed_from_double (matrix->m[0][0]);
    xtransform.matrix[0][1] = _cairo_fixed_from_double (matrix->m[1][0]);
    xtransform.matrix[0][2] = _cairo_fixed_from_double (matrix->m[2][0]);

    xtransform.matrix[1][0] = _cairo_fixed_from_double (matrix->m[0][1]);
    xtransform.matrix[1][1] = _cairo_fixed_from_double (matrix->m[1][1]);
    xtransform.matrix[1][2] = _cairo_fixed_from_double (matrix->m[2][1]);

    xtransform.matrix[2][0] = 0;
    xtransform.matrix[2][1] = 0;
    xtransform.matrix[2][2] = _cairo_fixed_from_double (1);

    if (CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM (surface))
    {
	XRenderSetPictureTransform (surface->dpy, surface->picture, &xtransform);
    } else {
	/* XXX: Need support here if using an old RENDER without support
	   for SetPictureTransform */
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_filter (void *abstract_surface, cairo_filter_t filter)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    char *render_filter;

    if (!(surface->picture 
	  && CAIRO_SURFACE_RENDER_HAS_FILTERS(surface)))
	return CAIRO_STATUS_SUCCESS;
    
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
_cairo_xlib_surface_set_repeat (void *abstract_surface, int repeat)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    unsigned long mask;
    XRenderPictureAttributes pa;

    if (!surface->picture)
	return CAIRO_STATUS_SUCCESS;
    
    mask = CPRepeat;
    pa.repeat = repeat;

    XRenderChangePicture (surface->dpy, surface->picture, mask, &pa);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_xlib_surface_t *
_cairo_xlib_surface_clone_similar (cairo_surface_t	*src,
				   cairo_xlib_surface_t	*template,
				   cairo_format_t	format,
				   int			depth)
{
    cairo_xlib_surface_t *clone;
    cairo_image_surface_t *src_image;

    src_image = _cairo_surface_get_image (src);

    clone = (cairo_xlib_surface_t *)
	_cairo_xlib_surface_create_similar (template, format, 0,
					    src_image->width,
					    src_image->height);
    if (clone == NULL)
	return NULL;

    _cairo_xlib_surface_set_filter (clone, cairo_surface_get_filter(src));

    _cairo_xlib_surface_set_image (clone, src_image);

    _cairo_xlib_surface_set_matrix (clone, &(src_image->base.matrix));

    cairo_surface_destroy (&src_image->base);

    return clone;
}

static int
_render_operator (cairo_operator_t operator)
{
    switch (operator) {
    case CAIRO_OPERATOR_CLEAR:
	return PictOpClear;
    case CAIRO_OPERATOR_SRC:
	return PictOpSrc;
    case CAIRO_OPERATOR_DST:
	return PictOpDst;
    case CAIRO_OPERATOR_OVER:
	return PictOpOver;
    case CAIRO_OPERATOR_OVER_REVERSE:
	return PictOpOverReverse;
    case CAIRO_OPERATOR_IN:
	return PictOpIn;
    case CAIRO_OPERATOR_IN_REVERSE:
	return PictOpInReverse;
    case CAIRO_OPERATOR_OUT:
	return PictOpOut;
    case CAIRO_OPERATOR_OUT_REVERSE:
	return PictOpOutReverse;
    case CAIRO_OPERATOR_ATOP:
	return PictOpAtop;
    case CAIRO_OPERATOR_ATOP_REVERSE:
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
			       cairo_surface_t		*generic_src,
			       cairo_surface_t		*generic_mask,
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
    cairo_xlib_surface_t *dst = abstract_dst;
    cairo_xlib_surface_t *src = (cairo_xlib_surface_t *) generic_src;
    cairo_xlib_surface_t *mask = (cairo_xlib_surface_t *) generic_mask;
    cairo_xlib_surface_t *src_clone = NULL;
    cairo_xlib_surface_t *mask_clone = NULL;
    XGCValues gc_values;
    int src_x_off, src_y_off, dst_x_off, dst_y_off;

    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE (dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (generic_src->backend != dst->base.backend || src->dpy != dst->dpy) {
	src_clone = _cairo_xlib_surface_clone_similar (generic_src, dst,
						       CAIRO_FORMAT_ARGB32, 32);
	if (!src_clone)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	src = src_clone;
    }
    if (generic_mask && (generic_mask->backend != dst->base.backend || mask->dpy != dst->dpy)) {
	mask_clone = _cairo_xlib_surface_clone_similar (generic_mask, dst,
							CAIRO_FORMAT_A8, 8);
	if (!mask_clone)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	mask = mask_clone;
    }

    if (operator == CAIRO_OPERATOR_SRC 
	&& !mask
	&& _cairo_matrix_is_integer_translation(&(src->base.matrix), 
						&src_x_off, &src_y_off)
	&& _cairo_matrix_is_integer_translation(&(dst->base.matrix), 
						&dst_x_off, &dst_y_off)) {
	/* Fast path for copying "raw" areas. */
 	_cairo_xlib_surface_ensure_gc (dst); 
	XGetGCValues(dst->dpy, dst->gc, GCGraphicsExposures, &gc_values);
	XSetGraphicsExposures(dst->dpy, dst->gc, False);
	XCopyArea(dst->dpy, 
		  src->drawable, 
		  dst->drawable, 
		  dst->gc, 
		  src_x + src_x_off, 
		  src_y + src_y_off, 
		  width, height, 
		  dst_x + dst_x_off, 
		  dst_y + dst_y_off);
	XSetGraphicsExposures(dst->dpy, dst->gc, gc_values.graphics_exposures);

    } else {	
    XRenderComposite (dst->dpy,
		      _render_operator (operator),
		      src->picture,
		      mask ? mask->picture : 0,
		      dst->picture,
		      src_x, src_y,
		      mask_x, mask_y,
		      dst_x, dst_y,
		      width, height);
    }

    /* XXX: This is messed up. If I can xlib_surface_create, then I
       should be able to xlib_surface_destroy. */
    if (src_clone)
	cairo_surface_destroy (&src_clone->base);
    if (mask_clone)
	cairo_surface_destroy (&mask_clone->base);

    return CAIRO_STATUS_SUCCESS;
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
					  cairo_surface_t	*generic_src,
					  void			*abstract_dst,
					  int			xSrc,
					  int			ySrc,
					  cairo_trapezoid_t	*traps,
					  int			num_traps)
{
    cairo_xlib_surface_t *dst = abstract_dst;
    cairo_xlib_surface_t *src = (cairo_xlib_surface_t *) generic_src;
    cairo_xlib_surface_t *src_clone = NULL;

    if (!CAIRO_SURFACE_RENDER_HAS_TRAPEZOIDS (dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (generic_src->backend != dst->base.backend || src->dpy != dst->dpy) {
	src_clone = _cairo_xlib_surface_clone_similar (generic_src, dst,
						       CAIRO_FORMAT_ARGB32, 32);
	if (!src_clone)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	src = src_clone;
    }

    /* XXX: The XTrapezoid cast is evil and needs to go away somehow. */
    XRenderCompositeTrapezoids (dst->dpy,
				_render_operator (operator),
				src->picture, dst->picture,
				XRenderFindStandardFormat (dst->dpy, PictStandardA8),
				xSrc, ySrc, (XTrapezoid *) traps, num_traps);

    /* XXX: This is messed up. If I can xlib_surface_create, then I
       should be able to xlib_surface_destroy. */
    if (src_clone)
	cairo_surface_destroy (&src_clone->base);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_copy_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_xlib_surface_show_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_xlib_surface_set_clip_region (void *abstract_surface,
				     pixman_region16_t *region)
{

    Region xregion;
    XRectangle xr;
    XRectangle *rects = NULL;
    XGCValues gc_values;
    pixman_box16_t *box;
    cairo_xlib_surface_t *surf;
    int n, m;

    surf = (cairo_xlib_surface_t *) abstract_surface;

    if (region == NULL) {
	/* NULL region == reset the clip */
	xregion = XCreateRegion();
	xr.x = 0;
	xr.y = 0;
	xr.width = surf->width;
	xr.height = surf->height;
	XUnionRectWithRegion (&xr, xregion, xregion);
	rects = malloc(sizeof(XRectangle));
	if (rects == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
	rects[0] = xr;
	m = 1;

    } else {
	n = pixman_region_num_rects (region);
	/* XXX: Are we sure these are the semantics we want for an
	 * empty, (not null) region? */
	if (n == 0)
	    return CAIRO_STATUS_SUCCESS;
	rects = malloc(sizeof(XRectangle) * n);
	if (rects == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
	box = pixman_region_rects (region);
	xregion = XCreateRegion();
	
	m = n;
	for (; n > 0; --n, ++box) {
	    xr.x = (short) box->x1;
	    xr.y = (short) box->y1;
	    xr.width = (unsigned short) (box->x2 - box->x1);
	    xr.height = (unsigned short) (box->y2 - box->y1);
	    rects[n-1] = xr;
	    XUnionRectWithRegion (&xr, xregion, xregion);
	}    
    }
    
    _cairo_xlib_surface_ensure_gc (surf); 
    XGetGCValues(surf->dpy, surf->gc, GCGraphicsExposures, &gc_values);
    XSetGraphicsExposures(surf->dpy, surf->gc, False);
    XSetClipRectangles(surf->dpy, surf->gc, 0, 0, rects, m, Unsorted);
    free(rects);
    if (surf->picture)
	XRenderSetPictureClipRegion (surf->dpy, surf->picture, xregion);
    XDestroyRegion(xregion);
    XSetGraphicsExposures(surf->dpy, surf->gc, gc_values.graphics_exposures);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_create_pattern (void *abstract_surface,
				    cairo_pattern_t *pattern,
				    cairo_box_t *extents)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_status_t
_cairo_xlib_surface_show_glyphs (cairo_unscaled_font_t  *font,
				 cairo_font_scale_t	*scale,
				 cairo_operator_t       operator,
				 cairo_surface_t        *source,
				 void			*abstract_surface,
				 int                    source_x,
				 int                    source_y,
				 const cairo_glyph_t    *glyphs,
				 int                    num_glyphs);

static const cairo_surface_backend_t cairo_xlib_surface_backend = {
    _cairo_xlib_surface_create_similar,
    _cairo_xlib_surface_destroy,
    _cairo_xlib_surface_pixels_per_inch,
    _cairo_xlib_surface_get_image,
    _cairo_xlib_surface_set_image,
    _cairo_xlib_surface_set_matrix,
    _cairo_xlib_surface_set_filter,
    _cairo_xlib_surface_set_repeat,
    _cairo_xlib_surface_composite,
    _cairo_xlib_surface_fill_rectangles,
    _cairo_xlib_surface_composite_trapezoids,
    _cairo_xlib_surface_copy_page,
    _cairo_xlib_surface_show_page,
    _cairo_xlib_surface_set_clip_region,
    _cairo_xlib_surface_create_pattern,
    _cairo_xlib_surface_show_glyphs
};

static cairo_surface_t *
_cairo_xlib_surface_create_with_size (Display		*dpy,
				      Drawable		drawable,
				      Visual		*visual,
				      cairo_format_t	format,
				      Colormap		colormap,
				      int               width,
				      int               height)
{
    cairo_xlib_surface_t *surface;
    int render_standard;

    surface = malloc (sizeof (cairo_xlib_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (&surface->base, &cairo_xlib_surface_backend);

    surface->visual = visual;
    surface->format = format;

    surface->dpy = dpy;

    surface->gc = 0;
    surface->drawable = drawable;
    surface->owns_pixmap = 0;
    surface->visual = visual;
    surface->width = width;
    surface->height = height;
    
    if (! XRenderQueryVersion (dpy, &surface->render_major, &surface->render_minor)) {
	surface->render_major = -1;
	surface->render_minor = -1;
    }

    switch (format) {
    case CAIRO_FORMAT_A1:
	render_standard = PictStandardA1;
	break;
    case CAIRO_FORMAT_A8:
	render_standard = PictStandardA8;
	break;
    case CAIRO_FORMAT_RGB24:
	render_standard = PictStandardRGB24;
	break;
    case CAIRO_FORMAT_ARGB32:
    default:
	render_standard = PictStandardARGB32;
	break;
    }

    /* XXX: I'm currently ignoring the colormap. Is that bad? */
    if (CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE (surface))
	surface->picture = XRenderCreatePicture (dpy, drawable,
						 visual ?
						 XRenderFindVisualFormat (dpy, visual) :
						 XRenderFindStandardFormat (dpy, render_standard),
						 0, NULL);
    else
	surface->picture = 0;

    return (cairo_surface_t *) surface;
}

cairo_surface_t *
cairo_xlib_surface_create (Display		*dpy,
			   Drawable		drawable,
			   Visual		*visual,
			   cairo_format_t	format,
			   Colormap		colormap)
{
    Window window_ignore;
    unsigned int int_ignore;
    unsigned int width, height;

    /* XXX: This call is a round-trip. We probably want to instead (or
     * also?) export a version that accepts width/height. Then, we'll
     * likely also need a resize function too.
     */
    XGetGeometry(dpy, drawable,
		 &window_ignore, &int_ignore, &int_ignore,
		 &width, &height,
		 &int_ignore, &int_ignore);

    return _cairo_xlib_surface_create_with_size (dpy, drawable, visual, format,
						 colormap, width, height);
}
DEPRECATE (cairo_surface_create_for_drawable, cairo_xlib_surface_create);

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

    if (g == NULL || v == NULL ||g == NULL || im_cache == NULL) {
	_cairo_unlock_global_image_glyph_cache ();
	return CAIRO_STATUS_NO_MEMORY;
    }

    status = _cairo_cache_lookup (im_cache, key, (void **) (&im));
    if (status != CAIRO_STATUS_SUCCESS || im == NULL) {
	_cairo_unlock_global_image_glyph_cache ();
	return CAIRO_STATUS_NO_MEMORY;
    }

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
		      im->image ? im->image->data : NULL,
		      im->image ? v->info.height * v->info.width : 0);

    v->key.base.memory = im->image ? im->image->width * im->image->stride : 0;
    *return_entry = v;
    _cairo_unlock_global_image_glyph_cache ();
    return CAIRO_STATUS_SUCCESS;
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
_cairo_xlib_surface_show_glyphs32 (cairo_unscaled_font_t  *font,
				   cairo_font_scale_t	  *scale,
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
_cairo_xlib_surface_show_glyphs16 (cairo_unscaled_font_t  *font,
				   cairo_font_scale_t	  *scale,
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
_cairo_xlib_surface_show_glyphs8 (cairo_unscaled_font_t  *font,
				  cairo_font_scale_t	  *scale,
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


static cairo_status_t
_cairo_xlib_surface_show_glyphs (cairo_unscaled_font_t  *font,
				 cairo_font_scale_t	*scale,
				 cairo_operator_t       operator,
				 cairo_surface_t        *source,
				 void		        *abstract_surface,
				 int                    source_x,
				 int                    source_y,
				 const cairo_glyph_t    *glyphs,
				 int                    num_glyphs)
{
    unsigned int elt_size;
    cairo_xlib_surface_t *self = abstract_surface;
    cairo_image_surface_t *tmp = NULL;
    cairo_xlib_surface_t *src = NULL;
    glyphset_cache_t *g;
    cairo_status_t status;
    cairo_glyph_cache_key_t key;
    glyphset_cache_entry_t **entries;
    glyphset_cache_entry_t *stack_entries [N_STACK_BUF];
    int i;

    /* Acquire an entry array of suitable size. */
    if (num_glyphs < N_STACK_BUF) {
	entries = stack_entries;

    } else {
	entries = malloc (num_glyphs * sizeof (glyphset_cache_entry_t *));
	if (entries == NULL)
	    goto FAIL;
    }

    /* prep the source surface. */
    if (source->backend == self->base.backend) {
	src = (cairo_xlib_surface_t *) source;

    } else {
	tmp = _cairo_surface_get_image (source);
	if (tmp == NULL)
	    goto FREE_ENTRIES;

	src = (cairo_xlib_surface_t *) 
	    _cairo_surface_create_similar_scratch (&self->base, self->format, 1,
						   tmp->width, tmp->height);

	if (src == NULL)
	    goto FREE_TMP;

	if (_cairo_surface_set_image (&(src->base), tmp) != CAIRO_STATUS_SUCCESS)
	    goto FREE_SRC;	    
    }

    _lock_xlib_glyphset_caches ();
    g = _get_glyphset_cache (self->dpy);
    if (g == NULL)
	goto UNLOCK;

    /* Work out the index size to use. */
    elt_size = 8;
    key.scale = *scale;
    key.unscaled = font;

    for (i = 0; i < num_glyphs; ++i) {
	key.index = glyphs[i].index;
	status = _cairo_cache_lookup (&g->base, &key, (void **) (&entries[i]));
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

    if (elt_size == 8)
	status = _cairo_xlib_surface_show_glyphs8 (font, scale, operator, g, &key, src, self,
						    source_x, source_y, 
						   glyphs, entries, num_glyphs);
    else if (elt_size == 16)
	status = _cairo_xlib_surface_show_glyphs16 (font, scale, operator, g, &key, src, self,
						    source_x, source_y, 
						    glyphs, entries, num_glyphs);
    else 
	status = _cairo_xlib_surface_show_glyphs32 (font, scale, operator, g, &key, src, self,
						    source_x, source_y, 
						    glyphs, entries, num_glyphs);

    _unlock_xlib_glyphset_caches ();

    if (tmp != NULL) {
	cairo_surface_destroy (&(src->base));
	cairo_surface_destroy (&(tmp->base));
    }

    if (num_glyphs >= N_STACK_BUF)
	free (entries); 

    return status;

 UNLOCK:
    _unlock_xlib_glyphset_caches ();

 FREE_SRC:
    cairo_surface_destroy (&(src->base));

 FREE_TMP:
    cairo_surface_destroy (&(tmp->base));

 FREE_ENTRIES:
    if (num_glyphs >= N_STACK_BUF)
	free (entries); 

 FAIL:
    return CAIRO_STATUS_NO_MEMORY;
}

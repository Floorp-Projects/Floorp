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
#include "cairo-xcb.h"

cairo_surface_t *
cairo_xcb_surface_create (XCBConnection		*dpy,
			   XCBDRAWABLE		drawable,
			   XCBVISUALTYPE		*visual,
			   cairo_format_t	format);

#define AllPlanes               ((unsigned long)~0L)

static XCBRenderPICTFORMAT
format_from_visual(XCBConnection *c, XCBVISUALID visual)
{
    static const XCBRenderPICTFORMAT nil = { 0 };
    XCBRenderQueryPictFormatsRep *r;
    XCBRenderPICTSCREENIter si;
    XCBRenderPICTDEPTHIter di;
    XCBRenderPICTVISUALIter vi;

    r = XCBRenderQueryPictFormatsReply(c, XCBRenderQueryPictFormats(c), 0);
    if(!r)
	return nil;

    for(si = XCBRenderQueryPictFormatsScreensIter(r); si.rem; XCBRenderPICTSCREENNext(&si))
	for(di = XCBRenderPICTSCREENDepthsIter(si.data); di.rem; XCBRenderPICTDEPTHNext(&di))
	    for(vi = XCBRenderPICTDEPTHVisualsIter(di.data); vi.rem; XCBRenderPICTVISUALNext(&vi))
		if(vi.data->visual.id == visual.id)
		{
		    XCBRenderPICTFORMAT ret = vi.data->format;
		    free(r);
		    return ret;
		}

    return nil;
}

static XCBRenderPICTFORMAT
format_from_cairo(XCBConnection *c, cairo_format_t fmt)
{
    XCBRenderPICTFORMAT ret = { 0 };
    struct tmpl_t {
	XCBRenderDIRECTFORMAT direct;
	CARD8 depth;
    };
    static const struct tmpl_t templates[] = {
	/* CAIRO_FORMAT_ARGB32 */
	{
	    {
		16, 0xff,
		8,  0xff,
		0,  0xff,
		24, 0xff
	    },
	    32
	},
	/* CAIRO_FORMAT_RGB24 */
	{
	    {
		16, 0xff,
		8,  0xff,
		0,  0xff,
		0,  0x00
	    },
	    24
	},
	/* CAIRO_FORMAT_A8 */
	{
	    {
		0,  0x00,
		0,  0x00,
		0,  0x00,
		0,  0xff
	    },
	    8
	},
	/* CAIRO_FORMAT_A1 */
	{
	    {
		0,  0x00,
		0,  0x00,
		0,  0x00,
		0,  0x01
	    },
	    1
	},
    };
    const struct tmpl_t *tmpl;
    XCBRenderQueryPictFormatsRep *r;
    XCBRenderPICTFORMINFOIter fi;

    if(fmt < 0 || fmt >= (sizeof(templates) / sizeof(*templates)))
	return ret;
    tmpl = templates + fmt;

    r = XCBRenderQueryPictFormatsReply(c, XCBRenderQueryPictFormats(c), 0);
    if(!r)
	return ret;

    for(fi = XCBRenderQueryPictFormatsFormatsIter(r); fi.rem; XCBRenderPICTFORMINFONext(&fi))
    {
	const XCBRenderDIRECTFORMAT *t, *f;
	if(fi.data->type != XCBRenderPictTypeDirect)
	    continue;
	if(fi.data->depth != tmpl->depth)
	    continue;
	t = &tmpl->direct;
	f = &fi.data->direct;
	if(t->red_mask && (t->red_mask != f->red_mask || t->red_shift != f->red_shift))
	    continue;
	if(t->green_mask && (t->green_mask != f->green_mask || t->green_shift != f->green_shift))
	    continue;
	if(t->blue_mask && (t->blue_mask != f->blue_mask || t->blue_shift != f->blue_shift))
	    continue;
	if(t->alpha_mask && (t->alpha_mask != f->alpha_mask || t->alpha_shift != f->alpha_shift))
	    continue;

	ret = fi.data->id;
    }

    free(r);
    return ret;
}

void
cairo_set_target_xcb (cairo_t		*cr,
		      XCBConnection	*dpy,
		      XCBDRAWABLE		drawable,
		      XCBVISUALTYPE	*visual,
		      cairo_format_t	format)
{
    cairo_surface_t *surface;

    if (cr->status && cr->status != CAIRO_STATUS_NO_TARGET_SURFACE)
	    return;

    surface = cairo_xcb_surface_create (dpy, drawable, visual, format);
    if (surface == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	return;
    }

    cairo_set_target_surface (cr, surface);

    /* cairo_set_target_surface takes a reference, so we must destroy ours */
    cairo_surface_destroy (surface);
}

typedef struct cairo_xcb_surface {
    cairo_surface_t base;

    XCBConnection *dpy;
    XCBGCONTEXT gc;
    XCBDRAWABLE drawable;
    int owns_pixmap;
    XCBVISUALTYPE *visual;
    cairo_format_t format;

    int render_major;
    int render_minor;

    int width;
    int height;

    XCBRenderPICTURE picture;
} cairo_xcb_surface_t;

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
_cairo_xcb_surface_create_similar (void		*abstract_src,
				    cairo_format_t	format,
				    int			drawable,
				    int			width,
				    int			height)
{
    cairo_xcb_surface_t *src = abstract_src;
    XCBConnection *dpy = src->dpy;
    XCBDRAWABLE d;
    cairo_xcb_surface_t *surface;

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

    d.pixmap = XCBPIXMAPNew (dpy);
    XCBCreatePixmap (dpy, _CAIRO_FORMAT_DEPTH (format),
		     d.pixmap, src->drawable,
		     width, height);
    
    surface = (cairo_xcb_surface_t *)
	cairo_xcb_surface_create (dpy, d, NULL, format);
    surface->owns_pixmap = 1;

    surface->width = width;
    surface->height = height;

    return &surface->base;
}

static void
_cairo_xcb_surface_destroy (void *abstract_surface)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    if (surface->picture.xid)
	XCBRenderFreePicture (surface->dpy, surface->picture);

    if (surface->owns_pixmap)
	XCBFreePixmap (surface->dpy, surface->drawable.pixmap);

    if (surface->gc.xid)
	XCBFreeGC (surface->dpy, surface->gc);

    surface->dpy = 0;

    free (surface);
}

static double
_cairo_xcb_surface_pixels_per_inch (void *abstract_surface)
{
    /* XXX: We should really get this value from somewhere like Xft.dpy */
    return 96.0;
}

static int
bits_per_pixel(XCBConnection *c, int depth)
{
    XCBFORMAT *fmt = XCBConnSetupSuccessRepPixmapFormats(XCBGetSetup(c));
    XCBFORMAT *fmtend = fmt + XCBConnSetupSuccessRepPixmapFormatsLength(XCBGetSetup(c));

    for(; fmt != fmtend; ++fmt)
	if(fmt->depth == depth)
	    return fmt->bits_per_pixel;

    if(depth <= 4)
	return 4;
    if(depth <= 8)
	return 8;
    if(depth <= 16)
	return 16;
    return 32;
}

static int
bytes_per_line(XCBConnection *c, int width, int bpp)
{
    int bitmap_pad = XCBGetSetup(c)->bitmap_format_scanline_pad;
    return ((bpp * width + bitmap_pad - 1) & -bitmap_pad) >> 3;
}

static cairo_status_t
_get_image_surface (cairo_xcb_surface_t    *surface,
		    cairo_rectangle_t      *interest_rect,
		    cairo_image_surface_t **image_out,
		    cairo_rectangle_t      *image_rect)
{
    cairo_image_surface_t *image;
    XCBGetGeometryRep *geomrep;
    XCBGetImageRep *imagerep;
    int bpp;
    int x1, y1, x2, y2;

    geomrep = XCBGetGeometryReply(surface->dpy, XCBGetGeometry(surface->dpy, surface->drawable), 0);
    if(!geomrep)
	return 0;

    surface->width = geomrep->width;
    surface->height = geomrep->height;
    free(geomrep);

    x1 = 0;
    y1 = 0;
    x2 = surface->width;
    y2 = surface->height;

    if (interest_rect) {
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
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    if (image_rect) {
	image_rect->x = x1;
	image_rect->y = y1;
	image_rect->width = x2 - x1;
	image_rect->height = y2 - y1;
    }

    imagerep = XCBGetImageReply(surface->dpy,
	    XCBGetImage(surface->dpy, ZPixmap,
			surface->drawable,
			x1, y1,
			x2 - x1, y2 - y1,
			AllPlanes), 0);
    if(!imagerep)
	return 0;

    bpp = bits_per_pixel(surface->dpy, imagerep->depth);

    if (surface->visual) {
	cairo_format_masks_t masks;

	/* XXX: Add support here for pictures with external alpha? */

	masks.bpp = bpp;
	masks.alpha_mask = 0;
	masks.red_mask = surface->visual->red_mask;
	masks.green_mask = surface->visual->green_mask;
	masks.blue_mask = surface->visual->blue_mask;

	image = _cairo_image_surface_create_with_masks (XCBGetImageData(imagerep),
							&masks,
							x2 - x1, 
							y2 - y1,
							bytes_per_line(surface->dpy, surface->width, bpp));
    } else {
	image = (cairo_image_surface_t *)
	    cairo_image_surface_create_for_data (XCBGetImageData(imagerep),
						 surface->format,
						 x2 - x1, 
						 y2 - y1,
						 bytes_per_line(surface->dpy, surface->width, bpp));
    }

    /* Let the surface take ownership of the data */
    /* XXX: Can probably come up with a cleaner API here. */
    _cairo_image_surface_assume_ownership_of_data (image);
    /* FIXME: imagerep can't be freed correctly, I think. must copy. :-( */
     
    _cairo_image_surface_set_repeat (image, surface->base.repeat);
    _cairo_image_surface_set_matrix (image, &(surface->base.matrix));

    *image_out = image;
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xcb_surface_ensure_gc (cairo_xcb_surface_t *surface)
{
    if (surface->gc.xid)
	return;

    surface->gc = XCBGCONTEXTNew(surface->dpy);
    XCBCreateGC (surface->dpy, surface->gc, surface->drawable, 0, 0);
}

static cairo_status_t
_draw_image_surface (cairo_xcb_surface_t    *surface,
		     cairo_image_surface_t  *image,
		     int                    dst_x,
		     int                    dst_y)
{
    int bpp, data_len;

    _cairo_xcb_surface_ensure_gc (surface);
    bpp = bits_per_pixel(surface->dpy, image->depth);
    data_len = bytes_per_line(surface->dpy, image->width, bpp) * image->height;
    XCBPutImage(surface->dpy, ZPixmap, surface->drawable, surface->gc,
	      image->width,
	      image->height,
	      dst_x, dst_y,
	      /* left_pad */ 0, image->depth, 
	      data_len, image->data);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xcb_surface_acquire_source_image (void                    *abstract_surface,
					 cairo_image_surface_t  **image_out,
					 void                   **image_extra)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    cairo_status_t status;

    status = _get_image_surface (surface, NULL, &image, NULL);
    if (status == CAIRO_STATUS_SUCCESS) {
	cairo_surface_set_filter (&image->base, surface->base.filter);
	cairo_surface_set_matrix (&image->base, &surface->base.matrix);
	cairo_surface_set_repeat (&image->base, surface->base.repeat);

	*image_out = image;
    }

    return status;
}

static void
_cairo_xcb_surface_release_source_image (void                   *abstract_surface,
					 cairo_image_surface_t  *image,
					 void                   *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_xcb_surface_acquire_dest_image (void                    *abstract_surface,
				       cairo_rectangle_t       *interest_rect,
				       cairo_image_surface_t  **image_out,
				       cairo_rectangle_t       *image_rect_out,
				       void                   **image_extra)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    cairo_status_t status;

    status = _get_image_surface (surface, interest_rect, &image, image_rect_out);
    if (status == CAIRO_STATUS_SUCCESS)
	*image_out = image;

    return status;
}

static void
_cairo_xcb_surface_release_dest_image (void                   *abstract_surface,
				       cairo_rectangle_t      *interest_rect,
				       cairo_image_surface_t  *image,
				       cairo_rectangle_t      *image_rect,
				       void                   *image_extra)
{
    cairo_xcb_surface_t *surface = abstract_surface;

    /* ignore errors */
    _draw_image_surface (surface, image, image_rect->x, image_rect->y);

    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_xcb_surface_clone_similar (void			*abstract_surface,
				  cairo_surface_t	*src,
				  cairo_surface_t     **clone_out)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_xcb_surface_t *clone;

    if (src->backend == surface->base.backend ) {
	cairo_xcb_surface_t *xcb_src = (cairo_xcb_surface_t *)src;

	if (xcb_src->dpy == surface->dpy) {
	    *clone_out = src;
	    cairo_surface_reference (src);
	    
	    return CAIRO_STATUS_SUCCESS;
	}
    } else if (_cairo_surface_is_image (src)) {
	cairo_image_surface_t *image_src = (cairo_image_surface_t *)src;
    
	clone = (cairo_xcb_surface_t *)
	    _cairo_xcb_surface_create_similar (surface, image_src->format, 0,
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
_cairo_xcb_surface_set_matrix (cairo_xcb_surface_t *surface,
			       cairo_matrix_t	   *matrix)
{
    XCBRenderTRANSFORM xtransform;

    if (!surface->picture.xid)
	return CAIRO_STATUS_SUCCESS;

    xtransform.matrix11 = _cairo_fixed_from_double (matrix->m[0][0]);
    xtransform.matrix12 = _cairo_fixed_from_double (matrix->m[1][0]);
    xtransform.matrix13 = _cairo_fixed_from_double (matrix->m[2][0]);

    xtransform.matrix21 = _cairo_fixed_from_double (matrix->m[0][1]);
    xtransform.matrix22 = _cairo_fixed_from_double (matrix->m[1][1]);
    xtransform.matrix23 = _cairo_fixed_from_double (matrix->m[2][1]);

    xtransform.matrix31 = 0;
    xtransform.matrix32 = 0;
    xtransform.matrix33 = _cairo_fixed_from_double (1);

    if (!CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM (surface))
    {
	static const XCBRenderTRANSFORM identity = {
	    1 << 16, 0x00000, 0x00000,
	    0x00000, 1 << 16, 0x00000,
	    0x00000, 0x00000, 1 << 16
	};

	if (memcmp (&xtransform, &identity, sizeof (XCBRenderTRANSFORM)) == 0)
	    return CAIRO_STATUS_SUCCESS;
	
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    
    XCBRenderSetPictureTransform (surface->dpy, surface->picture, xtransform);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xcb_surface_set_filter (cairo_xcb_surface_t *surface,
			       cairo_filter_t	   filter)
{
    char *render_filter;

    if (!surface->picture.xid)
	return CAIRO_STATUS_SUCCESS;
    
    if (!CAIRO_SURFACE_RENDER_HAS_FILTERS (surface))
    {
	if (filter == CAIRO_FILTER_FAST || filter == CAIRO_FILTER_NEAREST)
	    return CAIRO_STATUS_SUCCESS;
	
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    
    switch (filter) {
    case CAIRO_FILTER_FAST:
	render_filter = "fast";
	break;
    case CAIRO_FILTER_GOOD:
	render_filter = "good";
	break;
    case CAIRO_FILTER_BEST:
	render_filter = "best";
	break;
    case CAIRO_FILTER_NEAREST:
	render_filter = "nearest";
	break;
    case CAIRO_FILTER_BILINEAR:
	render_filter = "bilinear";
	break;
    default:
	render_filter = "best";
	break;
    }

    XCBRenderSetPictureFilter(surface->dpy, surface->picture,
			     strlen(render_filter), render_filter, 0, NULL);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xcb_surface_set_repeat (cairo_xcb_surface_t *surface, int repeat)
{
    CARD32 mask = XCBRenderCPRepeat;
    CARD32 pa[] = { repeat };

    if (!surface->picture.xid)
	return CAIRO_STATUS_SUCCESS;

    XCBRenderChangePicture (surface->dpy, surface->picture, mask, pa);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xcb_surface_set_attributes (cairo_xcb_surface_t	      *surface,
				   cairo_surface_attributes_t *attributes)
{
    cairo_int_status_t status;

    status = _cairo_xcb_surface_set_matrix (surface, &attributes->matrix);
    if (status)
	return status;
    
    switch (attributes->extend) {
    case CAIRO_EXTEND_NONE:
	_cairo_xcb_surface_set_repeat (surface, 0);
	break;
    case CAIRO_EXTEND_REPEAT:
	_cairo_xcb_surface_set_repeat (surface, 1);
	break;
    case CAIRO_EXTEND_REFLECT:
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    status = _cairo_xcb_surface_set_filter (surface, attributes->filter);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static int
_render_operator (cairo_operator_t operator)
{
    switch (operator) {
    case CAIRO_OPERATOR_CLEAR:
	return XCBRenderPictOpClear;
    case CAIRO_OPERATOR_SRC:
	return XCBRenderPictOpSrc;
    case CAIRO_OPERATOR_DST:
	return XCBRenderPictOpDst;
    case CAIRO_OPERATOR_OVER:
	return XCBRenderPictOpOver;
    case CAIRO_OPERATOR_OVER_REVERSE:
	return XCBRenderPictOpOverReverse;
    case CAIRO_OPERATOR_IN:
	return XCBRenderPictOpIn;
    case CAIRO_OPERATOR_IN_REVERSE:
	return XCBRenderPictOpInReverse;
    case CAIRO_OPERATOR_OUT:
	return XCBRenderPictOpOut;
    case CAIRO_OPERATOR_OUT_REVERSE:
	return XCBRenderPictOpOutReverse;
    case CAIRO_OPERATOR_ATOP:
	return XCBRenderPictOpAtop;
    case CAIRO_OPERATOR_ATOP_REVERSE:
	return XCBRenderPictOpAtopReverse;
    case CAIRO_OPERATOR_XOR:
	return XCBRenderPictOpXor;
    case CAIRO_OPERATOR_ADD:
	return XCBRenderPictOpAdd;
    case CAIRO_OPERATOR_SATURATE:
	return XCBRenderPictOpSaturate;
    default:
	return XCBRenderPictOpOver;
    }
}

static cairo_int_status_t
_cairo_xcb_surface_composite (cairo_operator_t		operator,
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
    cairo_xcb_surface_t		*dst = abstract_dst;
    cairo_xcb_surface_t		*src;
    cairo_xcb_surface_t		*mask;
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
    
    status = _cairo_xcb_surface_set_attributes (src, &src_attr);
    if (CAIRO_OK (status))
    {
	if (mask)
	{
	    status = _cairo_xcb_surface_set_attributes (mask, &mask_attr);
	    if (CAIRO_OK (status))
		XCBRenderComposite (dst->dpy,
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
	}
	else
	{
	    static XCBRenderPICTURE maskpict = { 0 };
	    
	    XCBRenderComposite (dst->dpy,
				_render_operator (operator),
				src->picture,
				maskpict,
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
_cairo_xcb_surface_fill_rectangles (void			*abstract_surface,
				     cairo_operator_t		operator,
				     const cairo_color_t	*color,
				     cairo_rectangle_t		*rects,
				     int			num_rects)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    XCBRenderCOLOR render_color;

    if (!CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLE (surface))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    render_color.red   = color->red_short;
    render_color.green = color->green_short;
    render_color.blue  = color->blue_short;
    render_color.alpha = color->alpha_short;

    /* XXX: This XCBRECTANGLE cast is evil... it needs to go away somehow. */
    XCBRenderFillRectangles (surface->dpy,
			   _render_operator (operator),
			   surface->picture,
			   render_color, num_rects, (XCBRECTANGLE *) rects);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xcb_surface_composite_trapezoids (cairo_operator_t	operator,
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
    cairo_xcb_surface_t		*dst = abstract_dst;
    cairo_xcb_surface_t		*src;
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
    /* XXX: format_from_cairo is slow. should cache something. */
    status = _cairo_xcb_surface_set_attributes (src, &attributes);
    if (CAIRO_OK (status))
	XCBRenderTrapezoids (dst->dpy,
			     _render_operator (operator),
			     src->picture, dst->picture,
			     format_from_cairo (dst->dpy, CAIRO_FORMAT_A8),
			     render_src_x + attributes.x_offset, 
			     render_src_y + attributes.y_offset,
			     num_traps, (XCBRenderTRAP *) traps);

    _cairo_pattern_release_surface (&dst->base, &src->base, &attributes);

    return status;
}

static cairo_int_status_t
_cairo_xcb_surface_copy_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_xcb_surface_show_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_xcb_surface_set_clip_region (void *abstract_surface,
				    pixman_region16_t *region)
{
    /* FIXME */
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static const cairo_surface_backend_t cairo_xcb_surface_backend = {
    _cairo_xcb_surface_create_similar,
    _cairo_xcb_surface_destroy,
    _cairo_xcb_surface_pixels_per_inch,
    _cairo_xcb_surface_acquire_source_image,
    _cairo_xcb_surface_release_source_image,
    _cairo_xcb_surface_acquire_dest_image,
    _cairo_xcb_surface_release_dest_image,
    _cairo_xcb_surface_clone_similar,
    _cairo_xcb_surface_composite,
    _cairo_xcb_surface_fill_rectangles,
    _cairo_xcb_surface_composite_trapezoids,
    _cairo_xcb_surface_copy_page,
    _cairo_xcb_surface_show_page,
    _cairo_xcb_surface_set_clip_region,
    NULL /* show_glyphs */
};

static void
query_render_version (XCBConnection *c, cairo_xcb_surface_t *surface)
{
    XCBRenderQueryVersionRep *r;

    surface->render_major = -1;
    surface->render_minor = -1;

    if (!XCBRenderInit(c))
	return;

    r = XCBRenderQueryVersionReply(c, XCBRenderQueryVersion(c, 0, 6), 0);
    if (!r)
	return;

    surface->render_major = r->major_version;
    surface->render_minor = r->minor_version;
    free(r);
}

cairo_surface_t *
cairo_xcb_surface_create (XCBConnection		*dpy,
			   XCBDRAWABLE		drawable,
			   XCBVISUALTYPE	*visual,
			   cairo_format_t	format)
{
    cairo_xcb_surface_t *surface;

    surface = malloc (sizeof (cairo_xcb_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (&surface->base, &cairo_xcb_surface_backend);

    surface->dpy = dpy;
    surface->gc.xid = 0;
    surface->drawable = drawable;
    surface->owns_pixmap = 0;
    surface->visual = visual;
    surface->format = format;

    query_render_version(dpy, surface);

    if (CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE (surface))
    {
	XCBRenderPICTFORMAT fmt;
	if(visual)
	    fmt = format_from_visual (dpy, visual->visual_id);
	else
	    fmt = format_from_cairo (dpy, format);
	surface->picture = XCBRenderPICTURENew(dpy);
	XCBRenderCreatePicture (dpy, surface->picture, drawable,
				fmt, 0, NULL);
    }
    else
	surface->picture.xid = 0;

    return (cairo_surface_t *) surface;
}

/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Intel Corporation
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
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-boxes-private.h"
#include "cairo-xcb-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-surface-backend-private.h"

/* XXX dithering */

typedef struct _cairo_xcb_pixmap {
    cairo_surface_t base;

    cairo_xcb_connection_t *connection;
    cairo_xcb_screen_t *screen;

    cairo_surface_t *owner;
    xcb_pixmap_t pixmap;
    int width;
    int height;
    int depth;
    int x0, y0;
    cairo_bool_t repeat;
} cairo_xcb_pixmap_t;

static cairo_status_t
_cairo_xcb_pixmap_finish (void *abstract_surface)
{
    cairo_xcb_pixmap_t *surface = abstract_surface;
    cairo_status_t status;

    if (surface->owner != NULL) {
	cairo_surface_destroy (surface->owner);
    } else {
	status = _cairo_xcb_connection_acquire (surface->connection);
	if (unlikely (status))
	    return status;

	_cairo_xcb_connection_free_pixmap (surface->connection,
					   surface->pixmap);
	_cairo_xcb_connection_release (surface->connection);
    }

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t _cairo_xcb_pixmap_backend = {
    CAIRO_SURFACE_TYPE_XCB,
    _cairo_xcb_pixmap_finish,
};

static cairo_xcb_pixmap_t *
_cairo_xcb_pixmap_create (cairo_xcb_surface_t *target,
			  int width, int height)
{
    cairo_xcb_pixmap_t *surface;

    surface = _cairo_malloc (sizeof (cairo_xcb_pixmap_t));
    if (unlikely (surface == NULL))
	return (cairo_xcb_pixmap_t *)
	    _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base,
			 &_cairo_xcb_pixmap_backend,
			 NULL,
			 target->base.content,
			 FALSE); /* is_vector */

    surface->connection = target->connection;
    surface->screen = target->screen;
    surface->owner = NULL;
    surface->width = width;
    surface->height = height;
    surface->depth = target->depth;
    surface->x0 = surface->y0 = 0;
    surface->repeat = FALSE;

    surface->pixmap =
	_cairo_xcb_connection_create_pixmap (surface->connection,
					     surface->depth,
					     target->drawable,
					     width, height);

    return surface;
}

static cairo_xcb_pixmap_t *
_cairo_xcb_pixmap_copy (cairo_xcb_surface_t *target)
{
    cairo_xcb_pixmap_t *surface;

    surface = _cairo_malloc (sizeof (cairo_xcb_pixmap_t));
    if (unlikely (surface == NULL))
	return (cairo_xcb_pixmap_t *)
	    _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base,
			 &_cairo_xcb_pixmap_backend,
			 NULL,
			 target->base.content,
			 FALSE); /* is_vector */

    surface->connection = target->connection;
    surface->screen = target->screen;
    surface->pixmap = target->drawable;
    surface->owner = cairo_surface_reference (&target->base);
    surface->width = target->width;
    surface->height = target->height;
    surface->depth = target->depth;
    surface->x0 = surface->y0 = 0;
    surface->repeat = FALSE;

    return surface;
}

#if CAIRO_HAS_XCB_SHM_FUNCTIONS
static cairo_status_t
_cairo_xcb_shm_image_create_shm (cairo_xcb_connection_t *connection,
				 pixman_format_code_t pixman_format,
				 int width, int height,
				 cairo_image_surface_t **image_out,
				 cairo_xcb_shm_info_t **shm_info_out)
{
    cairo_surface_t *image = NULL;
    cairo_xcb_shm_info_t *shm_info = NULL;
    cairo_status_t status;
    size_t size, stride;

    if (! (connection->flags & CAIRO_XCB_HAS_SHM))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (unlikely (width > XLIB_COORD_MAX || height > XLIB_COORD_MAX))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    stride = CAIRO_STRIDE_FOR_WIDTH_BPP (width, PIXMAN_FORMAT_BPP (pixman_format));
    size = stride * height;
    if (size <= CAIRO_XCB_SHM_SMALL_IMAGE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_xcb_connection_allocate_shm_info (connection, size,
						      FALSE, &shm_info);
    if (unlikely (status))
	return status;

    image = _cairo_image_surface_create_with_pixman_format (shm_info->mem,
							    pixman_format,
							    width, height,
							    stride);
    status = image->status;
    if (unlikely (status)) {
	_cairo_xcb_shm_info_destroy (shm_info);
	return status;
    }

    status = _cairo_user_data_array_set_data (&image->user_data,
					      (const cairo_user_data_key_t *) connection,
					      shm_info,
					      (cairo_destroy_func_t) _cairo_xcb_shm_info_destroy);

    if (unlikely (status)) {
	cairo_surface_destroy (image);
	_cairo_xcb_shm_info_destroy (shm_info);
	return status;
    }

    *image_out = (cairo_image_surface_t *) image;
    *shm_info_out = shm_info;
    return CAIRO_STATUS_SUCCESS;
}
#else
static cairo_status_t
_cairo_xcb_shm_image_create_shm (cairo_xcb_connection_t *connection,
				 pixman_format_code_t pixman_format,
				 int width, int height,
				 cairo_image_surface_t **image_out,
				 cairo_xcb_shm_info_t **shm_info_out)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}
#endif

cairo_status_t
_cairo_xcb_shm_image_create (cairo_xcb_connection_t *connection,
			     pixman_format_code_t pixman_format,
			     int width, int height,
			     cairo_image_surface_t **image_out,
			     cairo_xcb_shm_info_t **shm_info_out)
{
    cairo_surface_t *image = NULL;
    cairo_xcb_shm_info_t *shm_info = NULL;
    cairo_status_t status;

    status = _cairo_xcb_shm_image_create_shm (connection,
					      pixman_format,
					      width,
					      height,
					      image_out,
					      shm_info_out);

    if (status != CAIRO_STATUS_SUCCESS) {
	image = _cairo_image_surface_create_with_pixman_format (NULL,
								pixman_format,
								width, height,
								0);
	status = image->status;
	if (unlikely (status))
	    return status;

	*image_out = (cairo_image_surface_t *) image;
	*shm_info_out = shm_info;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_xcb_pixmap_t *
_pixmap_from_image (cairo_xcb_surface_t *target,
		    xcb_render_pictformat_t format,
		    cairo_image_surface_t *image,
		    cairo_xcb_shm_info_t *shm_info)
{
    xcb_gcontext_t gc;
    cairo_xcb_pixmap_t *pixmap;

    pixmap = _cairo_xcb_pixmap_create (target,
				       image->width,
				       image->height);
    if (unlikely (pixmap->base.status))
	return pixmap;

    gc = _cairo_xcb_screen_get_gc (target->screen, pixmap->pixmap, image->depth);

    if (shm_info != NULL) {
	_cairo_xcb_connection_shm_put_image (target->connection,
					     pixmap->pixmap, gc,
					     image->width, image->height,
					     0, 0,
					     image->width, image->height,
					     0, 0,
					     image->depth,
					     shm_info->shm,
					     shm_info->offset);
    } else {
	int len;

	/* Do we need to trim the image? */
	len = CAIRO_STRIDE_FOR_WIDTH_BPP (image->width,
					  PIXMAN_FORMAT_BPP (image->pixman_format));
	if (len == image->stride) {
	    _cairo_xcb_connection_put_image (target->connection,
					     pixmap->pixmap, gc,
					     image->width, image->height,
					     0, 0,
					     image->depth,
					     image->stride,
					     image->data);
	} else {
	    _cairo_xcb_connection_put_subimage (target->connection,
						pixmap->pixmap, gc,
						0, 0,
						image->width, image->height,
						PIXMAN_FORMAT_BPP (image->pixman_format) / 8,
						image->stride,
						0, 0,
						image->depth,
						image->data);

	}
    }

    _cairo_xcb_screen_put_gc (target->screen, image->depth, gc);

    return pixmap;
}

static cairo_xcb_pixmap_t *
_render_to_pixmap (cairo_xcb_surface_t *target,
		   const cairo_pattern_t *pattern,
		   const cairo_rectangle_int_t *extents)
{
    cairo_image_surface_t *image;
    cairo_xcb_shm_info_t *shm_info;
    cairo_pattern_union_t copy;
    cairo_status_t status;
    cairo_xcb_pixmap_t *pixmap;

    status = _cairo_xcb_shm_image_create (target->screen->connection,
					  target->pixman_format,
					  extents->width, extents->height,
					  &image, &shm_info);
    if (unlikely (status))
	return (cairo_xcb_pixmap_t *) _cairo_surface_create_in_error (status);

    _cairo_pattern_init_static_copy (&copy.base, pattern);
    cairo_matrix_translate (&copy.base.matrix, -extents->x, -extents->y);
    status = _cairo_surface_paint (&image->base,
				   CAIRO_OPERATOR_SOURCE,
				   &copy.base,
				   NULL);
    if (unlikely (status)) {
	cairo_surface_destroy (&image->base);
	return (cairo_xcb_pixmap_t *) _cairo_surface_create_in_error (status);
    }

    pixmap = _pixmap_from_image (target, target->xrender_format, image, shm_info);
    cairo_surface_destroy (&image->base);

    if (unlikely (pixmap->base.status))
	return pixmap;

    pixmap->x0 = -extents->x;
    pixmap->y0 = -extents->y;
    return pixmap;
}

static cairo_xcb_pixmap_t *
_copy_to_pixmap (cairo_xcb_surface_t *source)
{
    cairo_xcb_pixmap_t *pixmap;

    /* If the source may be a window, we need to copy it and its children
     * via a temporary pixmap so that we can IncludeInferiors on the source
     * and use ClipByChildren on the destination.
     */
    if (source->owns_pixmap) {
	pixmap = _cairo_xcb_pixmap_copy (source);
	if (unlikely (pixmap->base.status))
	    return pixmap;
    } else {
	uint32_t values[1];
	xcb_gcontext_t gc;

	pixmap = _cairo_xcb_pixmap_create (source,
					   source->width,
					   source->height);
	if (unlikely (pixmap->base.status))
	    return pixmap;

	gc = _cairo_xcb_screen_get_gc (source->screen,
				       pixmap->pixmap,
				       pixmap->depth);

	values[0] = TRUE;
	_cairo_xcb_connection_change_gc (pixmap->connection, gc,
					 XCB_GC_SUBWINDOW_MODE, values);

	_cairo_xcb_connection_copy_area (pixmap->connection,
					 source->drawable,
					 pixmap->pixmap, gc,
					 0, 0,
					 0, 0,
					 source->width,
					 source->height);

	values[0] = FALSE;
	_cairo_xcb_connection_change_gc (pixmap->connection, gc,
					 XCB_GC_SUBWINDOW_MODE, values);

	_cairo_xcb_screen_put_gc (source->screen,
				  pixmap->depth,
				  gc);
    }

    return pixmap;
}
static cairo_xcb_pixmap_t *
_cairo_xcb_surface_pixmap (cairo_xcb_surface_t *target,
			   const cairo_surface_pattern_t *pattern,
			   const cairo_rectangle_int_t *extents,
			   int tx, int ty)
{
    cairo_surface_t *source;
    cairo_xcb_pixmap_t *pixmap;

    source =  pattern->surface;
    pixmap = (cairo_xcb_pixmap_t *)
	_cairo_surface_has_snapshot (source, &_cairo_xcb_pixmap_backend);
    if (pixmap != NULL && pixmap->screen == target->screen)
	return (cairo_xcb_pixmap_t *) cairo_surface_reference (&pixmap->base);

    if (_cairo_surface_is_xcb(source) &&
	((cairo_xcb_surface_t *) source)->screen == target->screen)
    {
	cairo_xcb_surface_t *xcb_source = (cairo_xcb_surface_t *) source;

	if (xcb_source->depth == target->depth)
	    pixmap = _copy_to_pixmap (xcb_source);
    }
#if CAIRO_HAS_XLIB_XCB_FUNCTIONS
    else if (source->type == CAIRO_SURFACE_TYPE_XLIB &&
	     ((cairo_xlib_xcb_surface_t *) source)->xcb->screen == target->screen)
    {
	cairo_xcb_surface_t *xcb_source = ((cairo_xlib_xcb_surface_t *) source)->xcb;

	if (xcb_source->depth == target->depth)
	    pixmap = _copy_to_pixmap (xcb_source);
    }
#endif

    if (pixmap == NULL) {
	cairo_rectangle_int_t rect;

	if (! _cairo_surface_get_extents (source, &rect)) {
	    rect.x = rect.y = 0;
	    rect.width  = target->width;
	    rect.height = target->height;
	}

	pixmap = _render_to_pixmap (target, &pattern->base, &rect);
    }

    if (unlikely (pixmap->base.status))
	return pixmap;

    _cairo_surface_attach_snapshot (source, &pixmap->base, NULL);

    if (pattern->base.extend != CAIRO_EXTEND_NONE) {
	if (extents->x < 0 || extents->y < 0 ||
	    extents->x + extents->width  > pixmap->width ||
	    extents->y + extents->height > pixmap->height)
	{
	    pixmap->repeat = TRUE;
	}
    }

    pixmap->x0 += tx;
    pixmap->y0 += ty;

    return pixmap;
}

static cairo_xcb_pixmap_t *
_cairo_xcb_pixmap_for_pattern (cairo_xcb_surface_t *target,
			       const cairo_pattern_t *pattern,
			       const cairo_rectangle_int_t *extents)
{
    int tx, ty;

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SURFACE:
	/* Core can only perform a native, unscaled blit, but can handle tiles */
	if (_cairo_matrix_is_integer_translation (&pattern->matrix, &tx, &ty)) {
	    switch (pattern->extend) {
	    case CAIRO_EXTEND_NONE:
	    case CAIRO_EXTEND_REPEAT:
		return _cairo_xcb_surface_pixmap (target,
						  (cairo_surface_pattern_t *) pattern,
						  extents, tx, ty);

	    default:
	    case CAIRO_EXTEND_PAD:
	    case CAIRO_EXTEND_REFLECT:
		break;
	    }
	}
	/* fallthrough */
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
    case CAIRO_PATTERN_TYPE_MESH:
    case CAIRO_PATTERN_TYPE_RASTER_SOURCE:
	return _render_to_pixmap (target, pattern, extents);

    default:
    case CAIRO_PATTERN_TYPE_SOLID:
	ASSERT_NOT_REACHED;
	return NULL;
    }
}

cairo_status_t
_cairo_xcb_surface_core_copy_boxes (cairo_xcb_surface_t		*dst,
				   const cairo_pattern_t	*src_pattern,
				   const cairo_rectangle_int_t	*extents,
				   const cairo_boxes_t		*boxes)
{
    cairo_xcb_pixmap_t *src;
    const struct _cairo_boxes_chunk *chunk;
    xcb_gcontext_t gc;
    cairo_status_t status;

    status = _cairo_xcb_connection_acquire (dst->connection);
    if (unlikely (status))
	return status;

    src = _cairo_xcb_pixmap_for_pattern (dst, src_pattern, extents);
    status = src->base.status;
    if (unlikely (status))
	goto CLEANUP_CONNECTION;

    assert (src->depth == dst->depth);

    gc = _cairo_xcb_screen_get_gc (dst->screen, src->pixmap, src->depth);

    if (src->repeat) {
	uint32_t mask =
	    XCB_GC_FILL_STYLE |
	    XCB_GC_TILE |
	    XCB_GC_TILE_STIPPLE_ORIGIN_X |
	    XCB_GC_TILE_STIPPLE_ORIGIN_Y;
	uint32_t values[] = {
	    XCB_FILL_STYLE_TILED,
	    src->pixmap,
	    - src->x0, - src->y0,
	};
	xcb_rectangle_t *xcb_rects;

	_cairo_xcb_connection_change_gc (dst->connection, gc, mask, values);

	for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	    int i;

	    xcb_rects = (xcb_rectangle_t *) chunk->base;

	    for (i = 0; i < chunk->count; i++) {
		int x1 = _cairo_fixed_integer_round (chunk->base[i].p1.x);
		int x2 = _cairo_fixed_integer_round (chunk->base[i].p2.x);
		int y1 = _cairo_fixed_integer_round (chunk->base[i].p1.y);
		int y2 = _cairo_fixed_integer_round (chunk->base[i].p2.y);

		xcb_rects[i].x = x1;
		xcb_rects[i].y = y1;
		xcb_rects[i].width  = x2 - x1;
		xcb_rects[i].height = y2 - y1;
	    }
	    _cairo_xcb_connection_poly_fill_rectangle (dst->connection,
						       dst->drawable,
						       gc, chunk->count, xcb_rects);
	}

	values[0] = 0;
	_cairo_xcb_connection_change_gc (dst->connection, gc, XCB_GC_FILL_STYLE, values);
    } else {
	for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	    int i;

	    for (i = 0; i < chunk->count; i++) {
		int x1 = _cairo_fixed_integer_round (chunk->base[i].p1.x);
		int x2 = _cairo_fixed_integer_round (chunk->base[i].p2.x);
		int y1 = _cairo_fixed_integer_round (chunk->base[i].p1.y);
		int y2 = _cairo_fixed_integer_round (chunk->base[i].p2.y);

		_cairo_xcb_connection_copy_area (dst->connection,
						 src->pixmap,
						 dst->drawable, gc,
						 src->x0 + x1,
						 src->y0 + y1,
						 x1, y1,
						 x2 - x1, y2 - y1);
	    }
	}
    }

    _cairo_xcb_screen_put_gc (dst->screen, src->depth, gc);
    cairo_surface_destroy (&src->base);

  CLEANUP_CONNECTION:
    _cairo_xcb_connection_release (dst->connection);

    return status;
}

cairo_status_t
_cairo_xcb_surface_core_fill_boxes (cairo_xcb_surface_t *dst,
				    const cairo_color_t	*color,
				    cairo_boxes_t *boxes)
{
    struct _cairo_boxes_chunk *chunk;
    xcb_gcontext_t gc;
    cairo_status_t status;

    status = _cairo_xcb_connection_acquire (dst->connection);
    if (unlikely (status))
	return status;

    gc = _cairo_xcb_screen_get_gc (dst->screen, dst->drawable, dst->depth);

#if 0
    xcb_pixmap_t source;

    source = _dither_source (dst, color);
    XSetTSOrigin (surface->dpy, gc, 0, 0);
    XSetTile (surface->dpy, gc, source);
#endif

    for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	xcb_rectangle_t *xcb_rects;
	int i;

	xcb_rects = (xcb_rectangle_t *) chunk->base;
	for (i = 0; i < chunk->count; i++) {
	    int x1 = _cairo_fixed_integer_round (chunk->base[i].p1.x);
	    int x2 = _cairo_fixed_integer_round (chunk->base[i].p2.x);
	    int y1 = _cairo_fixed_integer_round (chunk->base[i].p1.y);
	    int y2 = _cairo_fixed_integer_round (chunk->base[i].p2.y);

	    xcb_rects[i].x = x1;
	    xcb_rects[i].y = y1;
	    xcb_rects[i].width  = x2 - x1;
	    xcb_rects[i].height = y2 - y1;
	}

	_cairo_xcb_connection_poly_fill_rectangle (dst->connection,
						   dst->drawable, gc,
						   chunk->count, xcb_rects);
    }

    _cairo_xcb_screen_put_gc (dst->screen, dst->depth, gc);
    _cairo_xcb_connection_release (dst->connection);

    return CAIRO_STATUS_SUCCESS;
}

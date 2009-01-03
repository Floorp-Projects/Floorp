/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright � 2008 Mozilla Corporation
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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 *
 * Contributor(s):
 *	Vladimir Vukicevic <vladimir@mozilla.com>
 */

#include "cairoint.h"

#include "cairo-quartz-image.h"
#include "cairo-quartz-private.h"

#define SURFACE_ERROR_NO_MEMORY (_cairo_surface_create_in_error(_cairo_error(CAIRO_STATUS_NO_MEMORY)))
#define SURFACE_ERROR_TYPE_MISMATCH (_cairo_surface_create_in_error(_cairo_error(CAIRO_STATUS_SURFACE_TYPE_MISMATCH)))
#define SURFACE_ERROR_INVALID_FORMAT (_cairo_surface_create_in_error(_cairo_error(CAIRO_STATUS_INVALID_FORMAT)))

static void
DataProviderReleaseCallback (void *info, const void *data, size_t size)
{
    cairo_surface_t *surface = (cairo_surface_t *) info;
    cairo_surface_destroy (surface);
}

static cairo_surface_t *
_cairo_quartz_image_surface_create_similar (void *asurface,
					    cairo_content_t content,
					    int width,
					    int height)
{
    cairo_surface_t *result;
    cairo_surface_t *isurf = cairo_image_surface_create (_cairo_format_from_content (content),
							 width,
							 height);
    if (cairo_surface_status(isurf))
	return isurf;

    result = cairo_quartz_image_surface_create (isurf);
    cairo_surface_destroy (isurf);

    return result;
}

static cairo_status_t
_cairo_quartz_image_surface_finish (void *asurface)
{
    cairo_quartz_image_surface_t *surface = (cairo_quartz_image_surface_t *) asurface;

    /* the imageSurface will be destroyed by the data provider's release callback */
    CGImageRelease (surface->image);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_quartz_image_surface_acquire_source_image (void *asurface,
						  cairo_image_surface_t **image_out,
						  void **image_extra)
{
    cairo_quartz_image_surface_t *surface = (cairo_quartz_image_surface_t *) asurface;

    *image_out = surface->imageSurface;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_quartz_image_surface_acquire_dest_image (void *asurface,
						cairo_rectangle_int_t *interest_rect,
						cairo_image_surface_t **image_out,
						cairo_rectangle_int_t *image_rect,
						void **image_extra)
{
    cairo_quartz_image_surface_t *surface = (cairo_quartz_image_surface_t *) asurface;

    *image_out = surface->imageSurface;
    *image_rect = surface->extents;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
   
}

static cairo_int_status_t
_cairo_quartz_image_surface_get_extents (void *asurface,
					 cairo_rectangle_int_t *extents)
{
    cairo_quartz_image_surface_t *surface = (cairo_quartz_image_surface_t *) asurface;

    *extents = surface->extents;

    return CAIRO_STATUS_SUCCESS;
}

/* we assume some drawing happened to the image buffer; make sure it's
 * represented in the CGImage on flush()
 */

static cairo_status_t
_cairo_quartz_image_surface_flush (void *asurface)
{
    cairo_quartz_image_surface_t *surface = (cairo_quartz_image_surface_t *) asurface;
    CGImageRef oldImage = surface->image;
    CGImageRef newImage = NULL;

    /* To be released by the ReleaseCallback */
    cairo_surface_reference ((cairo_surface_t*) surface->imageSurface);

    newImage = _cairo_quartz_create_cgimage (surface->imageSurface->format,
					     surface->imageSurface->width,
					     surface->imageSurface->height,
					     surface->imageSurface->stride,
					     surface->imageSurface->data,
					     TRUE,
					     NULL,
					     DataProviderReleaseCallback,
					     surface->imageSurface);

    surface->image = newImage;
    CGImageRelease (oldImage);

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_quartz_image_surface_backend = {
    CAIRO_SURFACE_TYPE_QUARTZ_IMAGE,
    _cairo_quartz_image_surface_create_similar,
    _cairo_quartz_image_surface_finish,
    _cairo_quartz_image_surface_acquire_source_image,
    NULL, /* release_source_image */
    _cairo_quartz_image_surface_acquire_dest_image,
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    NULL, /* show_page */
    NULL, /* set_clip_region */
    NULL, /* intersect_clip_path */
    _cairo_quartz_image_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    _cairo_quartz_image_surface_flush,
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
    NULL, /* surface_show_glyphs */
    NULL, /* snapshot */
    NULL, /* is_similar */
    NULL, /* reset */
    NULL  /* fill_stroke */

};

/**
 * cairo_quartz_image_surface_create
 * @surface: a cairo image surface to wrap with a quartz image surface
 *
 * Creates a Quartz surface backed by a CGImageRef that references the
 * given image surface. The resulting surface can be rendered quickly
 * when used as a source when rendering to a #cairo_quartz_surface.  If
 * the data in the image surface is ever updated, cairo_surface_flush()
 * must be called on the #cairo_quartz_image_surface to ensure that the
 * CGImageRef refers to the updated data.
 *
 * Return value: the newly created surface.
 *
 * Since: 1.6
 */
cairo_surface_t *
cairo_quartz_image_surface_create (cairo_surface_t *surface)
{
    cairo_quartz_image_surface_t *qisurf;

    CGImageRef image;

    cairo_image_surface_t *image_surface;
    int width, height, stride;
    cairo_format_t format;
    unsigned char *data;

    if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE)
	return SURFACE_ERROR_TYPE_MISMATCH;

    image_surface = (cairo_image_surface_t*) surface;
    width = image_surface->width;
    height = image_surface->height;
    stride = image_surface->stride;
    format = image_surface->format;
    data = image_surface->data;

    if (!_cairo_quartz_verify_surface_size(width, height))
	return SURFACE_ERROR_NO_MEMORY;

    if (width == 0 || height == 0)
	return SURFACE_ERROR_NO_MEMORY;

    if (format != CAIRO_FORMAT_ARGB32 && format != CAIRO_FORMAT_RGB24)
	return SURFACE_ERROR_INVALID_FORMAT;

    qisurf = malloc(sizeof(cairo_quartz_image_surface_t));
    if (qisurf == NULL)
	return SURFACE_ERROR_NO_MEMORY;

    memset (qisurf, 0, sizeof(cairo_quartz_image_surface_t));

    /* In case the create_cgimage fails, this ref will
     * be released via the callback (which will be called in
     * case of failure.)
     */
    cairo_surface_reference (surface);

    image = _cairo_quartz_create_cgimage (format,
					  width, height,
					  stride,
					  data,
					  TRUE,
					  NULL,
					  DataProviderReleaseCallback,
					  image_surface);

    if (!image) {
	free (qisurf);
	return SURFACE_ERROR_NO_MEMORY;
    }

    _cairo_surface_init (&qisurf->base,
			 &cairo_quartz_image_surface_backend,
			 _cairo_content_from_format (format));

    qisurf->extents.x = qisurf->extents.y = 0;
    qisurf->extents.width = width;
    qisurf->extents.height = height;

    qisurf->image = image;
    qisurf->imageSurface = image_surface;

    return &qisurf->base;
}


cairo_surface_t *
cairo_quartz_image_surface_get_image (cairo_surface_t *asurface)
{
    cairo_quartz_image_surface_t *surface = (cairo_quartz_image_surface_t*) asurface;

    if (cairo_surface_get_type(asurface) != CAIRO_SURFACE_TYPE_QUARTZ_IMAGE)
	return NULL;

    return (cairo_surface_t*) surface->imageSurface;
}

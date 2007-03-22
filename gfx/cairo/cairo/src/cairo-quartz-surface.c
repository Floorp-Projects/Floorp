/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Calum Robinson
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
 * The Initial Developer of the Original Code is Calum Robinson
 *
 * Contributor(s):
 *    Calum Robinson <calumr@mac.com>
 */

#include "cairoint.h"
#include "cairo-private.h"
#include "cairo-quartz-private.h"

static cairo_status_t
_cairo_quartz_surface_finish(void *abstract_surface)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    if (surface->clip_region)
      pixman_region_destroy (surface->clip_region);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_quartz_surface_acquire_source_image(void *abstract_surface,
					   cairo_image_surface_t **image_out,
					   void **image_extra)
{
  cairo_quartz_surface_t *surface = abstract_surface;

#if 0
  if (CGBitmapContextGetBitmapInfo (surface->context) != 0) {
    /* XXX: We can create an image out of the bitmap here */
  }
#endif

  return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_status_t
_cairo_quartz_surface_acquire_dest_image(void                    *abstract_surface,
                                         cairo_rectangle_int16_t *interest_rect,
                                         cairo_image_surface_t  **image_out,
                                         cairo_rectangle_int16_t *image_rect,
                                         void                   **image_extra)
{
    cairo_quartz_surface_t *surface = abstract_surface;
    cairo_surface_t *image_surface;
    unsigned char *data;
    int x1, y1, x2, y2;

    x1 = surface->extents.x;
    x2 = surface->extents.x + surface->extents.width;
    y1 = surface->extents.y;
    y2 = surface->extents.y + surface->extents.height;

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

    image_rect->x = x1;
    image_rect->y = y1;
    image_rect->width = x2 - x1;
    image_rect->height = y2 - y1;

    data = calloc (image_rect->width * image_rect->height * 4, 1);
    image_surface = cairo_image_surface_create_for_data (data,
							 CAIRO_FORMAT_ARGB32,
							 image_rect->width,
							 image_rect->height,
							 image_rect->width * 4);

    *image_out = (cairo_image_surface_t *)image_surface;
    *image_extra = data;

    return CAIRO_STATUS_SUCCESS;

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static CGImageRef
create_image_from_surface (cairo_image_surface_t *image_surface, void *data)
{
  CGImageRef image;
  CGColorSpaceRef color_space;
  CGDataProviderRef data_provider;
  int width, height;

  width = cairo_image_surface_get_width ((cairo_surface_t *)image_surface);
  height = cairo_image_surface_get_height ((cairo_surface_t *)image_surface);

  color_space = CGColorSpaceCreateDeviceRGB();
  data_provider = CGDataProviderCreateWithData (NULL, data,
						width * height * 4, NULL);
  image = CGImageCreate (width, height,
			 8, 32,
			 width * 4,
			 color_space,
			 kCGImageAlphaPremultipliedFirst,
			 data_provider,
			 NULL,
			 FALSE, kCGRenderingIntentDefault);

  CGColorSpaceRelease (color_space);
  CGDataProviderRelease (data_provider);

  return image;
}

static void
_cairo_quartz_surface_release_dest_image(void                    *abstract_surface,
                                         cairo_rectangle_int16_t *intersect_rect,
                                         cairo_image_surface_t   *image,
                                         cairo_rectangle_int16_t *image_rect,
                                         void                    *image_extra)
{
    cairo_quartz_surface_t *surface = abstract_surface;
    CGImageRef image_ref;
    CGRect rect;

    image_ref = create_image_from_surface (image, image_extra);

    rect = CGRectMake (image_rect->x, image_rect->y, image_rect->width, image_rect->height);

    if (surface->y_grows_down) {
	CGContextSaveGState (surface->context);
	CGContextTranslateCTM (surface->context, 0, image_rect->height + 2 * image_rect->y);
	CGContextScaleCTM (surface->context, 1, -1);
    }

    CGContextDrawImage(surface->context, rect, image_ref);
    CFRelease (image_ref);

    if (surface->y_grows_down) {
	CGContextRestoreGState (surface->context);
    }

    cairo_surface_destroy ((cairo_surface_t *)image);
    free (image_extra);
}

static cairo_int_status_t
_cairo_quartz_surface_set_clip_region(void *abstract_surface,
                                      pixman_region16_t * region)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    if (surface->clip_region)
	pixman_region_destroy (surface->clip_region);

    if (region) {
	surface->clip_region = pixman_region_create ();
	pixman_region_copy (surface->clip_region, region);
    } else
	surface->clip_region = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_quartz_surface_get_extents (void                    *abstract_surface,
				   cairo_rectangle_int16_t *rectangle)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    *rectangle = surface->extents;

    return CAIRO_STATUS_SUCCESS;
}

static const struct _cairo_surface_backend cairo_quartz_surface_backend = {
    CAIRO_SURFACE_TYPE_QUARTZ,
    NULL, /* create_similar */
    _cairo_quartz_surface_finish,
    _cairo_quartz_surface_acquire_source_image,
    NULL, /* release_source_image */
    _cairo_quartz_surface_acquire_dest_image,
    _cairo_quartz_surface_release_dest_image,
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_quartz_surface_set_clip_region,
    NULL, /* intersect_clip_path */
    _cairo_quartz_surface_get_extents,
    NULL  /* old_show_glyphs */
};

cairo_surface_t *cairo_quartz_surface_create(CGContextRef context,
					     int width,
					     int height,
					     cairo_bool_t y_grows_down)
{
    cairo_quartz_surface_t *surface;
    CGRect clip_box;

    surface = malloc(sizeof(cairo_quartz_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
        return (cairo_surface_t*) &_cairo_surface_nil;
    }

    /* XXX: The content value here might be totally wrong. */
    _cairo_surface_init(&surface->base, &cairo_quartz_surface_backend,
			CAIRO_CONTENT_COLOR_ALPHA);

    surface->context = context;
    surface->clip_region = NULL;
    surface->y_grows_down = y_grows_down;

    clip_box = CGContextGetClipBoundingBox (context);
    surface->extents.x = clip_box.origin.x;
    surface->extents.y = clip_box.origin.y;
    surface->extents.width = clip_box.size.width;
    surface->extents.height = clip_box.size.height;

    return (cairo_surface_t *) surface;
}

int
_cairo_surface_is_quartz (cairo_surface_t *surface)
{
    return surface->backend == &cairo_quartz_surface_backend;
}

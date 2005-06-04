/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
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
#include "cairo-ps.h"

#include <time.h>
#include <zlib.h>

static const cairo_surface_backend_t cairo_ps_surface_backend;

typedef struct cairo_ps_surface {
    cairo_surface_t base;

    /* PS-specific fields */
    cairo_output_stream_t *stream;

    double width_in_points;
    double height_in_points;
    double x_dpi;
    double y_dpi;

    int pages;

    cairo_image_surface_t *image;
} cairo_ps_surface_t;

#define PS_SURFACE_DPI_DEFAULT 300.0

static void
_cairo_ps_surface_erase (cairo_ps_surface_t *surface);

static cairo_surface_t *
_cairo_ps_surface_create_for_stream_internal (cairo_output_stream_t *stream,
					      double	    width_in_points,
					      double	   height_in_points)
{
    cairo_ps_surface_t *surface;
    int width_in_pixels, height_in_pixels;
    time_t now = time (0);

    surface = malloc (sizeof (cairo_ps_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (&surface->base, &cairo_ps_surface_backend);

    surface->stream = stream;

    surface->width_in_points  = width_in_points;
    surface->height_in_points = height_in_points;
    surface->x_dpi = PS_SURFACE_DPI_DEFAULT;
    surface->y_dpi = PS_SURFACE_DPI_DEFAULT;

    surface->pages = 0;

    width_in_pixels = (int) (surface->x_dpi * width_in_points / 72.0 + 1.0);
    height_in_pixels = (int) (surface->y_dpi * height_in_points / 72.0 + 1.0);

    surface->image = (cairo_image_surface_t *)
	cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
				    width_in_pixels, height_in_pixels);
    if (surface->image == NULL) {
	free (surface);
	return NULL;
    }

    _cairo_ps_surface_erase (surface);

    /* Document header */
    _cairo_output_stream_printf (surface->stream,
				 "%%!PS-Adobe-3.0\n"
				 "%%%%Creator: Cairo (http://cairographics.org)\n");
    _cairo_output_stream_printf (surface->stream,
				 "%%%%CreationDate: %s",
				 ctime (&now));
    _cairo_output_stream_printf (surface->stream,
				 "%%%%BoundingBox: %f %f %f %f\n",
				 0.0, 0.0,
				 surface->width_in_points,
				 surface->height_in_points);
    /* The "/FlateDecode filter" currently used is a feature of LanguageLevel 3 */
    _cairo_output_stream_printf (surface->stream,
				 "%%%%DocumentData: Clean7Bit\n"
				 "%%%%LanguageLevel: 3\n");
    _cairo_output_stream_printf (surface->stream,
				 "%%%%Orientation: Portrait\n"
				 "%%%%EndComments\n");

    return &surface->base;
}

cairo_surface_t *
cairo_ps_surface_create (const char    *filename,
			 double		width_in_points,
			 double		height_in_points)
{
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create_for_file (filename);
    if (stream == NULL)
	return NULL;

    return _cairo_ps_surface_create_for_stream_internal (stream,
							 width_in_points,
							 height_in_points);
}

cairo_surface_t *
cairo_ps_surface_create_for_stream (cairo_write_func_t	write_func,
				    void	       *closure,
				    double		width_in_points,
				    double		height_in_points)
{
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create (write_func, closure);
    if (stream == NULL)
	return NULL;

    return _cairo_ps_surface_create_for_stream_internal (stream,
							 width_in_points,
							 height_in_points);
}

static cairo_surface_t *
_cairo_ps_surface_create_similar (void		*abstract_src,
				 cairo_format_t	format,
				 int		drawable,
				 int		width,
				 int		height)
{
    return NULL;
}

static cairo_status_t
_cairo_ps_surface_finish (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    /* Document footer */
    _cairo_output_stream_printf (surface->stream,
				 "%%%%EOF\n");

    cairo_surface_destroy (&surface->image->base);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_ps_surface_erase (cairo_ps_surface_t *surface)
{
    _cairo_surface_fill_rectangle (&surface->image->base,
				   CAIRO_OPERATOR_SOURCE,
				   CAIRO_COLOR_TRANSPARENT,
				   0, 0,
				   surface->image->width,
				   surface->image->height);
}

static cairo_status_t
_cairo_ps_surface_acquire_source_image (void                    *abstract_surface,
					cairo_image_surface_t  **image_out,
					void                   **image_extra)
{
    cairo_ps_surface_t *surface = abstract_surface;
    
    *image_out = surface->image;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_acquire_dest_image (void                    *abstract_surface,
				      cairo_rectangle_t       *interest_rect,
				      cairo_image_surface_t  **image_out,
				      cairo_rectangle_t       *image_rect,
				      void                   **image_extra)
{
    cairo_ps_surface_t *surface = abstract_surface;
    
    image_rect->x = 0;
    image_rect->y = 0;
    image_rect->width = surface->image->width;
    image_rect->height = surface->image->height;
    
    *image_out = surface->image;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_copy_page (void *abstract_surface)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_ps_surface_t *surface = abstract_surface;
    int width = surface->image->width;
    int height = surface->image->height;
    cairo_output_stream_t *stream = surface->stream;

    int i, x, y;

    cairo_solid_pattern_t white_pattern;
    unsigned char *rgb, *compressed;
    unsigned long rgb_size, compressed_size;

    rgb_size = 3 * width * height;
    rgb = malloc (rgb_size);
    if (rgb == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL0;
    }

    compressed_size = (int) (1.0 + 1.1 * rgb_size);
    compressed = malloc (compressed_size);
    if (compressed == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL1;
    }

    /* PostScript can not represent the alpha channel, so we blend the
       current image over a white RGB surface to eliminate it. */

    _cairo_pattern_init_solid (&white_pattern, CAIRO_COLOR_WHITE);

    _cairo_surface_composite (CAIRO_OPERATOR_DEST_OVER,
			      &white_pattern.base,
			      NULL,
			      &surface->image->base,
			      0, 0,
			      0, 0,
			      0, 0,
			      width, height);
    
    _cairo_pattern_fini (&white_pattern.base);

    i = 0;
    for (y = 0; y < height; y++) {
	pixman_bits_t *pixel = (pixman_bits_t *) (surface->image->data + y * surface->image->stride);
	for (x = 0; x < width; x++, pixel++) {
	    rgb[i++] = (*pixel & 0x00ff0000) >> 16;
	    rgb[i++] = (*pixel & 0x0000ff00) >>  8;
	    rgb[i++] = (*pixel & 0x000000ff) >>  0;
	}
    }

    compress (compressed, &compressed_size, rgb, rgb_size);

    /* Page header */
    _cairo_output_stream_printf (stream, "%%%%Page: %d\n", ++surface->pages);

    _cairo_output_stream_printf (stream, "gsave\n");

    /* Image header goop */
    _cairo_output_stream_printf (stream, "%f %f translate\n",
				 0.0, surface->height_in_points);
    _cairo_output_stream_printf (stream, "/DeviceRGB setcolorspace\n");
    _cairo_output_stream_printf (stream, "<<\n");
    _cairo_output_stream_printf (stream, "	/ImageType 1\n");
    _cairo_output_stream_printf (stream, "	/Width %d\n", width);
    _cairo_output_stream_printf (stream, "	/Height %d\n", height);
    _cairo_output_stream_printf (stream, "	/BitsPerComponent 8\n");
    _cairo_output_stream_printf (stream, "	/Decode [ 0 1 0 1 0 1 ]\n");
    _cairo_output_stream_printf (stream, "	/DataSource currentfile /FlateDecode filter\n");
    _cairo_output_stream_printf (stream, "	/ImageMatrix [ 1 0 0 -1 0 1 ]\n");
    _cairo_output_stream_printf (stream, ">>\n");
    _cairo_output_stream_printf (stream, "image\n");

    /* Compressed image data */
    _cairo_output_stream_write (stream, compressed, compressed_size);

    _cairo_output_stream_printf (stream, "showpage\n");

    _cairo_output_stream_printf (stream, "grestore\n");

    /* Page footer */
    _cairo_output_stream_printf (stream, "%%%%EndPage\n");

    free (compressed);
    BAIL1:
    free (rgb);
    BAIL0:
    return status;
}

static cairo_int_status_t
_cairo_ps_surface_show_page (void *abstract_surface)
{
    cairo_int_status_t status;
    cairo_ps_surface_t *surface = abstract_surface;

    status = _cairo_ps_surface_copy_page (surface);
    if (status)
	return status;

    _cairo_ps_surface_erase (surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_set_clip_region (void *abstract_surface,
				   pixman_region16_t *region)
{
    cairo_ps_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_clip_region (surface->image, region);
}

static cairo_int_status_t
_cairo_ps_surface_get_extents (void		 *abstract_surface,
			       cairo_rectangle_t *rectangle)
{
    cairo_ps_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;

    /* XXX: The conversion to integers here is pretty bogus, (not to
     * mention the aribitray limitation of width to a short(!). We
     * may need to come up with a better interface for get_size.
     */
    rectangle->width  = (surface->width_in_points + 0.5);
    rectangle->height = (surface->height_in_points + 0.5);

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_ps_surface_backend = {
    _cairo_ps_surface_create_similar,
    _cairo_ps_surface_finish,
    _cairo_ps_surface_acquire_source_image,
    NULL, /* release_source_image */
    _cairo_ps_surface_acquire_dest_image,
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    _cairo_ps_surface_copy_page,
    _cairo_ps_surface_show_page,
    _cairo_ps_surface_set_clip_region,
    _cairo_ps_surface_get_extents,
    NULL /* show_glyphs */
};

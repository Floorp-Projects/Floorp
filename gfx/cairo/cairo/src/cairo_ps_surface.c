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

/**
 * cairo_set_target_ps:
 * @cr: a #cairo_t
 * @file: an open, writeable file
 * @width_inches: width of the output page, in inches
 * @height_inches: height of the output page, in inches
 * @x_pixels_per_inch: X resolution to use for image fallbacks;
 *   not all Cairo drawing can be represented in a postscript
 *   file, so Cairo will write out images for some portions
 *   of the output.
 * @y_pixels_per_inch: Y resolution to use for image fallbacks.
 * 
 * Directs output for a #cairo_t to a postscript file. The file must
 * be kept open until the #cairo_t is destroyed or set to have a
 * different target, and then must be closed by the application.
 **/
void
cairo_set_target_ps (cairo_t	*cr,
		     FILE	*file,
		     double	width_inches,
		     double	height_inches,
		     double	x_pixels_per_inch,
		     double	y_pixels_per_inch)
{
    cairo_surface_t *surface;

    surface = cairo_ps_surface_create (file,
				       width_inches, height_inches,
				       x_pixels_per_inch, y_pixels_per_inch);
    if (surface == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	return;
    }

    cairo_set_target_surface (cr, surface);

    /* cairo_set_target_surface takes a reference, so we must destroy ours */
    cairo_surface_destroy (surface);
}

typedef struct cairo_ps_surface {
    cairo_surface_t base;

    /* PS-specific fields */
    FILE *file;

    double width_inches;
    double height_inches;
    double x_ppi;
    double y_ppi;

    int pages;

    cairo_image_surface_t *image;
} cairo_ps_surface_t;

static void
_cairo_ps_surface_erase (cairo_ps_surface_t *surface);

cairo_surface_t *
cairo_ps_surface_create (FILE	*file,
			 double	width_inches,
			 double height_inches,
			 double	x_pixels_per_inch,
			 double	y_pixels_per_inch)
{
    cairo_ps_surface_t *surface;
    int width, height;
    time_t now = time (0);

    surface = malloc (sizeof (cairo_ps_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (&surface->base, &cairo_ps_surface_backend);

    surface->file = file;

    surface->width_inches = width_inches;
    surface->height_inches = height_inches;
    surface->x_ppi = x_pixels_per_inch;
    surface->y_ppi = x_pixels_per_inch;

    surface->pages = 0;

    width = (int) (x_pixels_per_inch * width_inches + 1.0);
    height = (int) (y_pixels_per_inch * height_inches + 1.0);

    surface->image = (cairo_image_surface_t *)
	cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    if (surface->image == NULL) {
	free (surface);
	return NULL;
    }

    _cairo_ps_surface_erase (surface);

    /* Document header */
    fprintf (file,
	     "%%!PS-Adobe-3.0\n"
	     "%%%%Creator: Cairo (http://cairographics.org)\n");
    fprintf (file,
	     "%%%%CreationDate: %s",
	     ctime (&now));
    fprintf (file,
	     "%%%%BoundingBox: %d %d %d %d\n",
	     0, 0, (int) (surface->width_inches * 72.0), (int) (surface->height_inches * 72.0));
    /* The "/FlateDecode filter" currently used is a feature of LanguageLevel 3 */
    fprintf (file,
	     "%%%%DocumentData: Clean7Bit\n"
	     "%%%%LanguageLevel: 3\n");
    fprintf (file,
	     "%%%%Orientation: Portrait\n"
	     "%%%%EndComments\n");

    return &surface->base;
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

static void
_cairo_ps_surface_destroy (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    /* Document footer */
    fprintf (surface->file, "%%%%EOF\n");

    cairo_surface_destroy (&surface->image->base);

    free (surface);
}

static void
_cairo_ps_surface_erase (cairo_ps_surface_t *surface)
{
    cairo_color_t transparent;

    _cairo_color_init (&transparent);
    _cairo_color_set_rgb (&transparent, 0., 0., 0.);
    _cairo_color_set_alpha (&transparent, 0.);
    _cairo_surface_fill_rectangle (&surface->image->base,
				   CAIRO_OPERATOR_SRC,
				   &transparent,
				   0, 0,
				   surface->image->width,
				   surface->image->height);
}

/* XXX: We should re-work this interface to return both X/Y ppi values. */
static double
_cairo_ps_surface_pixels_per_inch (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    return surface->y_ppi;
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

static void
_cairo_ps_surface_release_source_image (void                   *abstract_surface,
					cairo_image_surface_t  *image,
					void                   *image_extra)
{
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

static void
_cairo_ps_surface_release_dest_image (void                   *abstract_surface,
				      cairo_rectangle_t      *interest_rect,
				      cairo_image_surface_t  *image,
				      cairo_rectangle_t      *image_rect,
				      void                   *image_extra)
{
}

static cairo_status_t
_cairo_ps_surface_clone_similar (void			*abstract_surface,
				 cairo_surface_t	*src,
				 cairo_surface_t     **clone_out)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_ps_surface_composite (cairo_operator_t	operator,
			     cairo_pattern_t	*src,
			     cairo_pattern_t	*mask,
			     void		*abstract_dst,
			     int		src_x,
			     int		src_y,
			     int		mask_x,
			     int		mask_y,
			     int		dst_x,
			     int		dst_y,
			     unsigned int	width,
			     unsigned int	height)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_ps_surface_fill_rectangles (void			*abstract_surface,
				   cairo_operator_t	operator,
				   const cairo_color_t	*color,
				   cairo_rectangle_t	*rects,
				   int			num_rects)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_ps_surface_composite_trapezoids (cairo_operator_t	operator,
					cairo_pattern_t		*generic_src,
					void			*abstract_dst,
					int			x_src,
					int			y_src,
					int			x_dst,
					int			y_dst,
					unsigned int		width,
					unsigned int		height,
					cairo_trapezoid_t	*traps,
					int			num_traps)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_ps_surface_copy_page (void *abstract_surface)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_ps_surface_t *surface = abstract_surface;
    int width = surface->image->width;
    int height = surface->image->height;
    FILE *file = surface->file;

    int i, x, y;

    cairo_solid_pattern_t white_pattern;
    char *rgb, *compressed;
    long rgb_size, compressed_size;

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

    _cairo_pattern_init_solid (&white_pattern, 1.0, 1.0, 1.0);

    _cairo_surface_composite (CAIRO_OPERATOR_OVER_REVERSE,
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
    fprintf (file, "%%%%Page: %d\n", ++surface->pages);

    fprintf (file, "gsave\n");

    /* Image header goop */
    fprintf (file, "%g %g translate\n", 0.0, surface->height_inches * 72.0);
    fprintf (file, "%g %g scale\n", 72.0 / surface->x_ppi, 72.0 / surface->y_ppi);
    fprintf (file, "/DeviceRGB setcolorspace\n");
    fprintf (file, "<<\n");
    fprintf (file, "	/ImageType 1\n");
    fprintf (file, "	/Width %d\n", width);
    fprintf (file, "	/Height %d\n", height);
    fprintf (file, "	/BitsPerComponent 8\n");
    fprintf (file, "	/Decode [ 0 1 0 1 0 1 ]\n");
    fprintf (file, "	/DataSource currentfile /FlateDecode filter\n");
    fprintf (file, "	/ImageMatrix [ 1 0 0 -1 0 1 ]\n");
    fprintf (file, ">>\n");
    fprintf (file, "image\n");

    /* Compressed image data */
    fwrite (compressed, 1, compressed_size, file);

    fprintf (file, "showpage\n");

    fprintf (file, "grestore\n");

    /* Page footer */
    fprintf (file, "%%%%EndPage\n");

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

static const cairo_surface_backend_t cairo_ps_surface_backend = {
    _cairo_ps_surface_create_similar,
    _cairo_ps_surface_destroy,
    _cairo_ps_surface_pixels_per_inch,
    _cairo_ps_surface_acquire_source_image,
    _cairo_ps_surface_release_source_image,
    _cairo_ps_surface_acquire_dest_image,
    _cairo_ps_surface_release_dest_image,
    _cairo_ps_surface_clone_similar,
    _cairo_ps_surface_composite,
    _cairo_ps_surface_fill_rectangles,
    _cairo_ps_surface_composite_trapezoids,
    _cairo_ps_surface_copy_page,
    _cairo_ps_surface_show_page,
    _cairo_ps_surface_set_clip_region,
    NULL /* show_glyphs */
};

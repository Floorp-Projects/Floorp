/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 University of Southern California
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
 *      Olivier Andrieu <oliv__a@users.sourceforge.net>
 *	Carl D. Worth <cworth@cworth.org>
 */

#include <png.h>

#include "cairoint.h"
#include "cairo-png.h"

static const cairo_surface_backend_t cairo_png_surface_backend;

static cairo_int_status_t
_cairo_png_surface_copy_page (void *abstract_surface);

void
cairo_set_target_png (cairo_t	*cr,
		      FILE	*file,
		      cairo_format_t	format,
		      int	       	width,
		      int		height)
{
    cairo_surface_t *surface;

    surface = cairo_png_surface_create (file, format, 
					width, height);

    if (surface == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	return;
    }

    cairo_set_target_surface (cr, surface);

    /* cairo_set_target_surface takes a reference, so we must destroy ours */
    cairo_surface_destroy (surface);
}

typedef struct cairo_png_surface {
    cairo_surface_t base;

    /* PNG-specific fields */
    cairo_image_surface_t *image;
    FILE *file;
    int copied;

    cairo_format_t format;

} cairo_png_surface_t;


static void
_cairo_png_surface_erase (cairo_png_surface_t *surface);

cairo_surface_t *
cairo_png_surface_create (FILE			*file,
			  cairo_format_t	format,
			  int			width,
			  int			height)
{
    cairo_png_surface_t *surface;

    surface = malloc (sizeof (cairo_png_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (&surface->base, &cairo_png_surface_backend);

    surface->image = (cairo_image_surface_t *)
	cairo_image_surface_create (format, width, height);

    if (surface->image == NULL) {
	free (surface);
	return NULL;
    }

    _cairo_png_surface_erase (surface);

    surface->file = file;
    surface->copied = 0;

    surface->format = format;

    return &surface->base;
}


static cairo_surface_t *
_cairo_png_surface_create_similar (void		*abstract_src,
				   cairo_format_t	format,
				   int		drawable,
				   int		width,
				   int		height)
{
    return NULL;
}

static void
unpremultiply_data (png_structp png, png_row_infop row_info, png_bytep data)
{
    int i;

    for (i = 0; i < row_info->rowbytes; i += 4) {
        unsigned char *b = &data[i];
        unsigned int pixel;
        unsigned char alpha;

	memcpy (&pixel, b, sizeof (unsigned int));
	alpha = (pixel & 0xff000000) >> 24;
        if (alpha == 0) {
	    b[0] = b[1] = b[2] = b[3] = 0;
	} else {
            b[0] = (((pixel & 0x0000ff) >>  0) * 255) / alpha;
            b[1] = (((pixel & 0x00ff00) >>  8) * 255) / alpha;
            b[2] = (((pixel & 0xff0000) >> 16) * 255) / alpha;
	    b[3] = alpha;
	}
    }
}

static void
_cairo_png_surface_destroy (void *abstract_surface)
{
    cairo_png_surface_t *surface = abstract_surface;

    if (! surface->copied)
	_cairo_png_surface_copy_page (surface);

    cairo_surface_destroy (&surface->image->base);

    free (surface);
}

static void
_cairo_png_surface_erase (cairo_png_surface_t *surface)
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

static double
_cairo_png_surface_pixels_per_inch (void *abstract_surface)
{
    return 96.0;
}

static cairo_status_t
_cairo_png_surface_acquire_source_image (void                    *abstract_surface,
					 cairo_image_surface_t  **image_out,
					 void                   **image_extra)
{
    cairo_png_surface_t *surface = abstract_surface;
    
    *image_out = surface->image;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_png_surface_release_source_image (void                   *abstract_surface,
					 cairo_image_surface_t  *image,
					 void                   *image_extra)
{
}

static cairo_status_t
_cairo_png_surface_acquire_dest_image (void                    *abstract_surface,
				       cairo_rectangle_t       *interest_rect,
				       cairo_image_surface_t  **image_out,
				       cairo_rectangle_t       *image_rect,
				       void                   **image_extra)
{
    cairo_png_surface_t *surface = abstract_surface;
    
    image_rect->x = 0;
    image_rect->y = 0;
    image_rect->width = surface->image->width;
    image_rect->height = surface->image->height;
    
    *image_out = surface->image;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_png_surface_release_dest_image (void                   *abstract_surface,
				       cairo_rectangle_t      *interest_rect,
				       cairo_image_surface_t  *image,
				       cairo_rectangle_t      *image_rect,
				       void                   *image_extra)
{
}

static cairo_status_t
_cairo_png_surface_clone_similar (void			*abstract_surface,
				  cairo_surface_t	*src,
				  cairo_surface_t     **clone_out)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_png_surface_composite (cairo_operator_t	operator,
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
_cairo_png_surface_fill_rectangles (void			*abstract_surface,
				    cairo_operator_t	operator,
				    const cairo_color_t	*color,
				    cairo_rectangle_t	*rects,
				    int			num_rects)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_png_surface_composite_trapezoids (cairo_operator_t	operator,
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
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_png_surface_copy_page (void *abstract_surface)
{
    int i;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_png_surface_t *surface = abstract_surface;
    png_struct *png;
    png_info *info;
    png_byte **rows;
    png_color_16 white;
    int png_color_type;
    int depth;

    rows = malloc (surface->image->height * sizeof(png_byte*));
    if (rows == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    for (i = 0; i < surface->image->height; i++)
	rows[i] = surface->image->data + i * surface->image->stride;

    png = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL1;
    }

    info = png_create_info_struct (png);
    if (info == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL2;
    }

    if (setjmp (png_jmpbuf (png))) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto BAIL2;
    }
    
    png_init_io (png, surface->file);

    switch (surface->format) {
    case CAIRO_FORMAT_ARGB32:
	depth = 8;
	png_color_type = PNG_COLOR_TYPE_RGB_ALPHA;
	break;
    case CAIRO_FORMAT_RGB24:
	depth = 8;
	png_color_type = PNG_COLOR_TYPE_RGB;
	break;
    case CAIRO_FORMAT_A8:
	depth = 8;
	png_color_type = PNG_COLOR_TYPE_GRAY;
	break;
    case CAIRO_FORMAT_A1:
	depth = 1;
	png_color_type = PNG_COLOR_TYPE_GRAY;
	break;
    default:
	status = CAIRO_STATUS_NULL_POINTER;
	goto BAIL2;
    }

    png_set_IHDR (png, info,
		  surface->image->width,
		  surface->image->height, depth,
		  png_color_type,
		  PNG_INTERLACE_NONE,
		  PNG_COMPRESSION_TYPE_DEFAULT,
		  PNG_FILTER_TYPE_DEFAULT);

    white.red = 0xff;
    white.blue = 0xff;
    white.green = 0xff;
    png_set_bKGD (png, info, &white);

/* XXX: Setting the time is interfereing with the image comparison
    png_convert_from_time_t (&png_time, time (NULL));
    png_set_tIME (png, info, &png_time);
*/

    png_set_write_user_transform_fn (png, unpremultiply_data);
    if (surface->format == CAIRO_FORMAT_ARGB32 || surface->format == CAIRO_FORMAT_RGB24)
	png_set_bgr (png);
    if (surface->format == CAIRO_FORMAT_RGB24)
	png_set_filler (png, 0, PNG_FILLER_AFTER);

    png_write_info (png, info);
    png_write_image (png, rows);
    png_write_end (png, info);

    surface->copied = 1;

BAIL2:
    png_destroy_write_struct (&png, &info);
BAIL1:
    free (rows);

    return status;
}

static cairo_int_status_t
_cairo_png_surface_show_page (void *abstract_surface)
{
    cairo_int_status_t status;
    cairo_png_surface_t *surface = abstract_surface;

    status = _cairo_png_surface_copy_page (surface);
    if (status)
	return status;

    _cairo_png_surface_erase (surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_png_surface_set_clip_region (void *abstract_surface,
				    pixman_region16_t *region)
{
    cairo_png_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_clip_region (surface->image, region);
}

static const cairo_surface_backend_t cairo_png_surface_backend = {
    _cairo_png_surface_create_similar,
    _cairo_png_surface_destroy,
    _cairo_png_surface_pixels_per_inch,
    _cairo_png_surface_acquire_source_image,
    _cairo_png_surface_release_source_image,
    _cairo_png_surface_acquire_dest_image,
    _cairo_png_surface_release_dest_image,
    _cairo_png_surface_clone_similar,
    _cairo_png_surface_composite,
    _cairo_png_surface_fill_rectangles,
    _cairo_png_surface_composite_trapezoids,
    _cairo_png_surface_copy_page,
    _cairo_png_surface_show_page,
    _cairo_png_surface_set_clip_region,
    NULL /* show_glyphs */
};

/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
 * Copyright © 2005 Red Hat, Inc
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
 *	Kristian Høgsberg <krh@redhat.com>
 */

#include "cairoint.h"
#include "cairo-ps.h"
#include "cairo-font-subset-private.h"
#include "cairo-meta-surface-private.h"
#include "cairo-ft-private.h"

#include <time.h>
#include <zlib.h>

/* TODO:
 *
 * - Add document structure convention comments where appropriate.
 *
 * - Fix image compression.
 *
 * - Create a set of procs to use... specifically a trapezoid proc.
 */

static const cairo_surface_backend_t cairo_ps_surface_backend;

typedef struct cairo_ps_surface {
    cairo_surface_t base;

    /* PS-specific fields */
    cairo_output_stream_t *stream;

    double width;
    double height;
    double x_dpi;
    double y_dpi;

    cairo_surface_t *current_page;
    cairo_array_t pages;
    cairo_array_t fonts;
} cairo_ps_surface_t;

#define PS_SURFACE_DPI_DEFAULT 300.0

static cairo_int_status_t
_cairo_ps_surface_write_font_subsets (cairo_ps_surface_t *surface);

static cairo_int_status_t
_cairo_ps_surface_render_page (cairo_ps_surface_t *surface,
			       cairo_surface_t *page, int page_number);

static cairo_surface_t *
_cairo_ps_surface_create_for_stream_internal (cairo_output_stream_t *stream,
					      double		     width,
					      double		     height)
{
    cairo_ps_surface_t *surface;

    surface = malloc (sizeof (cairo_ps_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&surface->base, &cairo_ps_surface_backend);

    surface->stream = stream;

    surface->width  = width;
    surface->height = height;
    surface->x_dpi = PS_SURFACE_DPI_DEFAULT;
    surface->y_dpi = PS_SURFACE_DPI_DEFAULT;
    surface->base.device_x_scale = surface->x_dpi / 72.0;
    surface->base.device_y_scale = surface->y_dpi / 72.0;

    surface->current_page = _cairo_meta_surface_create (width,
							height);
    if (surface->current_page->status) {
	free (surface);
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_array_init (&surface->pages, sizeof (cairo_surface_t *));
    _cairo_array_init (&surface->fonts, sizeof (cairo_font_subset_t *));

    return &surface->base;
}

cairo_surface_t *
cairo_ps_surface_create (const char    *filename,
			 double		width_in_points,
			 double		height_in_points)
{
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create_for_file (filename);
    if (stream == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

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
    if (stream == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return _cairo_ps_surface_create_for_stream_internal (stream,
							 width_in_points,
							 height_in_points);
}

/**
 * cairo_ps_surface_set_dpi:
 * @surface: a postscript cairo_surface_t
 * @x_dpi: horizontal dpi
 * @y_dpi: vertical dpi
 * 
 * Set horizontal and vertical resolution for image fallbacks.  When
 * the postscript backend needs to fall back to image overlays, it
 * will use this resolution.
 **/
void
cairo_ps_surface_set_dpi (cairo_surface_t *surface,
			  double	   x_dpi,
			  double	   y_dpi)
{
    cairo_ps_surface_t *ps_surface = (cairo_ps_surface_t *) surface;

    ps_surface->x_dpi = x_dpi;    
    ps_surface->y_dpi = y_dpi;    
    ps_surface->base.device_x_scale = ps_surface->x_dpi / 72.0;
    ps_surface->base.device_y_scale = ps_surface->y_dpi / 72.0;
}

static cairo_status_t
_cairo_ps_surface_finish (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_surface_t *page;
    cairo_font_subset_t *subset;
    cairo_status_t status;
    int i;
    time_t now;

    now = time (NULL);

    /* Document header */
    _cairo_output_stream_printf (surface->stream,
				 "%%!PS-Adobe-3.0\n"
				 "%%%%Creator: Cairo (http://cairographics.org)\n"
				 "%%%%CreationDate: %s"
				 "%%%%Title: Some clever title\n"
				 "%%%%Pages: %d\n"
				 "%%%%BoundingBox: %f %f %f %f\n",
				 ctime (&now),
				 surface->pages.num_elements,
				 0.0, 0.0, 
				 surface->width,
				 surface->height);

    /* The "/FlateDecode filter" currently used is a feature of
     * LanguageLevel 3 */
    _cairo_output_stream_printf (surface->stream,
				 "%%%%DocumentData: Clean7Bit\n"
				 "%%%%LanguageLevel: 3\n"
				 "%%%%Orientation: Portrait\n"
				 "%%%%EndComments\n");

    _cairo_ps_surface_write_font_subsets (surface);

    status = CAIRO_STATUS_SUCCESS;
    for (i = 0; i < surface->pages.num_elements; i++) {
	_cairo_array_copy_element (&surface->pages, i, &page);

	status = _cairo_ps_surface_render_page (surface, page, i + 1);
	if (status)
	    break;
    }

    /* Document footer */
    _cairo_output_stream_printf (surface->stream,
				 "%%%%Trailer\n"
				 "%%%%EOF\n");

    for (i = 0; i < surface->pages.num_elements; i++) {
	_cairo_array_copy_element (&surface->pages, i, &page);
	cairo_surface_destroy (page);
    }	
    _cairo_array_fini (&surface->pages);

    for (i = 0; i < surface->fonts.num_elements; i++) {
	_cairo_array_copy_element (&surface->fonts, i, &subset);
	_cairo_font_subset_destroy (subset);
    }	
    _cairo_array_fini (&surface->fonts);

    _cairo_output_stream_destroy (surface->stream);
    cairo_surface_destroy (surface->current_page);

    return status;
}

static cairo_int_status_t
_cairo_ps_surface_composite (cairo_operator_t	operator,
			     cairo_pattern_t	*src_pattern,
			     cairo_pattern_t	*mask_pattern,
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
    cairo_ps_surface_t *surface = abstract_dst;

    return _cairo_surface_composite (operator,
				     src_pattern,
				     mask_pattern,
				     surface->current_page,
				     src_x,
				     src_y,
				     mask_x,
				     mask_y,
				     dst_x,
				     dst_y,
				     width,
				     height);
}

static cairo_int_status_t
_cairo_ps_surface_fill_rectangles (void			*abstract_surface,
				   cairo_operator_t	operator,
				   const cairo_color_t	*color,
				   cairo_rectangle_t	*rects,
				   int			num_rects)
{
    cairo_ps_surface_t *surface = abstract_surface;

    return _cairo_surface_fill_rectangles (surface->current_page,
					   operator,
					   color,
					   rects,
					   num_rects);
}

static cairo_int_status_t
_cairo_ps_surface_composite_trapezoids (cairo_operator_t	operator,
					cairo_pattern_t		*pattern,
					void			*abstract_dst,
					cairo_antialias_t	antialias,
					int			x_src,
					int			y_src,
					int			x_dst,
					int			y_dst,
					unsigned int		width,
					unsigned int		height,
					cairo_trapezoid_t	*traps,
					int			num_traps)
{
    cairo_ps_surface_t *surface = abstract_dst;

    return _cairo_surface_composite_trapezoids (operator,
						pattern,
						surface->current_page,
						antialias,
						x_src,
						y_src,
						x_dst,
						y_dst,
						width,
						height,
						traps,
						num_traps);
}

static cairo_int_status_t
_cairo_ps_surface_copy_page (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    /* FIXME: We need to copy the meta surface contents here */

    return _cairo_surface_show_page (&surface->base);
}

static cairo_int_status_t
_cairo_ps_surface_show_page (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    _cairo_array_append (&surface->pages, &surface->current_page, 1);
    surface->current_page = _cairo_meta_surface_create (surface->width,
							surface->height);
    if (surface->current_page->status)
	return CAIRO_STATUS_NO_MEMORY;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_intersect_clip_path (void		  *dst,
				       cairo_path_fixed_t *path,
				       cairo_fill_rule_t   fill_rule,
				       double		   tolerance,
				       cairo_antialias_t   antialias)
{
    cairo_ps_surface_t *surface = dst;

    return _cairo_surface_intersect_clip_path (surface->current_page,
					       path,
					       fill_rule,
					       tolerance,
					       antialias);
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
    rectangle->width  = surface->width;
    rectangle->height = surface->height;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_font_subset_t *
_cairo_ps_surface_get_font (cairo_ps_surface_t  *surface,
			    cairo_scaled_font_t *scaled_font)
{
    cairo_unscaled_font_t *unscaled_font;
    cairo_font_subset_t *subset;
    unsigned int num_fonts, i;

    /* XXX: Need to fix this to work with a general cairo_scaled_font_t. */
    if (! _cairo_scaled_font_is_ft (scaled_font))
	return NULL;

    /* XXX Why is this an ft specific function? */
    unscaled_font = _cairo_ft_scaled_font_get_unscaled_font (scaled_font);

    num_fonts = _cairo_array_num_elements (&surface->fonts);
    for (i = 0; i < num_fonts; i++) {
	_cairo_array_copy_element (&surface->fonts, i, &subset);
	if (subset->unscaled_font == unscaled_font)
	    return subset;
    }

    subset = _cairo_font_subset_create (unscaled_font);
    if (subset == NULL)
	return NULL;

    subset->font_id = surface->fonts.num_elements;
    if (_cairo_array_append (&surface->fonts, &subset, 1) == NULL) {
	_cairo_font_subset_destroy (subset);
	return NULL;
    }

    return subset;
}

static cairo_int_status_t
_cairo_ps_surface_show_glyphs (cairo_scaled_font_t	*scaled_font,
			       cairo_operator_t		operator,
			       cairo_pattern_t		*pattern,
			       void			*abstract_surface,
			       int			source_x,
			       int			source_y,
			       int			dest_x,
			       int			dest_y,
			       unsigned int		width,
			       unsigned int		height,
			       const cairo_glyph_t	*glyphs,
			       int			num_glyphs)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_font_subset_t *subset;
    int i;

    /* XXX: Need to fix this to work with a general cairo_scaled_font_t. */
    if (! _cairo_scaled_font_is_ft (scaled_font))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* Collect font subset info as we go. */
    subset = _cairo_ps_surface_get_font (surface, scaled_font);
    if (subset == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    for (i = 0; i < num_glyphs; i++)
	_cairo_font_subset_use_glyph (subset, glyphs[i].index);

    return _cairo_surface_show_glyphs (scaled_font,
				       operator,
				       pattern,
				       surface->current_page,
				       source_x,
				       source_y,
				       dest_x,
				       dest_y,
				       width,
				       height,
				       glyphs,
				       num_glyphs);
}

static cairo_int_status_t
_cairo_ps_surface_fill_path (cairo_operator_t	operator,
			     cairo_pattern_t	*pattern,
			     void		*abstract_dst,
			     cairo_path_fixed_t	*path,
			     cairo_fill_rule_t   fill_rule,
			     double		 tolerance)
{
    cairo_ps_surface_t *surface = abstract_dst;

    return _cairo_surface_fill_path (operator,
				     pattern,
				     surface->current_page,
				     path,
				     fill_rule,
				     tolerance);
}

static const cairo_surface_backend_t cairo_ps_surface_backend = {
    NULL, /* create_similar */
    _cairo_ps_surface_finish,
    NULL, /* acquire_source_image */
    NULL, /* release_source_image */
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    _cairo_ps_surface_composite,
    _cairo_ps_surface_fill_rectangles,
    _cairo_ps_surface_composite_trapezoids,
    _cairo_ps_surface_copy_page,
    _cairo_ps_surface_show_page,
    NULL, /* set_clip_region */
    _cairo_ps_surface_intersect_clip_path,
    _cairo_ps_surface_get_extents,
    _cairo_ps_surface_show_glyphs,
    _cairo_ps_surface_fill_path
};

static cairo_int_status_t
_cairo_ps_surface_write_type42_dict (cairo_ps_surface_t  *surface,
				     cairo_font_subset_t *subset)
{
    const char *data;
    unsigned long data_size;
    cairo_status_t status;
    int i;

    status = CAIRO_STATUS_SUCCESS;

    /* FIXME: Figure out document structure convention for fonts */

    _cairo_output_stream_printf (surface->stream,
				 "11 dict begin\n"
				 "/FontType 42 def\n"
				 "/FontName /f%d def\n"
				 "/PaintType 0 def\n"
				 "/FontMatrix [ 1 0 0 1 0 0 ] def\n"
				 "/FontBBox [ 0 0 0 0 ] def\n"
				 "/Encoding 256 array def\n"
				 "0 1 255 { Encoding exch /.notdef put } for\n",
				 subset->font_id);

    /* FIXME: Figure out how subset->x_max etc maps to the /FontBBox */

    for (i = 1; i < subset->num_glyphs; i++)
	_cairo_output_stream_printf (surface->stream,
				     "Encoding %d /g%d put\n", i, i);

    _cairo_output_stream_printf (surface->stream,
				 "/CharStrings %d dict dup begin\n"
				 "/.notdef 0 def\n",
				 subset->num_glyphs);

    for (i = 1; i < subset->num_glyphs; i++)
	_cairo_output_stream_printf (surface->stream,
				     "/g%d %d def\n", i, i);

    _cairo_output_stream_printf (surface->stream,
				 "end readonly def\n");

    status = _cairo_font_subset_generate (subset, &data, &data_size);

    /* FIXME: We need to break up fonts bigger than 64k so we don't
     * exceed string size limitation.  At glyph boundaries.  Stupid
     * postscript. */
    _cairo_output_stream_printf (surface->stream,
				 "/sfnts [<");

    _cairo_output_stream_write_hex_string (surface->stream, data, data_size);

    _cairo_output_stream_printf (surface->stream,
				 ">] def\n"
				 "FontName currentdict end definefont pop\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_write_font_subsets (cairo_ps_surface_t *surface)
{
    cairo_font_subset_t *subset;
    int i;

    for (i = 0; i < surface->fonts.num_elements; i++) {
	_cairo_array_copy_element (&surface->fonts, i, &subset);
	_cairo_ps_surface_write_type42_dict (surface, subset);
    }

    return CAIRO_STATUS_SUCCESS;
}

typedef struct _cairo_ps_fallback_area cairo_ps_fallback_area_t;
struct _cairo_ps_fallback_area {
    int x, y;
    unsigned int width, height;
    cairo_ps_fallback_area_t *next;
};

typedef struct _ps_output_surface {
    cairo_surface_t base;
    cairo_ps_surface_t *parent;
    cairo_ps_fallback_area_t *fallback_areas;
} ps_output_surface_t;

static cairo_int_status_t
_ps_output_add_fallback_area (ps_output_surface_t *surface,
			      int x, int y,
			      unsigned int width,
			      unsigned int height)
{
    cairo_ps_fallback_area_t *area;
    
    /* FIXME: Do a better job here.  Ideally, we would use a 32 bit
     * region type, but probably just computing bounding boxes would
     * also work fine. */

    area = malloc (sizeof (cairo_ps_fallback_area_t));
    if (area == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    area->x = x;
    area->y = y;
    area->width = width;
    area->height = height;
    area->next = surface->fallback_areas;

    surface->fallback_areas = area;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_ps_output_finish (void *abstract_surface)
{
    ps_output_surface_t *surface = abstract_surface;
    cairo_ps_fallback_area_t *area, *next;

    for (area = surface->fallback_areas; area != NULL; area = next) {
	next = area->next;
	free (area);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
color_is_gray (cairo_color_t *color)
{
    const double epsilon = 0.00001;

    return (fabs (color->red - color->green) < epsilon &&
	    fabs (color->red - color->blue) < epsilon);
}

static cairo_bool_t
color_is_translucent (const cairo_color_t *color)
{
    return color->alpha < 0.999;
}

static cairo_bool_t
pattern_is_translucent (cairo_pattern_t *abstract_pattern)
{
    cairo_pattern_union_t *pattern;

    pattern = (cairo_pattern_union_t *) abstract_pattern;
    switch (pattern->base.type) {
    case CAIRO_PATTERN_SOLID:
	return color_is_translucent (&pattern->solid.color);
    case CAIRO_PATTERN_SURFACE:
    case CAIRO_PATTERN_LINEAR:
    case CAIRO_PATTERN_RADIAL:
	return FALSE;
    }	

    ASSERT_NOT_REACHED;
    return FALSE;
}

/* PS Output - this section handles output of the parts of the meta
 * surface we can render natively in PS. */

static void *
compress_dup (const void *data, unsigned long data_size,
	      unsigned long *compressed_size)
{
    void *compressed;

    /* Bound calculation taken from zlib. */
    *compressed_size = data_size + (data_size >> 12) + (data_size >> 14) + 11;
    compressed = malloc (*compressed_size);
    if (compressed == NULL)
	return NULL;

    compress (compressed, compressed_size, data, data_size);

    return compressed;
}

static cairo_status_t
emit_image (cairo_ps_surface_t    *surface,
	    cairo_image_surface_t *image,
	    cairo_matrix_t	  *matrix)
{
    cairo_status_t status;
    unsigned char *rgb, *compressed;
    unsigned long rgb_size, compressed_size;
    cairo_surface_t *opaque;
    cairo_image_surface_t *opaque_image;
    cairo_pattern_union_t pattern;
    cairo_matrix_t d2i;
    int x, y, i;

    /* PostScript can not represent the alpha channel, so we blend the
       current image over a white RGB surface to eliminate it. */

    if (image->base.status)
	return image->base.status;

    if (image->format != CAIRO_FORMAT_RGB24) {
	opaque = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
					     image->width,
					     image->height);
	if (opaque->status) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto bail0;
	}
    
	_cairo_pattern_init_for_surface (&pattern.surface, &image->base);
    
	_cairo_surface_composite (CAIRO_OPERATOR_DEST_OVER,
				  &pattern.base,
				  NULL,
				  opaque,
				  0, 0,
				  0, 0,
				  0, 0,
				  image->width,
				  image->height);
    
	_cairo_pattern_fini (&pattern.base);
	opaque_image = (cairo_image_surface_t *) opaque;
    } else {
	opaque = &image->base;
	opaque_image = image;
    }

    rgb_size = 3 * opaque_image->width * opaque_image->height;
    rgb = malloc (rgb_size);
    if (rgb == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto bail1;
    }

    i = 0;
    for (y = 0; y < opaque_image->height; y++) {
	pixman_bits_t *pixel = (pixman_bits_t *) (opaque_image->data + y * opaque_image->stride);
	for (x = 0; x < opaque_image->width; x++, pixel++) {
	    rgb[i++] = (*pixel & 0x00ff0000) >> 16;
	    rgb[i++] = (*pixel & 0x0000ff00) >>  8;
	    rgb[i++] = (*pixel & 0x000000ff) >>  0;
	}
    }

    compressed = compress_dup (rgb, rgb_size, &compressed_size);
    if (compressed == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto bail2;
    }

    /* matrix transforms from user space to image space.  We need to
     * transform from device space to image space to compensate for
     * postscripts coordinate system. */
    cairo_matrix_init (&d2i, 1, 0, 0, 1, 0, 0);
    cairo_matrix_multiply (&d2i, &d2i, matrix);

    _cairo_output_stream_printf (surface->stream,
				 "/DeviceRGB setcolorspace\n"
				 "<<\n"
				 "	/ImageType 1\n"
				 "	/Width %d\n"
				 "	/Height %d\n"
				 "	/BitsPerComponent 8\n"
				 "	/Decode [ 0 1 0 1 0 1 ]\n"
				 "	/DataSource currentfile\n"
				 "	/ImageMatrix [ %f %f %f %f %f %f ]\n"
				 ">>\n"
				 "image\n",
				 opaque_image->width,
				 opaque_image->height,
				 d2i.xx, d2i.yx,
				 d2i.xy, d2i.yy,
				 d2i.x0, d2i.y0);

    /* Compressed image data */
    _cairo_output_stream_write (surface->stream, rgb, rgb_size);
    status = CAIRO_STATUS_SUCCESS;

    _cairo_output_stream_printf (surface->stream,
				 "\n");

    free (compressed);
 bail2:
    free (rgb);
 bail1:
    if (opaque_image != image)
	cairo_surface_destroy (opaque);
 bail0:
    return status;
}

static void
emit_solid_pattern (cairo_ps_surface_t *surface,
		    cairo_solid_pattern_t *pattern)
{
    if (color_is_gray (&pattern->color))
	_cairo_output_stream_printf (surface->stream,
				     "%f setgray\n",
				     pattern->color.red);
    else
	_cairo_output_stream_printf (surface->stream,
				     "%f %f %f setrgbcolor\n",
				     pattern->color.red,
				     pattern->color.green,
				     pattern->color.blue);
}

static void
emit_surface_pattern (cairo_ps_surface_t *surface,
		      cairo_surface_pattern_t *pattern)
{
}

static void
emit_linear_pattern (cairo_ps_surface_t *surface,
		     cairo_linear_pattern_t *pattern)
{
}

static void
emit_radial_pattern (cairo_ps_surface_t *surface,
		     cairo_radial_pattern_t *pattern)
{
}

static void
emit_pattern (cairo_ps_surface_t *surface, cairo_pattern_t *pattern)
{
    /* FIXME: We should keep track of what pattern is currently set in
     * the postscript file and only emit code if we're setting a
     * different pattern. */

    switch (pattern->type) {
    case CAIRO_PATTERN_SOLID:	
	emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_SURFACE:
	emit_surface_pattern (surface, (cairo_surface_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_LINEAR:
	emit_linear_pattern (surface, (cairo_linear_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_RADIAL:
	emit_radial_pattern (surface, (cairo_radial_pattern_t *) pattern);
	break;	    
    }
}


static cairo_int_status_t
_ps_output_composite (cairo_operator_t	operator,
		      cairo_pattern_t  *src_pattern,
		      cairo_pattern_t  *mask_pattern,
		      void	       *abstract_dst,
		      int		src_x,
		      int		src_y,
		      int		mask_x,
		      int		mask_y,
		      int		dst_x,
		      int		dst_y,
		      unsigned int	width,
		      unsigned int	height)
{
    ps_output_surface_t *surface = abstract_dst;
    cairo_output_stream_t *stream = surface->parent->stream;
    cairo_surface_pattern_t *surface_pattern;
    cairo_status_t status;
    cairo_image_surface_t *image;
    void *image_extra;

    if (mask_pattern) {
	/* FIXME: Investigate how this can be done... we'll probably
	 * need pixmap fallbacks for this, though. */
	_cairo_output_stream_printf (stream,
				     "%% _ps_output_composite: with mask\n");
	goto bail;
    }

    status = CAIRO_STATUS_SUCCESS;
    switch (src_pattern->type) {
    case CAIRO_PATTERN_SOLID:
	_cairo_output_stream_printf (stream,
				     "%% _ps_output_composite: solid\n");
	goto bail;

    case CAIRO_PATTERN_SURFACE:
	surface_pattern = (cairo_surface_pattern_t *) src_pattern;

	if (src_pattern->extend != CAIRO_EXTEND_NONE) {
	    _cairo_output_stream_printf (stream,
					 "%% _ps_output_composite: repeating image\n");
	    goto bail;
	}
	    

	status = _cairo_surface_acquire_source_image (surface_pattern->surface,
						      &image,
						      &image_extra);
	if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	    _cairo_output_stream_printf (stream,
					 "%% _ps_output_composite: src_pattern not available as image\n");
	    goto bail;
	} else if (status) {
	    break;
	}
	status = emit_image (surface->parent, image, &src_pattern->matrix);
	_cairo_surface_release_source_image (surface_pattern->surface,
					     image, image_extra);
	break;

    case CAIRO_PATTERN_LINEAR:
    case CAIRO_PATTERN_RADIAL:
	_cairo_output_stream_printf (stream,
				     "%% _ps_output_composite: gradient\n");
	goto bail;
    }

    return status;
bail:
    return _ps_output_add_fallback_area (surface, dst_x, dst_y, width, height);
}

static cairo_int_status_t
_ps_output_fill_rectangles (void		*abstract_surface,
			    cairo_operator_t	 operator,
			    const cairo_color_t	*color,
			    cairo_rectangle_t	*rects,
			    int			 num_rects)
{
    ps_output_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->parent->stream;
    cairo_solid_pattern_t solid;
    int i;

    if (!num_rects)
	return CAIRO_STATUS_SUCCESS;
    
    if (color_is_translucent (color)) {
	int min_x = rects[0].x;
	int min_y = rects[0].y;
	int max_x = rects[0].x + rects[0].width;
	int max_y = rects[0].y + rects[0].height;

	for (i = 1; i < num_rects; i++) {
	    if (rects[i].x < min_x) min_x = rects[i].x;
	    if (rects[i].y < min_y) min_y = rects[i].y;
	    if (rects[i].x + rects[i].width > max_x) max_x = rects[i].x + rects[i].width;
	    if (rects[i].y + rects[i].height > max_y) max_y = rects[i].y + rects[i].height;
	}
	return _ps_output_add_fallback_area (surface, min_x, min_y, max_x - min_x, max_y - min_y);
    }
	
    _cairo_output_stream_printf (stream,
				 "%% _ps_output_fill_rectangles\n");

    _cairo_pattern_init_solid (&solid, color);
    emit_pattern (surface->parent, &solid.base);
    _cairo_pattern_fini (&solid.base);

    _cairo_output_stream_printf (stream, "[");
    for (i = 0; i < num_rects; i++) {
      _cairo_output_stream_printf (stream,
				   " %d %d %d %d",
				   rects[i].x, rects[i].y,
				   rects[i].width, rects[i].height);
    }

    _cairo_output_stream_printf (stream, " ] rectfill\n");

    return CAIRO_STATUS_SUCCESS;
}

static double
intersect (cairo_line_t *line, cairo_fixed_t y)
{
    return _cairo_fixed_to_double (line->p1.x) +
	_cairo_fixed_to_double (line->p2.x - line->p1.x) *
	_cairo_fixed_to_double (y - line->p1.y) /
	_cairo_fixed_to_double (line->p2.y - line->p1.y);
}

static cairo_int_status_t
_ps_output_composite_trapezoids (cairo_operator_t	operator,
				 cairo_pattern_t	*pattern,
				 void			*abstract_dst,
				 cairo_antialias_t	antialias,
				 int			x_src,
				 int			y_src,
				 int			x_dst,
				 int			y_dst,
				 unsigned int		width,
				 unsigned int		height,
				 cairo_trapezoid_t	*traps,
				 int			num_traps)
{
    ps_output_surface_t *surface = abstract_dst;
    cairo_output_stream_t *stream = surface->parent->stream;
    int i;

    if (pattern_is_translucent (pattern))
	return _ps_output_add_fallback_area (surface, x_dst, y_dst, width, height);

    _cairo_output_stream_printf (stream,
				 "%% _ps_output_composite_trapezoids\n");

    emit_pattern (surface->parent, pattern);

    for (i = 0; i < num_traps; i++) {
	double left_x1, left_x2, right_x1, right_x2, top, bottom;

	left_x1  = intersect (&traps[i].left, traps[i].top);
	left_x2  = intersect (&traps[i].left, traps[i].bottom);
	right_x1 = intersect (&traps[i].right, traps[i].top);
	right_x2 = intersect (&traps[i].right, traps[i].bottom);
	top      = _cairo_fixed_to_double (traps[i].top);
	bottom   = _cairo_fixed_to_double (traps[i].bottom);

	_cairo_output_stream_printf
	    (stream,
	     "%f %f moveto %f %f lineto %f %f lineto %f %f lineto "
	     "closepath\n",
	     left_x1, top,
	     left_x2, bottom,
	     right_x2, bottom,
	     right_x1, top);
    }

    _cairo_output_stream_printf (stream,
				 "fill\n");

    return CAIRO_STATUS_SUCCESS;
}

typedef struct
{
    cairo_output_stream_t *output_stream;
    cairo_bool_t has_current_point;
} ps_output_path_info_t;

static cairo_status_t
_ps_output_path_move_to (void *closure, cairo_point_t *point)
{
    ps_output_path_info_t *info = closure;

    _cairo_output_stream_printf (info->output_stream,
				 "%f %f moveto ",
				 _cairo_fixed_to_double (point->x),
				 _cairo_fixed_to_double (point->y));
    info->has_current_point = TRUE;
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_ps_output_path_line_to (void *closure, cairo_point_t *point)
{
    ps_output_path_info_t *info = closure;
    const char *ps_operator;

    if (info->has_current_point)
	ps_operator = "lineto";
    else
	ps_operator = "moveto";
    
    _cairo_output_stream_printf (info->output_stream,
				 "%f %f %s ",
				 _cairo_fixed_to_double (point->x),
				 _cairo_fixed_to_double (point->y),
				 ps_operator);
    info->has_current_point = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_ps_output_path_curve_to (void          *closure,
			  cairo_point_t *b,
			  cairo_point_t *c,
			  cairo_point_t *d)
{
    ps_output_path_info_t *info = closure;

    _cairo_output_stream_printf (info->output_stream,
				 "%f %f %f %f %f %f curveto ",
				 _cairo_fixed_to_double (b->x),
				 _cairo_fixed_to_double (b->y),
				 _cairo_fixed_to_double (c->x),
				 _cairo_fixed_to_double (c->y),
				 _cairo_fixed_to_double (d->x),
				 _cairo_fixed_to_double (d->y));
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_ps_output_path_close_path (void *closure)
{
    ps_output_path_info_t *info = closure;
    
    _cairo_output_stream_printf (info->output_stream,
				 "closepath\n");
    info->has_current_point = FALSE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_ps_output_intersect_clip_path (void		   *abstract_surface,
				cairo_path_fixed_t *path,
				cairo_fill_rule_t   fill_rule,
				double		    tolerance,
				cairo_antialias_t   antialias)
{
    ps_output_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->parent->stream;
    cairo_status_t status;
    ps_output_path_info_t info;
    const char *ps_operator;

    _cairo_output_stream_printf (stream,
				 "%% _ps_output_intersect_clip_path\n");

    if (path == NULL) {
	_cairo_output_stream_printf (stream, "initclip\n");
	return CAIRO_STATUS_SUCCESS;
    }

    info.output_stream = stream;
    info.has_current_point = FALSE;

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _ps_output_path_move_to,
					  _ps_output_path_line_to,
					  _ps_output_path_curve_to,
					  _ps_output_path_close_path,
					  &info);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	ps_operator = "clip";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	ps_operator = "eoclip";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (stream,
				 "%s newpath\n",
				 ps_operator);

    return status;
}


static cairo_int_status_t
_ps_output_show_glyphs (cairo_scaled_font_t	*scaled_font,
			cairo_operator_t	operator,
			cairo_pattern_t		*pattern,
			void			*abstract_surface,
			int			source_x,
			int			source_y,
			int			dest_x,
			int			dest_y,
			unsigned int		width,
			unsigned int		height,
			const cairo_glyph_t	*glyphs,
			int			num_glyphs)
{
    ps_output_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->parent->stream;
    cairo_font_subset_t *subset;
    int i, subset_index;

    /* XXX: Need to fix this to work with a general cairo_scaled_font_t. */
    if (! _cairo_scaled_font_is_ft (scaled_font))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (pattern_is_translucent (pattern))
	return _ps_output_add_fallback_area (surface, dest_x, dest_y, width, height);

    _cairo_output_stream_printf (stream,
				 "%% _ps_output_show_glyphs\n");

    emit_pattern (surface->parent, pattern);

    /* FIXME: Need to optimize this so we only do this sequence if the
     * font isn't already set. */

    subset = _cairo_ps_surface_get_font (surface->parent, scaled_font);
    _cairo_output_stream_printf (stream,
				 "/f%d findfont\n"
				 "[ %f %f %f %f 0 0 ] makefont\n"
				 "setfont\n",
				 subset->font_id,
				 scaled_font->scale.xx,
				 scaled_font->scale.yx,
				 scaled_font->scale.xy,
				 -scaled_font->scale.yy);

    /* FIXME: Need to optimize per glyph code.  Should detect when
     * glyphs share the same baseline and when the spacing corresponds
     * to the glyph widths. */

    for (i = 0; i < num_glyphs; i++) {
	subset_index = _cairo_font_subset_use_glyph (subset, glyphs[i].index);
	_cairo_output_stream_printf (stream,
				     "%f %f moveto (\\%o) show\n",
				     glyphs[i].x,
				     glyphs[i].y,
				     subset_index);
	
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_ps_output_fill_path (cairo_operator_t	  operator,
		      cairo_pattern_t	 *pattern,
		      void		 *abstract_dst,
		      cairo_path_fixed_t *path,
		      cairo_fill_rule_t   fill_rule,
		      double		  tolerance)
{
    ps_output_surface_t *surface = abstract_dst;
    cairo_output_stream_t *stream = surface->parent->stream;
    cairo_int_status_t status;
    ps_output_path_info_t info;
    const char *ps_operator;

    if (pattern_is_translucent (pattern))
	return _ps_output_add_fallback_area (surface,
					     0, 0,
					     surface->parent->width,
					     surface->parent->height);
    _cairo_output_stream_printf (stream,
				 "%% _ps_output_fill_path\n");

    emit_pattern (surface->parent, pattern);

    info.output_stream = stream;
    info.has_current_point = FALSE;

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _ps_output_path_move_to,
					  _ps_output_path_line_to,
					  _ps_output_path_curve_to,
					  _ps_output_path_close_path,
					  &info);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	ps_operator = "fill";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	ps_operator = "eofill";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (stream,
				 "%s\n", ps_operator);

    return status;
}

static const cairo_surface_backend_t ps_output_backend = {
    NULL, /* create_similar */
    _ps_output_finish,
    NULL, /* acquire_source_image */
    NULL, /* release_source_image */
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    _ps_output_composite,
    _ps_output_fill_rectangles,
    _ps_output_composite_trapezoids,
    NULL, /* copy_page */
    NULL, /* show_page */
    NULL, /* set_clip_region */
    _ps_output_intersect_clip_path,
    NULL, /* get_extents */
    _ps_output_show_glyphs,
    _ps_output_fill_path
};

static cairo_int_status_t
_ps_output_render_fallbacks (cairo_surface_t *surface,
			     cairo_surface_t *page)
{
    ps_output_surface_t *ps_output;
    cairo_surface_t *image;
    cairo_int_status_t status;
    cairo_matrix_t matrix;
    int width, height;

    ps_output = (ps_output_surface_t *) surface;
    if (ps_output->fallback_areas == NULL)
	return CAIRO_STATUS_SUCCESS;

    width = ps_output->parent->width * ps_output->parent->x_dpi / 72;
    height = ps_output->parent->height * ps_output->parent->y_dpi / 72;

    image = cairo_image_surface_create (CAIRO_FORMAT_RGB24, width, height);
    if (image->status)
	return CAIRO_STATUS_NO_MEMORY;

    status = _cairo_surface_fill_rectangle (image,
					    CAIRO_OPERATOR_SOURCE,
					    CAIRO_COLOR_WHITE,
					    0, 0, width, height);
    if (status)
	goto bail;

    status = _cairo_meta_surface_replay (page, image);
    if (status)
	goto bail;

    matrix.xx = 1;
    matrix.xy = 0;
    matrix.yx = 0;
    matrix.yy = 1;
    matrix.x0 = 0;
    matrix.y0 = 0;

    status = emit_image (ps_output->parent,
			 (cairo_image_surface_t *) image, &matrix);

 bail:
    cairo_surface_destroy (image);

    return status;
}

static cairo_surface_t *
_ps_output_surface_create (cairo_ps_surface_t *parent)
{
    ps_output_surface_t *ps_output;

    ps_output = malloc (sizeof (ps_output_surface_t));
    if (ps_output == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&ps_output->base, &ps_output_backend);
    ps_output->parent = parent;
    ps_output->fallback_areas = NULL;

    return &ps_output->base;
}

static cairo_int_status_t
_cairo_ps_surface_render_page (cairo_ps_surface_t *surface,
			       cairo_surface_t *page, int page_number)
{
    cairo_surface_t *ps_output;
    cairo_int_status_t status;

    _cairo_output_stream_printf (surface->stream,
				 "%%%%Page: %d\n"
				 "gsave %f %f translate %f %f scale \n",
				 page_number,
				 0.0, surface->height,
				 1.0/surface->base.device_x_scale,
				 -1.0/surface->base.device_y_scale);

    ps_output = _ps_output_surface_create (surface);
    if (ps_output->status)
	return CAIRO_STATUS_NO_MEMORY;

    status = _cairo_meta_surface_replay (page, ps_output);

    _ps_output_render_fallbacks (ps_output, page);

    cairo_surface_destroy (ps_output);

    _cairo_output_stream_printf (surface->stream,
				 "showpage\n"
				 "grestore\n"
				 "%%%%EndPage\n");

    return status;
}

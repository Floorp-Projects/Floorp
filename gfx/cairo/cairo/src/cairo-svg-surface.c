/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
 * Copyright © 2005-2006 Emmanuel Pacaud <emmanuel.pacaud@free.fr>
 * Copyright © 2006 Red Hat, Inc
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
 *	Kristian Høgsberg <krh@redhat.com>
 * 	Emmanuel Pacaud <emmanuel.pacaud@univ-poitiers.fr>
 *	Carl Worth <cworth@cworth.org>
 */

#include "cairoint.h"
#include "cairo-svg.h"
#include "cairo-svg-test.h"
#include "cairo-path-fixed-private.h"
#include "cairo-meta-surface-private.h"
#include "cairo-paginated-surface-private.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-output-stream-private.h"

typedef struct cairo_svg_document cairo_svg_document_t;
typedef struct cairo_svg_surface cairo_svg_surface_t;
typedef struct cairo_svg_page cairo_svg_page_t;

static const int invalid_pattern_id = -1;

static const cairo_svg_version_t _cairo_svg_versions[] =
{
    CAIRO_SVG_VERSION_1_1,
    CAIRO_SVG_VERSION_1_2
};

#define CAIRO_SVG_VERSION_LAST ((int)(sizeof (_cairo_svg_versions) / sizeof (_cairo_svg_versions[0])))

static cairo_bool_t
_cairo_svg_version_has_page_set_support (cairo_svg_version_t version)
{
    return version > CAIRO_SVG_VERSION_1_1;
}

static const char * _cairo_svg_version_strings[CAIRO_SVG_VERSION_LAST] =
{
    "SVG 1.1",
    "SVG 1.2"
};

static const char * _cairo_svg_internal_version_strings[CAIRO_SVG_VERSION_LAST] =
{
    "1.1",
    "1.2"
};

struct cairo_svg_page {
    unsigned int surface_id;
    unsigned int clip_id;
    unsigned int clip_level;
    cairo_output_stream_t *xml_node;
};

struct cairo_svg_document {
    cairo_output_stream_t *output_stream;
    unsigned long refcount;
    cairo_surface_t *owner;
    cairo_bool_t finished;

    double width;
    double height;

    cairo_output_stream_t *xml_node_defs;
    cairo_output_stream_t *xml_node_glyphs;

    unsigned int surface_id;
    unsigned int linear_pattern_id;
    unsigned int radial_pattern_id;
    unsigned int pattern_id;
    unsigned int filter_id;
    unsigned int clip_id;
    unsigned int mask_id;

    cairo_bool_t alpha_filter;

    cairo_array_t meta_snapshots;

    cairo_svg_version_t svg_version;

    cairo_scaled_font_subsets_t *font_subsets;
};

struct cairo_svg_surface {
    cairo_surface_t base;

    cairo_content_t content;

    unsigned int id;

    double width;
    double height;

    cairo_svg_document_t *document;

    cairo_output_stream_t *xml_node;
    cairo_array_t	   page_set;

    unsigned int clip_level;
    unsigned int base_clip;

    cairo_paginated_mode_t paginated_mode;
};

typedef struct {
    unsigned int id;
    cairo_meta_surface_t *meta;
} cairo_meta_snapshot_t;

static cairo_svg_document_t *
_cairo_svg_document_create (cairo_output_stream_t   *stream,
			    double		     width,
			    double		     height,
			    cairo_svg_version_t	     version);

static void
_cairo_svg_document_destroy (cairo_svg_document_t *document);

static cairo_status_t
_cairo_svg_document_finish (cairo_svg_document_t *document);

static cairo_svg_document_t *
_cairo_svg_document_reference (cairo_svg_document_t *document);

static cairo_surface_t *
_cairo_svg_surface_create_for_document (cairo_svg_document_t	*document,
					cairo_content_t		 content,
					double			 width,
					double			 height);
static cairo_surface_t *
_cairo_svg_surface_create_for_stream_internal (cairo_output_stream_t	*stream,
					       double			 width,
					       double			 height,
					       cairo_svg_version_t	 version);

static const cairo_surface_backend_t cairo_svg_surface_backend;
static const cairo_paginated_surface_backend_t cairo_svg_surface_paginated_backend;

/**
 * cairo_svg_surface_create_for_stream:
 * @write_func: a #cairo_write_func_t to accept the output data
 * @closure: the closure argument for @write_func
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 *
 * Creates a SVG surface of the specified size in points to be written
 * incrementally to the stream represented by @write_func and @closure.
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 *
 * Since: 1.2
 */
cairo_surface_t *
cairo_svg_surface_create_for_stream (cairo_write_func_t		 write_func,
				     void			*closure,
				     double			 width,
				     double			 height)
{
    cairo_status_t status;
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create (write_func, NULL, closure);
    status = _cairo_output_stream_get_status (stream);
    if (status) {
	_cairo_error (status);
	return (cairo_surface_t *) &_cairo_surface_nil;
    }

    return _cairo_svg_surface_create_for_stream_internal (stream, width, height, CAIRO_SVG_VERSION_1_1);
}

/**
 * cairo_svg_surface_create:
 * @filename: a filename for the SVG output (must be writable)
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 *
 * Creates a SVG surface of the specified size in points to be written
 * to @filename.
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 *
 * Since: 1.2
 **/
cairo_surface_t *
cairo_svg_surface_create (const char	*filename,
			  double	 width,
			  double	 height)
{
    cairo_status_t status;
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create_for_filename (filename);
    status = _cairo_output_stream_get_status (stream);
    if (status) {
	_cairo_error (status);
	return (status == CAIRO_STATUS_WRITE_ERROR) ?
		(cairo_surface_t *) &_cairo_surface_nil_write_error :
		(cairo_surface_t *) &_cairo_surface_nil;
    }

    return _cairo_svg_surface_create_for_stream_internal (stream, width, height, CAIRO_SVG_VERSION_1_1);
}

static cairo_bool_t
_cairo_surface_is_svg (cairo_surface_t *surface)
{
    return surface->backend == &cairo_svg_surface_backend;
}

/* If the abstract_surface is a paginated surface, and that paginated
 * surface's target is a svg_surface, then set svg_surface to that
 * target. Otherwise return CAIRO_STATUS_SURFACE_TYPE_MISMATCH.
 */
static cairo_status_t
_extract_svg_surface (cairo_surface_t		 *surface,
		      cairo_svg_surface_t	**svg_surface)
{
    cairo_surface_t *target;

    if (! _cairo_surface_is_paginated (surface))
	return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;

    target = _cairo_paginated_surface_get_target (surface);

    if (! _cairo_surface_is_svg (target))
	return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;

    *svg_surface = (cairo_svg_surface_t *) target;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_svg_surface_restrict_to_version:
 * @surface: a SVG #cairo_surface_t
 * @version: SVG version
 *
 * Restricts the generated SVG file to @version. See cairo_svg_get_versions()
 * for a list of available version values that can be used here.
 *
 * This function should only be called before any drawing operations
 * have been performed on the given surface. The simplest way to do
 * this is to call this function immediately after creating the
 * surface.
 *
 * Since: 1.2
 **/
void
cairo_svg_surface_restrict_to_version (cairo_surface_t 		*abstract_surface,
				       cairo_svg_version_t  	 version)
{
    cairo_svg_surface_t *surface;
    cairo_status_t status;

    status = _extract_svg_surface (abstract_surface, &surface);
    if (status) {
	_cairo_surface_set_error (abstract_surface,
				  CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    if (version < CAIRO_SVG_VERSION_LAST)
	surface->document->svg_version = version;
}

/**
 * cairo_svg_get_versions:
 * @versions: supported version list
 * @num_versions: list length
 *
 * Used to retrieve the list of supported versions. See
 * cairo_svg_surface_restrict_to_version().
 *
 * Since: 1.2
 **/
void
cairo_svg_get_versions (cairo_svg_version_t const	**versions,
                        int                     	 *num_versions)
{
    if (versions != NULL)
	*versions = _cairo_svg_versions;

    if (num_versions != NULL)
	*num_versions = CAIRO_SVG_VERSION_LAST;
}

/**
 * cairo_svg_version_to_string:
 * @version: a version id
 *
 * Get the string representation of the given @version id. This function
 * will return NULL if @version isn't valid. See cairo_svg_get_versions()
 * for a way to get the list of valid version ids.
 *
 * Return value: the string associated to given version.
 *
 * Since: 1.2
 **/
const char *
cairo_svg_version_to_string (cairo_svg_version_t version)
{
    if (version >= CAIRO_SVG_VERSION_LAST)
	return NULL;

    return _cairo_svg_version_strings[version];
}

static cairo_surface_t *
_cairo_svg_surface_create_for_document (cairo_svg_document_t	*document,
					cairo_content_t		 content,
					double			 width,
					double			 height)
{
    cairo_svg_surface_t *surface;

    surface = malloc (sizeof (cairo_svg_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&surface->base, &cairo_svg_surface_backend,
			 content);

    surface->width = width;
    surface->height = height;

    surface->document = _cairo_svg_document_reference (document);

    surface->clip_level = 0;

    surface->id = document->surface_id++;
    surface->base_clip = document->clip_id++;

    _cairo_output_stream_printf (document->xml_node_defs,
				 "<clipPath id=\"clip%d\">\n"
				 "  <rect width=\"%f\" height=\"%f\"/>\n"
				 "</clipPath>\n",
				 surface->base_clip,
				 width, height);

    surface->xml_node = _cairo_memory_stream_create ();
    _cairo_array_init (&surface->page_set, sizeof (cairo_svg_page_t));

    if (content == CAIRO_CONTENT_COLOR) {
	_cairo_output_stream_printf (surface->xml_node,
				     "<rect width=\"%f\" height=\"%f\" "
				     "style=\"opacity: 1; stroke: none; "
				     "fill: rgb(0,0,0);\"/>\n",
				     width, height);
    }

    surface->paginated_mode = CAIRO_PAGINATED_MODE_ANALYZE;
    surface->content = content;

    return _cairo_paginated_surface_create (&surface->base,
					    surface->content,
					    surface->width, surface->height,
					    &cairo_svg_surface_paginated_backend);
}

static cairo_surface_t *
_cairo_svg_surface_create_for_stream_internal (cairo_output_stream_t	*stream,
					       double			 width,
					       double			 height,
					       cairo_svg_version_t	 version)
{
    cairo_svg_document_t *document;
    cairo_surface_t *surface;

    document = _cairo_svg_document_create (stream, width, height, version);
    if (document == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t *) &_cairo_surface_nil;
    }

    surface = _cairo_svg_surface_create_for_document (document, CAIRO_CONTENT_COLOR_ALPHA,
						      width, height);

    document->owner = surface;
    _cairo_svg_document_destroy (document);

    return surface;
}

static cairo_svg_page_t *
_cairo_svg_surface_store_page (cairo_svg_surface_t *surface)
{
    unsigned int i;
    cairo_svg_page_t page;

    page.surface_id = surface->id;
    page.clip_id = surface->base_clip;
    page.clip_level = surface->clip_level;
    page.xml_node = surface->xml_node;

    surface->xml_node = _cairo_memory_stream_create ();
    surface->clip_level = 0;

    for (i = 0; i < page.clip_level; i++)
	_cairo_output_stream_printf (page.xml_node, "</g>\n");

    _cairo_array_append (&surface->page_set, &page);

    return _cairo_array_index (&surface->page_set, surface->page_set.num_elements - 1);
}

static cairo_int_status_t
_cairo_svg_surface_copy_page (void *abstract_surface)
{
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_svg_page_t *page;

    page = _cairo_svg_surface_store_page (surface);

    _cairo_memory_stream_copy (page->xml_node, surface->xml_node);
    surface->clip_level = page->clip_level;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_show_page (void *abstract_surface)
{
    cairo_svg_surface_t *surface = abstract_surface;

    _cairo_svg_surface_store_page (surface);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_svg_surface_emit_transform (cairo_output_stream_t *output,
		char const *attribute_str,
		char const *trailer,
		cairo_matrix_t *matrix)
{
    _cairo_output_stream_printf (output,
				 "%s=\"matrix(%f,%f,%f,%f,%f,%f)\"%s",
				 attribute_str,
				 matrix->xx, matrix->yx,
				 matrix->xy, matrix->yy,
				 matrix->x0, matrix->y0,
				 trailer);
}

typedef struct
{
    cairo_output_stream_t *output;
    cairo_matrix_t *ctm_inverse;
} svg_path_info_t;

static cairo_status_t
_cairo_svg_path_move_to (void *closure, cairo_point_t *point)
{
    svg_path_info_t *info = closure;
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (info->ctm_inverse)
	cairo_matrix_transform_point (info->ctm_inverse, &x, &y);

    _cairo_output_stream_printf (info->output, "M %f %f ", x, y);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_path_line_to (void *closure, cairo_point_t *point)
{
    svg_path_info_t *info = closure;
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (info->ctm_inverse)
	cairo_matrix_transform_point (info->ctm_inverse, &x, &y);

    _cairo_output_stream_printf (info->output, "L %f %f ", x, y);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_path_curve_to (void          *closure,
			  cairo_point_t *b,
			  cairo_point_t *c,
			  cairo_point_t *d)
{
    svg_path_info_t *info = closure;
    double bx = _cairo_fixed_to_double (b->x);
    double by = _cairo_fixed_to_double (b->y);
    double cx = _cairo_fixed_to_double (c->x);
    double cy = _cairo_fixed_to_double (c->y);
    double dx = _cairo_fixed_to_double (d->x);
    double dy = _cairo_fixed_to_double (d->y);

    if (info->ctm_inverse) {
	cairo_matrix_transform_point (info->ctm_inverse, &bx, &by);
	cairo_matrix_transform_point (info->ctm_inverse, &cx, &cy);
	cairo_matrix_transform_point (info->ctm_inverse, &dx, &dy);
    }

    _cairo_output_stream_printf (info->output,
 				 "C %f %f %f %f %f %f ",
				 bx, by, cx, cy, dx, dy);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_path_close_path (void *closure)
{
    svg_path_info_t *info = closure;

    _cairo_output_stream_printf (info->output, "Z ");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_surface_emit_path (cairo_output_stream_t *output,
	   cairo_path_fixed_t    *path,
	   cairo_matrix_t	 *ctm_inverse)
{
    cairo_status_t status;
    svg_path_info_t info;

    _cairo_output_stream_printf (output, "d=\"");

    info.output = output;
    info.ctm_inverse = ctm_inverse;
    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_svg_path_move_to,
					  _cairo_svg_path_line_to,
					  _cairo_svg_path_curve_to,
					  _cairo_svg_path_close_path,
					  &info);

    _cairo_output_stream_printf (output, "\"");

    return status;
}

static cairo_int_status_t
_cairo_svg_document_emit_outline_glyph_data (cairo_svg_document_t	*document,
					     cairo_scaled_font_t	*scaled_font,
					     unsigned long		 glyph_index)
{
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_int_status_t status;

    status = _cairo_scaled_glyph_lookup (scaled_font,
					 glyph_index,
					 CAIRO_SCALED_GLYPH_INFO_METRICS|
					 CAIRO_SCALED_GLYPH_INFO_PATH,
					 &scaled_glyph);
    if (status)
	return status;

    _cairo_output_stream_printf (document->xml_node_glyphs,
				 "<path style=\"stroke: none;\" ");

    status = _cairo_svg_surface_emit_path (document->xml_node_glyphs, scaled_glyph->path, NULL);

    _cairo_output_stream_printf (document->xml_node_glyphs,
				 "/>\n");

    return status;
}

static cairo_int_status_t
_cairo_svg_document_emit_bitmap_glyph_data (cairo_svg_document_t	*document,
					    cairo_scaled_font_t		*scaled_font,
					    unsigned long		 glyph_index)
{
    cairo_image_surface_t *image;
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_status_t status;
    unsigned char *row, *byte;
    int rows, cols;
    int x, y, bit;

    status = _cairo_scaled_glyph_lookup (scaled_font,
					 glyph_index,
					 CAIRO_SCALED_GLYPH_INFO_METRICS|
					 CAIRO_SCALED_GLYPH_INFO_SURFACE,
					 &scaled_glyph);
    if (status)
	return status;

    image = scaled_glyph->surface;
    if (image->format != CAIRO_FORMAT_A1) {
	image = _cairo_image_surface_clone (image, CAIRO_FORMAT_A1);
	if (cairo_surface_status (&image->base))
	    return cairo_surface_status (&image->base);
    }

    _cairo_output_stream_printf (document->xml_node_glyphs, "<g");
    _cairo_svg_surface_emit_transform (document->xml_node_glyphs, " transform", ">/n", &image->base.device_transform);

    for (y = 0, row = image->data, rows = image->height; rows; row += image->stride, rows--, y++) {
	for (x = 0, byte = row, cols = (image->width + 7) / 8; cols; byte++, cols--) {
	    unsigned char output_byte = CAIRO_BITSWAP8_IF_LITTLE_ENDIAN (*byte);
	    for (bit = 7; bit >= 0 && x < image->width; bit--, x++) {
		if (output_byte & (1 << bit)) {
		    _cairo_output_stream_printf (document->xml_node_glyphs,
						 "<rect x=\"%d\" y=\"%d\" width=\"1\" height=\"1\"/>\n",
						 x, y);
		}
	    }
	}
    }
    _cairo_output_stream_printf (document->xml_node_glyphs, "</g>\n");

    if (image != scaled_glyph->surface)
	cairo_surface_destroy (&image->base);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_svg_document_emit_glyph (cairo_svg_document_t	*document,
				cairo_scaled_font_t	*scaled_font,
				unsigned long		 scaled_font_glyph_index,
				unsigned int		 font_id,
				unsigned int		 subset_glyph_index)
{
    cairo_status_t	     status;

    _cairo_output_stream_printf (document->xml_node_glyphs,
				 "<symbol overflow=\"visible\" id=\"glyph%d-%d\">\n",
 				 font_id,
 				 subset_glyph_index);

    status = _cairo_svg_document_emit_outline_glyph_data (document,
							  scaled_font,
							  scaled_font_glyph_index);
    if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	status = _cairo_svg_document_emit_bitmap_glyph_data (document,
							     scaled_font,
							     scaled_font_glyph_index);

    _cairo_output_stream_printf (document->xml_node_glyphs, "</symbol>\n");
}

static void
_cairo_svg_document_emit_font_subset (cairo_scaled_font_subset_t	*font_subset,
				      void				*closure)
{
    cairo_svg_document_t *document = closure;
    unsigned int i;

    for (i = 0; i < font_subset->num_glyphs; i++) {
	_cairo_svg_document_emit_glyph (document,
					font_subset->scaled_font,
					font_subset->glyphs[i],
					font_subset->font_id, i);
    }
}

static void
_cairo_svg_document_emit_font_subsets (cairo_svg_document_t *document)
{
    _cairo_scaled_font_subsets_foreach_scaled (document->font_subsets,
                                               _cairo_svg_document_emit_font_subset,
                                               document);
    _cairo_scaled_font_subsets_destroy (document->font_subsets);
    document->font_subsets = NULL;
}

static cairo_bool_t cairo_svg_force_fallbacks = FALSE;

/**
 * _cairo_svg_test_force_fallbacks
 *
 * Force the SVG surface backend to use image fallbacks for every
 * operation.
 *
 * <note>
 * This function is <emphasis>only</emphasis> intended for internal
 * testing use within the cairo distribution. It is not installed in
 * any public header file.
 * </note>
 **/
void
_cairo_svg_test_force_fallbacks (void)
{
    cairo_svg_force_fallbacks = TRUE;
}

static cairo_int_status_t
__cairo_svg_surface_operation_supported (cairo_svg_surface_t *surface,
		      cairo_operator_t op,
		      const cairo_pattern_t *pattern)
{
    cairo_svg_document_t *document = surface->document;

    if (cairo_svg_force_fallbacks)
	return FALSE;

    if (document->svg_version < CAIRO_SVG_VERSION_1_2)
	if (op != CAIRO_OPERATOR_OVER)
	    return FALSE;

    return TRUE;
}

static cairo_int_status_t
_cairo_svg_surface_analyze_operation (cairo_svg_surface_t *surface,
		    cairo_operator_t op,
		    const cairo_pattern_t *pattern)
{
    if (__cairo_svg_surface_operation_supported (surface, op, pattern))
	return CAIRO_STATUS_SUCCESS;
    else
	return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_surface_t *
_cairo_svg_surface_create_similar (void			*abstract_src,
				   cairo_content_t	 content,
				   int			 width,
				   int			 height)
{
    return _cairo_meta_surface_create (content, width, height);
}

static cairo_status_t
_cairo_svg_surface_finish (void *abstract_surface)
{
    cairo_status_t status;
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_svg_document_t *document = surface->document;
    cairo_svg_page_t *page;
    unsigned int i;

    if (_cairo_paginated_surface_get_target (document->owner) == &surface->base)
	status = _cairo_svg_document_finish (document);
    else
	status = CAIRO_STATUS_SUCCESS;

    _cairo_output_stream_destroy (surface->xml_node);

    for (i = 0; i < surface->page_set.num_elements; i++) {
	page = _cairo_array_index (&surface->page_set, i);
	_cairo_output_stream_destroy (page->xml_node);
    }
    _cairo_array_fini (&surface->page_set);

    _cairo_svg_document_destroy (document);

    return status;
}

static void
_cairo_svg_surface_emit_alpha_filter (cairo_svg_document_t *document)
{
    if (document->alpha_filter)
 	return;

    _cairo_output_stream_printf (document->xml_node_defs,
 				 "<filter id=\"alpha\" "
 				 "filterUnits=\"objectBoundingBox\" "
 				 "x=\"0%%\" y=\"0%%\" "
 				 "width=\"100%%\" height=\"100%%\">\n"
 				 "  <feColorMatrix type=\"matrix\" "
 				 "in=\"SourceGraphic\" "
 				 "values=\"0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 0 0 0 1 0\"/>\n"
 				 "</filter>\n");

    document->alpha_filter = TRUE;
}

typedef struct {
    cairo_output_stream_t *output;
    unsigned int in_mem;
    unsigned char src[3];
    unsigned char dst[5];
    unsigned int trailing;
} base64_write_closure_t;

static char const *base64_table =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static cairo_status_t
base64_write_func (void *closure,
		   const unsigned char *data,
		   unsigned int length)
{
    base64_write_closure_t *info = (base64_write_closure_t *) closure;
    unsigned int i;
    unsigned char *src, *dst;

    dst = info->dst;
    src = info->src;

    if (info->in_mem + length < 3) {
	for (i = 0; i < length; i++) {
	    src[i + info->in_mem] = *data;
	    data++;
	}
	info->in_mem += length;
	return CAIRO_STATUS_SUCCESS;
    }

    while (info->in_mem + length >= 3) {
	for (i = 0; i < 3 - info->in_mem; i++) {
	    src[i + info->in_mem] = *data;
	    data++;
	    length--;
	}
	dst[0] = base64_table[src[0] >> 2];
	dst[1] = base64_table[(src[0] & 0x03) << 4 | src[1] >> 4];
	dst[2] = base64_table[(src[1] & 0x0f) << 2 | src[2] >> 6];
	dst[3] = base64_table[src[2] & 0xfc >> 2];
	/* Special case for the last missing bits */
	switch (info->trailing) {
	    case 2:
		dst[2] = '=';
	    case 1:
		dst[3] = '=';
	    default:
		break;
	}
	_cairo_output_stream_write (info->output, dst, 4);
	info->in_mem = 0;
    }

    for (i = 0; i < length; i++) {
	src[i] = *data;
	data++;
    }
    info->in_mem = length;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_surface_base64_encode (cairo_surface_t       *surface,
			      cairo_output_stream_t *output)
{
    cairo_status_t status;
    base64_write_closure_t info;
    unsigned int i;

    info.output = output;
    info.in_mem = 0;
    info.trailing = 0;
    memset (info.dst, '\x0', 5);

    _cairo_output_stream_printf (info.output, "data:image/png;base64,");

    status = cairo_surface_write_to_png_stream (surface, base64_write_func,
						(void *) &info);

    if (status)
	return status;

    if (info.in_mem > 0) {
	for (i = info.in_mem; i < 3; i++)
	    info.src[i] = '\x0';
	info.trailing = 3 - info.in_mem;
	info.in_mem = 3;
	base64_write_func (&info, NULL, 0);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_surface_emit_composite_image_pattern (cairo_output_stream_t     *output,
			      cairo_svg_surface_t	*svg_surface,
			      cairo_surface_pattern_t 	*pattern,
			      int	 		 pattern_id,
			      const char		*extra_attributes)
{
    cairo_surface_t *surface;
    cairo_surface_attributes_t surface_attr;
    cairo_rectangle_int16_t extents;
    cairo_status_t status;
    cairo_matrix_t p2u;

    status = _cairo_pattern_acquire_surface ((cairo_pattern_t *)pattern,
					     (cairo_surface_t *)svg_surface,
					     0, 0, (unsigned int)-1, (unsigned int)-1,
					     &surface, &surface_attr);
    if (status)
	return status;

    status = _cairo_surface_get_extents (surface, &extents);

    if (status)
	return status;

    p2u = pattern->base.matrix;
    cairo_matrix_invert (&p2u);

    if (pattern_id != invalid_pattern_id) {
	_cairo_output_stream_printf (output,
				     "<pattern id=\"pattern%d\" "
				     "patternUnits=\"userSpaceOnUse\" "
				     "width=\"%d\" height=\"%d\"",
				     pattern_id,
				     extents.width, extents.height);
	_cairo_svg_surface_emit_transform (output, " patternTransform", ">\n", &p2u);
    }

    _cairo_output_stream_printf (output,
				 "  <image width=\"%d\" height=\"%d\"",
				 extents.width, extents.height);

    if (pattern_id == invalid_pattern_id)
	_cairo_svg_surface_emit_transform (output, " transform", "", &p2u);

    if (extra_attributes)
	_cairo_output_stream_printf (output, " %s", extra_attributes);

    _cairo_output_stream_printf (output, " xlink:href=\"");

    status = _cairo_surface_base64_encode (surface, output);

    _cairo_output_stream_printf (output, "\"/>\n");

    if (pattern_id != invalid_pattern_id)
	_cairo_output_stream_printf (output, "</pattern>\n");

    _cairo_pattern_release_surface ((cairo_pattern_t *)pattern,
				    surface, &surface_attr);

    return status;
}

static int
_cairo_svg_surface_emit_meta_surface (cairo_svg_document_t *document,
		   cairo_meta_surface_t *surface)
{
    cairo_surface_t *paginated_surface;
    cairo_surface_t *svg_surface;
    cairo_meta_snapshot_t new_snapshot;
    cairo_array_t *page_set;

    cairo_output_stream_t *contents;
    cairo_meta_surface_t *meta;
    cairo_meta_snapshot_t *snapshot;
    unsigned int num_elements;
    unsigned int i, id;

    /* search in already emitted meta snapshots */
    num_elements = document->meta_snapshots.num_elements;
    for (i = 0; i < num_elements; i++) {
	snapshot = _cairo_array_index (&document->meta_snapshots, i);
	meta = snapshot->meta;
	if (meta->commands.num_elements == surface->commands.num_elements &&
	    _cairo_array_index (&meta->commands, 0) == _cairo_array_index (&surface->commands, 0)) {
	    return snapshot->id;
	}
    }

    meta = (cairo_meta_surface_t *) _cairo_surface_snapshot ((cairo_surface_t *)surface);
    paginated_surface = _cairo_svg_surface_create_for_document (document,
								meta->content,
								meta->width_pixels,
								meta->height_pixels);
    svg_surface = _cairo_paginated_surface_get_target (paginated_surface);
    cairo_surface_set_fallback_resolution (paginated_surface,
					   document->owner->x_fallback_resolution,
					   document->owner->y_fallback_resolution);
    _cairo_meta_surface_replay ((cairo_surface_t *)meta, paginated_surface);
    _cairo_surface_show_page (paginated_surface);

    new_snapshot.meta = meta;
    new_snapshot.id = ((cairo_svg_surface_t *) svg_surface)->id;
    _cairo_array_append (&document->meta_snapshots, &new_snapshot);

    if (meta->content == CAIRO_CONTENT_ALPHA) {
	_cairo_svg_surface_emit_alpha_filter (document);
	_cairo_output_stream_printf (document->xml_node_defs,
				     "<g id=\"surface%d\" "
				     "clip-path=\"url(#clip%d)\" "
				     "filter=\"url(#alpha)\">\n",
				     ((cairo_svg_surface_t *) svg_surface)->id,
				     ((cairo_svg_surface_t *) svg_surface)->base_clip);
    } else {
	_cairo_output_stream_printf (document->xml_node_defs,
				     "<g id=\"surface%d\" "
				     "clip-path=\"url(#clip%d)\">\n",
				     ((cairo_svg_surface_t *) svg_surface)->id,
				     ((cairo_svg_surface_t *) svg_surface)->base_clip);
    }

    contents = ((cairo_svg_surface_t *) svg_surface)->xml_node;
    page_set = &((cairo_svg_surface_t *) svg_surface)->page_set;

    if (_cairo_memory_stream_length (contents) > 0)
	_cairo_svg_surface_store_page ((cairo_svg_surface_t *) svg_surface);

    if (page_set->num_elements > 0) {
	cairo_svg_page_t *page;

	page = _cairo_array_index (page_set, page_set->num_elements - 1);
	_cairo_memory_stream_copy (page->xml_node, document->xml_node_defs);
    }

    _cairo_output_stream_printf (document->xml_node_defs, "</g>\n");

    id = new_snapshot.id;

    cairo_surface_destroy (paginated_surface);

    /* FIXME: cairo_paginated_surface doesn't take a ref to the
     * passed in target surface so we can't call destroy here.
     * cairo_paginated_surface should be fixed, but for now just
     * work around it. */

    /* cairo_surface_destroy (svg_surface); */

    return id;
}

static cairo_status_t
_cairo_svg_surface_emit_composite_meta_pattern (cairo_output_stream_t	*output,
			     cairo_svg_surface_t	*surface,
			     cairo_surface_pattern_t	*pattern,
			     int			 pattern_id,
			     const char			*extra_attributes)
{
    cairo_svg_document_t *document = surface->document;
    cairo_meta_surface_t *meta_surface;
    cairo_matrix_t p2u;
    int id;

    meta_surface = (cairo_meta_surface_t *) pattern->surface;

    id = _cairo_svg_surface_emit_meta_surface (document, meta_surface);

    p2u = pattern->base.matrix;
    cairo_matrix_invert (&p2u);

    if (pattern_id != invalid_pattern_id) {
	_cairo_output_stream_printf (output,
				     "<pattern id=\"pattern%d\" "
				     "patternUnits=\"userSpaceOnUse\" "
				     "width=\"%d\" height=\"%d\"",
				     pattern_id,
				     meta_surface->width_pixels,
				     meta_surface->height_pixels);
	_cairo_svg_surface_emit_transform (output, " patternTransform", ">\n", &p2u);
    }

    _cairo_output_stream_printf (output,
				 "<use xlink:href=\"#surface%d\"",
				 id);

    if (pattern_id == invalid_pattern_id)
	_cairo_svg_surface_emit_transform (output, " transform", "", &p2u);

    if (extra_attributes)
	_cairo_output_stream_printf (output, " %s", extra_attributes);

    _cairo_output_stream_printf (output, "/>\n");

    if (pattern_id != invalid_pattern_id)
	_cairo_output_stream_printf (output, "</pattern>\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_surface_emit_composite_pattern (cairo_output_stream_t   *output,
			cairo_svg_surface_t	*surface,
			cairo_surface_pattern_t *pattern,
			int			 pattern_id,
			const char		*extra_attributes)
{

    if (_cairo_surface_is_meta (pattern->surface)) {
	return _cairo_svg_surface_emit_composite_meta_pattern (output, surface, pattern,
					    pattern_id, extra_attributes);
    }

    return _cairo_svg_surface_emit_composite_image_pattern (output, surface, pattern,
					 pattern_id, extra_attributes);
}

static void
_cairo_svg_surface_emit_operator (cairo_output_stream_t *output,
	       cairo_svg_surface_t   *surface,
	      cairo_operator_t	      op)
{
    char const *op_str[] = {
	"clear",

	"src",		"src-over",	"src-in",
	"src-out",	"src-atop",

	"dst",		"dst-over",	"dst-in",
	"dst-out",	"dst-atop",

	"xor", "plus",
	"color-dodge"	/* FIXME: saturate ? */
    };

    if (surface->document->svg_version >= CAIRO_SVG_VERSION_1_2)
	_cairo_output_stream_printf (output, "comp-op: %s; ", op_str[op]);
}

static void
_cairo_svg_surface_emit_solid_pattern (cairo_svg_surface_t	    *surface,
		    cairo_solid_pattern_t   *pattern,
		    cairo_output_stream_t   *style,
		    cairo_bool_t	     is_stroke)
{
    _cairo_output_stream_printf (style,
				 "%s: rgb(%f%%,%f%%,%f%%); "
				 "opacity: %f;",
				 is_stroke ? "stroke" : "fill",
				 pattern->color.red * 100.0,
				 pattern->color.green * 100.0,
				 pattern->color.blue * 100.0,
				 pattern->color.alpha);
}

static void
_cairo_svg_surface_emit_surface_pattern (cairo_svg_surface_t	*surface,
		      cairo_surface_pattern_t	*pattern,
		      cairo_output_stream_t     *style,
		      cairo_bool_t		 is_stroke)
{
    cairo_svg_document_t *document = surface->document;
    int pattern_id;

    pattern_id = document->pattern_id++;
    _cairo_svg_surface_emit_composite_pattern (document->xml_node_defs, surface, pattern,
			    pattern_id, NULL);

    _cairo_output_stream_printf (style,
				 "%s: url(#pattern%d);",
				 is_stroke ? "color" : "fill",
				 pattern_id);
}

static void
_cairo_svg_surface_emit_pattern_stops (cairo_output_stream_t *output,
		    cairo_gradient_pattern_t const *pattern,
		    double start_offset,
		    cairo_bool_t reverse_stops,
		    cairo_bool_t emulate_reflect)
{
    pixman_gradient_stop_t *stops;
    double offset;
    unsigned int n_stops;
    unsigned int i;

    if (pattern->n_stops < 1)
	return;

    if (pattern->n_stops == 1) {
	    _cairo_output_stream_printf (output,
					 "<stop offset=\"%f\" style=\""
					 "stop-color: rgb(%f%%,%f%%,%f%%); "
					 "stop-opacity: %f;\"/>\n",
					 _cairo_fixed_to_double (pattern->stops[0].x),
					 pattern->stops[0].color.red   / 655.35,
					 pattern->stops[0].color.green / 655.35,
					 pattern->stops[0].color.blue  / 655.35,
					 pattern->stops[0].color.alpha / 65535.0);
	    return;
    }

    if (emulate_reflect || reverse_stops) {
	n_stops = emulate_reflect ? pattern->n_stops * 2 - 2: pattern->n_stops;
	stops = malloc (sizeof (pixman_gradient_stop_t) * n_stops);

	for (i = 0; i < pattern->n_stops; i++) {
	    if (reverse_stops) {
		stops[i] = pattern->stops[pattern->n_stops - i - 1];
		stops[i].x = _cairo_fixed_from_double (1.0 - _cairo_fixed_to_double (stops[i].x));
	    } else
		stops[i] = pattern->stops[i];
	    if (emulate_reflect) {
		stops[i].x /= 2;
		if (i > 0 && i < (pattern->n_stops - 1)) {
		    if (reverse_stops) {
			stops[i + pattern->n_stops - 1] = pattern->stops[i];
			stops[i + pattern->n_stops - 1].x =
			    _cairo_fixed_from_double (0.5 + 0.5
				* _cairo_fixed_to_double (stops[i + pattern->n_stops - 1].x));
		    } else {
			stops[i + pattern->n_stops - 1] = pattern->stops[pattern->n_stops - i - 1];
			stops[i + pattern->n_stops - 1].x =
			    _cairo_fixed_from_double (1 - 0.5
				* _cairo_fixed_to_double (stops [i + pattern->n_stops - 1].x));
		    }
		}
	    }
	}
    } else {
	n_stops = pattern->n_stops;
	stops = pattern->stops;
    }

    if (start_offset >= 0.0)
	for (i = 0; i < n_stops; i++) {
	    offset = start_offset + (1 - start_offset ) *
		_cairo_fixed_to_double (stops[i].x);
	    _cairo_output_stream_printf (output,
					 "<stop offset=\"%f\" style=\""
					 "stop-color: rgb(%f%%,%f%%,%f%%); "
					 "stop-opacity: %f;\"/>\n",
					 offset,
					 stops[i].color.red   / 655.35,
					 stops[i].color.green / 655.35,
					 stops[i].color.blue  / 655.35,
					 stops[i].color.alpha / 65535.0);
	}
    else {
	cairo_bool_t found = FALSE;
	unsigned int offset_index;
	pixman_color_t offset_color_start, offset_color_stop;

	for (i = 0; i < n_stops; i++) {
	    if (_cairo_fixed_to_double (stops[i].x) >= -start_offset) {
		if (i > 0) {
		    if (stops[i].x != stops[i-1].x) {
			double x0, x1;
			pixman_color_t *color0, *color1;

			x0 = _cairo_fixed_to_double (stops[i-1].x);
			x1 = _cairo_fixed_to_double (stops[i].x);
			color0 = &stops[i-1].color;
			color1 = &stops[i].color;
			offset_color_start.red = color0->red + (color1->red - color0->red)
			    * (-start_offset - x0) / (x1 - x0);
			offset_color_start.green = color0->green + (color1->green - color0->green)
			    * (-start_offset - x0) / (x1 - x0);
			offset_color_start.blue = color0->blue + (color1->blue - color0->blue)
			    * (-start_offset - x0) / (x1 - x0);
			offset_color_start.alpha = color0->alpha + (color1->alpha - color0->alpha)
			    * (-start_offset - x0) / (x1 - x0);
			offset_color_stop = offset_color_start;
		    } else {
			offset_color_stop = stops[i-1].color;
			offset_color_start = stops[i].color;
		    }
		} else
			offset_color_stop = offset_color_start = stops[i].color;
	    offset_index = i;
	    found = TRUE;
	    break;
	    }
	}

	if (!found) {
	    offset_index = n_stops - 1;
	    offset_color_stop = offset_color_start = stops[offset_index].color;
	}

	_cairo_output_stream_printf (output,
				     "<stop offset=\"0\" style=\""
				     "stop-color: rgb(%f%%,%f%%,%f%%); "
				     "stop-opacity: %f;\"/>\n",
				     offset_color_start.red   / 655.35,
				     offset_color_start.green / 655.35,
				     offset_color_start.blue  / 655.35,
				     offset_color_start.alpha / 65535.0);
	for (i = offset_index; i < n_stops; i++) {
	    _cairo_output_stream_printf (output,
					 "<stop offset=\"%f\" style=\""
					 "stop-color: rgb(%f%%,%f%%,%f%%); "
					 "stop-opacity: %f;\"/>\n",
					 _cairo_fixed_to_double (stops[i].x) + start_offset,
					 stops[i].color.red   / 655.35,
					 stops[i].color.green / 655.35,
					 stops[i].color.blue  / 655.35,
					 stops[i].color.alpha / 65535.0);
	}
	for (i = 0; i < offset_index; i++) {
	    _cairo_output_stream_printf (output,
					 "<stop offset=\"%f\" style=\""
					 "stop-color: rgb(%f%%,%f%%,%f%%); "
					 "stop-opacity: %f;\"/>\n",
					 1.0 + _cairo_fixed_to_double (stops[i].x) + start_offset,
					 stops[i].color.red   / 655.35,
					 stops[i].color.green / 655.35,
					 stops[i].color.blue  / 655.35,
					 stops[i].color.alpha / 65535.0);
	}

	_cairo_output_stream_printf (output,
				     "<stop offset=\"1\" style=\""
				     "stop-color: rgb(%f%%,%f%%,%f%%); "
				     "stop-opacity: %f;\"/>\n",
				     offset_color_stop.red   / 655.35,
				     offset_color_stop.green / 655.35,
				     offset_color_stop.blue  / 655.35,
				     offset_color_stop.alpha / 65535.0);

    }

    if (reverse_stops || emulate_reflect)
	free (stops);
}

static void
_cairo_svg_surface_emit_pattern_extend (cairo_output_stream_t *output,
		     cairo_pattern_t       *pattern)
{
    switch (pattern->extend) {
	case CAIRO_EXTEND_REPEAT:
	    _cairo_output_stream_printf (output, "spreadMethod=\"repeat\" ");
	    break;
	case CAIRO_EXTEND_REFLECT:
	    _cairo_output_stream_printf (output, "spreadMethod=\"reflect\" ");
	    break;
	case CAIRO_EXTEND_NONE:
	case CAIRO_EXTEND_PAD:
	    break;
    }
}

static void
_cairo_svg_surface_emit_linear_pattern (cairo_svg_surface_t    *surface,
		     cairo_linear_pattern_t *pattern,
		     cairo_output_stream_t  *style,
		     cairo_bool_t	     is_stroke)
{
    cairo_svg_document_t *document = surface->document;
    double x0, y0, x1, y1;
    cairo_matrix_t p2u;

    x0 = _cairo_fixed_to_double (pattern->gradient.p1.x);
    y0 = _cairo_fixed_to_double (pattern->gradient.p1.y);
    x1 = _cairo_fixed_to_double (pattern->gradient.p2.x);
    y1 = _cairo_fixed_to_double (pattern->gradient.p2.y);

    _cairo_output_stream_printf (document->xml_node_defs,
				 "<linearGradient id=\"linear%d\" "
				 "gradientUnits=\"userSpaceOnUse\" "
				 "x1=\"%f\" y1=\"%f\" x2=\"%f\" y2=\"%f\" ",
				 document->linear_pattern_id,
				 x0, y0, x1, y1);

    _cairo_svg_surface_emit_pattern_extend (document->xml_node_defs, &pattern->base.base),
    p2u = pattern->base.base.matrix;
    cairo_matrix_invert (&p2u);
    _cairo_svg_surface_emit_transform (document->xml_node_defs, "gradientTransform", ">\n", &p2u);

    _cairo_svg_surface_emit_pattern_stops (document->xml_node_defs ,&pattern->base, 0.0, FALSE, FALSE);

    _cairo_output_stream_printf (document->xml_node_defs,
				 "</linearGradient>\n");

    _cairo_output_stream_printf (style,
				 "%s: url(#linear%d);",
				 is_stroke ? "color" : "fill",
				 document->linear_pattern_id);

    document->linear_pattern_id++;
}

static void
_cairo_svg_surface_emit_radial_pattern (cairo_svg_surface_t    *surface,
		     cairo_radial_pattern_t *pattern,
		     cairo_output_stream_t  *style,
		     cairo_bool_t            is_stroke)
{
    cairo_svg_document_t *document = surface->document;
    cairo_matrix_t p2u;
    cairo_extend_t extend;
    double x0, y0, x1, y1, r0, r1;
    double fx, fy;
    cairo_bool_t reverse_stops;
    pixman_circle_t *c0, *c1;

    extend = pattern->base.base.extend;

    if (pattern->gradient.c1.radius < pattern->gradient.c2.radius) {
	c0 = &pattern->gradient.c1;
	c1 = &pattern->gradient.c2;
	reverse_stops = FALSE;
    } else {
	c0 = &pattern->gradient.c2;
	c1 = &pattern->gradient.c1;
	reverse_stops = TRUE;
    }

    x0 = _cairo_fixed_to_double (c0->x);
    y0 = _cairo_fixed_to_double (c0->y);
    r0 = _cairo_fixed_to_double (c0->radius);
    x1 = _cairo_fixed_to_double (c1->x);
    y1 = _cairo_fixed_to_double (c1->y);
    r1 = _cairo_fixed_to_double (c1->radius);

    p2u = pattern->base.base.matrix;
    cairo_matrix_invert (&p2u);

    if (pattern->gradient.c1.radius == pattern->gradient.c2.radius) {
	_cairo_output_stream_printf (document->xml_node_defs,
				     "<radialGradient id=\"radial%d\" "
				     "gradientUnits=\"userSpaceOnUse\" "
				     "cx=\"%f\" cy=\"%f\" "
				     "fx=\"%f\" fy=\"%f\" r=\"%f\" ",
				     document->radial_pattern_id,
				     x1, y1,
				     x1, y1, r1);

	_cairo_svg_surface_emit_transform (document->xml_node_defs, "gradientTransform", ">\n", &p2u);

	if (extend == CAIRO_EXTEND_NONE ||
	    pattern->base.n_stops < 1)
	    _cairo_output_stream_printf (document->xml_node_defs,
					 "<stop offset=\"0\" style=\""
					 "stop-color: rgb(0%%,0%%,0%%); "
					 "stop-opacity: 0;\"/>\n");
	else {
	    _cairo_output_stream_printf (document->xml_node_defs,
					 "<stop offset=\"0\" style=\""
					 "stop-color: rgb(%f%%,%f%%,%f%%); "
					 "stop-opacity: %f;\"/>\n",
					 pattern->base.stops[0].color.red   / 655.35,
					 pattern->base.stops[0].color.green / 655.35,
					 pattern->base.stops[0].color.blue  / 655.35,
					 pattern->base.stops[0].color.alpha / 65535.0);
	    if (pattern->base.n_stops > 1)
		_cairo_output_stream_printf (document->xml_node_defs,
					     "<stop offset=\"0\" style=\""
					     "stop-color: rgb(%f%%,%f%%,%f%%); "
					     "stop-opacity: %f;\"/>\n",
					     pattern->base.stops[1].color.red   / 655.35,
					     pattern->base.stops[1].color.green / 655.35,
					     pattern->base.stops[1].color.blue  / 655.35,
					     pattern->base.stops[1].color.alpha / 65535.0);
	}

    } else {
	double offset, r, x, y;
	cairo_bool_t emulate_reflect = FALSE;

	fx = (r1 * x0 - r0 * x1) / (r1 - r0);
	fy = (r1 * y0 - r0 * y1) / (r1 - r0);

	/* SVG doesn't support the inner circle and use instead a gradient focal.
	 * That means we need to emulate the cairo behaviour by processing the
	 * cairo gradient stops.
	 * The CAIRO_EXTENT_NONE and CAIRO_EXTENT_PAD modes are quite easy to handle,
	 * it's just a matter of stop position translation and calculation of
	 * the corresponding SVG radial gradient focal.
	 * The CAIRO_EXTENT_REFLECT and CAIRO_EXTEND_REPEAT modes require to compute a new
	 * radial gradient, with an new outer circle, equal to r1 - r0 in the CAIRO_EXTEND_REPEAT
	 * case, and 2 * (r1 - r0) in the CAIRO_EXTENT_REFLECT case, and a new gradient stop
	 * list that maps to the original cairo stop list.
	 */
	if ((extend == CAIRO_EXTEND_REFLECT
	     || extend == CAIRO_EXTEND_REPEAT)
	    && r0 > 0.0) {
	    double r_org = r1;

	    if (extend == CAIRO_EXTEND_REFLECT) {
		r1 = 2 * r1 - r0;
		emulate_reflect = TRUE;
	    }

	    offset = fmod (r1, r1 - r0) / (r1 - r0) - 1.0;
	    r = r1 - r0;

	    /* New position of outer circle. */
	    x = r * (x1 - fx) / r_org + fx;
	    y = r * (y1 - fy) / r_org + fy;

	    x1 = x;
	    y1 = y;
	    r1 = r;
	    r0 = 0.0;
	} else {
	    offset = r0 / r1;
	}

	_cairo_output_stream_printf (document->xml_node_defs,
				     "<radialGradient id=\"radial%d\" "
				     "gradientUnits=\"userSpaceOnUse\" "
				     "cx=\"%f\" cy=\"%f\" "
				     "fx=\"%f\" fy=\"%f\" r=\"%f\" ",
				     document->radial_pattern_id,
				     x1, y1,
				     fx, fy, r1);

	if (emulate_reflect)
	    _cairo_output_stream_printf (document->xml_node_defs, "spreadMethod=\"repeat\" ");
	else
	    _cairo_svg_surface_emit_pattern_extend (document->xml_node_defs, &pattern->base.base);
	_cairo_svg_surface_emit_transform (document->xml_node_defs, "gradientTransform", ">\n", &p2u);

	/* To support cairo's EXTEND_NONE, (for which SVG has no similar
	 * notion), we add transparent color stops on either end of the
	 * user-provided stops. */
	if (extend == CAIRO_EXTEND_NONE) {
	    _cairo_output_stream_printf (document->xml_node_defs,
					 "<stop offset=\"0\" style=\""
					 "stop-color: rgb(0%%,0%%,0%%); "
					 "stop-opacity: 0;\"/>\n");
	    if (r0 != 0.0)
		_cairo_output_stream_printf (document->xml_node_defs,
					     "<stop offset=\"%f\" style=\""
					     "stop-color: rgb(0%%,0%%,0%%); "
					     "stop-opacity: 0;\"/>\n",
					     r0 / r1);
	}
	_cairo_svg_surface_emit_pattern_stops (document->xml_node_defs, &pattern->base, offset,
			    reverse_stops, emulate_reflect);
	if (pattern->base.base.extend == CAIRO_EXTEND_NONE)
	    _cairo_output_stream_printf (document->xml_node_defs,
					 "<stop offset=\"1.0\" style=\""
					 "stop-color: rgb(0%%,0%%,0%%); "
					 "stop-opacity: 0;\"/>\n");
    }

    _cairo_output_stream_printf (document->xml_node_defs,
				 "</radialGradient>\n");

    _cairo_output_stream_printf (style,
				 "%s: url(#radial%d);",
				 is_stroke ? "color" : "fill",
				 document->radial_pattern_id);

    document->radial_pattern_id++;
}

static void
_cairo_svg_surface_emit_pattern (cairo_svg_surface_t *surface, cairo_pattern_t *pattern,
	      cairo_output_stream_t *output, cairo_bool_t is_stroke)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	_cairo_svg_surface_emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern, output, is_stroke);
	break;

    case CAIRO_PATTERN_TYPE_SURFACE:
	_cairo_svg_surface_emit_surface_pattern (surface, (cairo_surface_pattern_t *) pattern, output, is_stroke);
	break;

    case CAIRO_PATTERN_TYPE_LINEAR:
	_cairo_svg_surface_emit_linear_pattern (surface, (cairo_linear_pattern_t *) pattern, output, is_stroke);
	break;

    case CAIRO_PATTERN_TYPE_RADIAL:
	_cairo_svg_surface_emit_radial_pattern (surface, (cairo_radial_pattern_t *) pattern, output, is_stroke);
	break;
    }
}

static cairo_int_status_t
_cairo_svg_surface_fill (void			*abstract_surface,
			 cairo_operator_t	 op,
			 cairo_pattern_t	*source,
			 cairo_path_fixed_t	*path,
			 cairo_fill_rule_t	 fill_rule,
			 double			 tolerance,
			 cairo_antialias_t	 antialias)
{
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_svg_surface_analyze_operation (surface, op, source);

    assert (__cairo_svg_surface_operation_supported (surface, op, source));

    _cairo_output_stream_printf (surface->xml_node,
 				 "<path style=\"stroke: none; "
 				 "fill-rule: %s; ",
 				 fill_rule == CAIRO_FILL_RULE_EVEN_ODD ?
 				 "evenodd" : "nonzero");
    _cairo_svg_surface_emit_operator (surface->xml_node, surface, op);
    _cairo_svg_surface_emit_pattern (surface, source, surface->xml_node, FALSE);
    _cairo_output_stream_printf (surface->xml_node, "\" ");

    status = _cairo_svg_surface_emit_path (surface->xml_node, path, NULL);

    _cairo_output_stream_printf (surface->xml_node, "/>\n");

    return status;
}

static cairo_int_status_t
_cairo_svg_surface_get_extents (void		        *abstract_surface,
				cairo_rectangle_int16_t *rectangle)
{
    cairo_svg_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;

    /* XXX: The conversion to integers here is pretty bogus, (not to
     * mention the aribitray limitation of width to a short(!). We
     * may need to come up with a better interface for get_size.
     */
    rectangle->width  = (int) ceil (surface->width);
    rectangle->height = (int) ceil (surface->height);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_surface_emit_paint (cairo_output_stream_t *output,
	    cairo_svg_surface_t   *surface,
	    cairo_operator_t	   op,
	    cairo_pattern_t	  *source,
	    const char		  *extra_attributes)
{
    if (source->type == CAIRO_PATTERN_TYPE_SURFACE &&
	source->extend == CAIRO_EXTEND_NONE)
	return _cairo_svg_surface_emit_composite_pattern (output,
				       surface,
				       (cairo_surface_pattern_t *) source,
				       invalid_pattern_id,
				       extra_attributes);

    _cairo_output_stream_printf (output,
				 "<rect x=\"0\" y=\"0\" "
				 "width=\"%f\" height=\"%f\" "
				 "style=\"",
				 surface->width, surface->height);
    _cairo_svg_surface_emit_operator (output, surface, op);
    _cairo_svg_surface_emit_pattern (surface, source, output, FALSE);
    _cairo_output_stream_printf (output, " stroke: none;\"");

    if (extra_attributes)
	_cairo_output_stream_printf (output, " %s", extra_attributes);

    _cairo_output_stream_printf (output, "/>\n");


    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_paint (void		    *abstract_surface,
			  cairo_operator_t   op,
			  cairo_pattern_t   *source)
{
    cairo_svg_surface_t *surface = abstract_surface;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_svg_surface_analyze_operation (surface, op, source);

    /* XXX: It would be nice to be able to assert this condition
     * here. But, we actually allow one 'cheat' that is used when
     * painting the final image-based fallbacks. The final fallbacks
     * do have alpha which we support by blending with white. This is
     * possible only because there is nothing between the fallback
     * images and the paper, nor is anything painted above. */
    /*
    assert (__cairo_svg_surface_operation_supported (surface, op, source));
    */

    /* Emulation of clear and source operators, when no clipping region
     * is defined. We just delete existing content of surface root node,
     * and exit early if operator is clear.
     * XXX: optimization of SOURCE operator doesn't work, since analyze
     * above always return FALSE. In order to make it work, we need a way
     * to know if there's an active clipping path.
     * Optimization of CLEAR works because of a test in paginated surface,
     * and an optimiszation in meta surface. */
    if (surface->clip_level == 0 &&
	(op == CAIRO_OPERATOR_CLEAR ||
	 op == CAIRO_OPERATOR_SOURCE)) {
	_cairo_output_stream_destroy (surface->xml_node);
	surface->xml_node = _cairo_memory_stream_create ();

	if (op == CAIRO_OPERATOR_CLEAR) {
	    if (surface->content == CAIRO_CONTENT_COLOR) {
		_cairo_output_stream_printf (surface->xml_node,
					     "<rect "
					     "width=\"%f\" height=\"%f\" "
					     "style=\"opacity: 1; "
					     "stroke: none; "
					     "fill: rgb(0,0,0);\"/>\n",
					     surface->width, surface->height);
	    }
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    _cairo_svg_surface_emit_paint (surface->xml_node, surface, op, source, NULL);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_mask (void		    *abstract_surface,
			cairo_operator_t     op,
			cairo_pattern_t	    *source,
			cairo_pattern_t	    *mask)
{
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_svg_document_t *document = surface->document;
    cairo_output_stream_t *mask_stream;
    char buffer[64];

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_svg_surface_analyze_operation (surface, op, source);

    assert (__cairo_svg_surface_operation_supported (surface, op, source));

    _cairo_svg_surface_emit_alpha_filter (document);

    /* _cairo_svg_surface_emit_paint() will output a pattern definition to
     * document->xml_node_defs so we need to write the mask element to
     * a temporary stream and then copy that to xml_node_defs. */
    mask_stream = _cairo_memory_stream_create ();
    _cairo_output_stream_printf (mask_stream,
				 "<mask id=\"mask%d\">\n"
				 "  <g filter=\"url(#alpha)\">\n",
				 document->mask_id);
    _cairo_svg_surface_emit_paint (mask_stream, surface, op, mask, NULL);
    _cairo_output_stream_printf (mask_stream,
				 "  </g>\n"
				 "</mask>\n");
    _cairo_memory_stream_copy (mask_stream, document->xml_node_defs);
    _cairo_output_stream_destroy (mask_stream);

    snprintf (buffer, sizeof buffer, "mask=\"url(#mask%d);\"",
	      document->mask_id);
    _cairo_svg_surface_emit_paint (surface->xml_node, surface, op, source, buffer);

    document->mask_id++;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_stroke (void			*abstract_dst,
			   cairo_operator_t      op,
			   cairo_pattern_t	*source,
			   cairo_path_fixed_t	*path,
			   cairo_stroke_style_t *stroke_style,
			   cairo_matrix_t	*ctm,
			   cairo_matrix_t	*ctm_inverse,
			   double		 tolerance,
			   cairo_antialias_t	 antialias)
{
    cairo_svg_surface_t *surface = abstract_dst;
    cairo_status_t status;
    const char *line_cap, *line_join;
    unsigned int i;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_svg_surface_analyze_operation (surface, op, source);

    assert (__cairo_svg_surface_operation_supported (surface, op, source));

    switch (stroke_style->line_cap) {
    case CAIRO_LINE_CAP_BUTT:
 	line_cap = "butt";
 	break;
    case CAIRO_LINE_CAP_ROUND:
 	line_cap = "round";
 	break;
    case CAIRO_LINE_CAP_SQUARE:
 	line_cap = "square";
 	break;
    default:
 	ASSERT_NOT_REACHED;
    }

    switch (stroke_style->line_join) {
    case CAIRO_LINE_JOIN_MITER:
 	line_join = "miter";
 	break;
    case CAIRO_LINE_JOIN_ROUND:
 	line_join = "round";
 	break;
    case CAIRO_LINE_JOIN_BEVEL:
 	line_join = "bevel";
 	break;
    default:
 	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (surface->xml_node,
 				 "<path style=\"fill: none; "
 				 "stroke-width: %f; "
 				 "stroke-linecap: %s; "
 				 "stroke-linejoin: %s; ",
				 stroke_style->line_width,
 				 line_cap,
 				 line_join);

     _cairo_svg_surface_emit_pattern (surface, source, surface->xml_node, TRUE);
     _cairo_svg_surface_emit_operator (surface->xml_node, surface, op);

    if (stroke_style->num_dashes > 0) {
 	_cairo_output_stream_printf (surface->xml_node, "stroke-dasharray: ");
	for (i = 0; i < stroke_style->num_dashes; i++) {
 	    _cairo_output_stream_printf (surface->xml_node, "%f",
 					 stroke_style->dash[i]);
 	    if (i + 1 < stroke_style->num_dashes)
 		_cairo_output_stream_printf (surface->xml_node, ",");
 	    else
 		_cairo_output_stream_printf (surface->xml_node, "; ");
	}
	if (stroke_style->dash_offset != 0.0) {
 	    _cairo_output_stream_printf (surface->xml_node,
 					 "stroke-dashoffset: %f; ",
 					 stroke_style->dash_offset);
	}
    }

    _cairo_output_stream_printf (surface->xml_node,
 				 "stroke-miterlimit: %f;\" ",
 				 stroke_style->miter_limit);

    status = _cairo_svg_surface_emit_path (surface->xml_node, path, ctm_inverse);

    _cairo_svg_surface_emit_transform (surface->xml_node, " transform", "/>\n", ctm);

    return status;
}

static cairo_int_status_t
_cairo_svg_surface_show_glyphs (void			*abstract_surface,
				cairo_operator_t	 op,
				cairo_pattern_t		*pattern,
				cairo_glyph_t		*glyphs,
				int			 num_glyphs,
				cairo_scaled_font_t	*scaled_font)
{
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_svg_document_t *document = surface->document;
    cairo_path_fixed_t path;
    cairo_status_t status;
    unsigned int font_id, subset_id, subset_glyph_index;
    int i;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_svg_surface_analyze_operation (surface, op, pattern);

    assert (__cairo_svg_surface_operation_supported (surface, op, pattern));

    if (num_glyphs <= 0)
	return CAIRO_STATUS_SUCCESS;

    /* FIXME it's probably possible to apply a pattern of a gradient to
     * a group of symbols, but I don't know how yet. Gradients or patterns
     * are translated by x and y properties of use element. */
    if (pattern->type != CAIRO_PATTERN_TYPE_SOLID)
	goto FALLBACK;

    _cairo_output_stream_printf (surface->xml_node, "<g style=\"");
    _cairo_svg_surface_emit_pattern (surface, pattern, surface->xml_node, FALSE);
    _cairo_output_stream_printf (surface->xml_node, "\">\n");

    for (i = 0; i < num_glyphs; i++) {
	status = _cairo_scaled_font_subsets_map_glyph (document->font_subsets,
						       scaled_font, glyphs[i].index,
						       &font_id, &subset_id, &subset_glyph_index);
	if (status) {
	    glyphs += i;
	    num_glyphs -= i;
	    goto FALLBACK;
	}

	_cairo_output_stream_printf (surface->xml_node,
				     "  <use xlink:href=\"#glyph%d-%d\" "
				     "x=\"%f\" y=\"%f\"/>\n",
				     font_id, subset_glyph_index,
				     glyphs[i].x, glyphs[i].y);
    }

    _cairo_output_stream_printf (surface->xml_node, "</g>\n");

    return CAIRO_STATUS_SUCCESS;

FALLBACK:

   _cairo_path_fixed_init (&path);

    status = _cairo_scaled_font_glyph_path (scaled_font,(cairo_glyph_t *) glyphs, num_glyphs, &path);

    if (status)
	    return status;

    status = _cairo_svg_surface_fill (abstract_surface, op, pattern,
				      &path, CAIRO_FILL_RULE_WINDING, 0.0, CAIRO_ANTIALIAS_SUBPIXEL);

    _cairo_path_fixed_fini (&path);

    return status;
}

static cairo_int_status_t
_cairo_svg_surface_intersect_clip_path (void			*dst,
					cairo_path_fixed_t	*path,
					cairo_fill_rule_t	 fill_rule,
					double			 tolerance,
					cairo_antialias_t	 antialias)
{
    cairo_svg_surface_t *surface = dst;
    cairo_svg_document_t *document = surface->document;
    cairo_status_t status;
    unsigned int i;

    if (path == NULL) {
 	for (i = 0; i < surface->clip_level; i++)
 	    _cairo_output_stream_printf (surface->xml_node, "</g>\n");

	surface->clip_level = 0;
	return CAIRO_STATUS_SUCCESS;
    }

    _cairo_output_stream_printf (document->xml_node_defs,
				 "<clipPath id=\"clip%d\">\n"
 				 "  <path ",
 				 document->clip_id);
    status = _cairo_svg_surface_emit_path (document->xml_node_defs, path, NULL);
    _cairo_output_stream_printf (document->xml_node_defs,
 				 "/>\n"
 				 "</clipPath>\n");

    _cairo_output_stream_printf (surface->xml_node,
				 "<g clip-path=\"url(#clip%d)\" "
				 "clip-rule=\"%s\">\n",
				 document->clip_id,
				 fill_rule == CAIRO_FILL_RULE_EVEN_ODD ?
				 "evenodd" : "nonzero");

    document->clip_id++;
    surface->clip_level++;

    return status;
}

static void
_cairo_svg_surface_get_font_options (void                  *abstract_surface,
				     cairo_font_options_t  *options)
{
    _cairo_font_options_init_default (options);

    cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_OFF);
    cairo_font_options_set_antialias (options, CAIRO_ANTIALIAS_GRAY);
}

static const cairo_surface_backend_t cairo_svg_surface_backend = {
	CAIRO_SURFACE_TYPE_SVG,
	_cairo_svg_surface_create_similar,
	_cairo_svg_surface_finish,
	NULL, /* acquire_source_image */
	NULL, /* release_source_image */
	NULL, /* acquire_dest_image */
	NULL, /* release_dest_image */
	NULL, /* clone_similar */
	NULL, /* _cairo_svg_surface_composite, */
	NULL, /* _cairo_svg_surface_fill_rectangles, */
	NULL, /* _cairo_svg_surface_composite_trapezoids,*/
	_cairo_svg_surface_copy_page,
	_cairo_svg_surface_show_page,
	NULL, /* set_clip_region */
	_cairo_svg_surface_intersect_clip_path,
	_cairo_svg_surface_get_extents,
	NULL, /* _cairo_svg_surface_old_show_glyphs, */
	_cairo_svg_surface_get_font_options,
	NULL, /* flush */
	NULL, /* mark dirty rectangle */
	NULL, /* scaled font fini */
	NULL, /* scaled glyph fini */
	_cairo_svg_surface_paint,
	_cairo_svg_surface_mask,
	_cairo_svg_surface_stroke,
	_cairo_svg_surface_fill,
	_cairo_svg_surface_show_glyphs
};

static cairo_svg_document_t *
_cairo_svg_document_create (cairo_output_stream_t	*output_stream,
			    double			 width,
			    double			 height,
			    cairo_svg_version_t		 version)
{
    cairo_svg_document_t *document;

    document = malloc (sizeof (cairo_svg_document_t));
    if (document == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    /* The use of defs for font glyphs imposes no per-subset limit. */
    document->font_subsets = _cairo_scaled_font_subsets_create (0, INT_MAX);
    if (document->font_subsets == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	free (document);
	return NULL;
    }

    document->output_stream = output_stream;
    document->refcount = 1;
    document->owner = NULL;
    document->finished = FALSE;
    document->width = width;
    document->height = height;

    document->surface_id = 0;
    document->linear_pattern_id = 0;
    document->radial_pattern_id = 0;
    document->pattern_id = 0;
    document->filter_id = 0;
    document->clip_id = 0;
    document->mask_id = 0;

    document->xml_node_defs = _cairo_memory_stream_create ();
    document->xml_node_glyphs = _cairo_memory_stream_create ();

    document->alpha_filter = FALSE;

    _cairo_array_init (&document->meta_snapshots,
		       sizeof (cairo_meta_snapshot_t));

    document->svg_version = version;

    return document;
}

static cairo_svg_document_t *
_cairo_svg_document_reference (cairo_svg_document_t *document)
{
    document->refcount++;

    return document;
}

static void
_cairo_svg_document_destroy (cairo_svg_document_t *document)
{
    document->refcount--;
    if (document->refcount > 0)
      return;

    _cairo_svg_document_finish (document);

    free (document);
}

static cairo_status_t
_cairo_svg_document_finish (cairo_svg_document_t *document)
{
    cairo_status_t status;
    cairo_output_stream_t *output = document->output_stream;
    cairo_meta_snapshot_t *snapshot;
    cairo_svg_surface_t *surface;
    cairo_svg_page_t *page;
    unsigned int i;

    if (document->finished)
	return CAIRO_STATUS_SUCCESS;

    _cairo_output_stream_printf (output,
				 "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				 "<svg xmlns=\"http://www.w3.org/2000/svg\" "
				 "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
				 "width=\"%fpt\" height=\"%fpt\" "
				 "viewBox=\"0 0 %f %f\" version=\"%s\">\n",
				 document->width, document->height,
				 document->width, document->height,
				 _cairo_svg_internal_version_strings [document->svg_version]);

    _cairo_svg_document_emit_font_subsets (document);
    if (_cairo_memory_stream_length (document->xml_node_glyphs) > 0 ||
	_cairo_memory_stream_length (document->xml_node_defs) > 0) {
	_cairo_output_stream_printf (output, "<defs>\n");
	if (_cairo_memory_stream_length (document->xml_node_glyphs) > 0) {
	    _cairo_output_stream_printf (output, "<g>\n");
	    _cairo_memory_stream_copy (document->xml_node_glyphs, output);
	    _cairo_output_stream_printf (output, "</g>\n");
	}
	_cairo_memory_stream_copy (document->xml_node_defs, output);
	_cairo_output_stream_printf (output, "</defs>\n");
    }

    surface = (cairo_svg_surface_t *) _cairo_paginated_surface_get_target (document->owner);
    if (_cairo_memory_stream_length (surface->xml_node) > 0)
	_cairo_svg_surface_store_page (surface);

    if (surface->page_set.num_elements > 1 &&
	_cairo_svg_version_has_page_set_support (document->svg_version)) {
	_cairo_output_stream_printf (output, "<pageSet>\n");
	for (i = 0; i < surface->page_set.num_elements; i++) {
	    page = _cairo_array_index (&surface->page_set, i);
	    _cairo_output_stream_printf (output, "<page>\n");
	    _cairo_output_stream_printf (output,
					 "<g id=\"surface%d\" "
					 "clip-path=\"url(#clip%d)\">\n",
					 page->surface_id,
					 page->clip_id);
	    _cairo_memory_stream_copy (page->xml_node, output);
	    _cairo_output_stream_printf (output, "</g>\n</page>\n");
	}
	_cairo_output_stream_printf (output, "</pageSet>\n");
    } else if (surface->page_set.num_elements > 0) {
	page = _cairo_array_index (&surface->page_set, surface->page_set.num_elements - 1);
	_cairo_output_stream_printf (output,
				     "<g id=\"surface%d\" "
				     "clip-path=\"url(#clip%d)\">\n",
				     page->surface_id,
				     page->clip_id);
	_cairo_memory_stream_copy (page->xml_node, output);
	_cairo_output_stream_printf (output, "</g>\n");
    }

    _cairo_output_stream_printf (output, "</svg>\n");

    _cairo_output_stream_destroy (document->xml_node_glyphs);
    _cairo_output_stream_destroy (document->xml_node_defs);

    status = _cairo_output_stream_destroy (output);

    for (i = 0; i < document->meta_snapshots.num_elements; i++) {
	snapshot = _cairo_array_index (&document->meta_snapshots, i);
	cairo_surface_destroy ((cairo_surface_t *) snapshot->meta);
    }
    _cairo_array_fini (&document->meta_snapshots);

    document->finished = TRUE;

    return status;
}

static void
_cairo_svg_surface_set_paginated_mode (void 		      	*abstract_surface,
				       cairo_paginated_mode_t 	 paginated_mode)
{
    cairo_svg_surface_t *surface = abstract_surface;

    surface->paginated_mode = paginated_mode;
}

static const cairo_paginated_surface_backend_t cairo_svg_surface_paginated_backend = {
    NULL /*_cairo_svg_surface_start_page*/,
    _cairo_svg_surface_set_paginated_mode
};

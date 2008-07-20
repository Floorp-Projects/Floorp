/* vim: set sw=4 sts=4: -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
 * Copyright © 2005-2007 Emmanuel Pacaud <emmanuel.pacaud@free.fr>
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
 *	Emmanuel Pacaud <emmanuel.pacaud@free.fr>
 *	Carl Worth <cworth@cworth.org>
 */

#define _BSD_SOURCE /* for snprintf() */
#include "cairoint.h"
#include "cairo-svg.h"
#include "cairo-svg-surface-private.h"
#include "cairo-path-fixed-private.h"
#include "cairo-meta-surface-private.h"
#include "cairo-paginated-private.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-output-stream-private.h"

typedef struct cairo_svg_page cairo_svg_page_t;

static const int invalid_pattern_id = -1;

static const cairo_svg_version_t _cairo_svg_versions[] =
{
    CAIRO_SVG_VERSION_1_1,
    CAIRO_SVG_VERSION_1_2
};

#define CAIRO_SVG_VERSION_LAST ARRAY_LENGTH (_cairo_svg_versions)

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

typedef struct {
    unsigned int id;
    cairo_meta_surface_t *meta;
} cairo_meta_snapshot_t;

static cairo_status_t
_cairo_svg_document_create (cairo_output_stream_t	 *stream,
			    double			  width,
			    double			  height,
			    cairo_svg_version_t		  version,
			    cairo_svg_document_t	**document_out);

static cairo_status_t
_cairo_svg_document_destroy (cairo_svg_document_t *document);

static cairo_status_t
_cairo_svg_document_finish (cairo_svg_document_t *document);

static cairo_svg_document_t *
_cairo_svg_document_reference (cairo_svg_document_t *document);

static unsigned int
_cairo_svg_document_allocate_mask_id (cairo_svg_document_t *document);

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
 * owns the surface and should call cairo_surface_destroy() when done
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
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create (write_func, NULL, closure);
    if (_cairo_output_stream_get_status (stream))
	return _cairo_surface_create_in_error (_cairo_output_stream_destroy (stream));

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
 * owns the surface and should call cairo_surface_destroy() when done
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
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create_for_filename (filename);
    if (_cairo_output_stream_get_status (stream))
	return _cairo_surface_create_in_error (_cairo_output_stream_destroy (stream));

    return _cairo_svg_surface_create_for_stream_internal (stream, width, height, CAIRO_SVG_VERSION_1_1);
}

static cairo_bool_t
_cairo_surface_is_svg (cairo_surface_t *surface)
{
    return surface->backend == &cairo_svg_surface_backend;
}

/* If the abstract_surface is a paginated surface, and that paginated
 * surface's target is a svg_surface, then set svg_surface to that
 * target. Otherwise return %CAIRO_STATUS_SURFACE_TYPE_MISMATCH.
 */
static cairo_status_t
_extract_svg_surface (cairo_surface_t		 *surface,
		      cairo_svg_surface_t	**svg_surface)
{
    cairo_surface_t *target;

    if (surface->status)
	return surface->status;

    if (! _cairo_surface_is_paginated (surface))
	return _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);

    target = _cairo_paginated_surface_get_target (surface);
    if (target->status)
	return target->status;

    if (! _cairo_surface_is_svg (target))
	return _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);

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
    cairo_svg_surface_t *surface = NULL; /* hide compiler warning */
    cairo_status_t status;

    status = _extract_svg_surface (abstract_surface, &surface);
    if (status) {
	status = _cairo_surface_set_error (abstract_surface, status);
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
 * will return %NULL if @version isn't valid. See cairo_svg_get_versions()
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
    cairo_surface_t *paginated;
    cairo_status_t status, status_ignored;

    surface = malloc (sizeof (cairo_svg_surface_t));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base, &cairo_svg_surface_backend,
			 content);

    surface->width = width;
    surface->height = height;

    surface->document = _cairo_svg_document_reference (document);

    surface->clip_level = 0;

    surface->id = document->surface_id++;
    surface->base_clip = document->clip_id++;
    surface->is_base_clip_emitted = FALSE;

    surface->xml_node = _cairo_memory_stream_create ();
    status = _cairo_output_stream_get_status (surface->xml_node);
    if (status)
	goto CLEANUP;

    _cairo_array_init (&surface->page_set, sizeof (cairo_svg_page_t));

    if (content == CAIRO_CONTENT_COLOR) {
	_cairo_output_stream_printf (surface->xml_node,
				     "<rect width=\"%f\" height=\"%f\" "
				     "style=\"opacity: 1; stroke: none; "
				     "fill: rgb(0,0,0);\"/>\n",
				     width, height);
	status = _cairo_output_stream_get_status (surface->xml_node);
	if (status)
	    goto CLEANUP;
    }

    surface->paginated_mode = CAIRO_PAGINATED_MODE_ANALYZE;
    surface->force_fallbacks = FALSE;
    surface->content = content;

    paginated = _cairo_paginated_surface_create (&surface->base,
					         surface->content,
					         surface->width,
						 surface->height,
						 &cairo_svg_surface_paginated_backend);
    status = paginated->status;
    if (status == CAIRO_STATUS_SUCCESS)
	return paginated;

    /* ignore status as we are on the error path */
CLEANUP:
    status_ignored = _cairo_output_stream_destroy (surface->xml_node);
    status_ignored = _cairo_svg_document_destroy (document);

    free (surface);

    return _cairo_surface_create_in_error (status);
}

static cairo_surface_t *
_cairo_svg_surface_create_for_stream_internal (cairo_output_stream_t	*stream,
					       double			 width,
					       double			 height,
					       cairo_svg_version_t	 version)
{
    cairo_svg_document_t *document = NULL; /* silence compiler */
    cairo_surface_t *surface;
    cairo_status_t status;

    status = _cairo_svg_document_create (stream,
	                                 width, height, version,
					 &document);
    if (status) {
	surface =  _cairo_surface_create_in_error (status);
	/* consume the output stream on behalf of caller */
	status = _cairo_output_stream_destroy (stream);
	return surface;
    }

    surface = _cairo_svg_surface_create_for_document (document, CAIRO_CONTENT_COLOR_ALPHA,
						      width, height);
    if (surface->status) {
	status = _cairo_svg_document_destroy (document);
	return surface;
    }

    document->owner = surface;
    status = _cairo_svg_document_destroy (document);
    /* the ref count should be 2 at this point */
    assert (status == CAIRO_STATUS_SUCCESS);

    return surface;
}

static cairo_svg_page_t *
_cairo_svg_surface_store_page (cairo_svg_surface_t *surface)
{
    unsigned int i;
    cairo_svg_page_t page;
    cairo_output_stream_t *stream;
    cairo_status_t status;

    stream = _cairo_memory_stream_create ();
    if (_cairo_output_stream_get_status (stream)) {
	status = _cairo_output_stream_destroy (stream);
	return NULL;
    }

    page.surface_id = surface->id;
    page.clip_level = surface->clip_level;
    page.xml_node = surface->xml_node;

    if (_cairo_array_append (&surface->page_set, &page)) {
	status = _cairo_output_stream_destroy (stream);
	return NULL;
    }

    surface->xml_node = stream;
    surface->clip_level = 0;

    for (i = 0; i < page.clip_level; i++)
	_cairo_output_stream_printf (page.xml_node, "</g>\n");

    return _cairo_array_index (&surface->page_set, surface->page_set.num_elements - 1);
}

static cairo_int_status_t
_cairo_svg_surface_copy_page (void *abstract_surface)
{
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_svg_page_t *page;

    page = _cairo_svg_surface_store_page (surface);
    if (page == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_memory_stream_copy (page->xml_node, surface->xml_node);
    surface->clip_level = page->clip_level;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_show_page (void *abstract_surface)
{
    cairo_svg_surface_t *surface = abstract_surface;

    if (_cairo_svg_surface_store_page (surface) == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_svg_surface_emit_transform (cairo_output_stream_t *output,
				   char const		 *attribute_str,
				   const cairo_matrix_t	 *object_matrix,
				   const cairo_matrix_t  *parent_matrix)
{
    cairo_matrix_t matrix = *object_matrix;

    if (parent_matrix != NULL)
	cairo_matrix_multiply (&matrix, &matrix, parent_matrix);

    if (!_cairo_matrix_is_identity (&matrix))
	_cairo_output_stream_printf (output,
				     "%s=\"matrix(%f,%f,%f,%f,%f,%f)\"",
				     attribute_str,
				     matrix.xx, matrix.yx,
				     matrix.xy, matrix.yy,
				     matrix.x0, matrix.y0);
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
			      cairo_matrix_t	    *ctm_inverse)
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
    if (status)
	return status;

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
    if (status)
	return status;

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
    _cairo_svg_surface_emit_transform (document->xml_node_glyphs, " transform",
				       &image->base.device_transform_inverse, NULL);
    _cairo_output_stream_printf (document->xml_node_glyphs, ">/n");

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

static cairo_status_t
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
    if (status)
	return status;

    _cairo_output_stream_printf (document->xml_node_glyphs, "</symbol>\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_document_emit_font_subset (cairo_scaled_font_subset_t	*font_subset,
				      void				*closure)
{
    cairo_svg_document_t *document = closure;
    unsigned int i;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    for (i = 0; i < font_subset->num_glyphs; i++) {
	status = _cairo_svg_document_emit_glyph (document,
					         font_subset->scaled_font,
					         font_subset->glyphs[i],
					         font_subset->font_id, i);
	if (status)
	    break;
    }

    return status;
}

static cairo_status_t
_cairo_svg_document_emit_font_subsets (cairo_svg_document_t *document)
{
    cairo_status_t status;

    status = _cairo_scaled_font_subsets_foreach_scaled (document->font_subsets,
                                                        _cairo_svg_document_emit_font_subset,
                                                        document);

    _cairo_scaled_font_subsets_destroy (document->font_subsets);
    document->font_subsets = NULL;

    return status;
}

static cairo_bool_t cairo_svg_force_fallbacks = FALSE;

static cairo_int_status_t
_cairo_svg_surface_analyze_operation (cairo_svg_surface_t   *surface,
				      cairo_operator_t	     op,
				      const cairo_pattern_t *pattern)
{
    cairo_svg_document_t *document = surface->document;

    if (cairo_svg_force_fallbacks)
	return FALSE;

    /* SVG doesn't support extend reflect for image pattern */
    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE &&
	pattern->extend == CAIRO_EXTEND_REFLECT)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (document->svg_version >= CAIRO_SVG_VERSION_1_2)
	return CAIRO_STATUS_SUCCESS;

    if (op == CAIRO_OPERATOR_OVER)
	return CAIRO_STATUS_SUCCESS;

    /* The SOURCE operator is only supported if there is nothing
     * painted underneath. */
    if (op == CAIRO_OPERATOR_SOURCE)
	return CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY;

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_svg_surface_operation_supported (cairo_svg_surface_t	*surface,
					cairo_operator_t	 op,
					const cairo_pattern_t	*pattern)
{
    if (_cairo_svg_surface_analyze_operation (surface, op, pattern)
	!= CAIRO_INT_STATUS_UNSUPPORTED)
    {
	return TRUE;
    } else {
	return FALSE;
    }
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
    cairo_status_t status, status2;
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_svg_document_t *document = surface->document;
    cairo_svg_page_t *page;
    unsigned int i;

    if (_cairo_paginated_surface_get_target (document->owner) == &surface->base)
	status = _cairo_svg_document_finish (document);
    else
	status = CAIRO_STATUS_SUCCESS;

    if (surface->xml_node != NULL) {
	status2 = _cairo_output_stream_destroy (surface->xml_node);
	if (status == CAIRO_STATUS_SUCCESS)
	    status = status2;
    }

    for (i = 0; i < surface->page_set.num_elements; i++) {
	page = _cairo_array_index (&surface->page_set, i);
	status2 = _cairo_output_stream_destroy (page->xml_node);
	if (status == CAIRO_STATUS_SUCCESS)
	    status = status2;
    }
    _cairo_array_fini (&surface->page_set);

    status2 = _cairo_svg_document_destroy (document);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;

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
	status = base64_write_func (&info, NULL, 0);
    }

    return status;
}

static cairo_status_t
_cairo_svg_surface_emit_composite_image_pattern (cairo_output_stream_t   *output,
						 cairo_svg_surface_t	 *svg_surface,
						 cairo_surface_pattern_t *pattern,
						 int			  pattern_id,
						 const cairo_matrix_t	 *parent_matrix,
						 const char		 *extra_attributes)
{
    cairo_surface_t *surface;
    cairo_surface_attributes_t surface_attr;
    cairo_rectangle_int_t extents;
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
	goto FAIL;

    p2u = pattern->base.matrix;
    status = cairo_matrix_invert (&p2u);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    if (pattern_id != invalid_pattern_id) {
	_cairo_output_stream_printf (output,
				     "<pattern id=\"pattern%d\" "
				     "patternUnits=\"userSpaceOnUse\" "
				     "width=\"%d\" height=\"%d\"",
				     pattern_id,
				     extents.width, extents.height);
	_cairo_svg_surface_emit_transform (output, " patternTransform", &p2u, parent_matrix);
	_cairo_output_stream_printf (output, ">\n");
    }

    _cairo_output_stream_printf (output,
				 "  <image width=\"%d\" height=\"%d\"",
				 extents.width, extents.height);

    if (pattern_id == invalid_pattern_id)
	_cairo_svg_surface_emit_transform (output, " transform", &p2u, parent_matrix);

    if (extra_attributes)
	_cairo_output_stream_printf (output, " %s", extra_attributes);

    _cairo_output_stream_printf (output, " xlink:href=\"");

    status = _cairo_surface_base64_encode (surface, output);

    _cairo_output_stream_printf (output, "\"/>\n");

    if (pattern_id != invalid_pattern_id)
	_cairo_output_stream_printf (output, "</pattern>\n");

  FAIL:
    _cairo_pattern_release_surface ((cairo_pattern_t *)pattern,
				    surface, &surface_attr);

    return status;
}

static cairo_status_t
_cairo_svg_surface_emit_meta_surface (cairo_svg_document_t *document,
				      cairo_meta_surface_t *surface,
				      int		   *id)
{
    cairo_status_t status;
    cairo_surface_t *paginated_surface;
    cairo_svg_surface_t *svg_surface;
    cairo_meta_snapshot_t new_snapshot;
    cairo_array_t *page_set;

    cairo_output_stream_t *contents;
    cairo_meta_surface_t *meta;
    cairo_meta_snapshot_t *snapshot;
    unsigned int num_elements;
    unsigned int i;

    /* search in already emitted meta snapshots */
    num_elements = document->meta_snapshots.num_elements;
    for (i = 0; i < num_elements; i++) {
	snapshot = _cairo_array_index (&document->meta_snapshots, i);
	meta = snapshot->meta;
	if (meta->commands.num_elements == surface->commands.num_elements &&
	    _cairo_array_index (&meta->commands, 0) == _cairo_array_index (&surface->commands, 0)) {
	    *id = snapshot->id;
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    meta = (cairo_meta_surface_t *) _cairo_surface_snapshot (&surface->base);
    paginated_surface = _cairo_svg_surface_create_for_document (document,
								meta->content,
								meta->width_pixels,
								meta->height_pixels);
    if (paginated_surface->status) {
	cairo_surface_destroy (&meta->base);
	return paginated_surface->status;
    }

    svg_surface = (cairo_svg_surface_t *) _cairo_paginated_surface_get_target (paginated_surface);
    cairo_surface_set_fallback_resolution (paginated_surface,
					   document->owner->x_fallback_resolution,
					   document->owner->y_fallback_resolution);

    status = _cairo_meta_surface_replay (&meta->base, paginated_surface);
    if (status) {
	cairo_surface_destroy (&meta->base);
	cairo_surface_destroy (paginated_surface);
	return status;
    }

    cairo_surface_show_page (paginated_surface);
    status = cairo_surface_status (paginated_surface);
    if (status) {
	cairo_surface_destroy (&meta->base);
	cairo_surface_destroy (paginated_surface);
	return status;
    }

    new_snapshot.meta = meta;
    new_snapshot.id = svg_surface->id;
    status = _cairo_array_append (&document->meta_snapshots, &new_snapshot);
    if (status) {
	cairo_surface_destroy (&meta->base);
	cairo_surface_destroy (paginated_surface);
	return status;
    }

    if (!svg_surface->is_base_clip_emitted) {
	svg_surface->is_base_clip_emitted = TRUE;
	_cairo_output_stream_printf (document->xml_node_defs,
				     "<clipPath id=\"clip%d\">\n"
				     "  <rect width=\"%f\" height=\"%f\"/>\n"
				     "</clipPath>\n",
				     svg_surface->base_clip,
				     svg_surface->width,
				     svg_surface->height);
    }

    if (meta->content == CAIRO_CONTENT_ALPHA) {
	_cairo_svg_surface_emit_alpha_filter (document);
	_cairo_output_stream_printf (document->xml_node_defs,
				     "<g id=\"surface%d\" "
				     "clip-path=\"url(#clip%d)\" "
				     "filter=\"url(#alpha)\">\n",
				     svg_surface->id,
				     svg_surface->base_clip);
    } else {
	_cairo_output_stream_printf (document->xml_node_defs,
				     "<g id=\"surface%d\" "
				     "clip-path=\"url(#clip%d)\">\n",
				     svg_surface->id,
				     svg_surface->base_clip);
    }

    contents = svg_surface->xml_node;
    page_set = &svg_surface->page_set;

    if (_cairo_memory_stream_length (contents) > 0) {
	if (_cairo_svg_surface_store_page (svg_surface) == NULL) {
	    cairo_surface_destroy (paginated_surface);
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}
    }

    if (page_set->num_elements > 0) {
	cairo_svg_page_t *page;

	page = _cairo_array_index (page_set, page_set->num_elements - 1);
	_cairo_memory_stream_copy (page->xml_node, document->xml_node_defs);
    }

    _cairo_output_stream_printf (document->xml_node_defs, "</g>\n");

    *id = new_snapshot.id;

    status = cairo_surface_status (paginated_surface);
    cairo_surface_destroy (paginated_surface);

    /* FIXME: cairo_paginated_surface doesn't take a ref to the
     * passed in target surface so we can't call destroy here.
     * cairo_paginated_surface should be fixed, but for now just
     * work around it. */

    /* cairo_surface_destroy (svg_surface); */

    return status;
}

static cairo_status_t
_cairo_svg_surface_emit_composite_meta_pattern (cairo_output_stream_t	*output,
						cairo_svg_surface_t	*surface,
						cairo_surface_pattern_t	*pattern,
						int			 pattern_id,
						const cairo_matrix_t	*parent_matrix,
						const char		*extra_attributes)
{
    cairo_svg_document_t *document = surface->document;
    cairo_meta_surface_t *meta_surface;
    cairo_matrix_t p2u;
    cairo_status_t status;
    int id = 0;

    p2u = pattern->base.matrix;
    status = cairo_matrix_invert (&p2u);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    meta_surface = (cairo_meta_surface_t *) pattern->surface;

    status = _cairo_svg_surface_emit_meta_surface (document, meta_surface, &id);
    if (status)
	return status;

    if (pattern_id != invalid_pattern_id) {
	_cairo_output_stream_printf (output,
				     "<pattern id=\"pattern%d\" "
				     "patternUnits=\"userSpaceOnUse\" "
				     "width=\"%d\" height=\"%d\"",
				     pattern_id,
				     meta_surface->width_pixels,
				     meta_surface->height_pixels);
	_cairo_svg_surface_emit_transform (output, " patternTransform", &p2u, parent_matrix);
	_cairo_output_stream_printf (output, ">\n");
    }

    _cairo_output_stream_printf (output,
				 "<use xlink:href=\"#surface%d\"",
				 id);

    if (pattern_id == invalid_pattern_id)
	_cairo_svg_surface_emit_transform (output, " transform", &p2u, parent_matrix);

    if (extra_attributes)
	_cairo_output_stream_printf (output, " %s", extra_attributes);

    _cairo_output_stream_printf (output, "/>\n");

    if (pattern_id != invalid_pattern_id)
	_cairo_output_stream_printf (output, "</pattern>\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_surface_emit_composite_pattern (cairo_output_stream_t   *output,
					   cairo_svg_surface_t	   *surface,
					   cairo_surface_pattern_t *pattern,
					   int			    pattern_id,
					   const cairo_matrix_t	   *parent_matrix,
					   const char		   *extra_attributes)
{

    if (_cairo_surface_is_meta (pattern->surface)) {
	return _cairo_svg_surface_emit_composite_meta_pattern (output, surface, pattern,
					    pattern_id, parent_matrix, extra_attributes);
    }

    return _cairo_svg_surface_emit_composite_image_pattern (output, surface, pattern,
					 pattern_id, parent_matrix, extra_attributes);
}

static void
_cairo_svg_surface_emit_operator (cairo_output_stream_t *output,
				  cairo_svg_surface_t   *surface,
				  cairo_operator_t	 op)
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

static cairo_status_t
_cairo_svg_surface_emit_solid_pattern (cairo_svg_surface_t    *surface,
				       cairo_solid_pattern_t  *pattern,
				       cairo_output_stream_t  *style,
				       cairo_bool_t	       is_stroke)
{
    _cairo_output_stream_printf (style, is_stroke ?
				 "stroke: rgb(%f%%,%f%%,%f%%); stroke-opacity: %f;":
				 "fill: rgb(%f%%,%f%%,%f%%); fill-opacity: %f;",
				 pattern->color.red * 100.0,
				 pattern->color.green * 100.0,
				 pattern->color.blue * 100.0,
				 pattern->color.alpha);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_surface_emit_surface_pattern (cairo_svg_surface_t	 *surface,
					 cairo_surface_pattern_t *pattern,
					 cairo_output_stream_t   *style,
					 cairo_bool_t		  is_stroke,
					 const cairo_matrix_t	 *parent_matrix)
{
    cairo_svg_document_t *document = surface->document;
    cairo_status_t status;
    int pattern_id;

    pattern_id = document->pattern_id++;
    status = _cairo_svg_surface_emit_composite_pattern (document->xml_node_defs,
	                                                surface, pattern,
							pattern_id, parent_matrix, NULL);
    if (status)
	return status;

    _cairo_output_stream_printf (style,
				 "%s: url(#pattern%d);",
				 is_stroke ? "stroke" : "fill",
				 pattern_id);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_surface_emit_pattern_stops (cairo_output_stream_t          *output,
				       cairo_gradient_pattern_t const *pattern,
				       double			       start_offset,
				       cairo_bool_t		       reverse_stops,
				       cairo_bool_t		       emulate_reflect)
{
    cairo_gradient_stop_t *stops;
    double offset;
    unsigned int n_stops;
    unsigned int i;

    if (pattern->n_stops < 1)
	return CAIRO_STATUS_SUCCESS;

    if (pattern->n_stops == 1) {
	    _cairo_output_stream_printf (output,
					 "<stop offset=\"%f\" style=\""
					 "stop-color: rgb(%f%%,%f%%,%f%%); "
					 "stop-opacity: %f;\"/>\n",
					 pattern->stops[0].offset,
					 pattern->stops[0].color.red   * 100.0,
					 pattern->stops[0].color.green * 100.0,
					 pattern->stops[0].color.blue  * 100.0,
					 pattern->stops[0].color.alpha);
	    return CAIRO_STATUS_SUCCESS;
    }

    if (emulate_reflect || reverse_stops) {
	n_stops = emulate_reflect ? pattern->n_stops * 2 - 2: pattern->n_stops;
	stops = _cairo_malloc_ab (n_stops, sizeof (cairo_gradient_stop_t));
	if (stops == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	for (i = 0; i < pattern->n_stops; i++) {
	    if (reverse_stops) {
		stops[i] = pattern->stops[pattern->n_stops - i - 1];
		stops[i].offset = 1.0 - stops[i].offset;
	    } else
		stops[i] = pattern->stops[i];
	    if (emulate_reflect) {
		stops[i].offset /= 2;
		if (i > 0 && i < (pattern->n_stops - 1)) {
		    if (reverse_stops) {
			stops[i + pattern->n_stops - 1] = pattern->stops[i];
			stops[i + pattern->n_stops - 1].offset =
			    0.5 + 0.5 * stops[i + pattern->n_stops - 1].offset;
		    } else {
			stops[i + pattern->n_stops - 1] = pattern->stops[pattern->n_stops - i - 1];
			stops[i + pattern->n_stops - 1].offset =
			    1 - 0.5 * stops[i + pattern->n_stops - 1].offset;
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
	    offset = start_offset + (1 - start_offset ) * stops[i].offset;
	    _cairo_output_stream_printf (output,
					 "<stop offset=\"%f\" style=\""
					 "stop-color: rgb(%f%%,%f%%,%f%%); "
					 "stop-opacity: %f;\"/>\n",
					 offset,
					 stops[i].color.red   * 100.0,
					 stops[i].color.green * 100.0,
					 stops[i].color.blue  * 100.0,
					 stops[i].color.alpha);
	}
    else {
	cairo_bool_t found = FALSE;
	unsigned int offset_index;
	cairo_color_t offset_color_start, offset_color_stop;

	for (i = 0; i < n_stops; i++) {
	    if (stops[i].offset >= -start_offset) {
		if (i > 0) {
		    if (stops[i].offset != stops[i-1].offset) {
			double x0, x1;
			cairo_color_t *color0, *color1;

			x0 = stops[i-1].offset;
			x1 = stops[i].offset;
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
				     offset_color_start.red   * 100.0,
				     offset_color_start.green * 100.0,
				     offset_color_start.blue  * 100.0,
				     offset_color_start.alpha);
	for (i = offset_index; i < n_stops; i++) {
	    _cairo_output_stream_printf (output,
					 "<stop offset=\"%f\" style=\""
					 "stop-color: rgb(%f%%,%f%%,%f%%); "
					 "stop-opacity: %f;\"/>\n",
					 stops[i].offset + start_offset,
					 stops[i].color.red   * 100.0,
					 stops[i].color.green * 100.0,
					 stops[i].color.blue  * 100.0,
					 stops[i].color.alpha);
	}
	for (i = 0; i < offset_index; i++) {
	    _cairo_output_stream_printf (output,
					 "<stop offset=\"%f\" style=\""
					 "stop-color: rgb(%f%%,%f%%,%f%%); "
					 "stop-opacity: %f;\"/>\n",
					 1.0 + stops[i].offset + start_offset,
					 stops[i].color.red   * 100.0,
					 stops[i].color.green * 100.0,
					 stops[i].color.blue  * 100.0,
					 stops[i].color.alpha);
	}

	_cairo_output_stream_printf (output,
				     "<stop offset=\"1\" style=\""
				     "stop-color: rgb(%f%%,%f%%,%f%%); "
				     "stop-opacity: %f;\"/>\n",
				     offset_color_stop.red   * 100.0,
				     offset_color_stop.green * 100.0,
				     offset_color_stop.blue  * 100.0,
				     offset_color_stop.alpha);

    }

    if (reverse_stops || emulate_reflect)
	free (stops);

    return CAIRO_STATUS_SUCCESS;
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

static cairo_status_t
_cairo_svg_surface_emit_linear_pattern (cairo_svg_surface_t    *surface,
					cairo_linear_pattern_t *pattern,
					cairo_output_stream_t  *style,
					cairo_bool_t	        is_stroke,
					const cairo_matrix_t   *parent_matrix)
{
    cairo_svg_document_t *document = surface->document;
    double x0, y0, x1, y1;
    cairo_matrix_t p2u;
    cairo_status_t status;

    p2u = pattern->base.base.matrix;
    status = cairo_matrix_invert (&p2u);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    x0 = _cairo_fixed_to_double (pattern->p1.x);
    y0 = _cairo_fixed_to_double (pattern->p1.y);
    x1 = _cairo_fixed_to_double (pattern->p2.x);
    y1 = _cairo_fixed_to_double (pattern->p2.y);

    _cairo_output_stream_printf (document->xml_node_defs,
				 "<linearGradient id=\"linear%d\" "
				 "gradientUnits=\"userSpaceOnUse\" "
				 "x1=\"%f\" y1=\"%f\" x2=\"%f\" y2=\"%f\" ",
				 document->linear_pattern_id,
				 x0, y0, x1, y1);

    _cairo_svg_surface_emit_pattern_extend (document->xml_node_defs, &pattern->base.base),
    _cairo_svg_surface_emit_transform (document->xml_node_defs, "gradientTransform", &p2u, parent_matrix);
    _cairo_output_stream_printf (document->xml_node_defs, ">\n");

    status = _cairo_svg_surface_emit_pattern_stops (document->xml_node_defs,
	                                            &pattern->base, 0.0,
						    FALSE, FALSE);
    if (status)
	return status;

    _cairo_output_stream_printf (document->xml_node_defs,
				 "</linearGradient>\n");

    _cairo_output_stream_printf (style,
				 "%s: url(#linear%d);",
				 is_stroke ? "stroke" : "fill",
				 document->linear_pattern_id);

    document->linear_pattern_id++;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_surface_emit_radial_pattern (cairo_svg_surface_t    *surface,
					cairo_radial_pattern_t *pattern,
					cairo_output_stream_t  *style,
					cairo_bool_t            is_stroke,
					const cairo_matrix_t   *parent_matrix)
{
    cairo_svg_document_t *document = surface->document;
    cairo_matrix_t p2u;
    cairo_extend_t extend;
    double x0, y0, x1, y1, r0, r1;
    double fx, fy;
    cairo_bool_t reverse_stops;
    cairo_status_t status;
    cairo_point_t *c0, *c1;
    cairo_fixed_t radius0, radius1;

    extend = pattern->base.base.extend;

    if (pattern->r1 < pattern->r2) {
	c0 = &pattern->c1;
	c1 = &pattern->c2;
	radius0 = pattern->r1;
	radius1 = pattern->r2;
	reverse_stops = FALSE;
    } else {
	c0 = &pattern->c2;
	c1 = &pattern->c1;
	radius0 = pattern->r2;
	radius1 = pattern->r1;
	reverse_stops = TRUE;
    }

    x0 = _cairo_fixed_to_double (c0->x);
    y0 = _cairo_fixed_to_double (c0->y);
    r0 = _cairo_fixed_to_double (radius0);
    x1 = _cairo_fixed_to_double (c1->x);
    y1 = _cairo_fixed_to_double (c1->y);
    r1 = _cairo_fixed_to_double (radius1);

    p2u = pattern->base.base.matrix;
    status = cairo_matrix_invert (&p2u);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    if (pattern->r1 == pattern->r2) {
	unsigned int n_stops = pattern->base.n_stops;

	_cairo_output_stream_printf (document->xml_node_defs,
				     "<radialGradient id=\"radial%d\" "
				     "gradientUnits=\"userSpaceOnUse\" "
				     "cx=\"%f\" cy=\"%f\" "
				     "fx=\"%f\" fy=\"%f\" r=\"%f\" ",
				     document->radial_pattern_id,
				     x1, y1,
				     x1, y1, r1);
	_cairo_svg_surface_emit_transform (document->xml_node_defs, "gradientTransform", &p2u, parent_matrix);
	_cairo_output_stream_printf (document->xml_node_defs, ">\n");

	if (extend == CAIRO_EXTEND_NONE || n_stops < 1)
	    _cairo_output_stream_printf (document->xml_node_defs,
					 "<stop offset=\"0\" style=\""
					 "stop-color: rgb(0%%,0%%,0%%); "
					 "stop-opacity: 0;\"/>\n");
	else {
	    _cairo_output_stream_printf (document->xml_node_defs,
					 "<stop offset=\"0\" style=\""
					 "stop-color: rgb(%f%%,%f%%,%f%%); "
					 "stop-opacity: %f;\"/>\n",
					 pattern->base.stops[0].color.red   * 100.0,
					 pattern->base.stops[0].color.green * 100.0,
					 pattern->base.stops[0].color.blue  * 100.0,
					 pattern->base.stops[0].color.alpha);
	    if (n_stops > 1)
		_cairo_output_stream_printf (document->xml_node_defs,
					     "<stop offset=\"0\" style=\""
					     "stop-color: rgb(%f%%,%f%%,%f%%); "
					     "stop-opacity: %f;\"/>\n",
					     pattern->base.stops[n_stops - 1].color.red   * 100.0,
					     pattern->base.stops[n_stops - 1].color.green * 100.0,
					     pattern->base.stops[n_stops - 1].color.blue  * 100.0,
					     pattern->base.stops[n_stops - 1].color.alpha);
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
	_cairo_svg_surface_emit_transform (document->xml_node_defs, "gradientTransform", &p2u, parent_matrix);
	_cairo_output_stream_printf (document->xml_node_defs, ">\n");

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
	status = _cairo_svg_surface_emit_pattern_stops (document->xml_node_defs,
		                                        &pattern->base, offset,
							reverse_stops,
							emulate_reflect);
	if (status)
	    return status;

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
				 is_stroke ? "stroke" : "fill",
				 document->radial_pattern_id);

    document->radial_pattern_id++;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_surface_emit_pattern (cairo_svg_surface_t   *surface,
				 cairo_pattern_t       *pattern,
				 cairo_output_stream_t *output,
				 cairo_bool_t		is_stroke,
				 const cairo_matrix_t  *parent_matrix)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return _cairo_svg_surface_emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern,
						      output, is_stroke);

    case CAIRO_PATTERN_TYPE_SURFACE:
	return _cairo_svg_surface_emit_surface_pattern (surface, (cairo_surface_pattern_t *) pattern,
							output, is_stroke, parent_matrix);

    case CAIRO_PATTERN_TYPE_LINEAR:
	return _cairo_svg_surface_emit_linear_pattern (surface, (cairo_linear_pattern_t *) pattern,
						       output, is_stroke, parent_matrix);

    case CAIRO_PATTERN_TYPE_RADIAL:
	return _cairo_svg_surface_emit_radial_pattern (surface, (cairo_radial_pattern_t *) pattern,
						       output, is_stroke, parent_matrix);
    }
    return _cairo_error (CAIRO_STATUS_PATTERN_TYPE_MISMATCH);
}

static cairo_status_t
_cairo_svg_surface_emit_fill_style (cairo_output_stream_t *output,
				    cairo_svg_surface_t   *surface,
				    cairo_operator_t       op,
				    cairo_pattern_t	  *source,
				    cairo_fill_rule_t	   fill_rule,
				    cairo_matrix_t	  *parent_matrix)
{
    _cairo_output_stream_printf (output,
				 "fill-rule: %s; ",
				 fill_rule == CAIRO_FILL_RULE_EVEN_ODD ?
				 "evenodd" : "nonzero");
    _cairo_svg_surface_emit_operator (output, surface, op);
    return _cairo_svg_surface_emit_pattern (surface, source, output, FALSE, parent_matrix);
}

static cairo_status_t
_cairo_svg_surface_emit_stroke_style (cairo_output_stream_t *output,
				      cairo_svg_surface_t   *surface,
				      cairo_operator_t	     op,
				      cairo_pattern_t	    *source,
				      cairo_stroke_style_t  *stroke_style,
				      cairo_matrix_t	    *parent_matrix)
{
    cairo_status_t status;
    const char *line_cap, *line_join;
    unsigned int i;

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

    _cairo_output_stream_printf (output,
				 "stroke-width: %f; "
				 "stroke-linecap: %s; "
				 "stroke-linejoin: %s; ",
				 stroke_style->line_width,
				 line_cap,
				 line_join);

     status = _cairo_svg_surface_emit_pattern (surface, source, output, TRUE, parent_matrix);
     if (status)
	 return status;

     _cairo_svg_surface_emit_operator (output, surface, op);

    if (stroke_style->num_dashes > 0) {
	_cairo_output_stream_printf (output, "stroke-dasharray: ");
	for (i = 0; i < stroke_style->num_dashes; i++) {
	    _cairo_output_stream_printf (output, "%f",
					 stroke_style->dash[i]);
	    if (i + 1 < stroke_style->num_dashes)
		_cairo_output_stream_printf (output, ",");
	    else
		_cairo_output_stream_printf (output, "; ");
	}
	if (stroke_style->dash_offset != 0.0) {
	    _cairo_output_stream_printf (output,
					 "stroke-dashoffset: %f; ",
					 stroke_style->dash_offset);
	}
    }

    _cairo_output_stream_printf (output,
				 "stroke-miterlimit: %f; ",
				 stroke_style->miter_limit);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_fill_stroke (void			*abstract_surface,
				cairo_operator_t	 fill_op,
				cairo_pattern_t		*fill_source,
				cairo_fill_rule_t	 fill_rule,
				double			 fill_tolerance,
				cairo_antialias_t	 fill_antialias,
				cairo_path_fixed_t	*path,
				cairo_operator_t	 stroke_op,
				cairo_pattern_t		*stroke_source,
				cairo_stroke_style_t	*stroke_style,
				cairo_matrix_t		*stroke_ctm,
				cairo_matrix_t		*stroke_ctm_inverse,
				double			 stroke_tolerance,
				cairo_antialias_t	 stroke_antialias)
{
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_status_t status;

    _cairo_output_stream_printf (surface->xml_node, "<path style=\"");
    status = _cairo_svg_surface_emit_fill_style (surface->xml_node, surface, fill_op,
						 fill_source, fill_rule, stroke_ctm_inverse);
    if (status)
	return status;

    status = _cairo_svg_surface_emit_stroke_style (surface->xml_node, surface, stroke_op,
						   stroke_source, stroke_style, stroke_ctm_inverse);
    if (status)
	return status;

    _cairo_output_stream_printf (surface->xml_node, "\" ");

    status = _cairo_svg_surface_emit_path (surface->xml_node, path, stroke_ctm_inverse);
    if (status)
	return status;

    _cairo_svg_surface_emit_transform (surface->xml_node, " transform", stroke_ctm, NULL);
    _cairo_output_stream_printf (surface->xml_node, "/>\n");

    return CAIRO_STATUS_SUCCESS;
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

    assert (_cairo_svg_surface_operation_supported (surface, op, source));

    _cairo_output_stream_printf (surface->xml_node, "<path style=\" stroke:none;");
    status = _cairo_svg_surface_emit_fill_style (surface->xml_node, surface, op, source, fill_rule, NULL);
    if (status)
	return status;

    _cairo_output_stream_printf (surface->xml_node, "\" ");

    status = _cairo_svg_surface_emit_path (surface->xml_node, path, NULL);
    if (status)
	return status;

    _cairo_output_stream_printf (surface->xml_node, "/>\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_get_extents (void		        *abstract_surface,
				cairo_rectangle_int_t   *rectangle)
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
			       cairo_operator_t	      op,
			       cairo_pattern_t	     *source,
			       const char	     *extra_attributes)
{
    cairo_status_t status;

    if (source->type == CAIRO_PATTERN_TYPE_SURFACE &&
	source->extend == CAIRO_EXTEND_NONE)
	return _cairo_svg_surface_emit_composite_pattern (output,
				       surface,
				       (cairo_surface_pattern_t *) source,
				       invalid_pattern_id,
				       NULL,
				       extra_attributes);

    _cairo_output_stream_printf (output,
				 "<rect x=\"0\" y=\"0\" "
				 "width=\"%f\" height=\"%f\" "
				 "style=\"",
				 surface->width, surface->height);
    _cairo_svg_surface_emit_operator (output, surface, op);
    status = _cairo_svg_surface_emit_pattern (surface, source, output, FALSE, NULL);
    if (status)
	return status;

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
    cairo_status_t status;
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
    assert (_cairo_svg_surface_operation_supported (surface, op, source));
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
	 op == CAIRO_OPERATOR_SOURCE))
    {
	status = _cairo_output_stream_destroy (surface->xml_node);
	if (status) {
	    surface->xml_node = NULL;
	    return status;
	}

	surface->xml_node = _cairo_memory_stream_create ();
	if (_cairo_output_stream_get_status (surface->xml_node)) {
	    status = _cairo_output_stream_destroy (surface->xml_node);
	    surface->xml_node = NULL;
	    return status;
	}

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

    return _cairo_svg_surface_emit_paint (surface->xml_node, surface, op, source, NULL);
}

static cairo_int_status_t
_cairo_svg_surface_mask (void		    *abstract_surface,
			cairo_operator_t     op,
			cairo_pattern_t	    *source,
			cairo_pattern_t	    *mask)
{
    cairo_status_t status;
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_svg_document_t *document = surface->document;
    cairo_output_stream_t *mask_stream;
    char buffer[64];
    cairo_bool_t discard_filter = FALSE;
    unsigned int mask_id;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_svg_surface_analyze_operation (surface, op, source);

    assert (_cairo_svg_surface_operation_supported (surface, op, source));

    if (cairo_pattern_get_type (mask) == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t*) mask;
	cairo_content_t content = cairo_surface_get_content (surface_pattern->surface);
	if (content == CAIRO_CONTENT_ALPHA)
	    discard_filter = TRUE;
    }

    if (!discard_filter)
	_cairo_svg_surface_emit_alpha_filter (document);

    /* _cairo_svg_surface_emit_paint() will output a pattern definition to
     * document->xml_node_defs so we need to write the mask element to
     * a temporary stream and then copy that to xml_node_defs. */
    mask_stream = _cairo_memory_stream_create ();
    if (_cairo_output_stream_get_status (mask_stream))
	return _cairo_output_stream_destroy (mask_stream);

    mask_id = _cairo_svg_document_allocate_mask_id (document);

    _cairo_output_stream_printf (mask_stream,
				 "<mask id=\"mask%d\">\n"
				 "%s",
				 mask_id,
				 discard_filter ? "" : "  <g filter=\"url(#alpha)\">\n");
    status = _cairo_svg_surface_emit_paint (mask_stream, surface, op, mask, NULL);
    if (status) {
	cairo_status_t ignore = _cairo_output_stream_destroy (mask_stream);
	return status;
	(void) ignore;
    }

    _cairo_output_stream_printf (mask_stream,
				 "%s"
				 "</mask>\n",
				 discard_filter ? "" : "  </g>\n");
    _cairo_memory_stream_copy (mask_stream, document->xml_node_defs);

    status = _cairo_output_stream_destroy (mask_stream);
    if (status)
	return status;

    snprintf (buffer, sizeof buffer, "mask=\"url(#mask%d)\"",
	      mask_id);
    status = _cairo_svg_surface_emit_paint (surface->xml_node, surface, op, source, buffer);
    if (status)
	return status;

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

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_svg_surface_analyze_operation (surface, op, source);

    assert (_cairo_svg_surface_operation_supported (surface, op, source));

    _cairo_output_stream_printf (surface->xml_node, "<path style=\"fill: none; ");
    status = _cairo_svg_surface_emit_stroke_style (surface->xml_node, surface, op,
						   source, stroke_style, ctm_inverse);
    if (status)
	return status;

    _cairo_output_stream_printf (surface->xml_node, "\" ");

    status = _cairo_svg_surface_emit_path (surface->xml_node, path, ctm_inverse);
    if (status)
	return status;

    _cairo_svg_surface_emit_transform (surface->xml_node, " transform", ctm, NULL);
    _cairo_output_stream_printf (surface->xml_node, "/>\n");

    return CAIRO_STATUS_SUCCESS;
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
    cairo_scaled_font_subsets_glyph_t subset_glyph;
    int i;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_svg_surface_analyze_operation (surface, op, pattern);

    assert (_cairo_svg_surface_operation_supported (surface, op, pattern));

    if (num_glyphs <= 0)
	return CAIRO_STATUS_SUCCESS;

    /* FIXME it's probably possible to apply a pattern of a gradient to
     * a group of symbols, but I don't know how yet. Gradients or patterns
     * are translated by x and y properties of use element. */
    if (pattern->type != CAIRO_PATTERN_TYPE_SOLID)
	goto FALLBACK;

    _cairo_output_stream_printf (surface->xml_node, "<g style=\"");
    status = _cairo_svg_surface_emit_pattern (surface, pattern,
	                                      surface->xml_node, FALSE, NULL);
    if (status)
	return status;

    _cairo_output_stream_printf (surface->xml_node, "\">\n");

    for (i = 0; i < num_glyphs; i++) {
	status = _cairo_scaled_font_subsets_map_glyph (document->font_subsets,
						       scaled_font, glyphs[i].index,
                                                       &subset_glyph);
	if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	    _cairo_output_stream_printf (surface->xml_node, "</g>\n");

	    glyphs += i;
	    num_glyphs -= i;
	    goto FALLBACK;
	}

	if (status)
	    return status;

	_cairo_output_stream_printf (surface->xml_node,
				     "  <use xlink:href=\"#glyph%d-%d\" "
				     "x=\"%f\" y=\"%f\"/>\n",
				     subset_glyph.font_id,
                                     subset_glyph.subset_glyph_index,
				     glyphs[i].x, glyphs[i].y);
    }

    _cairo_output_stream_printf (surface->xml_node, "</g>\n");

    return CAIRO_STATUS_SUCCESS;

FALLBACK:
   _cairo_path_fixed_init (&path);

    status = _cairo_scaled_font_glyph_path (scaled_font,(cairo_glyph_t *) glyphs, num_glyphs, &path);

    if (status) {
	_cairo_path_fixed_fini (&path);
	return status;
    }

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
    if (status)
	return status;

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

    return CAIRO_STATUS_SUCCESS;
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
	_cairo_svg_surface_show_glyphs,
	NULL, /* snapshot */
	NULL, /* is_similar */
	NULL, /* reset */
	_cairo_svg_surface_fill_stroke
};

static cairo_status_t
_cairo_svg_document_create (cairo_output_stream_t	 *output_stream,
			    double			  width,
			    double			  height,
			    cairo_svg_version_t		  version,
			    cairo_svg_document_t	**document_out)
{
    cairo_svg_document_t *document;
    cairo_status_t status, status_ignored;

    if (output_stream->status)
	return output_stream->status;

    document = malloc (sizeof (cairo_svg_document_t));
    if (document == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    /* The use of defs for font glyphs imposes no per-subset limit. */
    document->font_subsets = _cairo_scaled_font_subsets_create_scaled ();
    if (document->font_subsets == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_DOCUMENT;
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
    status = _cairo_output_stream_get_status (document->xml_node_defs);
    if (status)
	goto CLEANUP_NODE_DEFS;

    document->xml_node_glyphs = _cairo_memory_stream_create ();
    status = _cairo_output_stream_get_status (document->xml_node_glyphs);
    if (status)
	goto CLEANUP_NODE_GLYPHS;

    document->alpha_filter = FALSE;

    _cairo_array_init (&document->meta_snapshots,
		       sizeof (cairo_meta_snapshot_t));

    document->svg_version = version;

    *document_out = document;
    return CAIRO_STATUS_SUCCESS;

  CLEANUP_NODE_GLYPHS:
    status_ignored = _cairo_output_stream_destroy (document->xml_node_glyphs);
  CLEANUP_NODE_DEFS:
    status_ignored = _cairo_output_stream_destroy (document->xml_node_defs);
    _cairo_scaled_font_subsets_destroy (document->font_subsets);
  CLEANUP_DOCUMENT:
    free (document);
    return status;
}

static cairo_svg_document_t *
_cairo_svg_document_reference (cairo_svg_document_t *document)
{
    document->refcount++;

    return document;
}

static unsigned int
_cairo_svg_document_allocate_mask_id (cairo_svg_document_t *document)
{
    return document->mask_id++;
}

static cairo_status_t
_cairo_svg_document_destroy (cairo_svg_document_t *document)
{
    cairo_status_t status;

    document->refcount--;
    if (document->refcount > 0)
      return CAIRO_STATUS_SUCCESS;

    status = _cairo_svg_document_finish (document);

    free (document);

    return status;
}

static cairo_status_t
_cairo_svg_document_finish (cairo_svg_document_t *document)
{
    cairo_status_t status, status2;
    cairo_output_stream_t *output = document->output_stream;
    cairo_meta_snapshot_t *snapshot;
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

    status = _cairo_svg_document_emit_font_subsets (document);

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

    if (document->owner != NULL) {
	cairo_svg_surface_t *surface;

	surface = (cairo_svg_surface_t *) _cairo_paginated_surface_get_target (document->owner);
	if (surface->xml_node != NULL &&
		_cairo_memory_stream_length (surface->xml_node) > 0) {
	    if (_cairo_svg_surface_store_page (surface) == NULL) {
		if (status == CAIRO_STATUS_SUCCESS)
		    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    }
	}

	if (surface->page_set.num_elements > 1 &&
	    _cairo_svg_version_has_page_set_support (document->svg_version)) {
	    _cairo_output_stream_printf (output, "<pageSet>\n");
	    for (i = 0; i < surface->page_set.num_elements; i++) {
		page = _cairo_array_index (&surface->page_set, i);
		_cairo_output_stream_printf (output, "<page>\n");
		_cairo_output_stream_printf (output,
					     "<g id=\"surface%d\">\n",
					     page->surface_id);
		_cairo_memory_stream_copy (page->xml_node, output);
		_cairo_output_stream_printf (output, "</g>\n</page>\n");
	    }
	    _cairo_output_stream_printf (output, "</pageSet>\n");
	} else if (surface->page_set.num_elements > 0) {
	    page = _cairo_array_index (&surface->page_set, surface->page_set.num_elements - 1);
	    _cairo_output_stream_printf (output,
					 "<g id=\"surface%d\">\n",
					 page->surface_id);
	    _cairo_memory_stream_copy (page->xml_node, output);
	    _cairo_output_stream_printf (output, "</g>\n");
	}
    }

    _cairo_output_stream_printf (output, "</svg>\n");

    status2 = _cairo_output_stream_destroy (document->xml_node_glyphs);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;

    status2 = _cairo_output_stream_destroy (document->xml_node_defs);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;

    status2 = _cairo_output_stream_destroy (output);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;

    for (i = 0; i < document->meta_snapshots.num_elements; i++) {
	snapshot = _cairo_array_index (&document->meta_snapshots, i);
	status2 = cairo_surface_status (&snapshot->meta->base);
	cairo_surface_destroy (&snapshot->meta->base);
	if (status == CAIRO_STATUS_SUCCESS)
	    status = status2;
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

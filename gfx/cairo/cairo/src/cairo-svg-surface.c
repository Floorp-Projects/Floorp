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
#include "cairo-path-fixed-private.h"
#include "cairo-ft-private.h"
#include "cairo-meta-surface-private.h"
#include "cairo-paginated-surface-private.h"
#include "cairo-scaled-font-subsets-private.h"

#include <libxml/tree.h>

#define CC2XML(s) ((const xmlChar *)(s))
#define C2XML(s) ((xmlChar *)(s))

#define CAIRO_SVG_DTOSTR_BUFFER_LEN 30

#define CAIRO_SVG_DEFAULT_DPI 300

typedef struct cairo_svg_document cairo_svg_document_t;
typedef struct cairo_svg_surface cairo_svg_surface_t;

static const cairo_svg_version_t _cairo_svg_versions[CAIRO_SVG_VERSION_LAST] = 
{
    CAIRO_SVG_VERSION_1_1,
    CAIRO_SVG_VERSION_1_2
};

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

struct cairo_svg_document {
    cairo_output_stream_t *output_stream;
    unsigned long refcount;
    cairo_surface_t *owner;
    cairo_bool_t finished;

    double width;
    double height;
    double x_dpi;
    double y_dpi;

    xmlDocPtr	xml_doc;
    xmlNodePtr	xml_node_defs;
    xmlNodePtr  xml_node_main;
    xmlNodePtr	xml_node_glyphs;

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

    xmlNodePtr  xml_node;
    xmlNodePtr  xml_root_node;

    unsigned int clip_level;

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
 * @write: a #cairo_write_func_t to accept the output data
 * @closure: the closure argument for @write
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 * 
 * Creates a SVG surface of the specified size in points to be written
 * incrementally to the stream represented by @write and @closure.
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 */
cairo_surface_t *
cairo_svg_surface_create_for_stream (cairo_write_func_t		 write,
				     void			*closure,
				     double			 width,
				     double			 height)
{
    cairo_status_t status;
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create (write, NULL, closure);
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
	return (cairo_surface_t *) &_cairo_surface_nil;
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
 * cairo_svg_surface_set_dpi:
 * @surface: a svg cairo_surface_t
 * @x_dpi: horizontal dpi
 * @y_dpi: vertical dpi
 * 
 * Set the horizontal and vertical resolution for image fallbacks.
 * When the svg backend needs to fall back to image overlays, it will
 * use this resolution. These DPI values are not used for any other
 * purpose (in particular, they do not have any bearing on the size
 * passed to cairo_pdf_surface_create() nor on the CTM).
 **/

void
cairo_svg_surface_set_dpi (cairo_surface_t	*abstract_surface,
			   double		x_dpi,
			   double		y_dpi)
{
    cairo_svg_surface_t *surface;
    cairo_status_t status;

    status = _extract_svg_surface (abstract_surface, &surface);
    if (status) {
	_cairo_surface_set_error (abstract_surface, 
				  CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    surface->document->x_dpi = x_dpi;    
    surface->document->y_dpi = y_dpi;    
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
 **/

cairo_public void
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

    if (version >= 0 && version < CAIRO_SVG_VERSION_LAST)
	surface->document->svg_version = version;
}

/**
 * cairo_svg_get_versions:
 * @version: supported version list
 * @num_versions: list length
 *
 * Returns the list of supported versions. See 
 * cairo_svg_surface_restrict_to_version().
 **/

cairo_public void
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
 * Returns the string associated to given @version. This function
 * will return NULL if @version isn't valid. See cairo_svg_get_versions()
 * for a way to get the list of valid version ids.
 **/

cairo_public const char *
cairo_svg_version_to_string (cairo_svg_version_t version)
{
    if (version < 0 || version >= CAIRO_SVG_VERSION_LAST)
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
    xmlNodePtr clip, rect;
    int clip_id;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

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
    clip_id = document->clip_id++;
    
    clip = xmlNewChild (document->xml_node_defs, NULL, CC2XML ("clipPath"), NULL);
    snprintf (buffer, sizeof buffer, "clip%d", clip_id);
    xmlSetProp (clip, CC2XML ("id"), C2XML (buffer));
    rect = xmlNewChild (clip, NULL, CC2XML ("rect"), NULL);
    _cairo_dtostr (buffer, sizeof buffer, width);
    xmlSetProp (rect, CC2XML ("width"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, height);
    xmlSetProp (rect, CC2XML ("height"), C2XML (buffer));
    
    /* Use of xlink namespace requires node to be linked to tree. 
     * So by default we link surface main node to document svg node.
     * For surfaces that don't own document, their main node will be
     * unlinked and freed in surface finish. */
    surface->xml_node = xmlNewChild (document->xml_node_main, NULL, CC2XML ("g"), NULL);
    surface->xml_root_node = surface->xml_node;
	
    snprintf (buffer, sizeof buffer, "surface%d", surface->id);
    xmlSetProp (surface->xml_node, CC2XML ("id"), C2XML (buffer));
    snprintf (buffer, sizeof buffer, "url(#clip%d)", clip_id);
    xmlSetProp (surface->xml_node, CC2XML ("clip-path"), C2XML (buffer));

    if (content == CAIRO_CONTENT_COLOR) {
	rect = xmlNewChild (surface->xml_node, NULL, CC2XML ("rect"), NULL);
	_cairo_dtostr (buffer, sizeof buffer, width);
	xmlSetProp (rect, CC2XML ("width"), C2XML (buffer));
	_cairo_dtostr (buffer, sizeof buffer, height);
	xmlSetProp (rect, CC2XML ("height"), C2XML (buffer));
	xmlSetProp (rect, CC2XML ("style"), CC2XML ("opacity:1; stroke:none; fill:rgb(0,0,0);"));
    }

    surface->paginated_mode = CAIRO_PAGINATED_MODE_ANALYZE;
    surface->content = content;

    return &surface->base;
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

    return _cairo_paginated_surface_create (surface,
					    CAIRO_CONTENT_COLOR_ALPHA,
					    width, height,
					    &cairo_svg_surface_paginated_backend);
}

typedef struct
{
    cairo_svg_document_t *document;
    xmlBufferPtr path;
    cairo_matrix_t *ctm_inverse;
} svg_path_info_t;

static cairo_status_t
_cairo_svg_path_move_to (void *closure, cairo_point_t *point)
{
    svg_path_info_t *info = closure;
    xmlBufferPtr path = info->path;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (info->ctm_inverse)
	cairo_matrix_transform_point (info->ctm_inverse, &x, &y);

    xmlBufferCat (path, CC2XML ("M "));
    _cairo_dtostr (buffer, sizeof buffer, x);
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, y);
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_path_line_to (void *closure, cairo_point_t *point)
{
    svg_path_info_t *info = closure;
    xmlBufferPtr path = info->path;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (info->ctm_inverse)
	cairo_matrix_transform_point (info->ctm_inverse, &x, &y);

    xmlBufferCat (path, CC2XML ("L "));

    _cairo_dtostr (buffer, sizeof buffer, x);
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, y);
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_path_curve_to (void          *closure,
			  cairo_point_t *b,
			  cairo_point_t *c,
			  cairo_point_t *d)
{
    svg_path_info_t *info = closure;
    xmlBufferPtr path = info->path;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
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

    xmlBufferCat (path, CC2XML ("C "));
    _cairo_dtostr (buffer, sizeof buffer, bx);
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, by);
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, cx);
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, cy);
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, dx);
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, dy);
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_path_close_path (void *closure)
{
    svg_path_info_t *info = closure;

    xmlBufferCat (info->path, CC2XML ("Z "));

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_svg_document_emit_glyph (cairo_svg_document_t	*document,
				cairo_scaled_font_t	*scaled_font,
				unsigned long		 scaled_font_glyph_index,
				unsigned int		 font_id,
				unsigned int		 subset_glyph_index)
{
    cairo_scaled_glyph_t    *scaled_glyph;
    cairo_status_t	     status;
    svg_path_info_t	     info;
    xmlNodePtr		     symbol, child;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    
    status = _cairo_scaled_glyph_lookup (scaled_font,
					 scaled_font_glyph_index,
					 CAIRO_SCALED_GLYPH_INFO_METRICS|
					 CAIRO_SCALED_GLYPH_INFO_PATH,
					 &scaled_glyph);
    /*
     * If that fails, try again but ask for an image instead
     */
    if (status)
	status = _cairo_scaled_glyph_lookup (scaled_font,
					     scaled_font_glyph_index,
					     CAIRO_SCALED_GLYPH_INFO_METRICS|
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
					     &scaled_glyph);
    if (status) {
	_cairo_surface_set_error (document->owner, status);
	return;
    }
    
    info.document = document;
    info.path = xmlBufferCreate ();
    info.ctm_inverse = NULL;

    status = _cairo_path_fixed_interpret (scaled_glyph->path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_svg_path_move_to,
					  _cairo_svg_path_line_to,
					  _cairo_svg_path_curve_to,
					  _cairo_svg_path_close_path,
					  &info);

    symbol = xmlNewChild (document->xml_node_glyphs, NULL, 
			  CC2XML ("symbol"), NULL);
    snprintf (buffer, sizeof buffer, "glyph%d-%d", 
	      font_id,
	      subset_glyph_index);
    xmlSetProp (symbol, CC2XML ("id"), C2XML (buffer));
    child = xmlNewChild (symbol, NULL, CC2XML ("path"), NULL);
    xmlSetProp (child, CC2XML ("d"), xmlBufferContent (info.path));
    xmlSetProp (child, CC2XML ("style"), CC2XML ("stroke: none;"));
    
    xmlBufferFree (info.path);
}

static void
_cairo_svg_document_emit_font_subset (cairo_scaled_font_subset_t	*font_subset,
				      void				*closure)
{
    cairo_svg_document_t *document = closure;
    int i;

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
    _cairo_scaled_font_subsets_foreach (document->font_subsets,
					_cairo_svg_document_emit_font_subset,
					document);
    _cairo_scaled_font_subsets_destroy (document->font_subsets);
    document->font_subsets = NULL;
}

static cairo_int_status_t
_operation_supported (cairo_svg_surface_t *surface,
		      cairo_operator_t op,
		      const cairo_pattern_t *pattern)
{
    cairo_svg_document_t *document = surface->document;
    
    if (document->svg_version < CAIRO_SVG_VERSION_1_2)
	if (op != CAIRO_OPERATOR_OVER)
	    return FALSE;

    return TRUE;
}

static cairo_int_status_t
_analyze_operation (cairo_svg_surface_t *surface,
		    cairo_operator_t op,
		    const cairo_pattern_t *pattern)
{
    if (_operation_supported (surface, op, pattern))
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
    
    if (document->owner == &surface->base) {
	status = _cairo_svg_document_finish (document);
    } else {
	/* See _cairo_svg_surface_create_for_document */
	xmlUnlinkNode (surface->xml_root_node);
	xmlFreeNode (surface->xml_root_node);
	status = CAIRO_STATUS_SUCCESS;
    }

    _cairo_svg_document_destroy (document);

    return status;
}

static void
emit_alpha_filter (cairo_svg_document_t *document)
{
    if (!document->alpha_filter) {
	xmlNodePtr node;
	xmlNodePtr child;

	node = xmlNewChild (document->xml_node_defs, NULL,
			    CC2XML ("filter"), NULL);
	xmlSetProp (node, CC2XML ("id"), CC2XML ("alpha"));
	xmlSetProp (node, CC2XML ("filterUnits"), CC2XML ("objectBoundingBox"));
	xmlSetProp (node, CC2XML ("x"), CC2XML ("0%"));
	xmlSetProp (node, CC2XML ("y"), CC2XML ("0%"));
	xmlSetProp (node, CC2XML ("width"), CC2XML ("100%"));
	xmlSetProp (node, CC2XML ("height"), CC2XML ("100%"));
	child = xmlNewChild (node, NULL, CC2XML ("feColorMatrix"), NULL);
	xmlSetProp (child, CC2XML("type"), CC2XML ("matrix"));
	xmlSetProp (child, CC2XML("in"), CC2XML ("SourceGraphic"));
	xmlSetProp (child, CC2XML("values"), CC2XML ("0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 0 0 0 1 0")); 
	document->alpha_filter = TRUE;
    }
}

static void
emit_transform (xmlNodePtr node, 
		char const *attribute_str, 
		cairo_matrix_t *matrix)
{
    xmlBufferPtr matrix_buffer;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    
    matrix_buffer = xmlBufferCreate ();
    xmlBufferCat (matrix_buffer, CC2XML ("matrix("));
    _cairo_dtostr (buffer, sizeof buffer, matrix->xx);
    xmlBufferCat (matrix_buffer, C2XML (buffer));
    xmlBufferCat (matrix_buffer, CC2XML (","));
    _cairo_dtostr (buffer, sizeof buffer, matrix->yx);
    xmlBufferCat (matrix_buffer, C2XML (buffer));
    xmlBufferCat (matrix_buffer, CC2XML (","));
    _cairo_dtostr (buffer, sizeof buffer, matrix->xy);
    xmlBufferCat (matrix_buffer, C2XML (buffer));
    xmlBufferCat (matrix_buffer, CC2XML (","));
    _cairo_dtostr (buffer, sizeof buffer, matrix->yy);
    xmlBufferCat (matrix_buffer, C2XML (buffer));
    xmlBufferCat (matrix_buffer, CC2XML (","));
    _cairo_dtostr (buffer, sizeof buffer, matrix->x0);
    xmlBufferCat (matrix_buffer, C2XML (buffer));
    xmlBufferCat (matrix_buffer, CC2XML(","));
    _cairo_dtostr (buffer, sizeof buffer, matrix->y0);
    xmlBufferCat (matrix_buffer, C2XML (buffer));
    xmlBufferCat (matrix_buffer, CC2XML (")"));
    xmlSetProp (node, CC2XML (attribute_str), C2XML (xmlBufferContent (matrix_buffer)));
    xmlBufferFree (matrix_buffer);
}

typedef struct {
    xmlBufferPtr buffer;
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
	xmlBufferCat (info->buffer, dst);
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
_cairo_surface_base64_encode (cairo_surface_t *surface,
			      xmlBufferPtr *buffer)
{
    cairo_status_t status;
    base64_write_closure_t info;
    unsigned int i;

    if (buffer == NULL)
	return CAIRO_STATUS_NULL_POINTER;
    
    info.buffer = xmlBufferCreate();
    info.in_mem = 0;
    info.trailing = 0;
    memset (info.dst, '\x0', 5);
    *buffer = info.buffer;

    xmlBufferCat (info.buffer, CC2XML ("data:image/png;base64,"));

    status = cairo_surface_write_to_png_stream (surface, base64_write_func, 
						(void *) &info);

    if (status) {
	xmlBufferFree (*buffer);
	*buffer = NULL;
	return status;
    }
    
    if (info.in_mem > 0) {
	for (i = info.in_mem; i < 3; i++)
	    info.src[i] = '\x0';
	info.trailing = 3 - info.in_mem;
	info.in_mem = 3;
	base64_write_func (&info, NULL, 0);
    }
    
    return CAIRO_STATUS_SUCCESS;
}

static xmlNodePtr
emit_composite_image_pattern (xmlNodePtr 		 node,
			      cairo_svg_surface_t	*svg_surface,
			      cairo_surface_pattern_t 	*pattern,
			      double 			*width,
			      double 			*height,
			      cairo_bool_t 		 is_pattern)
{
    cairo_surface_t *surface = pattern->surface;
    cairo_image_surface_t *image;
    cairo_status_t status;
    cairo_matrix_t p2u;
    xmlNodePtr child = NULL;
    xmlBufferPtr image_buffer;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    void *image_extra;

    status = _cairo_surface_acquire_source_image (surface, &image, &image_extra);
    if (status)	 {
	if (width != NULL)
	    *width = 0;
	if (height != NULL)
	    *height = 0;
	return NULL;
    }

    status = _cairo_surface_base64_encode (surface, &image_buffer);
    if (status) 
	goto BAIL;

    child = xmlNewChild (node, NULL, CC2XML ("image"), NULL);
    _cairo_dtostr (buffer, sizeof buffer, image->width);
    xmlSetProp (child, CC2XML ("width"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, image->height);
    xmlSetProp (child, CC2XML ("height"), C2XML (buffer));
    xmlSetProp (child, CC2XML ("xlink:href"), C2XML (xmlBufferContent (image_buffer)));
		
    xmlBufferFree (image_buffer);

    if (!is_pattern) {
	p2u = pattern->base.matrix;
	cairo_matrix_invert (&p2u);
	emit_transform (child, "transform", &p2u);
    }

    if (width != NULL)
	    *width = image->width;
    if (height != NULL)
	    *height = image->height;

BAIL:
    _cairo_surface_release_source_image (pattern->surface, image, image_extra);

    return child;
}

static int
emit_meta_surface (cairo_svg_document_t *document,
		   cairo_meta_surface_t *surface)
{
    cairo_meta_surface_t *meta;
    cairo_meta_snapshot_t *snapshot;
    int num_elements;
    unsigned int i, id;

    num_elements = document->meta_snapshots.num_elements;
    for (i = 0; i < num_elements; i++) {
	snapshot = _cairo_array_index (&document->meta_snapshots, i);
	meta = snapshot->meta;
	if (meta->commands.num_elements == surface->commands.num_elements &&
	    _cairo_array_index (&meta->commands, 0) == _cairo_array_index (&surface->commands, 0)) {
	    id = snapshot->id;
	    break;
	}
    }
    
    if (i >= num_elements) {
	cairo_surface_t *paginated_surface;
	cairo_surface_t *svg_surface;
	cairo_meta_snapshot_t new_snapshot;
	xmlNodePtr child;

	meta = (cairo_meta_surface_t *) _cairo_surface_snapshot ((cairo_surface_t *)surface);
	svg_surface = _cairo_svg_surface_create_for_document (document,
							      meta->content,
							      meta->width_pixels, 
							      meta->height_pixels);
	paginated_surface = _cairo_paginated_surface_create (svg_surface,
							     meta->content,
							     meta->width_pixels, 
							     meta->height_pixels,
							     &cairo_svg_surface_paginated_backend);
	_cairo_meta_surface_replay ((cairo_surface_t *)meta, paginated_surface);
	_cairo_surface_show_page (paginated_surface);
	
	new_snapshot.meta = meta;
	new_snapshot.id = ((cairo_svg_surface_t *) svg_surface)->id;
	_cairo_array_append (&document->meta_snapshots, &new_snapshot);
	
	if (meta->content == CAIRO_CONTENT_ALPHA) 
	    emit_alpha_filter (document);
	child = xmlAddChild (document->xml_node_defs, 
			     xmlCopyNode (((cairo_svg_surface_t *) svg_surface)->xml_root_node, 1));
	if (meta->content == CAIRO_CONTENT_ALPHA) 
	    xmlSetProp (child, CC2XML ("filter"), CC2XML("url(#alpha)"));

	id = new_snapshot.id;

	cairo_surface_destroy (paginated_surface);
    }

    return id;
}

static xmlNodePtr
emit_composite_meta_pattern (xmlNodePtr node, 
			     cairo_svg_surface_t	*surface,
			     cairo_surface_pattern_t	*pattern,
			     double *width, 
			     double *height,
			     cairo_bool_t is_pattern)
{
    cairo_svg_document_t *document = surface->document;
    cairo_meta_surface_t *meta_surface;
    cairo_matrix_t p2u;
    xmlNodePtr child;
    int id;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    meta_surface = (cairo_meta_surface_t *) pattern->surface;
    
    id = emit_meta_surface (document, meta_surface);
    
    child = xmlNewChild (node, NULL, CC2XML("use"), NULL);
    snprintf (buffer, sizeof buffer, "#surface%d", id);
    xmlSetProp (child, CC2XML ("xlink:href"), C2XML (buffer));
    
    if (!is_pattern) {
	p2u = pattern->base.matrix;
	cairo_matrix_invert (&p2u);
	emit_transform (child, "transform", &p2u);
    }

    if (width != NULL)
	    *width = meta_surface->width_pixels;
    if (height != NULL)
	    *height = meta_surface->height_pixels;

    return child;
}

static xmlNodePtr
emit_composite_pattern (xmlNodePtr node, 
			cairo_svg_surface_t	*surface,
			cairo_surface_pattern_t *pattern,
			double *width,
			double *height,
			int is_pattern)
{

    if (_cairo_surface_is_meta (pattern->surface)) { 
	return emit_composite_meta_pattern (node, surface, pattern, width, height, is_pattern);
    }

    return emit_composite_image_pattern (node, surface, pattern, width, height, is_pattern);
}

static void
emit_operator (xmlNodePtr	     node,
	       cairo_svg_surface_t  *surface, 
	       cairo_operator_t	     op)
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
	xmlSetProp (node, CC2XML ("comp-op"), C2XML (op_str[op]));
}

static void
emit_color (cairo_color_t const *color, 
	    xmlBufferPtr	 style,
	    char const		*color_str, 
	    char const		*opacity_str)
{
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    xmlBufferCat (style, CC2XML (color_str));
    xmlBufferCat (style, CC2XML (": rgb("));
    _cairo_dtostr (buffer, sizeof buffer, color->red * 100.0);
    xmlBufferCat (style, C2XML (buffer));
    xmlBufferCat (style, CC2XML ("%,"));
    _cairo_dtostr (buffer, sizeof buffer, color->green * 100.0);
    xmlBufferCat (style, C2XML (buffer));
    xmlBufferCat (style, CC2XML ("%,"));
    _cairo_dtostr (buffer, sizeof buffer, color->blue * 100.0);
    xmlBufferCat (style, C2XML (buffer));
    xmlBufferCat (style, CC2XML ("%); "));
    xmlBufferCat (style, CC2XML (opacity_str));
    xmlBufferCat (style, CC2XML (": "));
    _cairo_dtostr (buffer, sizeof buffer, color->alpha);
    xmlBufferCat (style, CC2XML (buffer));
    xmlBufferCat (style, CC2XML (";"));
}

static void
emit_solid_pattern (cairo_svg_surface_t	    *surface,
		    cairo_solid_pattern_t   *pattern,
		    xmlBufferPtr	     style, 
		    int			     is_stroke)
{
    emit_color (&pattern->color, 
		style, is_stroke ? "stroke" : "fill", 
		"opacity");
}

static void
emit_surface_pattern (cairo_svg_surface_t	*surface,
		      cairo_surface_pattern_t	*pattern,
		      xmlBufferPtr		 style, 
		      int			 is_stroke)
{
    cairo_svg_document_t *document = surface->document;
    xmlNodePtr child;
    xmlBufferPtr id;
    cairo_matrix_t p2u;
    double width, height;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    
    child = xmlNewChild (document->xml_node_defs, NULL, CC2XML ("pattern"), NULL);
    
    id = xmlBufferCreate ();
    xmlBufferCat (id, CC2XML ("pattern"));
    snprintf (buffer, sizeof buffer, "%d", document->pattern_id);
    xmlBufferCat (id, C2XML (buffer));
    xmlSetProp (child, CC2XML ("id"), C2XML (xmlBufferContent (id)));

    xmlBufferCat (style, CC2XML (is_stroke ? "color: url(#" : "fill: url(#"));
    xmlBufferCat (style, xmlBufferContent (id));
    xmlBufferCat (style, CC2XML (");"));
    xmlBufferFree (id);

    document->pattern_id++;

    emit_composite_pattern (child, surface, pattern, &width, &height, TRUE);

    _cairo_dtostr (buffer, sizeof buffer, width);
    xmlSetProp (child, CC2XML ("width"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, height);
    xmlSetProp (child, CC2XML ("height"), C2XML (buffer));

    p2u = pattern->base.matrix;
    cairo_matrix_invert (&p2u);

    emit_transform (child, "patternTransform", &p2u);

    xmlSetProp (child, CC2XML ("patternUnits"), CC2XML ("userSpaceOnUse"));
}

static void 
emit_pattern_stops (xmlNodePtr parent,
		    cairo_gradient_pattern_t const *pattern, 
		    double start_offset)
{
    xmlNodePtr child;
    xmlBufferPtr style;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    cairo_color_t color;
    int i;

    for (i = 0; i < pattern->n_stops; i++) {
	child = xmlNewChild (parent, NULL, CC2XML ("stop"), NULL);
	_cairo_dtostr (buffer, sizeof buffer, 
		       start_offset + (1 - start_offset ) *
		       _cairo_fixed_to_double (pattern->stops[i].x));
	xmlSetProp (child, CC2XML ("offset"), C2XML (buffer));
	style = xmlBufferCreate ();
	_cairo_color_init_rgba (&color,
				pattern->stops[i].color.red   / 65535.0,
				pattern->stops[i].color.green / 65535.0,
				pattern->stops[i].color.blue  / 65535.0,
				pattern->stops[i].color.alpha / 65535.0);
	emit_color (&color, style, "stop-color", "stop-opacity");
	xmlSetProp (child, CC2XML ("style"), xmlBufferContent (style));
	xmlBufferFree (style);
    }
}

static void
emit_pattern_extend (xmlNodePtr node, cairo_extend_t extend)
{
    char const *value = NULL;

    switch (extend) {
	case CAIRO_EXTEND_REPEAT:
	    value = "repeat"; 
	    break;
	case CAIRO_EXTEND_REFLECT:
	    value = "reflect"; 
	    break;
	case CAIRO_EXTEND_NONE:
	    break;
	case CAIRO_EXTEND_PAD:
	    /* FIXME not implemented */
	    break;
    }
    
    if (value != NULL)
	xmlSetProp (node, CC2XML ("spreadMethod"), CC2XML (value));
}

static void
emit_linear_pattern (cairo_svg_surface_t    *surface, 
		     cairo_linear_pattern_t *pattern,
		     xmlBufferPtr	     style, 
		     int		     is_stroke)
{
    cairo_svg_document_t *document = surface->document;
    xmlNodePtr child;
    xmlBufferPtr id;
    double x0, y0, x1, y1;
    cairo_matrix_t p2u;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    
    child = xmlNewChild (document->xml_node_defs, NULL, CC2XML ("linearGradient"), NULL);
    
    id = xmlBufferCreate ();
    xmlBufferCat (id, CC2XML ("linear"));
    snprintf (buffer, sizeof buffer, "%d", document->linear_pattern_id);
    xmlBufferCat (id, C2XML (buffer));
    xmlSetProp (child, CC2XML ("id"), C2XML (xmlBufferContent (id)));
    xmlSetProp (child, CC2XML ("gradientUnits"), CC2XML ("userSpaceOnUse"));
    emit_pattern_extend (child, pattern->base.base.extend);

    xmlBufferCat (style, CC2XML (is_stroke ? "color: url(#" : "fill: url(#"));
    xmlBufferCat (style, xmlBufferContent (id));
    xmlBufferCat (style, CC2XML (");"));

    xmlBufferFree (id);

    document->linear_pattern_id++;

    emit_pattern_stops (child ,&pattern->base, 0.0);

    x0 = _cairo_fixed_to_double (pattern->gradient.p1.x);
    y0 = _cairo_fixed_to_double (pattern->gradient.p1.y);
    x1 = _cairo_fixed_to_double (pattern->gradient.p2.x);
    y1 = _cairo_fixed_to_double (pattern->gradient.p2.y);
    
    _cairo_dtostr (buffer, sizeof buffer, x0);
    xmlSetProp (child, CC2XML ("x1"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, y0);
    xmlSetProp (child, CC2XML ("y1"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, x1);
    xmlSetProp (child, CC2XML ("x2"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, y1);
    xmlSetProp (child, CC2XML ("y2"), C2XML (buffer));
    
    p2u = pattern->base.base.matrix;
    cairo_matrix_invert (&p2u);

    emit_transform (child, "gradientTransform", &p2u);
}
	
static void
emit_radial_pattern (cairo_svg_surface_t *surface, 
		     cairo_radial_pattern_t *pattern,
		     xmlBufferPtr style, int is_stroke)
{
    cairo_svg_document_t *document = surface->document;
    cairo_matrix_t p2u;
    xmlNodePtr child;
    xmlBufferPtr id;
    double x0, y0, x1, y1, r0, r1;
    double fx, fy;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    
    child = xmlNewChild (document->xml_node_defs, NULL, CC2XML ("radialGradient"), NULL);
    
    id = xmlBufferCreate ();
    xmlBufferCat (id, CC2XML ("radial"));
    snprintf (buffer, sizeof buffer, "%d", document->radial_pattern_id);
    xmlBufferCat (id, C2XML (buffer));
    xmlSetProp (child, CC2XML ("id"), C2XML (xmlBufferContent (id)));
    xmlSetProp (child, CC2XML ("gradientUnits"), CC2XML ("userSpaceOnUse"));
    emit_pattern_extend (child, pattern->base.base.extend);
    
    xmlBufferCat (style, CC2XML (is_stroke ? "color: url(#" : "fill: url(#"));
    xmlBufferCat (style, xmlBufferContent (id));
    xmlBufferCat (style, CC2XML (");"));

    xmlBufferFree (id);

    document->radial_pattern_id++;

    x0 = _cairo_fixed_to_double (pattern->gradient.inner.x);
    y0 = _cairo_fixed_to_double (pattern->gradient.inner.y);
    r0 = _cairo_fixed_to_double (pattern->gradient.inner.radius);
    x1 = _cairo_fixed_to_double (pattern->gradient.outer.x);
    y1 = _cairo_fixed_to_double (pattern->gradient.outer.y);
    r1 = _cairo_fixed_to_double (pattern->gradient.outer.radius);

    /* SVG doesn't have a start radius, so computing now SVG focal coordinates
     * and emulating start radius by translating color stops.
     * FIXME: We also need to emulate cairo behaviour inside start circle when
     * extend != CAIRO_EXTEND_NONE.
     * FIXME: Handle radius1 <= radius0 */
    fx = (r1 * x0 - r0 * x1) / (r1 - r0);
    fy = (r1 * y0 - r0 * y1) / (r1 - r0);

    emit_pattern_stops (child, &pattern->base, r0 / r1);

    _cairo_dtostr (buffer, sizeof buffer, x1);
    xmlSetProp (child, CC2XML ("cx"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, y1);
    xmlSetProp (child, CC2XML ("cy"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, fx);
    xmlSetProp (child, CC2XML ("fx"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, fy);
    xmlSetProp (child, CC2XML ("fy"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, r1);
    xmlSetProp (child, CC2XML ("r"), C2XML (buffer));

    p2u = pattern->base.base.matrix;
    cairo_matrix_invert (&p2u);

    emit_transform (child, "gradientTransform", &p2u);
}
	
static void
emit_pattern (cairo_svg_surface_t *surface, cairo_pattern_t *pattern, 
	      xmlBufferPtr style, int is_stroke)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:	
	emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern, style, is_stroke);
	break;

    case CAIRO_PATTERN_TYPE_SURFACE:
	emit_surface_pattern (surface, (cairo_surface_pattern_t *) pattern, style, is_stroke);
	break;

    case CAIRO_PATTERN_TYPE_LINEAR:
	emit_linear_pattern (surface, (cairo_linear_pattern_t *) pattern, style, is_stroke);
	break;

    case CAIRO_PATTERN_TYPE_RADIAL:
	emit_radial_pattern (surface, (cairo_radial_pattern_t *) pattern, style, is_stroke);
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
    cairo_svg_document_t *document = surface->document;
    cairo_status_t status;
    svg_path_info_t info;
    xmlNodePtr child;
    xmlBufferPtr style;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _analyze_operation (surface, op, source);

    assert (_operation_supported (surface, op, source));

    info.document = document;
    info.path = xmlBufferCreate ();
    info.ctm_inverse = NULL;
    
    style = xmlBufferCreate ();
    emit_pattern (surface, source, style, 0);
    xmlBufferCat (style, CC2XML (" stroke: none;"));
    xmlBufferCat (style, CC2XML (" fill-rule: "));
    xmlBufferCat (style, fill_rule == CAIRO_FILL_RULE_EVEN_ODD ? CC2XML("evenodd;") : CC2XML ("nonzero;"));

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_svg_path_move_to,
					  _cairo_svg_path_line_to,
					  _cairo_svg_path_curve_to,
					  _cairo_svg_path_close_path,
					  &info);

    child = xmlNewChild (surface->xml_node, NULL, CC2XML ("path"), NULL);
    xmlSetProp (child, CC2XML ("d"), xmlBufferContent (info.path));
    xmlSetProp (child, CC2XML ("style"), xmlBufferContent (style));
    emit_operator (child, surface, op);

    xmlBufferFree (info.path);
    xmlBufferFree (style);

    return status;
}

static cairo_int_status_t
_cairo_svg_surface_get_extents (void		        *abstract_surface,
				cairo_rectangle_fixed_t *rectangle)
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

static xmlNodePtr
emit_paint (xmlNodePtr		 node,
	    cairo_svg_surface_t	*surface,
	    cairo_operator_t	 op,
	    cairo_pattern_t	*source)
{
    xmlNodePtr child;
    xmlBufferPtr style;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    if (source->type == CAIRO_PATTERN_TYPE_SURFACE && 
	source->extend == CAIRO_EXTEND_NONE)
	return emit_composite_pattern (node, 
				       surface,
				       (cairo_surface_pattern_t *) source, 
				       NULL, NULL, FALSE);

    style = xmlBufferCreate ();
    emit_pattern (surface, source, style, 0);
    xmlBufferCat (style, CC2XML (" stroke: none;"));

    child = xmlNewChild (node, NULL, CC2XML ("rect"), NULL);
    xmlSetProp (child, CC2XML ("x"), CC2XML ("0"));
    xmlSetProp (child, CC2XML ("y"), CC2XML ("0"));
    _cairo_dtostr (buffer, sizeof buffer, surface->width);
    xmlSetProp (child, CC2XML ("width"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, surface->height);
    xmlSetProp (child, CC2XML ("height"), C2XML (buffer));
    xmlSetProp (child, CC2XML ("style"), xmlBufferContent (style));
    emit_operator (child, surface, op);

    xmlBufferFree (style);

    return child;
}

static cairo_int_status_t
_cairo_svg_surface_paint (void		    *abstract_surface,
			  cairo_operator_t   op,
			  cairo_pattern_t   *source)
{
    cairo_svg_surface_t *surface = abstract_surface;
    
    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) 
	return _analyze_operation (surface, op, source);

    /* XXX: It would be nice to be able to assert this condition
     * here. But, we actually allow one 'cheat' that is used when
     * painting the final image-based fallbacks. The final fallbacks
     * do have alpha which we support by blending with white. This is
     * possible only because there is nothing between the fallback
     * images and the paper, nor is anything painted above. */
    /*
    assert (_operation_supported (surface, op, source));
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
	xmlNodePtr child = surface->xml_root_node->children;
	
	while (child != NULL) {
	    xmlUnlinkNode (child);
	    xmlFreeNode (child);
	    child = surface->xml_root_node->children;
	}

	if (op == CAIRO_OPERATOR_CLEAR) {
	    if (surface->content == CAIRO_CONTENT_COLOR) {
		xmlNodePtr rect;
		char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

		rect = xmlNewChild (surface->xml_node, NULL, CC2XML ("rect"), NULL);
		_cairo_dtostr (buffer, sizeof buffer, surface->width);
		xmlSetProp (rect, CC2XML ("width"), C2XML (buffer));
		_cairo_dtostr (buffer, sizeof buffer, surface->height);
		xmlSetProp (rect, CC2XML ("height"), C2XML (buffer));
		xmlSetProp (rect, CC2XML ("style"), CC2XML ("opacity:1; stroke:none; fill:rgb(0,0,0);"));
	    } 
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    emit_paint (surface->xml_node, surface, op, source);
    
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
    xmlNodePtr child, mask_node;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _analyze_operation (surface, op, source);

    assert (_operation_supported (surface, op, source));
    
    emit_alpha_filter (document);

    mask_node = xmlNewChild (document->xml_node_defs, NULL, CC2XML ("mask"), NULL);
    snprintf (buffer, sizeof buffer, "mask%d", document->mask_id);
    xmlSetProp (mask_node, CC2XML ("id"), C2XML (buffer));
    child = xmlNewChild (mask_node, NULL, CC2XML ("g"), NULL);
    xmlSetProp (child, CC2XML ("filter"), CC2XML ("url(#alpha)"));
    emit_paint (child, surface, op, mask);

    /* mask node need to be located after surface it references,
     * but also needs to be linked to xml tree for xlink namespace.
     * So we unlink readd it here. */
    xmlUnlinkNode (mask_node);
    xmlAddChild (document->xml_node_defs, mask_node);

    child = emit_paint (surface->xml_node, surface, op, source);

    if (child) {
	snprintf (buffer, sizeof buffer, "url(#mask%d)", document->mask_id);
	xmlSetProp (child, CC2XML ("mask"), C2XML (buffer));
    }

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
    cairo_svg_document_t *document = surface->document;
    cairo_status_t status;
    xmlBufferPtr style;
    xmlNodePtr child;
    svg_path_info_t info;
    unsigned int i;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    
    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _analyze_operation (surface, op, source);

    assert (_operation_supported (surface, op, source));

    info.document = document;
    info.path = xmlBufferCreate ();
    info.ctm_inverse = ctm_inverse;

    style = xmlBufferCreate ();
    emit_pattern (surface, source, style, 1);
    xmlBufferCat (style, CC2XML ("fill: none; stroke-width: "));
    _cairo_dtostr (buffer, sizeof buffer, stroke_style->line_width);
    xmlBufferCat (style, C2XML (buffer)); 
    xmlBufferCat (style, CC2XML (";"));
    
    switch (stroke_style->line_cap) {
	    case CAIRO_LINE_CAP_BUTT:
		    xmlBufferCat (style, CC2XML ("stroke-linecap: butt;"));
		    break;
	    case CAIRO_LINE_CAP_ROUND:
		    xmlBufferCat (style, CC2XML ("stroke-linecap: round;"));
		    break;
	    case CAIRO_LINE_CAP_SQUARE:
		    xmlBufferCat (style, CC2XML ("stroke-linecap: square;"));
		    break;
    }

    switch (stroke_style->line_join) {
	    case CAIRO_LINE_JOIN_MITER:
		    xmlBufferCat (style, CC2XML ("stroke-linejoin: miter;"));
		    break; 
	    case CAIRO_LINE_JOIN_ROUND:
		    xmlBufferCat (style, CC2XML ("stroke-linejoin: round;"));
		    break; 
	    case CAIRO_LINE_JOIN_BEVEL:
		    xmlBufferCat (style, CC2XML ("stroke-linejoin: bevel;"));
		    break; 
    }

    if (stroke_style->num_dashes > 0) {
	xmlBufferCat (style, CC2XML (" stroke-dasharray: "));
	for (i = 0; i < stroke_style->num_dashes; i++) {
	    if (i != 0)
		xmlBufferCat (style, CC2XML (","));
	    _cairo_dtostr (buffer, sizeof buffer, stroke_style->dash[i]);
	    xmlBufferCat (style, C2XML (buffer));
	}
	xmlBufferCat (style, CC2XML (";"));
	if (stroke_style->dash_offset != 0.0) {
	    xmlBufferCat (style, CC2XML (" stroke-dashoffset: "));
	    _cairo_dtostr (buffer, sizeof buffer, stroke_style->dash_offset);
	    xmlBufferCat (style, C2XML (buffer));
	    xmlBufferCat (style, CC2XML (";"));
	}
    }

    xmlBufferCat (style, CC2XML (" stroke-miterlimit: "));
    _cairo_dtostr (buffer, sizeof buffer, stroke_style->miter_limit);
    xmlBufferCat (style, C2XML (buffer));
    xmlBufferCat (style, CC2XML (";"));

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_svg_path_move_to,
					  _cairo_svg_path_line_to,
					  _cairo_svg_path_curve_to,
					  _cairo_svg_path_close_path,
					  &info);
    
    child = xmlNewChild (surface->xml_node, NULL, CC2XML ("path"), NULL);
    emit_transform (child, "transform", ctm);
    xmlSetProp (child, CC2XML ("d"), xmlBufferContent (info.path));
    xmlSetProp (child, CC2XML ("style"), xmlBufferContent (style));
    emit_operator (child, surface, op);
    
    xmlBufferFree (info.path);
    xmlBufferFree (style);

    return status;
}

static cairo_int_status_t
_cairo_svg_surface_show_glyphs (void			*abstract_surface,
				cairo_operator_t	 op,
				cairo_pattern_t		*pattern,
				const cairo_glyph_t	*glyphs,
				int			 num_glyphs,
				cairo_scaled_font_t	*scaled_font)
{
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_svg_document_t *document = surface->document;
    cairo_path_fixed_t path;
    cairo_status_t status;
    xmlNodePtr glyph_node;
    xmlNodePtr child;
    xmlBufferPtr style;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    unsigned int font_id, subset_id, subset_glyph_index;
    int i;
    
    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _analyze_operation (surface, op, pattern);

    assert (_operation_supported (surface, op, pattern));

    if (num_glyphs <= 0)
	return CAIRO_STATUS_SUCCESS;

    /* FIXME it's probably possible to apply a pattern of a gradient to
     * a group of symbols, but I don't know how yet. Gradients or patterns
     * are translated by x and y properties of use element. */
    if (pattern->type != CAIRO_PATTERN_TYPE_SOLID)
	goto FALLBACK;

    child = xmlNewChild (surface->xml_node, NULL, CC2XML ("g"), NULL);
    style = xmlBufferCreate ();
    emit_pattern (surface, pattern, style, 0);
    xmlSetProp (child, CC2XML ("style"), xmlBufferContent (style));
    xmlBufferFree (style);

    for (i = 0; i < num_glyphs; i++) {
	status = _cairo_scaled_font_subsets_map_glyph (document->font_subsets,
						       scaled_font, glyphs[i].index,
						       &font_id, &subset_id, &subset_glyph_index);
	if (status) {
	    glyphs += i;
	    num_glyphs -= i;
	    goto FALLBACK;
	}

	glyph_node = xmlNewChild (child, NULL, CC2XML ("use"), NULL);
	snprintf (buffer, sizeof buffer, "#glyph%d-%d", 
		  font_id, subset_glyph_index);
	xmlSetProp (glyph_node, CC2XML ("xlink:href"), C2XML (buffer)); 
	_cairo_dtostr (buffer, sizeof buffer, glyphs[i].x);
	xmlSetProp (glyph_node, CC2XML ("x"), C2XML (buffer));
	_cairo_dtostr (buffer, sizeof buffer, glyphs[i].y);
	xmlSetProp (glyph_node, CC2XML ("y"), C2XML (buffer));
    }

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
    xmlNodePtr group, clip, clip_path;
    svg_path_info_t info;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    if (path == NULL) {
	surface->xml_node = surface->xml_root_node;
	surface->clip_level = 0;
	return CAIRO_STATUS_SUCCESS;
    }

    if (path != NULL) {
	info.document = document;
	info.path = xmlBufferCreate ();
	info.ctm_inverse = NULL;

	group = xmlNewChild (surface->xml_node, NULL, CC2XML ("g"), NULL);
	clip = xmlNewChild (document->xml_node_defs, NULL, CC2XML ("clipPath"), NULL);
	snprintf (buffer, sizeof buffer, "clip%d", document->clip_id);
	xmlSetProp (clip, CC2XML ("id"), C2XML (buffer));

	clip_path = xmlNewChild (clip, NULL, CC2XML ("path"), NULL);
	status = _cairo_path_fixed_interpret (path,
					      CAIRO_DIRECTION_FORWARD,
					      _cairo_svg_path_move_to,
					      _cairo_svg_path_line_to,
					      _cairo_svg_path_curve_to,
					      _cairo_svg_path_close_path,
					      &info);
	xmlSetProp (clip_path, CC2XML ("d"), xmlBufferContent (info.path));
	xmlBufferFree (info.path);

	snprintf (buffer, sizeof buffer, "url(#clip%d)", document->clip_id);
	xmlSetProp (group, CC2XML ("clip-path"), C2XML (buffer));
	xmlSetProp (group, CC2XML ("clip-rule"), 
		    fill_rule == CAIRO_FILL_RULE_EVEN_ODD ? 
		    CC2XML ("evenodd") : CC2XML ("nonzero"));

	document->clip_id++;
	surface->xml_node = group;
	surface->clip_level++;
    } 

    return status;
}

static void
_cairo_svg_surface_get_font_options (void                  *abstract_surface,
				     cairo_font_options_t  *options)
{
  _cairo_font_options_init_default (options);

  cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_NONE);
  cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_OFF);
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
	NULL, /* copy_page */
	NULL, /* show_page */
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
    xmlDocPtr doc;
    xmlNodePtr node;
    xmlBufferPtr xml_buffer;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    document = malloc (sizeof (cairo_svg_document_t));
    if (document == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    /* The use of defs for font glyphs imposes no per-subset limit. */
    document->font_subsets = _cairo_scaled_font_subsets_create (0);
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
    document->x_dpi = CAIRO_SVG_DEFAULT_DPI;
    document->y_dpi = CAIRO_SVG_DEFAULT_DPI;

    document->surface_id = 0;
    document->linear_pattern_id = 0;
    document->radial_pattern_id = 0;
    document->pattern_id = 0;
    document->filter_id = 0;
    document->clip_id = 0;
    document->mask_id = 0;

    doc = xmlNewDoc (CC2XML ("1.0")); 
    node = xmlNewNode (NULL, CC2XML ("svg"));

    xmlDocSetRootElement (doc, node);

    document->xml_doc = doc;
    document->xml_node_main = node;
    document->xml_node_defs = xmlNewChild (node, NULL, CC2XML ("defs"), NULL); 

    xml_buffer = xmlBufferCreate ();
    
    _cairo_dtostr (buffer, sizeof buffer, width);
    xmlBufferCat (xml_buffer, C2XML (buffer));
    xmlBufferCat (xml_buffer, CC2XML ("pt"));
    xmlSetProp (node, CC2XML ("width"), C2XML (xmlBufferContent (xml_buffer)));
    xmlBufferEmpty (xml_buffer);
    
    _cairo_dtostr (buffer, sizeof buffer, height);
    xmlBufferCat (xml_buffer, C2XML (buffer));
    xmlBufferCat (xml_buffer, CC2XML ("pt"));
    xmlSetProp (node, CC2XML ("height"), C2XML (xmlBufferContent (xml_buffer)));
    xmlBufferEmpty (xml_buffer);

    xmlBufferCat (xml_buffer, CC2XML ("0 0 "));
    _cairo_dtostr (buffer, sizeof buffer, width);
    xmlBufferCat (xml_buffer, C2XML (buffer));
    xmlBufferCat (xml_buffer, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, height);
    xmlBufferCat (xml_buffer, C2XML (buffer));
    xmlSetProp (node, CC2XML ("viewBox"), C2XML (xmlBufferContent (xml_buffer)));
    
    xmlBufferFree (xml_buffer);

    xmlNewNs (node, CC2XML ("http://www.w3.org/2000/svg"), NULL);
    xmlNewNs (node, CC2XML ("http://www.w3.org/1999/xlink"), CC2XML ("xlink"));

    document->xml_node_glyphs = xmlNewChild (document->xml_node_defs, NULL,
					     CC2XML ("g"), NULL);

    document->alpha_filter = FALSE;

    _cairo_array_init (&document->meta_snapshots, sizeof (cairo_meta_snapshot_t));

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

static int
_cairo_svg_document_write (cairo_output_stream_t *output_stream,
			   const char * buffer,
			   int len)
{
    cairo_status_t status;
    
    _cairo_output_stream_write (output_stream, buffer, len);
    status = _cairo_output_stream_get_status (output_stream);
    if (status) {
	_cairo_error (status);
	return -1;
    }

    return len;
}

static cairo_status_t
_cairo_svg_document_finish (cairo_svg_document_t *document)
{
    cairo_status_t status;
    cairo_output_stream_t *output = document->output_stream;
    cairo_meta_snapshot_t *snapshot;
    xmlOutputBufferPtr xml_output_buffer;
    unsigned int i;

    if (document->finished)
	return CAIRO_STATUS_SUCCESS;

    _cairo_svg_document_emit_font_subsets (document);

    xmlSetProp (document->xml_node_main, CC2XML ("version"), 
	CC2XML (_cairo_svg_internal_version_strings [document->svg_version]));

    xml_output_buffer = xmlOutputBufferCreateIO ((xmlOutputWriteCallback) _cairo_svg_document_write,
						 (xmlOutputCloseCallback) NULL,
						 (void *) document->output_stream, 
						 NULL);
    xmlSaveFormatFileTo (xml_output_buffer, document->xml_doc, "UTF-8", 1);

    xmlFreeDoc (document->xml_doc);

    status = _cairo_output_stream_get_status (output);
    _cairo_output_stream_destroy (output);

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

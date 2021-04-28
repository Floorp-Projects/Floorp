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

#define _DEFAULT_SOURCE /* for snprintf() */
#include "cairoint.h"

#include "cairo-svg.h"

#include "cairo-array-private.h"
#include "cairo-analysis-surface-private.h"
#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-image-info-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-recording-surface-inline.h"
#include "cairo-output-stream-private.h"
#include "cairo-path-fixed-private.h"
#include "cairo-paginated-private.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-surface-clipper-private.h"
#include "cairo-surface-snapshot-inline.h"
#include "cairo-svg-surface-private.h"

/**
 * SECTION:cairo-svg
 * @Title: SVG Surfaces
 * @Short_Description: Rendering SVG documents
 * @See_Also: #cairo_surface_t
 *
 * The SVG surface is used to render cairo graphics to
 * SVG files and is a multi-page vector surface backend.
 **/

/**
 * CAIRO_HAS_SVG_SURFACE:
 *
 * Defined if the SVG surface backend is available.
 * This macro can be used to conditionally compile backend-specific code.
 *
 * Since: 1.2
 **/

typedef struct cairo_svg_page cairo_svg_page_t;

static const int invalid_pattern_id = -1;

static const cairo_svg_version_t _cairo_svg_versions[] =
{
    CAIRO_SVG_VERSION_1_1,
    CAIRO_SVG_VERSION_1_2
};

#define CAIRO_SVG_VERSION_LAST ARRAY_LENGTH (_cairo_svg_versions)

static const char *_cairo_svg_supported_mime_types[] =
{
    CAIRO_MIME_TYPE_JPEG,
    CAIRO_MIME_TYPE_PNG,
    CAIRO_MIME_TYPE_UNIQUE_ID,
    CAIRO_MIME_TYPE_URI,
    NULL
};

static void
_cairo_svg_surface_emit_path (cairo_output_stream_t	*output,
			      const cairo_path_fixed_t	*path,
			      const cairo_matrix_t	*ctm_inverse);

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

static const char * _cairo_svg_unit_strings[] =
{
    "",
    "em",
    "ex",
    "px",
    "in",
    "cm",
    "mm",
    "pt",
    "pc",
    "%"
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
    cairo_svg_unit_t unit;

    cairo_output_stream_t *xml_node_defs;
    cairo_output_stream_t *xml_node_glyphs;

    unsigned int linear_pattern_id;
    unsigned int radial_pattern_id;
    unsigned int pattern_id;
    unsigned int filter_id;
    unsigned int clip_id;
    unsigned int mask_id;

    cairo_bool_t alpha_filter;

    cairo_svg_version_t svg_version;

    cairo_scaled_font_subsets_t *font_subsets;
};

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
					double			 height,
					cairo_bool_t             bounded);

static cairo_surface_t *
_cairo_svg_surface_create_for_stream_internal (cairo_output_stream_t	*stream,
					       double			 width,
					       double			 height,
					       cairo_svg_version_t	 version);

static const cairo_surface_backend_t cairo_svg_surface_backend;
static const cairo_paginated_surface_backend_t cairo_svg_surface_paginated_backend;

/**
 * cairo_svg_surface_create_for_stream:
 * @write_func: a #cairo_write_func_t to accept the output data, may be %NULL
 *              to indicate a no-op @write_func. With a no-op @write_func,
 *              the surface may be queried or used as a source without
 *              generating any temporary files.
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
 **/
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
 * @filename: a filename for the SVG output (must be writable), %NULL may be
 *            used to specify no output. This will generate a SVG surface that
 *            may be queried and used as a source, without generating a
 *            temporary file.
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 *
 * Creates a SVG surface of the specified size in points to be written
 * to @filename.
 *
 * The SVG surface backend recognizes the following MIME types for the
 * data attached to a surface (see cairo_surface_set_mime_data()) when
 * it is used as a source pattern for drawing on this surface:
 * %CAIRO_MIME_TYPE_JPEG, %CAIRO_MIME_TYPE_PNG,
 * %CAIRO_MIME_TYPE_URI. If any of them is specified, the SVG backend
 * emits a href with the content of MIME data instead of a surface
 * snapshot (PNG, Base64-encoded) in the corresponding image tag.
 *
 * The unofficial MIME type %CAIRO_MIME_TYPE_URI is examined
 * first. If present, the URI is emitted as is: assuring the
 * correctness of URI is left to the client code.
 *
 * If %CAIRO_MIME_TYPE_URI is not present, but %CAIRO_MIME_TYPE_JPEG
 * or %CAIRO_MIME_TYPE_PNG is specified, the corresponding data is
 * Base64-encoded and emitted.
 *
 * If %CAIRO_MIME_TYPE_UNIQUE_ID is present, all surfaces with the same
 * unique identifier will only be embedded once.
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
 * target. Otherwise return FALSE.
 */
static cairo_bool_t
_extract_svg_surface (cairo_surface_t		 *surface,
		      cairo_svg_surface_t	**svg_surface)
{
    cairo_surface_t *target;
    cairo_status_t status_ignored;

    if (surface->status)
	return FALSE;
    if (surface->finished) {
	status_ignored = _cairo_surface_set_error (surface,
						   _cairo_error (CAIRO_STATUS_SURFACE_FINISHED));
        return FALSE;
    }

    if (! _cairo_surface_is_paginated (surface)) {
	status_ignored = _cairo_surface_set_error (surface,
						   _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));
	return FALSE;
    }

    target = _cairo_paginated_surface_get_target (surface);
    if (target->status) {
	status_ignored = _cairo_surface_set_error (surface,
						   target->status);
	return FALSE;
    }
    if (target->finished) {
	status_ignored = _cairo_surface_set_error (surface,
						   _cairo_error (CAIRO_STATUS_SURFACE_FINISHED));
        return FALSE;
    }

    if (! _cairo_surface_is_svg (target)) {
	status_ignored = _cairo_surface_set_error (surface,
						   _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));
	return FALSE;
    }

    *svg_surface = (cairo_svg_surface_t *) target;
    return TRUE;
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
cairo_svg_surface_restrict_to_version (cairo_surface_t		*abstract_surface,
				       cairo_svg_version_t	 version)
{
    cairo_svg_surface_t *surface = NULL; /* hide compiler warning */

    if (! _extract_svg_surface (abstract_surface, &surface))
	return;

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
                        int				 *num_versions)
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

/**
 * cairo_svg_surface_set_document_unit:
 * @surface: a SVG #cairo_surface_t
 * @unit: SVG unit
 *
 * Use the specified unit for the width and height of the generated SVG file.
 * See #cairo_svg_unit_t for a list of available unit values that can be used
 * here.
 *
 * This function can be called at any time before generating the SVG file.
 *
 * However to minimize the risk of ambiguities it's recommended to call it
 * before any drawing operations have been performed on the given surface, to
 * make it clearer what the unit used in the drawing operations is.
 *
 * The simplest way to do this is to call this function immediately after
 * creating the SVG surface.
 *
 * Note if this function is never called, the default unit for SVG documents
 * generated by cairo will be "pt". This is for historical reasons.
 *
 * Since: 1.16
 **/
void
cairo_svg_surface_set_document_unit (cairo_surface_t	*abstract_surface,
				     cairo_svg_unit_t	 unit)
{
    cairo_svg_surface_t *surface = NULL; /* hide compiler warning */

    if (! _extract_svg_surface (abstract_surface, &surface))
	return;

    if (unit <= CAIRO_SVG_UNIT_PERCENT)
	surface->document->unit = unit;
}

/**
 * cairo_svg_surface_get_document_unit:
 * @surface: a SVG #cairo_surface_t
 *
 * Get the unit of the SVG surface.
 *
 * If the surface passed as an argument is not a SVG surface, the function
 * sets the error status to CAIRO_STATUS_SURFACE_TYPE_MISMATCH and returns
 * CAIRO_SVG_UNIT_USER.
 *
 * Return value: the SVG unit of the SVG surface.
 *
 * Since: 1.16
 **/
cairo_svg_unit_t
cairo_svg_surface_get_document_unit (cairo_surface_t	*abstract_surface)
{
    cairo_svg_surface_t *surface = NULL; /* hide compiler warning */

    if (! _extract_svg_surface (abstract_surface, &surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return CAIRO_SVG_UNIT_USER;
    }

    return surface->document->unit;
}

static void
_cairo_svg_source_surface_init_key (cairo_svg_source_surface_t *key)
{
    if (key->unique_id && key->unique_id_length > 0) {
	key->base.hash = _cairo_hash_bytes (_CAIRO_HASH_INIT_VALUE,
					    key->unique_id, key->unique_id_length);
    } else {
	key->base.hash = key->id;
    }
}

static cairo_bool_t
_cairo_svg_source_surface_equal (const void *key_a, const void *key_b)
{
    const cairo_svg_source_surface_t *a = key_a;
    const cairo_svg_source_surface_t *b = key_b;

    if (a->unique_id && b->unique_id && a->unique_id_length == b->unique_id_length)
	return (memcmp (a->unique_id, b->unique_id, a->unique_id_length) == 0);

    return (a->id == b->id);
}

static void
_cairo_svg_source_surface_pluck (void *entry, void *closure)
{
    cairo_svg_source_surface_t *surface_entry = entry;
    cairo_hash_table_t *patterns = closure;

    _cairo_hash_table_remove (patterns, &surface_entry->base);
    free (surface_entry->unique_id);
    free (surface_entry);
}

static cairo_status_t
_cairo_svg_surface_add_source_surface (cairo_svg_surface_t  *surface,
				       cairo_surface_t	    *source_surface,
				       int                  *source_id,
				       cairo_bool_t         *is_new)
{
    cairo_svg_source_surface_t source_key;
    cairo_svg_source_surface_t *source_entry;
    unsigned char *unique_id = NULL;
    unsigned long unique_id_length = 0;
    cairo_status_t status;

    source_key.id  = source_surface->unique_id;
    cairo_surface_get_mime_data (source_surface, CAIRO_MIME_TYPE_UNIQUE_ID,
				 (const unsigned char **) &source_key.unique_id,
				 &source_key.unique_id_length);
    _cairo_svg_source_surface_init_key (&source_key);
    source_entry = _cairo_hash_table_lookup (surface->source_surfaces, &source_key.base);
    if (source_entry) {
	*source_id = source_entry->id;
	*is_new = FALSE;
	return CAIRO_STATUS_SUCCESS;
    }

    if (source_key.unique_id && source_key.unique_id_length > 0) {
	unique_id = _cairo_malloc (source_key.unique_id_length);
	if (unique_id == NULL) {
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	unique_id_length = source_key.unique_id_length;
	memcpy (unique_id, source_key.unique_id, unique_id_length);
    } else {
	unique_id = NULL;
	unique_id_length = 0;
    }

    source_entry = malloc (sizeof (cairo_svg_source_surface_t));
    if (source_entry == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail;
    }

    source_entry->id = source_key.id;
    source_entry->unique_id_length = unique_id_length;
    source_entry->unique_id = unique_id;
    _cairo_svg_source_surface_init_key (source_entry);
    status = _cairo_hash_table_insert (surface->source_surfaces, &source_entry->base);
    if (unlikely(status))
	goto fail;

    *is_new = TRUE;
    *source_id = source_entry->id;
    return CAIRO_STATUS_SUCCESS;

  fail:
    free (unique_id);
    free (source_entry);
    return status;
}

static cairo_bool_t
_cliprect_covers_surface (cairo_svg_surface_t *surface,
			  cairo_path_fixed_t *path)
{
    cairo_box_t box;

    if (_cairo_path_fixed_is_box (path, &box)) {
	if (box.p1.x <= 0 &&
	    box.p1.y <= 0 &&
	    _cairo_fixed_to_double (box.p2.x) >= surface->width &&
	    _cairo_fixed_to_double (box.p2.y) >= surface->height)
	{
	    return TRUE;
	}
    }

    return FALSE;
}

static cairo_status_t
_cairo_svg_surface_clipper_intersect_clip_path (cairo_surface_clipper_t *clipper,
						cairo_path_fixed_t	*path,
						cairo_fill_rule_t	 fill_rule,
						double			 tolerance,
						cairo_antialias_t	 antialias)
{
    cairo_svg_surface_t *surface = cairo_container_of (clipper,
						       cairo_svg_surface_t,
						       clipper);
    cairo_svg_document_t *document = surface->document;
    unsigned int i;

    if (path == NULL) {
	for (i = 0; i < surface->clip_level; i++)
	    _cairo_output_stream_printf (surface->xml_node, "</g>\n");

	surface->clip_level = 0;
	return CAIRO_STATUS_SUCCESS;
    }

    /* skip trivial whole-page clips */
    if (_cliprect_covers_surface (surface, path))
	return CAIRO_STATUS_SUCCESS;

    _cairo_output_stream_printf (document->xml_node_defs,
				 "<clipPath id=\"clip%d\">\n"
				 "  <path ",
				 document->clip_id);
    _cairo_svg_surface_emit_path (document->xml_node_defs, path, NULL);

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

static cairo_surface_t *
_cairo_svg_surface_create_for_document (cairo_svg_document_t	*document,
					cairo_content_t		 content,
					double			 width,
					double			 height,
					cairo_bool_t             bounded)
{
    cairo_svg_surface_t *surface;
    cairo_surface_t *paginated;
    cairo_status_t status, status_ignored;

    surface = _cairo_malloc (sizeof (cairo_svg_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base,
			 &cairo_svg_surface_backend,
			 NULL, /* device */
			 content,
			 TRUE); /* is_vector */

    surface->width = width;
    surface->height = height;
    surface->surface_bounded = bounded;

    surface->document = _cairo_svg_document_reference (document);

    surface->clip_level = 0;
    _cairo_surface_clipper_init (&surface->clipper,
				 _cairo_svg_surface_clipper_intersect_clip_path);

    surface->base_clip = document->clip_id++;
    surface->is_base_clip_emitted = FALSE;

    surface->xml_node = _cairo_memory_stream_create ();
    status = _cairo_output_stream_get_status (surface->xml_node);
    if (unlikely (status))
	goto CLEANUP;

    _cairo_array_init (&surface->page_set, sizeof (cairo_svg_page_t));

    if (content == CAIRO_CONTENT_COLOR) {
	_cairo_output_stream_printf (surface->xml_node,
				     "<rect width=\"%f\" height=\"%f\" "
				     "style=\"opacity:1;stroke:none;"
				     "fill:rgb(0,0,0);\"/>\n",
				     width, height);
	status = _cairo_output_stream_get_status (surface->xml_node);
	if (unlikely (status))
	    goto CLEANUP;
    }

    surface->paginated_mode = CAIRO_PAGINATED_MODE_ANALYZE;
    surface->force_fallbacks = FALSE;
    surface->content = content;

    surface->source_surfaces = _cairo_hash_table_create (_cairo_svg_source_surface_equal);
    if (unlikely (surface->source_surfaces == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP;
    }

    paginated = _cairo_paginated_surface_create (&surface->base,
					         surface->content,
						 &cairo_svg_surface_paginated_backend);
    status = paginated->status;
    if (status == CAIRO_STATUS_SUCCESS) {
	/* paginated keeps the only reference to surface now, drop ours */
	cairo_surface_destroy (&surface->base);
	return paginated;
    }

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
    if (unlikely (status)) {
	surface =  _cairo_surface_create_in_error (status);
	/* consume the output stream on behalf of caller */
	status = _cairo_output_stream_destroy (stream);
	return surface;
    }

    surface = _cairo_svg_surface_create_for_document (document, CAIRO_CONTENT_COLOR_ALPHA,
						      width, height, TRUE);
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
    cairo_svg_page_t page;
    cairo_output_stream_t *stream;
    cairo_int_status_t status;
    unsigned int i;

    stream = _cairo_memory_stream_create ();
    if (_cairo_output_stream_get_status (stream)) {
	status = _cairo_output_stream_destroy (stream);
	return NULL;
    }

    page.surface_id = surface->base.unique_id;
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

    _cairo_surface_clipper_reset (&surface->clipper);

    return _cairo_array_index (&surface->page_set,
			       surface->page_set.num_elements - 1);
}

static cairo_int_status_t
_cairo_svg_surface_copy_page (void *abstract_surface)
{
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_svg_page_t *page;

    page = _cairo_svg_surface_store_page (surface);
    if (unlikely (page == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_memory_stream_copy (page->xml_node, surface->xml_node);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_show_page (void *abstract_surface)
{
    cairo_svg_surface_t *surface = abstract_surface;

    if (unlikely (_cairo_svg_surface_store_page (surface) == NULL))
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

typedef struct {
    cairo_output_stream_t *output;
    const cairo_matrix_t *ctm_inverse;
} svg_path_info_t;

static cairo_status_t
_cairo_svg_path_move_to (void *closure,
			 const cairo_point_t *point)
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
_cairo_svg_path_line_to (void *closure,
			 const cairo_point_t *point)
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
			  const cairo_point_t *b,
			  const cairo_point_t *c,
			  const cairo_point_t *d)
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

static void
_cairo_svg_surface_emit_path (cairo_output_stream_t	*output,
			      const cairo_path_fixed_t	*path,
			      const cairo_matrix_t	*ctm_inverse)
{
    cairo_status_t status;
    svg_path_info_t info;

    _cairo_output_stream_printf (output, "d=\"");

    info.output = output;
    info.ctm_inverse = ctm_inverse;
    status = _cairo_path_fixed_interpret (path,
					  _cairo_svg_path_move_to,
					  _cairo_svg_path_line_to,
					  _cairo_svg_path_curve_to,
					  _cairo_svg_path_close_path,
					  &info);
    assert (status == CAIRO_STATUS_SUCCESS);

    _cairo_output_stream_printf (output, "\"");
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
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (document->xml_node_glyphs,
				 "<path style=\"stroke:none;\" ");

    _cairo_svg_surface_emit_path (document->xml_node_glyphs,
				  scaled_glyph->path, NULL);

    _cairo_output_stream_printf (document->xml_node_glyphs,
				 "/>\n");

    return status;
}

static cairo_int_status_t
_cairo_svg_document_emit_bitmap_glyph_data (cairo_svg_document_t	*document,
					    cairo_scaled_font_t		*scaled_font,
					    unsigned long		 glyph_index)
{
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_image_surface_t *image;
    cairo_status_t status;
    uint8_t *row, *byte;
    int rows, cols;
    int x, y, bit;

    status = _cairo_scaled_glyph_lookup (scaled_font,
					 glyph_index,
					 CAIRO_SCALED_GLYPH_INFO_METRICS |
					 CAIRO_SCALED_GLYPH_INFO_SURFACE,
					 &scaled_glyph);
    if (unlikely (status))
	return status;

    image = _cairo_image_surface_coerce_to_format (scaled_glyph->surface,
					           CAIRO_FORMAT_A1);
    status = image->base.status;
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (document->xml_node_glyphs, "<g");
    _cairo_svg_surface_emit_transform (document->xml_node_glyphs, " transform",
				       &image->base.device_transform_inverse, NULL);
    _cairo_output_stream_printf (document->xml_node_glyphs, ">\n");

    for (y = 0, row = image->data, rows = image->height; rows; row += image->stride, rows--, y++) {
	for (x = 0, byte = row, cols = (image->width + 7) / 8; cols; byte++, cols--) {
	    uint8_t output_byte = CAIRO_BITSWAP8_IF_LITTLE_ENDIAN (*byte);
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

    cairo_surface_destroy (&image->base);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_document_emit_glyph (cairo_svg_document_t	*document,
				cairo_scaled_font_t	*scaled_font,
				unsigned long		 scaled_font_glyph_index,
				unsigned int		 font_id,
				unsigned int		 subset_glyph_index)
{
    cairo_int_status_t	     status;

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
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (document->xml_node_glyphs, "</symbol>\n");

    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_document_emit_font_subset (cairo_scaled_font_subset_t	*font_subset,
				      void				*closure)
{
    cairo_svg_document_t *document = closure;
    cairo_int_status_t status = CAIRO_INT_STATUS_SUCCESS;
    unsigned int i;

    _cairo_scaled_font_freeze_cache (font_subset->scaled_font);
    for (i = 0; i < font_subset->num_glyphs; i++) {
	status = _cairo_svg_document_emit_glyph (document,
					         font_subset->scaled_font,
					         font_subset->glyphs[i],
					         font_subset->font_id, i);
	if (unlikely (status))
	    break;
    }
    _cairo_scaled_font_thaw_cache (font_subset->scaled_font);

    return status;
}

static cairo_status_t
_cairo_svg_document_emit_font_subsets (cairo_svg_document_t *document)
{
    cairo_status_t status;

    status = _cairo_scaled_font_subsets_foreach_scaled (document->font_subsets,
                                                        _cairo_svg_document_emit_font_subset,
                                                        document);
    if (unlikely (status))
	goto FAIL;

    status = _cairo_scaled_font_subsets_foreach_user (document->font_subsets,
						      _cairo_svg_document_emit_font_subset,
						      document);

  FAIL:
    _cairo_scaled_font_subsets_destroy (document->font_subsets);
    document->font_subsets = NULL;

    return status;
}

static char const *
_cairo_svg_surface_operators[] = {
    "clear",

    "src", "src-over", "src-in",
    "src-out", "src-atop",

    "dst", "dst-over", "dst-in",
    "dst-out", "dst-atop",

    "xor", "plus",
    "color-dodge", /* FIXME: saturate ? */

    "multiply",	"screen", "overlay",
    "darken", "lighten",
    "color-dodge", "color-burn",
    "hard-light", "soft-light",
    "difference", "exclusion"
};

static cairo_bool_t
_cairo_svg_surface_analyze_operator (cairo_svg_surface_t   *surface,
				      cairo_operator_t	     op)
{
    /* guard against newly added operators */
    if (op >= ARRAY_LENGTH (_cairo_svg_surface_operators))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* allow operators being NULL if they are unsupported */
    if (_cairo_svg_surface_operators[op] == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_analyze_operation (cairo_svg_surface_t   *surface,
				      cairo_operator_t	     op,
				      const cairo_pattern_t *pattern)
{
    cairo_svg_document_t *document = surface->document;

    if (surface->force_fallbacks &&
	surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (pattern->type == CAIRO_PATTERN_TYPE_MESH)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* SVG doesn't support extend reflect for image pattern */
    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE &&
	pattern->extend == CAIRO_EXTEND_REFLECT)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (document->svg_version >= CAIRO_SVG_VERSION_1_2)
	return _cairo_svg_surface_analyze_operator (surface, op);

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
    return _cairo_svg_surface_analyze_operation (surface, op, pattern) != CAIRO_INT_STATUS_UNSUPPORTED;
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

    _cairo_surface_clipper_reset (&surface->clipper);

    _cairo_hash_table_foreach (surface->source_surfaces,
			       _cairo_svg_source_surface_pluck,
			       surface->source_surfaces);
    _cairo_hash_table_destroy (surface->source_surfaces);

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
    unsigned int trailing;
    unsigned char src[3];
} base64_write_closure_t;

static char const base64_table[64] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static cairo_status_t
base64_write_func (void *closure,
		   const unsigned char *data,
		   unsigned int length)
{
    base64_write_closure_t *info = (base64_write_closure_t *) closure;
    unsigned int i;
    unsigned char *src;

    src = info->src;

    if (info->in_mem + length < 3) {
	for (i = 0; i < length; i++) {
	    src[i + info->in_mem] = *data++;
	}
	info->in_mem += length;
	return CAIRO_STATUS_SUCCESS;
    }

    do {
	unsigned char dst[4];

	for (i = info->in_mem; i < 3; i++) {
	    src[i] = *data++;
	    length--;
	}
	info->in_mem = 0;

	dst[0] = base64_table[src[0] >> 2];
	dst[1] = base64_table[(src[0] & 0x03) << 4 | src[1] >> 4];
	dst[2] = base64_table[(src[1] & 0x0f) << 2 | src[2] >> 6];
	dst[3] = base64_table[src[2] & 0xfc >> 2];
	/* Special case for the last missing bits */
	switch (info->trailing) {
	    case 2:
		dst[2] = '=';
		/* fall through */
	    case 1:
		dst[3] = '=';
	    default:
		break;
	}
	_cairo_output_stream_write (info->output, dst, 4);
    } while (length >= 3);

    for (i = 0; i < length; i++) {
	src[i] = *data++;
    }
    info->in_mem = length;

    return _cairo_output_stream_get_status (info->output);
}

static cairo_int_status_t
_cairo_surface_base64_encode_jpeg (cairo_surface_t       *surface,
				   cairo_output_stream_t *output)
{
    const unsigned char *mime_data;
    unsigned long mime_data_length;
    cairo_image_info_t image_info;
    base64_write_closure_t info;
    cairo_status_t status;

    cairo_surface_get_mime_data (surface, CAIRO_MIME_TYPE_JPEG,
				 &mime_data, &mime_data_length);
    if (mime_data == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_image_info_get_jpeg_info (&image_info, mime_data, mime_data_length);
    if (unlikely (status))
	return status;

    if (image_info.num_components == 4)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    _cairo_output_stream_printf (output, "data:image/jpeg;base64,");

    info.output = output;
    info.in_mem = 0;
    info.trailing = 0;

    status = base64_write_func (&info, mime_data, mime_data_length);
    if (unlikely (status))
	return status;

    if (info.in_mem > 0) {
	memset (info.src + info.in_mem, 0, 3 - info.in_mem);
	info.trailing = 3 - info.in_mem;
	info.in_mem = 3;
	status = base64_write_func (&info, NULL, 0);
    }

    return status;
}

static cairo_int_status_t
_cairo_surface_base64_encode_png (cairo_surface_t       *surface,
				  cairo_output_stream_t *output)
{
    const unsigned char *mime_data;
    unsigned long mime_data_length;
    base64_write_closure_t info;
    cairo_status_t status;

    cairo_surface_get_mime_data (surface, CAIRO_MIME_TYPE_PNG,
				 &mime_data, &mime_data_length);
    if (unlikely (surface->status))
	return surface->status;
    if (mime_data == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    _cairo_output_stream_printf (output, "data:image/png;base64,");

    info.output = output;
    info.in_mem = 0;
    info.trailing = 0;

    status = base64_write_func (&info, mime_data, mime_data_length);
    if (unlikely (status))
	return status;

    if (info.in_mem > 0) {
	memset (info.src + info.in_mem, 0, 3 - info.in_mem);
	info.trailing = 3 - info.in_mem;
	info.in_mem = 3;
	status = base64_write_func (&info, NULL, 0);
    }

    return status;
}

static cairo_int_status_t
_cairo_surface_base64_encode (cairo_surface_t       *surface,
			      cairo_output_stream_t *output)
{
    cairo_int_status_t status;
    base64_write_closure_t info;

    status = _cairo_surface_base64_encode_jpeg (surface, output);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    status = _cairo_surface_base64_encode_png (surface, output);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    info.output = output;
    info.in_mem = 0;
    info.trailing = 0;

    _cairo_output_stream_printf (info.output, "data:image/png;base64,");

    status = cairo_surface_write_to_png_stream (surface, base64_write_func,
						(void *) &info);

    if (unlikely (status))
	return status;

    if (info.in_mem > 0) {
	memset (info.src + info.in_mem, 0, 3 - info.in_mem);
	info.trailing = 3 - info.in_mem;
	info.in_mem = 3;
	status = base64_write_func (&info, NULL, 0);
    }

    return status;
}

static void
_cairo_svg_surface_emit_operator (cairo_output_stream_t *output,
				  cairo_svg_surface_t   *surface,
				  cairo_operator_t	 op)
{
    if (surface->document->svg_version >= CAIRO_SVG_VERSION_1_2 &&
	op != CAIRO_OPERATOR_OVER) {
	_cairo_output_stream_printf (output, " comp-op=\"%s\"", _cairo_svg_surface_operators[op]);
	if (!_cairo_operator_bounded_by_source (op))
	   _cairo_output_stream_printf (output, " clip-to-self=\"true\"");
    }
}

static void
_cairo_svg_surface_emit_operator_for_style (cairo_output_stream_t *output,
					    cairo_svg_surface_t   *surface,
					    cairo_operator_t	 op)
{
    if (surface->document->svg_version >= CAIRO_SVG_VERSION_1_2 &&
	op != CAIRO_OPERATOR_OVER) {
	_cairo_output_stream_printf (output, "comp-op:%s;", _cairo_svg_surface_operators[op]);
	if (!_cairo_operator_bounded_by_source (op))
	   _cairo_output_stream_printf (output, "clip-to-self:true;");
    }
}

/**
 * _cairo_svg_surface_emit_attr_value:
 *
 * Write the value to output the stream as a sequence of characters,
 * while escaping those which have special meaning in the XML
 * attribute's value context: &amp; and &quot;.
 **/
static void
_cairo_svg_surface_emit_attr_value (cairo_output_stream_t *stream,
				    const unsigned char *value,
				    unsigned int length)
{
    const unsigned char *p;
    const unsigned char *q;
    unsigned int i;

    /* we'll accumulate non-special chars in [q, p) range */
    p = value;
    q = p;
    for (i = 0; i < length; i++, p++) {
	if (*p == '&' || *p == '"') {
	    /* flush what's left before special char */
	    if (p != q) {
		_cairo_output_stream_write (stream, q, p - q);
		q = p + 1;
	    }

	    if (*p == '&')
		_cairo_output_stream_printf (stream, "&amp;");
	    else // p == '"'
		_cairo_output_stream_printf (stream, "&quot;");
	}
    }

    /* flush the trailing chars if any */
    if (p != q)
	_cairo_output_stream_write (stream, q, p - q);
}

static cairo_status_t
_cairo_svg_surface_emit_surface (cairo_svg_document_t *document,
				 cairo_surface_t *surface,
				 int source_id)
{
    cairo_rectangle_int_t extents;
    cairo_bool_t is_bounded;
    cairo_status_t status;
    const unsigned char *uri;
    unsigned long uri_len;

    is_bounded = _cairo_surface_get_extents (surface, &extents);
    assert (is_bounded);

    _cairo_output_stream_printf (document->xml_node_defs,
				 "<image id=\"image%d\" width=\"%d\" height=\"%d\"",
				 source_id,
				 extents.width, extents.height);

    _cairo_output_stream_printf (document->xml_node_defs, " xlink:href=\"");

    cairo_surface_get_mime_data (surface, CAIRO_MIME_TYPE_URI,
				 &uri, &uri_len);
    if (uri != NULL) {
	_cairo_svg_surface_emit_attr_value (document->xml_node_defs,
					    uri, uri_len);
    } else {
	status = _cairo_surface_base64_encode (surface,
					       document->xml_node_defs);
	if (unlikely (status))
	    return status;
    }

    _cairo_output_stream_printf (document->xml_node_defs, "\"/>\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_surface_emit_composite_surface_pattern (cairo_output_stream_t   *output,
						   cairo_svg_surface_t	 *svg_surface,
						   cairo_operator_t	  op,
						   cairo_surface_pattern_t *pattern,
						   int			  pattern_id,
						   const cairo_matrix_t	 *parent_matrix,
						   const char		 *extra_attributes)
{
    cairo_status_t status;
    cairo_matrix_t p2u;
    int source_id;
    cairo_bool_t is_new;

    p2u = pattern->base.matrix;
    status = cairo_matrix_invert (&p2u);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    status = _cairo_svg_surface_add_source_surface (svg_surface,
						    pattern->surface,
						    &source_id,
						    &is_new);
    if (unlikely (status))
	return status;

    if (is_new) {
	status = _cairo_svg_surface_emit_surface (svg_surface->document,
						  pattern->surface,
						  source_id);
	if (unlikely (status))
	    return status;
    }

    if (pattern_id != invalid_pattern_id) {
	cairo_rectangle_int_t extents;
	cairo_bool_t is_bounded;

	is_bounded = _cairo_surface_get_extents (pattern->surface, &extents);
	assert (is_bounded);

	_cairo_output_stream_printf (output,
				     "<pattern id=\"pattern%d\" "
				     "patternUnits=\"userSpaceOnUse\" "
				     "width=\"%d\" height=\"%d\" ",
				     pattern_id,
				     extents.width, extents.height);
	_cairo_svg_surface_emit_transform (output,
					   " patternTransform",
					   &p2u, parent_matrix);
	_cairo_output_stream_printf (output, ">\n  ");
    }

    _cairo_output_stream_printf (output,
				 "<use xlink:href=\"#image%d\"",
				 source_id);
    if (extra_attributes)
	_cairo_output_stream_printf (output, " %s", extra_attributes);

    if (pattern_id == invalid_pattern_id) {
	_cairo_svg_surface_emit_operator (output, svg_surface, op);
	_cairo_svg_surface_emit_transform (output,
					   " transform",
					   &p2u, parent_matrix);
    }
    _cairo_output_stream_printf (output, "/>\n");


    if (pattern_id != invalid_pattern_id)
	_cairo_output_stream_printf (output, "</pattern>\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_surface_emit_recording_surface (cairo_svg_document_t      *document,
					   cairo_recording_surface_t *source,
					   int                        source_id)
{
    cairo_status_t status;
    cairo_surface_t *paginated_surface;
    cairo_svg_surface_t *svg_surface;
    cairo_array_t *page_set;
    cairo_rectangle_int_t extents;
    cairo_bool_t bounded;
    cairo_output_stream_t *contents;

    bounded = _cairo_surface_get_extents (&source->base, &extents);
    paginated_surface = _cairo_svg_surface_create_for_document (document,
								source->base.content,
								extents.width,
								extents.height,
								bounded);
    if (unlikely (paginated_surface->status))
	return paginated_surface->status;

    svg_surface = (cairo_svg_surface_t *)
    _cairo_paginated_surface_get_target (paginated_surface);
    cairo_surface_set_fallback_resolution (paginated_surface,
					   document->owner->x_fallback_resolution,
					   document->owner->y_fallback_resolution);
    cairo_surface_set_device_offset (&svg_surface->base,
				     -source->extents_pixels.x,
				     -source->extents_pixels.y);

    status = _cairo_recording_surface_replay (&source->base, paginated_surface);
    if (unlikely (status)) {
	cairo_surface_destroy (paginated_surface);
	return status;
    }

    cairo_surface_show_page (paginated_surface);
    status = cairo_surface_status (paginated_surface);
    if (unlikely (status)) {
	cairo_surface_destroy (paginated_surface);
	return status;
    }

    if (! svg_surface->is_base_clip_emitted) {
	svg_surface->is_base_clip_emitted = TRUE;
	if (_cairo_surface_get_extents (&svg_surface->base, &extents)) {
	    _cairo_output_stream_printf (document->xml_node_defs,
					 "<clipPath id=\"clip%d\">\n"
					 "  <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\"/>\n"
					 "</clipPath>\n",
					 svg_surface->base_clip,
					 extents.x,
					 extents.y,
					 extents.width,
					 extents.height);
	}
    }

    if (source->base.content == CAIRO_CONTENT_ALPHA) {
	_cairo_svg_surface_emit_alpha_filter (document);
	_cairo_output_stream_printf (document->xml_node_defs,
				     "<g id=\"surface%d\" "
				     "clip-path=\"url(#clip%d)\" "
				     "filter=\"url(#alpha)\">\n",
				     source_id,
				     svg_surface->base_clip);
    } else {
	_cairo_output_stream_printf (document->xml_node_defs,
				     "<g id=\"surface%d\" "
				     "clip-path=\"url(#clip%d)\">\n",
				     source_id,
				     svg_surface->base_clip);
    }

    contents = svg_surface->xml_node;
    page_set = &svg_surface->page_set;

    if (_cairo_memory_stream_length (contents) > 0) {
	if (unlikely (_cairo_svg_surface_store_page (svg_surface) == NULL)) {
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

    status = cairo_surface_status (paginated_surface);
    cairo_surface_destroy (paginated_surface);

    return status;
}

static cairo_recording_surface_t *
to_recording_surface (const cairo_surface_pattern_t *pattern)
{
    cairo_surface_t *surface = pattern->surface;
    if (_cairo_surface_is_paginated (surface))
	surface = _cairo_paginated_surface_get_recording (surface);
    if (_cairo_surface_is_snapshot (surface))
	surface = _cairo_surface_snapshot_get_target (surface);
    return (cairo_recording_surface_t *) surface;
}

static cairo_status_t
_cairo_svg_surface_emit_composite_recording_pattern (cairo_output_stream_t	*output,
						     cairo_svg_surface_t	*surface,
						     cairo_operator_t	         op,
						     cairo_surface_pattern_t	*pattern,
						     int			 pattern_id,
						     const cairo_matrix_t	*parent_matrix,
						     const char			*extra_attributes)
{
    cairo_svg_document_t *document = surface->document;
    cairo_recording_surface_t *recording_surface;
    cairo_matrix_t p2u;
    cairo_status_t status;
    int source_id;
    cairo_bool_t is_new;

    p2u = pattern->base.matrix;
    status = cairo_matrix_invert (&p2u);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    status = _cairo_svg_surface_add_source_surface (surface,
						    pattern->surface,
						    &source_id,
						    &is_new);
    if (unlikely (status))
	return status;

    recording_surface = to_recording_surface (pattern);
    if (is_new) {
	status = _cairo_svg_surface_emit_recording_surface (document, recording_surface, source_id);
	if (unlikely (status))
	    return status;
    }

    if (pattern_id != invalid_pattern_id) {
	_cairo_output_stream_printf (output,
				     "<pattern id=\"pattern%d\" "
				     "patternUnits=\"userSpaceOnUse\" "
				     "width=\"%d\" height=\"%d\"",
				     pattern_id,
				     recording_surface->extents.width,
				     recording_surface->extents.height);
	_cairo_svg_surface_emit_transform (output, " patternTransform", &p2u, parent_matrix);
	_cairo_output_stream_printf (output, ">\n");
    }

    _cairo_output_stream_printf (output,
				 "<use xlink:href=\"#surface%d\"",
				 source_id);

    if (pattern_id == invalid_pattern_id) {
	_cairo_svg_surface_emit_operator (output, surface, op);
	_cairo_svg_surface_emit_transform (output, " transform", &p2u, parent_matrix);
    }

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
					   cairo_operator_t	    op,
					   cairo_surface_pattern_t *pattern,
					   int			    pattern_id,
					   const cairo_matrix_t	   *parent_matrix,
					   const char		   *extra_attributes)
{

    if (pattern->surface->type == CAIRO_SURFACE_TYPE_RECORDING) {
	return _cairo_svg_surface_emit_composite_recording_pattern (output, surface,
								    op, pattern,
								    pattern_id,
								    parent_matrix,
								    extra_attributes);
    }

    return _cairo_svg_surface_emit_composite_surface_pattern (output, surface,
							      op, pattern,
							      pattern_id,
							      parent_matrix,
							      extra_attributes);
}

static cairo_status_t
_cairo_svg_surface_emit_solid_pattern (cairo_svg_surface_t    *surface,
				       cairo_solid_pattern_t  *pattern,
				       cairo_output_stream_t  *style,
				       cairo_bool_t	       is_stroke)
{
    _cairo_output_stream_printf (style, is_stroke ?
				 "stroke:rgb(%f%%,%f%%,%f%%);stroke-opacity:%f;":
				 "fill:rgb(%f%%,%f%%,%f%%);fill-opacity:%f;",
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
	                                                surface, CAIRO_OPERATOR_SOURCE, pattern,
							pattern_id, parent_matrix, NULL);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (style,
				 "%s:url(#pattern%d);",
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
					 "stop-color:rgb(%f%%,%f%%,%f%%);"
					 "stop-opacity:%f;\"/>\n",
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
	if (unlikely (stops == NULL))
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
					 "stop-color:rgb(%f%%,%f%%,%f%%);"
					 "stop-opacity:%f;\"/>\n",
					 offset,
					 stops[i].color.red   * 100.0,
					 stops[i].color.green * 100.0,
					 stops[i].color.blue  * 100.0,
					 stops[i].color.alpha);
	}
    else {
	cairo_bool_t found = FALSE;
	unsigned int offset_index;
	cairo_color_stop_t offset_color_start, offset_color_stop;

	for (i = 0; i < n_stops; i++) {
	    if (stops[i].offset >= -start_offset) {
		if (i > 0) {
		    if (stops[i].offset != stops[i-1].offset) {
			double x0, x1;
			cairo_color_stop_t *color0, *color1;

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
				     "stop-color:rgb(%f%%,%f%%,%f%%);"
				     "stop-opacity:%f;\"/>\n",
				     offset_color_start.red   * 100.0,
				     offset_color_start.green * 100.0,
				     offset_color_start.blue  * 100.0,
				     offset_color_start.alpha);
	for (i = offset_index; i < n_stops; i++) {
	    _cairo_output_stream_printf (output,
					 "<stop offset=\"%f\" style=\""
					 "stop-color:rgb(%f%%,%f%%,%f%%);"
					 "stop-opacity:%f;\"/>\n",
					 stops[i].offset + start_offset,
					 stops[i].color.red   * 100.0,
					 stops[i].color.green * 100.0,
					 stops[i].color.blue  * 100.0,
					 stops[i].color.alpha);
	}
	for (i = 0; i < offset_index; i++) {
	    _cairo_output_stream_printf (output,
					 "<stop offset=\"%f\" style=\""
					 "stop-color:rgb(%f%%,%f%%,%f%%);"
					 "stop-opacity:%f;\"/>\n",
					 1.0 + stops[i].offset + start_offset,
					 stops[i].color.red   * 100.0,
					 stops[i].color.green * 100.0,
					 stops[i].color.blue  * 100.0,
					 stops[i].color.alpha);
	}

	_cairo_output_stream_printf (output,
				     "<stop offset=\"1\" style=\""
				     "stop-color:rgb(%f%%,%f%%,%f%%);"
				     "stop-opacity:%f;\"/>\n",
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
    cairo_matrix_t p2u;
    cairo_status_t status;

    p2u = pattern->base.base.matrix;
    status = cairo_matrix_invert (&p2u);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    _cairo_output_stream_printf (document->xml_node_defs,
				 "<linearGradient id=\"linear%d\" "
				 "gradientUnits=\"userSpaceOnUse\" "
				 "x1=\"%f\" y1=\"%f\" x2=\"%f\" y2=\"%f\" ",
				 document->linear_pattern_id,
				 pattern->pd1.x, pattern->pd1.y,
				 pattern->pd2.x, pattern->pd2.y);

    _cairo_svg_surface_emit_pattern_extend (document->xml_node_defs, &pattern->base.base),
    _cairo_svg_surface_emit_transform (document->xml_node_defs, "gradientTransform", &p2u, parent_matrix);
    _cairo_output_stream_printf (document->xml_node_defs, ">\n");

    status = _cairo_svg_surface_emit_pattern_stops (document->xml_node_defs,
	                                            &pattern->base, 0.0,
						    FALSE, FALSE);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (document->xml_node_defs,
				 "</linearGradient>\n");

    _cairo_output_stream_printf (style,
				 "%s:url(#linear%d);",
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
    cairo_circle_double_t *c0, *c1;

    extend = pattern->base.base.extend;

    if (pattern->cd1.radius < pattern->cd2.radius) {
	c0 = &pattern->cd1;
	c1 = &pattern->cd2;
	reverse_stops = FALSE;
    } else {
	c0 = &pattern->cd2;
	c1 = &pattern->cd1;
	reverse_stops = TRUE;
    }

    x0 = c0->center.x;
    y0 = c0->center.y;
    r0 = c0->radius;
    x1 = c1->center.x;
    y1 = c1->center.y;
    r1 = c1->radius;

    p2u = pattern->base.base.matrix;
    status = cairo_matrix_invert (&p2u);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    if (r0 == r1) {
	unsigned int n_stops = pattern->base.n_stops;

	_cairo_output_stream_printf (document->xml_node_defs,
				     "<radialGradient id=\"radial%d\" "
				     "gradientUnits=\"userSpaceOnUse\" "
				     "cx=\"%f\" cy=\"%f\" "
				     "fx=\"%f\" fy=\"%f\" r=\"%f\" ",
				     document->radial_pattern_id,
				     x1, y1,
				     x1, y1, r1);
	_cairo_svg_surface_emit_transform (document->xml_node_defs,
					   "gradientTransform",
					   &p2u, parent_matrix);
	_cairo_output_stream_printf (document->xml_node_defs, ">\n");

	if (extend == CAIRO_EXTEND_NONE || n_stops < 1)
	    _cairo_output_stream_printf (document->xml_node_defs,
					 "<stop offset=\"0\" style=\""
					 "stop-color:rgb(0%%,0%%,0%%);"
					 "stop-opacity:0;\"/>\n");
	else {
	    _cairo_output_stream_printf (document->xml_node_defs,
					 "<stop offset=\"0\" style=\""
					 "stop-color:rgb(%f%%,%f%%,%f%%);"
					 "stop-opacity %f;\"/>\n",
					 pattern->base.stops[0].color.red   * 100.0,
					 pattern->base.stops[0].color.green * 100.0,
					 pattern->base.stops[0].color.blue  * 100.0,
					 pattern->base.stops[0].color.alpha);
	    if (n_stops > 1)
		_cairo_output_stream_printf (document->xml_node_defs,
					     "<stop offset=\"0\" style=\""
					     "stop-color:rgb(%f%%,%f%%,%f%%);"
					     "stop-opacity:%f;\"/>\n",
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
					 "stop-color:rgb(0%%,0%%,0%%);"
					 "stop-opacity:0;\"/>\n");
	    if (r0 != 0.0)
		_cairo_output_stream_printf (document->xml_node_defs,
					     "<stop offset=\"%f\" style=\""
					     "stop-color:rgb(0%%,0%%,0%%);"
					     "stop-opacity:0;\"/>\n",
					     r0 / r1);
	}
	status = _cairo_svg_surface_emit_pattern_stops (document->xml_node_defs,
		                                        &pattern->base, offset,
							reverse_stops,
							emulate_reflect);
	if (unlikely (status))
	    return status;

	if (pattern->base.base.extend == CAIRO_EXTEND_NONE)
	    _cairo_output_stream_printf (document->xml_node_defs,
					 "<stop offset=\"1.0\" style=\""
					 "stop-color:rgb(0%%,0%%,0%%);"
					 "stop-opacity:0;\"/>\n");
    }

    _cairo_output_stream_printf (document->xml_node_defs,
				 "</radialGradient>\n");

    _cairo_output_stream_printf (style,
				 "%s:url(#radial%d);",
				 is_stroke ? "stroke" : "fill",
				 document->radial_pattern_id);

    document->radial_pattern_id++;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_surface_emit_pattern (cairo_svg_surface_t   *surface,
				 const cairo_pattern_t       *pattern,
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

    case CAIRO_PATTERN_TYPE_MESH:
    case CAIRO_PATTERN_TYPE_RASTER_SOURCE:
	ASSERT_NOT_REACHED;
    }
    return _cairo_error (CAIRO_STATUS_PATTERN_TYPE_MISMATCH);
}

static cairo_status_t
_cairo_svg_surface_emit_fill_style (cairo_output_stream_t	*output,
				    cairo_svg_surface_t		*surface,
				    cairo_operator_t		 op,
				    const cairo_pattern_t	*source,
				    cairo_fill_rule_t		 fill_rule,
				    const cairo_matrix_t	*parent_matrix)
{
    _cairo_output_stream_printf (output,
				 "fill-rule:%s;",
				 fill_rule == CAIRO_FILL_RULE_EVEN_ODD ?
				 "evenodd" : "nonzero");
    _cairo_svg_surface_emit_operator_for_style (output, surface, op);
    return _cairo_svg_surface_emit_pattern (surface, source, output, FALSE, parent_matrix);
}

static cairo_status_t
_cairo_svg_surface_emit_stroke_style (cairo_output_stream_t	   *output,
				      cairo_svg_surface_t	   *surface,
				      cairo_operator_t		    op,
				      const cairo_pattern_t	   *source,
				      const cairo_stroke_style_t   *stroke_style,
				      const cairo_matrix_t	   *parent_matrix)
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
				 "stroke-width:%f;"
				 "stroke-linecap:%s;"
				 "stroke-linejoin:%s;",
				 stroke_style->line_width,
				 line_cap,
				 line_join);

     status = _cairo_svg_surface_emit_pattern (surface, source, output, TRUE, parent_matrix);
     if (unlikely (status))
	 return status;

     _cairo_svg_surface_emit_operator_for_style (output, surface, op);

    if (stroke_style->num_dashes > 0) {
	_cairo_output_stream_printf (output, "stroke-dasharray:");
	for (i = 0; i < stroke_style->num_dashes; i++) {
	    _cairo_output_stream_printf (output, "%f",
					 stroke_style->dash[i]);
	    if (i + 1 < stroke_style->num_dashes)
		_cairo_output_stream_printf (output, ",");
	    else
		_cairo_output_stream_printf (output, ";");
	}
	if (stroke_style->dash_offset != 0.0) {
	    _cairo_output_stream_printf (output,
					 "stroke-dashoffset:%f;",
					 stroke_style->dash_offset);
	}
    }

    _cairo_output_stream_printf (output,
				 "stroke-miterlimit:%f;",
				 stroke_style->miter_limit);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_fill_stroke (void			*abstract_surface,
				cairo_operator_t	 fill_op,
				const cairo_pattern_t	*fill_source,
				cairo_fill_rule_t	 fill_rule,
				double			 fill_tolerance,
				cairo_antialias_t	 fill_antialias,
				const cairo_path_fixed_t*path,
				cairo_operator_t	 stroke_op,
				const cairo_pattern_t	*stroke_source,
				const cairo_stroke_style_t	*stroke_style,
				const cairo_matrix_t		*stroke_ctm,
				const cairo_matrix_t		*stroke_ctm_inverse,
				double			 stroke_tolerance,
				cairo_antialias_t	 stroke_antialias,
				const cairo_clip_t	*clip)
{
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_status_t status;

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->xml_node, "<path style=\"");
    status = _cairo_svg_surface_emit_fill_style (surface->xml_node, surface, fill_op,
						 fill_source, fill_rule, stroke_ctm_inverse);
    if (unlikely (status))
	return status;

    status = _cairo_svg_surface_emit_stroke_style (surface->xml_node, surface, stroke_op,
						   stroke_source, stroke_style, stroke_ctm_inverse);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->xml_node, "\" ");

    _cairo_svg_surface_emit_path (surface->xml_node, path, stroke_ctm_inverse);

    _cairo_svg_surface_emit_transform (surface->xml_node, " transform", stroke_ctm, NULL);
    _cairo_output_stream_printf (surface->xml_node, "/>\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_fill (void			*abstract_surface,
			 cairo_operator_t	 op,
			 const cairo_pattern_t	*source,
			 const cairo_path_fixed_t*path,
			 cairo_fill_rule_t	 fill_rule,
			 double			 tolerance,
			 cairo_antialias_t	 antialias,
			 const cairo_clip_t	*clip)
{
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_svg_surface_analyze_operation (surface, op, source);

    assert (_cairo_svg_surface_operation_supported (surface, op, source));

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->xml_node, "<path style=\" stroke:none;");
    status = _cairo_svg_surface_emit_fill_style (surface->xml_node, surface, op, source, fill_rule, NULL);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->xml_node, "\" ");

    _cairo_svg_surface_emit_path (surface->xml_node, path, NULL);

    _cairo_output_stream_printf (surface->xml_node, "/>\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_svg_surface_get_extents (void		        *abstract_surface,
				cairo_rectangle_int_t   *rectangle)
{
    cairo_svg_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;

    /* XXX: The conversion to integers here is pretty bogus, (not to
     * mention the arbitrary limitation of width to a short(!). We
     * may need to come up with a better interface for get_size.
     */
    rectangle->width  = ceil (surface->width);
    rectangle->height = ceil (surface->height);

    return surface->surface_bounded;
}

static cairo_status_t
_cairo_svg_surface_emit_paint (cairo_output_stream_t *output,
			       cairo_svg_surface_t   *surface,
			       cairo_operator_t	      op,
			       const cairo_pattern_t	     *source,
			       const cairo_pattern_t	     *mask_source,
			       const char	     *extra_attributes)
{
    cairo_status_t status;

    if (source->type == CAIRO_PATTERN_TYPE_SURFACE &&
	source->extend == CAIRO_EXTEND_NONE)
	return _cairo_svg_surface_emit_composite_pattern (output,
							  surface,
							  op,
							  (cairo_surface_pattern_t *) source,
							  invalid_pattern_id,
							  mask_source ? &mask_source->matrix :NULL,
							  extra_attributes);

    _cairo_output_stream_printf (output,
				 "<rect x=\"0\" y=\"0\" "
				 "width=\"%f\" height=\"%f\" "
				 "style=\"",
				 surface->width, surface->height);
    _cairo_svg_surface_emit_operator_for_style (output, surface, op);
    status = _cairo_svg_surface_emit_pattern (surface, source, output, FALSE, NULL);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (output, "stroke:none;\"");

    if (extra_attributes)
	_cairo_output_stream_printf (output, " %s", extra_attributes);

    _cairo_output_stream_printf (output, "/>\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_paint (void		    *abstract_surface,
			  cairo_operator_t   op,
			  const cairo_pattern_t   *source,
			  const cairo_clip_t	  *clip)
{
    cairo_status_t status;
    cairo_svg_surface_t *surface = abstract_surface;

    /* Emulation of clear and source operators, when no clipping region
     * is defined. We just delete existing content of surface root node,
     * and exit early if operator is clear.
     */
    if ((op == CAIRO_OPERATOR_CLEAR || op == CAIRO_OPERATOR_SOURCE) &&
	clip == NULL)
    {
	switch (surface->paginated_mode) {
	case CAIRO_PAGINATED_MODE_FALLBACK:
	    ASSERT_NOT_REACHED;
	case CAIRO_PAGINATED_MODE_ANALYZE:
	    return CAIRO_STATUS_SUCCESS;

	case CAIRO_PAGINATED_MODE_RENDER:
	    status = _cairo_output_stream_destroy (surface->xml_node);
	    if (unlikely (status)) {
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
						 "style=\"opacity:1;"
						 "stroke:none;"
						 "fill:rgb(0,0,0);\"/>\n",
						 surface->width, surface->height);
		}
		return CAIRO_STATUS_SUCCESS;
	    }
	    break;
	}
    } else {
	if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	    return _cairo_svg_surface_analyze_operation (surface, op, source);

	assert (_cairo_svg_surface_operation_supported (surface, op, source));
    }

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status))
	return status;

    return _cairo_svg_surface_emit_paint (surface->xml_node,
					  surface, op, source, 0, NULL);
}

static cairo_int_status_t
_cairo_svg_surface_mask (void		    *abstract_surface,
			 cairo_operator_t     op,
			 const cairo_pattern_t	    *source,
			 const cairo_pattern_t	    *mask,
			 const cairo_clip_t	    *clip)
{
    cairo_status_t status;
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_svg_document_t *document = surface->document;
    cairo_output_stream_t *mask_stream;
    char buffer[64];
    cairo_bool_t discard_filter = FALSE;
    unsigned int mask_id;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	cairo_status_t source_status, mask_status;

	source_status = _cairo_svg_surface_analyze_operation (surface, op, source);
	if (_cairo_status_is_error (source_status))
	    return source_status;

	if (mask->has_component_alpha) {
	    mask_status = CAIRO_INT_STATUS_UNSUPPORTED;
	} else {
	    mask_status = _cairo_svg_surface_analyze_operation (surface, op, mask);
	    if (_cairo_status_is_error (mask_status))
		return mask_status;
	}

	return _cairo_analysis_surface_merge_status (source_status,
						     mask_status);
    }

    assert (_cairo_svg_surface_operation_supported (surface, op, source));
    assert (_cairo_svg_surface_operation_supported (surface, CAIRO_OPERATOR_OVER, mask));

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status))
	return status;

    if (mask->type == CAIRO_PATTERN_TYPE_SURFACE) {
	const cairo_surface_pattern_t *surface_pattern = (const cairo_surface_pattern_t*) mask;
	cairo_content_t content = surface_pattern->surface->content;
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
    status = _cairo_svg_surface_emit_paint (mask_stream, surface, CAIRO_OPERATOR_OVER, mask, source, NULL);
    if (unlikely (status)) {
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
    if (unlikely (status))
	return status;

    snprintf (buffer, sizeof buffer, "mask=\"url(#mask%d)\"",
	      mask_id);
    status = _cairo_svg_surface_emit_paint (surface->xml_node, surface, op, source, 0, buffer);
    if (unlikely (status))
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_stroke (void			*abstract_dst,
			   cairo_operator_t      op,
			   const cairo_pattern_t *source,
			   const cairo_path_fixed_t*path,
			   const cairo_stroke_style_t *stroke_style,
			   const cairo_matrix_t	*ctm,
			   const cairo_matrix_t	*ctm_inverse,
			   double		 tolerance,
			   cairo_antialias_t	 antialias,
			   const cairo_clip_t	*clip)
{
    cairo_svg_surface_t *surface = abstract_dst;
    cairo_status_t status;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_svg_surface_analyze_operation (surface, op, source);

    assert (_cairo_svg_surface_operation_supported (surface, op, source));

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->xml_node, "<path style=\"fill:none;");
    status = _cairo_svg_surface_emit_stroke_style (surface->xml_node, surface, op,
						   source, stroke_style, ctm_inverse);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->xml_node, "\" ");

    _cairo_svg_surface_emit_path (surface->xml_node, path, ctm_inverse);

    _cairo_svg_surface_emit_transform (surface->xml_node, " transform", ctm, NULL);
    _cairo_output_stream_printf (surface->xml_node, "/>\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_show_glyphs (void			*abstract_surface,
				cairo_operator_t	 op,
				const cairo_pattern_t	*pattern,
				cairo_glyph_t		*glyphs,
				int			 num_glyphs,
				cairo_scaled_font_t	*scaled_font,
				const cairo_clip_t	*clip)
{
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_svg_document_t *document = surface->document;
    cairo_path_fixed_t path;
    cairo_int_status_t status;
    cairo_scaled_font_subsets_glyph_t subset_glyph;
    int i;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_svg_surface_analyze_operation (surface, op, pattern);

    assert (_cairo_svg_surface_operation_supported (surface, op, pattern));

    if (num_glyphs <= 0)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status))
	return status;

    /* FIXME it's probably possible to apply a pattern of a gradient to
     * a group of symbols, but I don't know how yet. Gradients or patterns
     * are translated by x and y properties of use element. */
    if (pattern->type != CAIRO_PATTERN_TYPE_SOLID)
	goto FALLBACK;

    _cairo_output_stream_printf (surface->xml_node, "<g style=\"");
    status = _cairo_svg_surface_emit_pattern (surface, pattern,
	                                      surface->xml_node, FALSE, NULL);
    if (unlikely (status))
	return status;

    _cairo_svg_surface_emit_operator_for_style (surface->xml_node, surface, op);

    _cairo_output_stream_printf (surface->xml_node, "\">\n");

    for (i = 0; i < num_glyphs; i++) {
	status = _cairo_scaled_font_subsets_map_glyph (document->font_subsets,
						       scaled_font, glyphs[i].index,
						       NULL, 0,
                                                       &subset_glyph);
	if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	    _cairo_output_stream_printf (surface->xml_node, "</g>\n");

	    glyphs += i;
	    num_glyphs -= i;
	    goto FALLBACK;
	}

	if (unlikely (status))
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

    status = _cairo_scaled_font_glyph_path (scaled_font,
					    (cairo_glyph_t *) glyphs,
					    num_glyphs, &path);

    if (unlikely (status)) {
	_cairo_path_fixed_fini (&path);
	return status;
    }

    status = _cairo_svg_surface_fill (abstract_surface, op, pattern,
				      &path, CAIRO_FILL_RULE_WINDING,
				      0.0, CAIRO_ANTIALIAS_SUBPIXEL,
				      clip);

    _cairo_path_fixed_fini (&path);

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
    _cairo_font_options_set_round_glyph_positions (options, CAIRO_ROUND_GLYPH_POS_OFF);
}


static const char **
_cairo_svg_surface_get_supported_mime_types (void	   *abstract_surface)
{
    return _cairo_svg_supported_mime_types;
}

static const cairo_surface_backend_t cairo_svg_surface_backend = {
	CAIRO_SURFACE_TYPE_SVG,
	_cairo_svg_surface_finish,

	_cairo_default_context_create,

	NULL, /* create_similar: handled by wrapper */
	NULL, /* create_similar_image */
	NULL, /* map to image */
	NULL, /* unmap image */

	_cairo_surface_default_source,
	NULL, /* acquire_source_image */
	NULL, /* release_source_image */
	NULL, /* snapshot */

	_cairo_svg_surface_copy_page,
	_cairo_svg_surface_show_page,

	_cairo_svg_surface_get_extents,
	_cairo_svg_surface_get_font_options,

	NULL, /* flush */
	NULL, /* mark dirty rectangle */

	_cairo_svg_surface_paint,
	_cairo_svg_surface_mask,
	_cairo_svg_surface_stroke,
	_cairo_svg_surface_fill,
	_cairo_svg_surface_fill_stroke,
	_cairo_svg_surface_show_glyphs,
	NULL, /* has_show_text_glyphs */
	NULL, /* show_text_glyphs */
	_cairo_svg_surface_get_supported_mime_types,
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

    document = _cairo_malloc (sizeof (cairo_svg_document_t));
    if (unlikely (document == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    /* The use of defs for font glyphs imposes no per-subset limit. */
    document->font_subsets = _cairo_scaled_font_subsets_create_scaled ();
    if (unlikely (document->font_subsets == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_DOCUMENT;
    }

    document->output_stream = output_stream;
    document->refcount = 1;
    document->owner = NULL;
    document->finished = FALSE;
    document->width = width;
    document->height = height;
    document->unit = CAIRO_SVG_UNIT_PT;

    document->linear_pattern_id = 0;
    document->radial_pattern_id = 0;
    document->pattern_id = 0;
    document->filter_id = 0;
    document->clip_id = 0;
    document->mask_id = 0;

    document->xml_node_defs = _cairo_memory_stream_create ();
    status = _cairo_output_stream_get_status (document->xml_node_defs);
    if (unlikely (status))
	goto CLEANUP_NODE_DEFS;

    document->xml_node_glyphs = _cairo_memory_stream_create ();
    status = _cairo_output_stream_get_status (document->xml_node_glyphs);
    if (unlikely (status))
	goto CLEANUP_NODE_GLYPHS;

    document->alpha_filter = FALSE;

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
    cairo_svg_page_t *page;
    unsigned int i;

    if (document->finished)
	return CAIRO_STATUS_SUCCESS;

    /*
     * Should we add DOCTYPE?
     *
     * Google says no.
     *
     * http://tech.groups.yahoo.com/group/svg-developers/message/48562:
     *   There's a bunch of issues, but just to pick a few:
     *   - they'll give false positives.
     *   - they'll give false negatives.
     *   - they're namespace-unaware.
     *   - they don't wildcard.
     *   So when they say OK they really haven't checked anything, when
     *   they say NOT OK they might be on crack, and like all
     *   namespace-unaware things they're a dead branch of the XML tree.
     *
     * http://jwatt.org/svg/authoring/:
     *   Unfortunately the SVG DTDs are a source of so many issues that the
     *   SVG WG has decided not to write one for the upcoming SVG 1.2
     *   standard. In fact SVG WG members are even telling people not to use
     *   a DOCTYPE declaration in SVG 1.0 and 1.1 documents.
     */

    _cairo_output_stream_printf (output,
				 "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				 "<svg xmlns=\"http://www.w3.org/2000/svg\" "
				 "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
				 "width=\"%f%s\" height=\"%f%s\" "
				 "viewBox=\"0 0 %f %f\" version=\"%s\">\n",
				 document->width, _cairo_svg_unit_strings [document->unit],
				 document->height, _cairo_svg_unit_strings [document->unit],
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
	    if (unlikely (_cairo_svg_surface_store_page (surface) == NULL)) {
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

    document->finished = TRUE;

    return status;
}

static cairo_int_status_t
_cairo_svg_surface_set_paginated_mode (void			*abstract_surface,
				       cairo_paginated_mode_t	 paginated_mode)
{
    cairo_svg_surface_t *surface = abstract_surface;

    surface->paginated_mode = paginated_mode;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_svg_surface_supports_fine_grained_fallbacks (void	*abstract_surface)
{
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;

    if (surface->document->svg_version >= CAIRO_SVG_VERSION_1_2) {
	status =  _cairo_svg_surface_analyze_operator (surface,
						       CAIRO_OPERATOR_SOURCE);
    }

    return status == CAIRO_INT_STATUS_SUCCESS;
}

static const cairo_paginated_surface_backend_t cairo_svg_surface_paginated_backend = {
    NULL /*_cairo_svg_surface_start_page*/,
    _cairo_svg_surface_set_paginated_mode,
    NULL, /* _cairo_svg_surface_set_bounding_box */
    NULL, /* _cairo_svg_surface_set_fallback_images_required */
    _cairo_svg_surface_supports_fine_grained_fallbacks,

};

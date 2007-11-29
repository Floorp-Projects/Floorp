/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
 * Copyright © 2006 Red Hat, Inc
 * Copyright © 2007 Adrian Johnson
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
 *	Carl Worth <cworth@cworth.org>
 *	Adrian Johnson <ajohnson@redneon.com>
 */

#include "cairoint.h"
#include "cairo-pdf.h"
#include "cairo-pdf-surface-private.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-paginated-private.h"
#include "cairo-path-fixed-private.h"
#include "cairo-output-stream-private.h"
#include "cairo-meta-surface-private.h"

#include <time.h>
#include <zlib.h>

/* Issues:
 *
 * - We embed an image in the stream each time it's composited.  We
 *   could add generation counters to surfaces and remember the stream
 *   ID for a particular generation for a particular surface.
 *
 * - Images of other formats than 8 bit RGBA.
 *
 * - Backend specific meta data.
 *
 * - Surface patterns.
 *
 * - Should/does cairo support drawing into a scratch surface and then
 *   using that as a fill pattern?  For this backend, that would involve
 *   using a tiling pattern (4.6.2).  How do you create such a scratch
 *   surface?  cairo_surface_create_similar() ?
 *
 * - What if you create a similar surface and does show_page and then
 *   does show_surface on another surface?
 *
 * - Add test case for RGBA images.
 *
 * - Add test case for RGBA gradients.
 *
 * - Coordinate space for create_similar() args?
 */

/*
 * Page Structure of the Generated PDF:
 *
 * Each page requiring fallbacks images contains a knockout group at
 * the top level. The first operation of the knockout group paints a
 * group containing all the supported drawing operations. Fallback
 * images (if any) are painted in the knockout group. This ensures
 * that fallback images do not composite with any content under the
 * fallback images.
 *
 * The group containing the supported operations (content_group_list
 * in the example below) does not do any drawing directly. Instead it
 * paints groups containing the drawing operations and performs
 * clipping. The reason for this is that clipping operations performed
 * in a group do not affect the parent group.
 *
 * Example PDF Page Structure:
 *
 *   Page Content
 *   ------------
 *     /knockout_group Do
 *
 *   knockout_group
 *   --------------
 *     /content_group_list Do
 *     /fallback_image_1 Do
 *     /fallback_image_2 Do
 *     ...
 *
 *   content_group_list
 *   ------------------
 *     q
 *     /content_group_1 Do
 *     /content_group_2 Do
 *     10 10 m 10 20 l 20 20 l 20 10 l h W  # clip
 *     /content_group_3 Do
 *     Q q                                  # reset clip
 *     /content_group_4 Do
 *     Q
 *
 *
 * Streams:
 *
 * This PDF surface has three types of streams:
 *  - PDF Stream
 *  - Content Stream
 *  - Group Stream
 *
 * Calling _cairo_output_stream_printf (surface->output, ...) will
 * write to the currently open stream.
 *
 * PDF Stream:
 *   A PDF Stream may be opened and closed with the following functions:
 *     _cairo_pdf_surface_open_stream ()
 *     _cairo_pdf_surface_close_stream ()
 *
 *   PDF Streams are written directly to the PDF file. They are used for
 *   fonts, images and patterns.
 *
 * Content Stream:
 *   The Content Stream is opened and closed with the following functions:
 *     _cairo_pdf_surface_start_content_stream ()
 *     _cairo_pdf_surface_stop_content_stream ()
 *
 *   The Content Stream is written to content_group_n groups (as shown
 *   in the page structure example). The Content Stream may be paused
 *   and resumed with the following functions:
 *     _cairo_pdf_surface_pause_content_stream ()
 *     _cairo_pdf_surface_resume_content_stream ()
 *
 *   When the Content Stream is paused, a PDF Stream or Group Stream
 *   may be opened. After closing the PDF Stream or Group Stream the
 *   Content Stream may be resumed.
 *
 *   The Content Stream contains the text and graphics operators. When
 *   a pattern is required the Content Stream is paused, a PDF Stream
 *   is opened, the pattern is written to a PDF Stream, the PDF Stream
 *   is closed, then the Content Stream is resumed.
 *
 *   Each group comprising the Content Stream is stored in memory
 *   until the stream is closed or the maximum group size is
 *   exceeded. This is due to the need to list all resources used in
 *   the group in the group's stream dictionary.
 *
 * Group Stream:
 *   A Group Stream may be opened and closed with the following functions:
 *     _cairo_pdf_surface_open_group ()
 *     _cairo_pdf_surface_close_group ()
 *
 *   A Group Stream is written to a separate group in the PDF file
 *   that is not part of the Content Stream. Group Streams are also
 *   stored in memory until the stream is closed due to the need to
 *   list the resources used in the group in the group's stream
 *   dictionary.
 *
 *   Group Streams are used for short sequences of graphics operations
 *   that need to be in a separate group from the Content Stream.
 */

/* The group stream length is checked after each operation. When this
 * limit is exceeded the group is written out to the pdf stream and a
 * new group is created. */
#define GROUP_STREAM_LIMIT 65536

typedef struct _cairo_pdf_object {
    long offset;
} cairo_pdf_object_t;

typedef struct _cairo_pdf_font {
    unsigned int font_id;
    unsigned int subset_id;
    cairo_pdf_resource_t subset_resource;
} cairo_pdf_font_t;

typedef struct _cairo_pdf_rgb_linear_function {
    cairo_pdf_resource_t resource;
    double               color1[3];
    double               color2[3];
} cairo_pdf_rgb_linear_function_t;

typedef struct _cairo_pdf_alpha_linear_function {
    cairo_pdf_resource_t resource;
    double               alpha1;
    double               alpha2;
} cairo_pdf_alpha_linear_function_t;

typedef enum group_element_type {
    ELEM_GROUP,
    ELEM_CLIP
} group_element_type_t;

typedef struct _cairo_pdf_group_element {
    group_element_type_t  type;
    cairo_pdf_resource_t  group;
    cairo_path_fixed_t   *clip_path;
    cairo_fill_rule_t     fill_rule;
} cairo_pdf_group_element_t;


static cairo_pdf_resource_t
_cairo_pdf_surface_new_object (cairo_pdf_surface_t *surface);

static void
_cairo_pdf_group_element_array_finish (cairo_array_t *array);

static void
_cairo_pdf_surface_clear (cairo_pdf_surface_t *surface);

static void
_cairo_pdf_group_resources_init (cairo_pdf_group_resources_t *res);

static cairo_pdf_resource_t
_cairo_pdf_surface_open_stream (cairo_pdf_surface_t	*surface,
                                cairo_bool_t             compressed,
				const char		*fmt,
				...) CAIRO_PRINTF_FORMAT(3, 4);
static cairo_status_t
_cairo_pdf_surface_close_stream (cairo_pdf_surface_t	*surface);

static cairo_status_t
_cairo_pdf_surface_write_page (cairo_pdf_surface_t *surface);

static void
_cairo_pdf_surface_write_pages (cairo_pdf_surface_t *surface);

static cairo_pdf_resource_t
_cairo_pdf_surface_write_info (cairo_pdf_surface_t *surface);

static cairo_pdf_resource_t
_cairo_pdf_surface_write_catalog (cairo_pdf_surface_t *surface);

static long
_cairo_pdf_surface_write_xref (cairo_pdf_surface_t *surface);

static cairo_status_t
_cairo_pdf_surface_emit_clip (cairo_pdf_surface_t  *surface,
			      cairo_path_fixed_t   *path,
			      cairo_fill_rule_t	    fill_rule);

static cairo_status_t
_cairo_pdf_surface_write_page (cairo_pdf_surface_t *surface);

static cairo_status_t
_cairo_pdf_surface_emit_font_subsets (cairo_pdf_surface_t *surface);

static const cairo_surface_backend_t cairo_pdf_surface_backend;
static const cairo_paginated_surface_backend_t cairo_pdf_surface_paginated_backend;

static cairo_pdf_resource_t
_cairo_pdf_surface_new_object (cairo_pdf_surface_t *surface)
{
    cairo_pdf_resource_t resource;
    cairo_status_t status;
    cairo_pdf_object_t object;

    object.offset = _cairo_output_stream_get_position (surface->output);

    status = _cairo_array_append (&surface->objects, &object);
    if (status) {
	resource.id = 0;
	return resource;
    }

    resource = surface->next_available_resource;
    surface->next_available_resource.id++;

    return resource;
}

static void
_cairo_pdf_surface_update_object (cairo_pdf_surface_t	*surface,
				  cairo_pdf_resource_t	 resource)
{
    cairo_pdf_object_t *object;

    object = _cairo_array_index (&surface->objects, resource.id - 1);
    object->offset = _cairo_output_stream_get_position (surface->output);
}

static cairo_surface_t *
_cairo_pdf_surface_create_for_stream_internal (cairo_output_stream_t	*output,
					       double			 width,
					       double			 height)
{
    cairo_pdf_surface_t *surface;
    cairo_status_t status;

    surface = malloc (sizeof (cairo_pdf_surface_t));
    if (surface == NULL) {
	/* destroy stream on behalf of caller */
	status = _cairo_output_stream_destroy (output);
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&surface->base, &cairo_pdf_surface_backend,
			 CAIRO_CONTENT_COLOR_ALPHA);

    surface->output = output;

    surface->width = width;
    surface->height = height;
    cairo_matrix_init (&surface->cairo_to_pdf, 1, 0, 0, -1, 0, height);

    _cairo_array_init (&surface->objects, sizeof (cairo_pdf_object_t));
    _cairo_array_init (&surface->pages, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&surface->rgb_linear_functions, sizeof (cairo_pdf_rgb_linear_function_t));
    _cairo_array_init (&surface->alpha_linear_functions, sizeof (cairo_pdf_alpha_linear_function_t));
    _cairo_array_init (&surface->fonts, sizeof (cairo_pdf_font_t));
    _cairo_array_init (&surface->knockout_group, sizeof (cairo_pdf_group_element_t));
    _cairo_array_init (&surface->content_group, sizeof (cairo_pdf_group_element_t));

    _cairo_pdf_group_resources_init (&surface->group_stream.resources);
    _cairo_pdf_group_resources_init (&surface->content_stream.resources);

    surface->font_subsets = _cairo_scaled_font_subsets_create_composite ();
    if (! surface->font_subsets) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	goto BAIL0;
    }

    surface->next_available_resource.id = 1;
    surface->pages_resource = _cairo_pdf_surface_new_object (surface);
    if (surface->pages_resource.id == 0)
        goto BAIL1;

    surface->compress_content = TRUE;
    surface->pdf_stream.active = FALSE;
    surface->pdf_stream.old_output = NULL;
    surface->content_stream.active = FALSE;
    surface->content_stream.stream = NULL;
    surface->content_stream.mem_stream = NULL;
    surface->group_stream.active = FALSE;
    surface->group_stream.stream = NULL;
    surface->group_stream.mem_stream = NULL;

    surface->current_group = NULL;
    surface->current_resources = NULL;

    surface->paginated_mode = CAIRO_PAGINATED_MODE_ANALYZE;

    surface->force_fallbacks = FALSE;

    /* Document header */
    _cairo_output_stream_printf (surface->output,
				 "%%PDF-1.4\r\n");
    _cairo_output_stream_printf (surface->output,
				 "%%%c%c%c%c\r\n", 181, 237, 174, 251);

    surface->paginated_surface =  _cairo_paginated_surface_create (
	                                  &surface->base,
					  CAIRO_CONTENT_COLOR_ALPHA,
					  width, height,
					  &cairo_pdf_surface_paginated_backend);
    if (surface->paginated_surface->status == CAIRO_STATUS_SUCCESS)
	return surface->paginated_surface;

BAIL1:
    _cairo_scaled_font_subsets_destroy (surface->font_subsets);
BAIL0:
    _cairo_array_fini (&surface->objects);
    free (surface);

    /* destroy stream on behalf of caller */
    status = _cairo_output_stream_destroy (output);

    return (cairo_surface_t*) &_cairo_surface_nil;
}

/**
 * cairo_pdf_surface_create_for_stream:
 * @write_func: a #cairo_write_func_t to accept the output data
 * @closure: the closure argument for @write_func
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 *
 * Creates a PDF surface of the specified size in points to be written
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
cairo_pdf_surface_create_for_stream (cairo_write_func_t		 write_func,
				     void			*closure,
				     double			 width_in_points,
				     double			 height_in_points)
{
    cairo_status_t status;
    cairo_output_stream_t *output;

    output = _cairo_output_stream_create (write_func, NULL, closure);
    status = _cairo_output_stream_get_status (output);
    if (status)
	return (cairo_surface_t*) &_cairo_surface_nil;

    return _cairo_pdf_surface_create_for_stream_internal (output,
							  width_in_points,
							  height_in_points);
}

/**
 * cairo_pdf_surface_create:
 * @filename: a filename for the PDF output (must be writable)
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 *
 * Creates a PDF surface of the specified size in points to be written
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
cairo_pdf_surface_create (const char		*filename,
			  double		 width_in_points,
			  double		 height_in_points)
{
    cairo_status_t status;
    cairo_output_stream_t *output;

    output = _cairo_output_stream_create_for_filename (filename);
    status = _cairo_output_stream_get_status (output);
    if (status)
	return (status == CAIRO_STATUS_WRITE_ERROR) ?
		(cairo_surface_t*) &_cairo_surface_nil_write_error :
		(cairo_surface_t*) &_cairo_surface_nil;

    return _cairo_pdf_surface_create_for_stream_internal (output,
							  width_in_points,
							  height_in_points);
}

static cairo_bool_t
_cairo_surface_is_pdf (cairo_surface_t *surface)
{
    return surface->backend == &cairo_pdf_surface_backend;
}

/* If the abstract_surface is a paginated surface, and that paginated
 * surface's target is a pdf_surface, then set pdf_surface to that
 * target. Otherwise return CAIRO_STATUS_SURFACE_TYPE_MISMATCH.
 */
static cairo_status_t
_extract_pdf_surface (cairo_surface_t		 *surface,
		      cairo_pdf_surface_t	**pdf_surface)
{
    cairo_surface_t *target;

    if (! _cairo_surface_is_paginated (surface))
	return _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);

    target = _cairo_paginated_surface_get_target (surface);

    if (! _cairo_surface_is_pdf (target))
	return _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);

    *pdf_surface = (cairo_pdf_surface_t *) target;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_pdf_surface_set_size:
 * @surface: a PDF cairo_surface_t
 * @width_in_points: new surface width, in points (1 point == 1/72.0 inch)
 * @height_in_points: new surface height, in points (1 point == 1/72.0 inch)
 *
 * Changes the size of a PDF surface for the current (and
 * subsequent) pages.
 *
 * This function should only be called before any drawing operations
 * have been performed on the current page. The simplest way to do
 * this is to call this function immediately after creating the
 * surface or immediately after completing a page with either
 * cairo_show_page() or cairo_copy_page().
 *
 * Since: 1.2
 **/
void
cairo_pdf_surface_set_size (cairo_surface_t	*surface,
			    double		 width_in_points,
			    double		 height_in_points)
{
    cairo_pdf_surface_t *pdf_surface = NULL; /* hide compiler warning */
    cairo_status_t status;

    status = _extract_pdf_surface (surface, &pdf_surface);
    if (status) {
	status = _cairo_surface_set_error (surface, status);
	return;
    }

    pdf_surface->width = width_in_points;
    pdf_surface->height = height_in_points;
    cairo_matrix_init (&pdf_surface->cairo_to_pdf, 1, 0, 0, -1, 0, height_in_points);
    status = _cairo_paginated_surface_set_size (pdf_surface->paginated_surface,
						width_in_points,
						height_in_points);
    if (status)
	status = _cairo_surface_set_error (surface, status);
}

static void
_cairo_pdf_surface_clear (cairo_pdf_surface_t *surface)
{
    _cairo_pdf_group_element_array_finish (&surface->content_group);
    _cairo_pdf_group_element_array_finish (&surface->knockout_group);
    _cairo_array_truncate (&surface->content_group, 0);
    _cairo_array_truncate (&surface->knockout_group, 0);
}

static void
_cairo_pdf_group_resources_init (cairo_pdf_group_resources_t *res)
{
    _cairo_array_init (&res->alphas, sizeof (double));
    _cairo_array_init (&res->smasks, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&res->patterns, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&res->xobjects, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&res->fonts, sizeof (cairo_pdf_font_t));
}

static void
_cairo_pdf_group_resources_fini (cairo_pdf_group_resources_t *res)
{
    _cairo_array_fini (&res->alphas);
    _cairo_array_fini (&res->smasks);
    _cairo_array_fini (&res->patterns);
    _cairo_array_fini (&res->xobjects);
    _cairo_array_fini (&res->fonts);
}

static void
_cairo_pdf_group_resources_clear (cairo_pdf_group_resources_t *res)
{
    _cairo_array_truncate (&res->alphas, 0);
    _cairo_array_truncate (&res->smasks, 0);
    _cairo_array_truncate (&res->patterns, 0);
    _cairo_array_truncate (&res->xobjects, 0);
    _cairo_array_truncate (&res->fonts, 0);
}

static cairo_status_t
_cairo_pdf_surface_add_alpha (cairo_pdf_surface_t *surface,
			      double               alpha,
			      int                 *index)
{
    int num_alphas, i;
    double other;
    cairo_status_t status;
    cairo_pdf_group_resources_t *res = surface->current_resources;

    num_alphas = _cairo_array_num_elements (&res->alphas);
    for (i = 0; i < num_alphas; i++) {
	_cairo_array_copy_element (&res->alphas, i, &other);
	if (alpha == other) {
	    *index = i;
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    status = _cairo_array_append (&res->alphas, &alpha);
    if (status)
	return status;

    *index = _cairo_array_num_elements (&res->alphas) - 1;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_surface_add_smask (cairo_pdf_surface_t  *surface,
			      cairo_pdf_resource_t  smask)
{
    return _cairo_array_append (&surface->current_resources->smasks, &smask);
}

static cairo_status_t
_cairo_pdf_surface_add_pattern (cairo_pdf_surface_t  *surface,
				cairo_pdf_resource_t  pattern)
{
    return _cairo_array_append (&surface->current_resources->patterns, &pattern);
}

static cairo_status_t
_cairo_pdf_surface_add_xobject (cairo_pdf_surface_t  *surface,
				cairo_pdf_resource_t  xobject)
{
    return _cairo_array_append (&surface->current_resources->xobjects, &xobject);
}

static cairo_status_t
_cairo_pdf_surface_add_font (cairo_pdf_surface_t *surface,
			     unsigned int         font_id,
			     unsigned int         subset_id)
{
    cairo_pdf_font_t font;
    int num_fonts, i;
    cairo_status_t status;
    cairo_pdf_group_resources_t *res = surface->current_resources;

    num_fonts = _cairo_array_num_elements (&res->fonts);
    for (i = 0; i < num_fonts; i++) {
	_cairo_array_copy_element (&res->fonts, i, &font);
	if (font.font_id == font_id &&
	    font.subset_id == subset_id)
	    return CAIRO_STATUS_SUCCESS;
    }

    num_fonts = _cairo_array_num_elements (&surface->fonts);
    for (i = 0; i < num_fonts; i++) {
	_cairo_array_copy_element (&surface->fonts, i, &font);
	if (font.font_id == font_id &&
	    font.subset_id == subset_id)
	    return _cairo_array_append (&res->fonts, &font);
    }

    font.font_id = font_id;
    font.subset_id = subset_id;
    font.subset_resource = _cairo_pdf_surface_new_object (surface);
    if (font.subset_resource.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _cairo_array_append (&surface->fonts, &font);
    if (status)
	return status;

    return _cairo_array_append (&res->fonts, &font);
}

static cairo_pdf_resource_t
_cairo_pdf_surface_get_font_resource (cairo_pdf_surface_t *surface,
				      unsigned int         font_id,
				      unsigned int         subset_id)
{
    cairo_pdf_font_t font;
    int num_fonts, i;
    cairo_pdf_resource_t resource;

    num_fonts = _cairo_array_num_elements (&surface->fonts);
    for (i = 0; i < num_fonts; i++) {
	_cairo_array_copy_element (&surface->fonts, i, &font);
	if (font.font_id == font_id && font.subset_id == subset_id)
	    return font.subset_resource;
    }
    resource.id = 0;

    return resource;
}

static void
_cairo_pdf_surface_emit_group_resources (cairo_pdf_surface_t         *surface,
					 cairo_pdf_group_resources_t *res)
{
    int num_alphas, num_smasks, num_resources, i;
    double alpha;
    cairo_pdf_resource_t *smask, *pattern, *xobject;
    cairo_pdf_font_t *font;

    _cairo_output_stream_printf (surface->output, "   /Resources <<\r\n");

    num_alphas = _cairo_array_num_elements (&res->alphas);
    num_smasks = _cairo_array_num_elements (&res->smasks);
    if (num_alphas > 0 || num_smasks > 0) {
	_cairo_output_stream_printf (surface->output,
				     "      /ExtGState <<\r\n");

	for (i = 0; i < num_alphas; i++) {
	    _cairo_array_copy_element (&res->alphas, i, &alpha);
	    _cairo_output_stream_printf (surface->output,
					 "         /a%d << /CA %f /ca %f >>\r\n",
					 i, alpha, alpha);
	}

	for (i = 0; i < num_smasks; i++) {
	    smask = _cairo_array_index (&res->smasks, i);
	    _cairo_output_stream_printf (surface->output,
					 "         /s%d %d 0 R\r\n",
					 smask->id, smask->id);
	}

	_cairo_output_stream_printf (surface->output,
				     "      >>\r\n");
    }

    num_resources = _cairo_array_num_elements (&res->patterns);
    if (num_resources > 0) {
	_cairo_output_stream_printf (surface->output,
				     "      /Pattern <<");
	for (i = 0; i < num_resources; i++) {
	    pattern = _cairo_array_index (&res->patterns, i);
	    _cairo_output_stream_printf (surface->output,
					 " /p%d %d 0 R",
					 pattern->id, pattern->id);
	}

	_cairo_output_stream_printf (surface->output,
				     " >>\r\n");
    }

    num_resources = _cairo_array_num_elements (&res->xobjects);
    if (num_resources > 0) {
	_cairo_output_stream_printf (surface->output,
				     "      /XObject <<");

	for (i = 0; i < num_resources; i++) {
	    xobject = _cairo_array_index (&res->xobjects, i);
	    _cairo_output_stream_printf (surface->output,
					 " /x%d %d 0 R",
					 xobject->id, xobject->id);
	}

	_cairo_output_stream_printf (surface->output,
				     " >>\r\n");
    }

    num_resources = _cairo_array_num_elements (&res->fonts);
    if (num_resources > 0) {
	_cairo_output_stream_printf (surface->output,"      /Font <<\r\n");
	for (i = 0; i < num_resources; i++) {
	    font = _cairo_array_index (&res->fonts, i);
	    _cairo_output_stream_printf (surface->output,
					 "         /f-%d-%d %d 0 R\r\n",
					 font->font_id,
					 font->subset_id,
					 font->subset_resource.id);
	}
	_cairo_output_stream_printf (surface->output, "      >>\r\n");
    }

    _cairo_output_stream_printf (surface->output,
				 "   >>\r\n");
}

static cairo_pdf_resource_t
_cairo_pdf_surface_open_stream (cairo_pdf_surface_t	*surface,
				cairo_bool_t             compressed,
				const char		*fmt,
				...)
{
    va_list ap;
    cairo_pdf_resource_t self, length;
    cairo_output_stream_t *output = NULL;

    self = _cairo_pdf_surface_new_object (surface);
    if (self.id == 0)
	return self;

    length = _cairo_pdf_surface_new_object (surface);
    if (length.id == 0)
	return length;

    if (compressed) {
	output = _cairo_deflate_stream_create (surface->output);
	if (_cairo_output_stream_get_status (output)) {
	    self.id = 0;
	    return self;
	}
    }

    surface->pdf_stream.active = TRUE;
    surface->pdf_stream.self = self;
    surface->pdf_stream.length = length;
    surface->pdf_stream.compressed = compressed;

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Length %d 0 R\r\n",
				 surface->pdf_stream.self.id,
				 surface->pdf_stream.length.id);
    if (compressed)
	_cairo_output_stream_printf (surface->output,
				     "   /Filter /FlateDecode\r\n");

    if (fmt != NULL) {
	va_start (ap, fmt);
	_cairo_output_stream_vprintf (surface->output, fmt, ap);
	va_end (ap);
    }

    _cairo_output_stream_printf (surface->output,
				 ">>\r\n"
				 "stream\r\n");

    surface->pdf_stream.start_offset = _cairo_output_stream_get_position (surface->output);

    if (compressed) {
	assert (surface->pdf_stream.old_output == NULL);
        surface->pdf_stream.old_output = surface->output;
        surface->output = output;
    }

    return surface->pdf_stream.self;
}

static cairo_status_t
_cairo_pdf_surface_close_stream (cairo_pdf_surface_t *surface)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    long length;

    if (! surface->pdf_stream.active)
	return CAIRO_STATUS_SUCCESS;

    if (surface->pdf_stream.compressed) {
	status = _cairo_output_stream_destroy (surface->output);
	surface->output = surface->pdf_stream.old_output;
	surface->pdf_stream.old_output = NULL;
	_cairo_output_stream_printf (surface->output,
				     "\r\n");
    }

    length = _cairo_output_stream_get_position (surface->output) -
	surface->pdf_stream.start_offset;
    _cairo_output_stream_printf (surface->output,
				 "endstream\r\n"
				 "endobj\r\n");

    _cairo_pdf_surface_update_object (surface,
				      surface->pdf_stream.length);
    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "   %ld\r\n"
				 "endobj\r\n",
				 surface->pdf_stream.length.id,
				 length);

    surface->pdf_stream.active = FALSE;

    return status;
}

static cairo_pdf_resource_t
_cairo_pdf_surface_write_memory_stream (cairo_pdf_surface_t         *surface,
					cairo_output_stream_t       *mem_stream,
					cairo_pdf_group_resources_t *resources,
					cairo_bool_t                 is_knockout_group)
{
    cairo_pdf_resource_t group;

    group = _cairo_pdf_surface_new_object (surface);
    if (group.id == 0)
	return group;

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /XObject\r\n"
				 "   /Length %d\r\n",
				 group.id,
				 _cairo_memory_stream_length (mem_stream));

    if (surface->compress_content) {
	_cairo_output_stream_printf (surface->output,
				     "   /Filter /FlateDecode\r\n");
    }

    _cairo_output_stream_printf (surface->output,
				 "   /Subtype /Form\r\n"
				 "   /BBox [ 0 0 %f %f ]\r\n"
				 "   /Group <<\r\n"
				 "      /Type /Group\r\n"
				 "      /S /Transparency\r\n"
				 "      /CS /DeviceRGB\r\n",
				 surface->width,
				 surface->height);

    if (is_knockout_group)
	_cairo_output_stream_printf (surface->output,
				     "      /K true\r\n");

    _cairo_output_stream_printf (surface->output,
				 "   >>\r\n");
    _cairo_pdf_surface_emit_group_resources (surface, resources);
    _cairo_output_stream_printf (surface->output,
				 ">>\r\n"
				 "stream\r\n");
    _cairo_memory_stream_copy (mem_stream, surface->output);
    _cairo_output_stream_printf (surface->output,
				 "endstream\r\n"
				 "endobj\r\n");

    return group;
}

static cairo_status_t
_cairo_pdf_surface_open_group (cairo_pdf_surface_t *surface)
{
    cairo_status_t status;

    assert (surface->pdf_stream.active == FALSE);
    assert (surface->content_stream.active == FALSE);
    assert (surface->group_stream.active == FALSE);

    surface->group_stream.active = TRUE;

    surface->group_stream.mem_stream = _cairo_memory_stream_create ();

    if (surface->compress_content) {
	surface->group_stream.stream =
	    _cairo_deflate_stream_create (surface->group_stream.mem_stream);
    } else {
	surface->group_stream.stream = surface->group_stream.mem_stream;
    }
    status = _cairo_output_stream_get_status (surface->group_stream.stream);
    if (status)
	return status;

    surface->group_stream.old_output = surface->output;
    surface->output = surface->group_stream.stream;
    _cairo_pdf_group_resources_clear (&surface->group_stream.resources);
    surface->current_resources = &surface->group_stream.resources;
    surface->group_stream.is_knockout = FALSE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_surface_open_knockout_group (cairo_pdf_surface_t  *surface,
					cairo_pdf_resource_t *first_object)
{
    cairo_status_t status;

    status = _cairo_pdf_surface_open_group (surface);
    if (status)
	return status;

    surface->group_stream.is_knockout = TRUE;
    surface->group_stream.first_object = *first_object;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_surface_close_group (cairo_pdf_surface_t *surface,
				cairo_pdf_resource_t *group)
{
    cairo_status_t status;

    assert (surface->pdf_stream.active == FALSE);
    assert (surface->group_stream.active == TRUE);

    if (surface->compress_content) {
	status = _cairo_output_stream_destroy (surface->group_stream.stream);
	if (status)
	    return status;

	_cairo_output_stream_printf (surface->group_stream.mem_stream,
				     "\r\n");
    }
    surface->output = surface->group_stream.old_output;
    surface->group_stream.active = FALSE;
    *group = _cairo_pdf_surface_write_memory_stream (surface,
						     surface->group_stream.mem_stream,
						     &surface->group_stream.resources,
						     surface->group_stream.is_knockout);
    if (group->id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    return _cairo_output_stream_close (surface->group_stream.mem_stream);
}

static cairo_status_t
_cairo_pdf_surface_write_group_list (cairo_pdf_surface_t  *surface,
				     cairo_array_t        *group_list)
{
    int i, len;
    cairo_pdf_group_element_t *elem;
    cairo_status_t status;

    _cairo_output_stream_printf (surface->output, "q\r\n");
    if (surface->group_stream.is_knockout) {
	_cairo_output_stream_printf (surface->output,
				     "/x%d Do\r\n",
				     surface->group_stream.first_object.id);
	status = _cairo_pdf_surface_add_xobject (surface, surface->group_stream.first_object);
	if (status)
	    return status;
    }
    len = _cairo_array_num_elements (group_list);
    for (i = 0; i < len; i++) {
	elem = _cairo_array_index (group_list, i);
	if (elem->type == ELEM_GROUP) {
	    _cairo_output_stream_printf (surface->output,
					 "/x%d Do\r\n",
					 elem->group.id);
	    status = _cairo_pdf_surface_add_xobject (surface, elem->group);
	    if (status)
		return status;
	} else if (elem->type == ELEM_CLIP) {
	    status = _cairo_pdf_surface_emit_clip (surface, elem->clip_path, elem->fill_rule);
	    if (status)
		return status;
	}
    }
    _cairo_output_stream_printf (surface->output, "Q\r\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_surface_start_content_stream (cairo_pdf_surface_t *surface)
{
    cairo_status_t status;

    if (surface->content_stream.active)
        return CAIRO_STATUS_SUCCESS;

    assert (surface->pdf_stream.active == FALSE);
    assert (surface->content_stream.active == FALSE);
    assert (surface->group_stream.active == FALSE);

    surface->content_stream.active = TRUE;
    surface->content_stream.mem_stream = _cairo_memory_stream_create ();

    if (surface->compress_content) {
	surface->content_stream.stream =
	    _cairo_deflate_stream_create (surface->content_stream.mem_stream);
    } else {
	surface->content_stream.stream = surface->content_stream.mem_stream;
    }
    status = _cairo_output_stream_get_status (surface->content_stream.stream);
    if (status)
	return status;

    surface->content_stream.old_output = surface->output;
    surface->output = surface->content_stream.stream;
    _cairo_pdf_group_resources_clear (&surface->content_stream.resources);
    surface->current_resources = &surface->content_stream.resources;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_surface_add_group_to_content_stream (cairo_pdf_surface_t   *surface,
                                                cairo_pdf_resource_t   group)
{
    cairo_pdf_group_element_t elem;

    memset (&elem, 0, sizeof elem);
    elem.type = ELEM_GROUP;
    elem.group = group;

    return _cairo_array_append (surface->current_group, &elem);
}

static void
_cairo_pdf_surface_pause_content_stream (cairo_pdf_surface_t *surface)
{
    assert (surface->pdf_stream.active == FALSE);

    if (surface->content_stream.active == FALSE)
	return;

    surface->output = surface->content_stream.old_output;
    surface->content_stream.active = FALSE;
}

static void
_cairo_pdf_surface_resume_content_stream (cairo_pdf_surface_t *surface)
{
    assert (surface->pdf_stream.active == FALSE);

    if (surface->content_stream.active == TRUE)
	return;

    surface->output = surface->content_stream.stream;
    surface->current_resources = &surface->content_stream.resources;
    surface->content_stream.active = TRUE;
}

static cairo_status_t
_cairo_pdf_surface_stop_content_stream (cairo_pdf_surface_t *surface)
{
    cairo_pdf_resource_t group;
    cairo_status_t status;

    assert (surface->pdf_stream.active == FALSE);
    assert (surface->content_stream.active == TRUE);

    if (surface->compress_content) {
	status = _cairo_output_stream_destroy (surface->content_stream.stream);
	if (status)
	    return status;

	_cairo_output_stream_printf (surface->content_stream.mem_stream,
				     "\r\n");
    }
    surface->output = surface->content_stream.old_output;
    surface->content_stream.active = FALSE;
    if (_cairo_memory_stream_length (surface->content_stream.mem_stream) > 0) {
	group = _cairo_pdf_surface_write_memory_stream (surface,
							surface->content_stream.mem_stream,
							&surface->content_stream.resources,
							FALSE);
	if (group.id == 0)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	status = _cairo_pdf_surface_add_group_to_content_stream (surface, group);
	if (status)
	    return status;
    }
    surface->content_stream.active = FALSE;

    return _cairo_output_stream_close (surface->content_stream.mem_stream);
}

static cairo_status_t
_cairo_pdf_surface_check_content_stream_size (cairo_pdf_surface_t *surface)
{
    cairo_status_t status;

    if (surface->content_stream.active == FALSE)
	return CAIRO_STATUS_SUCCESS;

    if (_cairo_memory_stream_length (surface->content_stream.mem_stream) > GROUP_STREAM_LIMIT) {
	status = _cairo_pdf_surface_stop_content_stream (surface);
	if (status)
	    return status;

	status = _cairo_pdf_surface_start_content_stream (surface);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_pdf_group_element_array_finish (cairo_array_t *array)
{
    int i, len;
    cairo_pdf_group_element_t *elem;

    len = _cairo_array_num_elements (array);
    for (i = 0; i < len; i++) {
	elem = _cairo_array_index (array, i);
	if (elem->type == ELEM_CLIP && elem->clip_path)
	    _cairo_path_fixed_destroy (elem->clip_path);
    }
}

static cairo_surface_t *
_cairo_pdf_surface_create_similar (void			*abstract_surface,
				   cairo_content_t	 content,
				   int			 width,
				   int			 height)
{
    return _cairo_meta_surface_create (content, width, height);
}

static cairo_status_t
_cairo_pdf_surface_finish (void *abstract_surface)
{
    cairo_status_t status, status2;
    cairo_pdf_surface_t *surface = abstract_surface;
    long offset;
    cairo_pdf_resource_t info, catalog;

    status = _cairo_pdf_surface_emit_font_subsets (surface);

    _cairo_pdf_surface_write_pages (surface);

    info = _cairo_pdf_surface_write_info (surface);
    if (info.id == 0 && status == CAIRO_STATUS_SUCCESS)
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);

    catalog = _cairo_pdf_surface_write_catalog (surface);
    if (catalog.id == 0 && status == CAIRO_STATUS_SUCCESS)
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);

    offset = _cairo_pdf_surface_write_xref (surface);

    _cairo_output_stream_printf (surface->output,
				 "trailer\r\n"
				 "<< /Size %d\r\n"
				 "   /Root %d 0 R\r\n"
				 "   /Info %d 0 R\r\n"
				 ">>\r\n",
				 surface->next_available_resource.id,
				 catalog.id,
				 info.id);

    _cairo_output_stream_printf (surface->output,
				 "startxref\r\n"
				 "%ld\r\n"
				 "%%%%EOF\r\n",
				 offset);

    status2 = _cairo_output_stream_destroy (surface->output);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;

    _cairo_array_fini (&surface->objects);
    _cairo_array_fini (&surface->pages);
    _cairo_array_fini (&surface->rgb_linear_functions);
    _cairo_array_fini (&surface->alpha_linear_functions);
    _cairo_array_fini (&surface->fonts);

    _cairo_pdf_group_resources_fini (&surface->group_stream.resources);
    _cairo_pdf_group_resources_fini (&surface->content_stream.resources);

    _cairo_pdf_group_element_array_finish (&surface->knockout_group);
    _cairo_array_fini (&surface->knockout_group);

    _cairo_pdf_group_element_array_finish (&surface->content_group);
    _cairo_array_fini (&surface->content_group);

    if (surface->font_subsets) {
	_cairo_scaled_font_subsets_destroy (surface->font_subsets);
	surface->font_subsets = NULL;
    }

    return status;
}

static cairo_int_status_t
_cairo_pdf_surface_start_page (void *abstract_surface)
{
    cairo_status_t status;
    cairo_pdf_surface_t *surface = abstract_surface;

    surface->current_group = &surface->content_group;
    status = _cairo_pdf_surface_start_content_stream (surface);
    if (status)
	return status;


    return CAIRO_STATUS_SUCCESS;
}

static void *
compress_dup (const void *data, unsigned long data_size,
	      unsigned long *compressed_size)
{
    void *compressed;

    /* Bound calculation taken from zlib. */
    *compressed_size = data_size + (data_size >> 12) + (data_size >> 14) + 11;
    compressed = malloc (*compressed_size);
    if (compressed == NULL) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    if (compress (compressed, compressed_size, data, data_size) != Z_OK) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	free (compressed);
	compressed = NULL;
    }

    return compressed;
}

/* Emit alpha channel from the image into the given data, providing
 * an id that can be used to reference the resulting SMask object.
 *
 * In the case that the alpha channel happens to be all opaque, then
 * no SMask object will be emitted and *id_ret will be set to 0.
 */
static cairo_status_t
_cairo_pdf_surface_emit_smask (cairo_pdf_surface_t	*surface,
			       cairo_image_surface_t	*image,
			       cairo_pdf_resource_t	*stream_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    char *alpha, *alpha_compressed;
    unsigned long alpha_size, alpha_compressed_size;
    uint32_t *pixel32;
    uint8_t *pixel8;
    int i, x, y;
    cairo_bool_t opaque;
    uint8_t a;
    int src_bit, dst_bit;

    /* This is the only image format we support, which simplfies things. */
    assert (image->format == CAIRO_FORMAT_ARGB32 ||
	    image->format == CAIRO_FORMAT_A8 ||
	    image->format == CAIRO_FORMAT_A1 );

    if (image->format == CAIRO_FORMAT_A1)
	alpha_size = (image->height * image->width + 7)/8;
    else
	alpha_size = image->height * image->width;
    alpha = malloc (alpha_size);
    if (alpha == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP;
    }

    opaque = TRUE;
    i = 0;
    src_bit = 0;
    dst_bit = 7;
    for (y = 0; y < image->height; y++) {
	if (image->format == CAIRO_FORMAT_ARGB32) {
	    pixel32 = (uint32_t *) (image->data + y * image->stride);

	    for (x = 0; x < image->width; x++, pixel32++) {
		a = (*pixel32 & 0xff000000) >> 24;
		alpha[i++] = a;
		if (a != 0xff)
		    opaque = FALSE;
	    }
	} else if (image->format == CAIRO_FORMAT_A8){
	    pixel8 = (uint8_t *) (image->data + y * image->stride);

	    for (x = 0; x < image->width; x++, pixel8++) {
		a = *pixel8;
		alpha[i++] = a;
		if (a != 0xff)
		    opaque = FALSE;
	    }
	} else { /* image->format == CAIRO_FORMAT_A1 */
	    pixel8 = (uint8_t *) (image->data + y * image->stride);

	    for (x = 0; x < image->width; x++, pixel8++) {
		if (dst_bit == 7)
		    alpha[i] = 0;
		if ((*pixel8 >> src_bit) & 1) {
			opaque = FALSE;
			alpha[i] |= (1 << dst_bit);
		}
		if (++src_bit > 7) {
		    src_bit = 0;
		    pixel8++;
		}
		if (--dst_bit < 0) {
		    dst_bit = 7;
		    i++;
		}
	    }
	}
    }

    /* Bail out without emitting smask if it's all opaque. */
    if (opaque) {
	stream_ret->id = 0;
	goto CLEANUP_ALPHA;
    }

    alpha_compressed = compress_dup (alpha, alpha_size, &alpha_compressed_size);
    if (alpha_compressed == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_ALPHA;
    }

    *stream_ret = _cairo_pdf_surface_open_stream (surface,
                                                  FALSE,
						  "   /Type /XObject\r\n"
						  "   /Subtype /Image\r\n"
						  "   /Width %d\r\n"
						  "   /Height %d\r\n"
						  "   /ColorSpace /DeviceGray\r\n"
						  "   /BitsPerComponent %d\r\n"
						  "   /Filter /FlateDecode\r\n",
						  image->width, image->height,
						  image->format == CAIRO_FORMAT_A1 ? 1 : 8);
    if (stream_ret->id == 0) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_ALPHA_COMPRESSED;
    }

    _cairo_output_stream_write (surface->output, alpha_compressed, alpha_compressed_size);
    _cairo_output_stream_printf (surface->output, "\r\n");
    status = _cairo_pdf_surface_close_stream (surface);

 CLEANUP_ALPHA_COMPRESSED:
    free (alpha_compressed);
 CLEANUP_ALPHA:
    free (alpha);
 CLEANUP:
    return status;
}

/* Emit image data into the given surface, providing a resource that
 * can be used to reference the data in image_ret. */
static cairo_status_t
_cairo_pdf_surface_emit_image (cairo_pdf_surface_t   *surface,
                               cairo_image_surface_t *image,
                               cairo_pdf_resource_t  *image_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    char *rgb, *compressed;
    unsigned long rgb_size, compressed_size;
    uint32_t *pixel;
    int i, x, y;
    cairo_pdf_resource_t smask = {0}; /* squelch bogus compiler warning */
    cairo_bool_t need_smask;

    /* XXX: Need to rewrite this as a pdf_surface function with
     * pause/resume of content_stream, (currently the only caller does
     * the pause/resume already, but that is expected to change in the
     * future). */

    /* These are the only image formats we currently support, (which
     * makes things a lot simpler here). This is enforced through
     * _cairo_pdf_surface_analyze_operation which only accept source surfaces of
     * CONTENT_COLOR or CONTENT_COLOR_ALPHA.
     */
    assert (image->format == CAIRO_FORMAT_RGB24 ||
	    image->format == CAIRO_FORMAT_ARGB32 ||
	    image->format == CAIRO_FORMAT_A8 ||
	    image->format == CAIRO_FORMAT_A1);

    rgb_size = image->height * image->width * 3;
    rgb = malloc (rgb_size);
    if (rgb == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP;
    }

    i = 0;
    for (y = 0; y < image->height; y++) {
	pixel = (uint32_t *) (image->data + y * image->stride);

	for (x = 0; x < image->width; x++, pixel++) {
	    /* XXX: We're un-premultiplying alpha here. My reading of the PDF
	     * specification suggests that we should be able to avoid having
	     * to do this by filling in the SMask's Matte dictionary
	     * appropriately, but my attempts to do that so far have
	     * failed. */
	    if (image->format == CAIRO_FORMAT_ARGB32) {
		uint8_t a;
		a = (*pixel & 0xff000000) >> 24;
		if (a == 0) {
		    rgb[i++] = 0;
		    rgb[i++] = 0;
		    rgb[i++] = 0;
		} else {
		    rgb[i++] = (((*pixel & 0xff0000) >> 16) * 255 + a / 2) / a;
		    rgb[i++] = (((*pixel & 0x00ff00) >>  8) * 255 + a / 2) / a;
		    rgb[i++] = (((*pixel & 0x0000ff) >>  0) * 255 + a / 2) / a;
		}
	    } else if (image->format == CAIRO_FORMAT_RGB24) {
		rgb[i++] = (*pixel & 0x00ff0000) >> 16;
		rgb[i++] = (*pixel & 0x0000ff00) >>  8;
		rgb[i++] = (*pixel & 0x000000ff) >>  0;
	    } else {
		rgb[i++] = 0;
		rgb[i++] = 0;
		rgb[i++] = 0;
	    }
	}
    }

    compressed = compress_dup (rgb, rgb_size, &compressed_size);
    if (compressed == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_RGB;
    }

    need_smask = FALSE;
    if (image->format == CAIRO_FORMAT_ARGB32 ||
	image->format == CAIRO_FORMAT_A8 ||
	image->format == CAIRO_FORMAT_A1) {
	status = _cairo_pdf_surface_emit_smask (surface, image, &smask);
	if (status)
	    goto CLEANUP_COMPRESSED;

	if (smask.id)
	    need_smask = TRUE;
    }

#define IMAGE_DICTIONARY	"   /Type /XObject\r\n"		\
				"   /Subtype /Image\r\n"	\
				"   /Width %d\r\n"		\
				"   /Height %d\r\n"		\
				"   /ColorSpace /DeviceRGB\r\n"	\
				"   /BitsPerComponent 8\r\n"	\
				"   /Filter /FlateDecode\r\n"

    if (need_smask)
	*image_ret = _cairo_pdf_surface_open_stream (surface,
                                                     FALSE,
						     IMAGE_DICTIONARY
						     "   /SMask %d 0 R\r\n",
						     image->width, image->height,
						     smask.id);
    else
	*image_ret = _cairo_pdf_surface_open_stream (surface,
                                                     FALSE,
						     IMAGE_DICTIONARY,
						     image->width, image->height);
    if (image_ret->id == 0){
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_COMPRESSED;
    }

#undef IMAGE_DICTIONARY

    _cairo_output_stream_write (surface->output, compressed, compressed_size);
    _cairo_output_stream_printf (surface->output, "\r\n");
    status = _cairo_pdf_surface_close_stream (surface);

CLEANUP_COMPRESSED:
    free (compressed);
CLEANUP_RGB:
    free (rgb);
CLEANUP:
    return status;
}

static cairo_status_t
_cairo_pdf_surface_emit_image_surface (cairo_pdf_surface_t     *surface,
				       cairo_surface_pattern_t *pattern,
				       cairo_pdf_resource_t    *resource,
				       int                     *width,
				       int                     *height)
{
    cairo_surface_t *pat_surface;
    cairo_surface_attributes_t pat_attr;
    cairo_image_surface_t *image;
    void *image_extra;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    status = _cairo_pattern_acquire_surface ((cairo_pattern_t *)pattern,
					     (cairo_surface_t *)surface,
					     0, 0, -1, -1,
					     &pat_surface, &pat_attr);
    if (status)
	return status;

    status = _cairo_surface_acquire_source_image (pat_surface, &image, &image_extra);
    if (status)
	goto BAIL2;

    status = _cairo_pdf_surface_emit_image (surface, image, resource);
    if (status)
	goto BAIL;

    *width = image->width;
    *height = image->height;

BAIL:
    _cairo_surface_release_source_image (pat_surface, image, image_extra);
BAIL2:
    _cairo_pattern_release_surface ((cairo_pattern_t *)pattern, pat_surface, &pat_attr);

    return status;
}

static cairo_status_t
_cairo_pdf_surface_emit_meta_surface (cairo_pdf_surface_t  *surface,
				      cairo_surface_t      *meta_surface,
				      cairo_pdf_resource_t *resource)
{
    cairo_array_t group;
    cairo_array_t *old_group;
    double old_width, old_height;
    cairo_matrix_t old_cairo_to_pdf;
    cairo_rectangle_int16_t meta_extents;
    cairo_status_t status;
    int alpha = 0;

    status = _cairo_surface_get_extents (meta_surface, &meta_extents);
    if (status)
	return status;

    _cairo_pdf_surface_resume_content_stream (surface);
    status = _cairo_pdf_surface_stop_content_stream (surface);
    if (status)
	return status;

    _cairo_array_init (&group, sizeof (cairo_pdf_group_element_t));
    old_group = surface->current_group;
    old_width = surface->width;
    old_height = surface->height;
    old_cairo_to_pdf = surface->cairo_to_pdf;
    surface->current_group = &group;
    surface->width = meta_extents.width;
    surface->height = meta_extents.height;
    cairo_matrix_init (&surface->cairo_to_pdf, 1, 0, 0, -1, 0, surface->height);

    status = _cairo_pdf_surface_start_content_stream (surface);
    if (status)
	goto CLEANUP_GROUP;

    if (cairo_surface_get_content (meta_surface) == CAIRO_CONTENT_COLOR) {
	status = _cairo_pdf_surface_add_alpha (surface, 1.0, &alpha);
	if (status)
	    goto CLEANUP_GROUP;

	_cairo_output_stream_printf (surface->output,
				     "q /a%d gs 0 0 0 rg 0 0 %f %f re f Q\r\n",
				     alpha,
				     surface->width,
				     surface->height);
    }

    status = _cairo_meta_surface_replay (meta_surface, &surface->base);
    if (status)
	goto CLEANUP_GROUP;

    status = _cairo_pdf_surface_stop_content_stream (surface);
    if (status)
	goto CLEANUP_GROUP;

    status = _cairo_pdf_surface_open_group (surface);
    if (status)
	goto CLEANUP_GROUP;

    status = _cairo_pdf_surface_write_group_list (surface, &group);
    if (status)
	goto CLEANUP_GROUP;

    status = _cairo_pdf_surface_close_group (surface, resource);
    if (status)
	goto CLEANUP_GROUP;

 CLEANUP_GROUP:
    surface->current_group = old_group;
    surface->width = old_width;
    surface->height = old_height;
    surface->cairo_to_pdf = old_cairo_to_pdf;

    _cairo_pdf_group_element_array_finish (&group);
    _cairo_array_fini (&group);
    if (status)
	return status;

    status = _cairo_pdf_surface_start_content_stream (surface);
    if (status)
	return status;

    _cairo_pdf_surface_pause_content_stream (surface);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_pdf_surface_emit_solid_pattern (cairo_pdf_surface_t   *surface,
				       cairo_solid_pattern_t *pattern)
{
    surface->emitted_pattern.type = CAIRO_PATTERN_TYPE_SOLID;
    surface->emitted_pattern.red = pattern->color.red;
    surface->emitted_pattern.green = pattern->color.green;
    surface->emitted_pattern.blue = pattern->color.blue;
    surface->emitted_pattern.alpha = pattern->color.alpha;
    surface->emitted_pattern.smask.id = 0;
    surface->emitted_pattern.pattern.id = 0;
}

static cairo_status_t
_cairo_pdf_surface_emit_surface_pattern (cairo_pdf_surface_t	 *surface,
					 cairo_surface_pattern_t *pattern)
{
    cairo_pdf_resource_t stream;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_pdf_resource_t pattern_resource = {0}; /* squelch bogus compiler warning */
    cairo_matrix_t cairo_p2d, pdf_p2d;
    cairo_extend_t extend = cairo_pattern_get_extend (&pattern->base);
    double xstep, ystep;
    cairo_rectangle_int16_t surface_extents;
    int pattern_width = 0; /* squelch bogus compiler warning */
    int pattern_height = 0; /* squelch bogus compiler warning */
    int bbox_x, bbox_y;

    _cairo_pdf_surface_pause_content_stream (surface);

    if (_cairo_surface_is_meta (pattern->surface)) {
	cairo_surface_t *meta_surface = pattern->surface;
	cairo_rectangle_int16_t pattern_extents;

	status = _cairo_pdf_surface_emit_meta_surface (surface,
						       meta_surface,
						       &pattern_resource);
	if (status)
	    return status;

	status = _cairo_surface_get_extents (meta_surface, &pattern_extents);
	if (status)
	    return status;

	pattern_width = pattern_extents.width;
	pattern_height = pattern_extents.height;
    } else {
	status = _cairo_pdf_surface_emit_image_surface (surface,
							pattern,
							&pattern_resource,
							&pattern_width,
							&pattern_height);
	if (status)
	    return status;
    }

    status = _cairo_surface_get_extents (&surface->base, &surface_extents);
    if (status)
	return status;

    bbox_x = pattern_width;
    bbox_y = pattern_height;
    switch (extend) {
	/* We implement EXTEND_PAD like EXTEND_NONE for now */
    case CAIRO_EXTEND_PAD:
    case CAIRO_EXTEND_NONE:
    {
	/* In PS/PDF, (as far as I can tell), all patterns are
	 * repeating. So we support cairo's EXTEND_NONE semantics
	 * by setting the repeat step size to a size large enough
	 * to guarantee that no more than a single occurrence will
	 * be visible.
	 *
	 * First, map the surface extents into pattern space (since
	 * xstep and ystep are in pattern space).  Then use an upper
	 * bound on the length of the diagonal of the pattern image
	 * and the surface as repeat size.  This guarantees to never
	 * repeat visibly.
	 */
	double x1 = 0.0, y1 = 0.0;
	double x2 = surface->width, y2 = surface->height;
	_cairo_matrix_transform_bounding_box (&pattern->base.matrix,
					      &x1, &y1, &x2, &y2,
					      NULL);

	/* Rather than computing precise bounds of the union, just
	 * add the surface extents unconditionally. We only
	 * required an answer that's large enough, we don't really
	 * care if it's not as tight as possible.*/
	xstep = ystep = ceil ((x2 - x1) + (y2 - y1) +
			      pattern_width + pattern_height);
    }
    break;
    case CAIRO_EXTEND_REPEAT:
	xstep = pattern_width;
	ystep = pattern_height;
	break;
    case CAIRO_EXTEND_REFLECT:
	bbox_x = pattern_width*2;
	bbox_y = pattern_height*2;
	xstep = bbox_x;
	ystep = bbox_y;
	break;
	/* All the rest (if any) should have been analyzed away, so this
	 * case should be unreachable. */
    default:
	ASSERT_NOT_REACHED;
	xstep = 0;
	ystep = 0;
    }

    /* At this point, (that is, within the surface backend interface),
     * the pattern's matrix maps from cairo's device space to cairo's
     * pattern space, (both with their origin at the upper-left, and
     * cairo's pattern space of size width,height).
     *
     * Then, we must emit a PDF pattern object that maps from its own
     * pattern space, (which has a size that we establish in the BBox
     * dictionary entry), to the PDF page's *initial* space, (which
     * does not benefit from the Y-axis flipping matrix that we emit
     * on each page). So the PDF patterns matrix maps from a
     * (width,height) pattern space to a device space with the origin
     * in the lower-left corner.
     *
     * So to handle all of that, we start with an identity matrix for
     * the PDF pattern to device matrix. We translate it up by the
     * image height then flip it in the Y direction, (moving us from
     * the PDF origin to cairo's origin). We then multiply in the
     * inverse of the cairo pattern matrix, (since it maps from device
     * to pattern, while we're setting up pattern to device). Finally,
     * we translate back down by the image height and flip again to
     * end up at the lower-left origin that PDF expects.
     *
     * Additionally, within the stream that paints the pattern itself,
     * we are using a PDF image object that has a size of (1,1) so we
     * have to scale it up by the image width and height to fill our
     * pattern cell.
     */
    cairo_p2d = pattern->base.matrix;
    status = cairo_matrix_invert (&cairo_p2d);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    cairo_matrix_init_identity (&pdf_p2d);
    cairo_matrix_translate (&pdf_p2d, 0.0, surface_extents.height);
    cairo_matrix_scale (&pdf_p2d, 1.0, -1.0);
    cairo_matrix_multiply (&pdf_p2d, &cairo_p2d, &pdf_p2d);
    cairo_matrix_translate (&pdf_p2d, 0.0, pattern_height);
    cairo_matrix_scale (&pdf_p2d, 1.0, -1.0);

    stream = _cairo_pdf_surface_open_stream (surface,
					     FALSE,
					     "   /BBox [0 0 %d %d]\r\n"
					     "   /XStep %f\r\n"
					     "   /YStep %f\r\n"
					     "   /PatternType 1\r\n"
					     "   /TilingType 1\r\n"
					     "   /PaintType 1\r\n"
					     "   /Matrix [ %f %f %f %f %f %f ]\r\n"
					     "   /Resources << /XObject << /x%d %d 0 R >> >>\r\n",
					     bbox_x, bbox_y,
					     xstep, ystep,
					     pdf_p2d.xx, pdf_p2d.yx,
					     pdf_p2d.xy, pdf_p2d.yy,
					     pdf_p2d.x0, pdf_p2d.y0,
					     pattern_resource.id,
					     pattern_resource.id);
    if (stream.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    if (_cairo_surface_is_meta (pattern->surface)) {
	if (extend == CAIRO_EXTEND_REFLECT) {
	    _cairo_output_stream_printf (surface->output,
					 "q 0 0 %d %d re W n /x%d Do Q\r\n"
					 "q -1 0 0 1 %d 0 cm 0 0 %d %d re W n /x%d Do Q\r\n"
					 "q 1 0 0 -1 0 %d cm 0 0 %d %d re W n /x%d Do Q\r\n"
					 "q -1 0 0 -1 %d %d cm 0 0 %d %d re W n /x%d Do Q\r\n",
					 pattern_width, pattern_height,
					 pattern_resource.id,
					 pattern_width*2, pattern_width, pattern_height,
					 pattern_resource.id,
					 pattern_height*2, pattern_width, pattern_height,
					 pattern_resource.id,
					 pattern_width*2, pattern_height*2, pattern_width, pattern_height,
					 pattern_resource.id);
	} else {
	    _cairo_output_stream_printf (surface->output,
					 "/x%d Do\r\n",
					 pattern_resource.id);
	}
    } else {
	_cairo_output_stream_printf (surface->output,
				     "q %d 0 0 %d 0 0 cm /x%d Do Q\r\n",
				     pattern_width, pattern_height,
				     pattern_resource.id);
    }

    status = _cairo_pdf_surface_close_stream (surface);
    if (status)
	return status;

    _cairo_pdf_surface_resume_content_stream (surface);

    surface->emitted_pattern.type = CAIRO_PATTERN_TYPE_SURFACE;
    surface->emitted_pattern.smask.id = 0;
    surface->emitted_pattern.pattern = stream;
    surface->emitted_pattern.alpha = 1.0;

    return CAIRO_STATUS_SUCCESS;
}

typedef struct _cairo_pdf_color_stop {
    double offset;
    double color[4];
    cairo_pdf_resource_t resource;
} cairo_pdf_color_stop_t;

static cairo_status_t
cairo_pdf_surface_emit_rgb_linear_function (cairo_pdf_surface_t    *surface,
                                            cairo_pdf_color_stop_t *stop1,
                                            cairo_pdf_color_stop_t *stop2,
                                            cairo_pdf_resource_t   *function)
{
    int num_elems, i;
    cairo_pdf_rgb_linear_function_t elem;
    cairo_pdf_resource_t res;
    cairo_status_t status;

    num_elems = _cairo_array_num_elements (&surface->rgb_linear_functions);
    for (i = 0; i < num_elems; i++) {
	_cairo_array_copy_element (&surface->rgb_linear_functions, i, &elem);
        if (memcmp (&elem.color1[0], &stop1->color[0], sizeof (double)*3) != 0)
            continue;
        if (memcmp (&elem.color2[0], &stop2->color[0], sizeof (double)*3) != 0)
            continue;
        *function =  elem.resource;
        return CAIRO_STATUS_SUCCESS;
    }

    res = _cairo_pdf_surface_new_object (surface);
    if (res.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /FunctionType 2\r\n"
				 "   /Domain [ 0 1 ]\r\n"
				 "   /C0 [ %f %f %f ]\r\n"
				 "   /C1 [ %f %f %f ]\r\n"
				 "   /N 1\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 res.id,
                                 stop1->color[0],
                                 stop1->color[1],
                                 stop1->color[2],
                                 stop2->color[0],
                                 stop2->color[1],
                                 stop2->color[2]);

    elem.resource = res;
    memcpy (&elem.color1[0], &stop1->color[0], sizeof (double)*3);
    memcpy (&elem.color2[0], &stop2->color[0], sizeof (double)*3);

    status = _cairo_array_append (&surface->rgb_linear_functions, &elem);
    *function = res;

    return status;
}

static cairo_status_t
cairo_pdf_surface_emit_alpha_linear_function (cairo_pdf_surface_t    *surface,
                                              cairo_pdf_color_stop_t *stop1,
                                              cairo_pdf_color_stop_t *stop2,
                                              cairo_pdf_resource_t   *function)
{
    int num_elems, i;
    cairo_pdf_alpha_linear_function_t elem;
    cairo_pdf_resource_t res;
    cairo_status_t status;

    num_elems = _cairo_array_num_elements (&surface->alpha_linear_functions);
    for (i = 0; i < num_elems; i++) {
	_cairo_array_copy_element (&surface->alpha_linear_functions, i, &elem);
        if (elem.alpha1 != stop1->color[3])
            continue;
        if (elem.alpha2 != stop2->color[3])
            continue;
        *function =  elem.resource;
        return CAIRO_STATUS_SUCCESS;
    }

    res = _cairo_pdf_surface_new_object (surface);
    if (res.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /FunctionType 2\r\n"
				 "   /Domain [ 0 1 ]\r\n"
				 "   /C0 [ %f ]\r\n"
				 "   /C1 [ %f ]\r\n"
				 "   /N 1\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 res.id,
                                 stop1->color[3],
                                 stop2->color[3]);

    elem.resource = res;
    elem.alpha1 = stop1->color[3];
    elem.alpha2 = stop2->color[3];

    status = _cairo_array_append (&surface->alpha_linear_functions, &elem);
    *function = res;

    return status;
}

static cairo_status_t
_cairo_pdf_surface_emit_stitched_colorgradient (cairo_pdf_surface_t    *surface,
                                                unsigned int 	        n_stops,
                                                cairo_pdf_color_stop_t *stops,
                                                cairo_bool_t	        is_alpha,
                                                cairo_pdf_resource_t   *function)
{
    cairo_pdf_resource_t res;
    unsigned int i;
    cairo_status_t status;

    /* emit linear gradients between pairs of subsequent stops... */
    for (i = 0; i < n_stops-1; i++) {
        if (is_alpha) {
            status = cairo_pdf_surface_emit_alpha_linear_function (surface,
                                                                   &stops[i],
                                                                   &stops[i+1],
                                                                   &stops[i].resource);
            if (status)
                return status;
        } else {
            status = cairo_pdf_surface_emit_rgb_linear_function (surface,
                                                                 &stops[i],
                                                                 &stops[i+1],
                                                                 &stops[i].resource);
            if (status)
                return status;
        }
    }

    /* ... and stitch them together */
    res = _cairo_pdf_surface_new_object (surface);
    if (res.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /FunctionType 3\r\n"
				 "   /Domain [ %f %f ]\r\n",
				 res.id,
                                 stops[0].offset,
                                 stops[n_stops - 1].offset);

    _cairo_output_stream_printf (surface->output,
				 "   /Functions [ ");
    for (i = 0; i < n_stops-1; i++)
        _cairo_output_stream_printf (surface->output,
                                     "%d 0 R ", stops[i].resource.id);
    _cairo_output_stream_printf (surface->output,
		    		 "]\r\n");

    _cairo_output_stream_printf (surface->output,
				 "   /Bounds [ ");
    for (i = 1; i < n_stops-1; i++)
        _cairo_output_stream_printf (surface->output,
				     "%f ", stops[i].offset);
    _cairo_output_stream_printf (surface->output,
		    		 "]\r\n");

    _cairo_output_stream_printf (surface->output,
				 "   /Encode [ ");
    for (i = 1; i < n_stops; i++)
        _cairo_output_stream_printf (surface->output,
				     "0 1 ");
    _cairo_output_stream_printf (surface->output,
	    			 "]\r\n");

    _cairo_output_stream_printf (surface->output,
	    			 ">>\r\n"
				 "endobj\r\n");

    *function = res;

    return CAIRO_STATUS_SUCCESS;
}


static void
calc_gradient_color (cairo_pdf_color_stop_t *new_stop,
		     cairo_pdf_color_stop_t *stop1,
		     cairo_pdf_color_stop_t *stop2)
{
    int i;
    double offset = stop1->offset / (stop1->offset + 1.0 - stop2->offset);

    for (i = 0; i < 4; i++)
	new_stop->color[i] = stop1->color[i] + offset*(stop2->color[i] - stop1->color[i]);
}

#define COLOR_STOP_EPSILON 1e-6

static cairo_status_t
_cairo_pdf_surface_emit_pattern_stops (cairo_pdf_surface_t      *surface,
                                       cairo_gradient_pattern_t *pattern,
                                       cairo_pdf_resource_t     *color_function,
                                       cairo_pdf_resource_t     *alpha_function)
{
    cairo_pdf_color_stop_t *allstops, *stops;
    unsigned int n_stops;
    unsigned int i;
    cairo_bool_t emit_alpha = FALSE;
    cairo_status_t status;

    color_function->id = 0;
    alpha_function->id = 0;

    allstops = _cairo_malloc_ab ((pattern->n_stops + 2), sizeof (cairo_pdf_color_stop_t));
    if (allstops == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    stops = &allstops[1];
    n_stops = pattern->n_stops;

    for (i = 0; i < n_stops; i++) {
	stops[i].color[0] = pattern->stops[i].color.red;
	stops[i].color[1] = pattern->stops[i].color.green;
	stops[i].color[2] = pattern->stops[i].color.blue;
	stops[i].color[3] = pattern->stops[i].color.alpha;
        if (!CAIRO_ALPHA_IS_OPAQUE (stops[i].color[3]))
            emit_alpha = TRUE;
	stops[i].offset = _cairo_fixed_to_double (pattern->stops[i].x);
    }

    if (pattern->base.extend == CAIRO_EXTEND_REPEAT ||
	pattern->base.extend == CAIRO_EXTEND_REFLECT) {
	if (stops[0].offset > COLOR_STOP_EPSILON) {
	    if (pattern->base.extend == CAIRO_EXTEND_REFLECT)
		memcpy (allstops, stops, sizeof (cairo_pdf_color_stop_t));
	    else
		calc_gradient_color (&allstops[0], &stops[0], &stops[n_stops-1]);
	    stops = allstops;
	    n_stops++;
	}
	stops[0].offset = 0.0;

	if (stops[n_stops-1].offset < 1.0 - COLOR_STOP_EPSILON) {
	    if (pattern->base.extend == CAIRO_EXTEND_REFLECT) {
		memcpy (&stops[n_stops],
			&stops[n_stops - 1],
			sizeof (cairo_pdf_color_stop_t));
	    } else {
		calc_gradient_color (&stops[n_stops], &stops[0], &stops[n_stops-1]);
	    }
	    n_stops++;
	}
	stops[n_stops-1].offset = 1.0;
    }

    if (n_stops == 2) {
        /* no need for stitched function */
        status = cairo_pdf_surface_emit_rgb_linear_function (surface,
                                                             &stops[0],
                                                             &stops[1],
                                                             color_function);
        if (status)
            goto BAIL;

        if (emit_alpha) {
            status = cairo_pdf_surface_emit_alpha_linear_function (surface,
                                                                   &stops[0],
                                                                   &stops[1],
                                                                   alpha_function);
            if (status)
                goto BAIL;
        }
    } else {
        /* multiple stops: stitch. XXX possible optimization: regulary spaced
         * stops do not require stitching. XXX */
        status = _cairo_pdf_surface_emit_stitched_colorgradient (surface,
                                                                 n_stops,
                                                                 stops,
                                                                 FALSE,
                                                                 color_function);
        if (status)
            goto BAIL;

        if (emit_alpha) {
            status = _cairo_pdf_surface_emit_stitched_colorgradient (surface,
                                                                     n_stops,
                                                                     stops,
                                                                     TRUE,
                                                                     alpha_function);
            if (status)
                goto BAIL;
        }
    }

BAIL:
    free (allstops);
    return status;
}

static cairo_status_t
_cairo_pdf_surface_emit_repeating_function (cairo_pdf_surface_t      *surface,
					    cairo_gradient_pattern_t *pattern,
					    cairo_pdf_resource_t     *function,
					    int                       begin,
					    int                       end)
{
    cairo_pdf_resource_t res;
    int i;

    res = _cairo_pdf_surface_new_object (surface);
    if (res.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /FunctionType 3\r\n"
				 "   /Domain [ %d %d ]\r\n",
				 res.id,
                                 begin,
                                 end);

    _cairo_output_stream_printf (surface->output,
				 "   /Functions [ ");
    for (i = begin; i < end; i++)
        _cairo_output_stream_printf (surface->output,
                                     "%d 0 R ", function->id);
    _cairo_output_stream_printf (surface->output,
				 "]\r\n");

    _cairo_output_stream_printf (surface->output,
				 "   /Bounds [ ");
    for (i = begin + 1; i < end; i++)
        _cairo_output_stream_printf (surface->output,
				     "%d ", i);
    _cairo_output_stream_printf (surface->output,
				 "]\r\n");

    _cairo_output_stream_printf (surface->output,
				 "   /Encode [ ");
    for (i = begin; i < end; i++) {
	if ((i % 2) && pattern->base.extend == CAIRO_EXTEND_REFLECT) {
	    _cairo_output_stream_printf (surface->output,
					 "1 0 ");
	} else {
	    _cairo_output_stream_printf (surface->output,
					 "0 1 ");
	}
    }
    _cairo_output_stream_printf (surface->output,
				 "]\r\n");

    _cairo_output_stream_printf (surface->output,
				 ">>\r\n"
				 "endobj\r\n");

    *function = res;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_pdf_resource_t
cairo_pdf_surface_emit_transparency_group (cairo_pdf_surface_t  *surface,
                                           cairo_pdf_resource_t  gradient_mask)
{
    cairo_pdf_resource_t xobj_resource, smask_resource, gstate_resource;

    xobj_resource = _cairo_pdf_surface_open_stream (surface,
                                                    surface->compress_content,
                                                    "   /Type /XObject\r\n"
                                                    "   /Subtype /Form\r\n"
                                                    "   /FormType 1\r\n"
                                                    "   /BBox [ 0 0 %f %f ]\r\n"
                                                    "   /Resources\r\n"
                                                    "      << /ExtGState\r\n"
                                                    "            << /a0 << /ca 1 /CA 1 >>"
                                                    "      >>\r\n"
                                                    "         /Pattern\r\n"
                                                    "            << /p%d %d 0 R >>\r\n"
                                                    "      >>\r\n"
                                                    "   /Group\r\n"
                                                    "      << /Type /Group\r\n"
                                                    "         /S /Transparency\r\n"
                                                    "         /CS /DeviceGray\r\n"
                                                    "      >>\r\n",
                                                    surface->width,
                                                    surface->height,
                                                    gradient_mask.id,
                                                    gradient_mask.id);
    if (xobj_resource.id == 0)
	return xobj_resource;

    _cairo_output_stream_printf (surface->output,
                                 "q\r\n"
                                 "/a0 gs\r\n"
                                 "/Pattern cs /p%d scn\r\n"
                                 "0 0 %f %f re\r\n"
                                 "f\r\n"
                                 "Q\r\n",
                                 gradient_mask.id,
                                 surface->width,
                                 surface->height);

    if (_cairo_pdf_surface_close_stream (surface)) {
	smask_resource.id = 0;
	return smask_resource;
    }

    smask_resource = _cairo_pdf_surface_new_object (surface);
    if (smask_resource.id == 0)
	return smask_resource;

    _cairo_output_stream_printf (surface->output,
                                 "%d 0 obj\r\n"
                                 "<< /Type /Mask\r\n"
                                 "   /S /Luminosity\r\n"
                                 "   /G %d 0 R\r\n"
                                 ">>\r\n"
                                 "endobj\r\n",
                                 smask_resource.id,
                                 xobj_resource.id);

    /* Create GState which uses the transparency group as an SMask. */
    gstate_resource = _cairo_pdf_surface_new_object (surface);
    if (gstate_resource.id == 0)
	return gstate_resource;

    _cairo_output_stream_printf (surface->output,
                                 "%d 0 obj\r\n"
                                 "<< /Type /ExtGState\r\n"
                                 "   /SMask %d 0 R\r\n"
                                 "   /ca 1\r\n"
                                 "   /CA 1\r\n"
                                 "   /AIS false\r\n"
                                 ">>\r\n"
                                 "endobj\r\n",
                                 gstate_resource.id,
                                 smask_resource.id);

    return gstate_resource;
}

static cairo_status_t
_cairo_pdf_surface_emit_linear_pattern (cairo_pdf_surface_t    *surface,
                                        cairo_linear_pattern_t *pattern)
{
    cairo_pdf_resource_t pattern_resource, smask;
    cairo_pdf_resource_t color_function, alpha_function;
    double x1, y1, x2, y2;
    double _x1, _y1, _x2, _y2;
    cairo_matrix_t pat_to_pdf;
    cairo_extend_t extend;
    cairo_status_t status;
    cairo_gradient_pattern_t *gradient = &pattern->base;
    double first_stop, last_stop;
    int repeat_begin = 0, repeat_end = 1;

    extend = cairo_pattern_get_extend (&pattern->base.base);
    _cairo_pdf_surface_pause_content_stream (surface);

    pat_to_pdf = pattern->base.base.matrix;
    status = cairo_matrix_invert (&pat_to_pdf);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    cairo_matrix_multiply (&pat_to_pdf, &pat_to_pdf, &surface->cairo_to_pdf);
    first_stop = _cairo_fixed_to_double (gradient->stops[0].x);
    last_stop = _cairo_fixed_to_double (gradient->stops[gradient->n_stops - 1].x);

    if (pattern->base.base.extend == CAIRO_EXTEND_REPEAT ||
	pattern->base.base.extend == CAIRO_EXTEND_REFLECT) {
	double dx, dy;
	int x_rep = 0, y_rep = 0;

	x1 = _cairo_fixed_to_double (pattern->p1.x);
	y1 = _cairo_fixed_to_double (pattern->p1.y);
	cairo_matrix_transform_point (&pat_to_pdf, &x1, &y1);

	x2 = _cairo_fixed_to_double (pattern->p2.x);
	y2 = _cairo_fixed_to_double (pattern->p2.y);
	cairo_matrix_transform_point (&pat_to_pdf, &x2, &y2);

	dx = fabs (x2 - x1);
	dy = fabs (y2 - y1);
	if (dx > 1e-6)
	    x_rep = (int) ceil (surface->width/dx);
	if (dy > 1e-6)
	    y_rep = (int) ceil (surface->height/dy);

	repeat_end = MAX (x_rep, y_rep);
	repeat_begin = -repeat_end;
	first_stop = repeat_begin;
	last_stop = repeat_end;
    }

    /* PDF requires the first and last stop to be the same as the line
     * coordinates. For repeating patterns this moves the line
     * coordinates out to the begin/end of the repeating function. For
     * non repeating patterns this may move the line coordinates in if
     * there are not stops at offset 0 and 1. */
    x1 = _cairo_fixed_to_double (pattern->p1.x);
    y1 = _cairo_fixed_to_double (pattern->p1.y);
    x2 = _cairo_fixed_to_double (pattern->p2.x);
    y2 = _cairo_fixed_to_double (pattern->p2.y);

    _x1 = x1 + (x2 - x1)*first_stop;
    _y1 = y1 + (y2 - y1)*first_stop;
    _x2 = x1 + (x2 - x1)*last_stop;
    _y2 = y1 + (y2 - y1)*last_stop;

    x1 = _x1;
    x2 = _x2;
    y1 = _y1;
    y2 = _y2;

    /* For EXTEND_NONE and EXTEND_PAD if there are only two stops a
     * Type 2 function is used by itself without a stitching
     * function. Type 2 functions always have the domain [0 1] */
    if ((pattern->base.base.extend == CAIRO_EXTEND_NONE ||
	 pattern->base.base.extend == CAIRO_EXTEND_PAD) &&
	gradient->n_stops == 2) {
	first_stop = 0.0;
	last_stop = 1.0;
    }

    status = _cairo_pdf_surface_emit_pattern_stops (surface,
                                                    &pattern->base,
                                                    &color_function,
                                                    &alpha_function);
    if (status)
	return status;

    if (pattern->base.base.extend == CAIRO_EXTEND_REPEAT ||
	pattern->base.base.extend == CAIRO_EXTEND_REFLECT) {
	status = _cairo_pdf_surface_emit_repeating_function (surface,
							     &pattern->base,
							     &color_function,
							     repeat_begin,
							     repeat_end);
	if (status)
	    return status;

	if (alpha_function.id != 0) {
	    status = _cairo_pdf_surface_emit_repeating_function (surface,
								 &pattern->base,
								 &alpha_function,
								 repeat_begin,
								 repeat_end);
	    if (status)
		return status;
	}
    }

    pattern_resource = _cairo_pdf_surface_new_object (surface);
    if (pattern_resource.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
                                 "%d 0 obj\r\n"
                                 "<< /Type /Pattern\r\n"
                                 "   /PatternType 2\r\n"
                                 "   /Matrix [ %f %f %f %f %f %f ]\r\n"
                                 "   /Shading\r\n"
                                 "      << /ShadingType 2\r\n"
                                 "         /ColorSpace /DeviceRGB\r\n"
                                 "         /Coords [ %f %f %f %f ]\r\n"
                                 "         /Domain [ %f %f ]\r\n"
                                 "         /Function %d 0 R\r\n",
                                 pattern_resource.id,
                                 pat_to_pdf.xx, pat_to_pdf.yx,
                                 pat_to_pdf.xy, pat_to_pdf.yy,
                                 pat_to_pdf.x0, pat_to_pdf.y0,
                                 x1, y1, x2, y2,
                                 first_stop, last_stop,
                                 color_function.id);

    if (extend == CAIRO_EXTEND_PAD) {
        _cairo_output_stream_printf (surface->output,
                                     "         /Extend [ true true ]\r\n");
    } else {
        _cairo_output_stream_printf (surface->output,
                                     "         /Extend [ false false ]\r\n");
    }

    _cairo_output_stream_printf (surface->output,
                                 "      >>\r\n"
                                 ">>\r\n"
                                 "endobj\r\n");

    if (alpha_function.id == 0) {
         surface->emitted_pattern.smask.id = 0;
    } else {
        cairo_pdf_resource_t mask_resource;

        /* Create pattern for SMask. */
        mask_resource = _cairo_pdf_surface_new_object (surface);
	if (mask_resource.id == 0)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

        _cairo_output_stream_printf (surface->output,
                                     "%d 0 obj\r\n"
                                     "<< /Type /Pattern\r\n"
                                     "   /PatternType 2\r\n"
                                     "   /Matrix [ %f %f %f %f %f %f ]\r\n"
                                     "   /Shading\r\n"
                                     "      << /ShadingType 2\r\n"
                                     "         /ColorSpace /DeviceGray\r\n"
                                     "         /Coords [ %f %f %f %f ]\r\n"
				     "         /Domain [ %f %f ]\r\n"
                                     "         /Function %d 0 R\r\n",
                                     mask_resource.id,
                                     pat_to_pdf.xx, pat_to_pdf.yx,
                                     pat_to_pdf.xy, pat_to_pdf.yy,
                                     pat_to_pdf.x0, pat_to_pdf.y0,
                                     x1, y1, x2, y2,
				     first_stop, last_stop,
                                     alpha_function.id);

        if (extend == CAIRO_EXTEND_PAD) {
            _cairo_output_stream_printf (surface->output,
                                         "         /Extend [ true true ]\r\n");
        } else {
            _cairo_output_stream_printf (surface->output,
                                         "         /Extend [ false false ]\r\n");
        }

        _cairo_output_stream_printf (surface->output,
                                     "      >>\r\n"
                                     ">>\r\n"
                                     "endobj\r\n");
        status = _cairo_pdf_surface_add_pattern (surface, mask_resource);
        if (status)
            return status;

        smask = cairo_pdf_surface_emit_transparency_group (surface, mask_resource);
	if (smask.id == 0)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

        surface->emitted_pattern.smask = smask;
    }

    surface->emitted_pattern.type = CAIRO_PATTERN_TYPE_LINEAR;
    surface->emitted_pattern.pattern = pattern_resource;
    surface->emitted_pattern.alpha = 1.0;

    _cairo_pdf_surface_resume_content_stream (surface);

    return status;
}

static cairo_status_t
_cairo_pdf_surface_emit_radial_pattern (cairo_pdf_surface_t    *surface,
                                        cairo_radial_pattern_t *pattern)
{
    cairo_pdf_resource_t pattern_resource, smask;
    cairo_pdf_resource_t color_function, alpha_function;
    double x1, y1, x2, y2, r1, r2;
    cairo_matrix_t pat_to_pdf;
    cairo_extend_t extend;
    cairo_status_t status;

    extend = cairo_pattern_get_extend (&pattern->base.base);
    _cairo_pdf_surface_pause_content_stream (surface);

    status = _cairo_pdf_surface_emit_pattern_stops (surface,
                                                    &pattern->base,
                                                    &color_function,
                                                    &alpha_function);
    if (status)
	return status;

    pat_to_pdf = pattern->base.base.matrix;
    status = cairo_matrix_invert (&pat_to_pdf);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    cairo_matrix_multiply (&pat_to_pdf, &pat_to_pdf, &surface->cairo_to_pdf);
    x1 = _cairo_fixed_to_double (pattern->c1.x);
    y1 = _cairo_fixed_to_double (pattern->c1.y);
    r1 = _cairo_fixed_to_double (pattern->r1);
    x2 = _cairo_fixed_to_double (pattern->c2.x);
    y2 = _cairo_fixed_to_double (pattern->c2.y);
    r2 = _cairo_fixed_to_double (pattern->r2);

    pattern_resource = _cairo_pdf_surface_new_object (surface);
    if (pattern_resource.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
                                 "%d 0 obj\r\n"
                                 "<< /Type /Pattern\r\n"
                                 "   /PatternType 2\r\n"
                                 "   /Matrix [ %f %f %f %f %f %f ]\r\n"
                                 "   /Shading\r\n"
                                 "      << /ShadingType 3\r\n"
                                 "         /ColorSpace /DeviceRGB\r\n"
                                 "         /Coords [ %f %f %f %f %f %f ]\r\n"
                                 "         /Function %d 0 R\r\n",
                                 pattern_resource.id,
                                 pat_to_pdf.xx, pat_to_pdf.yx,
                                 pat_to_pdf.xy, pat_to_pdf.yy,
                                 pat_to_pdf.x0, pat_to_pdf.y0,
                                 x1, y1, r1, x2, y2, r2,
                                 color_function.id);

    if (extend == CAIRO_EXTEND_PAD) {
        _cairo_output_stream_printf (surface->output,
                                     "         /Extend [ true true ]\r\n");
    } else {
        _cairo_output_stream_printf (surface->output,
                                     "         /Extend [ false false ]\r\n");
    }

    _cairo_output_stream_printf (surface->output,
                                 "      >>\r\n"
                                 ">>\r\n"
                                 "endobj\r\n");

    if (alpha_function.id == 0) {
        surface->emitted_pattern.smask.id = 0;
    } else {
        cairo_pdf_resource_t mask_resource;

        /* Create pattern for SMask. */
        mask_resource = _cairo_pdf_surface_new_object (surface);
	if (mask_resource.id == 0)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

        _cairo_output_stream_printf (surface->output,
                                     "%d 0 obj\r\n"
                                     "<< /Type /Pattern\r\n"
                                     "   /PatternType 2\r\n"
                                     "   /Matrix [ %f %f %f %f %f %f ]\r\n"
                                     "   /Shading\r\n"
                                     "      << /ShadingType 3\r\n"
                                     "         /ColorSpace /DeviceGray\r\n"
                                     "         /Coords [ %f %f %f %f %f %f ]\r\n"
                                     "         /Function %d 0 R\r\n",
                                     mask_resource.id,
                                     pat_to_pdf.xx, pat_to_pdf.yx,
                                     pat_to_pdf.xy, pat_to_pdf.yy,
                                     pat_to_pdf.x0, pat_to_pdf.y0,
                                     x1, y1, r1, x2, y2, r2,
                                     alpha_function.id);

        if (extend == CAIRO_EXTEND_PAD) {
            _cairo_output_stream_printf (surface->output,
                                         "         /Extend [ true true ]\r\n");
        } else {
            _cairo_output_stream_printf (surface->output,
                                         "         /Extend [ false false ]\r\n");
        }

        _cairo_output_stream_printf (surface->output,
                                     "      >>\r\n"
                                     ">>\r\n"
                                     "endobj\r\n");

        smask = cairo_pdf_surface_emit_transparency_group (surface, mask_resource);
	if (smask.id == 0)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

        surface->emitted_pattern.smask = smask;
    }

    surface->emitted_pattern.type = CAIRO_PATTERN_TYPE_RADIAL;
    surface->emitted_pattern.pattern = pattern_resource;
    surface->emitted_pattern.alpha = 1.0;

     _cairo_pdf_surface_resume_content_stream (surface);

     return status;
}

static cairo_status_t
_cairo_pdf_surface_emit_pattern (cairo_pdf_surface_t *surface, cairo_pattern_t *pattern)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	_cairo_pdf_surface_emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern);
        return CAIRO_STATUS_SUCCESS;

    case CAIRO_PATTERN_TYPE_SURFACE:
	return _cairo_pdf_surface_emit_surface_pattern (surface, (cairo_surface_pattern_t *) pattern);

    case CAIRO_PATTERN_TYPE_LINEAR:
	return _cairo_pdf_surface_emit_linear_pattern (surface, (cairo_linear_pattern_t *) pattern);

    case CAIRO_PATTERN_TYPE_RADIAL:
	return _cairo_pdf_surface_emit_radial_pattern (surface, (cairo_radial_pattern_t *) pattern);
    }

    ASSERT_NOT_REACHED;
    return _cairo_error (CAIRO_STATUS_PATTERN_TYPE_MISMATCH);
}

static cairo_status_t
_cairo_pdf_surface_select_pattern (cairo_pdf_surface_t *surface,
                                   cairo_bool_t         is_stroke)
{
    cairo_status_t status;
    int alpha;

    status = _cairo_pdf_surface_add_alpha (surface, surface->emitted_pattern.alpha, &alpha);
    if (status)
	return status;

    if (surface->emitted_pattern.type == CAIRO_PATTERN_TYPE_SOLID) {
	_cairo_output_stream_printf (surface->output,
                                     "%f %f %f ",
                                     surface->emitted_pattern.red,
                                     surface->emitted_pattern.green,
                                     surface->emitted_pattern.blue);

        if (is_stroke)
            _cairo_output_stream_printf (surface->output, "RG ");
        else
            _cairo_output_stream_printf (surface->output, "rg ");

        _cairo_output_stream_printf (surface->output,
                                     "/a%d gs\r\n",
                                     alpha);
    } else {
        if (is_stroke) {
            _cairo_output_stream_printf (surface->output,
                                         "/Pattern CS /p%d SCN ",
                                         surface->emitted_pattern.pattern.id);
        } else {
            _cairo_output_stream_printf (surface->output,
                                         "/Pattern cs /p%d scn ",
                                         surface->emitted_pattern.pattern.id);
        }
        status = _cairo_pdf_surface_add_pattern (surface, surface->emitted_pattern.pattern);
	if (status)
	    return status;

        _cairo_output_stream_printf (surface->output,
                                     "/a%d gs ",
                                     alpha);

        _cairo_output_stream_printf (surface->output, "\r\n");
    }

    return _cairo_output_stream_get_status (surface->output);
}

static cairo_int_status_t
_cairo_pdf_surface_copy_page (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_int_status_t status;

    status = _cairo_pdf_surface_stop_content_stream (surface);
    if (status)
	return status;

    return _cairo_pdf_surface_write_page (surface);
}

static cairo_int_status_t
_cairo_pdf_surface_show_page (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_int_status_t status;

    status = _cairo_pdf_surface_stop_content_stream (surface);
    if (status)
	return status;

    status = _cairo_pdf_surface_write_page (surface);
    if (status)
	return status;

    _cairo_pdf_surface_clear (surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_get_extents (void		        *abstract_surface,
				cairo_rectangle_int_t   *rectangle)
{
    cairo_pdf_surface_t *surface = abstract_surface;

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

typedef struct _pdf_path_info {
    cairo_output_stream_t   *output;
    cairo_matrix_t	    *cairo_to_pdf;
    cairo_matrix_t	    *ctm_inverse;
} pdf_path_info_t;

static cairo_status_t
_cairo_pdf_path_move_to (void *closure, cairo_point_t *point)
{
    pdf_path_info_t *info = closure;
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (info->cairo_to_pdf)
        cairo_matrix_transform_point (info->cairo_to_pdf, &x, &y);
    if (info->ctm_inverse)
	cairo_matrix_transform_point (info->ctm_inverse, &x, &y);

    _cairo_output_stream_printf (info->output,
				 "%f %f m ", x, y);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_path_line_to (void *closure, cairo_point_t *point)
{
    pdf_path_info_t *info = closure;
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (info->cairo_to_pdf)
        cairo_matrix_transform_point (info->cairo_to_pdf, &x, &y);
    if (info->ctm_inverse)
	cairo_matrix_transform_point (info->ctm_inverse, &x, &y);

    _cairo_output_stream_printf (info->output,
				 "%f %f l ", x, y);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_path_curve_to (void          *closure,
			  cairo_point_t *b,
			  cairo_point_t *c,
			  cairo_point_t *d)
{
    pdf_path_info_t *info = closure;
    double bx = _cairo_fixed_to_double (b->x);
    double by = _cairo_fixed_to_double (b->y);
    double cx = _cairo_fixed_to_double (c->x);
    double cy = _cairo_fixed_to_double (c->y);
    double dx = _cairo_fixed_to_double (d->x);
    double dy = _cairo_fixed_to_double (d->y);

    if (info->cairo_to_pdf) {
        cairo_matrix_transform_point (info->cairo_to_pdf, &bx, &by);
        cairo_matrix_transform_point (info->cairo_to_pdf, &cx, &cy);
        cairo_matrix_transform_point (info->cairo_to_pdf, &dx, &dy);
    }
    if (info->ctm_inverse) {
	cairo_matrix_transform_point (info->ctm_inverse, &bx, &by);
	cairo_matrix_transform_point (info->ctm_inverse, &cx, &cy);
	cairo_matrix_transform_point (info->ctm_inverse, &dx, &dy);
    }

    _cairo_output_stream_printf (info->output,
				 "%f %f %f %f %f %f c ",
				 bx, by, cx, cy, dx, dy);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_path_close_path (void *closure)
{
    pdf_path_info_t *info = closure;

    _cairo_output_stream_printf (info->output,
				 "h\r\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_surface_add_clip (cairo_pdf_surface_t  *surface,
			     cairo_path_fixed_t	  *path,
			     cairo_fill_rule_t     fill_rule)
{
    cairo_pdf_group_element_t elem;
    cairo_status_t status;

    memset (&elem, 0, sizeof elem);
    elem.type = ELEM_CLIP;

    if (path == NULL) {
	elem.clip_path = NULL;
    } else {
	elem.clip_path = _cairo_path_fixed_create ();
	if (elem.clip_path == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	status = _cairo_path_fixed_init_copy (elem.clip_path, path);
	if (status) {
	    _cairo_path_fixed_destroy (elem.clip_path);
	    return status;
	}
    }
    elem.fill_rule = fill_rule;

    status = _cairo_pdf_surface_stop_content_stream (surface);
    if (status) {
	if (elem.clip_path != NULL)
	    _cairo_path_fixed_destroy (elem.clip_path);
	return status;
    }

    status = _cairo_array_append (surface->current_group, &elem);
    if (status) {
	if (elem.clip_path != NULL)
	    _cairo_path_fixed_destroy (elem.clip_path);
	return status;
    }

    status = _cairo_pdf_surface_start_content_stream (surface);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_intersect_clip_path (void			*abstract_surface,
					cairo_path_fixed_t	*path,
					cairo_fill_rule_t	fill_rule,
					double			tolerance,
					cairo_antialias_t	antialias)
{
    cairo_pdf_surface_t *surface = abstract_surface;

    return _cairo_pdf_surface_add_clip (surface, path, fill_rule);
}

static void
_cairo_pdf_surface_get_font_options (void                  *abstract_surface,
				     cairo_font_options_t  *options)
{
    _cairo_font_options_init_default (options);

    cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_OFF);
    cairo_font_options_set_antialias (options, CAIRO_ANTIALIAS_GRAY);
}

static cairo_pdf_resource_t
_cairo_pdf_surface_write_info (cairo_pdf_surface_t *surface)
{
    cairo_pdf_resource_t info;

    info = _cairo_pdf_surface_new_object (surface);
    if (info.id == 0)
	return info;

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Creator (cairo %s (http://cairographics.org))\r\n"
				 "   /Producer (cairo %s (http://cairographics.org))\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 info.id,
                                 cairo_version_string (),
                                 cairo_version_string ());

    return info;
}

static void
_cairo_pdf_surface_write_pages (cairo_pdf_surface_t *surface)
{
    cairo_pdf_resource_t page;
    int num_pages, i;

    _cairo_pdf_surface_update_object (surface, surface->pages_resource);
    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /Pages\r\n"
				 "   /Kids [ ",
				 surface->pages_resource.id);

    num_pages = _cairo_array_num_elements (&surface->pages);
    for (i = 0; i < num_pages; i++) {
	_cairo_array_copy_element (&surface->pages, i, &page);
	_cairo_output_stream_printf (surface->output, "%d 0 R ", page.id);
    }

    _cairo_output_stream_printf (surface->output, "]\r\n");
    _cairo_output_stream_printf (surface->output, "   /Count %d\r\n", num_pages);


    /* TODO: Figure out wich other defaults to be inherited by /Page
     * objects. */
    _cairo_output_stream_printf (surface->output,
				 ">>\r\n"
				 "endobj\r\n");
}

static cairo_int_status_t
_cairo_pdf_surface_emit_to_unicode_stream (cairo_pdf_surface_t		*surface,
					   cairo_scaled_font_subset_t	*font_subset,
                                           cairo_bool_t                  is_composite,
					   cairo_pdf_resource_t         *stream)
{
    const cairo_scaled_font_backend_t *backend;
    unsigned int i, num_bfchar;

    stream->id = 0;
    if (font_subset->to_unicode == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (_cairo_truetype_create_glyph_to_unicode_map (font_subset) != CAIRO_STATUS_SUCCESS) {
        backend = font_subset->scaled_font->backend;
        if (backend->map_glyphs_to_unicode == NULL)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

        backend->map_glyphs_to_unicode (font_subset->scaled_font, font_subset);
    }

    *stream = _cairo_pdf_surface_open_stream (surface,
					     surface->compress_content,
					     NULL);
    if (stream->id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
                                 "/CIDInit /ProcSet findresource begin\r\n"
                                 "12 dict begin\r\n"
                                 "begincmap\r\n"
                                 "/CIDSystemInfo\r\n"
                                 "<< /Registry (Adobe)\r\n"
                                 "   /Ordering (UCS)\r\n"
                                 "   /Supplement 0\r\n"
                                 ">> def\r\n"
                                 "/CMapName /Adobe-Identity-UCS def\r\n"
                                 "/CMapType 2 def\r\n"
                                 "1 begincodespacerange\r\n");

    if (is_composite) {
        _cairo_output_stream_printf (surface->output,
                                     "<0000> <ffff>\r\n");
    } else {
        _cairo_output_stream_printf (surface->output,
                                     "<00> <ff>\r\n");
    }

    _cairo_output_stream_printf (surface->output,
                                  "endcodespacerange\r\n");

    num_bfchar = font_subset->num_glyphs - 1;
    /* The CMap specification has a limit of 100 characters per beginbfchar operator */
    _cairo_output_stream_printf (surface->output,
                                 "%d beginbfchar\r\n",
                                 num_bfchar > 100 ? 100 : num_bfchar);
    for (i = 0; i < num_bfchar; i++) {
        if (i != 0 && i % 100 == 0) {
            _cairo_output_stream_printf (surface->output,
                                         "endbfchar\r\n"
                                         "%d beginbfchar\r\n",
                                         num_bfchar - i > 100 ? 100 : num_bfchar - i);
        }
        if (is_composite) {
            _cairo_output_stream_printf (surface->output,
                                         "<%04x> <%04x>\r\n",
                                         i + 1, font_subset->to_unicode[i + 1]);
        } else {
            _cairo_output_stream_printf (surface->output,
                                         "<%02x> <%04x>\r\n",
                                         i + 1, font_subset->to_unicode[i + 1]);
        }
    }
    _cairo_output_stream_printf (surface->output,
                                 "endbfchar\r\n");

    _cairo_output_stream_printf (surface->output,
                                 "endcmap\r\n"
                                 "CMapName currentdict /CMap defineresource pop\r\n"
                                 "end\r\n"
                                 "end\r\n");

    return _cairo_pdf_surface_close_stream (surface);
}

static cairo_status_t
_cairo_pdf_surface_emit_cff_font (cairo_pdf_surface_t		*surface,
                                  cairo_scaled_font_subset_t	*font_subset,
                                  cairo_cff_subset_t            *subset)
{
    cairo_pdf_resource_t stream, descriptor, cidfont_dict;
    cairo_pdf_resource_t subset_resource, to_unicode_stream;
    cairo_pdf_font_t font;
    unsigned long compressed_length;
    char *compressed;
    unsigned int i;
    cairo_status_t status;

    compressed = compress_dup (subset->data, subset->data_length, &compressed_length);
    if (compressed == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    stream = _cairo_pdf_surface_new_object (surface);
    if (stream.id == 0) {
	free (compressed);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Filter /FlateDecode\r\n"
				 "   /Length %lu\r\n"
				 "   /Subtype /CIDFontType0C\r\n"
				 ">>\r\n"
				 "stream\r\n",
				 stream.id,
				 compressed_length);
    _cairo_output_stream_write (surface->output, compressed, compressed_length);
    _cairo_output_stream_printf (surface->output,
				 "\r\n"
				 "endstream\r\n"
				 "endobj\r\n");
    free (compressed);

    status = _cairo_pdf_surface_emit_to_unicode_stream (surface,
	                                                font_subset, TRUE,
							&to_unicode_stream);
    if (status && status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    descriptor = _cairo_pdf_surface_new_object (surface);
    if (descriptor.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /FontDescriptor\r\n"
				 "   /FontName /%s\r\n"
				 "   /Flags 4\r\n"
				 "   /FontBBox [ %ld %ld %ld %ld ]\r\n"
				 "   /ItalicAngle 0\r\n"
				 "   /Ascent %ld\r\n"
				 "   /Descent %ld\r\n"
				 "   /CapHeight 500\r\n"
				 "   /StemV 80\r\n"
				 "   /StemH 80\r\n"
				 "   /FontFile3 %u 0 R\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 descriptor.id,
				 subset->base_font,
				 subset->x_min,
				 subset->y_min,
				 subset->x_max,
				 subset->y_max,
				 subset->ascent,
				 subset->descent,
				 stream.id);

    cidfont_dict = _cairo_pdf_surface_new_object (surface);
    if (cidfont_dict.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
                                 "%d 0 obj\r\n"
                                 "<< /Type /Font\r\n"
                                 "   /Subtype /CIDFontType0\r\n"
                                 "   /BaseFont /%s\r\n"
                                 "   /CIDSystemInfo\r\n"
                                 "   << /Registry (Adobe)\r\n"
                                 "      /Ordering (Identity)\r\n"
                                 "      /Supplement 0\r\n"
                                 "   >>\r\n"
                                 "   /FontDescriptor %d 0 R\r\n"
                                 "   /W [0 [",
                                 cidfont_dict.id,
                                 subset->base_font,
                                 descriptor.id);

    for (i = 0; i < font_subset->num_glyphs; i++)
	_cairo_output_stream_printf (surface->output,
				     " %d",
				     subset->widths[i]);

    _cairo_output_stream_printf (surface->output,
                                 " ]]\r\n"
				 ">>\r\n"
				 "endobj\r\n");

    subset_resource = _cairo_pdf_surface_get_font_resource (surface,
							    font_subset->font_id,
							    font_subset->subset_id);
    _cairo_pdf_surface_update_object (surface, subset_resource);
    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /Font\r\n"
				 "   /Subtype /Type0\r\n"
				 "   /BaseFont /%s\r\n"
                                 "   /Encoding /Identity-H\r\n"
				 "   /DescendantFonts [ %d 0 R]\r\n",
				 subset_resource.id,
				 subset->base_font,
				 cidfont_dict.id);

    if (to_unicode_stream.id != 0)
        _cairo_output_stream_printf (surface->output,
                                     "   /ToUnicode %d 0 R\r\n",
                                     to_unicode_stream.id);

    _cairo_output_stream_printf (surface->output,
				 ">>\r\n"
				 "endobj\r\n");

    font.font_id = font_subset->font_id;
    font.subset_id = font_subset->subset_id;
    font.subset_resource = subset_resource;
    status = _cairo_array_append (&surface->fonts, &font);

    return status;
}

static cairo_status_t
_cairo_pdf_surface_emit_cff_font_subset (cairo_pdf_surface_t	     *surface,
                                         cairo_scaled_font_subset_t  *font_subset)
{
    cairo_status_t status;
    cairo_cff_subset_t subset;
    char name[64];

    snprintf (name, sizeof name, "CairoFont-%d-%d",
              font_subset->font_id, font_subset->subset_id);
    status = _cairo_cff_subset_init (&subset, name, font_subset);
    if (status)
        return status;

    status = _cairo_pdf_surface_emit_cff_font (surface, font_subset, &subset);

    _cairo_cff_subset_fini (&subset);

    return status;
}

static cairo_status_t
_cairo_pdf_surface_emit_cff_fallback_font (cairo_pdf_surface_t	       *surface,
                                           cairo_scaled_font_subset_t  *font_subset)
{
    cairo_status_t status;
    cairo_cff_subset_t subset;
    char name[64];

    snprintf (name, sizeof name, "CairoFont-%d-%d",
              font_subset->font_id, font_subset->subset_id);
    status = _cairo_cff_fallback_init (&subset, name, font_subset);
    if (status)
        return status;

    status = _cairo_pdf_surface_emit_cff_font (surface, font_subset, &subset);

    _cairo_cff_fallback_fini (&subset);

    return status;
}

static cairo_status_t
_cairo_pdf_surface_emit_type1_font (cairo_pdf_surface_t		*surface,
                                    cairo_scaled_font_subset_t	*font_subset,
                                    cairo_type1_subset_t        *subset)
{
    cairo_pdf_resource_t stream, descriptor, subset_resource, to_unicode_stream;
    cairo_pdf_font_t font;
    cairo_status_t status;
    unsigned long length, compressed_length;
    char *compressed;
    unsigned int i;


    /* We ignore the zero-trailer and set Length3 to 0. */
    length = subset->header_length + subset->data_length;
    compressed = compress_dup (subset->data, length, &compressed_length);
    if (compressed == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    stream = _cairo_pdf_surface_new_object (surface);
    if (stream.id == 0) {
	free (compressed);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Filter /FlateDecode\r\n"
				 "   /Length %lu\r\n"
				 "   /Length1 %lu\r\n"
				 "   /Length2 %lu\r\n"
				 "   /Length3 0\r\n"
				 ">>\r\n"
				 "stream\r\n",
				 stream.id,
				 compressed_length,
				 subset->header_length,
				 subset->data_length);
    _cairo_output_stream_write (surface->output, compressed, compressed_length);
    _cairo_output_stream_printf (surface->output,
				 "\r\n"
				 "endstream\r\n"
				 "endobj\r\n");
    free (compressed);

    status = _cairo_pdf_surface_emit_to_unicode_stream (surface,
	                                                font_subset, FALSE,
							&to_unicode_stream);
    if (status && status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    descriptor = _cairo_pdf_surface_new_object (surface);
    if (descriptor.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /FontDescriptor\r\n"
				 "   /FontName /%s\r\n"
				 "   /Flags 4\r\n"
				 "   /FontBBox [ %ld %ld %ld %ld ]\r\n"
				 "   /ItalicAngle 0\r\n"
				 "   /Ascent %ld\r\n"
				 "   /Descent %ld\r\n"
				 "   /CapHeight 500\r\n"
				 "   /StemV 80\r\n"
				 "   /StemH 80\r\n"
				 "   /FontFile %u 0 R\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 descriptor.id,
				 subset->base_font,
				 subset->x_min,
				 subset->y_min,
				 subset->x_max,
				 subset->y_max,
				 subset->ascent,
				 subset->descent,
				 stream.id);

    subset_resource = _cairo_pdf_surface_get_font_resource (surface,
							    font_subset->font_id,
							    font_subset->subset_id);
    _cairo_pdf_surface_update_object (surface, subset_resource);
    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /Font\r\n"
				 "   /Subtype /Type1\r\n"
				 "   /BaseFont /%s\r\n"
				 "   /FirstChar 0\r\n"
				 "   /LastChar %d\r\n"
				 "   /FontDescriptor %d 0 R\r\n"
				 "   /Widths [",
				 subset_resource.id,
				 subset->base_font,
				 font_subset->num_glyphs - 1,
				 descriptor.id);

    for (i = 0; i < font_subset->num_glyphs; i++)
	_cairo_output_stream_printf (surface->output,
				     " %d",
				     subset->widths[i]);

    _cairo_output_stream_printf (surface->output,
				 " ]\r\n");

    if (to_unicode_stream.id != 0)
        _cairo_output_stream_printf (surface->output,
                                     "    /ToUnicode %d 0 R\r\n",
                                     to_unicode_stream.id);

    _cairo_output_stream_printf (surface->output,
				 ">>\r\n"
				 "endobj\r\n");

    font.font_id = font_subset->font_id;
    font.subset_id = font_subset->subset_id;
    font.subset_resource = subset_resource;
    return _cairo_array_append (&surface->fonts, &font);
}

#if CAIRO_HAS_FT_FONT
static cairo_status_t
_cairo_pdf_surface_emit_type1_font_subset (cairo_pdf_surface_t		*surface,
					   cairo_scaled_font_subset_t	*font_subset)
{
    cairo_status_t status;
    cairo_type1_subset_t subset;
    char name[64];

    snprintf (name, sizeof name, "CairoFont-%d-%d",
 	      font_subset->font_id, font_subset->subset_id);
    status = _cairo_type1_subset_init (&subset, name, font_subset, FALSE);
    if (status)
	return status;

    status = _cairo_pdf_surface_emit_type1_font (surface, font_subset, &subset);

    _cairo_type1_subset_fini (&subset);
    return status;
}
#endif

static cairo_status_t
_cairo_pdf_surface_emit_type1_fallback_font (cairo_pdf_surface_t	*surface,
                                             cairo_scaled_font_subset_t	*font_subset)
{
    cairo_status_t status;
    cairo_type1_subset_t subset;
    char name[64];

    snprintf (name, sizeof name, "CairoFont-%d-%d",
	      font_subset->font_id, font_subset->subset_id);
    status = _cairo_type1_fallback_init_binary (&subset, name, font_subset);
    if (status)
	return status;

    status = _cairo_pdf_surface_emit_type1_font (surface, font_subset, &subset);

    _cairo_type1_fallback_fini (&subset);
    return status;
}

#define PDF_UNITS_PER_EM 1000

static cairo_status_t
_cairo_pdf_surface_emit_truetype_font_subset (cairo_pdf_surface_t		*surface,
					      cairo_scaled_font_subset_t	*font_subset)
{
    cairo_pdf_resource_t stream, descriptor, cidfont_dict;
    cairo_pdf_resource_t subset_resource, to_unicode_stream;
    cairo_status_t status;
    cairo_pdf_font_t font;
    cairo_truetype_subset_t subset;
    unsigned long compressed_length;
    char *compressed;
    unsigned int i;

    status = _cairo_truetype_subset_init (&subset, font_subset);
    if (status)
	return status;

    compressed = compress_dup (subset.data, subset.data_length,
			       &compressed_length);
    if (compressed == NULL) {
	_cairo_truetype_subset_fini (&subset);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    stream = _cairo_pdf_surface_new_object (surface);
    if (stream.id == 0) {
	free (compressed);
	_cairo_truetype_subset_fini (&subset);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }
    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Filter /FlateDecode\r\n"
				 "   /Length %lu\r\n"
				 "   /Length1 %lu\r\n"
				 ">>\r\n"
				 "stream\r\n",
				 stream.id,
				 compressed_length,
				 subset.data_length);
    _cairo_output_stream_write (surface->output, compressed, compressed_length);
    _cairo_output_stream_printf (surface->output,
				 "\r\n"
				 "endstream\r\n"
				 "endobj\r\n");
    free (compressed);

    status = _cairo_pdf_surface_emit_to_unicode_stream (surface,
	                                                font_subset, TRUE,
							&to_unicode_stream);
    if (status && status != CAIRO_INT_STATUS_UNSUPPORTED) {
	_cairo_truetype_subset_fini (&subset);
	return status;
    }

    descriptor = _cairo_pdf_surface_new_object (surface);
    if (descriptor.id == 0) {
	_cairo_truetype_subset_fini (&subset);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /FontDescriptor\r\n"
				 "   /FontName /%s\r\n"
				 "   /Flags 4\r\n"
				 "   /FontBBox [ %ld %ld %ld %ld ]\r\n"
				 "   /ItalicAngle 0\r\n"
				 "   /Ascent %ld\r\n"
				 "   /Descent %ld\r\n"
				 "   /CapHeight %ld\r\n"
				 "   /StemV 80\r\n"
				 "   /StemH 80\r\n"
				 "   /FontFile2 %u 0 R\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 descriptor.id,
				 subset.base_font,
				 (long)(subset.x_min*PDF_UNITS_PER_EM),
				 (long)(subset.y_min*PDF_UNITS_PER_EM),
                                 (long)(subset.x_max*PDF_UNITS_PER_EM),
				 (long)(subset.y_max*PDF_UNITS_PER_EM),
				 (long)(subset.ascent*PDF_UNITS_PER_EM),
				 (long)(subset.descent*PDF_UNITS_PER_EM),
				 (long)(subset.y_max*PDF_UNITS_PER_EM),
				 stream.id);

    cidfont_dict = _cairo_pdf_surface_new_object (surface);
    if (cidfont_dict.id == 0) {
	_cairo_truetype_subset_fini (&subset);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    _cairo_output_stream_printf (surface->output,
                                 "%d 0 obj\r\n"
                                 "<< /Type /Font\r\n"
                                 "   /Subtype /CIDFontType2\r\n"
                                 "   /BaseFont /%s\r\n"
                                 "   /CIDSystemInfo\r\n"
                                 "   << /Registry (Adobe)\r\n"
                                 "      /Ordering (Identity)\r\n"
                                 "      /Supplement 0\r\n"
                                 "   >>\r\n"
                                 "   /FontDescriptor %d 0 R\r\n"
                                 "   /W [0 [",
                                 cidfont_dict.id,
                                 subset.base_font,
                                 descriptor.id);

    for (i = 0; i < font_subset->num_glyphs; i++)
        _cairo_output_stream_printf (surface->output,
                                     " %ld",
                                     (long)(subset.widths[i]*PDF_UNITS_PER_EM));

    _cairo_output_stream_printf (surface->output,
                                 " ]]\r\n"
				 ">>\r\n"
				 "endobj\r\n");

    subset_resource = _cairo_pdf_surface_get_font_resource (surface,
							    font_subset->font_id,
							    font_subset->subset_id);
    _cairo_pdf_surface_update_object (surface, subset_resource);
    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /Font\r\n"
				 "   /Subtype /Type0\r\n"
				 "   /BaseFont /%s\r\n"
                                 "   /Encoding /Identity-H\r\n"
				 "   /DescendantFonts [ %d 0 R]\r\n",
				 subset_resource.id,
				 subset.base_font,
				 cidfont_dict.id);

    if (to_unicode_stream.id != 0)
        _cairo_output_stream_printf (surface->output,
                                     "   /ToUnicode %d 0 R\r\n",
                                     to_unicode_stream.id);

    _cairo_output_stream_printf (surface->output,
				 ">>\r\n"
				 "endobj\r\n");

    font.font_id = font_subset->font_id;
    font.subset_id = font_subset->subset_id;
    font.subset_resource = subset_resource;
    status = _cairo_array_append (&surface->fonts, &font);

    _cairo_truetype_subset_fini (&subset);

    return status;
}

static cairo_int_status_t
_cairo_pdf_surface_emit_outline_glyph (cairo_pdf_surface_t	*surface,
				       cairo_scaled_font_t	*scaled_font,
				       unsigned long		 glyph_index,
				       cairo_pdf_resource_t	*glyph_ret)
{
    cairo_scaled_glyph_t *scaled_glyph;
    pdf_path_info_t info;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    status = _cairo_scaled_glyph_lookup (scaled_font,
					 glyph_index,
					 CAIRO_SCALED_GLYPH_INFO_METRICS|
					 CAIRO_SCALED_GLYPH_INFO_PATH,
					 &scaled_glyph);
    if (status)
	return status;

    *glyph_ret = _cairo_pdf_surface_open_stream (surface,
						 surface->compress_content,
						 NULL);
    if (glyph_ret->id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
				 "0 0 %f %f %f %f d1\r\n",
				 _cairo_fixed_to_double (scaled_glyph->bbox.p1.x),
				 -_cairo_fixed_to_double (scaled_glyph->bbox.p2.y),
				 _cairo_fixed_to_double (scaled_glyph->bbox.p2.x),
				 -_cairo_fixed_to_double (scaled_glyph->bbox.p1.y));

    info.output = surface->output;
    info.cairo_to_pdf = &surface->cairo_to_pdf;
    info.ctm_inverse = NULL;

    status = _cairo_path_fixed_interpret (scaled_glyph->path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_pdf_path_move_to,
					  _cairo_pdf_path_line_to,
					  _cairo_pdf_path_curve_to,
					  _cairo_pdf_path_close_path,
					  &info);
    if (status) {
	/* ignore status return as we are on the error path... */
	_cairo_pdf_surface_close_stream (surface);
	return status;
    }

    _cairo_output_stream_printf (surface->output,
				 " f");

    return _cairo_pdf_surface_close_stream (surface);
}

static cairo_int_status_t
_cairo_pdf_surface_emit_bitmap_glyph (cairo_pdf_surface_t	*surface,
				      cairo_scaled_font_t	*scaled_font,
				      unsigned long		 glyph_index,
				      cairo_pdf_resource_t	*glyph_ret,
                                      cairo_box_t               *bbox,
                                      double                    *width)
{
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_status_t status;
    cairo_image_surface_t *image;
    unsigned char *row, *byte;
    int rows, cols;
    double x_advance, y_advance;

    status = _cairo_scaled_glyph_lookup (scaled_font,
					 glyph_index,
					 CAIRO_SCALED_GLYPH_INFO_METRICS|
					 CAIRO_SCALED_GLYPH_INFO_SURFACE,
					 &scaled_glyph);
    if (status)
	return status;

    x_advance = scaled_glyph->metrics.x_advance;
    y_advance = scaled_glyph->metrics.y_advance;
    cairo_matrix_transform_distance (&scaled_font->ctm, &x_advance, &y_advance);
    *bbox = scaled_glyph->bbox;
    *width = x_advance;

    image = scaled_glyph->surface;
    if (image->format != CAIRO_FORMAT_A1) {
	image = _cairo_image_surface_clone (image, CAIRO_FORMAT_A1);
	if (cairo_surface_status (&image->base))
	    return cairo_surface_status (&image->base);
    }

    *glyph_ret = _cairo_pdf_surface_open_stream (surface,
						 surface->compress_content,
						 NULL);
    if (glyph_ret->id == 0) {
	if (image != scaled_glyph->surface)
	    cairo_surface_destroy (&image->base);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    _cairo_output_stream_printf (surface->output,
				 "%f 0 %f %f %f %f d1\r\n",
                                 x_advance,
				 _cairo_fixed_to_double (scaled_glyph->bbox.p1.x),
				 _cairo_fixed_to_double (scaled_glyph->bbox.p2.y),
				 _cairo_fixed_to_double (scaled_glyph->bbox.p2.x),
				 _cairo_fixed_to_double (scaled_glyph->bbox.p1.y));

    _cairo_output_stream_printf (surface->output,
				 "%f 0 0 %f %f %f cm\r\n",
				 _cairo_fixed_to_double (scaled_glyph->bbox.p2.x) - _cairo_fixed_to_double (scaled_glyph->bbox.p1.x),
				 _cairo_fixed_to_double (scaled_glyph->bbox.p1.y) - _cairo_fixed_to_double (scaled_glyph->bbox.p2.y),
				 _cairo_fixed_to_double (scaled_glyph->bbox.p1.x),
				 _cairo_fixed_to_double (scaled_glyph->bbox.p2.y));

    _cairo_output_stream_printf (surface->output,
				 "BI\r\n"
				 "/IM true\r\n"
				 "/W %d\r\n"
				 "/H %d\r\n"
				 "/BPC 1\r\n"
				 "/D [1 0]\r\n",
				 image->width,
				 image->height);

    _cairo_output_stream_printf (surface->output,
				 "ID ");
    for (row = image->data, rows = image->height; rows; row += image->stride, rows--) {
	for (byte = row, cols = (image->width + 7) / 8; cols; byte++, cols--) {
	    unsigned char output_byte = CAIRO_BITSWAP8_IF_LITTLE_ENDIAN (*byte);
	    _cairo_output_stream_write (surface->output, &output_byte, 1);
	}
    }
    _cairo_output_stream_printf (surface->output,
				 "\r\nEI\r\n");

    status = _cairo_pdf_surface_close_stream (surface);

    if (image != scaled_glyph->surface)
	cairo_surface_destroy (&image->base);

    return status;
}

static void
_cairo_pdf_surface_emit_glyph (cairo_pdf_surface_t	*surface,
			       cairo_scaled_font_t	*scaled_font,
			       unsigned long		 glyph_index,
			       cairo_pdf_resource_t	*glyph_ret,
                               cairo_box_t              *bbox,
                               double                   *width)
{
    cairo_status_t status;

    status = _cairo_pdf_surface_emit_outline_glyph (surface,
						    scaled_font,
						    glyph_index,
						    glyph_ret);
    if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	status = _cairo_pdf_surface_emit_bitmap_glyph (surface,
						       scaled_font,
						       glyph_index,
						       glyph_ret,
                                                       bbox,
                                                       width);

    if (status)
	status = _cairo_surface_set_error (&surface->base, status);
}

static cairo_status_t
_cairo_pdf_surface_emit_type3_font_subset (cairo_pdf_surface_t		*surface,
					   cairo_scaled_font_subset_t	*font_subset)
{
    cairo_status_t status;
    cairo_pdf_resource_t *glyphs, encoding, char_procs, subset_resource, to_unicode_stream;
    cairo_pdf_font_t font;
    cairo_matrix_t matrix;
    double *widths;
    unsigned int i;
    cairo_box_t font_bbox = {{0,0},{0,0}};
    cairo_box_t bbox = {{0,0},{0,0}};

    glyphs = _cairo_malloc_ab (font_subset->num_glyphs, sizeof (cairo_pdf_resource_t));
    if (glyphs == NULL)
	return _cairo_surface_set_error (&surface->base, CAIRO_STATUS_NO_MEMORY);

    widths = _cairo_malloc_ab (font_subset->num_glyphs, sizeof (double));
    if (widths == NULL) {
        free (glyphs);
	return _cairo_surface_set_error (&surface->base, CAIRO_STATUS_NO_MEMORY);
    }

    for (i = 0; i < font_subset->num_glyphs; i++) {
	_cairo_pdf_surface_emit_glyph (surface,
				       font_subset->scaled_font,
				       font_subset->glyphs[i],
				       &glyphs[i],
                                       &bbox,
                                       &widths[i]);
        if (i == 0) {
            font_bbox.p1.x = bbox.p1.x;
            font_bbox.p1.y = bbox.p1.y;
            font_bbox.p2.x = bbox.p2.x;
            font_bbox.p2.y = bbox.p2.y;
        } else {
            if (bbox.p1.x < font_bbox.p1.x)
                font_bbox.p1.x = bbox.p1.x;
            if (bbox.p1.y < font_bbox.p1.y)
                font_bbox.p1.y = bbox.p1.y;
            if (bbox.p2.x > font_bbox.p2.x)
                font_bbox.p2.x = bbox.p2.x;
            if (bbox.p2.y > font_bbox.p2.y)
                font_bbox.p2.y = bbox.p2.y;
        }
    }

    encoding = _cairo_pdf_surface_new_object (surface);
    if (encoding.id == 0) {
	free (glyphs);
	free (widths);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /Encoding\r\n"
				 "   /Differences [0", encoding.id);
    for (i = 0; i < font_subset->num_glyphs; i++)
	_cairo_output_stream_printf (surface->output,
				     " /%d", i);
    _cairo_output_stream_printf (surface->output,
				 "]\r\n"
				 ">>\r\n"
				 "endobj\r\n");

    char_procs = _cairo_pdf_surface_new_object (surface);
    if (char_procs.id == 0) {
	free (glyphs);
	free (widths);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<<\r\n", char_procs.id);
    for (i = 0; i < font_subset->num_glyphs; i++)
	_cairo_output_stream_printf (surface->output,
				     " /%d %d 0 R\r\n",
				     i, glyphs[i].id);
    _cairo_output_stream_printf (surface->output,
				 ">>\r\n"
				 "endobj\r\n");

    free (glyphs);

    status = _cairo_pdf_surface_emit_to_unicode_stream (surface,
	                                                font_subset, FALSE,
							&to_unicode_stream);
    if (status && status != CAIRO_INT_STATUS_UNSUPPORTED) {
	free (widths);
	return status;
    }

    subset_resource = _cairo_pdf_surface_get_font_resource (surface,
							    font_subset->font_id,
							    font_subset->subset_id);
    _cairo_pdf_surface_update_object (surface, subset_resource);
    matrix = font_subset->scaled_font->scale;
    status = cairo_matrix_invert (&matrix);
    /* _cairo_scaled_font_init ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);
    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /Font\r\n"
				 "   /Subtype /Type3\r\n"
				 "   /FontBBox [%f %f %f %f]\r\n"
				 "   /FontMatrix [ %f %f %f %f 0 0 ]\r\n"
				 "   /Encoding %d 0 R\r\n"
				 "   /CharProcs %d 0 R\r\n"
				 "   /FirstChar 0\r\n"
				 "   /LastChar %d\r\n",
				 subset_resource.id,
				 _cairo_fixed_to_double (font_bbox.p1.x),
				 _cairo_fixed_to_double (font_bbox.p1.y),
				 _cairo_fixed_to_double (font_bbox.p2.x),
				 _cairo_fixed_to_double (font_bbox.p2.y),
				 matrix.xx,
				 matrix.yx,
				 -matrix.xy,
				 -matrix.yy,
				 encoding.id,
				 char_procs.id,
				 font_subset->num_glyphs - 1);

    _cairo_output_stream_printf (surface->output,
				 "   /Widths [");
    for (i = 0; i < font_subset->num_glyphs; i++)
	_cairo_output_stream_printf (surface->output, " %f", widths[i]);
    _cairo_output_stream_printf (surface->output,
				 "]\r\n");
    free (widths);

    if (to_unicode_stream.id != 0)
        _cairo_output_stream_printf (surface->output,
                                     "    /ToUnicode %d 0 R\r\n",
                                     to_unicode_stream.id);

    _cairo_output_stream_printf (surface->output,
				 ">>\r\n"
				 "endobj\r\n");

    font.font_id = font_subset->font_id;
    font.subset_id = font_subset->subset_id;
    font.subset_resource = subset_resource;
    return _cairo_array_append (&surface->fonts, &font);
}

static cairo_status_t
_cairo_pdf_surface_emit_unscaled_font_subset (cairo_scaled_font_subset_t *font_subset,
                                              void			 *closure)
{
    cairo_pdf_surface_t *surface = closure;
    cairo_status_t status;

    if (font_subset->is_composite) {
        status = _cairo_pdf_surface_emit_cff_font_subset (surface, font_subset);
        if (status != CAIRO_INT_STATUS_UNSUPPORTED)
            return status;

        status = _cairo_pdf_surface_emit_truetype_font_subset (surface, font_subset);
        if (status != CAIRO_INT_STATUS_UNSUPPORTED)
            return status;

        status = _cairo_pdf_surface_emit_cff_fallback_font (surface, font_subset);
        if (status != CAIRO_INT_STATUS_UNSUPPORTED)
            return status;
    } else {
#if CAIRO_HAS_FT_FONT
        status = _cairo_pdf_surface_emit_type1_font_subset (surface, font_subset);
        if (status != CAIRO_INT_STATUS_UNSUPPORTED)
            return status;
#endif

        status = _cairo_pdf_surface_emit_type1_fallback_font (surface, font_subset);
        if (status != CAIRO_INT_STATUS_UNSUPPORTED)
            return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_surface_emit_scaled_font_subset (cairo_scaled_font_subset_t *font_subset,
                                            void		       *closure)
{
    cairo_pdf_surface_t *surface = closure;
    cairo_status_t status;

    status = _cairo_pdf_surface_emit_type3_font_subset (surface, font_subset);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_surface_emit_font_subsets (cairo_pdf_surface_t *surface)
{
    cairo_status_t status;

    status = _cairo_scaled_font_subsets_foreach_unscaled (surface->font_subsets,
                                                          _cairo_pdf_surface_emit_unscaled_font_subset,
                                                          surface);
    if (status)
	goto BAIL;

    status = _cairo_scaled_font_subsets_foreach_scaled (surface->font_subsets,
                                                        _cairo_pdf_surface_emit_scaled_font_subset,
                                                        surface);

BAIL:
    _cairo_scaled_font_subsets_destroy (surface->font_subsets);
    surface->font_subsets = NULL;

    return status;
}

static cairo_pdf_resource_t
_cairo_pdf_surface_write_catalog (cairo_pdf_surface_t *surface)
{
    cairo_pdf_resource_t catalog;

    catalog = _cairo_pdf_surface_new_object (surface);
    if (catalog.id == 0)
	return catalog;

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /Catalog\r\n"
				 "   /Pages %d 0 R\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 catalog.id,
				 surface->pages_resource.id);

    return catalog;
}

static long
_cairo_pdf_surface_write_xref (cairo_pdf_surface_t *surface)
{
    cairo_pdf_object_t *object;
    int num_objects, i;
    long offset;
    char buffer[11];

    num_objects = _cairo_array_num_elements (&surface->objects);

    offset = _cairo_output_stream_get_position (surface->output);
    _cairo_output_stream_printf (surface->output,
				 "xref\r\n"
				 "%d %d\r\n",
				 0, num_objects + 1);

    _cairo_output_stream_printf (surface->output,
				 "0000000000 65535 f\r\n");
    for (i = 0; i < num_objects; i++) {
	object = _cairo_array_index (&surface->objects, i);
	snprintf (buffer, sizeof buffer, "%010ld", object->offset);
	_cairo_output_stream_printf (surface->output,
				     "%s 00000 n\r\n", buffer);
    }

    return offset;
}

static cairo_status_t
_cairo_pdf_surface_emit_clip (cairo_pdf_surface_t  *surface,
			      cairo_path_fixed_t   *path,
			      cairo_fill_rule_t	    fill_rule)
{
    const char *pdf_operator;

    if (path == NULL) {
	_cairo_output_stream_printf (surface->output, "Q q\r\n");
	return CAIRO_STATUS_SUCCESS;
    }

    if (! path->has_current_point) {
	/* construct an empty path */
	_cairo_output_stream_printf (surface->output, "0 0 m ");
    } else {
	pdf_path_info_t info;
	cairo_status_t status;

	info.output = surface->output;
	info.cairo_to_pdf = &surface->cairo_to_pdf;
	info.ctm_inverse = NULL;

	status = _cairo_path_fixed_interpret (path,
					      CAIRO_DIRECTION_FORWARD,
					      _cairo_pdf_path_move_to,
					      _cairo_pdf_path_line_to,
					      _cairo_pdf_path_curve_to,
					      _cairo_pdf_path_close_path,
					      &info);
	if (status)
	    return status;
    }

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	pdf_operator = "W";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	pdf_operator = "W*";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (surface->output,
				 "%s n\r\n",
				 pdf_operator);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_surface_write_page (cairo_pdf_surface_t *surface)
{
    cairo_status_t status;
    cairo_pdf_resource_t page;
    cairo_pdf_resource_t content_group, knockout_group, page_content;
    cairo_bool_t has_fallback_images = FALSE;

    if (_cairo_array_num_elements (&surface->knockout_group) > 0)
	has_fallback_images = TRUE;

    status = _cairo_pdf_surface_open_group (surface);
    if (status)
	return status;

    status = _cairo_pdf_surface_write_group_list (surface, &surface->content_group);
    if (status)
	return status;

    status = _cairo_pdf_surface_close_group (surface, &content_group);
    if (status)
	return status;

    if (has_fallback_images) {
	status = _cairo_pdf_surface_open_knockout_group (surface, &content_group);
	if (status)
	    return status;

	status = _cairo_pdf_surface_write_group_list (surface, &surface->knockout_group);
	if (status)
	    return status;

	status = _cairo_pdf_surface_close_group (surface, &knockout_group);
	if (status)
	    return status;
    }

    page_content = _cairo_pdf_surface_open_stream (surface,
						   FALSE,
						   "   /Type /XObject\r\n"
						   "   /Subtype /Form\r\n"
						   "   /BBox [ 0 0 %f %f ]\r\n"
						   "   /Group <<\r\n"
						   "      /Type /Group\r\n"
						   "      /S /Transparency\r\n"
						   "      /CS /DeviceRGB\r\n"
						   "   >>\r\n",
						   surface->width,
						   surface->height);
    if (page_content.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
				 "/x%d Do\r\n",
				 has_fallback_images ? knockout_group.id : content_group.id);
    status = _cairo_pdf_surface_close_stream (surface);
    if (status)
	return status;

    page = _cairo_pdf_surface_new_object (surface);
    if (page.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /Page\r\n"
				 "   /Parent %d 0 R\r\n"
				 "   /MediaBox [ 0 0 %f %f ]\r\n"
				 "   /Contents [ %d 0 R ]\r\n"
				 "   /Group <<\r\n"
				 "      /Type /Group\r\n"
				 "      /S /Transparency\r\n"
				 "      /CS /DeviceRGB\r\n"
				 "   >>\r\n"
				 "   /Resources <<\r\n"
				 "      /XObject << /x%d %d 0 R >>\r\n"
				 "   >>\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 page.id,
				 surface->pages_resource.id,
				 surface->width,
				 surface->height,
				 page_content.id,
				 has_fallback_images ? knockout_group.id : content_group.id,
				 has_fallback_images ? knockout_group.id : content_group.id);

    status = _cairo_array_append (&surface->pages, &page);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_surface_pattern_supported (cairo_surface_pattern_t *pattern)
{
    cairo_extend_t extend;

    if (_cairo_surface_is_meta (pattern->surface))
	return TRUE;

    if (pattern->surface->backend->acquire_source_image == NULL)
	return FALSE;

    /* Does an ALPHA-only source surface even make sense? Maybe, but I
     * don't think it's worth the extra code to support it. */

/* XXX: Need to write this function here...
    content = cairo_surface_get_content (pattern->surface);
    if (content == CAIRO_CONTENT_ALPHA)
	return FALSE;
*/

    extend = cairo_pattern_get_extend (&pattern->base);
    switch (extend) {
    case CAIRO_EXTEND_NONE:
    case CAIRO_EXTEND_REPEAT:
    case CAIRO_EXTEND_REFLECT:
    /* There's no point returning FALSE for EXTEND_PAD, as the image
     * surface does not currently implement it either */
    case CAIRO_EXTEND_PAD:
	return TRUE;
    }

    ASSERT_NOT_REACHED;
    return FALSE;
}

static cairo_bool_t
_gradient_pattern_supported (cairo_pattern_t *pattern)
{
    cairo_extend_t extend;

    extend = cairo_pattern_get_extend (pattern);


    /* Radial gradients are currently only supported with EXTEND_NONE
     * and EXTEND_PAD and when one circle is inside the other. */
    if (pattern->type == CAIRO_PATTERN_TYPE_RADIAL) {
        double x1, y1, x2, y2, r1, r2, d;
        cairo_radial_pattern_t *radial = (cairo_radial_pattern_t *) pattern;

	if (extend == CAIRO_EXTEND_REPEAT ||
	    extend == CAIRO_EXTEND_REFLECT) {
	    return FALSE;
	}

	x1 = _cairo_fixed_to_double (radial->c1.x);
        y1 = _cairo_fixed_to_double (radial->c1.y);
        r1 = _cairo_fixed_to_double (radial->r1);
        x2 = _cairo_fixed_to_double (radial->c2.x);
        y2 = _cairo_fixed_to_double (radial->c2.y);
        r2 = _cairo_fixed_to_double (radial->r2);

        d = sqrt((x2 - x1)*(x2 - x1) + (y2 - y1)*(y2 - y1));
        if (d > fabs(r2 - r1)) {
            return FALSE;
        }
    }

    return TRUE;
}

static cairo_bool_t
_pattern_supported (cairo_pattern_t *pattern)
{
    if (pattern->type == CAIRO_PATTERN_TYPE_SOLID)
	return TRUE;

    if (pattern->type == CAIRO_PATTERN_TYPE_LINEAR ||
	pattern->type == CAIRO_PATTERN_TYPE_RADIAL)
	return _gradient_pattern_supported (pattern);

    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE)
	return _surface_pattern_supported ((cairo_surface_pattern_t *) pattern);

    return FALSE;
}

static cairo_int_status_t
_cairo_pdf_surface_analyze_operation (cairo_pdf_surface_t  *surface,
				      cairo_operator_t      op,
				      cairo_pattern_t      *pattern)
{
    if (surface->force_fallbacks && surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _pattern_supported (pattern))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (op == CAIRO_OPERATOR_OVER) {
	if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	    cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *) pattern;
	    if ( _cairo_surface_is_meta (surface_pattern->surface))
	    return CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN;
	}
	return CAIRO_STATUS_SUCCESS;
    }

    /* The SOURCE operator is only supported for the fallback images. */
    if (op == CAIRO_OPERATOR_SOURCE &&
	surface->paginated_mode == CAIRO_PAGINATED_MODE_RENDER)
	return CAIRO_STATUS_SUCCESS;

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_bool_t
_cairo_pdf_surface_operation_supported (cairo_pdf_surface_t  *surface,
					cairo_operator_t      op,
					cairo_pattern_t      *pattern)
{
    if (_cairo_pdf_surface_analyze_operation (surface, op, pattern) != CAIRO_INT_STATUS_UNSUPPORTED)
	return TRUE;
    else
	return FALSE;
}

static cairo_int_status_t
_cairo_pdf_surface_set_operator (cairo_pdf_surface_t *surface,
				 cairo_operator_t op)
{
    cairo_status_t status;

    if (op == CAIRO_OPERATOR_OVER)
	return CAIRO_STATUS_SUCCESS;

    if (op == CAIRO_OPERATOR_SOURCE) {
	surface->current_group = &surface->knockout_group;
	status = _cairo_pdf_surface_stop_content_stream (surface);
	if (status)
	    return status;

	status = _cairo_pdf_surface_start_content_stream (surface);
	if (status)
	    return status;

	return CAIRO_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_pdf_surface_paint (void			*abstract_surface,
			  cairo_operator_t	 op,
			  cairo_pattern_t	*source)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_resource_t smask_group = {0}; /* squelch bogus compiler warning */
    cairo_status_t status;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_pdf_surface_analyze_operation (surface, op, source);

    assert (_cairo_pdf_surface_operation_supported (surface, op, source));

    status = _cairo_pdf_surface_emit_pattern (surface, source);
    if (status)
	return status;

    status = _cairo_pdf_surface_set_operator (surface, op);
    if (status)
	return status;

    if (surface->emitted_pattern.smask.id != 0) {
	_cairo_pdf_surface_pause_content_stream (surface);
	status = _cairo_pdf_surface_open_group (surface);
	if (status)
	    return status;
    } else {
	_cairo_output_stream_printf (surface->output, "q ");
    }

    status = _cairo_pdf_surface_select_pattern (surface, FALSE);
    if (status)
	return status;

    _cairo_output_stream_printf (surface->output,
				 "0 0 %f %f re f\r\n",
				 surface->width, surface->height);

    if (surface->emitted_pattern.smask.id != 0) {
	status = _cairo_pdf_surface_close_group (surface, &smask_group);
	if (status)
	    return status;

	_cairo_pdf_surface_resume_content_stream (surface);
	_cairo_output_stream_printf (surface->output,
				     "q /s%d gs /x%d Do Q\r\n",
				     surface->emitted_pattern.smask,
				     smask_group.id);
	status = _cairo_pdf_surface_add_smask (surface, surface->emitted_pattern.smask);
	if (status)
	    return status;
	status = _cairo_pdf_surface_add_xobject (surface, smask_group);
	if (status)
	    return status;
    } else {
	_cairo_output_stream_printf (surface->output, "Q\r\n");
    }

    status = _cairo_output_stream_get_status (surface->output);
    if (status)
        return status;

    return _cairo_pdf_surface_check_content_stream_size (surface);
}

static cairo_int_status_t
_cairo_pdf_surface_mask	(void			*abstract_surface,
			 cairo_operator_t	 op,
			 cairo_pattern_t	*source,
			 cairo_pattern_t	*mask)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_resource_t mask_group;
    cairo_pdf_resource_t group = {0}; /* squelch bogus compiler warning */
    cairo_pdf_resource_t source_group;
    cairo_pdf_resource_t smask;
    cairo_pdf_resource_t gstate;
    cairo_status_t status, status2;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	status = _cairo_pdf_surface_analyze_operation (surface, op, source);
	if (status != CAIRO_STATUS_SUCCESS &&
	    status != CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN)
	    return status;

	status2 = _cairo_pdf_surface_analyze_operation (surface, op, mask);
	if (status2 != CAIRO_STATUS_SUCCESS)
	    return status2;

	return status;
    }

    assert (_cairo_pdf_surface_operation_supported (surface, op, source));
    assert (_cairo_pdf_surface_operation_supported (surface, op, mask));

    status = _cairo_pdf_surface_set_operator (surface, op);
    if (status)
	return status;

    /* Create mask group */
    status = _cairo_pdf_surface_emit_pattern (surface, mask);
    if (status)
	return status;

    _cairo_pdf_surface_pause_content_stream (surface);

    status = _cairo_pdf_surface_open_group (surface);
    if (status)
	return status;

    status = _cairo_pdf_surface_select_pattern (surface, FALSE);
    if (status)
	return status;

    _cairo_output_stream_printf (surface->output,
				 "0 0 %f %f re f\r\n",
				 surface->width, surface->height);
    status = _cairo_pdf_surface_close_group (surface, &mask_group);
    if (status)
	return status;

    if (surface->emitted_pattern.smask.id != 0) {
	group = mask_group;
	status = _cairo_pdf_surface_open_group (surface);
	if (status)
	    return status;

	_cairo_output_stream_printf (surface->output,
				     "/s%d gs /x%d Do\r\n",
				     surface->emitted_pattern.smask,
				     group.id);
	status = _cairo_pdf_surface_add_smask (surface, surface->emitted_pattern.smask);
	if (status)
	    return status;
	status = _cairo_pdf_surface_add_xobject (surface, group);
	if (status)
	    return status;

	status = _cairo_pdf_surface_close_group (surface, &mask_group);
	if (status)
	    return status;
    }

    /* Create source group */
    status = _cairo_pdf_surface_emit_pattern (surface, source);
    if (status)
	return status;

    _cairo_pdf_surface_pause_content_stream (surface);

    status = _cairo_pdf_surface_open_group (surface);
    if (status)
	return status;

    status = _cairo_pdf_surface_select_pattern (surface, FALSE);
    if (status)
	return status;

    _cairo_output_stream_printf (surface->output,
				 "0 0 %f %f re f\r\n",
				 surface->width, surface->height);
    status = _cairo_pdf_surface_close_group (surface, &source_group);
    if (status)
	return status;

    if (surface->emitted_pattern.smask.id != 0) {
	group = source_group;
	status = _cairo_pdf_surface_open_group (surface);
	if (status)
	    return status;

	_cairo_output_stream_printf (surface->output,
				     "/s%d gs /x%d Do\r\n",
				     surface->emitted_pattern.smask,
				     group.id);
	status = _cairo_pdf_surface_add_smask (surface, surface->emitted_pattern.smask);
	if (status)
	    return status;
	status = _cairo_pdf_surface_add_xobject (surface, group);
	if (status)
	    return status;

	status =_cairo_pdf_surface_close_group (surface, &source_group);
	if (status)
	    return status;
    }

    /* Create an smask based on the alpha component of mask_group */
    smask = _cairo_pdf_surface_new_object (surface);
    if (smask.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /Mask\r\n"
				 "   /S /Alpha\r\n"
				 "   /G %d 0 R\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 smask.id,
				 mask_group.id);

    /* Create a GState that uses the smask */
    gstate = _cairo_pdf_surface_new_object (surface);
    if (gstate.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /ExtGState\r\n"
				 "   /SMask %d 0 R\r\n"
				 "   /ca 1\r\n"
				 "   /CA 1\r\n"
				 "   /AIS false\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 gstate.id,
				 smask.id);

    /* Select the GState then draw the source */
    _cairo_pdf_surface_resume_content_stream (surface);
    _cairo_output_stream_printf (surface->output,
				 "q /s%d gs /x%d Do Q\r\n",
				 gstate.id,
				 source_group.id);
    status = _cairo_pdf_surface_add_smask (surface, gstate);
    if (status)
	return status;
    status = _cairo_pdf_surface_add_xobject (surface, source_group);
    if (status)
	return status;

    return _cairo_output_stream_get_status (surface->output);
}

static int
_cairo_pdf_line_cap (cairo_line_cap_t cap)
{
    switch (cap) {
    case CAIRO_LINE_CAP_BUTT:
	return 0;
    case CAIRO_LINE_CAP_ROUND:
	return 1;
    case CAIRO_LINE_CAP_SQUARE:
	return 2;
    default:
	ASSERT_NOT_REACHED;
	return 0;
    }
}

static int
_cairo_pdf_line_join (cairo_line_join_t join)
{
    switch (join) {
    case CAIRO_LINE_JOIN_MITER:
	return 0;
    case CAIRO_LINE_JOIN_ROUND:
	return 1;
    case CAIRO_LINE_JOIN_BEVEL:
	return 2;
    default:
	ASSERT_NOT_REACHED;
	return 0;
    }
}

static cairo_status_t
_cairo_pdf_surface_emit_stroke_style (cairo_pdf_surface_t	*surface,
				      cairo_stroke_style_t	*style)
{
    _cairo_output_stream_printf (surface->output,
				 "%f w\r\n",
				 style->line_width);

    _cairo_output_stream_printf (surface->output,
				 "%d J\r\n",
				 _cairo_pdf_line_cap (style->line_cap));

    _cairo_output_stream_printf (surface->output,
				 "%d j\r\n",
				 _cairo_pdf_line_join (style->line_join));

    if (style->num_dashes) {
	unsigned int d;
	_cairo_output_stream_printf (surface->output, "[");
	for (d = 0; d < style->num_dashes; d++)
	    _cairo_output_stream_printf (surface->output, " %f", style->dash[d]);
	_cairo_output_stream_printf (surface->output, "] %f d\r\n",
				     style->dash_offset);
    } else {
	_cairo_output_stream_printf (surface->output, "[] 0.0 d\r\n");
    }

    _cairo_output_stream_printf (surface->output,
				 "%f M ",
				 style->miter_limit);

    return _cairo_output_stream_get_status (surface->output);
}

static cairo_int_status_t
_cairo_pdf_surface_stroke (void			*abstract_surface,
			   cairo_operator_t	 op,
			   cairo_pattern_t	*source,
			   cairo_path_fixed_t	*path,
			   cairo_stroke_style_t	*style,
			   cairo_matrix_t	*ctm,
			   cairo_matrix_t	*ctm_inverse,
			   double		 tolerance,
			   cairo_antialias_t	 antialias)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_resource_t smask_group = {0}; /* squelch bogus compiler warning */
    pdf_path_info_t info;
    cairo_status_t status;
    cairo_matrix_t m;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_pdf_surface_analyze_operation (surface, op, source);

    assert (_cairo_pdf_surface_operation_supported (surface, op, source));

    status = _cairo_pdf_surface_emit_pattern (surface, source);
    if (status)
	return status;

    if (surface->emitted_pattern.smask.id != 0) {
	_cairo_pdf_surface_pause_content_stream (surface);
	status = _cairo_pdf_surface_open_group (surface);
	if (status)
	    return status;
    } else {
	_cairo_output_stream_printf (surface->output, "q ");
    }

    status = _cairo_pdf_surface_select_pattern (surface, TRUE);
    if (status)
	return status;

    status = _cairo_pdf_surface_emit_stroke_style (surface,
						   style);
    if (status)
	return status;

    info.output = surface->output;
    info.cairo_to_pdf = NULL;
    info.ctm_inverse = ctm_inverse;

    cairo_matrix_multiply (&m, ctm, &surface->cairo_to_pdf);
    _cairo_output_stream_printf (surface->output,
				 "q %f %f %f %f %f %f cm\r\n",
				 m.xx, m.yx, m.xy, m.yy,
				 m.x0, m.y0);

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_pdf_path_move_to,
					  _cairo_pdf_path_line_to,
					  _cairo_pdf_path_curve_to,
					  _cairo_pdf_path_close_path,
					  &info);
    if (status)
	return status;

    _cairo_output_stream_printf (surface->output, "S Q\r\n");

    if (surface->emitted_pattern.smask.id != 0) {
	status = _cairo_pdf_surface_close_group (surface, &smask_group);
	if (status)
	    return status;

	_cairo_pdf_surface_resume_content_stream (surface);
	_cairo_output_stream_printf (surface->output,
				     "q /s%d gs /x%d Do Q\r\n",
				     surface->emitted_pattern.smask,
				     smask_group.id);
	status = _cairo_pdf_surface_add_smask (surface, surface->emitted_pattern.smask);
	if (status)
	    return status;
	status = _cairo_pdf_surface_add_xobject (surface, smask_group);
	if (status)
	    return status;
    } else {
	_cairo_output_stream_printf (surface->output, "Q\r\n");
    }

    status = _cairo_output_stream_get_status (surface->output);
    if (status)
	return status;

    return _cairo_pdf_surface_check_content_stream_size (surface);
}

static cairo_int_status_t
_cairo_pdf_surface_fill (void			*abstract_surface,
			 cairo_operator_t	 op,
			 cairo_pattern_t	*source,
			 cairo_path_fixed_t	*path,
			 cairo_fill_rule_t	 fill_rule,
			 double			 tolerance,
			 cairo_antialias_t	 antialias)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_resource_t smask_group = {0}; /* squelch bogus compiler warning */
    const char *pdf_operator;
    cairo_status_t status;
    pdf_path_info_t info;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_pdf_surface_analyze_operation (surface, op, source);

    assert (_cairo_pdf_surface_operation_supported (surface, op, source));

    status = _cairo_pdf_surface_set_operator (surface, op);
    if (status)
	return status;

    status = _cairo_pdf_surface_emit_pattern (surface, source);
    if (status)
	return status;

    if (surface->emitted_pattern.smask.id != 0) {
	_cairo_pdf_surface_pause_content_stream (surface);
	status = _cairo_pdf_surface_open_group (surface);
	if (status)
	    return status;
    } else {
	_cairo_output_stream_printf (surface->output, "q ");
    }

    status = _cairo_pdf_surface_select_pattern (surface, FALSE);
    if (status)
	return status;

    info.output = surface->output;
    info.cairo_to_pdf = &surface->cairo_to_pdf;
    info.ctm_inverse = NULL;
    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_pdf_path_move_to,
					  _cairo_pdf_path_line_to,
					  _cairo_pdf_path_curve_to,
					  _cairo_pdf_path_close_path,
					  &info);
    if (status)
	return status;

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	pdf_operator = "f";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	pdf_operator = "f*";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (surface->output,
				 "%s\r\n",
				 pdf_operator);

    if (surface->emitted_pattern.smask.id != 0) {
	status = _cairo_pdf_surface_close_group (surface, &smask_group);
	if (status)
	    return status;

	_cairo_pdf_surface_resume_content_stream (surface);
	_cairo_output_stream_printf (surface->output,
				     "q /s%d gs /x%d Do Q\r\n",
				     surface->emitted_pattern.smask,
				     smask_group.id);
	status = _cairo_pdf_surface_add_smask (surface, surface->emitted_pattern.smask);
	if (status)
	    return status;
	status = _cairo_pdf_surface_add_xobject (surface, smask_group);
	if (status)
	    return status;
    } else {
	_cairo_output_stream_printf (surface->output, "Q\r\n");
    }

    status = _cairo_output_stream_get_status (surface->output);
    if (status)
	return status;

    return _cairo_pdf_surface_check_content_stream_size (surface);
}

#define GLYPH_POSITION_TOLERANCE 0.001

static cairo_int_status_t
_cairo_pdf_surface_show_glyphs (void			*abstract_surface,
				cairo_operator_t	 op,
				cairo_pattern_t		*source,
				cairo_glyph_t		*glyphs,
				int			 num_glyphs,
				cairo_scaled_font_t	*scaled_font)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_resource_t smask_group = {0}; /* squelch bogus compiler warning */
    unsigned int current_subset_id = (unsigned int)-1;
    cairo_scaled_font_subsets_glyph_t subset_glyph;
    cairo_bool_t diagonal, in_TJ;
    cairo_status_t status;
    double Tlm_x = 0, Tlm_y = 0;
    double Tm_x = 0, y;
    int i, hex_width;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_pdf_surface_analyze_operation (surface, op, source);

    assert (_cairo_pdf_surface_operation_supported (surface, op, source));

    status = _cairo_pdf_surface_emit_pattern (surface, source);
    if (status)
	return status;

    if (surface->emitted_pattern.smask.id != 0) {
	_cairo_pdf_surface_pause_content_stream (surface);
	status = _cairo_pdf_surface_open_group (surface);
	if (status)
	    return status;
    } else {
	_cairo_output_stream_printf (surface->output, "q ");
    }

    status = _cairo_pdf_surface_select_pattern (surface, FALSE);
    if (status)
	return status;

    _cairo_output_stream_printf (surface->output,
				 "BT\r\n");

    if (scaled_font->scale.xy == 0.0 &&
        scaled_font->scale.yx == 0.0)
        diagonal = TRUE;
    else
        diagonal = FALSE;

    in_TJ = FALSE;
    for (i = 0; i < num_glyphs; i++) {
        status = _cairo_scaled_font_subsets_map_glyph (surface->font_subsets,
                                                       scaled_font, glyphs[i].index,
                                                       &subset_glyph);
	if (status)
            return status;

        if (subset_glyph.is_composite)
            hex_width = 4;
        else
            hex_width = 2;

        if (subset_glyph.is_scaled == FALSE) {
            y = 0.0;
            cairo_matrix_transform_distance (&scaled_font->scale,
                                             &subset_glyph.x_advance,
                                             &y);
        }

	if (subset_glyph.subset_id != current_subset_id) {
            if (in_TJ) {
                _cairo_output_stream_printf (surface->output, ">] TJ\r\n");
                in_TJ = FALSE;
            }
	    _cairo_output_stream_printf (surface->output,
					 "/f-%d-%d 1 Tf\r\n",
					 subset_glyph.font_id,
					 subset_glyph.subset_id);
	    status = _cairo_pdf_surface_add_font (surface,
					          subset_glyph.font_id,
						  subset_glyph.subset_id);
	    if (status)
		return status;
        }

        if (subset_glyph.subset_id != current_subset_id || !diagonal) {
            _cairo_output_stream_printf (surface->output,
                                         "%f %f %f %f %f %f Tm\r\n",
                                         scaled_font->scale.xx,
                                         -scaled_font->scale.yx,
                                         -scaled_font->scale.xy,
                                         scaled_font->scale.yy,
                                         glyphs[i].x,
                                         surface->height - glyphs[i].y);
            current_subset_id = subset_glyph.subset_id;
            Tlm_x = glyphs[i].x;
            Tlm_y = glyphs[i].y;
            Tm_x = Tlm_x;
        }

        if (diagonal) {
            if (i < num_glyphs - 1 &&
                fabs((glyphs[i].y - glyphs[i+1].y)/scaled_font->scale.yy) < GLYPH_POSITION_TOLERANCE &&
                fabs((glyphs[i].x - glyphs[i+1].x)/scaled_font->scale.xx) < 10)
            {
                if (!in_TJ) {
                    if (i != 0) {
                        _cairo_output_stream_printf (surface->output,
                                                     "%f %f Td\r\n",
                                                     (glyphs[i].x - Tlm_x)/scaled_font->scale.xx,
                                                     -(glyphs[i].y - Tlm_y)/scaled_font->scale.yy);

                        Tlm_x = glyphs[i].x;
                        Tlm_y = glyphs[i].y;
                        Tm_x = Tlm_x;
                    }
                    _cairo_output_stream_printf (surface->output,
                                                 "[<%0*x",
                                                 hex_width,
                                                 subset_glyph.subset_glyph_index);
                    Tm_x += subset_glyph.x_advance;
                    in_TJ = TRUE;
                } else {
                    if (fabs((glyphs[i].x - Tm_x)/scaled_font->scale.xx) > GLYPH_POSITION_TOLERANCE) {
                        double delta = glyphs[i].x - Tm_x;

                        _cairo_output_stream_printf (surface->output,
                                                     "> %f <",
                                                     -1000.0*delta/scaled_font->scale.xx);
                        Tm_x += delta;
                    }
                    _cairo_output_stream_printf (surface->output,
                                                 "%0*x",
                                                 hex_width,
                                                 subset_glyph.subset_glyph_index);
                    Tm_x += subset_glyph.x_advance;
                }
            }
            else
            {
                if (in_TJ) {
                    if (fabs((glyphs[i].x - Tm_x)/scaled_font->scale.xx) > GLYPH_POSITION_TOLERANCE) {
                        double delta = glyphs[i].x - Tm_x;

                        _cairo_output_stream_printf (surface->output,
                                                     "> %f <",
                                                     -1000.0*delta/scaled_font->scale.xx);
                        Tm_x += delta;
                    }
                    _cairo_output_stream_printf (surface->output,
                                                 "%0*x>] TJ\r\n",
                                                 hex_width,
                                                 subset_glyph.subset_glyph_index);
                    Tm_x += subset_glyph.x_advance;
                    in_TJ = FALSE;
                } else {
                    if (i != 0) {
                        _cairo_output_stream_printf (surface->output,
                                                     "%f %f Td ",
                                                     (glyphs[i].x - Tlm_x)/scaled_font->scale.xx,
                                                     (glyphs[i].y - Tlm_y)/-scaled_font->scale.yy);
                        Tlm_x = glyphs[i].x;
                        Tlm_y = glyphs[i].y;
                        Tm_x = Tlm_x;
                    }
                    _cairo_output_stream_printf (surface->output,
                                                 "<%0*x> Tj ",
                                                 hex_width,
                                                 subset_glyph.subset_glyph_index);
                    Tm_x += subset_glyph.x_advance;
                }
            }
        } else {
            _cairo_output_stream_printf (surface->output,
                                         "<%0*x> Tj\r\n",
                                         hex_width,
                                         subset_glyph.subset_glyph_index);
        }
    }

    _cairo_output_stream_printf (surface->output,
				 "ET\r\n");

    if (surface->emitted_pattern.smask.id != 0) {
	status = _cairo_pdf_surface_close_group (surface, &smask_group);
	if (status)
	    return status;

	_cairo_pdf_surface_resume_content_stream (surface);

	_cairo_output_stream_printf (surface->output,
				     "q /s%d gs /x%d Do Q\r\n",
				     surface->emitted_pattern.smask,
				     smask_group.id);
	status = _cairo_pdf_surface_add_smask (surface, surface->emitted_pattern.smask);
	if (status)
	    return status;
	status = _cairo_pdf_surface_add_xobject (surface, smask_group);
	if (status)
	    return status;
    } else {
	_cairo_output_stream_printf (surface->output, "Q\r\n");
    }

    status = _cairo_output_stream_get_status (surface->output);
    if (status)
	return status;

    return _cairo_pdf_surface_check_content_stream_size (surface);
}

static void
_cairo_pdf_surface_set_paginated_mode (void			*abstract_surface,
				       cairo_paginated_mode_t	 paginated_mode)
{
    cairo_pdf_surface_t *surface = abstract_surface;

    surface->paginated_mode = paginated_mode;
}

static const cairo_surface_backend_t cairo_pdf_surface_backend = {
    CAIRO_SURFACE_TYPE_PDF,
    _cairo_pdf_surface_create_similar,
    _cairo_pdf_surface_finish,
    NULL, /* acquire_source_image */
    NULL, /* release_source_image */
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    _cairo_pdf_surface_copy_page,
    _cairo_pdf_surface_show_page,
    NULL, /* set_clip_region */
    _cairo_pdf_surface_intersect_clip_path,
    _cairo_pdf_surface_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_pdf_surface_get_font_options,
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    /* Here are the drawing functions */

    _cairo_pdf_surface_paint,
    _cairo_pdf_surface_mask,
    _cairo_pdf_surface_stroke,
    _cairo_pdf_surface_fill,
    _cairo_pdf_surface_show_glyphs,
    NULL, /* snapshot */

    NULL, /* is_compatible */
    NULL, /* reset */
};

static const cairo_paginated_surface_backend_t cairo_pdf_surface_paginated_backend = {
    _cairo_pdf_surface_start_page,
    _cairo_pdf_surface_set_paginated_mode
};

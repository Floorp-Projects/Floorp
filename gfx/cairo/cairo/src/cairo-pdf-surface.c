/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
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
 *	Carl Worth <cworth@cworth.org>
 */

#include "cairoint.h"
#include "cairo-pdf.h"
#include "cairo-pdf-surface-private.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-paginated-private.h"
#include "cairo-path-fixed-private.h"
#include "cairo-output-stream-private.h"

#include <time.h>
#include <zlib.h>

/* Issues:
 *
 * - Why doesn't pages inherit /alpha%d GS dictionaries from the Pages
 *   object?
 *
 * - We embed an image in the stream each time it's composited.  We
 *   could add generation counters to surfaces and remember the stream
 *   ID for a particular generation for a particular surface.
 *
 * - Clipping: must be able to reset clipping
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
 * - Output TM so page scales to the right size - PDF default user
 *   space has 1 unit = 1 / 72 inch.
 *
 * - Add test case for RGBA images.
 *
 * - Add test case for RGBA gradients.
 *
 * - Coordinate space for create_similar() args?
 *
 * - Investigate /Matrix entry in content stream dicts for pages
 *   instead of outputting the cm operator in every page.
 */

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

static cairo_pdf_resource_t
_cairo_pdf_surface_new_object (cairo_pdf_surface_t *surface);

static void
_cairo_pdf_surface_clear (cairo_pdf_surface_t *surface);

static cairo_pdf_resource_t
_cairo_pdf_surface_open_stream (cairo_pdf_surface_t	*surface,
                                cairo_bool_t             compressed,
				const char		*fmt,
				...) CAIRO_PRINTF_FORMAT(3, 4);
static cairo_status_t
_cairo_pdf_surface_close_stream (cairo_pdf_surface_t	*surface);

static cairo_status_t
_cairo_pdf_surface_add_stream (cairo_pdf_surface_t	*surface,
			       cairo_pdf_resource_t	 stream);

static void
_cairo_pdf_surface_write_pages (cairo_pdf_surface_t *surface);

static cairo_pdf_resource_t
_cairo_pdf_surface_write_info (cairo_pdf_surface_t *surface);

static cairo_pdf_resource_t
_cairo_pdf_surface_write_catalog (cairo_pdf_surface_t *surface);

static long
_cairo_pdf_surface_write_xref (cairo_pdf_surface_t *surface);

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

static cairo_status_t
_cairo_pdf_surface_add_stream (cairo_pdf_surface_t	*surface,
			       cairo_pdf_resource_t	 stream)
{
    return _cairo_array_append (&surface->streams, &stream);
}

static cairo_status_t
_cairo_pdf_surface_add_pattern (cairo_pdf_surface_t	*surface,
				cairo_pdf_resource_t	 pattern)
{
    return _cairo_array_append (&surface->patterns, &pattern);
}

static cairo_status_t
_cairo_pdf_surface_add_smask (cairo_pdf_surface_t  *surface,
                              cairo_pdf_resource_t  smask)
{
    return _cairo_array_append (&surface->smasks, &smask);
}

static cairo_status_t
_cairo_pdf_surface_add_alpha (cairo_pdf_surface_t *surface, double alpha, int *index)
{
    int num_alphas, i;
    double other;
    cairo_status_t status;

    num_alphas = _cairo_array_num_elements (&surface->alphas);
    for (i = 0; i < num_alphas; i++) {
        _cairo_array_copy_element (&surface->alphas, i, &other);
        if (alpha == other) {
            *index = i;
            return CAIRO_STATUS_SUCCESS;
        }
    }

    status = _cairo_array_append (&surface->alphas, &alpha);
    if (status)
	return status;

    *index = _cairo_array_num_elements (&surface->alphas) - 1;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
_cairo_pdf_surface_create_for_stream_internal (cairo_output_stream_t	*output,
					       double			 width,
					       double			 height)
{
    cairo_pdf_surface_t *surface;
    cairo_status_t status;
    int alpha;

    surface = malloc (sizeof (cairo_pdf_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
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
    _cairo_array_init (&surface->patterns, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&surface->xobjects, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&surface->streams, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&surface->alphas, sizeof (double));
    _cairo_array_init (&surface->smasks, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&surface->rgb_linear_functions, sizeof (cairo_pdf_rgb_linear_function_t));
    _cairo_array_init (&surface->alpha_linear_functions, sizeof (cairo_pdf_alpha_linear_function_t));


    /* Add alpha=1 as the first element in the list of alpha values as
     * this is the most frequently referenced value. */
    status = _cairo_pdf_surface_add_alpha (surface, 1, &alpha);
    if (status) {
	_cairo_error (status);
        goto fail1;
    }

    surface->font_subsets = _cairo_scaled_font_subsets_create_composite ();
    if (! surface->font_subsets) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
        goto fail2;
    }

    _cairo_array_init (&surface->fonts, sizeof (cairo_pdf_font_t));

    surface->next_available_resource.id = 1;
    surface->pages_resource = _cairo_pdf_surface_new_object (surface);

    surface->current_stream.active = FALSE;

    surface->has_clip = FALSE;

    surface->paginated_mode = CAIRO_PAGINATED_MODE_ANALYZE;

    surface->force_fallbacks = FALSE;

    /* Document header */
    _cairo_output_stream_printf (surface->output,
				 "%%PDF-1.4\r\n");
    _cairo_output_stream_printf (surface->output,
				 "%%%c%c%c%c\r\n", 181, 237, 174, 251);

    return _cairo_paginated_surface_create (&surface->base,
					    CAIRO_CONTENT_COLOR_ALPHA,
					    width, height,
					    &cairo_pdf_surface_paginated_backend);

fail1:
    free (surface);
fail2:
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
    if (status) {
	_cairo_error (status);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

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
    if (status) {
	_cairo_error (status);
	return (status == CAIRO_STATUS_WRITE_ERROR) ?
		(cairo_surface_t*) &_cairo_surface_nil_write_error :
		(cairo_surface_t*) &_cairo_surface_nil;
    }

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
	return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;

    target = _cairo_paginated_surface_get_target (surface);

    if (! _cairo_surface_is_pdf (target))
	return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;

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
    cairo_pdf_surface_t *pdf_surface;
    cairo_status_t status;

    status = _extract_pdf_surface (surface, &pdf_surface);
    if (status) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    pdf_surface->width = width_in_points;
    pdf_surface->height = height_in_points;
}

static void
_cairo_pdf_surface_clear (cairo_pdf_surface_t *surface)
{
    _cairo_array_truncate (&surface->streams, 0);
}

static cairo_pdf_resource_t
_cairo_pdf_surface_open_stream (cairo_pdf_surface_t	*surface,
                                cairo_bool_t             compressed,
				const char		*fmt,
				...)
{
    va_list ap;

    surface->current_stream.active = TRUE;
    surface->current_stream.self = _cairo_pdf_surface_new_object (surface);
    surface->current_stream.length = _cairo_pdf_surface_new_object (surface);
    surface->current_stream.compressed = compressed;

    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Length %d 0 R\r\n",
				 surface->current_stream.self.id,
				 surface->current_stream.length.id);
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

    surface->current_stream.start_offset = _cairo_output_stream_get_position (surface->output);

    if (compressed) {
        surface->current_stream.old_output = surface->output;
        surface->output = _cairo_deflate_stream_create (surface->output);
    }

    return surface->current_stream.self;
}

static cairo_status_t
_cairo_pdf_surface_close_stream (cairo_pdf_surface_t *surface)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    long length;

    if (! surface->current_stream.active)
	return CAIRO_STATUS_SUCCESS;

    if (surface->current_stream.compressed) {
        status = _cairo_output_stream_destroy (surface->output);
        surface->output = surface->current_stream.old_output;
        _cairo_output_stream_printf (surface->output,
                                     "\r\n");
    }

    length = _cairo_output_stream_get_position (surface->output) -
	surface->current_stream.start_offset;
    _cairo_output_stream_printf (surface->output,
				 "endstream\r\n"
				 "endobj\r\n");

    _cairo_pdf_surface_update_object (surface,
				      surface->current_stream.length);
    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "   %ld\r\n"
				 "endobj\r\n",
				 surface->current_stream.length.id,
				 length);

    surface->current_stream.active = FALSE;

    return status;
}

static cairo_status_t
_cairo_pdf_surface_finish (void *abstract_surface)
{
    cairo_status_t status, status2;
    cairo_pdf_surface_t *surface = abstract_surface;
    long offset;
    cairo_pdf_resource_t info, catalog;

    status = _cairo_pdf_surface_close_stream (surface);

    _cairo_pdf_surface_emit_font_subsets (surface);

    _cairo_pdf_surface_write_pages (surface);

    info = _cairo_pdf_surface_write_info (surface);
    catalog = _cairo_pdf_surface_write_catalog (surface);
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
    _cairo_array_fini (&surface->patterns);
    _cairo_array_fini (&surface->xobjects);
    _cairo_array_fini (&surface->streams);
    _cairo_array_fini (&surface->alphas);
    _cairo_array_fini (&surface->smasks);
    _cairo_array_fini (&surface->rgb_linear_functions);
    _cairo_array_fini (&surface->alpha_linear_functions);

    if (surface->font_subsets) {
	_cairo_scaled_font_subsets_destroy (surface->font_subsets);
	surface->font_subsets = NULL;
    }

    _cairo_array_fini (&surface->fonts);

    return status;
}

static cairo_status_t
_cairo_pdf_surface_pause_content_stream (cairo_pdf_surface_t *surface)
{
    return _cairo_pdf_surface_close_stream (surface);
}

static cairo_status_t
_cairo_pdf_surface_resume_content_stream (cairo_pdf_surface_t *surface)
{
    cairo_pdf_resource_t stream;

    stream = _cairo_pdf_surface_open_stream (surface,
                                             TRUE,
					     "   /Type /XObject\r\n"
					     "   /Subtype /Form\r\n"
					     "   /BBox [ 0 0 %f %f ]\r\n",
					     surface->width,
					     surface->height);

    return _cairo_pdf_surface_add_stream (surface, stream);
}

static cairo_status_t
_cairo_pdf_surface_begin_group (cairo_pdf_surface_t *surface, cairo_pdf_resource_t *group)
{
    cairo_pdf_resource_t g;
    cairo_status_t status;

    _cairo_pdf_surface_pause_content_stream (surface);
    g = _cairo_pdf_surface_open_stream (surface,
                                        TRUE,
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

    status = _cairo_array_append (&surface->xobjects, &g);
    *group = g;

    return status;
}

static void
_cairo_pdf_surface_end_group (cairo_pdf_surface_t *surface)
{
    _cairo_pdf_surface_close_stream (surface);
    _cairo_pdf_surface_resume_content_stream (surface);
}

static cairo_int_status_t
_cairo_pdf_surface_start_page (void *abstract_surface)
{
    cairo_status_t status;
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_resource_t stream;

    stream = _cairo_pdf_surface_open_stream (surface,
                                             TRUE,
					     "   /Type /XObject\r\n"
					     "   /Subtype /Form\r\n"
					     "   /BBox [ 0 0 %f %f ]\r\n",
					     surface->width,
					     surface->height);

    status = _cairo_pdf_surface_add_stream (surface, stream);
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
    if (compressed == NULL)
	return NULL;

    compress (compressed, compressed_size, data, data_size);

    return compressed;
}

/* Emit alpha channel from the image into the given data, providing
 * an id that can be used to reference the resulting SMask object.
 *
 * In the case that the alpha channel happens to be all opaque, then
 * no SMask object will be emitted and *id_ret will be set to 0.
 */
static cairo_status_t
_cairo_pdf_surface_emit_smask (cairo_pdf_surface_t		*surface,
	    cairo_image_surface_t	*image,
	    cairo_pdf_resource_t	*stream_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    char *alpha, *alpha_compressed;
    unsigned long alpha_size, alpha_compressed_size;
    uint32_t *pixel;
    int i, x, y;
    cairo_bool_t opaque;
    uint8_t a;

    /* This is the only image format we support, which simplfies things. */
    assert (image->format == CAIRO_FORMAT_ARGB32);

    alpha_size = image->height * image->width;
    alpha = malloc (alpha_size);
    if (alpha == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto CLEANUP;
    }

    opaque = TRUE;
    i = 0;
    for (y = 0; y < image->height; y++) {
	pixel = (uint32_t *) (image->data + y * image->stride);

	for (x = 0; x < image->width; x++, pixel++) {
	    a = (*pixel & 0xff000000) >> 24;
	    alpha[i++] = a;
	    if (a != 0xff)
		opaque = FALSE;
	}
    }

    /* Bail out without emitting smask if it's all opaque. */
    if (opaque) {
	stream_ret->id = 0;
	goto CLEANUP_ALPHA;
    }

    alpha_compressed = compress_dup (alpha, alpha_size, &alpha_compressed_size);
    if (alpha_compressed == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto CLEANUP_ALPHA;
    }

    *stream_ret = _cairo_pdf_surface_open_stream (surface,
                                                  FALSE,
						  "   /Type /XObject\r\n"
						  "   /Subtype /Image\r\n"
						  "   /Width %d\r\n"
						  "   /Height %d\r\n"
						  "   /ColorSpace /DeviceGray\r\n"
						  "   /BitsPerComponent 8\r\n"
						  "   /Filter /FlateDecode\r\n",
						  image->width, image->height);
    _cairo_output_stream_write (surface->output, alpha_compressed, alpha_compressed_size);
    _cairo_output_stream_printf (surface->output, "\r\n");
    status = _cairo_pdf_surface_close_stream (surface);

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
    assert (image->format == CAIRO_FORMAT_RGB24 || image->format == CAIRO_FORMAT_ARGB32);

    rgb_size = image->height * image->width * 3;
    rgb = malloc (rgb_size);
    if (rgb == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
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
	    } else {
		rgb[i++] = (*pixel & 0x00ff0000) >> 16;
		rgb[i++] = (*pixel & 0x0000ff00) >>  8;
		rgb[i++] = (*pixel & 0x000000ff) >>  0;
	    }
	}
    }

    compressed = compress_dup (rgb, rgb_size, &compressed_size);
    if (compressed == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto CLEANUP_RGB;
    }

    need_smask = FALSE;
    if (image->format == CAIRO_FORMAT_ARGB32) {
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
_cairo_pdf_surface_emit_solid_pattern (cairo_pdf_surface_t   *surface,
                                       cairo_solid_pattern_t *pattern)
{
    int alpha;
    cairo_status_t status;

    status = _cairo_pdf_surface_add_alpha (surface, pattern->color.alpha, &alpha);
    if (status)
	return status;

    surface->emitted_pattern.type = CAIRO_PATTERN_TYPE_SOLID;
    surface->emitted_pattern.red = pattern->color.red;
    surface->emitted_pattern.green = pattern->color.green;
    surface->emitted_pattern.blue = pattern->color.blue;
    surface->emitted_pattern.alpha = alpha;
    surface->emitted_pattern.smask.id = 0;
    surface->emitted_pattern.pattern.id = 0;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_surface_emit_surface_pattern (cairo_pdf_surface_t	 *surface,
                                         cairo_surface_pattern_t *pattern)
{
    cairo_pdf_resource_t stream;
    cairo_surface_t *pat_surface;
    cairo_surface_attributes_t pat_attr;
    cairo_image_surface_t *image;
    void *image_extra;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_pdf_resource_t image_resource = {0}; /* squelch bogus compiler warning */
    cairo_matrix_t cairo_p2d, pdf_p2d;
    cairo_extend_t extend = cairo_pattern_get_extend (&pattern->base);
    double xstep, ystep;
    cairo_rectangle_int_t surface_extents;

    /* XXX: Should do something clever here for PDF source surfaces ? */

    status = _cairo_pdf_surface_pause_content_stream (surface);
    if (status)
	return status;

    status = _cairo_pattern_acquire_surface ((cairo_pattern_t *)pattern,
					     (cairo_surface_t *)surface,
					     0, 0, -1, -1,
					     &pat_surface, &pat_attr);
    if (status)
	return status;

    status = _cairo_surface_acquire_source_image (pat_surface, &image, &image_extra);
    if (status)
	goto BAIL2;

    status = _cairo_pdf_surface_emit_image (surface, image, &image_resource);
    if (status)
	goto BAIL;

    status = _cairo_surface_get_extents (&surface->base, &surface_extents);
    if (status)
	goto BAIL;

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
				  image->width + image->height);
	}
	break;
    case CAIRO_EXTEND_REPEAT:
    case CAIRO_EXTEND_REFLECT:
	xstep = image->width;
	ystep = image->height;
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
     * on each page). So the PDF patterns patrix maps from a
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
    cairo_matrix_translate (&pdf_p2d, 0.0, image->height);
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
					     "   /Resources << /XObject << /res%d %d 0 R >> >>\r\n",
					     image->width, image->height,
					     xstep, ystep,
					     pdf_p2d.xx, pdf_p2d.yx,
					     pdf_p2d.xy, pdf_p2d.yy,
					     pdf_p2d.x0, pdf_p2d.y0,
					     image_resource.id,
					     image_resource.id);

    _cairo_output_stream_printf (surface->output,
				 "q %d 0 0 %d 0 0 cm /res%d Do Q\r\n",
				 image->width, image->height,
				 image_resource.id);

    status = _cairo_pdf_surface_close_stream (surface);
    if (status)
	goto BAIL;

    status = _cairo_pdf_surface_resume_content_stream (surface);
    if (status)
	goto BAIL;

    status = _cairo_pdf_surface_add_pattern (surface, stream);
    if (status)
	goto BAIL;

    status = _cairo_pdf_surface_add_alpha (surface, 1.0, &surface->emitted_pattern.alpha);
    if (status)
	goto BAIL;
    surface->emitted_pattern.type = CAIRO_PATTERN_TYPE_SURFACE;
    surface->emitted_pattern.smask.id = 0;
    surface->emitted_pattern.pattern = stream;

 BAIL:
    _cairo_surface_release_source_image (pat_surface, image, image_extra);
 BAIL2:
    _cairo_pattern_release_surface ((cairo_pattern_t *)pattern, pat_surface, &pat_attr);

    return status;
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
    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /FunctionType 3\r\n"
				 "   /Domain [ 0 1 ]\r\n",
				 res.id);

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
	return CAIRO_STATUS_NO_MEMORY;

    stops = &allstops[1];
    n_stops = pattern->n_stops;

    for (i = 0; i < n_stops; i++) {
	stops[i].color[0] = pattern->stops[i].color.red / 65535.0;
	stops[i].color[1] = pattern->stops[i].color.green / 65535.0;
	stops[i].color[2] = pattern->stops[i].color.blue / 65535.0;
	stops[i].color[3] = pattern->stops[i].color.alpha / 65535.0;
        if (!CAIRO_ALPHA_IS_OPAQUE (stops[i].color[3]))
            emit_alpha = TRUE;
	stops[i].offset = _cairo_fixed_to_double (pattern->stops[i].x);
    }

    /* make sure first offset is 0.0 and last offset is 1.0. (Otherwise Acrobat
     * Reader chokes.) */
    if (stops[0].offset > COLOR_STOP_EPSILON) {
	    memcpy (allstops, stops, sizeof (cairo_pdf_color_stop_t));
	    stops = allstops;
	    n_stops++;
    }
    stops[0].offset = 0.0;

    if (stops[n_stops-1].offset < 1.0 - COLOR_STOP_EPSILON) {
	    memcpy (&stops[n_stops],
		    &stops[n_stops - 1],
		    sizeof (cairo_pdf_color_stop_t));
	    n_stops++;
    }
    stops[n_stops-1].offset = 1.0;

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

static cairo_pdf_resource_t
cairo_pdf_surface_emit_transparency_group (cairo_pdf_surface_t  *surface,
                                           cairo_pdf_resource_t  gradient_mask)
{
    cairo_pdf_resource_t xobj_resource, smask_resource, gstate_resource;

    xobj_resource = _cairo_pdf_surface_open_stream (surface,
                                                    TRUE,
                                                    "   /Type /XObject\r\n"
                                                    "   /Subtype /Form\r\n"
                                                    "   /FormType 1\r\n"
                                                    "   /BBox [ 0 0 %f %f ]\r\n"
                                                    "   /Resources\r\n"
                                                    "      << /ExtGState\r\n"
                                                    "            << /a0 << /ca 1 /CA 1 >>"
                                                    "      >>\r\n"
                                                    "         /Pattern\r\n"
                                                    "            << /res%d %d 0 R >>\r\n"
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

    _cairo_output_stream_printf (surface->output,
                                 "q\r\n"
                                 "/a0 gs\r\n"
                                 "/Pattern cs /res%d scn\r\n"
                                 "0 0 %f %f re\r\n"
                                 "f\r\n"
                                 "Q\r\n",
                                 gradient_mask.id,
                                 surface->width,
                                 surface->height);

    _cairo_pdf_surface_close_stream (surface);

    smask_resource = _cairo_pdf_surface_new_object (surface);
    _cairo_output_stream_printf (surface->output,
                                 "%d 0 obj\r\n"
                                 "<< /Type /Mask\r\n"
                                 "   /S /Luminosity\r\n"
                                 "   /G %d 0 R\r\n"
                                 "   /BC [ 0.0 ]\r\n"
                                 ">>\r\n"
                                 "endobj\r\n",
                                 smask_resource.id,
                                 xobj_resource.id);

    /* Create GState which uses the transparency group as an SMask. */
    gstate_resource = _cairo_pdf_surface_new_object (surface);
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

    _cairo_pdf_surface_add_smask (surface, gstate_resource);

    return gstate_resource;
}

static cairo_status_t
_cairo_pdf_surface_emit_linear_pattern (cairo_pdf_surface_t    *surface,
                                        cairo_linear_pattern_t *pattern)
{
    cairo_pdf_resource_t pattern_resource, smask;
    cairo_pdf_resource_t color_function, alpha_function;
    double x1, y1, x2, y2;
    cairo_matrix_t pat_to_pdf;
    cairo_extend_t extend;
    cairo_status_t status;

    extend = cairo_pattern_get_extend (&pattern->base.base);
    status = _cairo_pdf_surface_pause_content_stream (surface);
    if (status)
	return status;

    status = _cairo_pdf_surface_emit_pattern_stops (surface,
                                                    &pattern->base,
                                                    &color_function,
                                                    &alpha_function);
    if (status)
	return status;

    pat_to_pdf = pattern->base.base.matrix;
    status = cairo_matrix_invert (&pat_to_pdf);
    if (status)
	return status;

    cairo_matrix_multiply (&pat_to_pdf, &pat_to_pdf, &surface->cairo_to_pdf);
    x1 = _cairo_fixed_to_double (pattern->p1.x);
    y1 = _cairo_fixed_to_double (pattern->p1.y);
    x2 = _cairo_fixed_to_double (pattern->p2.x);
    y2 = _cairo_fixed_to_double (pattern->p2.y);

    pattern_resource = _cairo_pdf_surface_new_object (surface);
    _cairo_output_stream_printf (surface->output,
                                 "%d 0 obj\r\n"
                                 "<< /Type /Pattern\r\n"
                                 "   /PatternType 2\r\n"
                                 "   /Matrix [ %f %f %f %f %f %f ]\r\n"
                                 "   /Shading\r\n"
                                 "      << /ShadingType 2\r\n"
                                 "         /ColorSpace /DeviceRGB\r\n"
                                 "         /Coords [ %f %f %f %f ]\r\n"
                                 "         /Function %d 0 R\r\n",
                                 pattern_resource.id,
                                 pat_to_pdf.xx, pat_to_pdf.yx,
                                 pat_to_pdf.xy, pat_to_pdf.yy,
                                 pat_to_pdf.x0, pat_to_pdf.y0,
                                 x1, y1, x2, y2,
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
        _cairo_output_stream_printf (surface->output,
                                     "%d 0 obj\r\n"
                                     "<< /Type /Pattern\r\n"
                                     "   /PatternType 2\r\n"
                                     "   /Matrix [ %f %f %f %f %f %f ]\r\n"
                                     "   /Shading\r\n"
                                     "      << /ShadingType 2\r\n"
                                     "         /ColorSpace /DeviceGray\r\n"
                                     "         /Coords [ %f %f %f %f ]\r\n"
                                     "         /Function %d 0 R\r\n",
                                     mask_resource.id,
                                     pat_to_pdf.xx, pat_to_pdf.yx,
                                     pat_to_pdf.xy, pat_to_pdf.yy,
                                     pat_to_pdf.x0, pat_to_pdf.y0,
                                     x1, y1, x2, y2,
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
        surface->emitted_pattern.smask = smask;
    }

    surface->emitted_pattern.type = CAIRO_PATTERN_TYPE_LINEAR;
    status = _cairo_pdf_surface_add_alpha (surface, 1, &surface->emitted_pattern.alpha);
    if (status)
        return status;

    surface->emitted_pattern.pattern = pattern_resource;
    status = _cairo_pdf_surface_add_pattern (surface, pattern_resource);
    if (status)
        return status;

    return _cairo_pdf_surface_resume_content_stream (surface);
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
    status = _cairo_pdf_surface_pause_content_stream (surface);
    if (status)
	return status;

    status = _cairo_pdf_surface_emit_pattern_stops (surface,
                                                    &pattern->base,
                                                    &color_function,
                                                    &alpha_function);
    if (status)
	return status;

    pat_to_pdf = pattern->base.base.matrix;
    status = cairo_matrix_invert (&pat_to_pdf);
    if (status)
	return status;

    cairo_matrix_multiply (&pat_to_pdf, &pat_to_pdf, &surface->cairo_to_pdf);
    x1 = _cairo_fixed_to_double (pattern->c1.x);
    y1 = _cairo_fixed_to_double (pattern->c1.y);
    r1 = _cairo_fixed_to_double (pattern->r1);
    x2 = _cairo_fixed_to_double (pattern->c2.x);
    y2 = _cairo_fixed_to_double (pattern->c2.y);
    r2 = _cairo_fixed_to_double (pattern->r2);

    pattern_resource = _cairo_pdf_surface_new_object (surface);
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
        surface->emitted_pattern.smask = smask;
    }

    surface->emitted_pattern.type = CAIRO_PATTERN_TYPE_RADIAL;

    status = _cairo_pdf_surface_add_alpha (surface, 1.0, &surface->emitted_pattern.alpha);
    if (status)
	return status;

    surface->emitted_pattern.pattern = pattern_resource;

    status = _cairo_pdf_surface_add_pattern (surface, pattern_resource);
    if (status)
	return status;

    return _cairo_pdf_surface_resume_content_stream (surface);
}

static cairo_status_t
_cairo_pdf_surface_emit_pattern (cairo_pdf_surface_t *surface, cairo_pattern_t *pattern)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return _cairo_pdf_surface_emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern);

    case CAIRO_PATTERN_TYPE_SURFACE:
	return _cairo_pdf_surface_emit_surface_pattern (surface, (cairo_surface_pattern_t *) pattern);

    case CAIRO_PATTERN_TYPE_LINEAR:
	return _cairo_pdf_surface_emit_linear_pattern (surface, (cairo_linear_pattern_t *) pattern);

    case CAIRO_PATTERN_TYPE_RADIAL:
	return _cairo_pdf_surface_emit_radial_pattern (surface, (cairo_radial_pattern_t *) pattern);
    }

    ASSERT_NOT_REACHED;
    return CAIRO_STATUS_PATTERN_TYPE_MISMATCH;
}

static cairo_status_t
_cairo_pdf_surface_select_pattern (cairo_pdf_surface_t *surface,
                                   cairo_bool_t         is_stroke)
{
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
                                     surface->emitted_pattern.alpha);
    } else {
        if (is_stroke) {
            _cairo_output_stream_printf (surface->output,
                                         "/Pattern CS /res%d SCN ",
                                         surface->emitted_pattern.pattern);
        } else {
            _cairo_output_stream_printf (surface->output,
                                         "/Pattern cs /res%d scn ",
                                         surface->emitted_pattern.pattern);
        }

        _cairo_output_stream_printf (surface->output,
                                     "/a%d gs ",
                                     surface->emitted_pattern.alpha );

        _cairo_output_stream_printf (surface->output, "\r\n");
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_copy_page (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;

    return _cairo_pdf_surface_write_page (surface);
}

static cairo_int_status_t
_cairo_pdf_surface_show_page (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_int_status_t status;

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

static cairo_int_status_t
_cairo_pdf_surface_intersect_clip_path (void			*abstract_surface,
					cairo_path_fixed_t	*path,
					cairo_fill_rule_t	fill_rule,
					double			tolerance,
					cairo_antialias_t	antialias)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_status_t status;
    const char *pdf_operator;
    pdf_path_info_t info;

    if (path == NULL) {
	if (surface->has_clip)
	    _cairo_output_stream_printf (surface->output, "Q\r\n");
	surface->has_clip = FALSE;
	return CAIRO_STATUS_SUCCESS;
    }

    if (!surface->has_clip) {
	_cairo_output_stream_printf (surface->output, "q ");
	surface->has_clip = TRUE;
    }

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

    return status;
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
    cairo_pdf_resource_t page, *res, smask;
    cairo_pdf_font_t font;
    int num_pages, num_fonts, i;
    int num_alphas, num_smasks, num_resources;
    double alpha;

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

    _cairo_output_stream_printf (surface->output, "   /Resources <<\r\n");

    num_alphas =  _cairo_array_num_elements (&surface->alphas);
    num_smasks =  _cairo_array_num_elements (&surface->smasks);
    if (num_alphas > 0 || num_smasks > 0) {
	_cairo_output_stream_printf (surface->output,
				     "      /ExtGState <<\r\n");

	for (i = 0; i < num_alphas; i++) {
	    _cairo_array_copy_element (&surface->alphas, i, &alpha);
	    _cairo_output_stream_printf (surface->output,
					 "         /a%d << /CA %f /ca %f >>\r\n",
					 i, alpha, alpha);
	}

        for (i = 0; i < num_smasks; i++) {
	    _cairo_array_copy_element (&surface->smasks, i, &smask);
	    _cairo_output_stream_printf (surface->output,
					 "         /sm%d %d 0 R\r\n",
					 smask.id, smask.id);
	}

	_cairo_output_stream_printf (surface->output,
				     "      >>\r\n");
    }

    num_resources = _cairo_array_num_elements (&surface->patterns);
    if (num_resources > 0) {
	_cairo_output_stream_printf (surface->output,
				     "      /Pattern <<");
	for (i = 0; i < num_resources; i++) {
	    res = _cairo_array_index (&surface->patterns, i);
	    _cairo_output_stream_printf (surface->output,
					 " /res%d %d 0 R",
					 res->id, res->id);
	}

	_cairo_output_stream_printf (surface->output,
				     " >>\r\n");
    }

    num_resources = _cairo_array_num_elements (&surface->xobjects);
    if (num_resources > 0) {
	_cairo_output_stream_printf (surface->output,
				     "      /XObject <<");

	for (i = 0; i < num_resources; i++) {
	    res = _cairo_array_index (&surface->xobjects, i);
	    _cairo_output_stream_printf (surface->output,
					 " /res%d %d 0 R",
					 res->id, res->id);
	}

	_cairo_output_stream_printf (surface->output,
				     " >>\r\n");
    }

    num_fonts = _cairo_array_num_elements (&surface->fonts);
    if (num_fonts > 0) {
        _cairo_output_stream_printf (surface->output,"      /Font <<\r\n");
        num_fonts = _cairo_array_num_elements (&surface->fonts);
        for (i = 0; i < num_fonts; i++) {
            _cairo_array_copy_element (&surface->fonts, i, &font);
            _cairo_output_stream_printf (surface->output,
                                         "         /CairoFont-%d-%d %d 0 R\r\n",
                                         font.font_id,
                                         font.subset_id,
                                         font.subset_resource.id);
        }
        _cairo_output_stream_printf (surface->output, "      >>\r\n");
    }

    _cairo_output_stream_printf (surface->output,
				 "   >>\r\n");

    /* TODO: Figure out wich other defaults to be inherited by /Page
     * objects. */
    _cairo_output_stream_printf (surface->output,
				 ">>\r\n"
				 "endobj\r\n");
}

static cairo_pdf_resource_t
_cairo_pdf_surface_emit_to_unicode_stream (cairo_pdf_surface_t		*surface,
					   cairo_scaled_font_subset_t	*font_subset,
                                           cairo_bool_t                  is_composite)
{
    const cairo_scaled_font_backend_t *backend;
    cairo_pdf_resource_t stream;
    unsigned int i, num_bfchar;
    cairo_status_t status;

    if (font_subset->to_unicode == NULL) {
        stream.id = 0;
        return stream;
    }

    if (_cairo_truetype_create_glyph_to_unicode_map (font_subset) != CAIRO_STATUS_SUCCESS) {
        backend = font_subset->scaled_font->backend;
        if (backend->map_glyphs_to_unicode == NULL) {
            stream.id = 0;
            return stream;
        }
        backend->map_glyphs_to_unicode (font_subset->scaled_font, font_subset);
    }

    stream = _cairo_pdf_surface_open_stream (surface, FALSE, NULL);
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

    status = _cairo_pdf_surface_close_stream (surface);
    if (status)
	stream.id = 0;

    return stream;
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
	return CAIRO_STATUS_NO_MEMORY;

    stream = _cairo_pdf_surface_new_object (surface);
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

    to_unicode_stream = _cairo_pdf_surface_emit_to_unicode_stream (surface, font_subset, TRUE);

    descriptor = _cairo_pdf_surface_new_object (surface);
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

    subset_resource = _cairo_pdf_surface_new_object (surface);
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
    unsigned long length, compressed_length;
    char *compressed;
    unsigned int i;


    /* We ignore the zero-trailer and set Length3 to 0. */
    length = subset->header_length + subset->data_length;
    compressed = compress_dup (subset->data, length, &compressed_length);
    if (compressed == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    stream = _cairo_pdf_surface_new_object (surface);
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

    to_unicode_stream = _cairo_pdf_surface_emit_to_unicode_stream (surface, font_subset, FALSE);

    descriptor = _cairo_pdf_surface_new_object (surface);
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

    subset_resource = _cairo_pdf_surface_new_object (surface);
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
	return CAIRO_STATUS_NO_MEMORY;
    }

    stream = _cairo_pdf_surface_new_object (surface);
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

    to_unicode_stream = _cairo_pdf_surface_emit_to_unicode_stream (surface, font_subset, TRUE);

    descriptor = _cairo_pdf_surface_new_object (surface);
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

    subset_resource = _cairo_pdf_surface_new_object (surface);
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

    *glyph_ret = _cairo_pdf_surface_open_stream (surface, FALSE, NULL);

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

    *glyph_ret = _cairo_pdf_surface_open_stream (surface, TRUE, NULL);

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

    return CAIRO_STATUS_SUCCESS;
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
	_cairo_surface_set_error (&surface->base, status);
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
    if (glyphs == NULL) {
	_cairo_surface_set_error (&surface->base, CAIRO_STATUS_NO_MEMORY);
	return CAIRO_STATUS_NO_MEMORY;
    }

    widths = _cairo_malloc_ab (font_subset->num_glyphs, sizeof (double));
    if (widths == NULL) {
        free (glyphs);
	_cairo_surface_set_error (&surface->base, CAIRO_STATUS_NO_MEMORY);
	return CAIRO_STATUS_NO_MEMORY;
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

    to_unicode_stream = _cairo_pdf_surface_emit_to_unicode_stream (surface, font_subset, FALSE);

    subset_resource = _cairo_pdf_surface_new_object (surface);
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

static void
_cairo_pdf_surface_emit_unscaled_font_subset (cairo_scaled_font_subset_t *font_subset,
                                              void			 *closure)
{
    cairo_pdf_surface_t *surface = closure;
    cairo_status_t status;

    if (font_subset->is_composite) {
        status = _cairo_pdf_surface_emit_cff_font_subset (surface, font_subset);
        if (status != CAIRO_INT_STATUS_UNSUPPORTED)
            return;

        status = _cairo_pdf_surface_emit_truetype_font_subset (surface, font_subset);
        if (status != CAIRO_INT_STATUS_UNSUPPORTED)
            return;

        status = _cairo_pdf_surface_emit_cff_fallback_font (surface, font_subset);
        if (status != CAIRO_INT_STATUS_UNSUPPORTED)
            return;
    } else {
#if CAIRO_HAS_FT_FONT
        status = _cairo_pdf_surface_emit_type1_font_subset (surface, font_subset);
        if (status != CAIRO_INT_STATUS_UNSUPPORTED)
            return;
#endif

        status = _cairo_pdf_surface_emit_type1_fallback_font (surface, font_subset);
        if (status != CAIRO_INT_STATUS_UNSUPPORTED)
            return;
    }
}

static void
_cairo_pdf_surface_emit_scaled_font_subset (cairo_scaled_font_subset_t *font_subset,
                                            void		       *closure)
{
    cairo_pdf_surface_t *surface = closure;
    cairo_status_t status;

    status = _cairo_pdf_surface_emit_type3_font_subset (surface, font_subset);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return;
}

static cairo_status_t
_cairo_pdf_surface_emit_font_subsets (cairo_pdf_surface_t *surface)
{
    cairo_status_t status;

    status = _cairo_scaled_font_subsets_foreach_unscaled (surface->font_subsets,
                                                          _cairo_pdf_surface_emit_unscaled_font_subset,
                                                          surface);
    status = _cairo_scaled_font_subsets_foreach_scaled (surface->font_subsets,
                                                        _cairo_pdf_surface_emit_scaled_font_subset,
                                                        surface);
    _cairo_scaled_font_subsets_destroy (surface->font_subsets);
    surface->font_subsets = NULL;

    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_pdf_resource_t
_cairo_pdf_surface_write_catalog (cairo_pdf_surface_t *surface)
{
    cairo_pdf_resource_t catalog;

    catalog = _cairo_pdf_surface_new_object (surface);
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
_cairo_pdf_surface_write_page (cairo_pdf_surface_t *surface)
{
    cairo_status_t status;
    cairo_pdf_resource_t page;
    cairo_pdf_resource_t stream;
    int num_streams, i;

    if (surface->has_clip) {
	_cairo_output_stream_printf (surface->output, "Q\r\n");
	surface->has_clip = FALSE;
    }

    status = _cairo_pdf_surface_close_stream (surface);
    if (status)
	return status;

    page = _cairo_pdf_surface_new_object (surface);
    _cairo_output_stream_printf (surface->output,
				 "%d 0 obj\r\n"
				 "<< /Type /Page\r\n"
				 "   /Parent %d 0 R\r\n",
				 page.id,
				 surface->pages_resource.id);

    _cairo_output_stream_printf (surface->output,
				 "   /MediaBox [ 0 0 %f %f ]\r\n",
				 surface->width,
				 surface->height);

    _cairo_output_stream_printf (surface->output,
				 "   /Contents [");
    num_streams = _cairo_array_num_elements (&surface->streams);
    for (i = 0; i < num_streams; i++) {
	_cairo_array_copy_element (&surface->streams, i, &stream);
	_cairo_output_stream_printf (surface->output,
				     " %d 0 R",
				     stream.id);
    }
    _cairo_output_stream_printf (surface->output,
				 " ]\r\n"
                                 "   /Group <<\r\n"
                                 "      /Type /Group\r\n"
                                 "      /S /Transparency\r\n"
                                 "      /CS /DeviceRGB\r\n"
                                 "   >>\r\n"
				 ">>\r\n"
				 "endobj\r\n");

    status = _cairo_array_append (&surface->pages, &page);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_surface_pattern_supported (cairo_surface_pattern_t *pattern)
{
    cairo_extend_t extend;

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

    if (extend == CAIRO_EXTEND_REPEAT ||
        extend == CAIRO_EXTEND_REFLECT) {
        return FALSE;
    }

    /* Radial gradients are currently only supported when one circle
     * is inside the other. */
    if (pattern->type == CAIRO_PATTERN_TYPE_RADIAL) {
        double x1, y1, x2, y2, r1, r2, d;
        cairo_radial_pattern_t *radial = (cairo_radial_pattern_t *) pattern;

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
_cairo_pdf_surface_operation_supported (cairo_pdf_surface_t *surface,
		      cairo_operator_t op,
		      cairo_pattern_t *pattern)
{
    if (surface->force_fallbacks)
	return FALSE;

    if (! _pattern_supported (pattern))
	return FALSE;

    /* XXX: We can probably support a fair amount more than just OVER,
     * but this should cover many common cases at least. */
    if (op == CAIRO_OPERATOR_OVER)
	return TRUE;

    return FALSE;
}

static cairo_int_status_t
_cairo_pdf_surface_analyze_operation (cairo_pdf_surface_t *surface,
		    cairo_operator_t op,
		    cairo_pattern_t *pattern)
{
    if (_cairo_pdf_surface_operation_supported (surface, op, pattern))
	return CAIRO_STATUS_SUCCESS;
    else
	return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_pdf_surface_paint (void			*abstract_surface,
			  cairo_operator_t	 op,
			  cairo_pattern_t	*source)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_resource_t group = {0}; /* squelch bogus compiler warning */
    cairo_status_t status;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_pdf_surface_analyze_operation (surface, op, source);

    /* XXX: It would be nice to be able to assert this condition
     * here. But, we actually allow one 'cheat' that is used when
     * painting the final image-based fallbacks. The final fallbacks
     * do have alpha which we support by blending with white. This is
     * possible only because there is nothing between the fallback
     * images and the paper, nor is anything painted above. */
    /*
    assert (_cairo_pdf_surface_operation_supported (op, source));
    */

    status = _cairo_pdf_surface_emit_pattern (surface, source);
    if (status)
	return status;

    if (surface->emitted_pattern.smask.id != 0) {
	status = _cairo_pdf_surface_begin_group (surface, &group);
        if (status)
            return status;
    } else {
        _cairo_output_stream_printf (surface->output, "q ");
    }

    _cairo_pdf_surface_select_pattern (surface, FALSE);

    _cairo_output_stream_printf (surface->output,
				 "0 0 %f %f re f\r\n",
				 surface->width, surface->height);

    if (surface->emitted_pattern.smask.id != 0) {
        _cairo_pdf_surface_end_group (surface);

        _cairo_output_stream_printf (surface->output,
                                     "q /sm%d gs /res%d Do Q\r\n",
                                     surface->emitted_pattern.smask,
                                     group.id);
    } else {
        _cairo_output_stream_printf (surface->output, "Q\r\n");
    }

    return _cairo_output_stream_get_status (surface->output);
}

static cairo_int_status_t
_cairo_pdf_surface_mask	(void			*abstract_surface,
			 cairo_operator_t	 op,
			 cairo_pattern_t	*source,
			 cairo_pattern_t	*mask)
{
    cairo_pdf_surface_t *surface = abstract_surface;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    ASSERT_NOT_REACHED;

    return CAIRO_INT_STATUS_UNSUPPORTED;
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
    cairo_pdf_resource_t group = {0}; /* squelch bogus compiler warning */
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
	status = _cairo_pdf_surface_begin_group (surface, &group);
        if (status)
            return status;
    } else {
        _cairo_output_stream_printf (surface->output, "q ");
    }

    _cairo_pdf_surface_select_pattern (surface, TRUE);

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

    _cairo_output_stream_printf (surface->output, "S Q\r\n");

    if (surface->emitted_pattern.smask.id != 0) {
        _cairo_pdf_surface_end_group (surface);

        _cairo_output_stream_printf (surface->output,
                                     "q /sm%d gs /res%d Do Q\r\n",
                                     surface->emitted_pattern.smask,
                                     group.id);
    } else {
        _cairo_output_stream_printf (surface->output, "Q\r\n");
    }

    return status;
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
    cairo_pdf_resource_t group = {0}; /* squelch bogus compiler warning */
    const char *pdf_operator;
    cairo_status_t status;
    pdf_path_info_t info;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_pdf_surface_analyze_operation (surface, op, source);

    assert (_cairo_pdf_surface_operation_supported (surface, op, source));

    status = _cairo_pdf_surface_emit_pattern (surface, source);
    if (status)
	return status;

    if (surface->emitted_pattern.smask.id != 0) {
	status = _cairo_pdf_surface_begin_group (surface, &group);
        if (status)
            return status;
    } else {
        _cairo_output_stream_printf (surface->output, "q ");
    }

    _cairo_pdf_surface_select_pattern (surface, FALSE);
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
        _cairo_pdf_surface_end_group (surface);

        _cairo_output_stream_printf (surface->output,
                                     "q /sm%d gs /res%d Do Q\r\n",
                                     surface->emitted_pattern.smask,
                                     group.id);
    } else {
        _cairo_output_stream_printf (surface->output, "Q\r\n");
    }

    return status;
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
    cairo_pdf_resource_t group = {0}; /* squelch bogus compiler warning */
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
	status = _cairo_pdf_surface_begin_group (surface, &group);
        if (status)
            return status;
    } else {
        _cairo_output_stream_printf (surface->output, "q ");
    }

    _cairo_pdf_surface_select_pattern (surface, FALSE);

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
                                         "/CairoFont-%d-%d 1 Tf\r\n",
                                         subset_glyph.font_id,
                                         subset_glyph.subset_id);
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
        _cairo_pdf_surface_end_group (surface);

        _cairo_output_stream_printf (surface->output,
                                     "q /sm%d gs /res%d Do Q\r\n",
                                     surface->emitted_pattern.smask,
                                     group.id);
    } else {
        _cairo_output_stream_printf (surface->output, "Q\r\n");
    }

    return _cairo_output_stream_get_status (surface->output);
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
    NULL, /* create_similar */
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

/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2008 Adrian Johnson
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
 * The Initial Developer of the Original Code is Adrian Johnson.
 *
 * Contributor(s):
 *	Adrian Johnson <ajohnson@redneon.com>
 */

#include "cairoint.h"

#if CAIRO_HAS_FONT_SUBSET

#include "cairo-type3-glyph-surface-private.h"
#include "cairo-output-stream-private.h"
#include "cairo-meta-surface-private.h"
#include "cairo-analysis-surface-private.h"

static const cairo_surface_backend_t cairo_type3_glyph_surface_backend;

cairo_surface_t *
_cairo_type3_glyph_surface_create (cairo_scaled_font_t	 		 *scaled_font,
				   cairo_output_stream_t 		 *stream,
				   cairo_type3_glyph_surface_emit_image_t emit_image,
				   cairo_scaled_font_subsets_t 		 *font_subsets)
{
    cairo_type3_glyph_surface_t *surface;
    cairo_matrix_t invert_y_axis;

    surface = malloc (sizeof (cairo_type3_glyph_surface_t));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base, &cairo_type3_glyph_surface_backend,
			 CAIRO_CONTENT_COLOR_ALPHA);

    surface->scaled_font = scaled_font;
    surface->stream = stream;
    surface->emit_image = emit_image;

    /* Setup the transform from the user-font device space to Type 3
     * font space. The Type 3 font space is defined by the FontMatrix
     * entry in the Type 3 dictionary. In the PDF backend this is an
     * identity matrix. */
    surface->cairo_to_pdf = scaled_font->scale_inverse;
    cairo_matrix_init_scale (&invert_y_axis, 1, -1);
    cairo_matrix_multiply (&surface->cairo_to_pdf, &surface->cairo_to_pdf, &invert_y_axis);

    _cairo_pdf_operators_init (&surface->pdf_operators,
			       surface->stream,
			       &surface->cairo_to_pdf,
			       font_subsets);

    return &surface->base;
}

static cairo_status_t
_cairo_type3_glyph_surface_emit_image (cairo_type3_glyph_surface_t *surface,
				       cairo_image_surface_t       *image,
				       cairo_matrix_t              *image_matrix)
{
    cairo_status_t status;
    cairo_image_surface_t *image_mask;

    /* The only image type supported by Type 3 fonts are 1-bit image
     * masks */
    if (image->format == CAIRO_FORMAT_A1) {
	image_mask = image;
    } else {
	image_mask = _cairo_image_surface_clone (image, CAIRO_FORMAT_A1);
	status = cairo_surface_status (&image->base);
	if (status)
	    return status;
    }

    _cairo_output_stream_printf (surface->stream,
				 "q %f %f %f %f %f %f cm\n",
				 image_matrix->xx,
				 image_matrix->xy,
				 image_matrix->yx,
				 image_matrix->yy,
				 image_matrix->x0,
				 image_matrix->y0);

    status = surface->emit_image (image_mask, surface->stream);

    _cairo_output_stream_printf (surface->stream,
				 "Q\n");

    if (image_mask != image)
	cairo_surface_destroy (&image_mask->base);

    return status;
}

static cairo_status_t
_cairo_type3_glyph_surface_emit_image_pattern (cairo_type3_glyph_surface_t *surface,
					       cairo_image_surface_t       *image,
					       cairo_matrix_t              *pattern_matrix)
{
    cairo_matrix_t mat, upside_down;
    cairo_status_t status;

    if (image->width == 0 || image->height == 0)
	return CAIRO_STATUS_SUCCESS;

    mat = *pattern_matrix;

    /* Get the pattern space to user space matrix  */
    status = cairo_matrix_invert (&mat);

    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    /* Make this a pattern space to Type 3 font space matrix */
    cairo_matrix_multiply (&mat, &mat, &surface->cairo_to_pdf);

    /* PDF images are in a 1 unit by 1 unit image space. Turn the 1 by
     * 1 image upside down to convert to flip the Y-axis going from
     * cairo to PDF. Then scale the image up to the required size. */
    cairo_matrix_scale (&mat, image->width, image->height);
    cairo_matrix_init (&upside_down, 1, 0, 0, -1, 0, 1);
    cairo_matrix_multiply (&mat, &upside_down, &mat);

    return _cairo_type3_glyph_surface_emit_image (surface, image, &mat);
}

static cairo_status_t
_cairo_type3_glyph_surface_finish (void *abstract_surface)
{
    cairo_type3_glyph_surface_t *surface = abstract_surface;

    return _cairo_pdf_operators_fini (&surface->pdf_operators);
}

static cairo_int_status_t
_cairo_type3_glyph_surface_intersect_clip_path (void		   *abstract_surface,
						cairo_path_fixed_t *path,
						cairo_fill_rule_t   fill_rule,
						double		    tolerance,
						cairo_antialias_t   antialias)
{
    cairo_type3_glyph_surface_t *surface = abstract_surface;

    if (path == NULL) {
	_cairo_output_stream_printf (surface->stream, "Q q\n");
	return CAIRO_STATUS_SUCCESS;
    }

    return _cairo_pdf_operators_clip (&surface->pdf_operators,
				      path,
				      fill_rule);
}

static cairo_int_status_t
_cairo_type3_glyph_surface_paint (void			*abstract_surface,
				  cairo_operator_t	 op,
				  cairo_pattern_t	*source)
{
    cairo_type3_glyph_surface_t *surface = abstract_surface;
    cairo_surface_pattern_t *pattern;
    cairo_image_surface_t *image;
    void *image_extra;
    cairo_status_t status;

    if (source->type != CAIRO_PATTERN_TYPE_SURFACE)
	return CAIRO_INT_STATUS_IMAGE_FALLBACK;

    pattern = (cairo_surface_pattern_t *) source;
    status = _cairo_surface_acquire_source_image (pattern->surface, &image, &image_extra);
    if (status)
	goto fail;

    status = _cairo_type3_glyph_surface_emit_image_pattern (surface,
							    image,
							    &pattern->base.matrix);

fail:
    _cairo_surface_release_source_image (pattern->surface, image, image_extra);

    return status;
}

static cairo_int_status_t
_cairo_type3_glyph_surface_mask (void			*abstract_surface,
				 cairo_operator_t	 op,
				 cairo_pattern_t	*source,
				 cairo_pattern_t	*mask)
{
    return _cairo_type3_glyph_surface_paint (abstract_surface, op, mask);
}

static cairo_int_status_t
_cairo_type3_glyph_surface_stroke (void			*abstract_surface,
				   cairo_operator_t	 op,
				   cairo_pattern_t	*source,
				   cairo_path_fixed_t	*path,
				   cairo_stroke_style_t	*style,
				   cairo_matrix_t	*ctm,
				   cairo_matrix_t	*ctm_inverse,
				   double		 tolerance,
				   cairo_antialias_t	 antialias)
{
    cairo_type3_glyph_surface_t *surface = abstract_surface;

    return _cairo_pdf_operators_stroke (&surface->pdf_operators,
					path,
					style,
					ctm,
					ctm_inverse);
}

static cairo_int_status_t
_cairo_type3_glyph_surface_fill (void			*abstract_surface,
				 cairo_operator_t	 op,
				 cairo_pattern_t	*source,
				 cairo_path_fixed_t	*path,
				 cairo_fill_rule_t	 fill_rule,
				 double			 tolerance,
				 cairo_antialias_t	 antialias)
{
    cairo_type3_glyph_surface_t *surface = abstract_surface;
    cairo_int_status_t status;

    status = _cairo_pdf_operators_fill (&surface->pdf_operators,
					path,
					fill_rule);

    return status;
}

static cairo_int_status_t
_cairo_type3_glyph_surface_show_glyphs (void		     *abstract_surface,
					cairo_operator_t      op,
					cairo_pattern_t	     *source,
					cairo_glyph_t        *glyphs,
					int		      num_glyphs,
					cairo_scaled_font_t  *scaled_font,
					int		     *remaining_glyphs)
{
    cairo_type3_glyph_surface_t *surface = abstract_surface;
    cairo_int_status_t status;
    cairo_scaled_font_t *font;
    cairo_matrix_t new_ctm, ctm_inverse;
    int i;

    for (i = 0; i < num_glyphs; i++)
	cairo_matrix_transform_point (&surface->cairo_to_pdf, &glyphs[i].x, &glyphs[i].y);

    /* We require the matrix to be invertable. */
    ctm_inverse = scaled_font->ctm;
    status = cairo_matrix_invert (&ctm_inverse);
    if (status)
	return CAIRO_INT_STATUS_IMAGE_FALLBACK;

    cairo_matrix_multiply (&new_ctm, &scaled_font->ctm, &ctm_inverse);
    font = cairo_scaled_font_create (scaled_font->font_face,
				     &scaled_font->font_matrix,
				     &new_ctm,
				     &scaled_font->options);

    status = _cairo_pdf_operators_show_text_glyphs (&surface->pdf_operators,
						    NULL, 0,
						    glyphs, num_glyphs,
						    NULL, 0,
						    FALSE,
						    font);

    cairo_scaled_font_destroy (font);

    return status;
}

static const cairo_surface_backend_t cairo_type3_glyph_surface_backend = {
    CAIRO_INTERNAL_SURFACE_TYPE_TYPE3_GLYPH,
    NULL, /* _cairo_type3_glyph_surface_create_similar */
    _cairo_type3_glyph_surface_finish,
    NULL, /* acquire_source_image */
    NULL, /* release_source_image */
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* cairo_type3_glyph_surface_copy_page */
    NULL, /* _cairo_type3_glyph_surface_show_page */
    NULL, /* set_clip_region */
    _cairo_type3_glyph_surface_intersect_clip_path,
    NULL, /* _cairo_type3_glyph_surface_get_extents */
    NULL, /* old_show_glyphs */
    NULL, /* _cairo_type3_glyph_surface_get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */
    _cairo_type3_glyph_surface_paint,
    _cairo_type3_glyph_surface_mask,
    _cairo_type3_glyph_surface_stroke,
    _cairo_type3_glyph_surface_fill,
    _cairo_type3_glyph_surface_show_glyphs,
    NULL, /* snapshot */
};

static void
_cairo_type3_glyph_surface_set_stream (cairo_type3_glyph_surface_t *surface,
				       cairo_output_stream_t       *stream)
{
    surface->stream = stream;
    _cairo_pdf_operators_set_stream (&surface->pdf_operators, stream);
}

static cairo_status_t
_cairo_type3_glyph_surface_emit_fallback_image (cairo_type3_glyph_surface_t *surface,
						unsigned long		     glyph_index)
{
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_status_t status;
    cairo_image_surface_t *image;
    cairo_matrix_t mat;
    double x, y;

    status = _cairo_scaled_glyph_lookup (surface->scaled_font,
					 glyph_index,
					 CAIRO_SCALED_GLYPH_INFO_METRICS |
					 CAIRO_SCALED_GLYPH_INFO_SURFACE,
					 &scaled_glyph);
    if (status)
	return status;

    image = scaled_glyph->surface;
    if (image->width == 0 || image->height == 0)
	return CAIRO_STATUS_SUCCESS;

    x = _cairo_fixed_to_double (scaled_glyph->bbox.p1.x);
    y = _cairo_fixed_to_double (scaled_glyph->bbox.p2.y);
    mat.xx = image->width;
    mat.xy = 0;
    mat.yx = 0;
    mat.yy = image->height;
    mat.x0 = x;
    mat.y0 = y;
    cairo_matrix_multiply (&mat, &mat, &surface->scaled_font->scale_inverse);
    mat.y0 *= -1;

    return _cairo_type3_glyph_surface_emit_image (surface, image, &mat);
}

void
_cairo_type3_glyph_surface_set_font_subsets_callback (void		     		    *abstract_surface,
						      cairo_pdf_operators_use_font_subset_t  use_font_subset,
						      void				    *closure)
{
    cairo_type3_glyph_surface_t *surface = abstract_surface;

    _cairo_pdf_operators_set_font_subsets_callback (&surface->pdf_operators,
						    use_font_subset,
						    closure);
}

cairo_status_t
_cairo_type3_glyph_surface_analyze_glyph (void		     *abstract_surface,
					  unsigned long	      glyph_index)
{
    cairo_type3_glyph_surface_t *surface = abstract_surface;
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_status_t status, status2;
    cairo_output_stream_t *null_stream;

    null_stream = _cairo_null_stream_create ();
    _cairo_type3_glyph_surface_set_stream (surface, null_stream);

    _cairo_scaled_font_freeze_cache (surface->scaled_font);
    status = _cairo_scaled_glyph_lookup (surface->scaled_font,
					 glyph_index,
					 CAIRO_SCALED_GLYPH_INFO_METRICS |
					 CAIRO_SCALED_GLYPH_INFO_META_SURFACE,
					 &scaled_glyph);

    if (_cairo_status_is_error (status))
	goto cleanup;

    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	status = CAIRO_STATUS_SUCCESS;
	goto cleanup;
    }

    status = _cairo_meta_surface_replay (scaled_glyph->meta_surface,
					 &surface->base);
    if (status)
	goto cleanup;

    status2 = _cairo_pdf_operators_flush (&surface->pdf_operators);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;

    if (status == CAIRO_INT_STATUS_IMAGE_FALLBACK)
	status = CAIRO_STATUS_SUCCESS;

cleanup:
    _cairo_scaled_font_thaw_cache (surface->scaled_font);

    status2 = _cairo_output_stream_destroy (null_stream);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;

    return status;
}

cairo_status_t
_cairo_type3_glyph_surface_emit_glyph (void		     *abstract_surface,
				       cairo_output_stream_t *stream,
				       unsigned long	      glyph_index,
				       cairo_box_t           *bbox,
				       double                *width)
{
    cairo_type3_glyph_surface_t *surface = abstract_surface;
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_status_t status, status2;
    double x_advance, y_advance;
    cairo_output_stream_t *mem_stream;
    cairo_matrix_t font_matrix_inverse;

    _cairo_type3_glyph_surface_set_stream (surface, stream);

    _cairo_scaled_font_freeze_cache (surface->scaled_font);
    status = _cairo_scaled_glyph_lookup (surface->scaled_font,
					 glyph_index,
					 CAIRO_SCALED_GLYPH_INFO_METRICS |
					 CAIRO_SCALED_GLYPH_INFO_META_SURFACE,
					 &scaled_glyph);
    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	status = _cairo_scaled_glyph_lookup (surface->scaled_font,
					     glyph_index,
					     CAIRO_SCALED_GLYPH_INFO_METRICS,
					     &scaled_glyph);
	if (status == CAIRO_STATUS_SUCCESS)
	    status = CAIRO_INT_STATUS_IMAGE_FALLBACK;
    }
    if (_cairo_status_is_error (status)) {
	_cairo_scaled_font_thaw_cache (surface->scaled_font);
	return status;
    }

    x_advance = scaled_glyph->metrics.x_advance;
    y_advance = scaled_glyph->metrics.y_advance;
    font_matrix_inverse = surface->scaled_font->font_matrix;
    status2 = cairo_matrix_invert (&font_matrix_inverse);

    /* The invertability of font_matrix is tested in
     * pdf_operators_show_glyphs before any glyphs are mappped to the
     * subset. */
    assert (status2 == CAIRO_STATUS_SUCCESS);

    cairo_matrix_transform_distance (&font_matrix_inverse, &x_advance, &y_advance);
    *width = x_advance;

    *bbox = scaled_glyph->bbox;
    _cairo_matrix_transform_bounding_box_fixed (&surface->scaled_font->scale_inverse,
						bbox, NULL);

    _cairo_output_stream_printf (surface->stream,
				 "%f 0 %f %f %f %f d1\n",
                                 x_advance,
				 _cairo_fixed_to_double (bbox->p1.x),
				 - _cairo_fixed_to_double (bbox->p2.y),
				 _cairo_fixed_to_double (bbox->p2.x),
				 - _cairo_fixed_to_double (bbox->p1.y));

    if (status == CAIRO_STATUS_SUCCESS) {
	mem_stream = _cairo_memory_stream_create ();
	_cairo_type3_glyph_surface_set_stream (surface, mem_stream);

	_cairo_output_stream_printf (surface->stream, "q\n");
	status = _cairo_meta_surface_replay (scaled_glyph->meta_surface,
					     &surface->base);

	status2 = _cairo_pdf_operators_flush (&surface->pdf_operators);
	if (status == CAIRO_STATUS_SUCCESS)
	    status = status2;

	_cairo_output_stream_printf (surface->stream, "Q\n");

	_cairo_type3_glyph_surface_set_stream (surface, stream);
	if (status == CAIRO_STATUS_SUCCESS)
	    _cairo_memory_stream_copy (mem_stream, stream);

	status2 = _cairo_output_stream_destroy (mem_stream);
	if (status == CAIRO_STATUS_SUCCESS)
	    status = status2;
    }

    if (status == CAIRO_INT_STATUS_IMAGE_FALLBACK)
	status = _cairo_type3_glyph_surface_emit_fallback_image (surface, glyph_index);

    _cairo_scaled_font_thaw_cache (surface->scaled_font);

    return status;
}

#endif /* CAIRO_HAS_FONT_SUBSET */

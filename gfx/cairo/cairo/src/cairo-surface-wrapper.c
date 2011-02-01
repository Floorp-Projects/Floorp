/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc
 * Copyright © 2007 Adrian Johnson
 * Copyright © 2009 Chris Wilson
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *      Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-surface-wrapper-private.h"

/* A collection of routines to facilitate surface wrapping */

static cairo_bool_t
_cairo_surface_wrapper_needs_device_transform (cairo_surface_wrapper_t *wrapper,
					       cairo_matrix_t *matrix)
{
    if (_cairo_matrix_is_identity (&wrapper->target->device_transform))
	return FALSE;

    *matrix = wrapper->target->device_transform;
    return TRUE;
}

cairo_status_t
_cairo_surface_wrapper_acquire_source_image (cairo_surface_wrapper_t *wrapper,
					     cairo_image_surface_t  **image_out,
					     void                   **image_extra)
{
    if (unlikely (wrapper->target->status))
	return wrapper->target->status;

    return _cairo_surface_acquire_source_image (wrapper->target,
						image_out, image_extra);
}

void
_cairo_surface_wrapper_release_source_image (cairo_surface_wrapper_t *wrapper,
					     cairo_image_surface_t  *image,
					     void                   *image_extra)
{
    _cairo_surface_release_source_image (wrapper->target, image, image_extra);
}

static void
_cairo_surface_wrapper_transform_pattern (cairo_surface_wrapper_t *wrapper,
                                          cairo_pattern_t         *pattern,
                                          const cairo_pattern_t   *original)
{
    _cairo_pattern_init_static_copy (pattern, original);

    if (_cairo_surface_has_device_transform (wrapper->target))
        _cairo_pattern_transform (pattern,
                                  &wrapper->target->device_transform_inverse);
}

cairo_status_t
_cairo_surface_wrapper_paint (cairo_surface_wrapper_t *wrapper,
			      cairo_operator_t	       op,
			      const cairo_pattern_t   *source,
			      cairo_clip_t	      *clip)
{
    cairo_status_t status;
    cairo_matrix_t device_transform;
    cairo_clip_t clip_copy, *dev_clip = clip;
    cairo_pattern_union_t source_pattern;

    if (unlikely (wrapper->target->status))
	return wrapper->target->status;

    if (clip && clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    if (clip != NULL) {
	if (_cairo_surface_wrapper_needs_device_transform (wrapper,
							   &device_transform))
	{
	    status = _cairo_clip_init_copy_transformed (&clip_copy, clip,
							&device_transform);
	    if (unlikely (status))
		goto FINISH;

	} else {
	    _cairo_clip_init_copy (&clip_copy, clip);
	}

	dev_clip = &clip_copy;
    }

    _cairo_surface_wrapper_transform_pattern (wrapper, &source_pattern.base, source);

    status = _cairo_surface_paint (wrapper->target, op, &source_pattern.base, dev_clip);

  FINISH:
    if (dev_clip != clip)
	_cairo_clip_reset (dev_clip);
    return status;
}

cairo_status_t
_cairo_surface_wrapper_mask (cairo_surface_wrapper_t *wrapper,
			     cairo_operator_t	 op,
			     const cairo_pattern_t *source,
			     const cairo_pattern_t *mask,
			     cairo_clip_t	    *clip)
{
    cairo_status_t status;
    cairo_matrix_t device_transform;
    cairo_clip_t clip_copy, *dev_clip = clip;
    cairo_pattern_union_t source_pattern, mask_pattern;

    if (unlikely (wrapper->target->status))
	return wrapper->target->status;

    if (clip && clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    if (clip != NULL) {
	if (_cairo_surface_wrapper_needs_device_transform (wrapper,
							   &device_transform))
	{
	    status = _cairo_clip_init_copy_transformed (&clip_copy, clip,
							&device_transform);
	    if (unlikely (status))
		goto FINISH;

	} else {
	    _cairo_clip_init_copy (&clip_copy, clip);
	}

	dev_clip = &clip_copy;
    }

    _cairo_surface_wrapper_transform_pattern (wrapper, &source_pattern.base, source);
    _cairo_surface_wrapper_transform_pattern (wrapper, &mask_pattern.base, mask);

    status = _cairo_surface_mask (wrapper->target, op,
                                  &source_pattern.base, &mask_pattern.base,
                                  dev_clip);

  FINISH:
    if (dev_clip != clip)
	_cairo_clip_reset (dev_clip);
    return status;
}

cairo_status_t
_cairo_surface_wrapper_stroke (cairo_surface_wrapper_t *wrapper,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*source,
			       cairo_path_fixed_t	*path,
			       cairo_stroke_style_t	*stroke_style,
			       cairo_matrix_t		*ctm,
			       cairo_matrix_t		*ctm_inverse,
			       double			 tolerance,
			       cairo_antialias_t	 antialias,
			       cairo_clip_t		*clip)
{
    cairo_status_t status;
    cairo_matrix_t device_transform;
    cairo_path_fixed_t path_copy, *dev_path = path;
    cairo_clip_t clip_copy, *dev_clip = clip;
    cairo_matrix_t dev_ctm = *ctm;
    cairo_matrix_t dev_ctm_inverse = *ctm_inverse;
    cairo_pattern_union_t source_pattern;

    if (unlikely (wrapper->target->status))
	return wrapper->target->status;

    if (clip && clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    if (_cairo_surface_wrapper_needs_device_transform (wrapper,
						       &device_transform))
    {
	status = _cairo_path_fixed_init_copy (&path_copy, dev_path);
	if (unlikely (status))
	    goto FINISH;

	_cairo_path_fixed_transform (&path_copy, &device_transform);
	dev_path = &path_copy;

	if (clip != NULL) {
	    status = _cairo_clip_init_copy_transformed (&clip_copy, clip,
							&device_transform);
	    if (unlikely (status))
		goto FINISH;

	    dev_clip = &clip_copy;
	}

	cairo_matrix_multiply (&dev_ctm, &dev_ctm, &device_transform);
	status = cairo_matrix_invert (&device_transform);
	assert (status == CAIRO_STATUS_SUCCESS);
	cairo_matrix_multiply (&dev_ctm_inverse,
			       &device_transform,
			       &dev_ctm_inverse);
    } else {
	if (clip != NULL) {
	    dev_clip = &clip_copy;
	    _cairo_clip_init_copy (&clip_copy, clip);
	}
    }

    _cairo_surface_wrapper_transform_pattern (wrapper, &source_pattern.base, source);

    status = _cairo_surface_stroke (wrapper->target, op, &source_pattern.base,
				    dev_path, stroke_style,
				    &dev_ctm, &dev_ctm_inverse,
				    tolerance, antialias,
				    dev_clip);

 FINISH:
    if (dev_path != path)
	_cairo_path_fixed_fini (dev_path);
    if (dev_clip != clip)
	_cairo_clip_reset (dev_clip);
    return status;
}

cairo_status_t
_cairo_surface_wrapper_fill_stroke (cairo_surface_wrapper_t *wrapper,
				    cairo_operator_t	     fill_op,
				    const cairo_pattern_t   *fill_source,
				    cairo_fill_rule_t	     fill_rule,
				    double		     fill_tolerance,
				    cairo_antialias_t	     fill_antialias,
				    cairo_path_fixed_t	    *path,
				    cairo_operator_t	     stroke_op,
				    const cairo_pattern_t   *stroke_source,
				    cairo_stroke_style_t    *stroke_style,
				    cairo_matrix_t	    *stroke_ctm,
				    cairo_matrix_t	    *stroke_ctm_inverse,
				    double		     stroke_tolerance,
				    cairo_antialias_t	     stroke_antialias,
				    cairo_clip_t	    *clip)
{
    cairo_status_t status;
    cairo_matrix_t device_transform;
    cairo_path_fixed_t path_copy, *dev_path = path;
    cairo_clip_t clip_copy, *dev_clip = clip;
    cairo_matrix_t dev_ctm = *stroke_ctm;
    cairo_matrix_t dev_ctm_inverse = *stroke_ctm_inverse;
    cairo_pattern_union_t fill_pattern, stroke_pattern;

    if (unlikely (wrapper->target->status))
	return wrapper->target->status;

    if (clip && clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    if (_cairo_surface_wrapper_needs_device_transform (wrapper,
						       &device_transform))
    {
	status = _cairo_path_fixed_init_copy (&path_copy, dev_path);
	if (unlikely (status))
	    goto FINISH;

	_cairo_path_fixed_transform (&path_copy, &device_transform);
	dev_path = &path_copy;

	if (clip != NULL) {
	    status = _cairo_clip_init_copy_transformed (&clip_copy, clip,
							&device_transform);
	    if (unlikely (status))
		goto FINISH;

	    dev_clip = &clip_copy;
	}

	cairo_matrix_multiply (&dev_ctm, &dev_ctm, &device_transform);
	status = cairo_matrix_invert (&device_transform);
	assert (status == CAIRO_STATUS_SUCCESS);
	cairo_matrix_multiply (&dev_ctm_inverse,
			       &device_transform,
			       &dev_ctm_inverse);
    } else {
	if (clip != NULL) {
	    dev_clip = &clip_copy;
	    _cairo_clip_init_copy (&clip_copy, clip);
	}
    }

    _cairo_surface_wrapper_transform_pattern (wrapper, &fill_pattern.base, fill_source);
    _cairo_surface_wrapper_transform_pattern (wrapper, &stroke_pattern.base, stroke_source);

    status = _cairo_surface_fill_stroke (wrapper->target,
					 fill_op, &fill_pattern.base, fill_rule,
					 fill_tolerance, fill_antialias,
					 dev_path,
					 stroke_op, &stroke_pattern.base,
					 stroke_style,
					 &dev_ctm, &dev_ctm_inverse,
					 stroke_tolerance, stroke_antialias,
					 dev_clip);

  FINISH:
    if (dev_path != path)
	_cairo_path_fixed_fini (dev_path);
    if (dev_clip != clip)
	_cairo_clip_reset (dev_clip);
    return status;
}

cairo_status_t
_cairo_surface_wrapper_fill (cairo_surface_wrapper_t	*wrapper,
			     cairo_operator_t	 op,
			     const cairo_pattern_t *source,
			     cairo_path_fixed_t	*path,
			     cairo_fill_rule_t	 fill_rule,
			     double		 tolerance,
			     cairo_antialias_t	 antialias,
			     cairo_clip_t	*clip)
{
    cairo_status_t status;
    cairo_matrix_t device_transform;
    cairo_path_fixed_t path_copy, *dev_path = path;
    cairo_clip_t clip_copy, *dev_clip = clip;
    cairo_pattern_union_t source_pattern;

    if (unlikely (wrapper->target->status))
	return wrapper->target->status;

    if (clip && clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    if (_cairo_surface_wrapper_needs_device_transform (wrapper,
						       &device_transform))
    {
	status = _cairo_path_fixed_init_copy (&path_copy, dev_path);
	if (unlikely (status))
	    goto FINISH;

	_cairo_path_fixed_transform (&path_copy, &device_transform);
	dev_path = &path_copy;

	if (clip != NULL) {
	    status = _cairo_clip_init_copy_transformed (&clip_copy, clip,
							&device_transform);
	    if (unlikely (status))
		goto FINISH;

	    dev_clip = &clip_copy;
	}
    } else {
	if (clip != NULL) {
	    dev_clip = &clip_copy;
	    _cairo_clip_init_copy (&clip_copy, clip);
	}
    }

    _cairo_surface_wrapper_transform_pattern (wrapper, &source_pattern.base, source);

    status = _cairo_surface_fill (wrapper->target, op,
                                  &source_pattern.base,
				  dev_path, fill_rule,
				  tolerance, antialias,
				  dev_clip);

 FINISH:
    if (dev_path != path)
	_cairo_path_fixed_fini (dev_path);
    if (dev_clip != clip)
	_cairo_clip_reset (dev_clip);
    return status;
}

cairo_status_t
_cairo_surface_wrapper_show_text_glyphs (cairo_surface_wrapper_t *wrapper,
					 cairo_operator_t	     op,
					 const cairo_pattern_t	    *source,
					 const char		    *utf8,
					 int			     utf8_len,
					 cairo_glyph_t		    *glyphs,
					 int			     num_glyphs,
					 const cairo_text_cluster_t *clusters,
					 int			     num_clusters,
					 cairo_text_cluster_flags_t  cluster_flags,
					 cairo_scaled_font_t	    *scaled_font,
					 cairo_clip_t		    *clip)
{
    cairo_status_t status;
    cairo_matrix_t device_transform;
    cairo_pattern_union_t source_pattern;
    cairo_clip_t clip_copy, *dev_clip = clip;
    cairo_glyph_t *dev_glyphs = glyphs;

    if (unlikely (wrapper->target->status))
	return wrapper->target->status;

    if (glyphs == NULL || num_glyphs == 0)
	return CAIRO_STATUS_SUCCESS;

    if (clip && clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    if (_cairo_surface_wrapper_needs_device_transform (wrapper,
						       &device_transform))
    {
	int i;

	if (clip != NULL) {
	    dev_clip = &clip_copy;
	    status = _cairo_clip_init_copy_transformed (&clip_copy, clip,
							&device_transform);
	    if (unlikely (status))
		goto FINISH;
	}

	dev_glyphs = _cairo_malloc_ab (num_glyphs, sizeof (cairo_glyph_t));
	if (dev_glyphs == NULL) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto FINISH;
	}

	for (i = 0; i < num_glyphs; i++) {
	    dev_glyphs[i] = glyphs[i];
	    cairo_matrix_transform_point (&device_transform,
					  &dev_glyphs[i].x,
					  &dev_glyphs[i].y);
	}
    } else {
	if (clip != NULL) {
	    dev_clip = &clip_copy;
	    _cairo_clip_init_copy (&clip_copy, clip);
	}
    }

    _cairo_surface_wrapper_transform_pattern (wrapper, &source_pattern.base, source);

    status = _cairo_surface_show_text_glyphs (wrapper->target, op,
                                              &source_pattern.base,
					      utf8, utf8_len,
					      dev_glyphs, num_glyphs,
					      clusters, num_clusters,
					      cluster_flags,
					      scaled_font,
					      dev_clip);
 FINISH:
    if (dev_clip != clip)
	_cairo_clip_reset (dev_clip);
    if (dev_glyphs != glyphs)
	free (dev_glyphs);
    return status;
}

cairo_surface_t *
_cairo_surface_wrapper_create_similar (cairo_surface_wrapper_t *wrapper,
				       cairo_content_t	content,
				       int		width,
				       int		height)
{
    return _cairo_surface_create_similar_solid (wrapper->target,
						content, width, height,
						CAIRO_COLOR_TRANSPARENT,
						TRUE);
}

cairo_bool_t
_cairo_surface_wrapper_get_extents (cairo_surface_wrapper_t *wrapper,
				    cairo_rectangle_int_t   *extents)
{
    return _cairo_surface_get_extents (wrapper->target, extents);
}

void
_cairo_surface_wrapper_get_font_options (cairo_surface_wrapper_t    *wrapper,
					 cairo_font_options_t	    *options)
{
    cairo_surface_get_font_options (wrapper->target, options);
}

cairo_surface_t *
_cairo_surface_wrapper_snapshot (cairo_surface_wrapper_t *wrapper)
{
    return _cairo_surface_snapshot (wrapper->target);
}

cairo_bool_t
_cairo_surface_wrapper_has_show_text_glyphs (cairo_surface_wrapper_t *wrapper)
{
    return cairo_surface_has_show_text_glyphs (wrapper->target);
}

void
_cairo_surface_wrapper_init (cairo_surface_wrapper_t *wrapper,
			     cairo_surface_t *target)
{
    wrapper->target = cairo_surface_reference (target);
}

void
_cairo_surface_wrapper_fini (cairo_surface_wrapper_t *wrapper)
{
    cairo_surface_destroy (wrapper->target);
}

cairo_status_t
_cairo_surface_wrapper_flush (cairo_surface_wrapper_t *wrapper)
{
    if (wrapper->target->backend->flush) {
	return wrapper->target->backend->flush(wrapper->target);
    }
}

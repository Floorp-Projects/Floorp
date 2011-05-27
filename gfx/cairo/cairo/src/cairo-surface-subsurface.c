/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Intel Corporation
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
 * The Initial Developer of the Original Code is Intel Corporation.
 *
 * Contributor(s):
 *      Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-error-private.h"
#include "cairo-recording-surface-private.h"
#include "cairo-surface-offset-private.h"
#include "cairo-surface-subsurface-private.h"

static const cairo_surface_backend_t _cairo_surface_subsurface_backend;

static cairo_status_t
_cairo_surface_subsurface_finish (void *abstract_surface)
{
    cairo_surface_subsurface_t *surface = abstract_surface;

    cairo_surface_destroy (surface->target);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
_cairo_surface_subsurface_create_similar (void *other,
					  cairo_content_t content,
					  int width, int height)
{
    cairo_surface_subsurface_t *surface = other;
    return surface->target->backend->create_similar (surface->target, content, width, height);
}

static cairo_int_status_t
_cairo_surface_subsurface_paint (void *abstract_surface,
				 cairo_operator_t op,
				 const cairo_pattern_t *source,
				 cairo_clip_t *clip)
{
    cairo_surface_subsurface_t *surface = abstract_surface;
    cairo_rectangle_int_t rect = { 0, 0, surface->extents.width, surface->extents.height };
    cairo_status_t status;
    cairo_clip_t target_clip;

    _cairo_clip_init_copy (&target_clip, clip);
    status = _cairo_clip_rectangle (&target_clip, &rect);
    if (unlikely (status))
	goto CLEANUP;

    status = _cairo_surface_offset_paint (surface->target,
					 -surface->extents.x, -surface->extents.y,
					  op, source, &target_clip);
  CLEANUP:
    _cairo_clip_fini (&target_clip);
    return status;
}

static cairo_int_status_t
_cairo_surface_subsurface_mask (void *abstract_surface,
				cairo_operator_t op,
				const cairo_pattern_t *source,
				const cairo_pattern_t *mask,
				cairo_clip_t *clip)
{
    cairo_surface_subsurface_t *surface = abstract_surface;
    cairo_rectangle_int_t rect = { 0, 0, surface->extents.width, surface->extents.height };
    cairo_status_t status;
    cairo_clip_t target_clip;

    _cairo_clip_init_copy (&target_clip, clip);
    status = _cairo_clip_rectangle (&target_clip, &rect);
    if (unlikely (status))
	goto CLEANUP;

    status = _cairo_surface_offset_mask (surface->target,
					 -surface->extents.x, -surface->extents.y,
					 op, source, mask, &target_clip);
  CLEANUP:
    _cairo_clip_fini (&target_clip);
    return status;
}

static cairo_int_status_t
_cairo_surface_subsurface_fill (void			*abstract_surface,
				cairo_operator_t	 op,
				const cairo_pattern_t	*source,
				cairo_path_fixed_t	*path,
				cairo_fill_rule_t	 fill_rule,
				double			 tolerance,
				cairo_antialias_t	 antialias,
				cairo_clip_t		*clip)
{
    cairo_surface_subsurface_t *surface = abstract_surface;
    cairo_rectangle_int_t rect = { 0, 0, surface->extents.width, surface->extents.height };
    cairo_status_t status;
    cairo_clip_t target_clip;

    _cairo_clip_init_copy (&target_clip, clip);
    status = _cairo_clip_rectangle (&target_clip, &rect);
    if (unlikely (status))
	goto CLEANUP;

    status = _cairo_surface_offset_fill (surface->target,
					 -surface->extents.x, -surface->extents.y,
					 op, source, path, fill_rule, tolerance, antialias,
					 &target_clip);
  CLEANUP:
    _cairo_clip_fini (&target_clip);
    return status;
}

static cairo_int_status_t
_cairo_surface_subsurface_stroke (void				*abstract_surface,
				  cairo_operator_t		 op,
				  const cairo_pattern_t		*source,
				  cairo_path_fixed_t		*path,
				  const cairo_stroke_style_t	*stroke_style,
				  const cairo_matrix_t		*ctm,
				  const cairo_matrix_t		*ctm_inverse,
				  double			 tolerance,
				  cairo_antialias_t		 antialias,
				  cairo_clip_t			*clip)
{
    cairo_surface_subsurface_t *surface = abstract_surface;
    cairo_rectangle_int_t rect = { 0, 0, surface->extents.width, surface->extents.height };
    cairo_status_t status;
    cairo_clip_t target_clip;

    _cairo_clip_init_copy (&target_clip, clip);
    status = _cairo_clip_rectangle (&target_clip, &rect);
    if (unlikely (status))
	goto CLEANUP;

    status = _cairo_surface_offset_stroke (surface->target,
					   -surface->extents.x, -surface->extents.y,
					   op, source, path, stroke_style, ctm, ctm_inverse,
					   tolerance, antialias,
					   &target_clip);
  CLEANUP:
    _cairo_clip_fini (&target_clip);
    return status;
}

static cairo_int_status_t
_cairo_surface_subsurface_glyphs (void			*abstract_surface,
				  cairo_operator_t	 op,
				  const cairo_pattern_t	*source,
				  cairo_glyph_t		*glyphs,
				  int			 num_glyphs,
				  cairo_scaled_font_t	*scaled_font,
				  cairo_clip_t		*clip,
				  int *remaining_glyphs)
{
    cairo_surface_subsurface_t *surface = abstract_surface;
    cairo_rectangle_int_t rect = { 0, 0, surface->extents.width, surface->extents.height };
    cairo_status_t status;
    cairo_clip_t target_clip;

    _cairo_clip_init_copy (&target_clip, clip);
    status = _cairo_clip_rectangle (&target_clip, &rect);
    if (unlikely (status))
	goto CLEANUP;

    status = _cairo_surface_offset_glyphs (surface->target,
					   -surface->extents.x, -surface->extents.y,
					   op, source,
					   scaled_font, glyphs, num_glyphs,
					   &target_clip);
    *remaining_glyphs = 0;
  CLEANUP:
    _cairo_clip_fini (&target_clip);
    return status;
}

static cairo_status_t
_cairo_surface_subsurface_flush (void *abstract_surface)
{
    cairo_surface_subsurface_t *surface = abstract_surface;
    cairo_status_t status;

    status = CAIRO_STATUS_SUCCESS;
    if (surface->target->backend->flush != NULL)
	status = surface->target->backend->flush (surface->target);

    return status;
}

static cairo_status_t
_cairo_surface_subsurface_mark_dirty (void *abstract_surface,
				      int x, int y,
				      int width, int height)
{
    cairo_surface_subsurface_t *surface = abstract_surface;
    cairo_status_t status;

    status = CAIRO_STATUS_SUCCESS;
    if (surface->target->backend->mark_dirty_rectangle != NULL) {
	cairo_rectangle_int_t rect, extents;

	rect.x = x;
	rect.y = y;
	rect.width  = width;
	rect.height = height;

	extents.x = extents.y = 0;
	extents.width  = surface->extents.width;
	extents.height = surface->extents.height;

	if (_cairo_rectangle_intersect (&rect, &extents)) {
	    status = surface->target->backend->mark_dirty_rectangle (surface->target,
								     rect.x + surface->extents.x,
								     rect.y + surface->extents.y,
								     rect.width, rect.height);
	}
    }

    return status;
}

static cairo_bool_t
_cairo_surface_subsurface_get_extents (void *abstract_surface,
				       cairo_rectangle_int_t *extents)
{
    cairo_surface_subsurface_t *surface = abstract_surface;

    extents->x = 0;
    extents->y = 0;
    extents->width  = surface->extents.width;
    extents->height = surface->extents.height;

    return TRUE;
}

static void
_cairo_surface_subsurface_get_font_options (void *abstract_surface,
					    cairo_font_options_t *options)
{
    cairo_surface_subsurface_t *surface = abstract_surface;

    if (surface->target->backend->get_font_options != NULL)
	surface->target->backend->get_font_options (surface->target, options);
}

struct extra {
    cairo_image_surface_t *image;
    void *image_extra;
};

static void
cairo_surface_paint_to_target (cairo_surface_t            *target,
                               cairo_surface_subsurface_t *subsurface)
{
    cairo_t *cr;
    
    cr = cairo_create (target);

    cairo_set_source_surface (cr,
                              subsurface->target,
                              - subsurface->extents.x,
                              - subsurface->extents.y);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    
    cairo_destroy (cr);
}

static cairo_status_t
_cairo_surface_subsurface_acquire_source_image (void                    *abstract_surface,
						cairo_image_surface_t  **image_out,
						void                   **extra_out)
{
    cairo_rectangle_int_t target_extents;
    cairo_surface_subsurface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    cairo_status_t status;
    struct extra *extra;
    uint8_t *data;
    cairo_bool_t ret;

    if (surface->target->type == CAIRO_SURFACE_TYPE_RECORDING) {
	cairo_recording_surface_t *meta = (cairo_recording_surface_t *) surface->target;
	cairo_surface_t *snapshot;

	snapshot = _cairo_surface_has_snapshot (&surface->base,
						&_cairo_image_surface_backend);
	if (snapshot != NULL) {
	    *image_out = (cairo_image_surface_t *) cairo_surface_reference (snapshot);
	    *extra_out = NULL;
	    return CAIRO_STATUS_SUCCESS;
	}

	if (! _cairo_surface_has_snapshot (&meta->base,
					   &_cairo_image_surface_backend))
	{
	    image = (cairo_image_surface_t *)
		_cairo_image_surface_create_with_content (meta->content,
							  surface->extents.width,
							  surface->extents.height);
	    if (unlikely (image->base.status))
		return image->base.status;

            cairo_surface_paint_to_target (&image->base, surface);

	    _cairo_surface_attach_snapshot (&surface->base, &image->base, NULL);

	    *image_out = image;
	    *extra_out = NULL;
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    extra = malloc (sizeof (struct extra));
    if (unlikely (extra == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _cairo_surface_acquire_source_image (surface->target, &extra->image, &extra->image_extra);
    if (unlikely (status))
	goto CLEANUP;

    ret = _cairo_surface_get_extents (&extra->image->base, &target_extents);
    assert (ret);

    /* only copy if we need to perform sub-byte manipulation */
    if (PIXMAN_FORMAT_BPP (extra->image->pixman_format) >= 8 &&
	target_extents.x <= surface->extents.x &&
	target_extents.y <= surface->extents.y &&
	surface->extents.x + surface->extents.width <= target_extents.x + target_extents.width &&
	surface->extents.y + surface->extents.height <= target_extents.y + target_extents.height) {

	assert ((PIXMAN_FORMAT_BPP (extra->image->pixman_format) % 8) == 0);

	data = extra->image->data + surface->extents.y * extra->image->stride;
	data += PIXMAN_FORMAT_BPP (extra->image->pixman_format) / 8 * surface->extents.x;

	image = (cairo_image_surface_t *)
	    _cairo_image_surface_create_with_pixman_format (data,
							    extra->image->pixman_format,
							    surface->extents.width,
							    surface->extents.height,
							    extra->image->stride);
	if (unlikely ((status = image->base.status)))
	    goto CLEANUP_IMAGE;

        image->base.is_clear = FALSE;
    } else {
	image = (cairo_image_surface_t *)
	    _cairo_image_surface_create_with_pixman_format (NULL,
							    extra->image->pixman_format,
							    surface->extents.width,
							    surface->extents.height,
							    0);
	if (unlikely ((status = image->base.status)))
	    goto CLEANUP_IMAGE;

        cairo_surface_paint_to_target (&image->base, surface);
    }

    *image_out = image;
    *extra_out = extra;
    return CAIRO_STATUS_SUCCESS;

CLEANUP_IMAGE:
    _cairo_surface_release_source_image (surface->target, extra->image, extra->image_extra);
CLEANUP:
    free (extra);
    return status;
}

static void
_cairo_surface_subsurface_release_source_image (void                   *abstract_surface,
						cairo_image_surface_t  *image,
						void                   *abstract_extra)
{
    cairo_surface_subsurface_t *surface = abstract_surface;

    if (abstract_extra != NULL) {
	struct extra *extra = abstract_extra;

	_cairo_surface_release_source_image (surface->target, extra->image, extra->image_extra);
	free (extra);
    }

    cairo_surface_destroy (&image->base);
}

static cairo_surface_t *
_cairo_surface_subsurface_snapshot (void *abstract_surface)
{
    cairo_surface_subsurface_t *surface = abstract_surface;
    cairo_surface_subsurface_t *snapshot;

    snapshot = malloc (sizeof (cairo_surface_subsurface_t));
    if (unlikely (snapshot == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&snapshot->base,
			 &_cairo_surface_subsurface_backend,
			 NULL, /* device */
			 surface->target->content);
    snapshot->target = _cairo_surface_snapshot (surface->target);
    if (unlikely (snapshot->target->status)) {
	cairo_status_t status;

	status = snapshot->target->status;
	free (snapshot);
	return _cairo_surface_create_in_error (status);
    }

    snapshot->base.type = snapshot->target->type;
    snapshot->extents = surface->extents;

    return &snapshot->base;
}

static const cairo_surface_backend_t _cairo_surface_subsurface_backend = {
    CAIRO_SURFACE_TYPE_SUBSURFACE,
    _cairo_surface_subsurface_create_similar,
    _cairo_surface_subsurface_finish,

    _cairo_surface_subsurface_acquire_source_image,
    _cairo_surface_subsurface_release_source_image,
    NULL, NULL, /* acquire, release dest */
    NULL, /* clone similar */
    NULL, /* composite */
    NULL, /* fill rectangles */
    NULL, /* composite trapezoids */
    NULL, /* create span renderer */
    NULL, /* check span renderer */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_surface_subsurface_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_surface_subsurface_get_font_options,
    _cairo_surface_subsurface_flush,
    _cairo_surface_subsurface_mark_dirty,
    NULL, /* font_fini */
    NULL, /* glyph_fini */

    _cairo_surface_subsurface_paint,
    _cairo_surface_subsurface_mask,
    _cairo_surface_subsurface_stroke,
    _cairo_surface_subsurface_fill,
    _cairo_surface_subsurface_glyphs,

    _cairo_surface_subsurface_snapshot,
};

/**
 * cairo_surface_create_for_rectangle:
 * @target: an existing surface for which the sub-surface will point to
 * @x: the x-origin of the sub-surface from the top-left of the target surface (in device-space units)
 * @y: the y-origin of the sub-surface from the top-left of the target surface (in device-space units)
 * @width: width of the sub-surface (in device-space units)
 * @height: height of the sub-surface (in device-space units)
 *
 * Create a new surface that is a rectangle within the target surface.
 * All operations drawn to this surface are then clipped and translated
 * onto the target surface. Nothing drawn via this sub-surface outside of
 * its bounds is drawn onto the target surface, making this a useful method
 * for passing constrained child surfaces to library routines that draw
 * directly onto the parent surface, i.e. with no further backend allocations,
 * double buffering or copies.
 *
 * <note><para>The semantics of subsurfaces have not been finalized yet
 * unless the rectangle is in full device units, is contained within
 * the extents of the target surface, and the target or subsurface's
 * device transforms are not changed.</para></note>
 *
 * Return value: a pointer to the newly allocated surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if @other is already in an error state
 * or any other error occurs.
 *
 * Since: 1.10
 **/
cairo_surface_t *
cairo_surface_create_for_rectangle (cairo_surface_t *target,
				    double x, double y,
				    double width, double height)
{
    cairo_surface_subsurface_t *surface;

    if (unlikely (target->status))
	return _cairo_surface_create_in_error (target->status);
    if (unlikely (target->finished))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_SURFACE_FINISHED));

    surface = malloc (sizeof (cairo_surface_subsurface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    assert (_cairo_matrix_is_translation (&target->device_transform));
    x += target->device_transform.x0;
    y += target->device_transform.y0;

    _cairo_surface_init (&surface->base,
			 &_cairo_surface_subsurface_backend,
			 NULL, /* device */
			 target->content);

    /* XXX forced integer alignment */
    surface->extents.x = ceil (x);
    surface->extents.y = ceil (y);
    surface->extents.width = floor (x + width) - surface->extents.x;
    surface->extents.height = floor (y + height) - surface->extents.y;

    if (target->backend->type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	/* Maintain subsurfaces as 1-depth */
	cairo_surface_subsurface_t *sub = (cairo_surface_subsurface_t *) target;
	surface->extents.x += sub->extents.x;
	surface->extents.y += sub->extents.y;
	target = sub->target;
    }

    surface->target = cairo_surface_reference (target);

    return &surface->base;
}
/* XXX observe mark-dirty */

/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Kristian Høgsberg <krh@redhat.com>
 *	Carl Worth <cworth@cworth.org>
 *	Adrian Johnson <ajohnson@redneon.com>
 */

/**
 * SECTION:cairo-recording
 * @Title: Recording Surfaces
 * @Short_Description: Records all drawing operations
 * @See_Also: #cairo_surface_t
 *
 * A recording surface is a surface that records all drawing operations at
 * the highest level of the surface backend interface, (that is, the
 * level of paint, mask, stroke, fill, and show_text_glyphs). The recording
 * surface can then be "replayed" against any target surface by using it
 * as a source surface.
 *
 * If you want to replay a surface so that the results in target will be
 * identical to the results that would have been obtained if the original
 * operations applied to the recording surface had instead been applied to the
 * target surface, you can use code like this:
 * <informalexample><programlisting>
 *      cairo_t *cr;
 *
 *	cr = cairo_create (target);
 *	cairo_set_source_surface (cr, recording_surface, 0.0, 0.0);
 *	cairo_paint (cr);
 *	cairo_destroy (cr);
 * </programlisting></informalexample>
 *
 * A recording surface is logically unbounded, i.e. it has no implicit constraint
 * on the size of the drawing surface. However, in practice this is rarely
 * useful as you wish to replay against a particular target surface with
 * known bounds. For this case, it is more efficient to specify the target
 * extents to the recording surface upon creation.
 *
 * The recording phase of the recording surface is careful to snapshot all
 * necessary objects (paths, patterns, etc.), in order to achieve
 * accurate replay. The efficiency of the recording surface could be
 * improved by improving the implementation of snapshot for the
 * various objects. For example, it would be nice to have a
 * copy-on-write implementation for _cairo_surface_snapshot.
 */

#include "cairoint.h"
#include "cairo-analysis-surface-private.h"
#include "cairo-clip-private.h"
#include "cairo-error-private.h"
#include "cairo-recording-surface-private.h"
#include "cairo-surface-wrapper-private.h"

typedef enum {
    CAIRO_RECORDING_REPLAY,
    CAIRO_RECORDING_CREATE_REGIONS
} cairo_recording_replay_type_t;

static const cairo_surface_backend_t cairo_recording_surface_backend;

/**
 * CAIRO_HAS_RECORDING_SURFACE:
 *
 * Defined if the recording surface backend is available.
 * The recording surface backend is always built in.
 * This macro was added for completeness in cairo 1.10.
 *
 * Since: 1.10
 */

/* Currently all recording surfaces do have a size which should be passed
 * in as the maximum size of any target surface against which the
 * recording-surface will ever be replayed.
 *
 * XXX: The naming of "pixels" in the size here is a misnomer. It's
 * actually a size in whatever device-space units are desired (again,
 * according to the intended replay target).
 */

/**
 * cairo_recording_surface_create:
 * @content: the content of the recording surface
 * @extents: the extents to record in pixels, can be %NULL to record
 *           unbounded operations.
 *
 * Creates a recording-surface which can be used to record all drawing operations
 * at the highest level (that is, the level of paint, mask, stroke, fill
 * and show_text_glyphs). The recording surface can then be "replayed" against
 * any target surface by using it as a source to drawing operations.
 *
 * The recording phase of the recording surface is careful to snapshot all
 * necessary objects (paths, patterns, etc.), in order to achieve
 * accurate replay.
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * Since: 1.10
 **/
cairo_surface_t *
cairo_recording_surface_create (cairo_content_t		 content,
				const cairo_rectangle_t	*extents)
{
    cairo_recording_surface_t *recording_surface;
    cairo_status_t status;

    recording_surface = malloc (sizeof (cairo_recording_surface_t));
    if (unlikely (recording_surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&recording_surface->base,
			 &cairo_recording_surface_backend,
			 NULL, /* device */
			 content);

    recording_surface->content = content;

    recording_surface->unbounded = TRUE;
    _cairo_clip_init (&recording_surface->clip);

    /* unbounded -> 'infinite' extents */
    if (extents != NULL) {
	recording_surface->extents_pixels = *extents;

	/* XXX check for overflow */
	recording_surface->extents.x = floor (extents->x);
	recording_surface->extents.y = floor (extents->y);
	recording_surface->extents.width = ceil (extents->x + extents->width) - recording_surface->extents.x;
	recording_surface->extents.height = ceil (extents->y + extents->height) - recording_surface->extents.y;

	status = _cairo_clip_rectangle (&recording_surface->clip,
					&recording_surface->extents);
	if (unlikely (status)) {
	    free (recording_surface);
	    return _cairo_surface_create_in_error (status);
	}

	recording_surface->unbounded = FALSE;
    }

    _cairo_array_init (&recording_surface->commands, sizeof (cairo_command_t *));

    recording_surface->replay_start_idx = 0;
    recording_surface->base.is_clear = TRUE;

    return &recording_surface->base;
}
slim_hidden_def (cairo_recording_surface_create);

static cairo_surface_t *
_cairo_recording_surface_create_similar (void		       *abstract_surface,
					 cairo_content_t	content,
					 int			width,
					 int			height)
{
    cairo_rectangle_t extents;
    extents.x = extents.y = 0;
    extents.width = width;
    extents.height = height;
    return cairo_recording_surface_create (content, &extents);
}

static cairo_status_t
_cairo_recording_surface_finish (void *abstract_surface)
{
    cairo_recording_surface_t *recording_surface = abstract_surface;
    cairo_command_t **elements;
    int i, num_elements;

    num_elements = recording_surface->commands.num_elements;
    elements = _cairo_array_index (&recording_surface->commands, 0);
    for (i = 0; i < num_elements; i++) {
	cairo_command_t *command = elements[i];

	switch (command->header.type) {
	case CAIRO_COMMAND_PAINT:
	    _cairo_pattern_fini (&command->paint.source.base);
	    break;

	case CAIRO_COMMAND_MASK:
	    _cairo_pattern_fini (&command->mask.source.base);
	    _cairo_pattern_fini (&command->mask.mask.base);
	    break;

	case CAIRO_COMMAND_STROKE:
	    _cairo_pattern_fini (&command->stroke.source.base);
	    _cairo_path_fixed_fini (&command->stroke.path);
	    _cairo_stroke_style_fini (&command->stroke.style);
	    break;

	case CAIRO_COMMAND_FILL:
	    _cairo_pattern_fini (&command->fill.source.base);
	    _cairo_path_fixed_fini (&command->fill.path);
	    break;

	case CAIRO_COMMAND_SHOW_TEXT_GLYPHS:
	    _cairo_pattern_fini (&command->show_text_glyphs.source.base);
	    free (command->show_text_glyphs.utf8);
	    free (command->show_text_glyphs.glyphs);
	    free (command->show_text_glyphs.clusters);
	    cairo_scaled_font_destroy (command->show_text_glyphs.scaled_font);
	    break;

	default:
	    ASSERT_NOT_REACHED;
	}

	_cairo_clip_fini (&command->header.clip);
	free (command);
    }

    _cairo_array_fini (&recording_surface->commands);
    _cairo_clip_fini (&recording_surface->clip);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_recording_surface_acquire_source_image (void			 *abstract_surface,
					       cairo_image_surface_t	**image_out,
					       void			**image_extra)
{
    cairo_status_t status;
    cairo_recording_surface_t *surface = abstract_surface;
    cairo_surface_t *image;

    image = _cairo_surface_has_snapshot (&surface->base,
					 &_cairo_image_surface_backend);
    if (image != NULL) {
	*image_out = (cairo_image_surface_t *) cairo_surface_reference (image);
	*image_extra = NULL;
	return CAIRO_STATUS_SUCCESS;
    }

    image = _cairo_image_surface_create_with_content (surface->content,
						      surface->extents.width,
						      surface->extents.height);
    if (unlikely (image->status))
	return image->status;

    cairo_surface_set_device_offset (image,
				     -surface->extents.x,
				     -surface->extents.y);

    status = _cairo_recording_surface_replay (&surface->base, image);
    if (unlikely (status)) {
	cairo_surface_destroy (image);
	return status;
    }

    _cairo_surface_attach_snapshot (&surface->base, image, NULL);

    *image_out = (cairo_image_surface_t *) image;
    *image_extra = NULL;
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_recording_surface_release_source_image (void			*abstract_surface,
					       cairo_image_surface_t	*image,
					       void			*image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_command_init (cairo_recording_surface_t *recording_surface,
	       cairo_command_header_t *command,
	       cairo_command_type_t type,
	       cairo_operator_t op,
	       cairo_clip_t *clip)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    command->type = type;
    command->op = op;
    command->region = CAIRO_RECORDING_REGION_ALL;
    _cairo_clip_init_copy (&command->clip, clip);
    if (recording_surface->clip.path != NULL)
	status = _cairo_clip_apply_clip (&command->clip, &recording_surface->clip);

    return status;
}

static cairo_int_status_t
_cairo_recording_surface_paint (void			  *abstract_surface,
				cairo_operator_t	   op,
				const cairo_pattern_t	  *source,
				cairo_clip_t		  *clip)
{
    cairo_status_t status;
    cairo_recording_surface_t *recording_surface = abstract_surface;
    cairo_command_paint_t *command;

    command = malloc (sizeof (cairo_command_paint_t));
    if (unlikely (command == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _command_init (recording_surface,
			    &command->header, CAIRO_COMMAND_PAINT, op, clip);
    if (unlikely (status))
	goto CLEANUP_COMMAND;

    status = _cairo_pattern_init_snapshot (&command->source.base, source);
    if (unlikely (status))
	goto CLEANUP_COMMAND;

    status = _cairo_array_append (&recording_surface->commands, &command);
    if (unlikely (status))
	goto CLEANUP_SOURCE;

    /* An optimisation that takes care to not replay what was done
     * before surface is cleared. We don't erase recorded commands
     * since we may have earlier snapshots of this surface. */
    if (op == CAIRO_OPERATOR_CLEAR && clip == NULL)
	recording_surface->replay_start_idx = recording_surface->commands.num_elements;

    return CAIRO_STATUS_SUCCESS;

  CLEANUP_SOURCE:
    _cairo_pattern_fini (&command->source.base);
  CLEANUP_COMMAND:
    _cairo_clip_fini (&command->header.clip);
    free (command);
    return status;
}

static cairo_int_status_t
_cairo_recording_surface_mask (void			*abstract_surface,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*source,
			       const cairo_pattern_t	*mask,
			       cairo_clip_t		*clip)
{
    cairo_status_t status;
    cairo_recording_surface_t *recording_surface = abstract_surface;
    cairo_command_mask_t *command;

    command = malloc (sizeof (cairo_command_mask_t));
    if (unlikely (command == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _command_init (recording_surface,
			    &command->header, CAIRO_COMMAND_MASK, op, clip);
    if (unlikely (status))
	goto CLEANUP_COMMAND;

    status = _cairo_pattern_init_snapshot (&command->source.base, source);
    if (unlikely (status))
	goto CLEANUP_COMMAND;

    status = _cairo_pattern_init_snapshot (&command->mask.base, mask);
    if (unlikely (status))
	goto CLEANUP_SOURCE;

    status = _cairo_array_append (&recording_surface->commands, &command);
    if (unlikely (status))
	goto CLEANUP_MASK;

    return CAIRO_STATUS_SUCCESS;

  CLEANUP_MASK:
    _cairo_pattern_fini (&command->mask.base);
  CLEANUP_SOURCE:
    _cairo_pattern_fini (&command->source.base);
  CLEANUP_COMMAND:
    _cairo_clip_fini (&command->header.clip);
    free (command);
    return status;
}

static cairo_int_status_t
_cairo_recording_surface_stroke (void			*abstract_surface,
				 cairo_operator_t	 op,
				 const cairo_pattern_t	*source,
				 cairo_path_fixed_t	*path,
				 const cairo_stroke_style_t	*style,
				 const cairo_matrix_t		*ctm,
				 const cairo_matrix_t		*ctm_inverse,
				 double			 tolerance,
				 cairo_antialias_t	 antialias,
				 cairo_clip_t		*clip)
{
    cairo_status_t status;
    cairo_recording_surface_t *recording_surface = abstract_surface;
    cairo_command_stroke_t *command;

    command = malloc (sizeof (cairo_command_stroke_t));
    if (unlikely (command == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _command_init (recording_surface,
			    &command->header, CAIRO_COMMAND_STROKE, op, clip);
    if (unlikely (status))
	goto CLEANUP_COMMAND;

    status = _cairo_pattern_init_snapshot (&command->source.base, source);
    if (unlikely (status))
	goto CLEANUP_COMMAND;

    status = _cairo_path_fixed_init_copy (&command->path, path);
    if (unlikely (status))
	goto CLEANUP_SOURCE;

    status = _cairo_stroke_style_init_copy (&command->style, style);
    if (unlikely (status))
	goto CLEANUP_PATH;

    command->ctm = *ctm;
    command->ctm_inverse = *ctm_inverse;
    command->tolerance = tolerance;
    command->antialias = antialias;

    status = _cairo_array_append (&recording_surface->commands, &command);
    if (unlikely (status))
	goto CLEANUP_STYLE;

    return CAIRO_STATUS_SUCCESS;

  CLEANUP_STYLE:
    _cairo_stroke_style_fini (&command->style);
  CLEANUP_PATH:
    _cairo_path_fixed_fini (&command->path);
  CLEANUP_SOURCE:
    _cairo_pattern_fini (&command->source.base);
  CLEANUP_COMMAND:
    _cairo_clip_fini (&command->header.clip);
    free (command);
    return status;
}

static cairo_int_status_t
_cairo_recording_surface_fill (void			*abstract_surface,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*source,
			       cairo_path_fixed_t	*path,
			       cairo_fill_rule_t	 fill_rule,
			       double			 tolerance,
			       cairo_antialias_t	 antialias,
			       cairo_clip_t		*clip)
{
    cairo_status_t status;
    cairo_recording_surface_t *recording_surface = abstract_surface;
    cairo_command_fill_t *command;

    command = malloc (sizeof (cairo_command_fill_t));
    if (unlikely (command == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status =_command_init (recording_surface,
			   &command->header, CAIRO_COMMAND_FILL, op, clip);
    if (unlikely (status))
	goto CLEANUP_COMMAND;

    status = _cairo_pattern_init_snapshot (&command->source.base, source);
    if (unlikely (status))
	goto CLEANUP_COMMAND;

    status = _cairo_path_fixed_init_copy (&command->path, path);
    if (unlikely (status))
	goto CLEANUP_SOURCE;

    command->fill_rule = fill_rule;
    command->tolerance = tolerance;
    command->antialias = antialias;

    status = _cairo_array_append (&recording_surface->commands, &command);
    if (unlikely (status))
	goto CLEANUP_PATH;

    return CAIRO_STATUS_SUCCESS;

  CLEANUP_PATH:
    _cairo_path_fixed_fini (&command->path);
  CLEANUP_SOURCE:
    _cairo_pattern_fini (&command->source.base);
  CLEANUP_COMMAND:
    _cairo_clip_fini (&command->header.clip);
    free (command);
    return status;
}

static cairo_bool_t
_cairo_recording_surface_has_show_text_glyphs (void *abstract_surface)
{
    return TRUE;
}

static cairo_int_status_t
_cairo_recording_surface_show_text_glyphs (void				*abstract_surface,
					   cairo_operator_t		 op,
					   const cairo_pattern_t	*source,
					   const char			*utf8,
					   int				 utf8_len,
					   cairo_glyph_t		*glyphs,
					   int				 num_glyphs,
					   const cairo_text_cluster_t	*clusters,
					   int				 num_clusters,
					   cairo_text_cluster_flags_t	 cluster_flags,
					   cairo_scaled_font_t		*scaled_font,
					   cairo_clip_t			*clip)
{
    cairo_status_t status;
    cairo_recording_surface_t *recording_surface = abstract_surface;
    cairo_command_show_text_glyphs_t *command;

    command = malloc (sizeof (cairo_command_show_text_glyphs_t));
    if (unlikely (command == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _command_init (recording_surface,
			    &command->header, CAIRO_COMMAND_SHOW_TEXT_GLYPHS,
			    op, clip);
    if (unlikely (status))
	goto CLEANUP_COMMAND;

    status = _cairo_pattern_init_snapshot (&command->source.base, source);
    if (unlikely (status))
	goto CLEANUP_COMMAND;

    command->utf8 = NULL;
    command->utf8_len = utf8_len;
    command->glyphs = NULL;
    command->num_glyphs = num_glyphs;
    command->clusters = NULL;
    command->num_clusters = num_clusters;

    if (utf8_len) {
	command->utf8 = malloc (utf8_len);
	if (unlikely (command->utf8 == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto CLEANUP_ARRAYS;
	}
	memcpy (command->utf8, utf8, utf8_len);
    }
    if (num_glyphs) {
	command->glyphs = _cairo_malloc_ab (num_glyphs, sizeof (glyphs[0]));
	if (unlikely (command->glyphs == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto CLEANUP_ARRAYS;
	}
	memcpy (command->glyphs, glyphs, sizeof (glyphs[0]) * num_glyphs);
    }
    if (num_clusters) {
	command->clusters = _cairo_malloc_ab (num_clusters, sizeof (clusters[0]));
	if (unlikely (command->clusters == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto CLEANUP_ARRAYS;
	}
	memcpy (command->clusters, clusters, sizeof (clusters[0]) * num_clusters);
    }

    command->cluster_flags = cluster_flags;

    command->scaled_font = cairo_scaled_font_reference (scaled_font);

    status = _cairo_array_append (&recording_surface->commands, &command);
    if (unlikely (status))
	goto CLEANUP_SCALED_FONT;

    return CAIRO_STATUS_SUCCESS;

  CLEANUP_SCALED_FONT:
    cairo_scaled_font_destroy (command->scaled_font);
  CLEANUP_ARRAYS:
    free (command->utf8);
    free (command->glyphs);
    free (command->clusters);

    _cairo_pattern_fini (&command->source.base);
  CLEANUP_COMMAND:
    _cairo_clip_fini (&command->header.clip);
    free (command);
    return status;
}

/**
 * _cairo_recording_surface_snapshot
 * @surface: a #cairo_surface_t which must be a recording surface
 *
 * Make an immutable copy of @surface. It is an error to call a
 * surface-modifying function on the result of this function.
 *
 * The caller owns the return value and should call
 * cairo_surface_destroy() when finished with it. This function will not
 * return %NULL, but will return a nil surface instead.
 *
 * Return value: The snapshot surface.
 **/
static cairo_surface_t *
_cairo_recording_surface_snapshot (void *abstract_other)
{
    cairo_recording_surface_t *other = abstract_other;
    cairo_recording_surface_t *recording_surface;
    cairo_status_t status;

    recording_surface = malloc (sizeof (cairo_recording_surface_t));
    if (unlikely (recording_surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&recording_surface->base,
			 &cairo_recording_surface_backend,
			 NULL, /* device */
			 other->base.content);

    recording_surface->extents_pixels = other->extents_pixels;
    recording_surface->extents = other->extents;
    recording_surface->unbounded = other->unbounded;
    recording_surface->content = other->content;

    _cairo_clip_init_copy (&recording_surface->clip, &other->clip);

    /* XXX We should in theory be able to reuse the original array, but we
     * need to handle reference cycles during subsurface and self-copy.
     */
    recording_surface->replay_start_idx = 0;
    recording_surface->base.is_clear = TRUE;

    _cairo_array_init (&recording_surface->commands, sizeof (cairo_command_t *));
    status = _cairo_recording_surface_replay (&other->base, &recording_surface->base);
    if (unlikely (status)) {
	cairo_surface_destroy (&recording_surface->base);
	return _cairo_surface_create_in_error (status);
    }

    return &recording_surface->base;
}

static cairo_bool_t
_cairo_recording_surface_get_extents (void		    *abstract_surface,
				      cairo_rectangle_int_t *rectangle)
{
    cairo_recording_surface_t *surface = abstract_surface;

    if (surface->unbounded)
	return FALSE;

    *rectangle = surface->extents;
    return TRUE;
}

/**
 * _cairo_surface_is_recording:
 * @surface: a #cairo_surface_t
 *
 * Checks if a surface is a #cairo_recording_surface_t
 *
 * Return value: %TRUE if the surface is a recording surface
 **/
cairo_bool_t
_cairo_surface_is_recording (const cairo_surface_t *surface)
{
    return surface->backend == &cairo_recording_surface_backend;
}

static const cairo_surface_backend_t cairo_recording_surface_backend = {
    CAIRO_SURFACE_TYPE_RECORDING,
    _cairo_recording_surface_create_similar,
    _cairo_recording_surface_finish,
    _cairo_recording_surface_acquire_source_image,
    _cairo_recording_surface_release_source_image,
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* create_span_renderer */
    NULL, /* check_span_renderer */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_recording_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    /* Here are the 5 basic drawing operations, (which are in some
     * sense the only things that cairo_recording_surface should need to
     * implement).  However, we implement the more generic show_text_glyphs
     * instead of show_glyphs.  One or the other is eough. */

    _cairo_recording_surface_paint,
    _cairo_recording_surface_mask,
    _cairo_recording_surface_stroke,
    _cairo_recording_surface_fill,
    NULL,

    _cairo_recording_surface_snapshot,

    NULL, /* is_similar */
    NULL, /* fill_stroke */
    NULL, /* create_solid_pattern_surface */
    NULL, /* can_repaint_solid_pattern_surface */

    _cairo_recording_surface_has_show_text_glyphs,
    _cairo_recording_surface_show_text_glyphs
};

cairo_int_status_t
_cairo_recording_surface_get_path (cairo_surface_t    *surface,
				   cairo_path_fixed_t *path)
{
    cairo_recording_surface_t *recording_surface;
    cairo_command_t **elements;
    int i, num_elements;
    cairo_int_status_t status;

    if (surface->status)
	return surface->status;

    recording_surface = (cairo_recording_surface_t *) surface;
    status = CAIRO_STATUS_SUCCESS;

    num_elements = recording_surface->commands.num_elements;
    elements = _cairo_array_index (&recording_surface->commands, 0);
    for (i = recording_surface->replay_start_idx; i < num_elements; i++) {
	cairo_command_t *command = elements[i];

	switch (command->header.type) {
	case CAIRO_COMMAND_PAINT:
	case CAIRO_COMMAND_MASK:
	    status = CAIRO_INT_STATUS_UNSUPPORTED;
	    break;

	case CAIRO_COMMAND_STROKE:
	{
	    cairo_traps_t traps;

	    _cairo_traps_init (&traps);

	    /* XXX call cairo_stroke_to_path() when that is implemented */
	    status = _cairo_path_fixed_stroke_to_traps (&command->stroke.path,
							&command->stroke.style,
							&command->stroke.ctm,
							&command->stroke.ctm_inverse,
							command->stroke.tolerance,
							&traps);

	    if (status == CAIRO_STATUS_SUCCESS)
		status = _cairo_traps_path (&traps, path);

	    _cairo_traps_fini (&traps);
	    break;
	}
	case CAIRO_COMMAND_FILL:
	{
	    status = _cairo_path_fixed_append (path,
					       &command->fill.path, CAIRO_DIRECTION_FORWARD,
					       0, 0);
	    break;
	}
	case CAIRO_COMMAND_SHOW_TEXT_GLYPHS:
	{
	    status = _cairo_scaled_font_glyph_path (command->show_text_glyphs.scaled_font,
						    command->show_text_glyphs.glyphs,
						    command->show_text_glyphs.num_glyphs,
						    path);
	    break;
	}

	default:
	    ASSERT_NOT_REACHED;
	}

	if (unlikely (status))
	    break;
    }

    return _cairo_surface_set_error (surface, status);
}

#define _clip(c) ((c)->header.clip.path ? &(c)->header.clip : NULL)
static cairo_status_t
_cairo_recording_surface_replay_internal (cairo_surface_t	     *surface,
					  const cairo_rectangle_int_t *surface_extents,
					  cairo_surface_t	     *target,
					  cairo_recording_replay_type_t type,
					  cairo_recording_region_type_t region)
{
    cairo_recording_surface_t *recording_surface;
    cairo_command_t **elements;
    int i, num_elements;
    cairo_int_status_t status;
    cairo_surface_wrapper_t wrapper;

    if (unlikely (surface->status))
	return surface->status;

    if (unlikely (target->status))
	return target->status;

    if (unlikely (surface->finished))
	return _cairo_error (CAIRO_STATUS_SURFACE_FINISHED);

    if (surface->is_clear)
	return CAIRO_STATUS_SUCCESS;

    assert (_cairo_surface_is_recording (surface));

    _cairo_surface_wrapper_init (&wrapper, target);
    _cairo_surface_wrapper_set_extents (&wrapper, surface_extents);

    recording_surface = (cairo_recording_surface_t *) surface;
    status = CAIRO_STATUS_SUCCESS;

    num_elements = recording_surface->commands.num_elements;
    elements = _cairo_array_index (&recording_surface->commands, 0);

    for (i = recording_surface->replay_start_idx; i < num_elements; i++) {
	cairo_command_t *command = elements[i];

	if (type == CAIRO_RECORDING_REPLAY && region != CAIRO_RECORDING_REGION_ALL) {
	    if (command->header.region != region)
		continue;
        }

	switch (command->header.type) {
	case CAIRO_COMMAND_PAINT:
	    status = _cairo_surface_wrapper_paint (&wrapper,
						   command->header.op,
						   &command->paint.source.base,
						   _clip (command));
	    break;

	case CAIRO_COMMAND_MASK:
	    status = _cairo_surface_wrapper_mask (&wrapper,
						  command->header.op,
						  &command->mask.source.base,
						  &command->mask.mask.base,
						  _clip (command));
	    break;

	case CAIRO_COMMAND_STROKE:
	{
	    status = _cairo_surface_wrapper_stroke (&wrapper,
						    command->header.op,
						    &command->stroke.source.base,
						    &command->stroke.path,
						    &command->stroke.style,
						    &command->stroke.ctm,
						    &command->stroke.ctm_inverse,
						    command->stroke.tolerance,
						    command->stroke.antialias,
						    _clip (command));
	    break;
	}
	case CAIRO_COMMAND_FILL:
	{
	    cairo_command_t *stroke_command;

	    stroke_command = NULL;
	    if (type != CAIRO_RECORDING_CREATE_REGIONS && i < num_elements - 1)
		stroke_command = elements[i + 1];

	    if (stroke_command != NULL &&
		type == CAIRO_RECORDING_REPLAY &&
		region != CAIRO_RECORDING_REGION_ALL)
	    {
		if (stroke_command->header.region != region)
		    stroke_command = NULL;
	    }

	    if (stroke_command != NULL &&
		stroke_command->header.type == CAIRO_COMMAND_STROKE &&
		_cairo_path_fixed_is_equal (&command->fill.path,
					    &stroke_command->stroke.path))
	    {
		status = _cairo_surface_wrapper_fill_stroke (&wrapper,
							     command->header.op,
							     &command->fill.source.base,
							     command->fill.fill_rule,
							     command->fill.tolerance,
							     command->fill.antialias,
							     &command->fill.path,
							     stroke_command->header.op,
							     &stroke_command->stroke.source.base,
							     &stroke_command->stroke.style,
							     &stroke_command->stroke.ctm,
							     &stroke_command->stroke.ctm_inverse,
							     stroke_command->stroke.tolerance,
							     stroke_command->stroke.antialias,
							     _clip (command));
		i++;
	    }
	    else
	    {
		status = _cairo_surface_wrapper_fill (&wrapper,
						      command->header.op,
						      &command->fill.source.base,
						      &command->fill.path,
						      command->fill.fill_rule,
						      command->fill.tolerance,
						      command->fill.antialias,
						      _clip (command));
	    }
	    break;
	}
	case CAIRO_COMMAND_SHOW_TEXT_GLYPHS:
	{
	    cairo_glyph_t *glyphs = command->show_text_glyphs.glyphs;
	    cairo_glyph_t *glyphs_copy;
	    int num_glyphs = command->show_text_glyphs.num_glyphs;

            /* show_text_glyphs is special because _cairo_surface_show_text_glyphs is allowed
	     * to modify the glyph array that's passed in.  We must always
	     * copy the array before handing it to the backend.
	     */
	    glyphs_copy = _cairo_malloc_ab (num_glyphs, sizeof (cairo_glyph_t));
	    if (unlikely (glyphs_copy == NULL)) {
		status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
		break;
	    }

	    memcpy (glyphs_copy, glyphs, sizeof (cairo_glyph_t) * num_glyphs);

	    status = _cairo_surface_wrapper_show_text_glyphs (&wrapper,
							      command->header.op,
							      &command->show_text_glyphs.source.base,
							      command->show_text_glyphs.utf8, command->show_text_glyphs.utf8_len,
							      glyphs_copy, num_glyphs,
							      command->show_text_glyphs.clusters, command->show_text_glyphs.num_clusters,
							      command->show_text_glyphs.cluster_flags,
							      command->show_text_glyphs.scaled_font,
							      _clip (command));
	    free (glyphs_copy);
	    break;
	}
	default:
	    ASSERT_NOT_REACHED;
	}

	if (type == CAIRO_RECORDING_CREATE_REGIONS) {
	    if (status == CAIRO_STATUS_SUCCESS) {
		command->header.region = CAIRO_RECORDING_REGION_NATIVE;
	    } else if (status == CAIRO_INT_STATUS_IMAGE_FALLBACK) {
		command->header.region = CAIRO_RECORDING_REGION_IMAGE_FALLBACK;
		status = CAIRO_STATUS_SUCCESS;
	    } else {
		assert (_cairo_status_is_error (status));
	    }
	}

	if (unlikely (status))
	    break;
    }

    /* free up any caches */
    for (i = recording_surface->replay_start_idx; i < num_elements; i++) {
	cairo_command_t *command = elements[i];

	_cairo_clip_drop_cache (&command->header.clip);
    }

    _cairo_surface_wrapper_fini (&wrapper);

    return _cairo_surface_set_error (surface, status);
}

/**
 * _cairo_recording_surface_replay:
 * @surface: the #cairo_recording_surface_t
 * @target: a target #cairo_surface_t onto which to replay the operations
 * @width_pixels: width of the surface, in pixels
 * @height_pixels: height of the surface, in pixels
 *
 * A recording surface can be "replayed" against any target surface,
 * after which the results in target will be identical to the results
 * that would have been obtained if the original operations applied to
 * the recording surface had instead been applied to the target surface.
 **/
cairo_status_t
_cairo_recording_surface_replay (cairo_surface_t *surface,
				 cairo_surface_t *target)
{
    return _cairo_recording_surface_replay_internal (surface, NULL,
						     target,
						     CAIRO_RECORDING_REPLAY,
						     CAIRO_RECORDING_REGION_ALL);
}

/* Replay recording to surface. When the return status of each operation is
 * one of %CAIRO_STATUS_SUCCESS, %CAIRO_INT_STATUS_UNSUPPORTED, or
 * %CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY the status of each operation
 * will be stored in the recording surface. Any other status will abort the
 * replay and return the status.
 */
cairo_status_t
_cairo_recording_surface_replay_and_create_regions (cairo_surface_t *surface,
						    cairo_surface_t *target)
{
    return _cairo_recording_surface_replay_internal (surface, NULL,
						     target,
						     CAIRO_RECORDING_CREATE_REGIONS,
						     CAIRO_RECORDING_REGION_ALL);
}

cairo_status_t
_cairo_recording_surface_replay_region (cairo_surface_t          *surface,
					const cairo_rectangle_int_t *surface_extents,
					cairo_surface_t          *target,
					cairo_recording_region_type_t  region)
{
    return _cairo_recording_surface_replay_internal (surface, surface_extents,
						     target,
						     CAIRO_RECORDING_REPLAY,
						     region);
}

static cairo_status_t
_recording_surface_get_ink_bbox (cairo_recording_surface_t *surface,
				 cairo_box_t *bbox,
				 const cairo_matrix_t *transform)
{
    cairo_surface_t *null_surface;
    cairo_surface_t *analysis_surface;
    cairo_status_t status;

    null_surface = _cairo_null_surface_create (surface->content);
    analysis_surface = _cairo_analysis_surface_create (null_surface);
    cairo_surface_destroy (null_surface);

    status = analysis_surface->status;
    if (unlikely (status))
	return status;

    if (transform != NULL)
	_cairo_analysis_surface_set_ctm (analysis_surface, transform);

    status = _cairo_recording_surface_replay (&surface->base, analysis_surface);
    _cairo_analysis_surface_get_bounding_box (analysis_surface, bbox);
    cairo_surface_destroy (analysis_surface);

    return status;
}

/**
 * cairo_recording_surface_ink_extents:
 * @surface: a #cairo_recording_surface_t
 * @x0: the x-coordinate of the top-left of the ink bounding box
 * @y0: the y-coordinate of the top-left of the ink bounding box
 * @width: the width of the ink bounding box
 * @height: the height of the ink bounding box
 *
 * Measures the extents of the operations stored within the recording-surface.
 * This is useful to compute the required size of an image surface (or
 * equivalent) into which to replay the full sequence of drawing operations.
 *
 * Since: 1.10
 **/
void
cairo_recording_surface_ink_extents (cairo_surface_t *surface,
				     double *x0,
				     double *y0,
				     double *width,
				     double *height)
{
    cairo_status_t status;
    cairo_box_t bbox;

    memset (&bbox, 0, sizeof (bbox));

    if (! _cairo_surface_is_recording (surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	goto DONE;
    }

    status = _recording_surface_get_ink_bbox ((cairo_recording_surface_t *) surface,
					 &bbox,
					 NULL);
    if (unlikely (status))
	status = _cairo_surface_set_error (surface, status);

DONE:
    if (x0)
	*x0 = _cairo_fixed_to_double (bbox.p1.x);
    if (y0)
	*y0 = _cairo_fixed_to_double (bbox.p1.y);
    if (width)
	*width = _cairo_fixed_to_double (bbox.p2.x - bbox.p1.x);
    if (height)
	*height = _cairo_fixed_to_double (bbox.p2.y - bbox.p1.y);
}

cairo_status_t
_cairo_recording_surface_get_bbox (cairo_recording_surface_t *surface,
				   cairo_box_t *bbox,
				   const cairo_matrix_t *transform)
{
    if (! surface->unbounded) {
	_cairo_box_from_rectangle (bbox, &surface->extents);
	if (transform != NULL)
	    _cairo_matrix_transform_bounding_box_fixed (transform, bbox, NULL);

	return CAIRO_STATUS_SUCCESS;
    }

    return _recording_surface_get_ink_bbox (surface, bbox, transform);
}

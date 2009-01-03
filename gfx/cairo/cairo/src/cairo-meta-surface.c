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
 *	Kristian Høgsberg <krh@redhat.com>
 *	Carl Worth <cworth@cworth.org>
 *	Adrian Johnson <ajohnson@redneon.com>
 */

/* A meta surface is a surface that records all drawing operations at
 * the highest level of the surface backend interface, (that is, the
 * level of paint, mask, stroke, fill, and show_text_glyphs). The meta
 * surface can then be "replayed" against any target surface with:
 *
 *	_cairo_meta_surface_replay (meta, target);
 *
 * after which the results in target will be identical to the results
 * that would have been obtained if the original operations applied to
 * the meta surface had instead been applied to the target surface.
 *
 * The recording phase of the meta surface is careful to snapshot all
 * necessary objects (paths, patterns, etc.), in order to achieve
 * accurate replay. The efficiency of the meta surface could be
 * improved by improving the implementation of snapshot for the
 * various objects. For example, it would be nice to have a
 * copy-on-write implementation for _cairo_surface_snapshot.
 */

#include "cairoint.h"
#include "cairo-meta-surface-private.h"
#include "cairo-clip-private.h"

typedef enum {
    CAIRO_META_REPLAY,
    CAIRO_META_CREATE_REGIONS
} cairo_meta_replay_type_t;

static const cairo_surface_backend_t cairo_meta_surface_backend;

/* Currently all meta surfaces do have a size which should be passed
 * in as the maximum size of any target surface against which the
 * meta-surface will ever be replayed.
 *
 * XXX: The naming of "pixels" in the size here is a misnomer. It's
 * actually a size in whatever device-space units are desired (again,
 * according to the intended replay target). This should likely also
 * be changed to use doubles not ints.
 */
cairo_surface_t *
_cairo_meta_surface_create (cairo_content_t	content,
			    int			width_pixels,
			    int			height_pixels)
{
    cairo_meta_surface_t *meta;

    meta = malloc (sizeof (cairo_meta_surface_t));
    if (meta == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&meta->base, &cairo_meta_surface_backend,
			 content);

    meta->content = content;
    meta->width_pixels = width_pixels;
    meta->height_pixels = height_pixels;

    _cairo_array_init (&meta->commands, sizeof (cairo_command_t *));
    meta->commands_owner = NULL;

    meta->is_clipped = FALSE;
    meta->replay_start_idx = 0;

    return &meta->base;
}

static cairo_surface_t *
_cairo_meta_surface_create_similar (void	       *abstract_surface,
				    cairo_content_t	content,
				    int			width,
				    int			height)
{
    return _cairo_meta_surface_create (content, width, height);
}

static cairo_status_t
_cairo_meta_surface_finish (void *abstract_surface)
{
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_t *command;
    cairo_command_t **elements;
    int i, num_elements;

    if (meta->commands_owner) {
	cairo_surface_destroy (meta->commands_owner);
	return CAIRO_STATUS_SUCCESS;
    }

    num_elements = meta->commands.num_elements;
    elements = _cairo_array_index (&meta->commands, 0);
    for (i = 0; i < num_elements; i++) {
	command = elements[i];
	switch (command->header.type) {

	/* 5 basic drawing operations */

	case CAIRO_COMMAND_PAINT:
	    _cairo_pattern_fini (&command->paint.source.base);
	    free (command);
	    break;

	case CAIRO_COMMAND_MASK:
	    _cairo_pattern_fini (&command->mask.source.base);
	    _cairo_pattern_fini (&command->mask.mask.base);
	    free (command);
	    break;

	case CAIRO_COMMAND_STROKE:
	    _cairo_pattern_fini (&command->stroke.source.base);
	    _cairo_path_fixed_fini (&command->stroke.path);
	    _cairo_stroke_style_fini (&command->stroke.style);
	    free (command);
	    break;

	case CAIRO_COMMAND_FILL:
	    _cairo_pattern_fini (&command->fill.source.base);
	    _cairo_path_fixed_fini (&command->fill.path);
	    free (command);
	    break;

	case CAIRO_COMMAND_SHOW_TEXT_GLYPHS:
	    _cairo_pattern_fini (&command->show_text_glyphs.source.base);
	    free (command->show_text_glyphs.utf8);
	    free (command->show_text_glyphs.glyphs);
	    free (command->show_text_glyphs.clusters);
	    cairo_scaled_font_destroy (command->show_text_glyphs.scaled_font);
	    free (command);
	    break;

	/* Other junk. */
	case CAIRO_COMMAND_INTERSECT_CLIP_PATH:
	    if (command->intersect_clip_path.path_pointer)
		_cairo_path_fixed_fini (&command->intersect_clip_path.path);
	    free (command);
	    break;

	default:
	    ASSERT_NOT_REACHED;
	}
    }

    _cairo_array_fini (&meta->commands);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_meta_surface_acquire_source_image (void			 *abstract_surface,
					  cairo_image_surface_t	**image_out,
					  void			**image_extra)
{
    cairo_status_t status;
    cairo_meta_surface_t *surface = abstract_surface;
    cairo_surface_t *image;

    image = _cairo_image_surface_create_with_content (surface->content,
						      surface->width_pixels,
						      surface->height_pixels);

    status = _cairo_meta_surface_replay (&surface->base, image);
    if (status) {
	cairo_surface_destroy (image);
	return status;
    }

    *image_out = (cairo_image_surface_t *) image;
    *image_extra = NULL;

    return status;
}

static void
_cairo_meta_surface_release_source_image (void			*abstract_surface,
					  cairo_image_surface_t	*image,
					  void			*image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_int_status_t
_cairo_meta_surface_paint (void			*abstract_surface,
			   cairo_operator_t	 op,
			   cairo_pattern_t	*source)
{
    cairo_status_t status;
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_paint_t *command;

    command = malloc (sizeof (cairo_command_paint_t));
    if (command == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    command->header.type = CAIRO_COMMAND_PAINT;
    command->header.region = CAIRO_META_REGION_ALL;
    command->op = op;

    status = _cairo_pattern_init_snapshot (&command->source.base, source);
    if (status)
	goto CLEANUP_COMMAND;

    status = _cairo_array_append (&meta->commands, &command);
    if (status)
	goto CLEANUP_SOURCE;

    /* An optimisation that takes care to not replay what was done
     * before surface is cleared. We don't erase recorded commands
     * since we may have earlier snapshots of this surface. */
    if (op == CAIRO_OPERATOR_CLEAR && !meta->is_clipped)
	meta->replay_start_idx = meta->commands.num_elements;

    return CAIRO_STATUS_SUCCESS;

  CLEANUP_SOURCE:
    _cairo_pattern_fini (&command->source.base);
  CLEANUP_COMMAND:
    free (command);
    return status;
}

static cairo_int_status_t
_cairo_meta_surface_mask (void			*abstract_surface,
			  cairo_operator_t	 op,
			  cairo_pattern_t	*source,
			  cairo_pattern_t	*mask)
{
    cairo_status_t status;
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_mask_t *command;

    command = malloc (sizeof (cairo_command_mask_t));
    if (command == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    command->header.type = CAIRO_COMMAND_MASK;
    command->header.region = CAIRO_META_REGION_ALL;
    command->op = op;

    status = _cairo_pattern_init_snapshot (&command->source.base, source);
    if (status)
	goto CLEANUP_COMMAND;

    status = _cairo_pattern_init_snapshot (&command->mask.base, mask);
    if (status)
	goto CLEANUP_SOURCE;

    status = _cairo_array_append (&meta->commands, &command);
    if (status)
	goto CLEANUP_MASK;

    return CAIRO_STATUS_SUCCESS;

  CLEANUP_MASK:
    _cairo_pattern_fini (&command->mask.base);
  CLEANUP_SOURCE:
    _cairo_pattern_fini (&command->source.base);
  CLEANUP_COMMAND:
    free (command);
    return status;
}

static cairo_int_status_t
_cairo_meta_surface_stroke (void			*abstract_surface,
			    cairo_operator_t		 op,
			    cairo_pattern_t		*source,
			    cairo_path_fixed_t		*path,
			    cairo_stroke_style_t	*style,
			    cairo_matrix_t		*ctm,
			    cairo_matrix_t		*ctm_inverse,
			    double			 tolerance,
			    cairo_antialias_t		 antialias)
{
    cairo_status_t status;
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_stroke_t *command;

    command = malloc (sizeof (cairo_command_stroke_t));
    if (command == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    command->header.type = CAIRO_COMMAND_STROKE;
    command->header.region = CAIRO_META_REGION_ALL;
    command->op = op;

    status = _cairo_pattern_init_snapshot (&command->source.base, source);
    if (status)
	goto CLEANUP_COMMAND;

    status = _cairo_path_fixed_init_copy (&command->path, path);
    if (status)
	goto CLEANUP_SOURCE;

    status = _cairo_stroke_style_init_copy (&command->style, style);
    if (status)
	goto CLEANUP_PATH;

    command->ctm = *ctm;
    command->ctm_inverse = *ctm_inverse;
    command->tolerance = tolerance;
    command->antialias = antialias;

    status = _cairo_array_append (&meta->commands, &command);
    if (status)
	goto CLEANUP_STYLE;

    return CAIRO_STATUS_SUCCESS;

  CLEANUP_STYLE:
    _cairo_stroke_style_fini (&command->style);
  CLEANUP_PATH:
    _cairo_path_fixed_fini (&command->path);
  CLEANUP_SOURCE:
    _cairo_pattern_fini (&command->source.base);
  CLEANUP_COMMAND:
    free (command);
    return status;
}

static cairo_int_status_t
_cairo_meta_surface_fill (void			*abstract_surface,
			  cairo_operator_t	 op,
			  cairo_pattern_t	*source,
			  cairo_path_fixed_t	*path,
			  cairo_fill_rule_t	 fill_rule,
			  double		 tolerance,
			  cairo_antialias_t	 antialias)
{
    cairo_status_t status;
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_fill_t *command;

    command = malloc (sizeof (cairo_command_fill_t));
    if (command == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    command->header.type = CAIRO_COMMAND_FILL;
    command->header.region = CAIRO_META_REGION_ALL;
    command->op = op;

    status = _cairo_pattern_init_snapshot (&command->source.base, source);
    if (status)
	goto CLEANUP_COMMAND;

    status = _cairo_path_fixed_init_copy (&command->path, path);
    if (status)
	goto CLEANUP_SOURCE;

    command->fill_rule = fill_rule;
    command->tolerance = tolerance;
    command->antialias = antialias;

    status = _cairo_array_append (&meta->commands, &command);
    if (status)
	goto CLEANUP_PATH;

    return CAIRO_STATUS_SUCCESS;

  CLEANUP_PATH:
    _cairo_path_fixed_fini (&command->path);
  CLEANUP_SOURCE:
    _cairo_pattern_fini (&command->source.base);
  CLEANUP_COMMAND:
    free (command);
    return status;
}

static cairo_bool_t
_cairo_meta_surface_has_show_text_glyphs (void *abstract_surface)
{
    return TRUE;
}

static cairo_int_status_t
_cairo_meta_surface_show_text_glyphs (void			    *abstract_surface,
				      cairo_operator_t		     op,
				      cairo_pattern_t		    *source,
				      const char		    *utf8,
				      int			     utf8_len,
				      cairo_glyph_t		    *glyphs,
				      int			     num_glyphs,
				      const cairo_text_cluster_t    *clusters,
				      int			     num_clusters,
				      cairo_text_cluster_flags_t     cluster_flags,
				      cairo_scaled_font_t	    *scaled_font)
{
    cairo_status_t status;
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_show_text_glyphs_t *command;

    command = malloc (sizeof (cairo_command_show_text_glyphs_t));
    if (command == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    command->header.type = CAIRO_COMMAND_SHOW_TEXT_GLYPHS;
    command->header.region = CAIRO_META_REGION_ALL;
    command->op = op;

    status = _cairo_pattern_init_snapshot (&command->source.base, source);
    if (status)
	goto CLEANUP_COMMAND;

    command->utf8 = NULL;
    command->utf8_len = utf8_len;
    command->glyphs = NULL;
    command->num_glyphs = num_glyphs;
    command->clusters = NULL;
    command->num_clusters = num_clusters;

    if (utf8_len) {
	command->utf8 = malloc (utf8_len);
	if (command->utf8 == NULL) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto CLEANUP_ARRAYS;
	}
	memcpy (command->utf8, utf8, utf8_len);
    }
    if (num_glyphs) {
	command->glyphs = _cairo_malloc_ab (num_glyphs, sizeof (glyphs[0]));
	if (command->glyphs == NULL) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto CLEANUP_ARRAYS;
	}
	memcpy (command->glyphs, glyphs, sizeof (glyphs[0]) * num_glyphs);
    }
    if (num_clusters) {
	command->clusters = _cairo_malloc_ab (num_clusters, sizeof (clusters[0]));
	if (command->clusters == NULL) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto CLEANUP_ARRAYS;
	}
	memcpy (command->clusters, clusters, sizeof (clusters[0]) * num_clusters);
    }

    command->cluster_flags = cluster_flags;

    command->scaled_font = cairo_scaled_font_reference (scaled_font);

    status = _cairo_array_append (&meta->commands, &command);
    if (status)
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
    free (command);
    return status;
}

/**
 * _cairo_meta_surface_snapshot
 * @surface: a #cairo_surface_t which must be a meta surface
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
_cairo_meta_surface_snapshot (void *abstract_other)
{
    cairo_meta_surface_t *other = abstract_other;
    cairo_meta_surface_t *meta;

    meta = malloc (sizeof (cairo_meta_surface_t));
    if (meta == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&meta->base, &cairo_meta_surface_backend,
			 other->base.content);
    meta->base.is_snapshot = TRUE;

    meta->width_pixels = other->width_pixels;
    meta->height_pixels = other->height_pixels;
    meta->replay_start_idx = other->replay_start_idx;
    meta->content = other->content;

    _cairo_array_init_snapshot (&meta->commands, &other->commands);
    meta->commands_owner = cairo_surface_reference (&other->base);

    return &meta->base;
}

static cairo_int_status_t
_cairo_meta_surface_intersect_clip_path (void		    *dst,
					 cairo_path_fixed_t *path,
					 cairo_fill_rule_t   fill_rule,
					 double		     tolerance,
					 cairo_antialias_t   antialias)
{
    cairo_meta_surface_t *meta = dst;
    cairo_command_intersect_clip_path_t *command;
    cairo_status_t status;

    command = malloc (sizeof (cairo_command_intersect_clip_path_t));
    if (command == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    command->header.type = CAIRO_COMMAND_INTERSECT_CLIP_PATH;
    command->header.region = CAIRO_META_REGION_ALL;

    if (path) {
	status = _cairo_path_fixed_init_copy (&command->path, path);
	if (status) {
	    free (command);
	    return status;
	}
	command->path_pointer = &command->path;
	meta->is_clipped = TRUE;
    } else {
	command->path_pointer = NULL;
	meta->is_clipped = FALSE;
    }
    command->fill_rule = fill_rule;
    command->tolerance = tolerance;
    command->antialias = antialias;

    status = _cairo_array_append (&meta->commands, &command);
    if (status) {
	if (path)
	    _cairo_path_fixed_fini (&command->path);
	free (command);
	return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

/* Currently, we're using as the "size" of a meta surface the largest
 * surface size against which the meta-surface is expected to be
 * replayed, (as passed in to _cairo_meta_surface_create).
 */
static cairo_int_status_t
_cairo_meta_surface_get_extents (void			 *abstract_surface,
				 cairo_rectangle_int_t   *rectangle)
{
    cairo_meta_surface_t *surface = abstract_surface;

    if (surface->width_pixels == -1 && surface->height_pixels == -1)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    rectangle->x = 0;
    rectangle->y = 0;
    rectangle->width = surface->width_pixels;
    rectangle->height = surface->height_pixels;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_surface_is_meta:
 * @surface: a #cairo_surface_t
 *
 * Checks if a surface is a #cairo_meta_surface_t
 *
 * Return value: %TRUE if the surface is a meta surface
 **/
cairo_bool_t
_cairo_surface_is_meta (const cairo_surface_t *surface)
{
    return surface->backend == &cairo_meta_surface_backend;
}

static const cairo_surface_backend_t cairo_meta_surface_backend = {
    CAIRO_INTERNAL_SURFACE_TYPE_META,
    _cairo_meta_surface_create_similar,
    _cairo_meta_surface_finish,
    _cairo_meta_surface_acquire_source_image,
    _cairo_meta_surface_release_source_image,
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    NULL, /* show_page */
    NULL, /* set_clip_region */
    _cairo_meta_surface_intersect_clip_path,
    _cairo_meta_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    /* Here are the 5 basic drawing operations, (which are in some
     * sense the only things that cairo_meta_surface should need to
     * implement).  However, we implement the more generic show_text_glyphs
     * instead of show_glyphs.  One or the other is eough. */

    _cairo_meta_surface_paint,
    _cairo_meta_surface_mask,
    _cairo_meta_surface_stroke,
    _cairo_meta_surface_fill,
    NULL,

    _cairo_meta_surface_snapshot,

    NULL, /* is_similar */
    NULL, /* reset */
    NULL, /* fill_stroke */
    NULL, /* create_solid_pattern_surface */

    _cairo_meta_surface_has_show_text_glyphs,
    _cairo_meta_surface_show_text_glyphs
};

static cairo_path_fixed_t *
_cairo_command_get_path (cairo_command_t *command)
{
    switch (command->header.type) {
    case CAIRO_COMMAND_PAINT:
    case CAIRO_COMMAND_MASK:
    case CAIRO_COMMAND_SHOW_TEXT_GLYPHS:
	return NULL;
    case CAIRO_COMMAND_STROKE:
	return &command->stroke.path;
    case CAIRO_COMMAND_FILL:
	return &command->fill.path;
    case CAIRO_COMMAND_INTERSECT_CLIP_PATH:
	return command->intersect_clip_path.path_pointer;
    }

    ASSERT_NOT_REACHED;
    return NULL;
}

cairo_int_status_t
_cairo_meta_surface_get_path (cairo_surface_t	 *surface,
			      cairo_path_fixed_t *path)
{
    cairo_meta_surface_t *meta;
    cairo_command_t *command, **elements;
    int i, num_elements;
    cairo_int_status_t status;

    if (surface->status)
	return surface->status;

    meta = (cairo_meta_surface_t *) surface;
    status = CAIRO_STATUS_SUCCESS;

    num_elements = meta->commands.num_elements;
    elements = _cairo_array_index (&meta->commands, 0);
    for (i = meta->replay_start_idx; i < num_elements; i++) {
	command = elements[i];

	switch (command->header.type) {
	case CAIRO_COMMAND_PAINT:
	case CAIRO_COMMAND_MASK:
	case CAIRO_COMMAND_INTERSECT_CLIP_PATH:
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
	    status = _cairo_path_fixed_append (path, &command->fill.path, CAIRO_DIRECTION_FORWARD);
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

	if (status)
	    break;
    }

    return _cairo_surface_set_error (surface, status);
}

static cairo_status_t
_cairo_meta_surface_replay_internal (cairo_surface_t	     *surface,
				     cairo_surface_t	     *target,
				     cairo_meta_replay_type_t type,
				     cairo_meta_region_type_t region)
{
    cairo_meta_surface_t *meta;
    cairo_command_t *command, **elements;
    int i, num_elements;
    cairo_int_status_t status, status2;
    cairo_clip_t clip, *old_clip;
    cairo_bool_t has_device_transform = _cairo_surface_has_device_transform (target);
    cairo_matrix_t *device_transform = &target->device_transform;
    cairo_path_fixed_t path_copy, *dev_path;

    if (surface->status)
	return surface->status;

    if (target->status)
	return _cairo_surface_set_error (surface, target->status);

    meta = (cairo_meta_surface_t *) surface;
    status = CAIRO_STATUS_SUCCESS;

    _cairo_clip_init (&clip, target);
    old_clip = _cairo_surface_get_clip (target);

    num_elements = meta->commands.num_elements;
    elements = _cairo_array_index (&meta->commands, 0);
    for (i = meta->replay_start_idx; i < num_elements; i++) {
	command = elements[i];

	if (type == CAIRO_META_REPLAY && region != CAIRO_META_REGION_ALL) {
	    if (command->header.region != region)
		continue;
        }

	/* For all commands except intersect_clip_path, we have to
	 * ensure the current clip gets set on the surface. */
	if (command->header.type != CAIRO_COMMAND_INTERSECT_CLIP_PATH) {
	    status = _cairo_surface_set_clip (target, &clip);
	    if (status)
		break;
	}

	dev_path = _cairo_command_get_path (command);
	if (dev_path && has_device_transform) {
	    status = _cairo_path_fixed_init_copy (&path_copy, dev_path);
	    if (status)
		break;
	    _cairo_path_fixed_transform (&path_copy, device_transform);
	    dev_path = &path_copy;
	}

	switch (command->header.type) {
	case CAIRO_COMMAND_PAINT:
	    status = _cairo_surface_paint (target,
					   command->paint.op,
					   &command->paint.source.base);
	    break;
	case CAIRO_COMMAND_MASK:
	    status = _cairo_surface_mask (target,
					  command->mask.op,
					  &command->mask.source.base,
					  &command->mask.mask.base);
	    break;
	case CAIRO_COMMAND_STROKE:
	{
	    cairo_matrix_t dev_ctm = command->stroke.ctm;
	    cairo_matrix_t dev_ctm_inverse = command->stroke.ctm_inverse;

	    if (has_device_transform) {
		cairo_matrix_multiply (&dev_ctm, &dev_ctm, device_transform);
		cairo_matrix_multiply (&dev_ctm_inverse,
				       &target->device_transform_inverse,
				       &dev_ctm_inverse);
	    }

	    status = _cairo_surface_stroke (target,
					    command->stroke.op,
					    &command->stroke.source.base,
					    dev_path,
					    &command->stroke.style,
					    &dev_ctm,
					    &dev_ctm_inverse,
					    command->stroke.tolerance,
					    command->stroke.antialias);
	    break;
	}
	case CAIRO_COMMAND_FILL:
	{
	    cairo_command_t *stroke_command;

	    if (type != CAIRO_META_CREATE_REGIONS)
		stroke_command = (i < num_elements - 1) ? elements[i + 1] : NULL;
	    else
		stroke_command = NULL;

	    if (stroke_command != NULL &&
		type == CAIRO_META_REPLAY && region != CAIRO_META_REGION_ALL)
	    {
		if (stroke_command->header.region != region)
		    stroke_command = NULL;
	    }
	    if (stroke_command != NULL &&
		stroke_command->header.type == CAIRO_COMMAND_STROKE &&
		_cairo_path_fixed_is_equal (dev_path, _cairo_command_get_path (stroke_command))) {
		cairo_matrix_t dev_ctm;
		cairo_matrix_t dev_ctm_inverse;

		dev_ctm = stroke_command->stroke.ctm;
		dev_ctm_inverse = stroke_command->stroke.ctm_inverse;

		if (has_device_transform) {
		    cairo_matrix_multiply (&dev_ctm, &dev_ctm, device_transform);
		    cairo_matrix_multiply (&dev_ctm_inverse,
					   &surface->device_transform_inverse,
					   &dev_ctm_inverse);
		}

		status = _cairo_surface_fill_stroke (target,
						     command->fill.op,
						     &command->fill.source.base,
						     command->fill.fill_rule,
						     command->fill.tolerance,
						     command->fill.antialias,
						     dev_path,
						     stroke_command->stroke.op,
						     &stroke_command->stroke.source.base,
						     &stroke_command->stroke.style,
						     &dev_ctm,
						     &dev_ctm_inverse,
						     stroke_command->stroke.tolerance,
						     stroke_command->stroke.antialias);
		i++;
	    } else
		status = _cairo_surface_fill (target,
					      command->fill.op,
					      &command->fill.source.base,
					      dev_path,
					      command->fill.fill_rule,
					      command->fill.tolerance,
					      command->fill.antialias);
	    break;
	}
	case CAIRO_COMMAND_SHOW_TEXT_GLYPHS:
	{
	    cairo_glyph_t *glyphs = command->show_text_glyphs.glyphs;
	    cairo_glyph_t *dev_glyphs;
	    int i, num_glyphs = command->show_text_glyphs.num_glyphs;

            /* show_text_glyphs is special because _cairo_surface_show_text_glyphs is allowed
	     * to modify the glyph array that's passed in.  We must always
	     * copy the array before handing it to the backend.
	     */
	    dev_glyphs = _cairo_malloc_ab (num_glyphs, sizeof (cairo_glyph_t));
	    if (dev_glyphs == NULL) {
		status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
		break;
	    }

	    if (has_device_transform) {
		for (i = 0; i < num_glyphs; i++) {
		    dev_glyphs[i] = glyphs[i];
		    cairo_matrix_transform_point (device_transform,
						  &dev_glyphs[i].x,
						  &dev_glyphs[i].y);
		}
	    } else {
		memcpy (dev_glyphs, glyphs, sizeof (cairo_glyph_t) * num_glyphs);
	    }

	    status = _cairo_surface_show_text_glyphs	(target,
							 command->show_text_glyphs.op,
							 &command->show_text_glyphs.source.base,
							 command->show_text_glyphs.utf8, command->show_text_glyphs.utf8_len,
							 dev_glyphs, num_glyphs,
							 command->show_text_glyphs.clusters, command->show_text_glyphs.num_clusters,
							 command->show_text_glyphs.cluster_flags,
							 command->show_text_glyphs.scaled_font);

	    free (dev_glyphs);
	    break;
	}
	case CAIRO_COMMAND_INTERSECT_CLIP_PATH:
	    /* XXX Meta surface clipping is broken and requires some
	     * cairo-gstate.c rewriting.  Work around it for now. */
	    if (dev_path == NULL)
		_cairo_clip_reset (&clip);
	    else
		status = _cairo_clip_clip (&clip, dev_path,
					   command->intersect_clip_path.fill_rule,
					   command->intersect_clip_path.tolerance,
					   command->intersect_clip_path.antialias,
					   target);
	    break;
	default:
	    ASSERT_NOT_REACHED;
	}

	if (dev_path == &path_copy)
	    _cairo_path_fixed_fini (&path_copy);

	if (type == CAIRO_META_CREATE_REGIONS) {
	    if (status == CAIRO_STATUS_SUCCESS) {
		command->header.region = CAIRO_META_REGION_NATIVE;
	    } else if (status == CAIRO_INT_STATUS_IMAGE_FALLBACK) {
		command->header.region = CAIRO_META_REGION_IMAGE_FALLBACK;
		status = CAIRO_STATUS_SUCCESS;
	    }
	}

	if (status)
	    break;
    }

    _cairo_clip_reset (&clip);
    status2 = _cairo_surface_set_clip (target, old_clip);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;

    return _cairo_surface_set_error (surface, status);
}

cairo_status_t
_cairo_meta_surface_replay (cairo_surface_t *surface,
			    cairo_surface_t *target)
{
    return _cairo_meta_surface_replay_internal (surface,
						target,
						CAIRO_META_REPLAY,
						CAIRO_META_REGION_ALL);
}

/* Replay meta to surface. When the return status of each operation is
 * one of %CAIRO_STATUS_SUCCESS, %CAIRO_INT_STATUS_UNSUPPORTED, or
 * %CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY the status of each operation
 * will be stored in the meta surface. Any other status will abort the
 * replay and return the status.
 */
cairo_status_t
_cairo_meta_surface_replay_and_create_regions (cairo_surface_t *surface,
					       cairo_surface_t *target)
{
    return _cairo_meta_surface_replay_internal (surface,
						target,
						CAIRO_META_CREATE_REGIONS,
						CAIRO_META_REGION_ALL);
}

cairo_status_t
_cairo_meta_surface_replay_region (cairo_surface_t          *surface,
				   cairo_surface_t          *target,
				   cairo_meta_region_type_t  region)
{
    return _cairo_meta_surface_replay_internal (surface,
						target,
						CAIRO_META_REPLAY,
						region);
}

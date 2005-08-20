/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc
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
 */

#include "cairoint.h"
#include "cairo-meta-surface-private.h"
#include "cairo-gstate-private.h"

static const cairo_surface_backend_t cairo_meta_surface_backend;

cairo_surface_t *
_cairo_meta_surface_create (double width, double height)
{
    cairo_meta_surface_t *meta;

    meta = malloc (sizeof (cairo_meta_surface_t));
    if (meta == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    meta->width = width;
    meta->height = height;
    _cairo_surface_init (&meta->base, &cairo_meta_surface_backend);
    _cairo_array_init (&meta->commands, sizeof (cairo_command_t *));

    return &meta->base;
}

static cairo_surface_t *
_cairo_meta_surface_create_similar (void	       *abstract_surface,
				    cairo_content_t	content,
				    int			width,
				    int			height)
{
    return _cairo_meta_surface_create (width, height);
}

static cairo_status_t
_cairo_meta_surface_finish (void *abstract_surface)
{
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_t *command;
    cairo_command_t **elements;
    int i, num_elements;

    num_elements = meta->commands.num_elements;
    elements = (cairo_command_t **) meta->commands.elements;
    for (i = 0; i < num_elements; i++) {
	command = elements[i];
	switch (command->type) {
	case CAIRO_COMMAND_COMPOSITE:
	    _cairo_pattern_fini (&command->composite.src_pattern.base);
	    if (command->composite.mask_pattern_pointer)
		_cairo_pattern_fini (command->composite.mask_pattern_pointer);
	    free (command);
	    break;

	case CAIRO_COMMAND_FILL_RECTANGLES:
	    free (command->fill_rectangles.rects);
	    free (command);
	    break;

	case CAIRO_COMMAND_COMPOSITE_TRAPEZOIDS:
	    _cairo_pattern_fini (&command->composite_trapezoids.pattern.base);
	    free (command->composite_trapezoids.traps);
	    free (command);
	    break;

	case CAIRO_COMMAND_INTERSECT_CLIP_PATH:
	    if (command->intersect_clip_path.path_pointer)
		_cairo_path_fixed_fini (&command->intersect_clip_path.path);
	    free (command);
	    break;

	case CAIRO_COMMAND_SHOW_GLYPHS:
	    cairo_scaled_font_destroy (command->show_glyphs.scaled_font);
	    _cairo_pattern_fini (&command->show_glyphs.pattern.base);
	    free (command->show_glyphs.glyphs);
	    free (command);
	    break;

	case CAIRO_COMMAND_FILL_PATH:
	    _cairo_pattern_fini (&command->fill_path.pattern.base);
	    _cairo_path_fixed_fini (&command->fill_path.path);
	    free (command);
	    break;

	default:
	    ASSERT_NOT_REACHED;
	}
    }

    _cairo_array_fini (&meta->commands);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_meta_surface_composite (cairo_operator_t	operator,
			       cairo_pattern_t	*src_pattern,
			       cairo_pattern_t	*mask_pattern,
			       void		*abstract_surface,
			       int		src_x,
			       int		src_y,
			       int		mask_x,
			       int		mask_y,
			       int		dst_x,
			       int		dst_y,
			       unsigned int	width,
			       unsigned int	height)
{
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_composite_t *command;

    command = malloc (sizeof (cairo_command_composite_t));
    if (command == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    command->type = CAIRO_COMMAND_COMPOSITE;
    command->operator = operator;
    _cairo_pattern_init_copy (&command->src_pattern.base, src_pattern);
    if (mask_pattern) {
	_cairo_pattern_init_copy (&command->mask_pattern.base, mask_pattern);
	command->mask_pattern_pointer = &command->mask_pattern.base;
    } else {
	command->mask_pattern_pointer = NULL;
    }
	
    command->src_x = src_x;
    command->src_y = src_y;
    command->mask_x = mask_x;
    command->mask_y = mask_y;
    command->dst_x = dst_x;
    command->dst_y = dst_y;
    command->width = width;
    command->height = height;

    if (_cairo_array_append (&meta->commands, &command, 1) == NULL) {
	_cairo_pattern_fini (&command->src_pattern.base);
	_cairo_pattern_fini (command->mask_pattern_pointer);
	free (command);
	return CAIRO_STATUS_NO_MEMORY;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_meta_surface_fill_rectangles (void			*abstract_surface,
				     cairo_operator_t		operator,
				     const cairo_color_t	*color,
				     cairo_rectangle_t		*rects,
				     int			num_rects)
{
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_fill_rectangles_t *command;

    command = malloc (sizeof (cairo_command_fill_rectangles_t));
    if (command == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    command->type = CAIRO_COMMAND_FILL_RECTANGLES;
    command->operator = operator;
    command->color = *color;

    command->rects = malloc (sizeof (cairo_rectangle_t) * num_rects);
    if (command->rects == NULL) {
	free (command);
        return CAIRO_STATUS_NO_MEMORY;
    }
    memcpy (command->rects, rects, sizeof (cairo_rectangle_t) * num_rects);

    command->num_rects = num_rects;

    if (_cairo_array_append (&meta->commands, &command, 1) == NULL) {
	free (command->rects);
	free (command);
	return CAIRO_STATUS_NO_MEMORY;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_meta_surface_composite_trapezoids (cairo_operator_t	operator,
					  cairo_pattern_t	*pattern,
					  void			*abstract_surface,
					  cairo_antialias_t	antialias,
					  int			x_src,
					  int			y_src,
					  int			x_dst,
					  int			y_dst,
					  unsigned int		width,
					  unsigned int		height,
					  cairo_trapezoid_t	*traps,
					  int			num_traps)
{
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_composite_trapezoids_t *command;

    command = malloc (sizeof (cairo_command_composite_trapezoids_t));
    if (command == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    command->type = CAIRO_COMMAND_COMPOSITE_TRAPEZOIDS;
    command->operator = operator;
    _cairo_pattern_init_copy (&command->pattern.base, pattern);
    command->antialias = antialias;
    command->x_src = x_src;
    command->y_src = y_src;
    command->x_dst = x_dst;
    command->y_dst = y_dst;
    command->width = width;
    command->height = height;

    command->traps = malloc (sizeof (cairo_trapezoid_t) * num_traps);
    if (command->traps == NULL) {
	_cairo_pattern_fini (&command->pattern.base);
	free (command);
        return CAIRO_STATUS_NO_MEMORY;
    }
    memcpy (command->traps, traps, sizeof (cairo_trapezoid_t) * num_traps);

    command->num_traps = num_traps;

    if (_cairo_array_append (&meta->commands, &command, 1) == NULL) {
	_cairo_pattern_fini (&command->pattern.base);
	free (command->traps);
	free (command);
	return CAIRO_STATUS_NO_MEMORY;
    }

    return CAIRO_STATUS_SUCCESS;
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
	return CAIRO_STATUS_NO_MEMORY;

    command->type = CAIRO_COMMAND_INTERSECT_CLIP_PATH;

    if (path) {
	status = _cairo_path_fixed_init_copy (&command->path, path);
	if (status) {
	    free (command);
	    return status;
	}
	command->path_pointer = &command->path;
    } else {
	command->path_pointer = NULL;
    }
    command->fill_rule = fill_rule;
    command->tolerance = tolerance;
    command->antialias = antialias;

    if (_cairo_array_append (&meta->commands, &command, 1) == NULL) {
	if (path)
	    _cairo_path_fixed_fini (&command->path);
	free (command);
	return CAIRO_STATUS_NO_MEMORY;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_meta_surface_get_extents (void		   *abstract_surface,
				 cairo_rectangle_t *rectangle)
{
    cairo_meta_surface_t *meta = abstract_surface;

    /* Currently this is used for getting the extents of the surface
     * before calling cairo_paint().  This is the only this that
     * requires the meta surface to have an explicit size.  If paint
     * was just a backend function, this would not be necessary. */

    rectangle->x = 0;
    rectangle->y = 0;
    rectangle->width = meta->width;
    rectangle->height = meta->height;
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_meta_surface_show_glyphs (cairo_scaled_font_t	*scaled_font,
				 cairo_operator_t	operator,
				 cairo_pattern_t	*pattern,
				 void			*abstract_surface,
				 int			source_x,
				 int			source_y,
				 int			dest_x,
				 int			dest_y,
				 unsigned int		width,
				 unsigned int		height,
				 const cairo_glyph_t	*glyphs,
				 int			num_glyphs)
{
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_show_glyphs_t *command;

    command = malloc (sizeof (cairo_command_show_glyphs_t));
    if (command == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    command->type = CAIRO_COMMAND_SHOW_GLYPHS;
    command->scaled_font = cairo_scaled_font_reference (scaled_font);
    command->operator = operator;
    _cairo_pattern_init_copy (&command->pattern.base, pattern);
    command->source_x = source_x;
    command->source_y = source_y;
    command->dest_x = dest_x;
    command->dest_y = dest_y;
    command->width = width;
    command->height = height;

    command->glyphs = malloc (sizeof (cairo_glyph_t) * num_glyphs);
    if (command->glyphs == NULL) {
	_cairo_pattern_fini (&command->pattern.base);
	free (command);
        return CAIRO_STATUS_NO_MEMORY;
    }
    memcpy (command->glyphs, glyphs, sizeof (cairo_glyph_t) * num_glyphs);

    command->num_glyphs = num_glyphs;

    if (_cairo_array_append (&meta->commands, &command, 1) == NULL) {
	_cairo_pattern_fini (&command->pattern.base);
	free (command->glyphs);
	free (command);
	return CAIRO_STATUS_NO_MEMORY;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_meta_surface_fill_path (cairo_operator_t	   operator,
			       cairo_pattern_t	  *pattern,
			       void		  *abstract_surface,
			       cairo_path_fixed_t *path,
			       cairo_fill_rule_t   fill_rule,
			       double		   tolerance)
{
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_fill_path_t *command;
    cairo_status_t status;

    command = malloc (sizeof (cairo_command_fill_path_t));
    if (command == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    command->type = CAIRO_COMMAND_FILL_PATH;
    command->operator = operator;
    _cairo_pattern_init_copy (&command->pattern.base, pattern);
    status = _cairo_path_fixed_init_copy (&command->path, path);
    if (status) {
	_cairo_pattern_fini (&command->pattern.base);
	free (command);
	return CAIRO_STATUS_NO_MEMORY;
    }	
    command->fill_rule = fill_rule;
    command->tolerance = tolerance;

    if (_cairo_array_append (&meta->commands, &command, 1) == NULL) {
	_cairo_path_fixed_fini (&command->path);
	_cairo_pattern_fini (&command->pattern.base);
	free (command);
	return CAIRO_STATUS_NO_MEMORY;
    }

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_meta_surface_backend = {
    _cairo_meta_surface_create_similar,
    _cairo_meta_surface_finish,
    NULL, /* acquire_source_image */
    NULL, /* release_source_image */
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    _cairo_meta_surface_composite,
    _cairo_meta_surface_fill_rectangles,
    _cairo_meta_surface_composite_trapezoids,
    NULL, /* copy_page */
    NULL, /* show_page */
    NULL, /* set_clip_region */
    _cairo_meta_surface_intersect_clip_path,
    _cairo_meta_surface_get_extents,
    _cairo_meta_surface_show_glyphs,
    _cairo_meta_surface_fill_path,
};

cairo_int_status_t
_cairo_meta_surface_replay (cairo_surface_t *surface,
			    cairo_surface_t *target)
{
    cairo_meta_surface_t *meta;
    cairo_command_t *command, **elements;
    int i, num_elements;
    cairo_int_status_t status;
    cairo_traps_t traps;
    cairo_clip_t clip;

    meta = (cairo_meta_surface_t *) surface;
    status = CAIRO_STATUS_SUCCESS;

    _cairo_clip_init (&clip, target);    

    num_elements = meta->commands.num_elements;
    elements = (cairo_command_t **) meta->commands.elements;
    for (i = 0; i < num_elements; i++) {
	command = elements[i];
	switch (command->type) {
	case CAIRO_COMMAND_COMPOSITE:
	    status = _cairo_surface_set_clip (target, &clip);
	    if (status)
		break;

	    status = _cairo_surface_composite
		(command->composite.operator,
		 &command->composite.src_pattern.base,
		 command->composite.mask_pattern_pointer,
		 target,
		 command->composite.src_x,
		 command->composite.src_y,
		 command->composite.mask_x,
		 command->composite.mask_y,
		 command->composite.dst_x,
		 command->composite.dst_y,
		 command->composite.width,
		 command->composite.height);
	    break;

	case CAIRO_COMMAND_FILL_RECTANGLES:
	    status = _cairo_surface_set_clip (target, &clip);
	    if (status)
		break;

	    status = _cairo_surface_fill_rectangles
		(target,
		 command->fill_rectangles.operator,
		 &command->fill_rectangles.color,
		 command->fill_rectangles.rects,
		 command->fill_rectangles.num_rects);
	    break;

	case CAIRO_COMMAND_COMPOSITE_TRAPEZOIDS:
	    status = _cairo_surface_set_clip (target, &clip);
	    if (status)
		break;

	    status = _cairo_surface_composite_trapezoids
		(command->composite_trapezoids.operator,
		 &command->composite_trapezoids.pattern.base,
		 target,
		 command->composite_trapezoids.antialias,
		 command->composite_trapezoids.x_src,
		 command->composite_trapezoids.y_src,
		 command->composite_trapezoids.x_dst,
		 command->composite_trapezoids.y_dst,
		 command->composite_trapezoids.width,
		 command->composite_trapezoids.height,
		 command->composite_trapezoids.traps,
		 command->composite_trapezoids.num_traps);
	    break;

	case CAIRO_COMMAND_INTERSECT_CLIP_PATH:
	    /* XXX Meta surface clipping is broken and requires some
	     * cairo-gstate.c rewriting.  Work around it for now. */
	    if (command->intersect_clip_path.path_pointer == NULL)
		status = _cairo_clip_reset (&clip);
	    else
		status = _cairo_clip_clip (&clip,
					   command->intersect_clip_path.path_pointer,
					   command->intersect_clip_path.fill_rule,
					   command->intersect_clip_path.tolerance,
					   command->intersect_clip_path.antialias,
					   target);
	    break;

	case CAIRO_COMMAND_SHOW_GLYPHS:
	    status = _cairo_surface_set_clip (target, &clip);
	    if (status)
		break;

	    status = _cairo_surface_show_glyphs
		(command->show_glyphs.scaled_font,
		 command->show_glyphs.operator,
		 &command->show_glyphs.pattern.base,
		 target,
		 command->show_glyphs.source_x,
		 command->show_glyphs.source_y,
		 command->show_glyphs.dest_x,
		 command->show_glyphs.dest_y,
		 command->show_glyphs.width,
		 command->show_glyphs.height,
		 command->show_glyphs.glyphs,
		 command->show_glyphs.num_glyphs);
	    break;

	case CAIRO_COMMAND_FILL_PATH:
	    status = _cairo_surface_set_clip (target, &clip);
	    if (status)
		break;

	    status = _cairo_surface_fill_path (command->fill_path.operator,
					       &command->fill_path.pattern.base,
					       target,
					       &command->fill_path.path,
					       command->fill_path.fill_rule,
					       command->fill_path.tolerance);
	    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
		break;

	    _cairo_traps_init (&traps);

	    status = _cairo_path_fixed_fill_to_traps (&command->fill_path.path,
						      command->fill_path.fill_rule,
						      command->fill_path.tolerance,
						      &traps);
	    if (status) {
		_cairo_traps_fini (&traps);
		break;
	    }

	    status = _cairo_surface_clip_and_composite_trapezoids (&command->fill_path.pattern.base,
								   command->fill_path.operator,
								   target,
								   &traps,
								   &clip,
								   command->fill_path.antialias);

	    _cairo_traps_fini (&traps);
	    break;

	default:
	    ASSERT_NOT_REACHED;
	}

	if (status)
	    break;
    }

    _cairo_clip_fini (&clip);

    return status;
}

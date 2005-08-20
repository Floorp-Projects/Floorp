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

#ifndef CAIRO_META_SURFACE_H
#define CAIRO_META_SURFACE_H

#include "cairoint.h"
#include "cairo-path-fixed-private.h"

typedef enum {
    CAIRO_COMMAND_COMPOSITE,
    CAIRO_COMMAND_FILL_RECTANGLES,
    CAIRO_COMMAND_COMPOSITE_TRAPEZOIDS,
    CAIRO_COMMAND_SET_CLIP_REGION,
    CAIRO_COMMAND_INTERSECT_CLIP_PATH,
    CAIRO_COMMAND_SHOW_GLYPHS,
    CAIRO_COMMAND_FILL_PATH
} cairo_command_type_t;

typedef struct _cairo_command_composite {
    cairo_command_type_t	type;
    cairo_operator_t		operator;
    cairo_pattern_union_t	src_pattern;
    cairo_pattern_union_t	mask_pattern;
    cairo_pattern_t	       *mask_pattern_pointer;
    int				src_x;
    int				src_y;
    int				mask_x;
    int				mask_y;
    int				dst_x;
    int				dst_y;
    unsigned int		width;
    unsigned int		height;
} cairo_command_composite_t;

typedef struct _cairo_command_fill_rectangles {
    cairo_command_type_t	type;
    cairo_operator_t		operator;
    cairo_color_t		color;
    cairo_rectangle_t	       *rects;
    int				num_rects;
} cairo_command_fill_rectangles_t;

typedef struct _cairo_command_composite_trapezoids {
    cairo_command_type_t	type;
    cairo_operator_t		operator;
    cairo_pattern_union_t	pattern;
    cairo_antialias_t		antialias;
    int				x_src;
    int				y_src;
    int				x_dst;
    int				y_dst;
    unsigned int		width;
    unsigned int		height;
    cairo_trapezoid_t	       *traps;
    int				num_traps;
} cairo_command_composite_trapezoids_t;

typedef struct _cairo_command_set_clip_region {
    cairo_command_type_t	type;
    pixman_region16_t	       *region;
    unsigned int		serial;
} cairo_command_set_clip_region_t;

typedef struct _cairo_command_intersect_clip_path {
    cairo_command_type_t	type;
    cairo_path_fixed_t	       *path_pointer;
    cairo_path_fixed_t		path;
    cairo_fill_rule_t		fill_rule;
    double			tolerance;
    cairo_antialias_t		antialias;
} cairo_command_intersect_clip_path_t;

typedef struct _cairo_command_show_glyphs {
    cairo_command_type_t	type;
    cairo_scaled_font_t	       *scaled_font;
    cairo_operator_t		operator;
    cairo_pattern_union_t	pattern;
    int				source_x;
    int				source_y;
    int				dest_x;
    int				dest_y;
    unsigned int		width;
    unsigned int		height;
    cairo_glyph_t		*glyphs;
    int				num_glyphs;
} cairo_command_show_glyphs_t;

typedef struct _cairo_command_fill_path {
    cairo_command_type_t	type;
    cairo_operator_t		operator;
    cairo_pattern_union_t	pattern;
    cairo_path_fixed_t		path;
    cairo_fill_rule_t		fill_rule;
    double			tolerance;
    cairo_antialias_t		antialias;
} cairo_command_fill_path_t;

typedef union _cairo_command {
    cairo_command_type_t			type;
    cairo_command_composite_t			composite;	
    cairo_command_fill_rectangles_t		fill_rectangles;
    cairo_command_composite_trapezoids_t	composite_trapezoids;
    cairo_command_set_clip_region_t		set_clip_region;
    cairo_command_intersect_clip_path_t		intersect_clip_path;
    cairo_command_show_glyphs_t			show_glyphs;
    cairo_command_fill_path_t			fill_path;
} cairo_command_t;

typedef struct _cairo_meta_surface {
    cairo_surface_t base;
    double width, height;
    cairo_array_t commands;
} cairo_meta_surface_t;

cairo_private cairo_surface_t *
_cairo_meta_surface_create (double width, double height);

cairo_private cairo_int_status_t
_cairo_meta_surface_replay (cairo_surface_t *surface,
			    cairo_surface_t *target);

#endif /* CAIRO_META_SURFACE_H */

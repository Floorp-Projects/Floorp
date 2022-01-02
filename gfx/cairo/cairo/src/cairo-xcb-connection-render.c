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
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-xcb-private.h"

#include <xcb/xcbext.h>

void
_cairo_xcb_connection_render_create_picture (cairo_xcb_connection_t  *connection,
					     xcb_render_picture_t     picture,
					     xcb_drawable_t           drawable,
					     xcb_render_pictformat_t  format,
					     uint32_t                 value_mask,
					     uint32_t	             *value_list)
{
    assert (connection->flags & CAIRO_XCB_HAS_RENDER);
    xcb_render_create_picture (connection->xcb_connection, picture, drawable,
			       format, value_mask, value_list);
}

void
_cairo_xcb_connection_render_change_picture (cairo_xcb_connection_t     *connection,
					     xcb_render_picture_t  picture,
					     uint32_t              value_mask,
					     uint32_t             *value_list)
{
    assert (connection->flags & CAIRO_XCB_HAS_RENDER);
    xcb_render_change_picture (connection->xcb_connection, picture,
			       value_mask, value_list);
}

void
_cairo_xcb_connection_render_set_picture_clip_rectangles (cairo_xcb_connection_t      *connection,
							  xcb_render_picture_t   picture,
							  int16_t                clip_x_origin,
							  int16_t                clip_y_origin,
							  uint32_t               rectangles_len,
							  xcb_rectangle_t *rectangles)
{
    assert (connection->flags & CAIRO_XCB_HAS_RENDER);
    xcb_render_set_picture_clip_rectangles (connection->xcb_connection, picture,
					    clip_x_origin, clip_y_origin,
					    rectangles_len, rectangles);
}

void
_cairo_xcb_connection_render_free_picture (cairo_xcb_connection_t *connection,
					   xcb_render_picture_t  picture)
{
    assert (connection->flags & CAIRO_XCB_HAS_RENDER);
    xcb_render_free_picture (connection->xcb_connection, picture);
    _cairo_xcb_connection_put_xid (connection, picture);
}

void
_cairo_xcb_connection_render_composite (cairo_xcb_connection_t     *connection,
					uint8_t               op,
					xcb_render_picture_t  src,
					xcb_render_picture_t  mask,
					xcb_render_picture_t  dst,
					int16_t               src_x,
					int16_t               src_y,
					int16_t               mask_x,
					int16_t               mask_y,
					int16_t               dst_x,
					int16_t               dst_y,
					uint16_t              width,
					uint16_t              height)
{
    assert (connection->flags & CAIRO_XCB_RENDER_HAS_COMPOSITE);
    xcb_render_composite (connection->xcb_connection, op, src, mask, dst,
			  src_x, src_y, mask_x, mask_y, dst_x, dst_y, width, height);
}

void
_cairo_xcb_connection_render_trapezoids (cairo_xcb_connection_t *connection,
					 uint8_t                       op,
					 xcb_render_picture_t          src,
					 xcb_render_picture_t          dst,
					 xcb_render_pictformat_t       mask_format,
					 int16_t                       src_x,
					 int16_t                       src_y,
					 uint32_t                      traps_len,
					 xcb_render_trapezoid_t *traps)
{
    assert (connection->flags & CAIRO_XCB_RENDER_HAS_COMPOSITE_TRAPEZOIDS);
    xcb_render_trapezoids (connection->xcb_connection, op, src, dst,
			   mask_format, src_x, src_y, traps_len, traps);
}

void
_cairo_xcb_connection_render_create_glyph_set (cairo_xcb_connection_t	*connection,
					       xcb_render_glyphset_t	 id,
					       xcb_render_pictformat_t  format)
{
    assert (connection->flags & CAIRO_XCB_HAS_RENDER);
    xcb_render_create_glyph_set (connection->xcb_connection, id, format);
}

void
_cairo_xcb_connection_render_free_glyph_set (cairo_xcb_connection_t      *connection,
					     xcb_render_glyphset_t  glyphset)
{
    assert (connection->flags & CAIRO_XCB_HAS_RENDER);
    xcb_render_free_glyph_set (connection->xcb_connection, glyphset);
    _cairo_xcb_connection_put_xid (connection, glyphset);
}

void
_cairo_xcb_connection_render_add_glyphs (cairo_xcb_connection_t             *connection,
					 xcb_render_glyphset_t         glyphset,
					 uint32_t                      num_glyphs,
					 uint32_t               *glyphs_id,
					 xcb_render_glyphinfo_t *glyphs,
					 uint32_t                      data_len,
					 uint8_t                *data)
{
    assert (connection->flags & CAIRO_XCB_HAS_RENDER);
    xcb_render_add_glyphs (connection->xcb_connection, glyphset, num_glyphs,
				   glyphs_id, glyphs, data_len, data);
}

void
_cairo_xcb_connection_render_free_glyphs (cairo_xcb_connection_t         *connection,
					  xcb_render_glyphset_t     glyphset,
					  uint32_t                  num_glyphs,
					  xcb_render_glyph_t *glyphs)
{
    assert (connection->flags & CAIRO_XCB_HAS_RENDER);
    xcb_render_free_glyphs (connection->xcb_connection, glyphset, num_glyphs, glyphs);
}

void
_cairo_xcb_connection_render_composite_glyphs_8 (cairo_xcb_connection_t        *connection,
						 uint8_t                  op,
						 xcb_render_picture_t     src,
						 xcb_render_picture_t     dst,
						 xcb_render_pictformat_t  mask_format,
						 xcb_render_glyphset_t    glyphset,
						 int16_t                  src_x,
						 int16_t                  src_y,
						 uint32_t                 glyphcmds_len,
						 uint8_t           *glyphcmds)
{
    assert (connection->flags & CAIRO_XCB_HAS_RENDER);
    xcb_render_composite_glyphs_8 (connection->xcb_connection, op, src, dst, mask_format,
				   glyphset, src_x, src_y, glyphcmds_len, glyphcmds);
}

void
_cairo_xcb_connection_render_composite_glyphs_16 (cairo_xcb_connection_t        *connection,
						  uint8_t                  op,
						  xcb_render_picture_t     src,
						  xcb_render_picture_t     dst,
						  xcb_render_pictformat_t  mask_format,
						  xcb_render_glyphset_t    glyphset,
						  int16_t                  src_x,
						  int16_t                  src_y,
						  uint32_t                 glyphcmds_len,
						  uint8_t           *glyphcmds)
{
    assert (connection->flags & CAIRO_XCB_HAS_RENDER);
    xcb_render_composite_glyphs_16 (connection->xcb_connection, op, src, dst, mask_format,
				    glyphset, src_x, src_y, glyphcmds_len, glyphcmds);
}

void
_cairo_xcb_connection_render_composite_glyphs_32 (cairo_xcb_connection_t        *connection,
						  uint8_t                  op,
						  xcb_render_picture_t     src,
						  xcb_render_picture_t     dst,
						  xcb_render_pictformat_t  mask_format,
						  xcb_render_glyphset_t    glyphset,
						  int16_t                  src_x,
						  int16_t                  src_y,
						  uint32_t                 glyphcmds_len,
						  uint8_t           *glyphcmds)
{
    assert (connection->flags & CAIRO_XCB_HAS_RENDER);
    xcb_render_composite_glyphs_32 (connection->xcb_connection, op, src, dst, mask_format,
				    glyphset, src_x, src_y, glyphcmds_len, glyphcmds);
}

void
_cairo_xcb_connection_render_fill_rectangles (cairo_xcb_connection_t      *connection,
					      uint8_t                op,
					      xcb_render_picture_t   dst,
					      xcb_render_color_t     color,
					      uint32_t               num_rects,
					      xcb_rectangle_t *rects)
{
    assert (connection->flags & CAIRO_XCB_RENDER_HAS_FILL_RECTANGLES);
    xcb_render_fill_rectangles (connection->xcb_connection, op, dst, color,
				num_rects, rects);
}

void
_cairo_xcb_connection_render_set_picture_transform (cairo_xcb_connection_t       *connection,
						    xcb_render_picture_t    picture,
						    xcb_render_transform_t  *transform)
{
    assert (connection->flags & CAIRO_XCB_RENDER_HAS_PICTURE_TRANSFORM);
    xcb_render_set_picture_transform (connection->xcb_connection, picture, *transform);
}

void
_cairo_xcb_connection_render_set_picture_filter (cairo_xcb_connection_t         *connection,
						 xcb_render_picture_t      picture,
						 uint16_t                  filter_len,
						 char               *filter)
{
    assert (connection->flags & CAIRO_XCB_RENDER_HAS_FILTERS);
    xcb_render_set_picture_filter (connection->xcb_connection, picture,
				   filter_len, filter, 0, NULL);
}

void
_cairo_xcb_connection_render_create_solid_fill (cairo_xcb_connection_t     *connection,
						xcb_render_picture_t  picture,
						xcb_render_color_t    color)
{
    assert (connection->flags & CAIRO_XCB_RENDER_HAS_GRADIENTS);
    xcb_render_create_solid_fill (connection->xcb_connection, picture, color);
}

void
_cairo_xcb_connection_render_create_linear_gradient (cairo_xcb_connection_t         *connection,
						     xcb_render_picture_t      picture,
						     xcb_render_pointfix_t     p1,
						     xcb_render_pointfix_t     p2,
						     uint32_t                  num_stops,
						     xcb_render_fixed_t *stops,
						     xcb_render_color_t *colors)
{
    assert (connection->flags & CAIRO_XCB_RENDER_HAS_GRADIENTS);
    xcb_render_create_linear_gradient (connection->xcb_connection, picture,
				       p1, p2, num_stops, stops, colors);
}

void
_cairo_xcb_connection_render_create_radial_gradient (cairo_xcb_connection_t         *connection,
						     xcb_render_picture_t      picture,
						     xcb_render_pointfix_t     inner,
						     xcb_render_pointfix_t     outer,
						     xcb_render_fixed_t        inner_radius,
						     xcb_render_fixed_t        outer_radius,
						     uint32_t                  num_stops,
						     xcb_render_fixed_t *stops,
						     xcb_render_color_t *colors)
{
    assert (connection->flags & CAIRO_XCB_RENDER_HAS_GRADIENTS);
    xcb_render_create_radial_gradient (connection->xcb_connection, picture,
				       inner, outer, inner_radius, outer_radius,
				       num_stops, stops, colors);
}

void
_cairo_xcb_connection_render_create_conical_gradient (cairo_xcb_connection_t         *connection,
						      xcb_render_picture_t      picture,
						      xcb_render_pointfix_t     center,
						      xcb_render_fixed_t        angle,
						      uint32_t                  num_stops,
						      xcb_render_fixed_t *stops,
						      xcb_render_color_t *colors)
{
    assert (connection->flags & CAIRO_XCB_RENDER_HAS_GRADIENTS);
    xcb_render_create_conical_gradient (connection->xcb_connection, picture,
				       center, angle, num_stops, stops, colors);
}

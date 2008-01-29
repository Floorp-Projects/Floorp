/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
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
#include "cairo-pdf-operators-private.h"
#include "cairo-path-fixed-private.h"
#include "cairo-output-stream-private.h"
#include "cairo-scaled-font-subsets-private.h"

void
_cairo_pdf_operators_init (cairo_pdf_operators_t	*pdf_operators,
			   cairo_output_stream_t	*stream,
			   cairo_matrix_t		 cairo_to_pdf,
			   cairo_scaled_font_subsets_t  *font_subsets)
{
    pdf_operators->stream = stream;
    pdf_operators->cairo_to_pdf = cairo_to_pdf;
    pdf_operators->font_subsets = font_subsets;
    pdf_operators->use_font_subset = NULL;
    pdf_operators->use_font_subset_closure = NULL;
}

void
_cairo_pdf_operators_fini (cairo_pdf_operators_t	*pdf_operators)
{
}

void
_cairo_pdf_operators_set_font_subsets_callback (cairo_pdf_operators_t		     *pdf_operators,
						cairo_pdf_operators_use_font_subset_t use_font_subset,
						void				     *closure)
{
    pdf_operators->use_font_subset = use_font_subset;
    pdf_operators->use_font_subset_closure = closure;
}

void
_cairo_pdf_operators_set_stream (cairo_pdf_operators_t	 *pdf_operators,
				 cairo_output_stream_t   *stream)
{
    pdf_operators->stream = stream;
}

void
_cairo_pdf_operators_set_cairo_to_pdf_matrix (cairo_pdf_operators_t *pdf_operators,
					      cairo_matrix_t	     cairo_to_pdf)
{
    pdf_operators->cairo_to_pdf = cairo_to_pdf;
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

    return _cairo_output_stream_get_status (info->output);
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

    return _cairo_output_stream_get_status (info->output);
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
    return _cairo_output_stream_get_status (info->output);
}

static cairo_status_t
_cairo_pdf_path_close_path (void *closure)
{
    pdf_path_info_t *info = closure;

    _cairo_output_stream_printf (info->output,
				 "h\r\n");

    return _cairo_output_stream_get_status (info->output);
}

cairo_int_status_t
_cairo_pdf_operators_clip (cairo_pdf_operators_t	*pdf_operators,
			   cairo_path_fixed_t		*path,
			   cairo_fill_rule_t		 fill_rule)
{
    const char *pdf_operator;

    if (! path->has_current_point) {
	/* construct an empty path */
	_cairo_output_stream_printf (pdf_operators->stream, "0 0 m ");
    } else {
	pdf_path_info_t info;
	cairo_status_t status;

	info.output = pdf_operators->stream;
	info.cairo_to_pdf = &pdf_operators->cairo_to_pdf;
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

    _cairo_output_stream_printf (pdf_operators->stream,
				 "%s n\r\n",
				 pdf_operator);

    return _cairo_output_stream_get_status (pdf_operators->stream);
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

static cairo_int_status_t
_cairo_pdf_operators_emit_stroke_style (cairo_pdf_operators_t	*pdf_operators,
					cairo_stroke_style_t	*style)
{
    _cairo_output_stream_printf (pdf_operators->stream,
				 "%f w\r\n",
				 style->line_width);

    _cairo_output_stream_printf (pdf_operators->stream,
				 "%d J\r\n",
				 _cairo_pdf_line_cap (style->line_cap));

    _cairo_output_stream_printf (pdf_operators->stream,
				 "%d j\r\n",
				 _cairo_pdf_line_join (style->line_join));

    if (style->num_dashes) {
	unsigned int d;
	_cairo_output_stream_printf (pdf_operators->stream, "[");
	for (d = 0; d < style->num_dashes; d++)
	    _cairo_output_stream_printf (pdf_operators->stream, " %f", style->dash[d]);
	_cairo_output_stream_printf (pdf_operators->stream, "] %f d\r\n",
				     style->dash_offset);
    } else {
	_cairo_output_stream_printf (pdf_operators->stream, "[] 0.0 d\r\n");
    }

    _cairo_output_stream_printf (pdf_operators->stream,
				 "%f M ",
				 style->miter_limit);

    return _cairo_output_stream_get_status (pdf_operators->stream);
}


cairo_int_status_t
_cairo_pdf_operator_stroke (cairo_pdf_operators_t	*pdf_operators,
			    cairo_path_fixed_t		*path,
			    cairo_stroke_style_t	*style,
			    cairo_matrix_t		*ctm,
			    cairo_matrix_t		*ctm_inverse)
{
    pdf_path_info_t info;
    cairo_status_t status;
    cairo_matrix_t m;

    status = _cairo_pdf_operators_emit_stroke_style (pdf_operators, style);
    if (status)
	return status;

    info.output = pdf_operators->stream;
    info.cairo_to_pdf = NULL;
    info.ctm_inverse = ctm_inverse;

    cairo_matrix_multiply (&m, ctm, &pdf_operators->cairo_to_pdf);
    _cairo_output_stream_printf (pdf_operators->stream,
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

    _cairo_output_stream_printf (pdf_operators->stream, "S Q\r\n");

    return _cairo_output_stream_get_status (pdf_operators->stream);
}

cairo_int_status_t
_cairo_pdf_operators_fill (cairo_pdf_operators_t	*pdf_operators,
			   cairo_path_fixed_t		*path,
			   cairo_fill_rule_t		fill_rule)
{
    const char *pdf_operator;
    cairo_status_t status;
    pdf_path_info_t info;

    info.output = pdf_operators->stream;
    info.cairo_to_pdf = &pdf_operators->cairo_to_pdf;
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

    _cairo_output_stream_printf (pdf_operators->stream,
				 "%s\r\n",
				 pdf_operator);

    return _cairo_output_stream_get_status (pdf_operators->stream);
}

#define GLYPH_POSITION_TOLERANCE 0.001

cairo_int_status_t
_cairo_pdf_operators_show_glyphs (cairo_pdf_operators_t		*pdf_operators,
				  cairo_glyph_t			*glyphs,
				  int				 num_glyphs,
				  cairo_scaled_font_t		*scaled_font)
{
    unsigned int current_subset_id = (unsigned int)-1;
    cairo_scaled_font_subsets_glyph_t subset_glyph;
    cairo_bool_t diagonal, in_TJ;
    cairo_status_t status;
    double Tlm_x = 0, Tlm_y = 0;
    double Tm_x = 0, y;
    int i, hex_width;

    for (i = 0; i < num_glyphs; i++)
	cairo_matrix_transform_point (&pdf_operators->cairo_to_pdf, &glyphs[i].x, &glyphs[i].y);

    _cairo_output_stream_printf (pdf_operators->stream,
				 "BT\r\n");

    if (scaled_font->scale.xy == 0.0 &&
        scaled_font->scale.yx == 0.0)
        diagonal = TRUE;
    else
        diagonal = FALSE;

    in_TJ = FALSE;
    for (i = 0; i < num_glyphs; i++) {
        status = _cairo_scaled_font_subsets_map_glyph (pdf_operators->font_subsets,
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
                _cairo_output_stream_printf (pdf_operators->stream, ">] TJ\r\n");
                in_TJ = FALSE;
            }
	    _cairo_output_stream_printf (pdf_operators->stream,
					 "/f-%d-%d 1 Tf\r\n",
					 subset_glyph.font_id,
					 subset_glyph.subset_id);
	    if (pdf_operators->use_font_subset) {
		status = pdf_operators->use_font_subset (subset_glyph.font_id,
							 subset_glyph.subset_id,
							 pdf_operators->use_font_subset_closure);
		if (status)
		    return status;
	    }
        }

        if (subset_glyph.subset_id != current_subset_id || !diagonal) {
            _cairo_output_stream_printf (pdf_operators->stream,
                                         "%f %f %f %f %f %f Tm\r\n",
                                         scaled_font->scale.xx,
                                         -scaled_font->scale.yx,
                                         -scaled_font->scale.xy,
                                         scaled_font->scale.yy,
                                         glyphs[i].x,
                                         glyphs[i].y);
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
                        _cairo_output_stream_printf (pdf_operators->stream,
                                                     "%f %f Td\r\n",
                                                     (glyphs[i].x - Tlm_x)/scaled_font->scale.xx,
                                                     (glyphs[i].y - Tlm_y)/scaled_font->scale.yy);

                        Tlm_x = glyphs[i].x;
                        Tlm_y = glyphs[i].y;
                        Tm_x = Tlm_x;
                    }
                    _cairo_output_stream_printf (pdf_operators->stream,
                                                 "[<%0*x",
                                                 hex_width,
                                                 subset_glyph.subset_glyph_index);
                    Tm_x += subset_glyph.x_advance;
                    in_TJ = TRUE;
                } else {
                    if (fabs((glyphs[i].x - Tm_x)/scaled_font->scale.xx) > GLYPH_POSITION_TOLERANCE) {
                        double delta = glyphs[i].x - Tm_x;

                        _cairo_output_stream_printf (pdf_operators->stream,
                                                     "> %f <",
                                                     -1000.0*delta/scaled_font->scale.xx);
                        Tm_x += delta;
                    }
                    _cairo_output_stream_printf (pdf_operators->stream,
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

                        _cairo_output_stream_printf (pdf_operators->stream,
                                                     "> %f <",
                                                     -1000.0*delta/scaled_font->scale.xx);
                        Tm_x += delta;
                    }
                    _cairo_output_stream_printf (pdf_operators->stream,
                                                 "%0*x>] TJ\r\n",
                                                 hex_width,
                                                 subset_glyph.subset_glyph_index);
                    Tm_x += subset_glyph.x_advance;
                    in_TJ = FALSE;
                } else {
                    if (i != 0) {
                        _cairo_output_stream_printf (pdf_operators->stream,
                                                     "%f %f Td ",
                                                     (glyphs[i].x - Tlm_x)/scaled_font->scale.xx,
                                                     (glyphs[i].y - Tlm_y)/-scaled_font->scale.yy);
                        Tlm_x = glyphs[i].x;
                        Tlm_y = glyphs[i].y;
                        Tm_x = Tlm_x;
                    }
                    _cairo_output_stream_printf (pdf_operators->stream,
                                                 "<%0*x> Tj ",
                                                 hex_width,
                                                 subset_glyph.subset_glyph_index);
                    Tm_x += subset_glyph.x_advance;
                }
            }
        } else {
            _cairo_output_stream_printf (pdf_operators->stream,
                                         "<%0*x> Tj\r\n",
                                         hex_width,
                                         subset_glyph.subset_glyph_index);
        }
    }

    _cairo_output_stream_printf (pdf_operators->stream,
				 "ET\r\n");

    return _cairo_output_stream_get_status (pdf_operators->stream);
}

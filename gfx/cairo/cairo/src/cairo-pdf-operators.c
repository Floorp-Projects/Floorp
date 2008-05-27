/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
 * Copyright © 2006 Red Hat, Inc
 * Copyright © 2007, 2008 Adrian Johnson
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

#include <ctype.h>

void
_cairo_pdf_operators_init (cairo_pdf_operators_t	*pdf_operators,
			   cairo_output_stream_t	*stream,
			   cairo_matrix_t		*cairo_to_pdf,
			   cairo_scaled_font_subsets_t  *font_subsets)
{
    pdf_operators->stream = stream;
    pdf_operators->cairo_to_pdf = *cairo_to_pdf;
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
					      cairo_matrix_t	    *cairo_to_pdf)
{
    pdf_operators->cairo_to_pdf = *cairo_to_pdf;
}

/* A word wrap stream can be used as a filter to do word wrapping on
 * top of an existing output stream. The word wrapping is quite
 * simple, using isspace to determine characters that separate
 * words. Any word that will cause the column count exceed the given
 * max_column will have a '\n' character emitted before it.
 *
 * The stream is careful to maintain integrity for words that cross
 * the boundary from one call to write to the next.
 *
 * Note: This stream does not guarantee that the output will never
 * exceed max_column. In particular, if a single word is larger than
 * max_column it will not be broken up.
 */
typedef struct _word_wrap_stream {
    cairo_output_stream_t base;
    cairo_output_stream_t *output;
    int max_column;
    int column;
    cairo_bool_t last_write_was_space;
    cairo_bool_t in_hexstring;
    cairo_bool_t empty_hexstring;
} word_wrap_stream_t;

static int
_count_word_up_to (const unsigned char *s, int length)
{
    int word = 0;

    while (length--) {
	if (! (isspace (*s) || *s == '<')) {
	    s++;
	    word++;
	} else {
	    return word;
	}
    }

    return word;
}


/* Count up to either the end of the ASCII hexstring or the number
 * of columns remaining.
 */
static int
_count_hexstring_up_to (const unsigned char *s, int length, int columns)
{
    int word = 0;

    while (length--) {
	if (*s++ != '>')
	    word++;
	else
	    return word;

	columns--;
	if (columns < 0 && word > 1)
	    return word;
    }

    return word;
}

static cairo_status_t
_word_wrap_stream_write (cairo_output_stream_t  *base,
			 const unsigned char	*data,
			 unsigned int		 length)
{
    word_wrap_stream_t *stream = (word_wrap_stream_t *) base;
    cairo_bool_t newline;
    int word;

    while (length) {
	if (*data == '<') {
	    stream->in_hexstring = TRUE;
	    stream->empty_hexstring = TRUE;
	    stream->last_write_was_space = FALSE;
	    data++;
	    length--;
	    _cairo_output_stream_printf (stream->output, "<");
	    stream->column++;
	} else if (*data == '>') {
	    stream->in_hexstring = FALSE;
	    stream->last_write_was_space = FALSE;
	    data++;
	    length--;
	    _cairo_output_stream_printf (stream->output, ">");
	    stream->column++;
	} else if (isspace (*data)) {
	    newline =  (*data == '\n' || *data == '\r');
	    if (! newline && stream->column >= stream->max_column) {
		_cairo_output_stream_printf (stream->output, "\n");
		stream->column = 0;
	    }
	    _cairo_output_stream_write (stream->output, data, 1);
	    data++;
	    length--;
	    if (newline) {
		stream->column = 0;
	    }
	    else
		stream->column++;
	    stream->last_write_was_space = TRUE;
	} else {
	    if (stream->in_hexstring) {
		word = _count_hexstring_up_to (data, length,
					       MAX (stream->max_column - stream->column, 0));
	    } else {
		word = _count_word_up_to (data, length);
	    }
	    /* Don't wrap if this word is a continuation of a non hex
	     * string word from a previous call to write. */
	    if (stream->column + word >= stream->max_column) {
		if (stream->last_write_was_space ||
		    (stream->in_hexstring && !stream->empty_hexstring))
		{
		    _cairo_output_stream_printf (stream->output, "\n");
		    stream->column = 0;
		}
	    }
	    _cairo_output_stream_write (stream->output, data, word);
	    data += word;
	    length -= word;
	    stream->column += word;
	    stream->last_write_was_space = FALSE;
	    if (stream->in_hexstring)
		stream->empty_hexstring = FALSE;
	}
    }

    return _cairo_output_stream_get_status (stream->output);
}

static cairo_status_t
_word_wrap_stream_close (cairo_output_stream_t *base)
{
    word_wrap_stream_t *stream = (word_wrap_stream_t *) base;

    return _cairo_output_stream_get_status (stream->output);
}

static cairo_output_stream_t *
_word_wrap_stream_create (cairo_output_stream_t *output, int max_column)
{
    word_wrap_stream_t *stream;

    if (output->status)
	return _cairo_output_stream_create_in_error (output->status);

    stream = malloc (sizeof (word_wrap_stream_t));
    if (stream == NULL) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return (cairo_output_stream_t *) &_cairo_output_stream_nil;
    }

    _cairo_output_stream_init (&stream->base,
			       _word_wrap_stream_write,
			       _word_wrap_stream_close);
    stream->output = output;
    stream->max_column = max_column;
    stream->column = 0;
    stream->last_write_was_space = FALSE;
    stream->in_hexstring = FALSE;
    stream->empty_hexstring = TRUE;

    return &stream->base;
}

typedef struct _pdf_path_info {
    cairo_output_stream_t   *output;
    cairo_matrix_t	    *path_transform;
    cairo_line_cap_t         line_cap;
    cairo_point_t            last_move_to_point;
    cairo_bool_t             has_sub_path;
} pdf_path_info_t;

static cairo_status_t
_cairo_pdf_path_move_to (void *closure, cairo_point_t *point)
{
    pdf_path_info_t *info = closure;
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    info->last_move_to_point = *point;
    info->has_sub_path = FALSE;
    cairo_matrix_transform_point (info->path_transform, &x, &y);
    _cairo_output_stream_printf (info->output,
				 "%g %g m ", x, y);

    return _cairo_output_stream_get_status (info->output);
}

static cairo_status_t
_cairo_pdf_path_line_to (void *closure, cairo_point_t *point)
{
    pdf_path_info_t *info = closure;
    double x = _cairo_fixed_to_double (point->x);
    double y = _cairo_fixed_to_double (point->y);

    if (info->line_cap != CAIRO_LINE_CAP_ROUND &&
	! info->has_sub_path &&
	point->x == info->last_move_to_point.x &&
	point->y == info->last_move_to_point.y)
    {
	return CAIRO_STATUS_SUCCESS;
    }

    info->has_sub_path = TRUE;
    cairo_matrix_transform_point (info->path_transform, &x, &y);
    _cairo_output_stream_printf (info->output,
				 "%g %g l ", x, y);

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

    info->has_sub_path = TRUE;
    cairo_matrix_transform_point (info->path_transform, &bx, &by);
    cairo_matrix_transform_point (info->path_transform, &cx, &cy);
    cairo_matrix_transform_point (info->path_transform, &dx, &dy);
    _cairo_output_stream_printf (info->output,
				 "%g %g %g %g %g %g c ",
				 bx, by, cx, cy, dx, dy);
    return _cairo_output_stream_get_status (info->output);
}

static cairo_status_t
_cairo_pdf_path_close_path (void *closure)
{
    pdf_path_info_t *info = closure;

    if (info->line_cap != CAIRO_LINE_CAP_ROUND &&
	! info->has_sub_path)
    {
	return CAIRO_STATUS_SUCCESS;
    }

    _cairo_output_stream_printf (info->output,
				 "h\n");

    return _cairo_output_stream_get_status (info->output);
}

static cairo_status_t
_cairo_pdf_path_rectangle (pdf_path_info_t *info, cairo_box_t *box)
{
    double x1 = _cairo_fixed_to_double (box->p1.x);
    double y1 = _cairo_fixed_to_double (box->p1.y);
    double x2 = _cairo_fixed_to_double (box->p2.x);
    double y2 = _cairo_fixed_to_double (box->p2.y);

    cairo_matrix_transform_point (info->path_transform, &x1, &y1);
    cairo_matrix_transform_point (info->path_transform, &x2, &y2);
    _cairo_output_stream_printf (info->output,
				 "%g %g %g %g re ",
				 x1, y1, x2 - x1, y2 - y1);

    return _cairo_output_stream_get_status (info->output);
}

/* The line cap value is needed to workaround the fact that PostScript
 * and PDF semantics for stroking degenerate sub-paths do not match
 * cairo semantics. (PostScript draws something for any line cap
 * value, while cairo draws something only for round caps).
 *
 * When using this function to emit a path to be filled, rather than
 * stroked, simply pass %CAIRO_LINE_CAP_ROUND which will guarantee that
 * the stroke workaround will not modify the path being emitted.
 */
static cairo_status_t
_cairo_pdf_operators_emit_path (cairo_pdf_operators_t	*pdf_operators,
				cairo_path_fixed_t      *path,
				cairo_matrix_t          *path_transform,
				cairo_line_cap_t         line_cap)
{
    cairo_output_stream_t *word_wrap;
    cairo_status_t status, status2;
    pdf_path_info_t info;
    cairo_box_t box;

    word_wrap = _word_wrap_stream_create (pdf_operators->stream, 72);
    status = _cairo_output_stream_get_status (word_wrap);
    if (status)
	return _cairo_output_stream_destroy (word_wrap);

    info.output = word_wrap;
    info.path_transform = path_transform;
    info.line_cap = line_cap;
    if (_cairo_path_fixed_is_rectangle (path, &box)) {
	status = _cairo_pdf_path_rectangle (&info, &box);
    } else {
	status = _cairo_path_fixed_interpret (path,
					      CAIRO_DIRECTION_FORWARD,
					      _cairo_pdf_path_move_to,
					      _cairo_pdf_path_line_to,
					      _cairo_pdf_path_curve_to,
					      _cairo_pdf_path_close_path,
					      &info);
    }

    status2 = _cairo_output_stream_destroy (word_wrap);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;

    return status;
}

cairo_int_status_t
_cairo_pdf_operators_clip (cairo_pdf_operators_t	*pdf_operators,
			   cairo_path_fixed_t		*path,
			   cairo_fill_rule_t		 fill_rule)
{
    const char *pdf_operator;
    cairo_status_t status;

    if (! path->has_current_point) {
	/* construct an empty path */
	_cairo_output_stream_printf (pdf_operators->stream, "0 0 m ");
    } else {
	status = _cairo_pdf_operators_emit_path (pdf_operators,
						 path,
						 &pdf_operators->cairo_to_pdf,
						 CAIRO_LINE_CAP_ROUND);
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
				 "%s n\n",
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
					cairo_stroke_style_t	*style,
					double			 scale)
{
    double *dash = style->dash;
    int num_dashes = style->num_dashes;
    double dash_offset = style->dash_offset;

    /* PostScript has "special needs" when it comes to zero-length
     * dash segments with butt caps. It apparently (at least
     * according to ghostscript) draws hairlines for this
     * case. That's not what the cairo semantics want, so we first
     * touch up the array to eliminate any 0.0 values that will
     * result in "on" segments.
     */
    if (num_dashes && style->line_cap == CAIRO_LINE_CAP_BUTT) {
	int i;

	/* If there's an odd number of dash values they will each get
	 * interpreted as both on and off. So we first explicitly
	 * expand the array to remove the duplicate usage so that we
	 * can modify some of the values.
	 */
	if (num_dashes % 2) {
	    dash = _cairo_malloc_abc (num_dashes, 2, sizeof (double));
	    if (dash == NULL)
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	    memcpy (dash, style->dash, num_dashes * sizeof (double));
	    memcpy (dash + num_dashes, style->dash, num_dashes * sizeof (double));

	    num_dashes *= 2;
	}

	for (i = 0; i < num_dashes; i += 2) {
	    if (dash[i] == 0.0) {
		/* If we're at the front of the list, we first rotate
		 * two elements from the end of the list to the front
		 * of the list before folding away the 0.0. Or, if
		 * there are only two dash elements, then there is
		 * nothing at all to draw.
		 */
		if (i == 0) {
		    double last_two[2];

		    if (num_dashes == 2) {
			if (dash != style->dash)
			    free (dash);
			return CAIRO_INT_STATUS_NOTHING_TO_DO;
		    }
		    /* The cases of num_dashes == 0, 1, or 3 elements
		     * cannot exist, so the rotation of 2 elements
		     * will always be safe */
		    memcpy (last_two, dash + num_dashes - 2, sizeof (last_two));
		    memmove (dash + 2, dash, (num_dashes - 2) * sizeof (double));
		    memcpy (dash, last_two, sizeof (last_two));
		    dash_offset += dash[0] + dash[1];
		    i = 2;
		}
		dash[i-1] += dash[i+1];
		num_dashes -= 2;
		memmove (dash + i, dash + i + 2, (num_dashes - i) * sizeof (double));
		/* If we might have just rotated, it's possible that
		 * we rotated a 0.0 value to the front of the list.
		 * Set i to -2 so it will get incremented to 0. */
		if (i == 2)
		    i = -2;
	    }
	}
    }

    _cairo_output_stream_printf (pdf_operators->stream,
				 "%f w\n",
				 style->line_width * scale);

    _cairo_output_stream_printf (pdf_operators->stream,
				 "%d J\n",
				 _cairo_pdf_line_cap (style->line_cap));

    _cairo_output_stream_printf (pdf_operators->stream,
				 "%d j\n",
				 _cairo_pdf_line_join (style->line_join));

    if (num_dashes) {
	int d;

	_cairo_output_stream_printf (pdf_operators->stream, "[");
	for (d = 0; d < num_dashes; d++)
	    _cairo_output_stream_printf (pdf_operators->stream, " %f", dash[d] * scale);
	_cairo_output_stream_printf (pdf_operators->stream, "] %f d\n",
				     dash_offset * scale);
    } else {
	_cairo_output_stream_printf (pdf_operators->stream, "[] 0.0 d\n");
    }
    if (dash != style->dash)
        free (dash);

    _cairo_output_stream_printf (pdf_operators->stream,
				 "%f M ",
				 style->miter_limit < 1.0 ? 1.0 : style->miter_limit);

    return _cairo_output_stream_get_status (pdf_operators->stream);
}

/* Scale the matrix so the largest absolute value of the non
 * translation components is 1.0. Return the scale required to restore
 * the matrix to the original values.
 *
 * eg the matrix  [ 100  0  0  50   20   10  ]
 *
 * is rescaled to [  1   0  0  0.5  0.2  0.1 ]
 * and the scale returned is 100
 */
static void
_cairo_matrix_factor_out_scale (cairo_matrix_t *m, double *scale)
{
    double s;

    s = fabs (m->xx);
    if (fabs (m->xy) > s)
	s = fabs (m->xy);
    if (fabs (m->yx) > s)
	s = fabs (m->yx);
    if (fabs (m->yy) > s)
	s = fabs (m->yy);
    *scale = s;
    s = 1.0/s;
    cairo_matrix_scale (m, s, s);
}

static cairo_int_status_t
_cairo_pdf_operators_emit_stroke (cairo_pdf_operators_t	*pdf_operators,
				  cairo_path_fixed_t	*path,
				  cairo_stroke_style_t	*style,
				  cairo_matrix_t	*ctm,
				  cairo_matrix_t	*ctm_inverse,
				  const char 		*pdf_operator)
{
    cairo_status_t status;
    cairo_matrix_t m, path_transform;
    cairo_bool_t has_ctm = TRUE;
    double scale = 1.0;

    /* Optimize away the stroke ctm when it does not affect the
     * stroke. There are other ctm cases that could be optimized
     * however this is the most common.
     */
    if (fabs(ctm->xx) == 1.0 && fabs(ctm->yy) == 1.0 &&
	fabs(ctm->xy) == 0.0 && fabs(ctm->yx) == 0.0)
    {
	has_ctm = FALSE;
    }

    /* The PDF CTM is transformed to the user space CTM when stroking
     * so the corect pen shape will be used. This also requires that
     * the path be transformed to user space when emitted. The
     * conversion of path coordinates to user space may cause rounding
     * errors. For example the device space point (1.234, 3.142) when
     * transformed to a user space CTM of [100 0 0 100 0 0] will be
     * emitted as (0.012, 0.031).
     *
     * To avoid the rounding problem we scale the user space CTM
     * matrix so that all the non translation components of the matrix
     * are <= 1. The line width and and dashes are scaled by the
     * inverse of the scale applied to the CTM. This maintains the
     * shape of the stroke pen while keeping the user space CTM within
     * the range that maximizes the precision of the emitted path.
     */
    if (has_ctm) {
	m = *ctm;
	/* Zero out the translation since it does not affect the pen
	 * shape however it may cause unnecessary digits to be emitted.
	 */
	m.x0 = 0.0;
	m.y0 = 0.0;
	_cairo_matrix_factor_out_scale (&m, &scale);
	path_transform = m;
	status = cairo_matrix_invert (&path_transform);
	if (status)
	    return status;

	cairo_matrix_multiply (&m, &m, &pdf_operators->cairo_to_pdf);
    }

    status = _cairo_pdf_operators_emit_stroke_style (pdf_operators, style, scale);
    if (status == CAIRO_INT_STATUS_NOTHING_TO_DO)
	return CAIRO_STATUS_SUCCESS;
    if (status)
	return status;

    if (has_ctm) {
	_cairo_output_stream_printf (pdf_operators->stream,
				     "q %f %f %f %f %f %f cm\n",
				     m.xx, m.yx, m.xy, m.yy,
				     m.x0, m.y0);
    } else {
	path_transform = pdf_operators->cairo_to_pdf;
    }

    status = _cairo_pdf_operators_emit_path (pdf_operators,
					     path,
					     &path_transform,
					     style->line_cap);
    if (status)
	return status;

    _cairo_output_stream_printf (pdf_operators->stream, "%s", pdf_operator);
    if (has_ctm)
	_cairo_output_stream_printf (pdf_operators->stream, " Q");

    _cairo_output_stream_printf (pdf_operators->stream, "\n");

    return _cairo_output_stream_get_status (pdf_operators->stream);
}

cairo_int_status_t
_cairo_pdf_operators_stroke (cairo_pdf_operators_t	*pdf_operators,
			     cairo_path_fixed_t		*path,
			     cairo_stroke_style_t	*style,
			     cairo_matrix_t		*ctm,
			     cairo_matrix_t		*ctm_inverse)
{
    return _cairo_pdf_operators_emit_stroke (pdf_operators,
					     path,
					     style,
					     ctm,
					     ctm_inverse,
					     "S");
}

cairo_int_status_t
_cairo_pdf_operators_fill (cairo_pdf_operators_t	*pdf_operators,
			   cairo_path_fixed_t		*path,
			   cairo_fill_rule_t		fill_rule)
{
    const char *pdf_operator;
    cairo_status_t status;

    status = _cairo_pdf_operators_emit_path (pdf_operators,
					     path,
					     &pdf_operators->cairo_to_pdf,
					     CAIRO_LINE_CAP_ROUND);
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
				 "%s\n",
				 pdf_operator);

    return _cairo_output_stream_get_status (pdf_operators->stream);
}

cairo_int_status_t
_cairo_pdf_operators_fill_stroke (cairo_pdf_operators_t 	*pdf_operators,
				  cairo_path_fixed_t		*path,
				  cairo_fill_rule_t	 	 fill_rule,
				  cairo_stroke_style_t	        *style,
				  cairo_matrix_t		*ctm,
				  cairo_matrix_t		*ctm_inverse)
{
    const char *operator;

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	operator = "B";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	operator = "B*";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    return _cairo_pdf_operators_emit_stroke (pdf_operators,
					     path,
					     style,
					     ctm,
					     ctm_inverse,
					     operator);
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
    cairo_status_t status, status_ignored;
    double Tlm_x = 0, Tlm_y = 0;
    double Tm_x = 0, y;
    int i, hex_width;
    cairo_output_stream_t *word_wrap_stream;

    for (i = 0; i < num_glyphs; i++)
	cairo_matrix_transform_point (&pdf_operators->cairo_to_pdf, &glyphs[i].x, &glyphs[i].y);

    word_wrap_stream = _word_wrap_stream_create (pdf_operators->stream, 72);
    status = _cairo_output_stream_get_status (word_wrap_stream);
    if (status)
	return _cairo_output_stream_destroy (word_wrap_stream);

    _cairo_output_stream_printf (word_wrap_stream,
				 "BT\n");

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
	if (status) {
	    status_ignored = _cairo_output_stream_destroy (word_wrap_stream);
            return status;
	}

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
                _cairo_output_stream_printf (word_wrap_stream, ">] TJ\n");
                in_TJ = FALSE;
            }
	    _cairo_output_stream_printf (word_wrap_stream,
					 "/f-%d-%d 1 Tf\n",
					 subset_glyph.font_id,
					 subset_glyph.subset_id);
	    if (pdf_operators->use_font_subset) {
		status = pdf_operators->use_font_subset (subset_glyph.font_id,
							 subset_glyph.subset_id,
							 pdf_operators->use_font_subset_closure);
		if (status) {
		    status_ignored = _cairo_output_stream_destroy (word_wrap_stream);
		    return status;
		}
	    }
        }

        if (subset_glyph.subset_id != current_subset_id || !diagonal) {
            _cairo_output_stream_printf (word_wrap_stream,
                                         "%f %f %f %f %f %f Tm\n",
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
                        _cairo_output_stream_printf (word_wrap_stream,
                                                     "%f %f Td\n",
                                                     (glyphs[i].x - Tlm_x)/scaled_font->scale.xx,
                                                     (glyphs[i].y - Tlm_y)/scaled_font->scale.yy);

                        Tlm_x = glyphs[i].x;
                        Tlm_y = glyphs[i].y;
                        Tm_x = Tlm_x;
                    }
                    _cairo_output_stream_printf (word_wrap_stream,
                                                 "[<%0*x",
                                                 hex_width,
                                                 subset_glyph.subset_glyph_index);
                    Tm_x += subset_glyph.x_advance;
                    in_TJ = TRUE;
                } else {
                    if (fabs((glyphs[i].x - Tm_x)/scaled_font->scale.xx) > GLYPH_POSITION_TOLERANCE) {
                        double delta = glyphs[i].x - Tm_x;
			int rounded_delta;

			delta = -1000.0*delta/scaled_font->scale.xx;
			/* As the delta is in 1/1000 of a unit of text
			 * space, rounding to an integer should still
			 * provide sufficient precision. We round the
			 * delta before adding to Tm_x so that we keep
			 * track of the accumulated rounding error in
			 * the PDF interpreter and compensate for it
			 * when calculating subsequent deltas.
			 */
			rounded_delta = _cairo_lround (delta);
			if (rounded_delta != 0) {
			    _cairo_output_stream_printf (word_wrap_stream,
							 "> %d <",
							 rounded_delta);
			}

			/* Convert the rounded delta back to cairo
			 * space before adding to the current text
			 * position. */
			delta = rounded_delta*scaled_font->scale.xx/-1000.0;
                        Tm_x += delta;
                    }
                    _cairo_output_stream_printf (word_wrap_stream,
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
			int rounded_delta;

			delta = -1000.0*delta/scaled_font->scale.xx;
			rounded_delta = _cairo_lround (delta);
			if (rounded_delta != 0) {
			    _cairo_output_stream_printf (word_wrap_stream,
							 "> %d <",
							 rounded_delta);
			}
			delta = rounded_delta*scaled_font->scale.xx/-1000.0;
                        Tm_x += delta;
                    }
                    _cairo_output_stream_printf (word_wrap_stream,
                                                 "%0*x>] TJ\n",
                                                 hex_width,
                                                 subset_glyph.subset_glyph_index);
                    Tm_x += subset_glyph.x_advance;
                    in_TJ = FALSE;
                } else {
                    if (i != 0) {
                        _cairo_output_stream_printf (word_wrap_stream,
                                                     "%f %f Td ",
                                                     (glyphs[i].x - Tlm_x)/scaled_font->scale.xx,
                                                     (glyphs[i].y - Tlm_y)/scaled_font->scale.yy);
                        Tlm_x = glyphs[i].x;
                        Tlm_y = glyphs[i].y;
                        Tm_x = Tlm_x;
                    }
                    _cairo_output_stream_printf (word_wrap_stream,
                                                 "<%0*x> Tj ",
                                                 hex_width,
                                                 subset_glyph.subset_glyph_index);
                    Tm_x += subset_glyph.x_advance;
                }
            }
        } else {
            _cairo_output_stream_printf (word_wrap_stream,
                                         "<%0*x> Tj\n",
                                         hex_width,
                                         subset_glyph.subset_glyph_index);
        }
    }

    _cairo_output_stream_printf (word_wrap_stream,
				 "ET\n");

    status = _cairo_output_stream_destroy (word_wrap_stream);
    if (status)
	return status;

    return _cairo_output_stream_get_status (pdf_operators->stream);
}

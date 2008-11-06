/*
 * Copyright © 2004 Keith Packard
 * Copyright © 2008 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is Keith Packard
 *
 * Contributor(s):
 *      Keith Packard <keithp@keithp.com>
 *      Behdad Esfahbod <behdad@behdad.org>
 */

#include "cairoint.h"

/*
 * This file implements a user-font rendering the decendant of the Hershey
 * font coded by Keith Packard for use in the Twin window system.
 * The actual font data is in cairo-font-face-twin-data.c
 *
 * Ported to cairo user font by Behdad Esfahbod.
 */


#define twin_glyph_left(g)      ((g)[0])
#define twin_glyph_right(g)     ((g)[1])
#define twin_glyph_ascent(g)    ((g)[2])
#define twin_glyph_descent(g)   ((g)[3])

#define twin_glyph_n_snap_x(g)  ((g)[4])
#define twin_glyph_n_snap_y(g)  ((g)[5])
#define twin_glyph_snap_x(g)    (&g[6])
#define twin_glyph_snap_y(g)    (twin_glyph_snap_x(g) + twin_glyph_n_snap_x(g))
#define twin_glyph_draw(g)      (twin_glyph_snap_y(g) + twin_glyph_n_snap_y(g))

#define SNAPI(p)	(p)
#define SNAPH(p)	(p)

#define FX(g)		((g) / 64.)
#define FY(g)		((g) / 64.)


static cairo_status_t
twin_scaled_font_init (cairo_scaled_font_t  *scaled_font,
		       cairo_t              *cr,
		       cairo_font_extents_t *metrics)
{
  metrics->ascent  = FY (50);
  metrics->descent = FY (14);
  return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
twin_scaled_font_unicode_to_glyph (cairo_scaled_font_t *scaled_font,
				   unsigned long        unicode,
				   unsigned long       *glyph)
{
    /* We use an identity charmap.  Which means we could live
     * with no unicode_to_glyph method too.  But we define this
     * to map all unknown chars to a single unknown glyph to
     * reduce pressure on cache. */

    if (unicode < ARRAY_LENGTH (_cairo_twin_charmap))
	*glyph = unicode;
    else
	*glyph = 0;

    return CAIRO_STATUS_SUCCESS;
}

#define SNAPX(p)	_twin_snap (p, info.snap_x, info.n_snap_x)
#define SNAPY(p)	_twin_snap (p, info.snap_y, info.n_snap_y)

static double
_twin_snap (double v, int a, int b)
{
    return v; /* XXX */
}

static cairo_status_t
twin_scaled_font_render_glyph (cairo_scaled_font_t  *scaled_font,
			       unsigned long         glyph,
			       cairo_t              *cr,
			       cairo_text_extents_t *metrics)
{
    double x1, y1, x2, y2, x3, y3;
    const int8_t *b = _cairo_twin_outlines +
		      _cairo_twin_charmap[glyph >= ARRAY_LENGTH (_cairo_twin_charmap) ? 0 : glyph];
    const int8_t *g = twin_glyph_draw(b);

    struct {
      cairo_bool_t snap;
      int snap_x;
      int snap_y;
      int n_snap_x;
      int n_snap_y;
    } info = {FALSE};

    cairo_set_line_width (cr, 0.06);
    cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

    for (;;) {
	switch (*g++) {
	case 'M':
	    cairo_close_path (cr);
	    /* fall through */
	case 'm':
	    x1 = FX(*g++);
	    y1 = FY(*g++);
	    if (info.snap)
	    {
		x1 = SNAPX (x1);
		y1 = SNAPY (y1);
	    }
	    cairo_move_to (cr, x1, y1);
	    continue;
	case 'L':
	    cairo_close_path (cr);
	    /* fall through */
	case 'l':
	    x1 = FX(*g++);
	    y1 = FY(*g++);
	    if (info.snap)
	    {
		x1 = SNAPX (x1);
		y1 = SNAPY (y1);
	    }
	    cairo_line_to (cr, x1, y1);
	    continue;
	case 'C':
	    cairo_close_path (cr);
	    /* fall through */
	case 'c':
	    x1 = FX(*g++);
	    y1 = FY(*g++);
	    x2 = FX(*g++);
	    y2 = FY(*g++);
	    x3 = FX(*g++);
	    y3 = FY(*g++);
	    if (info.snap)
	    {
		x1 = SNAPX (x1);
		y1 = SNAPY (y1);
		x2 = SNAPX (x2);
		y2 = SNAPY (y2);
		x3 = SNAPX (x3);
		y3 = SNAPY (y3);
	    }
	    cairo_curve_to (cr, x1, y1, x2, y2, x3, y3);
	    continue;
	case 'E':
	    cairo_close_path (cr);
	    /* fall through */
	case 'e':
	    cairo_stroke (cr);
	    break;
	case 'X':
	    /* filler */
	    continue;
	}
	break;
    }

    metrics->x_advance = FX(twin_glyph_right(b)) + cairo_get_line_width (cr);
    metrics->x_advance +=  cairo_get_line_width (cr)/* XXX 2*x.margin */;
    if (info.snap)
	metrics->x_advance = SNAPI (SNAPX (metrics->x_advance));


    return CAIRO_STATUS_SUCCESS;
}

cairo_font_face_t *
_cairo_font_face_twin_create (cairo_font_slant_t slant,
			      cairo_font_weight_t weight)
{
    cairo_font_face_t *twin_font_face;

    twin_font_face = cairo_user_font_face_create ();
    cairo_user_font_face_set_init_func             (twin_font_face, twin_scaled_font_init);
    cairo_user_font_face_set_render_glyph_func     (twin_font_face, twin_scaled_font_render_glyph);
    cairo_user_font_face_set_unicode_to_glyph_func (twin_font_face, twin_scaled_font_unicode_to_glyph);

    return twin_font_face;
}

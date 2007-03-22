/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
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
 *	Carl D. Worth <cworth@cworth.org>
 */

#include "cairoint.h"

static cairo_color_t const cairo_color_white = {
    1.0,    1.0,    1.0,    1.0,
    0xffff, 0xffff, 0xffff, 0xffff
};

static cairo_color_t const cairo_color_black = {
    0.0, 0.0, 0.0, 1.0,
    0x0, 0x0, 0x0, 0xffff
};

static cairo_color_t const cairo_color_transparent = {
    0.0, 0.0, 0.0, 0.0,
    0x0, 0x0, 0x0, 0x0
};

static cairo_color_t const cairo_color_magenta = {
    1.0,    0.0, 1.0,    1.0,
    0xffff, 0x0, 0xffff, 0xffff
};

const cairo_color_t *
_cairo_stock_color (cairo_stock_t stock)
{
    switch (stock) {
    case CAIRO_STOCK_WHITE:
	return &cairo_color_white;
    case CAIRO_STOCK_BLACK:
	return &cairo_color_black;
    case CAIRO_STOCK_TRANSPARENT:
	return &cairo_color_transparent;
    }

    ASSERT_NOT_REACHED;

    /* If the user can get here somehow, give a color that indicates a
     * problem. */
    return &cairo_color_magenta;
}

void
_cairo_color_init (cairo_color_t *color)
{
    *color = cairo_color_white;
}

void
_cairo_color_init_rgb (cairo_color_t *color,
		       double red, double green, double blue)
{
    _cairo_color_init_rgba (color, red, green, blue, 1.0);
}

/* Convert a double in [0.0, 1.0] to an integer in [0, 65535]
 * The conversion is designed to divide the input range into 65536
 * equally-sized regions. This is achieved by multiplying by 65536 and
 * then special-casing the result of an input value of 1.0 so that it
 * maps to 65535 instead of 65536.
 */
static inline uint16_t _color_to_short (double d)
{
    uint32_t i;
    i = (uint32_t) (d * 65536);
    i -= (i >> 16);
    return i;
}

static void
_cairo_color_compute_shorts (cairo_color_t *color)
{
    color->red_short   = _color_to_short (color->red   * color->alpha);
    color->green_short = _color_to_short (color->green * color->alpha);
    color->blue_short  = _color_to_short (color->blue  * color->alpha);
    color->alpha_short = _color_to_short (color->alpha);
}

void
_cairo_color_init_rgba (cairo_color_t *color,
			double red, double green, double blue,
			double alpha)
{
    color->red   = red;
    color->green = green;
    color->blue  = blue;
    color->alpha = alpha;

    _cairo_color_compute_shorts (color);
}

void
_cairo_color_multiply_alpha (cairo_color_t *color,
			     double	    alpha)
{
    color->alpha *= alpha;

    _cairo_color_compute_shorts (color);
}

void
_cairo_color_get_rgba (cairo_color_t *color,
		       double	     *red,
		       double	     *green,
		       double	     *blue,
		       double	     *alpha)
{
    *red   = color->red;
    *green = color->green;
    *blue  = color->blue;
    *alpha = color->alpha;
}

void
_cairo_color_get_rgba_premultiplied (cairo_color_t *color,
				     double	   *red,
				     double	   *green,
				     double	   *blue,
				     double	   *alpha)
{
    *red   = color->red   * color->alpha;
    *green = color->green * color->alpha;
    *blue  = color->blue  * color->alpha;
    *alpha = color->alpha;
}

/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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

static cairo_color_t const CAIRO_COLOR_WHITE = {
    1.0, 1.0, 1.0, 1.0,
    0xffff, 0xffff, 0xffff, 0xffff
};

static void
_cairo_color_compute_shorts (cairo_color_t *color);

void
_cairo_color_init (cairo_color_t *color)
{
    *color = CAIRO_COLOR_WHITE;
}

void
_cairo_color_fini (cairo_color_t *color)
{
    /* Nothing to do here */
}

void
_cairo_color_set_rgb (cairo_color_t *color, double red, double green, double blue)
{
    color->red   = red;
    color->green = green;
    color->blue  = blue;

    _cairo_color_compute_shorts (color);
}

void
_cairo_color_get_rgb (const cairo_color_t *color,
		      double *red, double *green, double *blue)
{
    if (red)
	*red   = color->red;
    if (green)
	*green = color->green;
    if (blue)
	*blue  = color->blue;
}

void
_cairo_color_set_alpha (cairo_color_t *color, double alpha)
{
    color->alpha = alpha;

    _cairo_color_compute_shorts (color);
}

static void
_cairo_color_compute_shorts (cairo_color_t *color)
{
    color->red_short   = (color->red   * color->alpha) * 0xffff;
    color->green_short = (color->green * color->alpha) * 0xffff;
    color->blue_short  = (color->blue  * color->alpha) * 0xffff;
    color->alpha_short =  color->alpha * 0xffff;
}


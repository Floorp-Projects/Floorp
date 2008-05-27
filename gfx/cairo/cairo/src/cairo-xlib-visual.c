/* Cairo - a vector graphics library with display and print output
 *
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include "cairoint.h"

#include "cairo-xlib-private.h"

/* A perceptual distance metric between two colors. No sqrt needed
 * since the square of the distance is still a valid metric. */

/* XXX: This is currently using linear distance in RGB space which is
 * decidedly not perceptually linear. If someone cared a lot about the
 * quality, they might choose something else here. Then again, they
 * might also choose not to use a PseudoColor visual... */
static inline int
_color_distance (unsigned short r1, unsigned short g1, unsigned short b1,
		 unsigned short r2, unsigned short g2, unsigned short b2)
{
    r1 >>= 8; g1 >>= 8; b1 >>= 8;
    r2 >>= 8; g2 >>= 8; b2 >>= 8;

    return ((r2 - r1) * (r2 - r1) +
	    (g2 - g1) * (g2 - g1) +
	    (b2 - b1) * (b2 - b1));
}

cairo_status_t
_cairo_xlib_visual_info_create (Display *dpy,
	                        int screen,
				VisualID visualid,
				cairo_xlib_visual_info_t **out)
{
    cairo_xlib_visual_info_t *info;
    Colormap colormap = DefaultColormap (dpy, screen);
    XColor color;
    int gray, red, green, blue;
    int i, index, distance, min_distance = 0;

    const unsigned short index5_to_short[5] = {
	0x0000, 0x4000, 0x8000, 0xc000, 0xffff
    };
    const unsigned short index8_to_short[8] = {
	0x0000, 0x2492, 0x4924, 0x6db6,
	0x9249, 0xb6db, 0xdb6d, 0xffff
    };

    info = malloc (sizeof (cairo_xlib_visual_info_t));
    if (info == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    info->visualid = visualid;

    /* Allocate a 16-entry gray ramp and a 5x5x5 color cube. Give up
     * as soon as failures start. */
    for (gray = 0; gray < 16; gray++) {
	color.red   = (gray << 12) | (gray << 8) | (gray << 4) | gray;
	color.green = (gray << 12) | (gray << 8) | (gray << 4) | gray;
	color.blue  = (gray << 12) | (gray << 8) | (gray << 4) | gray;
	if (! XAllocColor (dpy, colormap, &color))
	    goto DONE_ALLOCATE;
    }

    /* XXX: Could do this in a more clever order to have the best
     * possible results from early failure. Could also choose a cube
     * uniformly distributed in a better space than RGB. */
    for (red = 0; red < 5; red++) {
	for (green = 0; green < 5; green++) {
	    for (blue = 0; blue < 5; blue++) {
		color.red = index5_to_short[red];
		color.green = index5_to_short[green];
		color.blue = index5_to_short[blue];
		color.pixel = 0;
		color.flags = 0;
		color.pad = 0;
		if (! XAllocColor (dpy, colormap, &color))
		    goto DONE_ALLOCATE;
	    }
	}
    }
  DONE_ALLOCATE:

    for (i = 0; i < ARRAY_LENGTH (info->colors); i++)
	info->colors[i].pixel = i;
    XQueryColors (dpy, colormap, info->colors, ARRAY_LENGTH (info->colors));

    /* Search for nearest colors within allocated colormap. */
    for (red = 0; red < 8; red++) {
	for (green = 0; green < 8; green++) {
	    for (blue = 0; blue < 8; blue++) {
		index = (red << 6) | (green << 3) | (blue);
		for (i = 0; i < 256; i++) {
		    distance = _color_distance (index8_to_short[red],
						index8_to_short[green],
						index8_to_short[blue],
						info->colors[i].red,
						info->colors[i].green,
						info->colors[i].blue);
		    if (i == 0 || distance < min_distance) {
			info->rgb333_to_pseudocolor[index] = info->colors[i].pixel;
			min_distance = distance;
		    }
		}
	    }
	}
    }

    *out = info;
    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_xlib_visual_info_destroy (Display *dpy, cairo_xlib_visual_info_t *info)
{
    /* No need for XFreeColors() whilst using DefaultColormap */
    free (info);
}

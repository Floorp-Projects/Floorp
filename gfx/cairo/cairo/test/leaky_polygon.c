/*
 * Copyright © 2005 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@cworth.org>
 */

/* Bug history
 *
 * 2005-01-07 Carl Worth <cworth@cworth.org>
 *
 *   Bug reported:
 *
 *     From: Chris <fltk@functionalfuture.com>
 *     Subject: [cairo] Render to image buffer artifacts
 *     To: cairo@cairographics.org
 *     Date: Fri, 07 Jan 2005 02:22:28 -0500
 *
 *     I've attached the code and image that shows this off.  Scaling at
 *     different levels seems to change the corruption.
 *
 *     For some reason there are artifacts in the alpha channel.  I don't know
 *     if that's the only place, but the alpha channel looks bad.
 *
 *     If you run the code and parse the attached image, directing stdout to a
 *     file, you can see in the lower left corner there are alpha values where
 *     it should be transparent.
 *     [...]
 *
 * 2005-01-11 Carl Worth <cworth@cworth.org>
 *
 *   I trimmed the original test case down to the code that appears here.
 *
 */

#include "cairo_test.h"

#define WIDTH 21
#define HEIGHT 21

cairo_test_t test = {
    "leaky_polygon",
    "Exercises a corner case in the trapezoid rasterization in which pixels outside the trapezoids received a non-zero alpha",
    WIDTH, HEIGHT
};

static void
draw (cairo_t *cr, int width, int height)
{
    cairo_scale (cr, 1.0/(1<<16), 1.0/(1<<16));

    cairo_move_to (cr, 131072,39321);
    cairo_line_to (cr, 1103072,1288088);
    cairo_line_to (cr, 1179648,1294990);
    cairo_close_path (cr);

    cairo_fill (cr);
}

int
main (void)
{
    return cairo_test (&test, draw);
}

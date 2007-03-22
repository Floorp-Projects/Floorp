/*
 * Copyright © 2004 Red Hat, Inc.
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
 * 2004-10-25  Carl Worth  <cworth@cworth.org>
 *
 *   It looks like cairo_show_surface has no effect if it follows a
 *   call to cairo_move_to to any coordinate other than 0,0. A little
 *   bit of poking around suggests this isn't a regression, (at least
 *   not since the last pixman snapshot).
 *
 */


#include "cairo_test.h"

cairo_test_t test = {
    "move_to_show_surface",
    "Tests calls to cairo_show_surface after cairo_move_to",
    2, 2
};

static void
draw (cairo_t *cr, int width, int height)
{
    cairo_surface_t *surface;
    uint32_t colors[4] = {
	0xffffffff, 0xffff0000,
	0xff00ff00, 0xff0000ff
    };
    int i;

    for (i=0; i < 4; i++) {
	surface = cairo_surface_create_for_image ((char *) &colors[i],
						  CAIRO_FORMAT_ARGB32, 1, 1, 4);
	cairo_move_to (cr, i % 2, i / 2);
	cairo_show_surface (cr, surface, 1, 1);
	cairo_surface_destroy (surface);
    }
}

int
main (void)
{
    return cairo_test (&test, draw);
}

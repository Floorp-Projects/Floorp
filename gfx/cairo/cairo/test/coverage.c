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
 * Author: Kristian Høgsberg <krh@redhat.com>
 */

#include <math.h>
#include "cairo_test.h"

#define WIDTH 64
#define HEIGHT 64
#define PAD 10

/* XXX The test image uses Bitstream Vera Sans as the font - how do we
 * ensure that it's available?  Can we ship it with this test? */

const char	fontname[]	= "Bitstream Vera Sans";
const int	fontsize	= 40;
const char	png_filename[]	= "romedalen.png";

static void
set_solid_pattern (cairo_t *cr, int x, int y)
{
    cairo_set_rgb_color (cr, 0, 0, 0.6);
    cairo_set_alpha (cr, 1.0);
}

static void
set_translucent_pattern (cairo_t *cr, int x, int y)
{
    cairo_set_rgb_color (cr, 0, 0, 0.6);
    cairo_set_alpha (cr, 0.5);
}

static void
set_gradient_pattern (cairo_t *cr, int x, int y)
{
    cairo_pattern_t *pattern;

    pattern =
	cairo_pattern_create_linear (x, y, x + WIDTH, y + HEIGHT);
    cairo_pattern_add_color_stop (pattern, 0, 1, 1, 1, 1);
    cairo_pattern_add_color_stop (pattern, 1, 0, 0, 0.4, 1);
    cairo_set_pattern (cr, pattern);
    cairo_set_alpha (cr, 1);
}

static void
set_image_pattern (cairo_t *cr, int x, int y)
{
    cairo_pattern_t *pattern;

    pattern = cairo_test_create_png_pattern (cr, png_filename);
    cairo_set_pattern (cr, pattern);
    cairo_set_alpha (cr, 1);
}

static void
set_translucent_image_pattern (cairo_t *cr, int x, int y)
{
    cairo_pattern_t *pattern;

    pattern = cairo_test_create_png_pattern (cr, png_filename);
    cairo_set_pattern (cr, pattern);
    cairo_set_alpha (cr, 0.5);
}

static void
draw_text (cairo_t *cr, int x, int y)
{
    cairo_rel_move_to (cr, 0, fontsize);
    cairo_show_text (cr, "Aa");
}

static void
draw_polygon (cairo_t *cr, int x, int y)
{
    cairo_new_path (cr);
    cairo_move_to (cr, x, y);
    cairo_line_to (cr, x, y + HEIGHT);
    cairo_line_to (cr, x + WIDTH / 2, y + 3 * HEIGHT / 4);
    cairo_line_to (cr, x + WIDTH, y + HEIGHT);
    cairo_line_to (cr, x + WIDTH, y);
    cairo_line_to (cr, x + WIDTH / 2, y + HEIGHT / 4);
    cairo_close_path (cr);
    cairo_fill (cr);
}

static void (*pattern_funcs[])(cairo_t *cr, int x, int y) = {
    set_solid_pattern,
    set_translucent_pattern,
    set_gradient_pattern,
    set_image_pattern,
    set_translucent_image_pattern
};

static void (*draw_funcs[])(cairo_t *cr, int x, int y) = {
    draw_text,
    draw_polygon,
};

#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))
#define IMAGE_WIDTH (ARRAY_SIZE (pattern_funcs) * (WIDTH + PAD) + PAD)
#define IMAGE_HEIGHT (ARRAY_SIZE (draw_funcs) * (HEIGHT + PAD) * 2 + PAD)

static cairo_test_t test = {
    "coverage",
    "Various coverage test of cairo",
    IMAGE_WIDTH, IMAGE_HEIGHT
};


static void
draw (cairo_t *cr, int width, int height)
{
    /* TODO: pattern fill, gradient fill, clipping, gradient clipping,
       path+solid alpha mask clipping */

    int i, j, x, y;

    cairo_select_font (cr, fontname,
		       CAIRO_FONT_SLANT_NORMAL,
		       CAIRO_FONT_WEIGHT_BOLD);
    cairo_scale_font (cr, fontsize);

    for (j = 0; j < ARRAY_SIZE (draw_funcs); j++) {
	for (i = 0; i < ARRAY_SIZE (pattern_funcs); i++) {
	    x = i * (WIDTH + PAD) + PAD;
	    y = j * (HEIGHT + PAD) + PAD;
	    cairo_move_to (cr, x, y);
	    pattern_funcs[i] (cr, x, y);
	    draw_funcs[j] (cr, x, y);
	}
    }

    for (j = 0; j < ARRAY_SIZE (draw_funcs); j++) {
	for (i = 0; i < ARRAY_SIZE (pattern_funcs); i++) {
	    x = i * (WIDTH + PAD) + PAD;
	    y = (ARRAY_SIZE (draw_funcs) + j) * (HEIGHT + PAD) + PAD;

	    cairo_save (cr);

	    cairo_set_alpha (cr, 1.0);
	    cairo_new_path (cr);
	    cairo_arc (cr, x + WIDTH / 2, y + HEIGHT / 2,
		       WIDTH / 3, 0, 2 * M_PI);
	    cairo_clip (cr);

	    cairo_move_to (cr, x, y);
	    pattern_funcs[i] (cr, x, y);
	    draw_funcs[j] (cr, x, y);

	    cairo_restore (cr);

	}
    }
}

int
main (void)
{
    return cairo_test (&test, draw);
}

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

#ifndef _CAIRO_TEST_H_
#define _CAIRO_TEST_H_

#include <math.h>
#include <cairo.h>

typedef enum cairo_test_status {
    CAIRO_TEST_SUCCESS = 0,
    CAIRO_TEST_FAILURE
} cairo_test_status_t;

typedef struct cairo_test {
    char *name;
    char *description;
    int width;
    int height;
} cairo_test_t;

typedef void  (*cairo_test_draw_function_t) (cairo_t *cr, int width, int height);

/* cairo_test.c */
cairo_test_status_t
cairo_test (cairo_test_t *test, cairo_test_draw_function_t draw);

cairo_pattern_t *
cairo_test_create_png_pattern (cairo_t *cr, const char *filename);


#endif


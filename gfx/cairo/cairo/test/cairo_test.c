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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "cairo_test.h"

#include "read_png.h"
#include "write_png.h"
#include "xmalloc.h"

#define CAIRO_TEST_PNG_SUFFIX "-out.png"
#define CAIRO_TEST_REF_SUFFIX "-ref.png"
#define CAIRO_TEST_DIFF_SUFFIX "-diff.png"

static void
xasprintf (char **strp, const char *fmt, ...)
{
    va_list va;
    int ret;

    va_start (va, fmt);
    ret = vasprintf (strp, fmt, va);
    va_end (va);

    if (ret < 0) {
	fprintf (stderr, "Out of memory\n");
	exit (1);
    }
}

/* Image comparison code courttesy of Richard Worth.
 * Returns number of pixels changed.
 * Also fills out a "diff" image intended to visually show where the
 * images differ.
 */
static int
image_diff (char *buf_a, char *buf_b, char *buf_diff,
	    int width, int height, int stride)
{
    int x, y;
    int total_pixels_changed = 0;
    unsigned char *row_a, *row_b, *row;

    for (y = 0; y < height; y++)
    {
	row_a = buf_a + y * stride;
	row_b = buf_b + y * stride;
	row = buf_diff + y * stride;
	for (x = 0; x < width; x++)
	{
	    int channel;
	    unsigned char value_a, value_b;
	    int pixel_changed = 0;
	    for (channel = 0; channel < 4; channel++)
	    {
		double diff;
		value_a = row_a[x * 4 + channel];
		value_b = row_b[x * 4 + channel];
		if (value_a != value_b)
		    pixel_changed = 1;
		diff = value_a - value_b;
		row[x * 4 + channel] = 128 + diff / 3.0;
	    }
	    if (pixel_changed) {
		total_pixels_changed++;
	    } else {
		row[x*4+0] = 0;
		row[x*4+1] = 0;
		row[x*4+2] = 0;
	    }
	    row[x * 4 + 3] = 0xff; /* Set ALPHA to 100% (opaque) */
	}
    }

    return total_pixels_changed;
}

cairo_test_status_t
cairo_test (cairo_test_t *test, cairo_test_draw_function_t draw)
{
    cairo_t *cr;
    int stride;
    unsigned char *png_buf, *ref_buf, *diff_buf;
    char *png_name, *ref_name, *diff_name;
    char *srcdir;
    int pixels_changed;
    int ref_width, ref_height, ref_stride;
    read_png_status_t png_status;
    cairo_test_status_t ret;

    /* The cairo part of the test is the easiest part */
    cr = cairo_create ();

    stride = 4 * test->width;

    png_buf = xcalloc (stride * test->height, 1);
    diff_buf = xcalloc (stride * test->height, 1);

    cairo_set_target_image (cr, png_buf, CAIRO_FORMAT_ARGB32,
			    test->width, test->height, stride);

    (draw) (cr, test->width, test->height);

    cairo_destroy (cr);

    /* Skip image check for tests with no image (width,height == 0,0) */
    if (test->width == 0 || test->height == 0) {
	free (png_buf);
	free (diff_buf);
	return CAIRO_TEST_SUCCESS;
    }

    /* Then we've got a bunch of string manipulation and file I/O for the check */
    srcdir = getenv ("srcdir");
    if (!srcdir)
	srcdir = ".";
    xasprintf (&png_name, "%s%s", test->name, CAIRO_TEST_PNG_SUFFIX);
    xasprintf (&ref_name, "%s/%s%s", srcdir, test->name, CAIRO_TEST_REF_SUFFIX);
    xasprintf (&diff_name, "%s%s", test->name, CAIRO_TEST_DIFF_SUFFIX);

    write_png_argb32 (png_buf, png_name, test->width, test->height, stride);

    ref_buf = NULL;
    png_status = (read_png_argb32 (ref_name, &ref_buf, &ref_width, &ref_height, &ref_stride));
    if (png_status) {
	switch (png_status)
	{
	case READ_PNG_FILE_NOT_FOUND:
	    fprintf (stderr, "  Error: No reference image found: %s\n", ref_name);
	    break;
	case READ_PNG_FILE_NOT_PNG:
	    fprintf (stderr, "  Error: %s is not a png image\n", ref_name);
	    break;
	default:
	    fprintf (stderr, "  Error: Failed to read %s\n", ref_name);
	}
		
	ret = CAIRO_TEST_FAILURE;
	goto BAIL;
    }

    if (test->width != ref_width || test->height != ref_height) {
	fprintf (stderr,
		 "  Error: Image size mismatch: (%dx%d) vs. (%dx%d)\n"
		 "         for %s vs %s\n",
		 test->width, test->height,
		 ref_width, ref_height,
		 png_name, ref_name);
	ret = CAIRO_TEST_FAILURE;
	goto BAIL;
    }

    pixels_changed = image_diff (png_buf, ref_buf, diff_buf,
				 test->width, test->height, stride);
    if (pixels_changed) {
	fprintf (stderr, "  Error: %d pixels differ from reference image %s\n",
		 pixels_changed, ref_name);
	write_png_argb32 (diff_buf, diff_name, test->width, test->height, stride);
	ret = CAIRO_TEST_FAILURE;
	goto BAIL;
    } else {
	if (unlink (diff_name) < 0 && errno != ENOENT) {
	    fprintf (stderr, "  Error: Cannot remove %s: %s\n",
		     diff_name, strerror (errno));
	    ret = CAIRO_TEST_FAILURE;
	    goto BAIL;
	}
    }

    ret = CAIRO_TEST_SUCCESS;

BAIL:
    free (png_buf);
    free (ref_buf);
    free (diff_buf);
    free (png_name);
    free (ref_name);
    free (diff_name);

    return ret;
}

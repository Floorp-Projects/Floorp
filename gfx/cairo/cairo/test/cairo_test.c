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

#include "buffer_diff.h"
#include "read_png.h"
#include "write_png.h"
#include "xmalloc.h"

#define CAIRO_TEST_LOG_SUFFIX ".log"
#define CAIRO_TEST_PNG_SUFFIX "-out.png"
#define CAIRO_TEST_REF_SUFFIX "-ref.png"
#define CAIRO_TEST_DIFF_SUFFIX "-diff.png"

static void
xasprintf (char **strp, const char *fmt, ...)
{
#ifdef HAVE_VASPRINTF    
    va_list va;
    int ret;
    
    va_start (va, fmt);
    ret = vasprintf (strp, fmt, va);
    va_end (va);

    if (ret < 0) {
	fprintf (stderr, "Out of memory\n");
	exit (1);
    }
#else /* !HAVE_VASNPRINTF */
#define BUF_SIZE 1024
    va_list va;
    char buffer[BUF_SIZE];
    int ret;
    
    va_start (va, fmt);
    ret = vsnprintf (buffer, sizeof(buffer), fmt, va);
    va_end (va);

    if (ret < 0) {
	fprintf (stderr, "Failure in vsnprintf\n");
	exit (1);
    }
    
    if (strlen (buffer) == sizeof(buffer) - 1) {
	fprintf (stderr, "Overflowed fixed buffer\n");
	exit (1);
    }
    
    *strp = strdup (buffer);
    if (!*strp) {
	fprintf (stderr, "Out of memory\n");
	exit (1);
    }
#endif /* !HAVE_VASNPRINTF */
}

static void
xunlink (const char *pathname)
{
    if (unlink (pathname) < 0 && errno != ENOENT) {
	fprintf (stderr, "  Error: Cannot remove %s: %s\n",
		 pathname, strerror (errno));
	exit (1);
    }
}

cairo_test_status_t
cairo_test (cairo_test_t *test, cairo_test_draw_function_t draw)
{
    cairo_t *cr;
    int stride;
    unsigned char *png_buf, *ref_buf, *diff_buf;
    char *log_name, *png_name, *ref_name, *diff_name;
    char *srcdir;
    int pixels_changed;
    int ref_width, ref_height, ref_stride;
    read_png_status_t png_status;
    cairo_test_status_t ret;
    FILE *png_file;
    FILE *log_file;

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
    xasprintf (&log_name, "%s%s", test->name, CAIRO_TEST_LOG_SUFFIX);
    xasprintf (&png_name, "%s%s", test->name, CAIRO_TEST_PNG_SUFFIX);
    xasprintf (&ref_name, "%s/%s%s", srcdir, test->name, CAIRO_TEST_REF_SUFFIX);
    xasprintf (&diff_name, "%s%s", test->name, CAIRO_TEST_DIFF_SUFFIX);

    png_file = fopen (png_name, "w");
    write_png_argb32 (png_buf, png_file, test->width, test->height, stride);
    fclose (png_file);

    xunlink (log_name);

    ref_buf = NULL;
    png_status = (read_png_argb32 (ref_name, &ref_buf, &ref_width, &ref_height, &ref_stride));
    if (png_status) {
	log_file = fopen (log_name, "a");
	switch (png_status)
	{
	case READ_PNG_FILE_NOT_FOUND:
	    fprintf (log_file, "Error: No reference image found: %s\n", ref_name);
	    break;
	case READ_PNG_FILE_NOT_PNG:
	    fprintf (log_file, "Error: %s is not a png image\n", ref_name);
	    break;
	default:
	    fprintf (log_file, "Error: Failed to read %s\n", ref_name);
	}
	fclose (log_file);
		
	ret = CAIRO_TEST_FAILURE;
	goto BAIL;
    } else {
    }

    if (test->width != ref_width || test->height != ref_height) {
	log_file = fopen (log_name, "a");
	fprintf (log_file,
		 "Error: Image size mismatch: (%dx%d) vs. (%dx%d)\n"
		 "       for %s vs %s\n",
		 test->width, test->height,
		 ref_width, ref_height,
		 png_name, ref_name);
	fclose (log_file);

	ret = CAIRO_TEST_FAILURE;
	goto BAIL;
    }

    pixels_changed = buffer_diff (png_buf, ref_buf, diff_buf,
				  test->width, test->height, stride);
    if (pixels_changed) {
	log_file = fopen (log_name, "a");
	fprintf (log_file, "Error: %d pixels differ from reference image %s\n",
		 pixels_changed, ref_name);
	png_file = fopen (diff_name, "w");
	write_png_argb32 (diff_buf, png_file, test->width, test->height, stride);
	fclose (png_file);
	fclose (log_file);

	ret = CAIRO_TEST_FAILURE;
	goto BAIL;
    } else {
	xunlink (diff_name);
    }

    ret = CAIRO_TEST_SUCCESS;

BAIL:
    free (png_buf);
    free (ref_buf);
    free (diff_buf);
    free (log_name);
    free (png_name);
    free (ref_name);
    free (diff_name);

    return ret;
}

cairo_pattern_t *
cairo_test_create_png_pattern (cairo_t *cr, const char *filename)
{
    cairo_surface_t *image;
    cairo_pattern_t *pattern;
    unsigned char *buffer;
    int w, h, stride;
    read_png_status_t status;
    char *srcdir = getenv ("srcdir");

    status = read_png_argb32 (filename, &buffer, &w,&h, &stride);
    if (status != READ_PNG_SUCCESS) {
	if (srcdir) {
	    char *srcdir_filename;
	    xasprintf (&srcdir_filename, "%s/%s", srcdir, filename);
	    status = read_png_argb32 (srcdir_filename, &buffer, &w,&h, &stride);
	    free (srcdir_filename);
	}
    }
    if (status != READ_PNG_SUCCESS)
	return NULL;

    image = cairo_surface_create_for_image (buffer, CAIRO_FORMAT_ARGB32,
					    w, h, stride);

    cairo_surface_set_repeat (image, 1);

    pattern = cairo_pattern_create_for_surface (image);

    return pattern;
}

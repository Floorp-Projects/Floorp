/* cairo - a vector graphics library with display and print output
 *
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
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

/**
 * cairo_debug_reset_static_data:
 *
 * Resets all static data within cairo to its original state,
 * (ie. identical to the state at the time of program invocation). For
 * example, all caches within cairo will be flushed empty.
 *
 * This function is intended to be useful when using memory-checking
 * tools such as valgrind. When valgrind's memcheck analyzes a
 * cairo-using program without a call to cairo_debug_reset_static_data(),
 * it will report all data reachable via cairo's static objects as
 * "still reachable". Calling cairo_debug_reset_static_data() just prior
 * to program termination will make it easier to get squeaky clean
 * reports from valgrind.
 *
 * WARNING: It is only safe to call this function when there are no
 * active cairo objects remaining, (ie. the appropriate destroy
 * functions have been called as necessary). If there are active cairo
 * objects, this call is likely to cause a crash, (eg. an assertion
 * failure due to a hash table being destroyed when non-empty).
 **/
void
cairo_debug_reset_static_data (void)
{
    CAIRO_MUTEX_INITIALIZE ();

    _cairo_scaled_font_map_destroy ();

    _cairo_toy_font_face_reset_static_data ();

#if CAIRO_HAS_FT_FONT
    _cairo_ft_font_reset_static_data ();
#endif

    _cairo_intern_string_reset_static_data ();

    _cairo_scaled_font_reset_static_data ();

    _cairo_pattern_reset_static_data ();

    _cairo_clip_reset_static_data ();

    _cairo_image_reset_static_data ();

#if CAIRO_HAS_DRM_SURFACE
    _cairo_drm_device_reset_static_data ();
#endif

    _cairo_reset_static_data ();

    CAIRO_MUTEX_FINALIZE ();
}

#if HAVE_VALGRIND
void
_cairo_debug_check_image_surface_is_defined (const cairo_surface_t *surface)
{
    const cairo_image_surface_t *image = (cairo_image_surface_t *) surface;
    const uint8_t *bits;
    int row, width;

    if (surface == NULL)
	return;

    if (! RUNNING_ON_VALGRIND)
	return;

    bits = image->data;
    switch (image->format) {
    case CAIRO_FORMAT_A1:
	width = (image->width + 7)/8;
	break;
    case CAIRO_FORMAT_A8:
	width = image->width;
	break;
    case CAIRO_FORMAT_RGB16_565:
	width = image->width*2;
	break;
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
	width = image->width*4;
	break;
    case CAIRO_FORMAT_INVALID:
    default:
	/* XXX compute width from pixman bpp */
	return;
    }

    for (row = 0; row < image->height; row++) {
	VALGRIND_CHECK_MEM_IS_DEFINED (bits, width);
	/* and then silence any future valgrind warnings */
	VALGRIND_MAKE_MEM_DEFINED (bits, width);
	bits += image->stride;
    }
}
#endif


#if 0
void
_cairo_image_surface_write_to_ppm (cairo_image_surface_t *isurf, const char *fn)
{
    char *fmt;
    if (isurf->format == CAIRO_FORMAT_ARGB32 || isurf->format == CAIRO_FORMAT_RGB24)
        fmt = "P6";
    else if (isurf->format == CAIRO_FORMAT_A8)
        fmt = "P5";
    else
        return;

    FILE *fp = fopen(fn, "wb");
    if (!fp)
        return;

    fprintf (fp, "%s %d %d 255\n", fmt,isurf->width, isurf->height);
    for (int j = 0; j < isurf->height; j++) {
        unsigned char *row = isurf->data + isurf->stride * j;
        for (int i = 0; i < isurf->width; i++) {
            if (isurf->format == CAIRO_FORMAT_ARGB32 || isurf->format == CAIRO_FORMAT_RGB24) {
                unsigned char r = *row++;
                unsigned char g = *row++;
                unsigned char b = *row++;
                *row++;
                putc(r, fp);
                putc(g, fp);
                putc(b, fp);
            } else {
                unsigned char a = *row++;
                putc(a, fp);
            }
        }
    }

    fclose (fp);

    fprintf (stderr, "Wrote %s\n", fn);
}
#endif

static cairo_status_t
_print_move_to (void *closure,
		const cairo_point_t *point)
{
    fprintf (closure,
	     " %f %f m",
	     _cairo_fixed_to_double (point->x),
	     _cairo_fixed_to_double (point->y));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_print_line_to (void *closure,
		const cairo_point_t *point)
{
    fprintf (closure,
	     " %f %f l",
	     _cairo_fixed_to_double (point->x),
	     _cairo_fixed_to_double (point->y));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_print_curve_to (void *closure,
		 const cairo_point_t *p1,
		 const cairo_point_t *p2,
		 const cairo_point_t *p3)
{
    fprintf (closure,
	     " %f %f %f %f %f %f c",
	     _cairo_fixed_to_double (p1->x),
	     _cairo_fixed_to_double (p1->y),
	     _cairo_fixed_to_double (p2->x),
	     _cairo_fixed_to_double (p2->y),
	     _cairo_fixed_to_double (p3->x),
	     _cairo_fixed_to_double (p3->y));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_print_close (void *closure)
{
    fprintf (closure, " h");

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_debug_print_path (FILE *stream, cairo_path_fixed_t *path)
{
    cairo_status_t status;

    printf ("path: extents=(%f, %f), (%f, %f)\n",
	    _cairo_fixed_to_double (path->extents.p1.x),
	    _cairo_fixed_to_double (path->extents.p1.y),
	    _cairo_fixed_to_double (path->extents.p2.x),
	    _cairo_fixed_to_double (path->extents.p2.y));

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _print_move_to,
					  _print_line_to,
					  _print_curve_to,
					  _print_close,
					  stream);
    assert (status == CAIRO_STATUS_SUCCESS);

    printf ("\n");
}

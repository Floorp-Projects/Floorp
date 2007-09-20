/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
 * Copyright © 2005 Red Hat, Inc
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
 *	Kristian Høgsberg <krh@redhat.com>
 *	Keith Packard <keithp@keithp.com>
 */

#ifndef CAIRO_PS_SURFACE_PRIVATE_H
#define CAIRO_PS_SURFACE_PRIVATE_H

#include "cairo-ps.h"

#include "cairo-surface-private.h"

typedef struct cairo_ps_surface {
    cairo_surface_t base;

    /* Here final_stream corresponds to the stream/file passed to
     * cairo_ps_surface_create surface is built. Meanwhile stream is a
     * temporary stream in which the file output is built, (so that
     * the header can be built and inserted into the target stream
     * before the contents of the temporary stream are copied). */
    cairo_output_stream_t *final_stream;

    FILE *tmpfile;
    cairo_output_stream_t *stream;

    double width;
    double height;
    double max_width;
    double max_height;

    int num_pages;

    cairo_paginated_mode_t paginated_mode;

    cairo_bool_t force_fallbacks;

    cairo_scaled_font_subsets_t *font_subsets;

    cairo_array_t dsc_header_comments;
    cairo_array_t dsc_setup_comments;
    cairo_array_t dsc_page_setup_comments;

    cairo_array_t *dsc_comment_target;

    cairo_surface_t *paginated_surface;
} cairo_ps_surface_t;

#endif /* CAIRO_PS_SURFACE_PRIVATE_H */

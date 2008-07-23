/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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

#ifndef CAIRO_SCALED_FONT_PRIVATE_H
#define CAIRO_SCALED_FONT_PRIVATE_H

#include "cairo.h"

#include "cairo-types-private.h"
#include "cairo-mutex-type-private.h"
#include "cairo-reference-count-private.h"

struct _cairo_scaled_font {
    /* For most cairo objects, the rule for multiple threads is that
     * the user is responsible for any locking if the same object is
     * manipulated from multiple threads simultaneously.
     *
     * However, with the caching that cairo does for scaled fonts, a
     * user can easily end up with the same cairo_scaled_font object
     * being manipulated from multiple threads without the user ever
     * being aware of this, (and in fact, unable to control it).
     *
     * So, as a special exception, the cairo implementation takes care
     * of all locking needed for cairo_scaled_font_t. Most of what is
     * in the scaled font is immutable, (which is what allows for the
     * sharing in the first place). The things that are modified and
     * the locks protecting them are as follows:
     *
     * 1. The reference count (scaled_font->ref_count)
     *
     *    Modifications to the reference count are protected by the
     *    _cairo_scaled_font_map_mutex. This is because the reference
     *    count of a scaled font is intimately related with the font
     *    map itself, (and the magic holdovers array).
     *
     * 2. The cache of glyphs (scaled_font->glyphs)
     * 3. The backend private data (scaled_font->surface_backend,
     *				    scaled_font->surface_private)
     *
     *    Modifications to these fields are protected with locks on
     *    scaled_font->mutex in the generic scaled_font code.
     */

    /* must be first to be stored in a hash table */
    cairo_hash_entry_t hash_entry;

    /* useful bits for _cairo_scaled_font_nil */
    cairo_status_t status;
    cairo_reference_count_t ref_count;
    cairo_user_data_array_t user_data;

    /* hash key members */
    cairo_font_face_t *font_face; /* may be NULL */
    cairo_matrix_t font_matrix;	  /* font space => user space */
    cairo_matrix_t ctm;	          /* user space => device space */
    cairo_font_options_t options;

    cairo_bool_t placeholder; /*  protected by fontmap mutex */

    cairo_bool_t finished;

    /* "live" scaled_font members */
    cairo_matrix_t scale;	  /* font space => device space */
    cairo_matrix_t scale_inverse; /* device space => font space */
    double max_scale;		  /* maximum x/y expansion of scale */
    cairo_font_extents_t extents; /* user space */

    /* The mutex protects modification to all subsequent fields. */
    cairo_mutex_t mutex;

    cairo_cache_t *glyphs;	  /* glyph index -> cairo_scaled_glyph_t */

    /*
     * One surface backend may store data in each glyph.
     * Whichever surface manages to store its pointer here
     * first gets to store data in each glyph
     */
    const cairo_surface_backend_t *surface_backend;
    void *surface_private;

    /* font backend managing this scaled font */
    const cairo_scaled_font_backend_t *backend;
};

#endif /* CAIRO_SCALED_FONT_PRIVATE_H */

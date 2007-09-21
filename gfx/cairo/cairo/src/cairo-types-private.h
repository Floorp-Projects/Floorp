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

#ifndef CAIRO_TYPES_PRIVATE_H
#define CAIRO_TYPES_PRIVATE_H

/* This is the only header file not including cairoint.h.  It only contains
 * typedefs.*/
#include "cairo.h"

typedef struct _cairo_array cairo_array_t;
typedef struct _cairo_hash_table cairo_hash_table_t;
typedef struct _cairo_cache cairo_cache_t;
typedef struct _cairo_hash_entry cairo_hash_entry_t;
typedef struct _cairo_surface_backend cairo_surface_backend_t;
typedef struct _cairo_clip cairo_clip_t;
typedef struct _cairo_output_stream cairo_output_stream_t;
typedef struct _cairo_scaled_font_subsets cairo_scaled_font_subsets_t;
typedef struct _cairo_paginated_surface_backend cairo_paginated_surface_backend_t;
typedef struct _cairo_scaled_font_backend   cairo_scaled_font_backend_t;
typedef struct _cairo_font_face_backend     cairo_font_face_backend_t;
typedef struct _cairo_xlib_screen_info cairo_xlib_screen_info_t;
typedef enum _cairo_paginated_mode cairo_paginated_mode_t;
typedef cairo_array_t cairo_user_data_array_t;

/**
 * cairo_hash_entry_t:
 *
 * A #cairo_hash_entry_t contains both a key and a value for
 * cairo_hash_table_t. User-derived types for cairo_hash_entry_t must
 * be type-compatible with this structure (eg. they must have an
 * unsigned long as the first parameter. The easiest way to get this
 * is to use:
 *
 * 	typedef _my_entry {
 *	    cairo_hash_entry_t base;
 *	    ... Remainder of key and value fields here ..
 *	} my_entry_t;
 *
 * which then allows a pointer to my_entry_t to be passed to any of
 * the cairo_hash_table functions as follows without requiring a cast:
 *
 *	_cairo_hash_table_insert (hash_table, &my_entry->base);
 *
 * IMPORTANT: The caller is reponsible for initializing
 * my_entry->base.hash with a hash code derived from the key. The
 * essential property of the hash code is that keys_equal must never
 * return TRUE for two keys that have different hashes. The best hash
 * code will reduce the frequency of two keys with the same code for
 * which keys_equal returns FALSE.
 *
 * Which parts of the entry make up the "key" and which part make up
 * the value are entirely up to the caller, (as determined by the
 * computation going into base.hash as well as the keys_equal
 * function). A few of the cairo_hash_table functions accept an entry
 * which will be used exclusively as a "key", (indicated by a
 * parameter name of key). In these cases, the value-related fields of
 * the entry need not be initialized if so desired.
 **/
struct _cairo_hash_entry {
    unsigned long hash;
};

struct _cairo_array {
    unsigned int size;
    unsigned int num_elements;
    unsigned int element_size;
    char **elements;

    cairo_bool_t is_snapshot;
};

struct _cairo_font_options {
    cairo_antialias_t antialias;
    cairo_subpixel_order_t subpixel_order;
    cairo_hint_style_t hint_style;
    cairo_hint_metrics_t hint_metrics;
};

struct _cairo_cache {
    cairo_hash_table_t *hash_table;

    cairo_destroy_func_t entry_destroy;

    unsigned long max_size;
    unsigned long size;

    int freeze_count;
};

enum _cairo_paginated_mode {
    CAIRO_PAGINATED_MODE_ANALYZE,	/* analyze page regions */
    CAIRO_PAGINATED_MODE_RENDER		/* render page contents */
};

/* Sure wish C had a real enum type so that this would be distinct
   from cairo_status_t. Oh well, without that, I'll use this bogus 1000
   offset */
typedef enum _cairo_int_status {
    CAIRO_INT_STATUS_DEGENERATE = 1000,
    CAIRO_INT_STATUS_UNSUPPORTED,
    CAIRO_INT_STATUS_NOTHING_TO_DO,
    CAIRO_INT_STATUS_CACHE_EMPTY,
    CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY,
    CAIRO_INT_STATUS_IMAGE_FALLBACK,
    CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN,
} cairo_int_status_t;

typedef enum _cairo_internal_surface_type {
    CAIRO_INTERNAL_SURFACE_TYPE_META = 0x1000,
    CAIRO_INTERNAL_SURFACE_TYPE_PAGINATED,
    CAIRO_INTERNAL_SURFACE_TYPE_ANALYSIS,
    CAIRO_INTERNAL_SURFACE_TYPE_TEST_META,
    CAIRO_INTERNAL_SURFACE_TYPE_TEST_FALLBACK,
    CAIRO_INTERNAL_SURFACE_TYPE_TEST_PAGINATED
} cairo_internal_surface_type_t;

#endif /* CAIRO_TYPES_PRIVATE_H */

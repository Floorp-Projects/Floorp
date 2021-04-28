/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2008 Chris Wilson
 * Copyright © 2009 Intel Corporation
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
 * Authors:
 *    Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-xcb-private.h"
#include "cairo-list-inline.h"

#include "cairo-fontconfig-private.h"

static void
_cairo_xcb_init_screen_font_options (cairo_xcb_screen_t *screen)
{
    cairo_xcb_resources_t res;
    cairo_antialias_t antialias;
    cairo_subpixel_order_t subpixel_order;
    cairo_lcd_filter_t lcd_filter;
    cairo_hint_style_t hint_style;

    _cairo_xcb_resources_get (screen, &res);

    /* the rest of the code in this function is copied from
       _cairo_xlib_init_screen_font_options in cairo-xlib-screen.c */

    if (res.xft_hinting) {
	switch (res.xft_hintstyle) {
	case FC_HINT_NONE:
	    hint_style = CAIRO_HINT_STYLE_NONE;
	    break;
	case FC_HINT_SLIGHT:
	    hint_style = CAIRO_HINT_STYLE_SLIGHT;
	    break;
	case FC_HINT_MEDIUM:
	    hint_style = CAIRO_HINT_STYLE_MEDIUM;
	    break;
	case FC_HINT_FULL:
	    hint_style = CAIRO_HINT_STYLE_FULL;
	    break;
	default:
	    hint_style = CAIRO_HINT_STYLE_DEFAULT;
	}
    } else {
	hint_style = CAIRO_HINT_STYLE_NONE;
    }

    switch (res.xft_rgba) {
    case FC_RGBA_RGB:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_RGB;
	break;
    case FC_RGBA_BGR:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_BGR;
	break;
    case FC_RGBA_VRGB:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_VRGB;
	break;
    case FC_RGBA_VBGR:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_VBGR;
	break;
    case FC_RGBA_UNKNOWN:
    case FC_RGBA_NONE:
    default:
	subpixel_order = CAIRO_SUBPIXEL_ORDER_DEFAULT;
    }

    switch (res.xft_lcdfilter) {
    case FC_LCD_NONE:
	lcd_filter = CAIRO_LCD_FILTER_NONE;
	break;
    case FC_LCD_DEFAULT:
	lcd_filter = CAIRO_LCD_FILTER_FIR5;
	break;
    case FC_LCD_LIGHT:
	lcd_filter = CAIRO_LCD_FILTER_FIR3;
	break;
    case FC_LCD_LEGACY:
	lcd_filter = CAIRO_LCD_FILTER_INTRA_PIXEL;
	break;
    default:
	lcd_filter = CAIRO_LCD_FILTER_DEFAULT;
	break;
    }

    if (res.xft_antialias) {
	if (subpixel_order == CAIRO_SUBPIXEL_ORDER_DEFAULT)
	    antialias = CAIRO_ANTIALIAS_GRAY;
	else
	    antialias = CAIRO_ANTIALIAS_SUBPIXEL;
    } else {
	antialias = CAIRO_ANTIALIAS_NONE;
    }

    cairo_font_options_set_hint_style (&screen->font_options, hint_style);
    cairo_font_options_set_antialias (&screen->font_options, antialias);
    cairo_font_options_set_subpixel_order (&screen->font_options, subpixel_order);
    cairo_font_options_set_lcd_filter (&screen->font_options, lcd_filter);
    cairo_font_options_set_hint_metrics (&screen->font_options, CAIRO_HINT_METRICS_ON);
}

struct pattern_cache_entry {
    cairo_cache_entry_t key;
    cairo_xcb_screen_t *screen;
    cairo_pattern_union_t pattern;
    cairo_surface_t *picture;
};

void
_cairo_xcb_screen_finish (cairo_xcb_screen_t *screen)
{
    int i;

    CAIRO_MUTEX_LOCK (screen->connection->screens_mutex);
    cairo_list_del (&screen->link);
    CAIRO_MUTEX_UNLOCK (screen->connection->screens_mutex);

    while (! cairo_list_is_empty (&screen->surfaces)) {
	cairo_surface_t *surface;

	surface = &cairo_list_first_entry (&screen->surfaces,
					   cairo_xcb_surface_t,
					   link)->base;

	cairo_surface_finish (surface);
    }

    while (! cairo_list_is_empty (&screen->pictures)) {
	cairo_surface_t *surface;

	surface = &cairo_list_first_entry (&screen->pictures,
					   cairo_xcb_picture_t,
					   link)->base;

	cairo_surface_finish (surface);
    }

    for (i = 0; i < screen->solid_cache_size; i++)
	cairo_surface_destroy (screen->solid_cache[i].picture);

    for (i = 0; i < ARRAY_LENGTH (screen->stock_colors); i++)
	cairo_surface_destroy (screen->stock_colors[i]);

    for (i = 0; i < ARRAY_LENGTH (screen->gc); i++) {
	if (screen->gc_depths[i] != 0)
	    _cairo_xcb_connection_free_gc (screen->connection, screen->gc[i]);
    }

    _cairo_cache_fini (&screen->linear_pattern_cache);
    _cairo_cache_fini (&screen->radial_pattern_cache);
    _cairo_freelist_fini (&screen->pattern_cache_entry_freelist);

    free (screen);
}

static cairo_bool_t
_linear_pattern_cache_entry_equal (const void *A, const void *B)
{
    const struct pattern_cache_entry *a = A, *b = B;

    return _cairo_linear_pattern_equal (&a->pattern.gradient.linear,
					&b->pattern.gradient.linear);
}

static cairo_bool_t
_radial_pattern_cache_entry_equal (const void *A, const void *B)
{
    const struct pattern_cache_entry *a = A, *b = B;

    return _cairo_radial_pattern_equal (&a->pattern.gradient.radial,
					&b->pattern.gradient.radial);
}

static void
_pattern_cache_entry_destroy (void *closure)
{
    struct pattern_cache_entry *entry = closure;

    _cairo_pattern_fini (&entry->pattern.base);
    cairo_surface_destroy (entry->picture);
    _cairo_freelist_free (&entry->screen->pattern_cache_entry_freelist, entry);
}

static int _get_screen_index(cairo_xcb_connection_t *xcb_connection,
			     xcb_screen_t *xcb_screen)
{
    int idx = 0;
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_connection->root);
    for (; iter.rem; xcb_screen_next(&iter), idx++)
	if (iter.data->root == xcb_screen->root)
	    return idx;

    ASSERT_NOT_REACHED;
}

cairo_xcb_screen_t *
_cairo_xcb_screen_get (xcb_connection_t *xcb_connection,
		       xcb_screen_t *xcb_screen)
{
    cairo_xcb_connection_t *connection;
    cairo_xcb_screen_t *screen;
    cairo_status_t status;
    int screen_idx;
    int i;

    connection = _cairo_xcb_connection_get (xcb_connection);
    if (unlikely (connection == NULL))
	return NULL;

    CAIRO_MUTEX_LOCK (connection->screens_mutex);

    cairo_list_foreach_entry (screen,
			      cairo_xcb_screen_t,
			      &connection->screens,
			      link)
    {
	if (screen->xcb_screen == xcb_screen) {
	    /* Maintain list in MRU order */
	    if (&screen->link != connection->screens.next)
		cairo_list_move (&screen->link, &connection->screens);

	    goto unlock;
	}
    }

    screen = _cairo_malloc (sizeof (cairo_xcb_screen_t));
    if (unlikely (screen == NULL))
	goto unlock;

    screen_idx = _get_screen_index(connection, xcb_screen);

    screen->connection = connection;
    screen->xcb_screen = xcb_screen;
    screen->has_font_options = FALSE;
    screen->subpixel_order = connection->subpixel_orders[screen_idx];

    _cairo_freelist_init (&screen->pattern_cache_entry_freelist,
			  sizeof (struct pattern_cache_entry));
    cairo_list_init (&screen->link);
    cairo_list_init (&screen->surfaces);
    cairo_list_init (&screen->pictures);

    memset (screen->gc_depths, 0, sizeof (screen->gc_depths));
    memset (screen->gc, 0, sizeof (screen->gc));

    screen->solid_cache_size = 0;
    for (i = 0; i < ARRAY_LENGTH (screen->stock_colors); i++)
	screen->stock_colors[i] = NULL;

    status = _cairo_cache_init (&screen->linear_pattern_cache,
				_linear_pattern_cache_entry_equal,
				NULL,
				_pattern_cache_entry_destroy,
				16);
    if (unlikely (status))
	goto error_screen;

    status = _cairo_cache_init (&screen->radial_pattern_cache,
				_radial_pattern_cache_entry_equal,
				NULL,
				_pattern_cache_entry_destroy,
				4);
    if (unlikely (status))
	goto error_linear;

    cairo_list_add (&screen->link, &connection->screens);

unlock:
    CAIRO_MUTEX_UNLOCK (connection->screens_mutex);

    return screen;

error_linear:
    _cairo_cache_fini (&screen->linear_pattern_cache);
error_screen:
    CAIRO_MUTEX_UNLOCK (connection->screens_mutex);
    free (screen);

    return NULL;
}

static xcb_gcontext_t
_create_gc (cairo_xcb_screen_t *screen,
	    xcb_drawable_t drawable)
{
    uint32_t values[] = { 0 };

    return _cairo_xcb_connection_create_gc (screen->connection, drawable,
					    XCB_GC_GRAPHICS_EXPOSURES,
					    values);
}

xcb_gcontext_t
_cairo_xcb_screen_get_gc (cairo_xcb_screen_t *screen,
			  xcb_drawable_t drawable,
			  int depth)
{
    int i;

    assert (CAIRO_MUTEX_IS_LOCKED (screen->connection->device.mutex));

    for (i = 0; i < ARRAY_LENGTH (screen->gc); i++) {
	if (screen->gc_depths[i] == depth) {
	    screen->gc_depths[i] = 0;
	    return screen->gc[i];
	}
    }

    return _create_gc (screen, drawable);
}

void
_cairo_xcb_screen_put_gc (cairo_xcb_screen_t *screen, int depth, xcb_gcontext_t gc)
{
    int i;

    assert (CAIRO_MUTEX_IS_LOCKED (screen->connection->device.mutex));

    for (i = 0; i < ARRAY_LENGTH (screen->gc); i++) {
	if (screen->gc_depths[i] == 0)
	    break;
    }

    if (i == ARRAY_LENGTH (screen->gc)) {
	/* perform random substitution to ensure fair caching over depths */
	i = rand () % ARRAY_LENGTH (screen->gc);
	_cairo_xcb_connection_free_gc (screen->connection, screen->gc[i]);
    }

    screen->gc[i] = gc;
    screen->gc_depths[i] = depth;
}

cairo_status_t
_cairo_xcb_screen_store_linear_picture (cairo_xcb_screen_t *screen,
					const cairo_linear_pattern_t *linear,
					cairo_surface_t *picture)
{
    struct pattern_cache_entry *entry;
    cairo_status_t status;

    assert (CAIRO_MUTEX_IS_LOCKED (screen->connection->device.mutex));

    entry = _cairo_freelist_alloc (&screen->pattern_cache_entry_freelist);
    if (unlikely (entry == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    entry->key.hash = _cairo_linear_pattern_hash (_CAIRO_HASH_INIT_VALUE, linear);
    entry->key.size = 1;

    status = _cairo_pattern_init_copy (&entry->pattern.base, &linear->base.base);
    if (unlikely (status)) {
	_cairo_freelist_free (&screen->pattern_cache_entry_freelist, entry);
	return status;
    }

    entry->picture = cairo_surface_reference (picture);
    entry->screen = screen;

    status = _cairo_cache_insert (&screen->linear_pattern_cache,
				  &entry->key);
    if (unlikely (status)) {
	cairo_surface_destroy (picture);
	_cairo_freelist_free (&screen->pattern_cache_entry_freelist, entry);
	return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_surface_t *
_cairo_xcb_screen_lookup_linear_picture (cairo_xcb_screen_t *screen,
					 const cairo_linear_pattern_t *linear)
{
    cairo_surface_t *picture = NULL;
    struct pattern_cache_entry tmpl;
    struct pattern_cache_entry *entry;

    assert (CAIRO_MUTEX_IS_LOCKED (screen->connection->device.mutex));

    tmpl.key.hash = _cairo_linear_pattern_hash (_CAIRO_HASH_INIT_VALUE, linear);
    _cairo_pattern_init_static_copy (&tmpl.pattern.base, &linear->base.base);

    entry = _cairo_cache_lookup (&screen->linear_pattern_cache, &tmpl.key);
    if (entry != NULL)
	picture = cairo_surface_reference (entry->picture);

    return picture;
}

cairo_status_t
_cairo_xcb_screen_store_radial_picture (cairo_xcb_screen_t *screen,
					const cairo_radial_pattern_t *radial,
					cairo_surface_t *picture)
{
    struct pattern_cache_entry *entry;
    cairo_status_t status;

    assert (CAIRO_MUTEX_IS_LOCKED (screen->connection->device.mutex));

    entry = _cairo_freelist_alloc (&screen->pattern_cache_entry_freelist);
    if (unlikely (entry == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    entry->key.hash = _cairo_radial_pattern_hash (_CAIRO_HASH_INIT_VALUE, radial);
    entry->key.size = 1;

    status = _cairo_pattern_init_copy (&entry->pattern.base, &radial->base.base);
    if (unlikely (status)) {
	_cairo_freelist_free (&screen->pattern_cache_entry_freelist, entry);
	return status;
    }

    entry->picture = cairo_surface_reference (picture);
    entry->screen = screen;

    status = _cairo_cache_insert (&screen->radial_pattern_cache, &entry->key);
    if (unlikely (status)) {
	cairo_surface_destroy (picture);
	_cairo_freelist_free (&screen->pattern_cache_entry_freelist, entry);
	return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_surface_t *
_cairo_xcb_screen_lookup_radial_picture (cairo_xcb_screen_t *screen,
					 const cairo_radial_pattern_t *radial)
{
    cairo_surface_t *picture = NULL;
    struct pattern_cache_entry tmpl;
    struct pattern_cache_entry *entry;

    assert (CAIRO_MUTEX_IS_LOCKED (screen->connection->device.mutex));

    tmpl.key.hash = _cairo_radial_pattern_hash (_CAIRO_HASH_INIT_VALUE, radial);
    _cairo_pattern_init_static_copy (&tmpl.pattern.base, &radial->base.base);

    entry = _cairo_cache_lookup (&screen->radial_pattern_cache, &tmpl.key);
    if (entry != NULL)
	picture = cairo_surface_reference (entry->picture);

    return picture;
}

cairo_font_options_t *
_cairo_xcb_screen_get_font_options (cairo_xcb_screen_t *screen)
{
    if (! screen->has_font_options) {
	_cairo_font_options_init_default (&screen->font_options);
	_cairo_font_options_set_round_glyph_positions (&screen->font_options, CAIRO_ROUND_GLYPH_POS_ON);

	/* XXX: This is disabled because something seems to be merging
	   font options incorrectly for xcb.  This effectively reverts
	   the changes brought in git e691d242, and restores ~150 tests
	   to resume passing.  See mailing list archives for Sep 17,
	   2014 for more discussion. */
	if (0 && ! _cairo_xcb_connection_acquire (screen->connection)) {
	    _cairo_xcb_init_screen_font_options (screen);
	    _cairo_xcb_connection_release (screen->connection);
	}

	screen->has_font_options = TRUE;
    }

    return &screen->font_options;
}

/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
 * Copyright © 2005 Red Hat, Inc
 * Copyright © 2006 Keith Packard
 * Copyright © 2006 Red Hat, Inc
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

#include "cairoint.h"
#include "cairo-scaled-font-subsets-private.h"

struct _cairo_scaled_font_subsets {
    int max_glyphs_per_subset_limit;
    int max_glyphs_per_subset_used;
    int num_sub_fonts;

    cairo_hash_table_t *sub_fonts;
};

typedef struct _cairo_sub_font {
    cairo_hash_entry_t base;

    cairo_scaled_font_subsets_t *parent;
    cairo_scaled_font_t *scaled_font;
    unsigned int font_id;

    int current_subset;
    int num_glyphs_in_current_subset;
    int max_glyphs_per_subset;

    cairo_hash_table_t *sub_font_glyphs;
} cairo_sub_font_t;

typedef struct _cairo_sub_font_glyph {
    cairo_hash_entry_t base;

    unsigned int subset_id;
    unsigned int subset_glyph_index;
} cairo_sub_font_glyph_t;

typedef struct _cairo_sub_font_collection {
    unsigned long *glyphs; /* scaled_font_glyph_index */
    unsigned int glyphs_size;
    unsigned int max_glyph;
    unsigned int num_glyphs;

    unsigned int subset_id;

    cairo_scaled_font_subset_callback_func_t font_subset_callback;
    void *font_subset_callback_closure;
} cairo_sub_font_collection_t;

static void
_cairo_sub_font_glyph_init_key (cairo_sub_font_glyph_t  *sub_font_glyph,
				unsigned long		 scaled_font_glyph_index)
{
    sub_font_glyph->base.hash = scaled_font_glyph_index;
}

static cairo_bool_t
_cairo_sub_font_glyphs_equal (const void *key_a, const void *key_b)
{
    const cairo_sub_font_glyph_t *sub_font_glyph_a = key_a;
    const cairo_sub_font_glyph_t *sub_font_glyph_b = key_b;

    return sub_font_glyph_a->base.hash == sub_font_glyph_b->base.hash;
}

static cairo_sub_font_glyph_t *
_cairo_sub_font_glyph_create (unsigned long	scaled_font_glyph_index,
			      unsigned int	subset_id,
			      unsigned int	subset_glyph_index)
{
    cairo_sub_font_glyph_t *sub_font_glyph;

    sub_font_glyph = malloc (sizeof (cairo_sub_font_glyph_t));
    if (sub_font_glyph == NULL)
	return NULL;

    _cairo_sub_font_glyph_init_key (sub_font_glyph, scaled_font_glyph_index);
    sub_font_glyph->subset_id = subset_id;
    sub_font_glyph->subset_glyph_index = subset_glyph_index;

    return sub_font_glyph;
}

static void
_cairo_sub_font_glyph_destroy (cairo_sub_font_glyph_t *sub_font_glyph)
{
    free (sub_font_glyph);
}

static void
_cairo_sub_font_glyph_pluck (void *entry, void *closure)
{
    cairo_sub_font_glyph_t *sub_font_glyph = entry;
    cairo_hash_table_t *sub_font_glyphs = closure;

    _cairo_hash_table_remove (sub_font_glyphs, &sub_font_glyph->base);
    _cairo_sub_font_glyph_destroy (sub_font_glyph);
}

static void
_cairo_sub_font_glyph_collect (void *entry, void *closure)
{
    cairo_sub_font_glyph_t *sub_font_glyph = entry;
    cairo_sub_font_collection_t *collection = closure;
    unsigned long scaled_font_glyph_index;
    unsigned int subset_glyph_index;

    if (sub_font_glyph->subset_id != collection->subset_id)
	return;

    scaled_font_glyph_index = sub_font_glyph->base.hash;
    subset_glyph_index = sub_font_glyph->subset_glyph_index;

    /* Ensure we don't exceed the allocated bounds. */
    assert (subset_glyph_index < collection->glyphs_size);

    collection->glyphs[subset_glyph_index] = scaled_font_glyph_index;
    if (subset_glyph_index > collection->max_glyph)
	collection->max_glyph = subset_glyph_index;

    collection->num_glyphs++;
}

static cairo_bool_t
_cairo_sub_fonts_equal (const void *key_a, const void *key_b)
{
    const cairo_sub_font_t *sub_font_a = key_a;
    const cairo_sub_font_t *sub_font_b = key_b;

    return sub_font_a->scaled_font == sub_font_b->scaled_font;
}

static void
_cairo_sub_font_init_key (cairo_sub_font_t	*sub_font,
			  cairo_scaled_font_t	*scaled_font)
{
    sub_font->base.hash = (unsigned long) scaled_font;
    sub_font->scaled_font = scaled_font;
}

static cairo_sub_font_t *
_cairo_sub_font_create (cairo_scaled_font_subsets_t	*parent,
			cairo_scaled_font_t		*scaled_font,
			unsigned int			 font_id,
			int				 max_glyphs_per_subset)
{
    cairo_sub_font_t *sub_font;

    sub_font = malloc (sizeof (cairo_sub_font_t));
    if (sub_font == NULL)
	return NULL;

    _cairo_sub_font_init_key (sub_font, scaled_font);

    sub_font->parent = parent;
    sub_font->scaled_font = cairo_scaled_font_reference (scaled_font);
    sub_font->font_id = font_id;

    sub_font->current_subset = 0;
    sub_font->num_glyphs_in_current_subset = 0;
    sub_font->max_glyphs_per_subset = max_glyphs_per_subset;

    sub_font->sub_font_glyphs = _cairo_hash_table_create (_cairo_sub_font_glyphs_equal);
    if (! sub_font->sub_font_glyphs) {
	free (sub_font);
	return NULL;
    }

    return sub_font;
}

static void
_cairo_sub_font_destroy (cairo_sub_font_t *sub_font)
{
    _cairo_hash_table_foreach (sub_font->sub_font_glyphs,
			       _cairo_sub_font_glyph_pluck,
			       sub_font->sub_font_glyphs);
    _cairo_hash_table_destroy (sub_font->sub_font_glyphs);
    cairo_scaled_font_destroy (sub_font->scaled_font);
    free (sub_font);
}

static void
_cairo_sub_font_pluck (void *entry, void *closure)
{
    cairo_sub_font_t *sub_font = entry;
    cairo_hash_table_t *sub_fonts = closure;

    _cairo_hash_table_remove (sub_fonts, &sub_font->base);
    _cairo_sub_font_destroy (sub_font);
}

static cairo_status_t
_cairo_sub_font_map_glyph (cairo_sub_font_t	*sub_font,
			   unsigned long	 scaled_font_glyph_index,
			   unsigned int		*subset_id,
			   unsigned int		*subset_glyph_index)
{
    cairo_sub_font_glyph_t key, *sub_font_glyph;
    cairo_status_t status;

    _cairo_sub_font_glyph_init_key (&key, scaled_font_glyph_index);
    if (! _cairo_hash_table_lookup (sub_font->sub_font_glyphs, &key.base,
				    (cairo_hash_entry_t **) &sub_font_glyph))
    {
	if (sub_font->max_glyphs_per_subset &&
	    sub_font->num_glyphs_in_current_subset == sub_font->max_glyphs_per_subset)
	{
	    sub_font->current_subset++;
	    sub_font->num_glyphs_in_current_subset = 0;
	}

	sub_font_glyph = _cairo_sub_font_glyph_create (scaled_font_glyph_index,
						       sub_font->current_subset,
						       sub_font->num_glyphs_in_current_subset++);
	if (sub_font_glyph == NULL)
	    return CAIRO_STATUS_NO_MEMORY;

	if (sub_font->num_glyphs_in_current_subset > sub_font->parent->max_glyphs_per_subset_used)
	    sub_font->parent->max_glyphs_per_subset_used = sub_font->num_glyphs_in_current_subset;

	status = _cairo_hash_table_insert (sub_font->sub_font_glyphs, &sub_font_glyph->base);
	if (status)
	    return status;
    }

    *subset_id = sub_font_glyph->subset_id;
    *subset_glyph_index = sub_font_glyph->subset_glyph_index;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_sub_font_collect (void *entry, void *closure)
{
    cairo_sub_font_t *sub_font = entry;
    cairo_sub_font_collection_t *collection = closure;
    cairo_scaled_font_subset_t subset;
    int i;

    for (i = 0; i <= sub_font->current_subset; i++) {
	collection->subset_id = i;

	collection->num_glyphs = 0;
	collection->max_glyph = 0;

	_cairo_hash_table_foreach (sub_font->sub_font_glyphs,
				   _cairo_sub_font_glyph_collect, collection);

	/* Ensure the resulting array has no uninitialized holes */
	assert (collection->num_glyphs == collection->max_glyph + 1);

	subset.scaled_font = sub_font->scaled_font;
	subset.font_id = sub_font->font_id;
	subset.subset_id = i;
	subset.glyphs = collection->glyphs;
	subset.num_glyphs = collection->num_glyphs;

	(collection->font_subset_callback) (&subset,
					    collection->font_subset_callback_closure);
    }
}

cairo_scaled_font_subsets_t *
_cairo_scaled_font_subsets_create (int max_glyphs_per_subset)
{
    cairo_scaled_font_subsets_t *subsets;

    subsets = malloc (sizeof (cairo_scaled_font_subsets_t));
    if (subsets == NULL)
	return NULL;

    subsets->max_glyphs_per_subset_limit = max_glyphs_per_subset;
    subsets->max_glyphs_per_subset_used = 0;
    subsets->num_sub_fonts = 0;

    subsets->sub_fonts = _cairo_hash_table_create (_cairo_sub_fonts_equal);
    if (! subsets->sub_fonts) {
	free (subsets);
	return NULL;
    }

    return subsets;
}

void
_cairo_scaled_font_subsets_destroy (cairo_scaled_font_subsets_t *subsets)
{
    _cairo_hash_table_foreach (subsets->sub_fonts, _cairo_sub_font_pluck, subsets->sub_fonts);
    _cairo_hash_table_destroy (subsets->sub_fonts);
    free (subsets);
}

cairo_private cairo_status_t
_cairo_scaled_font_subsets_map_glyph (cairo_scaled_font_subsets_t	*subsets,
				      cairo_scaled_font_t		*scaled_font,
				      unsigned long			 scaled_font_glyph_index,
				      unsigned int			*font_id,
				      unsigned int			*subset_id,
				      unsigned int			*subset_glyph_index)
{
    cairo_sub_font_t key, *sub_font;
    cairo_status_t status;

    _cairo_sub_font_init_key (&key, scaled_font);
    if (! _cairo_hash_table_lookup (subsets->sub_fonts, &key.base,
				    (cairo_hash_entry_t **) &sub_font))
    {
	sub_font = _cairo_sub_font_create (subsets, scaled_font,
					   subsets->num_sub_fonts++,
					   subsets->max_glyphs_per_subset_limit);
	if (sub_font == NULL)
	    return CAIRO_STATUS_NO_MEMORY;

	status = _cairo_hash_table_insert (subsets->sub_fonts,
					   &sub_font->base);
	if (status)
	    return status;
    }

    *font_id = sub_font->font_id;

    return _cairo_sub_font_map_glyph (sub_font, scaled_font_glyph_index,
						  subset_id, subset_glyph_index);
}

cairo_private cairo_status_t
_cairo_scaled_font_subsets_foreach (cairo_scaled_font_subsets_t			*font_subsets,
				    cairo_scaled_font_subset_callback_func_t	 font_subset_callback,
				    void					*closure)
{
    cairo_sub_font_collection_t collection;

    collection.glyphs_size = font_subsets->max_glyphs_per_subset_used;
    collection.glyphs = malloc (collection.glyphs_size * sizeof(unsigned long));
    if (collection.glyphs == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    collection.font_subset_callback = font_subset_callback;
    collection.font_subset_callback_closure = closure;

    _cairo_hash_table_foreach (font_subsets->sub_fonts,
			       _cairo_sub_font_collect, &collection);

    free (collection.glyphs);

    return CAIRO_STATUS_SUCCESS;
}

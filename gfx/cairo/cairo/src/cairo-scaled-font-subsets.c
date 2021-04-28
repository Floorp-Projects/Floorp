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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Kristian Høgsberg <krh@redhat.com>
 *	Keith Packard <keithp@keithp.com>
 *	Adrian Johnson <ajohnson@redneon.com>
 */

#define _BSD_SOURCE /* for snprintf(), strdup() */
#include "cairoint.h"
#include "cairo-error-private.h"

#if CAIRO_HAS_FONT_SUBSET

#include "cairo-scaled-font-subsets-private.h"
#include "cairo-user-font-private.h"

#define MAX_GLYPHS_PER_SIMPLE_FONT 256
#define MAX_GLYPHS_PER_COMPOSITE_FONT 65536

typedef enum {
    CAIRO_SUBSETS_SCALED,
    CAIRO_SUBSETS_SIMPLE,
    CAIRO_SUBSETS_COMPOSITE
} cairo_subsets_type_t;

typedef enum {
    CAIRO_SUBSETS_FOREACH_UNSCALED,
    CAIRO_SUBSETS_FOREACH_SCALED,
    CAIRO_SUBSETS_FOREACH_USER
} cairo_subsets_foreach_type_t;

typedef struct _cairo_sub_font {
    cairo_hash_entry_t base;

    cairo_bool_t is_scaled;
    cairo_bool_t is_composite;
    cairo_bool_t is_user;
    cairo_scaled_font_subsets_t *parent;
    cairo_scaled_font_t *scaled_font;
    unsigned int font_id;

    int current_subset;
    int num_glyphs_in_current_subset;
    int max_glyphs_per_subset;

    cairo_hash_table_t *sub_font_glyphs;
    struct _cairo_sub_font *next;
} cairo_sub_font_t;

struct _cairo_scaled_font_subsets {
    cairo_subsets_type_t type;

    int max_glyphs_per_unscaled_subset_used;
    cairo_hash_table_t *unscaled_sub_fonts;
    cairo_sub_font_t *unscaled_sub_fonts_list;
    cairo_sub_font_t *unscaled_sub_fonts_list_end;

    int max_glyphs_per_scaled_subset_used;
    cairo_hash_table_t *scaled_sub_fonts;
    cairo_sub_font_t *scaled_sub_fonts_list;
    cairo_sub_font_t *scaled_sub_fonts_list_end;

    int num_sub_fonts;
};

typedef struct _cairo_sub_font_glyph {
    cairo_hash_entry_t base;

    unsigned int subset_id;
    unsigned int subset_glyph_index;
    double       x_advance;
    double       y_advance;

    cairo_bool_t is_mapped;
    uint32_t     unicode;
    char  	*utf8;
    int          utf8_len;
} cairo_sub_font_glyph_t;

typedef struct _cairo_sub_font_collection {
    unsigned long *glyphs; /* scaled_font_glyph_index */
    char       **utf8;
    unsigned int glyphs_size;
    unsigned int max_glyph;
    unsigned int num_glyphs;

    unsigned int subset_id;

    cairo_status_t status;
    cairo_scaled_font_subset_callback_func_t font_subset_callback;
    void *font_subset_callback_closure;
} cairo_sub_font_collection_t;

typedef struct _cairo_string_entry {
    cairo_hash_entry_t base;
    char *string;
} cairo_string_entry_t;

static cairo_status_t
_cairo_sub_font_map_glyph (cairo_sub_font_t	*sub_font,
			   unsigned long	 scaled_font_glyph_index,
			   const char *		 utf8,
			   int			 utf8_len,
                           cairo_scaled_font_subsets_glyph_t *subset_glyph);

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
			      unsigned int	subset_glyph_index,
                              double            x_advance,
                              double            y_advance)
{
    cairo_sub_font_glyph_t *sub_font_glyph;

    sub_font_glyph = malloc (sizeof (cairo_sub_font_glyph_t));
    if (unlikely (sub_font_glyph == NULL)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    _cairo_sub_font_glyph_init_key (sub_font_glyph, scaled_font_glyph_index);
    sub_font_glyph->subset_id = subset_id;
    sub_font_glyph->subset_glyph_index = subset_glyph_index;
    sub_font_glyph->x_advance = x_advance;
    sub_font_glyph->y_advance = y_advance;
    sub_font_glyph->is_mapped = FALSE;
    sub_font_glyph->unicode = -1;
    sub_font_glyph->utf8 = NULL;
    sub_font_glyph->utf8_len = 0;

    return sub_font_glyph;
}

static void
_cairo_sub_font_glyph_destroy (cairo_sub_font_glyph_t *sub_font_glyph)
{
    if (sub_font_glyph->utf8 != NULL)
	free (sub_font_glyph->utf8);

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
    collection->utf8[subset_glyph_index] = sub_font_glyph->utf8;
    if (subset_glyph_index > collection->max_glyph)
	collection->max_glyph = subset_glyph_index;

    collection->num_glyphs++;
}

static cairo_bool_t
_cairo_sub_fonts_equal (const void *key_a, const void *key_b)
{
    const cairo_sub_font_t *sub_font_a = key_a;
    const cairo_sub_font_t *sub_font_b = key_b;
    cairo_scaled_font_t *a = sub_font_a->scaled_font;
    cairo_scaled_font_t *b = sub_font_b->scaled_font;

    if (sub_font_a->is_scaled)
        return a == b;
    else
	return a->font_face == b->font_face || a->original_font_face == b->original_font_face;
}

static void
_cairo_sub_font_init_key (cairo_sub_font_t	*sub_font,
			  cairo_scaled_font_t	*scaled_font)
{
    if (sub_font->is_scaled)
    {
        sub_font->base.hash = (unsigned long) scaled_font;
        sub_font->scaled_font = scaled_font;
    }
    else
    {
        sub_font->base.hash = (unsigned long) scaled_font->font_face;
        sub_font->scaled_font = scaled_font;
    }
}

static cairo_status_t
_cairo_sub_font_create (cairo_scaled_font_subsets_t	*parent,
			cairo_scaled_font_t		*scaled_font,
			unsigned int			 font_id,
			int				 max_glyphs_per_subset,
                        cairo_bool_t                     is_scaled,
			cairo_bool_t                     is_composite,
			cairo_sub_font_t               **sub_font_out)
{
    cairo_sub_font_t *sub_font;
    cairo_status_t status;
    cairo_scaled_font_subsets_glyph_t subset_glyph;

    sub_font = malloc (sizeof (cairo_sub_font_t));
    if (unlikely (sub_font == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    sub_font->is_scaled = is_scaled;
    sub_font->is_composite = is_composite;
    sub_font->is_user = _cairo_font_face_is_user (scaled_font->font_face);
    _cairo_sub_font_init_key (sub_font, scaled_font);

    sub_font->parent = parent;
    sub_font->scaled_font = scaled_font;
    sub_font->font_id = font_id;

    sub_font->current_subset = 0;
    sub_font->num_glyphs_in_current_subset = 0;
    sub_font->max_glyphs_per_subset = max_glyphs_per_subset;

    sub_font->sub_font_glyphs = _cairo_hash_table_create (_cairo_sub_font_glyphs_equal);
    if (unlikely (sub_font->sub_font_glyphs == NULL)) {
	free (sub_font);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }
    sub_font->next = NULL;

    /* Reserve first glyph in subset for the .notdef glyph except for
     * Type 3 fonts */
    if (! is_scaled) {
	status = _cairo_sub_font_map_glyph (sub_font, 0, NULL, -1, &subset_glyph);
	if (unlikely (status)) {
	    _cairo_hash_table_destroy (sub_font->sub_font_glyphs);
	    free (sub_font);
	    return status;
	}
    }

    *sub_font_out = sub_font;
    return CAIRO_STATUS_SUCCESS;
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
_cairo_sub_font_glyph_lookup_unicode (cairo_sub_font_glyph_t *sub_font_glyph,
				      cairo_scaled_font_t    *scaled_font,
				      unsigned long	      scaled_font_glyph_index)
{
    uint32_t unicode;
    char buf[8];
    int len;
    cairo_status_t status;

    /* Do a reverse lookup on the glyph index. unicode is -1 if the
     * index could not be mapped to a unicode character. */
    unicode = -1;
    status = _cairo_truetype_index_to_ucs4 (scaled_font,
					    scaled_font_glyph_index,
					    &unicode);
    if (_cairo_status_is_error (status))
	return status;

    if (unicode == (uint32_t)-1 && scaled_font->backend->index_to_ucs4) {
	status = scaled_font->backend->index_to_ucs4 (scaled_font,
						      scaled_font_glyph_index,
						      &unicode);
	if (unlikely (status))
	    return status;
    }

    sub_font_glyph->unicode = unicode;
    sub_font_glyph->utf8 = NULL;
    sub_font_glyph->utf8_len = 0;
    if (unicode != (uint32_t) -1) {
	len = _cairo_ucs4_to_utf8 (unicode, buf);
	if (len > 0) {
	    sub_font_glyph->utf8 = malloc (len + 1);
	    if (unlikely (sub_font_glyph->utf8 == NULL))
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	    memcpy (sub_font_glyph->utf8, buf, len);
	    sub_font_glyph->utf8[len] = 0;
	    sub_font_glyph->utf8_len = len;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_sub_font_glyph_map_to_unicode (cairo_sub_font_glyph_t *sub_font_glyph,
				      const char	     *utf8,
				      int		      utf8_len,
				      cairo_bool_t	     *is_mapped)
{
    *is_mapped = FALSE;

    if (utf8_len < 0)
	return CAIRO_STATUS_SUCCESS;

    if (utf8 != NULL && utf8_len != 0 && utf8[utf8_len - 1] == '\0')
	utf8_len--;

    if (utf8 != NULL && utf8_len != 0) {
	if (sub_font_glyph->utf8 != NULL) {
	    if (utf8_len == sub_font_glyph->utf8_len &&
		memcmp (utf8, sub_font_glyph->utf8, utf8_len) == 0)
	    {
		/* Requested utf8 mapping matches the existing mapping */
		*is_mapped = TRUE;
	    }
	} else {
	    /* No existing mapping. Use the requested mapping */
	    sub_font_glyph->utf8 = malloc (utf8_len + 1);
	    if (unlikely (sub_font_glyph->utf8 == NULL))
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	    memcpy (sub_font_glyph->utf8, utf8, utf8_len);
	    sub_font_glyph->utf8[utf8_len] = 0;
	    sub_font_glyph->utf8_len = utf8_len;
	    *is_mapped = TRUE;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_sub_font_lookup_glyph (cairo_sub_font_t	                *sub_font,
                              unsigned long	                 scaled_font_glyph_index,
			      const char			*utf8,
			      int				 utf8_len,
                              cairo_scaled_font_subsets_glyph_t *subset_glyph)
{
    cairo_sub_font_glyph_t key, *sub_font_glyph;
    cairo_int_status_t status;

    _cairo_sub_font_glyph_init_key (&key, scaled_font_glyph_index);
    sub_font_glyph = _cairo_hash_table_lookup (sub_font->sub_font_glyphs,
					      &key.base);
    if (sub_font_glyph != NULL) {
        subset_glyph->font_id = sub_font->font_id;
        subset_glyph->subset_id = sub_font_glyph->subset_id;
        subset_glyph->subset_glyph_index = sub_font_glyph->subset_glyph_index;
        subset_glyph->is_scaled = sub_font->is_scaled;
        subset_glyph->is_composite = sub_font->is_composite;
        subset_glyph->x_advance = sub_font_glyph->x_advance;
        subset_glyph->y_advance = sub_font_glyph->y_advance;
	status = _cairo_sub_font_glyph_map_to_unicode (sub_font_glyph,
						       utf8, utf8_len,
						       &subset_glyph->utf8_is_mapped);
	subset_glyph->unicode = sub_font_glyph->unicode;

	return status;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_status_t
_cairo_sub_font_map_glyph (cairo_sub_font_t	*sub_font,
			   unsigned long	 scaled_font_glyph_index,
			   const char		*utf8,
			   int			 utf8_len,
                           cairo_scaled_font_subsets_glyph_t *subset_glyph)
{
    cairo_sub_font_glyph_t key, *sub_font_glyph;
    cairo_status_t status;

    _cairo_sub_font_glyph_init_key (&key, scaled_font_glyph_index);
    sub_font_glyph = _cairo_hash_table_lookup (sub_font->sub_font_glyphs,
					       &key.base);
    if (sub_font_glyph == NULL) {
	cairo_scaled_glyph_t *scaled_glyph;

	if (sub_font->num_glyphs_in_current_subset == sub_font->max_glyphs_per_subset)
	{
	    cairo_scaled_font_subsets_glyph_t tmp_subset_glyph;

	    sub_font->current_subset++;
	    sub_font->num_glyphs_in_current_subset = 0;

	    /* Reserve first glyph in subset for the .notdef glyph
	     * except for Type 3 fonts */
	    if (! _cairo_font_face_is_user (sub_font->scaled_font->font_face)) {
		status = _cairo_sub_font_map_glyph (sub_font, 0, NULL, -1, &tmp_subset_glyph);
		if (unlikely (status))
		    return status;
	    }
	}

	_cairo_scaled_font_freeze_cache (sub_font->scaled_font);
        status = _cairo_scaled_glyph_lookup (sub_font->scaled_font,
                                             scaled_font_glyph_index,
                                             CAIRO_SCALED_GLYPH_INFO_METRICS,
                                             &scaled_glyph);
	assert (status != CAIRO_INT_STATUS_UNSUPPORTED);
	if (unlikely (status)) {
	    _cairo_scaled_font_thaw_cache (sub_font->scaled_font);
	    return status;
	}

        sub_font_glyph = _cairo_sub_font_glyph_create (scaled_font_glyph_index,
						       sub_font->current_subset,
						       sub_font->num_glyphs_in_current_subset,
                                                       scaled_glyph->metrics.x_advance,
                                                       scaled_glyph->metrics.y_advance);
	_cairo_scaled_font_thaw_cache (sub_font->scaled_font);

	if (unlikely (sub_font_glyph == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	status = _cairo_sub_font_glyph_lookup_unicode (sub_font_glyph,
						       sub_font->scaled_font,
						       scaled_font_glyph_index);
	if (unlikely (status)) {
	    _cairo_sub_font_glyph_destroy (sub_font_glyph);
	    return status;
	}

	status = _cairo_hash_table_insert (sub_font->sub_font_glyphs, &sub_font_glyph->base);
	if (unlikely (status)) {
	    _cairo_sub_font_glyph_destroy (sub_font_glyph);
	    return status;
	}

	sub_font->num_glyphs_in_current_subset++;

        if (sub_font->is_scaled) {
            if (sub_font->num_glyphs_in_current_subset > sub_font->parent->max_glyphs_per_scaled_subset_used)
                sub_font->parent->max_glyphs_per_scaled_subset_used = sub_font->num_glyphs_in_current_subset;
        } else {
            if (sub_font->num_glyphs_in_current_subset > sub_font->parent->max_glyphs_per_unscaled_subset_used)
                sub_font->parent->max_glyphs_per_unscaled_subset_used = sub_font->num_glyphs_in_current_subset;
        }
    }

    subset_glyph->font_id = sub_font->font_id;
    subset_glyph->subset_id = sub_font_glyph->subset_id;
    subset_glyph->subset_glyph_index = sub_font_glyph->subset_glyph_index;
    subset_glyph->is_scaled = sub_font->is_scaled;
    subset_glyph->is_composite = sub_font->is_composite;
    subset_glyph->x_advance = sub_font_glyph->x_advance;
    subset_glyph->y_advance = sub_font_glyph->y_advance;
    status = _cairo_sub_font_glyph_map_to_unicode (sub_font_glyph,
						   utf8, utf8_len,
						   &subset_glyph->utf8_is_mapped);
    subset_glyph->unicode = sub_font_glyph->unicode;

    return status;
}

static void
_cairo_sub_font_collect (void *entry, void *closure)
{
    cairo_sub_font_t *sub_font = entry;
    cairo_sub_font_collection_t *collection = closure;
    cairo_scaled_font_subset_t subset;
    int i;
    unsigned int j;

    if (collection->status)
	return;

    collection->status = sub_font->scaled_font->status;
    if (collection->status)
	return;

    for (i = 0; i <= sub_font->current_subset; i++) {
	collection->subset_id = i;
	collection->num_glyphs = 0;
	collection->max_glyph = 0;

	_cairo_hash_table_foreach (sub_font->sub_font_glyphs,
				   _cairo_sub_font_glyph_collect, collection);
	if (collection->status)
	    break;
	if (collection->num_glyphs == 0)
	    continue;

        /* Ensure the resulting array has no uninitialized holes */
	assert (collection->num_glyphs == collection->max_glyph + 1);

	subset.scaled_font = sub_font->scaled_font;
	subset.is_composite = sub_font->is_composite;
	subset.is_scaled = sub_font->is_scaled;
	subset.font_id = sub_font->font_id;
	subset.subset_id = i;
	subset.glyphs = collection->glyphs;
	subset.utf8 = collection->utf8;
	subset.num_glyphs = collection->num_glyphs;
        subset.glyph_names = NULL;
        /* No need to check for out of memory here. If to_unicode is NULL, the PDF
         * surface does not emit an ToUnicode stream */
        subset.to_unicode = _cairo_malloc_ab (collection->num_glyphs, sizeof (unsigned long));
        if (subset.to_unicode) {
            for (j = 0; j < collection->num_glyphs; j++) {
                /* default unicode character required when mapping fails */
                subset.to_unicode[j] = 0xfffd;
            }
        }
        collection->status = (collection->font_subset_callback) (&subset,
					    collection->font_subset_callback_closure);

        if (subset.to_unicode != NULL)
            free (subset.to_unicode);

	if (subset.glyph_names != NULL) {
            for (j = 0; j < collection->num_glyphs; j++)
		free (subset.glyph_names[j]);
	    free (subset.glyph_names);
	}

	if (collection->status)
	    break;
    }
}

static cairo_scaled_font_subsets_t *
_cairo_scaled_font_subsets_create_internal (cairo_subsets_type_t type)
{
    cairo_scaled_font_subsets_t *subsets;

    subsets = malloc (sizeof (cairo_scaled_font_subsets_t));
    if (unlikely (subsets == NULL)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    subsets->type = type;
    subsets->max_glyphs_per_unscaled_subset_used = 0;
    subsets->max_glyphs_per_scaled_subset_used = 0;
    subsets->num_sub_fonts = 0;

    subsets->unscaled_sub_fonts = _cairo_hash_table_create (_cairo_sub_fonts_equal);
    if (! subsets->unscaled_sub_fonts) {
	free (subsets);
	return NULL;
    }
    subsets->unscaled_sub_fonts_list = NULL;
    subsets->unscaled_sub_fonts_list_end = NULL;

    subsets->scaled_sub_fonts = _cairo_hash_table_create (_cairo_sub_fonts_equal);
    if (! subsets->scaled_sub_fonts) {
	_cairo_hash_table_destroy (subsets->unscaled_sub_fonts);
	free (subsets);
	return NULL;
    }
    subsets->scaled_sub_fonts_list = NULL;
    subsets->scaled_sub_fonts_list_end = NULL;

    return subsets;
}

cairo_scaled_font_subsets_t *
_cairo_scaled_font_subsets_create_scaled (void)
{
    return _cairo_scaled_font_subsets_create_internal (CAIRO_SUBSETS_SCALED);
}

cairo_scaled_font_subsets_t *
_cairo_scaled_font_subsets_create_simple (void)
{
    return _cairo_scaled_font_subsets_create_internal (CAIRO_SUBSETS_SIMPLE);
}

cairo_scaled_font_subsets_t *
_cairo_scaled_font_subsets_create_composite (void)
{
    return _cairo_scaled_font_subsets_create_internal (CAIRO_SUBSETS_COMPOSITE);
}

void
_cairo_scaled_font_subsets_destroy (cairo_scaled_font_subsets_t *subsets)
{
    _cairo_hash_table_foreach (subsets->scaled_sub_fonts, _cairo_sub_font_pluck, subsets->scaled_sub_fonts);
    _cairo_hash_table_destroy (subsets->scaled_sub_fonts);

    _cairo_hash_table_foreach (subsets->unscaled_sub_fonts, _cairo_sub_font_pluck, subsets->unscaled_sub_fonts);
    _cairo_hash_table_destroy (subsets->unscaled_sub_fonts);

    free (subsets);
}

cairo_status_t
_cairo_scaled_font_subsets_map_glyph (cairo_scaled_font_subsets_t	*subsets,
				      cairo_scaled_font_t		*scaled_font,
				      unsigned long			 scaled_font_glyph_index,
				      const char *			 utf8,
				      int				 utf8_len,
                                      cairo_scaled_font_subsets_glyph_t *subset_glyph)
{
    cairo_sub_font_t key, *sub_font;
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_font_face_t *font_face;
    cairo_matrix_t identity;
    cairo_font_options_t font_options;
    cairo_scaled_font_t	*unscaled_font;
    cairo_status_t status;
    int max_glyphs;
    cairo_bool_t type1_font;

    /* Lookup glyph in unscaled subsets */
    if (subsets->type != CAIRO_SUBSETS_SCALED) {
        key.is_scaled = FALSE;
        _cairo_sub_font_init_key (&key, scaled_font);
	sub_font = _cairo_hash_table_lookup (subsets->unscaled_sub_fonts,
					     &key.base);
        if (sub_font != NULL) {
            status = _cairo_sub_font_lookup_glyph (sub_font,
						   scaled_font_glyph_index,
						   utf8, utf8_len,
						   subset_glyph);
	    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
                return status;
        }
    }

    /* Lookup glyph in scaled subsets */
    key.is_scaled = TRUE;
    _cairo_sub_font_init_key (&key, scaled_font);
    sub_font = _cairo_hash_table_lookup (subsets->scaled_sub_fonts,
					 &key.base);
    if (sub_font != NULL) {
	status = _cairo_sub_font_lookup_glyph (sub_font,
					       scaled_font_glyph_index,
					       utf8, utf8_len,
					       subset_glyph);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    /* Glyph not found. Determine whether the glyph is outline or
     * bitmap and add to the appropriate subset.
     *
     * glyph_index 0 (the .notdef glyph) is a special case. Some fonts
     * will return CAIRO_INT_STATUS_UNSUPPORTED when doing a
     * _scaled_glyph_lookup(_GLYPH_INFO_PATH). Type1-fallback creates
     * empty glyphs in this case so we can put the glyph in a unscaled
     * subset. */
    if (scaled_font_glyph_index == 0 ||
	_cairo_font_face_is_user (scaled_font->font_face)) {
	status = CAIRO_STATUS_SUCCESS;
    } else {
	_cairo_scaled_font_freeze_cache (scaled_font);
	status = _cairo_scaled_glyph_lookup (scaled_font,
					     scaled_font_glyph_index,
					     CAIRO_SCALED_GLYPH_INFO_PATH,
					     &scaled_glyph);
	_cairo_scaled_font_thaw_cache (scaled_font);
    }
    if (_cairo_status_is_error (status))
        return status;

    if (status == CAIRO_STATUS_SUCCESS &&
	subsets->type != CAIRO_SUBSETS_SCALED &&
	! _cairo_font_face_is_user (scaled_font->font_face))
    {
        /* Path available. Add to unscaled subset. */
        key.is_scaled = FALSE;
        _cairo_sub_font_init_key (&key, scaled_font);
	sub_font = _cairo_hash_table_lookup (subsets->unscaled_sub_fonts,
					     &key.base);
        if (sub_font == NULL) {
            font_face = cairo_scaled_font_get_font_face (scaled_font);
            cairo_matrix_init_identity (&identity);
            _cairo_font_options_init_default (&font_options);
            cairo_font_options_set_hint_style (&font_options, CAIRO_HINT_STYLE_NONE);
            cairo_font_options_set_hint_metrics (&font_options, CAIRO_HINT_METRICS_OFF);
            unscaled_font = cairo_scaled_font_create (font_face,
                                                      &identity,
                                                      &identity,
                                                      &font_options);
	    if (unlikely (unscaled_font->status))
		return unscaled_font->status;

            subset_glyph->is_scaled = FALSE;
            type1_font = FALSE;
#if CAIRO_HAS_FT_FONT
            type1_font = _cairo_type1_scaled_font_is_type1 (unscaled_font);
#endif
            if (subsets->type == CAIRO_SUBSETS_COMPOSITE && !type1_font) {
                max_glyphs = MAX_GLYPHS_PER_COMPOSITE_FONT;
                subset_glyph->is_composite = TRUE;
            } else {
                max_glyphs = MAX_GLYPHS_PER_SIMPLE_FONT;
                subset_glyph->is_composite = FALSE;
            }

            status = _cairo_sub_font_create (subsets,
					     unscaled_font,
					     subsets->num_sub_fonts,
					     max_glyphs,
					     subset_glyph->is_scaled,
					     subset_glyph->is_composite,
					     &sub_font);

            if (unlikely (status)) {
		cairo_scaled_font_destroy (unscaled_font);
                return status;
	    }

            status = _cairo_hash_table_insert (subsets->unscaled_sub_fonts,
                                               &sub_font->base);

            if (unlikely (status)) {
		_cairo_sub_font_destroy (sub_font);
                return status;
	    }
	    if (!subsets->unscaled_sub_fonts_list)
		subsets->unscaled_sub_fonts_list = sub_font;
	    else
		subsets->unscaled_sub_fonts_list_end->next = sub_font;
	    subsets->unscaled_sub_fonts_list_end = sub_font;
	    subsets->num_sub_fonts++;
        }
    } else {
        /* No path available. Add to scaled subset. */
        key.is_scaled = TRUE;
        _cairo_sub_font_init_key (&key, scaled_font);
	sub_font = _cairo_hash_table_lookup (subsets->scaled_sub_fonts,
					     &key.base);
        if (sub_font == NULL) {
            subset_glyph->is_scaled = TRUE;
            subset_glyph->is_composite = FALSE;
            if (subsets->type == CAIRO_SUBSETS_SCALED)
                max_glyphs = INT_MAX;
            else
                max_glyphs = MAX_GLYPHS_PER_SIMPLE_FONT;

            status = _cairo_sub_font_create (subsets,
					     cairo_scaled_font_reference (scaled_font),
					     subsets->num_sub_fonts,
					     max_glyphs,
					     subset_glyph->is_scaled,
					     subset_glyph->is_composite,
					     &sub_font);
            if (unlikely (status)) {
		cairo_scaled_font_destroy (scaled_font);
                return status;
	    }

            status = _cairo_hash_table_insert (subsets->scaled_sub_fonts,
                                               &sub_font->base);
            if (unlikely (status)) {
		_cairo_sub_font_destroy (sub_font);
                return status;
	    }
	    if (!subsets->scaled_sub_fonts_list)
		subsets->scaled_sub_fonts_list = sub_font;
	    else
		subsets->scaled_sub_fonts_list_end->next = sub_font;
	    subsets->scaled_sub_fonts_list_end = sub_font;
	    subsets->num_sub_fonts++;
        }
    }

    return _cairo_sub_font_map_glyph (sub_font,
				      scaled_font_glyph_index,
				      utf8, utf8_len,
				      subset_glyph);
}

static cairo_status_t
_cairo_scaled_font_subsets_foreach_internal (cairo_scaled_font_subsets_t              *font_subsets,
                                             cairo_scaled_font_subset_callback_func_t  font_subset_callback,
                                             void				      *closure,
					     cairo_subsets_foreach_type_t	       type)
{
    cairo_sub_font_collection_t collection;
    cairo_sub_font_t *sub_font;
    cairo_bool_t is_scaled, is_user;

    is_scaled = FALSE;
    is_user = FALSE;

    if (type == CAIRO_SUBSETS_FOREACH_USER)
	is_user = TRUE;

    if (type == CAIRO_SUBSETS_FOREACH_SCALED ||
	type == CAIRO_SUBSETS_FOREACH_USER)
    {
	is_scaled = TRUE;
    }

    if (is_scaled)
        collection.glyphs_size = font_subsets->max_glyphs_per_scaled_subset_used;
    else
        collection.glyphs_size = font_subsets->max_glyphs_per_unscaled_subset_used;

    if (! collection.glyphs_size)
	return CAIRO_STATUS_SUCCESS;

    collection.glyphs = _cairo_malloc_ab (collection.glyphs_size, sizeof(unsigned long));
    collection.utf8 = _cairo_malloc_ab (collection.glyphs_size, sizeof(char *));
    if (unlikely (collection.glyphs == NULL || collection.utf8 == NULL)) {
	if (collection.glyphs != NULL)
	    free (collection.glyphs);
	if (collection.utf8 != NULL)
	    free (collection.utf8);

	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    collection.font_subset_callback = font_subset_callback;
    collection.font_subset_callback_closure = closure;
    collection.status = CAIRO_STATUS_SUCCESS;

    if (is_scaled)
	sub_font = font_subsets->scaled_sub_fonts_list;
    else
	sub_font = font_subsets->unscaled_sub_fonts_list;

    while (sub_font) {
	if (sub_font->is_user == is_user)
	    _cairo_sub_font_collect (sub_font, &collection);

	sub_font = sub_font->next;
    }
    free (collection.utf8);
    free (collection.glyphs);

    return collection.status;
}

cairo_status_t
_cairo_scaled_font_subsets_foreach_scaled (cairo_scaled_font_subsets_t		    *font_subsets,
                                           cairo_scaled_font_subset_callback_func_t  font_subset_callback,
                                           void					    *closure)
{
    return _cairo_scaled_font_subsets_foreach_internal (font_subsets,
                                                        font_subset_callback,
                                                        closure,
							CAIRO_SUBSETS_FOREACH_SCALED);
}

cairo_status_t
_cairo_scaled_font_subsets_foreach_unscaled (cairo_scaled_font_subsets_t	    *font_subsets,
                                           cairo_scaled_font_subset_callback_func_t  font_subset_callback,
                                           void					    *closure)
{
    return _cairo_scaled_font_subsets_foreach_internal (font_subsets,
                                                        font_subset_callback,
                                                        closure,
							CAIRO_SUBSETS_FOREACH_UNSCALED);
}

cairo_status_t
_cairo_scaled_font_subsets_foreach_user (cairo_scaled_font_subsets_t		  *font_subsets,
					 cairo_scaled_font_subset_callback_func_t  font_subset_callback,
					 void					  *closure)
{
    return _cairo_scaled_font_subsets_foreach_internal (font_subsets,
                                                        font_subset_callback,
                                                        closure,
							CAIRO_SUBSETS_FOREACH_USER);
}

static cairo_bool_t
_cairo_string_equal (const void *key_a, const void *key_b)
{
    const cairo_string_entry_t *a = key_a;
    const cairo_string_entry_t *b = key_b;

    if (strcmp (a->string, b->string) == 0)
	return TRUE;
    else
	return FALSE;
}

static void
_cairo_string_init_key (cairo_string_entry_t *key, char *s)
{
    unsigned long sum = 0;
    unsigned int i;

    for (i = 0; i < strlen(s); i++)
        sum += s[i];
    key->base.hash = sum;
    key->string = s;
}

static cairo_status_t
create_string_entry (char *s, cairo_string_entry_t **entry)
{
    *entry = malloc (sizeof (cairo_string_entry_t));
    if (unlikely (*entry == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_string_init_key (*entry, s);

    return CAIRO_STATUS_SUCCESS;
}

static void
_pluck_entry (void *entry, void *closure)
{
    _cairo_hash_table_remove (closure, entry);
    free (entry);
}

cairo_int_status_t
_cairo_scaled_font_subset_create_glyph_names (cairo_scaled_font_subset_t *subset)
{
    unsigned int i;
    cairo_hash_table_t *names;
    cairo_string_entry_t key, *entry;
    char buf[30];
    char *utf8;
    uint16_t *utf16;
    int utf16_len;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    names = _cairo_hash_table_create (_cairo_string_equal);
    if (unlikely (names == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    subset->glyph_names = calloc (subset->num_glyphs, sizeof (char *));
    if (unlikely (subset->glyph_names == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_HASH;
    }

    i = 0;
    if (! subset->is_scaled) {
	subset->glyph_names[0] = strdup (".notdef");
	if (unlikely (subset->glyph_names[0] == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto CLEANUP_HASH;
	}

	status = create_string_entry (subset->glyph_names[0], &entry);
	if (unlikely (status))
	    goto CLEANUP_HASH;

	status = _cairo_hash_table_insert (names, &entry->base);
	if (unlikely (status)) {
	    free (entry);
	    goto CLEANUP_HASH;
	}
	i++;
    }

    for (; i < subset->num_glyphs; i++) {
	utf8 = subset->utf8[i];
	utf16 = NULL;
	utf16_len = 0;
	if (utf8 && *utf8) {
	    status = _cairo_utf8_to_utf16 (utf8, -1, &utf16, &utf16_len);
	    if (unlikely (status))
		goto CLEANUP_HASH;
	}

	if (utf16_len == 1) {
	    snprintf (buf, sizeof (buf), "uni%04X", (int) utf16[0]);
	    _cairo_string_init_key (&key, buf);
	    entry = _cairo_hash_table_lookup (names, &key.base);
	    if (entry != NULL)
		snprintf (buf, sizeof (buf), "g%d", i);
	} else {
	    snprintf (buf, sizeof (buf), "g%d", i);
	}
	if (utf16)
	    free (utf16);

	subset->glyph_names[i] = strdup (buf);
	if (unlikely (subset->glyph_names[i] == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto CLEANUP_HASH;
	}

	status = create_string_entry (subset->glyph_names[i], &entry);
	if (unlikely (status))
	    goto CLEANUP_HASH;

	status = _cairo_hash_table_insert (names, &entry->base);
	if (unlikely (status)) {
	    free (entry);
	    goto CLEANUP_HASH;
	}
    }

CLEANUP_HASH:
    _cairo_hash_table_foreach (names, _pluck_entry, names);
    _cairo_hash_table_destroy (names);

    if (likely (status == CAIRO_STATUS_SUCCESS))
	return CAIRO_STATUS_SUCCESS;

    if (subset->glyph_names != NULL) {
	for (i = 0; i < subset->num_glyphs; i++) {
	    if (subset->glyph_names[i] != NULL)
		free (subset->glyph_names[i]);
	}

	free (subset->glyph_names);
	subset->glyph_names = NULL;
    }

    return status;
}

#endif /* CAIRO_HAS_FONT_SUBSET */

/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat Inc.
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
 *      Graydon Hoare <graydon@redhat.com>
 *      Owen Taylor <otaylor@redhat.com>
 */

#include "cairoint.h"

/* cairo_font_face_t */

void
_cairo_font_face_init (cairo_font_face_t               *font_face, 
		       const cairo_font_face_backend_t *backend)
{
    font_face->refcount = 1;
    font_face->backend = backend;

    _cairo_user_data_array_init (&font_face->user_data);
}

/**
 * cairo_font_face_reference:
 * @font_face: a #cairo_font_face_t
 * 
 * Increases the reference count on @font_face by one. This prevents
 * @font_face from being destroyed until a matching call to cairo_font_face_destroy() 
 * is made.
 **/
void
cairo_font_face_reference (cairo_font_face_t *font_face)
{
    font_face->refcount++;
}

/**
 * cairo_font_face_destroy:
 * @font_face: a #cairo_font_face_t
 * 
 * Decreases the reference count on @font_face by one. If the result
 * is zero, then @font_face and all associated resources are freed.
 * See cairo_font_face_reference().
 **/
void
cairo_font_face_destroy (cairo_font_face_t *font_face)
{
    if (--(font_face->refcount) > 0)
	return;

    font_face->backend->destroy (font_face);

    /* We allow resurrection to deal with some memory management for the
     * FreeType backend where cairo_ft_font_face_t and cairo_ft_unscaled_font_t
     * need to effectively mutually reference each other
     */
    if (font_face->refcount > 0)
	return;

    _cairo_user_data_array_destroy (&font_face->user_data);

    free (font_face);
}

/**
 * cairo_font_face_get_user_data:
 * @font_face: a #cairo_font_face_t
 * @key: the address of the #cairo_user_data_key_t the user data was
 * attached to
 * 
 * Return user data previously attached to @font_face using the specified
 * key.  If no user data has been attached with the given key this
 * function returns %NULL.
 * 
 * Return value: the user data previously attached or %NULL.
 **/
void *
cairo_font_face_get_user_data (cairo_font_face_t	   *font_face,
			       const cairo_user_data_key_t *key)
{
    return _cairo_user_data_array_get_data (&font_face->user_data,
					    key);
}

/**
 * cairo_font_face_set_user_data:
 * @font_face: a #cairo_font_face_t
 * @key: the address of a #cairo_user_data_key_t to attach the user data to
 * @user_data: the user data to attach to the font face
 * @destroy: a #cairo_destroy_func_t which will be called when the
 * font face is destroyed or when new user data is attached using the
 * same key.
 * 
 * Attach user data to @font_face.  To remove user data from a font face,
 * call this function with the key that was used to set it and %NULL
 * for @data.
 *
 * Return value: %CAIRO_STATUS_SUCCESS or %CAIRO_STATUS_NO_MEMORY if a
 * slot could not be allocated for the user data.
 **/
cairo_status_t
cairo_font_face_set_user_data (cairo_font_face_t	   *font_face,
			       const cairo_user_data_key_t *key,
			       void			   *user_data,
			       cairo_destroy_func_t	    destroy)
{
    return _cairo_user_data_array_set_data (&font_face->user_data,
					    key, user_data, destroy);
}

/* cairo_simple_font_face_t - simple family/slant/weight font faces used for
 * the built-in font API
 */

typedef struct _cairo_simple_font_face cairo_simple_font_face_t;

struct _cairo_simple_font_face {
    cairo_font_face_t base;
    char *family;
    cairo_font_slant_t slant;
    cairo_font_weight_t weight;
};

static const cairo_font_face_backend_t _cairo_simple_font_face_backend;

/* We maintain a global cache from family/weight/slant => cairo_font_face_t
 * for cairo_simple_font_t. The primary purpose of this cache is to provide
 * unique cairo_font_face_t values so that our cache from
 * cairo_font_face_t => cairo_scaled_font_t works. For this reason, we don't need
 * this cache to keep font faces alive; we just add them to the cache and
 * remove them again when freed.
 */

typedef struct {
    cairo_cache_entry_base_t base;
    const char *family;
    cairo_font_slant_t slant;
    cairo_font_weight_t weight;
} cairo_simple_cache_key_t;

typedef struct {
    cairo_simple_cache_key_t key;
    cairo_simple_font_face_t *font_face;
} cairo_simple_cache_entry_t;

static const cairo_cache_backend_t _cairo_simple_font_cache_backend;

static void
_lock_global_simple_cache (void)
{
    /* FIXME: Perform locking here. */
}

static void
_unlock_global_simple_cache (void)
{
    /* FIXME: Perform locking here. */
}

static cairo_cache_t *
_get_global_simple_cache (void)
{
    static cairo_cache_t *global_simple_cache = NULL;

    if (global_simple_cache == NULL)
    {
	global_simple_cache = malloc (sizeof (cairo_cache_t));	
	if (!global_simple_cache)
	    goto FAIL;

	if (_cairo_cache_init (global_simple_cache,
			       &_cairo_simple_font_cache_backend,
			       0)) /* No memory limit */
	    goto FAIL;
    }
    return global_simple_cache;

 FAIL:
    if (global_simple_cache)
	free (global_simple_cache);
    global_simple_cache = NULL;
    return NULL;
}

static unsigned long
_cairo_simple_font_cache_hash (void *cache, void *key)
{
    cairo_simple_cache_key_t *k = (cairo_simple_cache_key_t *) key;
    unsigned long hash;

    /* 1607 and 1451 are just a couple random primes. */
    hash = _cairo_hash_string (k->family);
    hash += ((unsigned long) k->slant) * 1607;
    hash += ((unsigned long) k->weight) * 1451;
    
    return hash;
}

static int
_cairo_simple_font_cache_keys_equal (void *cache,
				     void *k1,
				     void *k2)
{
    cairo_simple_cache_key_t *a;
    cairo_simple_cache_key_t *b;
    a = (cairo_simple_cache_key_t *) k1;
    b = (cairo_simple_cache_key_t *) k2;

    return strcmp (a->family, b->family) == 0 &&
	a->slant == b->slant &&
	a->weight == b->weight;
}

static cairo_simple_font_face_t *
_cairo_simple_font_face_create_from_cache_key (cairo_simple_cache_key_t *key)
{
    cairo_simple_font_face_t *simple_face;

    simple_face = malloc (sizeof (cairo_simple_font_face_t));
    if (!simple_face)
	return NULL;
    
    simple_face->family = strdup (key->family);
    if (!simple_face->family) {
	free (simple_face);
	return NULL;
    }

    simple_face->slant = key->slant;
    simple_face->weight = key->weight;

    _cairo_font_face_init (&simple_face->base, &_cairo_simple_font_face_backend);

    return simple_face;
}

static cairo_status_t
_cairo_simple_font_cache_create_entry (void  *cache,
				       void  *key,
				       void **return_entry)
{
    cairo_simple_cache_key_t *k = (cairo_simple_cache_key_t *) key;
    cairo_simple_cache_entry_t *entry;

    entry = malloc (sizeof (cairo_simple_cache_entry_t));
    if (entry == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    entry->font_face = _cairo_simple_font_face_create_from_cache_key (k);
    if (!entry->font_face) {
	free (entry);
	return CAIRO_STATUS_NO_MEMORY;
    }
    
    entry->key.base.memory = 0;
    entry->key.family = entry->font_face->family;
    entry->key.slant = entry->font_face->slant;
    entry->key.weight = entry->font_face->weight;
    
    *return_entry = entry;

    return CAIRO_STATUS_SUCCESS;
}

/* Entries are never spontaneously destroyed; but only when
 * we remove them from the cache specifically. We free entry->font_face
 * in the code that removes the entry from the cache
 */
static void
_cairo_simple_font_cache_destroy_entry (void *cache,
					void *entry)
{    
    cairo_simple_cache_entry_t *e = (cairo_simple_cache_entry_t *) entry;

    free (e);
}

static void 
_cairo_simple_font_cache_destroy_cache (void *cache)
{
    free (cache);
}

static const cairo_cache_backend_t _cairo_simple_font_cache_backend = {
    _cairo_simple_font_cache_hash,
    _cairo_simple_font_cache_keys_equal,
    _cairo_simple_font_cache_create_entry,
    _cairo_simple_font_cache_destroy_entry,
    _cairo_simple_font_cache_destroy_cache
};

static void
_cairo_simple_font_face_destroy (void *abstract_face)
{
    cairo_simple_font_face_t *simple_face = abstract_face;
    cairo_cache_t *cache;
    cairo_simple_cache_key_t key;

    _lock_global_simple_cache ();
    cache = _get_global_simple_cache ();
    assert (cache);
    
    key.family = simple_face->family;
    key.slant = simple_face->slant;
    key.weight = simple_face->weight;
	
    _cairo_cache_remove (cache, &key);
    
    _unlock_global_simple_cache ();
    
    free (simple_face->family);
}

static cairo_status_t
_cairo_simple_font_face_create_font (void                 *abstract_face,
				     const cairo_matrix_t *font_matrix,
				     const cairo_matrix_t *ctm,
				     cairo_scaled_font_t **scaled_font)
{
    const cairo_scaled_font_backend_t *backend = CAIRO_FONT_BACKEND_DEFAULT;
    cairo_simple_font_face_t *simple_face = abstract_face;

    return backend->create (simple_face->family, simple_face->slant, simple_face->weight,
			    font_matrix, ctm, scaled_font);
}

static const cairo_font_face_backend_t _cairo_simple_font_face_backend = {
    _cairo_simple_font_face_destroy,
    _cairo_simple_font_face_create_font,
};

/**
 * _cairo_simple_font_face_create:
 * @family: a font family name, encoded in UTF-8
 * @slant: the slant for the font
 * @weight: the weight for the font
 * 
 * Creates a font face from a triplet of family, slant, and weight.
 * These font faces are used in implementation of the the #cairo_t "toy"
 * font API.
 * 
 * Return value: a newly created #cairo_font_face_t, destroy with
 *  cairo_font_face_destroy()
 **/
cairo_private cairo_font_face_t *
_cairo_simple_font_face_create (const char          *family, 
				cairo_font_slant_t   slant, 
				cairo_font_weight_t  weight)
{
    cairo_simple_cache_entry_t *entry;
    cairo_simple_cache_key_t key;
    cairo_cache_t *cache;
    cairo_status_t status;
    cairo_bool_t created_entry;

    key.family = family;
    key.slant = slant;
    key.weight = weight;
    
    _lock_global_simple_cache ();
    cache = _get_global_simple_cache ();
    if (cache == NULL) {
	_unlock_global_simple_cache ();
	return NULL;
    }
    status = _cairo_cache_lookup (cache, &key, (void **) &entry, &created_entry);
    if (CAIRO_OK (status) && !created_entry)
	cairo_font_face_reference (&entry->font_face->base);
    
    _unlock_global_simple_cache ();
    if (status)
	return NULL;

    return &entry->font_face->base;
}

/* cairo_scaled_font_t */

/* Here we keep a cache from cairo_font_face_t/matrix/ctm => cairo_scaled_font_t.
 *
 * The implementation is messy because we want
 *
 *  - All otherwise referenced cairo_scaled_font_t's to be in the cache
 *  - Some number of not otherwise referenced cairo_scaled_font_t's
 *
 * For this reason, we actually use *two* caches ... a finite size
 * cache that references the cairo_scaled_font_t as a first level (the outer
 * cache), then an infinite size cache as the second level (the inner
 * cache). A single cache could be used at the cost of complicating
 * cairo-cache.c
 */

/* This defines the size of the outer cache ... that is, the number
 * of scaled fonts we keep around even when not otherwise referenced
 */
#define MAX_CACHED_FONTS 24

typedef struct {
    cairo_cache_entry_base_t base;
    cairo_font_face_t *font_face;
    const cairo_matrix_t *font_matrix;
    const cairo_matrix_t *ctm;
} cairo_font_cache_key_t;

typedef struct {
    cairo_font_cache_key_t key;
    cairo_scaled_font_t *scaled_font;
} cairo_font_cache_entry_t;

static const cairo_cache_backend_t _cairo_outer_font_cache_backend;
static const cairo_cache_backend_t _cairo_inner_font_cache_backend;

static void
_lock_global_font_cache (void)
{
    /* FIXME: Perform locking here. */
}

static void
_unlock_global_font_cache (void)
{
    /* FIXME: Perform locking here. */
}

static cairo_cache_t *
_get_outer_font_cache (void)
{
    static cairo_cache_t *outer_font_cache = NULL;

    if (outer_font_cache == NULL)
    {
	outer_font_cache = malloc (sizeof (cairo_cache_t));	
	if (!outer_font_cache)
	    goto FAIL;

	if (_cairo_cache_init (outer_font_cache,
			       &_cairo_outer_font_cache_backend,
			       MAX_CACHED_FONTS))
	    goto FAIL;
    }
    return outer_font_cache;

 FAIL:
    if (outer_font_cache)
	free (outer_font_cache);
    outer_font_cache = NULL;
    return NULL;
}

static cairo_cache_t *
_get_inner_font_cache (void)
{
    static cairo_cache_t *inner_font_cache = NULL;

    if (inner_font_cache == NULL)
    {
	inner_font_cache = malloc (sizeof (cairo_cache_t));	
	if (!inner_font_cache)
	    goto FAIL;

	if (_cairo_cache_init (inner_font_cache,
			       &_cairo_inner_font_cache_backend,
			       MAX_CACHED_FONTS))
	    goto FAIL;
    }
    return inner_font_cache;

 FAIL:
    if (inner_font_cache)
	free (inner_font_cache);
    inner_font_cache = NULL;
    return NULL;
}


/* Fowler / Noll / Vo (FNV) Hash (http://www.isthe.com/chongo/tech/comp/fnv/)
 * 
 * Not necessarily better than a lot of other hashes, but should be OK, and
 * well tested with binary data.
 */

#define FNV_32_PRIME ((uint32_t)0x01000193)
#define FNV1_32_INIT ((uint32_t)0x811c9dc5)

static uint32_t
_hash_bytes_fnv (unsigned char *buffer,
		 int            len,
		 uint32_t       hval)
{
    while (len--) {
	hval *= FNV_32_PRIME;
	hval ^= *buffer++;
    }

    return hval;
}
static unsigned long
_cairo_font_cache_hash (void *cache, void *key)
{
    cairo_font_cache_key_t *k = (cairo_font_cache_key_t *) key;
    uint32_t hash = FNV1_32_INIT;

    /* We do a bytewise hash on the font matrices */
    hash = _hash_bytes_fnv ((unsigned char *)(&k->font_matrix->xx),
			    sizeof(double) * 4,
			    hash);
    hash = _hash_bytes_fnv ((unsigned char *)(&k->ctm->xx),
			    sizeof(double) * 4,
			    hash);

    return hash ^ (unsigned long)k->font_face;
}

static int
_cairo_font_cache_keys_equal (void *cache,
			      void *k1,
			      void *k2)
{
    cairo_font_cache_key_t *a;
    cairo_font_cache_key_t *b;
    a = (cairo_font_cache_key_t *) k1;
    b = (cairo_font_cache_key_t *) k2;

    return (a->font_face == b->font_face &&
	    memcmp ((unsigned char *)(&a->font_matrix->xx),
		    (unsigned char *)(&b->font_matrix->xx),
		    sizeof(double) * 4) == 0 &&
	    memcmp ((unsigned char *)(&a->ctm->xx),
		    (unsigned char *)(&b->ctm->xx),
		    sizeof(double) * 4) == 0);
}

/* The cache lookup failed in the outer cache, so we pull
 * the font from the inner cache (if that in turns fails,
 * it will create the font
 */
static cairo_status_t
_cairo_outer_font_cache_create_entry (void  *cache,
				      void  *key,
				      void **return_entry)
{
    cairo_font_cache_entry_t *entry;
    cairo_font_cache_entry_t *inner_entry;
    cairo_bool_t created_entry;
    cairo_status_t status;

    entry = malloc (sizeof (cairo_font_cache_entry_t));
    if (entry == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    cache = _get_inner_font_cache ();
    if (cache == NULL) {
	_unlock_global_font_cache ();
	return CAIRO_STATUS_NO_MEMORY;
    }
    
    status = _cairo_cache_lookup (cache, key, (void **) &inner_entry, &created_entry);
    if (!CAIRO_OK (status)) {
	free (entry);
	return status;
    }

    entry->scaled_font = inner_entry->scaled_font;
    if (!created_entry)
	cairo_scaled_font_reference (entry->scaled_font);
    
    entry->key.base.memory = 1;	
    entry->key.font_face = entry->scaled_font->font_face;
    entry->key.font_matrix = &entry->scaled_font->font_matrix;
    entry->key.ctm = &entry->scaled_font->ctm;
    
    *return_entry = entry;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_outer_font_cache_destroy_entry (void *cache,
				       void *entry)
{    
    cairo_font_cache_entry_t *e = (cairo_font_cache_entry_t *) entry;

    cairo_scaled_font_destroy (e->scaled_font);

    free (e);
}

/* Called when the lookup fails in the inner cache as well; there
 * is no existing font, so we have to create one.
 */
static cairo_status_t
_cairo_inner_font_cache_create_entry (void  *cache,
				      void  *key,
				      void **return_entry)
{
    cairo_font_cache_key_t *k = (cairo_font_cache_key_t *) key;
    cairo_font_cache_entry_t *entry;
    cairo_status_t status;

    entry = malloc (sizeof (cairo_font_cache_entry_t));
    if (entry == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    status = k->font_face->backend->create_font (k->font_face,
						 k->font_matrix,
						 k->ctm,
						 &entry->scaled_font);
    if (!CAIRO_OK (status)) {
	free (entry);
	return status;
    }

    entry->scaled_font->font_face = k->font_face;
    cairo_font_face_reference (k->font_face);

    entry->key.base.memory = 0;	
    entry->key.font_face = k->font_face;
    entry->key.font_matrix = &entry->scaled_font->font_matrix;
    entry->key.ctm = &entry->scaled_font->ctm;
    
    *return_entry = entry;

    return CAIRO_STATUS_SUCCESS;
}

/* Entries in the inner font cache are never spontaneously destroyed;
 * but only when we remove them from the cache specifically. We free
 * entry->scaled_font in the code that removes the entry from the cache
 */
static void
_cairo_inner_font_cache_destroy_entry (void *cache,
				       void *entry)
{    
    free (entry);
}

static void 
_cairo_font_cache_destroy_cache (void *cache)
{
    free (cache);
}

static const cairo_cache_backend_t _cairo_outer_font_cache_backend = {
    _cairo_font_cache_hash,
    _cairo_font_cache_keys_equal,
    _cairo_outer_font_cache_create_entry,
    _cairo_outer_font_cache_destroy_entry,
    _cairo_font_cache_destroy_cache
};

static const cairo_cache_backend_t _cairo_inner_font_cache_backend = {
    _cairo_font_cache_hash,
    _cairo_font_cache_keys_equal,
    _cairo_inner_font_cache_create_entry,
    _cairo_inner_font_cache_destroy_entry,
    _cairo_font_cache_destroy_cache
};

/**
 * cairo_scaled_font_create:
 * @font_face: a #cairo_font_face_t
 * @font_matrix: font space to user space transformation matrix for the
 *       font. In the simplest case of a N point font, this matrix is
 *       just a scale by N, but it can also be used to shear the font
 *       or stretch it unequally along the two axes. See
 *       cairo_set_font_matrix().
 * @ctm: user to device transformation matrix with which the font will
 *       be used.
 * 
 * Creates a #cairo_scaled_font_t object from a font face and matrices that
 * describe the size of the font and the environment in which it will
 * be used.
 * 
 * Return value: a newly created #cairo_scaled_font_t. Destroy with
 *  cairo_scaled_font_destroy()
 **/
cairo_scaled_font_t *
cairo_scaled_font_create (cairo_font_face_t    *font_face,
			  const cairo_matrix_t *font_matrix,
			  const cairo_matrix_t *ctm)
{
    cairo_font_cache_entry_t *entry;
    cairo_font_cache_key_t key;
    cairo_cache_t *cache;
    cairo_status_t status;

    key.font_face = font_face;
    key.font_matrix = font_matrix;
    key.ctm = ctm;
    
    _lock_global_font_cache ();
    cache = _get_outer_font_cache ();
    if (cache == NULL) {
	_unlock_global_font_cache ();
	return NULL;
    }
    
    status = _cairo_cache_lookup (cache, &key, (void **) &entry, NULL);
    if (CAIRO_OK (status))
	cairo_scaled_font_reference (entry->scaled_font);
    
    _unlock_global_font_cache ();
    if (!CAIRO_OK (status))
	return NULL;
    
    return entry->scaled_font;
}

void
_cairo_scaled_font_init (cairo_scaled_font_t               *scaled_font, 
			 const cairo_matrix_t              *font_matrix,
			 const cairo_matrix_t              *ctm,
			 const cairo_scaled_font_backend_t *backend)
{
    scaled_font->font_matrix = *font_matrix;
    scaled_font->ctm = *ctm;
    cairo_matrix_multiply (&scaled_font->scale, &scaled_font->font_matrix, &scaled_font->ctm);
    
    scaled_font->refcount = 1;
    scaled_font->backend = backend;
}

cairo_status_t
_cairo_scaled_font_text_to_glyphs (cairo_scaled_font_t *scaled_font,
				   const char          *utf8, 
				   cairo_glyph_t      **glyphs, 
				   int 		       *num_glyphs)
{
    return scaled_font->backend->text_to_glyphs (scaled_font, utf8, glyphs, num_glyphs);
}

cairo_status_t
_cairo_scaled_font_glyph_extents (cairo_scaled_font_t   *scaled_font,
				  cairo_glyph_t 	*glyphs,
				  int 			 num_glyphs,
				  cairo_text_extents_t  *extents)
{
    return scaled_font->backend->glyph_extents (scaled_font, glyphs, num_glyphs, extents);
}


cairo_status_t
_cairo_scaled_font_glyph_bbox (cairo_scaled_font_t *scaled_font,
			       cairo_glyph_t       *glyphs,
			       int                  num_glyphs,
			       cairo_box_t	   *bbox)
{
    return scaled_font->backend->glyph_bbox (scaled_font, glyphs, num_glyphs, bbox);
}

cairo_status_t
_cairo_scaled_font_show_glyphs (cairo_scaled_font_t    *scaled_font,
				cairo_operator_t        operator,
				cairo_pattern_t        *pattern,
				cairo_surface_t        *surface,
				int                     source_x,
				int                     source_y,
				int			dest_x,
				int			dest_y,
				unsigned int		width,
				unsigned int		height,
				cairo_glyph_t          *glyphs,
				int                     num_glyphs)
{
    cairo_status_t status;

    status = _cairo_surface_show_glyphs (scaled_font, operator, pattern, 
					 surface,
					 source_x, source_y,
					 dest_x, dest_y,
					 width, height,
					 glyphs, num_glyphs);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    /* Surface display routine either does not exist or failed. */
    return scaled_font->backend->show_glyphs (scaled_font, operator, pattern, 
					      surface,
					      source_x, source_y,
					      dest_x, dest_y,
					      width, height,
					      glyphs, num_glyphs);
}

cairo_status_t
_cairo_scaled_font_glyph_path (cairo_scaled_font_t *scaled_font,
			       cairo_glyph_t	   *glyphs, 
			       int		    num_glyphs,
			       cairo_path_fixed_t  *path)
{
    return scaled_font->backend->glyph_path (scaled_font, glyphs, num_glyphs, path);
}

void
_cairo_scaled_font_get_glyph_cache_key (cairo_scaled_font_t     *scaled_font,
					cairo_glyph_cache_key_t *key)
{
    scaled_font->backend->get_glyph_cache_key (scaled_font, key);
}

cairo_status_t
_cairo_scaled_font_font_extents (cairo_scaled_font_t  *scaled_font,
				 cairo_font_extents_t *extents)
{
    return scaled_font->backend->font_extents (scaled_font, extents);
}

void
_cairo_unscaled_font_init (cairo_unscaled_font_t               *unscaled_font, 
			   const cairo_unscaled_font_backend_t *backend)
{
    unscaled_font->refcount = 1;
    unscaled_font->backend = backend;
}

void
_cairo_unscaled_font_reference (cairo_unscaled_font_t *unscaled_font)
{
    unscaled_font->refcount++;
}

void
_cairo_unscaled_font_destroy (cairo_unscaled_font_t *unscaled_font)
{    
    if (--(unscaled_font->refcount) > 0)
	return;

    unscaled_font->backend->destroy (unscaled_font);

    free (unscaled_font);
}



/* Public font API follows. */

/**
 * cairo_scaled_font_reference:
 * @scaled_font: a #cairo_scaled_font_t
 * 
 * Increases the reference count on @scaled_font by one. This prevents
 * @scaled_font from being destroyed until a matching call to
 * cairo_scaled_font_destroy() is made.
 **/
void
cairo_scaled_font_reference (cairo_scaled_font_t *scaled_font)
{
    scaled_font->refcount++;
}

/**
 * cairo_scaled_font_destroy:
 * @scaled_font: a #cairo_scaled_font_t
 * 
 * Decreases the reference count on @font by one. If the result
 * is zero, then @font and all associated resources are freed.
 * See cairo_scaled_font_reference().
 **/
void
cairo_scaled_font_destroy (cairo_scaled_font_t *scaled_font)
{
    cairo_font_cache_key_t key;
    cairo_cache_t *cache;

    if (--(scaled_font->refcount) > 0)
	return;

    if (scaled_font->font_face) {
	_lock_global_font_cache ();
	cache = _get_inner_font_cache ();
	assert (cache);

	key.font_face = scaled_font->font_face;
	key.font_matrix = &scaled_font->font_matrix;
	key.ctm = &scaled_font->ctm;
	
	_cairo_cache_remove (cache, &key);
	_unlock_global_font_cache ();

	cairo_font_face_destroy (scaled_font->font_face);
    }

    scaled_font->backend->destroy (scaled_font);

    free (scaled_font);
}

/**
 * cairo_scaled_font_extents:
 * @scaled_font: a #cairo_scaled_font_t
 * @extents: a #cairo_font_extents_t which to store the retrieved extents.
 * 
 * Gets the metrics for a #cairo_scaled_font_t. 
 * 
 * Return value: %CAIRO_STATUS_SUCCESS on success. Otherwise, an
 *  error such as %CAIRO_STATUS_NO_MEMORY.
 **/
cairo_status_t
cairo_scaled_font_extents (cairo_scaled_font_t  *scaled_font,
			   cairo_font_extents_t *extents)
{
    cairo_int_status_t status;
    double  font_scale_x, font_scale_y;

    status = _cairo_scaled_font_font_extents (scaled_font, extents);

    if (!CAIRO_OK (status))
      return status;
    
    _cairo_matrix_compute_scale_factors (&scaled_font->font_matrix,
					 &font_scale_x, &font_scale_y,
					 /* XXX */ 1);
    
    /* 
     * The font responded in unscaled units, scale by the font
     * matrix scale factors to get to user space
     */
    
    extents->ascent *= font_scale_y;
    extents->descent *= font_scale_y;
    extents->height *= font_scale_y;
    extents->max_x_advance *= font_scale_x;
    extents->max_y_advance *= font_scale_y;
      
    return status;
}

/**
 * cairo_font_glyph_extents:
 * @scaled_font: a #cairo_scaled_font_t
 * @glyphs: an array of glyph IDs with X and Y offsets.
 * @num_glyphs: the number of glyphs in the @glyphs array
 * @extents: a #cairo_text_extents_t which to store the retrieved extents.
 * 
 * cairo_font_glyph_extents() gets the overall metrics for a string of
 * glyphs. The X and Y offsets in @glyphs are taken from an origin of 0,0. 
 **/
void
cairo_scaled_font_glyph_extents (cairo_scaled_font_t   *scaled_font,
				 cairo_glyph_t         *glyphs, 
				 int                    num_glyphs,
				 cairo_text_extents_t  *extents)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_glyph_t origin_glyph;
    cairo_text_extents_t origin_extents;
    int i;
    double min_x = 0.0, min_y = 0.0, max_x = 0.0, max_y = 0.0;
    double x_pos = 0.0, y_pos = 0.0;
    int set = 0;

    if (!num_glyphs)
    {
	extents->x_bearing = 0.0;
	extents->y_bearing = 0.0;
	extents->width = 0.0;
	extents->height = 0.0;
	extents->x_advance = 0.0;
	extents->y_advance = 0.0;
	
	return;
    }

    for (i = 0; i < num_glyphs; i++)
    {
	double		x, y;
	double		wm, hm;
	
	origin_glyph = glyphs[i];
	origin_glyph.x = 0.0;
	origin_glyph.y = 0.0;
	status = _cairo_scaled_font_glyph_extents (scaled_font,
						   &origin_glyph, 1,
						   &origin_extents);
	
	/*
	 * Transform font space metrics into user space metrics
	 * by running the corners through the font matrix and
	 * expanding the bounding box as necessary
	 */
	x = origin_extents.x_bearing;
	y = origin_extents.y_bearing;
	cairo_matrix_transform_point (&scaled_font->font_matrix,
				      &x, &y);

	for (hm = 0.0; hm <= 1.0; hm += 1.0)
	    for (wm = 0.0; wm <= 1.0; wm += 1.0)
	    {
		x = origin_extents.x_bearing + origin_extents.width * wm;
		y = origin_extents.y_bearing + origin_extents.height * hm;
		cairo_matrix_transform_point (&scaled_font->font_matrix,
					      &x, &y);
		x += glyphs[i].x;
		y += glyphs[i].y;
		if (!set)
		{
		    min_x = max_x = x;
		    min_y = max_y = y;
		    set = 1;
		}
		else
		{
		    if (x < min_x) min_x = x;
		    if (x > max_x) max_x = x;
		    if (y < min_y) min_y = y;
		    if (y > max_y) max_y = y;
		}
	    }

	x = origin_extents.x_advance;
	y = origin_extents.y_advance;
	cairo_matrix_transform_point (&scaled_font->font_matrix,
				      &x, &y);
	x_pos = glyphs[i].x + x;
	y_pos = glyphs[i].y + y;
    }

    extents->x_bearing = min_x - glyphs[0].x;
    extents->y_bearing = min_y - glyphs[0].y;
    extents->width = max_x - min_x;
    extents->height = max_y - min_y;
    extents->x_advance = x_pos - glyphs[0].x;
    extents->y_advance = y_pos - glyphs[0].y;
}

/* Now we implement functions to access a default global image & metrics
 * cache. 
 */

unsigned long
_cairo_glyph_cache_hash (void *cache, void *key)
{
    cairo_glyph_cache_key_t *in;
    in = (cairo_glyph_cache_key_t *) key;
    return 
	((unsigned long) in->unscaled) 
	^ ((unsigned long) in->scale.xx) 
	^ ((unsigned long) in->scale.yx) 
	^ ((unsigned long) in->scale.xy) 
	^ ((unsigned long) in->scale.yy)
        ^ (in->flags * 1451) /* 1451 is just an abitrary prime */
	^ in->index;
}

int
_cairo_glyph_cache_keys_equal (void *cache,
			       void *k1,
			       void *k2)
{
    cairo_glyph_cache_key_t *a, *b;
    a = (cairo_glyph_cache_key_t *) k1;
    b = (cairo_glyph_cache_key_t *) k2;
    return (a->index == b->index)
	&& (a->unscaled == b->unscaled)
	&& (a->flags == b->flags)
	&& (a->scale.xx == b->scale.xx)
	&& (a->scale.yx == b->scale.yx)
	&& (a->scale.xy == b->scale.xy)
	&& (a->scale.yy == b->scale.yy);
}


static cairo_status_t
_image_glyph_cache_create_entry (void *cache,
				 void *key,
				 void **return_value)
{
    cairo_glyph_cache_key_t *k = (cairo_glyph_cache_key_t *) key;
    cairo_image_glyph_cache_entry_t *im;
    cairo_status_t status;

    im = calloc (1, sizeof (cairo_image_glyph_cache_entry_t));
    if (im == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    im->key = *k;    
    status = im->key.unscaled->backend->create_glyph (im->key.unscaled,
						      im);

    if (status != CAIRO_STATUS_SUCCESS) {
	free (im);
	return status;
    }

    _cairo_unscaled_font_reference (im->key.unscaled);

    im->key.base.memory = 
	sizeof (cairo_image_glyph_cache_entry_t) 
	+ (im->image ? 
	   sizeof (cairo_image_surface_t) 
	   + 28 * sizeof (int) /* rough guess at size of pixman image structure */
	   + (im->image->height * im->image->stride) : 0);

    *return_value = im;

    return CAIRO_STATUS_SUCCESS;
}


static void
_image_glyph_cache_destroy_entry (void *cache,
				  void *value)
{
    cairo_image_glyph_cache_entry_t *im;

    im = (cairo_image_glyph_cache_entry_t *) value;
    _cairo_unscaled_font_destroy (im->key.unscaled);
    cairo_surface_destroy (&(im->image->base));
    free (im); 
}

static void 
_image_glyph_cache_destroy_cache (void *cache)
{
    free (cache);
}

static const cairo_cache_backend_t cairo_image_cache_backend = {
    _cairo_glyph_cache_hash,
    _cairo_glyph_cache_keys_equal,
    _image_glyph_cache_create_entry,
    _image_glyph_cache_destroy_entry,
    _image_glyph_cache_destroy_cache
};

void
_cairo_lock_global_image_glyph_cache()
{
    /* FIXME: implement locking. */
}

void
_cairo_unlock_global_image_glyph_cache()
{
    /* FIXME: implement locking. */
}

static cairo_cache_t *
_global_image_glyph_cache = NULL;

cairo_cache_t *
_cairo_get_global_image_glyph_cache ()
{
    if (_global_image_glyph_cache == NULL) {
	_global_image_glyph_cache = malloc (sizeof (cairo_cache_t));
	
	if (_global_image_glyph_cache == NULL)
	    goto FAIL;
	
	if (_cairo_cache_init (_global_image_glyph_cache,
			       &cairo_image_cache_backend,
			       CAIRO_IMAGE_GLYPH_CACHE_MEMORY_DEFAULT))
	    goto FAIL;
    }

    return _global_image_glyph_cache;
    
 FAIL:
    if (_global_image_glyph_cache)
	free (_global_image_glyph_cache);
    _global_image_glyph_cache = NULL;
    return NULL;
}

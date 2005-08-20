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

/* Forward declare so we can use it as an arbitrary backend for
 * _cairo_font_face_nil.
 */
static const cairo_font_face_backend_t _cairo_toy_font_face_backend;

/* cairo_font_face_t */

const cairo_font_face_t _cairo_font_face_nil = {
    { 0 },			/* hash_entry */
    CAIRO_STATUS_NO_MEMORY,	/* status */
    -1,		                /* ref_count */
    { 0, 0, 0, NULL },		/* user_data */
    &_cairo_toy_font_face_backend
};

void
_cairo_font_face_init (cairo_font_face_t               *font_face, 
		       const cairo_font_face_backend_t *backend)
{
    font_face->status = CAIRO_STATUS_SUCCESS;
    font_face->ref_count = 1;
    font_face->backend = backend;

    _cairo_user_data_array_init (&font_face->user_data);
}

/**
 * cairo_font_face_reference:
 * @font_face: a #cairo_font_face_t, (may be NULL in which case this
 * function does nothing).
 * 
 * Increases the reference count on @font_face by one. This prevents
 * @font_face from being destroyed until a matching call to
 * cairo_font_face_destroy() is made.
 *
 * Return value: the referenced #cairo_font_face_t.
 **/
cairo_font_face_t *
cairo_font_face_reference (cairo_font_face_t *font_face)
{
    if (font_face == NULL)
	return NULL;

    if (font_face->ref_count == (unsigned int)-1)
	return font_face;

    font_face->ref_count++;

    return font_face;
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
    if (font_face == NULL)
	return;

    if (font_face->ref_count == (unsigned int)-1)
	return;

    if (--(font_face->ref_count) > 0)
	return;

    font_face->backend->destroy (font_face);

    /* We allow resurrection to deal with some memory management for the
     * FreeType backend where cairo_ft_font_face_t and cairo_ft_unscaled_font_t
     * need to effectively mutually reference each other
     */
    if (font_face->ref_count > 0)
	return;

    _cairo_user_data_array_fini (&font_face->user_data);

    free (font_face);
}

/**
 * cairo_font_face_status:
 * @font_face: a #cairo_font_face_t
 * 
 * Checks whether an error has previously occurred for this
 * font face
 * 
 * Return value: %CAIRO_STATUS_SUCCESS or another error such as
 *   %CAIRO_STATUS_NO_MEMORY.
 **/
cairo_status_t
cairo_font_face_status (cairo_font_face_t *font_face)
{
    return font_face->status;
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
    if (font_face->ref_count == -1)
	return CAIRO_STATUS_NO_MEMORY;
    
    return _cairo_user_data_array_set_data (&font_face->user_data,
					    key, user_data, destroy);
}

static const cairo_font_face_backend_t _cairo_toy_font_face_backend;

static int
_cairo_toy_font_face_keys_equal (void *key_a,
				 void *key_b);

/* We maintain a hash table from family/weight/slant =>
 * cairo_font_face_t for cairo_toy_font_t. The primary purpose of
 * this mapping is to provide unique cairo_font_face_t values so that
 * our cache and mapping from cairo_font_face_t => cairo_scaled_font_t
 * works. Once the corresponding cairo_font_face_t objects fall out of
 * downstream caches, we don't need them in this hash table anymore.
 */

static cairo_hash_table_t *cairo_toy_font_face_hash_table = NULL;

CAIRO_MUTEX_DECLARE (cairo_toy_font_face_hash_table_mutex);

static cairo_hash_table_t *
_cairo_toy_font_face_hash_table_lock (void)
{
    CAIRO_MUTEX_LOCK (cairo_toy_font_face_hash_table_mutex);

    if (cairo_toy_font_face_hash_table == NULL)
    {
	cairo_toy_font_face_hash_table =
	    _cairo_hash_table_create (_cairo_toy_font_face_keys_equal);

	if (cairo_toy_font_face_hash_table == NULL) {
	    CAIRO_MUTEX_UNLOCK (cairo_toy_font_face_hash_table_mutex);
	    return NULL;
	}
    }

    return cairo_toy_font_face_hash_table;
}

static void
_cairo_toy_font_face_hash_table_unlock (void)
{
    CAIRO_MUTEX_UNLOCK (cairo_toy_font_face_hash_table_mutex);
}

/**
 * _cairo_toy_font_face_init_key:
 * 
 * Initialize those portions of cairo_toy_font_face_t needed to use
 * it as a hash table key, including the hash code buried away in
 * font_face->base.hash_entry. No memory allocation is performed here
 * so that no fini call is needed. We do this to make it easier to use
 * an automatic cairo_toy_font_face_t variable as a key.
 **/
static void
_cairo_toy_font_face_init_key (cairo_toy_font_face_t *key,
			       const char	     *family,
			       cairo_font_slant_t     slant,
			       cairo_font_weight_t    weight)
{
    unsigned long hash;

    key->family = family;
    key->owns_family = FALSE;

    key->slant = slant;
    key->weight = weight;

    /* 1607 and 1451 are just a couple of arbitrary primes. */
    hash = _cairo_hash_string (family);
    hash += ((unsigned long) slant) * 1607;
    hash += ((unsigned long) weight) * 1451;
    
    key->base.hash_entry.hash = hash;
}

static cairo_status_t
_cairo_toy_font_face_init (cairo_toy_font_face_t *font_face,
			   const char	         *family,
			   cairo_font_slant_t	  slant,
			   cairo_font_weight_t	  weight)
{
    char *family_copy;

    family_copy = strdup (family);
    if (family_copy == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    _cairo_toy_font_face_init_key (font_face, family_copy,
				      slant, weight);
    font_face->owns_family = TRUE;

    _cairo_font_face_init (&font_face->base, &_cairo_toy_font_face_backend);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_toy_font_face_fini (cairo_toy_font_face_t *font_face)
{
    /* We assert here that we own font_face->family before casting
     * away the const qualifer. */
    assert (font_face->owns_family);
    free ((char*) font_face->family);
}

static int
_cairo_toy_font_face_keys_equal (void *key_a,
				 void *key_b)
{
    cairo_toy_font_face_t *face_a = key_a;
    cairo_toy_font_face_t *face_b = key_b;

    return (strcmp (face_a->family, face_b->family) == 0 &&
	    face_a->slant == face_b->slant &&
	    face_a->weight == face_b->weight);
}

/**
 * _cairo_toy_font_face_create:
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
cairo_font_face_t *
_cairo_toy_font_face_create (const char          *family, 
			     cairo_font_slant_t   slant, 
			     cairo_font_weight_t  weight)
{
    cairo_status_t status;
    cairo_toy_font_face_t key, *font_face;
    cairo_hash_table_t *hash_table;

    hash_table = _cairo_toy_font_face_hash_table_lock ();
    if (hash_table == NULL)
	goto UNWIND;

    _cairo_toy_font_face_init_key (&key, family, slant, weight);
    
    /* Return existing font_face if it exists in the hash table. */
    if (_cairo_hash_table_lookup (hash_table,
				  &key.base.hash_entry,
				  (cairo_hash_entry_t **) &font_face))
    {
	_cairo_toy_font_face_hash_table_unlock ();
	return cairo_font_face_reference (&font_face->base);
    }

    /* Otherwise create it and insert into hash table. */
    font_face = malloc (sizeof (cairo_toy_font_face_t));
    if (font_face == NULL)
	goto UNWIND_HASH_TABLE_LOCK;

    status = _cairo_toy_font_face_init (font_face, family, slant, weight);
    if (status)
	goto UNWIND_FONT_FACE_MALLOC;

    status = _cairo_hash_table_insert (hash_table, &font_face->base.hash_entry);
    if (status)
	goto UNWIND_FONT_FACE_INIT;

    _cairo_toy_font_face_hash_table_unlock ();

    return &font_face->base;

 UNWIND_FONT_FACE_INIT:
 UNWIND_FONT_FACE_MALLOC:
    free (font_face);
 UNWIND_HASH_TABLE_LOCK:
    _cairo_toy_font_face_hash_table_unlock ();
 UNWIND:
    return (cairo_font_face_t*) &_cairo_font_face_nil;
}

static void
_cairo_toy_font_face_destroy (void *abstract_face)
{
    cairo_toy_font_face_t *font_face = abstract_face;
    cairo_hash_table_t *hash_table;

    if (font_face == NULL)
	return;

    hash_table = _cairo_toy_font_face_hash_table_lock ();
    /* All created objects must have been mapped in the hash table. */
    assert (hash_table != NULL);

    _cairo_hash_table_remove (hash_table, &font_face->base.hash_entry);
    
    _cairo_toy_font_face_hash_table_unlock ();
    
    _cairo_toy_font_face_fini (font_face);
}

static cairo_status_t
_cairo_toy_font_face_scaled_font_create (void                *abstract_font_face,
					 const cairo_matrix_t       *font_matrix,
					 const cairo_matrix_t       *ctm,
					 const cairo_font_options_t *options,
					 cairo_scaled_font_t	   **scaled_font)
{
    cairo_toy_font_face_t *font_face = abstract_font_face;
    const cairo_scaled_font_backend_t * backend = CAIRO_SCALED_FONT_BACKEND_DEFAULT;

    return backend->create_toy (font_face,
				font_matrix, ctm, options, scaled_font);
}

static const cairo_font_face_backend_t _cairo_toy_font_face_backend = {
    _cairo_toy_font_face_destroy,
    _cairo_toy_font_face_scaled_font_create
};

/* cairo_scaled_font_t */

static const cairo_scaled_font_t _cairo_scaled_font_nil = {
    { 0 },			/* hash_entry */
    CAIRO_STATUS_NO_MEMORY,	/* status */
    -1,				/* ref_count */
    NULL,			/* font_face */
    { 1., 0., 0., 1., 0, 0},	/* font_matrix */
    { 1., 0., 0., 1., 0, 0},	/* ctm */
    { 1., 0., 0., 1., 0, 0},	/* scale */
    { CAIRO_ANTIALIAS_DEFAULT,	/* options */
      CAIRO_SUBPIXEL_ORDER_DEFAULT,
      CAIRO_HINT_STYLE_DEFAULT,
      CAIRO_HINT_METRICS_DEFAULT} ,
    CAIRO_SCALED_FONT_BACKEND_DEFAULT,
};

/**
 * _cairo_scaled_font_set_error:
 * @scaled_font: a scaled_font
 * @status: a status value indicating an error, (eg. not
 * CAIRO_STATUS_SUCCESS)
 * 
 * Sets scaled_font->status to @status and calls _cairo_error;
 *
 * All assignments of an error status to scaled_font->status should happen
 * through _cairo_scaled_font_set_error() or else _cairo_error() should be
 * called immediately after the assignment.
 *
 * The purpose of this function is to allow the user to set a
 * breakpoint in _cairo_error() to generate a stack trace for when the
 * user causes cairo to detect an error.
 **/
void
_cairo_scaled_font_set_error (cairo_scaled_font_t *scaled_font,
			      cairo_status_t status)
{
    scaled_font->status = status;

    _cairo_error (status);
}

/**
 * cairo_scaled_font_status:
 * @scaled_font: a #cairo_scaled_font_t
 * 
 * Checks whether an error has previously occurred for this
 * scaled_font.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS or another error such as
 *   %CAIRO_STATUS_NO_MEMORY.
 **/
cairo_status_t
cairo_scaled_font_status (cairo_scaled_font_t *scaled_font)
{
    return scaled_font->status;
}

/* Here we keep a unique mapping from
 * cairo_font_face_t/matrix/ctm/options => cairo_scaled_font_t.
 *
 * Here are the things that we want to map:
 *
 *  a) All otherwise referenced cairo_scaled_font_t's
 *  b) Some number of not otherwise referenced cairo_scaled_font_t's
 *
 * The implementation uses a hash table which covers (a)
 * completely. Then, for (b) we have an array of otherwise
 * unreferenced fonts (holdovers) which are expired in
 * least-recently-used order.
 *
 * The cairo_scaled_font_create code gets to treat this like a regular
 * hash table. All of the magic for the little holdover cache is in
 * cairo_scaled_font_reference and cairo_scaled_font_destroy.
 */

/* This defines the size of the holdover array ... that is, the number
 * of scaled fonts we keep around even when not otherwise referenced
 */
#define CAIRO_SCALED_FONT_MAX_HOLDOVERS 24
 
typedef struct _cairo_scaled_font_map {
    cairo_hash_table_t *hash_table;
    cairo_scaled_font_t *holdovers[CAIRO_SCALED_FONT_MAX_HOLDOVERS];
    int num_holdovers;
} cairo_scaled_font_map_t;

static cairo_scaled_font_map_t *cairo_scaled_font_map = NULL;

CAIRO_MUTEX_DECLARE (cairo_scaled_font_map_mutex);

static int
_cairo_scaled_font_keys_equal (void *abstract_key_a, void *abstract_key_b);

static cairo_scaled_font_map_t *
_cairo_scaled_font_map_lock (void)
{
    CAIRO_MUTEX_LOCK (cairo_scaled_font_map_mutex);

    if (cairo_scaled_font_map == NULL)
    {
	cairo_scaled_font_map = malloc (sizeof (cairo_scaled_font_map_t));
	if (cairo_scaled_font_map == NULL)
	    goto CLEANUP_MUTEX_LOCK;

	cairo_scaled_font_map->hash_table =
	    _cairo_hash_table_create (_cairo_scaled_font_keys_equal);

	if (cairo_scaled_font_map->hash_table == NULL)
	    goto CLEANUP_SCALED_FONT_MAP;

	cairo_scaled_font_map->num_holdovers = 0;
    }

    return cairo_scaled_font_map;

 CLEANUP_SCALED_FONT_MAP:
    free (cairo_scaled_font_map);
 CLEANUP_MUTEX_LOCK:
    CAIRO_MUTEX_UNLOCK (cairo_scaled_font_map_mutex);
    return NULL;
}

static void
_cairo_scaled_font_map_unlock (void)
{
   CAIRO_MUTEX_UNLOCK (cairo_scaled_font_map_mutex);
}

static void
_cairo_scaled_font_map_destroy (void)
{
    int i;
    cairo_scaled_font_map_t *font_map = cairo_scaled_font_map;
    cairo_scaled_font_t *scaled_font;

    if (font_map == NULL)
	return;

    CAIRO_MUTEX_UNLOCK (cairo_scaled_font_map_mutex);

    for (i = 0; i < font_map->num_holdovers; i++) {
	scaled_font = font_map->holdovers[i];
	/* We should only get here through the reset_static_data path
	 * and there had better not be any active references at that
	 * point. */
	assert (scaled_font->ref_count == 0);
	_cairo_hash_table_remove (font_map->hash_table,
				  &scaled_font->hash_entry);
	_cairo_scaled_font_fini (scaled_font);
	free (scaled_font);
    }

    _cairo_hash_table_destroy (font_map->hash_table);

    free (cairo_scaled_font_map);
    cairo_scaled_font_map = NULL;
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

static void
_cairo_scaled_font_init_key (cairo_scaled_font_t        *scaled_font,
			     cairo_font_face_t	        *font_face,
			     const cairo_matrix_t       *font_matrix,
			     const cairo_matrix_t       *ctm,
			     const cairo_font_options_t *options)
{
    uint32_t hash = FNV1_32_INIT;

    scaled_font->status = CAIRO_STATUS_SUCCESS;
    scaled_font->font_face = font_face;
    scaled_font->font_matrix = *font_matrix;
    scaled_font->ctm = *ctm;
    scaled_font->options = *options;

    /* We do a bytewise hash on the font matrices, ignoring the
     * translation values. */
    hash = _hash_bytes_fnv ((unsigned char *)(&scaled_font->font_matrix.xx),
			    sizeof(double) * 4,
			    hash);
    hash = _hash_bytes_fnv ((unsigned char *)(&scaled_font->ctm.xx),
			    sizeof(double) * 4,
			    hash);

    hash ^= (unsigned long) scaled_font->font_face;

    hash ^= cairo_font_options_hash (&scaled_font->options);

    scaled_font->hash_entry.hash = hash;
}

static cairo_bool_t
_cairo_scaled_font_keys_equal (void *abstract_key_a, void *abstract_key_b)
{
    cairo_scaled_font_t *key_a = abstract_key_a;
    cairo_scaled_font_t *key_b = abstract_key_b;

    return (key_a->font_face == key_b->font_face &&
	    memcmp ((unsigned char *)(&key_a->font_matrix.xx),
		    (unsigned char *)(&key_b->font_matrix.xx),
		    sizeof(double) * 4) == 0 &&
	    memcmp ((unsigned char *)(&key_a->ctm.xx),
		    (unsigned char *)(&key_b->ctm.xx),
		    sizeof(double) * 4) == 0 &&
	    cairo_font_options_equal (&key_a->options, &key_b->options));
}

void
_cairo_scaled_font_init (cairo_scaled_font_t               *scaled_font, 
			 cairo_font_face_t		   *font_face,
			 const cairo_matrix_t              *font_matrix,
			 const cairo_matrix_t              *ctm,
			 const cairo_font_options_t	   *options,
			 const cairo_scaled_font_backend_t *backend)
{
    scaled_font->ref_count = 1;

    _cairo_scaled_font_init_key (scaled_font, font_face,
				 font_matrix, ctm, options);

    cairo_font_face_reference (font_face);

    cairo_matrix_multiply (&scaled_font->scale,
			   &scaled_font->font_matrix,
			   &scaled_font->ctm);

    scaled_font->backend = backend;
}

void
_cairo_scaled_font_fini (cairo_scaled_font_t *scaled_font)
{
    if (scaled_font->font_face)
	cairo_font_face_destroy (scaled_font->font_face);

    scaled_font->backend->fini (scaled_font);
}

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
 * @options: options to use when getting metrics for the font and
 *           rendering with it.
 * 
 * Creates a #cairo_scaled_font_t object from a font face and matrices that
 * describe the size of the font and the environment in which it will
 * be used.
 * 
 * Return value: a newly created #cairo_scaled_font_t. Destroy with
 *  cairo_scaled_font_destroy()
 **/
cairo_scaled_font_t *
cairo_scaled_font_create (cairo_font_face_t          *font_face,
			  const cairo_matrix_t       *font_matrix,
			  const cairo_matrix_t       *ctm,
			  const cairo_font_options_t *options)
{
    cairo_status_t status;
    cairo_scaled_font_map_t *font_map;
    cairo_scaled_font_t key, *scaled_font = NULL;

    font_map = _cairo_scaled_font_map_lock ();
    if (font_map == NULL)
	goto UNWIND;
    
    _cairo_scaled_font_init_key (&key, font_face,
				 font_matrix, ctm, options);

    /* Return existing scaled_font if it exists in the hash table. */
    if (_cairo_hash_table_lookup (font_map->hash_table, &key.hash_entry,
				  (cairo_hash_entry_t**) &scaled_font))
    {
	_cairo_scaled_font_map_unlock ();
	return cairo_scaled_font_reference (scaled_font);
    }

    /* Otherwise create it and insert it into the hash table. */
    status = font_face->backend->scaled_font_create (font_face, font_matrix,
						     ctm, options, &scaled_font);
    if (status)
	goto UNWIND_FONT_MAP_LOCK;

    status = _cairo_hash_table_insert (font_map->hash_table,
				       &scaled_font->hash_entry);
    if (status)
	goto UNWIND_SCALED_FONT_CREATE;

    _cairo_scaled_font_map_unlock ();

    return scaled_font;

UNWIND_SCALED_FONT_CREATE:
    /* We can't call _cairo_scaled_font_destroy here since it expects
     * that the font has already been successfully inserted into the
     * hash table. */
    _cairo_scaled_font_fini (scaled_font);
    free (scaled_font);
UNWIND_FONT_MAP_LOCK:
    _cairo_scaled_font_map_unlock ();
UNWIND:
    return NULL;
}

/**
 * cairo_scaled_font_reference:
 * @scaled_font: a #cairo_scaled_font_t, (may be NULL in which case
 * this function does nothing)
 * 
 * Increases the reference count on @scaled_font by one. This prevents
 * @scaled_font from being destroyed until a matching call to
 * cairo_scaled_font_destroy() is made.
 **/
cairo_scaled_font_t *
cairo_scaled_font_reference (cairo_scaled_font_t *scaled_font)
{
    if (scaled_font == NULL)
	return NULL;

    if (scaled_font->ref_count == (unsigned int)-1)
	return scaled_font;

    /* If the original reference count is 0, then this font must have
     * been found in font_map->holdovers, (which means this caching is
     * actually working). So now we remove it from the holdovers
     * array. */
    if (scaled_font->ref_count == 0) {
	cairo_scaled_font_map_t *font_map;
	int i;

	font_map = _cairo_scaled_font_map_lock ();
	{
	    for (i = 0; i < font_map->num_holdovers; i++)
		if (font_map->holdovers[i] == scaled_font)
		    break;
	    assert (i < font_map->num_holdovers);

	    font_map->num_holdovers--;
	    memmove (&font_map->holdovers[i],
		     &font_map->holdovers[i+1],
		     (font_map->num_holdovers - i) * sizeof (cairo_scaled_font_t*));
	}
	_cairo_scaled_font_map_unlock ();
    }

    scaled_font->ref_count++;

    return scaled_font;
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
    cairo_scaled_font_map_t *font_map;

    if (scaled_font == NULL)
	return;

    if (scaled_font->ref_count == (unsigned int)-1)
	return;

    if (--(scaled_font->ref_count) > 0)
	return;

    font_map = _cairo_scaled_font_map_lock ();
    assert (font_map != NULL);
    {
	/* Rather than immediately destroying this object, we put it into
	 * the font_map->holdovers array in case it will get used again
	 * soon. To make room for it, we do actually destroy the
	 * least-recently-used holdover.
	 */
	if (font_map->num_holdovers == CAIRO_SCALED_FONT_MAX_HOLDOVERS) {
	    cairo_scaled_font_t *lru;

	    lru = font_map->holdovers[0];
	    assert (lru->ref_count == 0);
	
	    _cairo_hash_table_remove (font_map->hash_table, &lru->hash_entry);

	    _cairo_scaled_font_fini (lru);
	    free (lru);
	
	    font_map->num_holdovers--;
	    memmove (&font_map->holdovers[0],
		     &font_map->holdovers[1],
		     font_map->num_holdovers * sizeof (cairo_scaled_font_t*));
	}

	font_map->holdovers[font_map->num_holdovers] = scaled_font;
	font_map->num_holdovers++;
    }
    _cairo_scaled_font_map_unlock ();
}

cairo_status_t
_cairo_scaled_font_text_to_glyphs (cairo_scaled_font_t *scaled_font,
				   const char          *utf8, 
				   cairo_glyph_t      **glyphs, 
				   int 		       *num_glyphs)
{
    if (scaled_font->status)
	return scaled_font->status;

    return scaled_font->backend->text_to_glyphs (scaled_font, utf8, glyphs, num_glyphs);
}

cairo_status_t
_cairo_scaled_font_glyph_extents (cairo_scaled_font_t   *scaled_font,
				  cairo_glyph_t 	*glyphs,
				  int 			 num_glyphs,
				  cairo_text_extents_t  *extents)
{
    if (scaled_font->status)
	return scaled_font->status;

    return scaled_font->backend->glyph_extents (scaled_font, glyphs, num_glyphs, extents);
}


cairo_status_t
_cairo_scaled_font_glyph_bbox (cairo_scaled_font_t *scaled_font,
			       cairo_glyph_t       *glyphs,
			       int                  num_glyphs,
			       cairo_box_t	   *bbox)
{
    if (scaled_font->status)
	return scaled_font->status;

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

    if (scaled_font->status)
	return scaled_font->status;

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
    if (scaled_font->status)
	return scaled_font->status;

    return scaled_font->backend->glyph_path (scaled_font, glyphs, num_glyphs, path);
}

cairo_status_t
_cairo_scaled_font_get_glyph_cache_key (cairo_scaled_font_t     *scaled_font,
					cairo_glyph_cache_key_t *key)
{
    if (scaled_font->status)
	return scaled_font->status;

    scaled_font->backend->get_glyph_cache_key (scaled_font, key);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_scaled_font_font_extents (cairo_scaled_font_t  *scaled_font,
				 cairo_font_extents_t *extents)
{
    if (scaled_font->status)
	return scaled_font->status;

    return scaled_font->backend->font_extents (scaled_font, extents);
}

void
_cairo_unscaled_font_init (cairo_unscaled_font_t               *unscaled_font, 
			   const cairo_unscaled_font_backend_t *backend)
{
    unscaled_font->ref_count = 1;
    unscaled_font->backend = backend;
}

cairo_unscaled_font_t *
_cairo_unscaled_font_reference (cairo_unscaled_font_t *unscaled_font)
{
    if (unscaled_font == NULL)
	return NULL;

    unscaled_font->ref_count++;

    return unscaled_font;
}

void
_cairo_unscaled_font_destroy (cairo_unscaled_font_t *unscaled_font)
{    
    if (unscaled_font == NULL)
	return;

    if (--(unscaled_font->ref_count) > 0)
	return;

    unscaled_font->backend->destroy (unscaled_font);

    free (unscaled_font);
}

/* Public font API follows. */

/**
 * cairo_scaled_font_extents:
 * @scaled_font: a #cairo_scaled_font_t
 * @extents: a #cairo_font_extents_t which to store the retrieved extents.
 * 
 * Gets the metrics for a #cairo_scaled_font_t. 
 **/
void
cairo_scaled_font_extents (cairo_scaled_font_t  *scaled_font,
			   cairo_font_extents_t *extents)
{
    cairo_int_status_t status;
    double  font_scale_x, font_scale_y;
    
    if (scaled_font->status) {
	_cairo_scaled_font_set_error (scaled_font, scaled_font->status);
	return;
    }

    status = _cairo_scaled_font_font_extents (scaled_font, extents);
    if (status) {
	_cairo_scaled_font_set_error (scaled_font, status);
	return;
    }
    
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

    if (scaled_font->status) {
	_cairo_scaled_font_set_error (scaled_font, scaled_font->status);
	return;
    }

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

CAIRO_MUTEX_DECLARE(_global_image_glyph_cache_mutex);

static cairo_cache_t *
_global_image_glyph_cache = NULL;

void
_cairo_lock_global_image_glyph_cache()
{
    CAIRO_MUTEX_LOCK (_global_image_glyph_cache_mutex);
}

void
_cairo_unlock_global_image_glyph_cache()
{
    if (_global_image_glyph_cache) {
	_cairo_cache_shrink_to (_global_image_glyph_cache, 
				CAIRO_IMAGE_GLYPH_CACHE_MEMORY_DEFAULT);
    }
    CAIRO_MUTEX_UNLOCK (_global_image_glyph_cache_mutex);
}

cairo_cache_t *
_cairo_get_global_image_glyph_cache ()
{
    if (_global_image_glyph_cache == NULL) {
	_global_image_glyph_cache = malloc (sizeof (cairo_cache_t));
	
	if (_global_image_glyph_cache == NULL)
	    goto FAIL;
	
	if (_cairo_cache_init (_global_image_glyph_cache,
			       &cairo_image_cache_backend,
			       0))
	    goto FAIL;
    }

    return _global_image_glyph_cache;
    
 FAIL:
    if (_global_image_glyph_cache)
	free (_global_image_glyph_cache);
    _global_image_glyph_cache = NULL;
    return NULL;
}

void
_cairo_font_reset_static_data (void)
{
    _cairo_scaled_font_map_destroy ();

    _cairo_lock_global_image_glyph_cache();
    _cairo_cache_destroy (_global_image_glyph_cache);
    _global_image_glyph_cache = NULL;
    _cairo_unlock_global_image_glyph_cache();

    CAIRO_MUTEX_LOCK (cairo_toy_font_face_hash_table_mutex);
    _cairo_hash_table_destroy (cairo_toy_font_face_hash_table);
    cairo_toy_font_face_hash_table = NULL;
    CAIRO_MUTEX_UNLOCK (cairo_toy_font_face_hash_table_mutex);
}

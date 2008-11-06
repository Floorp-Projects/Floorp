/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
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

#define _BSD_SOURCE /* for strdup() */
#include "cairoint.h"

static const cairo_font_face_backend_t _cairo_toy_font_face_backend;

/* #cairo_font_face_t */

const cairo_toy_font_face_t _cairo_font_face_nil = {
    {
    { 0 },				/* hash_entry */
    CAIRO_STATUS_NO_MEMORY,		/* status */
    CAIRO_REFERENCE_COUNT_INVALID,	/* ref_count */
    { 0, 0, 0, NULL },			/* user_data */
    &_cairo_toy_font_face_backend
    },
    CAIRO_FONT_FAMILY_DEFAULT,		/* family */
    TRUE,				/* owns_family */
    CAIRO_FONT_SLANT_DEFAULT,		/* slant */
    CAIRO_FONT_WEIGHT_DEFAULT		/* weight */
};

static const cairo_toy_font_face_t _cairo_font_face_null_pointer = {
    {
    { 0 },				/* hash_entry */
    CAIRO_STATUS_NULL_POINTER,		/* status */
    CAIRO_REFERENCE_COUNT_INVALID,	/* ref_count */
    { 0, 0, 0, NULL },			/* user_data */
    &_cairo_toy_font_face_backend
    },
    CAIRO_FONT_FAMILY_DEFAULT,		/* family */
    TRUE,				/* owns_family */
    CAIRO_FONT_SLANT_DEFAULT,		/* slant */
    CAIRO_FONT_WEIGHT_DEFAULT		/* weight */
};

static const cairo_toy_font_face_t _cairo_font_face_invalid_string = {
    {
    { 0 },				/* hash_entry */
    CAIRO_STATUS_INVALID_STRING,	/* status */
    CAIRO_REFERENCE_COUNT_INVALID,	/* ref_count */
    { 0, 0, 0, NULL },			/* user_data */
    &_cairo_toy_font_face_backend
    },
    CAIRO_FONT_FAMILY_DEFAULT,		/* family */
    TRUE,				/* owns_family */
    CAIRO_FONT_SLANT_DEFAULT,		/* slant */
    CAIRO_FONT_WEIGHT_DEFAULT		/* weight */
};

static const cairo_toy_font_face_t _cairo_font_face_invalid_slant = {
    {
    { 0 },				/* hash_entry */
    CAIRO_STATUS_INVALID_SLANT,		/* status */
    CAIRO_REFERENCE_COUNT_INVALID,	/* ref_count */
    { 0, 0, 0, NULL },			/* user_data */
    &_cairo_toy_font_face_backend
    },
    CAIRO_FONT_FAMILY_DEFAULT,		/* family */
    TRUE,				/* owns_family */
    CAIRO_FONT_SLANT_DEFAULT,		/* slant */
    CAIRO_FONT_WEIGHT_DEFAULT		/* weight */
};

static const cairo_toy_font_face_t _cairo_font_face_invalid_weight = {
    {
    { 0 },				/* hash_entry */
    CAIRO_STATUS_INVALID_WEIGHT,	/* status */
    CAIRO_REFERENCE_COUNT_INVALID,	/* ref_count */
    { 0, 0, 0, NULL },			/* user_data */
    &_cairo_toy_font_face_backend
    },
    CAIRO_FONT_FAMILY_DEFAULT,		/* family */
    TRUE,				/* owns_family */
    CAIRO_FONT_SLANT_DEFAULT,		/* slant */
    CAIRO_FONT_WEIGHT_DEFAULT		/* weight */
};

cairo_status_t
_cairo_font_face_set_error (cairo_font_face_t *font_face,
	                    cairo_status_t     status)
{
    if (status == CAIRO_STATUS_SUCCESS)
	return status;

    /* Don't overwrite an existing error. This preserves the first
     * error, which is the most significant. */
    _cairo_status_set_error (&font_face->status, status);

    return _cairo_error (status);
}

void
_cairo_font_face_init (cairo_font_face_t               *font_face,
		       const cairo_font_face_backend_t *backend)
{
    CAIRO_MUTEX_INITIALIZE ();

    font_face->status = CAIRO_STATUS_SUCCESS;
    CAIRO_REFERENCE_COUNT_INIT (&font_face->ref_count, 1);
    font_face->backend = backend;

    _cairo_user_data_array_init (&font_face->user_data);
}

/**
 * cairo_font_face_reference:
 * @font_face: a #cairo_font_face_t, (may be %NULL in which case this
 * function does nothing).
 *
 * Increases the reference count on @font_face by one. This prevents
 * @font_face from being destroyed until a matching call to
 * cairo_font_face_destroy() is made.
 *
 * The number of references to a #cairo_font_face_t can be get using
 * cairo_font_face_get_reference_count().
 *
 * Return value: the referenced #cairo_font_face_t.
 **/
cairo_font_face_t *
cairo_font_face_reference (cairo_font_face_t *font_face)
{
    if (font_face == NULL ||
	    CAIRO_REFERENCE_COUNT_IS_INVALID (&font_face->ref_count))
	return font_face;

    /* We would normally assert that we have a reference here but we
     * can't get away with that due to the zombie case as documented
     * in _cairo_ft_font_face_destroy. */

    _cairo_reference_count_inc (&font_face->ref_count);

    return font_face;
}
slim_hidden_def (cairo_font_face_reference);

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
    if (font_face == NULL ||
	    CAIRO_REFERENCE_COUNT_IS_INVALID (&font_face->ref_count))
	return;

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&font_face->ref_count));

    if (! _cairo_reference_count_dec_and_test (&font_face->ref_count))
	return;

    if (font_face->backend->destroy)
	font_face->backend->destroy (font_face);

    /* We allow resurrection to deal with some memory management for the
     * FreeType backend where cairo_ft_font_face_t and cairo_ft_unscaled_font_t
     * need to effectively mutually reference each other
     */
    if (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&font_face->ref_count))
	return;

    _cairo_user_data_array_fini (&font_face->user_data);

    free (font_face);
}
slim_hidden_def (cairo_font_face_destroy);

/**
 * cairo_font_face_get_type:
 * @font_face: a font face
 *
 * This function returns the type of the backend used to create
 * a font face. See #cairo_font_type_t for available types.
 *
 * Return value: The type of @font_face.
 *
 * Since: 1.2
 **/
cairo_font_type_t
cairo_font_face_get_type (cairo_font_face_t *font_face)
{
    return font_face->backend->type;
}

/**
 * cairo_font_face_get_reference_count:
 * @font_face: a #cairo_font_face_t
 *
 * Returns the current reference count of @font_face.
 *
 * Return value: the current reference count of @font_face.  If the
 * object is a nil object, 0 will be returned.
 *
 * Since: 1.4
 **/
unsigned int
cairo_font_face_get_reference_count (cairo_font_face_t *font_face)
{
    if (font_face == NULL ||
	    CAIRO_REFERENCE_COUNT_IS_INVALID (&font_face->ref_count))
	return 0;

    return CAIRO_REFERENCE_COUNT_GET_VALUE (&font_face->ref_count);
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
slim_hidden_def (cairo_font_face_get_user_data);

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
    if (CAIRO_REFERENCE_COUNT_IS_INVALID (&font_face->ref_count))
	return font_face->status;

    return _cairo_user_data_array_set_data (&font_face->user_data,
					    key, user_data, destroy);
}
slim_hidden_def (cairo_font_face_set_user_data);

static const cairo_font_face_backend_t _cairo_toy_font_face_backend;

static int
_cairo_toy_font_face_keys_equal (const void *key_a,
				 const void *key_b);

/* We maintain a hash table from family/weight/slant =>
 * #cairo_font_face_t for #cairo_toy_font_t. The primary purpose of
 * this mapping is to provide unique #cairo_font_face_t values so that
 * our cache and mapping from #cairo_font_face_t => #cairo_scaled_font_t
 * works. Once the corresponding #cairo_font_face_t objects fall out of
 * downstream caches, we don't need them in this hash table anymore.
 *
 * Modifications to this hash table are protected by
 * _cairo_font_face_mutex.
 */
static cairo_hash_table_t *cairo_toy_font_face_hash_table = NULL;

static cairo_hash_table_t *
_cairo_toy_font_face_hash_table_lock (void)
{
    CAIRO_MUTEX_LOCK (_cairo_font_face_mutex);

    if (cairo_toy_font_face_hash_table == NULL)
    {
	cairo_toy_font_face_hash_table =
	    _cairo_hash_table_create (_cairo_toy_font_face_keys_equal);

	if (cairo_toy_font_face_hash_table == NULL) {
	    CAIRO_MUTEX_UNLOCK (_cairo_font_face_mutex);
	    return NULL;
	}
    }

    return cairo_toy_font_face_hash_table;
}

static void
_cairo_toy_font_face_hash_table_unlock (void)
{
    CAIRO_MUTEX_UNLOCK (_cairo_font_face_mutex);
}

/**
 * _cairo_toy_font_face_init_key:
 *
 * Initialize those portions of #cairo_toy_font_face_t needed to use
 * it as a hash table key, including the hash code buried away in
 * font_face->base.hash_entry. No memory allocation is performed here
 * so that no fini call is needed. We do this to make it easier to use
 * an automatic #cairo_toy_font_face_t variable as a key.
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

    assert (hash != 0);
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
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

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
_cairo_toy_font_face_keys_equal (const void *key_a,
				 const void *key_b)
{
    const cairo_toy_font_face_t *face_a = key_a;
    const cairo_toy_font_face_t *face_b = key_b;

    return (strcmp (face_a->family, face_b->family) == 0 &&
	    face_a->slant == face_b->slant &&
	    face_a->weight == face_b->weight);
}

/**
 * cairo_toy_font_face_create:
 * @family: a font family name, encoded in UTF-8
 * @slant: the slant for the font
 * @weight: the weight for the font
 *
 * Creates a font face from a triplet of family, slant, and weight.
 * These font faces are used in implementation of the the #cairo_t "toy"
 * font API.
 *
 * If @family is the zero-length string "", the platform-specific default
 * family is assumed.  The default family then can be queried using
 * cairo_toy_font_face_get_family().
 *
 * The cairo_select_font_face() function uses this to create font faces.
 * See that function for limitations of toy font faces.
 *
 * Return value: a newly created #cairo_font_face_t. Free with
 *  cairo_font_face_destroy() when you are done using it.
 *
 * Since: 1.8
 **/
cairo_font_face_t *
cairo_toy_font_face_create (const char          *family,
			    cairo_font_slant_t   slant,
			    cairo_font_weight_t  weight)
{
    cairo_status_t status;
    cairo_toy_font_face_t key, *font_face;
    cairo_hash_table_t *hash_table;

    if (family == NULL)
	return (cairo_font_face_t*) &_cairo_font_face_null_pointer;

    /* Make sure we've got valid UTF-8 for the family */
    status = _cairo_utf8_to_ucs4 (family, -1, NULL, NULL);
    if (status == CAIRO_STATUS_INVALID_STRING)
	return (cairo_font_face_t*) &_cairo_font_face_invalid_string;
    else if (status)
	return (cairo_font_face_t*) &_cairo_font_face_nil;

    switch (slant) {
	case CAIRO_FONT_SLANT_NORMAL:
	case CAIRO_FONT_SLANT_ITALIC:
	case CAIRO_FONT_SLANT_OBLIQUE:
	    break;
	default:
	    return (cairo_font_face_t*) &_cairo_font_face_invalid_slant;
    }

    switch (weight) {
	case CAIRO_FONT_WEIGHT_NORMAL:
	case CAIRO_FONT_WEIGHT_BOLD:
	    break;
	default:
	    return (cairo_font_face_t*) &_cairo_font_face_invalid_weight;
    }

    if (*family == '\0')
	family = CAIRO_FONT_FAMILY_DEFAULT;

    hash_table = _cairo_toy_font_face_hash_table_lock ();
    if (hash_table == NULL)
	goto UNWIND;

    _cairo_toy_font_face_init_key (&key, family, slant, weight);

    /* Return existing font_face if it exists in the hash table. */
    if (_cairo_hash_table_lookup (hash_table,
				  &key.base.hash_entry,
				  (cairo_hash_entry_t **) &font_face))
    {
	if (! font_face->base.status)  {
	    /* We increment the reference count here manually to avoid
	       double-locking. */
	    _cairo_reference_count_inc (&font_face->base.ref_count);
	    _cairo_toy_font_face_hash_table_unlock ();
	    return &font_face->base;
	}

	/* remove the bad font from the hash table */
	_cairo_hash_table_remove (hash_table, &key.base.hash_entry);
	font_face->base.hash_entry.hash = 0;
    }

    /* Otherwise create it and insert into hash table. */
    font_face = malloc (sizeof (cairo_toy_font_face_t));
    if (font_face == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto UNWIND_HASH_TABLE_LOCK;
    }

    status = _cairo_toy_font_face_init (font_face, family, slant, weight);
    if (status)
	goto UNWIND_FONT_FACE_MALLOC;

    status = _cairo_hash_table_insert (hash_table, &font_face->base.hash_entry);
    if (status)
	goto UNWIND_FONT_FACE_INIT;

    _cairo_toy_font_face_hash_table_unlock ();

    return &font_face->base;

 UNWIND_FONT_FACE_INIT:
    _cairo_toy_font_face_fini (font_face);
 UNWIND_FONT_FACE_MALLOC:
    free (font_face);
 UNWIND_HASH_TABLE_LOCK:
    _cairo_toy_font_face_hash_table_unlock ();
 UNWIND:
    return (cairo_font_face_t*) &_cairo_font_face_nil;
}
slim_hidden_def (cairo_toy_font_face_create);

static void
_cairo_toy_font_face_destroy (void *abstract_face)
{
    cairo_toy_font_face_t *font_face = abstract_face;
    cairo_hash_table_t *hash_table;

    if (font_face == NULL ||
	    CAIRO_REFERENCE_COUNT_IS_INVALID (&font_face->base.ref_count))
	return;

    hash_table = _cairo_toy_font_face_hash_table_lock ();
    /* All created objects must have been mapped in the hash table. */
    assert (hash_table != NULL);

    if (font_face->base.hash_entry.hash != 0)
	_cairo_hash_table_remove (hash_table, &font_face->base.hash_entry);

    _cairo_toy_font_face_hash_table_unlock ();

    _cairo_toy_font_face_fini (font_face);
}

static cairo_status_t
_cairo_toy_font_face_scaled_font_get_implementation (void                *abstract_font_face,
						     cairo_font_face_t **font_face_out)
{
    cairo_toy_font_face_t *font_face = abstract_font_face;
    cairo_status_t status;

    if (font_face->base.status)
	return font_face->base.status;

    if (CAIRO_SCALED_FONT_BACKEND_DEFAULT != &_cairo_user_scaled_font_backend)
    {
	const cairo_scaled_font_backend_t * backend = CAIRO_SCALED_FONT_BACKEND_DEFAULT;

	if (backend->get_implementation == NULL) {
	    *font_face_out = &font_face->base;
	    return CAIRO_STATUS_SUCCESS;
	}

	status = backend->get_implementation (font_face,
					      font_face_out);

	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return _cairo_font_face_set_error (&font_face->base, status);
    }

    status = _cairo_user_scaled_font_backend.get_implementation (font_face,
								 font_face_out);

    return _cairo_font_face_set_error (&font_face->base, status);
}

static cairo_status_t
_cairo_toy_font_face_scaled_font_create (void                *abstract_font_face,
					 const cairo_matrix_t       *font_matrix,
					 const cairo_matrix_t       *ctm,
					 const cairo_font_options_t *options,
					 cairo_scaled_font_t	   **scaled_font)
{
    cairo_toy_font_face_t *font_face = abstract_font_face;
    cairo_status_t status;

    if (font_face->base.status)
	return font_face->base.status;

    status = cairo_font_options_status ((cairo_font_options_t *) options);
    if (status)
	return status;

    if (CAIRO_SCALED_FONT_BACKEND_DEFAULT != &_cairo_user_scaled_font_backend)
    {
	const cairo_scaled_font_backend_t * backend = CAIRO_SCALED_FONT_BACKEND_DEFAULT;

	*scaled_font = NULL;
	status =  backend->create_toy (font_face,
				       font_matrix,
				       ctm,
				       options,
				       scaled_font);

	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return _cairo_font_face_set_error (&font_face->base, status);

	if (*scaled_font)
	    cairo_scaled_font_destroy (*scaled_font);
    }

    status = _cairo_user_scaled_font_backend.create_toy (font_face,
							 font_matrix,
							 ctm,
							 options,
							 scaled_font);

    return _cairo_font_face_set_error (&font_face->base, status);
}

static cairo_bool_t
_cairo_font_face_is_toy (cairo_font_face_t *font_face)
{
    return font_face->backend == &_cairo_toy_font_face_backend;
}

/**
 * cairo_toy_font_face_get_family:
 * @font_face: A toy font face
 *
 * Gets the familly name of a toy font.
 *
 * Return value: The family name.  This string is owned by the font face
 * and remains valid as long as the font face is alive (referenced).
 *
 * Since: 1.8
 **/
const char *
cairo_toy_font_face_get_family (cairo_font_face_t *font_face)
{
    cairo_toy_font_face_t *toy_font_face = (cairo_toy_font_face_t *) font_face;
    if (! _cairo_font_face_is_toy (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return CAIRO_FONT_FAMILY_DEFAULT;
    }
    assert (toy_font_face->owns_family);
    return toy_font_face->family;
}

/**
 * cairo_toy_font_face_get_slant:
 * @font_face: A toy font face
 *
 * Gets the slant a toy font.
 *
 * Return value: The slant value
 *
 * Since: 1.8
 **/
cairo_font_slant_t
cairo_toy_font_face_get_slant (cairo_font_face_t *font_face)
{
    cairo_toy_font_face_t *toy_font_face = (cairo_toy_font_face_t *) font_face;
    if (! _cairo_font_face_is_toy (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return CAIRO_FONT_SLANT_DEFAULT;
    }
    return toy_font_face->slant;
}
slim_hidden_def (cairo_toy_font_face_get_slant);

/**
 * cairo_toy_font_face_get_weight:
 * @font_face: A toy font face
 *
 * Gets the weight a toy font.
 *
 * Return value: The weight value
 *
 * Since: 1.8
 **/
cairo_font_weight_t
cairo_toy_font_face_get_weight (cairo_font_face_t *font_face)
{
    cairo_toy_font_face_t *toy_font_face = (cairo_toy_font_face_t *) font_face;
    if (! _cairo_font_face_is_toy (font_face)) {
	if (_cairo_font_face_set_error (font_face, CAIRO_STATUS_FONT_TYPE_MISMATCH))
	    return CAIRO_FONT_WEIGHT_DEFAULT;
    }
    return toy_font_face->weight;
}
slim_hidden_def (cairo_toy_font_face_get_weight);

static const cairo_font_face_backend_t _cairo_toy_font_face_backend = {
    CAIRO_FONT_TYPE_TOY,
    _cairo_toy_font_face_destroy,
    _cairo_toy_font_face_scaled_font_get_implementation,
    _cairo_toy_font_face_scaled_font_create
};

void
_cairo_unscaled_font_init (cairo_unscaled_font_t               *unscaled_font,
			   const cairo_unscaled_font_backend_t *backend)
{
    CAIRO_REFERENCE_COUNT_INIT (&unscaled_font->ref_count, 1);
    unscaled_font->backend = backend;
}

cairo_unscaled_font_t *
_cairo_unscaled_font_reference (cairo_unscaled_font_t *unscaled_font)
{
    if (unscaled_font == NULL)
	return NULL;

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&unscaled_font->ref_count));

    _cairo_reference_count_inc (&unscaled_font->ref_count);

    return unscaled_font;
}

void
_cairo_unscaled_font_destroy (cairo_unscaled_font_t *unscaled_font)
{
    if (unscaled_font == NULL)
	return;

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&unscaled_font->ref_count));

    if (! _cairo_reference_count_dec_and_test (&unscaled_font->ref_count))
	return;

    unscaled_font->backend->destroy (unscaled_font);

    free (unscaled_font);
}

void
_cairo_font_face_reset_static_data (void)
{
    _cairo_scaled_font_map_destroy ();

    /* We manually acquire the lock rather than calling
     * cairo_toy_font_face_hash_table_lock simply to avoid
     * creating the table only to destroy it again. */
    CAIRO_MUTEX_LOCK (_cairo_font_face_mutex);
    _cairo_hash_table_destroy (cairo_toy_font_face_hash_table);
    cairo_toy_font_face_hash_table = NULL;
    CAIRO_MUTEX_UNLOCK (_cairo_font_face_mutex);
}

/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc.
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
 *      Keith Packard <keithp@keithp.com>
 *	Graydon Hoare <graydon@redhat.com>
 *	Carl Worth <cworth@cworth.org>
 */

#include "cairoint.h"
#include "cairo-error-private.h"

/*
 * An entry can be in one of three states:
 *
 * FREE: Entry has never been used, terminates all searches.
 *       Appears in the table as a %NULL pointer.
 *
 * DEAD: Entry had been live in the past. A dead entry can be reused
 *       but does not terminate a search for an exact entry.
 *       Appears in the table as a pointer to DEAD_ENTRY.
 *
 * LIVE: Entry is currently being used.
 *       Appears in the table as any non-%NULL, non-DEAD_ENTRY pointer.
 */

#define DEAD_ENTRY ((cairo_hash_entry_t *) 0x1)

#define ENTRY_IS_FREE(entry) ((entry) == NULL)
#define ENTRY_IS_DEAD(entry) ((entry) == DEAD_ENTRY)
#define ENTRY_IS_LIVE(entry) ((entry) >  DEAD_ENTRY)

/* We expect keys will not be destroyed frequently, so our table does not
 * contain any explicit shrinking code nor any chain-coalescing code for
 * entries randomly deleted by memory pressure (except during rehashing, of
 * course). These assumptions are potentially bad, but they make the
 * implementation straightforward.
 *
 * Revisit later if evidence appears that we're using excessive memory from
 * a mostly-dead table.
 *
 * This table is open-addressed with double hashing. Each table size is a
 * prime chosen to be a little more than double the high water mark for a
 * given arrangement, so the tables should remain < 50% full. The table
 * size makes for the "first" hash modulus; a second prime (2 less than the
 * first prime) serves as the "second" hash modulus, which is co-prime and
 * thus guarantees a complete permutation of table indices.
 *
 * This structure, and accompanying table, is borrowed/modified from the
 * file xserver/render/glyph.c in the freedesktop.org x server, with
 * permission (and suggested modification of doubling sizes) by Keith
 * Packard.
 */

typedef struct _cairo_hash_table_arrangement {
    unsigned long high_water_mark;
    unsigned long size;
    unsigned long rehash;
} cairo_hash_table_arrangement_t;

static const cairo_hash_table_arrangement_t hash_table_arrangements [] = {
    { 16,		43,		41		},
    { 32,		73,		71		},
    { 64,		151,		149		},
    { 128,		283,		281		},
    { 256,		571,		569		},
    { 512,		1153,		1151		},
    { 1024,		2269,		2267		},
    { 2048,		4519,		4517		},
    { 4096,		9013,		9011		},
    { 8192,		18043,		18041		},
    { 16384,		36109,		36107		},
    { 32768,		72091,		72089		},
    { 65536,		144409,		144407		},
    { 131072,		288361,		288359		},
    { 262144,		576883,		576881		},
    { 524288,		1153459,	1153457		},
    { 1048576,		2307163,	2307161		},
    { 2097152,		4613893,	4613891		},
    { 4194304,		9227641,	9227639		},
    { 8388608,		18455029,	18455027	},
    { 16777216,		36911011,	36911009	},
    { 33554432,		73819861,	73819859	},
    { 67108864,		147639589,	147639587	},
    { 134217728,	295279081,	295279079	},
    { 268435456,	590559793,	590559791	}
};

#define NUM_HASH_TABLE_ARRANGEMENTS ARRAY_LENGTH (hash_table_arrangements)

struct _cairo_hash_table {
    cairo_hash_keys_equal_func_t keys_equal;

    const cairo_hash_table_arrangement_t *arrangement;
    cairo_hash_entry_t **entries;

    unsigned long live_entries;
    unsigned long iterating;   /* Iterating, no insert, no resize */
};

/**
 * _cairo_hash_table_create:
 * @keys_equal: a function to return %TRUE if two keys are equal
 *
 * Creates a new hash table which will use the keys_equal() function
 * to compare hash keys. Data is provided to the hash table in the
 * form of user-derived versions of #cairo_hash_entry_t. A hash entry
 * must be able to hold both a key (including a hash code) and a
 * value. Sometimes only the key will be necessary, (as in
 * _cairo_hash_table_remove), and other times both a key and a value
 * will be necessary, (as in _cairo_hash_table_insert).
 *
 * See #cairo_hash_entry_t for more details.
 *
 * Return value: the new hash table or %NULL if out of memory.
 **/
cairo_hash_table_t *
_cairo_hash_table_create (cairo_hash_keys_equal_func_t keys_equal)
{
    cairo_hash_table_t *hash_table;

    hash_table = malloc (sizeof (cairo_hash_table_t));
    if (unlikely (hash_table == NULL)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    hash_table->keys_equal = keys_equal;

    hash_table->arrangement = &hash_table_arrangements[0];

    hash_table->entries = calloc (hash_table->arrangement->size,
				  sizeof(cairo_hash_entry_t *));
    if (unlikely (hash_table->entries == NULL)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	free (hash_table);
	return NULL;
    }

    hash_table->live_entries = 0;
    hash_table->iterating = 0;

    return hash_table;
}

/**
 * _cairo_hash_table_destroy:
 * @hash_table: an empty hash table to destroy
 *
 * Immediately destroys the given hash table, freeing all resources
 * associated with it.
 *
 * WARNING: The hash_table must have no live entries in it before
 * _cairo_hash_table_destroy is called. It is a fatal error otherwise,
 * and this function will halt. The rationale for this behavior is to
 * avoid memory leaks and to avoid needless complication of the API
 * with destroy notifiy callbacks.
 *
 * WARNING: The hash_table must have no running iterators in it when
 * _cairo_hash_table_destroy is called. It is a fatal error otherwise,
 * and this function will halt.
 **/
void
_cairo_hash_table_destroy (cairo_hash_table_t *hash_table)
{
    /* The hash table must be empty. Otherwise, halt. */
    assert (hash_table->live_entries == 0);
    /* No iterators can be running. Otherwise, halt. */
    assert (hash_table->iterating == 0);

    free (hash_table->entries);
    hash_table->entries = NULL;

    free (hash_table);
}

static cairo_hash_entry_t **
_cairo_hash_table_lookup_unique_key (cairo_hash_table_t *hash_table,
				     cairo_hash_entry_t *key)
{
    unsigned long table_size, i, idx, step;
    cairo_hash_entry_t **entry;

    table_size = hash_table->arrangement->size;
    idx = key->hash % table_size;

    entry = &hash_table->entries[idx];
    if (! ENTRY_IS_LIVE (*entry))
	return entry;

    i = 1;
    step = key->hash % hash_table->arrangement->rehash;
    if (step == 0)
	step = 1;
    do {
	idx += step;
	if (idx >= table_size)
	    idx -= table_size;

	entry = &hash_table->entries[idx];
	if (! ENTRY_IS_LIVE (*entry))
	    return entry;
    } while (++i < table_size);

    ASSERT_NOT_REACHED;
    return NULL;
}

/**
 * _cairo_hash_table_resize:
 * @hash_table: a hash table
 *
 * Resize the hash table if the number of entries has gotten much
 * bigger or smaller than the ideal number of entries for the current
 * size.
 *
 * Return value: %CAIRO_STATUS_SUCCESS if successful or
 * %CAIRO_STATUS_NO_MEMORY if out of memory.
 **/
static cairo_status_t
_cairo_hash_table_resize (cairo_hash_table_t *hash_table)
{
    cairo_hash_table_t tmp;
    unsigned long new_size, i;

    /* This keeps the hash table between 25% and 50% full. */
    unsigned long high = hash_table->arrangement->high_water_mark;
    unsigned long low = high >> 2;

    if (hash_table->live_entries >= low && hash_table->live_entries <= high)
	return CAIRO_STATUS_SUCCESS;

    tmp = *hash_table;

    if (hash_table->live_entries > high)
    {
	tmp.arrangement = hash_table->arrangement + 1;
	/* This code is being abused if we can't make a table big enough. */
	assert (tmp.arrangement - hash_table_arrangements <
		NUM_HASH_TABLE_ARRANGEMENTS);
    }
    else /* hash_table->live_entries < low */
    {
	/* Can't shrink if we're at the smallest size */
	if (hash_table->arrangement == &hash_table_arrangements[0])
	    return CAIRO_STATUS_SUCCESS;
	tmp.arrangement = hash_table->arrangement - 1;
    }

    new_size = tmp.arrangement->size;
    tmp.entries = calloc (new_size, sizeof (cairo_hash_entry_t*));
    if (unlikely (tmp.entries == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    for (i = 0; i < hash_table->arrangement->size; ++i) {
	if (ENTRY_IS_LIVE (hash_table->entries[i])) {
	    *_cairo_hash_table_lookup_unique_key (&tmp, hash_table->entries[i])
		= hash_table->entries[i];
	}
    }

    free (hash_table->entries);
    hash_table->entries = tmp.entries;
    hash_table->arrangement = tmp.arrangement;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_hash_table_lookup:
 * @hash_table: a hash table
 * @key: the key of interest
 *
 * Performs a lookup in @hash_table looking for an entry which has a
 * key that matches @key, (as determined by the keys_equal() function
 * passed to _cairo_hash_table_create).
 *
 * Return value: the matching entry, of %NULL if no match was found.
 **/
void *
_cairo_hash_table_lookup (cairo_hash_table_t *hash_table,
			  cairo_hash_entry_t *key)
{
    cairo_hash_entry_t *entry;
    unsigned long table_size, i, idx, step;

    table_size = hash_table->arrangement->size;
    idx = key->hash % table_size;

    entry = hash_table->entries[idx];
    if (ENTRY_IS_LIVE (entry)) {
	if (hash_table->keys_equal (key, entry))
	    return entry;
    } else if (ENTRY_IS_FREE (entry))
	return NULL;

    i = 1;
    step = key->hash % hash_table->arrangement->rehash;
    if (step == 0)
	step = 1;
    do {
	idx += step;
	if (idx >= table_size)
	    idx -= table_size;

	entry = hash_table->entries[idx];
	if (ENTRY_IS_LIVE (entry)) {
	    if (hash_table->keys_equal (key, entry))
		return entry;
	} else if (ENTRY_IS_FREE (entry))
	    return NULL;
    } while (++i < table_size);

    return NULL;
}

/**
 * _cairo_hash_table_random_entry:
 * @hash_table: a hash table
 * @predicate: a predicate function.
 *
 * Find a random entry in the hash table satisfying the given
 * @predicate.
 *
 * We use the same algorithm as the lookup algorithm to walk over the
 * entries in the hash table in a pseudo-random order. Walking
 * linearly would favor entries following gaps in the hash table. We
 * could also call rand() repeatedly, which works well for almost-full
 * tables, but degrades when the table is almost empty, or predicate
 * returns %TRUE for most entries.
 *
 * Return value: a random live entry or %NULL if there are no entries
 * that match the given predicate. In particular, if predicate is
 * %NULL, a %NULL return value indicates that the table is empty.
 **/
void *
_cairo_hash_table_random_entry (cairo_hash_table_t	   *hash_table,
				cairo_hash_predicate_func_t predicate)
{
    cairo_hash_entry_t *entry;
    unsigned long hash;
    unsigned long table_size, i, idx, step;

    assert (predicate != NULL);

    table_size = hash_table->arrangement->size;
    hash = rand ();
    idx = hash % table_size;

    entry = hash_table->entries[idx];
    if (ENTRY_IS_LIVE (entry) && predicate (entry))
	return entry;

    i = 1;
    step = hash % hash_table->arrangement->rehash;
    if (step == 0)
	step = 1;
    do {
	idx += step;
	if (idx >= table_size)
	    idx -= table_size;

	entry = hash_table->entries[idx];
	if (ENTRY_IS_LIVE (entry) && predicate (entry))
	    return entry;
    } while (++i < table_size);

    return NULL;
}

/**
 * _cairo_hash_table_insert:
 * @hash_table: a hash table
 * @key_and_value: an entry to be inserted
 *
 * Insert the entry #key_and_value into the hash table.
 *
 * WARNING: There must not be an existing entry in the hash table
 * with a matching key.
 *
 * WARNING: It is a fatal error to insert an element while
 * an iterator is running
 *
 * Instead of using insert to replace an entry, consider just editing
 * the entry obtained with _cairo_hash_table_lookup. Or if absolutely
 * necessary, use _cairo_hash_table_remove first.
 *
 * Return value: %CAIRO_STATUS_SUCCESS if successful or
 * %CAIRO_STATUS_NO_MEMORY if insufficient memory is available.
 **/
cairo_status_t
_cairo_hash_table_insert (cairo_hash_table_t *hash_table,
			  cairo_hash_entry_t *key_and_value)
{
    cairo_status_t status;

    /* Insert is illegal while an iterator is running. */
    assert (hash_table->iterating == 0);

    hash_table->live_entries++;
    status = _cairo_hash_table_resize (hash_table);
    if (unlikely (status)) {
	/* abort the insert... */
	hash_table->live_entries--;
	return status;
    }

    *_cairo_hash_table_lookup_unique_key (hash_table,
					  key_and_value) = key_and_value;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_hash_entry_t **
_cairo_hash_table_lookup_exact_key (cairo_hash_table_t *hash_table,
				    cairo_hash_entry_t *key)
{
    unsigned long table_size, i, idx, step;
    cairo_hash_entry_t **entry;

    table_size = hash_table->arrangement->size;
    idx = key->hash % table_size;

    entry = &hash_table->entries[idx];
    if (*entry == key)
	return entry;

    i = 1;
    step = key->hash % hash_table->arrangement->rehash;
    if (step == 0)
	step = 1;
    do {
	idx += step;
	if (idx >= table_size)
	    idx -= table_size;

	entry = &hash_table->entries[idx];
	if (*entry == key)
	    return entry;
    } while (++i < table_size);

    ASSERT_NOT_REACHED;
    return NULL;
}
/**
 * _cairo_hash_table_remove:
 * @hash_table: a hash table
 * @key: key of entry to be removed
 *
 * Remove an entry from the hash table which points to @key.
 *
 * Return value: %CAIRO_STATUS_SUCCESS if successful or
 * %CAIRO_STATUS_NO_MEMORY if out of memory.
 **/
void
_cairo_hash_table_remove (cairo_hash_table_t *hash_table,
			  cairo_hash_entry_t *key)
{
    *_cairo_hash_table_lookup_exact_key (hash_table, key) = DEAD_ENTRY;
    hash_table->live_entries--;

    /* Check for table resize. Don't do this when iterating as this will
     * reorder elements of the table and cause the iteration to potentially
     * skip some elements. */
    if (hash_table->iterating == 0) {
	/* This call _can_ fail, but only in failing to allocate new
	 * memory to shrink the hash table. It does leave the table in a
	 * consistent state, and we've already succeeded in removing the
	 * entry, so we don't examine the failure status of this call. */
	_cairo_hash_table_resize (hash_table);
    }
}

/**
 * _cairo_hash_table_foreach:
 * @hash_table: a hash table
 * @hash_callback: function to be called for each live entry
 * @closure: additional argument to be passed to @hash_callback
 *
 * Call @hash_callback for each live entry in the hash table, in a
 * non-specified order.
 *
 * Entries in @hash_table may be removed by code executed from @hash_callback.
 *
 * Entries may not be inserted to @hash_table, nor may @hash_table
 * be destroyed by code executed from @hash_callback. The relevant
 * functions will halt in these cases.
 **/
void
_cairo_hash_table_foreach (cairo_hash_table_t	      *hash_table,
			   cairo_hash_callback_func_t  hash_callback,
			   void			      *closure)
{
    unsigned long i;
    cairo_hash_entry_t *entry;

    /* Mark the table for iteration */
    ++hash_table->iterating;
    for (i = 0; i < hash_table->arrangement->size; i++) {
	entry = hash_table->entries[i];
	if (ENTRY_IS_LIVE(entry))
	    hash_callback (entry, closure);
    }
    /* If some elements were deleted during the iteration,
     * the table may need resizing. Just do this every time
     * as the check is inexpensive.
     */
    if (--hash_table->iterating == 0) {
	/* Should we fail to shrink the hash table, it is left unaltered,
	 * and we don't need to propagate the error status. */
	_cairo_hash_table_resize (hash_table);
    }
}

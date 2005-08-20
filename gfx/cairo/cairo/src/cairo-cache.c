/* cairo - a vector graphics library with display and print output
 *
 * This file is Copyright © 2004 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *      Keith Packard
 *	Graydon Hoare <graydon@redhat.com>
 */

#include "cairoint.h"

/* 
 * This structure, and accompanying table, is borrowed/modified from the
 * file xserver/render/glyph.c in the freedesktop.org x server, with
 * permission (and suggested modification of doubling sizes) by Keith
 * Packard.
 */

static const cairo_cache_arrangement_t cache_arrangements [] = {
    { 16,		43,		41        },
    { 32,		73,		71        },
    { 64,		151,		149       },
    { 128,		283,		281       },
    { 256,		571,		569       },
    { 512,		1153,		1151      },
    { 1024,		2269,		2267      },
    { 2048,		4519,		4517      },
    { 4096,		9013,		9011      },
    { 8192,		18043,		18041     },
    { 16384,		36109,		36107     },
    { 32768,		72091,		72089     },
    { 65536,		144409,		144407    },
    { 131072,		288361,		288359    },
    { 262144,		576883,		576881    },
    { 524288,		1153459,	1153457   },
    { 1048576,		2307163,	2307161   },
    { 2097152,		4613893,	4613891   },
    { 4194304,		9227641,	9227639   },
    { 8388608,		18455029,	18455027  },
    { 16777216,		36911011,	36911009  },
    { 33554432,		73819861,	73819859  },
    { 67108864,		147639589,	147639587 },
    { 134217728,	295279081,	295279079 },
    { 268435456,	590559793,	590559791 }
};

#define N_CACHE_SIZES (sizeof(cache_arrangements)/sizeof(cache_arrangements[0]))

/*
 * Entries 'e' are poiners, in one of 3 states:
 *
 * e == NULL: The entry has never had anything put in it
 * e != DEAD_ENTRY: The entry has an active value in it currently
 * e == DEAD_ENTRY: The entry *had* a value in it at some point, but the 
 *                  entry has been killed. Lookups requesting free space can
 *                  reuse these entries; lookups requesting a precise match
 *                  should neither return these entries nor stop searching when
 *                  seeing these entries. 
 *
 * We expect keys will not be destroyed frequently, so our table does not
 * contain any explicit shrinking code nor any chain-coalescing code for
 * entries randomly deleted by memory pressure (except during rehashing, of
 * course). These assumptions are potentially bad, but they make the
 * implementation straightforward.
 *
 * Revisit later if evidence appears that we're using excessive memory from
 * a mostly-dead table.
 *
 * Generally you do not need to worry about freeing cache entries; the
 * cache will expire entries randomly as it experiences memory pressure.
 * If max_memory is set, entries are not expired, and must be explicitely
 * removed.
 *
 * This table is open-addressed with double hashing. Each table size is a
 * prime chosen to be a little more than double the high water mark for a
 * given arrangement, so the tables should remain < 50% full. The table
 * size makes for the "first" hash modulus; a second prime (2 less than the
 * first prime) serves as the "second" hash modulus, which is co-prime and
 * thus guarantees a complete permutation of table indices.
 * 
 */

#define DEAD_ENTRY ((cairo_cache_entry_base_t *) 1)
#define NULL_ENTRY_P(cache, i) ((cache)->entries[i] == NULL)
#define DEAD_ENTRY_P(cache, i) ((cache)->entries[i] == DEAD_ENTRY)
#define LIVE_ENTRY_P(cache, i) \
 (!((NULL_ENTRY_P((cache),(i))) || (DEAD_ENTRY_P((cache),(i)))))

#ifdef NDEBUG
#define _cache_sane_state(c) 
#else
static void 
_cache_sane_state (cairo_cache_t *cache)
{
    assert (cache != NULL);
    assert (cache->entries != NULL);
    assert (cache->backend != NULL);
    assert (cache->arrangement != NULL);
    /* Cannot check this, a single object may larger */
    /* assert (cache->used_memory <= cache->max_memory); */
    assert (cache->live_entries <= cache->arrangement->size);   
}
#endif

static void
_entry_destroy (cairo_cache_t *cache, unsigned long i)
{
    _cache_sane_state (cache);

    if (LIVE_ENTRY_P(cache, i))
    {
	cairo_cache_entry_base_t *entry = cache->entries[i];
	assert(cache->live_entries > 0);
	assert(cache->used_memory >= entry->memory);

	cache->live_entries--;
 	cache->used_memory -= entry->memory;
	cache->backend->destroy_entry (cache, entry);
	cache->entries[i] = DEAD_ENTRY;
    }
}

static cairo_cache_entry_base_t **
_cache_lookup (cairo_cache_t *cache,
	       void *key,
	       int (*predicate)(void*,void*,void*))
{    

    cairo_cache_entry_base_t **probe;
    unsigned long hash;
    unsigned long table_size, i, idx, step;
    
    _cache_sane_state (cache);
    assert (key != NULL);

    table_size = cache->arrangement->size;
    hash = cache->backend->hash (cache, key);
    idx = hash % table_size;
    step = 0;

    for (i = 0; i < table_size; ++i)
    {
#ifdef CAIRO_MEASURE_CACHE_PERFORMANCE
	cache->probes++;
#endif	
	assert(idx < table_size);
	probe = cache->entries + idx;

	/* 
	 * There are two lookup modes: searching for a free slot and searching
	 * for an exact entry. 
	 */ 

	if (predicate != NULL)
	{
	    /* We are looking up an exact entry. */
	    if (*probe == NULL)
	    {
		/* Found an empty spot, there can't be a match */
		break;
	    }
	    else if (*probe != DEAD_ENTRY 
		     && (*probe)->hashcode == hash
		     && predicate (cache, key, *probe))
	    {
		return probe;
	    }
	}
	else
	{
	    /* We are just looking for a free slot. */
	    if (*probe == NULL 
		|| *probe == DEAD_ENTRY)
	    {
		return probe;
	    }
	}

	if (step == 0) { 	    
	    step = hash % cache->arrangement->rehash;
	    if (step == 0)
		step = 1;
	}

	idx += step;
	if (idx >= table_size)
	    idx -= table_size;
    }

    /* 
     * The table should not have permitted you to get here if you were just
     * looking for a free slot: there should have been room.
     */
    assert(predicate != NULL);
    return NULL;
}

static cairo_cache_entry_base_t **
_find_available_entry_for (cairo_cache_t *cache,
			   void *key)
{
    return _cache_lookup (cache, key, NULL);
}

static cairo_cache_entry_base_t **
_find_exact_live_entry_for (cairo_cache_t *cache,
			    void *key)
{    
    return _cache_lookup (cache, key, cache->backend->keys_equal);
}

static const cairo_cache_arrangement_t *
_find_cache_arrangement (unsigned long proposed_size)
{
    unsigned long idx;

    for (idx = 0; idx < N_CACHE_SIZES; ++idx)
	if (cache_arrangements[idx].high_water_mark >= proposed_size)
	    return &cache_arrangements[idx];
    return NULL;
}

static cairo_status_t
_resize_cache (cairo_cache_t *cache, unsigned long proposed_size)
{
    cairo_cache_t tmp;
    cairo_cache_entry_base_t **e;
    unsigned long new_size, i;

    tmp = *cache;
    tmp.arrangement = _find_cache_arrangement (proposed_size);
    assert(tmp.arrangement != NULL);
    if (tmp.arrangement == cache->arrangement)
	return CAIRO_STATUS_SUCCESS;

    new_size = tmp.arrangement->size;
    tmp.entries = calloc (new_size, sizeof (cairo_cache_entry_base_t *));
    if (tmp.entries == NULL) 
	return CAIRO_STATUS_NO_MEMORY;
        
    for (i = 0; i < cache->arrangement->size; ++i) {
	if (LIVE_ENTRY_P(cache, i)) {
	    e = _find_available_entry_for (&tmp, cache->entries[i]);
	    assert (e != NULL);
	    *e = cache->entries[i];
	}
    }
    free (cache->entries);
    cache->entries = tmp.entries;
    cache->arrangement = tmp.arrangement;
    return CAIRO_STATUS_SUCCESS;
}


#ifdef CAIRO_MEASURE_CACHE_PERFORMANCE
static double
_load_factor (cairo_cache_t *cache)
{
    return ((double) cache->live_entries) 
	/ ((double) cache->arrangement->size);
}
#endif

/* Find a random in the cache matching the given predicate. We use the
 * same algorithm as the probing algorithm to walk over the entries in
 * the hash table in a pseudo-random order. Walking linearly would
 * favor entries following gaps in the hash table. We could also
 * call rand() repeatedly, which works well for almost-full tables,
 * but degrades when the table is almost empty, or predicate
 * returns false for most entries.
 */
static cairo_cache_entry_base_t **
_random_entry (cairo_cache_t *cache,
	       int (*predicate)(void*))
{    
    cairo_cache_entry_base_t **probe;
    unsigned long hash;
    unsigned long table_size, i, idx, step;
    
    _cache_sane_state (cache);

    table_size = cache->arrangement->size;
    hash = rand ();
    idx = hash % table_size;
    step = 0;

    for (i = 0; i < table_size; ++i)
    {
	assert(idx < table_size);
	probe = cache->entries + idx;

	if (LIVE_ENTRY_P(cache, idx)
	    && (!predicate || predicate (*probe)))
	    return probe;

	if (step == 0) { 	    
	    step = hash % cache->arrangement->rehash;
	    if (step == 0)
		step = 1;
	}

	idx += step;
	if (idx >= table_size)
	    idx -= table_size;
    }

    return NULL;
}

/* public API follows */

cairo_status_t
_cairo_cache_init (cairo_cache_t *cache,
		   const cairo_cache_backend_t *backend,
		   unsigned long max_memory)
{    
    assert (backend != NULL);

    if (cache != NULL){
	cache->arrangement = &cache_arrangements[0];
	cache->max_memory = max_memory;
	cache->used_memory = 0;
	cache->live_entries = 0;

#ifdef CAIRO_MEASURE_CACHE_PERFORMANCE
	cache->hits = 0;
	cache->misses = 0;
	cache->probes = 0;
#endif

	cache->backend = backend;	
	cache->entries = calloc (cache->arrangement->size,
				 sizeof(cairo_cache_entry_base_t *));
				 
	if (cache->entries == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
    }    
    _cache_sane_state (cache);
    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_cache_destroy (cairo_cache_t *cache)
{
    unsigned long i;
    if (cache == NULL)
	return;
	
    _cache_sane_state (cache);

    for (i = 0; i < cache->arrangement->size; ++i)
	_entry_destroy (cache, i);
	
    free (cache->entries);
    cache->entries = NULL;
    cache->backend->destroy_cache (cache);
}

void
_cairo_cache_shrink_to (cairo_cache_t *cache,
			unsigned long max_memory)
{
    unsigned long idx;
    /* Make some entries die if we're under memory pressure. */
    while (cache->live_entries > 0 && cache->used_memory > max_memory) {
	idx =  _random_entry (cache, NULL) - cache->entries;
	assert (idx < cache->arrangement->size);
	_entry_destroy (cache, idx);
    }
}

cairo_status_t
_cairo_cache_lookup (cairo_cache_t *cache,
		     void          *key,
		     void         **entry_return,
		     int           *created_entry)
{

    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_cache_entry_base_t **slot = NULL, *new_entry;

    _cache_sane_state (cache);

#ifdef CAIRO_MEASURE_CACHE_PERFORMANCE
    if ((cache->hits + cache->misses) % 0xffff == 0)
	printf("cache %p stats: size %ld, live %ld, load %.2f\n"
	       "                      mem %ld/%ld, hit %ld, miss %ld\n"
	       "                      probe %ld, %.2f probe/access\n", 
	       cache,
	       cache->arrangement->size, 
	       cache->live_entries, 
	       _load_factor (cache),
	       cache->used_memory, 
	       cache->max_memory,
	       cache->hits, 
	       cache->misses, 
	       cache->probes,
	       ((double) cache->probes) 
	       / ((double) (cache->hits + 
			    cache->misses + 1)));
#endif
    
    /* See if we have an entry in the table already. */
    slot = _find_exact_live_entry_for (cache, key);
    if (slot != NULL) {
#ifdef CAIRO_MEASURE_CACHE_PERFORMANCE
	cache->hits++;
#endif
	*entry_return = *slot;
	if (created_entry)
	    *created_entry = 0;
	return status;
    }

#ifdef CAIRO_MEASURE_CACHE_PERFORMANCE
    cache->misses++;
#endif

    /* Build the new entry. */
    status = cache->backend->create_entry (cache, key, 
					   (void **)&new_entry);
    if (status != CAIRO_STATUS_SUCCESS)
	return status;

    /* Store the hash value in case the backend forgot. */
    new_entry->hashcode = cache->backend->hash (cache, key);

    if (cache->live_entries && cache->max_memory)
        _cairo_cache_shrink_to (cache, cache->max_memory);
    
    /* Can't assert this; new_entry->memory may be larger than max_memory */
    /* assert(cache->max_memory >= (cache->used_memory + new_entry->memory)); */
    
    /* Make room in the table for a new slot. */
    status = _resize_cache (cache, cache->live_entries + 1);
    if (status != CAIRO_STATUS_SUCCESS) {
	cache->backend->destroy_entry (cache, new_entry);
	return status;
    }

    slot = _find_available_entry_for (cache, key);
    assert(slot != NULL);
    
    /* Store entry in slot and increment statistics. */
    *slot = new_entry;
    cache->live_entries++;
    cache->used_memory += new_entry->memory;

    _cache_sane_state (cache);

    *entry_return = new_entry;
    if (created_entry)
      *created_entry = 1;
    
    return status;
}

cairo_status_t
_cairo_cache_remove (cairo_cache_t *cache,
		     void          *key)
{
    cairo_cache_entry_base_t **slot;

    _cache_sane_state (cache);

    /* See if we have an entry in the table already. */
    slot = _find_exact_live_entry_for (cache, key);
    if (slot != NULL)
      	_entry_destroy (cache, slot - cache->entries);

    return CAIRO_STATUS_SUCCESS;
}

void *
_cairo_cache_random_entry (cairo_cache_t *cache,
			   int (*predicate)(void*))
{
    cairo_cache_entry_base_t **slot = _random_entry (cache, predicate);

    return slot ? *slot : NULL;
}

unsigned long
_cairo_hash_string (const char *c)
{
    /* This is the djb2 hash. */
    unsigned long hash = 5381;
    while (c && *c)
	hash = ((hash << 5) + hash) + *c++;
    return hash;
}


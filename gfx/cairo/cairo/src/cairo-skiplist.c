/*
 * Copyright © 2006 Keith Packard
 * Copyright © 2006 Carl Worth
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include "cairoint.h"

#include "cairo-skiplist-private.h"

#if HAVE_FFS
#include <strings.h> /* ffs() */
#endif

#define ELT_DATA(elt) (void *)	((char*) (elt) - list->data_size)
#define NEXT_TO_ELT(next)	(skip_elt_t *) ((char *) (next) - offsetof (skip_elt_t, next))

static uint32_t
hars_petruska_f54_1_random (void)
{
#  define rol(x,k) ((x << k) | (x >> (32-k)))
    static uint32_t x = 0;
    x = (x ^ rol(x, 5) ^ rol(x, 24)) + 0x37798849;
    return x;
#  undef rol
}

struct pool {
    struct pool *next;
    char *ptr;
    unsigned int rem;
};

static struct pool *
pool_new (void)
{
    struct pool *pool;

    pool = malloc (8192 - 8);
    if (unlikely (pool == NULL))
	return NULL;

    pool->next = NULL;
    pool->rem = 8192 - 8 - sizeof (struct pool);
    pool->ptr = (char *) (pool + 1);

    return pool;
}

static void
pools_destroy (struct pool *pool)
{
    while (pool->next != NULL) {
	struct pool *next = pool->next;
	free (pool);
	pool = next;
    }
}

/*
 * Initialize an empty skip list
 */
void
_cairo_skip_list_init (cairo_skip_list_t	*list,
                       cairo_skip_list_compare_t compare,
                       size_t			 elt_size)
{
    int i;

    list->compare = compare;
    list->elt_size = elt_size;
    list->data_size = elt_size - sizeof (skip_elt_t);
    list->pool = (struct pool *) list->pool_embedded;
    list->pool->next = NULL;
    list->pool->rem = sizeof (list->pool_embedded) - sizeof (struct pool);
    list->pool->ptr = list->pool_embedded + sizeof (struct pool);

    for (i = 0; i < MAX_LEVEL; i++) {
	list->chains[i] = NULL;
    }

    for (i = 0; i < MAX_FREELIST_LEVEL; i++) {
	list->freelists[i] = NULL;
    }

    list->max_level = 0;
}

void
_cairo_skip_list_fini (cairo_skip_list_t *list)
{
    pools_destroy (list->pool);
}

/*
 * Generate a random level number, distributed
 * so that each level is 1/4 as likely as the one before
 *
 * Note that level numbers run 1 <= level < MAX_LEVEL
 */
static int
random_level (void)
{
    /* tricky bit -- each bit is '1' 75% of the time.
     * This works because we only use the lower MAX_LEVEL
     * bits, and MAX_LEVEL < 16 */
    uint32_t bits = hars_petruska_f54_1_random ();
#if HAVE_FFS
    return ffs (-(1<<MAX_LEVEL) | bits | bits >> 16);
#else
    int level = 1;

    bits |= -(1<<MAX_LEVEL) | bits >> 16;
    while ((bits & 1) == 0) {
	level++;
	bits >>= 1;
    }
    return level;
#endif
}

static void *
pool_alloc (cairo_skip_list_t *list,
	    unsigned int level)
{
    unsigned int size;
    struct pool *pool;
    void *ptr;

    size = list->elt_size +
	(FREELIST_MAX_LEVEL_FOR (level) - 1) * sizeof (skip_elt_t *);

    pool = list->pool;
    if (size > pool->rem) {
	pool = pool_new ();
	if (unlikely (pool == NULL))
	    return NULL;

	pool->next = list->pool;
	list->pool = pool;
    }

    ptr = pool->ptr;
    pool->ptr += size;
    pool->rem -= size;

    return ptr;
}

static void *
alloc_node_for_level (cairo_skip_list_t *list, unsigned level)
{
    int freelist_level = FREELIST_FOR_LEVEL (level);
    if (list->freelists[freelist_level]) {
	skip_elt_t *elt = list->freelists[freelist_level];
	list->freelists[freelist_level] = elt->prev;
	return ELT_DATA(elt);
    }
    return pool_alloc (list, level);
}

static void
free_elt (cairo_skip_list_t *list, skip_elt_t *elt)
{
    int level = elt->prev_index + 1;
    int freelist_level = FREELIST_FOR_LEVEL (level);
    elt->prev = list->freelists[freelist_level];
    list->freelists[freelist_level] = elt;
}

/*
 * Insert 'data' into the list
 */
void *
_cairo_skip_list_insert (cairo_skip_list_t *list, void *data, int unique)
{
    skip_elt_t **update[MAX_LEVEL];
    skip_elt_t *prev[MAX_LEVEL];
    char *data_and_elt;
    skip_elt_t *elt, **next;
    int	    i, level, prev_index;

    /*
     * Find links along each chain
     */
    elt = NULL;
    next = list->chains;
    for (i = list->max_level; --i >= 0; )
    {
	if (elt != next[i])
	{
	    for (; (elt = next[i]); next = elt->next)
	    {
		int cmp = list->compare (list, ELT_DATA(elt), data);
		if (unique && 0 == cmp)
		    return ELT_DATA(elt);
		if (cmp > 0)
		    break;
	    }
	}
        update[i] = next;
	if (next != list->chains)
	    prev[i] = NEXT_TO_ELT (next);
	else
	    prev[i] = NULL;
    }
    level = random_level ();
    prev_index = level - 1;

    /*
     * Create new list element
     */
    if (level > list->max_level)
    {
	level = list->max_level + 1;
	prev_index = level - 1;
	prev[prev_index] = NULL;
	update[list->max_level] = list->chains;
	list->max_level = level;
    }

    data_and_elt = alloc_node_for_level (list, level);
    if (unlikely (data_and_elt == NULL)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    memcpy (data_and_elt, data, list->data_size);
    elt = (skip_elt_t *) (data_and_elt + list->data_size);

    elt->prev_index = prev_index;
    elt->prev = prev[prev_index];

    /*
     * Insert into all chains
     */
    for (i = 0; i < level; i++)
    {
	elt->next[i] = update[i][i];
	if (elt->next[i] && elt->next[i]->prev_index == i)
	    elt->next[i]->prev = elt;
	update[i][i] = elt;
    }

    return data_and_elt;
}

void *
_cairo_skip_list_find (cairo_skip_list_t *list, void *data)
{
    int i;
    skip_elt_t **next = list->chains;
    skip_elt_t *elt;

    /*
     * Walk chain pointers one level at a time
     */
    for (i = list->max_level; --i >= 0;)
	while (next[i] && list->compare (list, data, ELT_DATA(next[i])) > 0)
	{
	    next = next[i]->next;
	}
    /*
     * Here we are
     */
    elt = next[0];
    if (elt && list->compare (list, data, ELT_DATA (elt)) == 0)
	return ELT_DATA (elt);

    return NULL;
}

void
_cairo_skip_list_delete (cairo_skip_list_t *list, void *data)
{
    skip_elt_t **update[MAX_LEVEL], *prev[MAX_LEVEL];
    skip_elt_t *elt, **next;
    int	i;

    /*
     * Find links along each chain
     */
    next = list->chains;
    for (i = list->max_level; --i >= 0; )
    {
	for (; (elt = next[i]); next = elt->next)
	{
	    if (list->compare (list, ELT_DATA (elt), data) >= 0)
		break;
	}
        update[i] = &next[i];
	if (next == list->chains)
	    prev[i] = NULL;
	else
	    prev[i] = NEXT_TO_ELT (next);
    }
    elt = next[0];
    assert (list->compare (list, ELT_DATA (elt), data) == 0);
    for (i = 0; i < list->max_level && *update[i] == elt; i++) {
	*update[i] = elt->next[i];
	if (elt->next[i] && elt->next[i]->prev_index == i)
	    elt->next[i]->prev = prev[i];
    }
    while (list->max_level > 0 && list->chains[list->max_level - 1] == NULL)
	list->max_level--;
    free_elt (list, elt);
}

void
_cairo_skip_list_delete_given (cairo_skip_list_t *list, skip_elt_t *given)
{
    skip_elt_t **update[MAX_LEVEL], *prev[MAX_LEVEL];
    skip_elt_t *elt, **next;
    int	i;

    /*
     * Find links along each chain
     */
    if (given->prev)
	next = given->prev->next;
    else
	next = list->chains;
    for (i = given->prev_index + 1; --i >= 0; )
    {
	for (; (elt = next[i]); next = elt->next)
	{
	    if (elt == given)
		break;
	}
        update[i] = &next[i];
	if (next == list->chains)
	    prev[i] = NULL;
	else
	    prev[i] = NEXT_TO_ELT (next);
    }
    elt = next[0];
    assert (elt == given);
    for (i = 0; i < (given->prev_index + 1) && *update[i] == elt; i++) {
	*update[i] = elt->next[i];
	if (elt->next[i] && elt->next[i]->prev_index == i)
	    elt->next[i]->prev = prev[i];
    }
    while (list->max_level > 0 && list->chains[list->max_level - 1] == NULL)
	list->max_level--;
    free_elt (list, elt);
}

#if MAIN
typedef struct {
    int n;
    skip_elt_t elt;
} test_elt_t;

static int
test_cmp (void *list, void *A, void *B)
{
    const test_elt_t *a = A, *b = B;
    return a->n - b->n;
}

int
main (void)
{
    cairo_skip_list_t list;
    test_elt_t elt;
    int n;

    _cairo_skip_list_init (&list, test_cmp, sizeof (test_elt_t));
    for (n = 0; n < 10000000; n++) {
	void *elt_and_data;
	elt.n = n;
	elt_and_data = _cairo_skip_list_insert (&list, &elt, TRUE);
	assert (elt_and_data != NULL);
    }
    _cairo_skip_list_fini (&list);

    return 0;
}

/* required supporting stubs */
cairo_status_t _cairo_error (cairo_status_t status) { return status; }
#endif

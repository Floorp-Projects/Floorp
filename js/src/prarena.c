/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * Lifetime-based fast allocation, inspired by much prior art, including
 * "Fast Allocation and Deallocation of Memory Based on Object Lifetimes"
 * David R. Hanson, Software -- Practice and Experience, Vol. 20(1).
 */
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "prarena.h"
#include "prlog.h"
#ifdef JS_THREADSAFE
extern js_CompareAndSwap(prword *, prword, prword);
#endif

static PRArena *arena_freelist;

#ifdef PR_ARENAMETER
static PRArenaStats *arena_stats_list;

#define COUNT(pool,what)  (pool)->stats.what++
#else
#define COUNT(pool,what)  /* nothing */
#endif

#define PR_ARENA_DEFAULT_ALIGN  sizeof(double)

PR_PUBLIC_API(void)
PR_InitArenaPool(PRArenaPool *pool, const char *name, size_t size, size_t align)
{
    if (align == 0)
	align = PR_ARENA_DEFAULT_ALIGN;
    pool->mask = PR_BITMASK(PR_CeilingLog2(align));
    pool->first.next = NULL;
    pool->first.base = pool->first.avail = pool->first.limit =
	(pruword)PR_ARENA_ALIGN(pool, &pool->first + 1);
    pool->current = &pool->first;
    pool->arenasize = size;
#ifdef PR_ARENAMETER
    memset(&pool->stats, 0, sizeof pool->stats);
    pool->stats.name = strdup(name);
    pool->stats.next = arena_stats_list;
    arena_stats_list = &pool->stats;
#endif
}

PR_PUBLIC_API(void *)
PR_ArenaAllocate(PRArenaPool *pool, size_t nb)
{
    PRArena **ap, *a, *b;
#ifdef JS_THREADSAFE
	PRArena *c;
#endif
    size_t sz;
    void *p;

    PR_ASSERT((nb & pool->mask) == 0);
#if defined(XP_PC) && !defined(_WIN32)
    if (nb >= 60000U)
	return 0;
#endif  /* WIN16 */
    ap = &arena_freelist;
    for (a = pool->current; a->avail + nb > a->limit; pool->current = a) {
        if (a->next) {                          /* move to next arena */
            a = a->next;
	    continue;
        }
	while ((b = *ap) != NULL) {		/* reclaim a free arena */
	    if (b->limit - b->base == pool->arenasize) {
#ifdef JS_THREADSAFE
		do {
		  c = b->next;
		} while (!js_CompareAndSwap((prword *)ap,(prword)b,(prword)c));
#else
		*ap = b->next;
#endif
		b->next = NULL;
		a = a->next = b;
		COUNT(pool, nreclaims);
		goto claim;
	    }
	    ap = &b->next;
	}
	sz = PR_MAX(pool->arenasize, nb);	/* allocate a new arena */
	sz += sizeof *a + pool->mask;           /* header and alignment slop */
	b = malloc(sz);
	if (!b)
	    return 0;
	a = a->next = b;
	a->next = NULL;
	a->limit = (pruword)a + sz;
	PR_COUNT_ARENA(pool,++);
	COUNT(pool, nmallocs);
    claim:
	a->base = a->avail = (pruword)PR_ARENA_ALIGN(pool, a + 1);
    }
    p = (void *)a->avail;
    a->avail += nb;
    return p;
}

PR_PUBLIC_API(void *)
PR_ArenaGrow(PRArenaPool *pool, void *p, size_t size, size_t incr)
{
    void *newp;

    PR_ARENA_ALLOCATE(newp, pool, size + incr);
    memcpy(newp, p, size);
    return newp;
}

/*
 * Free tail arenas linked after head, which may not be the true list head.
 * Reset pool->current to point to head in case it pointed at a tail arena.
 */
static void
FreeArenaList(PRArenaPool *pool, PRArena *head, PRBool reallyFree)
{
    PRArena **ap, *a;
#ifdef JS_THREADSAFE
	PRArena *b;
#endif

    ap = &head->next;
    a = *ap;
    if (!a)
	return;

#ifdef DEBUG
    do {
	PR_ASSERT(a->base <= a->avail && a->avail <= a->limit);
	a->avail = a->base;
	PR_CLEAR_UNUSED(a);
    } while ((a = a->next) != NULL);
    a = *ap;
#endif

    if (reallyFree) {
	do {
	    *ap = a->next;
	    PR_CLEAR_ARENA(a);
	    PR_COUNT_ARENA(pool,--);
	    free(a);
	} while ((a = *ap) != NULL);
    } else {
	/* Insert the whole arena chain at the front of the freelist. */
	do {
	    ap = &(*ap)->next;
	} while (*ap);
#ifdef JS_THREADSAFE
	do {
	  *ap = b = arena_freelist;
	} while (!js_CompareAndSwap((prword*)&arena_freelist,(prword)b,(prword)a));
#else
	*ap = arena_freelist;
	arena_freelist = a;
#endif
	head->next = NULL;
    }

    pool->current = head;
}

PR_PUBLIC_API(void)
PR_ArenaRelease(PRArenaPool *pool, char *mark)
{
    PRArena *a;

    for (a = pool->first.next; a; a = a->next) {
	if (PR_UPTRDIFF(mark, a) < PR_UPTRDIFF(a->avail, a)) {
	    a->avail = (pruword)PR_ARENA_ALIGN(pool, mark);
	    FreeArenaList(pool, a, PR_TRUE);
	    return;
	}
    }
}

PR_PUBLIC_API(void)
PR_FreeArenaPool(PRArenaPool *pool)
{
    FreeArenaList(pool, &pool->first, PR_FALSE);
    COUNT(pool, ndeallocs);
}

PR_PUBLIC_API(void)
PR_FinishArenaPool(PRArenaPool *pool)
{
    FreeArenaList(pool, &pool->first, PR_TRUE);
#ifdef PR_ARENAMETER
    {
	PRArenaStats *stats, **statsp;

	if (pool->stats.name)
	    free(pool->stats.name);
	for (statsp = &arena_stats_list; (stats = *statsp) != 0;
	     statsp = &stats->next) {
	    if (stats == &pool->stats) {
		*statsp = stats->next;
		return;
	    }
	}
    }
#endif
}

PR_PUBLIC_API(void)
PR_CompactArenaPool(PRArenaPool *pool)
{
#if 0 /* XP_MAC */
    PRArena *a = pool->first.next;

    while (a) {
        reallocSmaller(a, a->avail - (pruword)a);
        a->limit = a->avail;
        a = a->next;
    }
#endif
}

PR_PUBLIC_API(void)
PR_ArenaFinish()
{
    PRArena *a, *next;

#ifdef JS_THREADSAFE
    while (arena_freelist) {
	a = arena_freelist;
	next = a->next;
	if (js_CompareAndSwap((prword*)&arena_freelist,(prword)a,(prword)next))
	    free(a);
    }
#else
    for (a = arena_freelist; a; a = next) {
        next = a->next;
        free(a);
    }
    arena_freelist = NULL;
#endif
}

#ifdef PR_ARENAMETER
PR_PUBLIC_API(void)
PR_ArenaCountAllocation(PRArenaPool *pool, size_t nb)
{
    pool->stats.nallocs++;
    pool->stats.nbytes += nb;
    if (nb > pool->stats.maxalloc)
        pool->stats.maxalloc = nb;
    pool->stats.variance += nb * nb;
}

PR_PUBLIC_API(void)
PR_ArenaCountInplaceGrowth(PRArenaPool *pool, size_t size, size_t incr)
{
    pool->stats.ninplace++;
}

PR_PUBLIC_API(void)
PR_ArenaCountGrowth(PRArenaPool *pool, size_t size, size_t incr)
{
    pool->stats.ngrows++;
    pool->stats.nbytes += incr;
    pool->stats.variance -= size * size;
    size += incr;
    if (size > pool->stats.maxalloc)
        pool->stats.maxalloc = size;
    pool->stats.variance += size * size;
}

PR_PUBLIC_API(void)
PR_ArenaCountRelease(PRArenaPool *pool, char *mark)
{
    pool->stats.nreleases++;
}

PR_PUBLIC_API(void)
PR_ArenaCountRetract(PRArenaPool *pool, char *mark)
{
    pool->stats.nfastrels++;
}

#include <math.h>
#include <stdio.h>

PR_PUBLIC_API(void)
PR_DumpArenaStats(FILE *fp)
{
    PRArenaStats *stats;
    double mean, variance;

    for (stats = arena_stats_list; stats; stats = stats->next) {
        if (stats->nallocs != 0) {
	    mean = (double)stats->nbytes / stats->nallocs;
	    variance = fabs(stats->variance / stats->nallocs - mean * mean);
	} else {
	    mean = variance = 0;
	}

        fprintf(fp, "\n%s allocation statistics:\n", stats->name);
        fprintf(fp, "              number of arenas: %u\n", stats->narenas);
        fprintf(fp, "         number of allocations: %u\n", stats->nallocs);
        fprintf(fp, " number of free arena reclaims: %u\n", stats->nreclaims);
        fprintf(fp, "        number of malloc calls: %u\n", stats->nmallocs);
        fprintf(fp, "       number of deallocations: %u\n", stats->ndeallocs);
        fprintf(fp, "  number of allocation growths: %u\n", stats->ngrows);
        fprintf(fp, "    number of in-place growths: %u\n", stats->ninplace);
        fprintf(fp, "number of released allocations: %u\n", stats->nreleases);
        fprintf(fp, "       number of fast releases: %u\n", stats->nfastrels);
        fprintf(fp, "         total bytes allocated: %u\n", stats->nbytes);
        fprintf(fp, "          mean allocation size: %g\n", mean);
        fprintf(fp, "            standard deviation: %g\n", sqrt(variance));
        fprintf(fp, "       maximum allocation size: %u\n", stats->maxalloc);
    }
}
#endif /* PR_ARENAMETER */

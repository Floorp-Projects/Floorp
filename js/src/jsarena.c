/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
 * Lifetime-based fast allocation, inspired by much prior art, including
 * "Fast Allocation and Deallocation of Memory Based on Object Lifetimes"
 * David R. Hanson, Software -- Practice and Experience, Vol. 20(1).
 */
#include "jsstddef.h"
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsbit.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jslock.h"

static JSArena *arena_freelist;

#ifdef JS_THREADSAFE
static JSLock *arena_freelist_lock;
#endif

#ifdef JS_ARENAMETER
static JSArenaStats *arena_stats_list;

#define COUNT(pool,what)  (pool)->stats.what++
#else
#define COUNT(pool,what)  /* nothing */
#endif

#define JS_ARENA_DEFAULT_ALIGN  sizeof(double)

JS_PUBLIC_API(void)
JS_InitArenaPool(JSArenaPool *pool, const char *name, JSUint32 size, JSUint32 align)
{
#ifdef JS_THREADSAFE
    /* Must come through here once in primordial thread to init safely! */
    if (!arena_freelist_lock) {
        arena_freelist_lock = JS_NEW_LOCK();
        JS_ASSERT(arena_freelist_lock);
    }
#endif
    if (align == 0)
	align = JS_ARENA_DEFAULT_ALIGN;
    pool->mask = JS_BITMASK(JS_CeilingLog2(align));
    pool->first.next = NULL;
    pool->first.base = pool->first.avail = pool->first.limit =
	JS_ARENA_ALIGN(pool, &pool->first + 1);
    pool->current = &pool->first;
    pool->arenasize = size;
#ifdef JS_ARENAMETER
    memset(&pool->stats, 0, sizeof pool->stats);
    pool->stats.name = strdup(name);
    pool->stats.next = arena_stats_list;
    arena_stats_list = &pool->stats;
#endif
}

JS_PUBLIC_API(void *)
JS_ArenaAllocate(JSArenaPool *pool, JSUint32 nb)
{
    JSArena **ap, *a, *b;
    JSUint32 sz;
    void *p;

    JS_ASSERT((nb & pool->mask) == 0);
    for (a = pool->current; a->avail + nb > a->limit; pool->current = a) {
        if (!a->next) {
            ap = &arena_freelist;
            JS_ACQUIRE_LOCK(arena_freelist_lock);
            while ((b = *ap) != NULL) {         /* reclaim a free arena */
                /*
                 * Insist on exact arenasize match if nb is not greater than
                 * arenasize.  Otherwise take any arena big enough, but not by
                 * more than nb + arenasize.
                 */
                sz = (JSUint32)(b->limit - b->base);
                if ((nb > pool->arenasize)
                    ? sz >= nb && sz <= nb + pool->arenasize
                    : sz == pool->arenasize) {
                    *ap = b->next;
                    JS_RELEASE_LOCK(arena_freelist_lock);
                    b->next = NULL;
                    a = a->next = b;
                    COUNT(pool, nreclaims);
                    goto claim;
                }
                ap = &b->next;
            }
            JS_RELEASE_LOCK(arena_freelist_lock);
            sz = JS_MAX(pool->arenasize, nb);   /* allocate a new arena */
            sz += sizeof *a + pool->mask;       /* header and alignment slop */
            b = (JSArena *) malloc(sz);
            if (!b)
                return 0;
            a = a->next = b;
            a->next = NULL;
            a->limit = (jsuword)a + sz;
            JS_COUNT_ARENA(pool,++);
            COUNT(pool, nmallocs);
        claim:
            a->base = a->avail = JS_ARENA_ALIGN(pool, a + 1);
            continue;
        }
        a = a->next;                            /* move to next arena */
    }
    p = (void *)a->avail;
    a->avail += nb;
    return p;
}

JS_PUBLIC_API(void *)
JS_ArenaRealloc(JSArenaPool *pool, void *p, JSUint32 size, JSUint32 incr)
{
    JSArena **ap, *a;
    jsuword boff, aoff, newsize;

    ap = &pool->first.next;
    while ((a = *ap) != pool->current)
        ap = &a->next;
    JS_ASSERT(a->base == (jsuword)p);
    boff = JS_UPTRDIFF(a->base, a);
    aoff = newsize = size + incr;
    JS_ASSERT(newsize > pool->arenasize);
    newsize += sizeof *a + pool->mask;          /* header and alignment slop */
    a = (JSArena *) realloc(a, newsize);
    if (!a)
        return NULL;
    *ap = a;
    pool->current = a;
#ifdef JS_ARENAMETER
    pool->stats.nreallocs++;
#endif
    a->base = JS_ARENA_ALIGN(pool, a + 1);
    a->limit = (jsuword)a + newsize;
    a->avail = JS_ARENA_ALIGN(pool, a->base + aoff);

    /* Check whether realloc aligned differently, and copy if necessary. */
    if (boff != JS_UPTRDIFF(a->base, a))
        memmove((void *)a->base, (char *)a + boff, size);
    return (void *)a->base;
}

JS_PUBLIC_API(void *)
JS_ArenaGrow(JSArenaPool *pool, void *p, JSUint32 size, JSUint32 incr)
{
    void *newp;

    JS_ARENA_ALLOCATE(newp, pool, size + incr);
    if (newp)
        memcpy(newp, p, size);
    return newp;
}

/*
 * Free tail arenas linked after head, which may not be the true list head.
 * Reset pool->current to point to head in case it pointed at a tail arena.
 */
static void
FreeArenaList(JSArenaPool *pool, JSArena *head, JSBool reallyFree)
{
    JSArena **ap, *a;

    ap = &head->next;
    a = *ap;
    if (!a)
	return;

#ifdef DEBUG
    do {
	JS_ASSERT(a->base <= a->avail && a->avail <= a->limit);
	a->avail = a->base;
	JS_CLEAR_UNUSED(a);
    } while ((a = a->next) != NULL);
    a = *ap;
#endif

    if (reallyFree) {
	do {
	    *ap = a->next;
	    JS_CLEAR_ARENA(a);
	    JS_COUNT_ARENA(pool,--);
	    free(a);
	} while ((a = *ap) != NULL);
    } else {
	/* Insert the whole arena chain at the front of the freelist. */
	do {
	    ap = &(*ap)->next;
	} while (*ap);
        JS_ACQUIRE_LOCK(arena_freelist_lock);
	*ap = arena_freelist;
	arena_freelist = a;
        JS_RELEASE_LOCK(arena_freelist_lock);
	head->next = NULL;
    }

    pool->current = head;
}

JS_PUBLIC_API(void)
JS_ArenaRelease(JSArenaPool *pool, char *mark)
{
    JSArena *a;

    for (a = &pool->first; a; a = a->next) {
	if (JS_UPTRDIFF(mark, a->base) <= JS_UPTRDIFF(a->avail, a->base)) {
	    a->avail = JS_ARENA_ALIGN(pool, mark);
	    FreeArenaList(pool, a, JS_TRUE);
	    return;
	}
    }
}

JS_PUBLIC_API(void)
JS_FreeArenaPool(JSArenaPool *pool)
{
    FreeArenaList(pool, &pool->first, JS_FALSE);
    COUNT(pool, ndeallocs);
}

JS_PUBLIC_API(void)
JS_FinishArenaPool(JSArenaPool *pool)
{
    FreeArenaList(pool, &pool->first, JS_TRUE);
#ifdef JS_ARENAMETER
    {
	JSArenaStats *stats, **statsp;

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

JS_PUBLIC_API(void)
JS_CompactArenaPool(JSArenaPool *pool)
{
#if 0 /* XP_MAC */
    JSArena *a = pool->first.next;

    while (a) {
        reallocSmaller(a, a->avail - (jsuword)a);
        a->limit = a->avail;
        a = a->next;
    }
#endif
}

JS_PUBLIC_API(void)
JS_ArenaFinish()
{
    JSArena *a, *next;

    JS_ACQUIRE_LOCK(arena_freelist_lock);
    a = arena_freelist;
    arena_freelist = NULL;
    JS_RELEASE_LOCK(arena_freelist_lock);
    for (; a; a = next) {
        next = a->next;
        free(a);
    }
}

JS_PUBLIC_API(void)
JS_ArenaShutDown(void)
{
#ifdef JS_THREADSAFE
    /* Must come through here once in the process's last thread! */
    if (arena_freelist_lock) {
        JS_DESTROY_LOCK(arena_freelist_lock);
        arena_freelist_lock = NULL;
    }
#endif
}

#ifdef JS_ARENAMETER
JS_PUBLIC_API(void)
JS_ArenaCountAllocation(JSArenaPool *pool, JSUint32 nb)
{
    pool->stats.nallocs++;
    pool->stats.nbytes += nb;
    if (nb > pool->stats.maxalloc)
        pool->stats.maxalloc = nb;
    pool->stats.variance += nb * nb;
}

JS_PUBLIC_API(void)
JS_ArenaCountInplaceGrowth(JSArenaPool *pool, JSUint32 size, JSUint32 incr)
{
    pool->stats.ninplace++;
}

JS_PUBLIC_API(void)
JS_ArenaCountGrowth(JSArenaPool *pool, JSUint32 size, JSUint32 incr)
{
    pool->stats.ngrows++;
    pool->stats.nbytes += incr;
    pool->stats.variance -= size * size;
    size += incr;
    if (size > pool->stats.maxalloc)
        pool->stats.maxalloc = size;
    pool->stats.variance += size * size;
}

JS_PUBLIC_API(void)
JS_ArenaCountRelease(JSArenaPool *pool, char *mark)
{
    pool->stats.nreleases++;
}

JS_PUBLIC_API(void)
JS_ArenaCountRetract(JSArenaPool *pool, char *mark)
{
    pool->stats.nfastrels++;
}

#include <math.h>
#include <stdio.h>

JS_PUBLIC_API(void)
JS_DumpArenaStats(FILE *fp)
{
    JSArenaStats *stats;
    uint32 nallocs, nbytes;
    double mean, variance, sigma;

    for (stats = arena_stats_list; stats; stats = stats->next) {
        nallocs = stats->nallocs;
        if (nallocs != 0) {
            nbytes = stats->nbytes;
            mean = (double)nbytes / nallocs;
            variance = stats->variance * nallocs - nbytes * nbytes;
            if (variance < 0 || nallocs == 1)
                variance = 0;
            else
                variance /= nallocs * (nallocs - 1);
            sigma = sqrt(variance);
	} else {
	    mean = variance = sigma = 0;
	}

        fprintf(fp, "\n%s allocation statistics:\n", stats->name);
        fprintf(fp, "              number of arenas: %u\n", stats->narenas);
        fprintf(fp, "         number of allocations: %u\n", stats->nallocs);
        fprintf(fp, " number of free arena reclaims: %u\n", stats->nreclaims);
        fprintf(fp, "        number of malloc calls: %u\n", stats->nmallocs);
        fprintf(fp, "       number of deallocations: %u\n", stats->ndeallocs);
        fprintf(fp, "  number of allocation growths: %u\n", stats->ngrows);
        fprintf(fp, "    number of in-place growths: %u\n", stats->ninplace);
        fprintf(fp, " number of realloc'ing growths: %u\n", stats->nreallocs);
        fprintf(fp, "number of released allocations: %u\n", stats->nreleases);
        fprintf(fp, "       number of fast releases: %u\n", stats->nfastrels);
        fprintf(fp, "         total bytes allocated: %u\n", stats->nbytes);
        fprintf(fp, "          mean allocation size: %g\n", mean);
        fprintf(fp, "            standard deviation: %g\n", sigma);
        fprintf(fp, "       maximum allocation size: %u\n", stats->maxalloc);
    }
}
#endif /* JS_ARENAMETER */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

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
    JSArena **ap, **bp, *a, *b;
    JSUint32 extra, gross, sz;
    void *p;

    /*
     * An allocation that consumes more than pool->arenasize also has a footer
     * pointing back to its previous arena's next member.  This footer is not
     * included in [a->base, a->limit), so its space can't be wrongly claimed.
     */
    ap = NULL;
    JS_ASSERT((nb & pool->mask) == 0);
    extra = (nb > pool->arenasize) ? sizeof(JSArena **) : 0;
    gross = nb + extra;
    for (a = pool->current; a->avail + nb > a->limit; pool->current = a) {
        ap = &a->next;
        if (!*ap) {
            bp = &arena_freelist;
            JS_ACQUIRE_LOCK(arena_freelist_lock);
            while ((b = *bp) != NULL) {         /* reclaim a free arena */
                /*
                 * Insist on exact arenasize match if gross is not greater than
                 * arenasize.  Otherwise take any arena big enough, but not by
                 * more than gross + arenasize.
                 */
                sz = (JSUint32)(b->limit - b->base);
                if ((gross > pool->arenasize)
                    ? sz >= gross && sz <= gross + pool->arenasize
                    : sz == pool->arenasize) {
                    *bp = b->next;
                    JS_RELEASE_LOCK(arena_freelist_lock);
                    b->next = NULL;
                    COUNT(pool, nreclaims);
                    goto claim;
                }
                bp = &b->next;
            }
            JS_RELEASE_LOCK(arena_freelist_lock);
            sz = JS_MAX(pool->arenasize, nb);   /* allocate a new arena */
            sz += sizeof *a + pool->mask;       /* header and alignment slop */
            b = (JSArena *) malloc(sz + extra); /* footer if oversized load */
            if (!b)
                return 0;
            b->next = NULL;
            b->limit = (jsuword)b + sz;
            JS_COUNT_ARENA(pool,++);
            COUNT(pool, nmallocs);
        claim:
            *ap = a = b;
            a->base = a->avail = JS_ARENA_ALIGN(pool, a + 1);
            continue;
        }
        a = *ap;                                /* move to next arena */
    }
    p = (void *)a->avail;
    a->avail += nb;

    /*
     * If oversized, store ap in the footer, which lies at a->avail, but which
     * can't be overwritten by a further small allocation, because a->limit is
     * at most pool->mask bytes after a->avail, and no allocation can be fewer
     * than (pool->mask + 1) bytes.
     */
    if (extra && ap)
        *(JSArena ***)a->avail = ap;
    return p;
}

JS_PUBLIC_API(void *)
JS_ArenaRealloc(JSArenaPool *pool, void *p, JSUint32 size, JSUint32 incr)
{
    JSArena **ap, *a;
    jsuword boff, aoff, netsize, gross;

    /*
     * Use the oversized-single-allocation footer to avoid searching for ap.
     * See JS_ArenaAllocate, the extra variable.
     */
    if (size > pool->arenasize) {
        ap = *(JSArena ***)((jsuword)p + JS_ARENA_ALIGN(pool, size));
        a = *ap;
    } else {
        ap = &pool->first.next;
        while ((a = *ap) != pool->current)
            ap = &a->next;
    }
    JS_ASSERT(a->base == (jsuword)p);
    boff = JS_UPTRDIFF(a->base, a);
    aoff = netsize = size + incr;
    JS_ASSERT(netsize > pool->arenasize);
    netsize += sizeof *a + pool->mask;          /* header and alignment slop */
    gross = netsize + sizeof(JSArena **);       /* oversized footer holds ap */
    a = (JSArena *) realloc(a, gross);
    if (!a)
        return NULL;
    if (pool->current == *ap)
        pool->current = a;
    *ap = a;
#ifdef JS_ARENAMETER
    pool->stats.nreallocs++;
#endif
    a->base = JS_ARENA_ALIGN(pool, a + 1);
    a->limit = (jsuword)a + netsize;
    a->avail = JS_ARENA_ALIGN(pool, a->base + aoff);

    /* Check whether realloc aligned differently, and copy if necessary. */
    if (boff != JS_UPTRDIFF(a->base, a))
        memmove((void *)a->base, (char *)a + boff, size);

    /* Store ap in the oversized load footer. */
    *(JSArena ***)a->avail = ap;
    return (void *)a->base;
}

JS_PUBLIC_API(void *)
JS_ArenaGrow(JSArenaPool *pool, void *p, JSUint32 size, JSUint32 incr)
{
    void *newp;

    /*
     * If p points to an oversized allocation, it owns an entire arena, so we
     * can simply realloc the arena.
     */
    if (size > pool->arenasize)
        return JS_ArenaRealloc(pool, p, size, incr);

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
JS_ArenaFreeAllocation(JSArenaPool *pool, void *p, JSUint32 size)
{
    jsuword q;
    JSArena **ap, *a, *b;

    /*
     * If the allocation is oversized, it consumes an entire arena, and there
     * is a footer pointing back to its predecessor's next member.  Otherwise,
     * we have to search pool for a.
     */
    q = (jsuword)p + size;
    q = JS_ARENA_ALIGN(pool, q);
    if (size > pool->arenasize) {
        ap = *(JSArena ***)q;
        a = *ap;
    } else {
        ap = &pool->first.next;
        while ((a = *ap) != NULL) {
            if (a->avail == q) {
                /*
                 * If a is consumed by the allocation at p, we can free it to
                 * the malloc heap.
                 */
                if (a->base == (jsuword)p)
                    break;

                /*
                 * We can't free a, but we can "retract" its avail cursor --
                 * whether there are others after it in pool.
                 */
                a->avail = (jsuword)p;
                return;
            }
            ap = &a->next;
        }
    }

    /*
     * At this point, a is doomed, so ensure that pool->current doesn't point
     * at it.  What's more, force future allocations to scavenge all arenas on
     * pool, in case some have free space.
     */
    if (pool->current == a)
        pool->current = &pool->first;

    /*
     * This is a non-LIFO deallocation, so take care to fix up a->next's back
     * pointer in its footer, if a->next is oversized.
     */
    *ap = b = a->next;
    if (b && b->avail - b->base > pool->arenasize) {
        JS_ASSERT(*(JSArena ***)b->avail == &a->next);
        *(JSArena ***)b->avail = ap;
    }
    JS_CLEAR_ARENA(a);
    JS_COUNT_ARENA(pool,--);
    free(a);
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

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Lifetime-based fast allocation, inspired by much prior art, including
 * "Fast Allocation and Deallocation of Memory Based on Object Lifetimes"
 * David R. Hanson, Software -- Practice and Experience, Vol. 20(1).
 */
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsbit.h"
#include "jsarenainlines.h"
#include "jsutil.h"

#ifdef DEBUG
static JSArenaStats *arena_stats_list;
#define COUNT(pool,what)  (pool)->stats.what++
#else
#define COUNT(pool,what)  /* nothing */
#endif /* DEBUG */

#define JS_ARENA_DEFAULT_ALIGN  sizeof(double)

void
JSArenaPool::init(const char *name, size_t size, size_t align, size_t *quotap)
{
    if (align == 0)
        align = JS_ARENA_DEFAULT_ALIGN;
    mask = JS_BITMASK(JS_CeilingLog2(align));
    first.next = NULL;
    first.base = first.avail = first.limit = this->align(jsuword(&first + 1));
    current = &first;
    arenasize = size;
    this->quotap = quotap;
#ifdef DEBUG
    stats.init(name, arena_stats_list);
    arena_stats_list = &stats;
#endif
}

/*
 * An allocation that consumes more than pool->arenasize also has a header
 * pointing back to its previous arena's next member.  This header is not
 * included in [a->base, a->limit), so its space can't be wrongly claimed.
 *
 * As the header is a pointer, it must be well-aligned.  If pool->mask is
 * greater than or equal to POINTER_MASK, the header just preceding a->base
 * for an oversized arena a is well-aligned, because a->base is well-aligned.
 * However, we may need to add more space to pad the JSArena ** back-pointer
 * so that it lies just behind a->base, because a might not be aligned such
 * that (jsuword)(a + 1) is on a pointer boundary.
 *
 * By how much must we pad?  Let M be the alignment modulus for pool and P
 * the modulus for a pointer.  Given M >= P, the base of an oversized arena
 * that satisfies M is well-aligned for P.
 *
 * On the other hand, if M < P, we must include enough space in the header
 * size to align the back-pointer on a P boundary so that it can be found by
 * subtracting P from a->base.  This means a->base must be on a P boundary,
 * even though subsequent allocations from a may be aligned on a lesser (M)
 * boundary.  Given powers of two M and P as above, the extra space needed
 * when M < P is P-M or POINTER_MASK - pool->mask.
 *
 * The size of a header including padding is given by the headerSize method
 * for any pool (for any value of M).
 */

void *
JSArenaPool::allocate(size_t nb, bool limitCheck)
{
    countAllocation(nb);
    size_t alignedNB = align(nb);
    jsuword p = current->avail;
    /*
     * NB: always subtract nb from limit rather than adding nb to p to avoid overflowing a
     * 32-bit address space. See bug 279273.
     */
    if ((limitCheck && alignedNB > current->limit) || p > current->limit - alignedNB)
        p = jsuword(allocateInternal(alignedNB));
    else
        current->avail = p + alignedNB;
    return (void *) p;
}

void *
JSArenaPool::allocateInternal(size_t nb)
{
    /*
     * Search pool from current forward till we find or make enough space.
     *
     * NB: subtract nb from a->limit in the loop condition, instead of adding
     * nb to a->avail, to avoid overflowing a 32-bit address space (possible
     * when running a 32-bit program on a 64-bit system where the kernel maps
     * the heap up against the top of the 32-bit address space).
     *
     * Thanks to Juergen Kreileder <jk@blackdown.de>, who brought this up in
     * https://bugzilla.mozilla.org/show_bug.cgi?id=279273.
     */
    JS_ASSERT((nb & mask) == 0);
    JSArena *a;
    for (a = current; nb > a->limit || a->avail > a->limit - nb; current = a) {
        JSArena **ap = &a->next;
        if (!*ap) {
            /* Not enough space in pool, so we must malloc. */
            jsuword extra = (nb > arenasize) ? headerSize() : 0;
            jsuword hdrsz = sizeof *a + extra + mask;
            jsuword gross = hdrsz + JS_MAX(nb, arenasize);
            if (gross < nb)
                return NULL;
            JSArena *b;
            if (quotap) {
                if (gross > *quotap)
                    return NULL;
                b = (JSArena *) js_malloc(gross);
                if (!b)
                    return NULL;
                *quotap -= gross;
            } else {
                b = (JSArena *) js_malloc(gross);
                if (!b)
                    return NULL;
            }

            b->next = NULL;
            b->limit = (jsuword)b + gross;
            incArenaCount();
            COUNT(this, nmallocs);

            /* If oversized, store ap in the header, just before a->base. */
            *ap = a = b;
            JS_ASSERT(gross <= JS_UPTRDIFF(a->limit, a));
            if (extra) {
                a->base = a->avail =
                    ((jsuword)a + hdrsz) & ~headerBaseMask();
                setHeader(a, ap);
            } else {
                a->base = a->avail = align(jsuword(a + 1));
            }
            continue;
        }
        a = *ap;                                /* move to next arena */
    }

    void *p = (void *) a->avail;
    a->avail += nb;
    JS_ASSERT(a->base <= a->avail && a->avail <= a->limit);
    return p;
}

void *
JSArenaPool::reallocInternal(void *p, size_t size, size_t incr)
{
    JSArena **ap, *a, *b;
    jsuword boff, aoff, extra, hdrsz, gross, growth;

    /*
     * Use the oversized-single-allocation header to avoid searching for ap.
     * See JS_ArenaAllocate, the SET_HEADER call.
     */
    if (size > arenasize) {
        ap = *ptrToHeader(p);
        a = *ap;
    } else {
        ap = &first.next;
        while ((a = *ap) != current)
            ap = &a->next;
    }

    JS_ASSERT(a->base == (jsuword)p);
    boff = JS_UPTRDIFF(a->base, a);
    aoff = align(size + incr);
    JS_ASSERT(aoff > arenasize);
    extra = headerSize();                  /* oversized header holds ap */
    hdrsz = sizeof *a + extra + mask;     /* header and alignment slop */
    gross = hdrsz + aoff;
    JS_ASSERT(gross > aoff);
    if (quotap) {
        growth = gross - (a->limit - (jsuword) a);
        if (growth > *quotap)
            return NULL;
        a = (JSArena *) js_realloc(a, gross);
        if (!a)
            return NULL;
        *quotap -= growth;
    } else {
        a = (JSArena *) js_realloc(a, gross);
        if (!a)
            return NULL;
    }
    incReallocCount();

    if (a != *ap) {
        /* Oops, realloc moved the allocation: update other pointers to a. */
        if (current == *ap)
            current = a;
        b = a->next;
        if (b && b->avail - b->base > arenasize) {
            JS_ASSERT(getHeader(b) == &(*ap)->next);
            setHeader(b, &a->next);
        }

        /* Now update *ap, the next link of the arena before a. */
        *ap = a;
    }

    a->base = ((jsuword)a + hdrsz) & ~headerBaseMask();
    a->limit = (jsuword)a + gross;
    a->avail = a->base + aoff;
    JS_ASSERT(a->base <= a->avail && a->avail <= a->limit);

    /* Check whether realloc aligned differently, and copy if necessary. */
    if (boff != JS_UPTRDIFF(a->base, a))
        memmove((void *) a->base, (char *) a + boff, size);

    /* Store ap in the oversized-load arena header. */
    setHeader(a, ap);
    return (void *) a->base;
}

void
JSArenaPool::finish()
{
    freeArenaList(&first);
#ifdef DEBUG
    {
        JSArenaStats *stats, **statsp;

        this->stats.finish();
        for (statsp = &arena_stats_list; (stats = *statsp) != 0;
             statsp = &stats->next) {
            if (stats == &this->stats) {
                *statsp = stats->next;
                return;
            }
        }
    }
#endif
}

#ifdef DEBUG
void
JSArenaPool::countAllocation(size_t nb)
{
    stats.nallocs++;
    stats.nbytes += nb;
    if (nb > stats.maxalloc)
        stats.maxalloc = nb;
    stats.variance += nb * nb;
}

void
JSArenaPool::countGrowth(size_t size, size_t incr)
{
    stats.ngrows++;
    stats.nbytes += incr;
    stats.variance -= size * size;
    size += incr;
    if (size > stats.maxalloc)
        stats.maxalloc = size;
    stats.variance += size * size;
}

JS_FRIEND_API(void)
JS_DumpArenaStats()
{
    const char *filename = getenv("JS_ARENA_STATFILE");
    if (!filename)
        return;
    FILE *arenaStatFile = strcmp(filename, "stdout")
                          ? stdout : strcmp(filename, "stderr")
                          ? stderr : fopen(filename, "w");
    for (const JSArenaStats *stats = arena_stats_list; stats; stats = stats->getNext())
        stats->dump(arenaStatFile);
    fclose(arenaStatFile);
}

void
JSArenaStats::dump(FILE *fp) const
{
    double sigma;
    double mean = JS_MeanAndStdDev(nallocs, nbytes, variance, &sigma);

    fprintf(fp, "\n%s allocation statistics:\n", name);
    fprintf(fp, "              number of arenas: %u\n", narenas);
    fprintf(fp, "         number of allocations: %u\n", nallocs);
    fprintf(fp, "        number of malloc calls: %u\n", nmallocs);
    fprintf(fp, "       number of deallocations: %u\n", ndeallocs);
    fprintf(fp, "  number of allocation growths: %u\n", ngrows);
    fprintf(fp, "    number of in-place growths: %u\n", ninplace);
    fprintf(fp, " number of realloc'ing growths: %u\n", nreallocs);
    fprintf(fp, "number of released allocations: %u\n", nreleases);
    fprintf(fp, "       number of fast releases: %u\n", nfastrels);
    fprintf(fp, "         total bytes allocated: %u\n", unsigned(nbytes));
    fprintf(fp, "          mean allocation size: %g\n", mean);
    fprintf(fp, "            standard deviation: %g\n", sigma);
    fprintf(fp, "       maximum allocation size: %u\n", unsigned(maxalloc));
}
#endif /* DEBUG */

/* Backwards compatibility. */

JS_FRIEND_API(void *)
JS_ARENA_MARK(const JSArenaPool *pool)
{
    return pool->getMark();
}

JS_FRIEND_API(void)
JS_ARENA_RELEASE(JSArenaPool *pool, void *mark)
{
    pool->release(mark);
}

JS_FRIEND_API(void *)
JS_ARENA_ALLOCATE_COMMON_SANE(jsuword p, JSArenaPool *pool, size_t nb, bool limitCheck)
{
    return pool->allocate(nb, limitCheck);
}

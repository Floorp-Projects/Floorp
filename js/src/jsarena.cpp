/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "jsalloc.h"
#include "jstypes.h"
#include "jsstdint.h"
#include "jsbit.h"
#include "jsarena.h"
#include "jsprvtd.h"

using namespace js;

/* If JSArena's length is a multiple of 8, that ensures its payload is 8-aligned. */
JS_STATIC_ASSERT(sizeof(JSArena) % 8 == 0);

JS_PUBLIC_API(void)
JS_InitArenaPool(JSArenaPool *pool, const char *name, size_t size, size_t align)
{
    /* Restricting ourselves to some simple alignments keeps things simple. */
    if (align == 1 || align == 2 || align == 4 || align == 8) {
        pool->mask = align - 1;
    } else {
        /* This shouldn't happen, but set pool->mask reasonably if it does. */
        JS_NOT_REACHED("JS_InitArenaPool: bad align");
        pool->mask = 7;
    }
    pool->first.next = NULL;
    /* pool->first is a zero-sized dummy arena that's never allocated from. */
    pool->first.base = pool->first.avail = pool->first.limit =
        JS_ARENA_ALIGN(pool, &pool->first + 1);
    pool->current = &pool->first;
    pool->arenasize = size;
}

JS_PUBLIC_API(void *)
JS_ArenaAllocate(JSArenaPool *pool, size_t nb)
{
    /*
     * Search pool from current forward till we find or make enough space.
     *
     * NB: subtract nb from a->limit in the loop condition, instead of adding
     * nb to a->avail, to avoid overflow (possible when running a 32-bit
     * program on a 64-bit system where the kernel maps the heap up against the
     * top of the 32-bit address space, see bug 279273).  Note that this
     * necessitates a comparison between nb and a->limit that looks like a
     * (conceptual) type error but isn't.
     */
    JS_ASSERT((nb & pool->mask) == 0);
    JSArena *a;
    /*
     * Comparing nb to a->limit looks like a (conceptual) type error, but it's
     * necessary to avoid wrap-around.  Yuk.
     */
    for (a = pool->current; nb > a->limit || a->avail > a->limit - nb; pool->current = a) {
        JSArena **ap = &a->next;
        if (!*ap) {
            /* Not enough space in pool, so we must malloc. */
            size_t gross = sizeof(JSArena) + JS_MAX(nb, pool->arenasize);
            a = (JSArena *) OffTheBooks::malloc_(gross);
            if (!a)
                return NULL;

            a->next = NULL;
            a->base = a->avail = jsuword(a) + sizeof(JSArena);
            /*
             * Because malloc returns 8-aligned pointers and sizeof(JSArena) is
             * a multiple of 8, a->base will always be 8-aligned, which should
             * suffice for any valid pool.
             */
            JS_ASSERT(a->base == JS_ARENA_ALIGN(pool, a->base));
            a->limit = (jsuword)a + gross;

            *ap = a;
            continue;
        }
        a = *ap;        /* move to next arena */
    }

    void* p = (void *)a->avail;
    a->avail += nb;
    JS_ASSERT(a->base <= a->avail && a->avail <= a->limit);
    return p;
}

JS_PUBLIC_API(void *)
JS_ArenaRealloc(JSArenaPool *pool, void *p, size_t size, size_t incr)
{
    /* If we've called JS_ArenaRealloc, the new size must be bigger than pool->arenasize. */
    JS_ASSERT(size + incr > pool->arenasize);

    /* Find the arena containing |p|. */
    JSArena *a;
    JSArena **ap = &pool->first.next;
    while (true) {
        a = *ap;
        if (JS_IS_IN_ARENA(a, p))
            break;
        JS_ASSERT(a != pool->current);
        ap = &a->next;
    }
    /* If we've called JS_ArenaRealloc, p must be at the start of an arena. */
    JS_ASSERT(a->base == jsuword(p));

    size_t gross = sizeof(JSArena) + JS_ARENA_ALIGN(pool, size + incr);
    a = (JSArena *) OffTheBooks::realloc_(a, gross);
    if (!a)
        return NULL;

    a->base = jsuword(a) + sizeof(JSArena);
    a->avail = a->limit = jsuword(a) + gross;
    /*
     * Because realloc returns 8-aligned pointers and sizeof(JSArena) is a
     * multiple of 8, a->base will always be 8-aligned, which should suffice
     * for any valid pool.
     */
    JS_ASSERT(a->base == JS_ARENA_ALIGN(pool, a->base));

    if (a != *ap) {
        /* realloc moved the allocation: update other pointers to a. */
        if (pool->current == *ap)
            pool->current = a;
        *ap = a;
    }

    return (void *)a->base;
}

JS_PUBLIC_API(void *)
JS_ArenaGrow(JSArenaPool *pool, void *p, size_t size, size_t incr)
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
FreeArenaList(JSArenaPool *pool, JSArena *head)
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

    do {
        *ap = a->next;
        JS_CLEAR_ARENA(a);
        UnwantedForeground::free_(a);
    } while ((a = *ap) != NULL);

    pool->current = head;
}

JS_PUBLIC_API(void)
JS_ArenaRelease(JSArenaPool *pool, char *mark)
{
    JSArena *a;

    for (a = &pool->first; a; a = a->next) {
        JS_ASSERT(a->base <= a->avail && a->avail <= a->limit);

        if (JS_IS_IN_ARENA(a, mark)) {
            a->avail = JS_ARENA_ALIGN(pool, mark);
            JS_ASSERT(a->avail <= a->limit);
            FreeArenaList(pool, a);
            return;
        }
    }
}

JS_PUBLIC_API(void)
JS_FreeArenaPool(JSArenaPool *pool)
{
    FreeArenaList(pool, &pool->first);
}

JS_PUBLIC_API(void)
JS_FinishArenaPool(JSArenaPool *pool)
{
    FreeArenaList(pool, &pool->first);
}

JS_PUBLIC_API(void)
JS_ArenaFinish()
{
}

JS_PUBLIC_API(void)
JS_ArenaShutDown(void)
{
}

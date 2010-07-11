/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef jsarena_h___
#define jsarena_h___
/*
 * Lifetime-based fast allocation, inspired by much prior art, including
 * "Fast Allocation and Deallocation of Memory Based on Object Lifetimes"
 * David R. Hanson, Software -- Practice and Experience, Vol. 20(1).
 *
 * Also supports LIFO allocation (JS_ARENA_MARK/JS_ARENA_RELEASE).
 */
#include <stdlib.h>
#include <memory.h>
#ifdef DEBUG
#include <stdio.h>
#endif
#include "jstypes.h"
#include "jscompat.h"
#include "jsutil.h"

#ifdef DEBUG
const uint8_t JS_FREE_PATTERN = 0xDA;
#endif

#define JS_UPTRDIFF(p,q)        (jsuword(p) - jsuword(q))

struct JSArena
{
    JSArena     *next;          /* next arena for this lifetime */
    jsuword     base;           /* aligned base address, follows this header */
    jsuword     limit;          /* one beyond last byte in arena */
    jsuword     avail;          /* points to next available byte */

  public:
    jsuword getBase() const { return base; }
    jsuword getLimit() const { return limit; }
    jsuword getAvail() const { return avail; }
    void setAvail(jsuword newAvail) { avail = newAvail; }
    const JSArena *getNext() const { return next; }
    JSArena *getNext() { return next; }
    void munge(JSArena *next) { this-> next = next; }
    void munge(JSArena *next, jsuword base, jsuword avail, jsuword limit) {
        this->next = next;
        this->base = base;
        this->avail = avail;
        this->limit = limit;
    }

    /*
     * Check if the mark is inside arena's allocated area.
     */
    bool markMatch(void *mark) const {
        return JS_UPTRDIFF(mark, base) <= JS_UPTRDIFF(avail, base);
    }

#ifdef DEBUG
    void clear() { memset(this, JS_FREE_PATTERN, limit - jsuword(this)); }

    void clearUnused() {
        JS_ASSERT(avail <= limit);
        memset((void *) avail, JS_FREE_PATTERN, limit - avail);
    }
#else
    void clear() {}
    void clearUnused() {}
#endif

    friend class JSArenaPool;
};

#ifdef DEBUG
class JSArenaStats
{
    JSArenaStats *next;         /* next in arenaStats list */
    char        *name;          /* name for debugging */
    uint32      narenas;        /* number of arenas in pool */
    uint32      nallocs;        /* number of JS_ARENA_ALLOCATE() calls */
    uint32      nmallocs;       /* number of malloc() calls */
    uint32      ndeallocs;      /* number of lifetime deallocations */
    uint32      ngrows;         /* number of JS_ARENA_GROW() calls */
    uint32      ninplace;       /* number of in-place growths */
    uint32      nreallocs;      /* number of arena grow extending reallocs */
    uint32      nreleases;      /* number of JS_ARENA_RELEASE() calls */
    uint32      nfastrels;      /* number of "fast path" releases */
    size_t      nbytes;         /* total bytes allocated */
    size_t      maxalloc;       /* maximum allocation size in bytes */
    double      variance;       /* size variance accumulator */

  public:
    void init(const char *name, JSArenaStats *next) {
        memset(this, 0, sizeof *this);
        this->name = strdup(name);
        this->next = next;
    }
    void finish() { free(name); name = 0; }
    void dump(FILE *fp) const;
    const JSArenaStats *getNext() const { return next; }

    friend class JSArenaPool;
};

JS_FRIEND_API(void)
JS_DumpArenaStats();
#endif /* DEBUG */

class JSArenaPool
{
    JSArena     first;          /* first arena in pool list */
    JSArena     *current;       /* arena from which to allocate space */
    size_t      arenasize;      /* net exact size of a new arena */
    jsuword     mask;           /* alignment mask (power-of-2 - 1) */
    size_t      *quotap;        /* pointer to the quota on pool allocation
                                   size or null if pool is unlimited */
#ifdef DEBUG
    JSArenaStats stats;
#endif

  public:
    /*
     * Initialize an arena pool with a minimum size per arena of size bytes.
     */
    void init(const char *name, size_t size, size_t align, size_t *quotap);

    /*
     * Free the arenas in pool and finish using it altogether.
     */
    void finish();

    /*
     * Free the arenas in pool.  The user may continue to allocate from pool
     * after calling this function.  There is no need to call |init|
     * again unless JS_FinishArenaPool(pool) has been called.
     */
    void free() {
        freeArenaList(&first);
        incDeallocCount();
    }

    void *getMark() const { return (void *) current->avail; }
    jsuword align(jsuword n) const { return (n + mask) & ~mask; }
    jsuword align(void *n) const { return align(jsuword(n)); }
    bool currentIsFirst() const { return current == &first; }
    const JSArena *getFirst() const { return &first; }
    const JSArena *getSecond() const { return first.next; }
    const JSArena *getCurrent() const { return current; }
    JSArena *getFirst() { return &first; }
    JSArena *getSecond() { return first.next; }
    JSArena *getCurrent() { return current; }

    void *allocate(size_t nb, bool limitCheck);

    template <typename T>
    void allocateCast(T &p, size_t nb) {
        p = (T) allocate(nb, true);
    }

    template <typename T>
    void allocateType(T *&p) {
        p = (T *) allocate(sizeof(T), false);
    }

    void allocate(void *&p, size_t nb) { allocateCast<void *>(p, nb); }

    void *grow(jsuword p, size_t size, size_t incr);

    template <typename T>
    void growCast(T &p, size_t size, size_t incr) {
        p = (T) grow(jsuword(p), size, incr);
    }

    void release(void *mark) {
        if (current != &first && current->markMatch(mark)) {
            current->avail = jsuword(align(mark));
            JS_ASSERT(current->avail <= current->limit);
            current->clearUnused();
            countRetract(mark);
        } else {
            releaseInternal(mark);
        }
        countRelease(mark);
    }

    JSArena *destroy(JSArena *&a) {
        decArenaCount();
        if (current == a)
            current = &first;
        JSArena *next = a->next;
        a->clear();
        ::free(a);
        a = NULL;
        return next;
    }

    class ScopedExtension {
        JSArenaPool * const parent;
        JSArena     * const prevTop;
        JSArena     * const pushed;
      public:
        ScopedExtension(JSArenaPool *parent, JSArena *toPush)
          : parent(parent), prevTop(parent->getCurrent()), pushed(toPush) {
            JS_ASSERT(!prevTop->getNext());
            JS_ASSERT(!toPush->getNext());
            JS_ASSERT(prevTop != toPush);
            parent->current = prevTop->next = toPush;
        }
        ~ScopedExtension() {
            JS_ASSERT(!pushed->next);
            JS_ASSERT(prevTop->next == pushed);
            JS_ASSERT(parent->getCurrent() == pushed);
            parent->current = prevTop;
            prevTop->next = NULL;
        }
    };

  private:
    static const jsuword POINTER_MASK;

    /*
     * The mask to align |base|.
     */
    jsuword headerBaseMask() const;

    jsuword headerSize() const;

    /*
     * Compute the address of the back-pointer, given an oversized allocation at |p|.
     * |p| must be |a->base| for the arena |a| that contains |p|.
     */
    JSArena ***ptrToHeader(void *p) const {
        JS_ASSERT((jsuword(p) & headerBaseMask()) == 0);
        return (JSArena ***)(p) - 1;
    }
    JSArena ***ptrToHeader(jsuword p) const { return ptrToHeader((void *) p); }

    /*
     * |getHeader| and |setHeader| operate on an oversized arena.
     */
    JSArena **getHeader(const JSArena *a) const { return *ptrToHeader(a->base); }
    JSArena **setHeader(JSArena *a, JSArena **ap) const { return *ptrToHeader(a->base) = ap; }

    void *allocateInternal(size_t nb);
    void *reallocInternal(void *p, size_t size, size_t incr);

    void releaseInternal(void *mark) {
        for (JSArena *a = &first; a; a = a->next) {
            JS_ASSERT(a->base <= a->avail && a->avail <= a->limit);

            if (a->markMatch(mark)) {
                a->avail = align(mark);
                JS_ASSERT(a->avail <= a->limit);
                freeArenaList(a);
                return;
            }
        }
    }

    void *growInternal(void *p, size_t size, size_t incr) {
        /*
         * If p points to an oversized allocation, it owns an entire arena, so we
         * can simply realloc the arena.
         */
        if (size > arenasize)
            return reallocInternal(p, size, incr);

        void *newp = allocate(size + incr, true);
        if (newp)
            memcpy(newp, p, size);
        return newp;
    }

    /*
     * Free tail arenas linked after head, which may not be the true list head.
     * Reset current to point to head in case it pointed at a tail arena.
     */
    void freeArenaList(JSArena *head) {
        JSArena **ap = &head->next;
        JSArena *a = *ap;
        if (!a)
            return;

#ifdef DEBUG
        do {
            JS_ASSERT(a->base <= a->avail);
            JS_ASSERT(a->avail <= a->limit);
            a->avail = a->base;
            a->clearUnused();
        } while ((a = a->next) != NULL);
        a = *ap;
#endif

        do {
            *ap = a->next;
            if (quotap)
                *quotap += a->limit - (jsuword) a;
            a->clear();
            decArenaCount();
            js_free(a);
        } while ((a = *ap) != NULL);

        current = head;
    }

    /* Counters. */
#ifdef DEBUG
    void countAllocation(size_t nb);
    void countInplaceGrowth(size_t size, size_t incr) { stats.ninplace++; }
    void countGrowth(size_t size, size_t incr);
    void countRelease(void *mark) { stats.nreleases++; }
    void countRetract(void *mark) { stats.nfastrels++; }
    void incArenaCount() { stats.narenas++; }
    void decArenaCount() { stats.narenas--; }
    void incDeallocCount() { stats.ndeallocs++; }
    void incReallocCount() { stats.nreallocs++; }
#else
    void countAllocation(size_t nb) {}
    void countInplaceGrowth(size_t size, size_t incr) {}
    void countGrowth(size_t size, size_t incr) {}
    void countRelease(void *mark) {}
    void countRetract(void *mark) {}
    void incArenaCount() {}
    void decArenaCount() {}
    void incDeallocCount() {}
    void incReallocCount() {}
#endif
};

/* Backwards compatibility. */

JS_BEGIN_EXTERN_C

JS_FRIEND_API(void *)
JS_ARENA_MARK(const JSArenaPool *pool);

JS_FRIEND_API(void)
JS_ARENA_RELEASE(JSArenaPool *pool, void *mark);

JS_FRIEND_API(void *)
JS_ARENA_ALLOCATE_COMMON_SANE(jsuword p, JSArenaPool *pool, size_t nb, bool limitCheck);

#define JS_ARENA_ALLOCATE_CAST(p, type, pool, nb)                                   \
    JS_BEGIN_MACRO                                                                  \
        (p) = (type) JS_ARENA_ALLOCATE_COMMON_SANE(jsuword(p), (pool), (nb), true); \
    JS_END_MACRO
    

JS_END_EXTERN_C

#endif /* jsarena_h___ */

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Leary <cdleary@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef LifoAlloc_h__
#define LifoAlloc_h__

/*
 * This data structure supports stacky LIFO allocation (mark/release and
 * LifoAllocScope). It does not maintain one contiguous segment; instead, it
 * maintains a bunch of linked memory segments. In order to prevent malloc/free
 * thrashing, unused segments are deallocated when garbage collection occurs.
 */

#include "jsutil.h"

#include "js/TemplateLib.h"

namespace js {

namespace detail {

static const size_t LIFO_ALLOC_ALIGN = 8;

JS_ALWAYS_INLINE
char *
AlignPtr(void *orig)
{
    typedef tl::StaticAssert<
        tl::FloorLog2<LIFO_ALLOC_ALIGN>::result == tl::CeilingLog2<LIFO_ALLOC_ALIGN>::result
    >::result _;

    char *result = (char *) ((uintptr_t(orig) + (LIFO_ALLOC_ALIGN - 1)) & (~LIFO_ALLOC_ALIGN + 1));
    JS_ASSERT(uintptr_t(result) % LIFO_ALLOC_ALIGN == 0);
    return result;
}

/* Header for a chunk of memory wrangled by the LifoAlloc. */
class BumpChunk
{
    char        *bump;          /* start of the available data */
    char        *limit;         /* end of the data */
    BumpChunk   *next_;         /* the next BumpChunk */
    size_t      bumpSpaceSize;  /* size of the data area */

    char *headerBase() { return reinterpret_cast<char *>(this); }
    char *bumpBase() const { return limit - bumpSpaceSize; }

    BumpChunk *thisDuringConstruction() { return this; }

    explicit BumpChunk(size_t bumpSpaceSize)
      : bump(reinterpret_cast<char *>(thisDuringConstruction()) + sizeof(BumpChunk)),
        limit(bump + bumpSpaceSize),
        next_(NULL), bumpSpaceSize(bumpSpaceSize)
    {
        JS_ASSERT(bump == AlignPtr(bump));
    }

    void setBump(void *ptr) {
        JS_ASSERT(bumpBase() <= ptr);
        JS_ASSERT(ptr <= limit);
        DebugOnly<char *> prevBump = bump;
        bump = static_cast<char *>(ptr);
#ifdef DEBUG
        JS_ASSERT(contains(prevBump));

        /* Clobber the now-free space. */
        if (prevBump > bump)
            memset(bump, 0xcd, prevBump - bump);
#endif
    }

  public:
    BumpChunk *next() const { return next_; }
    void setNext(BumpChunk *succ) { next_ = succ; }

    size_t used() const { return bump - bumpBase(); }
    size_t sizeOf(JSUsableSizeFun usf) {
        size_t usable = usf((void*)this);
        return usable ? usable : limit - headerBase();
    }

    void resetBump() {
        setBump(headerBase() + sizeof(BumpChunk));
    }

    void *mark() const { return bump; }

    void release(void *mark) {
        JS_ASSERT(contains(mark));
        JS_ASSERT(mark <= bump);
        setBump(mark);
    }

    bool contains(void *mark) const {
        return bumpBase() <= mark && mark <= limit;
    }

    bool canAlloc(size_t n);
    bool canAllocUnaligned(size_t n);

    /* Try to perform an allocation of size |n|, return null if not possible. */
    JS_ALWAYS_INLINE
    void *tryAlloc(size_t n) {
        char *aligned = AlignPtr(bump);
        char *newBump = aligned + n;

        if (newBump > limit)
            return NULL;

        /* Check for overflow. */
        if (JS_UNLIKELY(newBump < bump))
            return NULL;

        JS_ASSERT(canAlloc(n)); /* Ensure consistency between "can" and "try". */
        setBump(newBump);
        return aligned;
    }

    void *tryAllocUnaligned(size_t n);

    void *allocInfallible(size_t n) {
        void *result = tryAlloc(n);
        JS_ASSERT(result);
        return result;
    }

    static BumpChunk *new_(size_t chunkSize);
    static void delete_(BumpChunk *chunk);
};

} /* namespace detail */

/*
 * LIFO bump allocator: used for phase-oriented and fast LIFO allocations.
 *
 * Note: |latest| is not necessary "last". We leave BumpChunks latent in the
 * chain after they've been released to avoid thrashing before a GC.
 */
class LifoAlloc
{
    typedef detail::BumpChunk BumpChunk;

    BumpChunk   *first;
    BumpChunk   *latest;
    size_t      markCount;
    size_t      defaultChunkSize_;

    void operator=(const LifoAlloc &);
    LifoAlloc(const LifoAlloc &);

    /* 
     * Return a BumpChunk that can perform an allocation of at least size |n|
     * and add it to the chain appropriately.
     *
     * Side effect: if retval is non-null, |first| and |latest| are initialized
     * appropriately.
     */
    BumpChunk *getOrCreateChunk(size_t n);

    void reset(size_t defaultChunkSize) {
        JS_ASSERT(RoundUpPow2(defaultChunkSize) == defaultChunkSize);
        first = latest = NULL;
        defaultChunkSize_ = defaultChunkSize;
        markCount = 0;
    }

  public:
    explicit LifoAlloc(size_t defaultChunkSize) { reset(defaultChunkSize); }

    /* Steal allocated chunks from |other|. */
    void steal(LifoAlloc *other) {
        JS_ASSERT(!other->markCount);
        PodCopy((char *) this, (char *) other, sizeof(*this));
        other->reset(defaultChunkSize_);
    }

    ~LifoAlloc() { freeAll(); }

    size_t defaultChunkSize() const { return defaultChunkSize_; }

    /* Frees all held memory. */
    void freeAll();

    /* Should be called on GC in order to release any held chunks. */
    void freeUnused();

    JS_ALWAYS_INLINE
    void *alloc(size_t n) {
        void *result;
        if (latest && (result = latest->tryAlloc(n)))
            return result;

        if (!getOrCreateChunk(n))
            return NULL;

        return latest->allocInfallible(n);
    }

    template <typename T>
    T *newArray(size_t count) {
        void *mem = alloc(sizeof(T) * count);
        if (!mem)
            return NULL;
        JS_STATIC_ASSERT(tl::IsPodType<T>::result);
        return (T *) mem;
    }

    /*
     * Create an array with uninitialized elements of type |T|.
     * The caller is responsible for initialization.
     */
    template <typename T>
    T *newArrayUninitialized(size_t count) {
        return static_cast<T *>(alloc(sizeof(T) * count));
    }

    void *mark() {
        markCount++;

        return latest ? latest->mark() : NULL;
    }

    void release(void *mark) {
        markCount--;

        if (!mark) {
            latest = first;
            if (latest)
                latest->resetBump();
            return;
        }

        /* 
         * Find the chunk that contains |mark|, and make sure we don't pass
         * |latest| along the way -- we should be making the chain of active
         * chunks shorter, not longer!
         */
        BumpChunk *container = first;
        while (true) {
            if (container->contains(mark))
                break;
            JS_ASSERT(container != latest);
            container = container->next();
        }
        latest = container;
        latest->release(mark);
    }

    /* Get the total "used" (occupied bytes) count for the arena chunks. */
    size_t used() const {
        size_t accum = 0;
        BumpChunk *it = first;
        while (it) {
            accum += it->used();
            it = it->next();
        }
        return accum;
    }

    /* Get the total size of the arena chunks (including unused space), plus,
     * if |countMe| is true, the size of the LifoAlloc itself. */
    size_t sizeOf(JSUsableSizeFun usf, bool countMe) const {
        size_t accum = 0;
        if (countMe) {
            size_t usable = usf((void*)this);
            accum += usable ? usable : sizeof(LifoAlloc);
        }
        BumpChunk *it = first;
        while (it) {
            accum += it->sizeOf(usf);
            it = it->next();
        }
        return accum;
    }

    /* Doesn't perform construction; useful for lazily-initialized POD types. */
    template <typename T>
    JS_ALWAYS_INLINE
    T *newPod() {
        return static_cast<T *>(alloc(sizeof(T)));
    }

    JS_DECLARE_NEW_METHODS(alloc, JS_ALWAYS_INLINE)

    /* Some legacy clients (ab)use LifoAlloc to act like a vector, see bug 688891. */

    void *allocUnaligned(size_t n);
    void *reallocUnaligned(void *origPtr, size_t origSize, size_t incr);
};

class LifoAllocScope {
    LifoAlloc   *lifoAlloc;
    void        *mark;
    bool        shouldRelease;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    explicit LifoAllocScope(LifoAlloc *lifoAlloc
                            JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : lifoAlloc(lifoAlloc), shouldRelease(true) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        mark = lifoAlloc->mark();
    }

    ~LifoAllocScope() {
        if (shouldRelease)
            lifoAlloc->release(mark);
    }

    LifoAlloc &alloc() {
        return *lifoAlloc;
    }

    void releaseEarly() {
        JS_ASSERT(shouldRelease);
        lifoAlloc->release(mark);
        shouldRelease = false;
    }
};

} /* namespace js */

#endif

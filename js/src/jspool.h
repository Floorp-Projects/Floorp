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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * July 16, 2009.
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
 *
 * Contributor(s):
 *   Luke Wagner <lw@mozilla.com>
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

#ifndef jspool_h_
#define jspool_h_

#include "jstl.h"

#include <new>

namespace js {

/*
 * A container providing a short-lived, dynamic pool allocation of homogeneous
 * objects. Pool allocation does not have to be contiguous, hence it can avoid
 * reallocation when it grows which provides stable pointers into the pool.
 * Pointers are, however, invalidated by several members as described in their
 * comments below. Pool calls the constructors and destructors of objects
 * created and destroyed in the pool, so non-PODs may be used safely. However,
 * for performance reasons, Pool assumes (and asserts) that all objects in the
 * pool are destroyed before the Pool itself is destroyed. ('freeRawMemory' and
 * 'clear' provide workarounds.)
 *
 * T requirements:
 *  - sizeof(T) >= sizeof(void *) (statically asserted)
 *  - default constructible, destructible
 *  - operations do not throw
 * N:
 *  - specifies the number of elements to store in-line before the first
 *    dynamic allocation. (default 0)
 * AllocPolicy:
 *  - see "Allocation policies" in jstl.h (default ContextAllocPolicy)
 *
 * N.B: Pool is not reentrant: T member functions called during Pool member
 * functions must not call back into the same object.
 */
template <class T, size_t N, class AllocPolicy>
class Pool : AllocPolicy
{
    typedef typename tl::StaticAssert<sizeof(T) >= sizeof(void *)>::result _;

    /* Pool is non-copyable */
    Pool(const Pool &);
    void operator=(const Pool &);

    /* utilities */
    void addRangeToFreeList(char *begin, char *end);
    bool addChunk();
    void freeChunkChain();

    /* magic constants */
    static const size_t sGrowthFactor = 2;
    static const size_t sInitialAllocElems = 32;

    /* compute constants */

    static const size_t sInlineBytes =
        N * sizeof(T);

    static const size_t sInitialAllocBytes =
        tl::Max<2 * sInlineBytes, sInitialAllocElems * sizeof(T)>::result;

    /* member data */

#ifdef DEBUG
    friend class ReentrancyGuard;
    bool entered;
    size_t allocCount;
#endif

    /*
     * Place |buf| in a template class so that it may be removed if sInlineBytes
     * is zero (C++ does not allow). Place the other members in the same class
     * to avoid wasting space on empty-objects (apparently not optimized away).
     */
    template <size_t NonZeroBytes, class>
    struct MemberData
    {
        /* inline storage for first sInlineBytes / sizeof(T) objects */
        char buf[sInlineBytes];

        /* singly-linked list of allocated chunks of memory */
        void *chunkHead;

        /* number of bytes allocated in last chunk */
        size_t lastAlloc;

        /* LIFO singly-linked list of available T-sized uninitialized memory */
        void *freeHead;

        void init(Pool &p) {
            chunkHead = NULL;
            lastAlloc = 0;
            freeHead = NULL;
            p.addRangeToFreeList(buf, tl::ArrayEnd(buf));
        }
    };

    MemberData<sInlineBytes, void> m;

  public:

    Pool(AllocPolicy = AllocPolicy());
    ~Pool();

    AllocPolicy &allocPolicy() { return *this; }

    /*
     * Allocate and default-construct an object. Objects created in this way
     * should be released by 'destroy'. Returns NULL on failure.
     */
    T *create();

    /* Call destructor and free associated memory of the given object. */
    void destroy(T *);

    /*
     * This function adds memory that has not been allocated by the pool to the
     * pool's free list. This memory will not be freed by the pool and must
     * remain valid until a call to clear(), freeRawMemory(), consolidate(), or
     * the Pool's destructor.
     */
    void lendUnusedMemory(void *begin, void *end);

    /*
     * Assuming that the given Range contains all the elements in the pool,
     * destroy all such elements and free all allocated memory.
     */
    template <class Range>
    void clear(Range r);

    /*
     * Assuming that all elements have been destroyed, or that T is a POD, free
     * all allocated memory.
     */
    void freeRawMemory();

    /*
     * Assuming that the given Range contains all the elements in the pool, and
     * 'count' is the size of the range, copy the pool elements into a new
     * buffer that is exactly big enough to hold them and free the old buffers.
     * Clearly, this breaks pointer stability, so return a pointer to the new
     * contiguous array of elements. On failure, returns NULL.
     */
    template <class InputRange>
    T *consolidate(InputRange i, size_t count);
};

/* Implementation */

/*
 * When sInlineBytes is zero, remove |buf| member variable. The 'Unused'
 * parameter does nothing and is only included because C++ has strange rules
 * (14.7.3.17-18) regarding explicit specializations.
 */
template <class T, size_t N, class AP>
template <class Unused>
struct Pool<T,N,AP>::MemberData<0,Unused>
{
    void *chunkHead;
    size_t lastAlloc;
    void *freeHead;

    void init(Pool<T,N,AP> &) {
        chunkHead = NULL;
        lastAlloc = 0;
        freeHead = NULL;
    }
};

/*
 * Divide the range of uninitialized memory [begin, end) into sizeof(T)
 * pieces of free memory in the free list.
 */
template <class T, size_t N, class AP>
inline void
Pool<T,N,AP>::addRangeToFreeList(char *begin, char *end)
{
    JS_ASSERT((end - begin) % sizeof(T) == 0);
    void *oldHead = m.freeHead;
    void **last = &m.freeHead;
    for (char *p = begin; p != end; p += sizeof(T)) {
        *last = p;
        last = reinterpret_cast<void **>(p);
    }
    *last = oldHead;
}

template <class T, size_t N, class AP>
inline
Pool<T,N,AP>::Pool(AP ap)
  : AP(ap)
#ifdef DEBUG
    , entered(false), allocCount(0)
#endif
{
    m.init(*this);
}

template <class T, size_t N, class AP>
inline void
Pool<T,N,AP>::freeChunkChain()
{
    void *p = m.chunkHead;
    while (p) {
        void *next = *reinterpret_cast<void **>(p);
        this->free(p);
        p = next;
    }
}

template <class T, size_t N, class AP>
inline
Pool<T,N,AP>::~Pool()
{
    JS_ASSERT(allocCount == 0 && !entered);
    freeChunkChain();
}

template <class T, size_t N, class AP>
inline bool
Pool<T,N,AP>::addChunk()
{
    /* Check for overflow in multiplication and ptrdiff_t. */
    if (m.lastAlloc & tl::MulOverflowMask<2 * sGrowthFactor>::result) {
        this->reportAllocOverflow();
        return false;
    }

    if (!m.lastAlloc)
        m.lastAlloc = sInitialAllocBytes;
    else
        m.lastAlloc *= sGrowthFactor;
    char *bytes = (char *)this->malloc(m.lastAlloc);
    if (!bytes)
        return false;

    /*
     * Add new chunk to the pool. To avoid alignment issues, start first free
     * element at the next multiple of sizeof(T), not sizeof(void*).
     */
    *reinterpret_cast<void **>(bytes) = m.chunkHead;
    m.chunkHead = bytes;
    addRangeToFreeList(bytes + sizeof(T), bytes + m.lastAlloc);
    return true;
}

template <class T, size_t N, class AP>
inline T *
Pool<T,N,AP>::create()
{
    ReentrancyGuard g(*this);
    if (!m.freeHead && !addChunk())
        return NULL;
    void *objMem = m.freeHead;
    m.freeHead = *reinterpret_cast<void **>(m.freeHead);
#ifdef DEBUG
    ++allocCount;
#endif
    return new(objMem) T();
}

template <class T, size_t N, class AP>
inline void
Pool<T,N,AP>::destroy(T *p)
{
    ReentrancyGuard g(*this);
    JS_ASSERT(p && allocCount-- > 0);
    p->~T();
    *reinterpret_cast<void **>(p) = m.freeHead;
    m.freeHead = p;
}

template <class T, size_t N, class AP>
inline void
Pool<T,N,AP>::lendUnusedMemory(void *vbegin, void *vend)
{
    JS_ASSERT(!entered);
    char *begin = (char *)vbegin, *end = (char *)vend;
    size_t mod = (end - begin) % sizeof(T);
    if (mod)
        end -= mod;
    addRangeToFreeList(begin, end);
}

template <class T, size_t N, class AP>
inline void
Pool<T,N,AP>::freeRawMemory()
{
    JS_ASSERT(!entered);
#ifdef DEBUG
    allocCount = 0;
#endif
    freeChunkChain();
    m.init(*this);
}

template <class T, size_t N, class AP>
template <class Range>
inline void
Pool<T,N,AP>::clear(Range r)
{
    typedef typename Range::ElementType Elem;
    for (; !r.empty(); r.popFront())
        r.front().~Elem();
    freeRawMemory();
}

template <class T, size_t N, class AP>
template <class Range>
inline T *
Pool<T,N,AP>::consolidate(Range r, size_t count)
{
    /* Cannot overflow because already allocated. */
    size_t size = (count + 1) * sizeof(T);
    char *bytes = (char *)this->malloc(size);
    if (!bytes)
        return NULL;

    /* Initialize new chunk with copies of old elements, destroy old. */
    *reinterpret_cast<void **>(bytes) = NULL;
    T *arrayBegin = reinterpret_cast<T *>(bytes);
    ++arrayBegin;  /* skip 'next' pointer hidden in first element */
    for (T *dst = arrayBegin; !r.empty(); r.popFront(), ++dst) {
        JS_ASSERT(count-- > 0);
        new(dst) T(r.front());
        r.front().~T();
    }
    JS_ASSERT(count == 0);

    freeChunkChain();

    /* Update pool state. */
    m.init(*this);
    m.chunkHead = bytes;
    m.lastAlloc = size;
    return arrayBegin;
}

}

#endif /* jspool_h_ */

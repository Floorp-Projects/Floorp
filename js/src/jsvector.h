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
 * June 12, 2009.
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

#ifndef jsvector_h_
#define jsvector_h_

#include <new>

#include "jstl.h"

namespace js {

/*
 * This template class provides a default implementation for vector operations
 * when the element type is not known to be a POD, as judged by IsPodType.
 */
template <class T, size_t N, class AP, bool IsPod>
struct VectorImpl
{
    /* Destroys constructed objects in the range [begin, end). */
    static inline void destroy(T *begin, T *end) {
        for (T *p = begin; p != end; ++p)
            p->~T();
    }

    /* Constructs objects in the uninitialized range [begin, end). */
    static inline void initialize(T *begin, T *end) {
        for (T *p = begin; p != end; ++p)
            new(p) T();
    }

    /*
     * Copy-constructs objects in the uninitialized range
     * [dst, dst+(srcend-srcbeg)) from the range [srcbeg, srcend).
     */
    template <class U>
    static inline void copyConstruct(T *dst, const U *srcbeg, const U *srcend) {
        for (const U *p = srcbeg; p != srcend; ++p, ++dst)
            new(dst) T(*p);
    }

    /*
     * Copy-constructs objects in the uninitialized range [dst, dst+n) from the
     * same object u.
     */
    template <class U>
    static inline void copyConstructN(T *dst, size_t n, const U &u) {
        for (T *end = dst + n; dst != end; ++dst)
            new(dst) T(u);
    }

    /*
     * Grows the given buffer to have capacity newcap, preserving the objects
     * constructed in the range [begin, end) and updating v. Assumes that (1)
     * newcap has not overflowed, and (2) multiplying newcap by sizeof(T) will
     * not overflow.
     */
    static inline bool growTo(Vector<T,N,AP> &v, size_t newcap) {
        JS_ASSERT(!v.usingInlineStorage());
        T *newbuf = reinterpret_cast<T *>(v.malloc(newcap * sizeof(T)));
        if (!newbuf)
            return false;
        for (T *dst = newbuf, *src = v.heapBegin(); src != v.heapEnd(); ++dst, ++src)
            new(dst) T(*src);
        VectorImpl::destroy(v.heapBegin(), v.heapEnd());
        v.free(v.heapBegin());
        v.heapEnd() = newbuf + v.heapLength();
        v.heapBegin() = newbuf;
        v.heapCapacity() = newcap;
        return true;
    }
};

/*
 * This partial template specialization provides a default implementation for
 * vector operations when the element type is known to be a POD, as judged by
 * IsPodType.
 */
template <class T, size_t N, class AP>
struct VectorImpl<T, N, AP, true>
{
    static inline void destroy(T *, T *) {}

    static inline void initialize(T *begin, T *end) {
        /*
         * You would think that memset would be a big win (or even break even)
         * when we know T is a POD. But currently it's not. This is probably
         * because |append| tends to be given small ranges and memset requires
         * a function call that doesn't get inlined.
         *
         * memset(begin, 0, sizeof(T) * (end-begin));
         */
        for (T *p = begin; p != end; ++p)
            new(p) T();
    }

    template <class U>
    static inline void copyConstruct(T *dst, const U *srcbeg, const U *srcend) {
        /*
         * See above memset comment. Also, notice that copyConstruct is
         * currently templated (T != U), so memcpy won't work without
         * requiring T == U.
         *
         * memcpy(dst, srcbeg, sizeof(T) * (srcend - srcbeg));
         */
        for (const U *p = srcbeg; p != srcend; ++p, ++dst)
            *dst = *p;
    }

    static inline void copyConstructN(T *dst, size_t n, const T &t) {
        for (T *p = dst, *end = dst + n; p != end; ++p)
            *p = t;
    }

    static inline bool growTo(Vector<T,N,AP> &v, size_t newcap) {
        JS_ASSERT(!v.usingInlineStorage());
        size_t bytes = sizeof(T) * newcap;
        T *newbuf = reinterpret_cast<T *>(v.realloc(v.heapBegin(), bytes));
        if (!newbuf)
            return false;
        v.heapEnd() = newbuf + v.heapLength();
        v.heapBegin() = newbuf;
        v.heapCapacity() = newcap;
        return true;
    }
};

/*
 * JS-friendly, STL-like container providing a short-lived, dynamic buffer.
 * Vector calls the constructors/destructors of all elements stored in
 * its internal buffer, so non-PODs may be safely used. Additionally,
 * Vector will store the first N elements in-place before resorting to
 * dynamic allocation.
 *
 * T requirements:
 *  - default and copy constructible, assignable, destructible
 *  - operations do not throw
 * N requirements:
 *  - any value, however, N is clamped to min/max values
 * AllocPolicy:
 *  - see "Allocation policies" in jstl.h (default ContextAllocPolicy)
 *
 * N.B: Vector is not reentrant: T member functions called during Vector member
 *      functions must not call back into the same object.
 */
template <class T, size_t N, class AllocPolicy>
class Vector : AllocPolicy
{
    /* utilities */

    static const bool sElemIsPod = tl::IsPodType<T>::result;
    typedef VectorImpl<T, N, AllocPolicy, sElemIsPod> Impl;
    friend struct VectorImpl<T, N, AllocPolicy, sElemIsPod>;

    bool calculateNewCapacity(size_t curLength, size_t lengthInc, size_t &newCap);
    bool growHeapStorageBy(size_t lengthInc);
    bool convertToHeapStorage(size_t lengthInc);

    /* magic constants */

    static const int sMaxInlineBytes = 1024;

    /* compute constants */

    /*
     * Pointers to the heap-allocated buffer. Only [heapBegin(), heapEnd())
     * hold valid constructed T objects. The range [heapEnd(), heapBegin() +
     * heapCapacity()) holds uninitialized memory.
     */
    struct BufferPtrs {
        T *mBegin, *mEnd;
    };

    /*
     * Since a vector either stores elements inline or in a heap-allocated
     * buffer, reuse the storage. mLengthOrCapacity serves as the union
     * discriminator. In inline mode (when elements are stored in u.mBuf),
     * mLengthOrCapacity holds the vector's length. In heap mode (when elements
     * are stored in [u.ptrs.mBegin, u.ptrs.mEnd)), mLengthOrCapacity holds the
     * vector's capacity.
     */
    static const size_t sInlineCapacity =
        tl::Clamp<N, sizeof(BufferPtrs) / sizeof(T),
                          sMaxInlineBytes / sizeof(T)>::result;

    /* Calculate inline buffer size; avoid 0-sized array. */
    static const size_t sInlineBytes =
        tl::Max<1, sInlineCapacity * sizeof(T)>::result;

    /* member data */

    size_t mLengthOrCapacity;
    bool usingInlineStorage() const { return mLengthOrCapacity <= sInlineCapacity; }

    union {
        BufferPtrs ptrs;
        char mBuf[sInlineBytes];
    } u;

    /* Only valid when usingInlineStorage() */
    size_t &inlineLength() {
        JS_ASSERT(usingInlineStorage());
        return mLengthOrCapacity;
    }

    size_t inlineLength() const {
        JS_ASSERT(usingInlineStorage());
        return mLengthOrCapacity;
    }

    T *inlineBegin() const {
        JS_ASSERT(usingInlineStorage());
        return (T *)u.mBuf;
    }

    T *inlineEnd() const {
        JS_ASSERT(usingInlineStorage());
        return (T *)u.mBuf + mLengthOrCapacity;
    }

    /* Only valid when !usingInlineStorage() */
    size_t heapLength() const {
        JS_ASSERT(!usingInlineStorage());
        /* Guaranteed by calculateNewCapacity. */
        JS_ASSERT(size_t(u.ptrs.mEnd - u.ptrs.mBegin) ==
                  ((size_t(u.ptrs.mEnd) - size_t(u.ptrs.mBegin)) / sizeof(T)));
        return u.ptrs.mEnd - u.ptrs.mBegin;
    }

    size_t &heapCapacity() {
        JS_ASSERT(!usingInlineStorage());
        return mLengthOrCapacity;
    }

    T *&heapBegin() {
        JS_ASSERT(!usingInlineStorage());
        return u.ptrs.mBegin;
    }

    T *&heapEnd() {
        JS_ASSERT(!usingInlineStorage());
        return u.ptrs.mEnd;
    }

    size_t heapCapacity() const {
        JS_ASSERT(!usingInlineStorage());
        return mLengthOrCapacity;
    }

    T *heapBegin() const {
        JS_ASSERT(!usingInlineStorage());
        return u.ptrs.mBegin;
    }

    T *heapEnd() const {
        JS_ASSERT(!usingInlineStorage());
        return u.ptrs.mEnd;
    }

#ifdef DEBUG
    friend class ReentrancyGuard;
    bool mEntered;
#endif

    Vector(const Vector &);
    Vector &operator=(const Vector &);

  public:
    Vector(AllocPolicy = AllocPolicy());
    ~Vector();

    /* accessors */

    size_t length() const {
        return usingInlineStorage() ? inlineLength() : heapLength();
    }

    bool empty() const {
        return usingInlineStorage() ? inlineLength() == 0 : heapBegin() == heapEnd();
    }

    size_t capacity() const {
        return usingInlineStorage() ? sInlineCapacity : heapCapacity();
    }

    T *begin() {
        JS_ASSERT(!mEntered);
        return usingInlineStorage() ? inlineBegin() : heapBegin();
    }

    const T *begin() const {
        JS_ASSERT(!mEntered);
        return usingInlineStorage() ? inlineBegin() : heapBegin();
    }

    T *end() {
        JS_ASSERT(!mEntered);
        return usingInlineStorage() ? inlineEnd() : heapEnd();
    }

    const T *end() const {
        JS_ASSERT(!mEntered);
        return usingInlineStorage() ? inlineEnd() : heapEnd();
    }

    T &operator[](size_t i) {
        JS_ASSERT(!mEntered && i < length());
        return begin()[i];
    }

    const T &operator[](size_t i) const {
        JS_ASSERT(!mEntered && i < length());
        return begin()[i];
    }

    T &back() {
        JS_ASSERT(!mEntered && !empty());
        return *(end() - 1);
    }

    const T &back() const {
        JS_ASSERT(!mEntered && !empty());
        return *(end() - 1);
    }

    /* mutators */

    /* If reserve(N) succeeds, the N next appends are guaranteed to succeed. */
    bool reserve(size_t capacity);

    /* Destroy elements in the range [begin() + incr, end()). */
    void shrinkBy(size_t incr);

    /*
     * Grow the vector by incr elements.  If T is a POD (as judged by
     * tl::IsPodType), leave as uninitialized memory.  Otherwise, default
     * construct each element.
     */
    bool growBy(size_t incr);

    /* Call shrinkBy or growBy based on whether newSize > length(). */
    bool resize(size_t newLength);

    void clear();

    bool append(const T &t);
    bool appendN(const T &t, size_t n);
    template <class U> bool append(const U *begin, const U *end);
    template <class U> bool append(const U *begin, size_t length);

    void popBack();

    /*
     * Transfers ownership of the internal buffer used by Vector to the caller.
     * After this call, the Vector is empty. Since the returned buffer may need
     * to be allocated (if the elements are currently stored in-place), the
     * call can fail, returning NULL.
     *
     * N.B. Although a T*, only the range [0, length()) is constructed.
     */
    T *extractRawBuffer();

    /*
     * Transfer ownership of an array of objects into the Vector.
     * N.B. This call assumes that there are no uninitialized elements in the
     *      passed array.
     */
    void replaceRawBuffer(T *p, size_t length);
};

/* Helper functions */

/*
 * This helper function is specialized for appending the characters of a string
 * literal to a vector. This could not be done generically since one must take
 * care not to append the terminating '\0'.
 */
template <class T, size_t N, class AP, size_t ArrayLength>
bool
js_AppendLiteral(Vector<T,N,AP> &v, const char (&array)[ArrayLength])
{
    return v.append(array, array + ArrayLength - 1);
}


/* Vector Implementation */

template <class T, size_t N, class AP>
inline
Vector<T,N,AP>::Vector(AP ap)
  : AP(ap), mLengthOrCapacity(0)
#ifdef DEBUG
    , mEntered(false)
#endif
{}

template <class T, size_t N, class AP>
inline
Vector<T,N,AP>::~Vector()
{
    ReentrancyGuard g(*this);
    if (usingInlineStorage()) {
        Impl::destroy(inlineBegin(), inlineEnd());
    } else {
        Impl::destroy(heapBegin(), heapEnd());
        this->free(heapBegin());
    }
}

/*
 * Calculate a new capacity that is at least lengthInc greater than
 * curLength and check for overflow.
 */
template <class T, size_t N, class AP>
inline bool
Vector<T,N,AP>::calculateNewCapacity(size_t curLength, size_t lengthInc,
                                     size_t &newCap)
{
    size_t newMinCap = curLength + lengthInc;

    /*
     * Check for overflow in the above addition, below CEILING_LOG2, and later
     * multiplication by sizeof(T).
     */
    if (newMinCap < curLength ||
        newMinCap & tl::MulOverflowMask<2 * sizeof(T)>::result) {
        this->reportAllocOverflow();
        return false;
    }

    /* Round up to next power of 2. */
    newCap = RoundUpPow2(newMinCap);

    /*
     * Do not allow a buffer large enough that the expression ((char *)end() -
     * (char *)begin()) overflows ptrdiff_t. See Bug 510319.
     */
    if (newCap & tl::UnsafeRangeSizeMask<T>::result) {
        this->reportAllocOverflow();
        return false;
    }
    return true;
}

/*
 * This function will grow the current heap capacity to have capacity
 * (heapLength() + lengthInc) and fail on OOM or integer overflow.
 */
template <class T, size_t N, class AP>
inline bool
Vector<T,N,AP>::growHeapStorageBy(size_t lengthInc)
{
    size_t newCap;
    return calculateNewCapacity(heapLength(), lengthInc, newCap) &&
           Impl::growTo(*this, newCap);
}

/*
 * This function will create a new heap buffer with capacity (inlineLength() +
 * lengthInc()), move all elements in the inline buffer to this new buffer,
 * and fail on OOM or integer overflow.
 */
template <class T, size_t N, class AP>
inline bool
Vector<T,N,AP>::convertToHeapStorage(size_t lengthInc)
{
    size_t newCap;
    if (!calculateNewCapacity(inlineLength(), lengthInc, newCap))
        return false;

    /* Allocate buffer. */
    T *newBuf = reinterpret_cast<T *>(this->malloc(newCap * sizeof(T)));
    if (!newBuf)
        return false;

    /* Copy inline elements into heap buffer. */
    size_t length = inlineLength();
    Impl::copyConstruct(newBuf, inlineBegin(), inlineEnd());
    Impl::destroy(inlineBegin(), inlineEnd());

    /* Switch in heap buffer. */
    mLengthOrCapacity = newCap;  /* marks us as !usingInlineStorage() */
    heapBegin() = newBuf;
    heapEnd() = newBuf + length;
    return true;
}

template <class T, size_t N, class AP>
inline bool
Vector<T,N,AP>::reserve(size_t request)
{
    ReentrancyGuard g(*this);
    if (usingInlineStorage()) {
        if (request > sInlineCapacity)
            return convertToHeapStorage(request - inlineLength());
    } else {
        if (request > heapCapacity())
            return growHeapStorageBy(request - heapLength());
    }
    return true;
}

template <class T, size_t N, class AP>
inline void
Vector<T,N,AP>::shrinkBy(size_t incr)
{
    ReentrancyGuard g(*this);
    JS_ASSERT(incr <= length());
    if (usingInlineStorage()) {
        Impl::destroy(inlineEnd() - incr, inlineEnd());
        inlineLength() -= incr;
    } else {
        Impl::destroy(heapEnd() - incr, heapEnd());
        heapEnd() -= incr;
    }
}

template <class T, size_t N, class AP>
inline bool
Vector<T,N,AP>::growBy(size_t incr)
{
    ReentrancyGuard g(*this);
    if (usingInlineStorage()) {
        size_t freespace = sInlineCapacity - inlineLength();
        if (incr <= freespace) {
            T *newend = inlineEnd() + incr;
            if (!tl::IsPodType<T>::result)
                Impl::initialize(inlineEnd(), newend);
            inlineLength() += incr;
            JS_ASSERT(usingInlineStorage());
            return true;
        }
        if (!convertToHeapStorage(incr))
            return false;
    }
    else {
        /* grow if needed */
        size_t freespace = heapCapacity() - heapLength();
        if (incr > freespace) {
            if (!growHeapStorageBy(incr))
                return false;
        }
    }

    /* We are !usingInlineStorage(). Initialize new elements. */
    JS_ASSERT(heapCapacity() - heapLength() >= incr);
    T *newend = heapEnd() + incr;
    if (!tl::IsPodType<T>::result)
        Impl::initialize(heapEnd(), newend);
    heapEnd() = newend;
    return true;
}

template <class T, size_t N, class AP>
inline bool
Vector<T,N,AP>::resize(size_t newLength)
{
    size_t curLength = length();
    if (newLength > curLength)
        return growBy(newLength - curLength);
    shrinkBy(curLength - newLength);
    return true;
}

template <class T, size_t N, class AP>
inline void
Vector<T,N,AP>::clear()
{
    ReentrancyGuard g(*this);
    if (usingInlineStorage()) {
        Impl::destroy(inlineBegin(), inlineEnd());
        inlineLength() = 0;
    }
    else {
        Impl::destroy(heapBegin(), heapEnd());
        heapEnd() = heapBegin();
    }
}

template <class T, size_t N, class AP>
inline bool
Vector<T,N,AP>::append(const T &t)
{
    ReentrancyGuard g(*this);
    if (usingInlineStorage()) {
        if (inlineLength() < sInlineCapacity) {
            new(inlineEnd()) T(t);
            ++inlineLength();
            JS_ASSERT(usingInlineStorage());
            return true;
        }
        if (!convertToHeapStorage(1))
            return false;
    } else {
        if (heapLength() == heapCapacity() && !growHeapStorageBy(1))
            return false;
    }

    /* We are !usingInlineStorage(). Initialize new elements. */
    JS_ASSERT(heapLength() <= heapCapacity() && heapCapacity() - heapLength() >= 1);
    new(heapEnd()++) T(t);
    return true;
}

template <class T, size_t N, class AP>
inline bool
Vector<T,N,AP>::appendN(const T &t, size_t needed)
{
    ReentrancyGuard g(*this);
    if (usingInlineStorage()) {
        size_t freespace = sInlineCapacity - inlineLength();
        if (needed <= freespace) {
            Impl::copyConstructN(inlineEnd(), needed, t);
            inlineLength() += needed;
            JS_ASSERT(usingInlineStorage());
            return true;
        }
        if (!convertToHeapStorage(needed))
            return false;
    } else {
        size_t freespace = heapCapacity() - heapLength();
        if (needed > freespace && !growHeapStorageBy(needed))
            return false;
    }

    /* We are !usingInlineStorage(). Initialize new elements. */
    JS_ASSERT(heapLength() <= heapCapacity() && heapCapacity() - heapLength() >= needed);
    Impl::copyConstructN(heapEnd(), needed, t);
    heapEnd() += needed;
    return true;
}

template <class T, size_t N, class AP>
template <class U>
inline bool
Vector<T,N,AP>::append(const U *insBegin, const U *insEnd)
{
    ReentrancyGuard g(*this);
    size_t needed = PointerRangeSize(insBegin, insEnd);
    if (usingInlineStorage()) {
        size_t freespace = sInlineCapacity - inlineLength();
        if (needed <= freespace) {
            Impl::copyConstruct(inlineEnd(), insBegin, insEnd);
            inlineLength() += needed;
            JS_ASSERT(usingInlineStorage());
            return true;
        }
        if (!convertToHeapStorage(needed))
            return false;
    } else {
        size_t freespace = heapCapacity() - heapLength();
        if (needed > freespace && !growHeapStorageBy(needed))
            return false;
    }

    /* We are !usingInlineStorage(). Initialize new elements. */
    JS_ASSERT(heapLength() <= heapCapacity() && heapCapacity() - heapLength() >= needed);
    Impl::copyConstruct(heapEnd(), insBegin, insEnd);
    heapEnd() += needed;
    return true;
}

template <class T, size_t N, class AP>
template <class U>
inline bool
Vector<T,N,AP>::append(const U *insBegin, size_t length)
{
    return this->append(insBegin, insBegin + length);
}

template <class T, size_t N, class AP>
inline void
Vector<T,N,AP>::popBack()
{
    ReentrancyGuard g(*this);
    JS_ASSERT(!empty());
    if (usingInlineStorage()) {
        --inlineLength();
        inlineEnd()->~T();
    } else {
        --heapEnd();
        heapEnd()->~T();
    }
}

template <class T, size_t N, class AP>
inline T *
Vector<T,N,AP>::extractRawBuffer()
{
    if (usingInlineStorage()) {
        T *ret = reinterpret_cast<T *>(this->malloc(inlineLength() * sizeof(T)));
        if (!ret)
            return NULL;
        Impl::copyConstruct(ret, inlineBegin(), inlineEnd());
        Impl::destroy(inlineBegin(), inlineEnd());
        inlineLength() = 0;
        return ret;
    }

    T *ret = heapBegin();
    mLengthOrCapacity = 0;  /* marks us as !usingInlineStorage() */
    return ret;
}

template <class T, size_t N, class AP>
inline void
Vector<T,N,AP>::replaceRawBuffer(T *p, size_t length)
{
    ReentrancyGuard g(*this);

    /* Destroy what we have. */
    if (usingInlineStorage()) {
        Impl::destroy(inlineBegin(), inlineEnd());
        inlineLength() = 0;
    } else {
        Impl::destroy(heapBegin(), heapEnd());
        this->free(heapBegin());
    }

    /* Take in the new buffer. */
    if (length <= sInlineCapacity) {
        /*
         * (mLengthOrCapacity <= sInlineCapacity) means inline storage, so we
         * MUST use inline storage, even though p might otherwise be acceptable.
         */
        mLengthOrCapacity = length;  /* marks us as usingInlineStorage() */
        Impl::copyConstruct(inlineBegin(), p, p + length);
        Impl::destroy(p, p + length);
        this->free(p);
    } else {
        mLengthOrCapacity = length;  /* marks us as !usingInlineStorage() */
        heapBegin() = p;
        heapEnd() = heapBegin() + length;
    }
}

}  /* namespace js */

#endif /* jsvector_h_ */

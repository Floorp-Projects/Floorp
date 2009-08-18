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

#include <string.h>
#include <new>

#include "jsbit.h"

/* Library of template meta-programs for use in the C++ JS data-structures. */
namespace JSUtils {

/* Statically compute min/max. */
template <size_t i, size_t j> struct min {
    static const size_t result = i < j ? i : j;
};
template <size_t i, size_t j> struct max {
    static const size_t result = i > j ? i : j;
};

/* Statically compute floor(log2(i)). */
template <size_t i> struct FloorLog2 {
    static const size_t result = 1 + FloorLog2<i / 2>::result;
};
template <> struct FloorLog2<0> { /* Error */ };
template <> struct FloorLog2<1> { static const size_t result = 0; };

/* Statically compute ceiling(log2(i)). */
template <size_t i> struct CeilingLog2 {
    static const size_t result = FloorLog2<2 * i - 1>::result;
};

/* Statically compute the number of bits in the given unsigned type. */
template <class T> struct BitSize {
    static const size_t result = sizeof(T) * JS_BITS_PER_BYTE;
};

/*
 * Produce an N-bit mask, where N <= BitSize<size_t>::result.  Handle the
 * language-undefined edge case when N = BitSize<size_t>::result.
 */
template <size_t N> struct NBitMask {
    JS_STATIC_ASSERT(N < BitSize<size_t>::result);
    static const size_t result = ~((size_t(1) << N) - 1);
};
template <> struct NBitMask<BitSize<size_t>::result> {
    static const size_t result = size_t(-1);
};

/*
 * For the unsigned integral type size_t, compute a mask M for N such that
 * for all X, !(X & M) implies X * N will not overflow (w.r.t size_t)
 */
template <size_t N> struct MulOverflowMask {
    static const size_t result =
        NBitMask<BitSize<size_t>::result - CeilingLog2<N>::result>::result;
};
template <> struct MulOverflowMask<0> { /* Error */ };
template <> struct MulOverflowMask<1> { static const size_t result = 0; };

/*
 * Safely subtract two pointers when it is known that end > begin.  This avoids
 * the common compiler bug that if (size_t(end) - size_t(begin)) has the MSB
 * set, the unsigned subtraction followed by right shift will produce -1, or
 * size_t(-1), instead of the real difference.
 */
template <class T>
size_t JS_ALWAYS_INLINE
PointerRangeSize(T *begin, T *end) {
    return (size_t(end) - size_t(begin)) / sizeof(T);
}

/*
 * Generate a mask for T such that if (X & sUnsafeRangeSizeMask), an X-sized
 * array of T's is big enough to cause a ptrdiff_t overflow when subtracting
 * a pointer to the end of the array from the beginning.
 */
template <class T> struct UnsafeRangeSizeMask {
    /*
     * The '2' factor means the top bit is clear, sizeof(T) converts from
     * units of elements to bytes.
     */
    static const size_t result = MulOverflowMask<2 * sizeof(T)>::result;
};

/*
 * Traits class for identifying POD types. Until C++0x, there is no automatic
 * way to detect PODs, so for the moment it is done manually.
 */
template <class T> struct IsPodType           { static const bool result = false; };
template <> struct IsPodType<char>            { static const bool result = true; };
template <> struct IsPodType<signed char>     { static const bool result = true; };
template <> struct IsPodType<unsigned char>   { static const bool result = true; };
template <> struct IsPodType<short>           { static const bool result = true; };
template <> struct IsPodType<unsigned short>  { static const bool result = true; };
template <> struct IsPodType<int>             { static const bool result = true; };
template <> struct IsPodType<unsigned int>    { static const bool result = true; };
template <> struct IsPodType<long>            { static const bool result = true; };
template <> struct IsPodType<unsigned long>   { static const bool result = true; };
template <> struct IsPodType<float>           { static const bool result = true; };
template <> struct IsPodType<double>          { static const bool result = true; };

} // end namespace JSUtils

/*
 * This template class provides a default implementation for vector operations
 * when the element type is not known to be a POD, as judged by IsPodType.
 */
template <class T, size_t N, bool IsPod>
struct JSTempVectorImpl
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
    static inline bool growTo(JSTempVector<T> &v, size_t newcap) {
        JS_ASSERT(!v.usingInlineStorage());
        T *newbuf = reinterpret_cast<T *>(v.mCx->malloc(newcap * sizeof(T)));
        if (!newbuf)
            return false;
        for (T *dst = newbuf, *src = v.heapBegin(); src != v.heapEnd(); ++dst, ++src)
            new(dst) T(*src);
        JSTempVectorImpl::destroy(v.heapBegin(), v.heapEnd());
        v.mCx->free(v.heapBegin());
        v.heapEnd() = newbuf + v.heapSize();
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
template <class T, size_t N>
struct JSTempVectorImpl<T, N, true>
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
        for (T *end = dst + n; dst != end; ++dst)
            *dst = t;
    }

    static inline bool growTo(JSTempVector<T,N> &v, size_t newcap) {
        JS_ASSERT(!v.usingInlineStorage());
        size_t bytes = sizeof(T) * newcap;
        T *newbuf = reinterpret_cast<T *>(v.mCx->realloc(v.heapBegin(), bytes));
        if (!newbuf)
            return false;
        v.heapEnd() = newbuf + v.heapSize();
        v.heapBegin() = newbuf;
        v.heapCapacity() = newcap;
        return true;
    }
};

/*
 * JS-friendly, STL-like container providing a short-lived, dynamic buffer.
 * JSTempVector calls the constructors/destructors of all elements stored in
 * its internal buffer, so non-PODs may be safely used. Additionally,
 * JSTempVector stores the first few elements in-place in its member data
 * before resorting to dynamic allocation. The minimum number of elements may
 * be specified by the parameter N.
 *
 * T requirements:
 *  - default and copy constructible, assignable, destructible
 *  - operations do not throw
 *
 * N.B: JSTempVector is not reentrant: T member functions called during
 *      JSTempVector member functions must not call back into the same object.
 */
template <class T, size_t N>
class JSTempVector
{
    /* utilities */

    typedef JSTempVectorImpl<T, N, JSUtils::IsPodType<T>::result> Impl;
    friend struct JSTempVectorImpl<T, N, JSUtils::IsPodType<T>::result>;

    bool calculateNewCapacity(size_t curSize, size_t sizeInc, size_t &newCap);
    bool growHeapStorageBy(size_t sizeInc);
    bool convertToHeapStorage(size_t sizeInc);

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
     * buffer, reuse the storage. mSizeOrCapacity serves as the union
     * discriminator. In inline mode (when elements are stored in u.mBuf),
     * mSizeOrCapacity holds the vector's size. In heap mode (when elements
     * are stored in [u.ptrs.mBegin, u.ptrs.mEnd)), mSizeOrCapacity holds the
     * vector's capacity.
     */
    static const size_t sInlineCapacity =
        JSUtils::min<JSUtils::max<N, sizeof(BufferPtrs) / sizeof(T)>::result,
                     sMaxInlineBytes / sizeof(T)>::result;

    /* member data */

    JSContext *mCx;

    size_t mSizeOrCapacity;
    bool usingInlineStorage() const { return mSizeOrCapacity <= sInlineCapacity; }

    union {
        BufferPtrs ptrs;
        char mBuf[sInlineCapacity * sizeof(T)];
    } u;

    /* Only valid when usingInlineStorage() */
    size_t &inlineSize() {
        JS_ASSERT(usingInlineStorage());
        return mSizeOrCapacity;
    }

    size_t inlineSize() const {
        JS_ASSERT(usingInlineStorage());
        return mSizeOrCapacity;
    }

    T *inlineBegin() const {
        JS_ASSERT(usingInlineStorage());
        return (T *)u.mBuf;
    }

    T *inlineEnd() const {
        JS_ASSERT(usingInlineStorage());
        return ((T *)u.mBuf) + mSizeOrCapacity;
    }

    /* Only valid when !usingInlineStorage() */
    size_t heapSize() const {
        JS_ASSERT(!usingInlineStorage());
        /* Guaranteed by calculateNewCapacity. */
        JS_ASSERT(size_t(u.ptrs.mEnd - u.ptrs.mBegin) ==
                  ((size_t(u.ptrs.mEnd) - size_t(u.ptrs.mBegin)) / sizeof(T)));
        return u.ptrs.mEnd - u.ptrs.mBegin;
    }

    size_t &heapCapacity() {
        JS_ASSERT(!usingInlineStorage());
        return mSizeOrCapacity;
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
        return mSizeOrCapacity;
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
    bool mInProgress;
#endif

    class ReentrancyGuard {
        JSTempVector &mVec;
      public:
        ReentrancyGuard(JSTempVector &v)
          : mVec(v)
        {
#ifdef DEBUG
            JS_ASSERT(!mVec.mInProgress);
            mVec.mInProgress = true;
#endif
        }
        ~ReentrancyGuard()
        {
#ifdef DEBUG
            mVec.mInProgress = false;
#endif
        }
    };

    JSTempVector(const JSTempVector &);
    JSTempVector &operator=(const JSTempVector &);

  public:
    JSTempVector(JSContext *cx)
      : mCx(cx), mSizeOrCapacity(0)
#ifdef DEBUG
        , mInProgress(false)
#endif
    {}
    ~JSTempVector();

    /* accessors */

    size_t size() const {
        return usingInlineStorage() ? inlineSize() : heapSize();
    }

    bool empty() const {
        return usingInlineStorage() ? inlineSize() == 0 : heapBegin() == heapEnd();
    }

    size_t capacity() const {
        return usingInlineStorage() ? sInlineCapacity : heapCapacity();
    }

    T *begin() {
        JS_ASSERT(!mInProgress);
        return usingInlineStorage() ? inlineBegin() : heapBegin();
    }

    const T *begin() const {
        JS_ASSERT(!mInProgress);
        return usingInlineStorage() ? inlineBegin() : heapBegin();
    }

    T *end() {
        JS_ASSERT(!mInProgress);
        return usingInlineStorage() ? inlineEnd() : heapEnd();
    }

    const T *end() const {
        JS_ASSERT(!mInProgress);
        return usingInlineStorage() ? inlineEnd() : heapEnd();
    }

    T &operator[](size_t i) {
        JS_ASSERT(!mInProgress && i < size());
        return begin()[i];
    }

    const T &operator[](size_t i) const {
        JS_ASSERT(!mInProgress && i < size());
        return begin()[i];
    }

    T &back() {
        JS_ASSERT(!mInProgress && !empty());
        return *(end() - 1);
    }

    const T &back() const {
        JS_ASSERT(!mInProgress && !empty());
        return *(end() - 1);
    }

    /* mutators */

    /* If reserve(N) succeeds, the N next appends are guaranteed to succeed. */
    bool reserve(size_t capacity);

    /* Destroy elements in the range [begin() + incr, end()). */
    void shrinkBy(size_t incr);

    /*
     * Grow the vector by incr elements.  If T is a POD (as judged by
     * JSUtils::IsPodType), leave as uninitialized memory.  Otherwise, default
     * construct each element.
     */
    bool growBy(size_t incr);

    /* Call shrinkBy or growBy based on whether newSize > size(). */
    bool resize(size_t newSize);

    void clear();

    bool append(const T &t);
    bool appendN(const T &t, size_t n);
    template <class U> bool append(const U *begin, const U *end);
    template <class U> bool append(const U *begin, size_t length);

    void popBack();

    /*
     * Transfers ownership of the internal buffer used by JSTempVector to the
     * caller. After this call, the JSTempVector is empty. Since the returned
     * buffer may need to be allocated (if the elements are currently
     * stored in-place), the call can fail, returning NULL.
     *
     * N.B. Although a T*, only the range [0, size()) is constructed.
     */
    T *extractRawBuffer();

    /*
     * Transfer ownership of an array of objects into the JSTempVector.
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
template <class T, size_t N, size_t ArraySize>
bool
js_AppendLiteral(JSTempVector<T,N> &v, const char (&array)[ArraySize])
{
    return v.append(array, array + ArraySize - 1);
}


/* JSTempVector Implementation */

template <class T, size_t N>
inline
JSTempVector<T,N>::~JSTempVector()
{
    ReentrancyGuard g(*this);
    if (usingInlineStorage()) {
        Impl::destroy(inlineBegin(), inlineEnd());
    } else {
        Impl::destroy(heapBegin(), heapEnd());
        mCx->free(heapBegin());
    }
}

/*
 * Calculate a new capacity size that is at least sizeInc greater than
 * curSize and check for overflow.
 */
template <class T, size_t N>
inline bool
JSTempVector<T,N>::calculateNewCapacity(size_t curSize, size_t sizeInc, 
                                        size_t &newCap)
{
    size_t newMinCap = curSize + sizeInc;

    /*
     * Check for overflow in the above addition, below CEILING_LOG2, and later
     * multiplication by sizeof(T).
     */
    if (newMinCap < curSize ||
        newMinCap & JSUtils::MulOverflowMask<2 * sizeof(T)>::result) {
        js_ReportAllocationOverflow(mCx);
        return false;
    }

    /* Round up to next power of 2. */
    size_t newCapLog2;
    JS_CEILING_LOG2(newCapLog2, newMinCap);
    JS_ASSERT(newCapLog2 < JSUtils::BitSize<size_t>::result);
    newCap = size_t(1) << newCapLog2;

    /*
     * Do not allow a buffer large enough that the expression ((char *)end() -
     * (char *)begin()) overflows ptrdiff_t. See Bug 510319.
     */
    if (newCap & JSUtils::UnsafeRangeSizeMask<T>::result) {
        js_ReportAllocationOverflow(mCx);
        return false;
    }
    return true;
}

/*
 * This function will grow the current heap capacity to have capacity
 * (heapSize() + sizeInc) and fail on OOM or integer overflow.
 */
template <class T, size_t N>
inline bool
JSTempVector<T,N>::growHeapStorageBy(size_t sizeInc)
{
    size_t newCap;
    return calculateNewCapacity(heapSize(), sizeInc, newCap) &&
           Impl::growTo(*this, newCap);
}

/*
 * This function will create a new heap buffer with capacity (inlineSize() +
 * sizeInc()), move all elements in the inline buffer to this new buffer,
 * and fail on OOM or integer overflow.
 */
template <class T, size_t N>
inline bool
JSTempVector<T,N>::convertToHeapStorage(size_t sizeInc)
{
    size_t newCap;
    if (!calculateNewCapacity(inlineSize(), sizeInc, newCap))
        return false;

    /* Allocate buffer. */
    T *newBuf = reinterpret_cast<T *>(mCx->malloc(newCap * sizeof(T)));
    if (!newBuf)
        return false;

    /* Copy inline elements into heap buffer. */
    size_t size = inlineSize();
    Impl::copyConstruct(newBuf, inlineBegin(), inlineEnd());
    Impl::destroy(inlineBegin(), inlineEnd());

    /* Switch in heap buffer. */
    mSizeOrCapacity = newCap;  /* marks us as !usingInlineStorage() */
    heapBegin() = newBuf;
    heapEnd() = newBuf + size;
    return true;
}

template <class T, size_t N>
inline bool
JSTempVector<T,N>::reserve(size_t request)
{
    ReentrancyGuard g(*this);
    if (usingInlineStorage()) {
        if (request > sInlineCapacity)
            return convertToHeapStorage(request - inlineSize());
    } else {
        if (request > heapCapacity())
            return growHeapStorageBy(request - heapSize());
    }
    return true;
}

template <class T, size_t N>
inline void
JSTempVector<T,N>::shrinkBy(size_t incr)
{
    ReentrancyGuard g(*this);
    JS_ASSERT(incr <= size());
    if (usingInlineStorage()) {
        Impl::destroy(inlineEnd() - incr, inlineEnd());
        inlineSize() -= incr;
    } else {
        Impl::destroy(heapEnd() - incr, heapEnd());
        heapEnd() -= incr;
    }
}

template <class T, size_t N>
inline bool
JSTempVector<T,N>::growBy(size_t incr)
{
    ReentrancyGuard g(*this);
    if (usingInlineStorage()) {
        size_t freespace = sInlineCapacity - inlineSize();
        if (incr <= freespace) {
            T *newend = inlineEnd() + incr;
            if (!JSUtils::IsPodType<T>::result)
                Impl::initialize(inlineEnd(), newend);
            inlineSize() += incr;
            JS_ASSERT(usingInlineStorage());
            return true;
        }
        if (!convertToHeapStorage(incr))
            return false;
    }
    else {
        /* grow if needed */
        size_t freespace = heapCapacity() - heapSize();
        if (incr > freespace) {
            if (!growHeapStorageBy(incr))
                return false;
        }
    }

    /* We are !usingInlineStorage(). Initialize new elements. */
    JS_ASSERT(heapCapacity() - heapSize() >= incr);
    T *newend = heapEnd() + incr;
    if (!JSUtils::IsPodType<T>::result)
        Impl::initialize(heapEnd(), newend);
    heapEnd() = newend;
    return true;
}

template <class T, size_t N>
inline bool
JSTempVector<T,N>::resize(size_t newsize)
{
    size_t cursize = size();
    if (newsize > cursize)
        return growBy(newsize - cursize);
    shrinkBy(cursize - newsize);
    return true;
}

template <class T, size_t N>
inline void
JSTempVector<T,N>::clear()
{
    ReentrancyGuard g(*this);
    if (usingInlineStorage()) {
        Impl::destroy(inlineBegin(), inlineEnd());
        inlineSize() = 0;
    }
    else {
        Impl::destroy(heapBegin(), heapEnd());
        heapEnd() = heapBegin();
    }
}

template <class T, size_t N>
inline bool
JSTempVector<T,N>::append(const T &t)
{
    ReentrancyGuard g(*this);
    if (usingInlineStorage()) {
        if (inlineSize() < sInlineCapacity) {
            new(inlineEnd()) T(t);
            ++inlineSize();
            JS_ASSERT(usingInlineStorage());
            return true;
        }
        if (!convertToHeapStorage(1))
            return false;
    } else {
        if (heapSize() == heapCapacity() && !growHeapStorageBy(1))
            return false;
    }

    /* We are !usingInlineStorage(). Initialize new elements. */
    JS_ASSERT(heapSize() <= heapCapacity() && heapCapacity() - heapSize() >= 1);
    new(heapEnd()++) T(t);
    return true;
}

template <class T, size_t N>
inline bool
JSTempVector<T,N>::appendN(const T &t, size_t needed)
{
    ReentrancyGuard g(*this);
    if (usingInlineStorage()) {
        size_t freespace = sInlineCapacity - inlineSize();
        if (needed <= freespace) {
            Impl::copyConstructN(inlineEnd(), needed, t);
            inlineSize() += needed;
            JS_ASSERT(usingInlineStorage());
            return true;
        }
        if (!convertToHeapStorage(needed))
            return false;
    } else {
        size_t freespace = heapCapacity() - heapSize();
        if (needed > freespace && !growHeapStorageBy(needed))
            return false;
    }

    /* We are !usingInlineStorage(). Initialize new elements. */
    JS_ASSERT(heapSize() <= heapCapacity() && heapCapacity() - heapSize() >= needed);
    Impl::copyConstructN(heapEnd(), needed, t);
    heapEnd() += needed;
    return true;
}

template <class T, size_t N>
template <class U>
inline bool
JSTempVector<T,N>::append(const U *insBegin, const U *insEnd)
{
    ReentrancyGuard g(*this);
    size_t needed = JSUtils::PointerRangeSize(insBegin, insEnd);
    if (usingInlineStorage()) {
        size_t freespace = sInlineCapacity - inlineSize();
        if (needed <= freespace) {
            Impl::copyConstruct(inlineEnd(), insBegin, insEnd);
            inlineSize() += needed;
            JS_ASSERT(usingInlineStorage());
            return true;
        }
        if (!convertToHeapStorage(needed))
            return false;
    } else {
        size_t freespace = heapCapacity() - heapSize();
        if (needed > freespace && !growHeapStorageBy(needed))
            return false;
    }

    /* We are !usingInlineStorage(). Initialize new elements. */
    JS_ASSERT(heapSize() <= heapCapacity() && heapCapacity() - heapSize() >= needed);
    Impl::copyConstruct(heapEnd(), insBegin, insEnd);
    heapEnd() += needed;
    return true;
}

template <class T, size_t N>
template <class U>
inline bool
JSTempVector<T,N>::append(const U *insBegin, size_t length)
{
    return this->append(insBegin, insBegin + length);
}

template <class T, size_t N>
inline void
JSTempVector<T,N>::popBack()
{
    ReentrancyGuard g(*this);
    JS_ASSERT(!empty());
    if (usingInlineStorage()) {
        --inlineSize();
        inlineEnd()->~T();
    } else {
        --heapEnd();
        heapEnd()->~T();
    }
}

template <class T, size_t N>
inline T *
JSTempVector<T,N>::extractRawBuffer()
{
    if (usingInlineStorage()) {
        T *ret = reinterpret_cast<T *>(mCx->malloc(inlineSize() * sizeof(T)));
        if (!ret)
            return NULL;
        Impl::copyConstruct(ret, inlineBegin(), inlineEnd());
        Impl::destroy(inlineBegin(), inlineEnd());
        inlineSize() = 0;
        return ret;
    }

    T *ret = heapBegin();
    mSizeOrCapacity = 0;  /* marks us as !usingInlineStorage() */
    return ret;
}

template <class T, size_t N>
inline void
JSTempVector<T,N>::replaceRawBuffer(T *p, size_t length)
{
    ReentrancyGuard g(*this);

    /* Destroy what we have. */
    if (usingInlineStorage()) {
        Impl::destroy(inlineBegin(), inlineEnd());
        inlineSize() = 0;
    } else {
        Impl::destroy(heapBegin(), heapEnd());
        mCx->free(heapBegin());
    }

    /* Take in the new buffer. */
    if (length <= sInlineCapacity) {
        /*
         * (mSizeOrCapacity <= sInlineCapacity) means inline storage, so we MUST
         * use inline storage, even though p might otherwise be acceptable.
         */
        mSizeOrCapacity = length;  /* marks us as usingInlineStorage() */
        Impl::copyConstruct(inlineBegin(), p, p + length);
        Impl::destroy(p, p + length);
        mCx->free(p);
    } else {
        mSizeOrCapacity = length;  /* marks us as !usingInlineStorage() */
        heapBegin() = p;
        heapEnd() = heapBegin() + length;
    }
}

#endif /* jsvector_h_ */

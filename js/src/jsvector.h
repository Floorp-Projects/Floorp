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

#include "jscntxt.h"

#include <string.h>
#include <new>

/*
 * Traits class for identifying POD types.  Until C++0x, there is no automatic
 * way to detect PODs, so for the moment it is done manually.
 */
template <class T> struct IsPodType     { static const bool result = false; };
template <> struct IsPodType<char>      { static const bool result = true; };
template <> struct IsPodType<int>       { static const bool result = true; };
template <> struct IsPodType<short>     { static const bool result = true; };
template <> struct IsPodType<long>      { static const bool result = true; };
template <> struct IsPodType<float>     { static const bool result = true; };
template <> struct IsPodType<double>    { static const bool result = true; };
template <> struct IsPodType<jschar>    { static const bool result = true; };

/*
 * This template class provides a default implementation for vector operations
 * when the element type is not known to be a POD, as judged by IsPodType.
 */
template <class T, bool IsPod>
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
    static inline void copyInitialize(T *dst, const U *srcbeg, const U *srcend) {
        for (const U *p = srcbeg; p != srcend; ++p, ++dst)
            new(dst) T(*p);
    }

    /*
     * Grows the given buffer to have capacity newcap, preserving the objects
     * constructed in the range [begin, end) and updating vec.
     */
    static inline bool growTo(JSTempVector<T> &vec, size_t newcap) {
        size_t bytes = sizeof(T) * newcap;
        T *newbuf = reinterpret_cast<T *>(malloc(bytes));
        if (!newbuf) {
            js_ReportOutOfMemory(vec.mCx);
            return false;
        }
        for (T *dst = newbuf, *src = vec.mBegin; src != vec.mEnd; ++dst, ++src)
            new(dst) T(*src);
        JSTempVectorImpl::destroy(vec.mBegin, vec.mEnd);
        free(vec.mBegin);
        vec.mEnd = newbuf + (vec.mEnd - vec.mBegin);
        vec.mBegin = newbuf;
        vec.mCapacity = newbuf + newcap;
        return true;
    }
};

/*
 * This partial template specialization provides a default implementation for
 * vector operations when the element type is known to be a POD, as judged by
 * IsPodType.
 */
template <class T>
struct JSTempVectorImpl<T, true>
{
    static inline void destroy(T *, T *) {}

    static inline void initialize(T *begin, T *end) {
        //memset(begin, 0, sizeof(T) * (end-begin));  //SLOWER
        for (T *p = begin; p != end; ++p)
            *p = 0;
    }

    static inline void copyInitialize(T *dst, const T *srcbeg, const T *srcend) {
        //memcpy(dst, srcbeg, sizeof(T) * (srcend-srcbeg));  //SLOWER
        for (const T *p = srcbeg; p != srcend; ++p, ++dst)
            *dst = *p;
    }

    static inline bool growTo(JSTempVector<T> &vec, size_t newcap) {
        size_t bytes = sizeof(T) * newcap;
        T *newbuf = reinterpret_cast<T *>(realloc(vec.mBegin, bytes));
        if (!newbuf) {
            js_ReportOutOfMemory(vec.mCx);
            return false;
        }
        vec.mEnd = newbuf + (vec.mEnd - vec.mBegin);
        vec.mBegin = newbuf;
        vec.mCapacity = newbuf + newcap;
        return true;
    }
};

/*
 * JS-friendly, STL-like container providing a short-lived, dynamic buffer.
 * JSTempVector calls the constructors/destructors of all elements stored in
 * its internal buffer, so non-PODs may be safely used.
 *
 * T requirements:
 *  - default and copy constructible, assignable, destructible
 *  - operations do not throw
 *
 * N.B: JSTempVector is not reentrant: T member functions called during
 *      JSTempVector member functions must not call back into the same
 *      JSTempVector.
 */
template <class T>
class JSTempVector
{
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

  public:
    JSTempVector(JSContext *cx)
      :
#ifdef DEBUG
        mInProgress(false),
#endif
        mCx(cx), mBegin(0), mEnd(0), mCapacity(0)
    {}
    ~JSTempVector();

    JSTempVector(const JSTempVector &);
    JSTempVector &operator=(const JSTempVector &);

    /* accessors */

    size_t size() const     { return mEnd - mBegin; }
    size_t capacity() const { return mCapacity - mBegin; }
    bool empty() const      { return mBegin == mEnd; }

    T &operator[](size_t i) {
        JS_ASSERT(!mInProgress && i < size());
        return mBegin[i];
    }

    const T &operator[](size_t i) const {
        JS_ASSERT(!mInProgress && i < size());
        return mBegin[i];
    }

    T *begin() {
        JS_ASSERT(!mInProgress);
        return mBegin;
    }

    const T *begin() const {
        JS_ASSERT(!mInProgress);
        return mBegin;
    }

    T *end() {
        JS_ASSERT(!mInProgress);
        return mEnd;
    }

    const T *end() const {
        JS_ASSERT(!mInProgress);
        return mEnd;
    }

    T &back() {
        JS_ASSERT(!mInProgress);
        return *(mEnd - 1);
    }

    const T &back() const {
        JS_ASSERT(!mInProgress && !empty());
        return *(mEnd - 1);
    }

    /* mutators */

    bool reserve(size_t);
    bool growBy(size_t);
    void clear();

    bool pushBack(const T &);
    template <class U> bool pushBack(const U *begin, const U *end);

    /*
     * Transfers ownership of the internal buffer used by JSTempVector to the
     * caller.  After this call, the JSTempVector is empty.
     * N.B. Although a T*, only the range [0, size()) is constructed.
     */
    T *extractRawBuffer();

    /*
     * Transfer ownership of an array of objects into the JSTempVector.
     * N.B. This call assumes that there are no uninitialized elements in the
     *      passed array.
     */
    void replaceRawBuffer(T *, size_t length);

  private:
    typedef JSTempVectorImpl<T, IsPodType<T>::result> Impl;
    friend class JSTempVectorImpl<T, IsPodType<T>::result>;

    static const int sGrowthFactor = 3;

    bool checkOverflow(size_t newval, size_t oldval, size_t diff) const;

    JSContext *mCx;
    T *mBegin, *mEnd, *mCapacity;
};

template <class T>
inline
JSTempVector<T>::~JSTempVector()
{
    ReentrancyGuard g(*this);
    Impl::destroy(mBegin, mEnd);
    free(mBegin);
}

template <class T>
inline bool
JSTempVector<T>::reserve(size_t newsz)
{
    ReentrancyGuard g(*this);
    size_t oldcap = capacity();
    if (newsz > oldcap) {
        size_t diff = newsz - oldcap;
        size_t newcap = diff + oldcap * sGrowthFactor;
        return checkOverflow(newcap, oldcap, diff) &&
               Impl::growTo(*this, newcap);
    }
    return true;
}

template <class T>
inline bool
JSTempVector<T>::growBy(size_t amount)
{
    /* grow if needed */
    size_t oldsize = size(), newsize = oldsize + amount;
    if (!checkOverflow(newsize, oldsize, amount) ||
        (newsize > capacity() && !reserve(newsize)))
        return false;

    /* initialize new elements */
    ReentrancyGuard g(*this);
    JS_ASSERT(mCapacity - (mBegin + newsize) >= 0);
    T *newend = mBegin + newsize;
    Impl::initialize(mEnd, newend);
    mEnd = newend;
    return true;
}

template <class T>
inline void
JSTempVector<T>::clear()
{
    ReentrancyGuard g(*this);
    Impl::destroy(mBegin, mEnd);
    mEnd = mBegin;
}

/*
 * Check for overflow of an increased size or capacity (generically, 'value').
 * 'diff' is how much greater newval should be compared to oldval.
 */
template <class T>
inline bool
JSTempVector<T>::checkOverflow(size_t newval, size_t oldval, size_t diff) const
{
    size_t newbytes = newval * sizeof(T),
           oldbytes = oldval * sizeof(T),
           diffbytes = diff * sizeof(T);
    bool ok = newbytes >= oldbytes && (newbytes - oldbytes) >= diffbytes;
    if (!ok)
        js_ReportAllocationOverflow(mCx);
    return ok;
}

template <class T>
inline bool
JSTempVector<T>::pushBack(const T &t)
{
    ReentrancyGuard g(*this);
    if (mEnd == mCapacity) {
        /* reallocate, doubling size */
        size_t oldcap = capacity();
        size_t newcap = empty() ? 1 : oldcap * sGrowthFactor;
        if (!checkOverflow(newcap, oldcap, 1) ||
            !Impl::growTo(*this, newcap))
            return false;
    }
    JS_ASSERT(mEnd != mCapacity);
    new(mEnd++) T(t);
    return true;
}

template <class T>
template <class U>
inline bool
JSTempVector<T>::pushBack(const U *begin, const U *end)
{
    ReentrancyGuard g(*this);
    size_t space = mCapacity - mEnd, needed = end - begin;
    if (space < needed) {
        /* reallocate, doubling size */
        size_t oldcap = capacity();
        size_t newcap = empty() ? needed : (needed + oldcap * sGrowthFactor);
        if (!checkOverflow(newcap, oldcap, needed) ||
            !Impl::growTo(*this, newcap))
            return false;
    }
    JS_ASSERT((mCapacity - mEnd) >= (end - begin));
    Impl::copyInitialize(mEnd, begin, end);
    mEnd += needed;
    return true;
}

template <class T>
inline T *
JSTempVector<T>::extractRawBuffer()
{
    T *ret = mBegin;
    mBegin = mEnd = mCapacity = 0;
    return ret;
}

template <class T>
inline void
JSTempVector<T>::replaceRawBuffer(T *p, size_t length)
{
    ReentrancyGuard g(*this);
    Impl::destroy(mBegin, mEnd);
    free(mBegin);
    mBegin = p;
    mCapacity = mEnd = mBegin + length;
}

#endif /* jsvector_h_ */

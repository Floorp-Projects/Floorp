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

/*
 * PR assertion checker.
 */

#ifndef jsutil_h___
#define jsutil_h___

#include "js/Utility.h"

/* Forward declarations. */
struct JSContext;

#ifdef __cplusplus
namespace js {

template <class T>
class AlignedPtrAndFlag
{
    uintptr_t bits;

  public:
    AlignedPtrAndFlag(T *t, bool flag) {
        JS_ASSERT((uintptr_t(t) & 1) == 0);
        bits = uintptr_t(t) | uintptr_t(flag);
    }

    T *ptr() const {
        return (T *)(bits & ~uintptr_t(1));
    }

    bool flag() const {
        return (bits & 1) != 0;
    }

    void setPtr(T *t) {
        JS_ASSERT((uintptr_t(t) & 1) == 0);
        bits = uintptr_t(t) | uintptr_t(flag());
    }

    void setFlag() {
        bits |= 1;
    }

    void unsetFlag() {
        bits &= ~uintptr_t(1);
    }

    void set(T *t, bool flag) {
        JS_ASSERT((uintptr_t(t) & 1) == 0);
        bits = uintptr_t(t) | flag;
    }
};

template <class T>
static inline void
Reverse(T *beg, T *end)
{
    while (beg != end) {
        if (--end == beg)
            return;
        T tmp = *beg;
        *beg = *end;
        *end = tmp;
        ++beg;
    }
}

template <class T>
static inline T *
Find(T *beg, T *end, const T &v)
{
    for (T *p = beg; p != end; ++p) {
        if (*p == v)
            return p;
    }
    return end;
}

template <class Container>
static inline typename Container::ElementType *
Find(Container &c, const typename Container::ElementType &v)
{
    return Find(c.begin(), c.end(), v);
}

template <typename InputIterT, typename CallableT>
void
ForEach(InputIterT begin, InputIterT end, CallableT f)
{
    for (; begin != end; ++begin)
        f(*begin);
}

template <class T>
static inline T
Min(T t1, T t2)
{
    return t1 < t2 ? t1 : t2;
}

template <class T>
static inline T
Max(T t1, T t2)
{
    return t1 > t2 ? t1 : t2;
}

/* Allows a const variable to be initialized after its declaration. */
template <class T>
static T&
InitConst(const T &t)
{
    return const_cast<T &>(t);
}

template <class T, class U>
JS_ALWAYS_INLINE T &
ImplicitCast(U &u)
{
    T &t = u;
    return t;
}

template<typename T>
class AutoScopedAssign
{
  private:
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
    T *addr;
    T old;

  public:
    AutoScopedAssign(T *addr, const T &value JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : addr(addr), old(*addr)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        *addr = value;
    }

    ~AutoScopedAssign() { *addr = old; }
};

template <class RefCountable>
class AlreadyIncRefed
{
    typedef RefCountable *****ConvertibleToBool;

    RefCountable *obj;

  public:
    explicit AlreadyIncRefed(RefCountable *obj = NULL) : obj(obj) {}

    bool null() const { return obj == NULL; }
    operator ConvertibleToBool() const { return (ConvertibleToBool)obj; }

    RefCountable *operator->() const { JS_ASSERT(!null()); return obj; }
    RefCountable &operator*() const { JS_ASSERT(!null()); return *obj; }
    RefCountable *get() const { return obj; }
};

template <class RefCountable>
class NeedsIncRef
{
    typedef RefCountable *****ConvertibleToBool;

    RefCountable *obj;

  public:
    explicit NeedsIncRef(RefCountable *obj = NULL) : obj(obj) {}

    bool null() const { return obj == NULL; }
    operator ConvertibleToBool() const { return (ConvertibleToBool)obj; }

    RefCountable *operator->() const { JS_ASSERT(!null()); return obj; }
    RefCountable &operator*() const { JS_ASSERT(!null()); return *obj; }
    RefCountable *get() const { return obj; }
};

template <class RefCountable>
class AutoRefCount
{
    typedef RefCountable *****ConvertibleToBool;

    JSContext *const cx;
    RefCountable *obj;

    AutoRefCount(const AutoRefCount &);
    void operator=(const AutoRefCount &);

  public:
    explicit AutoRefCount(JSContext *cx)
      : cx(cx), obj(NULL)
    {}

    AutoRefCount(JSContext *cx, NeedsIncRef<RefCountable> aobj)
      : cx(cx), obj(aobj.get())
    {
        if (obj)
            obj->incref(cx);
    }

    AutoRefCount(JSContext *cx, AlreadyIncRefed<RefCountable> aobj)
      : cx(cx), obj(aobj.get())
    {}

    ~AutoRefCount() {
        if (obj)
            obj->decref(cx);
    }

    void reset(NeedsIncRef<RefCountable> aobj) {
        if (obj)
            obj->decref(cx);
        obj = aobj.get();
        if (obj)
            obj->incref(cx);
    }

    void reset(AlreadyIncRefed<RefCountable> aobj) {
        if (obj)
            obj->decref(cx);
        obj = aobj.get();
    }

    bool null() const { return obj == NULL; }
    operator ConvertibleToBool() const { return (ConvertibleToBool)obj; }

    RefCountable *operator->() const { JS_ASSERT(!null()); return obj; }
    RefCountable &operator*() const { JS_ASSERT(!null()); return *obj; }
    RefCountable *get() const { return obj; }
};

template <class T>
JS_ALWAYS_INLINE static void
PodZero(T *t)
{
    memset(t, 0, sizeof(T));
}

template <class T>
JS_ALWAYS_INLINE static void
PodZero(T *t, size_t nelem)
{
    memset(t, 0, nelem * sizeof(T));
}

/*
 * Arrays implicitly convert to pointers to their first element, which is
 * dangerous when combined with the above PodZero definitions. Adding an
 * overload for arrays is ambiguous, so we need another identifier. The
 * ambiguous overload is left to catch mistaken uses of PodZero; if you get a
 * compile error involving PodZero and array types, use PodArrayZero instead.
 */
template <class T, size_t N> static void PodZero(T (&)[N]);          /* undefined */
template <class T, size_t N> static void PodZero(T (&)[N], size_t);  /* undefined */

template <class T, size_t N>
JS_ALWAYS_INLINE static void
PodArrayZero(T (&t)[N])
{
    memset(t, 0, N * sizeof(T));
}

template <class T>
JS_ALWAYS_INLINE static void
PodCopy(T *dst, const T *src, size_t nelem)
{
    /* Cannot find portable word-sized abs(). */
    JS_ASSERT_IF(dst >= src, size_t(dst - src) >= nelem);
    JS_ASSERT_IF(src >= dst, size_t(src - dst) >= nelem);

    if (nelem < 128) {
        for (const T *srcend = src + nelem; src != srcend; ++src, ++dst)
            *dst = *src;
    } else {
        memcpy(dst, src, nelem * sizeof(T));
    }
}

template <class T>
JS_ALWAYS_INLINE static bool
PodEqual(T *one, T *two, size_t len)
{
    if (len < 128) {
        T *p1end = one + len;
        for (T *p1 = one, *p2 = two; p1 != p1end; ++p1, ++p2) {
            if (*p1 != *p2)
                return false;
        }
        return true;
    }

    return !memcmp(one, two, len * sizeof(T));
}

JS_ALWAYS_INLINE static size_t
UnsignedPtrDiff(const void *bigger, const void *smaller)
{
    return size_t(bigger) - size_t(smaller);
}

/*
 * Ordinarily, a function taking a JSContext* 'cx' paremter reports errors on
 * the context. In some cases, functions optionally report and indicate this by
 * taking a nullable 'maybecx' parameter. In some cases, though, a function
 * always needs a 'cx', but optionally reports. This option is presented by the
 * MaybeReportError.
 */
enum MaybeReportError { REPORT_ERROR = true, DONT_REPORT_ERROR = false };

}  /* namespace js */
#endif  /* __cplusplus */

/*
 * JS_ROTATE_LEFT32
 *
 * There is no rotate operation in the C Language so the construct (a << 4) |
 * (a >> 28) is used instead. Most compilers convert this to a rotate
 * instruction but some versions of MSVC don't without a little help.  To get
 * MSVC to generate a rotate instruction, we have to use the _rotl intrinsic
 * and use a pragma to make _rotl inline.
 *
 * MSVC in VS2005 will do an inline rotate instruction on the above construct.
 */
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_AMD64) || \
    defined(_M_X64))
#include <stdlib.h>
#pragma intrinsic(_rotl)
#define JS_ROTATE_LEFT32(a, bits) _rotl(a, bits)
#else
#define JS_ROTATE_LEFT32(a, bits) (((a) << (bits)) | ((a) >> (32 - (bits))))
#endif

/* Static control-flow checks. */
#ifdef NS_STATIC_CHECKING
/* Trigger a control flow check to make sure that code flows through label */
inline __attribute__ ((unused)) void MUST_FLOW_THROUGH(const char *label) {}

/* Avoid unused goto-label warnings. */
# define MUST_FLOW_LABEL(label) goto label; label:

inline JS_FORCES_STACK void VOUCH_DOES_NOT_REQUIRE_STACK() {}

inline JS_FORCES_STACK void
JS_ASSERT_NOT_ON_TRACE(JSContext *cx)
{
    JS_ASSERT(!JS_ON_TRACE(cx));
}
#else
# define MUST_FLOW_THROUGH(label)            ((void) 0)
# define MUST_FLOW_LABEL(label)
# define VOUCH_DOES_NOT_REQUIRE_STACK()      ((void) 0)
# define JS_ASSERT_NOT_ON_TRACE(cx)          JS_ASSERT(!JS_ON_TRACE(cx))
#endif

/* Crash diagnostics */
#ifdef DEBUG
# define JS_CRASH_DIAGNOSTICS 1
#endif
#ifdef JS_CRASH_DIAGNOSTICS
# define JS_POISON(p, val, size) memset((p), (val), (size))
# define JS_OPT_ASSERT(expr)                                                  \
    ((expr) ? (void)0 : JS_Assert(#expr, __FILE__, __LINE__))
# define JS_OPT_ASSERT_IF(cond, expr)                                         \
    ((!(cond) || (expr)) ? (void)0 : JS_Assert(#expr, __FILE__, __LINE__))
#else
# define JS_POISON(p, val, size) ((void) 0)
# define JS_OPT_ASSERT(expr) ((void) 0)
# define JS_OPT_ASSERT_IF(cond, expr) ((void) 0)
#endif

/* Basic stats */
#ifdef DEBUG
# define JS_BASIC_STATS 1
#endif
#ifdef JS_BASIC_STATS
# include <stdio.h>
typedef struct JSBasicStats {
    uint32      num;
    uint32      max;
    double      sum;
    double      sqsum;
    uint32      logscale;           /* logarithmic scale: 0 (linear), 2, 10 */
    uint32      hist[11];
} JSBasicStats;
# define JS_INIT_STATIC_BASIC_STATS  {0,0,0,0,0,{0,0,0,0,0,0,0,0,0,0,0}}
# define JS_BASIC_STATS_INIT(bs)     memset((bs), 0, sizeof(JSBasicStats))
# define JS_BASIC_STATS_ACCUM(bs,val)                                         \
    JS_BasicStatsAccum(bs, val)
# define JS_MeanAndStdDevBS(bs,sigma)                                         \
    JS_MeanAndStdDev((bs)->num, (bs)->sum, (bs)->sqsum, sigma)
extern void
JS_BasicStatsAccum(JSBasicStats *bs, uint32 val);
extern double
JS_MeanAndStdDev(uint32 num, double sum, double sqsum, double *sigma);
extern void
JS_DumpBasicStats(JSBasicStats *bs, const char *title, FILE *fp);
extern void
JS_DumpHistogram(JSBasicStats *bs, FILE *fp);
#else
# define JS_BASIC_STATS_ACCUM(bs,val)
#endif

/* A jsbitmap_t is a long integer that can be used for bitmaps. */
typedef size_t jsbitmap;
#define JS_TEST_BIT(_map,_bit)  ((_map)[(_bit)>>JS_BITS_PER_WORD_LOG2] &      \
                                 ((jsbitmap)1<<((_bit)&(JS_BITS_PER_WORD-1))))
#define JS_SET_BIT(_map,_bit)   ((_map)[(_bit)>>JS_BITS_PER_WORD_LOG2] |=     \
                                 ((jsbitmap)1<<((_bit)&(JS_BITS_PER_WORD-1))))
#define JS_CLEAR_BIT(_map,_bit) ((_map)[(_bit)>>JS_BITS_PER_WORD_LOG2] &=     \
                                 ~((jsbitmap)1<<((_bit)&(JS_BITS_PER_WORD-1))))

#define VOUCH_HAVE_STACK                    VOUCH_DOES_NOT_REQUIRE_STACK

#endif /* jsutil_h___ */

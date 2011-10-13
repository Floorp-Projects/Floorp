/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
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
 * June 30, 2010
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

#ifndef jsvalimpl_h__
#define jsvalimpl_h__
/*
 * Implementation details for js::Value in jsapi.h.
 */
#include "js/Utility.h"

JS_BEGIN_EXTERN_C

/******************************************************************************/

/* To avoid a circular dependency, pull in the necessary pieces of jsnum.h. */

#define JSDOUBLE_SIGNBIT (((uint64) 1) << 63)
#define JSDOUBLE_EXPMASK (((uint64) 0x7ff) << 52)
#define JSDOUBLE_MANTMASK ((((uint64) 1) << 52) - 1)
#define JSDOUBLE_HI32_SIGNBIT   0x80000000

static JS_ALWAYS_INLINE JSBool
JSDOUBLE_IS_NEGZERO(double d)
{
    union {
        struct {
#if defined(IS_LITTLE_ENDIAN) && !defined(FPU_IS_ARM_FPA)
            uint32 lo, hi;
#else
            uint32 hi, lo;
#endif
        } s;
        double d;
    } x;
    if (d != 0)
        return JS_FALSE;
    x.d = d;
    return (x.s.hi & JSDOUBLE_HI32_SIGNBIT) != 0;
}

static JS_ALWAYS_INLINE JSBool
JSDOUBLE_IS_INT32(double d, int32* pi)
{
    if (JSDOUBLE_IS_NEGZERO(d))
        return JS_FALSE;
    return d == (*pi = (int32)d);
}

/******************************************************************************/

/*
 * Try to get jsvals 64-bit aligned. We could almost assert that all values are
 * aligned, but MSVC and GCC occasionally break alignment.
 */
#if defined(__GNUC__) || defined(__xlc__) || defined(__xlC__)
# define JSVAL_ALIGNMENT        __attribute__((aligned (8)))
#elif defined(_MSC_VER)
  /*
   * Structs can be aligned with MSVC, but not if they are used as parameters,
   * so we just don't try to align.
   */
# define JSVAL_ALIGNMENT
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
# define JSVAL_ALIGNMENT
#elif defined(__HP_cc) || defined(__HP_aCC)
# define JSVAL_ALIGNMENT
#endif

#if JS_BITS_PER_WORD == 64
# define JSVAL_TAG_SHIFT 47
#endif

/*
 * We try to use enums so that printing a jsval_layout in the debugger shows
 * nice symbolic type tags, however we can only do this when we can force the
 * underlying type of the enum to be the desired size.
 */
#if defined(__cplusplus) && !defined(__SUNPRO_CC) && !defined(__xlC__)

#if defined(_MSC_VER)
# define JS_ENUM_HEADER(id, type)              enum id : type
# define JS_ENUM_MEMBER(id, type, value)       id = (type)value,
# define JS_LAST_ENUM_MEMBER(id, type, value)  id = (type)value
# define JS_ENUM_FOOTER(id)
#else
# define JS_ENUM_HEADER(id, type)              enum id
# define JS_ENUM_MEMBER(id, type, value)       id = (type)value,
# define JS_LAST_ENUM_MEMBER(id, type, value)  id = (type)value
# define JS_ENUM_FOOTER(id)                    __attribute__((packed))
#endif

/* Remember to propagate changes to the C defines below. */
JS_ENUM_HEADER(JSValueType, uint8)
{
    JSVAL_TYPE_DOUBLE              = 0x00,
    JSVAL_TYPE_INT32               = 0x01,
    JSVAL_TYPE_UNDEFINED           = 0x02,
    JSVAL_TYPE_BOOLEAN             = 0x03,
    JSVAL_TYPE_MAGIC               = 0x04,
    JSVAL_TYPE_STRING              = 0x05,
    JSVAL_TYPE_NULL                = 0x06,
    JSVAL_TYPE_OBJECT              = 0x07,

    /* The below types never appear in a jsval; they are only used in tracing and type inference. */

    JSVAL_TYPE_UNKNOWN             = 0x20,

    JSVAL_TYPE_NONFUNOBJ           = 0x57,
    JSVAL_TYPE_FUNOBJ              = 0x67,

    JSVAL_TYPE_STRORNULL           = 0x77,
    JSVAL_TYPE_OBJORNULL           = 0x78,

    JSVAL_TYPE_BOXED               = 0x79
} JS_ENUM_FOOTER(JSValueType);

JS_STATIC_ASSERT(sizeof(JSValueType) == 1);

#if JS_BITS_PER_WORD == 32

/* Remember to propagate changes to the C defines below. */
JS_ENUM_HEADER(JSValueTag, uint32)
{
    JSVAL_TAG_CLEAR                = 0xFFFFFF80,
    JSVAL_TAG_INT32                = JSVAL_TAG_CLEAR | JSVAL_TYPE_INT32,
    JSVAL_TAG_UNDEFINED            = JSVAL_TAG_CLEAR | JSVAL_TYPE_UNDEFINED,
    JSVAL_TAG_STRING               = JSVAL_TAG_CLEAR | JSVAL_TYPE_STRING,
    JSVAL_TAG_BOOLEAN              = JSVAL_TAG_CLEAR | JSVAL_TYPE_BOOLEAN,
    JSVAL_TAG_MAGIC                = JSVAL_TAG_CLEAR | JSVAL_TYPE_MAGIC,
    JSVAL_TAG_NULL                 = JSVAL_TAG_CLEAR | JSVAL_TYPE_NULL,
    JSVAL_TAG_OBJECT               = JSVAL_TAG_CLEAR | JSVAL_TYPE_OBJECT
} JS_ENUM_FOOTER(JSValueTag);

JS_STATIC_ASSERT(sizeof(JSValueTag) == 4);

#elif JS_BITS_PER_WORD == 64

/* Remember to propagate changes to the C defines below. */
JS_ENUM_HEADER(JSValueTag, uint32)
{
    JSVAL_TAG_MAX_DOUBLE           = 0x1FFF0,
    JSVAL_TAG_INT32                = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_INT32,
    JSVAL_TAG_UNDEFINED            = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_UNDEFINED,
    JSVAL_TAG_STRING               = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_STRING,
    JSVAL_TAG_BOOLEAN              = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_BOOLEAN,
    JSVAL_TAG_MAGIC                = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_MAGIC,
    JSVAL_TAG_NULL                 = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_NULL,
    JSVAL_TAG_OBJECT               = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_OBJECT
} JS_ENUM_FOOTER(JSValueTag);

JS_STATIC_ASSERT(sizeof(JSValueTag) == sizeof(uint32));

JS_ENUM_HEADER(JSValueShiftedTag, uint64)
{
    JSVAL_SHIFTED_TAG_MAX_DOUBLE   = ((((uint64)JSVAL_TAG_MAX_DOUBLE) << JSVAL_TAG_SHIFT) | 0xFFFFFFFF),
    JSVAL_SHIFTED_TAG_INT32        = (((uint64)JSVAL_TAG_INT32)      << JSVAL_TAG_SHIFT),
    JSVAL_SHIFTED_TAG_UNDEFINED    = (((uint64)JSVAL_TAG_UNDEFINED)  << JSVAL_TAG_SHIFT),
    JSVAL_SHIFTED_TAG_STRING       = (((uint64)JSVAL_TAG_STRING)     << JSVAL_TAG_SHIFT),
    JSVAL_SHIFTED_TAG_BOOLEAN      = (((uint64)JSVAL_TAG_BOOLEAN)    << JSVAL_TAG_SHIFT),
    JSVAL_SHIFTED_TAG_MAGIC        = (((uint64)JSVAL_TAG_MAGIC)      << JSVAL_TAG_SHIFT),
    JSVAL_SHIFTED_TAG_NULL         = (((uint64)JSVAL_TAG_NULL)       << JSVAL_TAG_SHIFT),
    JSVAL_SHIFTED_TAG_OBJECT       = (((uint64)JSVAL_TAG_OBJECT)     << JSVAL_TAG_SHIFT)
} JS_ENUM_FOOTER(JSValueShiftedTag);

JS_STATIC_ASSERT(sizeof(JSValueShiftedTag) == sizeof(uint64));

#endif

#else  /* defined(__cplusplus) */

typedef uint8 JSValueType;
#define JSVAL_TYPE_DOUBLE            ((uint8)0x00)
#define JSVAL_TYPE_INT32             ((uint8)0x01)
#define JSVAL_TYPE_UNDEFINED         ((uint8)0x02)
#define JSVAL_TYPE_BOOLEAN           ((uint8)0x03)
#define JSVAL_TYPE_MAGIC             ((uint8)0x04)
#define JSVAL_TYPE_STRING            ((uint8)0x05)
#define JSVAL_TYPE_NULL              ((uint8)0x06)
#define JSVAL_TYPE_OBJECT            ((uint8)0x07)
#define JSVAL_TYPE_UNKNOWN           ((uint8)0x20)
#define JSVAL_TYPE_NONFUNOBJ         ((uint8)0x57)
#define JSVAL_TYPE_FUNOBJ            ((uint8)0x67)
#define JSVAL_TYPE_STRORNULL         ((uint8)0x77)
#define JSVAL_TYPE_OBJORNULL         ((uint8)0x78)
#define JSVAL_TYPE_BOXED             ((uint8)0x79)
#define JSVAL_TYPE_UNINITIALIZED     ((uint8)0x7d)

#if JS_BITS_PER_WORD == 32

typedef uint32 JSValueTag;
#define JSVAL_TAG_CLEAR              ((uint32)(0xFFFFFF80))
#define JSVAL_TAG_INT32              ((uint32)(JSVAL_TAG_CLEAR | JSVAL_TYPE_INT32))
#define JSVAL_TAG_UNDEFINED          ((uint32)(JSVAL_TAG_CLEAR | JSVAL_TYPE_UNDEFINED))
#define JSVAL_TAG_STRING             ((uint32)(JSVAL_TAG_CLEAR | JSVAL_TYPE_STRING))
#define JSVAL_TAG_BOOLEAN            ((uint32)(JSVAL_TAG_CLEAR | JSVAL_TYPE_BOOLEAN))
#define JSVAL_TAG_MAGIC              ((uint32)(JSVAL_TAG_CLEAR | JSVAL_TYPE_MAGIC))
#define JSVAL_TAG_NULL               ((uint32)(JSVAL_TAG_CLEAR | JSVAL_TYPE_NULL))
#define JSVAL_TAG_OBJECT             ((uint32)(JSVAL_TAG_CLEAR | JSVAL_TYPE_OBJECT))

#elif JS_BITS_PER_WORD == 64

typedef uint32 JSValueTag;
#define JSVAL_TAG_MAX_DOUBLE         ((uint32)(0x1FFF0))
#define JSVAL_TAG_INT32              (uint32)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_INT32)
#define JSVAL_TAG_UNDEFINED          (uint32)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_UNDEFINED)
#define JSVAL_TAG_STRING             (uint32)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_STRING)
#define JSVAL_TAG_BOOLEAN            (uint32)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_BOOLEAN)
#define JSVAL_TAG_MAGIC              (uint32)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_MAGIC)
#define JSVAL_TAG_NULL               (uint32)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_NULL)
#define JSVAL_TAG_OBJECT             (uint32)(JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_OBJECT)

typedef uint64 JSValueShiftedTag;
#define JSVAL_SHIFTED_TAG_MAX_DOUBLE ((((uint64)JSVAL_TAG_MAX_DOUBLE) << JSVAL_TAG_SHIFT) | 0xFFFFFFFF)
#define JSVAL_SHIFTED_TAG_INT32      (((uint64)JSVAL_TAG_INT32)      << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_UNDEFINED  (((uint64)JSVAL_TAG_UNDEFINED)  << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_STRING     (((uint64)JSVAL_TAG_STRING)     << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_BOOLEAN    (((uint64)JSVAL_TAG_BOOLEAN)    << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_MAGIC      (((uint64)JSVAL_TAG_MAGIC)      << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_NULL       (((uint64)JSVAL_TAG_NULL)       << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_OBJECT     (((uint64)JSVAL_TAG_OBJECT)     << JSVAL_TAG_SHIFT)

#endif  /* JS_BITS_PER_WORD */
#endif  /* defined(__cplusplus) && !defined(__SUNPRO_CC) */

#define JSVAL_LOWER_INCL_TYPE_OF_OBJ_OR_NULL_SET        JSVAL_TYPE_NULL
#define JSVAL_UPPER_EXCL_TYPE_OF_PRIMITIVE_SET          JSVAL_TYPE_OBJECT
#define JSVAL_UPPER_INCL_TYPE_OF_NUMBER_SET             JSVAL_TYPE_INT32
#define JSVAL_LOWER_INCL_TYPE_OF_PTR_PAYLOAD_SET        JSVAL_TYPE_MAGIC
#define JSVAL_UPPER_INCL_TYPE_OF_VALUE_SET              JSVAL_TYPE_OBJECT
#define JSVAL_UPPER_INCL_TYPE_OF_BOXABLE_SET            JSVAL_TYPE_FUNOBJ

#if JS_BITS_PER_WORD == 32

#define JSVAL_TYPE_TO_TAG(type)      ((JSValueTag)(JSVAL_TAG_CLEAR | (type)))

#define JSVAL_LOWER_INCL_TAG_OF_OBJ_OR_NULL_SET         JSVAL_TAG_NULL
#define JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET           JSVAL_TAG_OBJECT
#define JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET              JSVAL_TAG_INT32
#define JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET             JSVAL_TAG_STRING

#elif JS_BITS_PER_WORD == 64

#define JSVAL_PAYLOAD_MASK           0x00007FFFFFFFFFFFLL
#define JSVAL_TAG_MASK               0xFFFF800000000000LL
#define JSVAL_TYPE_TO_TAG(type)      ((JSValueTag)(JSVAL_TAG_MAX_DOUBLE | (type)))
#define JSVAL_TYPE_TO_SHIFTED_TAG(type) (((uint64)JSVAL_TYPE_TO_TAG(type)) << JSVAL_TAG_SHIFT)

#define JSVAL_LOWER_INCL_SHIFTED_TAG_OF_OBJ_OR_NULL_SET  JSVAL_SHIFTED_TAG_NULL
#define JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_PRIMITIVE_SET    JSVAL_SHIFTED_TAG_OBJECT
#define JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_NUMBER_SET       JSVAL_SHIFTED_TAG_UNDEFINED
#define JSVAL_LOWER_INCL_SHIFTED_TAG_OF_PTR_PAYLOAD_SET  JSVAL_SHIFTED_TAG_MAGIC
#define JSVAL_LOWER_INCL_SHIFTED_TAG_OF_GCTHING_SET      JSVAL_SHIFTED_TAG_STRING

#endif /* JS_BITS_PER_WORD */

typedef enum JSWhyMagic
{
    JS_ARRAY_HOLE,               /* a hole in a dense array */
    JS_ARGS_HOLE,                /* a hole in the args object's array */
    JS_NATIVE_ENUMERATE,         /* indicates that a custom enumerate hook forwarded
                                  * to JS_EnumerateState, which really means the object can be
                                  * enumerated like a native object. */
    JS_NO_ITER_VALUE,            /* there is not a pending iterator value */
    JS_GENERATOR_CLOSING,        /* exception value thrown when closing a generator */
    JS_NO_CONSTANT,              /* compiler sentinel value */
    JS_THIS_POISON,              /* used in debug builds to catch tracing errors */
    JS_ARG_POISON,               /* used in debug builds to catch tracing errors */
    JS_SERIALIZE_NO_NODE,        /* an empty subnode in the AST serializer */
    JS_LAZY_ARGUMENTS,           /* lazy arguments value on the stack */
    JS_GENERIC_MAGIC             /* for local use */
} JSWhyMagic;

#if defined(IS_LITTLE_ENDIAN)
# if JS_BITS_PER_WORD == 32
typedef union jsval_layout
{
    uint64 asBits;
    struct {
        union {
            int32          i32;
            uint32         u32;
            JSBool         boo;
            JSString       *str;
            JSObject       *obj;
            void           *ptr;
            JSWhyMagic     why;
            size_t         word;
        } payload;
        JSValueTag tag;
    } s;
    double asDouble;
    void *asPtr;
} jsval_layout;
# elif JS_BITS_PER_WORD == 64
typedef union jsval_layout
{
    uint64 asBits;
#if (!defined(_WIN64) && defined(__cplusplus))
    /* MSVC does not pack these correctly :-( */
    struct {
        uint64             payload47 : 47;
        JSValueTag         tag : 17;
    } debugView;
#endif
    struct {
        union {
            int32          i32;
            uint32         u32;
            JSWhyMagic     why;
        } payload;
    } s;
    double asDouble;
    void *asPtr;
    size_t asWord;
} jsval_layout;
# endif  /* JS_BITS_PER_WORD */
#else   /* defined(IS_LITTLE_ENDIAN) */
# if JS_BITS_PER_WORD == 32
typedef union jsval_layout
{
    uint64 asBits;
    struct {
        JSValueTag tag;
        union {
            int32          i32;
            uint32         u32;
            JSBool         boo;
            JSString       *str;
            JSObject       *obj;
            void           *ptr;
            JSWhyMagic     why;
            size_t         word;
        } payload;
    } s;
    double asDouble;
    void *asPtr;
} jsval_layout;
# elif JS_BITS_PER_WORD == 64
typedef union jsval_layout
{
    uint64 asBits;
    struct {
        JSValueTag         tag : 17;
        uint64             payload47 : 47;
    } debugView;
    struct {
        uint32             padding;
        union {
            int32          i32;
            uint32         u32;
            JSWhyMagic     why;
        } payload;
    } s;
    double asDouble;
    void *asPtr;
    size_t asWord;
} jsval_layout;
# endif /* JS_BITS_PER_WORD */
#endif  /* defined(IS_LITTLE_ENDIAN) */

JS_STATIC_ASSERT(sizeof(jsval_layout) == 8);

#if JS_BITS_PER_WORD == 32

/*
 * N.B. GCC, in some but not all cases, chooses to emit signed comparison of
 * JSValueTag even though its underlying type has been forced to be uint32.
 * Thus, all comparisons should explicitly cast operands to uint32.
 */

static JS_ALWAYS_INLINE jsval_layout
BUILD_JSVAL(JSValueTag tag, uint32 payload)
{
    jsval_layout l;
    l.asBits = (((uint64)(uint32)tag) << 32) | payload;
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_DOUBLE_IMPL(jsval_layout l)
{
    return (uint32)l.s.tag <= (uint32)JSVAL_TAG_CLEAR;
}

static JS_ALWAYS_INLINE jsval_layout
DOUBLE_TO_JSVAL_IMPL(double d)
{
    jsval_layout l;
    l.asDouble = d;
    JS_ASSERT(JSVAL_IS_DOUBLE_IMPL(l));
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_INT32_IMPL(jsval_layout l)
{
    return l.s.tag == JSVAL_TAG_INT32;
}

static JS_ALWAYS_INLINE int32
JSVAL_TO_INT32_IMPL(jsval_layout l)
{
    return l.s.payload.i32;
}

static JS_ALWAYS_INLINE jsval_layout
INT32_TO_JSVAL_IMPL(int32 i)
{
    jsval_layout l;
    l.s.tag = JSVAL_TAG_INT32;
    l.s.payload.i32 = i;
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_NUMBER_IMPL(jsval_layout l)
{
    JSValueTag tag = l.s.tag;
    JS_ASSERT(tag != JSVAL_TAG_CLEAR);
    return (uint32)tag <= (uint32)JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_UNDEFINED_IMPL(jsval_layout l)
{
    return l.s.tag == JSVAL_TAG_UNDEFINED;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_STRING_IMPL(jsval_layout l)
{
    return l.s.tag == JSVAL_TAG_STRING;
}

static JS_ALWAYS_INLINE jsval_layout
STRING_TO_JSVAL_IMPL(JSString *str)
{
    jsval_layout l;
    JS_ASSERT(str);
    l.s.tag = JSVAL_TAG_STRING;
    l.s.payload.str = str;
    return l;
}

static JS_ALWAYS_INLINE JSString *
JSVAL_TO_STRING_IMPL(jsval_layout l)
{
    return l.s.payload.str;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_BOOLEAN_IMPL(jsval_layout l)
{
    return l.s.tag == JSVAL_TAG_BOOLEAN;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_TO_BOOLEAN_IMPL(jsval_layout l)
{
    return l.s.payload.boo;
}

static JS_ALWAYS_INLINE jsval_layout
BOOLEAN_TO_JSVAL_IMPL(JSBool b)
{
    jsval_layout l;
    JS_ASSERT(b == JS_TRUE || b == JS_FALSE);
    l.s.tag = JSVAL_TAG_BOOLEAN;
    l.s.payload.boo = b;
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_MAGIC_IMPL(jsval_layout l)
{
    return l.s.tag == JSVAL_TAG_MAGIC;
}

static JS_ALWAYS_INLINE JSObject *
MAGIC_JSVAL_TO_OBJECT_OR_NULL_IMPL(jsval_layout l)
{
    JS_ASSERT(JSVAL_IS_MAGIC_IMPL(l));
    return l.s.payload.obj;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_OBJECT_IMPL(jsval_layout l)
{
    return l.s.tag == JSVAL_TAG_OBJECT;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_PRIMITIVE_IMPL(jsval_layout l)
{
    return (uint32)l.s.tag < (uint32)JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_OBJECT_OR_NULL_IMPL(jsval_layout l)
{
    JS_ASSERT((uint32)l.s.tag <= (uint32)JSVAL_TAG_OBJECT);
    return (uint32)l.s.tag >= (uint32)JSVAL_LOWER_INCL_TAG_OF_OBJ_OR_NULL_SET;
}

static JS_ALWAYS_INLINE JSObject *
JSVAL_TO_OBJECT_IMPL(jsval_layout l)
{
    return l.s.payload.obj;
}

static JS_ALWAYS_INLINE jsval_layout
OBJECT_TO_JSVAL_IMPL(JSObject *obj)
{
    jsval_layout l;
    JS_ASSERT(obj);
    l.s.tag = JSVAL_TAG_OBJECT;
    l.s.payload.obj = obj;
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_NULL_IMPL(jsval_layout l)
{
    return l.s.tag == JSVAL_TAG_NULL;
}

static JS_ALWAYS_INLINE jsval_layout
PRIVATE_PTR_TO_JSVAL_IMPL(void *ptr)
{
    jsval_layout l;
    JS_ASSERT(((uint32)ptr & 1) == 0);
    l.s.tag = (JSValueTag)0;
    l.s.payload.ptr = ptr;
    JS_ASSERT(JSVAL_IS_DOUBLE_IMPL(l));
    return l;
}

static JS_ALWAYS_INLINE void *
JSVAL_TO_PRIVATE_PTR_IMPL(jsval_layout l)
{
    return l.s.payload.ptr;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_GCTHING_IMPL(jsval_layout l)
{
    /* gcc sometimes generates signed < without explicit casts. */
    return (uint32)l.s.tag >= (uint32)JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET;
}

static JS_ALWAYS_INLINE void *
JSVAL_TO_GCTHING_IMPL(jsval_layout l)
{
    return l.s.payload.ptr;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_TRACEABLE_IMPL(jsval_layout l)
{
    return l.s.tag == JSVAL_TAG_STRING || l.s.tag == JSVAL_TAG_OBJECT;
}

static JS_ALWAYS_INLINE uint32
JSVAL_TRACE_KIND_IMPL(jsval_layout l)
{
    return (uint32)(JSBool)JSVAL_IS_STRING_IMPL(l);
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_SPECIFIC_INT32_IMPL(jsval_layout l, int32 i32)
{
    return l.s.tag == JSVAL_TAG_INT32 && l.s.payload.i32 == i32;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_SPECIFIC_BOOLEAN(jsval_layout l, JSBool b)
{
    return (l.s.tag == JSVAL_TAG_BOOLEAN) && (l.s.payload.boo == b);
}

static JS_ALWAYS_INLINE jsval_layout
MAGIC_TO_JSVAL_IMPL(JSWhyMagic why)
{
    jsval_layout l;
    l.s.tag = JSVAL_TAG_MAGIC;
    l.s.payload.why = why;
    return l;
}

static JS_ALWAYS_INLINE jsval_layout
MAGIC_OBJ_TO_JSVAL_IMPL(JSObject *obj)
{
    jsval_layout l;
    l.s.tag = JSVAL_TAG_MAGIC;
    l.s.payload.obj = obj;
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_SAME_TYPE_IMPL(jsval_layout lhs, jsval_layout rhs)
{
    JSValueTag ltag = lhs.s.tag, rtag = rhs.s.tag;
    return ltag == rtag || (ltag < JSVAL_TAG_CLEAR && rtag < JSVAL_TAG_CLEAR);
}

static JS_ALWAYS_INLINE jsval_layout
PRIVATE_UINT32_TO_JSVAL_IMPL(uint32 ui)
{
    jsval_layout l;
    l.s.tag = (JSValueTag)0;
    l.s.payload.u32 = ui;
    JS_ASSERT(JSVAL_IS_DOUBLE_IMPL(l));
    return l;
}

static JS_ALWAYS_INLINE uint32
JSVAL_TO_PRIVATE_UINT32_IMPL(jsval_layout l)
{
    return l.s.payload.u32;
}

static JS_ALWAYS_INLINE JSValueType
JSVAL_EXTRACT_NON_DOUBLE_TYPE_IMPL(jsval_layout l)
{
    uint32 type = l.s.tag & 0xF;
    JS_ASSERT(type > JSVAL_TYPE_DOUBLE);
    return (JSValueType)type;
}

static JS_ALWAYS_INLINE JSValueTag
JSVAL_EXTRACT_NON_DOUBLE_TAG_IMPL(jsval_layout l)
{
    JSValueTag tag = l.s.tag;
    JS_ASSERT(tag >= JSVAL_TAG_INT32);
    return tag;
}

#ifdef __cplusplus
JS_STATIC_ASSERT((JSVAL_TYPE_NONFUNOBJ & 0xF) == JSVAL_TYPE_OBJECT);
JS_STATIC_ASSERT((JSVAL_TYPE_FUNOBJ & 0xF) == JSVAL_TYPE_OBJECT);
#endif

static JS_ALWAYS_INLINE jsval_layout
BOX_NON_DOUBLE_JSVAL(JSValueType type, uint64 *slot)
{
    jsval_layout l;
    JS_ASSERT(type > JSVAL_TYPE_DOUBLE && type <= JSVAL_UPPER_INCL_TYPE_OF_BOXABLE_SET);
    JS_ASSERT_IF(type == JSVAL_TYPE_STRING ||
                 type == JSVAL_TYPE_OBJECT ||
                 type == JSVAL_TYPE_NONFUNOBJ ||
                 type == JSVAL_TYPE_FUNOBJ,
                 *(uint32 *)slot != 0);
    l.s.tag = JSVAL_TYPE_TO_TAG(type & 0xF);
    /* A 32-bit value in a 64-bit slot always occupies the low-addressed end. */
    l.s.payload.u32 = *(uint32 *)slot;
    return l;
}

static JS_ALWAYS_INLINE void
UNBOX_NON_DOUBLE_JSVAL(jsval_layout l, uint64 *out)
{
    JS_ASSERT(!JSVAL_IS_DOUBLE_IMPL(l));
    *(uint32 *)out = l.s.payload.u32;
}

#elif JS_BITS_PER_WORD == 64

static JS_ALWAYS_INLINE jsval_layout
BUILD_JSVAL(JSValueTag tag, uint64 payload)
{
    jsval_layout l;
    l.asBits = (((uint64)(uint32)tag) << JSVAL_TAG_SHIFT) | payload;
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_DOUBLE_IMPL(jsval_layout l)
{
    return l.asBits <= JSVAL_SHIFTED_TAG_MAX_DOUBLE;
}

static JS_ALWAYS_INLINE jsval_layout
DOUBLE_TO_JSVAL_IMPL(double d)
{
    jsval_layout l;
    l.asDouble = d;
    JS_ASSERT(l.asBits <= JSVAL_SHIFTED_TAG_MAX_DOUBLE);
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_INT32_IMPL(jsval_layout l)
{
    return (uint32)(l.asBits >> JSVAL_TAG_SHIFT) == JSVAL_TAG_INT32;
}

static JS_ALWAYS_INLINE int32
JSVAL_TO_INT32_IMPL(jsval_layout l)
{
    return (int32)l.asBits;
}

static JS_ALWAYS_INLINE jsval_layout
INT32_TO_JSVAL_IMPL(int32 i32)
{
    jsval_layout l;
    l.asBits = ((uint64)(uint32)i32) | JSVAL_SHIFTED_TAG_INT32;
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_NUMBER_IMPL(jsval_layout l)
{
    return l.asBits < JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_NUMBER_SET;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_UNDEFINED_IMPL(jsval_layout l)
{
    return l.asBits == JSVAL_SHIFTED_TAG_UNDEFINED;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_STRING_IMPL(jsval_layout l)
{
    return (uint32)(l.asBits >> JSVAL_TAG_SHIFT) == JSVAL_TAG_STRING;
}

static JS_ALWAYS_INLINE jsval_layout
STRING_TO_JSVAL_IMPL(JSString *str)
{
    jsval_layout l;
    uint64 strBits = (uint64)str;
    JS_ASSERT(str);
    JS_ASSERT((strBits >> JSVAL_TAG_SHIFT) == 0);
    l.asBits = strBits | JSVAL_SHIFTED_TAG_STRING;
    return l;
}

static JS_ALWAYS_INLINE JSString *
JSVAL_TO_STRING_IMPL(jsval_layout l)
{
    return (JSString *)(l.asBits & JSVAL_PAYLOAD_MASK);
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_BOOLEAN_IMPL(jsval_layout l)
{
    return (uint32)(l.asBits >> JSVAL_TAG_SHIFT) == JSVAL_TAG_BOOLEAN;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_TO_BOOLEAN_IMPL(jsval_layout l)
{
    return (JSBool)l.asBits;
}

static JS_ALWAYS_INLINE jsval_layout
BOOLEAN_TO_JSVAL_IMPL(JSBool b)
{
    jsval_layout l;
    JS_ASSERT(b == JS_TRUE || b == JS_FALSE);
    l.asBits = ((uint64)(uint32)b) | JSVAL_SHIFTED_TAG_BOOLEAN;
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_MAGIC_IMPL(jsval_layout l)
{
    return (l.asBits >> JSVAL_TAG_SHIFT) == JSVAL_TAG_MAGIC;
}

static JS_ALWAYS_INLINE JSObject *
MAGIC_JSVAL_TO_OBJECT_OR_NULL_IMPL(jsval_layout l)
{
    uint64 ptrBits = l.asBits & JSVAL_PAYLOAD_MASK;
    JS_ASSERT(JSVAL_IS_MAGIC_IMPL(l));
    JS_ASSERT((ptrBits >> JSVAL_TAG_SHIFT) == 0);
    return (JSObject *)ptrBits;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_PRIMITIVE_IMPL(jsval_layout l)
{
    return l.asBits < JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_PRIMITIVE_SET;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_OBJECT_IMPL(jsval_layout l)
{
    JS_ASSERT((l.asBits >> JSVAL_TAG_SHIFT) <= JSVAL_SHIFTED_TAG_OBJECT);
    return l.asBits >= JSVAL_SHIFTED_TAG_OBJECT;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_OBJECT_OR_NULL_IMPL(jsval_layout l)
{
    JS_ASSERT((l.asBits >> JSVAL_TAG_SHIFT) <= JSVAL_TAG_OBJECT);
    return l.asBits >= JSVAL_LOWER_INCL_SHIFTED_TAG_OF_OBJ_OR_NULL_SET;
}

static JS_ALWAYS_INLINE JSObject *
JSVAL_TO_OBJECT_IMPL(jsval_layout l)
{
    uint64 ptrBits = l.asBits & JSVAL_PAYLOAD_MASK;
    JS_ASSERT((ptrBits & 0x7) == 0);
    return (JSObject *)ptrBits;
}

static JS_ALWAYS_INLINE jsval_layout
OBJECT_TO_JSVAL_IMPL(JSObject *obj)
{
    jsval_layout l;
    uint64 objBits = (uint64)obj;
    JS_ASSERT(obj);
    JS_ASSERT((objBits >> JSVAL_TAG_SHIFT) == 0);
    l.asBits = objBits | JSVAL_SHIFTED_TAG_OBJECT;
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_NULL_IMPL(jsval_layout l)
{
    return l.asBits == JSVAL_SHIFTED_TAG_NULL;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_GCTHING_IMPL(jsval_layout l)
{
    return l.asBits >= JSVAL_LOWER_INCL_SHIFTED_TAG_OF_GCTHING_SET;
}

static JS_ALWAYS_INLINE void *
JSVAL_TO_GCTHING_IMPL(jsval_layout l)
{
    uint64 ptrBits = l.asBits & JSVAL_PAYLOAD_MASK;
    JS_ASSERT((ptrBits & 0x7) == 0);
    return (void *)ptrBits;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_TRACEABLE_IMPL(jsval_layout l)
{
    return JSVAL_IS_GCTHING_IMPL(l) && !JSVAL_IS_NULL_IMPL(l);
}

static JS_ALWAYS_INLINE uint32
JSVAL_TRACE_KIND_IMPL(jsval_layout l)
{
    return (uint32)(JSBool)!(JSVAL_IS_OBJECT_IMPL(l));
}

static JS_ALWAYS_INLINE jsval_layout
PRIVATE_PTR_TO_JSVAL_IMPL(void *ptr)
{
    jsval_layout l;
    uint64 ptrBits = (uint64)ptr;
    JS_ASSERT((ptrBits & 1) == 0);
    l.asBits = ptrBits >> 1;
    JS_ASSERT(JSVAL_IS_DOUBLE_IMPL(l));
    return l;
}

static JS_ALWAYS_INLINE void *
JSVAL_TO_PRIVATE_PTR_IMPL(jsval_layout l)
{
    JS_ASSERT((l.asBits & 0x8000000000000000LL) == 0);
    return (void *)(l.asBits << 1);
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_SPECIFIC_INT32_IMPL(jsval_layout l, int32 i32)
{
    return l.asBits == (((uint64)(uint32)i32) | JSVAL_SHIFTED_TAG_INT32);
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_SPECIFIC_BOOLEAN(jsval_layout l, JSBool b)
{
    return l.asBits == (((uint64)(uint32)b) | JSVAL_SHIFTED_TAG_BOOLEAN);
}

static JS_ALWAYS_INLINE jsval_layout
MAGIC_TO_JSVAL_IMPL(JSWhyMagic why)
{
    jsval_layout l;
    l.asBits = ((uint64)(uint32)why) | JSVAL_SHIFTED_TAG_MAGIC;
    return l;
}

static JS_ALWAYS_INLINE jsval_layout
MAGIC_OBJ_TO_JSVAL_IMPL(JSObject *obj)
{
    jsval_layout l;
    l.asBits = ((uint64)obj) | JSVAL_SHIFTED_TAG_MAGIC;
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_SAME_TYPE_IMPL(jsval_layout lhs, jsval_layout rhs)
{
    uint64 lbits = lhs.asBits, rbits = rhs.asBits;
    return (lbits <= JSVAL_SHIFTED_TAG_MAX_DOUBLE && rbits <= JSVAL_SHIFTED_TAG_MAX_DOUBLE) ||
           (((lbits ^ rbits) & 0xFFFF800000000000LL) == 0);
}

static JS_ALWAYS_INLINE jsval_layout
PRIVATE_UINT32_TO_JSVAL_IMPL(uint32 ui)
{
    jsval_layout l;
    l.asBits = (uint64)ui;
    JS_ASSERT(JSVAL_IS_DOUBLE_IMPL(l));
    return l;
}

static JS_ALWAYS_INLINE uint32
JSVAL_TO_PRIVATE_UINT32_IMPL(jsval_layout l)
{
    JS_ASSERT((l.asBits >> 32) == 0);
    return (uint32)l.asBits;
}

static JS_ALWAYS_INLINE JSValueType
JSVAL_EXTRACT_NON_DOUBLE_TYPE_IMPL(jsval_layout l)
{
   uint64 type = (l.asBits >> JSVAL_TAG_SHIFT) & 0xF;
   JS_ASSERT(type > JSVAL_TYPE_DOUBLE);
   return (JSValueType)type;
}

static JS_ALWAYS_INLINE JSValueTag
JSVAL_EXTRACT_NON_DOUBLE_TAG_IMPL(jsval_layout l)
{
    uint64 tag = l.asBits >> JSVAL_TAG_SHIFT;
    JS_ASSERT(tag > JSVAL_TAG_MAX_DOUBLE);
    return (JSValueTag)tag;
}

#ifdef __cplusplus
JS_STATIC_ASSERT((JSVAL_TYPE_NONFUNOBJ & 0xF) == JSVAL_TYPE_OBJECT);
JS_STATIC_ASSERT((JSVAL_TYPE_FUNOBJ & 0xF) == JSVAL_TYPE_OBJECT);
#endif

static JS_ALWAYS_INLINE jsval_layout
BOX_NON_DOUBLE_JSVAL(JSValueType type, uint64 *slot)
{
    uint32 isI32 = (uint32)(type < JSVAL_LOWER_INCL_TYPE_OF_PTR_PAYLOAD_SET);
    uint32 shift = isI32 * 32;
    uint64 mask = ((uint64)-1) >> shift;
    uint64 payload = *slot & mask;
    jsval_layout l;

    /* N.B. for 32-bit payloads, the high 32 bits of the slot are trash. */
    JS_ASSERT(type > JSVAL_TYPE_DOUBLE && type <= JSVAL_UPPER_INCL_TYPE_OF_BOXABLE_SET);
    JS_ASSERT_IF(type == JSVAL_TYPE_STRING ||
                 type == JSVAL_TYPE_OBJECT ||
                 type == JSVAL_TYPE_NONFUNOBJ ||
                 type == JSVAL_TYPE_FUNOBJ,
                 payload != 0);

    l.asBits = payload | JSVAL_TYPE_TO_SHIFTED_TAG(type & 0xF);
    return l;
}

static JS_ALWAYS_INLINE void
UNBOX_NON_DOUBLE_JSVAL(jsval_layout l, uint64 *out)
{
    JS_ASSERT(!JSVAL_IS_DOUBLE_IMPL(l));
    *out = (l.asBits & JSVAL_PAYLOAD_MASK);
}

#endif  /* JS_BITS_PER_WORD */

static JS_ALWAYS_INLINE double
JS_CANONICALIZE_NAN(double d)
{
    if (JS_UNLIKELY(d != d)) {
        jsval_layout l;
        l.asBits = 0x7FF8000000000000LL;
        return l.asDouble;
    }
    return d;
}

JS_END_EXTERN_C

#ifdef __cplusplus
static jsval_layout JSVAL_TO_IMPL(JS::Value);
static JS::Value IMPL_TO_JSVAL(jsval_layout);
#endif

#endif /* jsvalimpl_h__ */

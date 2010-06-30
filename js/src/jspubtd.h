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

#ifndef jspubtd_h___
#define jspubtd_h___
/*
 * JS public API typedefs.
 */
#include "jscompat.h"
#include "jsutil.h"

JS_BEGIN_EXTERN_C

/* Scalar typedefs. */
typedef uint16    jschar;
typedef int32     jsint;
typedef uint32    jsuint;
typedef float64   jsdouble;
typedef int32     jsrefcount;   /* PRInt32 if JS_THREADSAFE, see jslock.h */

/*
 * Run-time version enumeration.  See jsversion.h for compile-time counterparts
 * to these values that may be selected by the JS_VERSION macro, and tested by
 * #if expressions.
 */
typedef enum JSVersion {
    JSVERSION_1_0     = 100,
    JSVERSION_1_1     = 110,
    JSVERSION_1_2     = 120,
    JSVERSION_1_3     = 130,
    JSVERSION_1_4     = 140,
    JSVERSION_ECMA_3  = 148,
    JSVERSION_1_5     = 150,
    JSVERSION_1_6     = 160,
    JSVERSION_1_7     = 170,
    JSVERSION_1_8     = 180,
    JSVERSION_ECMA_5  = 185,
    JSVERSION_DEFAULT = 0,
    JSVERSION_UNKNOWN = -1,
    JSVERSION_LATEST  = JSVERSION_ECMA_5
} JSVersion;

#define JSVERSION_IS_ECMA(version) \
    ((version) == JSVERSION_DEFAULT || (version) >= JSVERSION_1_3)

/* Result of typeof operator enumeration. */
typedef enum JSType {
    JSTYPE_VOID,                /* undefined */
    JSTYPE_OBJECT,              /* object */
    JSTYPE_FUNCTION,            /* function */
    JSTYPE_STRING,              /* string */
    JSTYPE_NUMBER,              /* number */
    JSTYPE_BOOLEAN,             /* boolean */
    JSTYPE_NULL,                /* null */
    JSTYPE_XML,                 /* xml object */
    JSTYPE_LIMIT
} JSType;

/* Dense index into cached prototypes and class atoms for standard objects. */
typedef enum JSProtoKey {
#define JS_PROTO(name,code,init) JSProto_##name = code,
#include "jsproto.tbl"
#undef JS_PROTO
    JSProto_LIMIT
} JSProtoKey;

/* JSObjectOps.checkAccess mode enumeration. */
typedef enum JSAccessMode {
    JSACC_PROTO  = 0,           /* XXXbe redundant w.r.t. id */
    JSACC_PARENT = 1,           /* XXXbe redundant w.r.t. id */

                                /*
                                 * enum value #2 formerly called JSACC_IMPORT,
                                 * gap preserved for ABI compatibility.
                                 */

    JSACC_WATCH  = 3,           /* a watchpoint on object foo for id 'bar' */
    JSACC_READ   = 4,           /* a "get" of foo.bar */
    JSACC_WRITE  = 8,           /* a "set" of foo.bar = baz */
    JSACC_LIMIT
} JSAccessMode;

#define JSACC_TYPEMASK          (JSACC_WRITE - 1)

/*
 * This enum type is used to control the behavior of a JSObject property
 * iterator function that has type JSNewEnumerate.
 */
typedef enum JSIterateOp {
    JSENUMERATE_INIT,       /* Create new iterator state */
    JSENUMERATE_NEXT,       /* Iterate once */
    JSENUMERATE_DESTROY     /* Destroy iterator state */
} JSIterateOp;

/* Struct typedefs. */
typedef struct JSClass           JSClass;
typedef struct JSExtendedClass   JSExtendedClass;
typedef struct JSConstDoubleSpec JSConstDoubleSpec;
typedef struct JSContext         JSContext;
typedef struct JSErrorReport     JSErrorReport;
typedef struct JSFunction        JSFunction;
typedef struct JSFunctionSpec    JSFunctionSpec;
typedef struct JSTracer          JSTracer;
typedef struct JSIdArray         JSIdArray;
typedef struct JSPropertyDescriptor JSPropertyDescriptor;
typedef struct JSPropertySpec    JSPropertySpec;
typedef struct JSObject          JSObject;
typedef struct JSObjectMap       JSObjectMap;
typedef struct JSObjectOps       JSObjectOps;
typedef struct JSRuntime         JSRuntime;
typedef struct JSScript          JSScript;
typedef struct JSStackFrame      JSStackFrame;
typedef struct JSString          JSString;
typedef struct JSXDRState        JSXDRState;
typedef struct JSExceptionState  JSExceptionState;
typedef struct JSLocaleCallbacks JSLocaleCallbacks;
typedef struct JSSecurityCallbacks JSSecurityCallbacks;
typedef struct JSONParser        JSONParser;
typedef struct JSCompartment     JSCompartment;

/*
 * JavaScript engine unboxed value representation
 *
 * TODO: explain boxing strategy
 */

#ifdef __GNUC__
# define VALUE_ALIGNMENT        __attribute__((aligned (8)))
# define ASSERT_DOUBLE_ALIGN()  JS_ASSERT(size_t(this) % sizeof(double) == 0)
#elif defined(_MSC_VER)
  /*
   * Structs can be aligned with MSVC, but not if they are used as parameters,
   * so we just don't try to align.
   */
# define VALUE_ALIGNMENT
# define ASSERT_DOUBLE_ALIGN()
#endif

/*
 * When we can ensure that the underlying type is uint32, use an enum so that
 * symbolic type names show up in the debugger. Otherwise, use #defines.
 */
#ifdef __cplusplus

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

    /* The below types never appear in a jsval; they are only used in tracing. */

    JSVAL_TYPE_NONFUNOBJ           = 0x57,
    JSVAL_TYPE_FUNOBJ              = 0x67,

    JSVAL_TYPE_STRORNULL           = 0x97,
    JSVAL_TYPE_OBJORNULL           = 0x98,
    JSVAL_TYPE_BOXED               = 0x99,
    JSVAL_TYPE_UNINITIALIZED       = 0xcd
} JS_ENUM_FOOTER(JSValueType);

#if JS_BITS_PER_WORD == 32

/* Remember to propagate changes to the C defines below. */
JS_ENUM_HEADER(JSValueTag, uint32)
{
    JSVAL_TAG_CLEAR                = 0xFFFF0000,
    JSVAL_TAG_INT32                = JSVAL_TAG_CLEAR | JSVAL_TYPE_INT32,
    JSVAL_TAG_UNDEFINED            = JSVAL_TAG_CLEAR | JSVAL_TYPE_UNDEFINED,
    JSVAL_TAG_STRING               = JSVAL_TAG_CLEAR | JSVAL_TYPE_STRING,
    JSVAL_TAG_BOOLEAN              = JSVAL_TAG_CLEAR | JSVAL_TYPE_BOOLEAN,
    JSVAL_TAG_MAGIC                = JSVAL_TAG_CLEAR | JSVAL_TYPE_MAGIC,
    JSVAL_TAG_NULL                 = JSVAL_TAG_CLEAR | JSVAL_TYPE_NULL,
    JSVAL_TAG_OBJECT               = JSVAL_TAG_CLEAR | JSVAL_TYPE_OBJECT
} JS_ENUM_FOOTER(JSValueType);

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
} JS_ENUM_FOOTER(JSValueType);

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
#define JSVAL_TYPE_NONFUNOBJ         ((uint8)0x57)
#define JSVAL_TYPE_FUNOBJ            ((uint8)0x67)
#define JSVAL_TYPE_STRORNULL         ((uint8)0x97)
#define JSVAL_TYPE_OBJORNULL         ((uint8)0x98)
#define JSVAL_TYPE_BOXED             ((uint8)0x99)
#define JSVAL_TYPE_UNINITIALIZED     ((uint8)0xcd)

#if JS_BITS_PER_WORD == 32

typedef uint32 JSValueTag;
#define JSVAL_TAG_CLEAR              ((uint32)(0xFFFF0000))
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

#endif  /* JS_BITS_PER_WORD */
#endif  /* defined(__cplusplus) */

#define JSVAL_LOWER_INCL_TYPE_OF_OBJ_OR_NULL_SET        JSVAL_TYPE_NULL
#define JSVAL_UPPER_EXCL_TYPE_OF_PRIMITIVE_SET          JSVAL_TYPE_OBJECT
#define JSVAL_UPPER_INCL_TYPE_OF_NUMBER_SET             JSVAL_TYPE_INT32
#define JSVAL_LOWER_INCL_TYPE_OF_GCTHING_SET            JSVAL_TYPE_STRING
#define JSVAL_UPPER_INCL_TYPE_OF_VALUE_SET              JSVAL_TYPE_OBJECT
#define JSVAL_UPPER_INCL_TYPE_OF_BOXABLE_SET            JSVAL_TYPE_FUNOBJ

#if JS_BITS_PER_WORD == 32

#define JSVAL_TYPE_TO_TAG(type)      ((JSValueTag)(JSVAL_TAG_CLEAR | (type)))

#define JSVAL_LOWER_INCL_TAG_OF_OBJ_OR_NULL_SET         JSVAL_TAG_NULL
#define JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET           JSVAL_TAG_OBJECT
#define JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET              JSVAL_TAG_INT32
#define JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET             JSVAL_TAG_STRING

#elif JS_BITS_PER_WORD == 64

#define JSVAL_TAG_SHIFT              47
#define JSVAL_PAYLOAD_MASK           0x00007FFFFFFFFFFFLL
#define JSVAL_TYPE_TO_TAG(type)      ((JSValueTag)(JSVAL_TAG_MAX_DOUBLE | (type)))
#define JSVAL_TYPE_TO_SHIFTED_TAG(type) (((uint64)JSVAL_TYPE_TO_TAG(type)) << JSVAL_TAG_SHIFT)

#define JSVAL_SHIFTED_TAG_MAX_DOUBLE (((uint64)JSVAL_TAG_MAX_DOUBLE) << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_INT32      (((uint64)JSVAL_TAG_INT32)      << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_UNDEFINED  (((uint64)JSVAL_TAG_UNDEFINED)  << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_STRING     (((uint64)JSVAL_TAG_STRING)     << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_BOOLEAN    (((uint64)JSVAL_TAG_BOOLEAN)    << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_MAGIC      (((uint64)JSVAL_TAG_MAGIC)      << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_NULL       (((uint64)JSVAL_TAG_NULL)       << JSVAL_TAG_SHIFT)
#define JSVAL_SHIFTED_TAG_OBJECT     (((uint64)JSVAL_TAG_OBJECT)     << JSVAL_TAG_SHIFT)

#define JSVAL_LOWER_INCL_SHIFTED_TAG_OF_OBJ_OR_NULL_SET  JSVAL_SHIFTED_TAG_NULL
#define JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_PRIMITIVE_SET    JSVAL_SHIFTED_TAG_OBJECT
#define JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_NUMBER_SET       JSVAL_SHIFTED_TAG_UNDEFINED
#define JSVAL_LOWER_INCL_SHIFTED_TAG_OF_GCTHING_SET      JSVAL_SHIFTED_TAG_STRING

#endif /* JS_BITS_PER_WORD */

typedef enum JSWhyMagic
{
    JS_ARRAY_HOLE,               /* a hole in a dense array */
    JS_ARGS_HOLE,                /* a hole in the args object's array */
    JS_NATIVE_ENUMERATE,         /* indicates that a custom enumerate hook forwarded
                                  * to js_Enumerate, which really means the object can be
                                  * enumerated like a native object. */
    JS_NO_ITER_VALUE,            /* there is not a pending iterator value */
    JS_GENERATOR_CLOSING,        /* exception value thrown when closing a generator */
    JS_NO_CONSTANT               /* compiler sentinel value */
} JSWhyMagic;

#if defined(IS_LITTLE_ENDIAN)
#if JS_BITS_PER_WORD == 32

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
        } payload;
        JSValueTag tag;
    } s;
    double asDouble;
} jsval_layout;

#elif JS_BITS_PER_WORD == 64

typedef union jsval_layout
{
    uint64 asBits;
    struct {
        uint64             payload47 : 47;
        JSValueTag         tag : 17;
    } debugView;
    struct {
        union {
            int32          i32;
            uint32         u32;
            JSWhyMagic     why;
        } payload;
    } s;
    double asDouble;
} jsval_layout;

#endif  /* JS_BITS_PER_WORD */
#endif  /* IS_LITTLE_ENDIAN */

#if JS_BITS_PER_WORD == 32

#define BUILD_JSVAL(tag, payload) \
    ((((uint64)(uint32)(tag)) << 32) | (uint32)(payload))

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_DOUBLE_IMPL(jsval_layout l)
{
    return l.s.tag < JSVAL_TAG_CLEAR;
}

static JS_ALWAYS_INLINE jsval_layout
DOUBLE_TO_JSVAL_IMPL(jsdouble d)
{
    jsval_layout l;
    l.asDouble = d;
    JS_ASSERT(l.s.tag < JSVAL_TAG_CLEAR);
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_INT32_IMPL(jsval_layout l)
{
    return l.s.tag == JSVAL_TAG_INT32;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_SPECIFIC_INT32_IMPL(jsval_layout l, int32 i32)
{
    return l.s.tag == JSVAL_TAG_INT32 && l.s.payload.i32 == i32;
}

static JS_ALWAYS_INLINE jsint
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
    return tag <= JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET;
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
    l.s.tag = JSVAL_TAG_BOOLEAN;
    l.s.payload.boo = b;
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_SPECIFIC_BOOLEAN(jsval_layout l, JSBool b)
{
    return (l.s.tag == JSVAL_TAG_BOOLEAN) && (l.s.payload.boo == b);
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_MAGIC_IMPL(jsval_layout l)
{
    return l.s.tag == JSVAL_TAG_MAGIC;
}

static JS_ALWAYS_INLINE jsval_layout
MAGIC_TO_JSVAL_IMPL(JSWhyMagic why)
{
    jsval_layout l;
    l.s.tag = JSVAL_TAG_MAGIC;
    l.s.payload.why = why;
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_OBJECT_IMPL(jsval_layout l)
{
    return l.s.tag == JSVAL_TAG_OBJECT;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_PRIMITIVE_IMPL(jsval_layout l)
{
    return l.s.tag < JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_OBJECT_OR_NULL_IMPL(jsval_layout l)
{
    JS_ASSERT(l.s.tag <= JSVAL_TAG_OBJECT);
    return l.s.tag >= JSVAL_LOWER_INCL_TAG_OF_OBJ_OR_NULL_SET;
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
    return l.s.tag >= JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET;
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
JSVAL_SAME_TYPE_IMPL(jsval_layout lhs, jsval_layout rhs)
{
    JSValueTag ltag = lhs.s.tag, rtag = rhs.s.tag;
    return ltag == rtag || (ltag < JSVAL_TAG_CLEAR && rtag < JSVAL_TAG_CLEAR);
}

static JS_ALWAYS_INLINE JSValueType
JSVAL_EXTRACT_NON_DOUBLE_OBJECT_TYPE_IMPL(jsval_layout l)
{
    uint32 type = l.s.tag & 0xF;
    JS_ASSERT(type > JSVAL_TYPE_DOUBLE && type < JSVAL_TYPE_OBJECT);
    return (JSValueType)type;
}

static JS_ALWAYS_INLINE JSValueTag
JSVAL_EXTRACT_NON_DOUBLE_OBJECT_TAG_IMPL(jsval_layout l)
{
    JSValueTag tag = l.s.tag;
    JS_ASSERT(tag >= JSVAL_TAG_INT32 && tag != JSVAL_TAG_OBJECT);
    return tag;
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

#ifdef __cplusplus
JS_STATIC_ASSERT((JSVAL_TYPE_NONFUNOBJ & 0xF) == JSVAL_TYPE_OBJECT);
JS_STATIC_ASSERT((JSVAL_TYPE_FUNOBJ & 0xF) == JSVAL_TYPE_OBJECT);
#endif

static JS_ALWAYS_INLINE jsval_layout
BOX_NON_DOUBLE_JSVAL(JSValueType type, uint64 *slot)
{
    jsval_layout l;
    JS_ASSERT(type > JSVAL_TYPE_DOUBLE && type <= JSVAL_UPPER_INCL_TYPE_OF_BOXABLE_SET);
    l.s.tag = JSVAL_TYPE_TO_TAG(type & 0xF);
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

#define BUILD_JSVAL(tag, payload) \
    ((((uint64)(uint32)(tag)) << JSVAL_TAG_SHIFT) | (payload))

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_DOUBLE_IMPL(jsval_layout l)
{
    return l.asBits <= JSVAL_SHIFTED_TAG_MAX_DOUBLE;
}

static JS_ALWAYS_INLINE jsval_layout
DOUBLE_TO_JSVAL_IMPL(jsdouble d)
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

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_SPECIFIC_INT32_IMPL(jsval_layout l, int32 i32)
{
    return l.asBits == (((uint64)(uint32)i32) | JSVAL_SHIFTED_TAG_INT32);
}

static JS_ALWAYS_INLINE jsint
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
    l.asBits = ((uint64)(uint32)b) | JSVAL_SHIFTED_TAG_BOOLEAN;
    return l;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_SPECIFIC_BOOLEAN(jsval_layout l, JSBool b)
{
    return l.asBits == (((uint64)(uint32)b) | JSVAL_SHIFTED_TAG_BOOLEAN);
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_MAGIC_IMPL(jsval_layout l)
{
    return (l.asBits >> JSVAL_TAG_SHIFT) == JSVAL_TAG_MAGIC;
}

static JS_ALWAYS_INLINE jsval_layout
MAGIC_TO_JSVAL_IMPL(JSWhyMagic why)
{
    jsval_layout l;
    l.asBits = ((uint64)(uint32)why) | JSVAL_SHIFTED_TAG_MAGIC;
    return l;
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
JSVAL_SAME_TYPE_IMPL(jsval_layout lhs, jsval_layout rhs)
{
    uint64 lbits = lhs.asBits, rbits = rhs.asBits;
    return (lbits <= JSVAL_TAG_MAX_DOUBLE && rbits <= JSVAL_TAG_MAX_DOUBLE) ||
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
JSVAL_EXTRACT_NON_DOUBLE_OBJECT_TYPE_IMPL(jsval_layout l)
{
   uint64 type = (l.asBits >> JSVAL_TAG_SHIFT) & 0xF;
   JS_ASSERT(type > JSVAL_TYPE_DOUBLE && type < JSVAL_TYPE_OBJECT);
   return (JSValueType)type;
}

static JS_ALWAYS_INLINE JSValueTag
JSVAL_EXTRACT_NON_DOUBLE_OBJECT_TAG_IMPL(jsval_layout l)
{
    uint64 tag = l.asBits >> JSVAL_TAG_SHIFT;
    JS_ASSERT(tag > JSVAL_TAG_MAX_DOUBLE && tag != JSVAL_TAG_OBJECT);
    return (JSValueTag)tag;
}

#ifdef __cplusplus
JS_STATIC_ASSERT(offsetof(jsval_layout, s.payload) == 0);
JS_STATIC_ASSERT((JSVAL_TYPE_NONFUNOBJ & 0xF) == JSVAL_TYPE_OBJECT);
JS_STATIC_ASSERT((JSVAL_TYPE_FUNOBJ & 0xF) == JSVAL_TYPE_OBJECT);
#endif

static JS_ALWAYS_INLINE jsval_layout
BOX_NON_DOUBLE_JSVAL(JSValueType type, uint64 *slot)
{
    /* N.B. for 32-bit payloads, the high 32 bits of the slot are trash. */
    jsval_layout l;
    JS_ASSERT(type > JSVAL_TYPE_DOUBLE && type <= JSVAL_UPPER_INCL_TYPE_OF_BOXABLE_SET);
    uint32 isI32 = (uint32)(type < JSVAL_LOWER_INCL_TYPE_OF_GCTHING_SET);
    uint32 shift = isI32 * 32;
    uint64 mask = ((uint64)-1) >> shift;
    uint64 payload = *slot & mask;
    l.asBits = payload | JSVAL_TYPE_TO_SHIFTED_TAG(type & 0xF);
    return l;
}

static JS_ALWAYS_INLINE void
UNBOX_NON_DOUBLE_JSVAL(jsval_layout l, uint64 *out)
{
    JS_ASSERT(!JSVAL_IS_DOUBLE_IMPL(l));
    *out = (l.asBits & JSVAL_PAYLOAD_MASK);
}

#endif

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_UNDERLYING_TYPE_OF_PRIVATE_IMPL(jsval_layout l)
{
    return JSVAL_IS_DOUBLE_IMPL(l);
}

#ifdef DEBUG

/*
 * To catch subtle bugs that may arise fromt he implicit conversion of jsval
 * and jsid to each other, bool and integral types, debug builds make these
 * types structs.
 */
typedef VALUE_ALIGNMENT jsval_layout jsval;
typedef struct jsid { size_t bits; } jsid;

#define JSVAL_BITS(v)    (v.asBits)
#define IMPL_TO_JSVAL(v) (v)
#define JSID_BITS(id)    (id.bits)

#if defined(__cplusplus)
extern "C++" {
    static JS_ALWAYS_INLINE bool
    operator==(jsid lhs, jsid rhs)
    {
        return JSID_BITS(lhs) == JSID_BITS(rhs);
    }

    static JS_ALWAYS_INLINE bool
    operator!=(jsid lhs, jsid rhs)
    {
        return JSID_BITS(lhs) != JSID_BITS(rhs);
    }

    static JS_ALWAYS_INLINE bool
    operator==(jsval lhs, jsval rhs)
    {
        return JSVAL_BITS(lhs) == JSVAL_BITS(rhs);
    }

    static JS_ALWAYS_INLINE bool
    operator!=(jsval lhs, jsval rhs)
    {
        return JSVAL_BITS(lhs) != JSVAL_BITS(rhs);
    }
}
# endif /* defined(__cplusplus) */

#else   /* defined(DEBUG) */

typedef VALUE_ALIGNMENT uint64 jsval;
typedef size_t  jsid;

#define JSVAL_BITS(v)    (v)
#define IMPL_TO_JSVAL(v) ((v).asBits)
#define JSID_BITS(id)    (id)

#endif  /* defined(DEBUG) */

/* JSClass (and JSObjectOps where appropriate) function pointer typedefs. */

/*
 * Add, delete, get or set a property named by id in obj.  Note the jsid id
 * type -- id may be a string (Unicode property identifier) or an int (element
 * index).  The *vp out parameter, on success, is the new property value after
 * an add, get, or set.  After a successful delete, *vp is JSVAL_FALSE iff
 * obj[id] can't be deleted (because it's permanent).
 */
typedef JSBool
(* JSPropertyOp)(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

/*
 * This function type is used for callbacks that enumerate the properties of
 * a JSObject.  The behavior depends on the value of enum_op:
 *
 *  JSENUMERATE_INIT
 *    A new, opaque iterator state should be allocated and stored in *statep.
 *    (You can use PRIVATE_TO_JSVAL() to tag the pointer to be stored).
 *
 *    The number of properties that will be enumerated should be returned as
 *    an integer jsval in *idp, if idp is non-null, and provided the number of
 *    enumerable properties is known.  If idp is non-null and the number of
 *    enumerable properties can't be computed in advance, *idp should be set
 *    to JSVAL_ZERO.
 *
 *  JSENUMERATE_NEXT
 *    A previously allocated opaque iterator state is passed in via statep.
 *    Return the next jsid in the iteration using *idp.  The opaque iterator
 *    state pointed at by statep is destroyed and *statep is set to JSVAL_NULL
 *    if there are no properties left to enumerate.
 *
 *  JSENUMERATE_DESTROY
 *    Destroy the opaque iterator state previously allocated in *statep by a
 *    call to this function when enum_op was JSENUMERATE_INIT.
 *
 * The return value is used to indicate success, with a value of JS_FALSE
 * indicating failure.
 */
typedef JSBool
(* JSNewEnumerateOp)(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                     jsval *statep, jsid *idp);

/*
 * The old-style JSClass.enumerate op should define all lazy properties not
 * yet reflected in obj.
 */
typedef JSBool
(* JSEnumerateOp)(JSContext *cx, JSObject *obj);

/*
 * Resolve a lazy property named by id in obj by defining it directly in obj.
 * Lazy properties are those reflected from some peer native property space
 * (e.g., the DOM attributes for a given node reflected as obj) on demand.
 *
 * JS looks for a property in an object, and if not found, tries to resolve
 * the given id.  If resolve succeeds, the engine looks again in case resolve
 * defined obj[id].  If no such property exists directly in obj, the process
 * is repeated with obj's prototype, etc.
 *
 * NB: JSNewResolveOp provides a cheaper way to resolve lazy properties.
 */
typedef JSBool
(* JSResolveOp)(JSContext *cx, JSObject *obj, jsid id);

/*
 * Like JSResolveOp, but flags provide contextual information as follows:
 *
 *  JSRESOLVE_QUALIFIED   a qualified property id: obj.id or obj[id], not id
 *  JSRESOLVE_ASSIGNING   obj[id] is on the left-hand side of an assignment
 *  JSRESOLVE_DETECTING   'if (o.p)...' or similar detection opcode sequence
 *  JSRESOLVE_DECLARING   var, const, or function prolog declaration opcode
 *  JSRESOLVE_CLASSNAME   class name used when constructing
 *
 * The *objp out parameter, on success, should be null to indicate that id
 * was not resolved; and non-null, referring to obj or one of its prototypes,
 * if id was resolved.
 *
 * This hook instead of JSResolveOp is called via the JSClass.resolve member
 * if JSCLASS_NEW_RESOLVE is set in JSClass.flags.
 *
 * Setting JSCLASS_NEW_RESOLVE and JSCLASS_NEW_RESOLVE_GETS_START further
 * extends this hook by passing in the starting object on the prototype chain
 * via *objp.  Thus a resolve hook implementation may define the property id
 * being resolved in the object in which the id was first sought, rather than
 * in a prototype object whose class led to the resolve hook being called.
 *
 * When using JSCLASS_NEW_RESOLVE_GETS_START, the resolve hook must therefore
 * null *objp to signify "not resolved".  With only JSCLASS_NEW_RESOLVE and no
 * JSCLASS_NEW_RESOLVE_GETS_START, the hook can assume *objp is null on entry.
 * This is not good practice, but enough existing hook implementations count
 * on it that we can't break compatibility by passing the starting object in
 * *objp without a new JSClass flag.
 */
typedef JSBool
(* JSNewResolveOp)(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                   JSObject **objp);

/*
 * Convert obj to the given type, returning true with the resulting value in
 * *vp on success, and returning false on error or exception.
 */
typedef JSBool
(* JSConvertOp)(JSContext *cx, JSObject *obj, JSType type, jsval *vp);

/*
 * Delegate typeof to an object so it can cloak a primitive or another object.
 */
typedef JSType
(* JSTypeOfOp)(JSContext *cx, JSObject *obj);

/*
 * Finalize obj, which the garbage collector has determined to be unreachable
 * from other live objects or from GC roots.  Obviously, finalizers must never
 * store a reference to obj.
 */
typedef void
(* JSFinalizeOp)(JSContext *cx, JSObject *obj);

/*
 * Used by JS_AddExternalStringFinalizer and JS_RemoveExternalStringFinalizer
 * to extend and reduce the set of string types finalized by the GC.
 */
typedef void
(* JSStringFinalizeOp)(JSContext *cx, JSString *str);

/*
 * The signature for JSClass.getObjectOps, used by JS_NewObject's internals
 * to discover the set of high-level object operations to use for new objects
 * of the given class.  All native objects have a JSClass, which is stored as
 * a private (int-tagged) pointer in obj slots. In contrast, all native and
 * host objects have a JSObjectMap at obj->map, which may be shared among a
 * number of objects, and which contains the JSObjectOps *ops pointer used to
 * dispatch object operations from API calls.
 *
 * Thus JSClass (which pre-dates JSObjectOps in the API) provides a low-level
 * interface to class-specific code and data, while JSObjectOps allows for a
 * higher level of operation, which does not use the object's class except to
 * find the class's JSObjectOps struct, by calling clasp->getObjectOps, and to
 * finalize the object.
 *
 * If this seems backwards, that's because it is!  API compatibility requires
 * a JSClass *clasp parameter to JS_NewObject, etc.  Most host objects do not
 * need to implement the larger JSObjectOps, and can share the common JSScope
 * code and data used by the native (js_ObjectOps, see jsobj.c) ops.
 */
typedef JSObjectOps *
(* JSGetObjectOps)(JSContext *cx, JSClass *clasp);

/*
 * JSClass.checkAccess type: check whether obj[id] may be accessed per mode,
 * returning false on error/exception, true on success with obj[id]'s last-got
 * value in *vp, and its attributes in *attrsp.  As for JSPropertyOp above, id
 * is either a string or an int jsval.
 *
 * See JSCheckAccessIdOp, below, for the JSObjectOps counterpart, which takes
 * a jsid (a tagged int or aligned, unique identifier pointer) rather than a
 * jsval.  The native js_ObjectOps.checkAccess simply forwards to the object's
 * clasp->checkAccess, so that both JSClass and JSObjectOps implementors may
 * specialize access checks.
 */
typedef JSBool
(* JSCheckAccessOp)(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
                    jsval *vp);

/*
 * Encode or decode an object, given an XDR state record representing external
 * data.  See jsxdrapi.h.
 */
typedef JSBool
(* JSXDRObjectOp)(JSXDRState *xdr, JSObject **objp);

/*
 * Check whether v is an instance of obj.  Return false on error or exception,
 * true on success with JS_TRUE in *bp if v is an instance of obj, JS_FALSE in
 * *bp otherwise.
 */
typedef JSBool
(* JSHasInstanceOp)(JSContext *cx, JSObject *obj, const jsval *v, JSBool *bp);

/*
 * Deprecated function type for JSClass.mark. All new code should define
 * JSTraceOp instead to ensure the traversal of traceable things stored in
 * the native structures.
 */
typedef uint32
(* JSMarkOp)(JSContext *cx, JSObject *obj, void *arg);

/*
 * Function type for trace operation of the class called to enumerate all
 * traceable things reachable from obj's private data structure. For each such
 * thing, a trace implementation must call
 *
 *    JS_CallTracer(trc, thing, kind);
 *
 * or one of its convenience macros as described in jsapi.h.
 *
 * JSTraceOp implementation can assume that no other threads mutates object
 * state. It must not change state of the object or corresponding native
 * structures. The only exception for this rule is the case when the embedding
 * needs a tight integration with GC. In that case the embedding can check if
 * the traversal is a part of the marking phase through calling
 * JS_IsGCMarkingTracer and apply a special code like emptying caches or
 * marking its native structures.
 *
 * To define the tracer for a JSClass, the implementation must add
 * JSCLASS_MARK_IS_TRACE to class flags and use JS_CLASS_TRACE(method)
 * macro below to convert JSTraceOp to JSMarkOp when initializing or
 * assigning JSClass.mark field.
 */
typedef void
(* JSTraceOp)(JSTracer *trc, JSObject *obj);

#if defined __GNUC__ && __GNUC__ >= 4 && !defined __cplusplus
# define JS_CLASS_TRACE(method)                                               \
    (__builtin_types_compatible_p(JSTraceOp, __typeof(&(method)))             \
     ? (JSMarkOp)(method)                                                     \
     : js_WrongTypeForClassTracer)

extern JSMarkOp js_WrongTypeForClassTracer;

#else
# define JS_CLASS_TRACE(method) ((JSMarkOp)(method))
#endif

/*
 * Tracer callback, called for each traceable thing directly referenced by a
 * particular object or runtime structure. It is the callback responsibility
 * to ensure the traversal of the full object graph via calling eventually
 * JS_TraceChildren on the passed thing. In this case the callback must be
 * prepared to deal with cycles in the traversal graph.
 *
 * kind argument is one of JSTRACE_OBJECT, JSTRACE_STRING or a tag denoting
 * internal implementation-specific traversal kind. In the latter case the only
 * operations on thing that the callback can do is to call JS_TraceChildren or
 * DEBUG-only JS_PrintTraceThingInfo.
 */
typedef void
(* JSTraceCallback)(JSTracer *trc, void *thing, uint32 kind);

/*
 * DEBUG only callback that JSTraceOp implementation can provide to return
 * a string describing the reference traced with JS_CallTracer.
 */
typedef void
(* JSTraceNamePrinter)(JSTracer *trc, char *buf, size_t bufsize);

/*
 * The optional JSClass.reserveSlots hook allows a class to make computed
 * per-instance object slots reservations, in addition to or instead of using
 * JSCLASS_HAS_RESERVED_SLOTS(n) in the JSClass.flags initializer to reserve
 * a constant-per-class number of slots.  Implementations of this hook should
 * return the number of slots to reserve, not including any reserved by using
 * JSCLASS_HAS_RESERVED_SLOTS(n) in JSClass.flags.
 *
 * NB: called with obj locked by the JSObjectOps-specific mutual exclusion
 * mechanism appropriate for obj, so don't nest other operations that might
 * also lock obj.
 */
typedef uint32
(* JSReserveSlotsOp)(JSContext *cx, JSObject *obj);

/* JSExtendedClass function pointer typedefs. */

/*
 *
 */
typedef JSBool
(* JSEqualityOp)(JSContext *cx, JSObject *obj, const jsval *v, JSBool *bp);

/*
 * A generic type for functions mapping an object to another object, or null
 * if an error or exception was thrown on cx.  Used by JSObjectOps.thisObject
 * at present.
 */
typedef JSObject *
(* JSObjectOp)(JSContext *cx, JSObject *obj);

/*
 * Hook that creates an iterator object for a given object. Returns the
 * iterator object or null if an error or exception was thrown on cx.
 */
typedef JSObject *
(* JSIteratorOp)(JSContext *cx, JSObject *obj, JSBool keysonly);

/* Typedef for native functions called by the JS VM. */

typedef JSBool
(* JSNative)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval);

typedef JSBool
(* JSFastNative)(JSContext *cx, uintN argc, jsval *vp);

/* Callbacks and their arguments. */

typedef enum JSContextOp {
    JSCONTEXT_NEW,
    JSCONTEXT_DESTROY
} JSContextOp;

/*
 * The possible values for contextOp when the runtime calls the callback are:
 *   JSCONTEXT_NEW      JS_NewContext successfully created a new JSContext
 *                      instance. The callback can initialize the instance as
 *                      required. If the callback returns false, the instance
 *                      will be destroyed and JS_NewContext returns null. In
 *                      this case the callback is not called again.
 *   JSCONTEXT_DESTROY  One of JS_DestroyContext* methods is called. The
 *                      callback may perform its own cleanup and must always
 *                      return true.
 *   Any other value    For future compatibility the callback must do nothing
 *                      and return true in this case.
 */
typedef JSBool
(* JSContextCallback)(JSContext *cx, uintN contextOp);

#ifndef JS_THREADSAFE
typedef void
(* JSHeartbeatCallback)(JSRuntime *rt);
#endif

typedef enum JSGCStatus {
    JSGC_BEGIN,
    JSGC_END,
    JSGC_MARK_END,
    JSGC_FINALIZE_END
} JSGCStatus;

typedef JSBool
(* JSGCCallback)(JSContext *cx, JSGCStatus status);

/*
 * Generic trace operation that calls JS_CallTracer on each traceable thing
 * stored in data.
 */
typedef void
(* JSTraceDataOp)(JSTracer *trc, void *data);

typedef JSBool
(* JSOperationCallback)(JSContext *cx);

/*
 * Deprecated form of JSOperationCallback.
 */
typedef JSBool
(* JSBranchCallback)(JSContext *cx, JSScript *script);

typedef void
(* JSErrorReporter)(JSContext *cx, const char *message, JSErrorReport *report);

/*
 * Possible exception types. These types are part of a JSErrorFormatString
 * structure. They define which error to throw in case of a runtime error.
 * JSEXN_NONE marks an unthrowable error.
 */
typedef enum JSExnType {
    JSEXN_NONE = -1,
      JSEXN_ERR,
        JSEXN_INTERNALERR,
        JSEXN_EVALERR,
        JSEXN_RANGEERR,
        JSEXN_REFERENCEERR,
        JSEXN_SYNTAXERR,
        JSEXN_TYPEERR,
        JSEXN_URIERR,
        JSEXN_LIMIT
} JSExnType;

typedef struct JSErrorFormatString {
    /* The error format string (UTF-8 if js_CStringsAreUTF8). */
    const char *format;

    /* The number of arguments to expand in the formatted error message. */
    uint16 argCount;

    /* One of the JSExnType constants above. */
    int16 exnType;
} JSErrorFormatString;

typedef const JSErrorFormatString *
(* JSErrorCallback)(void *userRef, const char *locale,
                    const uintN errorNumber);

#ifdef va_start
#define JS_ARGUMENT_FORMATTER_DEFINED 1

typedef JSBool
(* JSArgumentFormatter)(JSContext *cx, const char *format, JSBool fromJS,
                        jsval **vpp, va_list *app);
#endif

typedef JSBool
(* JSLocaleToUpperCase)(JSContext *cx, JSString *src, jsval *rval);

typedef JSBool
(* JSLocaleToLowerCase)(JSContext *cx, JSString *src, jsval *rval);

typedef JSBool
(* JSLocaleCompare)(JSContext *cx, JSString *src1, JSString *src2,
                    jsval *rval);

typedef JSBool
(* JSLocaleToUnicode)(JSContext *cx, char *src, jsval *rval);

/*
 * Security protocol types.
 */
typedef struct JSPrincipals JSPrincipals;

/*
 * XDR-encode or -decode a principals instance, based on whether xdr->mode is
 * JSXDR_ENCODE, in which case *principalsp should be encoded; or JSXDR_DECODE,
 * in which case implementations must return a held (via JSPRINCIPALS_HOLD),
 * non-null *principalsp out parameter.  Return true on success, false on any
 * error, which the implementation must have reported.
 */
typedef JSBool
(* JSPrincipalsTranscoder)(JSXDRState *xdr, JSPrincipals **principalsp);

/*
 * Return a weak reference to the principals associated with obj, possibly via
 * the immutable parent chain leading from obj to a top-level container (e.g.,
 * a window object in the DOM level 0).  If there are no principals associated
 * with obj, return null.  Therefore null does not mean an error was reported;
 * in no event should an error be reported or an exception be thrown by this
 * callback's implementation.
 */
typedef JSPrincipals *
(* JSObjectPrincipalsFinder)(JSContext *cx, JSObject *obj);

/*
 * Used to check if a CSP instance wants to disable eval() and friends.
 * See js_CheckCSPPermitsJSAction() in jsobj.
 */
typedef JSBool
(* JSCSPEvalChecker)(JSContext *cx);

JS_END_EXTERN_C

#ifdef __cplusplus
namespace js {

class Value;
struct Class;

typedef JSBool
(* Native)(JSContext *cx, JSObject *obj, uintN argc, Value *argv, Value *rval);
typedef JSBool
(* FastNative)(JSContext *cx, uintN argc, Value *vp);
typedef JSBool
(* PropertyOp)(JSContext *cx, JSObject *obj, jsid id, Value *vp);
typedef JSBool
(* ConvertOp)(JSContext *cx, JSObject *obj, JSType type, Value *vp);
typedef JSBool
(* NewEnumerateOp)(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                   Value *statep, jsid *idp);
typedef JSBool
(* HasInstanceOp)(JSContext *cx, JSObject *obj, const Value *v, JSBool *bp);
typedef JSBool
(* CheckAccessOp)(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
                  Value *vp);
typedef JSObjectOps *
(* GetObjectOps)(JSContext *cx, Class *clasp);
typedef JSBool
(* EqualityOp)(JSContext *cx, JSObject *obj, const Value *v, JSBool *bp);

/*
 * Since jsval and Value are layout-compatible, pointers to otherwise-identical
 * functions can be cast back and forth. To avoid widespread casting, the
 * following safe casts are provided.
 *
 * See also Valueify and Jsvalify overloads in jsprvtd.h.
 */

static inline Native           Valueify(JSNative f)         { return (Native)f; }
static inline JSNative         Jsvalify(Native f)           { return (JSNative)f; }
static inline FastNative       Valueify(JSFastNative f)     { return (FastNative)f; }
static inline JSFastNative     Jsvalify(FastNative f)       { return (JSFastNative)f; }
static inline PropertyOp       Valueify(JSPropertyOp f)     { return (PropertyOp)f; }
static inline JSPropertyOp     Jsvalify(PropertyOp f)       { return (JSPropertyOp)f; }
static inline ConvertOp        Valueify(JSConvertOp f)      { return (ConvertOp)f; }
static inline JSConvertOp      Jsvalify(ConvertOp f)        { return (JSConvertOp)f; }
static inline NewEnumerateOp   Valueify(JSNewEnumerateOp f) { return (NewEnumerateOp)f; }
static inline JSNewEnumerateOp Jsvalify(NewEnumerateOp f)   { return (JSNewEnumerateOp)f; }
static inline HasInstanceOp    Valueify(JSHasInstanceOp f)  { return (HasInstanceOp)f; }
static inline JSHasInstanceOp  Jsvalify(HasInstanceOp f)    { return (JSHasInstanceOp)f; }
static inline CheckAccessOp    Valueify(JSCheckAccessOp f)  { return (CheckAccessOp)f; }
static inline JSCheckAccessOp  Jsvalify(CheckAccessOp f)    { return (JSCheckAccessOp)f; }
static inline GetObjectOps     Valueify(JSGetObjectOps f)   { return (GetObjectOps)f; }
static inline JSGetObjectOps   Jsvalify(GetObjectOps f)     { return (JSGetObjectOps)f; }
static inline EqualityOp       Valueify(JSEqualityOp f);    /* Same type as JSHasInstanceOp */
static inline JSEqualityOp     Jsvalify(EqualityOp f);      /* Same type as HasInstanceOp */

}  /* namespace js */
#endif /* defined(__cplusplus) */

#endif /* jspubtd_h___ */

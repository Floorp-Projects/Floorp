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

/*
 * JavaScript engine unboxed value representation
 */

/*
 * A jsval has an abstract type which is represented by a mask which assigns a
 * bit to each type. This allows fast set-membership queries. However, we give
 * one type (null) a mask of 0 for two reasons:
 *  1. memset'ing values to 0 produces a valid value. This was true of the old,
 *     boxed jsvals (and now jsboxedwords) and eases the transition.
 *  2. Testing for null can often be compiled to slightly shorter/faster code.
 *
 * The down-side is that set-membership queries need to be done more carefully.
 * E.g., to test whether a value v is undefined or null, the correct test is:
 *   (v.mask & ~UndefinedMask) == 0
 * instead of the intuitive (but incorrect) test:
 *   (v.mask & (NullMask | UndefinedMask)) != 0
 * Since the value representation is kept a private detail of js::Value and
 * only exposed to a few functions through friendship, this type of error
 * should be hidden behind simple inline methods like v.isNullOrUndefined().
 */

/*
 * Types are unsigned machine-words. On 32-bit systems, values are padded with
 * an extra word so that double payloads are aligned properly.
 */
#if JS_BITS_PER_WORD == 32
typedef uint32 JSValueMaskType;
# define JSVAL_TYPE_BITS 32
# define JS_INSERT_VALUE_PADDING() uint32 padding;
#elif JS_BITS_PER_WORD == 64
typedef JSUint64 JSValueMaskType;
# define JSVAL_TYPE_BITS 32
# define JS_INSERT_VALUE_PADDING()
#else
# error "Unsupported word size"
#endif

#define JSVAL_NULL_MASK        ((JSValueMaskType)0x00)
#define JSVAL_UNDEFINED_MASK   ((JSValueMaskType)0x01)
#define JSVAL_INT32_MASK       ((JSValueMaskType)0x02)
#define JSVAL_DOUBLE_MASK      ((JSValueMaskType)0x04)
#define JSVAL_STRING_MASK      ((JSValueMaskType)0x08)
#define JSVAL_NONFUNOBJ_MASK   ((JSValueMaskType)0x10)
#define JSVAL_FUNOBJ_MASK      ((JSValueMaskType)0x20)
#define JSVAL_BOOLEAN_MASK     ((JSValueMaskType)0x40)
#define JSVAL_MAGIC_MASK       ((JSValueMaskType)0x80)

/*
 * Magic value enumeration (private engine detail)
 *
 * This enumeration provides a debug-only code describing the source of an
 * invalid value. These codes can be used to assert that the different sources
 * of invalid never mix.
 */
typedef enum JSWhyMagic
{
    JS_ARRAY_HOLE,               /* a hole in a dense array */
    JS_ARGS_HOLE,                /* a hole in the args object's array */
    JS_NATIVE_ENUMERATE,         /* indicates that a custom enumerate hook forwarded
                                  * to js_Enumerate, which really means the object can be
                                  * enumerated like a native object. */
    JS_NO_ITER_VALUE,            /* there is not a pending iterator value */
    JS_GENERATOR_CLOSING         /* exception value thrown when closing a generator */
} JSWhyMagic;

typedef union jsval_data
{
    int32          i32;
    uint32         u32;
    double         dbl;
    JSString *     str;
    JSObject *     obj;
    void *         ptr;
    JSBool         boo;
#ifdef DEBUG
    JSWhyMagic     why;
#endif
    struct { int32 first; int32 second; } bits;
} jsval_data;

/* See js::Value. */
typedef struct jsval
{
    JSValueMaskType mask;
    JS_INSERT_VALUE_PADDING()
    jsval_data data;
} jsval;

/*
 * Boxed word macros (private engine detail)
 *
 * N.B. jsboxedword and the JSBOXEDWORD macros are engine-private. Callers
 * should use only JSID macros (below) instead.
 *
 * The jsboxedword type is used by atoms and jsids. Eventually, the ability to
 * atomize any primitive will be removed and atoms will simply be unboxed,
 * interned JSString*s. However, jsids will always need boxing. Using a
 * one-word boxing scheme instead of the normal jsval 16-byte unboxed scheme
 * allows jsids to be passed by value without penalty, since jsids never are
 * doubles nor are jsids used to build typemaps for entering/leaving trace.
 */

typedef jsword jsboxedword;

#define JSBOXEDWORD_TYPE_OBJECT     0x0
#define JSBOXEDWORD_TYPE_INT        0x1
#define JSBOXEDWORD_TYPE_DOUBLE     0x2
#define JSBOXEDWORD_TYPE_STRING     0x4
#define JSBOXEDWORD_TYPE_SPECIAL    0x6

/* Type tag bitfield length and derived macros. */
#define JSBOXEDWORD_TAGBITS         3
#define JSBOXEDWORD_TAGMASK         ((jsboxedword) JS_BITMASK(JSBOXEDWORD_TAGBITS))
#define JSBOXEDWORD_ALIGN           JS_BIT(JSBOXEDWORD_TAGBITS)

static const jsboxedword JSBOXEDWORD_NULL  = (jsboxedword)0x0;
static const jsboxedword JSBOXEDWORD_FALSE = (jsboxedword)0x6;
static const jsboxedword JSBOXEDWORD_TRUE  = (jsboxedword)0xe;
static const jsboxedword JSBOXEDWORD_VOID  = (jsboxedword)0x16;

static JS_ALWAYS_INLINE JSBool
JSBOXEDWORD_IS_NULL(jsboxedword w)
{
    return w == JSBOXEDWORD_NULL;
}

static JS_ALWAYS_INLINE JSBool
JSBOXEDWORD_IS_VOID(jsboxedword w)
{
    return w == JSBOXEDWORD_VOID;
}

static JS_ALWAYS_INLINE unsigned
JSBOXEDWORD_TAG(jsboxedword w)
{
    return (unsigned)(w & JSBOXEDWORD_TAGMASK);
}

static JS_ALWAYS_INLINE jsboxedword
JSBOXEDWORD_SETTAG(jsboxedword w, unsigned t)
{
    return w | t;
}

static JS_ALWAYS_INLINE jsboxedword
JSBOXEDWORD_CLRTAG(jsboxedword w)
{
    return w & ~(jsboxedword)JSBOXEDWORD_TAGMASK;
}

static JS_ALWAYS_INLINE JSBool
JSBOXEDWORD_IS_DOUBLE(jsboxedword w)
{
    return JSBOXEDWORD_TAG(w) == JSBOXEDWORD_TYPE_DOUBLE;
}

static JS_ALWAYS_INLINE double *
JSBOXEDWORD_TO_DOUBLE(jsboxedword w)
{
    JS_ASSERT(JSBOXEDWORD_IS_DOUBLE(w));
    return (double *)JSBOXEDWORD_CLRTAG(w);
}

static JS_ALWAYS_INLINE jsboxedword
DOUBLE_TO_JSBOXEDWORD(double *d)
{
    JS_ASSERT(((JSUword)d & JSBOXEDWORD_TAGMASK) == 0);
    return (jsboxedword)((JSUword)d | JSBOXEDWORD_TYPE_DOUBLE);
}

static JS_ALWAYS_INLINE JSBool
JSBOXEDWORD_IS_STRING(jsboxedword w)
{
    return JSBOXEDWORD_TAG(w) == JSBOXEDWORD_TYPE_STRING;
}

static JS_ALWAYS_INLINE JSString *
JSBOXEDWORD_TO_STRING(jsboxedword w)
{
    JS_ASSERT(JSBOXEDWORD_IS_STRING(w));
    return (JSString *)JSBOXEDWORD_CLRTAG(w);
}

static JS_ALWAYS_INLINE JSBool
JSBOXEDWORD_IS_SPECIAL(jsboxedword w)
{
    return JSBOXEDWORD_TAG(w) == JSBOXEDWORD_TYPE_SPECIAL;
}

static JS_ALWAYS_INLINE jsint
JSBOXEDWORD_TO_SPECIAL(jsboxedword w)
{
    JS_ASSERT(JSBOXEDWORD_IS_SPECIAL(w));
    return w >> JSBOXEDWORD_TAGBITS;
}

static JS_ALWAYS_INLINE jsboxedword
SPECIAL_TO_JSBOXEDWORD(jsint i)
{
    return (i << JSBOXEDWORD_TAGBITS) | JSBOXEDWORD_TYPE_SPECIAL;
}

static JS_ALWAYS_INLINE JSBool
JSBOXEDWORD_IS_BOOLEAN(jsboxedword w)
{
    return (w & ~((jsboxedword)1 << JSBOXEDWORD_TAGBITS)) == JSBOXEDWORD_TYPE_SPECIAL;
}

static JS_ALWAYS_INLINE JSBool
JSBOXEDWORD_TO_BOOLEAN(jsboxedword w)
{
    JS_ASSERT(w == JSBOXEDWORD_TRUE || w == JSBOXEDWORD_FALSE);
    return JSBOXEDWORD_TO_SPECIAL(w);
}

static JS_ALWAYS_INLINE jsboxedword
BOOLEAN_TO_JSBOXEDWORD(JSBool b)
{
    JS_ASSERT(b == JS_TRUE || b == JS_FALSE);
    return SPECIAL_TO_JSBOXEDWORD(b);
}

static JS_ALWAYS_INLINE jsboxedword
STRING_TO_JSBOXEDWORD(JSString *str)
{
    JS_ASSERT(((JSUword)str & JSBOXEDWORD_TAGMASK) == 0);
    return (jsboxedword)str | JSBOXEDWORD_TYPE_STRING;
}

static JS_ALWAYS_INLINE JSBool
JSBOXEDWORD_IS_GCTHING(jsboxedword w)
{
    return !(w & JSBOXEDWORD_TYPE_INT) &&
           JSBOXEDWORD_TAG(w) != JSBOXEDWORD_TYPE_SPECIAL;
}

static JS_ALWAYS_INLINE void *
JSBOXEDWORD_TO_GCTHING(jsboxedword w)
{
    JS_ASSERT(JSBOXEDWORD_IS_GCTHING(w));
    return (void *)JSBOXEDWORD_CLRTAG(w);
}

static JS_ALWAYS_INLINE uint32
JSBOXEDWORD_TRACE_KIND(jsboxedword w)
{
    JS_ASSERT(w == 0x0 || w == 0x2 || w == 0x4);
    /*
     * We need to map:
     *  XXXXXXXXXXXXXXXXXXXXXXXXXXXXX000 -> 00 (object)
     *  XXXXXXXXXXXXXXXXXXXXXXXXXXXXX010 -> 10 (double)
     *  XXXXXXXXXXXXXXXXXXXXXXXXXXXXX100 -> 01 (string)
     */
    return (w | ((w & 0x4) >> 2)) & 0x3;
}

static JS_ALWAYS_INLINE JSBool
JSBOXEDWORD_IS_OBJECT(jsboxedword w)
{
    return JSBOXEDWORD_TAG(w) == JSBOXEDWORD_TYPE_OBJECT;
}

static JS_ALWAYS_INLINE JSObject *
JSBOXEDWORD_TO_OBJECT(jsboxedword w)
{
    JS_ASSERT(JSBOXEDWORD_IS_OBJECT(w));
    return (JSObject *)JSBOXEDWORD_TO_GCTHING(w);
}

static JS_ALWAYS_INLINE jsboxedword
OBJECT_TO_JSBOXEDWORD(JSObject *obj)
{
    JS_ASSERT(((JSUword)obj & JSBOXEDWORD_TAGMASK) == 0);
    return (jsboxedword)obj | JSBOXEDWORD_TYPE_OBJECT;
}

static JS_ALWAYS_INLINE JSBool
JSBOXEDWORD_IS_PRIMITIVE(jsboxedword w)
{
    return !JSBOXEDWORD_IS_OBJECT(w) || JSBOXEDWORD_IS_NULL(w);
}

/* Domain limits for the jsboxedword int type. */
#define JSBOXEDWORD_INT_BITS          31
#define JSBOXEDWORD_INT_POW2(n)       ((jsboxedword)1 << (n))
#define JSBOXEDWORD_INT_MIN           (-JSBOXEDWORD_INT_POW2(30))
#define JSBOXEDWORD_INT_MAX           (JSBOXEDWORD_INT_POW2(30) - 1)

static JS_ALWAYS_INLINE JSBool
INT32_FITS_IN_JSBOXEDWORD(jsint i)
{
    return ((jsuint)(i) - (jsuint)JSBOXEDWORD_INT_MIN <=
            (jsuint)(JSBOXEDWORD_INT_MAX - JSBOXEDWORD_INT_MIN));
}

static JS_ALWAYS_INLINE JSBool
JSBOXEDWORD_IS_INT(jsboxedword w)
{
    return w & JSBOXEDWORD_TYPE_INT;
}

static JS_ALWAYS_INLINE jsint
JSBOXEDWORD_TO_INT(jsboxedword v)
{
    JS_ASSERT(JSBOXEDWORD_IS_INT(v));
    return (jsint)(v >> 1);
}

static JS_ALWAYS_INLINE jsboxedword
INT_TO_JSBOXEDWORD(jsint i)
{
    JS_ASSERT(INT32_FITS_IN_JSBOXEDWORD(i));
    return (i << 1) | JSBOXEDWORD_TYPE_INT;
}

/*
 * Identifier (jsid) macros.
 */

typedef jsboxedword jsid;

#define JSID_NULL                     ((jsid)JSBOXEDWORD_NULL)
#define JSID_VOID                     ((jsid)JSBOXEDWORD_VOID)
#define JSID_IS_NULL(id)              JSBOXEDWORD_IS_NULL((jsboxedword)(id))
#define JSID_IS_VOID(id)              JSBOXEDWORD_IS_VOID((jsboxedword)(id))
#define JSID_IS_ATOM(id)              JSBOXEDWORD_IS_STRING((jsboxedword)(id))
#define JSID_TO_ATOM(id)              ((JSAtom *)(id))
#define ATOM_TO_JSID(atom)            (JS_ASSERT(ATOM_IS_STRING(atom)),        \
                                       (jsid)(atom))

#define INT32_FITS_IN_JSID(id)        INT32_FITS_IN_JSBOXEDWORD(id)
#define JSID_IS_INT(id)               JSBOXEDWORD_IS_INT((jsboxedword)(id))
#define JSID_TO_INT(id)               JSBOXEDWORD_TO_INT((jsboxedword)(id))
#define INT_TO_JSID(i)                ((jsid)INT_TO_JSBOXEDWORD((i)))

#define JSID_IS_OBJECT(id)            JSBOXEDWORD_IS_OBJECT((jsboxedword)(id))
#define JSID_TO_OBJECT(id)            JSBOXEDWORD_TO_OBJECT((jsboxedword)(id))
#define OBJECT_TO_JSID(obj)           ((jsid)OBJECT_TO_JSBOXEDWORD((obj)))

/* Objects and strings (no doubles in jsids). */
#define JSID_IS_GCTHING(id)           JSBOXEDWORD_IS_GCTHING(id)
#define JSID_TO_GCTHING(id)           (JS_ASSERT(JSID_IS_GCTHING((id))),       \
                                       JSBOXEDWORD_TO_GCTHING((jsboxedword)(id)))
#define JSID_TRACE_KIND(id)           (JS_ASSERT(JSID_IS_GCTHING((id))),       \
                                       JSBOXEDWORD_TRACE_KIND((jsboxedword)(id)))

JS_PUBLIC_API(jsval)
JSID_TO_JSVAL(jsid id);

/* JSClass (and JSObjectOps where appropriate) function pointer typedefs. */

/*
 * Add, delete, get or set a property named by id in obj.  Note the jsval id
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
 * Tracer callback, called for each traceable thing directly refrenced by a
 * particular object or runtime structure. It is the callback responsibility
 * to ensure the traversal of the full object graph via calling eventually
 * JS_TraceChildren on the passed thing. In this case the callback must be
 * prepared to deal with cycles in the traversal graph.
 *
 * kind argument is one of JSTRACE_OBJECT, JSTRACE_DOUBLE, JSTRACE_STRING or
 * a tag denoting internal implementation-specific traversal kind. In the
 * latter case the only operations on thing that the callback can do is to call
 * JS_TraceChildren or DEBUG-only JS_PrintTraceThingInfo.
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
class Class;

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
#endif /* __cplusplus */
#endif /* jspubtd_h___ */

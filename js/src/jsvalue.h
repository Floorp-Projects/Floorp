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

#ifndef jsvalue_h__
#define jsvalue_h__
/*
 * Private value interface.
 */
#include "jsprvtd.h"
#include "jsstdint.h"

/*
 * js::Value is a C++-ified version of jsval that provides more information and
 * helper functions than the basic jsval interface exposed by jsapi.h. A few
 * general notes on js::Value:
 *
 * - Since js::Value and jsval have the same representation, values of these
 *   types, function pointer types differing only in these types, and structs
 *   differing only in these types can be converted back and forth at no cost
 *   using the Jsvalify() and Valueify(). See Jsvalify comment below.
 *
 * - js::Value has setX() and isX() members for X in
 *
 *     { Int32, Double, String, Boolean, Undefined, Null, Object, Magic }
 *
 *   js::Value also contains toX() for each of the non-singleton types.
 *
 * - Magic is a singleton type whose payload contains a JSWhyMagic "reason" for
 *   the magic value. By providing JSWhyMagic values when creating and checking
 *   for magic values, it is possible to assert, at runtime, that only magic
 *   values with the expected reason flow through a particular value. For
 *   example, if cx->exception has a magic value, the reason must be
 *   JS_GENERATOR_CLOSING.
 *
 * - A key difference between jsval and js::Value is that js::Value gives null
 *   a separate type. Thus
 *
 *           JSVAL_IS_OBJECT(v) === v.isObjectOrNull()
 *       !JSVAL_IS_PRIMITIVE(v) === v.isObject()
 *
 *   To help prevent mistakenly boxing a nullable JSObject* as an object,
 *   Value::setObject takes a JSObject&. (Conversely, Value::asObject returns a
 *   JSObject&. A convenience member Value::setObjectOrNull is provided.
 *
 * - JSVAL_VOID is the same as the singleton value of the Undefined type.
 *
 * - Note that js::Value is always 64-bit. Thus, on 32-bit user code should
 *   avoid copying jsval/js::Value as much as possible, preferring to pass by
 *   const Value &.
 */

/******************************************************************************/

/* To avoid a circular dependency, pull in the necessary pieces of jsnum.h. */

#include <math.h>
#if defined(XP_WIN) || defined(XP_OS2)
#include <float.h>
#endif
#ifdef SOLARIS
#include <ieeefp.h>
#endif

static inline int
JSDOUBLE_IS_NEGZERO(jsdouble d)
{
#ifdef WIN32
    return (d == 0 && (_fpclass(d) & _FPCLASS_NZ));
#elif defined(SOLARIS)
    return (d == 0 && copysign(1, d) < 0);
#else
    return (d == 0 && signbit(d));
#endif
}

static inline bool
JSDOUBLE_IS_INT32(jsdouble d, int32_t* pi)
{
    if (JSDOUBLE_IS_NEGZERO(d))
        return false;
    return d == (*pi = int32_t(d));
}

/******************************************************************************/

/* Additional value operations used in js::Value but not in jsapi.h. */

#if JS_BITS_PER_WORD == 32

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

/******************************************************************************/

namespace js {

class Value
{
  public:
    /*** Constructors ***/

    /* N.B. the default constructor creates a double. */
    Value() { data.asBits = 0; }

    /*** Mutatators ***/

    void setNull() {
        data.asBits = JSVAL_BITS(JSVAL_NULL);
    }

    void setUndefined() {
        data.asBits = JSVAL_BITS(JSVAL_VOID);
    }

    void setInt32(int32 i) {
        data = INT32_TO_JSVAL_IMPL(i);
    }

    int32 &getInt32Ref() {
        JS_ASSERT(isInt32());
        return data.s.payload.i32;
    }

    void setDouble(double d) {
        data = DOUBLE_TO_JSVAL_IMPL(d);
    }

    double &getDoubleRef() {
        JS_ASSERT(isDouble());
        return data.asDouble;
    }

    void setString(JSString *str) {
        data = STRING_TO_JSVAL_IMPL(str);
    }

    void setObject(JSObject &obj) {
        JS_ASSERT(&obj != NULL);
        data = OBJECT_TO_JSVAL_IMPL(&obj);
    }

    void setBoolean(bool b) {
        data = BOOLEAN_TO_JSVAL_IMPL(b);
    }

    void setMagic(JSWhyMagic why) {
        data = MAGIC_TO_JSVAL_IMPL(why);
    }

    JS_ALWAYS_INLINE
    void setNumber(uint32 ui) {
        if (ui > JSVAL_INT_MAX)
            setDouble((double)ui);
        else
            setInt32((int32)ui);
    }

    JS_ALWAYS_INLINE
    void setNumber(double d) {
        int32_t i;
        if (JSDOUBLE_IS_INT32(d, &i))
            setInt32(i);
        else
            setDouble(d);
    }

    JS_ALWAYS_INLINE
    void setObjectOrNull(JSObject *arg) {
        if (arg)
            setObject(*arg);
        else
            setNull();
    }

    JS_ALWAYS_INLINE
    void setObjectOrUndefined(JSObject *arg) {
        if (arg)
            setObject(*arg);
        else
            setUndefined();
    }

    void swap(Value &rhs) {
        uint64 tmp = rhs.data.asBits;
        rhs.data.asBits = data.asBits;
        data.asBits = tmp;
    }

    /*** Value type queries ***/

    bool isUndefined() const {
        return JSVAL_IS_UNDEFINED_IMPL(data);
    }

    bool isNull() const {
        return JSVAL_IS_NULL_IMPL(data);
    }

    bool isNullOrUndefined() const {
        return isNull() || isUndefined();
    }

    bool isInt32() const {
        return JSVAL_IS_INT32_IMPL(data);
    }

    bool isInt32(int32 i32) const {
        return JSVAL_IS_SPECIFIC_INT32_IMPL(data, i32);
    }

    bool isDouble() const {
        return JSVAL_IS_DOUBLE_IMPL(data);
    }

    bool isNumber() const {
        return JSVAL_IS_NUMBER_IMPL(data);
    }

    bool isString() const {
        return JSVAL_IS_STRING_IMPL(data);
    }

    bool isObject() const {
        return JSVAL_IS_OBJECT_IMPL(data);
    }

    bool isPrimitive() const {
        return JSVAL_IS_PRIMITIVE_IMPL(data);
    }

    bool isObjectOrNull() const {
        return JSVAL_IS_OBJECT_OR_NULL_IMPL(data);
    }

    bool isGCThing() const {
        return JSVAL_IS_GCTHING_IMPL(data);
    }

    bool isBoolean() const {
        return JSVAL_IS_BOOLEAN_IMPL(data);
    }

    bool isTrue() const {
        return JSVAL_IS_SPECIFIC_BOOLEAN(data, true);
    }

    bool isFalse() const {
        return JSVAL_IS_SPECIFIC_BOOLEAN(data, false);
    }

    bool isMagic() const {
        return JSVAL_IS_MAGIC_IMPL(data);
    }

    bool isMagic(JSWhyMagic why) const {
        JS_ASSERT_IF(isMagic(), data.s.payload.why == why);
        return JSVAL_IS_MAGIC_IMPL(data);
    }

    bool isMarkable() const {
        return JSVAL_IS_TRACEABLE_IMPL(data);
    }

    int32 gcKind() const {
        JS_ASSERT(isMarkable());
        return JSVAL_TRACE_KIND_IMPL(data);
    }

#ifdef DEBUG
    JSWhyMagic whyMagic() const {
        JS_ASSERT(isMagic());
        return data.s.payload.why;
    }
#endif

    /*** Comparison ***/

    bool operator==(const Value &rhs) const {
        return data.asBits == rhs.data.asBits;
    }

    bool operator!=(const Value &rhs) const {
        return data.asBits != rhs.data.asBits;
    }

    friend bool SameType(const Value &lhs, const Value &rhs) {
        return JSVAL_SAME_TYPE_IMPL(lhs.data, rhs.data);
    }

    /*** Extract the value's typed payload ***/

    int32 toInt32() const {
        JS_ASSERT(isInt32());
        return JSVAL_TO_INT32_IMPL(data);
    }

    double toDouble() const {
        JS_ASSERT(isDouble());
        return data.asDouble;
    }

    double toNumber() const {
        JS_ASSERT(isNumber());
        return isDouble() ? toDouble() : double(toInt32());
    }

    JSString *toString() const {
        JS_ASSERT(isString());
        return JSVAL_TO_STRING_IMPL(data);
    }

    JSObject &toObject() const {
        JS_ASSERT(isObject());
        return *JSVAL_TO_OBJECT_IMPL(data);
    }

    JSObject *toObjectOrNull() const {
        JS_ASSERT(isObjectOrNull());
        return JSVAL_TO_OBJECT_IMPL(data);
    }

    void *asGCThing() const {
        JS_ASSERT(isGCThing());
        return JSVAL_TO_GCTHING_IMPL(data);
    }

    bool toBoolean() const {
        JS_ASSERT(isBoolean());
        return JSVAL_TO_BOOLEAN_IMPL(data);
    }

    uint32 payloadAsRawUint32() const {
        JS_ASSERT(!isDouble());
        return data.s.payload.u32;
    }

    uint64 asRawBits() const {
        return data.asBits;
    }

    /*
     * In the extract/box/unbox functions below, "NonDouble" means this
     * functions must not be called on a value that is a double. This allows
     * these operations to be implemented more efficiently, since doubles
     * generally already require special handling by the caller.
     */
    JSValueType extractNonDoubleType() const {
        return JSVAL_EXTRACT_NON_DOUBLE_TYPE_IMPL(data);
    }

    JSValueTag extractNonDoubleTag() const {
        return JSVAL_EXTRACT_NON_DOUBLE_TAG_IMPL(data);
    }

    void unboxNonDoubleTo(uint64 *out) const {
        UNBOX_NON_DOUBLE_JSVAL(data, out);
    }

    void boxNonDoubleFrom(JSValueType type, uint64 *out) {
        data = BOX_NON_DOUBLE_JSVAL(type, out);
    }

    /*
     * The trace-jit specializes JSVAL_TYPE_OBJECT into JSVAL_TYPE_FUNOBJ and
     * JSVAL_TYPE_NONFUNOBJ. Since these two operations just return the type of
     * a value, the caller must handle JSVAL_TYPE_OBJECT separately.
     */
    JSValueType extractNonDoubleObjectTraceType() const {
        JS_ASSERT(!isObject());
        return JSVAL_EXTRACT_NON_DOUBLE_TYPE_IMPL(data);
    }

    JSValueTag extractNonDoubleObjectTraceTag() const {
        JS_ASSERT(!isObject());
        return JSVAL_EXTRACT_NON_DOUBLE_TAG_IMPL(data);
    }

    /*
     * Private API
     *
     * Private setters/getters allow the caller to read/write arbitrary types
     * that fit in the 64-bit payload. It is the caller's responsibility, after
     * storing to a value with setPrivateX to only read with getPrivateX.
     * Privates values are given a type type which ensures they are not marked.
     */

    bool isUnderlyingTypeOfPrivate() const {
        return JSVAL_IS_UNDERLYING_TYPE_OF_PRIVATE_IMPL(data);
    }

    void setPrivate(void *ptr) {
        data = PRIVATE_PTR_TO_JSVAL_IMPL(ptr);
    }

    void *toPrivate() const {
        JS_ASSERT(JSVAL_IS_UNDERLYING_TYPE_OF_PRIVATE_IMPL(data));
        return JSVAL_TO_PRIVATE_PTR_IMPL(data);
    }

    void setPrivateUint32(uint32 ui) {
        data = PRIVATE_UINT32_TO_JSVAL_IMPL(ui);
    }

    uint32 toPrivateUint32() const {
        JS_ASSERT(JSVAL_IS_UNDERLYING_TYPE_OF_PRIVATE_IMPL(data));
        return JSVAL_TO_PRIVATE_UINT32_IMPL(data);
    }

    uint32 &getPrivateUint32Ref() {
        JS_ASSERT(isDouble());
        return data.s.payload.u32;
    }

  private:
    void staticAssertions() {
        JS_STATIC_ASSERT(sizeof(JSValueType) == 1);
        JS_STATIC_ASSERT(sizeof(JSValueTag) == 4);
        JS_STATIC_ASSERT(sizeof(JSBool) == 4);
        JS_STATIC_ASSERT(sizeof(JSWhyMagic) <= 4);
        JS_STATIC_ASSERT(sizeof(jsval) == 8);
    }

    jsval_layout data;
} JSVAL_ALIGNMENT;

static JS_ALWAYS_INLINE Value
NullValue()
{
    Value v;
    v.setNull();
    return v;
}

static JS_ALWAYS_INLINE Value
UndefinedValue()
{
    Value v;
    v.setUndefined();
    return v;
}

static JS_ALWAYS_INLINE Value
Int32Value(int32 i32)
{
    Value v;
    v.setInt32(i32);
    return v;
}

static JS_ALWAYS_INLINE Value
DoubleValue(double dbl)
{
    Value v;
    v.setDouble(dbl);
    return v;
}

static JS_ALWAYS_INLINE Value
StringValue(JSString *str)
{
    Value v;
    v.setString(str);
    return v;
}

static JS_ALWAYS_INLINE Value
BooleanValue(bool boo)
{
    Value v;
    v.setBoolean(boo);
    return v;
}

static JS_ALWAYS_INLINE Value
ObjectValue(JSObject &obj)
{
    Value v;
    v.setObject(obj);
    return v;
}

static JS_ALWAYS_INLINE Value
MagicValue(JSWhyMagic why)
{
    Value v;
    v.setMagic(why);
    return v;
}

static JS_ALWAYS_INLINE Value
NumberValue(double dbl)
{
    Value v;
    v.setNumber(dbl);
    return v;
}

static JS_ALWAYS_INLINE Value
ObjectOrNullValue(JSObject *obj)
{
    Value v;
    v.setObjectOrNull(obj);
    return v;
}

static JS_ALWAYS_INLINE Value
PrivateValue(void *ptr)
{
    Value v;
    v.setPrivate(ptr);
    return v;
}

/******************************************************************************/

/*
 * As asserted above, js::Value and jsval are layout equivalent. This means:
 *  - an instance of jsval may be reinterpreted as a js::Value and vice versa;
 *  - a pointer to a function taking jsval arguments may be reinterpreted as a
 *    function taking the same arguments, s/jsval/js::Value/, and vice versa;
 *  - a struct containing jsval members may be reinterpreted as a struct with
 *    the same members, s/jsval/js::Value/, and vice versa.
 *
 * To prevent widespread conversion using casts, which would effectively
 * disable the C++ typesystem in places where we want it, a set of safe
 * conversions between known-equivalent types is provided below. Given a type
 * JsvalT expressedin terms of jsval and an equivalent type ValueT expressed in
 * terms of js::Value, instances may be converted back and forth using:
 *
 *   JsvalT *x = ...
 *   ValueT *y = js::Valueify(x);
 *   JsvalT *z = js::Jsvalify(y);
 *   assert(x == z);
 *
 * Conversions between references is also provided for some types. If it seems
 * like a cast is needed to convert between jsval/js::Value, consider adding a
 * new safe overload to Jsvalify/Valueify.
 */

static inline jsval *        Jsvalify(Value *v)        { return (jsval *)v; }
static inline const jsval *  Jsvalify(const Value *v)  { return (const jsval *)v; }
static inline jsval &        Jsvalify(Value &v)        { return (jsval &)v; }
static inline const jsval &  Jsvalify(const Value &v)  { return (const jsval &)v; }
static inline Value *        Valueify(jsval *v)        { return (Value *)v; }
static inline const Value *  Valueify(const jsval *v)  { return (const Value *)v; }
static inline Value **       Valueify(jsval **v)       { return (Value **)v; }
static inline Value &        Valueify(jsval &v)        { return (Value &)v; }
static inline const Value &  Valueify(const jsval &v)  { return (const Value &)v; }

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
typedef JSBool
(* DefinePropOp)(JSContext *cx, JSObject *obj, jsid id, const Value *value,
                 PropertyOp getter, PropertyOp setter, uintN attrs);
typedef JSBool
(* PropertyIdOp)(JSContext *cx, JSObject *obj, jsid id, Value *vp);
typedef JSBool
(* CallOp)(JSContext *cx, uintN argc, Value *vp);

static inline Native            Valueify(JSNative f)          { return (Native)f; }
static inline JSNative          Jsvalify(Native f)            { return (JSNative)f; }
static inline FastNative        Valueify(JSFastNative f)      { return (FastNative)f; }
static inline JSFastNative      Jsvalify(FastNative f)        { return (JSFastNative)f; }
static inline PropertyOp        Valueify(JSPropertyOp f)      { return (PropertyOp)f; }
static inline JSPropertyOp      Jsvalify(PropertyOp f)        { return (JSPropertyOp)f; }
static inline ConvertOp         Valueify(JSConvertOp f)       { return (ConvertOp)f; }
static inline JSConvertOp       Jsvalify(ConvertOp f)         { return (JSConvertOp)f; }
static inline NewEnumerateOp    Valueify(JSNewEnumerateOp f)  { return (NewEnumerateOp)f; }
static inline JSNewEnumerateOp  Jsvalify(NewEnumerateOp f)    { return (JSNewEnumerateOp)f; }
static inline HasInstanceOp     Valueify(JSHasInstanceOp f)   { return (HasInstanceOp)f; }
static inline JSHasInstanceOp   Jsvalify(HasInstanceOp f)     { return (JSHasInstanceOp)f; }
static inline CheckAccessOp     Valueify(JSCheckAccessOp f)   { return (CheckAccessOp)f; }
static inline JSCheckAccessOp   Jsvalify(CheckAccessOp f)     { return (JSCheckAccessOp)f; }
static inline GetObjectOps      Valueify(JSGetObjectOps f)    { return (GetObjectOps)f; }
static inline JSGetObjectOps    Jsvalify(GetObjectOps f)      { return (JSGetObjectOps)f; }
static inline EqualityOp        Valueify(JSEqualityOp f);     /* Same type as JSHasInstanceOp */
static inline JSEqualityOp      Jsvalify(EqualityOp f);       /* Same type as HasInstanceOp */
static inline DefinePropOp      Valueify(JSDefinePropOp f)    { return (DefinePropOp)f; }
static inline JSDefinePropOp    Jsvalify(DefinePropOp f)      { return (JSDefinePropOp)f; }
static inline PropertyIdOp      Valueify(JSPropertyIdOp f);   /* Same type as JSPropertyOp */
static inline JSPropertyIdOp    Jsvalify(PropertyIdOp f);     /* Same type as PropertyOp */
static inline CallOp            Valueify(JSCallOp f);         /* Same type as JSFastNative */
static inline JSCallOp          Jsvalify(CallOp f);           /* Same type as FastNative */

static const PropertyOp    PropertyStub  = (PropertyOp)JS_PropertyStub;
static const JSEnumerateOp EnumerateStub = JS_EnumerateStub;
static const JSResolveOp   ResolveStub   = JS_ResolveStub;
static const ConvertOp     ConvertStub   = (ConvertOp)JS_ConvertStub;
static const JSFinalizeOp  FinalizeStub  = JS_FinalizeStub;

struct Class {
    const char          *name;
    uint32              flags;

    /* Mandatory non-null function pointer members. */
    PropertyOp          addProperty;
    PropertyOp          delProperty;
    PropertyOp          getProperty;
    PropertyOp          setProperty;
    JSEnumerateOp       enumerate;
    JSResolveOp         resolve;
    ConvertOp           convert;
    JSFinalizeOp        finalize;

    /* Optionally non-null members start here. */
    GetObjectOps        getObjectOps;
    CheckAccessOp       checkAccess;
    Native              call;
    Native              construct;
    JSXDRObjectOp       xdrObject;
    HasInstanceOp       hasInstance;
    JSMarkOp            mark;
    void                (*reserved0)(void);
};
JS_STATIC_ASSERT(offsetof(JSClass, name) == offsetof(Class, name));
JS_STATIC_ASSERT(offsetof(JSClass, flags) == offsetof(Class, flags));
JS_STATIC_ASSERT(offsetof(JSClass, addProperty) == offsetof(Class, addProperty));
JS_STATIC_ASSERT(offsetof(JSClass, delProperty) == offsetof(Class, delProperty));
JS_STATIC_ASSERT(offsetof(JSClass, getProperty) == offsetof(Class, getProperty));
JS_STATIC_ASSERT(offsetof(JSClass, setProperty) == offsetof(Class, setProperty));
JS_STATIC_ASSERT(offsetof(JSClass, enumerate) == offsetof(Class, enumerate));
JS_STATIC_ASSERT(offsetof(JSClass, resolve) == offsetof(Class, resolve));
JS_STATIC_ASSERT(offsetof(JSClass, convert) == offsetof(Class, convert));
JS_STATIC_ASSERT(offsetof(JSClass, finalize) == offsetof(Class, finalize));
JS_STATIC_ASSERT(offsetof(JSClass, getObjectOps) == offsetof(Class, getObjectOps));
JS_STATIC_ASSERT(offsetof(JSClass, checkAccess) == offsetof(Class, checkAccess));
JS_STATIC_ASSERT(offsetof(JSClass, call) == offsetof(Class, call));
JS_STATIC_ASSERT(offsetof(JSClass, construct) == offsetof(Class, construct));
JS_STATIC_ASSERT(offsetof(JSClass, xdrObject) == offsetof(Class, xdrObject));
JS_STATIC_ASSERT(offsetof(JSClass, hasInstance) == offsetof(Class, hasInstance));
JS_STATIC_ASSERT(offsetof(JSClass, mark) == offsetof(Class, mark));
JS_STATIC_ASSERT(offsetof(JSClass, reserved0) == offsetof(Class, reserved0));
JS_STATIC_ASSERT(sizeof(JSClass) == sizeof(Class));

struct ExtendedClass {
    Class               base;
    EqualityOp          equality;
    JSObjectOp          outerObject;
    JSObjectOp          innerObject;
    JSIteratorOp        iteratorObject;
    JSObjectOp          wrappedObject;          /* NB: infallible, null
                                                   returns are treated as
                                                   the original object */
    void                (*reserved0)(void);
    void                (*reserved1)(void);
    void                (*reserved2)(void);
};
JS_STATIC_ASSERT(offsetof(JSExtendedClass, base) == offsetof(ExtendedClass, base));
JS_STATIC_ASSERT(offsetof(JSExtendedClass, equality) == offsetof(ExtendedClass, equality));
JS_STATIC_ASSERT(offsetof(JSExtendedClass, outerObject) == offsetof(ExtendedClass, outerObject));
JS_STATIC_ASSERT(offsetof(JSExtendedClass, innerObject) == offsetof(ExtendedClass, innerObject));
JS_STATIC_ASSERT(offsetof(JSExtendedClass, iteratorObject) == offsetof(ExtendedClass, iteratorObject));
JS_STATIC_ASSERT(offsetof(JSExtendedClass, wrappedObject) == offsetof(ExtendedClass, wrappedObject));
JS_STATIC_ASSERT(offsetof(JSExtendedClass, reserved0) == offsetof(ExtendedClass, reserved0));
JS_STATIC_ASSERT(offsetof(JSExtendedClass, reserved1) == offsetof(ExtendedClass, reserved1));
JS_STATIC_ASSERT(offsetof(JSExtendedClass, reserved2) == offsetof(ExtendedClass, reserved2));
JS_STATIC_ASSERT(sizeof(JSExtendedClass) == sizeof(ExtendedClass));

struct PropertyDescriptor {
    JSObject     *obj;
    uintN        attrs;
    PropertyOp   getter;
    PropertyOp   setter;
    Value        value;
    uintN        shortid;
};
JS_STATIC_ASSERT(offsetof(JSPropertyDescriptor, obj) == offsetof(PropertyDescriptor, obj));
JS_STATIC_ASSERT(offsetof(JSPropertyDescriptor, attrs) == offsetof(PropertyDescriptor, attrs));
JS_STATIC_ASSERT(offsetof(JSPropertyDescriptor, getter) == offsetof(PropertyDescriptor, getter));
JS_STATIC_ASSERT(offsetof(JSPropertyDescriptor, setter) == offsetof(PropertyDescriptor, setter));
JS_STATIC_ASSERT(offsetof(JSPropertyDescriptor, value) == offsetof(PropertyDescriptor, value));
JS_STATIC_ASSERT(offsetof(JSPropertyDescriptor, shortid) == offsetof(PropertyDescriptor, shortid));
JS_STATIC_ASSERT(sizeof(JSPropertyDescriptor) == sizeof(PropertyDescriptor));

static JS_ALWAYS_INLINE JSClass *              Jsvalify(Class *c)                { return (JSClass *)c; }
static JS_ALWAYS_INLINE Class *                Valueify(JSClass *c)              { return (Class *)c; }
static JS_ALWAYS_INLINE JSExtendedClass *      Jsvalify(ExtendedClass *c)        { return (JSExtendedClass *)c; }
static JS_ALWAYS_INLINE ExtendedClass *        Valueify(JSExtendedClass *c)      { return (ExtendedClass *)c; }
static JS_ALWAYS_INLINE JSPropertyDescriptor * Jsvalify(PropertyDescriptor *p) { return (JSPropertyDescriptor *) p; }
static JS_ALWAYS_INLINE PropertyDescriptor *   Valueify(JSPropertyDescriptor *p) { return (PropertyDescriptor *) p; }

/******************************************************************************/

/*
 * In some cases (quickstubs) we want to take a value in whatever manner is
 * appropriate for the architecture and normalize to a const js::Value &. On
 * x64, passing a js::Value may cause the to unnecessarily be passed through
 * memory instead of registers, so jsval, which is a builtin uint64 is used.
 */
#if JS_BITS_PER_WORD == 32
typedef const js::Value *ValueArgType;

static JS_ALWAYS_INLINE const js::Value &
ValueArgToConstRef(const js::Value *arg)
{
    return *arg;
}

#elif JS_BITS_PER_WORD == 64
typedef js::Value        ValueArgType;

static JS_ALWAYS_INLINE const Value &
ValueArgToConstRef(const Value &v)
{
    return v;
}
#endif

}      /* namespace js */
#endif /* jsvalue_h__ */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS SIMD pseudo-module.
 * Specification matches polyfill:
 * https://github.com/johnmccutchan/ecmascript_simd/blob/master/src/ecmascript_simd.js
 * The objects float32x4 and int32x4 are installed on the SIMD pseudo-module.
 */

#include "builtin/SIMD.h"

#include "jsapi.h"
#include "jsfriendapi.h"

#include "builtin/TypedObject.h"
#include "js/Value.h"

#include "jsobjinlines.h"

using namespace js;

using mozilla::ArrayLength;
using mozilla::IsFinite;
using mozilla::IsNaN;

namespace js {
extern const JSFunctionSpec Float32x4Methods[];
extern const JSFunctionSpec Int32x4Methods[];
}

///////////////////////////////////////////////////////////////////////////
// X4

static const char *laneNames[] = {"lane 0", "lane 1", "lane 2", "lane3"};

template<typename Type32x4, int lane>
static bool GetX4Lane(JSContext *cx, unsigned argc, Value *vp) {
    typedef typename Type32x4::Elem Elem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.thisv().isObject() || !args.thisv().toObject().is<TypedObject>()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                             X4TypeDescr::class_.name, laneNames[lane],
                             InformalValueTypeName(args.thisv()));
        return false;
    }

    TypedObject &typedObj = args.thisv().toObject().as<TypedObject>();
    TypeDescr &descr = typedObj.typeDescr();
    if (descr.kind() != type::X4 || descr.as<X4TypeDescr>().type() != Type32x4::type) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                             X4TypeDescr::class_.name, laneNames[lane],
                             InformalValueTypeName(args.thisv()));
        return false;
    }

    MOZ_ASSERT(!typedObj.owner().isNeutered());
    Elem *data = reinterpret_cast<Elem *>(typedObj.typedMem());
    Type32x4::setReturn(args, data[lane]);
    return true;
}

#define LANE_ACCESSOR(type, lane) \
static bool type##Lane##lane(JSContext *cx, unsigned argc, Value *vp) { \
    return GetX4Lane<type, lane>(cx, argc, vp);\
}

#define FOUR_LANES_ACCESSOR(type) \
    LANE_ACCESSOR(type, 0); \
    LANE_ACCESSOR(type, 1); \
    LANE_ACCESSOR(type, 2); \
    LANE_ACCESSOR(type, 3);

    FOUR_LANES_ACCESSOR(Int32x4);
    FOUR_LANES_ACCESSOR(Float32x4);
#undef FOUR_LANES_ACCESSOR
#undef LANE_ACCESSOR

template<typename Type32x4>
static bool SignMask(JSContext *cx, unsigned argc, Value *vp) {
    typedef typename Type32x4::Elem Elem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.thisv().isObject() || !args.thisv().toObject().is<TypedObject>()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                             X4TypeDescr::class_.name, "signMask",
                             InformalValueTypeName(args.thisv()));
        return false;
    }

    TypedObject &typedObj = args.thisv().toObject().as<TypedObject>();
    TypeDescr &descr = typedObj.typeDescr();
    if (descr.kind() != type::X4 || descr.as<X4TypeDescr>().type() != Type32x4::type) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                             X4TypeDescr::class_.name, "signMask",
                             InformalValueTypeName(args.thisv()));
        return false;
    }

    MOZ_ASSERT(!typedObj.owner().isNeutered());
    Elem *data = reinterpret_cast<Elem *>(typedObj.typedMem());
    int32_t mx = data[0] < 0.0 ? 1 : 0;
    int32_t my = data[1] < 0.0 ? 1 : 0;
    int32_t mz = data[2] < 0.0 ? 1 : 0;
    int32_t mw = data[3] < 0.0 ? 1 : 0;
    int32_t result = mx | my << 1 | mz << 2 | mw << 3;
    args.rval().setInt32(result);
    return true;
}

#define SIGN_MASK(type) \
static bool type##SignMask(JSContext *cx, unsigned argc, Value *vp) { \
    return SignMask<Int32x4>(cx, argc, vp); \
}
    SIGN_MASK(Float32x4);
    SIGN_MASK(Int32x4);
#undef SIGN_MASK

const Class X4TypeDescr::class_ = {
    "X4",
    JSCLASS_HAS_RESERVED_SLOTS(JS_DESCR_SLOTS),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,             /* finalize    */
    call,                /* call        */
    nullptr,             /* hasInstance */
    nullptr,             /* construct   */
    nullptr
};

// These classes just exist to group together various properties and so on.
namespace js {
class Int32x4Defn {
  public:
    static const X4TypeDescr::Type type = X4TypeDescr::TYPE_INT32;
    static const JSFunctionSpec TypeDescriptorMethods[];
    static const JSPropertySpec TypedObjectProperties[];
    static const JSFunctionSpec TypedObjectMethods[];
};
class Float32x4Defn {
  public:
    static const X4TypeDescr::Type type = X4TypeDescr::TYPE_FLOAT32;
    static const JSFunctionSpec TypeDescriptorMethods[];
    static const JSPropertySpec TypedObjectProperties[];
    static const JSFunctionSpec TypedObjectMethods[];
};
} // namespace js

const JSFunctionSpec js::Float32x4Defn::TypeDescriptorMethods[] = {
    JS_SELF_HOSTED_FN("toSource", "DescrToSource", 0, 0),
    JS_SELF_HOSTED_FN("array", "ArrayShorthand", 1, 0),
    JS_SELF_HOSTED_FN("equivalent", "TypeDescrEquivalent", 1, 0),
    JS_FS_END
};

const JSPropertySpec js::Float32x4Defn::TypedObjectProperties[] = {
    JS_PSG("x", Float32x4Lane0, JSPROP_PERMANENT),
    JS_PSG("y", Float32x4Lane1, JSPROP_PERMANENT),
    JS_PSG("z", Float32x4Lane2, JSPROP_PERMANENT),
    JS_PSG("w", Float32x4Lane3, JSPROP_PERMANENT),
    JS_PSG("signMask", Float32x4SignMask, JSPROP_PERMANENT),
    JS_PS_END
};

const JSFunctionSpec js::Float32x4Defn::TypedObjectMethods[] = {
    JS_SELF_HOSTED_FN("toSource", "X4ToSource", 0, 0),
    JS_FS_END
};

const JSFunctionSpec js::Int32x4Defn::TypeDescriptorMethods[] = {
    JS_SELF_HOSTED_FN("toSource", "DescrToSource", 0, 0),
    JS_SELF_HOSTED_FN("array", "ArrayShorthand", 1, 0),
    JS_SELF_HOSTED_FN("equivalent", "TypeDescrEquivalent", 1, 0),
    JS_FS_END,
};

const JSPropertySpec js::Int32x4Defn::TypedObjectProperties[] = {
    JS_PSG("x", Int32x4Lane0, JSPROP_PERMANENT),
    JS_PSG("y", Int32x4Lane1, JSPROP_PERMANENT),
    JS_PSG("z", Int32x4Lane2, JSPROP_PERMANENT),
    JS_PSG("w", Int32x4Lane3, JSPROP_PERMANENT),
    JS_PSG("signMask", Int32x4SignMask, JSPROP_PERMANENT),
    JS_PS_END
};

const JSFunctionSpec js::Int32x4Defn::TypedObjectMethods[] = {
    JS_SELF_HOSTED_FN("toSource", "X4ToSource", 0, 0),
    JS_FS_END
};

template<typename T>
static JSObject *
CreateX4Class(JSContext *cx,
              Handle<GlobalObject*> global,
              HandlePropertyName stringRepr)
{
    const X4TypeDescr::Type type = T::type;

    RootedObject funcProto(cx, global->getOrCreateFunctionPrototype(cx));
    if (!funcProto)
        return nullptr;

    // Create type constructor itself and initialize its reserved slots.

    Rooted<X4TypeDescr*> x4(cx);
    x4 = NewObjectWithProto<X4TypeDescr>(cx, funcProto, global, TenuredObject);
    if (!x4)
        return nullptr;

    x4->initReservedSlot(JS_DESCR_SLOT_KIND, Int32Value(type::X4));
    x4->initReservedSlot(JS_DESCR_SLOT_STRING_REPR, StringValue(stringRepr));
    x4->initReservedSlot(JS_DESCR_SLOT_ALIGNMENT, Int32Value(X4TypeDescr::size(type)));
    x4->initReservedSlot(JS_DESCR_SLOT_SIZE, Int32Value(X4TypeDescr::alignment(type)));
    x4->initReservedSlot(JS_DESCR_SLOT_OPAQUE, BooleanValue(false));
    x4->initReservedSlot(JS_DESCR_SLOT_TYPE, Int32Value(T::type));

    if (!CreateUserSizeAndAlignmentProperties(cx, x4))
        return nullptr;

    // Create prototype property, which inherits from Object.prototype.

    RootedObject objProto(cx, global->getOrCreateObjectPrototype(cx));
    if (!objProto)
        return nullptr;
    Rooted<TypedProto*> proto(cx);
    proto = NewObjectWithProto<TypedProto>(cx, objProto, nullptr, TenuredObject);
    if (!proto)
        return nullptr;
    proto->initTypeDescrSlot(*x4);
    x4->initReservedSlot(JS_DESCR_SLOT_TYPROTO, ObjectValue(*proto));

    // Link constructor to prototype and install properties.

    if (!JS_DefineFunctions(cx, x4, T::TypeDescriptorMethods))
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, x4, proto) ||
        !DefinePropertiesAndBrand(cx, proto, T::TypedObjectProperties,
                                  T::TypedObjectMethods))
    {
        return nullptr;
    }

    return x4;
}

bool
X4TypeDescr::call(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    const unsigned LANES = 4;

    if (args.length() < LANES) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             args.callee().getClass()->name, "3", "s");
        return false;
    }

    double values[LANES];
    for (unsigned i = 0; i < LANES; i++) {
        if (!ToNumber(cx, args[i], &values[i]))
            return false;
    }

    Rooted<X4TypeDescr*> descr(cx, &args.callee().as<X4TypeDescr>());
    Rooted<TypedObject*> result(cx, TypedObject::createZeroed(cx, descr, 0));
    if (!result)
        return false;

    MOZ_ASSERT(!result->owner().isNeutered());
    switch (descr->type()) {
#define STORE_LANES(_constant, _type, _name)                                  \
      case _constant:                                                         \
      {                                                                       \
        _type *mem = reinterpret_cast<_type*>(result->typedMem());            \
        for (unsigned i = 0; i < LANES; i++)                                  \
            mem[i] = ConvertScalar<_type>(values[i]);                         \
        break;                                                                \
      }
      JS_FOR_EACH_X4_TYPE_REPR(STORE_LANES)
#undef STORE_LANES
    }
    args.rval().setObject(*result);
    return true;
}

///////////////////////////////////////////////////////////////////////////
// SIMD class

const Class SIMDObject::class_ = {
        "SIMD",
        JSCLASS_HAS_CACHED_PROTO(JSProto_SIMD),
        JS_PropertyStub,         /* addProperty */
        JS_DeletePropertyStub,   /* delProperty */
        JS_PropertyStub,         /* getProperty */
        JS_StrictPropertyStub,   /* setProperty */
        JS_EnumerateStub,
        JS_ResolveStub,
        JS_ConvertStub,
        nullptr,             /* finalize    */
        nullptr,             /* call        */
        nullptr,             /* hasInstance */
        nullptr,             /* construct   */
        nullptr
};

JSObject *
SIMDObject::initClass(JSContext *cx, Handle<GlobalObject *> global)
{
    // SIMD relies on having the TypedObject module initialized.
    // In particular, the self-hosted code for array() wants
    // to be able to call GetTypedObjectModule(). It is NOT necessary
    // to install the TypedObjectModule global, but at the moment
    // those two things are not separable.
    if (!global->getOrCreateTypedObjectModule(cx))
        return nullptr;

    // Create SIMD Object.
    RootedObject objProto(cx, global->getOrCreateObjectPrototype(cx));
    if (!objProto)
        return nullptr;
    RootedObject SIMD(cx, NewObjectWithGivenProto(cx, &SIMDObject::class_, objProto,
                                                  global, SingletonObject));
    if (!SIMD)
        return nullptr;

    // float32x4
    RootedObject float32x4Object(cx);
    float32x4Object = CreateX4Class<Float32x4Defn>(cx, global,
                                                   cx->names().float32x4);
    if (!float32x4Object)
        return nullptr;

    // Define float32x4 functions and install as a property of the SIMD object.
    RootedValue float32x4Value(cx, ObjectValue(*float32x4Object));
    if (!JS_DefineFunctions(cx, float32x4Object, Float32x4Methods) ||
        !JSObject::defineProperty(cx, SIMD, cx->names().float32x4,
                                  float32x4Value, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
    {
        return nullptr;
    }

    // int32x4
    RootedObject int32x4Object(cx);
    int32x4Object = CreateX4Class<Int32x4Defn>(cx, global,
                                               cx->names().int32x4);
    if (!int32x4Object)
        return nullptr;

    // Define int32x4 functions and install as a property of the SIMD object.
    RootedValue int32x4Value(cx, ObjectValue(*int32x4Object));
    if (!JS_DefineFunctions(cx, int32x4Object, Int32x4Methods) ||
        !JSObject::defineProperty(cx, SIMD, cx->names().int32x4,
                                  int32x4Value, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
    {
        return nullptr;
    }

    RootedValue SIMDValue(cx, ObjectValue(*SIMD));

    // Everything is set up, install SIMD on the global object.
    if (!JSObject::defineProperty(cx, global, cx->names().SIMD, SIMDValue, nullptr, nullptr, 0))
        return nullptr;

    global->setConstructor(JSProto_SIMD, SIMDValue);
    global->setFloat32x4TypeDescr(*float32x4Object);
    global->setInt32x4TypeDescr(*int32x4Object);
    return SIMD;
}

JSObject *
js_InitSIMDClass(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->is<GlobalObject>());
    Rooted<GlobalObject *> global(cx, &obj->as<GlobalObject>());
    return SIMDObject::initClass(cx, global);
}

template<typename V>
static bool
IsVectorObject(HandleValue v)
{
    if (!v.isObject())
        return false;

    JSObject &obj = v.toObject();
    if (!obj.is<TypedObject>())
        return false;

    TypeDescr &typeRepr = obj.as<TypedObject>().typeDescr();
    if (typeRepr.kind() != type::X4)
        return false;

    return typeRepr.as<X4TypeDescr>().type() == V::type;
}

template<typename Elem>
static Elem
TypedObjectMemory(HandleValue v)
{
    TypedObject &obj = v.toObject().as<TypedObject>();
    MOZ_ASSERT(!obj.owner().isNeutered());
    return reinterpret_cast<Elem>(obj.typedMem());
}

template<typename V>
JSObject *
js::Create(JSContext *cx, typename V::Elem *data)
{
    typedef typename V::Elem Elem;
    Rooted<TypeDescr*> typeDescr(cx, &V::GetTypeDescr(*cx->global()));
    JS_ASSERT(typeDescr);

    Rooted<TypedObject *> result(cx, TypedObject::createZeroed(cx, typeDescr, 0));
    if (!result)
        return nullptr;

    MOZ_ASSERT(!result->owner().isNeutered());
    Elem *resultMem = reinterpret_cast<Elem *>(result->typedMem());
    memcpy(resultMem, data, sizeof(Elem) * V::lanes);
    return result;
}

template JSObject *js::Create<Float32x4>(JSContext *cx, Float32x4::Elem *data);
template JSObject *js::Create<Int32x4>(JSContext *cx, Int32x4::Elem *data);

namespace js {
template<typename T, typename V>
struct Abs {
    static inline T apply(T x, T zero) { return V::toType(x < 0 ? -1 * x : x); }
};
template<typename T, typename V>
struct Neg {
    static inline T apply(T x, T zero) { return V::toType(-1 * x); }
};
template<typename T, typename V>
struct Not {
    static inline T apply(T x, T zero) { return V::toType(~x); }
};
template<typename T, typename V>
struct Rec {
    static inline T apply(T x, T zero) { return V::toType(1 / x); }
};
template<typename T, typename V>
struct RecSqrt {
    static inline T apply(T x, T zero) { return V::toType(1 / sqrt(x)); }
};
template<typename T, typename V>
struct Sqrt {
    static inline T apply(T x, T zero) { return V::toType(sqrt(x)); }
};
template<typename T, typename V>
struct Add {
    static inline T apply(T l, T r) { return V::toType(l + r); }
};
template<typename T, typename V>
struct Sub {
    static inline T apply(T l, T r) { return V::toType(l - r); }
};
template<typename T, typename V>
struct Div {
    static inline T apply(T l, T r) { return V::toType(l / r); }
};
template<typename T, typename V>
struct Mul {
    static inline T apply(T l, T r) { return V::toType(l * r); }
};
template<typename T, typename V>
struct Minimum {
    static inline T apply(T l, T r) { return V::toType(l < r ? l : r); }
};
template<typename T, typename V>
struct Maximum {
    static inline T apply(T l, T r) { return V::toType(l > r ? l : r); }
};
template<typename T, typename V>
struct LessThan {
    static inline T apply(T l, T r) { return V::toType(l < r ? 0xFFFFFFFF : 0x0); }
};
template<typename T, typename V>
struct LessThanOrEqual {
    static inline T apply(T l, T r) { return V::toType(l <= r ? 0xFFFFFFFF : 0x0); }
};
template<typename T, typename V>
struct GreaterThan {
    static inline T apply(T l, T r) { return V::toType(l > r ? 0xFFFFFFFF : 0x0); }
};
template<typename T, typename V>
struct GreaterThanOrEqual {
    static inline T apply(T l, T r) { return V::toType(l >= r ? 0xFFFFFFFF : 0x0); }
};
template<typename T, typename V>
struct Equal {
    static inline T apply(T l, T r) { return V::toType(l == r ? 0xFFFFFFFF : 0x0); }
};
template<typename T, typename V>
struct NotEqual {
    static inline T apply(T l, T r) { return V::toType(l != r ? 0xFFFFFFFF : 0x0); }
};
template<typename T, typename V>
struct Xor {
    static inline T apply(T l, T r) { return V::toType(l ^ r); }
};
template<typename T, typename V>
struct And {
    static inline T apply(T l, T r) { return V::toType(l & r); }
};
template<typename T, typename V>
struct Or {
    static inline T apply(T l, T r) { return V::toType(l | r); }
};
template<typename T, typename V>
struct Scale {
    static inline T apply(int32_t lane, T scalar, T x) { return V::toType(scalar * x); }
};
template<typename T, typename V>
struct WithX {
    static inline T apply(int32_t lane, T scalar, T x) { return V::toType(lane == 0 ? scalar : x); }
};
template<typename T, typename V>
struct WithY {
    static inline T apply(int32_t lane, T scalar, T x) { return V::toType(lane == 1 ? scalar : x); }
};
template<typename T, typename V>
struct WithZ {
    static inline T apply(int32_t lane, T scalar, T x) { return V::toType(lane == 2 ? scalar : x); }
};
template<typename T, typename V>
struct WithW {
    static inline T apply(int32_t lane, T scalar, T x) { return V::toType(lane == 3 ? scalar : x); }
};
template<typename T, typename V>
struct WithFlagX {
    static inline T apply(T l, T f, T x) { return V::toType(l == 0 ? (f ? 0xFFFFFFFF : 0x0) : x); }
};
template<typename T, typename V>
struct WithFlagY {
    static inline T apply(T l, T f, T x) { return V::toType(l == 1 ? (f ? 0xFFFFFFFF : 0x0) : x); }
};
template<typename T, typename V>
struct WithFlagZ {
    static inline T apply(T l, T f, T x) { return V::toType(l == 2 ? (f ? 0xFFFFFFFF : 0x0) : x); }
};
template<typename T, typename V>
struct WithFlagW {
    static inline T apply(T l, T f, T x) { return V::toType(l == 3 ? (f ? 0xFFFFFFFF : 0x0) : x); }
};
template<typename T, typename V>
struct Shuffle {
    static inline int32_t apply(int32_t l, int32_t mask) { return V::toType((mask >> l) & 0x3); }
};
struct ShiftLeft {
    static inline int32_t apply(int32_t v, int32_t bits) { return v << bits; }
};
struct ShiftRight {
    static inline int32_t apply(int32_t v, int32_t bits) { return v >> bits; }
};
struct ShiftRightLogical {
    static inline int32_t apply(int32_t v, int32_t bits) { return uint32_t(v) >> (bits & 31); }
};
}

static inline bool
ErrorBadArgs(JSContext *cx)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
    return false;
}

// Coerces the inputs of type In to the type Coercion, apply the operator Op
// and converts the result to the type Out.
template<typename In, typename Coercion, typename Op, typename Out>
static bool
CoercedFunc(JSContext *cx, unsigned argc, Value *vp)
{
    typedef typename Coercion::Elem CoercionElem;
    typedef typename Out::Elem RetElem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1 && args.length() != 2)
        return ErrorBadArgs(cx);

    CoercionElem result[Coercion::lanes];
    if (args.length() == 1) {
        if (!IsVectorObject<In>(args[0]))
            return ErrorBadArgs(cx);

        CoercionElem *val = TypedObjectMemory<CoercionElem *>(args[0]);
        for (unsigned i = 0; i < Coercion::lanes; i++)
            result[i] = Op::apply(val[i], 0);
    } else {
        JS_ASSERT(args.length() == 2);
        if (!IsVectorObject<In>(args[0]) || !IsVectorObject<In>(args[1]))
            return ErrorBadArgs(cx);

        CoercionElem *left = TypedObjectMemory<CoercionElem *>(args[0]);
        CoercionElem *right = TypedObjectMemory<CoercionElem *>(args[1]);
        for (unsigned i = 0; i < Coercion::lanes; i++)
            result[i] = Op::apply(left[i], right[i]);
    }

    RetElem *coercedResult = reinterpret_cast<RetElem *>(result);
    RootedObject obj(cx, Create<Out>(cx, coercedResult));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

// Same as above, with Coercion == Out
template<typename In, typename Op, typename Out>
static bool
Func(JSContext *cx, unsigned argc, Value *vp)
{
    return CoercedFunc<In, Out, Op, Out>(cx, argc, vp);
}

template<typename V, typename OpWith, typename Vret>
static bool
FuncWith(JSContext *cx, unsigned argc, Value *vp)
{
    typedef typename V::Elem Elem;
    typedef typename Vret::Elem RetElem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 2 || !IsVectorObject<V>(args[0]) ||
        (!args[1].isNumber() && !args[1].isBoolean()))
    {
        return ErrorBadArgs(cx);
    }

    Elem *val = TypedObjectMemory<Elem *>(args[0]);
    RetElem result[Vret::lanes];

    if (args[1].isNumber()) {
        Elem withAsNumber;
        if (!Vret::toType(cx, args[1], &withAsNumber))
            return false;
        for (unsigned i = 0; i < Vret::lanes; i++)
            result[i] = OpWith::apply(i, withAsNumber, val[i]);
    } else {
        JS_ASSERT(args[1].isBoolean());
        bool withAsBool = args[1].toBoolean();
        for (unsigned i = 0; i < Vret::lanes; i++)
            result[i] = OpWith::apply(i, withAsBool, val[i]);
    }

    RootedObject obj(cx, Create<Vret>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

template<typename V, typename OpShuffle, typename Vret>
static bool
FuncShuffle(JSContext *cx, unsigned argc, Value *vp)
{
    typedef typename V::Elem Elem;
    typedef typename Vret::Elem RetElem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 2 && args.length() != 3)
        return ErrorBadArgs(cx);

    RetElem result[Vret::lanes];
    if (args.length() == 2) {
        if (!IsVectorObject<V>(args[0]) || !args[1].isNumber())
            return ErrorBadArgs(cx);

        Elem *val = TypedObjectMemory<Elem *>(args[0]);;
        Elem arg1;
        if (!Vret::toType(cx, args[1], &arg1))
            return false;

        for (unsigned i = 0; i < Vret::lanes; i++)
            result[i] = val[OpShuffle::apply(i * 2, arg1)];
    } else {
        JS_ASSERT(args.length() == 3);
        if (!IsVectorObject<V>(args[0]) || !IsVectorObject<V>(args[1]) || !args[2].isNumber())
            return ErrorBadArgs(cx);

        Elem *val1 = TypedObjectMemory<Elem *>(args[0]);
        Elem *val2 = TypedObjectMemory<Elem *>(args[1]);
        Elem arg2;
        if (!Vret::toType(cx, args[2], &arg2))
            return false;

        unsigned i = 0;
        for (; i < Vret::lanes / 2; i++)
            result[i] = val1[OpShuffle::apply(i * 2, arg2)];
        for (; i < Vret::lanes; i++)
            result[i] = val2[OpShuffle::apply(i * 2, arg2)];
    }

    RootedObject obj(cx, Create<Vret>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

template<typename Op>
static bool
Int32x4BinaryScalar(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 2)
        return ErrorBadArgs(cx);

    int32_t result[4];
    if (!IsVectorObject<Int32x4>(args[0]) || !args[1].isNumber())
        return ErrorBadArgs(cx);

    int32_t *val = TypedObjectMemory<int32_t *>(args[0]);;
    int32_t bits;
    if (!ToInt32(cx, args[1], &bits))
        return false;

    for (unsigned i = 0; i < 4; i++)
        result[i] = Op::apply(val[i], bits);

    RootedObject obj(cx, Create<Int32x4>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

template<typename V, typename Vret>
static bool
FuncConvert(JSContext *cx, unsigned argc, Value *vp)
{
    typedef typename V::Elem Elem;
    typedef typename Vret::Elem RetElem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1 || !IsVectorObject<V>(args[0]))
        return ErrorBadArgs(cx);

    Elem *val = TypedObjectMemory<Elem *>(args[0]);
    RetElem result[Vret::lanes];
    for (unsigned i = 0; i < Vret::lanes; i++)
        result[i] = RetElem(val[i]);

    RootedObject obj(cx, Create<Vret>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

template<typename V, typename Vret>
static bool
FuncConvertBits(JSContext *cx, unsigned argc, Value *vp)
{
    typedef typename Vret::Elem RetElem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1 || !IsVectorObject<V>(args[0]))
        return ErrorBadArgs(cx);

    RetElem *val = TypedObjectMemory<RetElem *>(args[0]);
    RootedObject obj(cx, Create<Vret>(cx, val));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

template<typename Vret>
static bool
FuncZero(JSContext *cx, unsigned argc, Value *vp)
{
    typedef typename Vret::Elem RetElem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 0)
        return ErrorBadArgs(cx);

    RetElem result[Vret::lanes];
    for (unsigned i = 0; i < Vret::lanes; i++)
        result[i] = RetElem(0);

    RootedObject obj(cx, Create<Vret>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

template<typename Vret>
static bool
FuncSplat(JSContext *cx, unsigned argc, Value *vp)
{
    typedef typename Vret::Elem RetElem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1 || !args[0].isNumber())
        return ErrorBadArgs(cx);

    RetElem arg;
    if (!Vret::toType(cx, args[0], &arg))
        return false;

    RetElem result[Vret::lanes];
    for (unsigned i = 0; i < Vret::lanes; i++)
        result[i] = arg;

    RootedObject obj(cx, Create<Vret>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
Int32x4Bool(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 4 ||
        !args[0].isBoolean() || !args[1].isBoolean() ||
        !args[2].isBoolean() || !args[3].isBoolean())
    {
        return ErrorBadArgs(cx);
    }

    int32_t result[Int32x4::lanes];
    for (unsigned i = 0; i < Int32x4::lanes; i++)
        result[i] = args[i].toBoolean() ? 0xFFFFFFFF : 0x0;

    RootedObject obj(cx, Create<Int32x4>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
Float32x4Clamp(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 3 || !IsVectorObject<Float32x4>(args[0]) ||
        !IsVectorObject<Float32x4>(args[1]) || !IsVectorObject<Float32x4>(args[2]))
    {
        return ErrorBadArgs(cx);
    }

    float *val = TypedObjectMemory<float *>(args[0]);
    float *lowerLimit = TypedObjectMemory<float *>(args[1]);
    float *upperLimit = TypedObjectMemory<float *>(args[2]);

    float result[Float32x4::lanes];
    for (unsigned i = 0; i < Float32x4::lanes; i++) {
        result[i] = val[i] < lowerLimit[i] ? lowerLimit[i] : val[i];
        result[i] = result[i] > upperLimit[i] ? upperLimit[i] : result[i];
    }

    RootedObject obj(cx, Create<Float32x4>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
Int32x4Select(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 3 || !IsVectorObject<Int32x4>(args[0]) ||
        !IsVectorObject<Float32x4>(args[1]) || !IsVectorObject<Float32x4>(args[2]))
    {
        return ErrorBadArgs(cx);
    }

    int32_t *val = TypedObjectMemory<int32_t *>(args[0]);
    int32_t *tv = TypedObjectMemory<int32_t *>(args[1]);
    int32_t *fv = TypedObjectMemory<int32_t *>(args[2]);

    int32_t tr[Int32x4::lanes];
    for (unsigned i = 0; i < Int32x4::lanes; i++)
        tr[i] = And<int32_t, Int32x4>::apply(val[i], tv[i]);

    int32_t fr[Int32x4::lanes];
    for (unsigned i = 0; i < Int32x4::lanes; i++)
        fr[i] = And<int32_t, Int32x4>::apply(Not<int32_t, Int32x4>::apply(val[i], 0), fv[i]);

    int32_t orInt[Int32x4::lanes];
    for (unsigned i = 0; i < Int32x4::lanes; i++)
        orInt[i] = Or<int32_t, Int32x4>::apply(tr[i], fr[i]);

    float *result = reinterpret_cast<float *>(orInt);
    RootedObject obj(cx, Create<Float32x4>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

#define DEFINE_SIMD_FLOAT32X4_FUNCTION(Name, Func, Operands, Flags, MIRId)     \
bool                                                                           \
js::simd_float32x4_##Name(JSContext *cx, unsigned argc, Value *vp)             \
{                                                                              \
    return Func(cx, argc, vp);                                                 \
}
FLOAT32X4_FUNCTION_LIST(DEFINE_SIMD_FLOAT32X4_FUNCTION)
#undef DEFINE_SIMD_FLOAT32x4_FUNCTION

#define DEFINE_SIMD_INT32X4_FUNCTION(Name, Func, Operands, Flags, MIRId)       \
bool                                                                           \
js::simd_int32x4_##Name(JSContext *cx, unsigned argc, Value *vp)               \
{                                                                              \
    return Func(cx, argc, vp);                                                 \
}
INT32X4_FUNCTION_LIST(DEFINE_SIMD_INT32X4_FUNCTION)
#undef DEFINE_SIMD_INT32X4_FUNCTION

const JSFunctionSpec js::Float32x4Methods[] = {
#define SIMD_FLOAT32X4_FUNCTION_ITEM(Name, Func, Operands, Flags, MIRId)       \
        JS_FN(#Name, js::simd_float32x4_##Name, Operands, Flags),
        FLOAT32X4_FUNCTION_LIST(SIMD_FLOAT32X4_FUNCTION_ITEM)
#undef SIMD_FLOAT32x4_FUNCTION_ITEM
        JS_FS_END
};

const JSFunctionSpec js::Int32x4Methods[] = {
#define SIMD_INT32X4_FUNCTION_ITEM(Name, Func, Operands, Flags, MIRId)         \
        JS_FN(#Name, js::simd_int32x4_##Name, Operands, Flags),
        INT32X4_FUNCTION_LIST(SIMD_INT32X4_FUNCTION_ITEM)
#undef SIMD_INT32X4_FUNCTION_ITEM
        JS_FS_END
};

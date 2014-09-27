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
using mozilla::FloorLog2;

namespace js {
extern const JSFunctionSpec Float32x4Methods[];
extern const JSFunctionSpec Int32x4Methods[];
}

///////////////////////////////////////////////////////////////////////////
// SIMD

static const char *laneNames[] = {"lane 0", "lane 1", "lane 2", "lane3"};

static bool
CheckVectorObject(HandleValue v, SimdTypeDescr::Type expectedType)
{
    if (!v.isObject())
        return false;

    JSObject &obj = v.toObject();
    if (!obj.is<TypedObject>())
        return false;

    TypeDescr &typeRepr = obj.as<TypedObject>().typeDescr();
    if (typeRepr.kind() != type::Simd)
        return false;

    return typeRepr.as<SimdTypeDescr>().type() == expectedType;
}

template<class V>
bool
js::IsVectorObject(HandleValue v)
{
    return CheckVectorObject(v, V::type);
}

template bool js::IsVectorObject<Int32x4>(HandleValue v);
template bool js::IsVectorObject<Float32x4>(HandleValue v);

template<typename V>
bool
js::ToSimdConstant(JSContext *cx, HandleValue v, jit::SimdConstant *out)
{
    typedef typename V::Elem Elem;
    if (!IsVectorObject<V>(v)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_SIMD_NOT_A_VECTOR);
        return false;
    }

    Elem *mem = reinterpret_cast<Elem *>(v.toObject().as<TypedObject>().typedMem());
    *out = jit::SimdConstant::CreateX4(mem);
    return true;
}

template bool js::ToSimdConstant<Int32x4>(JSContext *cx, HandleValue v, jit::SimdConstant *out);
template bool js::ToSimdConstant<Float32x4>(JSContext *cx, HandleValue v, jit::SimdConstant *out);

template<typename Elem>
static Elem
TypedObjectMemory(HandleValue v)
{
    OutlineTypedObject &obj = v.toObject().as<OutlineTypedObject>();
    return reinterpret_cast<Elem>(obj.typedMem());
}

template<typename SimdType, int lane>
static bool GetSimdLane(JSContext *cx, unsigned argc, Value *vp)
{
    typedef typename SimdType::Elem Elem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (!IsVectorObject<SimdType>(args.thisv())) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                             SimdTypeDescr::class_.name, laneNames[lane],
                             InformalValueTypeName(args.thisv()));
        return false;
    }

    Elem *data = TypedObjectMemory<Elem *>(args.thisv());
    SimdType::setReturn(args, data[lane]);
    return true;
}

#define LANE_ACCESSOR(type, lane) \
static bool type##Lane##lane(JSContext *cx, unsigned argc, Value *vp) { \
    return GetSimdLane<type, lane>(cx, argc, vp);\
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

template<typename SimdType>
static bool SignMask(JSContext *cx, unsigned argc, Value *vp)
{
    typedef typename SimdType::Elem Elem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.thisv().isObject() || !args.thisv().toObject().is<TypedObject>()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                             SimdTypeDescr::class_.name, "signMask",
                             InformalValueTypeName(args.thisv()));
        return false;
    }

    OutlineTypedObject &typedObj = args.thisv().toObject().as<OutlineTypedObject>();
    TypeDescr &descr = typedObj.typeDescr();
    if (descr.kind() != type::Simd || descr.as<SimdTypeDescr>().type() != SimdType::type) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                             SimdTypeDescr::class_.name, "signMask",
                             InformalValueTypeName(args.thisv()));
        return false;
    }

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

const Class SimdTypeDescr::class_ = {
    "SIMD",
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
    static const SimdTypeDescr::Type type = SimdTypeDescr::TYPE_INT32;
    static const JSFunctionSpec TypeDescriptorMethods[];
    static const JSPropertySpec TypedObjectProperties[];
    static const JSFunctionSpec TypedObjectMethods[];
};
class Float32x4Defn {
  public:
    static const SimdTypeDescr::Type type = SimdTypeDescr::TYPE_FLOAT32;
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
    JS_SELF_HOSTED_FN("toSource", "SimdToSource", 0, 0),
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
    JS_SELF_HOSTED_FN("toSource", "SimdToSource", 0, 0),
    JS_FS_END
};

template<typename T>
static JSObject *
CreateSimdClass(JSContext *cx,
              Handle<GlobalObject*> global,
              HandlePropertyName stringRepr)
{
    const SimdTypeDescr::Type type = T::type;

    RootedObject funcProto(cx, global->getOrCreateFunctionPrototype(cx));
    if (!funcProto)
        return nullptr;

    // Create type constructor itself and initialize its reserved slots.

    Rooted<SimdTypeDescr*> typeDescr(cx);
    typeDescr = NewObjectWithProto<SimdTypeDescr>(cx, funcProto, global, TenuredObject);
    if (!typeDescr)
        return nullptr;

    typeDescr->initReservedSlot(JS_DESCR_SLOT_KIND, Int32Value(type::Simd));
    typeDescr->initReservedSlot(JS_DESCR_SLOT_STRING_REPR, StringValue(stringRepr));
    typeDescr->initReservedSlot(JS_DESCR_SLOT_ALIGNMENT, Int32Value(SimdTypeDescr::alignment(type)));
    typeDescr->initReservedSlot(JS_DESCR_SLOT_SIZE, Int32Value(SimdTypeDescr::size(type)));
    typeDescr->initReservedSlot(JS_DESCR_SLOT_OPAQUE, BooleanValue(false));
    typeDescr->initReservedSlot(JS_DESCR_SLOT_TYPE, Int32Value(T::type));

    if (!CreateUserSizeAndAlignmentProperties(cx, typeDescr))
        return nullptr;

    // Create prototype property, which inherits from Object.prototype.

    RootedObject objProto(cx, global->getOrCreateObjectPrototype(cx));
    if (!objProto)
        return nullptr;
    Rooted<TypedProto*> proto(cx);
    proto = NewObjectWithProto<TypedProto>(cx, objProto, nullptr, TenuredObject);
    if (!proto)
        return nullptr;
    proto->initTypeDescrSlot(*typeDescr);
    typeDescr->initReservedSlot(JS_DESCR_SLOT_TYPROTO, ObjectValue(*proto));

    // Link constructor to prototype and install properties.

    if (!JS_DefineFunctions(cx, typeDescr, T::TypeDescriptorMethods))
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, typeDescr, proto) ||
        !DefinePropertiesAndFunctions(cx, proto, T::TypedObjectProperties,
                                      T::TypedObjectMethods))
    {
        return nullptr;
    }

    return typeDescr;
}

bool
SimdTypeDescr::call(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    const unsigned LANES = 4;

    Rooted<SimdTypeDescr*> descr(cx, &args.callee().as<SimdTypeDescr>());
    if (args.length() == 1) {
        // SIMD type used as a coercion
        if (!CheckVectorObject(args[0], descr->type())) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_SIMD_NOT_A_VECTOR);
            return false;
        }

        args.rval().setObject(args[0].toObject());
        return true;
    }

    if (args.length() < LANES) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             args.callee().getClass()->name, "3", "s");
        return false;
    }

    Rooted<TypedObject*> result(cx, OutlineTypedObject::createZeroed(cx, descr, 0));
    if (!result)
        return false;

    switch (descr->type()) {
      case SimdTypeDescr::TYPE_INT32: {
        int32_t *mem = reinterpret_cast<int32_t*>(result->typedMem());
        for (unsigned i = 0; i < 4; i++) {
            if (!ToInt32(cx, args[i], &mem[i]))
                return false;
        }
        break;
      }
      case SimdTypeDescr::TYPE_FLOAT32: {
        float *mem = reinterpret_cast<float*>(result->typedMem());
        for (unsigned i = 0; i < 4; i++) {
            if (!RoundFloat32(cx, args[i], &mem[i]))
                return false;
        }
        break;
      }
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

static JSConstIntegerSpec SHUFFLE_MASKS[] = {
    {"XXXX", 0x0},
    {"XXXY", 0x40},
    {"XXXZ", 0x80},
    {"XXXW", 0xC0},
    {"XXYX", 0x10},
    {"XXYY", 0x50},
    {"XXYZ", 0x90},
    {"XXYW", 0xD0},
    {"XXZX", 0x20},
    {"XXZY", 0x60},
    {"XXZZ", 0xA0},
    {"XXZW", 0xE0},
    {"XXWX", 0x30},
    {"XXWY", 0x70},
    {"XXWZ", 0xB0},
    {"XXWW", 0xF0},
    {"XYXX", 0x4},
    {"XYXY", 0x44},
    {"XYXZ", 0x84},
    {"XYXW", 0xC4},
    {"XYYX", 0x14},
    {"XYYY", 0x54},
    {"XYYZ", 0x94},
    {"XYYW", 0xD4},
    {"XYZX", 0x24},
    {"XYZY", 0x64},
    {"XYZZ", 0xA4},
    {"XYZW", 0xE4},
    {"XYWX", 0x34},
    {"XYWY", 0x74},
    {"XYWZ", 0xB4},
    {"XYWW", 0xF4},
    {"XZXX", 0x8},
    {"XZXY", 0x48},
    {"XZXZ", 0x88},
    {"XZXW", 0xC8},
    {"XZYX", 0x18},
    {"XZYY", 0x58},
    {"XZYZ", 0x98},
    {"XZYW", 0xD8},
    {"XZZX", 0x28},
    {"XZZY", 0x68},
    {"XZZZ", 0xA8},
    {"XZZW", 0xE8},
    {"XZWX", 0x38},
    {"XZWY", 0x78},
    {"XZWZ", 0xB8},
    {"XZWW", 0xF8},
    {"XWXX", 0xC},
    {"XWXY", 0x4C},
    {"XWXZ", 0x8C},
    {"XWXW", 0xCC},
    {"XWYX", 0x1C},
    {"XWYY", 0x5C},
    {"XWYZ", 0x9C},
    {"XWYW", 0xDC},
    {"XWZX", 0x2C},
    {"XWZY", 0x6C},
    {"XWZZ", 0xAC},
    {"XWZW", 0xEC},
    {"XWWX", 0x3C},
    {"XWWY", 0x7C},
    {"XWWZ", 0xBC},
    {"XWWW", 0xFC},
    {"YXXX", 0x1},
    {"YXXY", 0x41},
    {"YXXZ", 0x81},
    {"YXXW", 0xC1},
    {"YXYX", 0x11},
    {"YXYY", 0x51},
    {"YXYZ", 0x91},
    {"YXYW", 0xD1},
    {"YXZX", 0x21},
    {"YXZY", 0x61},
    {"YXZZ", 0xA1},
    {"YXZW", 0xE1},
    {"YXWX", 0x31},
    {"YXWY", 0x71},
    {"YXWZ", 0xB1},
    {"YXWW", 0xF1},
    {"YYXX", 0x5},
    {"YYXY", 0x45},
    {"YYXZ", 0x85},
    {"YYXW", 0xC5},
    {"YYYX", 0x15},
    {"YYYY", 0x55},
    {"YYYZ", 0x95},
    {"YYYW", 0xD5},
    {"YYZX", 0x25},
    {"YYZY", 0x65},
    {"YYZZ", 0xA5},
    {"YYZW", 0xE5},
    {"YYWX", 0x35},
    {"YYWY", 0x75},
    {"YYWZ", 0xB5},
    {"YYWW", 0xF5},
    {"YZXX", 0x9},
    {"YZXY", 0x49},
    {"YZXZ", 0x89},
    {"YZXW", 0xC9},
    {"YZYX", 0x19},
    {"YZYY", 0x59},
    {"YZYZ", 0x99},
    {"YZYW", 0xD9},
    {"YZZX", 0x29},
    {"YZZY", 0x69},
    {"YZZZ", 0xA9},
    {"YZZW", 0xE9},
    {"YZWX", 0x39},
    {"YZWY", 0x79},
    {"YZWZ", 0xB9},
    {"YZWW", 0xF9},
    {"YWXX", 0xD},
    {"YWXY", 0x4D},
    {"YWXZ", 0x8D},
    {"YWXW", 0xCD},
    {"YWYX", 0x1D},
    {"YWYY", 0x5D},
    {"YWYZ", 0x9D},
    {"YWYW", 0xDD},
    {"YWZX", 0x2D},
    {"YWZY", 0x6D},
    {"YWZZ", 0xAD},
    {"YWZW", 0xED},
    {"YWWX", 0x3D},
    {"YWWY", 0x7D},
    {"YWWZ", 0xBD},
    {"YWWW", 0xFD},
    {"ZXXX", 0x2},
    {"ZXXY", 0x42},
    {"ZXXZ", 0x82},
    {"ZXXW", 0xC2},
    {"ZXYX", 0x12},
    {"ZXYY", 0x52},
    {"ZXYZ", 0x92},
    {"ZXYW", 0xD2},
    {"ZXZX", 0x22},
    {"ZXZY", 0x62},
    {"ZXZZ", 0xA2},
    {"ZXZW", 0xE2},
    {"ZXWX", 0x32},
    {"ZXWY", 0x72},
    {"ZXWZ", 0xB2},
    {"ZXWW", 0xF2},
    {"ZYXX", 0x6},
    {"ZYXY", 0x46},
    {"ZYXZ", 0x86},
    {"ZYXW", 0xC6},
    {"ZYYX", 0x16},
    {"ZYYY", 0x56},
    {"ZYYZ", 0x96},
    {"ZYYW", 0xD6},
    {"ZYZX", 0x26},
    {"ZYZY", 0x66},
    {"ZYZZ", 0xA6},
    {"ZYZW", 0xE6},
    {"ZYWX", 0x36},
    {"ZYWY", 0x76},
    {"ZYWZ", 0xB6},
    {"ZYWW", 0xF6},
    {"ZZXX", 0xA},
    {"ZZXY", 0x4A},
    {"ZZXZ", 0x8A},
    {"ZZXW", 0xCA},
    {"ZZYX", 0x1A},
    {"ZZYY", 0x5A},
    {"ZZYZ", 0x9A},
    {"ZZYW", 0xDA},
    {"ZZZX", 0x2A},
    {"ZZZY", 0x6A},
    {"ZZZZ", 0xAA},
    {"ZZZW", 0xEA},
    {"ZZWX", 0x3A},
    {"ZZWY", 0x7A},
    {"ZZWZ", 0xBA},
    {"ZZWW", 0xFA},
    {"ZWXX", 0xE},
    {"ZWXY", 0x4E},
    {"ZWXZ", 0x8E},
    {"ZWXW", 0xCE},
    {"ZWYX", 0x1E},
    {"ZWYY", 0x5E},
    {"ZWYZ", 0x9E},
    {"ZWYW", 0xDE},
    {"ZWZX", 0x2E},
    {"ZWZY", 0x6E},
    {"ZWZZ", 0xAE},
    {"ZWZW", 0xEE},
    {"ZWWX", 0x3E},
    {"ZWWY", 0x7E},
    {"ZWWZ", 0xBE},
    {"ZWWW", 0xFE},
    {"WXXX", 0x3},
    {"WXXY", 0x43},
    {"WXXZ", 0x83},
    {"WXXW", 0xC3},
    {"WXYX", 0x13},
    {"WXYY", 0x53},
    {"WXYZ", 0x93},
    {"WXYW", 0xD3},
    {"WXZX", 0x23},
    {"WXZY", 0x63},
    {"WXZZ", 0xA3},
    {"WXZW", 0xE3},
    {"WXWX", 0x33},
    {"WXWY", 0x73},
    {"WXWZ", 0xB3},
    {"WXWW", 0xF3},
    {"WYXX", 0x7},
    {"WYXY", 0x47},
    {"WYXZ", 0x87},
    {"WYXW", 0xC7},
    {"WYYX", 0x17},
    {"WYYY", 0x57},
    {"WYYZ", 0x97},
    {"WYYW", 0xD7},
    {"WYZX", 0x27},
    {"WYZY", 0x67},
    {"WYZZ", 0xA7},
    {"WYZW", 0xE7},
    {"WYWX", 0x37},
    {"WYWY", 0x77},
    {"WYWZ", 0xB7},
    {"WYWW", 0xF7},
    {"WZXX", 0xB},
    {"WZXY", 0x4B},
    {"WZXZ", 0x8B},
    {"WZXW", 0xCB},
    {"WZYX", 0x1B},
    {"WZYY", 0x5B},
    {"WZYZ", 0x9B},
    {"WZYW", 0xDB},
    {"WZZX", 0x2B},
    {"WZZY", 0x6B},
    {"WZZZ", 0xAB},
    {"WZZW", 0xEB},
    {"WZWX", 0x3B},
    {"WZWY", 0x7B},
    {"WZWZ", 0xBB},
    {"WZWW", 0xFB},
    {"WWXX", 0xF},
    {"WWXY", 0x4F},
    {"WWXZ", 0x8F},
    {"WWXW", 0xCF},
    {"WWYX", 0x1F},
    {"WWYY", 0x5F},
    {"WWYZ", 0x9F},
    {"WWYW", 0xDF},
    {"WWZX", 0x2F},
    {"WWZY", 0x6F},
    {"WWZZ", 0xAF},
    {"WWZW", 0xEF},
    {"WWWX", 0x3F},
    {"WWWY", 0x7F},
    {"WWWZ", 0xBF},
    {"WWWW", 0xFF},
    {"XX", 0x0},
    {"XY", 0x2},
    {"YX", 0x1},
    {"YY", 0x3},
    {0,0}
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

    if (!JS_DefineConstIntegers(cx, SIMD, SHUFFLE_MASKS))
        return nullptr;

    // float32x4
    RootedObject float32x4Object(cx);
    float32x4Object = CreateSimdClass<Float32x4Defn>(cx, global,
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
    int32x4Object = CreateSimdClass<Int32x4Defn>(cx, global,
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
JSObject *
js::CreateSimd(JSContext *cx, typename V::Elem *data)
{
    typedef typename V::Elem Elem;
    Rooted<TypeDescr*> typeDescr(cx, &V::GetTypeDescr(*cx->global()));
    JS_ASSERT(typeDescr);

    Rooted<TypedObject *> result(cx, OutlineTypedObject::createZeroed(cx, typeDescr, 0));
    if (!result)
        return nullptr;

    Elem *resultMem = reinterpret_cast<Elem *>(result->typedMem());
    memcpy(resultMem, data, sizeof(Elem) * V::lanes);
    return result;
}

template JSObject *js::CreateSimd<Float32x4>(JSContext *cx, Float32x4::Elem *data);
template JSObject *js::CreateSimd<Int32x4>(JSContext *cx, Int32x4::Elem *data);

namespace js {
// Unary SIMD operators
template<typename T>
struct Abs {
    static inline T apply(T x) { return x < 0 ? -1 * x : x; }
};
template<typename T>
struct Neg {
    static inline T apply(T x) { return -1 * x; }
};
template<typename T>
struct Not {
    static inline T apply(T x) { return ~x; }
};
template<typename T>
struct Rec {
    static inline T apply(T x) { return 1 / x; }
};
template<typename T>
struct RecSqrt {
    static inline T apply(T x) { return 1 / sqrt(x); }
};
template<typename T>
struct Sqrt {
    static inline T apply(T x) { return sqrt(x); }
};

// Binary SIMD operators
template<typename T>
struct Add {
    static inline T apply(T l, T r) { return l + r; }
};
template<typename T>
struct Sub {
    static inline T apply(T l, T r) { return l - r; }
};
template<typename T>
struct Div {
    static inline T apply(T l, T r) { return l / r; }
};
template<typename T>
struct Mul {
    static inline T apply(T l, T r) { return l * r; }
};
template<typename T>
struct Minimum {
    static inline T apply(T l, T r) { return l < r ? l : r; }
};
template<typename T>
struct Maximum {
    static inline T apply(T l, T r) { return l > r ? l : r; }
};
template<typename T>
struct LessThan {
    static inline int32_t apply(T l, T r) { return l < r ? 0xFFFFFFFF : 0x0; }
};
template<typename T>
struct LessThanOrEqual {
    static inline int32_t apply(T l, T r) { return l <= r ? 0xFFFFFFFF : 0x0; }
};
template<typename T>
struct GreaterThan {
    static inline int32_t apply(T l, T r) { return l > r ? 0xFFFFFFFF : 0x0; }
};
template<typename T>
struct GreaterThanOrEqual {
    static inline int32_t apply(T l, T r) { return l >= r ? 0xFFFFFFFF : 0x0; }
};
template<typename T>
struct Equal {
    static inline int32_t apply(T l, T r) { return l == r ? 0xFFFFFFFF : 0x0; }
};
template<typename T>
struct NotEqual {
    static inline int32_t apply(T l, T r) { return l != r ? 0xFFFFFFFF : 0x0; }
};
template<typename T>
struct Xor {
    static inline T apply(T l, T r) { return l ^ r; }
};
template<typename T>
struct And {
    static inline T apply(T l, T r) { return l & r; }
};
template<typename T>
struct Or {
    static inline T apply(T l, T r) { return l | r; }
};
template<typename T>
struct Scale {
    static inline T apply(int32_t lane, T scalar, T x) { return scalar * x; }
};
template<typename T>
struct WithX {
    static inline T apply(int32_t lane, T scalar, T x) { return lane == 0 ? scalar : x; }
};
template<typename T>
struct WithY {
    static inline T apply(int32_t lane, T scalar, T x) { return lane == 1 ? scalar : x; }
};
template<typename T>
struct WithZ {
    static inline T apply(int32_t lane, T scalar, T x) { return lane == 2 ? scalar : x; }
};
template<typename T>
struct WithW {
    static inline T apply(int32_t lane, T scalar, T x) { return lane == 3 ? scalar : x; }
};
template<typename T>
struct WithFlagX {
    static inline T apply(T l, T f, T x) { return l == 0 ? (f ? 0xFFFFFFFF : 0x0) : x; }
};
template<typename T>
struct WithFlagY {
    static inline T apply(T l, T f, T x) { return l == 1 ? (f ? 0xFFFFFFFF : 0x0) : x; }
};
template<typename T>
struct WithFlagZ {
    static inline T apply(T l, T f, T x) { return l == 2 ? (f ? 0xFFFFFFFF : 0x0) : x; }
};
template<typename T>
struct WithFlagW {
    static inline T apply(T l, T f, T x) { return l == 3 ? (f ? 0xFFFFFFFF : 0x0) : x; }
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

template<typename Out>
static bool
StoreResult(JSContext *cx, CallArgs &args, typename Out::Elem *result)
{
    RootedObject obj(cx, CreateSimd<Out>(cx, result));
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

// Coerces the inputs of type In to the type Coercion, apply the operator Op
// and converts the result to the type Out.
template<typename In, typename Coercion, template<typename C> class Op, typename Out>
static bool
CoercedUnaryFunc(JSContext *cx, unsigned argc, Value *vp)
{
    typedef typename Coercion::Elem CoercionElem;
    typedef typename Out::Elem RetElem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1 || !IsVectorObject<In>(args[0]))
        return ErrorBadArgs(cx);

    CoercionElem result[Coercion::lanes];
    CoercionElem *val = TypedObjectMemory<CoercionElem *>(args[0]);
    for (unsigned i = 0; i < Coercion::lanes; i++)
        result[i] = Op<CoercionElem>::apply(val[i]);
    return StoreResult<Out>(cx, args, (RetElem*) result);
}

// Coerces the inputs of type In to the type Coercion, apply the operator Op
// and converts the result to the type Out.
template<typename In, typename Coercion, template<typename C> class Op, typename Out>
static bool
CoercedBinaryFunc(JSContext *cx, unsigned argc, Value *vp)
{
    typedef typename Coercion::Elem CoercionElem;
    typedef typename Out::Elem RetElem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 2 || !IsVectorObject<In>(args[0]) || !IsVectorObject<In>(args[1]))
        return ErrorBadArgs(cx);

    CoercionElem result[Coercion::lanes];
    CoercionElem *left = TypedObjectMemory<CoercionElem *>(args[0]);
    CoercionElem *right = TypedObjectMemory<CoercionElem *>(args[1]);
    for (unsigned i = 0; i < Coercion::lanes; i++)
        result[i] = Op<CoercionElem>::apply(left[i], right[i]);
    return StoreResult<Out>(cx, args, (RetElem *) result);
}

// Same as above, with no coercion, i.e. Coercion == In.
template<typename In, template<typename C> class Op, typename Out>
static bool
UnaryFunc(JSContext *cx, unsigned argc, Value *vp)
{
    return CoercedUnaryFunc<In, Out, Op, Out>(cx, argc, vp);
}

template<typename In, template<typename C> class Op, typename Out>
static bool
BinaryFunc(JSContext *cx, unsigned argc, Value *vp)
{
    return CoercedBinaryFunc<In, Out, Op, Out>(cx, argc, vp);
}

template<typename V, template<typename T> class OpWith>
static bool
FuncWith(JSContext *cx, unsigned argc, Value *vp)
{
    typedef typename V::Elem Elem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 2 || !IsVectorObject<V>(args[0]) ||
        (!args[1].isNumber() && !args[1].isBoolean()))
    {
        return ErrorBadArgs(cx);
    }

    Elem *val = TypedObjectMemory<Elem *>(args[0]);
    Elem result[V::lanes];

    if (args[1].isNumber()) {
        Elem withAsNumber;
        if (!V::toType(cx, args[1], &withAsNumber))
            return false;
        for (unsigned i = 0; i < V::lanes; i++)
            result[i] = OpWith<Elem>::apply(i, withAsNumber, val[i]);
    } else {
        JS_ASSERT(args[1].isBoolean());
        bool withAsBool = args[1].toBoolean();
        for (unsigned i = 0; i < V::lanes; i++)
            result[i] = OpWith<Elem>::apply(i, withAsBool, val[i]);
    }
    return StoreResult<V>(cx, args, result);
}

template<typename V>
static bool
FuncShuffle(JSContext *cx, unsigned argc, Value *vp)
{
    typedef typename V::Elem Elem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 2 && args.length() != 3)
        return ErrorBadArgs(cx);

    // Let L be V::Lanes. Each lane can contain L lanes, so the log2(L) first
    // bits select the first lane, the next log2(L) the second, and so on.
    const uint32_t SELECT_SHIFT = FloorLog2(V::lanes);
    const uint32_t SELECT_MASK  = V::lanes - 1;
    const int32_t MAX_MASK_VALUE = int32_t(pow(double(V::lanes), double(V::lanes))) - 1;
    JS_ASSERT(MAX_MASK_VALUE > 0);

    Elem result[V::lanes];
    if (args.length() == 2) {
        if (!IsVectorObject<V>(args[0]) || !args[1].isInt32())
            return ErrorBadArgs(cx);

        Elem *val = TypedObjectMemory<Elem *>(args[0]);
        int32_t maskArg;
        if (!ToInt32(cx, args[1], &maskArg))
            return false;
        if (maskArg < 0 || maskArg > MAX_MASK_VALUE)
            return ErrorBadArgs(cx);

        for (unsigned i = 0; i < V::lanes; i++)
            result[i] = val[(maskArg >> (i * SELECT_SHIFT)) & SELECT_MASK];
    } else {
        JS_ASSERT(args.length() == 3);
        if (!IsVectorObject<V>(args[0]) || !IsVectorObject<V>(args[1]) || !args[2].isInt32())
            return ErrorBadArgs(cx);

        Elem *val1 = TypedObjectMemory<Elem *>(args[0]);
        Elem *val2 = TypedObjectMemory<Elem *>(args[1]);
        int32_t maskArg;
        if (!ToInt32(cx, args[2], &maskArg))
            return false;
        if (maskArg < 0 || maskArg > MAX_MASK_VALUE)
            return ErrorBadArgs(cx);

        unsigned i = 0;
        for (; i < V::lanes / 2; i++)
            result[i] = val1[(maskArg >> (i * SELECT_SHIFT)) & SELECT_MASK];
        for (; i < V::lanes; i++)
            result[i] = val2[(maskArg >> (i * SELECT_SHIFT)) & SELECT_MASK];
    }

    return StoreResult<V>(cx, args, result);
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

    int32_t *val = TypedObjectMemory<int32_t *>(args[0]);
    int32_t bits;
    if (!ToInt32(cx, args[1], &bits))
        return false;

    for (unsigned i = 0; i < 4; i++)
        result[i] = Op::apply(val[i], bits);
    return StoreResult<Int32x4>(cx, args, result);
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
        result[i] = ConvertScalar<RetElem>(val[i]);
    return StoreResult<Vret>(cx, args, result);
}

template<typename V, typename Vret>
static bool
FuncConvertBits(JSContext *cx, unsigned argc, Value *vp)
{
    typedef typename Vret::Elem RetElem;

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1 || !IsVectorObject<V>(args[0]))
        return ErrorBadArgs(cx);

    RetElem *result = TypedObjectMemory<RetElem *>(args[0]);
    return StoreResult<Vret>(cx, args, result);
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
    return StoreResult<Vret>(cx, args, result);
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
    return StoreResult<Vret>(cx, args, result);
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
    return StoreResult<Int32x4>(cx, args, result);
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

    return StoreResult<Float32x4>(cx, args, result);
}

static bool
Int32x4Select(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 3 || !IsVectorObject<Int32x4>(args[0]) ||
        !IsVectorObject<Int32x4>(args[1]) || !IsVectorObject<Int32x4>(args[2]))
    {
        return ErrorBadArgs(cx);
    }

    int32_t *val = TypedObjectMemory<int32_t *>(args[0]);
    int32_t *tv = TypedObjectMemory<int32_t *>(args[1]);
    int32_t *fv = TypedObjectMemory<int32_t *>(args[2]);

    int32_t tr[Int32x4::lanes];
    for (unsigned i = 0; i < Int32x4::lanes; i++)
        tr[i] = And<int32_t>::apply(val[i], tv[i]);

    int32_t fr[Int32x4::lanes];
    for (unsigned i = 0; i < Int32x4::lanes; i++)
        fr[i] = And<int32_t>::apply(Not<int32_t>::apply(val[i]), fv[i]);

    int32_t orInt[Int32x4::lanes];
    for (unsigned i = 0; i < Int32x4::lanes; i++)
        orInt[i] = Or<int32_t>::apply(tr[i], fr[i]);

    return StoreResult<Int32x4>(cx, args, orInt);
}

static bool
Float32x4Select(JSContext *cx, unsigned argc, Value *vp)
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
        tr[i] = And<int32_t>::apply(val[i], tv[i]);

    int32_t fr[Int32x4::lanes];
    for (unsigned i = 0; i < Int32x4::lanes; i++)
        fr[i] = And<int32_t>::apply(Not<int32_t>::apply(val[i]), fv[i]);

    int32_t orInt[Int32x4::lanes];
    for (unsigned i = 0; i < Int32x4::lanes; i++)
        orInt[i] = Or<int32_t>::apply(tr[i], fr[i]);

    float *result = reinterpret_cast<float *>(orInt);
    return StoreResult<Float32x4>(cx, args, result);
}

#define DEFINE_SIMD_FLOAT32X4_FUNCTION(Name, Func, Operands, Flags) \
bool                                                                \
js::simd_float32x4_##Name(JSContext *cx, unsigned argc, Value *vp)  \
{                                                                   \
    return Func(cx, argc, vp);                                      \
}
FLOAT32X4_FUNCTION_LIST(DEFINE_SIMD_FLOAT32X4_FUNCTION)
#undef DEFINE_SIMD_FLOAT32x4_FUNCTION

#define DEFINE_SIMD_INT32X4_FUNCTION(Name, Func, Operands, Flags)   \
bool                                                                \
js::simd_int32x4_##Name(JSContext *cx, unsigned argc, Value *vp)    \
{                                                                   \
    return Func(cx, argc, vp);                                      \
}
INT32X4_FUNCTION_LIST(DEFINE_SIMD_INT32X4_FUNCTION)
#undef DEFINE_SIMD_INT32X4_FUNCTION

const JSFunctionSpec js::Float32x4Methods[] = {
#define SIMD_FLOAT32X4_FUNCTION_ITEM(Name, Func, Operands, Flags)   \
        JS_FN(#Name, js::simd_float32x4_##Name, Operands, Flags),
        FLOAT32X4_FUNCTION_LIST(SIMD_FLOAT32X4_FUNCTION_ITEM)
#undef SIMD_FLOAT32x4_FUNCTION_ITEM
        JS_FS_END
};

const JSFunctionSpec js::Int32x4Methods[] = {
#define SIMD_INT32X4_FUNCTION_ITEM(Name, Func, Operands, Flags)     \
        JS_FN(#Name, js::simd_int32x4_##Name, Operands, Flags),
        INT32X4_FUNCTION_LIST(SIMD_INT32X4_FUNCTION_ITEM)
#undef SIMD_INT32X4_FUNCTION_ITEM
        JS_FS_END
};

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/BigInt.h"

#include "jsapi.h"

#include "builtin/TypedObject.h"
#include "gc/Tracer.h"
#include "js/PropertySpec.h"
#include "js/TracingAPI.h"
#include "vm/ArrayBufferObject.h"
#include "vm/BigIntType.h"
#include "vm/SelfHosting.h"
#include "vm/TaggedProto.h"

#include "vm/JSObject-inl.h"

using namespace js;

static MOZ_ALWAYS_INLINE bool IsBigInt(HandleValue v) {
  return v.isBigInt() || (v.isObject() && v.toObject().is<BigIntObject>());
}

// BigInt proposal section 5.1.3
static bool BigIntConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (args.isConstructing()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_CONSTRUCTOR, "BigInt");
    return false;
  }

  // Step 2.
  RootedValue v(cx, args.get(0));
  if (!ToPrimitive(cx, JSTYPE_NUMBER, &v)) {
    return false;
  }

  // Steps 3-4.
  BigInt* bi =
      v.isNumber() ? NumberToBigInt(cx, v.toNumber()) : ToBigInt(cx, v);
  if (!bi) {
    return false;
  }

  args.rval().setBigInt(bi);
  return true;
}

JSObject* BigIntObject::create(JSContext* cx, HandleBigInt bigInt) {
  RootedObject obj(cx, NewBuiltinClassInstance(cx, &class_));
  if (!obj) {
    return nullptr;
  }
  BigIntObject& bn = obj->as<BigIntObject>();
  bn.setFixedSlot(PRIMITIVE_VALUE_SLOT, BigIntValue(bigInt));
  return &bn;
}

BigInt* BigIntObject::unbox() const {
  return getFixedSlot(PRIMITIVE_VALUE_SLOT).toBigInt();
}

// BigInt proposal section 5.3.4
bool BigIntObject::valueOf_impl(JSContext* cx, const CallArgs& args) {
  // Step 1.
  HandleValue thisv = args.thisv();
  MOZ_ASSERT(IsBigInt(thisv));
  RootedBigInt bi(cx, thisv.isBigInt()
                          ? thisv.toBigInt()
                          : thisv.toObject().as<BigIntObject>().unbox());

  args.rval().setBigInt(bi);
  return true;
}

bool BigIntObject::valueOf(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsBigInt, valueOf_impl>(cx, args);
}

// BigInt proposal section 5.3.3
bool BigIntObject::toString_impl(JSContext* cx, const CallArgs& args) {
  // Step 1.
  HandleValue thisv = args.thisv();
  MOZ_ASSERT(IsBigInt(thisv));
  RootedBigInt bi(cx, thisv.isBigInt()
                          ? thisv.toBigInt()
                          : thisv.toObject().as<BigIntObject>().unbox());

  // Steps 2-3.
  uint8_t radix = 10;

  // Steps 4-5.
  if (args.hasDefined(0)) {
    double d;
    if (!ToInteger(cx, args[0], &d)) {
      return false;
    }
    if (d < 2 || d > 36) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_BAD_RADIX);
      return false;
    }
    radix = d;
  }

  // Steps 6-7.
  JSLinearString* str = BigInt::toString<CanGC>(cx, bi, radix);
  if (!str) {
    return false;
  }
  args.rval().setString(str);
  return true;
}

bool BigIntObject::toString(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsBigInt, toString_impl>(cx, args);
}

#ifndef ENABLE_INTL_API
// BigInt proposal section 5.3.2. "This function is
// implementation-dependent, and it is permissible, but not encouraged,
// for it to return the same thing as toString."
bool BigIntObject::toLocaleString_impl(JSContext* cx, const CallArgs& args) {
  HandleValue thisv = args.thisv();
  MOZ_ASSERT(IsBigInt(thisv));
  RootedBigInt bi(cx, thisv.isBigInt()
                          ? thisv.toBigInt()
                          : thisv.toObject().as<BigIntObject>().unbox());

  RootedString str(cx, BigInt::toString<CanGC>(cx, bi, 10));
  if (!str) {
    return false;
  }
  args.rval().setString(str);
  return true;
}

bool BigIntObject::toLocaleString(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsBigInt, toLocaleString_impl>(cx, args);
}
#endif /* !ENABLE_INTL_API */

// BigInt proposal section 5.2.1. BigInt.asUintN ( bits, bigint )
bool BigIntObject::asUintN(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  uint64_t bits;
  if (!ToIndex(cx, args.get(0), &bits)) {
    return false;
  }

  // Step 2.
  RootedBigInt bi(cx, ToBigInt(cx, args.get(1)));
  if (!bi) {
    return false;
  }

  // Step 3.
  BigInt* res = BigInt::asUintN(cx, bi, bits);
  if (!res) {
    return false;
  }

  args.rval().setBigInt(res);
  return true;
}

// BigInt proposal section 5.2.2. BigInt.asIntN ( bits, bigint )
bool BigIntObject::asIntN(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  uint64_t bits;
  if (!ToIndex(cx, args.get(0), &bits)) {
    return false;
  }

  // Step 2.
  RootedBigInt bi(cx, ToBigInt(cx, args.get(1)));
  if (!bi) {
    return false;
  }

  // Step 3.
  BigInt* res = BigInt::asIntN(cx, bi, bits);
  if (!res) {
    return false;
  }

  args.rval().setBigInt(res);
  return true;
}

const ClassSpec BigIntObject::classSpec_ = {
    GenericCreateConstructor<BigIntConstructor, 1, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<BigIntObject>,
    BigIntObject::staticMethods,
    nullptr,
    BigIntObject::methods,
    BigIntObject::properties};

// The class is named "Object" as a workaround for bug 1277801.
const Class BigIntObject::class_ = {
    "Object",
    JSCLASS_HAS_CACHED_PROTO(JSProto_BigInt) |
        JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS),
    JS_NULL_CLASS_OPS, &BigIntObject::classSpec_};

const Class BigIntObject::protoClass_ = {
    js_Object_str, JSCLASS_HAS_CACHED_PROTO(JSProto_BigInt), JS_NULL_CLASS_OPS,
    &BigIntObject::classSpec_};

const JSPropertySpec BigIntObject::properties[] = {
    // BigInt proposal section 5.3.5
    JS_STRING_SYM_PS(toStringTag, "BigInt", JSPROP_READONLY), JS_PS_END};

const JSFunctionSpec BigIntObject::methods[] = {
    JS_FN("valueOf", valueOf, 0, 0), JS_FN("toString", toString, 0, 0),
#ifdef ENABLE_INTL_API
    JS_SELF_HOSTED_FN("toLocaleString", "BigInt_toLocaleString", 0, 0),
#else
    JS_FN("toLocaleString", toLocaleString, 0, 0),
#endif
    JS_FS_END};

const JSFunctionSpec BigIntObject::staticMethods[] = {
    JS_FN("asUintN", asUintN, 2, 0), JS_FN("asIntN", asIntN, 2, 0), JS_FS_END};

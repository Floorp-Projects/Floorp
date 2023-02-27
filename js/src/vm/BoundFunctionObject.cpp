/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/BoundFunctionObject.h"

#include <string_view>

#include "util/StringBuffer.h"
#include "vm/Interpreter.h"
#include "vm/Shape.h"
#include "vm/Stack.h"

#include "vm/JSFunction-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/Shape-inl.h"

using namespace js;

// Helper function to initialize `args` with all bound arguments + the arguments
// supplied in `callArgs`.
template <typename Args>
static MOZ_ALWAYS_INLINE void FillArguments(Args& args,
                                            BoundFunctionObject* bound,
                                            size_t numBoundArgs,
                                            const CallArgs& callArgs) {
  MOZ_ASSERT(args.length() == numBoundArgs + callArgs.length());

  if (numBoundArgs <= BoundFunctionObject::MaxInlineBoundArgs) {
    for (size_t i = 0; i < numBoundArgs; i++) {
      args[i].set(bound->getInlineBoundArg(i));
    }
  } else {
    ArrayObject* boundArgs = bound->getBoundArgsArray();
    for (size_t i = 0; i < numBoundArgs; i++) {
      args[i].set(boundArgs->getDenseElement(i));
    }
  }

  for (size_t i = 0; i < callArgs.length(); i++) {
    args[numBoundArgs + i].set(callArgs[i]);
  }
}

// ES2023 10.4.1.1 [[Call]]
// https://tc39.es/ecma262/#sec-bound-function-exotic-objects-call-thisargument-argumentslist
// static
bool BoundFunctionObject::call(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  Rooted<BoundFunctionObject*> bound(cx,
                                     &args.callee().as<BoundFunctionObject>());

  // Step 1.
  Rooted<Value> target(cx, bound->getTargetVal());

  // Step 2.
  Rooted<Value> boundThis(cx, bound->getBoundThis());

  // Steps 3-4.
  size_t numBoundArgs = bound->numBoundArgs();
  InvokeArgs args2(cx);
  if (!args2.init(cx, uint64_t(numBoundArgs) + args.length())) {
    return false;
  }
  FillArguments(args2, bound, numBoundArgs, args);

  // Step 5.
  return Call(cx, target, boundThis, args2, args.rval());
}

// ES2023 10.4.1.2 [[Construct]]
// https://tc39.es/ecma262/#sec-bound-function-exotic-objects-construct-argumentslist-newtarget
// static
bool BoundFunctionObject::construct(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  Rooted<BoundFunctionObject*> bound(cx,
                                     &args.callee().as<BoundFunctionObject>());

  MOZ_ASSERT(bound->isConstructor(),
             "shouldn't have called this hook if not a constructor");

  // Step 1.
  Rooted<Value> target(cx, bound->getTargetVal());

  // Step 2.
  MOZ_ASSERT(IsConstructor(target));

  // Steps 3-4.
  size_t numBoundArgs = bound->numBoundArgs();
  ConstructArgs args2(cx);
  if (!args2.init(cx, uint64_t(numBoundArgs) + args.length())) {
    return false;
  }
  FillArguments(args2, bound, numBoundArgs, args);

  // Step 5.
  Rooted<Value> newTarget(cx, args.newTarget());
  if (newTarget == ObjectValue(*bound)) {
    newTarget = target;
  }

  // Step 6.
  Rooted<JSObject*> res(cx);
  if (!Construct(cx, target, args2, newTarget, &res)) {
    return false;
  }
  args.rval().setObject(*res);
  return true;
}

// static
JSString* BoundFunctionObject::funToString(JSContext* cx, Handle<JSObject*> obj,
                                           bool isToSource) {
  // Implementation of the funToString hook used by Function.prototype.toString.

  // For the non-standard toSource extension, we include "bound" to indicate
  // it's a bound function.
  if (isToSource) {
    static constexpr std::string_view nativeCodeBound =
        "function bound() {\n    [native code]\n}";
    return NewStringCopy<CanGC>(cx, nativeCodeBound);
  }

  static constexpr std::string_view nativeCode =
      "function() {\n    [native code]\n}";
  return NewStringCopy<CanGC>(cx, nativeCode);
}

// static
SharedShape* BoundFunctionObject::assignInitialShape(
    JSContext* cx, Handle<BoundFunctionObject*> obj) {
  MOZ_ASSERT(obj->empty());

  constexpr PropertyFlags propFlags = {PropertyFlag::Configurable};
  if (!NativeObject::addPropertyInReservedSlot(cx, obj, cx->names().length,
                                               LengthSlot, propFlags)) {
    return nullptr;
  }
  if (!NativeObject::addPropertyInReservedSlot(cx, obj, cx->names().name,
                                               NameSlot, propFlags)) {
    return nullptr;
  }

  return obj->sharedShape();
}

static MOZ_ALWAYS_INLINE JSAtom* AppendBoundFunctionPrefix(JSContext* cx,
                                                           JSString* str) {
  StringBuffer sb(cx);
  if (!sb.append("bound ") || !sb.append(str)) {
    return nullptr;
  }
  return sb.finishAtom();
}

// ES2023 20.2.3.2 Function.prototype.bind
// https://tc39.es/ecma262/#sec-function.prototype.bind
//
// ES2023 10.4.1.3 BoundFunctionCreate
// https://tc39.es/ecma262/#sec-boundfunctioncreate
//
// BoundFunctionCreate has been inlined in Function.prototype.bind for
// performance reasons.
//
// static
bool BoundFunctionObject::functionBind(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // ES2023 20.2.3.2 Function.prototype.bind
  // Steps 1-2.
  if (MOZ_UNLIKELY(!IsCallable(args.thisv()))) {
    ReportIncompatibleMethod(cx, args, &FunctionClass);
    return false;
  }

  size_t numBoundArgs = args.length() > 0 ? args.length() - 1 : 0;
  if (MOZ_UNLIKELY(numBoundArgs > ARGS_LENGTH_MAX)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TOO_MANY_ARGUMENTS);
    return false;
  }

  Rooted<JSObject*> target(cx, &args.thisv().toObject());

  // ES2023 10.4.1.3 BoundFunctionCreate
  // Step 1.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototype(cx, target, &proto)) {
    return false;
  }

  // Steps 2-5.
  Rooted<BoundFunctionObject*> bound(
      cx, NewObjectWithGivenProto<BoundFunctionObject>(cx, proto));
  if (!bound) {
    return false;
  }
  if (!SharedShape::ensureInitialCustomShape<BoundFunctionObject>(cx, bound)) {
    return false;
  }

  MOZ_ASSERT(bound->lookupPure(cx->names().length)->slot() == LengthSlot);
  MOZ_ASSERT(bound->lookupPure(cx->names().name)->slot() == NameSlot);

  // Steps 6 and 9.
  bound->initFlags(numBoundArgs, target->isConstructor());

  // Step 7.
  bound->initReservedSlot(TargetSlot, ObjectValue(*target));

  // Step 8.
  bound->initReservedSlot(BoundThisSlot, args.get(0));

  if (numBoundArgs <= MaxInlineBoundArgs) {
    for (size_t i = 0; i < numBoundArgs; i++) {
      bound->initReservedSlot(BoundArg0Slot + i, args[i + 1]);
    }
  } else {
    ArrayObject* arr = NewDenseCopiedArray(cx, numBoundArgs, args.array() + 1);
    if (!arr) {
      return false;
    }
    bound->initReservedSlot(BoundArg0Slot, ObjectValue(*arr));
  }

  // ES2023 20.2.3.2 Function.prototype.bind
  // Step 4.
  double length = 0.0;

  // Step 5.
  bool hasLength;
  Rooted<PropertyKey> key(cx, NameToId(cx->names().length));
  if (!HasOwnProperty(cx, target, key, &hasLength)) {
    return false;
  }

  // Step 6.
  if (hasLength) {
    Rooted<Value> targetLength(cx);
    if (!GetProperty(cx, target, target, key, &targetLength)) {
      return false;
    }

    if (targetLength.isNumber()) {
      length = std::max(
          0.0, JS::ToInteger(targetLength.toNumber()) - double(numBoundArgs));
    }
  }

  // Step 7.
  bound->initLength(length);

  // Step 8.
  Rooted<Value> targetName(cx);
  if (!GetProperty(cx, target, target, cx->names().name, &targetName)) {
    return false;
  }

  // Step 9.
  JSAtom* name;
  if (targetName.isString()) {
    name = AppendBoundFunctionPrefix(cx, targetName.toString());
    if (!name) {
      return false;
    }
  } else {
    name = cx->names().boundWithSpace;
  }

  // Step 10.
  bound->initName(name);

  // Step 11.
  args.rval().setObject(*bound);
  return true;
}

static const JSClassOps classOps = {
    nullptr,                         // addProperty
    nullptr,                         // delProperty
    nullptr,                         // enumerate
    nullptr,                         // newEnumerate
    nullptr,                         // resolve
    nullptr,                         // mayResolve
    nullptr,                         // finalize
    BoundFunctionObject::call,       // call
    BoundFunctionObject::construct,  // construct
    nullptr,                         // trace
};

static const ObjectOps objOps = {
    nullptr,                           // lookupProperty
    nullptr,                           // qdefineProperty
    nullptr,                           // hasProperty
    nullptr,                           // getProperty
    nullptr,                           // setProperty
    nullptr,                           // getOwnPropertyDescriptor
    nullptr,                           // deleteProperty
    nullptr,                           // getElements
    BoundFunctionObject::funToString,  // funToString
};

const JSClass BoundFunctionObject::class_ = {
    "BoundFunctionObject",
    // Note: bound functions don't have their own constructor or prototype (they
    // use the prototype of the target object), but we give them a JSProtoKey
    // because that's what Xray wrappers use to identify builtin objects.
    JSCLASS_HAS_CACHED_PROTO(JSProto_BoundFunction) |
        JSCLASS_HAS_RESERVED_SLOTS(BoundFunctionObject::SlotCount),
    &classOps,
    JS_NULL_CLASS_SPEC,
    JS_NULL_CLASS_EXT,
    &objOps,
};

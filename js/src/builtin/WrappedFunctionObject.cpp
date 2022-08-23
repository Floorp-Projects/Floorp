/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/WrappedFunctionObject.h"

#include "jsapi.h"

#include "builtin/ShadowRealm.h"
#include "js/CallAndConstruct.h"
#include "js/Class.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/JSFunction.h"
#include "vm/ObjectOperations.h"

#include "vm/JSObject-inl.h"
#include "vm/Realm-inl.h"

using namespace js;
using namespace JS;

// GetWrappedValue ( callerRealm: a Realm Record, value: unknown )
bool js::GetWrappedValue(JSContext* cx, Realm* callerRealm, Handle<Value> value,
                         MutableHandle<Value> res) {
  // Step 2. Return value (Reordered)
  if (!value.isObject()) {
    res.set(value);
    return true;
  }

  // Step 1. If Type(value) is Object, then
  //      a. If IsCallable(value) is false, throw a TypeError exception.
  Rooted<JSObject*> objectVal(cx, &value.toObject());
  if (!IsCallable(objectVal)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_SHADOW_REALM_INVALID_RETURN);
    return false;
  }

  //     b. Return ? WrappedFunctionCreate(callerRealm, value).
  return WrappedFunctionCreate(cx, callerRealm, objectVal, res);
}

// [[Call]]
// https://tc39.es/proposal-shadowrealm/#sec-wrapped-function-exotic-objects-call-thisargument-argumentslist
static bool WrappedFunction_Call(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  Rooted<JSObject*> callee(cx, &args.callee());
  MOZ_ASSERT(callee->is<WrappedFunctionObject>());

  Handle<WrappedFunctionObject*> F = callee.as<WrappedFunctionObject>();

  // 1. Let target be F.[[WrappedTargetFunction]].
  Rooted<JSObject*> target(cx, F->getTargetFunction());

  // 2. Assert: IsCallable(target) is true.
  MOZ_ASSERT(IsCallable(ObjectValue(*target)));

  // 3. Let targetRealm be ? GetFunctionRealm(target).
  Rooted<Realm*> targetRealm(cx, GetFunctionRealm(cx, target));
  if (!targetRealm) {
    return false;
  }
  // 4. Let callerRealm be ? GetFunctionRealm(F).
  Rooted<Realm*> callerRealm(cx, GetFunctionRealm(cx, F));
  if (!callerRealm) {
    return false;
  }

  // 5. NOTE: Any exception objects produced after this point are associated
  //    with callerRealm.
  Rooted<Value> result(cx);
  {
    Rooted<JSObject*> global(cx, JS::GetRealmGlobalOrNull(callerRealm));
    MOZ_RELEASE_ASSERT(
        global, "global is null; executing in a realm that's being GC'd?");
    AutoRealm ar(cx, global);

    // https://searchfox.org/mozilla-central/source/js/public/CallAndConstruct.h#57-73
    // 6. Let wrappedArgs be a new empty List.
    RootedVector<Value> wrappedArgs(cx);

    // 7. For each element arg of argumentsList, do
    //     a. Let wrappedValue be ? GetWrappedValue(targetRealm, arg).
    //     b. Append wrappedValue to wrappedArgs.
    Rooted<Value> element(cx);
    for (size_t i = 0; i < args.length(); i++) {
      element = args.get(i);
      if (!GetWrappedValue(cx, targetRealm, element, &element)) {
        return false;
      }

      if (!wrappedArgs.append(element)) {
        return false;
      }
    }

    // 8. Let wrappedThisArgument to ? GetWrappedValue(targetRealm,
    // thisArgument).
    Rooted<Value> wrappedThisArgument(cx);
    if (!GetWrappedValue(cx, targetRealm, args.thisv(), &wrappedThisArgument)) {
      return false;
    }

    // 9. Let result be the Completion Record of Call(target,
    //    wrappedThisArgument, wrappedArgs).
    if (!JS::Call(cx, wrappedThisArgument, target, wrappedArgs, &result)) {
      // 11. Else (reordered);
      //     a. Throw a TypeError exception.
      ReportPotentiallyDetailedMessage(
          cx, JSMSG_SHADOW_REALM_WRAPPED_EXECUTION_FAILURE_DETAIL,
          JSMSG_SHADOW_REALM_WRAPPED_EXECUTION_FAILURE);
      return false;
    }

    // 10. If result.[[Type]] is normal or result.[[Type]] is return, then
    //     a. Return ? GetWrappedValue(callerRealm, result.[[Value]]).
    if (!GetWrappedValue(cx, callerRealm, result, args.rval())) {
      return false;
    }
  }

  return true;
}

static bool CopyNameAndLength(JSContext* cx, HandleObject F,
                              HandleObject Target, char* prefix = nullptr,
                              unsigned argCount = 0) {
  // 1. If argCount is undefined, then set argCount to 0 (implicit)
  // 2. Let L be 0.
  double L = 0;

  // 3. Let targetHasLength be ? HasOwnProperty(Target, "length").

  Rooted<jsid> length(cx, NameToId(cx->names().length));
  Rooted<jsid> name(cx, NameToId(cx->names().name));
  // Try to avoid invoking the resolve hook.
  if (Target->is<JSFunction>() &&
      !Target->as<JSFunction>().hasResolvedLength()) {
    Rooted<Value> targetLen(cx);
    if (!JSFunction::getUnresolvedLength(cx, Target.as<JSFunction>(),
                                         &targetLen)) {
      return false;
    }

    L = std::max(0.0, targetLen.toNumber() - argCount);
  } else {
    bool targetHasLength;
    if (!HasOwnProperty(cx, Target, length, &targetHasLength)) {
      return false;
    }
    // https://searchfox.org/mozilla-central/source/js/src/vm/JSFunction.cpp#1298
    // 4. If targetHasLength is true, then
    if (targetHasLength) {
      //     a. Let targetLen be ? Get(Target, "length").
      Rooted<Value> targetLen(cx);
      if (!GetProperty(cx, Target, Target, length, &targetLen)) {
        return false;
      }

      //     b. If Type(targetLen) is Number, then
      //         i. If targetLen is +∞𝔽, set L to +∞.
      //         ii. Else if targetLen is -∞𝔽, set L to 0.
      //         iii. Else,
      //             1. Let targetLenAsInt be ! ToIntegerOrInfinity(targetLen).
      //             2. Assert: targetLenAsInt is finite.
      //             3. Set L to max(targetLenAsInt - argCount, 0).
      if (targetLen.isNumber()) {
        L = std::max(0.0, JS::ToInteger(targetLen.toNumber()) - argCount);
      }
    }
  }

  // 5. Perform ! SetFunctionLength(F, L).
  Rooted<Value> rootedL(cx, DoubleValue(L));
  if (!JS_DefinePropertyById(cx, F, length, rootedL, JSPROP_READONLY)) {
    return false;
  }

  // 6. Let targetName be ? Get(Target, "name").
  Rooted<Value> targetName(cx);
  if (!GetProperty(cx, Target, Target, cx->names().name, &targetName)) {
    return false;
  }

  // 7. If Type(targetName) is not String, set targetName to the empty String.
  if (!targetName.isString()) {
    targetName = StringValue(cx->runtime()->emptyString);
  }

  Rooted<JSString*> targetString(cx, targetName.toString());
  Rooted<jsid> targetNameId(cx);
  if (!JS_StringToId(cx, targetString, &targetNameId)) {
    return false;
  }

  // 8. Perform ! SetFunctionName(F, targetName, prefix).
  // (inlined and specialized from js::SetFunctionName)
  Rooted<JSAtom*> funName(cx, IdToFunctionName(cx, targetNameId));
  if (!funName) {
    return false;
  }
  Rooted<Value> rootedFunName(cx, StringValue(funName));
  return JS_DefinePropertyById(cx, F, name, rootedFunName, JSPROP_READONLY);
}

static const JSClassOps classOps = {
    nullptr,               // addProperty
    nullptr,               // delProperty
    nullptr,               // enumerate
    nullptr,               // newEnumerate
    nullptr,               // resolve
    nullptr,               // mayResolve
    nullptr,               // finalize
    WrappedFunction_Call,  // call
    nullptr,               // construct
    nullptr,               // trace
};

const JSClass WrappedFunctionObject::class_ = {
    "WrappedFunctionObject",
    JSCLASS_HAS_CACHED_PROTO(
        JSProto_Function) |  // This sets the prototype to Function.prototype,
                             // Step 3 of WrappedFunctionCreate
        JSCLASS_HAS_RESERVED_SLOTS(WrappedFunctionObject::SlotCount),
    &classOps};

JSObject* GetRealmFunctionPrototype(JSContext* cx, Realm* realm) {
  CHECK_THREAD(cx);
  Rooted<GlobalObject*> global(cx, realm->maybeGlobal());
  MOZ_RELEASE_ASSERT(global);
  return GlobalObject::getOrCreateFunctionPrototype(cx, global);
}

// WrappedFunctionCreate ( callerRealm: a Realm Record, Target: a function
// object)
bool js::WrappedFunctionCreate(JSContext* cx, Realm* callerRealm,
                               HandleObject target, MutableHandle<Value> res) {
  // Ensure that the function object has the correct realm by allocating it
  // into that realm.
  Rooted<JSObject*> global(cx, JS::GetRealmGlobalOrNull(callerRealm));
  MOZ_RELEASE_ASSERT(global,
                     "global is null; executing in a realm that's being GC'd?");
  AutoRealm ar(cx, global);

  MOZ_ASSERT(target);

  // Target *could* be a function from another realm
  Rooted<JSObject*> maybeWrappedTarget(cx, target);
  if (!JS_WrapObject(cx, &maybeWrappedTarget)) {
    return false;
  }

  // 1. Let internalSlotsList be the internal slots listed in Table 2, plus
  // [[Prototype]] and [[Extensible]].
  // 2. Let wrapped be ! MakeBasicObject(internalSlotsList).
  // 3. Set wrapped.[[Prototype]] to
  //    callerRealm.[[Intrinsics]].[[%Function.prototype%]].
  Rooted<JSObject*> functionPrototype(
      cx, GetRealmFunctionPrototype(cx, callerRealm));
  MOZ_ASSERT(cx->compartment() == functionPrototype->compartment());

  Rooted<WrappedFunctionObject*> wrapped(
      cx,
      NewObjectWithGivenProto<WrappedFunctionObject>(cx, functionPrototype));
  if (!wrapped) {
    return false;
  }

  // 4. Set wrapped.[[Call]] as described in 2.1 (implicit in JSClass call
  // hook)
  // 5. Set wrapped.[[WrappedTargetFunction]] to Target.
  wrapped->setTargetFunction(*maybeWrappedTarget);
  // 6. Set wrapped.[[Realm]] to callerRealm. (implicitly the realm of
  //    wrapped, which we assured with the AutoRealm

  MOZ_ASSERT(wrapped->realm() == callerRealm);

  // 7. Let result be CopyNameAndLength(wrapped, Target).
  if (!CopyNameAndLength(cx, wrapped, maybeWrappedTarget)) {
    // 8. If result is an Abrupt Completion, throw a TypeError exception.
    JS_ClearPendingException(cx);

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_SHADOW_REALM_WRAP_FAILURE);
    return false;
  }

  // 9. Return wrapped.
  res.set(ObjectValue(*wrapped));
  return true;
}

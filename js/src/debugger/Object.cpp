/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "debugger/Object-inl.h"

#include "mozilla/Maybe.h"   // for Maybe, Nothing, Some
#include "mozilla/Range.h"   // for Range
#include "mozilla/Result.h"  // for Result
#include "mozilla/Vector.h"  // for Vector

#include <string.h>     // for size_t, strlen
#include <type_traits>  // for remove_reference<>::type
#include <utility>      // for move

#include "jsapi.h"        // for CallArgs, RootedObject, Rooted
#include "jsfriendapi.h"  // for GetErrorMessage
#include "jsutil.h"       // for Min

#include "builtin/Array.h"       // for NewDenseCopiedArray
#include "debugger/Debugger.h"   // for Completion, Debugger
#include "debugger/NoExecute.h"  // for LeaveDebuggeeNoExecute
#include "debugger/Script.h"     // for DebuggerScript
#include "gc/Barrier.h"          // for ImmutablePropertyNamePtr
#include "gc/Rooting.h"          // for RootedDebuggerObject
#include "gc/Tracer.h"       // for TraceManuallyBarrieredCrossCompartmentEdge
#include "js/Conversions.h"  // for ToObject
#include "js/HeapAPI.h"      // for IsInsideNursery
#include "js/Promise.h"      // for PromiseState
#include "js/Proxy.h"        // for PropertyDescriptor
#include "js/StableStringChars.h"        // for AutoStableStringChars
#include "proxy/ScriptedProxyHandler.h"  // for ScriptedProxyHandler
#include "vm/ArgumentsObject.h"          // for ARGS_LENGTH_MAX
#include "vm/ArrayObject.h"              // for ArrayObject
#include "vm/BytecodeUtil.h"             // for JSDVG_SEARCH_STACK
#include "vm/Compartment.h"              // for Compartment
#include "vm/EnvironmentObject.h"        // for GetDebugEnvironmentForFunction
#include "vm/ErrorObject.h"              // for JSObject::is, ErrorObject
#include "vm/GlobalObject.h"             // for JSObject::is, GlobalObject
#include "vm/Instrumentation.h"          // for RealmInstrumentation
#include "vm/Interpreter.h"              // for Call
#include "vm/JSAtom.h"                   // for Atomize, js_apply_str
#include "vm/JSContext.h"                // for JSContext, ReportValueError
#include "vm/JSFunction.h"               // for JSFunction
#include "vm/JSScript.h"                 // for JSScript
#include "vm/NativeObject.h"             // for NativeObject, JSObject::is
#include "vm/ObjectGroup.h"              // for GenericObject, NewObjectKind
#include "vm/ObjectOperations.h"         // for DefineProperty
#include "vm/Realm.h"                    // for AutoRealm, ErrorCopier, Realm
#include "vm/Runtime.h"                  // for JSAtomState
#include "vm/SavedFrame.h"               // for SavedFrame
#include "vm/Scope.h"                    // for PositionalFormalParameterIter
#include "vm/Shape.h"                    // for Shape
#include "vm/Stack.h"                    // for InvokeArgs
#include "vm/StringType.h"               // for JSAtom, PropertyName
#include "vm/WrapperObject.h"            // for JSObject::is, WrapperObject

#include "vm/Compartment-inl.h"       // for Compartment::wrap
#include "vm/JSAtom-inl.h"            // for ValueToId
#include "vm/JSObject-inl.h"          // for GetObjectClassName, InitClass
#include "vm/NativeObject-inl.h"      // for NativeObject::global
#include "vm/ObjectOperations-inl.h"  // for DeleteProperty, GetProperty
#include "vm/Realm-inl.h"             // for AutoRealm::AutoRealm

using namespace js;

using JS::AutoStableStringChars;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

const JSClassOps DebuggerObject::classOps_ = {
    nullptr,                         /* addProperty */
    nullptr,                         /* delProperty */
    nullptr,                         /* enumerate   */
    nullptr,                         /* newEnumerate */
    nullptr,                         /* resolve     */
    nullptr,                         /* mayResolve  */
    nullptr,                         /* finalize    */
    nullptr,                         /* call        */
    nullptr,                         /* hasInstance */
    nullptr,                         /* construct   */
    CallTraceMethod<DebuggerObject>, /* trace */
};

const JSClass DebuggerObject::class_ = {
    "Object", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS),
    &classOps_};

void DebuggerObject::trace(JSTracer* trc) {
  // There is a barrier on private pointers, so the Unbarriered marking
  // is okay.
  if (JSObject* referent = (JSObject*)getPrivate()) {
    TraceManuallyBarrieredCrossCompartmentEdge(
        trc, static_cast<JSObject*>(this), &referent,
        "Debugger.Object referent");
    setPrivateUnbarriered(referent);
  }
}

static DebuggerObject* DebuggerObject_checkThis(JSContext* cx,
                                                const CallArgs& args,
                                                const char* fnname) {
  JSObject* thisobj = RequireObject(cx, args.thisv());
  if (!thisobj) {
    return nullptr;
  }
  if (thisobj->getClass() != &DebuggerObject::class_) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Debugger.Object",
                              fnname, thisobj->getClass()->name);
    return nullptr;
  }

  // Forbid Debugger.Object.prototype, which is of class DebuggerObject::class_
  // but isn't a real working Debugger.Object. The prototype object is
  // distinguished by having no referent.
  DebuggerObject* nthisobj = &thisobj->as<DebuggerObject>();
  if (!nthisobj->getPrivate()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Debugger.Object",
                              fnname, "prototype object");
    return nullptr;
  }
  return nthisobj;
}

#define THIS_DEBUGOBJECT(cx, argc, vp, fnname, args, object)                   \
  CallArgs args = CallArgsFromVp(argc, vp);                                    \
  RootedDebuggerObject object(cx, DebuggerObject_checkThis(cx, args, fnname)); \
  if (!object) return false;

#define THIS_DEBUGOBJECT_REFERENT(cx, argc, vp, fnname, args, obj)  \
  CallArgs args = CallArgsFromVp(argc, vp);                         \
  RootedObject obj(cx, DebuggerObject_checkThis(cx, args, fnname)); \
  if (!obj) return false;                                           \
  obj = (JSObject*)obj->as<NativeObject>().getPrivate();            \
  MOZ_ASSERT(obj)

#define THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, fnname, args, dbg, obj) \
  CallArgs args = CallArgsFromVp(argc, vp);                                   \
  RootedObject obj(cx, DebuggerObject_checkThis(cx, args, fnname));           \
  if (!obj) return false;                                                     \
  Debugger* dbg = Debugger::fromChildJSObject(obj);                           \
  obj = (JSObject*)obj->as<NativeObject>().getPrivate();                      \
  MOZ_ASSERT(obj)

#define THIS_DEBUGOBJECT_PROMISE(cx, argc, vp, fnname, args, obj)             \
  THIS_DEBUGOBJECT_REFERENT(cx, argc, vp, fnname, args, obj);                 \
  /* We only care about promises, so CheckedUnwrapStatic is OK. */            \
  obj = CheckedUnwrapStatic(obj);                                             \
  if (!obj) {                                                                 \
    ReportAccessDenied(cx);                                                   \
    return false;                                                             \
  }                                                                           \
  if (!obj->is<PromiseObject>()) {                                            \
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,                   \
                              JSMSG_NOT_EXPECTED_TYPE, "Debugger", "Promise", \
                              obj->getClass()->name);                         \
    return false;                                                             \
  }                                                                           \
  Rooted<PromiseObject*> promise(cx, &obj->as<PromiseObject>());

#define THIS_DEBUGOBJECT_OWNER_PROMISE(cx, argc, vp, fnname, args, dbg, obj)  \
  THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, fnname, args, dbg, obj);      \
  /* We only care about promises, so CheckedUnwrapStatic is OK. */            \
  obj = CheckedUnwrapStatic(obj);                                             \
  if (!obj) {                                                                 \
    ReportAccessDenied(cx);                                                   \
    return false;                                                             \
  }                                                                           \
  if (!obj->is<PromiseObject>()) {                                            \
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,                   \
                              JSMSG_NOT_EXPECTED_TYPE, "Debugger", "Promise", \
                              obj->getClass()->name);                         \
    return false;                                                             \
  }                                                                           \
  Rooted<PromiseObject*> promise(cx, &obj->as<PromiseObject>());

/* static */
bool DebuggerObject::construct(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NO_CONSTRUCTOR,
                            "Debugger.Object");
  return false;
}

/* static */
bool DebuggerObject::callableGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get callable", args, object)

  args.rval().setBoolean(object->isCallable());
  return true;
}

/* static */
bool DebuggerObject::isBoundFunctionGetter(JSContext* cx, unsigned argc,
                                           Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get isBoundFunction", args, object)

  if (!object->isDebuggeeFunction()) {
    args.rval().setUndefined();
    return true;
  }

  args.rval().setBoolean(object->isBoundFunction());
  return true;
}

/* static */
bool DebuggerObject::isArrowFunctionGetter(JSContext* cx, unsigned argc,
                                           Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get isArrowFunction", args, object)

  if (!object->isDebuggeeFunction()) {
    args.rval().setUndefined();
    return true;
  }

  args.rval().setBoolean(object->isArrowFunction());
  return true;
}

/* static */
bool DebuggerObject::isAsyncFunctionGetter(JSContext* cx, unsigned argc,
                                           Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get isAsyncFunction", args, object)

  if (!object->isDebuggeeFunction()) {
    args.rval().setUndefined();
    return true;
  }

  args.rval().setBoolean(object->isAsyncFunction());
  return true;
}

/* static */
bool DebuggerObject::isGeneratorFunctionGetter(JSContext* cx, unsigned argc,
                                               Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get isGeneratorFunction", args, object)

  if (!object->isDebuggeeFunction()) {
    args.rval().setUndefined();
    return true;
  }

  args.rval().setBoolean(object->isGeneratorFunction());
  return true;
}

/* static */
bool DebuggerObject::protoGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get proto", args, object)

  RootedDebuggerObject result(cx);
  if (!DebuggerObject::getPrototypeOf(cx, object, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

/* static */
bool DebuggerObject::classGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get class", args, object)

  RootedString result(cx);
  if (!DebuggerObject::getClassName(cx, object, &result)) {
    return false;
  }

  args.rval().setString(result);
  return true;
}

/* static */
bool DebuggerObject::nameGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get name", args, object)

  if (!object->isFunction()) {
    args.rval().setUndefined();
    return true;
  }

  RootedString result(cx, object->name(cx));
  if (result) {
    args.rval().setString(result);
  } else {
    args.rval().setUndefined();
  }
  return true;
}

/* static */
bool DebuggerObject::displayNameGetter(JSContext* cx, unsigned argc,
                                       Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get displayName", args, object)

  if (!object->isFunction()) {
    args.rval().setUndefined();
    return true;
  }

  RootedString result(cx, object->displayName(cx));
  if (result) {
    args.rval().setString(result);
  } else {
    args.rval().setUndefined();
  }
  return true;
}

/* static */
bool DebuggerObject::parameterNamesGetter(JSContext* cx, unsigned argc,
                                          Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get parameterNames", args, object)

  if (!object->isDebuggeeFunction()) {
    args.rval().setUndefined();
    return true;
  }

  Rooted<StringVector> names(cx, StringVector(cx));
  if (!DebuggerObject::getParameterNames(cx, object, &names)) {
    return false;
  }

  RootedArrayObject obj(cx, NewDenseFullyAllocatedArray(cx, names.length()));
  if (!obj) {
    return false;
  }

  obj->ensureDenseInitializedLength(cx, 0, names.length());
  for (size_t i = 0; i < names.length(); ++i) {
    Value v;
    if (names[i]) {
      v = StringValue(names[i]);
    } else {
      v = UndefinedValue();
    }
    obj->setDenseElement(i, v);
  }

  args.rval().setObject(*obj);
  return true;
}

/* static */
bool DebuggerObject::scriptGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "get script", args, dbg, obj);

  if (!obj->is<JSFunction>()) {
    args.rval().setUndefined();
    return true;
  }

  RootedFunction fun(cx, &obj->as<JSFunction>());
  if (!IsInterpretedNonSelfHostedFunction(fun)) {
    args.rval().setUndefined();
    return true;
  }

  RootedScript script(cx, GetOrCreateFunctionScript(cx, fun));
  if (!script) {
    return false;
  }

  // Only hand out debuggee scripts.
  if (!dbg->observesScript(script)) {
    args.rval().setNull();
    return true;
  }

  RootedDebuggerScript scriptObject(cx, dbg->wrapScript(cx, script));
  if (!scriptObject) {
    return false;
  }

  args.rval().setObject(*scriptObject);
  return true;
}

/* static */
bool DebuggerObject::environmentGetter(JSContext* cx, unsigned argc,
                                       Value* vp) {
  THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "get environment", args, dbg,
                                  obj);

  // Don't bother switching compartments just to check obj's type and get its
  // env.
  if (!obj->is<JSFunction>()) {
    args.rval().setUndefined();
    return true;
  }

  RootedFunction fun(cx, &obj->as<JSFunction>());
  if (!IsInterpretedNonSelfHostedFunction(fun)) {
    args.rval().setUndefined();
    return true;
  }

  // Only hand out environments of debuggee functions.
  if (!dbg->observesGlobal(&fun->global())) {
    args.rval().setNull();
    return true;
  }

  Rooted<Env*> env(cx);
  {
    AutoRealm ar(cx, fun);
    env = GetDebugEnvironmentForFunction(cx, fun);
    if (!env) {
      return false;
    }
  }

  return dbg->wrapEnvironment(cx, env, args.rval());
}

/* static */
bool DebuggerObject::boundTargetFunctionGetter(JSContext* cx, unsigned argc,
                                               Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get boundTargetFunction", args, object)

  if (!object->isDebuggeeFunction() || !object->isBoundFunction()) {
    args.rval().setUndefined();
    return true;
  }

  RootedDebuggerObject result(cx);
  if (!DebuggerObject::getBoundTargetFunction(cx, object, &result)) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/* static */
bool DebuggerObject::boundThisGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get boundThis", args, object)

  if (!object->isDebuggeeFunction() || !object->isBoundFunction()) {
    args.rval().setUndefined();
    return true;
  }

  return DebuggerObject::getBoundThis(cx, object, args.rval());
}

/* static */
bool DebuggerObject::boundArgumentsGetter(JSContext* cx, unsigned argc,
                                          Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get boundArguments", args, object)

  if (!object->isDebuggeeFunction() || !object->isBoundFunction()) {
    args.rval().setUndefined();
    return true;
  }

  Rooted<ValueVector> result(cx, ValueVector(cx));
  if (!DebuggerObject::getBoundArguments(cx, object, &result)) {
    return false;
  }

  RootedObject obj(cx,
                   NewDenseCopiedArray(cx, result.length(), result.begin()));
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/* static */
bool DebuggerObject::allocationSiteGetter(JSContext* cx, unsigned argc,
                                          Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get allocationSite", args, object)

  RootedObject result(cx);
  if (!DebuggerObject::getAllocationSite(cx, object, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

// Returns the "name" field (see js.msg), which may be used as a unique
// identifier, for any error object with a JSErrorReport or undefined
// if the object has no JSErrorReport.
/* static */
bool DebuggerObject::errorMessageNameGetter(JSContext* cx, unsigned argc,
                                            Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get errorMessageName", args, object)

  RootedString result(cx);
  if (!DebuggerObject::getErrorMessageName(cx, object, &result)) {
    return false;
  }

  if (result) {
    args.rval().setString(result);
  } else {
    args.rval().setUndefined();
  }
  return true;
}

/* static */
bool DebuggerObject::errorNotesGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get errorNotes", args, object)

  return DebuggerObject::getErrorNotes(cx, object, args.rval());
}

/* static */
bool DebuggerObject::errorLineNumberGetter(JSContext* cx, unsigned argc,
                                           Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get errorLineNumber", args, object)

  return DebuggerObject::getErrorLineNumber(cx, object, args.rval());
}

/* static */
bool DebuggerObject::errorColumnNumberGetter(JSContext* cx, unsigned argc,
                                             Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get errorColumnNumber", args, object)

  return DebuggerObject::getErrorColumnNumber(cx, object, args.rval());
}

/* static */
bool DebuggerObject::isProxyGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get isProxy", args, object)

  args.rval().setBoolean(object->isScriptedProxy());
  return true;
}

/* static */
bool DebuggerObject::proxyTargetGetter(JSContext* cx, unsigned argc,
                                       Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get proxyTarget", args, object)

  if (!object->isScriptedProxy()) {
    args.rval().setUndefined();
    return true;
  }

  Rooted<DebuggerObject*> result(cx);
  if (!DebuggerObject::getScriptedProxyTarget(cx, object, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

/* static */
bool DebuggerObject::proxyHandlerGetter(JSContext* cx, unsigned argc,
                                        Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get proxyHandler", args, object)

  if (!object->isScriptedProxy()) {
    args.rval().setUndefined();
    return true;
  }
  Rooted<DebuggerObject*> result(cx);
  if (!DebuggerObject::getScriptedProxyHandler(cx, object, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

/* static */
bool DebuggerObject::isPromiseGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get isPromise", args, object)

  args.rval().setBoolean(object->isPromise());
  return true;
}

/* static */
bool DebuggerObject::promiseStateGetter(JSContext* cx, unsigned argc,
                                        Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get promiseState", args, object);

  if (!DebuggerObject::requirePromise(cx, object)) {
    return false;
  }

  RootedValue result(cx);
  switch (object->promiseState()) {
    case JS::PromiseState::Pending:
      result.setString(cx->names().pending);
      break;
    case JS::PromiseState::Fulfilled:
      result.setString(cx->names().fulfilled);
      break;
    case JS::PromiseState::Rejected:
      result.setString(cx->names().rejected);
      break;
  }

  args.rval().set(result);
  return true;
}

/* static */
bool DebuggerObject::promiseValueGetter(JSContext* cx, unsigned argc,
                                        Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get promiseValue", args, object);

  if (!DebuggerObject::requirePromise(cx, object)) {
    return false;
  }

  if (object->promiseState() != JS::PromiseState::Fulfilled) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_PROMISE_NOT_FULFILLED);
    return false;
  }

  return DebuggerObject::getPromiseValue(cx, object, args.rval());
  ;
}

/* static */
bool DebuggerObject::promiseReasonGetter(JSContext* cx, unsigned argc,
                                         Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get promiseReason", args, object);

  if (!DebuggerObject::requirePromise(cx, object)) {
    return false;
  }

  if (object->promiseState() != JS::PromiseState::Rejected) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_PROMISE_NOT_REJECTED);
    return false;
  }

  return DebuggerObject::getPromiseReason(cx, object, args.rval());
  ;
}

/* static */
bool DebuggerObject::promiseLifetimeGetter(JSContext* cx, unsigned argc,
                                           Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get promiseLifetime", args, object);

  if (!DebuggerObject::requirePromise(cx, object)) {
    return false;
  }

  args.rval().setNumber(object->promiseLifetime());
  return true;
}

/* static */
bool DebuggerObject::promiseTimeToResolutionGetter(JSContext* cx, unsigned argc,
                                                   Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "get promiseTimeToResolution", args, object);

  if (!DebuggerObject::requirePromise(cx, object)) {
    return false;
  }

  if (object->promiseState() == JS::PromiseState::Pending) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_PROMISE_NOT_RESOLVED);
    return false;
  }

  args.rval().setNumber(object->promiseTimeToResolution());
  return true;
}

/* static */
bool DebuggerObject::promiseAllocationSiteGetter(JSContext* cx, unsigned argc,
                                                 Value* vp) {
  THIS_DEBUGOBJECT_PROMISE(cx, argc, vp, "get promiseAllocationSite", args,
                           refobj);

  RootedObject allocSite(cx, promise->allocationSite());
  if (!allocSite) {
    args.rval().setNull();
    return true;
  }

  if (!cx->compartment()->wrap(cx, &allocSite)) {
    return false;
  }
  args.rval().set(ObjectValue(*allocSite));
  return true;
}

/* static */
bool DebuggerObject::promiseResolutionSiteGetter(JSContext* cx, unsigned argc,
                                                 Value* vp) {
  THIS_DEBUGOBJECT_PROMISE(cx, argc, vp, "get promiseResolutionSite", args,
                           refobj);

  if (promise->state() == JS::PromiseState::Pending) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_PROMISE_NOT_RESOLVED);
    return false;
  }

  RootedObject resolutionSite(cx, promise->resolutionSite());
  if (!resolutionSite) {
    args.rval().setNull();
    return true;
  }

  if (!cx->compartment()->wrap(cx, &resolutionSite)) {
    return false;
  }
  args.rval().set(ObjectValue(*resolutionSite));
  return true;
}

/* static */
bool DebuggerObject::promiseIDGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT_PROMISE(cx, argc, vp, "get promiseID", args, refobj);

  args.rval().setNumber(double(promise->getID()));
  return true;
}

/* static */
bool DebuggerObject::promiseDependentPromisesGetter(JSContext* cx,
                                                    unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT_OWNER_PROMISE(cx, argc, vp, "get promiseDependentPromises",
                                 args, dbg, refobj);

  Rooted<GCVector<Value>> values(cx, GCVector<Value>(cx));
  {
    JSAutoRealm ar(cx, promise);
    if (!promise->dependentPromises(cx, &values)) {
      return false;
    }
  }
  for (size_t i = 0; i < values.length(); i++) {
    if (!dbg->wrapDebuggeeValue(cx, values[i])) {
      return false;
    }
  }
  RootedArrayObject promises(cx);
  if (values.length() == 0) {
    promises = NewDenseEmptyArray(cx);
  } else {
    promises = NewDenseCopiedArray(cx, values.length(), values[0].address());
  }
  if (!promises) {
    return false;
  }
  args.rval().setObject(*promises);
  return true;
}

/* static */
bool DebuggerObject::isExtensibleMethod(JSContext* cx, unsigned argc,
                                        Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "isExtensible", args, object)

  bool result;
  if (!DebuggerObject::isExtensible(cx, object, result)) {
    return false;
  }

  args.rval().setBoolean(result);
  return true;
}

/* static */
bool DebuggerObject::isSealedMethod(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "isSealed", args, object)

  bool result;
  if (!DebuggerObject::isSealed(cx, object, result)) {
    return false;
  }

  args.rval().setBoolean(result);
  return true;
}

/* static */
bool DebuggerObject::isFrozenMethod(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "isFrozen", args, object)

  bool result;
  if (!DebuggerObject::isFrozen(cx, object, result)) {
    return false;
  }

  args.rval().setBoolean(result);
  return true;
}

/* static */
bool DebuggerObject::getOwnPropertyNamesMethod(JSContext* cx, unsigned argc,
                                               Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "getOwnPropertyNames", args, object)

  Rooted<IdVector> ids(cx, IdVector(cx));
  if (!DebuggerObject::getOwnPropertyNames(cx, object, &ids)) {
    return false;
  }

  RootedObject obj(cx, IdVectorToArray(cx, ids));
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/* static */
bool DebuggerObject::getOwnPropertySymbolsMethod(JSContext* cx, unsigned argc,
                                                 Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "getOwnPropertySymbols", args, object)

  Rooted<IdVector> ids(cx, IdVector(cx));
  if (!DebuggerObject::getOwnPropertySymbols(cx, object, &ids)) {
    return false;
  }

  RootedObject obj(cx, IdVectorToArray(cx, ids));
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/* static */
bool DebuggerObject::getOwnPropertyDescriptorMethod(JSContext* cx,
                                                    unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "getOwnPropertyDescriptor", args, object)

  RootedId id(cx);
  if (!ValueToId<CanGC>(cx, args.get(0), &id)) {
    return false;
  }

  Rooted<PropertyDescriptor> desc(cx);
  if (!DebuggerObject::getOwnPropertyDescriptor(cx, object, id, &desc)) {
    return false;
  }

  return JS::FromPropertyDescriptor(cx, desc, args.rval());
}

/* static */
bool DebuggerObject::preventExtensionsMethod(JSContext* cx, unsigned argc,
                                             Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "preventExtensions", args, object)

  if (!DebuggerObject::preventExtensions(cx, object)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool DebuggerObject::sealMethod(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "seal", args, object)

  if (!DebuggerObject::seal(cx, object)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool DebuggerObject::freezeMethod(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "freeze", args, object)

  if (!DebuggerObject::freeze(cx, object)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool DebuggerObject::definePropertyMethod(JSContext* cx, unsigned argc,
                                          Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "defineProperty", args, object)
  if (!args.requireAtLeast(cx, "Debugger.Object.defineProperty", 2)) {
    return false;
  }

  RootedId id(cx);
  if (!ValueToId<CanGC>(cx, args[0], &id)) {
    return false;
  }

  Rooted<PropertyDescriptor> desc(cx);
  if (!ToPropertyDescriptor(cx, args[1], false, &desc)) {
    return false;
  }

  if (!DebuggerObject::defineProperty(cx, object, id, desc)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool DebuggerObject::definePropertiesMethod(JSContext* cx, unsigned argc,
                                            Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "defineProperties", args, object);
  if (!args.requireAtLeast(cx, "Debugger.Object.defineProperties", 1)) {
    return false;
  }

  RootedValue arg(cx, args[0]);
  RootedObject props(cx, ToObject(cx, arg));
  if (!props) {
    return false;
  }
  RootedIdVector ids(cx);
  Rooted<PropertyDescriptorVector> descs(cx, PropertyDescriptorVector(cx));
  if (!ReadPropertyDescriptors(cx, props, false, &ids, &descs)) {
    return false;
  }
  Rooted<IdVector> ids2(cx, IdVector(cx));
  if (!ids2.append(ids.begin(), ids.end())) {
    return false;
  }

  if (!DebuggerObject::defineProperties(cx, object, ids2, descs)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/*
 * This does a non-strict delete, as a matter of API design. The case where the
 * property is non-configurable isn't necessarily exceptional here.
 */
/* static */
bool DebuggerObject::deletePropertyMethod(JSContext* cx, unsigned argc,
                                          Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "deleteProperty", args, object)

  RootedId id(cx);
  if (!ValueToId<CanGC>(cx, args.get(0), &id)) {
    return false;
  }

  ObjectOpResult result;
  if (!DebuggerObject::deleteProperty(cx, object, id, result)) {
    return false;
  }

  args.rval().setBoolean(result.ok());
  return true;
}

/* static */
bool DebuggerObject::callMethod(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "call", callArgs, object);

  RootedValue thisv(cx, callArgs.get(0));

  Rooted<ValueVector> args(cx, ValueVector(cx));
  if (callArgs.length() >= 2) {
    if (!args.growBy(callArgs.length() - 1)) {
      return false;
    }
    for (size_t i = 1; i < callArgs.length(); ++i) {
      args[i - 1].set(callArgs[i]);
    }
  }

  Rooted<Maybe<Completion>> completion(
      cx, DebuggerObject::call(cx, object, thisv, args));
  if (!completion.get()) {
    return false;
  }

  return completion->buildCompletionValue(cx, object->owner(), callArgs.rval());
}

/* static */
bool DebuggerObject::getPropertyMethod(JSContext* cx, unsigned argc,
                                       Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "getProperty", args, object)
  Debugger* dbg = Debugger::fromChildJSObject(object);

  RootedId id(cx);
  if (!ValueToId<CanGC>(cx, args.get(0), &id)) {
    return false;
  }

  RootedValue receiver(cx,
                       args.length() < 2 ? ObjectValue(*object) : args.get(1));

  Rooted<Completion> comp(cx);
  JS_TRY_VAR_OR_RETURN_FALSE(cx, comp, getProperty(cx, object, id, receiver));
  return comp.get().buildCompletionValue(cx, dbg, args.rval());
}

/* static */
bool DebuggerObject::setPropertyMethod(JSContext* cx, unsigned argc,
                                       Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "setProperty", args, object)
  Debugger* dbg = Debugger::fromChildJSObject(object);

  RootedId id(cx);
  if (!ValueToId<CanGC>(cx, args.get(0), &id)) {
    return false;
  }

  RootedValue value(cx, args.get(1));

  RootedValue receiver(cx,
                       args.length() < 3 ? ObjectValue(*object) : args.get(2));

  Rooted<Completion> comp(cx);
  JS_TRY_VAR_OR_RETURN_FALSE(cx, comp,
                             setProperty(cx, object, id, value, receiver));
  return comp.get().buildCompletionValue(cx, dbg, args.rval());
}

/* static */
bool DebuggerObject::applyMethod(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "apply", callArgs, object);

  RootedValue thisv(cx, callArgs.get(0));

  Rooted<ValueVector> args(cx, ValueVector(cx));
  if (callArgs.length() >= 2 && !callArgs[1].isNullOrUndefined()) {
    if (!callArgs[1].isObject()) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_BAD_APPLY_ARGS, js_apply_str);
      return false;
    }

    RootedObject argsobj(cx, &callArgs[1].toObject());

    unsigned argc = 0;
    if (!GetLengthProperty(cx, argsobj, &argc)) {
      return false;
    }
    argc = unsigned(Min(argc, ARGS_LENGTH_MAX));

    if (!args.growBy(argc) || !GetElements(cx, argsobj, argc, args.begin())) {
      return false;
    }
  }

  Rooted<Maybe<Completion>> completion(
      cx, DebuggerObject::call(cx, object, thisv, args));
  if (!completion.get()) {
    return false;
  }

  return completion->buildCompletionValue(cx, object->owner(), callArgs.rval());
}

static void EnterDebuggeeObjectRealm(JSContext* cx, Maybe<AutoRealm>& ar,
                                     JSObject* referent) {
  // |referent| may be a cross-compartment wrapper and CCWs normally
  // shouldn't be used with AutoRealm, but here we use an arbitrary realm for
  // now because we don't really have another option.
  ar.emplace(cx, referent->maybeCCWRealm()->maybeGlobal());
}

static bool RequireGlobalObject(JSContext* cx, HandleValue dbgobj,
                                HandleObject referent) {
  RootedObject obj(cx, referent);

  if (!obj->is<GlobalObject>()) {
    const char* isWrapper = "";
    const char* isWindowProxy = "";

    // Help the poor programmer by pointing out wrappers around globals...
    if (obj->is<WrapperObject>()) {
      obj = js::UncheckedUnwrap(obj);
      isWrapper = "a wrapper around ";
    }

    // ... and WindowProxies around Windows.
    if (IsWindowProxy(obj)) {
      obj = ToWindowIfWindowProxy(obj);
      isWindowProxy = "a WindowProxy referring to ";
    }

    if (obj->is<GlobalObject>()) {
      ReportValueError(cx, JSMSG_DEBUG_WRAPPER_IN_WAY, JSDVG_SEARCH_STACK,
                       dbgobj, nullptr, isWrapper, isWindowProxy);
    } else {
      ReportValueError(cx, JSMSG_DEBUG_BAD_REFERENT, JSDVG_SEARCH_STACK, dbgobj,
                       nullptr, "a global object");
    }
    return false;
  }

  return true;
}

/* static */
bool DebuggerObject::asEnvironmentMethod(JSContext* cx, unsigned argc,
                                         Value* vp) {
  THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "asEnvironment", args, dbg,
                                  referent);
  if (!RequireGlobalObject(cx, args.thisv(), referent)) {
    return false;
  }

  Rooted<Env*> env(cx);
  {
    AutoRealm ar(cx, referent);
    env = GetDebugEnvironmentForGlobalLexicalEnvironment(cx);
    if (!env) {
      return false;
    }
  }

  return dbg->wrapEnvironment(cx, env, args.rval());
}

// Lookup a binding on the referent's global scope and change it to undefined
// if it is an uninitialized lexical, otherwise do nothing. The method's
// JavaScript return value is true _only_ when an uninitialized lexical has been
// altered, otherwise it is false.
/* static */
bool DebuggerObject::forceLexicalInitializationByNameMethod(JSContext* cx,
                                                            unsigned argc,
                                                            Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "forceLexicalInitializationByName", args,
                   object)
  if (!args.requireAtLeast(
          cx, "Debugger.Object.prototype.forceLexicalInitializationByName",
          1)) {
    return false;
  }

  if (!DebuggerObject::requireGlobal(cx, object)) {
    return false;
  }

  RootedId id(cx);
  if (!ValueToIdentifier(cx, args[0], &id)) {
    return false;
  }

  bool result;
  if (!DebuggerObject::forceLexicalInitializationByName(cx, object, id,
                                                        result)) {
    return false;
  }

  args.rval().setBoolean(result);
  return true;
}

/* static */
bool DebuggerObject::executeInGlobalMethod(JSContext* cx, unsigned argc,
                                           Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "executeInGlobal", args, object);
  if (!args.requireAtLeast(cx, "Debugger.Object.prototype.executeInGlobal",
                           1)) {
    return false;
  }

  if (!DebuggerObject::requireGlobal(cx, object)) {
    return false;
  }

  AutoStableStringChars stableChars(cx);
  if (!ValueToStableChars(cx, "Debugger.Object.prototype.executeInGlobal",
                          args[0], stableChars)) {
    return false;
  }
  mozilla::Range<const char16_t> chars = stableChars.twoByteRange();

  EvalOptions options;
  if (!ParseEvalOptions(cx, args.get(1), options)) {
    return false;
  }

  Rooted<Completion> comp(cx);
  JS_TRY_VAR_OR_RETURN_FALSE(
      cx, comp,
      DebuggerObject::executeInGlobal(cx, object, chars, nullptr, options));
  return comp.get().buildCompletionValue(cx, object->owner(), args.rval());
}

/* static */
bool DebuggerObject::executeInGlobalWithBindingsMethod(JSContext* cx,
                                                       unsigned argc,
                                                       Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "executeInGlobalWithBindings", args, object);
  if (!args.requireAtLeast(
          cx, "Debugger.Object.prototype.executeInGlobalWithBindings", 2)) {
    return false;
  }

  if (!DebuggerObject::requireGlobal(cx, object)) {
    return false;
  }

  AutoStableStringChars stableChars(cx);
  if (!ValueToStableChars(
          cx, "Debugger.Object.prototype.executeInGlobalWithBindings", args[0],
          stableChars)) {
    return false;
  }
  mozilla::Range<const char16_t> chars = stableChars.twoByteRange();

  RootedObject bindings(cx, RequireObject(cx, args[1]));
  if (!bindings) {
    return false;
  }

  EvalOptions options;
  if (!ParseEvalOptions(cx, args.get(2), options)) {
    return false;
  }

  Rooted<Completion> comp(cx);
  JS_TRY_VAR_OR_RETURN_FALSE(
      cx, comp,
      DebuggerObject::executeInGlobal(cx, object, chars, bindings, options));
  return comp.get().buildCompletionValue(cx, object->owner(), args.rval());
}

/* static */
bool DebuggerObject::makeDebuggeeValueMethod(JSContext* cx, unsigned argc,
                                             Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "makeDebuggeeValue", args, object);
  if (!args.requireAtLeast(cx, "Debugger.Object.prototype.makeDebuggeeValue",
                           1)) {
    return false;
  }

  return DebuggerObject::makeDebuggeeValue(cx, object, args[0], args.rval());
}

/* static */
bool DebuggerObject::makeDebuggeeNativeFunctionMethod(JSContext* cx,
                                                      unsigned argc,
                                                      Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "makeDebuggeeNativeFunction", args, object);
  if (!args.requireAtLeast(
          cx, "Debugger.Object.prototype.makeDebuggeeNativeFunction", 1)) {
    return false;
  }

  return DebuggerObject::makeDebuggeeNativeFunction(cx, object, args[0],
                                                    args.rval());
}

/* static */
bool DebuggerObject::unsafeDereferenceMethod(JSContext* cx, unsigned argc,
                                             Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "unsafeDereference", args, object);

  RootedObject result(cx);
  if (!DebuggerObject::unsafeDereference(cx, object, &result)) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/* static */
bool DebuggerObject::unwrapMethod(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "unwrap", args, object);

  RootedDebuggerObject result(cx);
  if (!DebuggerObject::unwrap(cx, object, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

/* static */
bool DebuggerObject::setInstrumentationMethod(JSContext* cx, unsigned argc,
                                              Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "setInstrumentation", args, object);

  if (!args.requireAtLeast(cx, "Debugger.Object.prototype.setInstrumentation",
                           2)) {
    return false;
  }

  if (!DebuggerObject::requireGlobal(cx, object)) {
    return false;
  }
  RootedGlobalObject global(cx, &object->referent()->as<GlobalObject>());

  RootedValue v(cx, args[0]);
  if (!object->owner()->unwrapDebuggeeValue(cx, &v)) {
    return false;
  }
  if (!v.isObject()) {
    JS_ReportErrorASCII(cx, "Instrumentation callback must be an object");
    return false;
  }
  RootedObject callback(cx, &v.toObject());

  if (!args[1].isObject()) {
    JS_ReportErrorASCII(cx, "Instrumentation kinds must be an object");
    return false;
  }
  RootedObject kindsObj(cx, &args[1].toObject());

  unsigned length = 0;
  if (!GetLengthProperty(cx, kindsObj, &length)) {
    return false;
  }

  Rooted<ValueVector> values(cx, ValueVector(cx));
  if (!values.growBy(length) ||
      !GetElements(cx, kindsObj, length, values.begin())) {
    return false;
  }

  Rooted<StringVector> kinds(cx, StringVector(cx));
  for (size_t i = 0; i < values.length(); i++) {
    if (!values[i].isString()) {
      JS_ReportErrorASCII(cx, "Instrumentation kind must be a string");
      return false;
    }
    if (!kinds.append(values[i].toString())) {
      return false;
    }
  }

  {
    AutoRealm ar(cx, global);
    RootedObject dbgObject(cx, object->owner()->toJSObject());
    if (!RealmInstrumentation::install(cx, global, callback, dbgObject,
                                       kinds)) {
      return false;
    }
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool DebuggerObject::setInstrumentationActiveMethod(JSContext* cx,
                                                    unsigned argc, Value* vp) {
  THIS_DEBUGOBJECT(cx, argc, vp, "setInstrumentationActive", args, object);

  if (!DebuggerObject::requireGlobal(cx, object)) {
    return false;
  }

  if (!args.requireAtLeast(
          cx, "Debugger.Object.prototype.setInstrumentationActive", 1)) {
    return false;
  }

  RootedGlobalObject global(cx, &object->referent()->as<GlobalObject>());
  bool active = ToBoolean(args[0]);

  {
    AutoRealm ar(cx, global);
    if (!RealmInstrumentation::setActive(cx, global, object->owner(), active)) {
      return false;
    }
  }

  args.rval().setUndefined();
  return true;
}

const JSPropertySpec DebuggerObject::properties_[] = {
    JS_PSG("callable", DebuggerObject::callableGetter, 0),
    JS_PSG("isBoundFunction", DebuggerObject::isBoundFunctionGetter, 0),
    JS_PSG("isArrowFunction", DebuggerObject::isArrowFunctionGetter, 0),
    JS_PSG("isGeneratorFunction", DebuggerObject::isGeneratorFunctionGetter, 0),
    JS_PSG("isAsyncFunction", DebuggerObject::isAsyncFunctionGetter, 0),
    JS_PSG("proto", DebuggerObject::protoGetter, 0),
    JS_PSG("class", DebuggerObject::classGetter, 0),
    JS_PSG("name", DebuggerObject::nameGetter, 0),
    JS_PSG("displayName", DebuggerObject::displayNameGetter, 0),
    JS_PSG("parameterNames", DebuggerObject::parameterNamesGetter, 0),
    JS_PSG("script", DebuggerObject::scriptGetter, 0),
    JS_PSG("environment", DebuggerObject::environmentGetter, 0),
    JS_PSG("boundTargetFunction", DebuggerObject::boundTargetFunctionGetter, 0),
    JS_PSG("boundThis", DebuggerObject::boundThisGetter, 0),
    JS_PSG("boundArguments", DebuggerObject::boundArgumentsGetter, 0),
    JS_PSG("allocationSite", DebuggerObject::allocationSiteGetter, 0),
    JS_PSG("errorMessageName", DebuggerObject::errorMessageNameGetter, 0),
    JS_PSG("errorNotes", DebuggerObject::errorNotesGetter, 0),
    JS_PSG("errorLineNumber", DebuggerObject::errorLineNumberGetter, 0),
    JS_PSG("errorColumnNumber", DebuggerObject::errorColumnNumberGetter, 0),
    JS_PSG("isProxy", DebuggerObject::isProxyGetter, 0),
    JS_PSG("proxyTarget", DebuggerObject::proxyTargetGetter, 0),
    JS_PSG("proxyHandler", DebuggerObject::proxyHandlerGetter, 0),
    JS_PS_END};

const JSPropertySpec DebuggerObject::promiseProperties_[] = {
    JS_PSG("isPromise", DebuggerObject::isPromiseGetter, 0),
    JS_PSG("promiseState", DebuggerObject::promiseStateGetter, 0),
    JS_PSG("promiseValue", DebuggerObject::promiseValueGetter, 0),
    JS_PSG("promiseReason", DebuggerObject::promiseReasonGetter, 0),
    JS_PSG("promiseLifetime", DebuggerObject::promiseLifetimeGetter, 0),
    JS_PSG("promiseTimeToResolution",
           DebuggerObject::promiseTimeToResolutionGetter, 0),
    JS_PSG("promiseAllocationSite", DebuggerObject::promiseAllocationSiteGetter,
           0),
    JS_PSG("promiseResolutionSite", DebuggerObject::promiseResolutionSiteGetter,
           0),
    JS_PSG("promiseID", DebuggerObject::promiseIDGetter, 0),
    JS_PSG("promiseDependentPromises",
           DebuggerObject::promiseDependentPromisesGetter, 0),
    JS_PS_END};

const JSFunctionSpec DebuggerObject::methods_[] = {
    JS_FN("isExtensible", DebuggerObject::isExtensibleMethod, 0, 0),
    JS_FN("isSealed", DebuggerObject::isSealedMethod, 0, 0),
    JS_FN("isFrozen", DebuggerObject::isFrozenMethod, 0, 0),
    JS_FN("getProperty", DebuggerObject::getPropertyMethod, 0, 0),
    JS_FN("setProperty", DebuggerObject::setPropertyMethod, 0, 0),
    JS_FN("getOwnPropertyNames", DebuggerObject::getOwnPropertyNamesMethod, 0,
          0),
    JS_FN("getOwnPropertySymbols", DebuggerObject::getOwnPropertySymbolsMethod,
          0, 0),
    JS_FN("getOwnPropertyDescriptor",
          DebuggerObject::getOwnPropertyDescriptorMethod, 1, 0),
    JS_FN("preventExtensions", DebuggerObject::preventExtensionsMethod, 0, 0),
    JS_FN("seal", DebuggerObject::sealMethod, 0, 0),
    JS_FN("freeze", DebuggerObject::freezeMethod, 0, 0),
    JS_FN("defineProperty", DebuggerObject::definePropertyMethod, 2, 0),
    JS_FN("defineProperties", DebuggerObject::definePropertiesMethod, 1, 0),
    JS_FN("deleteProperty", DebuggerObject::deletePropertyMethod, 1, 0),
    JS_FN("call", DebuggerObject::callMethod, 0, 0),
    JS_FN("apply", DebuggerObject::applyMethod, 0, 0),
    JS_FN("asEnvironment", DebuggerObject::asEnvironmentMethod, 0, 0),
    JS_FN("forceLexicalInitializationByName",
          DebuggerObject::forceLexicalInitializationByNameMethod, 1, 0),
    JS_FN("executeInGlobal", DebuggerObject::executeInGlobalMethod, 1, 0),
    JS_FN("executeInGlobalWithBindings",
          DebuggerObject::executeInGlobalWithBindingsMethod, 2, 0),
    JS_FN("makeDebuggeeValue", DebuggerObject::makeDebuggeeValueMethod, 1, 0),
    JS_FN("makeDebuggeeNativeFunction",
          DebuggerObject::makeDebuggeeNativeFunctionMethod, 1, 0),
    JS_FN("unsafeDereference", DebuggerObject::unsafeDereferenceMethod, 0, 0),
    JS_FN("unwrap", DebuggerObject::unwrapMethod, 0, 0),
    JS_FN("setInstrumentation", DebuggerObject::setInstrumentationMethod, 2, 0),
    JS_FN("setInstrumentationActive",
          DebuggerObject::setInstrumentationActiveMethod, 1, 0),
    JS_FS_END};

/* static */
NativeObject* DebuggerObject::initClass(JSContext* cx,
                                        Handle<GlobalObject*> global,
                                        HandleObject debugCtor) {
  RootedNativeObject objectProto(
      cx, InitClass(cx, debugCtor, nullptr, &class_, construct, 0, properties_,
                    methods_, nullptr, nullptr));

  if (!objectProto) {
    return nullptr;
  }

  if (!DefinePropertiesAndFunctions(cx, objectProto, promiseProperties_,
                                    nullptr)) {
    return nullptr;
  }

  return objectProto;
}

/* static */
DebuggerObject* DebuggerObject::create(JSContext* cx, HandleObject proto,
                                       HandleObject referent,
                                       HandleNativeObject debugger) {
  NewObjectKind newKind =
      IsInsideNursery(referent) ? GenericObject : TenuredObject;
  DebuggerObject* obj =
      NewObjectWithGivenProto<DebuggerObject>(cx, proto, newKind);
  if (!obj) {
    return nullptr;
  }

  obj->setPrivateGCThing(referent);
  obj->setReservedSlot(JSSLOT_DEBUGOBJECT_OWNER, ObjectValue(*debugger));

  return obj;
}

bool DebuggerObject::isCallable() const { return referent()->isCallable(); }

bool DebuggerObject::isFunction() const { return referent()->is<JSFunction>(); }

bool DebuggerObject::isDebuggeeFunction() const {
  return referent()->is<JSFunction>() &&
         owner()->observesGlobal(&referent()->as<JSFunction>().global());
}

bool DebuggerObject::isBoundFunction() const {
  MOZ_ASSERT(isDebuggeeFunction());

  return referent()->isBoundFunction();
}

bool DebuggerObject::isArrowFunction() const {
  MOZ_ASSERT(isDebuggeeFunction());

  return referent()->as<JSFunction>().isArrow();
}

bool DebuggerObject::isAsyncFunction() const {
  MOZ_ASSERT(isDebuggeeFunction());

  return referent()->as<JSFunction>().isAsync();
}

bool DebuggerObject::isGeneratorFunction() const {
  MOZ_ASSERT(isDebuggeeFunction());

  return referent()->as<JSFunction>().isGenerator();
}

bool DebuggerObject::isGlobal() const { return referent()->is<GlobalObject>(); }

bool DebuggerObject::isScriptedProxy() const {
  return js::IsScriptedProxy(referent());
}

bool DebuggerObject::isPromise() const {
  JSObject* referent = this->referent();

  if (IsCrossCompartmentWrapper(referent)) {
    /* We only care about promises, so CheckedUnwrapStatic is OK. */
    referent = CheckedUnwrapStatic(referent);
    if (!referent) {
      return false;
    }
  }

  return referent->is<PromiseObject>();
}

/* static */
bool DebuggerObject::getClassName(JSContext* cx, HandleDebuggerObject object,
                                  MutableHandleString result) {
  RootedObject referent(cx, object->referent());

  const char* className;
  {
    Maybe<AutoRealm> ar;
    EnterDebuggeeObjectRealm(cx, ar, referent);
    className = GetObjectClassName(cx, referent);
  }

  JSAtom* str = Atomize(cx, className, strlen(className));
  if (!str) {
    return false;
  }

  result.set(str);
  return true;
}

JSAtom* DebuggerObject::name(JSContext* cx) const {
  MOZ_ASSERT(isFunction());

  JSAtom* atom = referent()->as<JSFunction>().explicitName();
  if (atom) {
    cx->markAtom(atom);
  }
  return atom;
}

JSAtom* DebuggerObject::displayName(JSContext* cx) const {
  MOZ_ASSERT(isFunction());

  JSAtom* atom = referent()->as<JSFunction>().displayAtom();
  if (atom) {
    cx->markAtom(atom);
  }
  return atom;
}

JS::PromiseState DebuggerObject::promiseState() const {
  return promise()->state();
}

double DebuggerObject::promiseLifetime() const { return promise()->lifetime(); }

double DebuggerObject::promiseTimeToResolution() const {
  MOZ_ASSERT(promiseState() != JS::PromiseState::Pending);

  return promise()->timeToResolution();
}

/* static */
bool DebuggerObject::getParameterNames(JSContext* cx,
                                       HandleDebuggerObject object,
                                       MutableHandle<StringVector> result) {
  MOZ_ASSERT(object->isDebuggeeFunction());

  RootedFunction referent(cx, &object->referent()->as<JSFunction>());

  if (!result.growBy(referent->nargs())) {
    return false;
  }
  if (IsInterpretedNonSelfHostedFunction(referent)) {
    RootedScript script(cx, GetOrCreateFunctionScript(cx, referent));
    if (!script) {
      return false;
    }

    MOZ_ASSERT(referent->nargs() == script->numArgs());

    if (referent->nargs() > 0) {
      PositionalFormalParameterIter fi(script);
      for (size_t i = 0; i < referent->nargs(); i++, fi++) {
        MOZ_ASSERT(fi.argumentSlot() == i);
        JSAtom* atom = fi.name();
        if (atom) {
          cx->markAtom(atom);
        }
        result[i].set(atom);
      }
    }
  } else {
    for (size_t i = 0; i < referent->nargs(); i++) {
      result[i].set(nullptr);
    }
  }

  return true;
}

/* static */
bool DebuggerObject::getBoundTargetFunction(
    JSContext* cx, HandleDebuggerObject object,
    MutableHandleDebuggerObject result) {
  MOZ_ASSERT(object->isBoundFunction());

  RootedFunction referent(cx, &object->referent()->as<JSFunction>());
  Debugger* dbg = object->owner();

  RootedObject target(cx, referent->getBoundFunctionTarget());
  return dbg->wrapDebuggeeObject(cx, target, result);
}

/* static */
bool DebuggerObject::getBoundThis(JSContext* cx, HandleDebuggerObject object,
                                  MutableHandleValue result) {
  MOZ_ASSERT(object->isBoundFunction());

  RootedFunction referent(cx, &object->referent()->as<JSFunction>());
  Debugger* dbg = object->owner();

  result.set(referent->getBoundFunctionThis());
  return dbg->wrapDebuggeeValue(cx, result);
}

/* static */
bool DebuggerObject::getBoundArguments(JSContext* cx,
                                       HandleDebuggerObject object,
                                       MutableHandle<ValueVector> result) {
  MOZ_ASSERT(object->isBoundFunction());

  RootedFunction referent(cx, &object->referent()->as<JSFunction>());
  Debugger* dbg = object->owner();

  size_t length = referent->getBoundFunctionArgumentCount();
  if (!result.resize(length)) {
    return false;
  }
  for (size_t i = 0; i < length; i++) {
    result[i].set(referent->getBoundFunctionArgument(i));
    if (!dbg->wrapDebuggeeValue(cx, result[i])) {
      return false;
    }
  }
  return true;
}

/* static */
SavedFrame* Debugger::getObjectAllocationSite(JSObject& obj) {
  JSObject* metadata = GetAllocationMetadata(&obj);
  if (!metadata) {
    return nullptr;
  }

  MOZ_ASSERT(!metadata->is<WrapperObject>());
  return metadata->is<SavedFrame>() ? &metadata->as<SavedFrame>() : nullptr;
}

/* static */
bool DebuggerObject::getAllocationSite(JSContext* cx,
                                       HandleDebuggerObject object,
                                       MutableHandleObject result) {
  RootedObject referent(cx, object->referent());

  RootedObject allocSite(cx, Debugger::getObjectAllocationSite(*referent));
  if (!cx->compartment()->wrap(cx, &allocSite)) {
    return false;
  }

  result.set(allocSite);
  return true;
}

/* static */
bool DebuggerObject::getErrorReport(JSContext* cx, HandleObject maybeError,
                                    JSErrorReport*& report) {
  JSObject* obj = maybeError;
  if (IsCrossCompartmentWrapper(obj)) {
    /* We only care about Error objects, so CheckedUnwrapStatic is OK. */
    obj = CheckedUnwrapStatic(obj);
  }

  if (!obj) {
    ReportAccessDenied(cx);
    return false;
  }

  if (!obj->is<ErrorObject>()) {
    report = nullptr;
    return true;
  }

  report = obj->as<ErrorObject>().getErrorReport();
  return true;
}

/* static */
bool DebuggerObject::getErrorMessageName(JSContext* cx,
                                         HandleDebuggerObject object,
                                         MutableHandleString result) {
  RootedObject referent(cx, object->referent());
  JSErrorReport* report;
  if (!getErrorReport(cx, referent, report)) {
    return false;
  }

  if (!report) {
    result.set(nullptr);
    return true;
  }

  const JSErrorFormatString* efs =
      GetErrorMessage(nullptr, report->errorNumber);
  if (!efs) {
    result.set(nullptr);
    return true;
  }

  RootedString str(cx, JS_NewStringCopyZ(cx, efs->name));
  if (!str) {
    return false;
  }
  result.set(str);
  return true;
}

/* static */
bool DebuggerObject::getErrorNotes(JSContext* cx, HandleDebuggerObject object,
                                   MutableHandleValue result) {
  RootedObject referent(cx, object->referent());
  JSErrorReport* report;
  if (!getErrorReport(cx, referent, report)) {
    return false;
  }

  if (!report) {
    result.setUndefined();
    return true;
  }

  RootedObject errorNotesArray(cx, CreateErrorNotesArray(cx, report));
  if (!errorNotesArray) {
    return false;
  }

  if (!cx->compartment()->wrap(cx, &errorNotesArray)) {
    return false;
  }
  result.setObject(*errorNotesArray);
  return true;
}

/* static */
bool DebuggerObject::getErrorLineNumber(JSContext* cx,
                                        HandleDebuggerObject object,
                                        MutableHandleValue result) {
  RootedObject referent(cx, object->referent());
  JSErrorReport* report;
  if (!getErrorReport(cx, referent, report)) {
    return false;
  }

  if (!report) {
    result.setUndefined();
    return true;
  }

  result.setNumber(report->lineno);
  return true;
}

/* static */
bool DebuggerObject::getErrorColumnNumber(JSContext* cx,
                                          HandleDebuggerObject object,
                                          MutableHandleValue result) {
  RootedObject referent(cx, object->referent());
  JSErrorReport* report;
  if (!getErrorReport(cx, referent, report)) {
    return false;
  }

  if (!report) {
    result.setUndefined();
    return true;
  }

  result.setNumber(report->column);
  return true;
}

/* static */
bool DebuggerObject::getPromiseValue(JSContext* cx, HandleDebuggerObject object,
                                     MutableHandleValue result) {
  MOZ_ASSERT(object->promiseState() == JS::PromiseState::Fulfilled);

  result.set(object->promise()->value());
  return object->owner()->wrapDebuggeeValue(cx, result);
}

/* static */
bool DebuggerObject::getPromiseReason(JSContext* cx,
                                      HandleDebuggerObject object,
                                      MutableHandleValue result) {
  MOZ_ASSERT(object->promiseState() == JS::PromiseState::Rejected);

  result.set(object->promise()->reason());
  return object->owner()->wrapDebuggeeValue(cx, result);
}

/* static */
bool DebuggerObject::isExtensible(JSContext* cx, HandleDebuggerObject object,
                                  bool& result) {
  RootedObject referent(cx, object->referent());

  Maybe<AutoRealm> ar;
  EnterDebuggeeObjectRealm(cx, ar, referent);

  ErrorCopier ec(ar);
  return IsExtensible(cx, referent, &result);
}

/* static */
bool DebuggerObject::isSealed(JSContext* cx, HandleDebuggerObject object,
                              bool& result) {
  RootedObject referent(cx, object->referent());

  Maybe<AutoRealm> ar;
  EnterDebuggeeObjectRealm(cx, ar, referent);

  ErrorCopier ec(ar);
  return TestIntegrityLevel(cx, referent, IntegrityLevel::Sealed, &result);
}

/* static */
bool DebuggerObject::isFrozen(JSContext* cx, HandleDebuggerObject object,
                              bool& result) {
  RootedObject referent(cx, object->referent());

  Maybe<AutoRealm> ar;
  EnterDebuggeeObjectRealm(cx, ar, referent);

  ErrorCopier ec(ar);
  return TestIntegrityLevel(cx, referent, IntegrityLevel::Frozen, &result);
}

/* static */
bool DebuggerObject::getPrototypeOf(JSContext* cx, HandleDebuggerObject object,
                                    MutableHandleDebuggerObject result) {
  RootedObject referent(cx, object->referent());
  Debugger* dbg = object->owner();

  RootedObject proto(cx);
  {
    Maybe<AutoRealm> ar;
    EnterDebuggeeObjectRealm(cx, ar, referent);
    if (!GetPrototype(cx, referent, &proto)) {
      return false;
    }
  }

  if (!proto) {
    result.set(nullptr);
    return true;
  }

  return dbg->wrapDebuggeeObject(cx, proto, result);
}

/* static */
bool DebuggerObject::getOwnPropertyNames(JSContext* cx,
                                         HandleDebuggerObject object,
                                         MutableHandle<IdVector> result) {
  RootedObject referent(cx, object->referent());

  RootedIdVector ids(cx);
  {
    Maybe<AutoRealm> ar;
    EnterDebuggeeObjectRealm(cx, ar, referent);

    ErrorCopier ec(ar);
    if (!GetPropertyKeys(cx, referent, JSITER_OWNONLY | JSITER_HIDDEN, &ids)) {
      return false;
    }
  }

  for (size_t i = 0; i < ids.length(); i++) {
    cx->markId(ids[i]);
  }

  return result.append(ids.begin(), ids.end());
}

/* static */
bool DebuggerObject::getOwnPropertySymbols(JSContext* cx,
                                           HandleDebuggerObject object,
                                           MutableHandle<IdVector> result) {
  RootedObject referent(cx, object->referent());

  RootedIdVector ids(cx);
  {
    Maybe<AutoRealm> ar;
    EnterDebuggeeObjectRealm(cx, ar, referent);

    ErrorCopier ec(ar);
    if (!GetPropertyKeys(cx, referent,
                         JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS |
                             JSITER_SYMBOLSONLY,
                         &ids)) {
      return false;
    }
  }

  for (size_t i = 0; i < ids.length(); i++) {
    cx->markId(ids[i]);
  }

  return result.append(ids.begin(), ids.end());
}

/* static */
bool DebuggerObject::getOwnPropertyDescriptor(
    JSContext* cx, HandleDebuggerObject object, HandleId id,
    MutableHandle<PropertyDescriptor> desc) {
  RootedObject referent(cx, object->referent());
  Debugger* dbg = object->owner();

  // Bug: This can cause the debuggee to run!
  {
    Maybe<AutoRealm> ar;
    EnterDebuggeeObjectRealm(cx, ar, referent);

    cx->markId(id);

    ErrorCopier ec(ar);
    if (!GetOwnPropertyDescriptor(cx, referent, id, desc)) {
      return false;
    }
  }

  if (desc.object()) {
    // Rewrap the debuggee values in desc for the debugger.
    if (!dbg->wrapDebuggeeValue(cx, desc.value())) {
      return false;
    }

    if (desc.hasGetterObject()) {
      RootedValue get(cx, ObjectOrNullValue(desc.getterObject()));
      if (!dbg->wrapDebuggeeValue(cx, &get)) {
        return false;
      }
      desc.setGetterObject(get.toObjectOrNull());
    }
    if (desc.hasSetterObject()) {
      RootedValue set(cx, ObjectOrNullValue(desc.setterObject()));
      if (!dbg->wrapDebuggeeValue(cx, &set)) {
        return false;
      }
      desc.setSetterObject(set.toObjectOrNull());
    }

    // Avoid tripping same-compartment assertions in
    // JS::FromPropertyDescriptor().
    desc.object().set(object);
  }

  return true;
}

/* static */
bool DebuggerObject::preventExtensions(JSContext* cx,
                                       HandleDebuggerObject object) {
  RootedObject referent(cx, object->referent());

  Maybe<AutoRealm> ar;
  EnterDebuggeeObjectRealm(cx, ar, referent);

  ErrorCopier ec(ar);
  return PreventExtensions(cx, referent);
}

/* static */
bool DebuggerObject::seal(JSContext* cx, HandleDebuggerObject object) {
  RootedObject referent(cx, object->referent());

  Maybe<AutoRealm> ar;
  EnterDebuggeeObjectRealm(cx, ar, referent);

  ErrorCopier ec(ar);
  return SetIntegrityLevel(cx, referent, IntegrityLevel::Sealed);
}

/* static */
bool DebuggerObject::freeze(JSContext* cx, HandleDebuggerObject object) {
  RootedObject referent(cx, object->referent());

  Maybe<AutoRealm> ar;
  EnterDebuggeeObjectRealm(cx, ar, referent);

  ErrorCopier ec(ar);
  return SetIntegrityLevel(cx, referent, IntegrityLevel::Frozen);
}

/* static */
bool DebuggerObject::defineProperty(JSContext* cx, HandleDebuggerObject object,
                                    HandleId id,
                                    Handle<PropertyDescriptor> desc_) {
  RootedObject referent(cx, object->referent());
  Debugger* dbg = object->owner();

  Rooted<PropertyDescriptor> desc(cx, desc_);
  if (!dbg->unwrapPropertyDescriptor(cx, referent, &desc)) {
    return false;
  }
  JS_TRY_OR_RETURN_FALSE(cx, CheckPropertyDescriptorAccessors(cx, desc));

  Maybe<AutoRealm> ar;
  EnterDebuggeeObjectRealm(cx, ar, referent);

  if (!cx->compartment()->wrap(cx, &desc)) {
    return false;
  }
  cx->markId(id);

  ErrorCopier ec(ar);
  return DefineProperty(cx, referent, id, desc);
}

/* static */
bool DebuggerObject::defineProperties(JSContext* cx,
                                      HandleDebuggerObject object,
                                      Handle<IdVector> ids,
                                      Handle<PropertyDescriptorVector> descs_) {
  RootedObject referent(cx, object->referent());
  Debugger* dbg = object->owner();

  Rooted<PropertyDescriptorVector> descs(cx, PropertyDescriptorVector(cx));
  if (!descs.append(descs_.begin(), descs_.end())) {
    return false;
  }
  for (size_t i = 0; i < descs.length(); i++) {
    if (!dbg->unwrapPropertyDescriptor(cx, referent, descs[i])) {
      return false;
    }
    JS_TRY_OR_RETURN_FALSE(cx, CheckPropertyDescriptorAccessors(cx, descs[i]));
  }

  Maybe<AutoRealm> ar;
  EnterDebuggeeObjectRealm(cx, ar, referent);

  for (size_t i = 0; i < descs.length(); i++) {
    if (!cx->compartment()->wrap(cx, descs[i])) {
      return false;
    }
    cx->markId(ids[i]);
  }

  ErrorCopier ec(ar);
  for (size_t i = 0; i < descs.length(); i++) {
    if (!DefineProperty(cx, referent, ids[i], descs[i])) {
      return false;
    }
  }

  return true;
}

/* static */
bool DebuggerObject::deleteProperty(JSContext* cx, HandleDebuggerObject object,
                                    HandleId id, ObjectOpResult& result) {
  RootedObject referent(cx, object->referent());

  Maybe<AutoRealm> ar;
  EnterDebuggeeObjectRealm(cx, ar, referent);

  cx->markId(id);

  ErrorCopier ec(ar);
  return DeleteProperty(cx, referent, id, result);
}

/* static */
Result<Completion> DebuggerObject::getProperty(JSContext* cx,
                                               HandleDebuggerObject object,
                                               HandleId id,
                                               HandleValue receiver_) {
  RootedObject referent(cx, object->referent());
  Debugger* dbg = object->owner();

  // Unwrap Debugger.Objects. This happens in the debugger's compartment since
  // that is where any exceptions must be reported.
  RootedValue receiver(cx, receiver_);
  if (!dbg->unwrapDebuggeeValue(cx, &receiver)) {
    return cx->alreadyReportedError();
  }

  // Enter the debuggee compartment and rewrap all input value for that
  // compartment. (Rewrapping always takes place in the destination
  // compartment.)
  Maybe<AutoRealm> ar;
  EnterDebuggeeObjectRealm(cx, ar, referent);
  if (!cx->compartment()->wrap(cx, &referent) ||
      !cx->compartment()->wrap(cx, &receiver)) {
    return cx->alreadyReportedError();
  }
  cx->markId(id);

  LeaveDebuggeeNoExecute nnx(cx);

  RootedValue result(cx);
  bool ok = GetProperty(cx, referent, receiver, id, &result);
  return Completion::fromJSResult(cx, ok, result);
}

/* static */
Result<Completion> DebuggerObject::setProperty(JSContext* cx,
                                               HandleDebuggerObject object,
                                               HandleId id, HandleValue value_,
                                               HandleValue receiver_) {
  RootedObject referent(cx, object->referent());
  Debugger* dbg = object->owner();

  // Unwrap Debugger.Objects. This happens in the debugger's compartment since
  // that is where any exceptions must be reported.
  RootedValue value(cx, value_);
  RootedValue receiver(cx, receiver_);
  if (!dbg->unwrapDebuggeeValue(cx, &value) ||
      !dbg->unwrapDebuggeeValue(cx, &receiver)) {
    return cx->alreadyReportedError();
  }

  // Enter the debuggee compartment and rewrap all input value for that
  // compartment. (Rewrapping always takes place in the destination
  // compartment.)
  Maybe<AutoRealm> ar;
  EnterDebuggeeObjectRealm(cx, ar, referent);
  if (!cx->compartment()->wrap(cx, &referent) ||
      !cx->compartment()->wrap(cx, &value) ||
      !cx->compartment()->wrap(cx, &receiver)) {
    return cx->alreadyReportedError();
  }
  cx->markId(id);

  LeaveDebuggeeNoExecute nnx(cx);

  ObjectOpResult opResult;
  bool ok = SetProperty(cx, referent, id, value, receiver, opResult);

  return Completion::fromJSResult(cx, ok,
                                  BooleanValue(ok && opResult.reallyOk()));
}

/* static */
Maybe<Completion> DebuggerObject::call(JSContext* cx,
                                       HandleDebuggerObject object,
                                       HandleValue thisv_,
                                       Handle<ValueVector> args) {
  RootedObject referent(cx, object->referent());
  Debugger* dbg = object->owner();

  if (!referent->isCallable()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Debugger.Object",
                              "call", referent->getClass()->name);
    return Nothing();
  }

  RootedValue calleev(cx, ObjectValue(*referent));

  // Unwrap Debugger.Objects. This happens in the debugger's compartment since
  // that is where any exceptions must be reported.
  RootedValue thisv(cx, thisv_);
  if (!dbg->unwrapDebuggeeValue(cx, &thisv)) {
    return Nothing();
  }
  Rooted<ValueVector> args2(cx, ValueVector(cx));
  if (!args2.append(args.begin(), args.end())) {
    return Nothing();
  }
  for (size_t i = 0; i < args2.length(); ++i) {
    if (!dbg->unwrapDebuggeeValue(cx, args2[i])) {
      return Nothing();
    }
  }

  // Enter the debuggee compartment and rewrap all input value for that
  // compartment. (Rewrapping always takes place in the destination
  // compartment.)
  Maybe<AutoRealm> ar;
  EnterDebuggeeObjectRealm(cx, ar, referent);
  if (!cx->compartment()->wrap(cx, &calleev) ||
      !cx->compartment()->wrap(cx, &thisv)) {
    return Nothing();
  }
  for (size_t i = 0; i < args2.length(); ++i) {
    if (!cx->compartment()->wrap(cx, args2[i])) {
      return Nothing();
    }
  }

  // Call the function.
  LeaveDebuggeeNoExecute nnx(cx);

  RootedValue result(cx);
  bool ok;
  {
    InvokeArgs invokeArgs(cx);

    ok = invokeArgs.init(cx, args2.length());
    if (ok) {
      for (size_t i = 0; i < args2.length(); ++i) {
        invokeArgs[i].set(args2[i]);
      }

      ok = js::Call(cx, calleev, thisv, invokeArgs, &result);
    }
  }

  Rooted<Completion> completion(cx, Completion::fromJSResult(cx, ok, result));
  ar.reset();
  return Some(std::move(completion.get()));
}

/* static */
bool DebuggerObject::forceLexicalInitializationByName(
    JSContext* cx, HandleDebuggerObject object, HandleId id, bool& result) {
  if (!JSID_IS_STRING(id)) {
    JS_ReportErrorNumberASCII(
        cx, GetErrorMessage, nullptr, JSMSG_NOT_EXPECTED_TYPE,
        "Debugger.Object.prototype.forceLexicalInitializationByName", "string",
        InformalValueTypeName(IdToValue(id)));
    return false;
  }

  MOZ_ASSERT(object->isGlobal());

  Rooted<GlobalObject*> referent(cx, &object->referent()->as<GlobalObject>());

  RootedObject globalLexical(cx, &referent->lexicalEnvironment());
  RootedObject pobj(cx);
  Rooted<PropertyResult> prop(cx);
  if (!LookupProperty(cx, globalLexical, id, &pobj, &prop)) {
    return false;
  }

  result = false;
  if (prop) {
    MOZ_ASSERT(prop.isNativeProperty());
    Shape* shape = prop.shape();
    Value v = globalLexical->as<NativeObject>().getSlot(shape->slot());
    if (shape->isDataProperty() && v.isMagic() &&
        v.whyMagic() == JS_UNINITIALIZED_LEXICAL) {
      globalLexical->as<NativeObject>().setSlot(shape->slot(),
                                                UndefinedValue());
      result = true;
    }
  }

  return true;
}

/* static */
Result<Completion> DebuggerObject::executeInGlobal(
    JSContext* cx, HandleDebuggerObject object,
    mozilla::Range<const char16_t> chars, HandleObject bindings,
    const EvalOptions& options) {
  MOZ_ASSERT(object->isGlobal());

  Rooted<GlobalObject*> referent(cx, &object->referent()->as<GlobalObject>());
  Debugger* dbg = object->owner();

  RootedObject globalLexical(cx, &referent->lexicalEnvironment());
  return DebuggerGenericEval(cx, chars, bindings, options, dbg, globalLexical,
                             nullptr);
}

/* static */
bool DebuggerObject::makeDebuggeeValue(JSContext* cx,
                                       HandleDebuggerObject object,
                                       HandleValue value_,
                                       MutableHandleValue result) {
  RootedObject referent(cx, object->referent());
  Debugger* dbg = object->owner();

  RootedValue value(cx, value_);

  // Non-objects are already debuggee values.
  if (value.isObject()) {
    // Enter this Debugger.Object's referent's compartment, and wrap the
    // argument as appropriate for references from there.
    {
      Maybe<AutoRealm> ar;
      EnterDebuggeeObjectRealm(cx, ar, referent);
      if (!cx->compartment()->wrap(cx, &value)) {
        return false;
      }
    }

    // Back in the debugger's compartment, produce a new Debugger.Object
    // instance referring to the wrapped argument.
    if (!dbg->wrapDebuggeeValue(cx, &value)) {
      return false;
    }
  }

  result.set(value);
  return true;
}

/* static */
bool DebuggerObject::makeDebuggeeNativeFunction(JSContext* cx,
                                                HandleDebuggerObject object,
                                                HandleValue value,
                                                MutableHandleValue result) {
  RootedObject referent(cx, object->referent());
  Debugger* dbg = object->owner();

  if (!value.isObject()) {
    JS_ReportErrorASCII(cx, "Need object");
    return false;
  }

  RootedObject obj(cx, &value.toObject());
  if (!obj->is<JSFunction>()) {
    JS_ReportErrorASCII(cx, "Need function");
    return false;
  }

  RootedFunction fun(cx, &obj->as<JSFunction>());
  if (!fun->isNative() || fun->isExtended()) {
    JS_ReportErrorASCII(cx, "Need native function");
    return false;
  }

  RootedValue newValue(cx);
  {
    Maybe<AutoRealm> ar;
    EnterDebuggeeObjectRealm(cx, ar, referent);

    unsigned nargs = fun->nargs();
    RootedAtom name(cx, fun->displayAtom());
    JSFunction* newFun = NewNativeFunction(cx, fun->native(), nargs, name);
    if (!newFun) {
      return false;
    }

    newValue.setObject(*newFun);
  }

  // Back in the debugger's compartment, produce a new Debugger.Object
  // instance referring to the wrapped argument.
  if (!dbg->wrapDebuggeeValue(cx, &newValue)) {
    return false;
  }

  result.set(newValue);
  return true;
}

/* static */
bool DebuggerObject::unsafeDereference(JSContext* cx,
                                       HandleDebuggerObject object,
                                       MutableHandleObject result) {
  RootedObject referent(cx, object->referent());

  if (!cx->compartment()->wrap(cx, &referent)) {
    return false;
  }

  // Wrapping should return the WindowProxy.
  MOZ_ASSERT(!IsWindow(referent));

  result.set(referent);
  return true;
}

/* static */
bool DebuggerObject::unwrap(JSContext* cx, HandleDebuggerObject object,
                            MutableHandleDebuggerObject result) {
  RootedObject referent(cx, object->referent());
  Debugger* dbg = object->owner();

  RootedObject unwrapped(cx, UnwrapOneCheckedStatic(referent));
  if (!unwrapped) {
    result.set(nullptr);
    return true;
  }

  // Don't allow unwrapping to create a D.O whose referent is in an
  // invisible-to-Debugger compartment. (If our referent is a *wrapper* to such,
  // and the wrapper is in a visible compartment, that's fine.)
  if (unwrapped->compartment()->invisibleToDebugger()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_INVISIBLE_COMPARTMENT);
    return false;
  }

  return dbg->wrapDebuggeeObject(cx, unwrapped, result);
}

/* static */
bool DebuggerObject::requireGlobal(JSContext* cx, HandleDebuggerObject object) {
  if (!object->isGlobal()) {
    RootedObject referent(cx, object->referent());

    const char* isWrapper = "";
    const char* isWindowProxy = "";

    // Help the poor programmer by pointing out wrappers around globals...
    if (referent->is<WrapperObject>()) {
      referent = js::UncheckedUnwrap(referent);
      isWrapper = "a wrapper around ";
    }

    // ... and WindowProxies around Windows.
    if (IsWindowProxy(referent)) {
      referent = ToWindowIfWindowProxy(referent);
      isWindowProxy = "a WindowProxy referring to ";
    }

    RootedValue dbgobj(cx, ObjectValue(*object));
    if (referent->is<GlobalObject>()) {
      ReportValueError(cx, JSMSG_DEBUG_WRAPPER_IN_WAY, JSDVG_SEARCH_STACK,
                       dbgobj, nullptr, isWrapper, isWindowProxy);
    } else {
      ReportValueError(cx, JSMSG_DEBUG_BAD_REFERENT, JSDVG_SEARCH_STACK, dbgobj,
                       nullptr, "a global object");
    }
    return false;
  }

  return true;
}

/* static */
bool DebuggerObject::requirePromise(JSContext* cx,
                                    HandleDebuggerObject object) {
  RootedObject referent(cx, object->referent());

  if (IsCrossCompartmentWrapper(referent)) {
    /* We only care about promises, so CheckedUnwrapStatic is OK. */
    referent = CheckedUnwrapStatic(referent);
    if (!referent) {
      ReportAccessDenied(cx);
      return false;
    }
  }

  if (!referent->is<PromiseObject>()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_EXPECTED_TYPE, "Debugger", "Promise",
                              object->getClass()->name);
    return false;
  }

  return true;
}

/* static */
bool DebuggerObject::getScriptedProxyTarget(
    JSContext* cx, HandleDebuggerObject object,
    MutableHandleDebuggerObject result) {
  MOZ_ASSERT(object->isScriptedProxy());
  RootedObject referent(cx, object->referent());
  Debugger* dbg = object->owner();
  RootedObject unwrapped(cx, js::GetProxyTargetObject(referent));
  if (!unwrapped) {
    result.set(nullptr);
    return true;
  }
  return dbg->wrapDebuggeeObject(cx, unwrapped, result);
}

/* static */
bool DebuggerObject::getScriptedProxyHandler(
    JSContext* cx, HandleDebuggerObject object,
    MutableHandleDebuggerObject result) {
  MOZ_ASSERT(object->isScriptedProxy());
  RootedObject referent(cx, object->referent());
  Debugger* dbg = object->owner();
  RootedObject unwrapped(cx, ScriptedProxyHandler::handlerObject(referent));
  if (!unwrapped) {
    result.set(nullptr);
    return true;
  }
  return dbg->wrapDebuggeeObject(cx, unwrapped, result);
}

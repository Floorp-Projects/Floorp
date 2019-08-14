/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "debugger/Environment-inl.h"

#include "mozilla/Assertions.h"  // for AssertionConditionType
#include "mozilla/Maybe.h"       // for Maybe, Some, Nothing
#include "mozilla/Vector.h"      // for Vector

#include <string.h>  // for strlen, size_t
#include <utility>   // for move

#include "jsapi.h"        // for Rooted, CallArgs, MutableHandle
#include "jsfriendapi.h"  // for GetErrorMessage, GetPropertyKeys

#include "debugger/Debugger.h"          // for Env, Debugger, ValueToIdentifier
#include "debugger/Object.h"            // for DebuggerObject
#include "frontend/BytecodeCompiler.h"  // for IsIdentifier
#include "gc/Rooting.h"                 // for RootedDebuggerEnvironment
#include "gc/Tracer.h"       // for TraceManuallyBarrieredCrossCompartmentEdge
#include "js/HeapAPI.h"      // for IsInsideNursery
#include "vm/Compartment.h"  // for Compartment
#include "vm/EnvironmentObject.h"  // for JSObject::is, DebugEnvironmentProxy
#include "vm/JSAtom.h"             // for Atomize, PinAtom
#include "vm/JSContext.h"          // for JSContext
#include "vm/JSFunction.h"         // for JSFunction
#include "vm/JSObject.h"           // for JSObject, RequireObject
#include "vm/NativeObject.h"       // for NativeObject, JSObject::is
#include "vm/ObjectGroup.h"        // for GenericObject, NewObjectKind
#include "vm/Realm.h"              // for AutoRealm, ErrorCopier
#include "vm/Scope.h"              // for ScopeKind, ScopeKindString
#include "vm/StringType.h"         // for JSAtom

#include "vm/Compartment-inl.h"        // for Compartment::wrap
#include "vm/EnvironmentObject-inl.h"  // for JSObject::enclosingEnvironment
#include "vm/JSObject-inl.h"           // for IsInternalFunctionObject
#include "vm/ObjectOperations-inl.h"   // for HasProperty, GetProperty
#include "vm/Realm-inl.h"              // for AutoRealm::AutoRealm

namespace js {
class GlobalObject;
}

using namespace js;

using js::frontend::IsIdentifier;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

const JSClassOps DebuggerEnvironment::classOps_ = {
    nullptr,                              /* addProperty */
    nullptr,                              /* delProperty */
    nullptr,                              /* enumerate   */
    nullptr,                              /* newEnumerate */
    nullptr,                              /* resolve     */
    nullptr,                              /* mayResolve  */
    nullptr,                              /* finalize    */
    nullptr,                              /* call        */
    nullptr,                              /* hasInstance */
    nullptr,                              /* construct   */
    CallTraceMethod<DebuggerEnvironment>, /* trace */
};

const JSClass DebuggerEnvironment::class_ = {
    "Environment",
    JSCLASS_HAS_PRIVATE |
        JSCLASS_HAS_RESERVED_SLOTS(DebuggerEnvironment::RESERVED_SLOTS),
    &classOps_};

void DebuggerEnvironment::trace(JSTracer* trc) {
  // There is a barrier on private pointers, so the Unbarriered marking
  // is okay.
  if (Env* referent = (JSObject*)getPrivate()) {
    TraceManuallyBarrieredCrossCompartmentEdge(
        trc, static_cast<JSObject*>(this), &referent,
        "Debugger.Environment referent");
    setPrivateUnbarriered(referent);
  }
}

static DebuggerEnvironment* DebuggerEnvironment_checkThis(
    JSContext* cx, const CallArgs& args, const char* fnname,
    bool requireDebuggee) {
  JSObject* thisobj = RequireObject(cx, args.thisv());
  if (!thisobj) {
    return nullptr;
  }
  if (thisobj->getClass() != &DebuggerEnvironment::class_) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Debugger.Environment",
                              fnname, thisobj->getClass()->name);
    return nullptr;
  }

  // Forbid Debugger.Environment.prototype, which is of class
  // DebuggerEnvironment::class_ but isn't a real working Debugger.Environment.
  // The prototype object is distinguished by having no referent.
  DebuggerEnvironment* nthisobj = &thisobj->as<DebuggerEnvironment>();
  if (!nthisobj->getPrivate()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Debugger.Environment",
                              fnname, "prototype object");
    return nullptr;
  }

  // Forbid access to Debugger.Environment objects that are not debuggee
  // environments.
  if (requireDebuggee) {
    Rooted<Env*> env(cx, static_cast<Env*>(nthisobj->getPrivate()));
    if (!Debugger::fromChildJSObject(nthisobj)->observesGlobal(
            &env->nonCCWGlobal())) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_DEBUG_NOT_DEBUGGEE,
                                "Debugger.Environment", "environment");
      return nullptr;
    }
  }

  return nthisobj;
}

#define THIS_DEBUGGER_ENVIRONMENT(cx, argc, vp, fnname, args, environment) \
  CallArgs args = CallArgsFromVp(argc, vp);                                \
  Rooted<DebuggerEnvironment*> environment(                                \
      cx, DebuggerEnvironment_checkThis(cx, args, fnname, false));         \
  if (!environment) return false;

/* static */
bool DebuggerEnvironment::construct(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NO_CONSTRUCTOR,
                            "Debugger.Environment");
  return false;
}

static bool IsDeclarative(Env* env) {
  return env->is<DebugEnvironmentProxy>() &&
         env->as<DebugEnvironmentProxy>().isForDeclarative();
}

template <typename T>
static bool IsDebugEnvironmentWrapper(Env* env) {
  return env->is<DebugEnvironmentProxy>() &&
         env->as<DebugEnvironmentProxy>().environment().is<T>();
}

bool DebuggerEnvironment::typeGetter(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_ENVIRONMENT(cx, argc, vp, "get type", args, environment);

  if (!environment->requireDebuggee(cx)) {
    return false;
  }

  DebuggerEnvironmentType type = environment->type();

  const char* s;
  switch (type) {
    case DebuggerEnvironmentType::Declarative:
      s = "declarative";
      break;
    case DebuggerEnvironmentType::With:
      s = "with";
      break;
    case DebuggerEnvironmentType::Object:
      s = "object";
      break;
  }

  JSAtom* str = Atomize(cx, s, strlen(s), PinAtom);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

bool DebuggerEnvironment::scopeKindGetter(JSContext* cx, unsigned argc,
                                          Value* vp) {
  THIS_DEBUGGER_ENVIRONMENT(cx, argc, vp, "get scopeKind", args, environment);

  if (!environment->requireDebuggee(cx)) {
    return false;
  }

  Maybe<ScopeKind> kind = environment->scopeKind();
  if (kind.isSome()) {
    const char* s = ScopeKindString(*kind);
    JSAtom* str = Atomize(cx, s, strlen(s), PinAtom);
    if (!str) {
      return false;
    }
    args.rval().setString(str);
  } else {
    args.rval().setNull();
  }

  return true;
}

/* static */
bool DebuggerEnvironment::parentGetter(JSContext* cx, unsigned argc,
                                       Value* vp) {
  THIS_DEBUGGER_ENVIRONMENT(cx, argc, vp, "get type", args, environment);

  if (!environment->requireDebuggee(cx)) {
    return false;
  }

  RootedDebuggerEnvironment result(cx);
  if (!environment->getParent(cx, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

/* static */
bool DebuggerEnvironment::objectGetter(JSContext* cx, unsigned argc,
                                       Value* vp) {
  THIS_DEBUGGER_ENVIRONMENT(cx, argc, vp, "get type", args, environment);

  if (!environment->requireDebuggee(cx)) {
    return false;
  }

  if (environment->type() == DebuggerEnvironmentType::Declarative) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_NO_ENV_OBJECT);
    return false;
  }

  RootedDebuggerObject result(cx);
  if (!environment->getObject(cx, &result)) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/* static */
bool DebuggerEnvironment::calleeGetter(JSContext* cx, unsigned argc,
                                       Value* vp) {
  THIS_DEBUGGER_ENVIRONMENT(cx, argc, vp, "get callee", args, environment);

  if (!environment->requireDebuggee(cx)) {
    return false;
  }

  RootedDebuggerObject result(cx);
  if (!environment->getCallee(cx, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

/* static */
bool DebuggerEnvironment::inspectableGetter(JSContext* cx, unsigned argc,
                                            Value* vp) {
  THIS_DEBUGGER_ENVIRONMENT(cx, argc, vp, "get inspectable", args, environment);

  args.rval().setBoolean(environment->isDebuggee());
  return true;
}

/* static */
bool DebuggerEnvironment::optimizedOutGetter(JSContext* cx, unsigned argc,
                                             Value* vp) {
  THIS_DEBUGGER_ENVIRONMENT(cx, argc, vp, "get optimizedOut", args,
                            environment);

  args.rval().setBoolean(environment->isOptimized());
  return true;
}

/* static */
bool DebuggerEnvironment::namesMethod(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_ENVIRONMENT(cx, argc, vp, "names", args, environment);

  if (!environment->requireDebuggee(cx)) {
    return false;
  }

  Rooted<IdVector> ids(cx, IdVector(cx));
  if (!DebuggerEnvironment::getNames(cx, environment, &ids)) {
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
bool DebuggerEnvironment::findMethod(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGGER_ENVIRONMENT(cx, argc, vp, "find", args, environment);
  if (!args.requireAtLeast(cx, "Debugger.Environment.find", 1)) {
    return false;
  }

  if (!environment->requireDebuggee(cx)) {
    return false;
  }

  RootedId id(cx);
  if (!ValueToIdentifier(cx, args[0], &id)) {
    return false;
  }

  RootedDebuggerEnvironment result(cx);
  if (!DebuggerEnvironment::find(cx, environment, id, &result)) {
    return false;
  }

  args.rval().setObjectOrNull(result);
  return true;
}

/* static */
bool DebuggerEnvironment::getVariableMethod(JSContext* cx, unsigned argc,
                                            Value* vp) {
  THIS_DEBUGGER_ENVIRONMENT(cx, argc, vp, "getVariable", args, environment);
  if (!args.requireAtLeast(cx, "Debugger.Environment.getVariable", 1)) {
    return false;
  }

  if (!environment->requireDebuggee(cx)) {
    return false;
  }

  RootedId id(cx);
  if (!ValueToIdentifier(cx, args[0], &id)) {
    return false;
  }

  return DebuggerEnvironment::getVariable(cx, environment, id, args.rval());
}

/* static */
bool DebuggerEnvironment::setVariableMethod(JSContext* cx, unsigned argc,
                                            Value* vp) {
  THIS_DEBUGGER_ENVIRONMENT(cx, argc, vp, "setVariable", args, environment);
  if (!args.requireAtLeast(cx, "Debugger.Environment.setVariable", 2)) {
    return false;
  }

  if (!environment->requireDebuggee(cx)) {
    return false;
  }

  RootedId id(cx);
  if (!ValueToIdentifier(cx, args[0], &id)) {
    return false;
  }

  if (!DebuggerEnvironment::setVariable(cx, environment, id, args[1])) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

bool DebuggerEnvironment::requireDebuggee(JSContext* cx) const {
  if (!isDebuggee()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_NOT_DEBUGGEE, "Debugger.Environment",
                              "environment");

    return false;
  }

  return true;
}

const JSPropertySpec DebuggerEnvironment::properties_[] = {
    JS_PSG("type", DebuggerEnvironment::typeGetter, 0),
    JS_PSG("scopeKind", DebuggerEnvironment::scopeKindGetter, 0),
    JS_PSG("parent", DebuggerEnvironment::parentGetter, 0),
    JS_PSG("object", DebuggerEnvironment::objectGetter, 0),
    JS_PSG("callee", DebuggerEnvironment::calleeGetter, 0),
    JS_PSG("inspectable", DebuggerEnvironment::inspectableGetter, 0),
    JS_PSG("optimizedOut", DebuggerEnvironment::optimizedOutGetter, 0),
    JS_PS_END};

const JSFunctionSpec DebuggerEnvironment::methods_[] = {
    JS_FN("names", DebuggerEnvironment::namesMethod, 0, 0),
    JS_FN("find", DebuggerEnvironment::findMethod, 1, 0),
    JS_FN("getVariable", DebuggerEnvironment::getVariableMethod, 1, 0),
    JS_FN("setVariable", DebuggerEnvironment::setVariableMethod, 2, 0),
    JS_FS_END};

/* static */
NativeObject* DebuggerEnvironment::initClass(JSContext* cx,
                                             Handle<GlobalObject*> global,
                                             HandleObject dbgCtor) {
  return InitClass(cx, dbgCtor, nullptr, &DebuggerEnvironment::class_,
                   construct, 0, properties_, methods_, nullptr, nullptr);
}

/* static */
DebuggerEnvironment* DebuggerEnvironment::create(JSContext* cx,
                                                 HandleObject proto,
                                                 HandleObject referent,
                                                 HandleNativeObject debugger) {
  NewObjectKind newKind =
      IsInsideNursery(referent) ? GenericObject : TenuredObject;
  DebuggerEnvironment* obj =
      NewObjectWithGivenProto<DebuggerEnvironment>(cx, proto, newKind);
  if (!obj) {
    return nullptr;
  }

  obj->setPrivateGCThing(referent);
  obj->setReservedSlot(OWNER_SLOT, ObjectValue(*debugger));

  return obj;
}

/* static */
DebuggerEnvironmentType DebuggerEnvironment::type() const {
  // Don't bother switching compartments just to check env's type.
  if (IsDeclarative(referent())) {
    return DebuggerEnvironmentType::Declarative;
  }
  if (IsDebugEnvironmentWrapper<WithEnvironmentObject>(referent())) {
    return DebuggerEnvironmentType::With;
  }
  return DebuggerEnvironmentType::Object;
}

mozilla::Maybe<ScopeKind> DebuggerEnvironment::scopeKind() const {
  if (!referent()->is<DebugEnvironmentProxy>()) {
    return Nothing();
  }
  EnvironmentObject& env =
      referent()->as<DebugEnvironmentProxy>().environment();
  Scope* scope = GetEnvironmentScope(env);
  return scope ? Some(scope->kind()) : Nothing();
}

bool DebuggerEnvironment::getParent(
    JSContext* cx, MutableHandleDebuggerEnvironment result) const {
  // Don't bother switching compartments just to get env's parent.
  Rooted<Env*> parent(cx, referent()->enclosingEnvironment());
  if (!parent) {
    result.set(nullptr);
    return true;
  }

  return owner()->wrapEnvironment(cx, parent, result);
}

bool DebuggerEnvironment::getObject(JSContext* cx,
                                    MutableHandleDebuggerObject result) const {
  MOZ_ASSERT(type() != DebuggerEnvironmentType::Declarative);

  // Don't bother switching compartments just to get env's object.
  RootedObject object(cx);
  if (IsDebugEnvironmentWrapper<WithEnvironmentObject>(referent())) {
    object.set(&referent()
                    ->as<DebugEnvironmentProxy>()
                    .environment()
                    .as<WithEnvironmentObject>()
                    .object());
  } else if (IsDebugEnvironmentWrapper<NonSyntacticVariablesObject>(
                 referent())) {
    object.set(&referent()
                    ->as<DebugEnvironmentProxy>()
                    .environment()
                    .as<NonSyntacticVariablesObject>());
  } else {
    object.set(referent());
    MOZ_ASSERT(!object->is<DebugEnvironmentProxy>());
  }

  return owner()->wrapDebuggeeObject(cx, object, result);
}

bool DebuggerEnvironment::getCallee(JSContext* cx,
                                    MutableHandleDebuggerObject result) const {
  if (!referent()->is<DebugEnvironmentProxy>()) {
    result.set(nullptr);
    return true;
  }

  JSObject& scope = referent()->as<DebugEnvironmentProxy>().environment();
  if (!scope.is<CallObject>()) {
    result.set(nullptr);
    return true;
  }

  RootedObject callee(cx, &scope.as<CallObject>().callee());
  if (IsInternalFunctionObject(*callee)) {
    result.set(nullptr);
    return true;
  }

  return owner()->wrapDebuggeeObject(cx, callee, result);
}

bool DebuggerEnvironment::isDebuggee() const {
  MOZ_ASSERT(referent());
  MOZ_ASSERT(!referent()->is<EnvironmentObject>());

  return owner()->observesGlobal(&referent()->nonCCWGlobal());
}

bool DebuggerEnvironment::isOptimized() const {
  return referent()->is<DebugEnvironmentProxy>() &&
         referent()->as<DebugEnvironmentProxy>().isOptimizedOut();
}

/* static */
bool DebuggerEnvironment::getNames(JSContext* cx,
                                   HandleDebuggerEnvironment environment,
                                   MutableHandle<IdVector> result) {
  MOZ_ASSERT(environment->isDebuggee());

  Rooted<Env*> referent(cx, environment->referent());

  RootedIdVector ids(cx);
  {
    Maybe<AutoRealm> ar;
    ar.emplace(cx, referent);

    ErrorCopier ec(ar);
    if (!GetPropertyKeys(cx, referent, JSITER_HIDDEN, &ids)) {
      return false;
    }
  }

  for (size_t i = 0; i < ids.length(); ++i) {
    jsid id = ids[i];
    if (JSID_IS_ATOM(id) && IsIdentifier(JSID_TO_ATOM(id))) {
      cx->markId(id);
      if (!result.append(id)) {
        return false;
      }
    }
  }

  return true;
}

/* static */
bool DebuggerEnvironment::find(JSContext* cx,
                               HandleDebuggerEnvironment environment,
                               HandleId id,
                               MutableHandleDebuggerEnvironment result) {
  MOZ_ASSERT(environment->isDebuggee());

  Rooted<Env*> env(cx, environment->referent());
  Debugger* dbg = environment->owner();

  {
    Maybe<AutoRealm> ar;
    ar.emplace(cx, env);

    cx->markId(id);

    // This can trigger resolve hooks.
    ErrorCopier ec(ar);
    for (; env; env = env->enclosingEnvironment()) {
      bool found;
      if (!HasProperty(cx, env, id, &found)) {
        return false;
      }
      if (found) {
        break;
      }
    }
  }

  if (!env) {
    result.set(nullptr);
    return true;
  }

  return dbg->wrapEnvironment(cx, env, result);
}

/* static */
bool DebuggerEnvironment::getVariable(JSContext* cx,
                                      HandleDebuggerEnvironment environment,
                                      HandleId id, MutableHandleValue result) {
  MOZ_ASSERT(environment->isDebuggee());

  Rooted<Env*> referent(cx, environment->referent());
  Debugger* dbg = environment->owner();

  {
    Maybe<AutoRealm> ar;
    ar.emplace(cx, referent);

    cx->markId(id);

    // This can trigger getters.
    ErrorCopier ec(ar);

    bool found;
    if (!HasProperty(cx, referent, id, &found)) {
      return false;
    }
    if (!found) {
      result.setUndefined();
      return true;
    }

    // For DebugEnvironmentProxys, we get sentinel values for optimized out
    // slots and arguments instead of throwing (the default behavior).
    //
    // See wrapDebuggeeValue for how the sentinel values are wrapped.
    if (referent->is<DebugEnvironmentProxy>()) {
      Rooted<DebugEnvironmentProxy*> env(
          cx, &referent->as<DebugEnvironmentProxy>());
      if (!DebugEnvironmentProxy::getMaybeSentinelValue(cx, env, id, result)) {
        return false;
      }
    } else {
      if (!GetProperty(cx, referent, referent, id, result)) {
        return false;
      }
    }
  }

  // When we've faked up scope chain objects for optimized-out scopes,
  // declarative environments may contain internal JSFunction objects, which
  // we shouldn't expose to the user.
  if (result.isObject()) {
    RootedObject obj(cx, &result.toObject());
    if (obj->is<JSFunction>() &&
        IsInternalFunctionObject(obj->as<JSFunction>()))
      result.setMagic(JS_OPTIMIZED_OUT);
  }

  return dbg->wrapDebuggeeValue(cx, result);
}

/* static */
bool DebuggerEnvironment::setVariable(JSContext* cx,
                                      HandleDebuggerEnvironment environment,
                                      HandleId id, HandleValue value_) {
  MOZ_ASSERT(environment->isDebuggee());

  Rooted<Env*> referent(cx, environment->referent());
  Debugger* dbg = environment->owner();

  RootedValue value(cx, value_);
  if (!dbg->unwrapDebuggeeValue(cx, &value)) {
    return false;
  }

  {
    Maybe<AutoRealm> ar;
    ar.emplace(cx, referent);
    if (!cx->compartment()->wrap(cx, &value)) {
      return false;
    }
    cx->markId(id);

    // This can trigger setters.
    ErrorCopier ec(ar);

    // Make sure the environment actually has the specified binding.
    bool found;
    if (!HasProperty(cx, referent, id, &found)) {
      return false;
    }
    if (!found) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_DEBUG_VARIABLE_NOT_FOUND);
      return false;
    }

    // Just set the property.
    if (!SetProperty(cx, referent, id, value)) {
      return false;
    }
  }

  return true;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsfriendapi.h"

#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/PodOperations.h"
#include "mozilla/TimeStamp.h"

#include <stdint.h>

#include "builtin/BigInt.h"
#include "builtin/MapObject.h"
#include "builtin/Promise.h"
#include "builtin/TestingFunctions.h"
#include "gc/GCInternals.h"
#include "gc/PublicIterators.h"
#include "gc/WeakMap.h"
#include "js/CharacterEncoding.h"
#include "js/Printf.h"
#include "js/Proxy.h"
#include "js/Wrapper.h"
#include "proxy/DeadObjectProxy.h"
#include "vm/ArgumentsObject.h"
#include "vm/DateObject.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/Realm.h"
#include "vm/Time.h"
#include "vm/WrapperObject.h"

#include "gc/Nursery-inl.h"
#include "vm/EnvironmentObject-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

using mozilla::PodArrayZero;

JS::RootingContext::RootingContext()
    : autoGCRooters_(nullptr), realm_(nullptr), zone_(nullptr) {
  for (auto& stackRootPtr : stackRoots_) {
    stackRootPtr = nullptr;
  }

  PodArrayZero(nativeStackLimit);
#if JS_STACK_GROWTH_DIRECTION > 0
  for (int i = 0; i < StackKindCount; i++) {
    nativeStackLimit[i] = UINTPTR_MAX;
  }
#endif
}

JS_FRIEND_API void JS_SetGrayGCRootsTracer(JSContext* cx, JSTraceDataOp traceOp,
                                           void* data) {
  cx->runtime()->gc.setGrayRootsTracer(traceOp, data);
}

JS_FRIEND_API JSObject* JS_FindCompilationScope(JSContext* cx,
                                                HandleObject objArg) {
  cx->check(objArg);

  RootedObject obj(cx, objArg);

  /*
   * We unwrap wrappers here. This is a little weird, but it's what's being
   * asked of us.
   */
  if (obj->is<WrapperObject>()) {
    obj = UncheckedUnwrap(obj);
  }

  /*
   * Get the Window if `obj` is a WindowProxy so that we compile in the
   * correct (global) scope.
   */
  return ToWindowIfWindowProxy(obj);
}

JS_FRIEND_API JSFunction* JS_GetObjectFunction(JSObject* obj) {
  if (obj->is<JSFunction>()) {
    return &obj->as<JSFunction>();
  }
  return nullptr;
}

JS_FRIEND_API bool JS_SplicePrototype(JSContext* cx, HandleObject obj,
                                      HandleObject proto) {
  /*
   * Change the prototype of an object which hasn't been used anywhere
   * and does not share its type with another object. Unlike JS_SetPrototype,
   * does not nuke type information for the object.
   */
  CHECK_THREAD(cx);
  cx->check(obj, proto);

  if (!obj->isSingleton()) {
    /*
     * We can see non-singleton objects when trying to splice prototypes
     * due to mutable __proto__ (ugh).
     */
    return JS_SetPrototype(cx, obj, proto);
  }

  Rooted<TaggedProto> tagged(cx, TaggedProto(proto));
  return JSObject::splicePrototype(cx, obj, tagged);
}

JS_FRIEND_API JSObject* JS_NewObjectWithUniqueType(JSContext* cx,
                                                   const JSClass* clasp,
                                                   HandleObject proto) {
  /*
   * Create our object with a null proto and then splice in the correct proto
   * after we setSingleton, so that we don't pollute the default
   * ObjectGroup attached to our proto with information about our object, since
   * we're not going to be using that ObjectGroup anyway.
   */
  RootedObject obj(cx, NewObjectWithGivenProto(cx, Valueify(clasp), nullptr,
                                               SingletonObject));
  if (!obj) {
    return nullptr;
  }
  if (!JS_SplicePrototype(cx, obj, proto)) {
    return nullptr;
  }
  return obj;
}

JS_FRIEND_API JSObject* JS_NewObjectWithoutMetadata(
    JSContext* cx, const JSClass* clasp, JS::Handle<JSObject*> proto) {
  cx->check(proto);
  AutoSuppressAllocationMetadataBuilder suppressMetadata(cx);
  return JS_NewObjectWithGivenProto(cx, clasp, proto);
}

JS_FRIEND_API bool JS::GetIsSecureContext(JS::Realm* realm) {
  return realm->creationOptions().secureContext();
}

JS_FRIEND_API void js::AssertCompartmentHasSingleRealm(JS::Compartment* comp) {
  MOZ_RELEASE_ASSERT(comp->realms().length() == 1);
}

JS_FRIEND_API JSPrincipals* JS::GetRealmPrincipals(JS::Realm* realm) {
  return realm->principals();
}

JS_FRIEND_API void JS::SetRealmPrincipals(JS::Realm* realm,
                                          JSPrincipals* principals) {
  // Short circuit if there's no change.
  if (principals == realm->principals()) {
    return;
  }

  // We'd like to assert that our new principals is always same-origin
  // with the old one, but JSPrincipals doesn't give us a way to do that.
  // But we can at least assert that we're not switching between system
  // and non-system.
  const JSPrincipals* trusted =
      realm->runtimeFromMainThread()->trustedPrincipals();
  bool isSystem = principals && principals == trusted;
  MOZ_RELEASE_ASSERT(realm->isSystem() == isSystem);

  // Clear out the old principals, if any.
  if (realm->principals()) {
    JS_DropPrincipals(TlsContext.get(), realm->principals());
    realm->setPrincipals(nullptr);
  }

  // Set up the new principals.
  if (principals) {
    JS_HoldPrincipals(principals);
    realm->setPrincipals(principals);
  }
}

JS_FRIEND_API JSPrincipals* JS_GetScriptPrincipals(JSScript* script) {
  return script->principals();
}

JS_FRIEND_API JS::Realm* js::GetScriptRealm(JSScript* script) {
  return script->realm();
}

JS_FRIEND_API bool JS_ScriptHasMutedErrors(JSScript* script) {
  return script->mutedErrors();
}

JS_FRIEND_API bool JS_WrapPropertyDescriptor(
    JSContext* cx, JS::MutableHandle<JS::PropertyDescriptor> desc) {
  return cx->compartment()->wrap(cx, desc);
}

JS_FRIEND_API void JS_TraceShapeCycleCollectorChildren(JS::CallbackTracer* trc,
                                                       JS::GCCellPtr shape) {
  MOZ_ASSERT(shape.is<Shape>());
  TraceCycleCollectorChildren(trc, &shape.as<Shape>());
}

JS_FRIEND_API void JS_TraceObjectGroupCycleCollectorChildren(
    JS::CallbackTracer* trc, JS::GCCellPtr group) {
  MOZ_ASSERT(group.is<ObjectGroup>());
  TraceCycleCollectorChildren(trc, &group.as<ObjectGroup>());
}

static bool DefineHelpProperty(JSContext* cx, HandleObject obj,
                               const char* prop, const char* value) {
  RootedAtom atom(cx, Atomize(cx, value, strlen(value)));
  if (!atom) {
    return false;
  }
  return JS_DefineProperty(cx, obj, prop, atom,
                           JSPROP_READONLY | JSPROP_PERMANENT);
}

JS_FRIEND_API bool JS_DefineFunctionsWithHelp(
    JSContext* cx, HandleObject obj, const JSFunctionSpecWithHelp* fs) {
  MOZ_ASSERT(!cx->zone()->isAtomsZone());

  CHECK_THREAD(cx);
  cx->check(obj);
  for (; fs->name; fs++) {
    JSAtom* atom = Atomize(cx, fs->name, strlen(fs->name));
    if (!atom) {
      return false;
    }

    Rooted<jsid> id(cx, AtomToId(atom));
    RootedFunction fun(cx, DefineFunction(cx, obj, id, fs->call, fs->nargs,
                                          fs->flags | JSPROP_RESOLVING));
    if (!fun) {
      return false;
    }

    if (fs->jitInfo) {
      fun->setJitInfo(fs->jitInfo);
    }

    if (fs->usage) {
      if (!DefineHelpProperty(cx, fun, "usage", fs->usage)) {
        return false;
      }
    }

    if (fs->help) {
      if (!DefineHelpProperty(cx, fun, "help", fs->help)) {
        return false;
      }
    }
  }

  return true;
}

JS_FRIEND_API bool js::GetBuiltinClass(JSContext* cx, HandleObject obj,
                                       ESClass* cls) {
  if (MOZ_UNLIKELY(obj->is<ProxyObject>())) {
    return Proxy::getBuiltinClass(cx, obj, cls);
  }

  if (obj->is<PlainObject>()) {
    *cls = ESClass::Object;
  } else if (obj->is<ArrayObject>()) {
    *cls = ESClass::Array;
  } else if (obj->is<NumberObject>()) {
    *cls = ESClass::Number;
  } else if (obj->is<StringObject>()) {
    *cls = ESClass::String;
  } else if (obj->is<BooleanObject>()) {
    *cls = ESClass::Boolean;
  } else if (obj->is<RegExpObject>()) {
    *cls = ESClass::RegExp;
  } else if (obj->is<ArrayBufferObject>()) {
    *cls = ESClass::ArrayBuffer;
  } else if (obj->is<SharedArrayBufferObject>()) {
    *cls = ESClass::SharedArrayBuffer;
  } else if (obj->is<DateObject>()) {
    *cls = ESClass::Date;
  } else if (obj->is<SetObject>()) {
    *cls = ESClass::Set;
  } else if (obj->is<MapObject>()) {
    *cls = ESClass::Map;
  } else if (obj->is<PromiseObject>()) {
    *cls = ESClass::Promise;
  } else if (obj->is<MapIteratorObject>()) {
    *cls = ESClass::MapIterator;
  } else if (obj->is<SetIteratorObject>()) {
    *cls = ESClass::SetIterator;
  } else if (obj->is<ArgumentsObject>()) {
    *cls = ESClass::Arguments;
  } else if (obj->is<ErrorObject>()) {
    *cls = ESClass::Error;
  } else if (obj->is<BigIntObject>()) {
    *cls = ESClass::BigInt;
  } else {
    *cls = ESClass::Other;
  }

  return true;
}

JS_FRIEND_API bool js::IsArgumentsObject(HandleObject obj) {
  return obj->is<ArgumentsObject>();
}

JS_FRIEND_API const char* js::ObjectClassName(JSContext* cx, HandleObject obj) {
  cx->check(obj);
  return GetObjectClassName(cx, obj);
}

JS_FRIEND_API JS::Zone* js::GetRealmZone(JS::Realm* realm) {
  return realm->zone();
}

JS_FRIEND_API bool js::IsSystemCompartment(JS::Compartment* comp) {
  // Realms in the same compartment must either all be system realms or
  // non-system realms. We assert this in NewRealm and SetRealmPrincipals,
  // but do an extra sanity check here.
  MOZ_ASSERT(comp->realms()[0]->isSystem() ==
             comp->realms().back()->isSystem());
  return comp->realms()[0]->isSystem();
}

JS_FRIEND_API bool js::IsSystemRealm(JS::Realm* realm) {
  return realm->isSystem();
}

JS_FRIEND_API bool js::IsSystemZone(Zone* zone) { return zone->isSystem; }

JS_FRIEND_API bool js::IsAtomsZone(JS::Zone* zone) {
  return zone->runtimeFromAnyThread()->isAtomsZone(zone);
}

JS_FRIEND_API bool js::IsFunctionObject(JSObject* obj) {
  return obj->is<JSFunction>();
}

JS_FRIEND_API bool js::UninlinedIsCrossCompartmentWrapper(const JSObject* obj) {
  return js::IsCrossCompartmentWrapper(obj);
}

JS_FRIEND_API JSObject* js::GetPrototypeNoProxy(JSObject* obj) {
  MOZ_ASSERT(!obj->is<js::ProxyObject>());
  return obj->staticPrototype();
}

JS_FRIEND_API void js::AssertSameCompartment(JSContext* cx, JSObject* obj) {
  cx->check(obj);
}

JS_FRIEND_API void js::AssertSameCompartment(JSContext* cx, JS::HandleValue v) {
  cx->check(v);
}

#ifdef DEBUG
JS_FRIEND_API void js::AssertSameCompartment(JSObject* objA, JSObject* objB) {
  MOZ_ASSERT(objA->compartment() == objB->compartment());
}
#endif

JS_FRIEND_API void js::NotifyAnimationActivity(JSObject* obj) {
  MOZ_ASSERT(obj->is<GlobalObject>());

  auto timeNow = mozilla::TimeStamp::Now();
  obj->as<GlobalObject>().realm()->lastAnimationTime = timeNow;
  obj->runtimeFromMainThread()->lastAnimationTime = timeNow;
}

JS_FRIEND_API uint32_t js::GetObjectSlotSpan(JSObject* obj) {
  return obj->as<NativeObject>().slotSpan();
}

JS_FRIEND_API bool js::IsObjectInContextCompartment(JSObject* obj,
                                                    const JSContext* cx) {
  return obj->compartment() == cx->compartment();
}

JS_FRIEND_API bool js::RunningWithTrustedPrincipals(JSContext* cx) {
  return cx->runningWithTrustedPrincipals();
}

JS_FRIEND_API JSFunction* js::DefineFunctionWithReserved(
    JSContext* cx, JSObject* objArg, const char* name, JSNative call,
    unsigned nargs, unsigned attrs) {
  RootedObject obj(cx, objArg);
  MOZ_ASSERT(!cx->zone()->isAtomsZone());
  CHECK_THREAD(cx);
  cx->check(obj);
  JSAtom* atom = Atomize(cx, name, strlen(name));
  if (!atom) {
    return nullptr;
  }
  Rooted<jsid> id(cx, AtomToId(atom));
  return DefineFunction(cx, obj, id, call, nargs, attrs,
                        gc::AllocKind::FUNCTION_EXTENDED);
}

JS_FRIEND_API JSFunction* js::NewFunctionWithReserved(JSContext* cx,
                                                      JSNative native,
                                                      unsigned nargs,
                                                      unsigned flags,
                                                      const char* name) {
  MOZ_ASSERT(!cx->zone()->isAtomsZone());

  CHECK_THREAD(cx);

  RootedAtom atom(cx);
  if (name) {
    atom = Atomize(cx, name, strlen(name));
    if (!atom) {
      return nullptr;
    }
  }

  return (flags & JSFUN_CONSTRUCTOR)
             ? NewNativeConstructor(cx, native, nargs, atom,
                                    gc::AllocKind::FUNCTION_EXTENDED)
             : NewNativeFunction(cx, native, nargs, atom,
                                 gc::AllocKind::FUNCTION_EXTENDED);
}

JS_FRIEND_API JSFunction* js::NewFunctionByIdWithReserved(
    JSContext* cx, JSNative native, unsigned nargs, unsigned flags, jsid id) {
  MOZ_ASSERT(JSID_IS_STRING(id));
  MOZ_ASSERT(!cx->zone()->isAtomsZone());
  CHECK_THREAD(cx);
  cx->check(id);

  RootedAtom atom(cx, JSID_TO_ATOM(id));
  return (flags & JSFUN_CONSTRUCTOR)
             ? NewNativeConstructor(cx, native, nargs, atom,
                                    gc::AllocKind::FUNCTION_EXTENDED)
             : NewNativeFunction(cx, native, nargs, atom,
                                 gc::AllocKind::FUNCTION_EXTENDED);
}

JS_FRIEND_API const Value& js::GetFunctionNativeReserved(JSObject* fun,
                                                         size_t which) {
  MOZ_ASSERT(fun->as<JSFunction>().isNative());
  return fun->as<JSFunction>().getExtendedSlot(which);
}

JS_FRIEND_API void js::SetFunctionNativeReserved(JSObject* fun, size_t which,
                                                 const Value& val) {
  MOZ_ASSERT(fun->as<JSFunction>().isNative());
  MOZ_ASSERT_IF(val.isObject(),
                val.toObject().compartment() == fun->compartment());
  fun->as<JSFunction>().setExtendedSlot(which, val);
}

JS_FRIEND_API bool js::FunctionHasNativeReserved(JSObject* fun) {
  MOZ_ASSERT(fun->as<JSFunction>().isNative());
  return fun->as<JSFunction>().isExtended();
}

JS_FRIEND_API bool js::GetObjectProto(JSContext* cx, JS::Handle<JSObject*> obj,
                                      JS::MutableHandle<JSObject*> proto) {
  cx->check(obj);

  if (IsProxy(obj)) {
    return JS_GetPrototype(cx, obj, proto);
  }

  proto.set(reinterpret_cast<const shadow::Object*>(obj.get())->group->proto);
  return true;
}

JS_FRIEND_API JSObject* js::GetStaticPrototype(JSObject* obj) {
  MOZ_ASSERT(obj->hasStaticPrototype());
  return obj->staticPrototype();
}

JS_FRIEND_API bool js::GetRealmOriginalEval(JSContext* cx,
                                            MutableHandleObject eval) {
  return GlobalObject::getOrCreateEval(cx, cx->global(), eval);
}

JS_FRIEND_API void js::SetReservedSlotWithBarrier(JSObject* obj, size_t slot,
                                                  const js::Value& value) {
  if (IsProxy(obj)) {
    obj->as<ProxyObject>().setReservedSlot(slot, value);
  } else {
    obj->as<NativeObject>().setSlot(slot, value);
  }
}

void js::SetPreserveWrapperCallback(JSContext* cx,
                                    PreserveWrapperCallback callback) {
  cx->runtime()->preserveWrapperCallback = callback;
}

JS_FRIEND_API unsigned JS_PCToLineNumber(JSScript* script, jsbytecode* pc,
                                         unsigned* columnp) {
  return PCToLineNumber(script, pc, columnp);
}

JS_FRIEND_API bool JS_IsDeadWrapper(JSObject* obj) {
  return IsDeadProxyObject(obj);
}

JS_FRIEND_API JSObject* JS_NewDeadWrapper(JSContext* cx, JSObject* origObj) {
  return NewDeadProxyObject(cx, origObj);
}

void js::TraceWeakMaps(WeakMapTracer* trc) {
  WeakMapBase::traceAllMappings(trc);
}

extern JS_FRIEND_API bool js::AreGCGrayBitsValid(JSRuntime* rt) {
  return rt->gc.areGrayBitsValid();
}

JS_FRIEND_API bool js::ZoneGlobalsAreAllGray(JS::Zone* zone) {
  for (RealmsInZoneIter realm(zone); !realm.done(); realm.next()) {
    JSObject* obj = realm->unsafeUnbarrieredMaybeGlobal();
    if (!obj || !JS::ObjectIsMarkedGray(obj)) {
      return false;
    }
  }
  return true;
}

JS_FRIEND_API bool js::IsCompartmentZoneSweepingOrCompacting(
    JS::Compartment* comp) {
  MOZ_ASSERT(comp);
  return comp->zone()->isGCSweepingOrCompacting();
}

JS_FRIEND_API void js::VisitGrayWrapperTargets(Zone* zone,
                                               GCThingCallback callback,
                                               void* closure) {
  for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
    for (Compartment::ObjectWrapperEnum e(comp); !e.empty(); e.popFront()) {
      JSObject* target = e.front().key();
      if (target->isMarkedGray()) {
        JS::AutoSuppressGCAnalysis nogc;
        callback(closure, JS::GCCellPtr(target));
      }
    }
  }
}

JS_FRIEND_API JSLinearString* js::StringToLinearStringSlow(JSContext* cx,
                                                           JSString* str) {
  return str->ensureLinear(cx);
}

JS_FRIEND_API void JS_SetAccumulateTelemetryCallback(
    JSContext* cx, JSAccumulateTelemetryDataCallback callback) {
  cx->runtime()->setTelemetryCallback(cx->runtime(), callback);
}

JS_FRIEND_API void JS_SetSetUseCounterCallback(
    JSContext* cx, JSSetUseCounterCallback callback) {
  cx->runtime()->setUseCounterCallback(cx->runtime(), callback);
}

JS_FRIEND_API JSObject* JS_CloneObject(JSContext* cx, HandleObject obj,
                                       HandleObject protoArg) {
  // |obj| might be in a different compartment.
  cx->check(protoArg);
  Rooted<TaggedProto> proto(cx, TaggedProto(protoArg.get()));
  return CloneObject(cx, obj, proto);
}

#if defined(DEBUG) || defined(JS_JITSPEW)

// We don't want jsfriendapi.h to depend on GenericPrinter,
// so these functions are declared directly in the cpp.

namespace js {

extern JS_FRIEND_API void DumpString(JSString* str, js::GenericPrinter& out);

extern JS_FRIEND_API void DumpAtom(JSAtom* atom, js::GenericPrinter& out);

extern JS_FRIEND_API void DumpObject(JSObject* obj, js::GenericPrinter& out);

extern JS_FRIEND_API void DumpChars(const char16_t* s, size_t n,
                                    js::GenericPrinter& out);

extern JS_FRIEND_API void DumpValue(const JS::Value& val,
                                    js::GenericPrinter& out);

extern JS_FRIEND_API void DumpId(jsid id, js::GenericPrinter& out);

extern JS_FRIEND_API void DumpInterpreterFrame(
    JSContext* cx, js::GenericPrinter& out, InterpreterFrame* start = nullptr);

}  // namespace js

JS_FRIEND_API void js::DumpString(JSString* str, js::GenericPrinter& out) {
  str->dump(out);
}

JS_FRIEND_API void js::DumpAtom(JSAtom* atom, js::GenericPrinter& out) {
  atom->dump(out);
}

JS_FRIEND_API void js::DumpChars(const char16_t* s, size_t n,
                                 js::GenericPrinter& out) {
  out.printf("char16_t * (%p) = ", (void*)s);
  JSString::dumpChars(s, n, out);
  out.putChar('\n');
}

JS_FRIEND_API void js::DumpObject(JSObject* obj, js::GenericPrinter& out) {
  if (!obj) {
    out.printf("NULL\n");
    return;
  }
  obj->dump(out);
}

JS_FRIEND_API void js::DumpString(JSString* str, FILE* fp) {
  Fprinter out(fp);
  js::DumpString(str, out);
}

JS_FRIEND_API void js::DumpAtom(JSAtom* atom, FILE* fp) {
  Fprinter out(fp);
  js::DumpAtom(atom, out);
}

JS_FRIEND_API void js::DumpChars(const char16_t* s, size_t n, FILE* fp) {
  Fprinter out(fp);
  js::DumpChars(s, n, out);
}

JS_FRIEND_API void js::DumpObject(JSObject* obj, FILE* fp) {
  Fprinter out(fp);
  js::DumpObject(obj, out);
}

JS_FRIEND_API void js::DumpId(jsid id, FILE* fp) {
  Fprinter out(fp);
  js::DumpId(id, out);
}

JS_FRIEND_API void js::DumpValue(const JS::Value& val, FILE* fp) {
  Fprinter out(fp);
  js::DumpValue(val, out);
}

JS_FRIEND_API void js::DumpString(JSString* str) { DumpString(str, stderr); }
JS_FRIEND_API void js::DumpAtom(JSAtom* atom) { DumpAtom(atom, stderr); }
JS_FRIEND_API void js::DumpObject(JSObject* obj) { DumpObject(obj, stderr); }
JS_FRIEND_API void js::DumpChars(const char16_t* s, size_t n) {
  DumpChars(s, n, stderr);
}
JS_FRIEND_API void js::DumpValue(const JS::Value& val) {
  DumpValue(val, stderr);
}
JS_FRIEND_API void js::DumpId(jsid id) { DumpId(id, stderr); }
JS_FRIEND_API void js::DumpInterpreterFrame(JSContext* cx,
                                            InterpreterFrame* start) {
  Fprinter out(stderr);
  DumpInterpreterFrame(cx, out, start);
}
JS_FRIEND_API bool js::DumpPC(JSContext* cx) { return DumpPC(cx, stdout); }
JS_FRIEND_API bool js::DumpScript(JSContext* cx, JSScript* scriptArg) {
  return DumpScript(cx, scriptArg, stdout);
}

#endif

static const char* FormatValue(JSContext* cx, HandleValue v,
                               UniqueChars& bytes) {
  if (v.isMagic()) {
    MOZ_ASSERT(v.whyMagic() == JS_OPTIMIZED_OUT ||
               v.whyMagic() == JS_UNINITIALIZED_LEXICAL);
    return "[unavailable]";
  }

  if (IsCallable(v)) {
    return "[function]";
  }

  if (v.isObject() && IsCrossCompartmentWrapper(&v.toObject())) {
    return "[cross-compartment wrapper]";
  }

  JSString* str;
  {
    mozilla::Maybe<AutoRealm> ar;
    if (v.isObject()) {
      ar.emplace(cx, &v.toObject());
    }

    str = ToString<CanGC>(cx, v);
    if (!str) {
      return nullptr;
    }
  }

  bytes = StringToNewUTF8CharsZ(cx, *str);
  return bytes.get();
}

static bool FormatFrame(JSContext* cx, const FrameIter& iter, Sprinter& sp,
                        int num, bool showArgs, bool showLocals,
                        bool showThisProps) {
  MOZ_ASSERT(!cx->isExceptionPending());
  RootedScript script(cx, iter.script());
  jsbytecode* pc = iter.pc();

  RootedObject envChain(cx, iter.environmentChain(cx));
  JSAutoRealm ar(cx, envChain);

  const char* filename = script->filename();
  unsigned column = 0;
  unsigned lineno = PCToLineNumber(script, pc, &column);
  RootedFunction fun(cx, iter.maybeCallee(cx));
  RootedString funname(cx);
  if (fun) {
    funname = fun->displayAtom();
  }

  RootedValue thisVal(cx);
  if (iter.hasUsableAbstractFramePtr() && iter.isFunctionFrame() && fun &&
      !fun->isArrow() && !fun->isDerivedClassConstructor() &&
      !(fun->isBoundFunction() && iter.isConstructing())) {
    if (!GetFunctionThis(cx, iter.abstractFramePtr(), &thisVal)) {
      return false;
    }
  }

  // print the frame number and function name
  if (funname) {
    UniqueChars funbytes = StringToNewUTF8CharsZ(cx, *funname);
    if (!funbytes) {
      return false;
    }
    if (!sp.printf("%d %s(", num, funbytes.get())) {
      return false;
    }
  } else if (fun) {
    if (!sp.printf("%d anonymous(", num)) {
      return false;
    }
  } else {
    if (!sp.printf("%d <TOP LEVEL>", num)) {
      return false;
    }
  }

  if (showArgs && iter.hasArgs()) {
    PositionalFormalParameterIter fi(script);
    bool first = true;
    for (unsigned i = 0; i < iter.numActualArgs(); i++) {
      RootedValue arg(cx);
      if (i < iter.numFormalArgs() && fi.closedOver()) {
        if (iter.hasInitialEnvironment(cx)) {
          arg = iter.callObj(cx).aliasedBinding(fi);
        } else {
          arg = MagicValue(JS_OPTIMIZED_OUT);
        }
      } else if (iter.hasUsableAbstractFramePtr()) {
        if (script->analyzedArgsUsage() && script->argsObjAliasesFormals() &&
            iter.hasArgsObj()) {
          arg = iter.argsObj().arg(i);
        } else {
          arg = iter.unaliasedActual(i, DONT_CHECK_ALIASING);
        }
      } else {
        arg = MagicValue(JS_OPTIMIZED_OUT);
      }

      UniqueChars valueBytes;
      const char* value = FormatValue(cx, arg, valueBytes);
      if (!value) {
        if (cx->isThrowingOutOfMemory()) {
          return false;
        }
        cx->clearPendingException();
      }

      UniqueChars nameBytes;
      const char* name = nullptr;

      if (i < iter.numFormalArgs()) {
        MOZ_ASSERT(fi.argumentSlot() == i);
        if (!fi.isDestructured()) {
          nameBytes = StringToNewUTF8CharsZ(cx, *fi.name());
          name = nameBytes.get();
          if (!name) {
            return false;
          }
        } else {
          name = "(destructured parameter)";
        }
        fi++;
      }

      if (value) {
        if (!sp.printf("%s%s%s%s%s%s", !first ? ", " : "", name ? name : "",
                       name ? " = " : "", arg.isString() ? "\"" : "", value,
                       arg.isString() ? "\"" : "")) {
          return false;
        }

        first = false;
      } else {
        if (!sp.put("    <Failed to get argument while inspecting stack "
                    "frame>\n")) {
          return false;
        }
      }
    }
  }

  // print filename, line number and column
  if (!sp.printf("%s [\"%s\":%d:%d]\n", fun ? ")" : "",
                 filename ? filename : "<unknown>", lineno, column)) {
    return false;
  }

  // Note: Right now we don't dump the local variables anymore, because
  // that is hard to support across all the JITs etc.

  // print the value of 'this'
  if (showLocals) {
    if (!thisVal.isUndefined()) {
      RootedString thisValStr(cx, ToString<CanGC>(cx, thisVal));
      if (!thisValStr) {
        if (cx->isThrowingOutOfMemory()) {
          return false;
        }
        cx->clearPendingException();
      }
      if (thisValStr) {
        UniqueChars thisValBytes = StringToNewUTF8CharsZ(cx, *thisValStr);
        if (!thisValBytes) {
          return false;
        }
        if (!sp.printf("    this = %s\n", thisValBytes.get())) {
          return false;
        }
      } else {
        if (!sp.put("    <failed to get 'this' value>\n")) {
          return false;
        }
      }
    }
  }

  if (showThisProps && thisVal.isObject()) {
    RootedObject obj(cx, &thisVal.toObject());

    RootedIdVector keys(cx);
    if (!GetPropertyKeys(cx, obj, JSITER_OWNONLY, &keys)) {
      if (cx->isThrowingOutOfMemory()) {
        return false;
      }
      cx->clearPendingException();
    }

    for (size_t i = 0; i < keys.length(); i++) {
      RootedId id(cx, keys[i]);
      RootedValue key(cx, IdToValue(id));
      RootedValue v(cx);

      if (!GetProperty(cx, obj, obj, id, &v)) {
        if (cx->isThrowingOutOfMemory()) {
          return false;
        }
        cx->clearPendingException();
        if (!sp.put("    <Failed to fetch property while inspecting stack "
                    "frame>\n")) {
          return false;
        }
        continue;
      }

      UniqueChars nameBytes;
      const char* name = FormatValue(cx, key, nameBytes);
      if (!name) {
        if (cx->isThrowingOutOfMemory()) {
          return false;
        }
        cx->clearPendingException();
      }

      UniqueChars valueBytes;
      const char* value = FormatValue(cx, v, valueBytes);
      if (!value) {
        if (cx->isThrowingOutOfMemory()) {
          return false;
        }
        cx->clearPendingException();
      }

      if (name && value) {
        if (!sp.printf("    this.%s = %s%s%s\n", name, v.isString() ? "\"" : "",
                       value, v.isString() ? "\"" : "")) {
          return false;
        }
      } else {
        if (!sp.put("    <Failed to format values while inspecting stack "
                    "frame>\n")) {
          return false;
        }
      }
    }
  }

  MOZ_ASSERT(!cx->isExceptionPending());
  return true;
}

static bool FormatWasmFrame(JSContext* cx, const FrameIter& iter, Sprinter& sp,
                            int num) {
  UniqueChars nameStr;
  if (JSAtom* functionDisplayAtom = iter.maybeFunctionDisplayAtom()) {
    nameStr = StringToNewUTF8CharsZ(cx, *functionDisplayAtom);
    if (!nameStr) {
      return false;
    }
  }

  if (!sp.printf("%d %s()", num, nameStr ? nameStr.get() : "<wasm-function>")) {
    return false;
  }

  if (!sp.printf(" [\"%s\":wasm-function[%d]:0x%x]\n",
                 iter.filename() ? iter.filename() : "<unknown>",
                 iter.wasmFuncIndex(), iter.wasmBytecodeOffset())) {
    return false;
  }

  MOZ_ASSERT(!cx->isExceptionPending());
  return true;
}

JS_FRIEND_API JS::UniqueChars JS::FormatStackDump(JSContext* cx, bool showArgs,
                                                  bool showLocals,
                                                  bool showThisProps) {
  int num = 0;

  Sprinter sp(cx);
  if (!sp.init()) {
    return nullptr;
  }

  for (AllFramesIter i(cx); !i.done(); ++i) {
    bool ok = i.hasScript() ? FormatFrame(cx, i, sp, num, showArgs, showLocals,
                                          showThisProps)
                            : FormatWasmFrame(cx, i, sp, num);
    if (!ok) {
      return nullptr;
    }
    num++;
  }

  if (num == 0) {
    if (!sp.put("JavaScript stack is empty\n")) {
      return nullptr;
    }
  }

  return sp.release();
}

extern JS_FRIEND_API bool JS::ForceLexicalInitialization(JSContext* cx,
                                                         HandleObject obj) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(obj);

  bool initializedAny = false;
  NativeObject* nobj = &obj->as<NativeObject>();

  for (Shape::Range<NoGC> r(nobj->lastProperty()); !r.empty(); r.popFront()) {
    Shape* s = &r.front();
    Value v = nobj->getSlot(s->slot());
    if (s->isDataProperty() && v.isMagic() &&
        v.whyMagic() == JS_UNINITIALIZED_LEXICAL) {
      nobj->setSlot(s->slot(), UndefinedValue());
      initializedAny = true;
    }
  }
  return initializedAny;
}

extern JS_FRIEND_API int JS::IsGCPoisoning() {
#ifdef JS_GC_POISONING
  return !js::gDisablePoisoning;
#else
  return false;
#endif
}

struct DumpHeapTracer final : public JS::CallbackTracer, public WeakMapTracer {
  const char* prefix;
  FILE* output;
  mozilla::MallocSizeOf mallocSizeOf;

  DumpHeapTracer(FILE* fp, JSContext* cx, mozilla::MallocSizeOf mallocSizeOf)
      : JS::CallbackTracer(cx, DoNotTraceWeakMaps),
        js::WeakMapTracer(cx->runtime()),
        prefix(""),
        output(fp),
        mallocSizeOf(mallocSizeOf) {}

 private:
  void trace(JSObject* map, JS::GCCellPtr key, JS::GCCellPtr value) override {
    JSObject* kdelegate = nullptr;
    if (key.is<JSObject>()) {
      kdelegate = UncheckedUnwrapWithoutExpose(&key.as<JSObject>());
    }

    fprintf(output, "WeakMapEntry map=%p key=%p keyDelegate=%p value=%p\n", map,
            key.asCell(), kdelegate, value.asCell());
  }

  bool onChild(const JS::GCCellPtr& thing) override;
};

static char MarkDescriptor(void* thing) {
  gc::TenuredCell* cell = gc::TenuredCell::fromPointer(thing);
  if (cell->isMarkedBlack()) {
    return 'B';
  }
  if (cell->isMarkedGray()) {
    return 'G';
  }
  if (cell->isMarkedAny()) {
    return 'X';
  }
  return 'W';
}

static void DumpHeapVisitZone(JSRuntime* rt, void* data, Zone* zone) {
  DumpHeapTracer* dtrc = static_cast<DumpHeapTracer*>(data);
  fprintf(dtrc->output, "# zone %p\n", (void*)zone);
}

static void DumpHeapVisitRealm(JSContext* cx, void* data,
                               Handle<Realm*> realm) {
  char name[1024];
  if (auto nameCallback = cx->runtime()->realmNameCallback) {
    nameCallback(cx, realm, name, sizeof(name));
  } else {
    strcpy(name, "<unknown>");
  }

  DumpHeapTracer* dtrc = static_cast<DumpHeapTracer*>(data);
  fprintf(dtrc->output, "# realm %s [in compartment %p, zone %p]\n", name,
          (void*)realm->compartment(), (void*)realm->zone());
}

static void DumpHeapVisitArena(JSRuntime* rt, void* data, gc::Arena* arena,
                               JS::TraceKind traceKind, size_t thingSize) {
  DumpHeapTracer* dtrc = static_cast<DumpHeapTracer*>(data);
  fprintf(dtrc->output, "# arena allockind=%u size=%u\n",
          unsigned(arena->getAllocKind()), unsigned(thingSize));
}

static void DumpHeapVisitCell(JSRuntime* rt, void* data, void* thing,
                              JS::TraceKind traceKind, size_t thingSize) {
  DumpHeapTracer* dtrc = static_cast<DumpHeapTracer*>(data);
  char cellDesc[1024 * 32];
  JS_GetTraceThingInfo(cellDesc, sizeof(cellDesc), dtrc, thing, traceKind,
                       true);

  fprintf(dtrc->output, "%p %c %s", thing, MarkDescriptor(thing), cellDesc);
  if (dtrc->mallocSizeOf) {
    auto size =
        JS::ubi::Node(JS::GCCellPtr(thing, traceKind)).size(dtrc->mallocSizeOf);
    fprintf(dtrc->output, " SIZE:: %" PRIu64 "\n", size);
  } else {
    fprintf(dtrc->output, "\n");
  }

  js::TraceChildren(dtrc, thing, traceKind);
}

bool DumpHeapTracer::onChild(const JS::GCCellPtr& thing) {
  if (gc::IsInsideNursery(thing.asCell())) {
    return true;
  }

  char buffer[1024];
  getTracingEdgeName(buffer, sizeof(buffer));
  fprintf(output, "%s%p %c %s\n", prefix, thing.asCell(),
          MarkDescriptor(thing.asCell()), buffer);
  return true;
}

void js::DumpHeap(JSContext* cx, FILE* fp,
                  js::DumpHeapNurseryBehaviour nurseryBehaviour,
                  mozilla::MallocSizeOf mallocSizeOf) {
  if (nurseryBehaviour == js::CollectNurseryBeforeDump) {
    cx->runtime()->gc.evictNursery(JS::GCReason::API);
  }

  DumpHeapTracer dtrc(fp, cx, mallocSizeOf);

  fprintf(dtrc.output, "# Roots.\n");
  {
    JSRuntime* rt = cx->runtime();
    js::gc::AutoTraceSession session(rt);
    gcstats::AutoPhase ap(rt->gc.stats(), gcstats::PhaseKind::TRACE_HEAP);
    rt->gc.traceRuntime(&dtrc, session);
  }

  fprintf(dtrc.output, "# Weak maps.\n");
  WeakMapBase::traceAllMappings(&dtrc);

  fprintf(dtrc.output, "==========\n");

  dtrc.prefix = "> ";
  IterateHeapUnbarriered(cx, &dtrc, DumpHeapVisitZone, DumpHeapVisitRealm,
                         DumpHeapVisitArena, DumpHeapVisitCell);

  fflush(dtrc.output);
}

JS_FRIEND_API void JS::NotifyGCRootsRemoved(JSContext* cx) {
  cx->runtime()->gc.notifyRootsRemoved();
}

JS_FRIEND_API JS::Realm* js::GetAnyRealmInZone(JS::Zone* zone) {
  if (zone->isAtomsZone()) {
    return nullptr;
  }

  RealmsInZoneIter realm(zone);
  MOZ_ASSERT(!realm.done());
  return realm.get();
}

JS_FRIEND_API bool js::IsSharableCompartment(JS::Compartment* comp) {
  // If this compartment has nuked outgoing wrappers (because all its globals
  // got nuked), we won't be able to create any useful CCWs out of it in the
  // future, and so we shouldn't use it for any new globals.
  if (comp->nukedOutgoingWrappers) {
    return false;
  }

  // If this compartment has no live globals, it might be in the middle of being
  // GCed.  Don't create any new Realms inside.  There's no point to doing that
  // anyway, since the idea would be to avoid CCWs from existing Realms in the
  // compartment to the new Realm, and there are no existing Realms.
  if (!CompartmentHasLiveGlobal(comp)) {
    return false;
  }

  // Good to go.
  return true;
}

JS_FRIEND_API JSObject* js::GetTestingFunctions(JSContext* cx) {
  RootedObject obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    return nullptr;
  }

  if (!DefineTestingFunctions(cx, obj, false, false)) {
    return nullptr;
  }

  return obj;
}

JS_FRIEND_API void js::SetDOMCallbacks(JSContext* cx,
                                       const DOMCallbacks* callbacks) {
  cx->runtime()->DOMcallbacks = callbacks;
}

JS_FRIEND_API const DOMCallbacks* js::GetDOMCallbacks(JSContext* cx) {
  return cx->runtime()->DOMcallbacks;
}

static const void* gDOMProxyHandlerFamily = nullptr;
static DOMProxyShadowsCheck gDOMProxyShadowsCheck;
static const void* gDOMRemoteProxyHandlerFamily = nullptr;

JS_FRIEND_API void js::SetDOMProxyInformation(
    const void* domProxyHandlerFamily,
    DOMProxyShadowsCheck domProxyShadowsCheck,
    const void* domRemoteProxyHandlerFamily) {
  gDOMProxyHandlerFamily = domProxyHandlerFamily;
  gDOMProxyShadowsCheck = domProxyShadowsCheck;
  gDOMRemoteProxyHandlerFamily = domRemoteProxyHandlerFamily;
}

const void* js::GetDOMProxyHandlerFamily() { return gDOMProxyHandlerFamily; }

DOMProxyShadowsCheck js::GetDOMProxyShadowsCheck() {
  return gDOMProxyShadowsCheck;
}

const void* js::GetDOMRemoteProxyHandlerFamily() {
  return gDOMRemoteProxyHandlerFamily;
}

static XrayJitInfo* gXrayJitInfo = nullptr;

JS_FRIEND_API void js::SetXrayJitInfo(XrayJitInfo* info) {
  gXrayJitInfo = info;
}

XrayJitInfo* js::GetXrayJitInfo() { return gXrayJitInfo; }

bool js::detail::IdMatchesAtom(jsid id, JSAtom* atom) {
  return id == INTERNED_STRING_TO_JSID(nullptr, atom);
}

bool js::detail::IdMatchesAtom(jsid id, JSString* atom) {
  return id == INTERNED_STRING_TO_JSID(nullptr, atom);
}

JS_FRIEND_API void js::PrepareScriptEnvironmentAndInvoke(
    JSContext* cx, HandleObject global,
    ScriptEnvironmentPreparer::Closure& closure) {
  MOZ_ASSERT(!cx->isExceptionPending());
  MOZ_ASSERT(global->is<GlobalObject>());

  MOZ_RELEASE_ASSERT(
      cx->runtime()->scriptEnvironmentPreparer,
      "Embedding needs to set a scriptEnvironmentPreparer callback");

  cx->runtime()->scriptEnvironmentPreparer->invoke(global, closure);
}

JS_FRIEND_API void js::SetScriptEnvironmentPreparer(
    JSContext* cx, ScriptEnvironmentPreparer* preparer) {
  cx->runtime()->scriptEnvironmentPreparer = preparer;
}

JS_FRIEND_API void js::SetCTypesActivityCallback(JSContext* cx,
                                                 CTypesActivityCallback cb) {
  cx->runtime()->ctypesActivityCallback = cb;
}

js::AutoCTypesActivityCallback::AutoCTypesActivityCallback(
    JSContext* cx, js::CTypesActivityType beginType,
    js::CTypesActivityType endType MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
    : cx(cx),
      callback(cx->runtime()->ctypesActivityCallback),
      endType(endType) {
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;

  if (callback) {
    callback(cx, beginType);
  }
}

JS_FRIEND_API void js::SetAllocationMetadataBuilder(
    JSContext* cx, const AllocationMetadataBuilder* callback) {
  cx->realm()->setAllocationMetadataBuilder(callback);
}

JS_FRIEND_API JSObject* js::GetAllocationMetadata(JSObject* obj) {
  ObjectWeakMap* map = ObjectRealm::get(obj).objectMetadataTable.get();
  if (map) {
    return map->lookup(obj);
  }
  return nullptr;
}

JS_FRIEND_API bool js::ReportIsNotFunction(JSContext* cx, HandleValue v) {
  cx->check(v);
  return ReportIsNotFunction(cx, v, -1);
}

#ifdef DEBUG
bool js::HasObjectMovedOp(JSObject* obj) {
  return !!GetObjectClass(obj)->extObjectMovedOp();
}
#endif

JS_FRIEND_API bool js::ForwardToNative(JSContext* cx, JSNative native,
                                       const CallArgs& args) {
  return native(cx, args.length(), args.base());
}

JS_FRIEND_API JSObject* js::ConvertArgsToArray(JSContext* cx,
                                               const CallArgs& args) {
  RootedObject argsArray(cx,
                         NewDenseCopiedArray(cx, args.length(), args.array()));
  return argsArray;
}

JS_FRIEND_API JSAtom* js::GetPropertyNameFromPC(JSScript* script,
                                                jsbytecode* pc) {
  if (!IsGetPropPC(pc) && !IsSetPropPC(pc)) {
    return nullptr;
  }
  return script->getName(pc);
}

JS_FRIEND_API void js::SetWindowProxyClass(JSContext* cx,
                                           const js::Class* clasp) {
  MOZ_ASSERT(!cx->runtime()->maybeWindowProxyClass());
  cx->runtime()->setWindowProxyClass(clasp);
}

JS_FRIEND_API void js::SetWindowProxy(JSContext* cx, HandleObject global,
                                      HandleObject windowProxy) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  cx->check(global, windowProxy);
  MOZ_ASSERT(IsWindowProxy(windowProxy));

  GlobalObject& globalObj = global->as<GlobalObject>();
  globalObj.setWindowProxy(windowProxy);
  globalObj.lexicalEnvironment().setWindowProxyThisValue(windowProxy);
}

JS_FRIEND_API JSObject* js::ToWindowIfWindowProxy(JSObject* obj) {
  if (IsWindowProxy(obj)) {
    return &obj->nonCCWGlobal();
  }
  return obj;
}

JS_FRIEND_API JSObject* js::detail::ToWindowProxyIfWindowSlow(JSObject* obj) {
  if (JSObject* windowProxy = obj->as<GlobalObject>().maybeWindowProxy()) {
    return windowProxy;
  }
  return obj;
}

JS_FRIEND_API bool js::IsWindowProxy(JSObject* obj) {
  // Note: simply checking `obj == obj->global().windowProxy()` is not
  // sufficient: we may have transplanted the window proxy with a CCW.
  // Check the Class to ensure we really have a window proxy.
  return obj->getClass() ==
         obj->runtimeFromAnyThread()->maybeWindowProxyClass();
}

JS_FRIEND_API bool js::detail::IsWindowSlow(JSObject* obj) {
  return obj->as<GlobalObject>().maybeWindowProxy();
}

AutoAssertNoContentJS::AutoAssertNoContentJS(JSContext* cx)
    : context_(cx), prevAllowContentJS_(cx->runtime()->allowContentJS_) {
  cx->runtime()->allowContentJS_ = false;
}

AutoAssertNoContentJS::~AutoAssertNoContentJS() {
  context_->runtime()->allowContentJS_ = prevAllowContentJS_;
}

JS_FRIEND_API void js::EnableAccessValidation(JSContext* cx, bool enabled) {
  cx->enableAccessValidation = enabled;
}

JS_FRIEND_API void js::SetRealmValidAccessPtr(JSContext* cx,
                                              JS::HandleObject global,
                                              bool* accessp) {
  MOZ_ASSERT(global->is<GlobalObject>());
  global->as<GlobalObject>().realm()->setValidAccessPtr(accessp);
}

JS_FRIEND_API bool js::SystemZoneAvailable(JSContext* cx) { return true; }

static LogCtorDtor sLogCtor = nullptr;
static LogCtorDtor sLogDtor = nullptr;

JS_FRIEND_API void js::SetLogCtorDtorFunctions(LogCtorDtor ctor,
                                               LogCtorDtor dtor) {
  MOZ_ASSERT(!sLogCtor && !sLogDtor);
  MOZ_ASSERT(ctor && dtor);
  sLogCtor = ctor;
  sLogDtor = dtor;
}

JS_FRIEND_API void js::LogCtor(void* self, const char* type, uint32_t sz) {
  if (LogCtorDtor fun = sLogCtor) {
    fun(self, type, sz);
  }
}

JS_FRIEND_API void js::LogDtor(void* self, const char* type, uint32_t sz) {
  if (LogCtorDtor fun = sLogDtor) {
    fun(self, type, sz);
  }
}

JS_FRIEND_API JS::Value js::MaybeGetScriptPrivate(JSObject* object) {
  if (!object->is<ScriptSourceObject>()) {
    return UndefinedValue();
  }

  return object->as<ScriptSourceObject>().canonicalPrivate();
}

JS_FRIEND_API uint64_t js::GetGCHeapUsageForObjectZone(JSObject* obj) {
  return obj->zone()->zoneSize.gcBytes();
}

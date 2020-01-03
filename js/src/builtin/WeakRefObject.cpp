/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/WeakRefObject.h"

#include "jsapi.h"
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"

#include "gc/PrivateIterators-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

namespace js {

/* static */
bool WeakRefObject::construct(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // https://tc39.es/proposal-weakrefs/#sec-weak-ref-constructor
  // The WeakRef constructor is not intended to be called as a function and will
  // throw an exception when called in that manner.
  if (!ThrowIfNotConstructing(cx, args, "WeakRef")) {
    return false;
  }

  // https://tc39.es/proposal-weakrefs/#sec-weak-ref-target
  // 1. If NewTarget is undefined, throw a TypeError exception.
  // 2. If Type(target) is not Object, throw a TypeError exception.
  if (!args.get(0).isObject()) {
    ReportNotObject(cx, args.get(0));
    return false;
  }

  // 3. Let weakRef be ? OrdinaryCreateFromConstructor(NewTarget,
  //    "%WeakRefPrototype%", « [[Target]] »).
  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_WeakRef, &proto)) {
    return false;
  }

  Rooted<WeakRefObject*> weakRef(
      cx, NewObjectWithClassProto<WeakRefObject>(cx, proto));
  if (!weakRef) {
    return false;
  }

  RootedObject target(cx);
  target = CheckedUnwrapDynamic(&args[0].toObject(), cx);
  if (!target) {
    ReportAccessDenied(cx);
    return false;
  }

  // 4. Perfom ! KeepDuringJob(target).
  if (!target->zone()->keepDuringJob(target)) {
    return false;
  };

  // 5. Set weakRef.[[Target]] to target.
  weakRef->setPrivateGCThing(target);

  // Wrap the weakRef into the target's compartment.
  RootedObject wrappedWeakRef(cx, weakRef);
  AutoRealm ar(cx, target);
  if (!JS_WrapObject(cx, &wrappedWeakRef)) {
    return false;
  }

  if (JS_IsDeadWrapper(wrappedWeakRef)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
    return false;
  }

  // Add an entry to the per-zone maps from target JS object to a list of weak
  // ref objects.
  gc::GCRuntime* gc = &cx->runtime()->gc;
  if (!gc->registerWeakRef(target, wrappedWeakRef)) {
    return false;
  };

  // 6. Return weakRef.
  args.rval().setObject(*weakRef);
  return true;
}

/* static */
void WeakRefObject::trace(JSTracer* trc, JSObject* obj) {
  WeakRefObject* weakRef = &obj->as<WeakRefObject>();

  if (trc->traceWeakEdges()) {
    JSObject* target = weakRef->target();
    if (target) {
      TraceManuallyBarrieredEdge(trc, &target, "WeakRefObject::target");
    }
    weakRef->setPrivate(target);
  }
}

/* static */
void WeakRefObject::finalize(JSFreeOp* fop, JSObject* obj) {
  JSContext* cx = fop->runtime()->mainContextFromOwnThread();
  WeakRefObject* weakRef = &obj->as<WeakRefObject>();
  JSObject* target = weakRef->target();
  if (!target) {
    return;
  }

  gc::GCRuntime* gc = &fop->runtime()->gc;
  // TODO Bug 1602625: test unregisterWeakRef
  // not tested, weakRef->target() has been swept.
  gc->unregisterWeakRef(cx, target, weakRef);
}

const JSClassOps WeakRefObject::classOps_ = {
    nullptr,   // addProperty
    nullptr,   // delProperty
    nullptr,   // enumerate
    nullptr,   // newEnumerate
    nullptr,   // resolve
    nullptr,   // mayResolve
    finalize,  // finalize
    nullptr,   // call
    nullptr,   // hasInstance
    nullptr,   // construct
    trace,     // trace
};

const ClassSpec WeakRefObject::classSpec_ = {
    GenericCreateConstructor<WeakRefObject::construct, 0,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<WeakRefObject>,
    nullptr,
    nullptr,
    WeakRefObject::methods,
    WeakRefObject::properties,
};

const JSClass WeakRefObject::class_ = {
    "WeakRef",
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_CACHED_PROTO(JSProto_WeakRef) |
        JSCLASS_FOREGROUND_FINALIZE,
    &classOps_, &classSpec_};

const JSClass WeakRefObject::protoClass_ = {
    // https://tc39.es/proposal-weakrefs/#sec-weak-ref.prototype
    // https://tc39.es/proposal-weakrefs/#sec-properties-of-the-weak-ref-prototype-object
    "WeakRefPrototype", JSCLASS_HAS_CACHED_PROTO(JSProto_WeakRef),
    JS_NULL_CLASS_OPS, &classSpec_};

const JSPropertySpec WeakRefObject::properties[] = {
    JS_STRING_SYM_PS(toStringTag, "WeakRef", JSPROP_READONLY), JS_PS_END};

const JSFunctionSpec WeakRefObject::methods[] = {JS_FN("deref", deref, 2, 0),
                                                 JS_FS_END};

/* static */
bool WeakRefObject::deref(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // https://tc39.es/proposal-weakrefs/#sec-weak-ref.prototype.deref
  // 1. Let weakRef be the this value.
  // 2. If Type(weakRef) is not Object, throw a TypeError exception.
  // 3. If weakRef does not have a [[Target]] internal slot, throw a TypeError
  //    exception.
  if (!args.thisv().isObject() ||
      !args.thisv().toObject().is<WeakRefObject>()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_A_WEAK_REF,
                              "Receiver of WeakRef.deref call");
    return false;
  }

  Rooted<WeakRefObject*> weakRef(cx,
                                 &args.thisv().toObject().as<WeakRefObject>());

  // 4. Let target be the value of weakRef.[[Target]].
  // 5. If target is not empty,
  //    a. Perform ! KeepDuringJob(target).
  //    b. Return target.
  // 6. Return undefined.
  if (!weakRef->target()) {
    args.rval().setUndefined();
    return true;
  }

  RootedObject target(cx, weakRef->target());
  if (!target->zone()->keepDuringJob(target)) {
    return false;
  }

  // We need to perform a read barrier on the object.
  JSObject::readBarrier(target);

  // Target should be wrapped into the current realm before returning it.
  RootedObject wrappedTarget(cx, target);
  if (!JS_WrapObject(cx, &wrappedTarget)) {
    return false;
  }

  args.rval().setObject(*wrappedTarget);
  return true;
}

inline JSObject* WeakRefObject::target() {
  return static_cast<JSObject*>(getPrivate());
}

void WeakRefObject::setTarget(JSObject* target) { setPrivate(target); }

namespace gc {

bool GCRuntime::registerWeakRef(HandleObject target, HandleObject weakRef) {
  MOZ_ASSERT(!IsCrossCompartmentWrapper(target));
  MOZ_ASSERT(UncheckedUnwrap(weakRef)->is<WeakRefObject>());
  MOZ_ASSERT(target->compartment() == weakRef->compartment());

  auto& map = target->zone()->weakRefMap();
  auto ptr = map.lookupForAdd(target);
  if (!ptr && !map.add(ptr, target, WeakRefHeapPtrVector(target->zone()))) {
    return false;
  }

  auto& refs = ptr->value();
  return refs.emplaceBack(weakRef);
}

bool GCRuntime::unregisterWeakRef(JSContext* cx, JSObject* target,
                                  WeakRefObject* weakRef) {
  auto& map = target->zone()->weakRefMap();
  auto ptr = map.lookup(target);
  if (!ptr) {
    return false;
  }

  ptr->value().eraseIfEqual(weakRef);
  return true;
}

void GCRuntime::traceKeptObjects(JSTracer* trc) {
  for (GCZonesIter zone(this); !zone.done(); zone.next()) {
    zone->traceKeptObjects(trc);
  }
}

}  // namespace gc

void WeakRefMap::sweep() {
  for (typename Base::Enum e(*this); !e.empty(); e.popFront()) {
    // If target is dying, clear the target field of all weakRefs, and remove
    // the entry from the map.
    if (JS::GCPolicy<HeapPtrObject>::needsSweep(&e.front().mutableKey())) {
      for (HeapPtrObject& obj : e.front().value()) {
        obj = UncheckedUnwrapWithoutExpose(obj);
        if (!obj->is<WeakRefObject>()) {
          MOZ_ASSERT(JS_IsDeadWrapper(obj));
          continue;
        }

        WeakRefObject* weakRef = &obj->as<WeakRefObject>();
        weakRef->setTarget(nullptr);
      }
      e.front().value().clear();
      e.removeFront();
    } else {
      // Update the target field after compacting.
      e.front().value().sweep(e.front().mutableKey());
    }
  }
}

// Like GCVector::sweep, but this method will also update the target in every
// weakRef in this GCVector.
void WeakRefHeapPtrVector::sweep(HeapPtrObject& target) {
  HeapPtrObject* src = begin();
  HeapPtrObject* dst = begin();
  while (src != end()) {
    bool needsSweep = JS::GCPolicy<HeapPtrObject>::needsSweep(src);
    JSObject* obj = UncheckedUnwrapWithoutExpose(*src);
    if (!obj->is<WeakRefObject>()) {
      MOZ_ASSERT(JS_IsDeadWrapper(obj));
      continue;
    }

    WeakRefObject* weakRef = &obj->as<WeakRefObject>();

    if (needsSweep) {
      weakRef->setTarget(nullptr);
    } else {
      weakRef->setTarget(target.get());

      if (src != dst) {
        *dst = std::move(*src);
      }
      dst++;
    }
    src++;
  }

  MOZ_ASSERT(dst <= end());
  shrinkBy(end() - dst);
}

}  // namespace js

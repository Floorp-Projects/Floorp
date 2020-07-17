/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implementation of JS FinalizationRegistry objects.

#include "builtin/FinalizationRegistryObject.h"

#include "mozilla/ScopeExit.h"

#include "gc/Zone.h"
#include "vm/GlobalObject.h"
#include "vm/PlainObject.h"  // js::PlainObject

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

///////////////////////////////////////////////////////////////////////////
// FinalizationRecordObject

const JSClass FinalizationRecordObject::class_ = {
    "FinalizationRecord", JSCLASS_HAS_RESERVED_SLOTS(SlotCount), &classOps_,
    JS_NULL_CLASS_SPEC};

const JSClassOps FinalizationRecordObject::classOps_ = {
    nullptr,                          // addProperty
    nullptr,                          // delProperty
    nullptr,                          // enumerate
    nullptr,                          // newEnumerate
    nullptr,                          // resolve
    nullptr,                          // mayResolve
    nullptr,                          // finalize
    nullptr,                          // call
    nullptr,                          // hasInstance
    nullptr,                          // construct
    FinalizationRecordObject::trace,  // trace
};

/* static */
FinalizationRecordObject* FinalizationRecordObject::create(
    JSContext* cx, HandleFinalizationRegistryObject registry,
    HandleValue heldValue) {
  MOZ_ASSERT(registry);

  auto record = NewObjectWithGivenProto<FinalizationRecordObject>(cx, nullptr);
  if (!record) {
    return nullptr;
  }

  MOZ_ASSERT(registry->compartment() == record->compartment());

  record->initReservedSlot(WeakRegistrySlot, PrivateValue(registry));
  record->initReservedSlot(HeldValueSlot, heldValue);

  return record;
}

FinalizationRegistryObject* FinalizationRecordObject::registryDuringGC(
    gc::GCRuntime* gc) const {
  MOZ_ASSERT(JS::RuntimeHeapIsMajorCollecting());

  FinalizationRegistryObject* registry = registryUnbarriered();

  // Perform a manual read barrier. This is the only place where the GC itself
  // needs to perform a read barrier so we must work around our assertions that
  // this doesn't happen.
  if (registry->zone()->isGCMarking()) {
    FinalizationRegistryObject* tmp = registry;
    TraceManuallyBarrieredEdge(&gc->marker, &tmp,
                               "FinalizationRegistry read barrier");
    MOZ_ASSERT(tmp == registry);
  } else if (registry->isMarkedGray()) {
    gc::UnmarkGrayGCThingUnchecked(gc->rt, JS::GCCellPtr(registry));
  }

  return registry;
}

FinalizationRegistryObject* FinalizationRecordObject::registryUnbarriered()
    const {
  Value value = getReservedSlot(WeakRegistrySlot);
  if (value.isUndefined()) {
    return nullptr;
  }
  return static_cast<FinalizationRegistryObject*>(value.toPrivate());
}

Value FinalizationRecordObject::heldValue() const {
  return getReservedSlot(HeldValueSlot);
}

bool FinalizationRecordObject::isActive() const {
  MOZ_ASSERT_IF(!registryUnbarriered(), heldValue().isUndefined());
  return registryUnbarriered();
}

void FinalizationRecordObject::clear() {
  MOZ_ASSERT(registryUnbarriered());
  setReservedSlot(WeakRegistrySlot, UndefinedValue());
  setReservedSlot(HeldValueSlot, UndefinedValue());
}

bool FinalizationRecordObject::sweep() {
  FinalizationRegistryObject* obj = registryUnbarriered();
  MOZ_ASSERT(obj);

  if (IsAboutToBeFinalizedUnbarriered(&obj)) {
    clear();
    return false;
  }

  return true;
}

/* static */
void FinalizationRecordObject::trace(JSTracer* trc, JSObject* obj) {
  if (!trc->traceWeakEdges()) {
    return;
  }

  auto record = &obj->as<FinalizationRecordObject>();
  FinalizationRegistryObject* registry = record->registryUnbarriered();
  if (!registry) {
    return;
  }

  TraceManuallyBarrieredEdge(trc, &registry,
                             "FinalizationRecordObject weak registry");
  if (registry != record->registryUnbarriered()) {
    record->setReservedSlot(WeakRegistrySlot, PrivateValue(registry));
  }
}

///////////////////////////////////////////////////////////////////////////
// FinalizationRegistrationsObject

const JSClass FinalizationRegistrationsObject::class_ = {
    "FinalizationRegistrations",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount) | JSCLASS_BACKGROUND_FINALIZE,
    &classOps_, JS_NULL_CLASS_SPEC};

const JSClassOps FinalizationRegistrationsObject::classOps_ = {
    nullptr,                                    // addProperty
    nullptr,                                    // delProperty
    nullptr,                                    // enumerate
    nullptr,                                    // newEnumerate
    nullptr,                                    // resolve
    nullptr,                                    // mayResolve
    FinalizationRegistrationsObject::finalize,  // finalize
    nullptr,                                    // call
    nullptr,                                    // hasInstance
    nullptr,                                    // construct
    FinalizationRegistrationsObject::trace,     // trace
};

/* static */
FinalizationRegistrationsObject* FinalizationRegistrationsObject::create(
    JSContext* cx) {
  auto records = cx->make_unique<WeakFinalizationRecordVector>(cx->zone());
  if (!records) {
    return nullptr;
  }

  auto object =
      NewObjectWithGivenProto<FinalizationRegistrationsObject>(cx, nullptr);
  if (!object) {
    return nullptr;
  }

  InitReservedSlot(object, RecordsSlot, records.release(),
                   MemoryUse::FinalizationRecordVector);

  return object;
}

/* static */
void FinalizationRegistrationsObject::trace(JSTracer* trc, JSObject* obj) {
  if (!trc->traceWeakEdges()) {
    return;
  }

  auto* self = &obj->as<FinalizationRegistrationsObject>();
  if (WeakFinalizationRecordVector* records = self->records()) {
    TraceRange(trc, records->length(), records->begin(),
               "FinalizationRegistrationsObject records");
  }
}

/* static */
void FinalizationRegistrationsObject::finalize(JSFreeOp* fop, JSObject* obj) {
  auto* self = &obj->as<FinalizationRegistrationsObject>();
  fop->delete_(obj, self->records(), MemoryUse::FinalizationRecordVector);
}

inline WeakFinalizationRecordVector*
FinalizationRegistrationsObject::records() {
  return static_cast<WeakFinalizationRecordVector*>(privatePtr());
}

inline const WeakFinalizationRecordVector*
FinalizationRegistrationsObject::records() const {
  return static_cast<const WeakFinalizationRecordVector*>(privatePtr());
}

inline void* FinalizationRegistrationsObject::privatePtr() const {
  Value value = getReservedSlot(RecordsSlot);
  if (value.isUndefined()) {
    return nullptr;
  }
  void* ptr = value.toPrivate();
  MOZ_ASSERT(ptr);
  return ptr;
}

inline bool FinalizationRegistrationsObject::isEmpty() const {
  MOZ_ASSERT(records());
  return records()->empty();
}

inline bool FinalizationRegistrationsObject::append(
    HandleFinalizationRecordObject record) {
  MOZ_ASSERT(records());
  return records()->append(record);
}

inline void FinalizationRegistrationsObject::remove(
    HandleFinalizationRecordObject record) {
  MOZ_ASSERT(records());
  records()->eraseIfEqual(record);
}

inline void FinalizationRegistrationsObject::sweep() {
  MOZ_ASSERT(records());
  return records()->sweep();
}

///////////////////////////////////////////////////////////////////////////
// FinalizationRegistryObject

// Bug 1600300: FinalizationRegistryObject is foreground finalized so that
// HeapPtr destructors never see referents with released arenas. When this is
// fixed we may be able to make this background finalized again.
const JSClass FinalizationRegistryObject::class_ = {
    "FinalizationRegistry",
    JSCLASS_HAS_CACHED_PROTO(JSProto_FinalizationRegistry) |
        JSCLASS_HAS_RESERVED_SLOTS(SlotCount) | JSCLASS_FOREGROUND_FINALIZE,
    &classOps_, &classSpec_};

const JSClass FinalizationRegistryObject::protoClass_ = {
    "FinalizationRegistry.prototype",
    JSCLASS_HAS_CACHED_PROTO(JSProto_FinalizationRegistry), JS_NULL_CLASS_OPS,
    &classSpec_};

const JSClassOps FinalizationRegistryObject::classOps_ = {
    nullptr,                               // addProperty
    nullptr,                               // delProperty
    nullptr,                               // enumerate
    nullptr,                               // newEnumerate
    nullptr,                               // resolve
    nullptr,                               // mayResolve
    FinalizationRegistryObject::finalize,  // finalize
    nullptr,                               // call
    nullptr,                               // hasInstance
    nullptr,                               // construct
    FinalizationRegistryObject::trace,     // trace
};

const ClassSpec FinalizationRegistryObject::classSpec_ = {
    GenericCreateConstructor<construct, 1, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<FinalizationRegistryObject>,
    nullptr,
    nullptr,
    methods_,
    properties_};

const JSFunctionSpec FinalizationRegistryObject::methods_[] = {
    JS_FN(js_register_str, register_, 2, 0),
    JS_FN(js_unregister_str, unregister, 1, 0),
    JS_FN(js_cleanupSome_str, cleanupSome, 0, 0), JS_FS_END};

const JSPropertySpec FinalizationRegistryObject::properties_[] = {
    JS_STRING_SYM_PS(toStringTag, "FinalizationRegistry", JSPROP_READONLY),
    JS_PS_END};

/* static */
bool FinalizationRegistryObject::construct(JSContext* cx, unsigned argc,
                                           Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "FinalizationRegistry")) {
    return false;
  }

  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(
          cx, args, JSProto_FinalizationRegistry, &proto)) {
    return false;
  }

  RootedObject cleanupCallback(
      cx, ValueToCallable(cx, args.get(0), 1, NO_CONSTRUCT));
  if (!cleanupCallback) {
    return false;
  }

  // It's problematic storing a CCW to a global in another compartment because
  // you don't know how far to unwrap it to get the original object
  // back. Instead store a CCW to a plain object in the same compartment as the
  // global (this uses Object.prototype).
  RootedObject incumbentObject(cx);
  if (!GetObjectFromIncumbentGlobal(cx, &incumbentObject)) {
    return false;
  }

  Rooted<UniquePtr<ObjectWeakMap>> registrations(
      cx, cx->make_unique<ObjectWeakMap>(cx));
  if (!registrations) {
    return false;
  }

  Rooted<UniquePtr<FinalizationRecordSet>> activeRecords(
      cx, cx->make_unique<FinalizationRecordSet>(cx->zone()));
  if (!activeRecords) {
    return false;
  }

  Rooted<UniquePtr<FinalizationRecordVector>> recordsToBeCleanedUp(
      cx, cx->make_unique<FinalizationRecordVector>(cx->zone()));
  if (!recordsToBeCleanedUp) {
    return false;
  }

  HandlePropertyName funName = cx->names().empty;
  RootedFunction doCleanupFunction(
      cx, NewNativeFunction(cx, doCleanup, 0, funName,
                            gc::AllocKind::FUNCTION_EXTENDED));
  if (!doCleanupFunction) {
    return false;
  }

  FinalizationRegistryObject* registry =
      NewObjectWithClassProto<FinalizationRegistryObject>(cx, proto);
  if (!registry) {
    return false;
  }

  registry->initReservedSlot(CleanupCallbackSlot,
                             ObjectValue(*cleanupCallback));
  registry->initReservedSlot(IncumbentObjectSlot,
                             ObjectValue(*incumbentObject));
  InitReservedSlot(registry, RegistrationsSlot, registrations.release(),
                   MemoryUse::FinalizationRegistryRegistrations);
  InitReservedSlot(registry, ActiveRecords, activeRecords.release(),
                   MemoryUse::FinalizationRegistryRecordSet);
  InitReservedSlot(registry, RecordsToBeCleanedUpSlot,
                   recordsToBeCleanedUp.release(),
                   MemoryUse::FinalizationRegistryRecordVector);
  registry->initReservedSlot(IsQueuedForCleanupSlot, BooleanValue(false));
  registry->initReservedSlot(DoCleanupFunctionSlot,
                             ObjectValue(*doCleanupFunction));

  doCleanupFunction->setExtendedSlot(DoCleanupFunction_RegistrySlot,
                                     ObjectValue(*registry));

  if (!cx->runtime()->gc.addFinalizationRegistry(cx, registry)) {
    return false;
  }

  args.rval().setObject(*registry);
  return true;
}

/* static */
void FinalizationRegistryObject::trace(JSTracer* trc, JSObject* obj) {
  auto registry = &obj->as<FinalizationRegistryObject>();

  // Trace the registrations weak map. At most this traces the
  // FinalizationRegistrationsObject values of the map; the contents of those
  // objects are weakly held and are not traced.
  if (ObjectWeakMap* registrations = registry->registrations()) {
    registrations->trace(trc);
  }

  // The active record set is weakly held and is not traced. For moving GC this
  // is updated in sweep(), which is called for all FinalizationRegistryObjects
  // in a zone.

  if (FinalizationRecordVector* records = registry->recordsToBeCleanedUp()) {
    records->trace(trc);
  }
}

void FinalizationRegistryObject::sweep() {
  // Sweep the set of active records. These may die if CCWs to record objects
  // get nuked.
  MOZ_ASSERT(activeRecords());
  activeRecords()->sweep();

  // Sweep the contents of the registrations weak map's values.
  MOZ_ASSERT(registrations());
  for (ObjectValueWeakMap::Enum e(registrations()->valueMap()); !e.empty();
       e.popFront()) {
    auto registrations =
        &e.front().value().toObject().as<FinalizationRegistrationsObject>();
    registrations->sweep();
    if (registrations->isEmpty()) {
      e.removeFront();
    }
  }
}

/* static */
void FinalizationRegistryObject::finalize(JSFreeOp* fop, JSObject* obj) {
  auto registry = &obj->as<FinalizationRegistryObject>();

  // Clear the weak pointer to the registry in all remaining records.

  // FinalizationRegistries are foreground finalized whereas record objects are
  // background finalized, so record objects are guaranteed to still be
  // accessible at this point.
  MOZ_ASSERT(registry->getClass()->flags & JSCLASS_FOREGROUND_FINALIZE);

  FinalizationRecordSet* allRecords = registry->activeRecords();
  for (auto r = allRecords->all(); !r.empty(); r.popFront()) {
    auto record = &r.front()->as<FinalizationRecordObject>();
    MOZ_ASSERT(!(record->getClass()->flags & JSCLASS_FOREGROUND_FINALIZE));
    MOZ_ASSERT(record->zone() == registry->zone());
    if (record->isActive()) {
      record->clear();
    }
  }

  fop->delete_(obj, registry->registrations(),
               MemoryUse::FinalizationRegistryRegistrations);
  fop->delete_(obj, registry->activeRecords(),
               MemoryUse::FinalizationRegistryRecordSet);
  fop->delete_(obj, registry->recordsToBeCleanedUp(),
               MemoryUse::FinalizationRegistryRecordVector);
}

JSObject* FinalizationRegistryObject::cleanupCallback() const {
  Value value = getReservedSlot(CleanupCallbackSlot);
  if (value.isUndefined()) {
    return nullptr;
  }
  return &value.toObject();
}

JSObject* FinalizationRegistryObject::incumbentObject() const {
  Value value = getReservedSlot(IncumbentObjectSlot);
  if (value.isUndefined()) {
    return nullptr;
  }
  return &value.toObject();
}

ObjectWeakMap* FinalizationRegistryObject::registrations() const {
  Value value = getReservedSlot(RegistrationsSlot);
  if (value.isUndefined()) {
    return nullptr;
  }
  return static_cast<ObjectWeakMap*>(value.toPrivate());
}

FinalizationRecordSet* FinalizationRegistryObject::activeRecords() const {
  Value value = getReservedSlot(ActiveRecords);
  if (value.isUndefined()) {
    return nullptr;
  }
  return static_cast<FinalizationRecordSet*>(value.toPrivate());
}

FinalizationRecordVector* FinalizationRegistryObject::recordsToBeCleanedUp()
    const {
  Value value = getReservedSlot(RecordsToBeCleanedUpSlot);
  if (value.isUndefined()) {
    return nullptr;
  }
  return static_cast<FinalizationRecordVector*>(value.toPrivate());
}

bool FinalizationRegistryObject::isQueuedForCleanup() const {
  return getReservedSlot(IsQueuedForCleanupSlot).toBoolean();
}

JSFunction* FinalizationRegistryObject::doCleanupFunction() const {
  Value value = getReservedSlot(DoCleanupFunctionSlot);
  if (value.isUndefined()) {
    return nullptr;
  }
  return &value.toObject().as<JSFunction>();
}

void FinalizationRegistryObject::queueRecordToBeCleanedUp(
    FinalizationRecordObject* record) {
  AutoEnterOOMUnsafeRegion oomUnsafe;
  if (!recordsToBeCleanedUp()->append(record)) {
    oomUnsafe.crash("FinalizationRegistryObject::queueRecordsToBeCleanedUp");
  }
}

void FinalizationRegistryObject::setQueuedForCleanup(bool value) {
  MOZ_ASSERT(value != isQueuedForCleanup());
  setReservedSlot(IsQueuedForCleanupSlot, BooleanValue(value));
}

// FinalizationRegistry.prototype.register(target, heldValue [, unregisterToken
// ])
// https://tc39.es/proposal-weakrefs/#sec-finalization-registry.prototype.register
/* static */
bool FinalizationRegistryObject::register_(JSContext* cx, unsigned argc,
                                           Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // 1. Let finalizationRegistry be the this value.
  // 2. If Type(finalizationRegistry) is not Object, throw a TypeError
  // exception.
  // 3. If finalizationRegistry does not have a [[Cells]] internal slot, throw a
  // TypeError exception.
  if (!args.thisv().isObject() ||
      !args.thisv().toObject().is<FinalizationRegistryObject>()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_A_FINALIZATION_REGISTRY,
                              "Receiver of FinalizationRegistry.register call");
    return false;
  }

  RootedFinalizationRegistryObject registry(
      cx, &args.thisv().toObject().as<FinalizationRegistryObject>());

  // 4. If Type(target) is not Object, throw a TypeError exception.
  if (!args.get(0).isObject()) {
    JS_ReportErrorNumberASCII(
        cx, GetErrorMessage, nullptr, JSMSG_OBJECT_REQUIRED,
        "target argument to FinalizationRegistry.register");
    return false;
  }

  RootedObject target(cx, &args[0].toObject());

  // 5. If SameValue(target, heldValue), throw a TypeError exception.
  if (args.get(1).isObject() && &args.get(1).toObject() == target) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BAD_HELD_VALUE);
    return false;
  }

  HandleValue heldValue = args.get(1);

  // 6. If Type(unregisterToken) is not Object,
  //    a. If unregisterToken is not undefined, throw a TypeError exception.
  if (!args.get(2).isUndefined() && !args.get(2).isObject()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BAD_UNREGISTER_TOKEN,
                              "FinalizationRegistry.register");
    return false;
  }

  RootedObject unregisterToken(cx);
  if (!args.get(2).isUndefined()) {
    unregisterToken = &args[2].toObject();
  }

  // Create the finalization record representing this target and heldValue.
  Rooted<FinalizationRecordObject*> record(
      cx, FinalizationRecordObject::create(cx, registry, heldValue));
  if (!record) {
    return false;
  }

  // Add the record to the list of records with live targets.
  if (!registry->activeRecords()->put(record)) {
    ReportOutOfMemory(cx);
    return false;
  }

  auto recordsGuard = mozilla::MakeScopeExit(
      [&] { registry->activeRecords()->remove(record); });

  // Add the record to the registrations if an unregister token was supplied.
  if (unregisterToken &&
      !addRegistration(cx, registry, unregisterToken, record)) {
    return false;
  }

  auto registrationsGuard = mozilla::MakeScopeExit([&] {
    if (unregisterToken) {
      removeRegistrationOnError(registry, unregisterToken, record);
    }
  });

  // Fully unwrap the target to pass it to the GC.
  RootedObject unwrappedTarget(cx);
  unwrappedTarget = CheckedUnwrapDynamic(target, cx);
  if (!unwrappedTarget) {
    ReportAccessDenied(cx);
    return false;
  }

  // If the target is a DOM wrapper, preserve it.
  if (!preserveDOMWrapper(cx, target)) {
    return false;
  }

  // Wrap the record into the compartment of the target.
  RootedObject wrappedRecord(cx, record);
  AutoRealm ar(cx, unwrappedTarget);
  if (!JS_WrapObject(cx, &wrappedRecord)) {
    return false;
  }

  if (JS_IsDeadWrapper(wrappedRecord)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
    return false;
  }

  // Register the record with the target.
  gc::GCRuntime* gc = &cx->runtime()->gc;
  if (!gc->registerWithFinalizationRegistry(cx, unwrappedTarget,
                                            wrappedRecord)) {
    return false;
  }

  recordsGuard.release();
  registrationsGuard.release();
  args.rval().setUndefined();
  return true;
}

/* static */
bool FinalizationRegistryObject::preserveDOMWrapper(JSContext* cx,
                                                    HandleObject obj) {
  if (!MaybePreserveDOMWrapper(cx, obj)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BAD_FINALIZATION_REGISTRY_OBJECT);
    return false;
  }

  return true;
}

/* static */
bool FinalizationRegistryObject::addRegistration(
    JSContext* cx, HandleFinalizationRegistryObject registry,
    HandleObject unregisterToken, HandleFinalizationRecordObject record) {
  // Add the record to the list of records associated with this unregister
  // token.

  MOZ_ASSERT(unregisterToken);
  MOZ_ASSERT(registry->registrations());

  auto& map = *registry->registrations();
  Rooted<FinalizationRegistrationsObject*> recordsObject(cx);
  JSObject* obj = map.lookup(unregisterToken);
  if (obj) {
    recordsObject = &obj->as<FinalizationRegistrationsObject>();
  } else {
    recordsObject = FinalizationRegistrationsObject::create(cx);
    if (!recordsObject || !map.add(cx, unregisterToken, recordsObject)) {
      return false;
    }
  }

  if (!recordsObject->append(record)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

/* static */ void FinalizationRegistryObject::removeRegistrationOnError(
    HandleFinalizationRegistryObject registry, HandleObject unregisterToken,
    HandleFinalizationRecordObject record) {
  // Remove a registration if something went wrong before we added it to the
  // target zone's map. Note that this can't remove a registration after that
  // point.

  MOZ_ASSERT(unregisterToken);
  MOZ_ASSERT(registry->registrations());
  JS::AutoAssertNoGC nogc;

  auto& map = *registry->registrations();
  JSObject* obj = map.lookup(unregisterToken);
  MOZ_ASSERT(obj);
  auto records = &obj->as<FinalizationRegistrationsObject>();
  records->remove(record);

  if (records->empty()) {
    map.remove(unregisterToken);
  }
}

// FinalizationRegistry.prototype.unregister ( unregisterToken )
// https://tc39.es/proposal-weakrefs/#sec-finalization-registry.prototype.unregister
/* static */
bool FinalizationRegistryObject::unregister(JSContext* cx, unsigned argc,
                                            Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // 1. Let finalizationRegistry be the this value.
  // 2. If Type(finalizationRegistry) is not Object, throw a TypeError
  //    exception.
  // 3. If finalizationRegistry does not have a [[Cells]] internal slot, throw a
  //    TypeError exception.
  if (!args.thisv().isObject() ||
      !args.thisv().toObject().is<FinalizationRegistryObject>()) {
    JS_ReportErrorNumberASCII(
        cx, GetErrorMessage, nullptr, JSMSG_NOT_A_FINALIZATION_REGISTRY,
        "Receiver of FinalizationRegistry.unregister call");
    return false;
  }

  RootedFinalizationRegistryObject registry(
      cx, &args.thisv().toObject().as<FinalizationRegistryObject>());

  // 4. If Type(unregisterToken) is not Object, throw a TypeError exception.
  if (!args.get(0).isObject()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BAD_UNREGISTER_TOKEN,
                              "FinalizationRegistry.unregister");
    return false;
  }

  RootedObject unregisterToken(cx, &args[0].toObject());

  // 5. Let removed be false.
  bool removed = false;

  // 6. For each Record { [[Target]], [[HeldValue]], [[UnregisterToken]] } cell
  //    that is an element of finalizationRegistry.[[Cells]], do
  //    a. If SameValue(cell.[[UnregisterToken]], unregisterToken) is true, then
  //       i. Remove cell from finalizationRegistry.[[Cells]].
  //       ii. Set removed to true.

  RootedObject obj(cx, registry->registrations()->lookup(unregisterToken));
  if (obj) {
    auto* records = obj->as<FinalizationRegistrationsObject>().records();
    MOZ_ASSERT(records);
    MOZ_ASSERT(!records->empty());
    for (FinalizationRecordObject* record : *records) {
      if (unregisterRecord(record)) {
        removed = true;
      }
      MOZ_ASSERT(!registry->activeRecords()->has(record));
    }
    registry->registrations()->remove(unregisterToken);
  }

  // 7. Return removed.
  args.rval().setBoolean(removed);
  return true;
}

/* static */
bool FinalizationRegistryObject::unregisterRecord(
    FinalizationRecordObject* record) {
  if (!record->isActive()) {
    return false;
  }

  FinalizationRegistryObject* registry = record->registryUnbarriered();
  MOZ_ASSERT(registry);

  // Clear the fields of this record; it will be removed from the target's
  // list when it is next swept.
  registry->activeRecords()->remove(record);
  record->clear();
  return true;
}

// FinalizationRegistry.prototype.cleanupSome ( [ callback ] )
// https://tc39.es/proposal-weakrefs/#sec-finalization-registry.prototype.cleanupSome
bool FinalizationRegistryObject::cleanupSome(JSContext* cx, unsigned argc,
                                             Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // 1. Let finalizationRegistry be the this value.
  // 2. Perform ? RequireInternalSlot(finalizationRegistry, [[Cells]]).
  if (!args.thisv().isObject() ||
      !args.thisv().toObject().is<FinalizationRegistryObject>()) {
    JS_ReportErrorNumberASCII(
        cx, GetErrorMessage, nullptr, JSMSG_NOT_A_FINALIZATION_REGISTRY,
        "Receiver of FinalizationRegistry.cleanupSome call");
    return false;
  }

  RootedFinalizationRegistryObject registry(
      cx, &args.thisv().toObject().as<FinalizationRegistryObject>());

  // 3. If callback is not undefined and IsCallable(callback) is false, throw a
  //    TypeError exception.
  RootedObject cleanupCallback(cx);
  if (!args.get(0).isUndefined()) {
    cleanupCallback = ValueToCallable(cx, args.get(0), -1, NO_CONSTRUCT);
    if (!cleanupCallback) {
      return false;
    }
  }

  if (!cleanupQueuedRecords(cx, registry, cleanupCallback)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool FinalizationRegistryObject::doCleanup(JSContext* cx, unsigned argc,
                                           Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  RootedFunction callee(cx, &args.callee().as<JSFunction>());

  Value value = callee->getExtendedSlot(DoCleanupFunction_RegistrySlot);
  RootedFinalizationRegistryObject registry(
      cx, &value.toObject().as<FinalizationRegistryObject>());

  registry->setQueuedForCleanup(false);
  return cleanupQueuedRecords(cx, registry);
}

// CleanupFinalizationRegistry ( finalizationRegistry [ , callback ] )
// https://tc39.es/proposal-weakrefs/#sec-cleanup-finalization-registry
/* static */
bool FinalizationRegistryObject::cleanupQueuedRecords(
    JSContext* cx, HandleFinalizationRegistryObject registry,
    HandleObject callbackArg) {
  MOZ_ASSERT(cx->compartment() == registry->compartment());

  // 2. If callback is undefined, set callback to
  //    finalizationRegistry.[[CleanupCallback]].
  RootedValue callback(cx);
  if (callbackArg) {
    callback.setObject(*callbackArg);
  } else {
    JSObject* cleanupCallback = registry->cleanupCallback();
    MOZ_ASSERT(cleanupCallback);
    callback.setObject(*cleanupCallback);
  }

  // 3. While finalizationRegistry.[[Cells]] contains a Record cell such that
  //    cell.[[WeakRefTarget]] is empty, then an implementation may perform the
  //    following steps,
  //    a. Choose any such cell.
  //    b. Remove cell from finalizationRegistry.[[Cells]].
  //    c. Perform ? Call(callback, undefined, « cell.[[HeldValue]] »).

  RootedValue heldValue(cx);
  RootedValue rval(cx);
  FinalizationRecordVector* records = registry->recordsToBeCleanedUp();
  FinalizationRecordSet* activeRecords = registry->activeRecords();
  while (!records->empty()) {
    FinalizationRecordObject* record = records->popCopy();

    // Skip over records that have been unregistered.
    if (!record->isActive()) {
      continue;
    }

    heldValue.set(record->heldValue());

    activeRecords->remove(record);
    record->clear();

    if (!Call(cx, callback, UndefinedHandleValue, heldValue, &rval)) {
      return false;
    }
  }

  return true;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implementation of JS FinalizationGroup objects.

#include "builtin/FinalizationGroupObject.h"

#include "mozilla/ScopeExit.h"

#include "gc/Zone.h"
#include "vm/GlobalObject.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

///////////////////////////////////////////////////////////////////////////
// FinalizationRecordObject

const JSClass FinalizationRecordObject::class_ = {
    "FinalizationRecord", JSCLASS_HAS_RESERVED_SLOTS(SlotCount),
    JS_NULL_CLASS_OPS, JS_NULL_CLASS_SPEC};

/* static */
FinalizationRecordObject* FinalizationRecordObject::create(
    JSContext* cx, HandleFinalizationGroupObject group, HandleValue holdings) {
  MOZ_ASSERT(group);

  auto record = NewObjectWithNullTaggedProto<FinalizationRecordObject>(cx);
  if (!record) {
    return nullptr;
  }

  record->initReservedSlot(GroupSlot, ObjectValue(*group));
  record->initReservedSlot(HoldingsSlot, holdings);

  return record;
}

FinalizationGroupObject* FinalizationRecordObject::group() const {
  Value value = getReservedSlot(GroupSlot);
  if (value.isNull()) {
    return nullptr;
  }
  return &value.toObject().as<FinalizationGroupObject>();
}

Value FinalizationRecordObject::holdings() const {
  return getReservedSlot(HoldingsSlot);
}

void FinalizationRecordObject::clear() {
  MOZ_ASSERT(group());
  setReservedSlot(GroupSlot, NullValue());
  setReservedSlot(HoldingsSlot, UndefinedValue());
}

///////////////////////////////////////////////////////////////////////////
// FinalizationRecordVectorObject

const JSClass FinalizationRecordVectorObject::class_ = {
    "FinalizationRecordVector",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount) | JSCLASS_BACKGROUND_FINALIZE,
    &classOps_, JS_NULL_CLASS_SPEC};

const JSClassOps FinalizationRecordVectorObject::classOps_ = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* enumerate   */
    nullptr, /* newEnumerate */
    nullptr, /* resolve     */
    nullptr, /* mayResolve  */
    FinalizationRecordVectorObject::finalize,
    nullptr, /* call        */
    nullptr, /* hasInstance */
    nullptr, /* construct   */
    FinalizationRecordVectorObject::trace};

/* static */
FinalizationRecordVectorObject* FinalizationRecordVectorObject::create(
    JSContext* cx) {
  auto records = cx->make_unique<RecordVector>(cx->zone());
  if (!records) {
    return nullptr;
  }

  auto object =
      NewObjectWithNullTaggedProto<FinalizationRecordVectorObject>(cx);
  if (!object) {
    return nullptr;
  }

  InitReservedSlot(object, RecordsSlot, records.release(),
                   MemoryUse::FinalizationRecordVector);

  return object;
}

/* static */
void FinalizationRecordVectorObject::trace(JSTracer* trc, JSObject* obj) {
  auto rv = &obj->as<FinalizationRecordVectorObject>();
  rv->records().trace(trc);
}

/* static */
void FinalizationRecordVectorObject::finalize(JSFreeOp* fop, JSObject* obj) {
  auto rv = &obj->as<FinalizationRecordVectorObject>();
  fop->delete_(obj, &rv->records(), MemoryUse::FinalizationRecordVector);
}

inline FinalizationRecordVectorObject::RecordVector&
FinalizationRecordVectorObject::records() {
  return *static_cast<RecordVector*>(privatePtr());
}

inline const FinalizationRecordVectorObject::RecordVector&
FinalizationRecordVectorObject::records() const {
  return *static_cast<const RecordVector*>(privatePtr());
}

inline void* FinalizationRecordVectorObject::privatePtr() const {
  void* ptr = getReservedSlot(RecordsSlot).toPrivate();
  MOZ_ASSERT(ptr);
  return ptr;
}

inline bool FinalizationRecordVectorObject::isEmpty() const {
  return records().empty();
}

inline bool FinalizationRecordVectorObject::append(
    HandleFinalizationRecordObject record) {
  return records().append(record);
}

inline void FinalizationRecordVectorObject::remove(
    HandleFinalizationRecordObject record) {
  records().eraseIfEqual(record);
}

///////////////////////////////////////////////////////////////////////////
// FinalizationGroupObject

const JSClass FinalizationGroupObject::class_ = {
    "FinalizationGroup",
    JSCLASS_HAS_CACHED_PROTO(JSProto_FinalizationGroup) |
        JSCLASS_HAS_RESERVED_SLOTS(SlotCount) | JSCLASS_BACKGROUND_FINALIZE,
    &classOps_, &classSpec_};

const JSClass FinalizationGroupObject::protoClass_ = {
    "FinalizationGroupPrototype",
    JSCLASS_HAS_CACHED_PROTO(JSProto_FinalizationGroup), JS_NULL_CLASS_OPS,
    &classSpec_};

const JSClassOps FinalizationGroupObject::classOps_ = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* enumerate   */
    nullptr, /* newEnumerate */
    nullptr, /* resolve     */
    nullptr, /* mayResolve  */
    FinalizationGroupObject::finalize,
    nullptr, /* call        */
    nullptr, /* hasInstance */
    nullptr, /* construct   */
    FinalizationGroupObject::trace};

const ClassSpec FinalizationGroupObject::classSpec_ = {
    GenericCreateConstructor<construct, 1, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<FinalizationGroupObject>,
    nullptr,
    nullptr,
    methods_,
    properties_};

const JSFunctionSpec FinalizationGroupObject::methods_[] = {
    JS_FN(js_register_str, register_, 2, 0),
    JS_FN(js_unregister_str, unregister, 1, 0),
    JS_FN(js_cleanupSome_str, cleanupSome, 0, 0), JS_FS_END};

const JSPropertySpec FinalizationGroupObject::properties_[] = {
    JS_STRING_SYM_PS(toStringTag, "FinalizationGroup", JSPROP_READONLY),
    JS_PS_END};

/* static */
bool FinalizationGroupObject::construct(JSContext* cx, unsigned argc,
                                        Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "FinalizationGroup")) {
    return false;
  }

  RootedObject cleanupCallback(
      cx, ValueToCallable(cx, args.get(0), 1, NO_CONSTRUCT));
  if (!cleanupCallback) {
    return false;
  }

  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_FinalizationGroup,
                                          &proto)) {
    return false;
  }

  Rooted<UniquePtr<ObjectWeakMap>> registrations(
      cx, cx->make_unique<ObjectWeakMap>(cx));
  if (!registrations) {
    return false;
  }

  Rooted<UniquePtr<HoldingsVector>> holdings(
      cx, cx->make_unique<HoldingsVector>(cx->zone()));
  if (!holdings) {
    return false;
  }

  FinalizationGroupObject* group =
      NewObjectWithClassProto<FinalizationGroupObject>(cx, proto);
  if (!group) {
    return false;
  }

  group->initReservedSlot(CleanupCallbackSlot, ObjectValue(*cleanupCallback));
  InitReservedSlot(group, RegistrationsSlot, registrations.release(),
                   MemoryUse::FinalizationGroupRegistrations);
  InitReservedSlot(group, HoldingsToBeCleanedUpSlot, holdings.release(),
                   MemoryUse::FinalizationGroupHoldingsVector);
  group->initReservedSlot(IsQueuedForCleanupSlot, BooleanValue(false));
  group->initReservedSlot(IsCleanupJobActiveSlot, BooleanValue(false));

  args.rval().setObject(*group);
  return true;
}

/* static */
void FinalizationGroupObject::trace(JSTracer* trc, JSObject* obj) {
  auto group = &obj->as<FinalizationGroupObject>();
  if (HoldingsVector* holdings = group->holdingsToBeCleanedUp()) {
    holdings->trace(trc);
  }
  if (ObjectWeakMap* registrations = group->registrations()) {
    registrations->trace(trc);
  }
}

/* static */
void FinalizationGroupObject::finalize(JSFreeOp* fop, JSObject* obj) {
  auto group = &obj->as<FinalizationGroupObject>();
  fop->delete_(obj, group->holdingsToBeCleanedUp(),
               MemoryUse::FinalizationGroupHoldingsVector);
  fop->delete_(obj, group->registrations(),
               MemoryUse::FinalizationGroupRegistrations);
}

inline JSObject* FinalizationGroupObject::cleanupCallback() const {
  Value value = getReservedSlot(CleanupCallbackSlot);
  if (value.isUndefined()) {
    return nullptr;
  }
  return &value.toObject();
}

ObjectWeakMap* FinalizationGroupObject::registrations() const {
  Value value = getReservedSlot(RegistrationsSlot);
  if (value.isUndefined()) {
    return nullptr;
  }
  return static_cast<ObjectWeakMap*>(value.toPrivate());
}

FinalizationGroupObject::HoldingsVector*
FinalizationGroupObject::holdingsToBeCleanedUp() const {
  Value value = getReservedSlot(HoldingsToBeCleanedUpSlot);
  if (value.isUndefined()) {
    return nullptr;
  }
  return static_cast<HoldingsVector*>(value.toPrivate());
}

bool FinalizationGroupObject::isQueuedForCleanup() const {
  return getReservedSlot(IsQueuedForCleanupSlot).toBoolean();
}

bool FinalizationGroupObject::isCleanupJobActive() const {
  return getReservedSlot(IsCleanupJobActiveSlot).toBoolean();
}

void FinalizationGroupObject::queueHoldingsToBeCleanedUp(
    const Value& holdings) {
  AutoEnterOOMUnsafeRegion oomUnsafe;
  if (!holdingsToBeCleanedUp()->append(holdings)) {
    oomUnsafe.crash("FinalizationGroupObject::queueHoldingsToBeCleanedUp");
  }
}

void FinalizationGroupObject::setQueuedForCleanup(bool value) {
  MOZ_ASSERT(value != isQueuedForCleanup());
  setReservedSlot(IsQueuedForCleanupSlot, BooleanValue(value));
}

void FinalizationGroupObject::setCleanupJobActive(bool value) {
  MOZ_ASSERT(value != isCleanupJobActive());
  setReservedSlot(IsCleanupJobActiveSlot, BooleanValue(value));
}

// FinalizationGroup.prototype.register(target , holdings [, unregisterToken ])
// https://tc39.es/proposal-weakrefs/#sec-finalization-group.prototype.register
/* static */
bool FinalizationGroupObject::register_(JSContext* cx, unsigned argc,
                                        Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // 1. Let finalizationGroup be the this value.
  // 2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  // 3. If finalizationGroup does not have a [[Cells]] internal slot, throw a
  // TypeError exception.
  if (!args.thisv().isObject() ||
      !args.thisv().toObject().is<FinalizationGroupObject>()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_A_FINALIZATION_GROUP,
                              "Receiver of FinalizationGroup.register call");
    return false;
  }

  RootedFinalizationGroupObject group(
      cx, &args.thisv().toObject().as<FinalizationGroupObject>());

  // 4. If Type(target) is not Object, throw a TypeError exception.
  if (!args.get(0).isObject()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_OBJECT_REQUIRED,
                              "target argument to FinalizationGroup.register");
    return false;
  }

  RootedObject target(cx, &args[0].toObject());

  // 5. If SameValue(target, holdings), throw a TypeError exception.
  if (args.get(1).isObject() && &args.get(1).toObject() == target) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_BAD_HOLDINGS);
    return false;
  }

  RootedValue holdings(cx, args.get(1));

  // 6. If Type(unregisterToken) is not Object,
  //    a. If unregisterToken is not undefined, throw a TypeError exception.
  if (!args.get(2).isUndefined() && !args.get(2).isObject()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BAD_UNREGISTER_TOKEN,
                              "FinalizationGroup.register");
    return false;
  }

  RootedObject unregisterToken(cx);
  if (!args.get(2).isUndefined()) {
    unregisterToken = &args[2].toObject();
  }

  // Create the finalization record representing this target and holdings.
  Rooted<FinalizationRecordObject*> record(
      cx, FinalizationRecordObject::create(cx, group, holdings));
  if (!record) {
    return false;
  }

  if (unregisterToken && !addRegistration(cx, group, unregisterToken, record)) {
    return false;
  }

  auto guard = mozilla::MakeScopeExit([&] {
    if (unregisterToken) {
      removeRegistrationOnError(group, unregisterToken, record);
    }
  });

  // Fully unwrap the target to pass it to the GC.
  RootedObject unwrappedTarget(cx);
  unwrappedTarget = CheckedUnwrapDynamic(target, cx);
  if (!unwrappedTarget) {
    ReportAccessDenied(cx);
    return false;
  }

  // Wrap the record into the compartment of the target.
  RootedObject wrappedRecord(cx, record);
  AutoRealm ar(cx, unwrappedTarget);
  if (!JS_WrapObject(cx, &wrappedRecord)) {
    return false;
  }

  // Register the record with the target.
  gc::GCRuntime* gc = &cx->runtime()->gc;
  if (!gc->registerWithFinalizationGroup(cx, unwrappedTarget, wrappedRecord)) {
    return false;
  }

  guard.release();
  args.rval().setUndefined();
  return true;
}

/* static */
bool FinalizationGroupObject::addRegistration(
    JSContext* cx, HandleFinalizationGroupObject group,
    HandleObject unregisterToken, HandleFinalizationRecordObject record) {
  // Add the record to the list of records associated with this unregister
  // token.

  MOZ_ASSERT(unregisterToken);
  MOZ_ASSERT(group->registrations());

  auto& map = *group->registrations();
  Rooted<FinalizationRecordVectorObject*> recordsObject(cx);
  JSObject* obj = map.lookup(unregisterToken);
  if (obj) {
    recordsObject = &obj->as<FinalizationRecordVectorObject>();
  } else {
    recordsObject = FinalizationRecordVectorObject::create(cx);
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

/* static */ void FinalizationGroupObject::removeRegistrationOnError(
    HandleFinalizationGroupObject group, HandleObject unregisterToken,
    HandleFinalizationRecordObject record) {
  // Remove a registration if something went wrong before we added it to the
  // target zone's map. Note that this can't remove a registration after that
  // point.

  MOZ_ASSERT(unregisterToken);
  MOZ_ASSERT(group->registrations());
  JS::AutoAssertNoGC nogc;

  auto& map = *group->registrations();
  JSObject* obj = map.lookup(unregisterToken);
  MOZ_ASSERT(obj);
  auto records = &obj->as<FinalizationRecordVectorObject>();
  records->remove(record);

  if (records->empty()) {
    map.remove(unregisterToken);
  }
}

// FinalizationGroup.prototype.unregister ( unregisterToken )
// https://tc39.es/proposal-weakrefs/#sec-finalization-group.prototype.unregister
/* static */
bool FinalizationGroupObject::unregister(JSContext* cx, unsigned argc,
                                         Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // 1. Let finalizationGroup be the this value.
  // 2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  // 3. If finalizationGroup does not have a [[Cells]] internal slot, throw a
  //    TypeError exception.
  if (!args.thisv().isObject() ||
      !args.thisv().toObject().is<FinalizationGroupObject>()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_A_FINALIZATION_GROUP,
                              "Receiver of FinalizationGroup.unregister call");
    return false;
  }

  RootedFinalizationGroupObject group(
      cx, &args.thisv().toObject().as<FinalizationGroupObject>());

  // 4. If Type(unregisterToken) is not Object, throw a TypeError exception.
  if (!args.get(0).isObject()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BAD_UNREGISTER_TOKEN,
                              "FinalizationGroup.unregister");
    return false;
  }

  RootedObject unregisterToken(cx, &args[0].toObject());

  RootedObject obj(cx, group->registrations()->lookup(unregisterToken));
  if (obj) {
    auto& records = obj->as<FinalizationRecordVectorObject>().records();
    MOZ_ASSERT(!records.empty());
    for (FinalizationRecordObject* record : records) {
      // Clear the fields of this record; it will be removed from the target's
      // list when it is next swept.
      record->clear();
    }
    group->registrations()->remove(unregisterToken);
  }

  args.rval().setBoolean(bool(obj));
  return true;
}

// FinalizationGroup.prototype.cleanupSome ( [ callback ] )
// https://tc39.es/proposal-weakrefs/#sec-finalization-group.prototype.cleanupSome
bool FinalizationGroupObject::cleanupSome(JSContext* cx, unsigned argc,
                                          Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // 1. Let finalizationGroup be the this value.
  // 2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  // 3. If finalizationGroup does not have [[Cells]] and
  //    [[IsFinalizationGroupCleanupJobActive]] internal slots, throw a
  //    TypeError exception.
  if (!args.thisv().isObject() ||
      !args.thisv().toObject().is<FinalizationGroupObject>()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_A_FINALIZATION_GROUP,
                              "Receiver of FinalizationGroup.cleanupSome call");
    return false;
  }

  // 4. If finalizationGroup.[[IsFinalizationGroupCleanupJobActive]] is true,
  //    throw a TypeError exception.
  RootedFinalizationGroupObject group(
      cx, &args.thisv().toObject().as<FinalizationGroupObject>());
  if (group->isCleanupJobActive()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_BAD_CLEANUP_STATE);
    return false;
  }

  // 5. If callback is not undefined and IsCallable(callback) is false, throw a
  //    TypeError exception.
  RootedObject cleanupCallback(cx);
  if (!args.get(0).isUndefined()) {
    cleanupCallback = ValueToCallable(cx, args.get(0), -1, NO_CONSTRUCT);
    if (!cleanupCallback) {
      return false;
    }
  }

  if (!cleanupQueuedHoldings(cx, group, cleanupCallback)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

// CleanupFinalizationGroup ( finalizationGroup [ , callback ] )
// https://tc39.es/proposal-weakrefs/#sec-cleanup-finalization-group
/* static */
bool FinalizationGroupObject::cleanupQueuedHoldings(
    JSContext* cx, HandleFinalizationGroupObject group,
    HandleObject callbackArg) {
  MOZ_ASSERT(cx->compartment() == group->compartment());

  // 2. If CheckForEmptyCells(finalizationGroup) is false, return.
  HoldingsVector* holdings = group->holdingsToBeCleanedUp();
  size_t initialLength = holdings->length();
  if (initialLength == 0) {
    return true;
  }

  // 3. Let iterator be
  //    !CreateFinalizationGroupCleanupIterator(finalizationGroup).
  Rooted<FinalizationIteratorObject*> iterator(
      cx, FinalizationIteratorObject::create(cx, group));
  if (!iterator) {
    return false;
  }

  // 4. If callback is undefined, set callback to
  //    finalizationGroup.[[CleanupCallback]].
  RootedObject callback(cx, callbackArg);
  if (!callbackArg) {
    callback = group->cleanupCallback();
  }

  // 5. Set finalizationGroup.[[IsFinalizationGroupCleanupJobActive]] to true.
  group->setCleanupJobActive(true);

  // 6. Let result be Call(callback, undefined, iterator).
  RootedValue rval(cx);
  JS::AutoValueArray<1> args(cx);
  args[0].setObject(*iterator);
  bool ok = JS::Call(cx, UndefinedHandleValue, callback, args, &rval);

  // Remove holdings that were iterated over.
  size_t index = iterator->index();
  MOZ_ASSERT(index <= initialLength);
  MOZ_ASSERT(initialLength <= holdings->length());
  if (index > 0) {
    holdings->erase(holdings->begin(), holdings->begin() + index);
  }

  // 7. Set finalizationGroup.[[IsFinalizationGroupCleanupJobActive]] to false.
  group->setCleanupJobActive(false);

  // 8. Set iterator.[[FinalizationGroup]] to empty.
  iterator->clearFinalizationGroup();

  return ok;
}

///////////////////////////////////////////////////////////////////////////
// FinalizationIteratorObject

const JSClass FinalizationIteratorObject::class_ = {
    "FinalizationGroupCleanupIterator", JSCLASS_HAS_RESERVED_SLOTS(SlotCount),
    JS_NULL_CLASS_OPS, JS_NULL_CLASS_SPEC};

const JSFunctionSpec FinalizationIteratorObject::methods_[] = {
    JS_FN(js_next_str, next, 0, 0), JS_FS_END};

const JSPropertySpec FinalizationIteratorObject::properties_[] = {
    JS_STRING_SYM_PS(toStringTag, "FinalizationGroup Cleanup Iterator",
                     JSPROP_READONLY),
    JS_PS_END};

/* static */
bool GlobalObject::initFinalizationIteratorProto(JSContext* cx,
                                                 Handle<GlobalObject*> global) {
  Rooted<JSObject*> base(
      cx, GlobalObject::getOrCreateIteratorPrototype(cx, global));
  if (!base) {
    return false;
  }
  RootedPlainObject proto(cx, NewObjectWithGivenProto<PlainObject>(cx, base));
  if (!proto) {
    return false;
  }
  if (!JS_DefineFunctions(cx, proto, FinalizationIteratorObject::methods_) ||
      !JS_DefineProperties(cx, proto,
                           FinalizationIteratorObject::properties_)) {
    return false;
  }
  global->setReservedSlot(FINALIZATION_ITERATOR_PROTO, ObjectValue(*proto));
  return true;
}

/* static */ FinalizationIteratorObject* FinalizationIteratorObject::create(
    JSContext* cx, HandleFinalizationGroupObject group) {
  MOZ_ASSERT(group);

  RootedObject proto(cx, GlobalObject::getOrCreateFinalizationIteratorPrototype(
                             cx, cx->global()));
  if (!proto) {
    return nullptr;
  }

  FinalizationIteratorObject* iterator =
      NewObjectWithClassProto<FinalizationIteratorObject>(cx, proto);
  if (!iterator) {
    return nullptr;
  }

  iterator->initReservedSlot(FinalizationGroupSlot, ObjectValue(*group));
  iterator->initReservedSlot(IndexSlot, Int32Value(0));

  return iterator;
}

FinalizationGroupObject* FinalizationIteratorObject::finalizationGroup() const {
  Value value = getReservedSlot(FinalizationGroupSlot);
  if (value.isUndefined()) {
    return nullptr;
  }
  return &value.toObject().as<FinalizationGroupObject>();
}

size_t FinalizationIteratorObject::index() const {
  int32_t i = getReservedSlot(IndexSlot).toInt32();
  MOZ_ASSERT(i >= 0);
  return size_t(i);
}

void FinalizationIteratorObject::incIndex() {
  int32_t i = index();
  MOZ_ASSERT(i < INT32_MAX);
  setReservedSlot(IndexSlot, Int32Value(i + 1));
}

void FinalizationIteratorObject::clearFinalizationGroup() {
  MOZ_ASSERT(finalizationGroup());
  setReservedSlot(FinalizationGroupSlot, UndefinedValue());
}

// %FinalizationGroupCleanupIteratorPrototype%.next()
// https://tc39.es/proposal-weakrefs/#sec-%finalizationgroupcleanupiterator%.next
/* static */
bool FinalizationIteratorObject::next(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // 1. Let iterator be the this value.
  // 2. If Type(iterator) is not Object, throw a TypeError exception.
  // 3. If iterator does not have a [[FinalizationGroup]] internal slot, throw a
  //    TypeError exception.
  if (!args.thisv().isObject() ||
      !args.thisv().toObject().is<FinalizationIteratorObject>()) {
    JS_ReportErrorNumberASCII(
        cx, GetErrorMessage, nullptr, JSMSG_NOT_A_FINALIZATION_ITERATOR,
        "Receiver of FinalizationGroupCleanupIterator.next call");
    return false;
  }

  RootedFinalizationIteratorObject iterator(
      cx, &args.thisv().toObject().as<FinalizationIteratorObject>());

  // 4. If iterator.[[FinalizationGroup]] is empty, throw a TypeError exception.
  // 5. Let finalizationGroup be iterator.[[FinalizationGroup]].
  RootedFinalizationGroupObject group(cx, iterator->finalizationGroup());
  if (!group) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_STALE_FINALIZATION_GROUP_ITERATOR);
    return false;
  }

  // 8. If finalizationGroup.[[Cells]] contains a Record cell such that
  //    cell.[[Target]] is empty,
  //    a. Choose any such cell.
  //    b. Remove cell from finalizationGroup.[[Cells]].
  //    c. Return CreateIterResultObject(cell.[[Holdings]], false).
  auto* holdings = group->holdingsToBeCleanedUp();
  size_t index = iterator->index();
  MOZ_ASSERT(index <= holdings->length());
  if (index < holdings->length() && index < INT32_MAX) {
    RootedValue value(cx, (*holdings)[index]);
    JSObject* result = CreateIterResultObject(cx, value, false);
    if (!result) {
      return false;
    }

    iterator->incIndex();

    args.rval().setObject(*result);
    return true;
  }

  // 9. Otherwise, return CreateIterResultObject(undefined, true).
  JSObject* result = CreateIterResultObject(cx, UndefinedHandleValue, true);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

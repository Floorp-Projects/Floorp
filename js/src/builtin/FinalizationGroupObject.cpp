/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implementation of JS FinalizationGroup objects.

#include "builtin/FinalizationGroupObject.h"

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

const JSFunctionSpec FinalizationGroupObject::methods_[] = {JS_FS_END};

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

  if (!proto) {
    proto =
        GlobalObject::getOrCreateFinalizationGroupPrototype(cx, cx->global());
    if (!proto) {
      return false;
    }
  }

  FinalizationGroupObject* group =
      NewObjectWithClassProto<FinalizationGroupObject>(cx, proto);
  if (!group) {
    return false;
  }

  auto registrations = cx->make_unique<ObjectWeakMap>(cx);
  if (!registrations) {
    return false;
  }

  auto holdings = cx->make_unique<HoldingsVector>(cx->zone());
  if (!holdings) {
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
  group->holdingsToBeCleanedUp()->trace(trc);
  group->registrations()->trace(trc);
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
  return static_cast<ObjectWeakMap*>(
      getReservedSlot(RegistrationsSlot).toPrivate());
}

FinalizationGroupObject::HoldingsVector*
FinalizationGroupObject::holdingsToBeCleanedUp() const {
  return static_cast<HoldingsVector*>(
      getReservedSlot(HoldingsToBeCleanedUpSlot).toPrivate());
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
bool FinalizationIteratorObject::next(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setUndefined();
  return true;
}

size_t FinalizationIteratorObject::index() const {
  int32_t i = getReservedSlot(IndexSlot).toInt32();
  MOZ_ASSERT(i >= 0);
  return size_t(i);
}

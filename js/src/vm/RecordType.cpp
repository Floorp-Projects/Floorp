/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/RecordType.h"

#include "mozilla/Assertions.h"

#include "jsapi.h"

#include "gc/Nursery.h"
#include "gc/Rooting.h"
#include "js/Array.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "util/StringBuffer.h"
#include "vm/ArrayObject.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/ObjectFlags.h"
#include "vm/PropertyInfo.h"
#include "vm/PropMap.h"
#include "vm/StringType.h"
#include "vm/ToSource.h"
#include "vm/TupleType.h"

#include "vm/JSAtom-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/Shape-inl.h"

using namespace js;

static bool RecordConstructor(JSContext* cx, unsigned argc, Value* vp);

const JSClass RecordType::class_ = {"record",
                                    JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT),
                                    JS_NULL_CLASS_OPS, &RecordType::classSpec_};

const ClassSpec RecordType::classSpec_ = {
    GenericCreateConstructor<RecordConstructor, 1, gc::AllocKind::FUNCTION>,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr};

RecordType* RecordType::createUninitialized(JSContext* cx,
                                            uint32_t initialLength) {
  RootedShape shape(
      cx, SharedShape::getInitialShape(cx, &RecordType::class_, cx->realm(),
                                       TaggedProto(nullptr), SLOT_COUNT));
  if (!shape) {
    return nullptr;
  }

  JSObject* obj = js::AllocateObject(cx, NewObjectGCKind(), initialLength,
                                     gc::DefaultHeap, &RecordType::class_);
  if (!obj) {
    return nullptr;
  }
  obj->initShape(shape);

  Rooted<RecordType*> rec(cx, static_cast<RecordType*>(obj));
  rec->setEmptyElements();
  rec->initEmptyDynamicSlots();
  rec->initFixedSlots(SLOT_COUNT);

  RootedArrayObject sortedKeys(cx,
                               NewDensePartlyAllocatedArray(cx, initialLength));
  if (!sortedKeys->ensureElements(cx, initialLength)) {
    return nullptr;
  }

  rec->initFixedSlot(INITIALIZED_LENGTH_SLOT, PrivateUint32Value(0));
  rec->initFixedSlot(SORTED_KEYS_SLOT, ObjectValue(*sortedKeys));

  return rec;
}

bool RecordType::initializeNextProperty(JSContext* cx, HandleId key,
                                        HandleValue value) {
  if (key.isSymbol()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_RECORD_NO_SYMBOL_KEY);
    return false;
  }

  if (!value.isPrimitive()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_RECORD_TUPLE_NO_OBJECT);
    return false;
  }

  mozilla::Maybe<PropertyInfo> prop = lookupPure(key);

  if (prop.isSome()) {
    MOZ_ASSERT(prop.value().hasSlot());
    setSlot(prop.value().slot(), value);
    return true;
  }

  constexpr PropertyFlags propFlags = {PropertyFlag::Enumerable};
  Rooted<NativeObject*> target(cx, this);
  uint32_t slot;
  if (!NativeObject::addProperty(cx, target, key, propFlags, &slot)) {
    return false;
  }
  initSlot(slot, value);

  // Add the key to the SORTED_KEYS instenal slot

  JSString* keyStr = IdToString(cx, key);
  if (!keyStr) {
    return false;
  }
  JSLinearString* keyLinearStr = keyStr->ensureLinear(cx);
  if (!keyLinearStr) {
    return false;
  }

  uint32_t initializedLength =
      getFixedSlot(INITIALIZED_LENGTH_SLOT).toPrivateUint32();
  ArrayObject* sortedKeys =
      &getFixedSlot(SORTED_KEYS_SLOT).toObject().as<ArrayObject>();
  MOZ_ASSERT(initializedLength < sortedKeys->length());

  if (!sortedKeys->ensureElements(cx, initializedLength + 1)) {
    return false;
  }
  sortedKeys->setDenseInitializedLength(initializedLength + 1);
  sortedKeys->initDenseElement(initializedLength, StringValue(keyLinearStr));

  setFixedSlot(INITIALIZED_LENGTH_SLOT,
               PrivateUint32Value(initializedLength + 1));

  return true;
}

bool RecordType::finishInitialization(JSContext* cx) {
  RootedNativeObject obj(cx, this);
  if (!JSObject::setFlag(cx, obj, ObjectFlag::NotExtensible)) {
    return false;
  }
  if (!ObjectElements::FreezeOrSeal(cx, obj, IntegrityLevel::Frozen)) {
    return false;
  }
  if (getFixedSlot(INITIALIZED_LENGTH_SLOT).toPrivateUint32() == 0) {
    return true;
  }

  ArrayObject& sortedKeys =
      getFixedSlot(SORTED_KEYS_SLOT).toObject().as<ArrayObject>();

  Rooted<JSLinearString*> tmpKey(cx);

  // Sort the keys. This is insertion sort - O(n^2) - but it's ok for now
  // becase records are probably not too big anyway.
  for (uint32_t i = 1, j; i < sortedKeys.length(); i++) {
#define KEY(index) sortedKeys.getDenseElement(index)
#define KEY_S(index) &KEY(index).toString()->asLinear()

    MOZ_ASSERT(KEY(i).isString());
    MOZ_ASSERT(KEY(i).toString()->isLinear());

    tmpKey = KEY_S(i);

    for (j = i; j > 0 && CompareStrings(KEY_S(j - 1), tmpKey) > 0; j--) {
      sortedKeys.setDenseElement(j, KEY(j - 1));
    }

    sortedKeys.setDenseElement(j, StringValue(tmpKey));

#undef KEY
#undef KEY_S
  }

  return true;
}

bool RecordType::sameValueZero(JSContext* cx, RecordType* lhs, RecordType* rhs,
                               bool* equal) {
  MOZ_CRASH("Unsupported");
  return false;
}

bool RecordType::sameValue(JSContext* cx, RecordType* lhs, RecordType* rhs,
                           bool* equal) {
  MOZ_CRASH("Unsupported");
  return false;
}

// Record and Record proposal section 9.2.1
static bool RecordConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (args.isConstructing()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_CONSTRUCTOR, "Record");
    return false;
  }
  // Step 2.
  RootedObject obj(cx, ToObject(cx, args.get(0)));
  if (!obj) {
    return false;
  }

  // Step 3.
  RootedIdVector keys(cx);
  if (!GetPropertyKeys(cx, obj, JSITER_OWNONLY, &keys)) {
    return false;
  }

  size_t len = keys.length();

  Rooted<RecordType*> rec(cx, RecordType::createUninitialized(cx, len));

  if (!rec) {
    return false;
  }

  RootedId propKey(cx);
  RootedValue propValue(cx);
  for (size_t i = 0; i < len; i++) {
    propKey.set(keys[i]);
    MOZ_ASSERT(!propKey.isSymbol(), "symbols are filtered out at step 3");

    // Step 4.c.ii.1.
    if (MOZ_UNLIKELY(!GetProperty(cx, obj, obj, propKey, &propValue))) {
      return false;
    }

    if (MOZ_UNLIKELY(!rec->initializeNextProperty(cx, propKey, propValue))) {
      return false;
    }
  }

  if (MOZ_UNLIKELY(!rec->finishInitialization(cx))) {
    return false;
  }

  args.rval().setExtendedPrimitive(*rec);
  return true;
}

JSString* js::RecordToSource(JSContext* cx, RecordType* rec) {
  JSStringBuilder sb(cx);

  if (!sb.append("#{")) {
    return nullptr;
  }

  ArrayObject& sortedKeys = rec->getFixedSlot(RecordType::SORTED_KEYS_SLOT)
                                .toObject()
                                .as<ArrayObject>();

  uint32_t length = sortedKeys.length();

  Rooted<RecordType*> rootedRec(cx, rec);
  RootedValue recValue(cx, ExtendedPrimitiveValue(*rec));
  RootedValue value(cx);
  RootedString keyStr(cx);
  RootedId key(cx);
  JSString* str;
  for (uint32_t index = 0; index < length; index++) {
    value.set(sortedKeys.getDenseElement(index));
    MOZ_ASSERT(value.isString());

    str = ValueToSource(cx, value);
    if (!str) {
      return nullptr;
    }
    if (!sb.append(str)) {
      return nullptr;
    }

    if (!sb.append(": ")) {
      return nullptr;
    }

    keyStr.set(value.toString());
    if (!JS_StringToId(cx, keyStr, &key)) {
      return nullptr;
    }

    if (!GetProperty(cx, rootedRec, recValue, key, &value)) {
      return nullptr;
    }

    str = ValueToSource(cx, value);
    if (!str) {
      return nullptr;
    }
    if (!sb.append(str)) {
      return nullptr;
    }

    if (index + 1 != length) {
      if (!sb.append(", ")) {
        return nullptr;
      }
    }
  }

  /* Finalize the buffer. */
  if (!sb.append('}')) {
    return nullptr;
  }

  return sb.finishString();
}

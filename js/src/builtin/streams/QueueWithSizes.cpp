/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Queue-with-sizes operations. */

#include "builtin/streams/QueueWithSizes.h"

#include "mozilla/Assertions.h"     // MOZ_ASSERT
#include "mozilla/Attributes.h"     // MOZ_MUST_USE
#include "mozilla/FloatingPoint.h"  // mozilla::Is{Infinite,NaN}

#include "jsapi.h"  // JS_ReportErrorNumberASCII

#include "builtin/streams/ReadableStreamController.h"  // js::ReadableStreamController
#include "js/Class.h"         // JSClass, JSCLASS_HAS_RESERVED_SLOTS
#include "js/Conversions.h"   // JS::ToNumber
#include "js/RootingAPI.h"    // JS::Rooted
#include "js/Value.h"         // JS::Value, JS::{Number,Object}Value
#include "vm/Compartment.h"   // JSCompartment
#include "vm/JSContext.h"     // JSContext
#include "vm/List.h"          // js::ListObject
#include "vm/NativeObject.h"  // js::NativeObject

#include "vm/Compartment-inl.h"  // JSCompartment::wrap
#include "vm/JSContext-inl.h"    // JSContext::check
#include "vm/JSObject-inl.h"     // js::NewBuiltinClassInstance
#include "vm/List-inl.h"   // js::ListObject::*, js::StoreNewListInFixedSlot
#include "vm/Realm-inl.h"  // js::AutoRealm

using JS::Handle;
using JS::MutableHandle;
using JS::NumberValue;
using JS::ObjectValue;
using JS::Rooted;
using JS::ToNumber;
using JS::Value;

class QueueEntry : public js::NativeObject {
 private:
  enum Slots { Slot_Value = 0, Slot_Size, SlotCount };

 public:
  static const JSClass class_;

  Value value() { return getFixedSlot(Slot_Value); }
  double size() { return getFixedSlot(Slot_Size).toNumber(); }

  static QueueEntry* create(JSContext* cx, Handle<Value> value, double size) {
    Rooted<QueueEntry*> entry(cx, js::NewBuiltinClassInstance<QueueEntry>(cx));
    if (!entry) {
      return nullptr;
    }

    entry->setFixedSlot(Slot_Value, value);
    entry->setFixedSlot(Slot_Size, NumberValue(size));

    return entry;
  }
};

const JSClass QueueEntry::class_ = {"QueueEntry",
                                    JSCLASS_HAS_RESERVED_SLOTS(SlotCount)};

/*** 6.2. Queue-with-sizes operations ***************************************/

/**
 * Streams spec, 6.2.1. DequeueValue ( container ) nothrow
 */
MOZ_MUST_USE bool js::DequeueValue(
    JSContext* cx, Handle<ReadableStreamController*> unwrappedContainer,
    MutableHandle<Value> chunk) {
  // Step 1: Assert: container has [[queue]] and [[queueTotalSize]] internal
  //         slots (implicit).
  // Step 2: Assert: queue is not empty.
  Rooted<ListObject*> unwrappedQueue(cx, unwrappedContainer->queue());
  MOZ_ASSERT(unwrappedQueue->length() > 0);

  // Step 3. Let pair be the first element of queue.
  // Step 4. Remove pair from queue, shifting all other elements downward
  //         (so that the second becomes the first, and so on).
  Rooted<QueueEntry*> unwrappedPair(
      cx, &unwrappedQueue->popFirstAs<QueueEntry>(cx));
  MOZ_ASSERT(unwrappedPair);

  // Step 5: Set container.[[queueTotalSize]] to
  //         container.[[queueTotalSize]] − pair.[[size]].
  // Step 6: If container.[[queueTotalSize]] < 0, set
  //         container.[[queueTotalSize]] to 0.
  //         (This can occur due to rounding errors.)
  double totalSize = unwrappedContainer->queueTotalSize();
  totalSize -= unwrappedPair->size();
  if (totalSize < 0) {
    totalSize = 0;
  }
  unwrappedContainer->setQueueTotalSize(totalSize);

  // Step 7: Return pair.[[value]].
  Rooted<Value> val(cx, unwrappedPair->value());
  if (!cx->compartment()->wrap(cx, &val)) {
    return false;
  }

  chunk.set(val);
  return true;
}

/**
 * Streams spec, 6.2.2. EnqueueValueWithSize ( container, value, size ) throws
 */
MOZ_MUST_USE bool js::EnqueueValueWithSize(
    JSContext* cx, Handle<ReadableStreamController*> unwrappedContainer,
    Handle<Value> value, Handle<Value> sizeVal) {
  cx->check(value, sizeVal);

  // Step 1: Assert: container has [[queue]] and [[queueTotalSize]] internal
  //         slots (implicit).
  // Step 2: Let size be ? ToNumber(size).
  double size;
  if (!ToNumber(cx, sizeVal, &size)) {
    return false;
  }

  // Step 3: If ! IsFiniteNonNegativeNumber(size) is false, throw a RangeError
  //         exception.
  if (size < 0 || mozilla::IsNaN(size) || mozilla::IsInfinite(size)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NUMBER_MUST_BE_FINITE_NON_NEGATIVE, "size");
    return false;
  }

  // Step 4: Append Record {[[value]]: value, [[size]]: size} as the last
  //         element of container.[[queue]].
  {
    AutoRealm ar(cx, unwrappedContainer);
    Rooted<ListObject*> queue(cx, unwrappedContainer->queue());
    Rooted<Value> wrappedVal(cx, value);
    if (!cx->compartment()->wrap(cx, &wrappedVal)) {
      return false;
    }

    QueueEntry* entry = QueueEntry::create(cx, wrappedVal, size);
    if (!entry) {
      return false;
    }
    Rooted<Value> val(cx, ObjectValue(*entry));
    if (!queue->append(cx, val)) {
      return false;
    }
  }

  // Step 5: Set container.[[queueTotalSize]] to
  //         container.[[queueTotalSize]] + size.
  unwrappedContainer->setQueueTotalSize(unwrappedContainer->queueTotalSize() +
                                        size);

  return true;
}

/**
 * Streams spec, 6.2.4. ResetQueue ( container ) nothrow
 */
MOZ_MUST_USE bool js::ResetQueue(
    JSContext* cx, Handle<ReadableStreamController*> unwrappedContainer) {
  // Step 1: Assert: container has [[queue]] and [[queueTotalSize]] internal
  //         slots (implicit).
  // Step 2: Set container.[[queue]] to a new empty List.
  if (!StoreNewListInFixedSlot(cx, unwrappedContainer,
                               StreamController::Slot_Queue)) {
    return false;
  }

  // Step 3: Set container.[[queueTotalSize]] to 0.
  unwrappedContainer->setQueueTotalSize(0);

  return true;
}

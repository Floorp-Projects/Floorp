/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/TupleType.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/HashFunctions.h"

#include "jsapi.h"

#include "builtin/TupleObject.h"
#include "gc/Allocator.h"
#include "gc/AllocKind.h"
#include "gc/Nursery.h"
#include "gc/Tracer.h"
#include "js/TypeDecls.h"
#include "util/StringBuffer.h"
#include "vm/EqualityOperations.h"
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"
#include "vm/RecordType.h"
#include "vm/SelfHosting.h"
#include "vm/ToSource.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

TupleType* TupleType::create(JSContext* cx, uint32_t length,
                             const Value* elements) {
  for (uint32_t index = 0; index < length; index++) {
    if (!elements[index].isPrimitive()) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_RECORD_TUPLE_NO_OBJECT);
      return nullptr;
    }
  }

  TupleType* tup = TupleType::createUninitialized(cx, length);
  if (!tup) {
    return nullptr;
  }

  tup->initDenseElements(elements, length);
  tup->finishInitialization(cx);

  return tup;
}

TupleType* TupleType::createUninitialized(JSContext* cx, uint32_t length) {
  RootedShape shape(
      cx, SharedShape::getInitialShape(
              cx, &TupleType::class_, cx->realm(), TaggedProto(nullptr),
              // tuples don't have slots, but only integer-indexed elements.
              /* nfixed = */ 0));
  if (!shape) {
    return nullptr;
  }

  gc::AllocKind allocKind = GuessArrayGCKind(length);

  JSObject* obj =
      js::AllocateObject(cx, allocKind, 0, gc::DefaultHeap, &TupleType::class_);
  if (!obj) {
    return nullptr;
  }

  TupleType* tup = static_cast<TupleType*>(obj);

  tup->initShape(shape);
  tup->initEmptyDynamicSlots();

  uint32_t capacity =
      gc::GetGCKindSlots(allocKind) - ObjectElements::VALUES_PER_HEADER;

  tup->setFixedElements(0);
  new (tup->getElementsHeader()) ObjectElements(capacity, length);

  if (!tup->ensureElements(cx, length)) {
    return nullptr;
  }

  return tup;
}

bool TupleType::initializeNextElement(JSContext* cx, HandleValue elt) {
  if (!elt.isPrimitive()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_RECORD_TUPLE_NO_OBJECT);
    return false;
  }

  uint32_t length = getDenseInitializedLength();

  if (!ensureElements(cx, length + 1)) {
    return false;
  }
  setDenseInitializedLength(length + 1);
  initDenseElement(length, elt);

  return true;
}

void TupleType::finishInitialization(JSContext* cx) {
  shrinkCapacityToInitializedLength(cx);

  ObjectElements* header = getElementsHeader();
  header->length = header->initializedLength;
  header->setNotExtensible();
  header->seal();
  header->freeze();
}

bool TupleType::getOwnProperty(HandleId id, MutableHandleValue vp) const {
  if (!id.isInt()) {
    return false;
  }

  int32_t index = id.toInt();
  if (index < 0 || uint32_t(index) >= length()) {
    return false;
  }

  vp.set(getDenseElement(index));
  return true;
}

js::HashNumber TupleType::hash(const TupleType::ElementHasher& hasher) const {
  MOZ_ASSERT(isAtomized());

  js::HashNumber h = mozilla::HashGeneric(length());
  for (uint32_t i = 0; i < length(); i++) {
    h = mozilla::AddToHash(h, hasher(getDenseElement(i)));
  }
  return h;
}

bool TupleType::ensureAtomized(JSContext* cx) {
  if (isAtomized()) {
    return true;
  }

  RootedValue child(cx);
  bool changed;

  for (uint32_t i = 0; i < length(); i++) {
    child.set(getDenseElement(i));
    if (!EnsureAtomized(cx, &child, &changed)) {
      return false;
    }
    if (changed) {
      // We cannot use setDenseElement(), because this object is frozen.
      elements_[i].set(this, HeapSlot::Element, unshiftedIndex(i), child);
    }
  }

  getElementsHeader()->setTupleIsAtomized();

  return true;
}

bool TupleType::sameValueZero(JSContext* cx, TupleType* lhs, TupleType* rhs,
                              bool* equal) {
  return sameValueWith<SameValueZero>(cx, lhs, rhs, equal);
}

bool TupleType::sameValue(JSContext* cx, TupleType* lhs, TupleType* rhs,
                          bool* equal) {
  return sameValueWith<SameValue>(cx, lhs, rhs, equal);
}

bool TupleType::sameValueZero(TupleType* lhs, TupleType* rhs) {
  MOZ_ASSERT(lhs->isAtomized());
  MOZ_ASSERT(rhs->isAtomized());

  if (lhs == rhs) {
    return true;
  }
  if (lhs->length() != rhs->length()) {
    return false;
  }

  Value v1, v2;

  for (uint32_t index = 0; index < lhs->length(); index++) {
    v1 = lhs->getDenseElement(index);
    v2 = rhs->getDenseElement(index);

    if (!js::SameValueZeroLinear(v1, v2)) {
      return false;
    }
  }

  return true;
}

template <bool Comparator(JSContext*, HandleValue, HandleValue, bool*)>
bool TupleType::sameValueWith(JSContext* cx, TupleType* lhs, TupleType* rhs,
                              bool* equal) {
  MOZ_ASSERT(lhs->getElementsHeader()->isFrozen());
  MOZ_ASSERT(rhs->getElementsHeader()->isFrozen());

  if (lhs == rhs) {
    *equal = true;
    return true;
  }

  if (lhs->length() != rhs->length()) {
    *equal = false;
    return true;
  }

  *equal = true;

  RootedValue v1(cx);
  RootedValue v2(cx);

  for (uint32_t index = 0; index < lhs->length(); index++) {
    v1.set(lhs->getDenseElement(index));
    v2.set(rhs->getDenseElement(index));

    if (!Comparator(cx, v1, v2, equal)) {
      return false;
    }

    if (!*equal) {
      return true;
    }
  }

  return true;
}

JSString* js::TupleToSource(JSContext* cx, TupleType* tup) {
  JSStringBuilder sb(cx);

  if (!sb.append("#[")) {
    return nullptr;
  }

  uint32_t length = tup->length();

  RootedValue elt(cx);
  for (uint32_t index = 0; index < length; index++) {
    elt.set(tup->getDenseElement(index));

    /* Get element's character string. */
    JSString* str = ValueToSource(cx, elt);
    if (!str) {
      return nullptr;
    }

    /* Append element to buffer. */
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
  if (!sb.append(']')) {
    return nullptr;
  }

  return sb.finishString();
}

// Record and Tuple proposal section 9.2.1
bool TupleConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (args.isConstructing()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_CONSTRUCTOR, "Tuple");
    return false;
  }

  TupleType* tup = TupleType::create(cx, args.length(), args.array());
  if (!tup) {
    return false;
  }

  args.rval().setExtendedPrimitive(*tup);
  return true;
}

/*===========================================================================*\
                       BEGIN: Tuple.prototype methods
\*===========================================================================*/

bool IsTuple(HandleValue v) {
  if (v.isExtendedPrimitive()) return v.toExtendedPrimitive().is<TupleType>();
  if (v.isObject()) return v.toObject().is<TupleObject>();
  return false;
};

static MOZ_ALWAYS_INLINE TupleType* ThisTupleValue(HandleValue val) {
  MOZ_ASSERT(IsTuple(val));
  return val.isExtendedPrimitive() ? &val.toExtendedPrimitive().as<TupleType>()
                                   : val.toObject().as<TupleObject>().unbox();
}

// 8.2.3.2 get Tuple.prototype.length
bool lengthAccessor_impl(JSContext* cx, const CallArgs& args) {
  // Step 1.
  TupleType* tuple = ThisTupleValue(args.thisv());
  // Step 2.
  args.rval().setInt32(tuple->length());
  return true;
}

bool TupleType::lengthAccessor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTuple, lengthAccessor_impl>(cx, args);
}

/*===========================================================================*\
                         END: Tuple.prototype methods
\*===========================================================================*/

const JSClass TupleType::class_ = {"tuple", 0, JS_NULL_CLASS_OPS,
                                   &TupleType::classSpec_};

const JSClass TupleType::protoClass_ = {
    "Tuple.prototype", JSCLASS_HAS_CACHED_PROTO(JSProto_Tuple),
    JS_NULL_CLASS_OPS, &TupleType::classSpec_};

const JSPropertySpec properties_[] = {
    JS_PSG("length", TupleType::lengthAccessor, 0), JS_PS_END};

const ClassSpec TupleType::classSpec_ = {
    GenericCreateConstructor<TupleConstructor, 1, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<TupleType>,
    nullptr,
    nullptr,
    nullptr,
    properties_,
    nullptr};

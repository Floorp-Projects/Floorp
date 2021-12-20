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
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"
#include "vm/RecordType.h"
#include "vm/SelfHosting.h"
#include "vm/ToSource.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

TupleType* AllocateTuple(JSContext* cx, uint32_t length) {
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

TupleType* TupleType::create(JSContext* cx, uint32_t length,
                             const Value* elements) {
  for (uint32_t index = 0; index < length; index++) {
    if (!elements[index].isPrimitive()) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_RECORD_TUPLE_NO_OBJECT);
      return nullptr;
    }
  }

  TupleType* tup = AllocateTuple(cx, length);
  ObjectElements* header = tup->getElementsHeader();

  tup->initDenseElements(elements, length);
  tup->shrinkCapacityToInitializedLength(cx);

  header->setNotExtensible();
  header->seal();
  header->freeze();

  return tup;
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

NativeObject* tuplePrototype(JSContext* cx) {
  JSObject* proto = GlobalObject::getOrCreateTuplePrototype(cx, cx->global());
  if (!proto) {
    return nullptr;
  }

  MOZ_ASSERT(proto->is<NativeObject>());
  return &proto->as<NativeObject>();
}

bool Tuple_getProperty(JSContext* cx, HandleObject obj, HandleValue receiver,
                       HandleId id, MutableHandleValue vp) {
  MOZ_ASSERT(obj->is<TupleType>());

  if (id.isInt()) {
    int32_t index = id.toInt();
    TupleType& tup = obj->as<TupleType>();
    if (index >= 0 && uint32_t(index) < tup.length()) {
      vp.set(tup.getDenseElement(index));
      return true;
    }
    return false;
  }

  // We resolve the prototype dynamically because, since tuples are primitives,
  // it's realm-specific and not instance-specific.
  RootedNativeObject proto(cx, tuplePrototype(cx));
  if (!proto) return false;

  return NativeGetProperty(cx, proto, receiver, id, vp);
}

bool Tuple_setProperty(JSContext* cx, HandleObject obj, HandleId id,
                       HandleValue v, HandleValue receiver,
                       ObjectOpResult& result) {
  return result.failReadOnly();
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

const ObjectOps TupleTypeObjectOps = {
    /* lookupProperty */ nullptr,
    /* defineProperty */ nullptr,
    /* hasProperty */ nullptr,
    Tuple_getProperty,
    Tuple_setProperty,
    /* getOwnPropertyDescriptor */ nullptr,
    /* deleteProperty */ nullptr,
    /* getElements */ nullptr,
    /* getElements */ nullptr,
};

const JSClass TupleType::class_ = {"tuple",           0,
                                   JS_NULL_CLASS_OPS, &TupleType::classSpec_,
                                   JS_NULL_CLASS_EXT, &TupleTypeObjectOps};

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

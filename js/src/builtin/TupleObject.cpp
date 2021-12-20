/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/TupleObject.h"

#include "jsapi.h"

#include "js/Class.h"
#include "vm/ObjectOperations.h"
#include "vm/TupleType.h"

#include "vm/JSObject-inl.h"

using namespace js;

TupleObject* TupleObject::create(JSContext* cx, Handle<TupleType*> tuple) {
  TupleObject* tup = NewBuiltinClassInstance<TupleObject>(cx);
  if (!tup) {
    return nullptr;
  }
  tup->setFixedSlot(PrimitiveValueSlot, ExtendedPrimitiveValue(*tuple));
  return tup;
}

TupleType* TupleObject::unbox() const {
  return &getFixedSlot(PrimitiveValueSlot)
              .toExtendedPrimitive()
              .as<TupleType>();
}

const JSClass TupleObject::class_ = {
    "TupleObject",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_Tuple),
    JS_NULL_CLASS_OPS,
    JS_NULL_CLASS_SPEC,
    JS_NULL_CLASS_EXT,
    JS_NULL_OBJECT_OPS};

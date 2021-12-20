/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/RecordObject.h"

#include "jsapi.h"

#include "js/Class.h"
#include "vm/ObjectOperations.h"
#include "vm/RecordType.h"

#include "vm/JSObject-inl.h"

using namespace js;

RecordObject* RecordObject::create(JSContext* cx, Handle<RecordType*> record) {
  RecordObject* rec = NewBuiltinClassInstance<RecordObject>(cx);
  if (!rec) {
    return nullptr;
  }
  rec->setFixedSlot(PrimitiveValueSlot, ExtendedPrimitiveValue(*record));
  return rec;
}

RecordType* RecordObject::unbox() const {
  return &getFixedSlot(PrimitiveValueSlot)
              .toExtendedPrimitive()
              .as<RecordType>();
}

const JSClass RecordObject::class_ = {
    "RecordObject",    JSCLASS_HAS_RESERVED_SLOTS(SlotCount),
    JS_NULL_CLASS_OPS, JS_NULL_CLASS_SPEC,
    JS_NULL_CLASS_EXT, JS_NULL_OBJECT_OPS};

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/RecordObject.h"

#include "jsapi.h"

#include "vm/ObjectOperations.h"
#include "vm/RecordType.h"

#include "vm/JSObject-inl.h"

using namespace js;

// Record and Record proposal section 9.2.1

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

bool RecordObject::maybeUnbox(JSObject* obj, MutableHandle<RecordType*> rrec) {
  if (obj->is<RecordType>()) {
    rrec.set(&obj->as<RecordType>());
    return true;
  }
  if (obj->is<RecordObject>()) {
    rrec.set(obj->as<RecordObject>().unbox());
    return true;
  }
  return false;
}

bool rec_resolve(JSContext* cx, HandleObject obj, HandleId id,
                 bool* resolvedp) {
  Rooted<RecordType*> rec(cx, obj->as<RecordObject>().unbox());

  PropertyResult prop;
  if (!NativeLookupOwnProperty<CanGC>(cx, rec, id, &prop)) {
    return false;
  }
  if (prop.isNotFound()) {
    *resolvedp = false;
    return true;
  }
  MOZ_ASSERT(prop.isNativeProperty() && prop.propertyInfo().isDataProperty());

  RootedValue value(cx);
  if (prop.isDenseElement()) {
    value.set(rec->getDenseElement(prop.denseElementIndex()));
  } else {
    value.set(rec->getSlot(prop.propertyInfo().slot()));
  }

  static const unsigned RECORD_PROPERTY_ATTRS =
      JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
  if (!DefineDataProperty(cx, obj, id, value,
                          RECORD_PROPERTY_ATTRS | JSPROP_RESOLVING)) {
    return false;
  }

  *resolvedp = true;
  return true;
}

static const JSClassOps RecordObjectClassOps = {
    nullptr,      // addProperty
    nullptr,      // delProperty
    nullptr,      // enumerate
    nullptr,      // newEnumerate
    rec_resolve,  // resolve
    nullptr,      // mayResolve
    nullptr,      // finalize
    nullptr,      // call
    nullptr,      // hasInstance
    nullptr,      // construct
    nullptr,      // trace
};

const JSClass RecordObject::class_ = {"RecordObject",
                                      JSCLASS_HAS_RESERVED_SLOTS(SlotCount),
                                      &RecordObjectClassOps};

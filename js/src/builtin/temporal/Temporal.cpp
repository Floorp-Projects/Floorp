/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/Temporal.h"

#include "jspubtd.h"

#include "js/Class.h"
#include "js/PropertySpec.h"
#include "vm/GlobalObject.h"
#include "vm/ObjectOperations.h"

#include "vm/JSObject-inl.h"

using namespace js;
using namespace js::temporal;

static JSObject* CreateTemporalObject(JSContext* cx, JSProtoKey key) {
  RootedObject proto(cx, &cx->global()->getObjectPrototype());

  // The |Temporal| object is just a plain object with some "static" data
  // properties and some constructor properties.
  return NewTenuredObjectWithGivenProto<TemporalObject>(cx, proto);
}

/**
 * Initializes the Temporal Object and its standard built-in properties.
 */
static bool TemporalClassFinish(JSContext* cx, Handle<JSObject*> temporal,
                                Handle<JSObject*> proto) {
  Rooted<PropertyKey> ctorId(cx);
  Rooted<Value> ctorValue(cx);
  auto defineProperty = [&](JSProtoKey protoKey, Handle<PropertyName*> name) {
    JSObject* ctor = GlobalObject::getOrCreateConstructor(cx, protoKey);
    if (!ctor) {
      return false;
    }

    ctorId = NameToId(name);
    ctorValue.setObject(*ctor);
    return DefineDataProperty(cx, temporal, ctorId, ctorValue, 0);
  };

  // Add the constructor properties.
  for (const auto& protoKey :
       {JSProto_Calendar, JSProto_Duration, JSProto_Instant, JSProto_PlainDate,
        JSProto_PlainDateTime, JSProto_PlainMonthDay, JSProto_PlainTime,
        JSProto_PlainYearMonth, JSProto_TimeZone, JSProto_ZonedDateTime}) {
    if (!defineProperty(protoKey, ClassName(protoKey, cx))) {
      return false;
    }
  }

  // ClassName(JSProto_TemporalNow) returns "TemporalNow", so we need to handle
  // it separately.
  if (!defineProperty(JSProto_TemporalNow, cx->names().Now)) {
    return false;
  }

  return true;
}

const JSClass TemporalObject::class_ = {
    "Temporal",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Temporal),
    JS_NULL_CLASS_OPS,
    &TemporalObject::classSpec_,
};

static const JSPropertySpec Temporal_properties[] = {
    JS_PS_END,
};

const ClassSpec TemporalObject::classSpec_ = {
    CreateTemporalObject, nullptr, nullptr,
    Temporal_properties,  nullptr, nullptr,
    TemporalClassFinish,
};

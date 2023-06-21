/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/Temporal.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <iterator>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>

#include "jsfriendapi.h"
#include "jsnum.h"
#include "jspubtd.h"

#include "gc/Barrier.h"
#include "js/Class.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCVector.h"
#include "js/Id.h"
#include "js/Printer.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/String.h"
#include "js/Utility.h"
#include "js/Value.h"
#include "util/StringBuffer.h"
#include "vm/BigIntType.h"
#include "vm/BytecodeUtil.h"
#include "vm/GlobalObject.h"
#include "vm/JSAtom.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/ObjectOperations.h"
#include "vm/PlainObject.h"
#include "vm/Realm.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/ObjectOperations-inl.h"

using namespace js;
using namespace js::temporal;

/**
 * ToPositiveIntegerWithTruncation ( argument )
 */
bool js::temporal::ToPositiveIntegerWithTruncation(JSContext* cx,
                                                   Handle<Value> value,
                                                   const char* name,
                                                   double* result) {
  // Step 1.
  double number;
  if (!ToIntegerWithTruncation(cx, value, name, &number)) {
    return false;
  }

  // Step 2.
  if (number <= 0) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INVALID_NUMBER, name);
    return false;
  }

  // Step 3.
  *result = number;
  return true;
}

/**
 * ToIntegerWithTruncation ( argument )
 */
bool js::temporal::ToIntegerWithTruncation(JSContext* cx, Handle<Value> value,
                                           const char* name, double* result) {
  // Step 1.
  double number;
  if (!JS::ToNumber(cx, value, &number)) {
    return false;
  }

  // Step 2.
  if (!std::isfinite(number)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INVALID_INTEGER, name);
    return false;
  }

  // Step 3.
  *result = std::trunc(number) + (+0.0);  // Add zero to convert -0 to +0.
  return true;
}

/**
 * GetMethod ( V, P )
 */
bool js::temporal::GetMethod(JSContext* cx, Handle<JSObject*> object,
                             Handle<PropertyName*> name,
                             MutableHandle<Value> result) {
  // We don't directly invoke |Call|, because |Call| tries to find the function
  // on the stack (JSDVG_SEARCH_STACK). This leads to confusing error messages
  // like:
  //
  // js> print(new Temporal.ZonedDateTime(0n, {}, {}))
  // typein:1:6 TypeError: print is not a function

  // Step 1.
  if (!GetProperty(cx, object, object, name, result)) {
    return false;
  }

  // Step 2.
  if (result.isNullOrUndefined()) {
    return true;
  }

  // Step 3.
  if (!IsCallable(result)) {
    if (auto chars = StringToNewUTF8CharsZ(cx, *name)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_PROPERTY_NOT_CALLABLE, chars.get());
    }
    return false;
  }

  // Step 4.
  return true;
}

/**
 * GetMethod ( V, P )
 */
bool js::temporal::GetMethodForCall(JSContext* cx, Handle<JSObject*> object,
                                    Handle<PropertyName*> name,
                                    MutableHandle<Value> result) {
  // We don't directly invoke |Call|, because |Call| tries to find the function
  // on the stack (JSDVG_SEARCH_STACK). This leads to confusing error messages
  // like:
  //
  // js> print(new Temporal.ZonedDateTime(0n, {}, {}))
  // typein:1:6 TypeError: print is not a function

  // Step 1.
  if (!GetProperty(cx, object, object, name, result)) {
    return false;
  }

  // Steps 2-3.
  if (!IsCallable(result)) {
    if (auto chars = StringToNewUTF8CharsZ(cx, *name)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_PROPERTY_NOT_CALLABLE, chars.get());
    }
    return false;
  }

  // Step 4.
  return true;
}

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

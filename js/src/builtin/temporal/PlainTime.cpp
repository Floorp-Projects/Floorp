/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/PlainTime.h"

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/FloatingPoint.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stddef.h>
#include <type_traits>
#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TemporalUnit.h"
#include "ds/IdValuePair.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
#include "js/Class.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/friend/ErrorMessages.h"
#include "js/Printer.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Utility.h"
#include "js/Value.h"
#include "util/StringBuffer.h"
#include "vm/Compartment.h"
#include "vm/GlobalObject.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/PlainObject.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectOperations-inl.h"

using namespace js;
using namespace js::temporal;

static inline bool IsPlainTime(Handle<Value> v) {
  return v.isObject() && v.toObject().is<PlainTimeObject>();
}

#ifdef DEBUG
/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
template <typename T>
static bool IsValidTime(T hour, T minute, T second, T millisecond,
                        T microsecond, T nanosecond) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  // Step 1.
  MOZ_ASSERT(IsInteger(hour));
  MOZ_ASSERT(IsInteger(minute));
  MOZ_ASSERT(IsInteger(second));
  MOZ_ASSERT(IsInteger(millisecond));
  MOZ_ASSERT(IsInteger(microsecond));
  MOZ_ASSERT(IsInteger(nanosecond));

  // Step 2.
  if (hour < 0 || hour > 23) {
    return false;
  }

  // Step 3.
  if (minute < 0 || minute > 59) {
    return false;
  }

  // Step 4.
  if (second < 0 || second > 59) {
    return false;
  }

  // Step 5.
  if (millisecond < 0 || millisecond > 999) {
    return false;
  }

  // Step 6.
  if (microsecond < 0 || microsecond > 999) {
    return false;
  }

  // Step 7.
  if (nanosecond < 0 || nanosecond > 999) {
    return false;
  }

  // Step 8.
  return true;
}

/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
bool js::temporal::IsValidTime(const PlainTime& time) {
  auto& [hour, minute, second, millisecond, microsecond, nanosecond] = time;
  return ::IsValidTime(hour, minute, second, millisecond, microsecond,
                       nanosecond);
}

/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
bool js::temporal::IsValidTime(double hour, double minute, double second,
                               double millisecond, double microsecond,
                               double nanosecond) {
  return ::IsValidTime(hour, minute, second, millisecond, microsecond,
                       nanosecond);
}
#endif

static void ReportInvalidTimeValue(JSContext* cx, const char* name, int32_t min,
                                   int32_t max, double num) {
  Int32ToCStringBuf minCbuf;
  const char* minStr = Int32ToCString(&minCbuf, min);

  Int32ToCStringBuf maxCbuf;
  const char* maxStr = Int32ToCString(&maxCbuf, max);

  ToCStringBuf numCbuf;
  const char* numStr = NumberToCString(&numCbuf, num);

  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_TEMPORAL_PLAIN_TIME_INVALID_VALUE, name,
                            minStr, maxStr, numStr);
}

template <typename T>
static inline bool ThrowIfInvalidTimeValue(JSContext* cx, const char* name,
                                           int32_t min, int32_t max, T num) {
  if (min <= num && num <= max) {
    return true;
  }
  ReportInvalidTimeValue(cx, name, min, max, num);
  return false;
}

/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
template <typename T>
static bool ThrowIfInvalidTime(JSContext* cx, T hour, T minute, T second,
                               T millisecond, T microsecond, T nanosecond) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  // Step 1.
  MOZ_ASSERT(IsInteger(hour));
  MOZ_ASSERT(IsInteger(minute));
  MOZ_ASSERT(IsInteger(second));
  MOZ_ASSERT(IsInteger(millisecond));
  MOZ_ASSERT(IsInteger(microsecond));
  MOZ_ASSERT(IsInteger(nanosecond));

  // Step 2.
  if (!ThrowIfInvalidTimeValue(cx, "hour", 0, 23, hour)) {
    return false;
  }

  // Step 3.
  if (!ThrowIfInvalidTimeValue(cx, "minute", 0, 59, minute)) {
    return false;
  }

  // Step 4.
  if (!ThrowIfInvalidTimeValue(cx, "second", 0, 59, second)) {
    return false;
  }

  // Step 5.
  if (!ThrowIfInvalidTimeValue(cx, "millisecond", 0, 999, millisecond)) {
    return false;
  }

  // Step 6.
  if (!ThrowIfInvalidTimeValue(cx, "microsecond", 0, 999, microsecond)) {
    return false;
  }

  // Step 7.
  if (!ThrowIfInvalidTimeValue(cx, "nanosecond", 0, 999, nanosecond)) {
    return false;
  }

  // Step 8.
  return true;
}

/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
bool js::temporal::ThrowIfInvalidTime(JSContext* cx, const PlainTime& time) {
  auto& [hour, minute, second, millisecond, microsecond, nanosecond] = time;
  return ::ThrowIfInvalidTime(cx, hour, minute, second, millisecond,
                              microsecond, nanosecond);
}

/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
bool js::temporal::ThrowIfInvalidTime(JSContext* cx, double hour, double minute,
                                      double second, double millisecond,
                                      double microsecond, double nanosecond) {
  return ::ThrowIfInvalidTime(cx, hour, minute, second, millisecond,
                              microsecond, nanosecond);
}

/**
 * CreateTemporalTime ( hour, minute, second, millisecond, microsecond,
 * nanosecond [ , newTarget ] )
 */
static PlainTimeObject* CreateTemporalTime(JSContext* cx, const CallArgs& args,
                                           double hour, double minute,
                                           double second, double millisecond,
                                           double microsecond,
                                           double nanosecond) {
  MOZ_ASSERT(IsInteger(hour));
  MOZ_ASSERT(IsInteger(minute));
  MOZ_ASSERT(IsInteger(second));
  MOZ_ASSERT(IsInteger(millisecond));
  MOZ_ASSERT(IsInteger(microsecond));
  MOZ_ASSERT(IsInteger(nanosecond));

  // Step 1.
  if (!ThrowIfInvalidTime(cx, hour, minute, second, millisecond, microsecond,
                          nanosecond)) {
    return nullptr;
  }

  // Steps 2-3.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_PlainTime,
                                          &proto)) {
    return nullptr;
  }

  auto* object = NewObjectWithClassProto<PlainTimeObject>(cx, proto);
  if (!object) {
    return nullptr;
  }

  // Step 4.
  object->setFixedSlot(PlainTimeObject::ISO_HOUR_SLOT, Int32Value(hour));

  // Step 5.
  object->setFixedSlot(PlainTimeObject::ISO_MINUTE_SLOT, Int32Value(minute));

  // Step 6.
  object->setFixedSlot(PlainTimeObject::ISO_SECOND_SLOT, Int32Value(second));

  // Step 7.
  object->setFixedSlot(PlainTimeObject::ISO_MILLISECOND_SLOT,
                       Int32Value(millisecond));

  // Step 8.
  object->setFixedSlot(PlainTimeObject::ISO_MICROSECOND_SLOT,
                       Int32Value(microsecond));

  // Step 9.
  object->setFixedSlot(PlainTimeObject::ISO_NANOSECOND_SLOT,
                       Int32Value(nanosecond));

  // Step 10.
  object->setFixedSlot(PlainTimeObject::CALENDAR_SLOT, NullValue());

  // Step 11.
  return object;
}

/**
 * CreateTemporalTime ( hour, minute, second, millisecond, microsecond,
 * nanosecond [ , newTarget ] )
 */
PlainTimeObject* js::temporal::CreateTemporalTime(JSContext* cx,
                                                  const PlainTime& time) {
  auto& [hour, minute, second, millisecond, microsecond, nanosecond] = time;

  // Step 1.
  if (!ThrowIfInvalidTime(cx, time)) {
    return nullptr;
  }

  // Steps 2-3.
  auto* object = NewBuiltinClassInstance<PlainTimeObject>(cx);
  if (!object) {
    return nullptr;
  }

  // Step 4.
  object->setFixedSlot(PlainTimeObject::ISO_HOUR_SLOT, Int32Value(hour));

  // Step 5.
  object->setFixedSlot(PlainTimeObject::ISO_MINUTE_SLOT, Int32Value(minute));

  // Step 6.
  object->setFixedSlot(PlainTimeObject::ISO_SECOND_SLOT, Int32Value(second));

  // Step 7.
  object->setFixedSlot(PlainTimeObject::ISO_MILLISECOND_SLOT,
                       Int32Value(millisecond));

  // Step 8.
  object->setFixedSlot(PlainTimeObject::ISO_MICROSECOND_SLOT,
                       Int32Value(microsecond));

  // Step 9.
  object->setFixedSlot(PlainTimeObject::ISO_NANOSECOND_SLOT,
                       Int32Value(nanosecond));

  // Step 10.
  object->setFixedSlot(PlainTimeObject::CALENDAR_SLOT, NullValue());

  // Step 11.
  return object;
}

/**
 * BalanceTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
template <typename IntT>
static BalancedTime BalanceTime(IntT hour, IntT minute, IntT second,
                                IntT millisecond, IntT microsecond,
                                IntT nanosecond) {
  // Step 1. (Not applicable in our implementation.)

  // Combined floor'ed division and modulo operation.
  auto divmod = [](IntT dividend, int32_t divisor, int32_t* remainder) {
    MOZ_ASSERT(divisor > 0);

    IntT quotient = dividend / divisor;
    *remainder = dividend % divisor;

    // The remainder is negative, add the divisor and simulate a floor instead
    // of trunc division.
    if (*remainder < 0) {
      *remainder += divisor;
      quotient -= 1;
    }

    return quotient;
  };

  PlainTime time = {};

  // Steps 2-3.
  microsecond += divmod(nanosecond, 1000, &time.nanosecond);

  // Steps 4-5.
  millisecond += divmod(microsecond, 1000, &time.microsecond);

  // Steps 6-7.
  second += divmod(millisecond, 1000, &time.millisecond);

  // Steps 8-9.
  minute += divmod(second, 60, &time.second);

  // Steps 10-11.
  hour += divmod(minute, 60, &time.minute);

  // Steps 12-13.
  int32_t days = divmod(hour, 24, &time.hour);

  // Step 14.
  MOZ_ASSERT(IsValidTime(time));
  return {days, time};
}

/**
 * BalanceTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
BalancedTime js::temporal::BalanceTime(const PlainTime& time,
                                       int64_t nanoseconds) {
  MOZ_ASSERT(IsValidTime(time));
  MOZ_ASSERT(std::abs(nanoseconds) <= 2 * ToNanoseconds(TemporalUnit::Day));

  return ::BalanceTime<int64_t>(time.hour, time.minute, time.second,
                                time.millisecond, time.microsecond,
                                time.nanosecond + nanoseconds);
}

JSObject* js::temporal::PlainTimeObject::createCalendar(
    JSContext* cx, Handle<PlainTimeObject*> obj) {
  auto* calendar = GetISO8601Calendar(cx);
  if (!calendar) {
    return nullptr;
  }

  obj->setCalendar(calendar);
  return calendar;
}

/**
 * Temporal.PlainTime ( [ hour [ , minute [ , second [ , millisecond [ ,
 * microsecond [ , nanosecond ] ] ] ] ] ] )
 */
static bool PlainTimeConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.PlainTime")) {
    return false;
  }

  // Step 2.
  double hour = 0;
  if (args.hasDefined(0)) {
    if (!ToIntegerWithTruncation(cx, args[0], "hour", &hour)) {
      return false;
    }
  }

  // Step 3.
  double minute = 0;
  if (args.hasDefined(1)) {
    if (!ToIntegerWithTruncation(cx, args[1], "minute", &minute)) {
      return false;
    }
  }

  // Step 4.
  double second = 0;
  if (args.hasDefined(2)) {
    if (!ToIntegerWithTruncation(cx, args[2], "second", &second)) {
      return false;
    }
  }

  // Step 5.
  double millisecond = 0;
  if (args.hasDefined(3)) {
    if (!ToIntegerWithTruncation(cx, args[3], "millisecond", &millisecond)) {
      return false;
    }
  }

  // Step 6.
  double microsecond = 0;
  if (args.hasDefined(4)) {
    if (!ToIntegerWithTruncation(cx, args[4], "microsecond", &microsecond)) {
      return false;
    }
  }

  // Step 7.
  double nanosecond = 0;
  if (args.hasDefined(5)) {
    if (!ToIntegerWithTruncation(cx, args[5], "nanosecond", &nanosecond)) {
      return false;
    }
  }

  // Step 8.
  auto* temporalTime = CreateTemporalTime(cx, args, hour, minute, second,
                                          millisecond, microsecond, nanosecond);
  if (!temporalTime) {
    return false;
  }

  args.rval().setObject(*temporalTime);
  return true;
}

/**
 * Temporal.PlainTime.prototype.getISOFields ( )
 */
static bool PlainTime_getISOFields(JSContext* cx, const CallArgs& args) {
  Rooted<PlainTimeObject*> temporalTime(
      cx, &args.thisv().toObject().as<PlainTimeObject>());
  auto time = ToPlainTime(temporalTime);

  auto* calendar = PlainTimeObject::getOrCreateCalendar(cx, temporalTime);
  if (!calendar) {
    return false;
  }

  // Step 3.
  Rooted<IdValueVector> fields(cx, IdValueVector(cx));

  // Step 4.
  if (!fields.emplaceBack(NameToId(cx->names().calendar),
                          ObjectValue(*calendar))) {
    return false;
  }

  // Step 5.
  if (!fields.emplaceBack(NameToId(cx->names().isoHour),
                          Int32Value(time.hour))) {
    return false;
  }

  // Step 6.
  if (!fields.emplaceBack(NameToId(cx->names().isoMicrosecond),
                          Int32Value(time.microsecond))) {
    return false;
  }

  // Step 7.
  if (!fields.emplaceBack(NameToId(cx->names().isoMillisecond),
                          Int32Value(time.millisecond))) {
    return false;
  }

  // Step 8.
  if (!fields.emplaceBack(NameToId(cx->names().isoMinute),
                          Int32Value(time.minute))) {
    return false;
  }

  // Step 9.
  if (!fields.emplaceBack(NameToId(cx->names().isoNanosecond),
                          Int32Value(time.nanosecond))) {
    return false;
  }

  // Step 10.
  if (!fields.emplaceBack(NameToId(cx->names().isoSecond),
                          Int32Value(time.second))) {
    return false;
  }

  // Step 11.
  auto* obj =
      NewPlainObjectWithUniqueNames(cx, fields.begin(), fields.length());
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainTime.prototype.getISOFields ( )
 */
static bool PlainTime_getISOFields(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainTime, PlainTime_getISOFields>(cx, args);
}

/**
 * Temporal.PlainTime.prototype.valueOf ( )
 */
static bool PlainTime_valueOf(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                            "PlainTime", "primitive type");
  return false;
}

const JSClass PlainTimeObject::class_ = {
    "Temporal.PlainTime",
    JSCLASS_HAS_RESERVED_SLOTS(PlainTimeObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_PlainTime),
    JS_NULL_CLASS_OPS,
    &PlainTimeObject::classSpec_,
};

const JSClass& PlainTimeObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec PlainTime_methods[] = {
    JS_FS_END,
};

static const JSFunctionSpec PlainTime_prototype_methods[] = {
    JS_FN("getISOFields", PlainTime_getISOFields, 0, 0),
    JS_FN("valueOf", PlainTime_valueOf, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec PlainTime_prototype_properties[] = {
    JS_PS_END,
};

const ClassSpec PlainTimeObject::classSpec_ = {
    GenericCreateConstructor<PlainTimeConstructor, 0, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<PlainTimeObject>,
    PlainTime_methods,
    nullptr,
    PlainTime_prototype_methods,
    PlainTime_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};

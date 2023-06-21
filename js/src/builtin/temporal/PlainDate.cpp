/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/PlainDate.h"

#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <stdint.h>
#include <type_traits>
#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/PlainTime.h"
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalTypes.h"
#include "ds/IdValuePair.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
#include "js/Class.h"
#include "js/Conversions.h"
#include "js/Date.h"
#include "js/ErrorReport.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCVector.h"
#include "js/Id.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
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

static inline bool IsPlainDate(Handle<Value> v) {
  return v.isObject() && v.toObject().is<PlainDateObject>();
}

#ifdef DEBUG
/**
 * IsValidISODate ( year, month, day )
 */
template <typename T>
static bool IsValidISODate(T year, T month, T day) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  // Step 1.
  MOZ_ASSERT(IsInteger(year));
  MOZ_ASSERT(IsInteger(month));
  MOZ_ASSERT(IsInteger(day));

  // Step 2.
  if (month < 1 || month > 12) {
    return false;
  }

  // Step 3.
  int32_t daysInMonth = js::temporal::ISODaysInMonth(year, int32_t(month));

  // Step 4.
  if (day < 1 || day > daysInMonth) {
    return false;
  }

  // Step 5.
  return true;
}

/**
 * IsValidISODate ( year, month, day )
 */
bool js::temporal::IsValidISODate(const PlainDate& date) {
  auto& [year, month, day] = date;
  return ::IsValidISODate(year, month, day);
}

/**
 * IsValidISODate ( year, month, day )
 */
bool js::temporal::IsValidISODate(double year, double month, double day) {
  return ::IsValidISODate(year, month, day);
}
#endif

static void ReportInvalidDateValue(JSContext* cx, const char* name, int32_t min,
                                   int32_t max, double num) {
  Int32ToCStringBuf minCbuf;
  const char* minStr = Int32ToCString(&minCbuf, min);

  Int32ToCStringBuf maxCbuf;
  const char* maxStr = Int32ToCString(&maxCbuf, max);

  ToCStringBuf numCbuf;
  const char* numStr = NumberToCString(&numCbuf, num);

  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_TEMPORAL_PLAIN_DATE_INVALID_VALUE, name,
                            minStr, maxStr, numStr);
}

template <typename T>
static inline bool ThrowIfInvalidDateValue(JSContext* cx, const char* name,
                                           int32_t min, int32_t max, T num) {
  if (min <= num && num <= max) {
    return true;
  }
  ReportInvalidDateValue(cx, name, min, max, num);
  return false;
}

/**
 * IsValidISODate ( year, month, day )
 */
template <typename T>
static bool ThrowIfInvalidISODate(JSContext* cx, T year, T month, T day) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  // Step 1.
  MOZ_ASSERT(IsInteger(year));
  MOZ_ASSERT(IsInteger(month));
  MOZ_ASSERT(IsInteger(day));

  // Step 2.
  if (!ThrowIfInvalidDateValue(cx, "month", 1, 12, month)) {
    return false;
  }

  // Step 3.
  int32_t daysInMonth = js::temporal::ISODaysInMonth(year, int32_t(month));

  // Step 4.
  if (!ThrowIfInvalidDateValue(cx, "day", 1, daysInMonth, day)) {
    return false;
  }

  // Step 5.
  return true;
}

/**
 * IsValidISODate ( year, month, day )
 */
bool js::temporal::ThrowIfInvalidISODate(JSContext* cx, const PlainDate& date) {
  auto& [year, month, day] = date;
  return ::ThrowIfInvalidISODate(cx, year, month, day);
}

/**
 * IsValidISODate ( year, month, day )
 */
bool js::temporal::ThrowIfInvalidISODate(JSContext* cx, double year,
                                         double month, double day) {
  return ::ThrowIfInvalidISODate(cx, year, month, day);
}

/**
 * CreateTemporalDate ( isoYear, isoMonth, isoDay, calendar [ , newTarget ] )
 */
static PlainDateObject* CreateTemporalDate(JSContext* cx, const CallArgs& args,
                                           double isoYear, double isoMonth,
                                           double isoDay,
                                           Handle<JSObject*> calendar) {
  MOZ_ASSERT(IsInteger(isoYear));
  MOZ_ASSERT(IsInteger(isoMonth));
  MOZ_ASSERT(IsInteger(isoDay));

  // Step 1.
  if (!ThrowIfInvalidISODate(cx, isoYear, isoMonth, isoDay)) {
    return nullptr;
  }

  // Step 2.
  if (!ISODateTimeWithinLimits(isoYear, isoMonth, isoDay)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_INVALID);
    return nullptr;
  }

  // Steps 3-4.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_PlainDate,
                                          &proto)) {
    return nullptr;
  }

  auto* object = NewObjectWithClassProto<PlainDateObject>(cx, proto);
  if (!object) {
    return nullptr;
  }

  // Step 5.
  object->setFixedSlot(PlainDateObject::ISO_YEAR_SLOT, Int32Value(isoYear));

  // Step 6.
  object->setFixedSlot(PlainDateObject::ISO_MONTH_SLOT, Int32Value(isoMonth));

  // Step 7.
  object->setFixedSlot(PlainDateObject::ISO_DAY_SLOT, Int32Value(isoDay));

  // Step 8.
  object->setFixedSlot(PlainDateObject::CALENDAR_SLOT, ObjectValue(*calendar));

  // Step 9.
  return object;
}

/**
 * CreateTemporalDate ( isoYear, isoMonth, isoDay, calendar [ , newTarget ] )
 */
PlainDateObject* js::temporal::CreateTemporalDate(JSContext* cx,
                                                  const PlainDate& date,
                                                  Handle<JSObject*> calendar) {
  auto& [isoYear, isoMonth, isoDay] = date;

  // Step 1.
  if (!ThrowIfInvalidISODate(cx, date)) {
    return nullptr;
  }

  // Step 2.
  if (!ISODateTimeWithinLimits(date)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_INVALID);
    return nullptr;
  }

  // Steps 3-4.
  auto* object = NewBuiltinClassInstance<PlainDateObject>(cx);
  if (!object) {
    return nullptr;
  }

  // Step 5.
  object->setFixedSlot(PlainDateObject::ISO_YEAR_SLOT, Int32Value(isoYear));

  // Step 6.
  object->setFixedSlot(PlainDateObject::ISO_MONTH_SLOT, Int32Value(isoMonth));

  // Step 7.
  object->setFixedSlot(PlainDateObject::ISO_DAY_SLOT, Int32Value(isoDay));

  // Step 8.
  object->setFixedSlot(PlainDateObject::CALENDAR_SLOT, ObjectValue(*calendar));

  // Step 9.
  return object;
}

/**
 * ToTemporalDate ( item [ , options ] )
 */
Wrapped<PlainDateObject*> js::temporal::ToTemporalDate(JSContext* cx,
                                                       Handle<JSObject*> item) {
  MOZ_CRASH("NYI");
}

/**
 * ToTemporalDate ( item [ , options ] )
 */
bool js::temporal::ToTemporalDate(JSContext* cx, Handle<Value> item,
                                  PlainDate* result) {
  MOZ_CRASH("NYI");
}

/**
 * ToTemporalDate ( item [ , options ] )
 */
bool js::temporal::ToTemporalDate(JSContext* cx, Handle<Value> item,
                                  PlainDate* result,
                                  MutableHandle<JSObject*> calendar) {
  MOZ_CRASH("NYI");
}

/**
 * Temporal.PlainDate ( isoYear, isoMonth, isoDay [ , calendarLike ] )
 */
static bool PlainDateConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.PlainDate")) {
    return false;
  }

  // Step 2.
  double isoYear;
  if (!ToIntegerWithTruncation(cx, args.get(0), "year", &isoYear)) {
    return false;
  }

  // Step 3.
  double isoMonth;
  if (!ToIntegerWithTruncation(cx, args.get(1), "month", &isoMonth)) {
    return false;
  }

  // Step 4.
  double isoDay;
  if (!ToIntegerWithTruncation(cx, args.get(2), "day", &isoDay)) {
    return false;
  }

  // Step 5.
  Rooted<JSObject*> calendar(cx,
                             ToTemporalCalendarWithISODefault(cx, args.get(3)));
  if (!calendar) {
    return false;
  }

  // Step 6.
  auto* temporalDate =
      CreateTemporalDate(cx, args, isoYear, isoMonth, isoDay, calendar);
  if (!temporalDate) {
    return false;
  }

  args.rval().setObject(*temporalDate);
  return true;
}

/**
 * Temporal.PlainDate.prototype.getISOFields ( )
 */
static bool PlainDate_getISOFields(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  auto date = ToPlainDate(temporalDate);
  JSObject* calendar = temporalDate->calendar();

  // Step 3.
  Rooted<IdValueVector> fields(cx, IdValueVector(cx));

  // Step 4.
  if (!fields.emplaceBack(NameToId(cx->names().calendar),
                          ObjectValue(*calendar))) {
    return false;
  }

  // Step 5.
  if (!fields.emplaceBack(NameToId(cx->names().isoDay), Int32Value(date.day))) {
    return false;
  }

  // Step 6.
  if (!fields.emplaceBack(NameToId(cx->names().isoMonth),
                          Int32Value(date.month))) {
    return false;
  }

  // Step 7.
  if (!fields.emplaceBack(NameToId(cx->names().isoYear),
                          Int32Value(date.year))) {
    return false;
  }

  // Step 8.
  auto* obj =
      NewPlainObjectWithUniqueNames(cx, fields.begin(), fields.length());
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDate.prototype.getISOFields ( )
 */
static bool PlainDate_getISOFields(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_getISOFields>(cx, args);
}

/**
 *  Temporal.PlainDate.prototype.valueOf ( )
 */
static bool PlainDate_valueOf(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                            "PlainDate", "primitive type");
  return false;
}

const JSClass PlainDateObject::class_ = {
    "Temporal.PlainDate",
    JSCLASS_HAS_RESERVED_SLOTS(PlainDateObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_PlainDate),
    JS_NULL_CLASS_OPS,
    &PlainDateObject::classSpec_,
};

const JSClass& PlainDateObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec PlainDate_methods[] = {
    JS_FS_END,
};

static const JSFunctionSpec PlainDate_prototype_methods[] = {
    JS_FN("getISOFields", PlainDate_getISOFields, 0, 0),
    JS_FN("valueOf", PlainDate_valueOf, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec PlainDate_prototype_properties[] = {
    JS_PS_END,
};

const ClassSpec PlainDateObject::classSpec_ = {
    GenericCreateConstructor<PlainDateConstructor, 3, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<PlainDateObject>,
    PlainDate_methods,
    nullptr,
    PlainDate_prototype_methods,
    PlainDate_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};

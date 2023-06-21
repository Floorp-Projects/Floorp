/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/PlainYearMonth.h"

#include "mozilla/Assertions.h"

#include <cstdlib>
#include <initializer_list>
#include <stddef.h>
#include <type_traits>
#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/PlainDate.h"
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
#include "vm/ObjectOperations.h"
#include "vm/PlainObject.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::temporal;

/**
 * ISOYearMonthWithinLimits ( year, month )
 */
template <typename T>
static bool ISOYearMonthWithinLimits(T year, int32_t month) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  // Step 1.
  MOZ_ASSERT(IsInteger(year));
  MOZ_ASSERT(1 <= month && month <= 12);

  // Step 2.
  if (year < -271821 || year > 275760) {
    return false;
  }

  // Step 3.
  if (year == -271821 && month < 4) {
    return false;
  }

  // Step 4.
  if (year == 275760 && month > 9) {
    return false;
  }

  // Step 5.
  return true;
}

/**
 * CreateTemporalYearMonth ( isoYear, isoMonth, calendar, referenceISODay [ ,
 * newTarget ] )
 */
static PlainYearMonthObject* CreateTemporalYearMonth(
    JSContext* cx, const CallArgs& args, double isoYear, double isoMonth,
    double isoDay, Handle<JSObject*> calendar) {
  MOZ_ASSERT(IsInteger(isoYear));
  MOZ_ASSERT(IsInteger(isoMonth));
  MOZ_ASSERT(IsInteger(isoDay));

  // Step 1.
  if (!ThrowIfInvalidISODate(cx, isoYear, isoMonth, isoDay)) {
    return nullptr;
  }

  // FIXME: spec issue - Consider calling ISODateTimeWithinLimits to include
  // testing |referenceISODay|?

  // Step 2.
  if (!ISOYearMonthWithinLimits(isoYear, isoMonth)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_YEAR_MONTH_INVALID);
    return nullptr;
  }

  // Steps 3-4.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_PlainYearMonth,
                                          &proto)) {
    return nullptr;
  }

  auto* obj = NewObjectWithClassProto<PlainYearMonthObject>(cx, proto);
  if (!obj) {
    return nullptr;
  }

  // Step 5.
  obj->setFixedSlot(PlainYearMonthObject::ISO_YEAR_SLOT, Int32Value(isoYear));

  // Step 6.
  obj->setFixedSlot(PlainYearMonthObject::ISO_MONTH_SLOT, Int32Value(isoMonth));

  // Step 7.
  obj->setFixedSlot(PlainYearMonthObject::CALENDAR_SLOT,
                    ObjectValue(*calendar));

  // Step 8.
  obj->setFixedSlot(PlainYearMonthObject::ISO_DAY_SLOT, Int32Value(isoDay));

  // Step 9.
  return obj;
}

/**
 * CreateTemporalYearMonth ( isoYear, isoMonth, calendar, referenceISODay [ ,
 * newTarget ] )
 */
PlainYearMonthObject* js::temporal::CreateTemporalYearMonth(
    JSContext* cx, const PlainDate& date, Handle<JSObject*> calendar) {
  auto& [isoYear, isoMonth, isoDay] = date;

  // Step 1.
  if (!ThrowIfInvalidISODate(cx, date)) {
    return nullptr;
  }

  // FIXME: spec issue - Consider calling ISODateTimeWithinLimits to include
  // testing |referenceISODay|?

  // Step 2.
  if (!ISOYearMonthWithinLimits(isoYear, isoMonth)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_YEAR_MONTH_INVALID);
    return nullptr;
  }

  // Steps 3-4.
  auto* obj = NewBuiltinClassInstance<PlainYearMonthObject>(cx);
  if (!obj) {
    return nullptr;
  }

  // Step 5.
  obj->setFixedSlot(PlainYearMonthObject::ISO_YEAR_SLOT, Int32Value(isoYear));

  // Step 6.
  obj->setFixedSlot(PlainYearMonthObject::ISO_MONTH_SLOT, Int32Value(isoMonth));

  // Step 7.
  obj->setFixedSlot(PlainYearMonthObject::CALENDAR_SLOT,
                    ObjectValue(*calendar));

  // Step 8.
  obj->setFixedSlot(PlainYearMonthObject::ISO_DAY_SLOT, Int32Value(isoDay));

  // Step 9.
  return obj;
}

/**
 * Temporal.PlainYearMonth ( isoYear, isoMonth [ , calendarLike [ ,
 * referenceISODay ] ] )
 */
static bool PlainYearMonthConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.PlainYearMonth")) {
    return false;
  }

  // Step 3.
  double isoYear;
  if (!ToIntegerWithTruncation(cx, args.get(0), "year", &isoYear)) {
    return false;
  }

  // Step 4.
  double isoMonth;
  if (!ToIntegerWithTruncation(cx, args.get(1), "month", &isoMonth)) {
    return false;
  }

  // Step 5.
  Rooted<JSObject*> calendar(cx,
                             ToTemporalCalendarWithISODefault(cx, args.get(2)));
  if (!calendar) {
    return false;
  }

  // Steps 2 and 6.
  double isoDay = 1;
  if (args.hasDefined(3)) {
    if (!ToIntegerWithTruncation(cx, args[3], "day", &isoDay)) {
      return false;
    }
  }

  // Step 7.
  auto* yearMonth =
      CreateTemporalYearMonth(cx, args, isoYear, isoMonth, isoDay, calendar);
  if (!yearMonth) {
    return false;
  }

  args.rval().setObject(*yearMonth);
  return true;
}

const JSClass PlainYearMonthObject::class_ = {
    "Temporal.PlainYearMonth",
    JSCLASS_HAS_RESERVED_SLOTS(PlainYearMonthObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_PlainYearMonth),
    JS_NULL_CLASS_OPS,
    &PlainYearMonthObject::classSpec_,
};

const JSClass& PlainYearMonthObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec PlainYearMonth_methods[] = {
    JS_FS_END,
};

static const JSFunctionSpec PlainYearMonth_prototype_methods[] = {
    JS_FS_END,
};

static const JSPropertySpec PlainYearMonth_prototype_properties[] = {
    JS_PS_END,
};

const ClassSpec PlainYearMonthObject::classSpec_ = {
    GenericCreateConstructor<PlainYearMonthConstructor, 2,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<PlainYearMonthObject>,
    PlainYearMonth_methods,
    nullptr,
    PlainYearMonth_prototype_methods,
    PlainYearMonth_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};

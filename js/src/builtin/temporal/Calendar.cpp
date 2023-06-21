/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/Calendar.h"

#include "mozilla/Assertions.h"
#include "mozilla/Range.h"
#include "mozilla/RangedPtr.h"
#include "mozilla/TextUtils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iterator>
#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "jsfriendapi.h"
#include "jsnum.h"
#include "jspubtd.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "builtin/Array.h"
#include "builtin/String.h"
#include "gc/Allocator.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
#include "js/Class.h"
#include "js/ComparisonOperators.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/ForOfIterator.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCAPI.h"
#include "js/GCHashTable.h"
#include "js/GCVector.h"
#include "js/HashTable.h"
#include "js/Id.h"
#include "js/Printer.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Utility.h"
#include "js/Value.h"
#include "util/StringBuffer.h"
#include "util/Text.h"
#include "vm/ArrayObject.h"
#include "vm/BytecodeUtil.h"
#include "vm/Compartment.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/PlainObject.h"
#include "vm/PropertyInfo.h"
#include "vm/PropertyKey.h"
#include "vm/Shape.h"
#include "vm/Stack.h"
#include "vm/StringType.h"

#include "vm/JSAtom-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectOperations-inl.h"
#include "vm/Realm-inl.h"

using namespace js;
using namespace js::temporal;

static inline bool IsCalendar(Handle<Value> v) {
  return v.isObject() && v.toObject().is<CalendarObject>();
}

/**
 * IsISOLeapYear ( year )
 */
static constexpr bool IsISOLeapYear(int32_t year) {
  // Steps 1-5.
  return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
}

/**
 * IsISOLeapYear ( year )
 */
static bool IsISOLeapYear(double year) {
  // Step 1.
  MOZ_ASSERT(IsInteger(year));

  // Steps 2-5.
  return std::fmod(year, 4) == 0 &&
         (std::fmod(year, 100) != 0 || std::fmod(year, 400) == 0);
}

/**
 * ISODaysInYear ( year )
 */
int32_t js::temporal::ISODaysInYear(int32_t year) {
  // Steps 1-3.
  return IsISOLeapYear(year) ? 366 : 365;
}

/**
 * ISODaysInMonth ( year, month )
 */
static constexpr int32_t ISODaysInMonth(int32_t year, int32_t month) {
  MOZ_ASSERT(1 <= month && month <= 12);

  constexpr uint8_t daysInMonth[2][13] = {
      {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
      {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};

  // Steps 1-4.
  return daysInMonth[IsISOLeapYear(year)][month];
}

/**
 * ISODaysInMonth ( year, month )
 */
int32_t js::temporal::ISODaysInMonth(int32_t year, int32_t month) {
  return ::ISODaysInMonth(year, month);
}

/**
 * ISODaysInMonth ( year, month )
 */
int32_t js::temporal::ISODaysInMonth(double year, int32_t month) {
  MOZ_ASSERT(1 <= month && month <= 12);

  static constexpr uint8_t daysInMonth[2][13] = {
      {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
      {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};

  // Steps 1-4.
  return daysInMonth[IsISOLeapYear(year)][month];
}

#ifdef DEBUG
template <typename CharT>
static bool StringIsAsciiLowerCase(mozilla::Range<CharT> str) {
  return std::all_of(str.begin().get(), str.end().get(), [](CharT ch) {
    return mozilla::IsAscii(ch) && !mozilla::IsAsciiUppercaseAlpha(ch);
  });
}

static bool StringIsAsciiLowerCase(JSLinearString* str) {
  JS::AutoCheckCannotGC nogc;
  return str->hasLatin1Chars()
             ? StringIsAsciiLowerCase(str->latin1Range(nogc))
             : StringIsAsciiLowerCase(str->twoByteRange(nogc));
}
#endif

/**
 * IsBuiltinCalendar ( id )
 */
static bool IsBuiltinCalendar(JSLinearString* id) {
  // Callers must convert to lower case.
  MOZ_ASSERT(StringIsAsciiLowerCase(id));

  // Steps 1-3.
  return StringEqualsLiteral(id, "iso8601");
}

static JSLinearString* ThrowIfNotBuiltinCalendar(JSContext* cx,
                                                 Handle<JSLinearString*> id) {
  if (!StringIsAscii(id)) {
    if (auto chars = QuoteString(cx, id)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_CALENDAR_INVALID_ID, chars.get());
    }
    return nullptr;
  }

  JSString* lower = StringToLowerCase(cx, id);
  if (!lower) {
    return nullptr;
  }

  JSLinearString* linear = lower->ensureLinear(cx);
  if (!linear) {
    return nullptr;
  }

  if (!IsBuiltinCalendar(linear)) {
    if (auto chars = QuoteString(cx, id)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_CALENDAR_INVALID_ID, chars.get());
    }
    return nullptr;
  }

  return linear;
}

/**
 * CreateTemporalCalendar ( identifier [ , newTarget ] )
 */
static CalendarObject* CreateTemporalCalendar(
    JSContext* cx, const CallArgs& args, Handle<JSLinearString*> identifier) {
  // Step 1.
  MOZ_ASSERT(IsBuiltinCalendar(identifier));

  // Steps 2-3.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_Calendar, &proto)) {
    return nullptr;
  }

  auto* obj = NewObjectWithClassProto<CalendarObject>(cx, proto);
  if (!obj) {
    return nullptr;
  }

  // Step 4.
  obj->setFixedSlot(CalendarObject::IDENTIFIER_SLOT, StringValue(identifier));

  // Step 5.
  return obj;
}

static bool Calendar_toString(JSContext* cx, unsigned argc, Value* vp);

JSString* js::temporal::CalendarToString(JSContext* cx,
                                         Handle<JSObject*> calendar) {
  if (calendar->is<CalendarObject>() &&
      HasNoToPrimitiveMethodPure(calendar, cx) &&
      HasNativeMethodPure(calendar, cx->names().toString, Calendar_toString,
                          cx)) {
    JSString* id = calendar->as<CalendarObject>().identifier();
    MOZ_ASSERT(id);
    return id;
  }

  Rooted<Value> calendarValue(cx, ObjectValue(*calendar));
  return JS::ToString(cx, calendarValue);
}

/**
 * Temporal.Calendar ( id )
 */
static bool CalendarConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.Calendar")) {
    return false;
  }

  // Step 2.
  JSString* id = JS::ToString(cx, args.get(0));
  if (!id) {
    return false;
  }

  Rooted<JSLinearString*> linear(cx, id->ensureLinear(cx));
  if (!linear) {
    return false;
  }

  // Step 3.
  linear = ThrowIfNotBuiltinCalendar(cx, linear);
  if (!linear) {
    return false;
  }

  // Step 4.
  auto* calendar = CreateTemporalCalendar(cx, args, linear);
  if (!calendar) {
    return false;
  }

  args.rval().setObject(*calendar);
  return true;
}

/**
 * get Temporal.Calendar.prototype.id
 */
static bool Calendar_id(JSContext* cx, const CallArgs& args) {
  auto* calendar = &args.thisv().toObject().as<CalendarObject>();

  // Step 3.
  args.rval().setString(calendar->identifier());
  return true;
}

/**
 * get Temporal.Calendar.prototype.id
 */
static bool Calendar_id(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_id>(cx, args);
}

/**
 * Temporal.Calendar.prototype.toString ( )
 */
static bool Calendar_toString(JSContext* cx, const CallArgs& args) {
  auto* calendar = &args.thisv().toObject().as<CalendarObject>();

  // Step 3.
  args.rval().setString(calendar->identifier());
  return true;
}

/**
 * Temporal.Calendar.prototype.toString ( )
 */
static bool Calendar_toString(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_toString>(cx, args);
}

/**
 * Temporal.Calendar.prototype.toJSON ( )
 */
static bool Calendar_toJSON(JSContext* cx, const CallArgs& args) {
  Rooted<JSObject*> calendar(cx, &args.thisv().toObject());

  // Step 3.
  JSString* str = CalendarToString(cx, calendar);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.Calendar.prototype.toJSON ( )
 */
static bool Calendar_toJSON(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_toJSON>(cx, args);
}

const JSClass CalendarObject::class_ = {
    "Temporal.Calendar",
    JSCLASS_HAS_RESERVED_SLOTS(CalendarObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_Calendar),
    JS_NULL_CLASS_OPS,
    &CalendarObject::classSpec_,
};

const JSClass& CalendarObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec Calendar_methods[] = {
    JS_FS_END,
};

static const JSFunctionSpec Calendar_prototype_methods[] = {
    JS_FN("toString", Calendar_toString, 0, 0),
    JS_FN("toJSON", Calendar_toJSON, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec Calendar_prototype_properties[] = {
    JS_PSG("id", Calendar_id, 0),
    JS_STRING_SYM_PS(toStringTag, "Temporal.Calendar", JSPROP_READONLY),
    JS_PS_END,
};

const ClassSpec CalendarObject::classSpec_ = {
    GenericCreateConstructor<CalendarConstructor, 1, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<CalendarObject>,
    Calendar_methods,
    nullptr,
    Calendar_prototype_methods,
    Calendar_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};

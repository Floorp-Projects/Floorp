/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_Calendar_h
#define builtin_temporal_Calendar_h

#include "mozilla/Assertions.h"

#include <initializer_list>
#include <stdint.h>

#include "builtin/temporal/Wrapped.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/NativeObject.h"
#include "vm/StringType.h"

class JS_PUBLIC_API JSTracer;

namespace js {
struct ClassSpec;
class PlainObject;
}  // namespace js

namespace js::temporal {

class CalendarObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  static constexpr uint32_t IDENTIFIER_SLOT = 0;
  static constexpr uint32_t SLOT_COUNT = 1;

  JSLinearString* identifier() const {
    return &getFixedSlot(IDENTIFIER_SLOT).toString()->asLinear();
  }

 private:
  static const ClassSpec classSpec_;
};

/**
 * Calendar value, which is either a string containing a canonical calendar
 * identifier or an object.
 */
class CalendarValue final {
  JS::Value value_{};

 public:
  /**
   * Default initialize this CalendarValue.
   */
  CalendarValue() = default;

  /**
   * Default initialize this CalendarValue.
   */
  explicit CalendarValue(const JS::Value& value) : value_(value) {
    MOZ_ASSERT(value.isString() || value.isObject());
    MOZ_ASSERT_IF(value.isString(), value.toString()->isLinear());
  }

  /**
   * Initialize this CalendarValue with a canonical calendar identifier.
   */
  explicit CalendarValue(JSLinearString* calendarId)
      : value_(JS::StringValue(calendarId)) {}

  /**
   * Initialize this CalendarValue with a calendar object.
   */
  explicit CalendarValue(JSObject* calendar)
      : value_(JS::ObjectValue(*calendar)) {}

  /**
   * Return true iff this CalendarValue is initialized with either a canonical
   * calendar identifier or a calendar object.
   */
  explicit operator bool() const { return !value_.isUndefined(); }

  /**
   * Return this CalendarValue as a JS::Value.
   */
  JS::Value toValue() const { return value_; }

  /**
   * Return true if this CalendarValue is a string.
   */
  bool isString() const { return value_.isString(); }

  /**
   * Return true if this CalendarValue is an object.
   */
  bool isObject() const { return value_.isObject(); }

  /**
   * Return the calendar identifier.
   */
  JSLinearString* toString() const { return &value_.toString()->asLinear(); }

  /**
   * Return the calendar object.
   */
  JSObject* toObject() const { return &value_.toObject(); }

  void trace(JSTracer* trc);

  JS::Value* valueDoNotUse() { return &value_; }
  JS::Value const* valueDoNotUse() const { return &value_; }
};

struct Duration;
struct PlainDate;
struct PlainDateTime;
class DurationObject;
class PlainDateObject;
class PlainDateTimeObject;
class PlainMonthDayObject;
class PlainYearMonthObject;
enum class CalendarOption;
enum class TemporalUnit;

/**
 * ISODaysInYear ( year )
 */
int32_t ISODaysInYear(int32_t year);

/**
 * ISODaysInMonth ( year, month )
 */
int32_t ISODaysInMonth(int32_t year, int32_t month);

/**
 * ISODaysInMonth ( year, month )
 */
int32_t ISODaysInMonth(double year, int32_t month);

/**
 * ToISODayOfYear ( year, month, day )
 */
int32_t ToISODayOfYear(const PlainDate& date);

/**
 * 21.4.1.12 MakeDay ( year, month, date )
 */
int32_t MakeDay(const PlainDate& date);

/**
 * 21.4.1.13 MakeDate ( day, time )
 */
int64_t MakeDate(const PlainDateTime& dateTime);

/**
 * 21.4.1.13 MakeDate ( day, time )
 */
int64_t MakeDate(int32_t year, int32_t month, int32_t day);

/**
 * Return the case-normalized calendar identifier if |id| is a built-in calendar
 * identifier. Otherwise throws a RangeError.
 */
bool ToBuiltinCalendar(JSContext* cx, JS::Handle<JSString*> id,
                       JS::MutableHandle<CalendarValue> result);

/**
 * ToTemporalCalendarSlotValue ( temporalCalendarLike [ , default ] )
 */
bool ToTemporalCalendar(JSContext* cx,
                        JS::Handle<JS::Value> temporalCalendarLike,
                        JS::MutableHandle<CalendarValue> result);

/**
 * ToTemporalCalendarSlotValue ( temporalCalendarLike [ , default ] )
 */
bool ToTemporalCalendarWithISODefault(
    JSContext* cx, JS::Handle<JS::Value> temporalCalendarLike,
    JS::MutableHandle<CalendarValue> result);

/**
 * GetTemporalCalendarWithISODefault ( item )
 */
bool GetTemporalCalendarWithISODefault(JSContext* cx,
                                       JS::Handle<JSObject*> item,
                                       JS::MutableHandle<CalendarValue> result);

/**
 * ToTemporalCalendarIdentifier ( calendarSlotValue )
 */
JSString* ToTemporalCalendarIdentifier(JSContext* cx,
                                       JS::Handle<CalendarValue> calendar);

/**
 * ToTemporalCalendarObject ( calendarSlotValue )
 */
JSObject* ToTemporalCalendarObject(JSContext* cx,
                                   JS::Handle<CalendarValue> calendar);

enum class CalendarField {
  Year,
  Month,
  MonthCode,
  Day,
};

using CalendarFieldNames = JS::StackGCVector<JS::PropertyKey>;

/**
 * CalendarFields ( calendar, fieldNames )
 */
bool CalendarFields(JSContext* cx, JS::Handle<CalendarValue> calendar,
                    std::initializer_list<CalendarField> fieldNames,
                    JS::MutableHandle<CalendarFieldNames> result);

/**
 * CalendarMergeFields ( calendar, fields, additionalFields )
 */
JSObject* CalendarMergeFields(JSContext* cx, JS::Handle<CalendarValue> calendar,
                              JS::Handle<PlainObject*> fields,
                              JS::Handle<PlainObject*> additionalFields);

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
Wrapped<PlainDateObject*> CalendarDateAdd(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<Wrapped<PlainDateObject*>> date, const Duration& duration,
    JS::Handle<JSObject*> options);

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
Wrapped<PlainDateObject*> CalendarDateAdd(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<Wrapped<PlainDateObject*>> date, const Duration& duration,
    JS::Handle<JS::Value> dateAdd);

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
Wrapped<PlainDateObject*> CalendarDateAdd(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<Wrapped<PlainDateObject*>> date, const Duration& duration,
    JS::Handle<JSObject*> options, JS::Handle<JS::Value> dateAdd);

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
Wrapped<PlainDateObject*> CalendarDateAdd(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<Wrapped<PlainDateObject*>> date,
    JS::Handle<Wrapped<DurationObject*>> duration,
    JS::Handle<JS::Value> dateAdd);

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
Wrapped<PlainDateObject*> CalendarDateAdd(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<Wrapped<PlainDateObject*>> date,
    JS::Handle<Wrapped<DurationObject*>> duration,
    JS::Handle<JSObject*> options);

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
bool CalendarDateAdd(JSContext* cx, JS::Handle<CalendarValue> calendar,
                     const PlainDate& date, const Duration& duration,
                     PlainDate* result);

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
bool CalendarDateAdd(JSContext* cx, JS::Handle<CalendarValue> calendar,
                     const PlainDate& date, const Duration& duration,
                     JS::Handle<JSObject*> options, PlainDate* result);

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
bool CalendarDateAdd(JSContext* cx, JS::Handle<CalendarValue> calendar,
                     JS::Handle<Wrapped<PlainDateObject*>> date,
                     const Duration& duration, JS::Handle<JS::Value> dateAdd,
                     PlainDate* result);

/**
 * CalendarDateUntil ( calendar, one, two, options [ , dateUntil ] )
 */
bool CalendarDateUntil(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       JS::Handle<Wrapped<PlainDateObject*>> one,
                       JS::Handle<Wrapped<PlainDateObject*>> two,
                       TemporalUnit largestUnit,
                       JS::Handle<JS::Value> dateUntil, Duration* result);

/**
 * CalendarDateUntil ( calendar, one, two, options [ , dateUntil ] )
 */
bool CalendarDateUntil(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       JS::Handle<Wrapped<PlainDateObject*>> one,
                       JS::Handle<Wrapped<PlainDateObject*>> two,
                       JS::Handle<JSObject*> options, Duration* result);

/**
 * CalendarDateUntil ( calendar, one, two, options [ , dateUntil ] )
 */
bool CalendarDateUntil(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       JS::Handle<Wrapped<PlainDateObject*>> one,
                       JS::Handle<Wrapped<PlainDateObject*>> two,
                       TemporalUnit largestUnit, Duration* result);

/**
 * CalendarYear ( calendar, dateLike )
 */
bool CalendarYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                  JS::Handle<PlainDateObject*> dateLike,
                  JS::MutableHandle<JS::Value> result);

/**
 * CalendarYear ( calendar, dateLike )
 */
bool CalendarYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                  JS::Handle<PlainDateTimeObject*> dateLike,
                  JS::MutableHandle<JS::Value> result);

/**
 * CalendarYear ( calendar, dateLike )
 */
bool CalendarYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                  JS::Handle<PlainYearMonthObject*> dateLike,
                  JS::MutableHandle<JS::Value> result);

/**
 * CalendarYear ( calendar, dateLike )
 */
bool CalendarYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                  const PlainDateTime& dateTime,
                  JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonth ( calendar, dateLike )
 */
bool CalendarMonth(JSContext* cx, JS::Handle<CalendarValue> calendar,
                   JS::Handle<PlainDateObject*> dateLike,
                   JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonth ( calendar, dateLike )
 */
bool CalendarMonth(JSContext* cx, JS::Handle<CalendarValue> calendar,
                   JS::Handle<PlainDateTimeObject*> dateLike,
                   JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonth ( calendar, dateLike )
 */
bool CalendarMonth(JSContext* cx, JS::Handle<CalendarValue> calendar,
                   JS::Handle<PlainYearMonthObject*> dateLike,
                   JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonth ( calendar, dateLike )
 */
bool CalendarMonth(JSContext* cx, JS::Handle<CalendarValue> calendar,
                   const PlainDateTime& dateTime,
                   JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonthCode ( calendar, dateLike )
 */
bool CalendarMonthCode(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       JS::Handle<PlainDateObject*> dateLike,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonthCode ( calendar, dateLike )
 */
bool CalendarMonthCode(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       JS::Handle<PlainDateTimeObject*> dateLike,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonthCode ( calendar, dateLike )
 */
bool CalendarMonthCode(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       JS::Handle<PlainMonthDayObject*> dateLike,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonthCode ( calendar, dateLike )
 */
bool CalendarMonthCode(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       JS::Handle<PlainYearMonthObject*> dateLike,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonthCode ( calendar, dateLike )
 */
bool CalendarMonthCode(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       const PlainDateTime& dateTime,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarDay ( calendar, dateLike )
 */
bool CalendarDay(JSContext* cx, JS::Handle<CalendarValue> calendar,
                 JS::Handle<PlainDateObject*> dateLike,
                 JS::MutableHandle<JS::Value> result);

/**
 * CalendarDay ( calendar, dateLike )
 */
bool CalendarDay(JSContext* cx, JS::Handle<CalendarValue> calendar,
                 JS::Handle<PlainDateTimeObject*> dateLike,
                 JS::MutableHandle<JS::Value> result);

/**
 * CalendarDay ( calendar, dateLike )
 */
bool CalendarDay(JSContext* cx, JS::Handle<CalendarValue> calendar,
                 JS::Handle<PlainMonthDayObject*> dateLike,
                 JS::MutableHandle<JS::Value> result);

/**
 * CalendarDay ( calendar, dateLike )
 */
bool CalendarDay(JSContext* cx, JS::Handle<CalendarValue> calendar,
                 const PlainDateTime& dateTime,
                 JS::MutableHandle<JS::Value> result);

/**
 * CalendarDay ( calendar, dateLike )
 */
bool CalendarDayWrapped(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        JS::Handle<Wrapped<PlainDateObject*>> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarDayOfWeek ( calendar, dateLike )
 */
bool CalendarDayOfWeek(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       JS::Handle<PlainDateObject*> dateLike,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarDayOfWeek ( calendar, dateLike )
 */
bool CalendarDayOfWeek(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       JS::Handle<PlainDateTimeObject*> dateLike,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarDayOfWeek ( calendar, dateLike )
 */
bool CalendarDayOfWeek(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       const PlainDateTime& dateTime,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarDayOfYear ( calendar, dateLike )
 */
bool CalendarDayOfYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       JS::Handle<PlainDateObject*> dateLike,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarDayOfYear ( calendar, dateLike )
 */
bool CalendarDayOfYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       JS::Handle<PlainDateTimeObject*> dateLike,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarDayOfYear ( calendar, dateLike )
 */
bool CalendarDayOfYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       const PlainDateTime& dateTime,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarWeekOfYear ( calendar, dateLike )
 */
bool CalendarWeekOfYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        JS::Handle<PlainDateObject*> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarWeekOfYear ( calendar, dateLike )
 */
bool CalendarWeekOfYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        JS::Handle<PlainDateTimeObject*> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarWeekOfYear ( calendar, dateLike )
 */
bool CalendarWeekOfYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        const PlainDateTime& dateTime,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarYearOfWeek ( calendar, dateLike )
 */
bool CalendarYearOfWeek(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        JS::Handle<PlainDateObject*> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarYearOfWeek ( calendar, dateLike )
 */
bool CalendarYearOfWeek(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        JS::Handle<PlainDateTimeObject*> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarYearOfWeek ( calendar, dateLike )
 */
bool CalendarYearOfWeek(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        const PlainDateTime& dateTime,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInWeek ( calendar, dateLike )
 */
bool CalendarDaysInWeek(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        JS::Handle<PlainDateObject*> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInWeek ( calendar, dateLike )
 */
bool CalendarDaysInWeek(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        JS::Handle<PlainDateTimeObject*> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInWeek ( calendar, dateLike )
 */
bool CalendarDaysInWeek(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        const PlainDateTime& dateTime,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInMonth ( calendar, dateLike )
 */
bool CalendarDaysInMonth(JSContext* cx, JS::Handle<CalendarValue> calendar,
                         JS::Handle<PlainDateObject*> dateLike,
                         JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInMonth ( calendar, dateLike )
 */
bool CalendarDaysInMonth(JSContext* cx, JS::Handle<CalendarValue> calendar,
                         JS::Handle<PlainDateTimeObject*> dateLike,
                         JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInMonth ( calendar, dateLike )
 */
bool CalendarDaysInMonth(JSContext* cx, JS::Handle<CalendarValue> calendar,
                         JS::Handle<PlainYearMonthObject*> dateLike,
                         JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInMonth ( calendar, dateLike )
 */
bool CalendarDaysInMonth(JSContext* cx, JS::Handle<CalendarValue> calendar,
                         const PlainDateTime& dateTime,
                         JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInYear ( calendar, dateLike )
 */
bool CalendarDaysInYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        JS::Handle<PlainDateObject*> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInYear ( calendar, dateLike )
 */
bool CalendarDaysInYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        JS::Handle<PlainDateTimeObject*> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInYear ( calendar, dateLike )
 */
bool CalendarDaysInYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        JS::Handle<PlainYearMonthObject*> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInYear ( calendar, dateLike )
 */
bool CalendarDaysInYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        const PlainDateTime& dateTime,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonthsInYear ( calendar, dateLike )
 */
bool CalendarMonthsInYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                          JS::Handle<PlainDateObject*> dateLike,
                          JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonthsInYear ( calendar, dateLike )
 */
bool CalendarMonthsInYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                          JS::Handle<PlainDateTimeObject*> dateLike,
                          JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonthsInYear ( calendar, dateLike )
 */
bool CalendarMonthsInYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                          JS::Handle<PlainYearMonthObject*> dateLike,
                          JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonthsInYear ( calendar, dateLike )
 */
bool CalendarMonthsInYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                          const PlainDateTime& dateTime,
                          JS::MutableHandle<JS::Value> result);

/**
 * CalendarInLeapYear ( calendar, dateLike )
 */
bool CalendarInLeapYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        JS::Handle<PlainDateObject*> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarInLeapYear ( calendar, dateLike )
 */
bool CalendarInLeapYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        JS::Handle<PlainDateTimeObject*> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarInLeapYear ( calendar, dateLike )
 */
bool CalendarInLeapYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        JS::Handle<PlainYearMonthObject*> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarInLeapYear ( calendar, dateLike )
 */
bool CalendarInLeapYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        const PlainDateTime& dateTime,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarDateFromFields ( calendar, fields [ , options ] )
 */
Wrapped<PlainDateObject*> CalendarDateFromFields(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<JSObject*> fields);

/**
 * CalendarDateFromFields ( calendar, fields [ , options ] )
 */
Wrapped<PlainDateObject*> CalendarDateFromFields(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<JSObject*> fields, JS::Handle<JSObject*> options);

/**
 * CalendarYearMonthFromFields ( calendar, fields [ , options ] )
 */
Wrapped<PlainYearMonthObject*> CalendarYearMonthFromFields(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<JSObject*> fields);

/**
 * CalendarYearMonthFromFields ( calendar, fields [ , options ] )
 */
Wrapped<PlainYearMonthObject*> CalendarYearMonthFromFields(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<JSObject*> fields, JS::Handle<JSObject*> options);

/**
 * CalendarMonthDayFromFields ( calendar, fields [ , options ] )
 */
Wrapped<PlainMonthDayObject*> CalendarMonthDayFromFields(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<JSObject*> fields);

/**
 * CalendarMonthDayFromFields ( calendar, fields [ , options ] )
 */
Wrapped<PlainMonthDayObject*> CalendarMonthDayFromFields(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<JSObject*> fields, JS::Handle<JSObject*> options);

/**
 * CalendarEquals ( one, two )
 */
bool CalendarEquals(JSContext* cx, JS::Handle<CalendarValue> one,
                    JS::Handle<CalendarValue> two, bool* equals);

/**
 * CalendarEquals ( one, two )
 */
bool CalendarEqualsOrThrow(JSContext* cx, JS::Handle<CalendarValue> one,
                           JS::Handle<CalendarValue> two);

/**
 * ConsolidateCalendars ( one, two )
 */
bool ConsolidateCalendars(JSContext* cx, JS::Handle<CalendarValue> one,
                          JS::Handle<CalendarValue> two,
                          JS::MutableHandle<CalendarValue> result);

/**
 * Return true when accessing the calendar fields |fieldNames| can be optimized.
 * Otherwise returns false.
 */
bool IsBuiltinAccess(JSContext* cx, JS::Handle<CalendarObject*> calendar,
                     std::initializer_list<CalendarField> fieldNames);

/**
 * Return true when accessing the calendar fields can be optimized.
 * Otherwise returns false.
 */
bool IsBuiltinAccessForStringCalendar(JSContext* cx);

// Helper for MutableWrappedPtrOperations.
bool WrapCalendarValue(JSContext* cx, JS::MutableHandle<JS::Value> calendar);

} /* namespace js::temporal */

namespace js {

template <typename Wrapper>
class WrappedPtrOperations<temporal::CalendarValue, Wrapper> {
  const auto& container() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  explicit operator bool() const { return bool(container()); }

  JS::Handle<JS::Value> toValue() const {
    return JS::Handle<JS::Value>::fromMarkedLocation(
        container().valueDoNotUse());
  }

  bool isString() const { return container().isString(); }

  bool isObject() const { return container().isObject(); }

  JSLinearString* toString() const { return container().toString(); }

  JSObject* toObject() const { return container().toObject(); }
};

template <typename Wrapper>
class MutableWrappedPtrOperations<temporal::CalendarValue, Wrapper>
    : public WrappedPtrOperations<temporal::CalendarValue, Wrapper> {
  auto& container() { return static_cast<Wrapper*>(this)->get(); }

  JS::MutableHandle<JS::Value> toMutableValue() {
    return JS::MutableHandle<JS::Value>::fromMarkedLocation(
        container().valueDoNotUse());
  }

 public:
  bool wrap(JSContext* cx) {
    return temporal::WrapCalendarValue(cx, toMutableValue());
  }
};

} /* namespace js */

#endif /* builtin_temporal_Calendar_h */

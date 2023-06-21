/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_Calendar_h
#define builtin_temporal_Calendar_h

#include <stdint.h>

#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/NativeObject.h"

namespace js {
struct ClassSpec;
class JSStringBuilder;
}  // namespace js

namespace js::temporal {

class CalendarObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  static constexpr uint32_t IDENTIFIER_SLOT = 0;
  static constexpr uint32_t SLOT_COUNT = 1;

  JSString* identifier() const {
    return getFixedSlot(IDENTIFIER_SLOT).toString();
  }

 private:
  static const ClassSpec classSpec_;
};

struct PlainDate;
struct PlainDateTime;
enum class CalendarOption;

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
 * GetISO8601Calendar ( )
 */
CalendarObject* GetISO8601Calendar(JSContext* cx);

/**
 * ToTemporalCalendar ( temporalCalendarLike )
 */
JSObject* ToTemporalCalendar(JSContext* cx,
                             JS::Handle<JS::Value> temporalCalendarLike);

/**
 * ToTemporalCalendarWithISODefault ( temporalCalendarLike )
 */
JSObject* ToTemporalCalendarWithISODefault(
    JSContext* cx, JS::Handle<JS::Value> temporalCalendarLike);

/**
 * GetTemporalCalendarWithISODefault ( item )
 */
JSObject* GetTemporalCalendarWithISODefault(JSContext* cx,
                                            JS::Handle<JSObject*> item);

/**
 * CalendarYear ( calendar, dateLike )
 */
bool CalendarYear(JSContext* cx, JS::Handle<JSObject*> calendar,
                  JS::Handle<JS::Value> dateLike,
                  JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonth ( calendar, dateLike )
 */
bool CalendarMonth(JSContext* cx, JS::Handle<JSObject*> calendar,
                   JS::Handle<JS::Value> dateLike,
                   JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonthCode ( calendar, dateLike )
 */
bool CalendarMonthCode(JSContext* cx, JS::Handle<JSObject*> calendar,
                       JS::Handle<JS::Value> dateLike,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarDay ( calendar, dateLike )
 */
bool CalendarDay(JSContext* cx, JS::Handle<JSObject*> calendar,
                 JS::Handle<JS::Value> dateLike,
                 JS::MutableHandle<JS::Value> result);

/**
 * CalendarDayOfWeek ( calendar, dateLike )
 */
bool CalendarDayOfWeek(JSContext* cx, JS::Handle<JSObject*> calendar,
                       JS::Handle<JS::Value> dateLike,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarDayOfYear ( calendar, dateLike )
 */
bool CalendarDayOfYear(JSContext* cx, JS::Handle<JSObject*> calendar,
                       JS::Handle<JS::Value> dateLike,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarWeekOfYear ( calendar, dateLike )
 */
bool CalendarWeekOfYear(JSContext* cx, JS::Handle<JSObject*> calendar,
                        JS::Handle<JS::Value> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarYearOfWeek ( calendar, dateLike )
 */
bool CalendarYearOfWeek(JSContext* cx, JS::Handle<JSObject*> calendar,
                        JS::Handle<JS::Value> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInWeek ( calendar, dateLike )
 */
bool CalendarDaysInWeek(JSContext* cx, JS::Handle<JSObject*> calendar,
                        JS::Handle<JS::Value> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInMonth ( calendar, dateLike )
 */
bool CalendarDaysInMonth(JSContext* cx, JS::Handle<JSObject*> calendar,
                         JS::Handle<JS::Value> dateLike,
                         JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInYear ( calendar, dateLike )
 */
bool CalendarDaysInYear(JSContext* cx, JS::Handle<JSObject*> calendar,
                        JS::Handle<JS::Value> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonthsInYear ( calendar, dateLike )
 */
bool CalendarMonthsInYear(JSContext* cx, JS::Handle<JSObject*> calendar,
                          JS::Handle<JS::Value> dateLike,
                          JS::MutableHandle<JS::Value> result);

/**
 * CalendarInLeapYear ( calendar, dateLike )
 */
bool CalendarInLeapYear(JSContext* cx, JS::Handle<JSObject*> calendar,
                        JS::Handle<JS::Value> dateLike,
                        JS::MutableHandle<JS::Value> result);

/**
 * MaybeFormatCalendarAnnotation ( calendarObject, showCalendar )
 */
bool MaybeFormatCalendarAnnotation(JSContext* cx, JSStringBuilder& result,
                                   JS::Handle<JSObject*> calendarObject,
                                   CalendarOption showCalendar);

/**
 * FormatCalendarAnnotation ( id, showCalendar )
 */
bool FormatCalendarAnnotation(JSContext* cx, JSStringBuilder& result,
                              JS::Handle<JSString*> id,
                              CalendarOption showCalendar);

/**
 * Perform `ToString(calendar)` with an optimization when the built-in
 * Temporal.Calendar.prototype.toString method is called.
 */
JSString* CalendarToString(JSContext* cx, JS::Handle<JSObject*> calendar);

} /* namespace js::temporal */

#endif /* builtin_temporal_Calendar_h */

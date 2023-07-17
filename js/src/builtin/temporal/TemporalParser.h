/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_TemporalParser_h
#define builtin_temporal_TemporalParser_h

#include <stdint.h>

#include "js/TypeDecls.h"

class JSLinearString;

namespace js::temporal {

struct Duration;
struct PlainDate;
struct PlainDateTime;
struct PlainTime;

/**
 * ParseTemporalInstantString ( isoString )
 */
bool ParseTemporalInstantString(JSContext* cx, JS::Handle<JSString*> str,
                                PlainDateTime* result, int64_t* offset);

/**
 * ParseTemporalTimeZoneString ( isoString )
 */
bool ParseTemporalTimeZoneString(JSContext* cx, JS::Handle<JSString*> str,
                                 JS::MutableHandle<JSString*> timeZoneName,
                                 int64_t* offsetNanoseconds);

/**
 * ParseTimeZoneOffsetString ( isoString )
 */
bool ParseTimeZoneOffsetString(JSContext* cx, JS::Handle<JSString*> str,
                               int64_t* result);

/**
 * ParseTemporalDurationString ( isoString )
 */
bool ParseTemporalDurationString(JSContext* cx, JS::Handle<JSString*> str,
                                 Duration* result);

/**
 * ParseTemporalCalendarString ( isoString )
 */
JSLinearString* ParseTemporalCalendarString(JSContext* cx,
                                            JS::Handle<JSString*> str);

/**
 * ParseTemporalTimeString ( isoString )
 */
bool ParseTemporalTimeString(JSContext* cx, JS::Handle<JSString*> str,
                             PlainTime* result);

/**
 * ParseTemporalDateString ( isoString )
 */
bool ParseTemporalDateString(JSContext* cx, JS::Handle<JSString*> str,
                             PlainDate* result,
                             JS::MutableHandle<JSString*> calendar);

/**
 * ParseTemporalMonthDayString ( isoString )
 */
bool ParseTemporalMonthDayString(JSContext* cx, JS::Handle<JSString*> str,
                                 PlainDate* result, bool* hasYear,
                                 JS::MutableHandle<JSString*> calendar);

/**
 * ParseTemporalYearMonthString ( isoString )
 */
bool ParseTemporalYearMonthString(JSContext* cx, JS::Handle<JSString*> str,
                                  PlainDate* result,
                                  JS::MutableHandle<JSString*> calendar);

/**
 * ParseTemporalDateTimeString ( isoString )
 */
bool ParseTemporalDateTimeString(JSContext* cx, JS::Handle<JSString*> str,
                                 PlainDateTime* result,
                                 JS::MutableHandle<JSString*> calendar);

/**
 * ParseTemporalZonedDateTimeString ( isoString )
 */
bool ParseTemporalZonedDateTimeString(JSContext* cx, JS::Handle<JSString*> str,
                                      PlainDateTime* dateTime, bool* isUTC,
                                      bool* hasOffset, int64_t* timeZoneOffset,
                                      JS::MutableHandle<JSString*> timeZoneName,
                                      JS::MutableHandle<JSString*> calendar);

/**
 * ParseTemporalRelativeToString ( isoString )
 */
bool ParseTemporalRelativeToString(JSContext* cx, JS::Handle<JSString*> str,
                                   PlainDateTime* dateTime, bool* isUTC,
                                   bool* hasOffset, int64_t* timeZoneOffset,
                                   JS::MutableHandle<JSString*> timeZoneName,
                                   JS::MutableHandle<JSString*> calendar);

} /* namespace js::temporal */

#endif /* builtin_temporal_TemporalParser_h */

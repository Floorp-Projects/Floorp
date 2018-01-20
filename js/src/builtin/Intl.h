/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Intl_h
#define builtin_Intl_h

#include "mozilla/HashFunctions.h"
#include "mozilla/MemoryReporting.h"

#include "jsalloc.h"
#include "NamespaceImports.h"

#include "builtin/SelfHostingDefines.h"
#include "js/Class.h"
#include "js/GCHashTable.h"
#include "vm/NativeObject.h"

class JSLinearString;

/*
 * The Intl module specified by standard ECMA-402,
 * ECMAScript Internationalization API Specification.
 */

namespace js {

class FreeOp;

/**
 * Initializes the Intl Object and its standard built-in properties.
 * Spec: ECMAScript Internationalization API Specification, 8.0, 8.1
 */
extern JSObject*
InitIntlClass(JSContext* cx, HandleObject obj);

/*
 * The following functions are for use by self-hosted code.
 */


/******************** DateTimeFormat ********************/

class DateTimeFormatObject : public NativeObject
{
  public:
    static const Class class_;

    static constexpr uint32_t INTERNALS_SLOT = 0;
    static constexpr uint32_t UDATE_FORMAT_SLOT = 1;
    static constexpr uint32_t SLOT_COUNT = 2;

    static_assert(INTERNALS_SLOT == INTL_INTERNALS_OBJECT_SLOT,
                  "INTERNALS_SLOT must match self-hosting define for internals object slot");

  private:
    static const ClassOps classOps_;

    static void finalize(FreeOp* fop, JSObject* obj);
};

/**
 * Returns a new instance of the standard built-in DateTimeFormat constructor.
 * Self-hosted code cannot cache this constructor (as it does for others in
 * Utilities.js) because it is initialized after self-hosted code is compiled.
 *
 * Usage: dateTimeFormat = intl_DateTimeFormat(locales, options)
 */
extern MOZ_MUST_USE bool
intl_DateTimeFormat(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns an object indicating the supported locales for date and time
 * formatting by having a true-valued property for each such locale with the
 * canonicalized language tag as the property name. The object has no
 * prototype.
 *
 * Usage: availableLocales = intl_DateTimeFormat_availableLocales()
 */
extern MOZ_MUST_USE bool
intl_DateTimeFormat_availableLocales(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns an array with the calendar type identifiers per Unicode
 * Technical Standard 35, Unicode Locale Data Markup Language, for the
 * supported calendars for the given locale. The default calendar is
 * element 0.
 *
 * Usage: calendars = intl_availableCalendars(locale)
 */
extern MOZ_MUST_USE bool
intl_availableCalendars(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns the calendar type identifier per Unicode Technical Standard 35,
 * Unicode Locale Data Markup Language, for the default calendar for the given
 * locale.
 *
 * Usage: calendar = intl_defaultCalendar(locale)
 */
extern MOZ_MUST_USE bool
intl_defaultCalendar(JSContext* cx, unsigned argc, Value* vp);

/**
 * 6.4.1 IsValidTimeZoneName ( timeZone )
 *
 * Verifies that the given string is a valid time zone name. If it is a valid
 * time zone name, its IANA time zone name is returned. Otherwise returns null.
 *
 * ES2017 Intl draft rev 4a23f407336d382ed5e3471200c690c9b020b5f3
 *
 * Usage: ianaTimeZone = intl_IsValidTimeZoneName(timeZone)
 */
extern MOZ_MUST_USE bool
intl_IsValidTimeZoneName(JSContext* cx, unsigned argc, Value* vp);

/**
 * Return the canonicalized time zone name. Canonicalization resolves link
 * names to their target time zones.
 *
 * Usage: ianaTimeZone = intl_canonicalizeTimeZone(timeZone)
 */
extern MOZ_MUST_USE bool
intl_canonicalizeTimeZone(JSContext* cx, unsigned argc, Value* vp);

/**
 * Return the default time zone name. The time zone name is not canonicalized.
 *
 * Usage: icuDefaultTimeZone = intl_defaultTimeZone()
 */
extern MOZ_MUST_USE bool
intl_defaultTimeZone(JSContext* cx, unsigned argc, Value* vp);

/**
 * Return the raw offset from GMT in milliseconds for the default time zone.
 *
 * Usage: defaultTimeZoneOffset = intl_defaultTimeZoneOffset()
 */
extern MOZ_MUST_USE bool
intl_defaultTimeZoneOffset(JSContext* cx, unsigned argc, Value* vp);

/**
 * Return true if the given string is the default time zone as returned by
 * intl_defaultTimeZone(). Otherwise return false.
 *
 * Usage: isIcuDefaultTimeZone = intl_isDefaultTimeZone(icuDefaultTimeZone)
 */
extern MOZ_MUST_USE bool
intl_isDefaultTimeZone(JSContext* cx, unsigned argc, Value* vp);

/**
 * Return a pattern in the date-time format pattern language of Unicode
 * Technical Standard 35, Unicode Locale Data Markup Language, for the
 * best-fit date-time format pattern corresponding to skeleton for the
 * given locale.
 *
 * Usage: pattern = intl_patternForSkeleton(locale, skeleton)
 */
extern MOZ_MUST_USE bool
intl_patternForSkeleton(JSContext* cx, unsigned argc, Value* vp);

/**
 * Return a pattern in the date-time format pattern language of Unicode
 * Technical Standard 35, Unicode Locale Data Markup Language, for the
 * best-fit date-time style for the given locale.
 * The function takes four arguments:
 *
 *   locale
 *     BCP47 compliant locale string
 *   dateStyle
 *     A string with values: full or long or medium or short, or `undefined`
 *   timeStyle
 *     A string with values: full or long or medium or short, or `undefined`
 *   timeZone
 *     IANA time zone name
 *
 * Date and time style categories map to CLDR time/date standard
 * format patterns.
 *
 * For the definition of a pattern string, see LDML 4.8:
 * http://unicode.org/reports/tr35/tr35-dates.html#Date_Format_Patterns
 *
 * If `undefined` is passed to `dateStyle` or `timeStyle`, the respective
 * portions of the pattern will not be included in the result.
 *
 * Usage: pattern = intl_patternForStyle(locale, dateStyle, timeStyle, timeZone)
 */
extern MOZ_MUST_USE bool
intl_patternForStyle(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns a String value representing x (which must be a Number value)
 * according to the effective locale and the formatting options of the
 * given DateTimeFormat.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.3.2.
 *
 * Usage: formatted = intl_FormatDateTime(dateTimeFormat, x, formatToParts)
 */
extern MOZ_MUST_USE bool
intl_FormatDateTime(JSContext* cx, unsigned argc, Value* vp);


/******************** PluralRules ********************/

class PluralRulesObject : public NativeObject
{
  public:
    static const Class class_;

    static constexpr uint32_t INTERNALS_SLOT = 0;
    static constexpr uint32_t UPLURAL_RULES_SLOT = 1;
    static constexpr uint32_t SLOT_COUNT = 2;

    static_assert(INTERNALS_SLOT == INTL_INTERNALS_OBJECT_SLOT,
                  "INTERNALS_SLOT must match self-hosting define for internals object slot");

  private:
    static const ClassOps classOps_;

    static void finalize(FreeOp* fop, JSObject* obj);
};

/**
 * Returns an object indicating the supported locales for plural rules
 * by having a true-valued property for each such locale with the
 * canonicalized language tag as the property name. The object has no
 * prototype.
 *
 * Usage: availableLocales = intl_PluralRules_availableLocales()
 */
extern MOZ_MUST_USE bool
intl_PluralRules_availableLocales(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns a plural rule for the number x according to the effective
 * locale and the formatting options of the given PluralRules.
 *
 * A plural rule is a grammatical category that expresses count distinctions
 * (such as "one", "two", "few" etc.).
 *
 * Usage: rule = intl_SelectPluralRule(pluralRules, x)
 */
extern MOZ_MUST_USE bool
intl_SelectPluralRule(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns an array of plural rules categories for a given
 * locale and type.
 *
 * Usage: categories = intl_GetPluralCategories(locale, type)
 *
 * Example:
 *
 * intl_getPluralCategories('pl', 'cardinal'); // ['one', 'few', 'many', 'other']
 */
extern MOZ_MUST_USE bool
intl_GetPluralCategories(JSContext* cx, unsigned argc, Value* vp);

/******************** RelativeTimeFormat ********************/

class RelativeTimeFormatObject : public NativeObject
{
  public:
    static const Class class_;

    static constexpr uint32_t INTERNALS_SLOT = 0;
    static constexpr uint32_t URELATIVE_TIME_FORMAT_SLOT = 1;
    static constexpr uint32_t SLOT_COUNT = 2;

    static_assert(INTERNALS_SLOT == INTL_INTERNALS_OBJECT_SLOT,
                  "INTERNALS_SLOT must match self-hosting define for internals object slot");

  private:
    static const ClassOps classOps_;

    static void finalize(FreeOp* fop, JSObject* obj);
};

/**
 * Returns an object indicating the supported locales for relative time format
 * by having a true-valued property for each such locale with the
 * canonicalized language tag as the property name. The object has no
 * prototype.
 *
 * Usage: availableLocales = intl_RelativeTimeFormat_availableLocales()
 */
extern MOZ_MUST_USE bool
intl_RelativeTimeFormat_availableLocales(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns a relative time as a string formatted according to the effective
 * locale and the formatting options of the given RelativeTimeFormat.
 *
 * t should be a number representing a number to be formatted.
 * unit should be "second", "minute", "hour", "day", "week", "month", "quarter", or "year".
 *
 * Usage: formatted = intl_FormatRelativeTime(relativeTimeFormat, t, unit)
 */
extern MOZ_MUST_USE bool
intl_FormatRelativeTime(JSContext* cx, unsigned argc, Value* vp);

/******************** Intl ********************/

/**
 * Returns a plain object with calendar information for a single valid locale
 * (callers must perform this validation).  The object will have these
 * properties:
 *
 *   firstDayOfWeek
 *     an integer in the range 1=Sunday to 7=Saturday indicating the day
 *     considered the first day of the week in calendars, e.g. 1 for en-US,
 *     2 for en-GB, 1 for bn-IN
 *   minDays
 *     an integer in the range of 1 to 7 indicating the minimum number
 *     of days required in the first week of the year, e.g. 1 for en-US, 4 for de
 *   weekendStart
 *     an integer in the range 1=Sunday to 7=Saturday indicating the day
 *     considered the beginning of a weekend, e.g. 7 for en-US, 7 for en-GB,
 *     1 for bn-IN
 *   weekendEnd
 *     an integer in the range 1=Sunday to 7=Saturday indicating the day
 *     considered the end of a weekend, e.g. 1 for en-US, 1 for en-GB,
 *     1 for bn-IN (note that "weekend" is *not* necessarily two days)
 *
 * NOTE: "calendar" and "locale" properties are *not* added to the object.
 */
extern MOZ_MUST_USE bool
intl_GetCalendarInfo(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns a plain object with locale information for a single valid locale
 * (callers must perform this validation).  The object will have these
 * properties:
 *
 *   direction
 *     a string with a value "ltr" for left-to-right locale, and "rtl" for
 *     right-to-left locale.
 *   locale
 *     a BCP47 compilant locale string for the resolved locale.
 */
extern MOZ_MUST_USE bool
intl_GetLocaleInfo(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns an Array with CLDR-based fields display names.
 * The function takes three arguments:
 *
 *   locale
 *     BCP47 compliant locale string
 *   style
 *     A string with values: long or short or narrow
 *   keys
 *     An array or path-like strings that identify keys to be returned
 *     At the moment the following types of keys are supported:
 *
 *       'dates/fields/{year|month|week|day}'
 *       'dates/gregorian/months/{january|...|december}'
 *       'dates/gregorian/weekdays/{sunday|...|saturday}'
 *       'dates/gregorian/dayperiods/{am|pm}'
 *
 * Example:
 *
 * let info = intl_ComputeDisplayNames(
 *   'en-US',
 *   'long',
 *   [
 *     'dates/fields/year',
 *     'dates/gregorian/months/january',
 *     'dates/gregorian/weekdays/monday',
 *     'dates/gregorian/dayperiods/am',
 *   ]
 * );
 *
 * Returned value:
 *
 * [
 *   'year',
 *   'January',
 *   'Monday',
 *   'AM'
 * ]
 */
extern MOZ_MUST_USE bool
intl_ComputeDisplayNames(JSContext* cx, unsigned argc, Value* vp);


/******************** String ********************/

/**
 * Returns the input string converted to lower case based on the language
 * specific case mappings for the input locale.
 *
 * Usage: lowerCase = intl_toLocaleLowerCase(string, locale)
 */
extern MOZ_MUST_USE bool
intl_toLocaleLowerCase(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns the input string converted to upper case based on the language
 * specific case mappings for the input locale.
 *
 * Usage: upperCase = intl_toLocaleUpperCase(string, locale)
 */
extern MOZ_MUST_USE bool
intl_toLocaleUpperCase(JSContext* cx, unsigned argc, Value* vp);


} // namespace js

#endif /* builtin_Intl_h */

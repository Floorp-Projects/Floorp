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

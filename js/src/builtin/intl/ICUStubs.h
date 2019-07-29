/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * ICU types and declarations used by the Intl implementation, with fallbacks
 * for when the engine is being built without Intl support that nonetheless
 * allow our Intl implementation to still compile (even tho they should never
 * be called).
 *
 * Signatures adapted from ICU header files locid.h, numsys.h, ucal.h, ucol.h,
 * udat.h, udatpg.h, uenum.h, unum.h, uloc.h; see the ICU directory for
 * license.
 */

#ifndef builtin_intl_ICUStubs_h
#define builtin_intl_ICUStubs_h

#include "mozilla/Assertions.h"
#include "mozilla/TypeTraits.h"

#if ENABLE_INTL_API
#  include "unicode/ucal.h"
#  include "unicode/ucol.h"
#  include "unicode/udat.h"
#  include "unicode/udata.h"
#  include "unicode/udatpg.h"
#  include "unicode/udisplaycontext.h"
#  include "unicode/uenum.h"
#  include "unicode/uformattedvalue.h"
#  include "unicode/uloc.h"
#  include "unicode/unum.h"
#  include "unicode/unumberformatter.h"
#  include "unicode/unumsys.h"
#  include "unicode/upluralrules.h"
#  include "unicode/ureldatefmt.h"
#  include "unicode/ures.h"
#  include "unicode/ustring.h"
#  include "unicode/utypes.h"
#endif

/*
 * Pervasive note: ICU functions taking a UErrorCode in/out parameter always
 * test that parameter before doing anything, and will return immediately if
 * the value indicates that a failure occurred in a prior ICU call,
 * without doing anything else. See
 * http://userguide.icu-project.org/design#TOC-Error-Handling
 */

#if !ENABLE_INTL_API

enum UErrorCode {
  U_ZERO_ERROR,
  U_BUFFER_OVERFLOW_ERROR,
};

using UBool = bool;
using UChar = char16_t;
using UDate = double;

inline UBool U_FAILURE(UErrorCode code) {
  MOZ_CRASH("U_FAILURE: Intl API disabled");
}

inline const char* uloc_getAvailable(int32_t n) {
  MOZ_CRASH("uloc_getAvailable: Intl API disabled");
}

inline int32_t uloc_countAvailable() {
  MOZ_CRASH("uloc_countAvailable: Intl API disabled");
}

inline UBool uloc_isRightToLeft(const char* locale) {
  MOZ_CRASH("uloc_isRightToLeft: Intl API disabled");
}

inline int32_t uloc_addLikelySubtags(const char* localeID,
                                     char* maximizedLocaleID,
                                     int32_t maximizedLocaleIDCapacity,
                                     UErrorCode* err) {
  MOZ_CRASH("uloc_addLikelySubtags: Intl API disabled");
}

inline int32_t uloc_minimizeSubtags(const char* localeID,
                                    char* minimizedLocaleID,
                                    int32_t minimizedLocaleIDCapacity,
                                    UErrorCode* err) {
  MOZ_CRASH("uloc_minimizeSubtags: Intl API disabled");
}

struct UEnumeration;

inline int32_t uenum_count(UEnumeration* en, UErrorCode* status) {
  MOZ_CRASH("uenum_count: Intl API disabled");
}

inline const char* uenum_next(UEnumeration* en, int32_t* resultLength,
                              UErrorCode* status) {
  MOZ_CRASH("uenum_next: Intl API disabled");
}

inline void uenum_close(UEnumeration* en) {
  MOZ_CRASH("uenum_close: Intl API disabled");
}

struct UCollator;

enum UColAttribute {
  UCOL_ALTERNATE_HANDLING,
  UCOL_CASE_FIRST,
  UCOL_CASE_LEVEL,
  UCOL_NORMALIZATION_MODE,
  UCOL_STRENGTH,
  UCOL_NUMERIC_COLLATION,
};

enum UColAttributeValue {
  UCOL_DEFAULT = -1,
  UCOL_PRIMARY = 0,
  UCOL_SECONDARY = 1,
  UCOL_TERTIARY = 2,
  UCOL_OFF = 16,
  UCOL_ON = 17,
  UCOL_SHIFTED = 20,
  UCOL_LOWER_FIRST = 24,
  UCOL_UPPER_FIRST = 25,
};

enum UCollationResult { UCOL_EQUAL = 0, UCOL_GREATER = 1, UCOL_LESS = -1 };

inline int32_t ucol_countAvailable() {
  MOZ_CRASH("ucol_countAvailable: Intl API disabled");
}

inline const char* ucol_getAvailable(int32_t localeIndex) {
  MOZ_CRASH("ucol_getAvailable: Intl API disabled");
}

inline UEnumeration* ucol_openAvailableLocales(UErrorCode* status) {
  MOZ_CRASH("ucol_openAvailableLocales: Intl API disabled");
}

inline UCollator* ucol_open(const char* loc, UErrorCode* status) {
  MOZ_CRASH("ucol_open: Intl API disabled");
}

inline UColAttributeValue ucol_getAttribute(const UCollator* coll,
                                            UColAttribute attr,
                                            UErrorCode* status) {
  MOZ_CRASH("ucol_getAttribute: Intl API disabled");
}

inline void ucol_setAttribute(UCollator* coll, UColAttribute attr,
                              UColAttributeValue value, UErrorCode* status) {
  MOZ_CRASH("ucol_setAttribute: Intl API disabled");
}

inline UCollationResult ucol_strcoll(const UCollator* coll, const UChar* source,
                                     int32_t sourceLength, const UChar* target,
                                     int32_t targetLength) {
  MOZ_CRASH("ucol_strcoll: Intl API disabled");
}

inline void ucol_close(UCollator* coll) {
  MOZ_CRASH("ucol_close: Intl API disabled");
}

inline UEnumeration* ucol_getKeywordValuesForLocale(const char* key,
                                                    const char* locale,
                                                    UBool commonlyUsed,
                                                    UErrorCode* status) {
  MOZ_CRASH("ucol_getKeywordValuesForLocale: Intl API disabled");
}

struct UParseError;
struct UFieldPosition;
struct UFieldPositionIterator;
using UNumberFormat = void*;

inline int32_t unum_countAvailable() {
  MOZ_CRASH("unum_countAvailable: Intl API disabled");
}

inline const char* unum_getAvailable(int32_t localeIndex) {
  MOZ_CRASH("unum_getAvailable: Intl API disabled");
}

enum UNumberFormatFields {
  UNUM_INTEGER_FIELD,
  UNUM_GROUPING_SEPARATOR_FIELD,
  UNUM_DECIMAL_SEPARATOR_FIELD,
  UNUM_FRACTION_FIELD,
  UNUM_SIGN_FIELD,
  UNUM_PERCENT_FIELD,
  UNUM_CURRENCY_FIELD,
  UNUM_PERMILL_FIELD,
  UNUM_EXPONENT_SYMBOL_FIELD,
  UNUM_EXPONENT_SIGN_FIELD,
  UNUM_EXPONENT_FIELD,
  UNUM_MEASURE_UNIT_FIELD,
  UNUM_COMPACT_FIELD,
  UNUM_FIELD_COUNT,
};

struct UNumberingSystem;

inline UNumberingSystem* unumsys_open(const char* locale, UErrorCode* status) {
  MOZ_CRASH("unumsys_open: Intl API disabled");
}

inline const char* unumsys_getName(const UNumberingSystem* unumsys) {
  MOZ_CRASH("unumsys_getName: Intl API disabled");
}

inline void unumsys_close(UNumberingSystem* unumsys) {
  MOZ_CRASH("unumsys_close: Intl API disabled");
}

struct UNumberFormatter;
struct UFormattedNumber;

inline UNumberFormatter* unumf_openForSkeletonAndLocale(const UChar* skeleton,
                                                        int32_t skeletonLen,
                                                        const char* locale,
                                                        UErrorCode* status) {
  MOZ_CRASH("unumf_openForSkeletonAndLocale: Intl API disabled");
}

inline void unumf_close(UNumberFormatter* f) {
  MOZ_CRASH("unumf_close: Intl API disabled");
}

inline UFormattedNumber* unumf_openResult(UErrorCode* status) {
  MOZ_CRASH("unumf_openResult: Intl API disabled");
}

inline void unumf_closeResult(UFormattedNumber* uresult) {
  MOZ_CRASH("unumf_closeResult: Intl API disabled");
}

inline void unumf_formatInt(const UNumberFormatter* uformatter, int64_t value,
                            UFormattedNumber* uresult, UErrorCode* status) {
  MOZ_CRASH("unumf_formatInt: Intl API disabled");
}

inline void unumf_formatDouble(const UNumberFormatter* uformatter, double value,
                               UFormattedNumber* uresult, UErrorCode* status) {
  MOZ_CRASH("unumf_formatDouble: Intl API disabled");
}

inline void unumf_formatDecimal(const UNumberFormatter* uformatter,
                                const char* value, int32_t valueLen,
                                UFormattedNumber* uresult, UErrorCode* status) {
  MOZ_CRASH("unumf_formatDecimal: Intl API disabled");
}

struct UFormattedValue;

inline const UFormattedValue* unumf_resultAsValue(
    const UFormattedNumber* uresult, UErrorCode* status) {
  MOZ_CRASH("unumf_resultAsValue: Intl API disabled");
}

inline void unumf_resultGetAllFieldPositions(const UFormattedNumber* uresult,
                                             UFieldPositionIterator* ufpositer,
                                             UErrorCode* status) {
  MOZ_CRASH("unumf_resultGetAllFieldPositions: Intl API disabled");
}

inline int32_t unumf_resultToString(const UFormattedNumber* uresult,
                                    UChar* buffer, int32_t bufferCapacity,
                                    UErrorCode* status) {
  MOZ_CRASH("unumf_resultToString: Intl API disabled");
}

using UCalendar = void*;

enum UCalendarType {
  UCAL_TRADITIONAL,
  UCAL_DEFAULT = UCAL_TRADITIONAL,
  UCAL_GREGORIAN
};

enum UCalendarAttribute {
  UCAL_FIRST_DAY_OF_WEEK,
  UCAL_MINIMAL_DAYS_IN_FIRST_WEEK
};

enum UCalendarDaysOfWeek {
  UCAL_SUNDAY,
  UCAL_MONDAY,
  UCAL_TUESDAY,
  UCAL_WEDNESDAY,
  UCAL_THURSDAY,
  UCAL_FRIDAY,
  UCAL_SATURDAY
};

enum UCalendarWeekdayType {
  UCAL_WEEKDAY,
  UCAL_WEEKEND,
  UCAL_WEEKEND_ONSET,
  UCAL_WEEKEND_CEASE
};

enum UCalendarDateFields {
  UCAL_ERA,
  UCAL_YEAR,
  UCAL_MONTH,
  UCAL_WEEK_OF_YEAR,
  UCAL_WEEK_OF_MONTH,
  UCAL_DATE,
  UCAL_DAY_OF_YEAR,
  UCAL_DAY_OF_WEEK,
  UCAL_DAY_OF_WEEK_IN_MONTH,
  UCAL_AM_PM,
  UCAL_HOUR,
  UCAL_HOUR_OF_DAY,
  UCAL_MINUTE,
  UCAL_SECOND,
  UCAL_MILLISECOND,
  UCAL_ZONE_OFFSET,
  UCAL_DST_OFFSET,
  UCAL_YEAR_WOY,
  UCAL_DOW_LOCAL,
  UCAL_EXTENDED_YEAR,
  UCAL_JULIAN_DAY,
  UCAL_MILLISECONDS_IN_DAY,
  UCAL_IS_LEAP_MONTH,
  UCAL_FIELD_COUNT,
  UCAL_DAY_OF_MONTH = UCAL_DATE
};

enum UCalendarMonths {
  UCAL_JANUARY,
  UCAL_FEBRUARY,
  UCAL_MARCH,
  UCAL_APRIL,
  UCAL_MAY,
  UCAL_JUNE,
  UCAL_JULY,
  UCAL_AUGUST,
  UCAL_SEPTEMBER,
  UCAL_OCTOBER,
  UCAL_NOVEMBER,
  UCAL_DECEMBER,
  UCAL_UNDECIMBER
};

enum UCalendarAMPMs { UCAL_AM, UCAL_PM };

inline UCalendar* ucal_open(const UChar* zoneID, int32_t len,
                            const char* locale, UCalendarType type,
                            UErrorCode* status) {
  MOZ_CRASH("ucal_open: Intl API disabled");
}

inline const char* ucal_getType(const UCalendar* cal, UErrorCode* status) {
  MOZ_CRASH("ucal_getType: Intl API disabled");
}

inline UEnumeration* ucal_getKeywordValuesForLocale(const char* key,
                                                    const char* locale,
                                                    UBool commonlyUsed,
                                                    UErrorCode* status) {
  MOZ_CRASH("ucal_getKeywordValuesForLocale: Intl API disabled");
}

inline void ucal_close(UCalendar* cal) {
  MOZ_CRASH("ucal_close: Intl API disabled");
}

inline UCalendarWeekdayType ucal_getDayOfWeekType(const UCalendar* cal,
                                                  UCalendarDaysOfWeek dayOfWeek,
                                                  UErrorCode* status) {
  MOZ_CRASH("ucal_getDayOfWeekType: Intl API disabled");
}

inline int32_t ucal_getAttribute(const UCalendar* cal,
                                 UCalendarAttribute attr) {
  MOZ_CRASH("ucal_getAttribute: Intl API disabled");
}

inline int32_t ucal_get(const UCalendar* cal, UCalendarDateFields field,
                        UErrorCode* status) {
  MOZ_CRASH("ucal_get: Intl API disabled");
}

inline UEnumeration* ucal_openTimeZones(UErrorCode* status) {
  MOZ_CRASH("ucal_openTimeZones: Intl API disabled");
}

inline int32_t ucal_getCanonicalTimeZoneID(const UChar* id, int32_t len,
                                           UChar* result,
                                           int32_t resultCapacity,
                                           UBool* isSystemID,
                                           UErrorCode* status) {
  MOZ_CRASH("ucal_getCanonicalTimeZoneID: Intl API disabled");
}

inline int32_t ucal_getDefaultTimeZone(UChar* result, int32_t resultCapacity,
                                       UErrorCode* status) {
  MOZ_CRASH("ucal_getDefaultTimeZone: Intl API disabled");
}

enum UDateTimePatternField {
  UDATPG_YEAR_FIELD,
  UDATPG_MONTH_FIELD,
  UDATPG_WEEK_OF_YEAR_FIELD,
  UDATPG_DAY_FIELD,
};

using UDateTimePatternGenerator = void*;

inline UDateTimePatternGenerator* udatpg_open(const char* locale,
                                              UErrorCode* pErrorCode) {
  MOZ_CRASH("udatpg_open: Intl API disabled");
}

inline int32_t udatpg_getBestPattern(UDateTimePatternGenerator* dtpg,
                                     const UChar* skeleton, int32_t length,
                                     UChar* bestPattern, int32_t capacity,
                                     UErrorCode* pErrorCode) {
  MOZ_CRASH("udatpg_getBestPattern: Intl API disabled");
}

inline const UChar* udatpg_getAppendItemName(
    const UDateTimePatternGenerator* dtpg, UDateTimePatternField field,
    int32_t* pLength) {
  MOZ_CRASH("udatpg_getAppendItemName: Intl API disabled");
}

inline void udatpg_close(UDateTimePatternGenerator* dtpg) {
  MOZ_CRASH("udatpg_close: Intl API disabled");
}

using UDateFormat = void*;

enum UDateFormatField {
  UDAT_ERA_FIELD = 0,
  UDAT_YEAR_FIELD = 1,
  UDAT_MONTH_FIELD = 2,
  UDAT_DATE_FIELD = 3,
  UDAT_HOUR_OF_DAY1_FIELD = 4,
  UDAT_HOUR_OF_DAY0_FIELD = 5,
  UDAT_MINUTE_FIELD = 6,
  UDAT_SECOND_FIELD = 7,
  UDAT_FRACTIONAL_SECOND_FIELD = 8,
  UDAT_DAY_OF_WEEK_FIELD = 9,
  UDAT_DAY_OF_YEAR_FIELD = 10,
  UDAT_DAY_OF_WEEK_IN_MONTH_FIELD = 11,
  UDAT_WEEK_OF_YEAR_FIELD = 12,
  UDAT_WEEK_OF_MONTH_FIELD = 13,
  UDAT_AM_PM_FIELD = 14,
  UDAT_HOUR1_FIELD = 15,
  UDAT_HOUR0_FIELD = 16,
  UDAT_TIMEZONE_FIELD = 17,
  UDAT_YEAR_WOY_FIELD = 18,
  UDAT_DOW_LOCAL_FIELD = 19,
  UDAT_EXTENDED_YEAR_FIELD = 20,
  UDAT_JULIAN_DAY_FIELD = 21,
  UDAT_MILLISECONDS_IN_DAY_FIELD = 22,
  UDAT_TIMEZONE_RFC_FIELD = 23,
  UDAT_TIMEZONE_GENERIC_FIELD = 24,
  UDAT_STANDALONE_DAY_FIELD = 25,
  UDAT_STANDALONE_MONTH_FIELD = 26,
  UDAT_QUARTER_FIELD = 27,
  UDAT_STANDALONE_QUARTER_FIELD = 28,
  UDAT_TIMEZONE_SPECIAL_FIELD = 29,
  UDAT_YEAR_NAME_FIELD = 30,
  UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD = 31,
  UDAT_TIMEZONE_ISO_FIELD = 32,
  UDAT_TIMEZONE_ISO_LOCAL_FIELD = 33,
  UDAT_RELATED_YEAR_FIELD = 34,
  UDAT_AM_PM_MIDNIGHT_NOON_FIELD = 35,
  UDAT_FLEXIBLE_DAY_PERIOD_FIELD = 36,
  UDAT_TIME_SEPARATOR_FIELD = 37,
  UDAT_FIELD_COUNT = 38
};

enum UDateFormatStyle {
  UDAT_FULL,
  UDAT_LONG,
  UDAT_MEDIUM,
  UDAT_SHORT,
  UDAT_DEFAULT = UDAT_MEDIUM,
  UDAT_NONE = -1,
  UDAT_PATTERN = -2,
  UDAT_IGNORE = UDAT_PATTERN
};

enum UDateFormatSymbolType {
  UDAT_ERAS,
  UDAT_MONTHS,
  UDAT_SHORT_MONTHS,
  UDAT_WEEKDAYS,
  UDAT_SHORT_WEEKDAYS,
  UDAT_AM_PMS,
  UDAT_LOCALIZED_CHARS,
  UDAT_ERA_NAMES,
  UDAT_NARROW_MONTHS,
  UDAT_NARROW_WEEKDAYS,
  UDAT_STANDALONE_MONTHS,
  UDAT_STANDALONE_SHORT_MONTHS,
  UDAT_STANDALONE_NARROW_MONTHS,
  UDAT_STANDALONE_WEEKDAYS,
  UDAT_STANDALONE_SHORT_WEEKDAYS,
  UDAT_STANDALONE_NARROW_WEEKDAYS,
  UDAT_QUARTERS,
  UDAT_SHORT_QUARTERS,
  UDAT_STANDALONE_QUARTERS,
  UDAT_STANDALONE_SHORT_QUARTERS,
  UDAT_SHORTER_WEEKDAYS,
  UDAT_STANDALONE_SHORTER_WEEKDAYS,
  UDAT_CYCLIC_YEARS_WIDE,
  UDAT_CYCLIC_YEARS_ABBREVIATED,
  UDAT_CYCLIC_YEARS_NARROW,
  UDAT_ZODIAC_NAMES_WIDE,
  UDAT_ZODIAC_NAMES_ABBREVIATED,
  UDAT_ZODIAC_NAMES_NARROW
};

inline int32_t udat_countAvailable() {
  MOZ_CRASH("udat_countAvailable: Intl API disabled");
}

inline int32_t udat_toPattern(const UDateFormat* fmt, UBool localized,
                              UChar* result, int32_t resultLength,
                              UErrorCode* status) {
  MOZ_CRASH("udat_toPattern: Intl API disabled");
}

inline const char* udat_getAvailable(int32_t localeIndex) {
  MOZ_CRASH("udat_getAvailable: Intl API disabled");
}

inline UDateFormat* udat_open(UDateFormatStyle timeStyle,
                              UDateFormatStyle dateStyle, const char* locale,
                              const UChar* tzID, int32_t tzIDLength,
                              const UChar* pattern, int32_t patternLength,
                              UErrorCode* status) {
  MOZ_CRASH("udat_open: Intl API disabled");
}

inline const UCalendar* udat_getCalendar(const UDateFormat* fmt) {
  MOZ_CRASH("udat_getCalendar: Intl API disabled");
}

inline void ucal_setGregorianChange(UCalendar* cal, UDate date,
                                    UErrorCode* pErrorCode) {
  MOZ_CRASH("ucal_setGregorianChange: Intl API disabled");
}

inline int32_t udat_format(const UDateFormat* format, UDate dateToFormat,
                           UChar* result, int32_t resultLength,
                           UFieldPosition* position, UErrorCode* status) {
  MOZ_CRASH("udat_format: Intl API disabled");
}

inline int32_t udat_formatForFields(const UDateFormat* format,
                                    UDate dateToFormat, UChar* result,
                                    int32_t resultLength,
                                    UFieldPositionIterator* fpositer,
                                    UErrorCode* status) {
  MOZ_CRASH("udat_formatForFields: Intl API disabled");
}

inline UFieldPositionIterator* ufieldpositer_open(UErrorCode* status) {
  MOZ_CRASH("ufieldpositer_open: Intl API disabled");
}

inline void ufieldpositer_close(UFieldPositionIterator* fpositer) {
  MOZ_CRASH("ufieldpositer_close: Intl API disabled");
}

inline int32_t ufieldpositer_next(UFieldPositionIterator* fpositer,
                                  int32_t* beginIndex, int32_t* endIndex) {
  MOZ_CRASH("ufieldpositer_next: Intl API disabled");
}

inline void udat_close(UDateFormat* format) {
  MOZ_CRASH("udat_close: Intl API disabled");
}

inline int32_t udat_getSymbols(const UDateFormat* fmt,
                               UDateFormatSymbolType type, int32_t symbolIndex,
                               UChar* result, int32_t resultLength,
                               UErrorCode* status) {
  MOZ_CRASH("udat_getSymbols: Intl API disabled");
}

struct UPluralRules;

enum UPluralType { UPLURAL_TYPE_CARDINAL, UPLURAL_TYPE_ORDINAL };

inline void uplrules_close(UPluralRules* uplrules) {
  MOZ_CRASH("uplrules_close: Intl API disabled");
}

inline UPluralRules* uplrules_openForType(const char* locale, UPluralType type,
                                          UErrorCode* status) {
  MOZ_CRASH("uplrules_openForType: Intl API disabled");
}

inline int32_t uplrules_selectFormatted(const UPluralRules* uplrules,
                                        const UFormattedNumber* number,
                                        UChar* keyword, int32_t capacity,
                                        UErrorCode* status) {
  MOZ_CRASH("uplrules_selectFormatted: Intl API disabled");
}

inline UEnumeration* uplrules_getKeywords(const UPluralRules* uplrules,
                                          UErrorCode* status) {
  MOZ_CRASH("uplrules_getKeywords: Intl API disabled");
}

inline int32_t u_strToLower(UChar* dest, int32_t destCapacity, const UChar* src,
                            int32_t srcLength, const char* locale,
                            UErrorCode* pErrorCode) {
  MOZ_CRASH("u_strToLower: Intl API disabled");
}

inline int32_t u_strToUpper(UChar* dest, int32_t destCapacity, const UChar* src,
                            int32_t srcLength, const char* locale,
                            UErrorCode* pErrorCode) {
  MOZ_CRASH("u_strToUpper: Intl API disabled");
}

inline const char* uloc_toUnicodeLocaleType(const char* keyword,
                                            const char* value) {
  MOZ_CRASH("uloc_toUnicodeLocaleType: Intl API disabled");
}

enum UDateRelativeDateTimeFormatterStyle {
  UDAT_STYLE_LONG,
  UDAT_STYLE_SHORT,
  UDAT_STYLE_NARROW
};

enum URelativeDateTimeUnit {
  UDAT_REL_UNIT_YEAR,
  UDAT_REL_UNIT_QUARTER,
  UDAT_REL_UNIT_MONTH,
  UDAT_REL_UNIT_WEEK,
  UDAT_REL_UNIT_DAY,
  UDAT_REL_UNIT_HOUR,
  UDAT_REL_UNIT_MINUTE,
  UDAT_REL_UNIT_SECOND,
};

enum UDisplayContext {
  UDISPCTX_STANDARD_NAMES,
  UDISPCTX_DIALECT_NAMES,
  UDISPCTX_CAPITALIZATION_NONE,
  UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,
  UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE,
  UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,
  UDISPCTX_CAPITALIZATION_FOR_STANDALONE,
  UDISPCTX_LENGTH_FULL,
  UDISPCTX_LENGTH_SHORT
};

struct URelativeDateTimeFormatter;

inline URelativeDateTimeFormatter* ureldatefmt_open(
    const char* locale, UNumberFormat* nfToAdopt,
    UDateRelativeDateTimeFormatterStyle width,
    UDisplayContext capitalizationContext, UErrorCode* status) {
  MOZ_CRASH("ureldatefmt_open: Intl API disabled");
}

inline void ureldatefmt_close(URelativeDateTimeFormatter* reldatefmt) {
  MOZ_CRASH("ureldatefmt_close: Intl API disabled");
}

inline int32_t ureldatefmt_format(const URelativeDateTimeFormatter* reldatefmt,
                                  double offset, URelativeDateTimeUnit unit,
                                  UChar* result, int32_t resultCapacity,
                                  UErrorCode* status) {
  MOZ_CRASH("ureldatefmt_format: Intl API disabled");
}

inline int32_t ureldatefmt_formatNumeric(
    const URelativeDateTimeFormatter* reldatefmt, double offset,
    URelativeDateTimeUnit unit, UChar* result, int32_t resultCapacity,
    UErrorCode* status) {
  MOZ_CRASH("ureldatefmt_formatNumeric: Intl API disabled");
}

struct UFormattedRelativeDateTime;

inline UFormattedRelativeDateTime* ureldatefmt_openResult(UErrorCode* status) {
  MOZ_CRASH("ureldatefmt_openResult: Intl API disabled");
}

inline void ureldatefmt_closeResult(UFormattedRelativeDateTime* ufrdt) {
  MOZ_CRASH("ureldatefmt_closeResult: Intl API disabled");
}

inline void ureldatefmt_formatToResult(
    const URelativeDateTimeFormatter* reldatefmt, double offset,
    URelativeDateTimeUnit unit, UFormattedRelativeDateTime* result,
    UErrorCode* status) {
  MOZ_CRASH("ureldatefmt_formatToResult: Intl API disabled");
}

inline void ureldatefmt_formatNumericToResult(
    const URelativeDateTimeFormatter* reldatefmt, double offset,
    URelativeDateTimeUnit unit, UFormattedRelativeDateTime* result,
    UErrorCode* status) {
  MOZ_CRASH("ureldatefmt_formatToResult: Intl API disabled");
}

struct UFormattedValue;

inline const UFormattedValue* ureldatefmt_resultAsValue(
    const UFormattedRelativeDateTime* ufrdt, UErrorCode* status) {
  MOZ_CRASH("ureldatefmt_resultAsValue: Intl API disabled");
}

inline const UChar* ufmtval_getString(const UFormattedValue* ufmtval,
                                      int32_t* pLength, UErrorCode* status) {
  MOZ_CRASH("ufmtval_getString: Intl API disabled");
}

struct UConstrainedFieldPosition;

inline UConstrainedFieldPosition* ucfpos_open(UErrorCode* status) {
  MOZ_CRASH("ucfpos_open: Intl API disabled");
}

inline void ucfpos_close(UConstrainedFieldPosition* ucfpos) {
  MOZ_CRASH("ucfpos_close: Intl API disabled");
}

typedef enum UFieldCategory { UFIELD_CATEGORY_NUMBER } UFieldCategory;

inline void ucfpos_constrainCategory(UConstrainedFieldPosition* ucfpos,
                                     int32_t category, UErrorCode* status) {
  MOZ_CRASH("ucfpos_constrainCategory: Intl API disabled");
}

inline bool ufmtval_nextPosition(const UFormattedValue* ufmtval,
                                 UConstrainedFieldPosition* ucfpos,
                                 UErrorCode* status) {
  MOZ_CRASH("ufmtval_nextPosition: Intl API disabled");
}

inline int32_t ucfpos_getField(const UConstrainedFieldPosition* ucfpos,
                               UErrorCode* status) {
  MOZ_CRASH("ucfpos_getField: Intl API disabled");
}

inline void ucfpos_getIndexes(const UConstrainedFieldPosition* ucfpos,
                              int32_t* pStart, int32_t* pLimit,
                              UErrorCode* status) {
  MOZ_CRASH("ucfpos_getIndexes: Intl API disabled");
}

#  define U_ICUDATA_NAME ""
#  define U_TREE_SEPARATOR_STRING ""

struct UResourceBundle;

inline UResourceBundle* ures_open(const char* packageName, const char* locale,
                                  UErrorCode* status) {
  MOZ_CRASH("ures_open: Intl API disabled");
}

inline void ures_close(UResourceBundle* resourceBundle) {
  MOZ_CRASH("ures_close: Intl API disabled");
}

inline UResourceBundle* ures_getByKey(const UResourceBundle* resourceBundle,
                                      const char* key, UResourceBundle* fillIn,
                                      UErrorCode* status) {
  MOZ_CRASH("ures_getByKey: Intl API disabled");
}

inline UResourceBundle* ures_getByIndex(const UResourceBundle* resourceBundle,
                                        int32_t indexR, UResourceBundle* fillIn,
                                        UErrorCode* status) {
  MOZ_CRASH("ures_getByIndex: Intl API disabled");
}

inline int32_t ures_getSize(const UResourceBundle* resourceBundle) {
  MOZ_CRASH("ures_getSize: Intl API disabled");
}

inline const char* ures_getKey(const UResourceBundle* resourceBundle) {
  MOZ_CRASH("ures_getKey: Intl API disabled");
}

#endif  // !ENABLE_INTL_API

#endif /* builtin_intl_ICUStubs_h */

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

#include "js/GCAPI.h"
#include "js/GCHashTable.h"

#if ENABLE_INTL_API
#include "unicode/utypes.h"
#endif

class JSLinearString;

/*
 * The Intl module specified by standard ECMA-402,
 * ECMAScript Internationalization API Specification.
 */

namespace js {

/**
 * Initializes the Intl Object and its standard built-in properties.
 * Spec: ECMAScript Internationalization API Specification, 8.0, 8.1
 */
extern JSObject*
InitIntlClass(JSContext* cx, HandleObject obj);

/**
 * Stores Intl data which can be shared across compartments (but not contexts).
 *
 * Used for data which is expensive when computed repeatedly or is not
 * available through ICU.
 */
class SharedIntlData
{
    /**
     * Information tracking the set of the supported time zone names, derived
     * from the IANA time zone database <https://www.iana.org/time-zones>.
     *
     * There are two kinds of IANA time zone names: Zone and Link (denoted as
     * such in database source files). Zone names are the canonical, preferred
     * name for a time zone, e.g. Asia/Kolkata. Link names simply refer to
     * target Zone names for their meaning, e.g. Asia/Calcutta targets
     * Asia/Kolkata. That a name is a Link doesn't *necessarily* reflect a
     * sense of deprecation: some Link names also exist partly for convenience,
     * e.g. UTC and GMT as Link names targeting the Zone name Etc/UTC.
     *
     * Two data sources determine the time zone names we support: those ICU
     * supports and IANA's zone information.
     *
     * Unfortunately the names ICU and IANA support, and their Link
     * relationships from name to target, aren't identical, so we can't simply
     * implicitly trust ICU's name handling. We must perform various
     * preprocessing of user-provided zone names and post-processing of
     * ICU-provided zone names to implement ECMA-402's IANA-consistent behavior.
     *
     * Also see <https://ssl.icu-project.org/trac/ticket/12044> and
     * <http://unicode.org/cldr/trac/ticket/9892>.
     */

    using TimeZoneName = JSAtom*;

    struct TimeZoneHasher
    {
        struct Lookup
        {
            union {
                const JS::Latin1Char* latin1Chars;
                const char16_t* twoByteChars;
            };
            bool isLatin1;
            size_t length;
            JS::AutoCheckCannotGC nogc;
            HashNumber hash;

            explicit Lookup(JSLinearString* timeZone);
        };

        static js::HashNumber hash(const Lookup& lookup) { return lookup.hash; }
        static bool match(TimeZoneName key, const Lookup& lookup);
    };

    using TimeZoneSet = js::GCHashSet<TimeZoneName,
                                      TimeZoneHasher,
                                      js::SystemAllocPolicy>;

    using TimeZoneMap = js::GCHashMap<TimeZoneName,
                                      TimeZoneName,
                                      TimeZoneHasher,
                                      js::SystemAllocPolicy>;

    /**
     * As a threshold matter, available time zones are those time zones ICU
     * supports, via ucal_openTimeZones. But ICU supports additional non-IANA
     * time zones described in intl/icu/source/tools/tzcode/icuzones (listed in
     * IntlTimeZoneData.cpp's |legacyICUTimeZones|) for its own backwards
     * compatibility purposes. This set consists of ICU's supported time zones,
     * minus all backwards-compatibility time zones.
     */
    TimeZoneSet availableTimeZones;

    /**
     * IANA treats some time zone names as Zones, that ICU instead treats as
     * Links. For example, IANA considers "America/Indiana/Indianapolis" to be
     * a Zone and "America/Fort_Wayne" a Link that targets it, but ICU
     * considers the former a Link that targets "America/Indianapolis" (which
     * IANA treats as a Link).
     *
     * ECMA-402 requires that we respect IANA data, so if we're asked to
     * canonicalize a time zone name in this set, we must *not* return ICU's
     * canonicalization.
     */
    TimeZoneSet ianaZonesTreatedAsLinksByICU;

    /**
     * IANA treats some time zone names as Links to one target, that ICU
     * instead treats as either Zones, or Links to different targets. An
     * example of the former is "Asia/Calcutta, which IANA assigns the target
     * "Asia/Kolkata" but ICU considers its own Zone. An example of the latter
     * is "America/Virgin", which IANA assigns the target
     * "America/Port_of_Spain" but ICU assigns the target "America/St_Thomas".
     *
     * ECMA-402 requires that we respect IANA data, so if we're asked to
     * canonicalize a time zone name that's a key in this map, we *must* return
     * the corresponding value and *must not* return ICU's canonicalization.
     */
    TimeZoneMap ianaLinksCanonicalizedDifferentlyByICU;

    bool timeZoneDataInitialized = false;

    /**
     * Precomputes the available time zone names, because it's too expensive to
     * call ucal_openTimeZones() repeatedly.
     */
    bool ensureTimeZones(JSContext* cx);

  public:
    /**
     * Returns the validated time zone name in |result|. If the input time zone
     * isn't a valid IANA time zone name, |result| remains unchanged.
     */
    bool validateTimeZoneName(JSContext* cx, JS::HandleString timeZone,
                              JS::MutableHandleString result);

    /**
     * Returns the canonical time zone name in |result|. If no canonical name
     * was found, |result| remains unchanged.
     *
     * This method only handles time zones which are canonicalized differently
     * by ICU when compared to IANA.
     */
    bool tryCanonicalizeTimeZoneConsistentWithIANA(JSContext* cx, JS::HandleString timeZone,
                                                   JS::MutableHandleString result);

    void destroyInstance();

    void trace(JSTracer* trc);

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
};

/*
 * The following functions are for use by self-hosted code.
 */


/******************** Collator ********************/

/**
 * Returns a new instance of the standard built-in Collator constructor.
 * Self-hosted code cannot cache this constructor (as it does for others in
 * Utilities.js) because it is initialized after self-hosted code is compiled.
 *
 * Usage: collator = intl_Collator(locales, options)
 */
extern MOZ_MUST_USE bool
intl_Collator(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns an object indicating the supported locales for collation
 * by having a true-valued property for each such locale with the
 * canonicalized language tag as the property name. The object has no
 * prototype.
 *
 * Usage: availableLocales = intl_Collator_availableLocales()
 */
extern MOZ_MUST_USE bool
intl_Collator_availableLocales(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns an array with the collation type identifiers per Unicode
 * Technical Standard 35, Unicode Locale Data Markup Language, for the
 * collations supported for the given locale. "standard" and "search" are
 * excluded.
 *
 * Usage: collations = intl_availableCollations(locale)
 */
extern MOZ_MUST_USE bool
intl_availableCollations(JSContext* cx, unsigned argc, Value* vp);

/**
 * Compares x and y (which must be String values), and returns a number less
 * than 0 if x < y, 0 if x = y, or a number greater than 0 if x > y according
 * to the sort order for the locale and collation options of the given
 * Collator.
 *
 * Spec: ECMAScript Internationalization API Specification, 10.3.2.
 *
 * Usage: result = intl_CompareStrings(collator, x, y)
 */
extern MOZ_MUST_USE bool
intl_CompareStrings(JSContext* cx, unsigned argc, Value* vp);


/******************** NumberFormat ********************/

/**
 * Returns a new instance of the standard built-in NumberFormat constructor.
 * Self-hosted code cannot cache this constructor (as it does for others in
 * Utilities.js) because it is initialized after self-hosted code is compiled.
 *
 * Usage: numberFormat = intl_NumberFormat(locales, options)
 */
extern MOZ_MUST_USE bool
intl_NumberFormat(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns an object indicating the supported locales for number formatting
 * by having a true-valued property for each such locale with the
 * canonicalized language tag as the property name. The object has no
 * prototype.
 *
 * Usage: availableLocales = intl_NumberFormat_availableLocales()
 */
extern MOZ_MUST_USE bool
intl_NumberFormat_availableLocales(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns the numbering system type identifier per Unicode
 * Technical Standard 35, Unicode Locale Data Markup Language, for the
 * default numbering system for the given locale.
 *
 * Usage: defaultNumberingSystem = intl_numberingSystem(locale)
 */
extern MOZ_MUST_USE bool
intl_numberingSystem(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns a string representing the number x according to the effective
 * locale and the formatting options of the given NumberFormat.
 *
 * Spec: ECMAScript Internationalization API Specification, 11.3.2.
 *
 * Usage: formatted = intl_FormatNumber(numberFormat, x)
 */
extern MOZ_MUST_USE bool
intl_FormatNumber(JSContext* cx, unsigned argc, Value* vp);


/******************** DateTimeFormat ********************/

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
 * Returns a String value representing x (which must be a Number value)
 * according to the effective locale and the formatting options of the
 * given DateTimeFormat.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.3.2.
 *
 * Usage: formatted = intl_FormatDateTime(dateTimeFormat, x)
 */
extern MOZ_MUST_USE bool
intl_FormatDateTime(JSContext* cx, unsigned argc, Value* vp);

/******************** PluralRules ********************/

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

#if ENABLE_INTL_API
/**
 * Cast char16_t* strings to UChar* strings used by ICU.
 */
inline const UChar*
Char16ToUChar(const char16_t* chars)
{
  return reinterpret_cast<const UChar*>(chars);
}

inline UChar*
Char16ToUChar(char16_t* chars)
{
  return reinterpret_cast<UChar*>(chars);
}

inline char16_t*
UCharToChar16(UChar* chars)
{
  return reinterpret_cast<char16_t*>(chars);
}

inline const char16_t*
UCharToChar16(const UChar* chars)
{
  return reinterpret_cast<const char16_t*>(chars);
}
#endif // ENABLE_INTL_API

} // namespace js

#endif /* builtin_Intl_h */

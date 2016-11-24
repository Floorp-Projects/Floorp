/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The Intl module specified by standard ECMA-402,
 * ECMAScript Internationalization API Specification.
 */

#include "builtin/Intl.h"

#include "mozilla/PodOperations.h"
#include "mozilla/Range.h"
#include "mozilla/ScopeExit.h"

#include <string.h>

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsobj.h"

#include "builtin/IntlTimeZoneData.h"
#if ENABLE_INTL_API
#include "unicode/ucal.h"
#include "unicode/ucol.h"
#include "unicode/udat.h"
#include "unicode/udatpg.h"
#include "unicode/uenum.h"
#include "unicode/unum.h"
#include "unicode/unumsys.h"
#include "unicode/ustring.h"
#endif
#include "vm/DateTime.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/Stack.h"
#include "vm/StringBuffer.h"
#include "vm/Unicode.h"

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;

using mozilla::IsFinite;
using mozilla::IsNegativeZero;
using mozilla::MakeScopeExit;
using mozilla::PodCopy;


/*
 * Pervasive note: ICU functions taking a UErrorCode in/out parameter always
 * test that parameter before doing anything, and will return immediately if
 * the value indicates that a failure occurred in a prior ICU call,
 * without doing anything else. See
 * http://userguide.icu-project.org/design#TOC-Error-Handling
 */


/******************** ICU stubs ********************/

#if !ENABLE_INTL_API

/*
 * When the Internationalization API isn't enabled, we also shouldn't link
 * against ICU. However, we still want to compile this code in order to prevent
 * bit rot. The following stub implementations for ICU functions make this
 * possible. The functions using them should never be called, so they assert
 * and return error codes. Signatures adapted from ICU header files locid.h,
 * numsys.h, ucal.h, ucol.h, udat.h, udatpg.h, uenum.h, unum.h; see the ICU
 * directory for license.
 */

namespace {

typedef bool UBool;
typedef char16_t UChar;
typedef double UDate;

enum UErrorCode {
    U_ZERO_ERROR,
    U_BUFFER_OVERFLOW_ERROR,
};

inline UBool
U_FAILURE(UErrorCode code)
{
    MOZ_CRASH("U_FAILURE: Intl API disabled");
}

inline const UChar*
Char16ToUChar(const char16_t* chars)
{
    MOZ_CRASH("Char16ToUChar: Intl API disabled");
}

inline UChar*
Char16ToUChar(char16_t* chars)
{
    MOZ_CRASH("Char16ToUChar: Intl API disabled");
}

struct UEnumeration;

int32_t
uenum_count(UEnumeration* en, UErrorCode* status)
{
    MOZ_CRASH("uenum_count: Intl API disabled");
}

const char*
uenum_next(UEnumeration* en, int32_t* resultLength, UErrorCode* status)
{
    MOZ_CRASH("uenum_next: Intl API disabled");
}

void
uenum_close(UEnumeration* en)
{
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

enum UCollationResult {
    UCOL_EQUAL = 0,
    UCOL_GREATER = 1,
    UCOL_LESS = -1
};

int32_t
ucol_countAvailable()
{
    MOZ_CRASH("ucol_countAvailable: Intl API disabled");
}

const char*
ucol_getAvailable(int32_t localeIndex)
{
    MOZ_CRASH("ucol_getAvailable: Intl API disabled");
}

UCollator*
ucol_open(const char* loc, UErrorCode* status)
{
    MOZ_CRASH("ucol_open: Intl API disabled");
}

void
ucol_setAttribute(UCollator* coll, UColAttribute attr, UColAttributeValue value, UErrorCode* status)
{
    MOZ_CRASH("ucol_setAttribute: Intl API disabled");
}

UCollationResult
ucol_strcoll(const UCollator* coll, const UChar* source, int32_t sourceLength,
             const UChar* target, int32_t targetLength)
{
    MOZ_CRASH("ucol_strcoll: Intl API disabled");
}

void
ucol_close(UCollator* coll)
{
    MOZ_CRASH("ucol_close: Intl API disabled");
}

UEnumeration*
ucol_getKeywordValuesForLocale(const char* key, const char* locale, UBool commonlyUsed,
                               UErrorCode* status)
{
    MOZ_CRASH("ucol_getKeywordValuesForLocale: Intl API disabled");
}

struct UParseError;
struct UFieldPosition;
struct UFieldPositionIterator;
typedef void* UNumberFormat;

enum UNumberFormatStyle {
    UNUM_DECIMAL = 1,
    UNUM_CURRENCY,
    UNUM_PERCENT,
    UNUM_CURRENCY_ISO,
    UNUM_CURRENCY_PLURAL,
};

enum UNumberFormatRoundingMode {
    UNUM_ROUND_HALFUP,
};

enum UNumberFormatAttribute {
    UNUM_GROUPING_USED,
    UNUM_MIN_INTEGER_DIGITS,
    UNUM_MAX_FRACTION_DIGITS,
    UNUM_MIN_FRACTION_DIGITS,
    UNUM_ROUNDING_MODE,
    UNUM_SIGNIFICANT_DIGITS_USED,
    UNUM_MIN_SIGNIFICANT_DIGITS,
    UNUM_MAX_SIGNIFICANT_DIGITS,
};

enum UNumberFormatTextAttribute {
    UNUM_CURRENCY_CODE,
};

int32_t
unum_countAvailable()
{
    MOZ_CRASH("unum_countAvailable: Intl API disabled");
}

const char*
unum_getAvailable(int32_t localeIndex)
{
    MOZ_CRASH("unum_getAvailable: Intl API disabled");
}

UNumberFormat*
unum_open(UNumberFormatStyle style, const UChar* pattern, int32_t patternLength,
          const char* locale, UParseError* parseErr, UErrorCode* status)
{
    MOZ_CRASH("unum_open: Intl API disabled");
}

void
unum_setAttribute(UNumberFormat* fmt, UNumberFormatAttribute  attr, int32_t newValue)
{
    MOZ_CRASH("unum_setAttribute: Intl API disabled");
}

int32_t
unum_formatDouble(const UNumberFormat* fmt, double number, UChar* result,
                  int32_t resultLength, UFieldPosition* pos, UErrorCode* status)
{
    MOZ_CRASH("unum_formatDouble: Intl API disabled");
}

void
unum_close(UNumberFormat* fmt)
{
    MOZ_CRASH("unum_close: Intl API disabled");
}

void
unum_setTextAttribute(UNumberFormat* fmt, UNumberFormatTextAttribute tag, const UChar* newValue,
                      int32_t newValueLength, UErrorCode* status)
{
    MOZ_CRASH("unum_setTextAttribute: Intl API disabled");
}

typedef void* UNumberingSystem;

UNumberingSystem*
unumsys_open(const char* locale, UErrorCode* status)
{
    MOZ_CRASH("unumsys_open: Intl API disabled");
}

const char*
unumsys_getName(const UNumberingSystem* unumsys)
{
    MOZ_CRASH("unumsys_getName: Intl API disabled");
}

void
unumsys_close(UNumberingSystem* unumsys)
{
    MOZ_CRASH("unumsys_close: Intl API disabled");
}

typedef void* UCalendar;

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

UCalendar*
ucal_open(const UChar* zoneID, int32_t len, const char* locale,
          UCalendarType type, UErrorCode* status)
{
    MOZ_CRASH("ucal_open: Intl API disabled");
}

const char*
ucal_getType(const UCalendar* cal, UErrorCode* status)
{
    MOZ_CRASH("ucal_getType: Intl API disabled");
}

UEnumeration*
ucal_getKeywordValuesForLocale(const char* key, const char* locale,
                               UBool commonlyUsed, UErrorCode* status)
{
    MOZ_CRASH("ucal_getKeywordValuesForLocale: Intl API disabled");
}

void
ucal_close(UCalendar* cal)
{
    MOZ_CRASH("ucal_close: Intl API disabled");
}

UCalendarWeekdayType
ucal_getDayOfWeekType(const UCalendar *cal, UCalendarDaysOfWeek dayOfWeek, UErrorCode* status)
{
    MOZ_CRASH("ucal_getDayOfWeekType: Intl API disabled");
}

int32_t
ucal_getAttribute(const UCalendar*    cal,
                  UCalendarAttribute  attr)
{
    MOZ_CRASH("ucal_getAttribute: Intl API disabled");
}

int32_t
ucal_get(const UCalendar *cal, UCalendarDateFields field, UErrorCode *status)
{
    MOZ_CRASH("ucal_get: Intl API disabled");
}

UEnumeration*
ucal_openTimeZones(UErrorCode* status)
{
    MOZ_CRASH("ucal_openTimeZones: Intl API disabled");
}

int32_t
ucal_getCanonicalTimeZoneID(const UChar* id, int32_t len, UChar* result, int32_t resultCapacity,
                            UBool* isSystemID, UErrorCode* status)
{
    MOZ_CRASH("ucal_getCanonicalTimeZoneID: Intl API disabled");
}

int32_t
ucal_getDefaultTimeZone(UChar* result, int32_t resultCapacity, UErrorCode* status)
{
    MOZ_CRASH("ucal_getDefaultTimeZone: Intl API disabled");
}

typedef void* UDateTimePatternGenerator;

UDateTimePatternGenerator*
udatpg_open(const char* locale, UErrorCode* pErrorCode)
{
    MOZ_CRASH("udatpg_open: Intl API disabled");
}

int32_t
udatpg_getBestPattern(UDateTimePatternGenerator* dtpg, const UChar* skeleton,
                      int32_t length, UChar* bestPattern, int32_t capacity,
                      UErrorCode* pErrorCode)
{
    MOZ_CRASH("udatpg_getBestPattern: Intl API disabled");
}

void
udatpg_close(UDateTimePatternGenerator* dtpg)
{
    MOZ_CRASH("udatpg_close: Intl API disabled");
}

typedef void* UCalendar;
typedef void* UDateFormat;

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
    UDAT_PATTERN = -2,
    UDAT_IGNORE = UDAT_PATTERN
};

int32_t
udat_countAvailable()
{
    MOZ_CRASH("udat_countAvailable: Intl API disabled");
}

const char*
udat_getAvailable(int32_t localeIndex)
{
    MOZ_CRASH("udat_getAvailable: Intl API disabled");
}

UDateFormat*
udat_open(UDateFormatStyle timeStyle, UDateFormatStyle dateStyle, const char* locale,
          const UChar* tzID, int32_t tzIDLength, const UChar* pattern,
          int32_t patternLength, UErrorCode* status)
{
    MOZ_CRASH("udat_open: Intl API disabled");
}

const UCalendar*
udat_getCalendar(const UDateFormat* fmt)
{
    MOZ_CRASH("udat_getCalendar: Intl API disabled");
}

void
ucal_setGregorianChange(UCalendar* cal, UDate date, UErrorCode* pErrorCode)
{
    MOZ_CRASH("ucal_setGregorianChange: Intl API disabled");
}

int32_t
udat_format(const UDateFormat* format, UDate dateToFormat, UChar* result,
            int32_t resultLength, UFieldPosition* position, UErrorCode* status)
{
    MOZ_CRASH("udat_format: Intl API disabled");
}

int32_t
udat_formatForFields(const UDateFormat* format, UDate dateToFormat,
                     UChar* result, int32_t resultLength, UFieldPositionIterator* fpositer,
                     UErrorCode* status)
{
    MOZ_CRASH("udat_formatForFields: Intl API disabled");
}

UFieldPositionIterator*
ufieldpositer_open(UErrorCode* status)
{
    MOZ_CRASH("ufieldpositer_open: Intl API disabled");
}

void
ufieldpositer_close(UFieldPositionIterator* fpositer)
{
    MOZ_CRASH("ufieldpositer_close: Intl API disabled");
}

int32_t
ufieldpositer_next(UFieldPositionIterator* fpositer, int32_t* beginIndex, int32_t* endIndex)
{
    MOZ_CRASH("ufieldpositer_next: Intl API disabled");
}

void
udat_close(UDateFormat* format)
{
    MOZ_CRASH("udat_close: Intl API disabled");
}

} // anonymous namespace

#endif


/******************** Common to Intl constructors ********************/

static bool
IntlInitialize(JSContext* cx, HandleObject obj, Handle<PropertyName*> initializer,
               HandleValue locales, HandleValue options)
{
    RootedValue initializerValue(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), initializer, &initializerValue))
        return false;
    MOZ_ASSERT(initializerValue.isObject());
    MOZ_ASSERT(initializerValue.toObject().is<JSFunction>());

    FixedInvokeArgs<3> args(cx);

    args[0].setObject(*obj);
    args[1].set(locales);
    args[2].set(options);

    RootedValue thisv(cx, NullValue());
    RootedValue ignored(cx);
    return js::Call(cx, initializerValue, thisv, args, &ignored);
}

static bool
CreateDefaultOptions(JSContext* cx, MutableHandleValue defaultOptions)
{
    RootedObject options(cx, NewObjectWithGivenProto<PlainObject>(cx, nullptr));
    if (!options)
        return false;
    defaultOptions.setObject(*options);
    return true;
}

// CountAvailable and GetAvailable describe the signatures used for ICU API
// to determine available locales for various functionality.
typedef int32_t
(* CountAvailable)();

typedef const char*
(* GetAvailable)(int32_t localeIndex);

static bool
intl_availableLocales(JSContext* cx, CountAvailable countAvailable,
                      GetAvailable getAvailable, MutableHandleValue result)
{
    RootedObject locales(cx, NewObjectWithGivenProto<PlainObject>(cx, nullptr));
    if (!locales)
        return false;

#if ENABLE_INTL_API
    uint32_t count = countAvailable();
    RootedValue t(cx, BooleanValue(true));
    for (uint32_t i = 0; i < count; i++) {
        const char* locale = getAvailable(i);
        auto lang = DuplicateString(cx, locale);
        if (!lang)
            return false;
        char* p;
        while ((p = strchr(lang.get(), '_')))
            *p = '-';
        RootedAtom a(cx, Atomize(cx, lang.get(), strlen(lang.get())));
        if (!a)
            return false;
        if (!DefineProperty(cx, locales, a->asPropertyName(), t, nullptr, nullptr,
                            JSPROP_ENUMERATE))
        {
            return false;
        }
    }
#endif
    result.setObject(*locales);
    return true;
}

/**
 * Returns the object holding the internal properties for obj.
 */
static JSObject*
GetInternals(JSContext* cx, HandleObject obj)
{
    RootedValue getInternalsValue(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), cx->names().getInternals,
                                         &getInternalsValue))
    {
        return nullptr;
    }
    MOZ_ASSERT(getInternalsValue.isObject());
    MOZ_ASSERT(getInternalsValue.toObject().is<JSFunction>());

    FixedInvokeArgs<1> args(cx);

    args[0].setObject(*obj);

    RootedValue v(cx, NullValue());
    if (!js::Call(cx, getInternalsValue, v, args, &v))
        return nullptr;

    return &v.toObject();
}

static bool
equal(const char* s1, const char* s2)
{
    return !strcmp(s1, s2);
}

static bool
equal(JSAutoByteString& s1, const char* s2)
{
    return !strcmp(s1.ptr(), s2);
}

static const char*
icuLocale(const char* locale)
{
    if (equal(locale, "und"))
        return ""; // ICU root locale
    return locale;
}

// Simple RAII for ICU objects.  Unfortunately, ICU's C++ API is uniformly
// unstable, so we can't use its smart pointers for this.
template <typename T, void (Delete)(T*)>
class ScopedICUObject
{
    T* ptr_;

  public:
    explicit ScopedICUObject(T* ptr)
      : ptr_(ptr)
    {}

    ~ScopedICUObject() {
        if (ptr_)
            Delete(ptr_);
    }

    // In cases where an object should be deleted on abnormal exits,
    // but returned to the caller if everything goes well, call forget()
    // to transfer the object just before returning.
    T* forget() {
        T* tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }
};

// The inline capacity we use for the char16_t Vectors.
static const size_t INITIAL_CHAR_BUFFER_SIZE = 32;

/******************** Collator ********************/

static void collator_finalize(FreeOp* fop, JSObject* obj);

static const uint32_t UCOLLATOR_SLOT = 0;
static const uint32_t COLLATOR_SLOTS_COUNT = 1;

static const ClassOps CollatorClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    collator_finalize
};

static const Class CollatorClass = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(COLLATOR_SLOTS_COUNT) |
    JSCLASS_FOREGROUND_FINALIZE,
    &CollatorClassOps
};

#if JS_HAS_TOSOURCE
static bool
collator_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().Collator);
    return true;
}
#endif

static const JSFunctionSpec collator_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf", "Intl_Collator_supportedLocalesOf", 1, 0),
    JS_FS_END
};

static const JSFunctionSpec collator_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_Collator_resolvedOptions", 0, 0),
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, collator_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * 10.1.2 Intl.Collator([ locales [, options]])
 *
 * ES2017 Intl draft rev 94045d234762ad107a3d09bb6f7381a65f1a2f9b
 */
static bool
Collator(JSContext* cx, const CallArgs& args, bool construct)
{
    RootedObject obj(cx);

    // We're following ECMA-402 1st Edition when Collator is called because of
    // backward compatibility issues.
    // See https://github.com/tc39/ecma402/issues/57
    if (!construct) {
        // ES Intl 1st ed., 10.1.2.1 step 3
        JSObject* intl = cx->global()->getOrCreateIntlObject(cx);
        if (!intl)
            return false;
        RootedValue self(cx, args.thisv());
        if (!self.isUndefined() && (!self.isObject() || self.toObject() != *intl)) {
            // ES Intl 1st ed., 10.1.2.1 step 4
            obj = ToObject(cx, self);
            if (!obj)
                return false;

            // ES Intl 1st ed., 10.1.2.1 step 5
            bool extensible;
            if (!IsExtensible(cx, obj, &extensible))
                return false;
            if (!extensible)
                return Throw(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE);
        } else {
            // ES Intl 1st ed., 10.1.2.1 step 3.a
            construct = true;
        }
    }

    if (construct) {
        // Steps 2-5 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
        RootedObject proto(cx);
        if (args.isConstructing() && !GetPrototypeFromCallableConstructor(cx, args, &proto))
            return false;

        if (!proto) {
            proto = cx->global()->getOrCreateCollatorPrototype(cx);
            if (!proto)
                return false;
        }

        obj = NewObjectWithGivenProto(cx, &CollatorClass, proto);
        if (!obj)
            return false;

        obj->as<NativeObject>().setReservedSlot(UCOLLATOR_SLOT, PrivateValue(nullptr));
    }

    RootedValue locales(cx, args.length() > 0 ? args[0] : UndefinedValue());
    RootedValue options(cx, args.length() > 1 ? args[1] : UndefinedValue());

    // Step 6.
    if (!IntlInitialize(cx, obj, cx->names().InitializeCollator, locales, options))
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
Collator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return Collator(cx, args, args.isConstructing());
}

bool
js::intl_Collator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(!args.isConstructing());
    // intl_Collator is an intrinsic for self-hosted JavaScript, so it cannot
    // be used with "new", but it still has to be treated as a constructor.
    return Collator(cx, args, true);
}

static void
collator_finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->onMainThread());

    // This is-undefined check shouldn't be necessary, but for internal
    // brokenness in object allocation code.  For the moment, hack around it by
    // explicitly guarding against the possibility of the reserved slot not
    // containing a private.  See bug 949220.
    const Value& slot = obj->as<NativeObject>().getReservedSlot(UCOLLATOR_SLOT);
    if (!slot.isUndefined()) {
        if (UCollator* coll = static_cast<UCollator*>(slot.toPrivate()))
            ucol_close(coll);
    }
}

static JSObject*
CreateCollatorPrototype(JSContext* cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx, global->createConstructor(cx, &Collator, cx->names().Collator, 0));
    if (!ctor)
        return nullptr;

    RootedNativeObject proto(cx, global->createBlankPrototype(cx, &CollatorClass));
    if (!proto)
        return nullptr;
    proto->setReservedSlot(UCOLLATOR_SLOT, PrivateValue(nullptr));

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    // 10.2.2
    if (!JS_DefineFunctions(cx, ctor, collator_static_methods))
        return nullptr;

    // 10.3.2 and 10.3.3
    if (!JS_DefineFunctions(cx, proto, collator_methods))
        return nullptr;

    /*
     * Install the getter for Collator.prototype.compare, which returns a bound
     * comparison function for the specified Collator object (suitable for
     * passing to methods like Array.prototype.sort).
     */
    RootedValue getter(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), cx->names().CollatorCompareGet, &getter))
        return nullptr;
    if (!DefineProperty(cx, proto, cx->names().compare, UndefinedHandleValue,
                        JS_DATA_TO_FUNC_PTR(JSGetterOp, &getter.toObject()),
                        nullptr, JSPROP_GETTER | JSPROP_SHARED))
    {
        return nullptr;
    }

    RootedValue options(cx);
    if (!CreateDefaultOptions(cx, &options))
        return nullptr;

    // 10.2.1 and 10.3
    if (!IntlInitialize(cx, proto, cx->names().InitializeCollator, UndefinedHandleValue, options))
        return nullptr;

    // 8.1
    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!DefineProperty(cx, Intl, cx->names().Collator, ctorValue, nullptr, nullptr, 0))
        return nullptr;

    return proto;
}

bool
js::intl_Collator_availableLocales(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    RootedValue result(cx);
    if (!intl_availableLocales(cx, ucol_countAvailable, ucol_getAvailable, &result))
        return false;
    args.rval().set(result);
    return true;
}

bool
js::intl_availableCollations(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration* values = ucol_getKeywordValuesForLocale("co", locale.ptr(), false, &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }
    ScopedICUObject<UEnumeration, uenum_close> toClose(values);

    uint32_t count = uenum_count(values, &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    RootedObject collations(cx, NewDenseEmptyArray(cx));
    if (!collations)
        return false;

    uint32_t index = 0;
    for (uint32_t i = 0; i < count; i++) {
        const char* collation = uenum_next(values, nullptr, &status);
        if (U_FAILURE(status)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
            return false;
        }

        // Per ECMA-402, 10.2.3, we don't include standard and search:
        // "The values 'standard' and 'search' must not be used as elements in
        // any [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co
        // array."
        if (equal(collation, "standard") || equal(collation, "search"))
            continue;

        // ICU returns old-style keyword values; map them to BCP 47 equivalents
        // (see http://bugs.icu-project.org/trac/ticket/9620).
        if (equal(collation, "dictionary"))
            collation = "dict";
        else if (equal(collation, "gb2312han"))
            collation = "gb2312";
        else if (equal(collation, "phonebook"))
            collation = "phonebk";
        else if (equal(collation, "traditional"))
            collation = "trad";

        RootedString jscollation(cx, JS_NewStringCopyZ(cx, collation));
        if (!jscollation)
            return false;
        RootedValue element(cx, StringValue(jscollation));
        if (!DefineElement(cx, collations, index++, element))
            return false;
    }

    args.rval().setObject(*collations);
    return true;
}

/**
 * Returns a new UCollator with the locale and collation options
 * of the given Collator.
 */
static UCollator*
NewUCollator(JSContext* cx, HandleObject collator)
{
    RootedValue value(cx);

    RootedObject internals(cx, GetInternals(cx, collator));
    if (!internals)
        return nullptr;

    if (!GetProperty(cx, internals, internals, cx->names().locale, &value))
        return nullptr;
    JSAutoByteString locale(cx, value.toString());
    if (!locale)
        return nullptr;

    // UCollator options with default values.
    UColAttributeValue uStrength = UCOL_DEFAULT;
    UColAttributeValue uCaseLevel = UCOL_OFF;
    UColAttributeValue uAlternate = UCOL_DEFAULT;
    UColAttributeValue uNumeric = UCOL_OFF;
    // Normalization is always on to meet the canonical equivalence requirement.
    UColAttributeValue uNormalization = UCOL_ON;
    UColAttributeValue uCaseFirst = UCOL_DEFAULT;

    if (!GetProperty(cx, internals, internals, cx->names().usage, &value))
        return nullptr;
    JSAutoByteString usage(cx, value.toString());
    if (!usage)
        return nullptr;
    if (equal(usage, "search")) {
        // ICU expects search as a Unicode locale extension on locale.
        // Unicode locale extensions must occur before private use extensions.
        const char* oldLocale = locale.ptr();
        const char* p;
        size_t index;
        size_t localeLen = strlen(oldLocale);
        if ((p = strstr(oldLocale, "-x-")))
            index = p - oldLocale;
        else
            index = localeLen;

        const char* insert;
        if ((p = strstr(oldLocale, "-u-")) && static_cast<size_t>(p - oldLocale) < index) {
            index = p - oldLocale + 2;
            insert = "-co-search";
        } else {
            insert = "-u-co-search";
        }
        size_t insertLen = strlen(insert);
        char* newLocale = cx->pod_malloc<char>(localeLen + insertLen + 1);
        if (!newLocale)
            return nullptr;
        memcpy(newLocale, oldLocale, index);
        memcpy(newLocale + index, insert, insertLen);
        memcpy(newLocale + index + insertLen, oldLocale + index, localeLen - index + 1); // '\0'
        locale.clear();
        locale.initBytes(newLocale);
    }

    // We don't need to look at the collation property - it can only be set
    // via the Unicode locale extension and is therefore already set on
    // locale.

    if (!GetProperty(cx, internals, internals, cx->names().sensitivity, &value))
        return nullptr;
    JSAutoByteString sensitivity(cx, value.toString());
    if (!sensitivity)
        return nullptr;
    if (equal(sensitivity, "base")) {
        uStrength = UCOL_PRIMARY;
    } else if (equal(sensitivity, "accent")) {
        uStrength = UCOL_SECONDARY;
    } else if (equal(sensitivity, "case")) {
        uStrength = UCOL_PRIMARY;
        uCaseLevel = UCOL_ON;
    } else {
        MOZ_ASSERT(equal(sensitivity, "variant"));
        uStrength = UCOL_TERTIARY;
    }

    if (!GetProperty(cx, internals, internals, cx->names().ignorePunctuation, &value))
        return nullptr;
    // According to the ICU team, UCOL_SHIFTED causes punctuation to be
    // ignored. Looking at Unicode Technical Report 35, Unicode Locale Data
    // Markup Language, "shifted" causes whitespace and punctuation to be
    // ignored - that's a bit more than asked for, but there's no way to get
    // less.
    if (value.toBoolean())
        uAlternate = UCOL_SHIFTED;

    if (!GetProperty(cx, internals, internals, cx->names().numeric, &value))
        return nullptr;
    if (!value.isUndefined() && value.toBoolean())
        uNumeric = UCOL_ON;

    if (!GetProperty(cx, internals, internals, cx->names().caseFirst, &value))
        return nullptr;
    if (!value.isUndefined()) {
        JSAutoByteString caseFirst(cx, value.toString());
        if (!caseFirst)
            return nullptr;
        if (equal(caseFirst, "upper"))
            uCaseFirst = UCOL_UPPER_FIRST;
        else if (equal(caseFirst, "lower"))
            uCaseFirst = UCOL_LOWER_FIRST;
        else
            MOZ_ASSERT(equal(caseFirst, "false"));
    }

    UErrorCode status = U_ZERO_ERROR;
    UCollator* coll = ucol_open(icuLocale(locale.ptr()), &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return nullptr;
    }

    ucol_setAttribute(coll, UCOL_STRENGTH, uStrength, &status);
    ucol_setAttribute(coll, UCOL_CASE_LEVEL, uCaseLevel, &status);
    ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, uAlternate, &status);
    ucol_setAttribute(coll, UCOL_NUMERIC_COLLATION, uNumeric, &status);
    ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, uNormalization, &status);
    ucol_setAttribute(coll, UCOL_CASE_FIRST, uCaseFirst, &status);
    if (U_FAILURE(status)) {
        ucol_close(coll);
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return nullptr;
    }

    return coll;
}

static bool
intl_CompareStrings(JSContext* cx, UCollator* coll, HandleString str1, HandleString str2,
                    MutableHandleValue result)
{
    MOZ_ASSERT(str1);
    MOZ_ASSERT(str2);

    if (str1 == str2) {
        result.setInt32(0);
        return true;
    }

    AutoStableStringChars stableChars1(cx);
    if (!stableChars1.initTwoByte(cx, str1))
        return false;

    AutoStableStringChars stableChars2(cx);
    if (!stableChars2.initTwoByte(cx, str2))
        return false;

    mozilla::Range<const char16_t> chars1 = stableChars1.twoByteRange();
    mozilla::Range<const char16_t> chars2 = stableChars2.twoByteRange();

    UCollationResult uresult = ucol_strcoll(coll,
                                            Char16ToUChar(chars1.begin().get()), chars1.length(),
                                            Char16ToUChar(chars2.begin().get()), chars2.length());
    int32_t res;
    switch (uresult) {
        case UCOL_LESS: res = -1; break;
        case UCOL_EQUAL: res = 0; break;
        case UCOL_GREATER: res = 1; break;
        default: MOZ_CRASH("ucol_strcoll returned bad UCollationResult");
    }
    result.setInt32(res);
    return true;
}

bool
js::intl_CompareStrings(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);
    MOZ_ASSERT(args[0].isObject());
    MOZ_ASSERT(args[1].isString());
    MOZ_ASSERT(args[2].isString());

    RootedObject collator(cx, &args[0].toObject());

    // Obtain a UCollator object, cached if possible.
    // XXX Does this handle Collator instances from other globals correctly?
    bool isCollatorInstance = collator->getClass() == &CollatorClass;
    UCollator* coll;
    if (isCollatorInstance) {
        void* priv = collator->as<NativeObject>().getReservedSlot(UCOLLATOR_SLOT).toPrivate();
        coll = static_cast<UCollator*>(priv);
        if (!coll) {
            coll = NewUCollator(cx, collator);
            if (!coll)
                return false;
            collator->as<NativeObject>().setReservedSlot(UCOLLATOR_SLOT, PrivateValue(coll));
        }
    } else {
        // There's no good place to cache the ICU collator for an object
        // that has been initialized as a Collator but is not a Collator
        // instance. One possibility might be to add a Collator instance as an
        // internal property to each such object.
        coll = NewUCollator(cx, collator);
        if (!coll)
            return false;
    }

    // Use the UCollator to actually compare the strings.
    RootedString str1(cx, args[1].toString());
    RootedString str2(cx, args[2].toString());
    RootedValue result(cx);
    bool success = intl_CompareStrings(cx, coll, str1, str2, &result);

    if (!isCollatorInstance)
        ucol_close(coll);
    if (!success)
        return false;
    args.rval().set(result);
    return true;
}


/******************** NumberFormat ********************/

static void numberFormat_finalize(FreeOp* fop, JSObject* obj);

static const uint32_t UNUMBER_FORMAT_SLOT = 0;
static const uint32_t NUMBER_FORMAT_SLOTS_COUNT = 1;

static const ClassOps NumberFormatClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    numberFormat_finalize
};

static const Class NumberFormatClass = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(NUMBER_FORMAT_SLOTS_COUNT) |
    JSCLASS_FOREGROUND_FINALIZE,
    &NumberFormatClassOps
};

#if JS_HAS_TOSOURCE
static bool
numberFormat_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().NumberFormat);
    return true;
}
#endif

static const JSFunctionSpec numberFormat_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf", "Intl_NumberFormat_supportedLocalesOf", 1, 0),
    JS_FS_END
};

static const JSFunctionSpec numberFormat_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_NumberFormat_resolvedOptions", 0, 0),
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, numberFormat_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * 11.2.1 Intl.NumberFormat([ locales [, options]])
 *
 * ES2017 Intl draft rev 94045d234762ad107a3d09bb6f7381a65f1a2f9b
 */
static bool
NumberFormat(JSContext* cx, const CallArgs& args, bool construct)
{
    RootedObject obj(cx);

    // We're following ECMA-402 1st Edition when NumberFormat is called
    // because of backward compatibility issues.
    // See https://github.com/tc39/ecma402/issues/57
    if (!construct) {
        // ES Intl 1st ed., 11.1.2.1 step 3
        JSObject* intl = cx->global()->getOrCreateIntlObject(cx);
        if (!intl)
            return false;
        RootedValue self(cx, args.thisv());
        if (!self.isUndefined() && (!self.isObject() || self.toObject() != *intl)) {
            // ES Intl 1st ed., 11.1.2.1 step 4
            obj = ToObject(cx, self);
            if (!obj)
                return false;

            // ES Intl 1st ed., 11.1.2.1 step 5
            bool extensible;
            if (!IsExtensible(cx, obj, &extensible))
                return false;
            if (!extensible)
                return Throw(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE);
        } else {
            // ES Intl 1st ed., 11.1.2.1 step 3.a
            construct = true;
        }
    }

    if (construct) {
        // Step 2 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
        RootedObject proto(cx);
        if (args.isConstructing() && !GetPrototypeFromCallableConstructor(cx, args, &proto))
            return false;

        if (!proto) {
            proto = cx->global()->getOrCreateNumberFormatPrototype(cx);
            if (!proto)
                return false;
        }

        obj = NewObjectWithGivenProto(cx, &NumberFormatClass, proto);
        if (!obj)
            return false;

        obj->as<NativeObject>().setReservedSlot(UNUMBER_FORMAT_SLOT, PrivateValue(nullptr));
    }

    RootedValue locales(cx, args.length() > 0 ? args[0] : UndefinedValue());
    RootedValue options(cx, args.length() > 1 ? args[1] : UndefinedValue());

    // Step 3.
    if (!IntlInitialize(cx, obj, cx->names().InitializeNumberFormat, locales, options))
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
NumberFormat(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return NumberFormat(cx, args, args.isConstructing());
}

bool
js::intl_NumberFormat(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(!args.isConstructing());
    // intl_NumberFormat is an intrinsic for self-hosted JavaScript, so it
    // cannot be used with "new", but it still has to be treated as a
    // constructor.
    return NumberFormat(cx, args, true);
}

static void
numberFormat_finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->onMainThread());

    // This is-undefined check shouldn't be necessary, but for internal
    // brokenness in object allocation code.  For the moment, hack around it by
    // explicitly guarding against the possibility of the reserved slot not
    // containing a private.  See bug 949220.
    const Value& slot = obj->as<NativeObject>().getReservedSlot(UNUMBER_FORMAT_SLOT);
    if (!slot.isUndefined()) {
        if (UNumberFormat* nf = static_cast<UNumberFormat*>(slot.toPrivate()))
            unum_close(nf);
    }
}

static JSObject*
CreateNumberFormatPrototype(JSContext* cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, &NumberFormat, cx->names().NumberFormat, 0);
    if (!ctor)
        return nullptr;

    RootedNativeObject proto(cx, global->createBlankPrototype(cx, &NumberFormatClass));
    if (!proto)
        return nullptr;
    proto->setReservedSlot(UNUMBER_FORMAT_SLOT, PrivateValue(nullptr));

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    // 11.2.2
    if (!JS_DefineFunctions(cx, ctor, numberFormat_static_methods))
        return nullptr;

    // 11.3.2 and 11.3.3
    if (!JS_DefineFunctions(cx, proto, numberFormat_methods))
        return nullptr;

    /*
     * Install the getter for NumberFormat.prototype.format, which returns a
     * bound formatting function for the specified NumberFormat object (suitable
     * for passing to methods like Array.prototype.map).
     */
    RootedValue getter(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), cx->names().NumberFormatFormatGet,
                                         &getter))
    {
        return nullptr;
    }
    if (!DefineProperty(cx, proto, cx->names().format, UndefinedHandleValue,
                        JS_DATA_TO_FUNC_PTR(JSGetterOp, &getter.toObject()),
                        nullptr, JSPROP_GETTER | JSPROP_SHARED))
    {
        return nullptr;
    }

    RootedValue options(cx);
    if (!CreateDefaultOptions(cx, &options))
        return nullptr;

    // 11.2.1 and 11.3
    if (!IntlInitialize(cx, proto, cx->names().InitializeNumberFormat, UndefinedHandleValue,
                        options))
    {
        return nullptr;
    }

    // 8.1
    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!DefineProperty(cx, Intl, cx->names().NumberFormat, ctorValue, nullptr, nullptr, 0))
        return nullptr;

    return proto;
}

bool
js::intl_NumberFormat_availableLocales(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    RootedValue result(cx);
    if (!intl_availableLocales(cx, unum_countAvailable, unum_getAvailable, &result))
        return false;
    args.rval().set(result);
    return true;
}

bool
js::intl_numberingSystem(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;

    UErrorCode status = U_ZERO_ERROR;
    UNumberingSystem* numbers = unumsys_open(icuLocale(locale.ptr()), &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    ScopedICUObject<UNumberingSystem, unumsys_close> toClose(numbers);

    const char* name = unumsys_getName(numbers);
    RootedString jsname(cx, JS_NewStringCopyZ(cx, name));
    if (!jsname)
        return false;

    args.rval().setString(jsname);
    return true;
}

/**
 * Returns a new UNumberFormat with the locale and number formatting options
 * of the given NumberFormat.
 */
static UNumberFormat*
NewUNumberFormat(JSContext* cx, HandleObject numberFormat)
{
    RootedValue value(cx);

    RootedObject internals(cx, GetInternals(cx, numberFormat));
    if (!internals)
       return nullptr;

    if (!GetProperty(cx, internals, internals, cx->names().locale, &value))
        return nullptr;
    JSAutoByteString locale(cx, value.toString());
    if (!locale)
        return nullptr;

    // UNumberFormat options with default values
    UNumberFormatStyle uStyle = UNUM_DECIMAL;
    const UChar* uCurrency = nullptr;
    uint32_t uMinimumIntegerDigits = 1;
    uint32_t uMinimumFractionDigits = 0;
    uint32_t uMaximumFractionDigits = 3;
    int32_t uMinimumSignificantDigits = -1;
    int32_t uMaximumSignificantDigits = -1;
    bool uUseGrouping = true;

    // Sprinkle appropriate rooting flavor over things the GC might care about.
    RootedString currency(cx);
    AutoStableStringChars stableChars(cx);

    // We don't need to look at numberingSystem - it can only be set via
    // the Unicode locale extension and is therefore already set on locale.

    if (!GetProperty(cx, internals, internals, cx->names().style, &value))
        return nullptr;
    JSAutoByteString style(cx, value.toString());
    if (!style)
        return nullptr;

    if (equal(style, "currency")) {
        if (!GetProperty(cx, internals, internals, cx->names().currency, &value))
            return nullptr;
        currency = value.toString();
        MOZ_ASSERT(currency->length() == 3,
                   "IsWellFormedCurrencyCode permits only length-3 strings");
        if (!stableChars.initTwoByte(cx, currency))
            return nullptr;
        // uCurrency remains owned by stableChars.
        uCurrency = Char16ToUChar(stableChars.twoByteRange().begin().get());

        if (!GetProperty(cx, internals, internals, cx->names().currencyDisplay, &value))
            return nullptr;
        JSAutoByteString currencyDisplay(cx, value.toString());
        if (!currencyDisplay)
            return nullptr;
        if (equal(currencyDisplay, "code")) {
            uStyle = UNUM_CURRENCY_ISO;
        } else if (equal(currencyDisplay, "symbol")) {
            uStyle = UNUM_CURRENCY;
        } else {
            MOZ_ASSERT(equal(currencyDisplay, "name"));
            uStyle = UNUM_CURRENCY_PLURAL;
        }
    } else if (equal(style, "percent")) {
        uStyle = UNUM_PERCENT;
    } else {
        MOZ_ASSERT(equal(style, "decimal"));
        uStyle = UNUM_DECIMAL;
    }

    RootedId id(cx, NameToId(cx->names().minimumSignificantDigits));
    bool hasP;
    if (!HasProperty(cx, internals, id, &hasP))
        return nullptr;
    if (hasP) {
        if (!GetProperty(cx, internals, internals, cx->names().minimumSignificantDigits,
                         &value))
        {
            return nullptr;
        }
        uMinimumSignificantDigits = int32_t(value.toNumber());
        if (!GetProperty(cx, internals, internals, cx->names().maximumSignificantDigits,
                         &value))
        {
            return nullptr;
        }
        uMaximumSignificantDigits = int32_t(value.toNumber());
    } else {
        if (!GetProperty(cx, internals, internals, cx->names().minimumIntegerDigits,
                         &value))
        {
            return nullptr;
        }
        uMinimumIntegerDigits = int32_t(value.toNumber());
        if (!GetProperty(cx, internals, internals, cx->names().minimumFractionDigits,
                         &value))
        {
            return nullptr;
        }
        uMinimumFractionDigits = int32_t(value.toNumber());
        if (!GetProperty(cx, internals, internals, cx->names().maximumFractionDigits,
                         &value))
        {
            return nullptr;
        }
        uMaximumFractionDigits = int32_t(value.toNumber());
    }

    if (!GetProperty(cx, internals, internals, cx->names().useGrouping, &value))
        return nullptr;
    uUseGrouping = value.toBoolean();

    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* nf = unum_open(uStyle, nullptr, 0, icuLocale(locale.ptr()), nullptr, &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return nullptr;
    }
    ScopedICUObject<UNumberFormat, unum_close> toClose(nf);

    if (uCurrency) {
        unum_setTextAttribute(nf, UNUM_CURRENCY_CODE, uCurrency, 3, &status);
        if (U_FAILURE(status)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
            return nullptr;
        }
    }
    if (uMinimumSignificantDigits != -1) {
        unum_setAttribute(nf, UNUM_SIGNIFICANT_DIGITS_USED, true);
        unum_setAttribute(nf, UNUM_MIN_SIGNIFICANT_DIGITS, uMinimumSignificantDigits);
        unum_setAttribute(nf, UNUM_MAX_SIGNIFICANT_DIGITS, uMaximumSignificantDigits);
    } else {
        unum_setAttribute(nf, UNUM_MIN_INTEGER_DIGITS, uMinimumIntegerDigits);
        unum_setAttribute(nf, UNUM_MIN_FRACTION_DIGITS, uMinimumFractionDigits);
        unum_setAttribute(nf, UNUM_MAX_FRACTION_DIGITS, uMaximumFractionDigits);
    }
    unum_setAttribute(nf, UNUM_GROUPING_USED, uUseGrouping);
    unum_setAttribute(nf, UNUM_ROUNDING_MODE, UNUM_ROUND_HALFUP);

    return toClose.forget();
}

static bool
intl_FormatNumber(JSContext* cx, UNumberFormat* nf, double x, MutableHandleValue result)
{
    // FormatNumber doesn't consider -0.0 to be negative.
    if (IsNegativeZero(x))
        x = 0.0;

    Vector<char16_t, INITIAL_CHAR_BUFFER_SIZE> chars(cx);
    if (!chars.resize(INITIAL_CHAR_BUFFER_SIZE))
        return false;
    UErrorCode status = U_ZERO_ERROR;
    int32_t size = unum_formatDouble(nf, x, Char16ToUChar(chars.begin()), INITIAL_CHAR_BUFFER_SIZE,
                                     nullptr, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        MOZ_ASSERT(size >= 0);
        if (!chars.resize(size))
            return false;
        status = U_ZERO_ERROR;
        unum_formatDouble(nf, x, Char16ToUChar(chars.begin()), size, nullptr, &status);
    }
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    MOZ_ASSERT(size >= 0);
    JSString* str = NewStringCopyN<CanGC>(cx, chars.begin(), size);
    if (!str)
        return false;

    result.setString(str);
    return true;
}

bool
js::intl_FormatNumber(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(args[0].isObject());
    MOZ_ASSERT(args[1].isNumber());

    RootedObject numberFormat(cx, &args[0].toObject());

    // Obtain a UNumberFormat object, cached if possible.
    bool isNumberFormatInstance = numberFormat->getClass() == &NumberFormatClass;
    UNumberFormat* nf;
    if (isNumberFormatInstance) {
        void* priv =
            numberFormat->as<NativeObject>().getReservedSlot(UNUMBER_FORMAT_SLOT).toPrivate();
        nf = static_cast<UNumberFormat*>(priv);
        if (!nf) {
            nf = NewUNumberFormat(cx, numberFormat);
            if (!nf)
                return false;
            numberFormat->as<NativeObject>().setReservedSlot(UNUMBER_FORMAT_SLOT, PrivateValue(nf));
        }
    } else {
        // There's no good place to cache the ICU number format for an object
        // that has been initialized as a NumberFormat but is not a
        // NumberFormat instance. One possibility might be to add a
        // NumberFormat instance as an internal property to each such object.
        nf = NewUNumberFormat(cx, numberFormat);
        if (!nf)
            return false;
    }

    // Use the UNumberFormat to actually format the number.
    RootedValue result(cx);
    bool success = intl_FormatNumber(cx, nf, args[1].toNumber(), &result);

    if (!isNumberFormatInstance)
        unum_close(nf);
    if (!success)
        return false;
    args.rval().set(result);
    return true;
}


/******************** DateTimeFormat ********************/

static void dateTimeFormat_finalize(FreeOp* fop, JSObject* obj);

static const uint32_t UDATE_FORMAT_SLOT = 0;
static const uint32_t DATE_TIME_FORMAT_SLOTS_COUNT = 1;

static const ClassOps DateTimeFormatClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    dateTimeFormat_finalize
};

static const Class DateTimeFormatClass = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(DATE_TIME_FORMAT_SLOTS_COUNT) |
    JSCLASS_FOREGROUND_FINALIZE,
    &DateTimeFormatClassOps
};

#if JS_HAS_TOSOURCE
static bool
dateTimeFormat_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().DateTimeFormat);
    return true;
}
#endif

static const JSFunctionSpec dateTimeFormat_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf", "Intl_DateTimeFormat_supportedLocalesOf", 1, 0),
    JS_FS_END
};

static const JSFunctionSpec dateTimeFormat_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_DateTimeFormat_resolvedOptions", 0, 0),
    JS_SELF_HOSTED_FN("formatToParts", "Intl_DateTimeFormat_formatToParts", 0, 0),
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, dateTimeFormat_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * 12.2.1 Intl.DateTimeFormat([ locales [, options]])
 *
 * ES2017 Intl draft rev 94045d234762ad107a3d09bb6f7381a65f1a2f9b
 */
static bool
DateTimeFormat(JSContext* cx, const CallArgs& args, bool construct)
{
    RootedObject obj(cx);

    // We're following ECMA-402 1st Edition when DateTimeFormat is called
    // because of backward compatibility issues.
    // See https://github.com/tc39/ecma402/issues/57
    if (!construct) {
        // ES Intl 1st ed., 12.1.2.1 step 3
        JSObject* intl = cx->global()->getOrCreateIntlObject(cx);
        if (!intl)
            return false;
        RootedValue self(cx, args.thisv());
        if (!self.isUndefined() && (!self.isObject() || self.toObject() != *intl)) {
            // ES Intl 1st ed., 12.1.2.1 step 4
            obj = ToObject(cx, self);
            if (!obj)
                return false;

            // ES Intl 1st ed., 12.1.2.1 step 5
            bool extensible;
            if (!IsExtensible(cx, obj, &extensible))
                return false;
            if (!extensible)
                return Throw(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE);
        } else {
            // ES Intl 1st ed., 12.1.2.1 step 3.a
            construct = true;
        }
    }

    if (construct) {
        // Step 2 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
        RootedObject proto(cx);
        if (args.isConstructing() && !GetPrototypeFromCallableConstructor(cx, args, &proto))
            return false;

        if (!proto) {
            proto = cx->global()->getOrCreateDateTimeFormatPrototype(cx);
            if (!proto)
                return false;
        }

        obj = NewObjectWithGivenProto(cx, &DateTimeFormatClass, proto);
        if (!obj)
            return false;

        obj->as<NativeObject>().setReservedSlot(UDATE_FORMAT_SLOT, PrivateValue(nullptr));
    }

    RootedValue locales(cx, args.length() > 0 ? args[0] : UndefinedValue());
    RootedValue options(cx, args.length() > 1 ? args[1] : UndefinedValue());

    // Step 3.
    if (!IntlInitialize(cx, obj, cx->names().InitializeDateTimeFormat, locales, options))
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
DateTimeFormat(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return DateTimeFormat(cx, args, args.isConstructing());
}

bool
js::intl_DateTimeFormat(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(!args.isConstructing());
    // intl_DateTimeFormat is an intrinsic for self-hosted JavaScript, so it
    // cannot be used with "new", but it still has to be treated as a
    // constructor.
    return DateTimeFormat(cx, args, true);
}

static void
dateTimeFormat_finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->onMainThread());

    // This is-undefined check shouldn't be necessary, but for internal
    // brokenness in object allocation code.  For the moment, hack around it by
    // explicitly guarding against the possibility of the reserved slot not
    // containing a private.  See bug 949220.
    const Value& slot = obj->as<NativeObject>().getReservedSlot(UDATE_FORMAT_SLOT);
    if (!slot.isUndefined()) {
        if (UDateFormat* df = static_cast<UDateFormat*>(slot.toPrivate()))
            udat_close(df);
    }
}

static JSObject*
CreateDateTimeFormatPrototype(JSContext* cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, &DateTimeFormat, cx->names().DateTimeFormat, 0);
    if (!ctor)
        return nullptr;

    RootedNativeObject proto(cx, global->createBlankPrototype(cx, &DateTimeFormatClass));
    if (!proto)
        return nullptr;
    proto->setReservedSlot(UDATE_FORMAT_SLOT, PrivateValue(nullptr));

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    // 12.2.2
    if (!JS_DefineFunctions(cx, ctor, dateTimeFormat_static_methods))
        return nullptr;

    // 12.3.2 and 12.3.3
    if (!JS_DefineFunctions(cx, proto, dateTimeFormat_methods))
        return nullptr;

    // Install a getter for DateTimeFormat.prototype.format that returns a
    // formatting function bound to a specified DateTimeFormat object (suitable
    // for passing to methods like Array.prototype.map).
    RootedValue getter(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), cx->names().DateTimeFormatFormatGet,
                                         &getter))
    {
        return nullptr;
    }
    if (!DefineProperty(cx, proto, cx->names().format, UndefinedHandleValue,
                        JS_DATA_TO_FUNC_PTR(JSGetterOp, &getter.toObject()),
                        nullptr, JSPROP_GETTER | JSPROP_SHARED))
    {
        return nullptr;
    }

    RootedValue options(cx);
    if (!CreateDefaultOptions(cx, &options))
        return nullptr;

    // 12.2.1 and 12.3
    if (!IntlInitialize(cx, proto, cx->names().InitializeDateTimeFormat, UndefinedHandleValue,
                        options))
    {
        return nullptr;
    }

    // 8.1
    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!DefineProperty(cx, Intl, cx->names().DateTimeFormat, ctorValue, nullptr, nullptr, 0))
        return nullptr;

    return proto;
}

bool
js::intl_DateTimeFormat_availableLocales(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    RootedValue result(cx);
    if (!intl_availableLocales(cx, udat_countAvailable, udat_getAvailable, &result))
        return false;
    args.rval().set(result);
    return true;
}

// ICU returns old-style keyword values; map them to BCP 47 equivalents
// (see http://bugs.icu-project.org/trac/ticket/9620).
static const char*
bcp47CalendarName(const char* icuName)
{
    if (equal(icuName, "ethiopic-amete-alem"))
        return "ethioaa";
    if (equal(icuName, "gregorian"))
        return "gregory";
    if (equal(icuName, "islamic-civil"))
        return "islamicc";
    return icuName;
}

bool
js::intl_availableCalendars(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;

    RootedObject calendars(cx, NewDenseEmptyArray(cx));
    if (!calendars)
        return false;
    uint32_t index = 0;

    // We need the default calendar for the locale as the first result.
    UErrorCode status = U_ZERO_ERROR;
    RootedString jscalendar(cx);
    {
        UCalendar* cal = ucal_open(nullptr, 0, locale.ptr(), UCAL_DEFAULT, &status);

        // This correctly handles nullptr |cal| when opening failed.
        ScopedICUObject<UCalendar, ucal_close> closeCalendar(cal);

        const char* calendar = ucal_getType(cal, &status);
        if (U_FAILURE(status)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
            return false;
        }

        jscalendar = JS_NewStringCopyZ(cx, bcp47CalendarName(calendar));
        if (!jscalendar)
            return false;
    }

    RootedValue element(cx, StringValue(jscalendar));
    if (!DefineElement(cx, calendars, index++, element))
        return false;

    // Now get the calendars that "would make a difference", i.e., not the default.
    UEnumeration* values = ucal_getKeywordValuesForLocale("ca", locale.ptr(), false, &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }
    ScopedICUObject<UEnumeration, uenum_close> toClose(values);

    uint32_t count = uenum_count(values, &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    for (; count > 0; count--) {
        const char* calendar = uenum_next(values, nullptr, &status);
        if (U_FAILURE(status)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
            return false;
        }

        jscalendar = JS_NewStringCopyZ(cx, bcp47CalendarName(calendar));
        if (!jscalendar)
            return false;
        element = StringValue(jscalendar);
        if (!DefineElement(cx, calendars, index++, element))
            return false;
    }

    args.rval().setObject(*calendars);
    return true;
}

template<typename Char>
static constexpr Char
ToUpperASCII(Char c)
{
    return ('a' <= c && c <= 'z')
           ? (c & ~0x20)
           : c;
}

static_assert(ToUpperASCII('a') == 'A', "verifying 'a' uppercases correctly");
static_assert(ToUpperASCII('m') == 'M', "verifying 'm' uppercases correctly");
static_assert(ToUpperASCII('z') == 'Z', "verifying 'z' uppercases correctly");
static_assert(ToUpperASCII(u'a') == u'A', "verifying u'a' uppercases correctly");
static_assert(ToUpperASCII(u'k') == u'K', "verifying u'k' uppercases correctly");
static_assert(ToUpperASCII(u'z') == u'Z', "verifying u'z' uppercases correctly");

template<typename Char1, typename Char2>
static bool
EqualCharsIgnoreCaseASCII(const Char1* s1, const Char2* s2, size_t len)
{
    for (const Char1* s1end = s1 + len; s1 < s1end; s1++, s2++) {
        if (ToUpperASCII(*s1) != ToUpperASCII(*s2))
            return false;
    }
    return true;
}

template<typename Char>
static js::HashNumber
HashStringIgnoreCaseASCII(const Char* s, size_t length)
{
    uint32_t hash = 0;
    for (size_t i = 0; i < length; i++)
        hash = mozilla::AddToHash(hash, ToUpperASCII(s[i]));
    return hash;
}

js::SharedIntlData::TimeZoneHasher::Lookup::Lookup(JSLinearString* timeZone)
  : isLatin1(timeZone->hasLatin1Chars()), length(timeZone->length())
{
    if (isLatin1) {
        latin1Chars = timeZone->latin1Chars(nogc);
        hash = HashStringIgnoreCaseASCII(latin1Chars, length);
    } else {
        twoByteChars = timeZone->twoByteChars(nogc);
        hash = HashStringIgnoreCaseASCII(twoByteChars, length);
    }
}

bool
js::SharedIntlData::TimeZoneHasher::match(TimeZoneName key, const Lookup& lookup)
{
    if (key->length() != lookup.length)
        return false;

    // Compare time zone names ignoring ASCII case differences.
    if (key->hasLatin1Chars()) {
        const Latin1Char* keyChars = key->latin1Chars(lookup.nogc);
        if (lookup.isLatin1)
            return EqualCharsIgnoreCaseASCII(keyChars, lookup.latin1Chars, lookup.length);
        return EqualCharsIgnoreCaseASCII(keyChars, lookup.twoByteChars, lookup.length);
    }

    const char16_t* keyChars = key->twoByteChars(lookup.nogc);
    if (lookup.isLatin1)
        return EqualCharsIgnoreCaseASCII(lookup.latin1Chars, keyChars, lookup.length);
    return EqualCharsIgnoreCaseASCII(keyChars, lookup.twoByteChars, lookup.length);
}

static bool
IsLegacyICUTimeZone(const char* timeZone)
{
    for (const auto& legacyTimeZone : timezone::legacyICUTimeZones) {
        if (equal(timeZone, legacyTimeZone))
            return true;
    }
    return false;
}

bool
js::SharedIntlData::ensureTimeZones(JSContext* cx)
{
    if (timeZoneDataInitialized)
        return true;

    // If initTimeZones() was called previously, but didn't complete due to
    // OOM, clear all sets/maps and start from scratch.
    if (availableTimeZones.initialized())
        availableTimeZones.finish();
    if (!availableTimeZones.init()) {
        ReportOutOfMemory(cx);
        return false;
    }

    UErrorCode status = U_ZERO_ERROR;
    UEnumeration* values = ucal_openTimeZones(&status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }
    ScopedICUObject<UEnumeration, uenum_close> toClose(values);

    RootedAtom timeZone(cx);
    while (true) {
        int32_t size;
        const char* rawTimeZone = uenum_next(values, &size, &status);
        if (U_FAILURE(status)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
            return false;
        }

        if (rawTimeZone == nullptr)
            break;

        // Skip legacy ICU time zone names.
        if (IsLegacyICUTimeZone(rawTimeZone))
            continue;

        MOZ_ASSERT(size >= 0);
        timeZone = Atomize(cx, rawTimeZone, size_t(size));
        if (!timeZone)
            return false;

        TimeZoneHasher::Lookup lookup(timeZone);
        TimeZoneSet::AddPtr p = availableTimeZones.lookupForAdd(lookup);

        // ICU shouldn't report any duplicate time zone names, but if it does,
        // just ignore the duplicate name.
        if (!p && !availableTimeZones.add(p, timeZone)) {
            ReportOutOfMemory(cx);
            return false;
        }
    }

    if (ianaZonesTreatedAsLinksByICU.initialized())
        ianaZonesTreatedAsLinksByICU.finish();
    if (!ianaZonesTreatedAsLinksByICU.init()) {
        ReportOutOfMemory(cx);
        return false;
    }

    for (const char* rawTimeZone : timezone::ianaZonesTreatedAsLinksByICU) {
        MOZ_ASSERT(rawTimeZone != nullptr);
        timeZone = Atomize(cx, rawTimeZone, strlen(rawTimeZone));
        if (!timeZone)
            return false;

        TimeZoneHasher::Lookup lookup(timeZone);
        TimeZoneSet::AddPtr p = ianaZonesTreatedAsLinksByICU.lookupForAdd(lookup);
        MOZ_ASSERT(!p, "Duplicate entry in timezone::ianaZonesTreatedAsLinksByICU");

        if (!ianaZonesTreatedAsLinksByICU.add(p, timeZone)) {
            ReportOutOfMemory(cx);
            return false;
        }
    }

    if (ianaLinksCanonicalizedDifferentlyByICU.initialized())
        ianaLinksCanonicalizedDifferentlyByICU.finish();
    if (!ianaLinksCanonicalizedDifferentlyByICU.init()) {
        ReportOutOfMemory(cx);
        return false;
    }

    RootedAtom linkName(cx);
    RootedAtom& target = timeZone;
    for (const auto& linkAndTarget : timezone::ianaLinksCanonicalizedDifferentlyByICU) {
        const char* rawLinkName = linkAndTarget.link;
        const char* rawTarget = linkAndTarget.target;

        MOZ_ASSERT(rawLinkName != nullptr);
        linkName = Atomize(cx, rawLinkName, strlen(rawLinkName));
        if (!linkName)
            return false;

        MOZ_ASSERT(rawTarget != nullptr);
        target = Atomize(cx, rawTarget, strlen(rawTarget));
        if (!target)
            return false;

        TimeZoneHasher::Lookup lookup(linkName);
        TimeZoneMap::AddPtr p = ianaLinksCanonicalizedDifferentlyByICU.lookupForAdd(lookup);
        MOZ_ASSERT(!p, "Duplicate entry in timezone::ianaLinksCanonicalizedDifferentlyByICU");

        if (!ianaLinksCanonicalizedDifferentlyByICU.add(p, linkName, target)) {
            ReportOutOfMemory(cx);
            return false;
        }
    }

    MOZ_ASSERT(!timeZoneDataInitialized, "ensureTimeZones is neither reentrant nor thread-safe");
    timeZoneDataInitialized = true;

    return true;
}

bool
js::SharedIntlData::validateTimeZoneName(JSContext* cx, HandleString timeZone,
                                         MutableHandleString result)
{
    if (!ensureTimeZones(cx))
        return false;

    RootedLinearString timeZoneLinear(cx, timeZone->ensureLinear(cx));
    if (!timeZoneLinear)
        return false;

    TimeZoneHasher::Lookup lookup(timeZoneLinear);
    if (TimeZoneSet::Ptr p = availableTimeZones.lookup(lookup))
        result.set(*p);

    return true;
}

bool
js::SharedIntlData::tryCanonicalizeTimeZoneConsistentWithIANA(JSContext* cx, HandleString timeZone,
                                                              MutableHandleString result)
{
    if (!ensureTimeZones(cx))
        return false;

    RootedLinearString timeZoneLinear(cx, timeZone->ensureLinear(cx));
    if (!timeZoneLinear)
        return false;

    TimeZoneHasher::Lookup lookup(timeZoneLinear);
    MOZ_ASSERT(availableTimeZones.has(lookup), "Invalid time zone name");

    if (TimeZoneMap::Ptr p = ianaLinksCanonicalizedDifferentlyByICU.lookup(lookup)) {
        // The effectively supported time zones aren't known at compile time,
        // when
        // 1. SpiderMonkey was compiled with "--with-system-icu".
        // 2. ICU's dynamic time zone data loading feature was used.
        //    (ICU supports loading time zone files at runtime through the
        //    ICU_TIMEZONE_FILES_DIR environment variable.)
        // Ensure ICU supports the new target zone before applying the update.
        TimeZoneName targetTimeZone = p->value();
        TimeZoneHasher::Lookup targetLookup(targetTimeZone);
        if (availableTimeZones.has(targetLookup))
            result.set(targetTimeZone);
    } else if (TimeZoneSet::Ptr p = ianaZonesTreatedAsLinksByICU.lookup(lookup)) {
        result.set(*p);
    }

    return true;
}

void
js::SharedIntlData::destroyInstance()
{
    availableTimeZones.finish();
    ianaZonesTreatedAsLinksByICU.finish();
    ianaLinksCanonicalizedDifferentlyByICU.finish();
}

void
js::SharedIntlData::trace(JSTracer* trc)
{
    // Atoms are always tenured.
    if (!trc->runtime()->isHeapMinorCollecting()) {
        availableTimeZones.trace(trc);
        ianaZonesTreatedAsLinksByICU.trace(trc);
        ianaLinksCanonicalizedDifferentlyByICU.trace(trc);
    }
}

size_t
js::SharedIntlData::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    return availableTimeZones.sizeOfExcludingThis(mallocSizeOf) +
           ianaZonesTreatedAsLinksByICU.sizeOfExcludingThis(mallocSizeOf) +
           ianaLinksCanonicalizedDifferentlyByICU.sizeOfExcludingThis(mallocSizeOf);
}

bool
js::intl_IsValidTimeZoneName(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    SharedIntlData& sharedIntlData = cx->sharedIntlData;

    RootedString timeZone(cx, args[0].toString());
    RootedString validatedTimeZone(cx);
    if (!sharedIntlData.validateTimeZoneName(cx, timeZone, &validatedTimeZone))
        return false;

    if (validatedTimeZone)
        args.rval().setString(validatedTimeZone);
    else
        args.rval().setNull();

    return true;
}

bool
js::intl_canonicalizeTimeZone(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isString());

    SharedIntlData& sharedIntlData = cx->sharedIntlData;

    // Some time zone names are canonicalized differently by ICU -- handle
    // those first:
    RootedString timeZone(cx, args[0].toString());
    RootedString ianaTimeZone(cx);
    if (!sharedIntlData.tryCanonicalizeTimeZoneConsistentWithIANA(cx, timeZone, &ianaTimeZone))
        return false;

    if (ianaTimeZone) {
        args.rval().setString(ianaTimeZone);
        return true;
    }

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, timeZone))
        return false;

    mozilla::Range<const char16_t> tzchars = stableChars.twoByteRange();

    Vector<char16_t, INITIAL_CHAR_BUFFER_SIZE> chars(cx);
    if (!chars.resize(INITIAL_CHAR_BUFFER_SIZE))
        return false;

    UBool* isSystemID = nullptr;
    UErrorCode status = U_ZERO_ERROR;
    int32_t size = ucal_getCanonicalTimeZoneID(Char16ToUChar(tzchars.begin().get()),
                                               tzchars.length(), Char16ToUChar(chars.begin()),
                                               INITIAL_CHAR_BUFFER_SIZE, isSystemID, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        MOZ_ASSERT(size >= 0);
        if (!chars.resize(size_t(size)))
            return false;
        status = U_ZERO_ERROR;
        ucal_getCanonicalTimeZoneID(Char16ToUChar(tzchars.begin().get()), tzchars.length(),
                                    Char16ToUChar(chars.begin()), size, isSystemID, &status);
    }
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    MOZ_ASSERT(size >= 0);
    JSString* str = NewStringCopyN<CanGC>(cx, chars.begin(), size_t(size));
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

bool
js::intl_defaultTimeZone(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    // The current default might be stale, because JS::ResetTimeZone() doesn't
    // immediately update ICU's default time zone. So perform an update if
    // needed.
    js::ResyncICUDefaultTimeZone();

    Vector<char16_t, INITIAL_CHAR_BUFFER_SIZE> chars(cx);
    if (!chars.resize(INITIAL_CHAR_BUFFER_SIZE))
        return false;

    UErrorCode status = U_ZERO_ERROR;
    int32_t size = ucal_getDefaultTimeZone(Char16ToUChar(chars.begin()), INITIAL_CHAR_BUFFER_SIZE,
                                           &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        MOZ_ASSERT(size >= 0);
        if (!chars.resize(size_t(size)))
            return false;
        status = U_ZERO_ERROR;
        ucal_getDefaultTimeZone(Char16ToUChar(chars.begin()), size, &status);
    }
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    MOZ_ASSERT(size >= 0);
    JSString* str = NewStringCopyN<CanGC>(cx, chars.begin(), size_t(size));
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

bool
js::intl_defaultTimeZoneOffset(JSContext* cx, unsigned argc, Value* vp) {
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    UErrorCode status = U_ZERO_ERROR;
    const UChar* uTimeZone = nullptr;
    int32_t uTimeZoneLength = 0;
    const char* rootLocale = "";
    UCalendar* cal = ucal_open(uTimeZone, uTimeZoneLength, rootLocale, UCAL_DEFAULT, &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }
    ScopedICUObject<UCalendar, ucal_close> toClose(cal);

    int32_t offset = ucal_get(cal, UCAL_ZONE_OFFSET, &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    args.rval().setInt32(offset);
    return true;
}

bool
js::intl_patternForSkeleton(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(args[0].isString());
    MOZ_ASSERT(args[1].isString());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;

    AutoStableStringChars skeleton(cx);
    if (!skeleton.initTwoByte(cx, args[1].toString()))
        return false;

    mozilla::Range<const char16_t> skeletonChars = skeleton.twoByteRange();

    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator* gen = udatpg_open(icuLocale(locale.ptr()), &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }
    ScopedICUObject<UDateTimePatternGenerator, udatpg_close> toClose(gen);

    Vector<char16_t, INITIAL_CHAR_BUFFER_SIZE> chars(cx);
    if (!chars.resize(INITIAL_CHAR_BUFFER_SIZE))
        return false;

    int32_t size = udatpg_getBestPattern(gen, Char16ToUChar(skeletonChars.begin().get()),
                                         skeletonChars.length(), Char16ToUChar(chars.begin()),
                                         INITIAL_CHAR_BUFFER_SIZE, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        MOZ_ASSERT(size >= 0);
        if (!chars.resize(size))
            return false;
        status = U_ZERO_ERROR;
        udatpg_getBestPattern(gen, Char16ToUChar(skeletonChars.begin().get()),
                              skeletonChars.length(), Char16ToUChar(chars.begin()), size, &status);
    }
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    MOZ_ASSERT(size >= 0);
    JSString* str = NewStringCopyN<CanGC>(cx, chars.begin(), size);
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

/**
 * Returns a new UDateFormat with the locale and date-time formatting options
 * of the given DateTimeFormat.
 */
static UDateFormat*
NewUDateFormat(JSContext* cx, HandleObject dateTimeFormat)
{
    RootedValue value(cx);

    RootedObject internals(cx, GetInternals(cx, dateTimeFormat));
    if (!internals)
       return nullptr;

    if (!GetProperty(cx, internals, internals, cx->names().locale, &value))
        return nullptr;
    JSAutoByteString locale(cx, value.toString());
    if (!locale)
        return nullptr;

    // We don't need to look at calendar and numberingSystem - they can only be
    // set via the Unicode locale extension and are therefore already set on
    // locale.

    if (!GetProperty(cx, internals, internals, cx->names().timeZone, &value))
        return nullptr;

    AutoStableStringChars timeZone(cx);
    if (!timeZone.initTwoByte(cx, value.toString()))
        return nullptr;

    mozilla::Range<const char16_t> timeZoneChars = timeZone.twoByteRange();

    if (!GetProperty(cx, internals, internals, cx->names().pattern, &value))
        return nullptr;

    AutoStableStringChars pattern(cx);
    if (!pattern.initTwoByte(cx, value.toString()))
        return nullptr;

    mozilla::Range<const char16_t> patternChars = pattern.twoByteRange();

    UErrorCode status = U_ZERO_ERROR;
    UDateFormat* df =
        udat_open(UDAT_PATTERN, UDAT_PATTERN, icuLocale(locale.ptr()),
                  Char16ToUChar(timeZoneChars.begin().get()), timeZoneChars.length(),
                  Char16ToUChar(patternChars.begin().get()), patternChars.length(), &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return nullptr;
    }

    // ECMAScript requires the Gregorian calendar to be used from the beginning
    // of ECMAScript time.
    UCalendar* cal = const_cast<UCalendar*>(udat_getCalendar(df));
    ucal_setGregorianChange(cal, StartOfTime, &status);

    // An error here means the calendar is not Gregorian, so we don't care.

    return df;
}

static bool
intl_FormatDateTime(JSContext* cx, UDateFormat* df, double x, MutableHandleValue result)
{
    if (!IsFinite(x)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DATE_NOT_FINITE);
        return false;
    }

    Vector<char16_t, INITIAL_CHAR_BUFFER_SIZE> chars(cx);
    if (!chars.resize(INITIAL_CHAR_BUFFER_SIZE))
        return false;
    UErrorCode status = U_ZERO_ERROR;
    int32_t size = udat_format(df, x, Char16ToUChar(chars.begin()), INITIAL_CHAR_BUFFER_SIZE,
                               nullptr, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        MOZ_ASSERT(size >= 0);
        if (!chars.resize(size))
            return false;
        status = U_ZERO_ERROR;
        udat_format(df, x, Char16ToUChar(chars.begin()), size, nullptr, &status);
    }
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    MOZ_ASSERT(size >= 0);
    JSString* str = NewStringCopyN<CanGC>(cx, chars.begin(), size);
    if (!str)
        return false;

    result.setString(str);

    return true;
}

using FieldType = ImmutablePropertyNamePtr JSAtomState::*;

static FieldType
GetFieldTypeForFormatField(UDateFormatField fieldName)
{
    // See intl/icu/source/i18n/unicode/udat.h for a detailed field list.  This
    // switch is deliberately exhaustive: cases might have to be added/removed
    // if this code is compiled with a different ICU with more
    // UDateFormatField enum initializers.  Please guard such cases with
    // appropriate ICU version-testing #ifdefs, should cross-version divergence
    // occur.
    switch (fieldName) {
      case UDAT_ERA_FIELD:
        return &JSAtomState::era;
      case UDAT_YEAR_FIELD:
      case UDAT_YEAR_WOY_FIELD:
      case UDAT_EXTENDED_YEAR_FIELD:
      case UDAT_YEAR_NAME_FIELD:
        return &JSAtomState::year;

      case UDAT_MONTH_FIELD:
      case UDAT_STANDALONE_MONTH_FIELD:
        return &JSAtomState::month;

      case UDAT_DATE_FIELD:
      case UDAT_JULIAN_DAY_FIELD:
        return &JSAtomState::day;

      case UDAT_HOUR_OF_DAY1_FIELD:
      case UDAT_HOUR_OF_DAY0_FIELD:
      case UDAT_HOUR1_FIELD:
      case UDAT_HOUR0_FIELD:
        return &JSAtomState::hour;

      case UDAT_MINUTE_FIELD:
        return &JSAtomState::minute;

      case UDAT_SECOND_FIELD:
        return &JSAtomState::second;

      case UDAT_DAY_OF_WEEK_FIELD:
      case UDAT_STANDALONE_DAY_FIELD:
      case UDAT_DOW_LOCAL_FIELD:
      case UDAT_DAY_OF_WEEK_IN_MONTH_FIELD:
        return &JSAtomState::weekday;

      case UDAT_AM_PM_FIELD:
        return &JSAtomState::dayPeriod;

      case UDAT_TIMEZONE_FIELD:
        return &JSAtomState::timeZoneName;

      case UDAT_FRACTIONAL_SECOND_FIELD:
      case UDAT_DAY_OF_YEAR_FIELD:
      case UDAT_WEEK_OF_YEAR_FIELD:
      case UDAT_WEEK_OF_MONTH_FIELD:
      case UDAT_MILLISECONDS_IN_DAY_FIELD:
      case UDAT_TIMEZONE_RFC_FIELD:
      case UDAT_TIMEZONE_GENERIC_FIELD:
      case UDAT_QUARTER_FIELD:
      case UDAT_STANDALONE_QUARTER_FIELD:
      case UDAT_TIMEZONE_SPECIAL_FIELD:
      case UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD:
      case UDAT_TIMEZONE_ISO_FIELD:
      case UDAT_TIMEZONE_ISO_LOCAL_FIELD:
#ifndef U_HIDE_INTERNAL_API
      case UDAT_RELATED_YEAR_FIELD:
#endif
#ifndef U_HIDE_DRAFT_API
      case UDAT_AM_PM_MIDNIGHT_NOON_FIELD:
      case UDAT_FLEXIBLE_DAY_PERIOD_FIELD:
#endif
#ifndef U_HIDE_INTERNAL_API
      case UDAT_TIME_SEPARATOR_FIELD:
#endif
        // These fields are all unsupported.
        return nullptr;

      case UDAT_FIELD_COUNT:
        MOZ_ASSERT_UNREACHABLE("format field sentinel value returned by "
                               "iterator!");
    }

    MOZ_ASSERT_UNREACHABLE("unenumerated, undocumented format field returned "
                           "by iterator");
    return nullptr;
}

static bool
intl_FormatToPartsDateTime(JSContext* cx, UDateFormat* df, double x, MutableHandleValue result)
{
    if (!IsFinite(x)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DATE_NOT_FINITE);
        return false;
    }

    Vector<char16_t, INITIAL_CHAR_BUFFER_SIZE> chars(cx);
    if (!chars.resize(INITIAL_CHAR_BUFFER_SIZE))
        return false;

    UErrorCode status = U_ZERO_ERROR;
    UFieldPositionIterator* fpositer = ufieldpositer_open(&status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }
    auto closeFieldPosIter = MakeScopeExit([&]() { ufieldpositer_close(fpositer); });

    int32_t resultSize =
        udat_formatForFields(df, x, Char16ToUChar(chars.begin()), INITIAL_CHAR_BUFFER_SIZE,
                             fpositer, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        MOZ_ASSERT(resultSize >= 0);
        if (!chars.resize(resultSize))
            return false;
        status = U_ZERO_ERROR;
        udat_formatForFields(df, x, Char16ToUChar(chars.begin()), resultSize, fpositer, &status);
    }
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    RootedArrayObject partsArray(cx, NewDenseEmptyArray(cx));
    if (!partsArray)
        return false;

    MOZ_ASSERT(resultSize >= 0);
    if (resultSize == 0) {
        // An empty string contains no parts, so avoid extra work below.
        result.setObject(*partsArray);
        return true;
    }

    RootedString overallResult(cx, NewStringCopyN<CanGC>(cx, chars.begin(), resultSize));
    if (!overallResult)
        return false;

    size_t lastEndIndex = 0;

    uint32_t partIndex = 0;
    RootedObject singlePart(cx);
    RootedValue partType(cx);
    RootedString partSubstr(cx);
    RootedValue val(cx);

    auto AppendPart = [&](FieldType type, size_t beginIndex, size_t endIndex) {
        singlePart = NewBuiltinClassInstance<PlainObject>(cx);
        if (!singlePart)
            return false;

        partType = StringValue(cx->names().*type);
        if (!DefineProperty(cx, singlePart, cx->names().type, partType))
            return false;

        partSubstr = SubstringKernel(cx, overallResult, beginIndex, endIndex - beginIndex);
        if (!partSubstr)
            return false;

        val = StringValue(partSubstr);
        if (!DefineProperty(cx, singlePart, cx->names().value, val))
            return false;

        val = ObjectValue(*singlePart);
        if (!DefineElement(cx, partsArray, partIndex, val))
            return false;

        lastEndIndex = endIndex;
        partIndex++;
        return true;
    };

    int32_t fieldInt, beginIndexInt, endIndexInt;
    while ((fieldInt = ufieldpositer_next(fpositer, &beginIndexInt, &endIndexInt)) >= 0) {
        MOZ_ASSERT(beginIndexInt >= 0);
        MOZ_ASSERT(endIndexInt >= 0);
        MOZ_ASSERT(beginIndexInt <= endIndexInt,
                   "field iterator returning invalid range");

        size_t beginIndex(beginIndexInt);
        size_t endIndex(endIndexInt);

        // Technically this isn't guaranteed.  But it appears true in pratice,
        // and http://bugs.icu-project.org/trac/ticket/12024 is expected to
        // correct the documentation lapse.
        MOZ_ASSERT(lastEndIndex <= beginIndex,
                   "field iteration didn't return fields in order start to "
                   "finish as expected");

        if (FieldType type = GetFieldTypeForFormatField(static_cast<UDateFormatField>(fieldInt))) {
            if (lastEndIndex < beginIndex) {
                if (!AppendPart(&JSAtomState::literal, lastEndIndex, beginIndex))
                    return false;
            }

            if (!AppendPart(type, beginIndex, endIndex))
                return false;
        }
    }

    // Append any final literal.
    if (lastEndIndex < overallResult->length()) {
        if (!AppendPart(&JSAtomState::literal, lastEndIndex, overallResult->length()))
            return false;
    }

    result.setObject(*partsArray);
    return true;
}

bool
js::intl_FormatDateTime(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);
    MOZ_ASSERT(args[0].isObject());
    MOZ_ASSERT(args[1].isNumber());
    MOZ_ASSERT(args[2].isBoolean());

    RootedObject dateTimeFormat(cx, &args[0].toObject());

    // Obtain a UDateFormat object, cached if possible.
    bool isDateTimeFormatInstance = dateTimeFormat->getClass() == &DateTimeFormatClass;
    UDateFormat* df;
    if (isDateTimeFormatInstance) {
        void* priv =
            dateTimeFormat->as<NativeObject>().getReservedSlot(UDATE_FORMAT_SLOT).toPrivate();
        df = static_cast<UDateFormat*>(priv);
        if (!df) {
            df = NewUDateFormat(cx, dateTimeFormat);
            if (!df)
                return false;
            dateTimeFormat->as<NativeObject>().setReservedSlot(UDATE_FORMAT_SLOT, PrivateValue(df));
        }
    } else {
        // There's no good place to cache the ICU date-time format for an object
        // that has been initialized as a DateTimeFormat but is not a
        // DateTimeFormat instance. One possibility might be to add a
        // DateTimeFormat instance as an internal property to each such object.
        df = NewUDateFormat(cx, dateTimeFormat);
        if (!df)
            return false;
    }

    // Use the UDateFormat to actually format the time stamp.
    RootedValue result(cx);
    bool success = args[2].toBoolean()
                   ? intl_FormatToPartsDateTime(cx, df, args[1].toNumber(), &result)
                   : intl_FormatDateTime(cx, df, args[1].toNumber(), &result);

    if (!isDateTimeFormatInstance)
        udat_close(df);
    if (!success)
        return false;
    args.rval().set(result);
    return true;
}

bool
js::intl_GetCalendarInfo(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;

    UErrorCode status = U_ZERO_ERROR;
    const UChar* uTimeZone = nullptr;
    int32_t uTimeZoneLength = 0;
    UCalendar* cal = ucal_open(uTimeZone, uTimeZoneLength, locale.ptr(), UCAL_DEFAULT, &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }
    ScopedICUObject<UCalendar, ucal_close> toClose(cal);

    RootedObject info(cx, NewBuiltinClassInstance<PlainObject>(cx));
    if (!info)
        return false;

    RootedValue v(cx);
    int32_t firstDayOfWeek = ucal_getAttribute(cal, UCAL_FIRST_DAY_OF_WEEK);
    v.setInt32(firstDayOfWeek);

    if (!DefineProperty(cx, info, cx->names().firstDayOfWeek, v))
        return false;

    int32_t minDays = ucal_getAttribute(cal, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK);
    v.setInt32(minDays);
    if (!DefineProperty(cx, info, cx->names().minDays, v))
        return false;

    UCalendarWeekdayType prevDayType = ucal_getDayOfWeekType(cal, UCAL_SATURDAY, &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    RootedValue weekendStart(cx), weekendEnd(cx);

    for (int i = UCAL_SUNDAY; i <= UCAL_SATURDAY; i++) {
        UCalendarDaysOfWeek dayOfWeek = static_cast<UCalendarDaysOfWeek>(i);
        UCalendarWeekdayType type = ucal_getDayOfWeekType(cal, dayOfWeek, &status);
        if (U_FAILURE(status)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
            return false;
        }

        if (prevDayType != type) {
            switch (type) {
              case UCAL_WEEKDAY:
                // If the first Weekday after Weekend is Sunday (1),
                // then the last Weekend day is Saturday (7).
                // Otherwise we'll just take the previous days number.
                weekendEnd.setInt32(i == 1 ? 7 : i - 1);
                break;
              case UCAL_WEEKEND:
                weekendStart.setInt32(i);
                break;
              case UCAL_WEEKEND_ONSET:
              case UCAL_WEEKEND_CEASE:
                // At the time this code was added, ICU apparently never behaves this way,
                // so just throw, so that users will report a bug and we can decide what to
                // do.
                JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
                return false;
              default:
                break;
            }
        }

        prevDayType = type;
    }

    MOZ_ASSERT(weekendStart.isInt32());
    MOZ_ASSERT(weekendEnd.isInt32());

    if (!DefineProperty(cx, info, cx->names().weekendStart, weekendStart))
        return false;

    if (!DefineProperty(cx, info, cx->names().weekendEnd, weekendEnd))
        return false;

    args.rval().setObject(*info);
    return true;
}

/******************** Intl ********************/

const Class js::IntlClass = {
    js_Object_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_Intl)
};

#if JS_HAS_TOSOURCE
static bool
intl_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().Intl);
    return true;
}
#endif

static const JSFunctionSpec intl_static_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,  intl_toSource,        0, 0),
#endif
    JS_SELF_HOSTED_FN("getCanonicalLocales", "Intl_getCanonicalLocales", 1, 0),
    JS_FS_END
};

/**
 * Initializes the Intl Object and its standard built-in properties.
 * Spec: ECMAScript Internationalization API Specification, 8.0, 8.1
 */
bool
GlobalObject::initIntlObject(JSContext* cx, Handle<GlobalObject*> global)
{
    RootedObject proto(cx, global->getOrCreateObjectPrototype(cx));
    if (!proto)
        return false;

    // The |Intl| object is just a plain object with some "static" function
    // properties and some constructor properties.
    RootedObject intl(cx, NewObjectWithGivenProto(cx, &IntlClass, proto, SingletonObject));
    if (!intl)
        return false;

    // Add the static functions.
    if (!JS_DefineFunctions(cx, intl, intl_static_methods))
        return false;

    // Add the constructor properties, computing and returning the relevant
    // prototype objects needed below.
    RootedObject collatorProto(cx, CreateCollatorPrototype(cx, intl, global));
    if (!collatorProto)
        return false;
    RootedObject dateTimeFormatProto(cx, CreateDateTimeFormatPrototype(cx, intl, global));
    if (!dateTimeFormatProto)
        return false;
    RootedObject numberFormatProto(cx, CreateNumberFormatPrototype(cx, intl, global));
    if (!numberFormatProto)
        return false;

    // The |Intl| object is fully set up now, so define the global property.
    RootedValue intlValue(cx, ObjectValue(*intl));
    if (!DefineProperty(cx, global, cx->names().Intl, intlValue, nullptr, nullptr,
                        JSPROP_RESOLVING))
    {
        return false;
    }

    // Now that the |Intl| object is successfully added, we can OOM-safely fill
    // in all relevant reserved global slots.

    // Cache the various prototypes, for use in creating instances of these
    // objects with the proper [[Prototype]] as "the original value of
    // |Intl.Collator.prototype|" and similar.  For builtin classes like
    // |String.prototype| we have |JSProto_*| that enables
    // |getPrototype(JSProto_*)|, but that has global-object-property-related
    // baggage we don't need or want, so we use one-off reserved slots.
    global->setReservedSlot(COLLATOR_PROTO, ObjectValue(*collatorProto));
    global->setReservedSlot(DATE_TIME_FORMAT_PROTO, ObjectValue(*dateTimeFormatProto));
    global->setReservedSlot(NUMBER_FORMAT_PROTO, ObjectValue(*numberFormatProto));

    // Also cache |Intl| to implement spec language that conditions behavior
    // based on values being equal to "the standard built-in |Intl| object".
    // Use |setConstructor| to correspond with |JSProto_Intl|.
    //
    // XXX We should possibly do a one-off reserved slot like above.
    global->setConstructor(JSProto_Intl, ObjectValue(*intl));
    return true;
}

JSObject*
js::InitIntlClass(JSContext* cx, HandleObject obj)
{
    Handle<GlobalObject*> global = obj.as<GlobalObject>();
    if (!GlobalObject::initIntlObject(cx, global))
        return nullptr;

    return &global->getConstructor(JSProto_Intl).toObject();
}

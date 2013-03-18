/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The Intl module specified by standard ECMA-402,
 * ECMAScript Internationalization API Specification.
 */

#include <string.h>

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsinterp.h"
#include "jsobj.h"

#include "builtin/Intl.h"
#include "vm/GlobalObject.h"
#include "vm/Stack.h"

#if ENABLE_INTL_API
#include "unicode/locid.h"
#include "unicode/numsys.h"
#include "unicode/ucal.h"
#include "unicode/ucol.h"
#include "unicode/udat.h"
#include "unicode/udatpg.h"
#include "unicode/uenum.h"
#include "unicode/unum.h"
#include "unicode/ustring.h"
#endif
#include "unicode/utypes.h"

#include "jsobjinlines.h"

using namespace js;

#if ENABLE_INTL_API
using icu::Locale;
using icu::NumberingSystem;
#endif


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

#ifdef __GNUC__
#  define SUPPRESS_UNUSED_WARNING __attribute__ ((unused))
#else
#  define SUPPRESS_UNUSED_WARNING
#endif

SUPPRESS_UNUSED_WARNING static int32_t
u_strlen(const UChar *s)
{
    MOZ_NOT_REACHED("u_strlen: Intl API disabled");
    return 0;
}

struct UEnumeration;

static int32_t
uenum_count(UEnumeration *en, UErrorCode *status)
{
    MOZ_NOT_REACHED("uenum_count: Intl API disabled");
    *status = U_UNSUPPORTED_ERROR;
    return 0;
}

static const char *
uenum_next(UEnumeration *en, int32_t *resultLength, UErrorCode *status)
{
    MOZ_NOT_REACHED("uenum_next: Intl API disabled");
    *status = U_UNSUPPORTED_ERROR;
    return NULL;
}

static void
uenum_close(UEnumeration *en)
{
    MOZ_NOT_REACHED("uenum_close: Intl API disabled");
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

static int32_t
ucol_countAvailable(void)
{
    MOZ_NOT_REACHED("ucol_countAvailable: Intl API disabled");
    return 0;
}

static const char *
ucol_getAvailable(int32_t localeIndex)
{
    MOZ_NOT_REACHED("ucol_getAvailable: Intl API disabled");
    return NULL;
}

static UCollator *
ucol_open(const char *loc, UErrorCode *status)
{
    MOZ_NOT_REACHED("ucol_open: Intl API disabled");
    *status = U_UNSUPPORTED_ERROR;
    return NULL;
}

static void
ucol_setAttribute(UCollator *coll, UColAttribute attr, UColAttributeValue value, UErrorCode *status)
{
    MOZ_NOT_REACHED("ucol_setAttribute: Intl API disabled");
    *status = U_UNSUPPORTED_ERROR;
}

static UCollationResult
ucol_strcoll(const UCollator *coll, const UChar *source, int32_t sourceLength,
             const UChar *target, int32_t targetLength)
{
    MOZ_NOT_REACHED("ucol_strcoll: Intl API disabled");
    return (UCollationResult) 0;
}

static void
ucol_close(UCollator *coll)
{
    MOZ_NOT_REACHED("ucol_close: Intl API disabled");
}

static UEnumeration *
ucol_getKeywordValuesForLocale(const char *key, const char *locale, UBool commonlyUsed,
                               UErrorCode *status)
{
    MOZ_NOT_REACHED("ucol_getKeywordValuesForLocale: Intl API disabled");
    *status = U_UNSUPPORTED_ERROR;
    return NULL;
}

struct UParseError;
struct UFieldPosition;
typedef void *UNumberFormat;

enum UNumberFormatStyle {
    UNUM_DECIMAL = 1,
    UNUM_CURRENCY,
    UNUM_PERCENT,
    UNUM_CURRENCY_ISO,
    UNUM_CURRENCY_PLURAL,
};

enum UNumberFormatAttribute {
  UNUM_GROUPING_USED,
  UNUM_MIN_INTEGER_DIGITS,
  UNUM_MAX_FRACTION_DIGITS,
  UNUM_MIN_FRACTION_DIGITS,
  UNUM_SIGNIFICANT_DIGITS_USED,
  UNUM_MIN_SIGNIFICANT_DIGITS,
  UNUM_MAX_SIGNIFICANT_DIGITS,
};

enum UNumberFormatTextAttribute {
  UNUM_CURRENCY_CODE,
};

SUPPRESS_UNUSED_WARNING static int32_t
unum_countAvailable(void)
{
    MOZ_NOT_REACHED("unum_countAvailable: Intl API disabled");
    return 0;
}

SUPPRESS_UNUSED_WARNING static const char *
unum_getAvailable(int32_t localeIndex)
{
    MOZ_NOT_REACHED("unum_getAvailable: Intl API disabled");
    return NULL;
}

SUPPRESS_UNUSED_WARNING static UNumberFormat *
unum_open(UNumberFormatStyle style, const UChar *pattern, int32_t patternLength,
          const char *locale, UParseError *parseErr, UErrorCode *status)
{
    MOZ_NOT_REACHED("unum_open: Intl API disabled");
    *status = U_UNSUPPORTED_ERROR;
    return NULL;
}

SUPPRESS_UNUSED_WARNING static void
unum_setAttribute(UNumberFormat *fmt, UNumberFormatAttribute  attr, int32_t newValue)
{
    MOZ_NOT_REACHED("unum_setAttribute: Intl API disabled");
}

SUPPRESS_UNUSED_WARNING static int32_t
unum_formatDouble(const UNumberFormat *fmt, double number, UChar *result,
                  int32_t resultLength, UFieldPosition *pos, UErrorCode *status)
{
    MOZ_NOT_REACHED("unum_formatDouble: Intl API disabled");
    *status = U_UNSUPPORTED_ERROR;
    return 0;
}

SUPPRESS_UNUSED_WARNING static void
unum_close(UNumberFormat *fmt)
{
    MOZ_NOT_REACHED("unum_close: Intl API disabled");
}

SUPPRESS_UNUSED_WARNING static void
unum_setTextAttribute(UNumberFormat *fmt, UNumberFormatTextAttribute tag, const UChar *newValue,
                      int32_t newValueLength, UErrorCode *status)
{
    MOZ_NOT_REACHED("unum_setTextAttribute: Intl API disabled");
    *status = U_UNSUPPORTED_ERROR;
}

class Locale {
  public:
    Locale(const char *language, const char *country = 0, const char *variant = 0,
           const char *keywordsAndValues = 0);
};

Locale::Locale(const char *language, const char *country, const char *variant,
               const char *keywordsAndValues)
{
    MOZ_NOT_REACHED("Locale::Locale: Intl API disabled");
}

class NumberingSystem {
  public:
    static NumberingSystem *createInstance(const Locale &inLocale, UErrorCode &status);
    const char *getName();
};

NumberingSystem *
NumberingSystem::createInstance(const Locale &inLocale, UErrorCode &status)
{
    MOZ_NOT_REACHED("NumberingSystem::createInstance: Intl API disabled");
    status = U_UNSUPPORTED_ERROR;
    return NULL;
}

const char *
NumberingSystem::getName()
{
    MOZ_NOT_REACHED("NumberingSystem::getName: Intl API disabled");
    return NULL;
}

typedef void *UCalendar;

enum UCalendarType {
  UCAL_TRADITIONAL,
  UCAL_DEFAULT = UCAL_TRADITIONAL,
  UCAL_GREGORIAN
};

SUPPRESS_UNUSED_WARNING static UCalendar *
ucal_open(const UChar *zoneID, int32_t len, const char *locale,
          UCalendarType type, UErrorCode *status)
{
    MOZ_NOT_REACHED("ucal_open: Intl API disabled");
    *status = U_UNSUPPORTED_ERROR;
    return NULL;
}

SUPPRESS_UNUSED_WARNING static const char *
ucal_getType(const UCalendar *cal, UErrorCode *status)
{
    MOZ_NOT_REACHED("ucal_getType: Intl API disabled");
    *status = U_UNSUPPORTED_ERROR;
    return NULL;
}

SUPPRESS_UNUSED_WARNING static UEnumeration *
ucal_getKeywordValuesForLocale(const char *key, const char *locale,
                               UBool commonlyUsed, UErrorCode *status)
{
    MOZ_NOT_REACHED("ucal_getKeywordValuesForLocale: Intl API disabled");
    *status = U_UNSUPPORTED_ERROR;
    return NULL;
}

SUPPRESS_UNUSED_WARNING static void
ucal_close(UCalendar *cal)
{
    MOZ_NOT_REACHED("ucal_close: Intl API disabled");
}

typedef void *UDateTimePatternGenerator;

SUPPRESS_UNUSED_WARNING static UDateTimePatternGenerator *
udatpg_open(const char *locale, UErrorCode *pErrorCode)
{
    MOZ_NOT_REACHED("udatpg_open: Intl API disabled");
    *pErrorCode = U_UNSUPPORTED_ERROR;
    return NULL;
}

SUPPRESS_UNUSED_WARNING static int32_t
udatpg_getBestPattern(UDateTimePatternGenerator *dtpg, const UChar *skeleton,
                      int32_t length, UChar *bestPattern, int32_t capacity,
                      UErrorCode *pErrorCode)
{
    MOZ_NOT_REACHED("udatpg_getBestPattern: Intl API disabled");
    *pErrorCode = U_UNSUPPORTED_ERROR;
    return 0;
}

SUPPRESS_UNUSED_WARNING static void
udatpg_close(UDateTimePatternGenerator *dtpg)
{
    MOZ_NOT_REACHED("udatpg_close: Intl API disabled");
}

typedef void *UCalendar;
typedef void *UDateFormat;

enum UDateFormatStyle {
    UDAT_PATTERN = -2,
    UDAT_IGNORE = UDAT_PATTERN
};

SUPPRESS_UNUSED_WARNING static int32_t
udat_countAvailable(void)
{
    MOZ_NOT_REACHED("udat_countAvailable: Intl API disabled");
    return 0;
}

SUPPRESS_UNUSED_WARNING static const char *
udat_getAvailable(int32_t localeIndex)
{
    MOZ_NOT_REACHED("udat_getAvailable: Intl API disabled");
    return NULL;
}

SUPPRESS_UNUSED_WARNING static UDateFormat *
udat_open(UDateFormatStyle timeStyle, UDateFormatStyle dateStyle, const char *locale,
          const UChar *tzID, int32_t tzIDLength, const UChar *pattern,
          int32_t patternLength, UErrorCode *status)
{
    MOZ_NOT_REACHED("udat_open: Intl API disabled");
    *status = U_UNSUPPORTED_ERROR;
    return NULL;
}

SUPPRESS_UNUSED_WARNING static const UCalendar *
udat_getCalendar(const UDateFormat *fmt)
{
    MOZ_NOT_REACHED("udat_getCalendar: Intl API disabled");
    return NULL;
}

SUPPRESS_UNUSED_WARNING static void
ucal_setGregorianChange(UCalendar *cal, UDate date, UErrorCode *pErrorCode)
{
    MOZ_NOT_REACHED("ucal_setGregorianChange: Intl API disabled");
    *pErrorCode = U_UNSUPPORTED_ERROR;
}

SUPPRESS_UNUSED_WARNING static int32_t
udat_format(const UDateFormat *format, UDate dateToFormat, UChar *result,
            int32_t resultLength, UFieldPosition *position, UErrorCode *status)
{
    MOZ_NOT_REACHED("udat_format: Intl API disabled");
    *status = U_UNSUPPORTED_ERROR;
    return 0;
}

SUPPRESS_UNUSED_WARNING static void
udat_close(UDateFormat *format)
{
    MOZ_NOT_REACHED("udat_close: Intl API disabled");
}

#endif


/******************** Common to Intl constructors ********************/

static bool
IntlInitialize(JSContext *cx, HandleObject obj, Handle<PropertyName*> initializer,
               HandleValue locales, HandleValue options)
{
    RootedValue initializerValue(cx);
    if (!cx->global()->getIntrinsicValue(cx, initializer, &initializerValue))
        return false;
    JS_ASSERT(initializerValue.isObject());
    JS_ASSERT(initializerValue.toObject().isFunction());

    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, 3, &args))
        return false;

    args.setCallee(initializerValue);
    args.setThis(NullValue());
    args[0] = ObjectValue(*obj);
    args[1] = locales;
    args[2] = options;

    return Invoke(cx, args);
}

// CountAvailable and GetAvailable describe the signatures used for ICU API
// to determine available locales for various functionality.
typedef int32_t
(* CountAvailable)(void);

typedef const char *
(* GetAvailable)(int32_t localeIndex);

SUPPRESS_UNUSED_WARNING static bool
intl_availableLocales(JSContext *cx, CountAvailable countAvailable,
                      GetAvailable getAvailable, MutableHandleValue result)
{
    RootedObject locales(cx, NewObjectWithGivenProto(cx, &ObjectClass, NULL, NULL));
    if (!locales)
        return false;

#if ENABLE_INTL_API
    uint32_t count = countAvailable();
    RootedValue t(cx, BooleanValue(true));
    for (uint32_t i = 0; i < count; i++) {
        const char *locale = getAvailable(i);
        ScopedJSFreePtr<char> lang(JS_strdup(cx, locale));
        if (!lang)
            return false;
        char *p;
        while ((p = strchr(lang, '_')))
            *p = '-';
        RootedAtom a(cx, Atomize(cx, lang, strlen(lang)));
        if (!a)
            return false;
        if (!JSObject::defineProperty(cx, locales, a->asPropertyName(), t,
                                      JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE))
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
SUPPRESS_UNUSED_WARNING static bool
GetInternals(JSContext *cx, HandleObject obj, MutableHandleObject internals)
{
    RootedValue getInternalsValue(cx);
    if (!cx->global()->getIntrinsicValue(cx, cx->names().getInternals, &getInternalsValue))
        return false;
    JS_ASSERT(getInternalsValue.isObject());
    JS_ASSERT(getInternalsValue.toObject().isFunction());

    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, 1, &args))
        return false;

    args.setCallee(getInternalsValue);
    args.setThis(NullValue());
    args[0] = ObjectValue(*obj);

    if (!Invoke(cx, args))
        return false;
    internals.set(&args.rval().toObject());
    return true;
}

static bool
equal(const char *s1, const char *s2)
{
    return !strcmp(s1, s2);
}

SUPPRESS_UNUSED_WARNING static bool
equal(JSAutoByteString &s1, const char *s2)
{
    return !strcmp(s1.ptr(), s2);
}

SUPPRESS_UNUSED_WARNING static const char *
icuLocale(const char *locale)
{
    if (equal(locale, "und"))
        return ""; // ICU root locale
    return locale;
}

// Simple RAII for ICU objects. MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE
// unfortunately doesn't work because of namespace incompatibilities
// (TypeSpecificDelete cannot be in icu and mozilla at the same time)
// and because ICU declares both UNumberFormat and UDateTimePatternGenerator
// as void*.
template <typename T>
class ScopedICUObject
{
    T *ptr_;
    void (* deleter_)(T*);

  public:
    ScopedICUObject(T *ptr, void (*deleter)(T*))
      : ptr_(ptr),
        deleter_(deleter)
    {}

    ~ScopedICUObject() {
        if (ptr_)
            deleter_(ptr_);
    }

    // In cases where an object should be deleted on abnormal exits,
    // but returned to the caller if everything goes well, call forget()
    // to transfer the object just before returning.
    T *forget() {
        T *tmp = ptr_;
        ptr_ = NULL;
        return tmp;
    }
};

static const size_t STACK_STRING_SIZE = 50;


/******************** Collator ********************/

static void collator_finalize(FreeOp *fop, JSObject *obj);

static const uint32_t UCOLLATOR_SLOT = 0;
static const uint32_t COLLATOR_SLOTS_COUNT = 1;

static Class CollatorClass = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(COLLATOR_SLOTS_COUNT),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    collator_finalize
};

#if JS_HAS_TOSOURCE
static JSBool
collator_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    vp->setString(cx->names().Collator);
    return true;
}
#endif

static JSFunctionSpec collator_static_methods[] = {
    {"supportedLocalesOf", JSOP_NULLWRAPPER, 1, JSFunction::INTERPRETED, "Intl_Collator_supportedLocalesOf"},
    JS_FS_END
};

static JSFunctionSpec collator_methods[] = {
    {"resolvedOptions", JSOP_NULLWRAPPER, 0, JSFunction::INTERPRETED, "Intl_Collator_resolvedOptions"},
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, collator_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * Collator constructor.
 * Spec: ECMAScript Internationalization API Specification, 10.1
 */
static JSBool
Collator(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject obj(cx);

    bool construct = IsConstructing(args);
    if (!construct) {
        // 10.1.2.1 step 3
        JSObject *intl = cx->global()->getOrCreateIntlObject(cx);
        if (!intl)
            return false;
        RootedValue self(cx, args.thisv());
        if (!self.isUndefined() && (!self.isObject() || self.toObject() != *intl)) {
            // 10.1.2.1 step 4
            obj = ToObject(cx, self);
            if (!obj)
                return false;

            // 10.1.2.1 step 5
            if (!obj->isExtensible())
                return Throw(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE);
        } else {
            // 10.1.2.1 step 3.a
            construct = true;
        }
    }
    if (construct) {
        // 10.1.3.1 paragraph 2
        RootedObject proto(cx, cx->global()->getOrCreateCollatorPrototype(cx));
        if (!proto)
            return false;
        obj = NewObjectWithGivenProto(cx, &CollatorClass, proto, cx->global());
        if (!obj)
            return false;

        obj->setReservedSlot(UCOLLATOR_SLOT, PrivateValue(NULL));
    }

    // 10.1.2.1 steps 1 and 2; 10.1.3.1 steps 1 and 2
    RootedValue locales(cx, args.length() > 0 ? args[0] : UndefinedValue());
    RootedValue options(cx, args.length() > 1 ? args[1] : UndefinedValue());

    // 10.1.2.1 step 6; 10.1.3.1 step 3
    if (!IntlInitialize(cx, obj, cx->names().InitializeCollator, locales, options))
        return false;

    // 10.1.2.1 steps 3.a and 7
    args.rval().setObject(*obj);
    return true;
}

static void
collator_finalize(FreeOp *fop, JSObject *obj)
{
    UCollator *coll = static_cast<UCollator *>(obj->getReservedSlot(UCOLLATOR_SLOT).toPrivate());
    if (coll)
        ucol_close(coll);
}

static JSObject *
InitCollatorClass(JSContext *cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx, global->createConstructor(cx, &Collator, cx->names().Collator, 0));
    if (!ctor)
        return NULL;

    RootedObject proto(cx, global->asGlobal().getOrCreateCollatorPrototype(cx));
    if (!proto)
        return NULL;
    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return NULL;

    // 10.2.2
    if (!JS_DefineFunctions(cx, ctor, collator_static_methods))
        return NULL;

    // 10.3.2 and 10.3.3
    if (!JS_DefineFunctions(cx, proto, collator_methods))
        return NULL;

    /*
     * Install the getter for Collator.prototype.compare, which returns a bound
     * comparison function for the specified Collator object (suitable for
     * passing to methods like Array.prototype.sort).
     */
    RootedValue getter(cx);
    if (!cx->global()->getIntrinsicValue(cx, cx->names().CollatorCompareGet, &getter))
        return NULL;
    RootedValue undefinedValue(cx, UndefinedValue());
    if (!JSObject::defineProperty(cx, proto, cx->names().compare, undefinedValue,
                                  JS_DATA_TO_FUNC_PTR(JSPropertyOp, &getter.toObject()),
                                  NULL, JSPROP_GETTER))
    {
        return NULL;
    }

    // 10.2.1 and 10.3
    RootedValue locales(cx, UndefinedValue());
    RootedValue options(cx, UndefinedValue());
    if (!IntlInitialize(cx, proto, cx->names().InitializeCollator, locales, options))
        return NULL;

    // 8.1
    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!JSObject::defineProperty(cx, Intl, cx->names().Collator, ctorValue,
                                  JS_PropertyStub, JS_StrictPropertyStub, 0))
    {
        return NULL;
    }

    return ctor;
}

bool
GlobalObject::initCollatorProto(JSContext *cx, Handle<GlobalObject*> global)
{
    RootedObject proto(cx, global->createBlankPrototype(cx, &CollatorClass));
    if (!proto)
        return false;
    proto->setReservedSlot(UCOLLATOR_SLOT, PrivateValue(NULL));
    global->setReservedSlot(COLLATOR_PROTO, ObjectValue(*proto));
    return true;
}

JSBool
js::intl_Collator_availableLocales(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 0);

    RootedValue result(cx);
    if (!intl_availableLocales(cx, ucol_countAvailable, ucol_getAvailable, &result))
        return false;
    args.rval().set(result);
    return true;
}

JSBool
js::intl_availableCollations(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isString());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration *values = ucol_getKeywordValuesForLocale("co", locale.ptr(), false, &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }
    ScopedICUObject<UEnumeration> toClose(values, uenum_close);

    uint32_t count = uenum_count(values, &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    RootedObject collations(cx, NewDenseEmptyArray(cx));
    if (!collations)
        return false;

    uint32_t index = 0;
    for (uint32_t i = 0; i < count; i++) {
        const char *collation = uenum_next(values, NULL, &status);
        if (U_FAILURE(status)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INTERNAL_INTL_ERROR);
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
        if (!JSObject::defineElement(cx, collations, index++, element))
            return false;
    }

    args.rval().setObject(*collations);
    return true;
}

/**
 * Returns a new UCollator with the locale and collation options
 * of the given Collator.
 */
static UCollator *
NewUCollator(JSContext *cx, HandleObject collator)
{
    RootedValue value(cx);

    RootedObject internals(cx);
    if (!GetInternals(cx, collator, &internals))
        return NULL;

    if (!JSObject::getProperty(cx, internals, internals, cx->names().locale, &value))
        return NULL;
    JSAutoByteString locale(cx, value.toString());
    if (!locale)
        return NULL;

    // UCollator options with default values.
    UColAttributeValue uStrength = UCOL_DEFAULT;
    UColAttributeValue uCaseLevel = UCOL_OFF;
    UColAttributeValue uAlternate = UCOL_DEFAULT;
    UColAttributeValue uNumeric = UCOL_OFF;
    // Normalization is always on to meet the canonical equivalence requirement.
    UColAttributeValue uNormalization = UCOL_ON;
    UColAttributeValue uCaseFirst = UCOL_DEFAULT;

    if (!JSObject::getProperty(cx, internals, internals, cx->names().usage, &value))
        return NULL;
    JSAutoByteString usage(cx, value.toString());
    if (!usage)
        return NULL;
    if (equal(usage, "search")) {
        // ICU expects search as a Unicode locale extension on locale.
        // Unicode locale extensions must occur before private use extensions.
        const char *oldLocale = locale.ptr();
        const char *p;
        size_t index;
        size_t localeLen = strlen(oldLocale);
        if ((p = strstr(oldLocale, "-x-")))
            index = p - oldLocale;
        else
            index = localeLen;

        const char *insert;
        if ((p = strstr(oldLocale, "-u-")) && static_cast<size_t>(p - oldLocale) < index) {
            index = p - oldLocale + 2;
            insert = "-co-search";
        } else {
            insert = "-u-co-search";
        }
        size_t insertLen = strlen(insert);
        char *newLocale = cx->pod_malloc<char>(localeLen + insertLen + 1);
        if (!newLocale)
            return NULL;
        memcpy(newLocale, oldLocale, index);
        memcpy(newLocale + index, insert, insertLen);
        memcpy(newLocale + index + insertLen, oldLocale + index, localeLen - index + 1); // '\0'
        locale.clear();
        locale.initBytes(newLocale);
    }

    // We don't need to look at the collation property - it can only be set
    // via the Unicode locale extension and is therefore already set on
    // locale.

    if (!JSObject::getProperty(cx, internals, internals, cx->names().sensitivity, &value))
        return NULL;
    JSAutoByteString sensitivity(cx, value.toString());
    if (!sensitivity)
        return NULL;
    if (equal(sensitivity, "base")) {
        uStrength = UCOL_PRIMARY;
    } else if (equal(sensitivity, "accent")) {
        uStrength = UCOL_SECONDARY;
    } else if (equal(sensitivity, "case")) {
        uStrength = UCOL_PRIMARY;
        uCaseLevel = UCOL_ON;
    } else {
        JS_ASSERT(equal(sensitivity, "variant"));
        uStrength = UCOL_TERTIARY;
    }

    if (!JSObject::getProperty(cx, internals, internals, cx->names().ignorePunctuation, &value))
        return NULL;
    // According to the ICU team, UCOL_SHIFTED causes punctuation to be
    // ignored. Looking at Unicode Technical Report 35, Unicode Locale Data
    // Markup Language, "shifted" causes whitespace and punctuation to be
    // ignored - that's a bit more than asked for, but there's no way to get
    // less.
    if (value.toBoolean())
        uAlternate = UCOL_SHIFTED;

    if (!JSObject::getProperty(cx, internals, internals, cx->names().numeric, &value))
        return NULL;
    if (!value.isUndefined() && value.toBoolean())
        uNumeric = UCOL_ON;

    if (!JSObject::getProperty(cx, internals, internals, cx->names().caseFirst, &value))
        return NULL;
    if (!value.isUndefined()) {
        JSAutoByteString caseFirst(cx, value.toString());
        if (!caseFirst)
            return NULL;
        if (equal(caseFirst, "upper"))
            uCaseFirst = UCOL_UPPER_FIRST;
        else if (equal(caseFirst, "lower"))
            uCaseFirst = UCOL_LOWER_FIRST;
        else
            JS_ASSERT(equal(caseFirst, "false"));
    }

    UErrorCode status = U_ZERO_ERROR;
    UCollator *coll = ucol_open(icuLocale(locale.ptr()), &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INTERNAL_INTL_ERROR);
        return NULL;
    }

    // According to http://userguide.icu-project.org/design#TOC-Error-Handling
    // we don't have to check the error code after each function call.
    ucol_setAttribute(coll, UCOL_STRENGTH, uStrength, &status);
    ucol_setAttribute(coll, UCOL_CASE_LEVEL, uCaseLevel, &status);
    ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, uAlternate, &status);
    ucol_setAttribute(coll, UCOL_NUMERIC_COLLATION, uNumeric, &status);
    ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, uNormalization, &status);
    ucol_setAttribute(coll, UCOL_CASE_FIRST, uCaseFirst, &status);
    if (U_FAILURE(status)) {
        ucol_close(coll);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INTERNAL_INTL_ERROR);
        return NULL;
    }

    return coll;
}

static bool
intl_CompareStrings(JSContext *cx, UCollator *coll, HandleString str1, HandleString str2, MutableHandleValue result)
{
    JS_ASSERT(str1);
    JS_ASSERT(str2);

    if (str1 == str2) {
        result.setInt32(0);
        return true;
    }

    size_t length1 = str1->length();
    const jschar *chars1 = str1->getChars(cx);
    if (!chars1)
        return false;
    size_t length2 = str2->length();
    const jschar *chars2 = str2->getChars(cx);
    if (!chars2)
        return false;

    UCollationResult uresult = ucol_strcoll(coll, chars1, length1, chars2, length2);

    int32_t res;
    switch (uresult) {
        case UCOL_LESS: res = -1; break;
        case UCOL_EQUAL: res = 0; break;
        case UCOL_GREATER: res = 1; break;
    }
    result.setInt32(res);
    return true;
}

JSBool
js::intl_CompareStrings(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 3);
    JS_ASSERT(args[0].isObject());
    JS_ASSERT(args[1].isString());
    JS_ASSERT(args[2].isString());

    RootedObject collator(cx, &args[0].toObject());

    // Obtain a UCollator object, cached if possible.
    // XXX Does this handle Collator instances from other globals correctly?
    bool isCollatorInstance = collator->getClass() == &CollatorClass;
    UCollator *coll;
    if (isCollatorInstance) {
        coll = (UCollator *) collator->getReservedSlot(UCOLLATOR_SLOT).toPrivate();
        if (!coll) {
            coll = NewUCollator(cx, collator);
            if (!coll)
                return false;
            collator->setReservedSlot(UCOLLATOR_SLOT, PrivateValue(coll));
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

static void numberFormat_finalize(FreeOp *fop, JSObject *obj);

static const uint32_t UNUMBER_FORMAT_SLOT = 0;
static const uint32_t NUMBER_FORMAT_SLOTS_COUNT = 1;

static Class NumberFormatClass = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(NUMBER_FORMAT_SLOTS_COUNT),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    numberFormat_finalize
};

#if JS_HAS_TOSOURCE
static JSBool
numberFormat_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    vp->setString(cx->names().NumberFormat);
    return true;
}
#endif

static JSFunctionSpec numberFormat_static_methods[] = {
    {"supportedLocalesOf", JSOP_NULLWRAPPER, 1, JSFunction::INTERPRETED, "Intl_NumberFormat_supportedLocalesOf"},
    JS_FS_END
};

static JSFunctionSpec numberFormat_methods[] = {
    {"resolvedOptions", JSOP_NULLWRAPPER, 0, JSFunction::INTERPRETED, "Intl_NumberFormat_resolvedOptions"},
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, numberFormat_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * NumberFormat constructor.
 * Spec: ECMAScript Internationalization API Specification, 11.1
 */
static JSBool
NumberFormat(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject obj(cx);

    bool construct = IsConstructing(args);
    if (!construct) {
        // 11.1.2.1 step 3
        JSObject *intl = cx->global()->getOrCreateIntlObject(cx);
        if (!intl)
            return false;
        RootedValue self(cx, args.thisv());
        if (!self.isUndefined() && (!self.isObject() || self.toObject() != *intl)) {
            // 11.1.2.1 step 4
            obj = ToObject(cx, self);
            if (!obj)
                return false;

            // 11.1.2.1 step 5
            if (!obj->isExtensible())
                return Throw(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE);
        } else {
            // 11.1.2.1 step 3.a
            construct = true;
        }
    }
    if (construct) {
        // 11.1.3.1 paragraph 2
        RootedObject proto(cx, cx->global()->getOrCreateNumberFormatPrototype(cx));
        if (!proto)
            return false;
        obj = NewObjectWithGivenProto(cx, &NumberFormatClass, proto, cx->global());
        if (!obj)
            return false;

        obj->setReservedSlot(UNUMBER_FORMAT_SLOT, PrivateValue(NULL));
    }

    // 11.1.2.1 steps 1 and 2; 11.1.3.1 steps 1 and 2
    RootedValue locales(cx, args.length() > 0 ? args[0] : UndefinedValue());
    RootedValue options(cx, args.length() > 1 ? args[1] : UndefinedValue());

    // 11.1.2.1 step 6; 11.1.3.1 step 3
    if (!IntlInitialize(cx, obj, cx->names().InitializeNumberFormat, locales, options))
        return false;

    // 11.1.2.1 steps 3.a and 7
    args.rval().setObject(*obj);
    return true;
}

static void
numberFormat_finalize(FreeOp *fop, JSObject *obj)
{
    UNumberFormat *nf = static_cast<UNumberFormat *>(obj->getReservedSlot(UNUMBER_FORMAT_SLOT).toPrivate());
    if (nf)
        unum_close(nf);
}

static JSObject *
InitNumberFormatClass(JSContext *cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx, global->createConstructor(cx, &NumberFormat, cx->names().NumberFormat, 0));
    if (!ctor)
        return NULL;

    RootedObject proto(cx, global->asGlobal().getOrCreateNumberFormatPrototype(cx));
    if (!proto)
        return NULL;
    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return NULL;

    // 11.2.2
    if (!JS_DefineFunctions(cx, ctor, numberFormat_static_methods))
        return NULL;

    // 11.3.2 and 11.3.3
    if (!JS_DefineFunctions(cx, proto, numberFormat_methods))
        return NULL;

    /*
     * Install the getter for NumberFormat.prototype.format, which returns a
     * bound formatting function for the specified NumberFormat object (suitable
     * for passing to methods like Array.prototype.map).
     */
    RootedValue getter(cx);
    if (!cx->global()->getIntrinsicValue(cx, cx->names().NumberFormatFormatGet, &getter))
        return NULL;
    RootedValue undefinedValue(cx, UndefinedValue());
    if (!JSObject::defineProperty(cx, proto, cx->names().format, undefinedValue,
                                  JS_DATA_TO_FUNC_PTR(JSPropertyOp, &getter.toObject()),
                                  NULL, JSPROP_GETTER))
    {
        return NULL;
    }

    // 11.2.1 and 11.3
    RootedValue locales(cx, UndefinedValue());
    RootedValue options(cx, UndefinedValue());
    if (!IntlInitialize(cx, proto, cx->names().InitializeNumberFormat, locales, options))
        return NULL;

    // 8.1
    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!JSObject::defineProperty(cx, Intl, cx->names().NumberFormat, ctorValue,
                                  JS_PropertyStub, JS_StrictPropertyStub, 0))
    {
        return NULL;
    }

    return ctor;
}

bool
GlobalObject::initNumberFormatProto(JSContext *cx, Handle<GlobalObject*> global)
{
    RootedObject proto(cx, global->createBlankPrototype(cx, &NumberFormatClass));
    if (!proto)
        return false;
    proto->setReservedSlot(UNUMBER_FORMAT_SLOT, PrivateValue(NULL));
    global->setReservedSlot(NUMBER_FORMAT_PROTO, ObjectValue(*proto));
    return true;
}

JSBool
js::intl_NumberFormat_availableLocales(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 0);

    RootedValue result(cx);
    if (!intl_availableLocales(cx, unum_countAvailable, unum_getAvailable, &result))
        return false;
    args.rval().set(result);
    return true;
}

JSBool
js::intl_numberingSystem(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isString());

    JSAutoByteString locale(cx, args[0].toString());
    if (!locale)
        return false;

    // There's no C API for numbering system, so use the C++ API and hope it
    // won't break. http://bugs.icu-project.org/trac/ticket/10039
    Locale ulocale(locale.ptr());
    UErrorCode status = U_ZERO_ERROR;
    NumberingSystem *numbers = NumberingSystem::createInstance(ulocale, status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }
    const char *name = numbers->getName();
    JSString *jsname = JS_NewStringCopyZ(cx, name);
    delete numbers;
    if (!jsname)
        return false;
    args.rval().setString(jsname);
    return true;
}

/******************** DateTimeFormat ********************/

static Class DateTimeFormatClass = {
    js_Object_str,
    0,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

#if JS_HAS_TOSOURCE
static JSBool
dateTimeFormat_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    vp->setString(cx->names().DateTimeFormat);
    return true;
}
#endif

static JSFunctionSpec dateTimeFormat_static_methods[] = {
    {"supportedLocalesOf", JSOP_NULLWRAPPER, 1, JSFunction::INTERPRETED, "Intl_DateTimeFormat_supportedLocalesOf"},
    JS_FS_END
};

static JSFunctionSpec dateTimeFormat_methods[] = {
    {"resolvedOptions", JSOP_NULLWRAPPER, 0, JSFunction::INTERPRETED, "Intl_DateTimeFormat_resolvedOptions"},
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, dateTimeFormat_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * DateTimeFormat constructor.
 * Spec: ECMAScript Internationalization API Specification, 12.1
 */
static JSBool
DateTimeFormat(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject obj(cx);

    bool construct = IsConstructing(args);
    if (!construct) {
        // 12.1.2.1 step 3
        JSObject *intl = cx->global()->getOrCreateIntlObject(cx);
        if (!intl)
            return false;
        RootedValue self(cx, args.thisv());
        if (!self.isUndefined() && (!self.isObject() || self.toObject() != *intl)) {
            // 12.1.2.1 step 4
            obj = ToObject(cx, self);
            if (!obj)
                return false;
            // 12.1.2.1 step 5
            if (!obj->isExtensible())
                return Throw(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE);
        } else {
            // 12.1.2.1 step 3.a
            construct = true;
        }
    }
    if (construct) {
        // 12.1.3.1 paragraph 2
        RootedObject proto(cx, cx->global()->getOrCreateDateTimeFormatPrototype(cx));
        if (!proto)
            return false;
        obj = NewObjectWithGivenProto(cx, &DateTimeFormatClass, proto, cx->global());
        if (!obj)
            return false;
    }

    // 12.1.2.1 steps 1 and 2; 12.1.3.1 steps 1 and 2
    RootedValue locales(cx, args.length() > 0 ? args[0] : UndefinedValue());
    RootedValue options(cx, args.length() > 1 ? args[1] : UndefinedValue());
    // 12.1.2.1 step 6; 12.1.3.1 step 3
    if (!IntlInitialize(cx, obj, cx->names().InitializeDateTimeFormat, locales, options))
        return false;

    // 12.1.2.1 steps 3.a and 7
    args.rval().setObject(*obj);
    return true;
}

static JSObject *
InitDateTimeFormatClass(JSContext *cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx, global->createConstructor(cx, &DateTimeFormat, cx->names().DateTimeFormat, 0));
    if (!ctor)
        return NULL;

    RootedObject proto(cx, global->asGlobal().getOrCreateDateTimeFormatPrototype(cx));
    if (!proto)
        return NULL;
    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return NULL;

    // 12.2.2
    if (!JS_DefineFunctions(cx, ctor, dateTimeFormat_static_methods))
        return NULL;

    // 12.3.2 and 12.3.3
    if (!JS_DefineFunctions(cx, proto, dateTimeFormat_methods))
        return NULL;

    /*
     * Install the getter for DateTimeFormat.prototype.format, which returns a
     * bound formatting function for the specified DateTimeFormat object
     * (suitable for passing to methods like Array.prototype.map).
     */
    RootedValue getter(cx);
    if (!cx->global()->getIntrinsicValue(cx, cx->names().DateTimeFormatFormatGet, &getter))
        return NULL;
    RootedValue undefinedValue(cx, UndefinedValue());
    if (!JSObject::defineProperty(cx, proto, cx->names().format, undefinedValue,
                                  JS_DATA_TO_FUNC_PTR(JSPropertyOp, &getter.toObject()),
                                  NULL, JSPROP_GETTER)) {
        return NULL;
    }

    // 12.2.1 and 12.3
    RootedValue locales(cx, UndefinedValue());
    RootedValue options(cx, UndefinedValue());
    if (!IntlInitialize(cx, proto, cx->names().InitializeDateTimeFormat, locales, options))
        return NULL;

    // 8.1
    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!JSObject::defineProperty(cx, Intl, cx->names().DateTimeFormat, ctorValue,
                                  JS_PropertyStub, JS_StrictPropertyStub, 0)) {
        return NULL;
    }

    return ctor;
}

bool
GlobalObject::initDateTimeFormatProto(JSContext *cx, Handle<GlobalObject*> global)
{
    RootedObject proto(cx, global->createBlankPrototype(cx, &DateTimeFormatClass));
    if (!proto)
        return false;
    global->setReservedSlot(DATE_TIME_FORMAT_PROTO, ObjectValue(*proto));
    return true;
}


/******************** Intl ********************/

Class js::IntlClass = {
    js_Object_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_Intl),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

#if JS_HAS_TOSOURCE
static JSBool
intl_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    vp->setString(cx->names().Intl);
    return true;
}
#endif

static JSFunctionSpec intl_static_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,  intl_toSource,        0, 0),
#endif
    JS_FS_END
};

/**
 * Initializes the Intl Object and its standard built-in properties.
 * Spec: ECMAScript Internationalization API Specification, 8.0, 8.1
 */
JSObject *
js_InitIntlClass(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isGlobal());
    Rooted<GlobalObject*> global(cx, &obj->asGlobal());

    // The constructors above need to be able to determine whether they've been
    // called with this being "the standard built-in Intl object". The global
    // object reserves slots to track standard built-in objects, but doesn't
    // normally keep references to non-constructors. This makes sure there is one.
    RootedObject Intl(cx, global->getOrCreateIntlObject(cx));
    if (!Intl)
        return NULL;

    RootedValue IntlValue(cx, ObjectValue(*Intl));
    if (!JSObject::defineProperty(cx, global, cx->names().Intl, IntlValue,
                                  JS_PropertyStub, JS_StrictPropertyStub, 0)) {
        return NULL;
    }

    if (!JS_DefineFunctions(cx, Intl, intl_static_methods))
        return NULL;

    // Skip initialization of the Intl constructors during initialization of the
    // self-hosting global as we may get here before self-hosted code is compiled,
    // and no core code refers to the Intl classes.
    if (!cx->runtime->isSelfHostingGlobal(cx->global())) {
        if (!InitCollatorClass(cx, Intl, global))
            return NULL;
        if (!InitNumberFormatClass(cx, Intl, global))
            return NULL;
        if (!InitDateTimeFormatClass(cx, Intl, global))
            return NULL;
    }

    MarkStandardClassInitializedNoProto(global, &IntlClass);

    return Intl;
}

bool
GlobalObject::initIntlObject(JSContext *cx, Handle<GlobalObject*> global)
{
    RootedObject Intl(cx);
    Intl = NewObjectWithGivenProto(cx, &IntlClass, global->getOrCreateObjectPrototype(cx),
                                   global, SingletonObject);
    if (!Intl)
        return false;

    global->setReservedSlot(JSProto_Intl, ObjectValue(*Intl));
    return true;
}

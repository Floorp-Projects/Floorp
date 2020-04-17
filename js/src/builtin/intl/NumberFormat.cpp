/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Intl.NumberFormat implementation. */

#include "builtin/intl/NumberFormat.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/UniquePtr.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <type_traits>

#include "builtin/Array.h"
#include "builtin/intl/CommonFunctions.h"
#include "builtin/intl/LanguageTag.h"
#include "builtin/intl/RelativeTimeFormat.h"
#include "builtin/intl/ScopedICUObject.h"
#include "ds/Sort.h"
#include "gc/FreeOp.h"
#include "js/CharacterEncoding.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Vector.h"
#include "unicode/udata.h"
#include "unicode/ufieldpositer.h"
#include "unicode/uformattedvalue.h"
#include "unicode/unum.h"
#include "unicode/unumberformatter.h"
#include "unicode/unumsys.h"
#include "unicode/ures.h"
#include "unicode/utypes.h"
#include "vm/BigIntType.h"
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"
#include "vm/PlainObject.h"  // js::PlainObject
#include "vm/SelfHosting.h"
#include "vm/Stack.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"

using namespace js;

using mozilla::AssertedCast;
using mozilla::IsFinite;
using mozilla::IsNaN;
using mozilla::IsNegative;
using mozilla::SpecificNaN;

using js::intl::CallICU;
using js::intl::DateTimeFormatOptions;
using js::intl::FieldType;
using js::intl::IcuLocale;

const JSClassOps NumberFormatObject::classOps_ = {
    nullptr,                       // addProperty
    nullptr,                       // delProperty
    nullptr,                       // enumerate
    nullptr,                       // newEnumerate
    nullptr,                       // resolve
    nullptr,                       // mayResolve
    NumberFormatObject::finalize,  // finalize
    nullptr,                       // call
    nullptr,                       // hasInstance
    nullptr,                       // construct
    nullptr,                       // trace
};

const JSClass NumberFormatObject::class_ = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(NumberFormatObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_NumberFormat) |
        JSCLASS_FOREGROUND_FINALIZE,
    &NumberFormatObject::classOps_, &NumberFormatObject::classSpec_};

const JSClass& NumberFormatObject::protoClass_ = PlainObject::class_;

static bool numberFormat_toSource(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setString(cx->names().NumberFormat);
  return true;
}

static const JSFunctionSpec numberFormat_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf",
                      "Intl_NumberFormat_supportedLocalesOf", 1, 0),
    JS_FS_END};

static const JSFunctionSpec numberFormat_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_NumberFormat_resolvedOptions", 0,
                      0),
    JS_SELF_HOSTED_FN("formatToParts", "Intl_NumberFormat_formatToParts", 1, 0),
    JS_FN(js_toSource_str, numberFormat_toSource, 0, 0), JS_FS_END};

static const JSPropertySpec numberFormat_properties[] = {
    JS_SELF_HOSTED_GET("format", "$Intl_NumberFormat_format_get", 0),
    JS_STRING_SYM_PS(toStringTag, "Object", JSPROP_READONLY), JS_PS_END};

static bool NumberFormat(JSContext* cx, unsigned argc, Value* vp);

const ClassSpec NumberFormatObject::classSpec_ = {
    GenericCreateConstructor<NumberFormat, 0, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<NumberFormatObject>,
    numberFormat_static_methods,
    nullptr,
    numberFormat_methods,
    numberFormat_properties,
    nullptr,
    ClassSpec::DontDefineConstructor};

/**
 * 11.2.1 Intl.NumberFormat([ locales [, options]])
 *
 * ES2017 Intl draft rev 94045d234762ad107a3d09bb6f7381a65f1a2f9b
 */
static bool NumberFormat(JSContext* cx, const CallArgs& args, bool construct) {
  // Step 1 (Handled by OrdinaryCreateFromConstructor fallback code).

  // Step 2 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_NumberFormat,
                                          &proto)) {
    return false;
  }

  Rooted<NumberFormatObject*> numberFormat(cx);
  numberFormat = NewObjectWithClassProto<NumberFormatObject>(cx, proto);
  if (!numberFormat) {
    return false;
  }

  RootedValue thisValue(cx,
                        construct ? ObjectValue(*numberFormat) : args.thisv());
  HandleValue locales = args.get(0);
  HandleValue options = args.get(1);

  // Step 3.
  return intl::LegacyInitializeObject(
      cx, numberFormat, cx->names().InitializeNumberFormat, thisValue, locales,
      options, DateTimeFormatOptions::Standard, args.rval());
}

static bool NumberFormat(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return NumberFormat(cx, args, args.isConstructing());
}

bool js::intl_NumberFormat(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 2);
  MOZ_ASSERT(!args.isConstructing());
  // intl_NumberFormat is an intrinsic for self-hosted JavaScript, so it
  // cannot be used with "new", but it still has to be treated as a
  // constructor.
  return NumberFormat(cx, args, true);
}

void js::NumberFormatObject::finalize(JSFreeOp* fop, JSObject* obj) {
  MOZ_ASSERT(fop->onMainThread());

  auto* numberFormat = &obj->as<NumberFormatObject>();
  UNumberFormatter* nf = numberFormat->getNumberFormatter();
  UFormattedNumber* formatted = numberFormat->getFormattedNumber();

  if (nf) {
    intl::RemoveICUCellMemory(fop, obj, NumberFormatObject::EstimatedMemoryUse);

    unumf_close(nf);
  }
  if (formatted) {
    // UFormattedNumber memory tracked as part of UNumberFormatter.

    unumf_closeResult(formatted);
  }
}

bool js::intl_numberingSystem(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 1);
  MOZ_ASSERT(args[0].isString());

  UniqueChars locale = intl::EncodeLocale(cx, args[0].toString());
  if (!locale) {
    return false;
  }

  UErrorCode status = U_ZERO_ERROR;
  UNumberingSystem* numbers = unumsys_open(IcuLocale(locale.get()), &status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return false;
  }

  ScopedICUObject<UNumberingSystem, unumsys_close> toClose(numbers);

  const char* name = unumsys_getName(numbers);
  if (!name) {
    intl::ReportInternalError(cx);
    return false;
  }

  JSString* jsname = NewStringCopyZ<CanGC>(cx, name);
  if (!jsname) {
    return false;
  }

  args.rval().setString(jsname);
  return true;
}

#if DEBUG || MOZ_SYSTEM_ICU
class UResourceBundleDeleter {
 public:
  void operator()(UResourceBundle* aPtr) { ures_close(aPtr); }
};

using UniqueUResourceBundle =
    mozilla::UniquePtr<UResourceBundle, UResourceBundleDeleter>;

bool js::intl_availableMeasurementUnits(JSContext* cx, unsigned argc,
                                        Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 0);

  RootedObject measurementUnits(
      cx, NewObjectWithGivenProto<PlainObject>(cx, nullptr));
  if (!measurementUnits) {
    return false;
  }

  // Lookup the available measurement units in the resource boundle of the root
  // locale.

  static const char packageName[] =
      U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "unit";
  static const char rootLocale[] = "";

  UErrorCode status = U_ZERO_ERROR;
  UResourceBundle* rawRes = ures_open(packageName, rootLocale, &status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return false;
  }
  UniqueUResourceBundle res(rawRes);

  UResourceBundle* rawUnits =
      ures_getByKey(res.get(), "units", nullptr, &status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return false;
  }
  UniqueUResourceBundle units(rawUnits);

  RootedAtom unitAtom(cx);

  int32_t unitsSize = ures_getSize(units.get());
  for (int32_t i = 0; i < unitsSize; i++) {
    UResourceBundle* rawType =
        ures_getByIndex(units.get(), i, nullptr, &status);
    if (U_FAILURE(status)) {
      intl::ReportInternalError(cx);
      return false;
    }
    UniqueUResourceBundle type(rawType);

    int32_t typeSize = ures_getSize(type.get());
    for (int32_t j = 0; j < typeSize; j++) {
      UResourceBundle* rawSubtype =
          ures_getByIndex(type.get(), j, nullptr, &status);
      if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
      }
      UniqueUResourceBundle subtype(rawSubtype);

      const char* unitIdentifier = ures_getKey(subtype.get());

      unitAtom = Atomize(cx, unitIdentifier, strlen(unitIdentifier));
      if (!unitAtom) {
        return false;
      }
      if (!DefineDataProperty(cx, measurementUnits, unitAtom->asPropertyName(),
                              TrueHandleValue)) {
        return false;
      }
    }
  }

  args.rval().setObject(*measurementUnits);
  return true;
}
#endif

bool js::intl::NumberFormatterSkeleton::currency(JSLinearString* currency) {
  MOZ_ASSERT(currency->length() == 3,
             "IsWellFormedCurrencyCode permits only length-3 strings");

  char16_t currencyChars[] = {currency->latin1OrTwoByteChar(0),
                              currency->latin1OrTwoByteChar(1),
                              currency->latin1OrTwoByteChar(2), '\0'};
  return append(u"currency/") && append(currencyChars) && append(' ');
}

bool js::intl::NumberFormatterSkeleton::currencyDisplay(
    CurrencyDisplay display) {
  switch (display) {
    case CurrencyDisplay::Code:
      return appendToken(u"unit-width-iso-code");
    case CurrencyDisplay::Name:
      return appendToken(u"unit-width-full-name");
    case CurrencyDisplay::Symbol:
      // Default, no additional tokens needed.
      return true;
    case CurrencyDisplay::NarrowSymbol:
      return appendToken(u"unit-width-narrow");
  }
  MOZ_CRASH("unexpected currency display type");
}

struct MeasureUnit {
  const char* const type;
  const char* const subtype;
};

/**
 * The list of currently supported simple unit identifiers.
 *
 * Note: Keep in sync with the measure unit lists in
 * - js/src/builtin/intl/NumberFormat.js
 * - intl/icu/data_filter.json
 *
 * The list must be kept in alphabetical order of the |subtype|.
 */
static constexpr MeasureUnit simpleMeasureUnits[] = {
    // clang-format off
    {"area", "acre"},
    {"digital", "bit"},
    {"digital", "byte"},
    {"temperature", "celsius"},
    {"length", "centimeter"},
    {"duration", "day"},
    {"angle", "degree"},
    {"temperature", "fahrenheit"},
    {"volume", "fluid-ounce"},
    {"length", "foot"},
    {"volume", "gallon"},
    {"digital", "gigabit"},
    {"digital", "gigabyte"},
    {"mass", "gram"},
    {"area", "hectare"},
    {"duration", "hour"},
    {"length", "inch"},
    {"digital", "kilobit"},
    {"digital", "kilobyte"},
    {"mass", "kilogram"},
    {"length", "kilometer"},
    {"volume", "liter"},
    {"digital", "megabit"},
    {"digital", "megabyte"},
    {"length", "meter"},
    {"length", "mile"},
    {"length", "mile-scandinavian"},
    {"volume", "milliliter"},
    {"length", "millimeter"},
    {"duration", "millisecond"},
    {"duration", "minute"},
    {"duration", "month"},
    {"mass", "ounce"},
    {"concentr", "percent"},
    {"digital", "petabyte"},
    {"mass", "pound"},
    {"duration", "second"},
    {"mass", "stone"},
    {"digital", "terabit"},
    {"digital", "terabyte"},
    {"duration", "week"},
    {"length", "yard"},
    {"duration", "year"},
    // clang-format on
};

static const MeasureUnit& FindSimpleMeasureUnit(const char* subtype) {
  auto measureUnit = std::lower_bound(
      std::begin(simpleMeasureUnits), std::end(simpleMeasureUnits), subtype,
      [](const auto& measureUnit, const char* subtype) {
        return strcmp(measureUnit.subtype, subtype) < 0;
      });
  MOZ_ASSERT(measureUnit != std::end(simpleMeasureUnits),
             "unexpected unit identifier: unit not found");
  MOZ_ASSERT(strcmp(measureUnit->subtype, subtype) == 0,
             "unexpected unit identifier: wrong unit found");
  return *measureUnit;
}

static constexpr size_t MaxUnitLength() {
  // Enable by default when libstdc++ 7 is the minimal version expected
#if _GLIBCXX_RELEASE >= 7
  size_t length = 0;
  for (const auto& unit : simpleMeasureUnits) {
    length = std::max(length, std::char_traits<char>::length(unit.subtype));
  }
  return length * 2 + std::char_traits<char>::length("-per-");
#else
  return mozilla::ArrayLength("mile-scandinavian-per-mile-scandinavian") - 1;
#endif
}

bool js::intl::NumberFormatterSkeleton::unit(JSLinearString* unit) {
  MOZ_RELEASE_ASSERT(unit->length() <= MaxUnitLength());

  char unitChars[MaxUnitLength() + 1] = {};
  CopyChars(reinterpret_cast<Latin1Char*>(unitChars), *unit);

  auto appendUnit = [this](const MeasureUnit& unit) {
    return append(unit.type, strlen(unit.type)) && append('-') &&
           append(unit.subtype, strlen(unit.subtype));
  };

  // |unit| can be a compound unit identifier, separated by "-per-".

  static constexpr char separator[] = "-per-";
  if (char* p = strstr(unitChars, separator)) {
    // Split into two strings.
    p[0] = '\0';

    auto& numerator = FindSimpleMeasureUnit(unitChars);
    if (!append(u"measure-unit/") || !appendUnit(numerator) || !append(' ')) {
      return false;
    }

    auto& denominator = FindSimpleMeasureUnit(p + strlen(separator));
    if (!append(u"per-measure-unit/") || !appendUnit(denominator) ||
        !append(' ')) {
      return false;
    }
  } else {
    auto& simple = FindSimpleMeasureUnit(unitChars);
    if (!append(u"measure-unit/") || !appendUnit(simple) || !append(' ')) {
      return false;
    }
  }
  return true;
}

bool js::intl::NumberFormatterSkeleton::unitDisplay(UnitDisplay display) {
  switch (display) {
    case UnitDisplay::Short:
      return appendToken(u"unit-width-short");
    case UnitDisplay::Narrow:
      return appendToken(u"unit-width-narrow");
    case UnitDisplay::Long:
      return appendToken(u"unit-width-full-name");
  }
  MOZ_CRASH("unexpected unit display type");
}

bool js::intl::NumberFormatterSkeleton::percent() {
  return appendToken(u"percent scale/100");
}

bool js::intl::NumberFormatterSkeleton::fractionDigits(uint32_t min,
                                                       uint32_t max) {
  // Note: |min| can be zero here.
  MOZ_ASSERT(min <= max);
  return append('.') && appendN('0', min) && appendN('#', max - min) &&
         append(' ');
}

bool js::intl::NumberFormatterSkeleton::integerWidth(uint32_t min) {
  MOZ_ASSERT(min > 0);
  return append(u"integer-width/+") && appendN('0', min) && append(' ');
}

bool js::intl::NumberFormatterSkeleton::significantDigits(uint32_t min,
                                                          uint32_t max) {
  MOZ_ASSERT(min > 0);
  MOZ_ASSERT(min <= max);
  return appendN('@', min) && appendN('#', max - min) && append(' ');
}

bool js::intl::NumberFormatterSkeleton::useGrouping(bool on) {
  return on || appendToken(u"group-off");
}

bool js::intl::NumberFormatterSkeleton::notation(Notation style) {
  switch (style) {
    case Notation::Standard:
      // Default, no additional tokens needed.
      return true;
    case Notation::Scientific:
      return appendToken(u"scientific");
    case Notation::Engineering:
      return appendToken(u"engineering");
    case Notation::CompactShort:
      return appendToken(u"compact-short");
    case Notation::CompactLong:
      return appendToken(u"compact-long");
  }
  MOZ_CRASH("unexpected notation style");
}

bool js::intl::NumberFormatterSkeleton::signDisplay(SignDisplay display) {
  switch (display) {
    case SignDisplay::Auto:
      // Default, no additional tokens needed.
      return true;
    case SignDisplay::Always:
      return appendToken(u"sign-always");
    case SignDisplay::Never:
      return appendToken(u"sign-never");
    case SignDisplay::ExceptZero:
      return appendToken(u"sign-except-zero");
    case SignDisplay::Accounting:
      return appendToken(u"sign-accounting");
    case SignDisplay::AccountingAlways:
      return appendToken(u"sign-accounting-always");
    case SignDisplay::AccountingExceptZero:
      return appendToken(u"sign-accounting-except-zero");
  }
  MOZ_CRASH("unexpected sign display type");
}

bool js::intl::NumberFormatterSkeleton::roundingModeHalfUp() {
  return appendToken(u"rounding-mode-half-up");
}

UNumberFormatter* js::intl::NumberFormatterSkeleton::toFormatter(
    JSContext* cx, const char* locale) {
  UErrorCode status = U_ZERO_ERROR;
  UNumberFormatter* nf = unumf_openForSkeletonAndLocale(
      vector_.begin(), vector_.length(), locale, &status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return nullptr;
  }
  return nf;
}

/**
 * Returns a new UNumberFormatter with the locale and number formatting options
 * of the given NumberFormat.
 */
static UNumberFormatter* NewUNumberFormatter(
    JSContext* cx, Handle<NumberFormatObject*> numberFormat) {
  RootedValue value(cx);

  RootedObject internals(cx, intl::GetInternalsObject(cx, numberFormat));
  if (!internals) {
    return nullptr;
  }

  if (!GetProperty(cx, internals, internals, cx->names().locale, &value)) {
    return nullptr;
  }

  // ICU expects numberingSystem as a Unicode locale extensions on locale.

  intl::LanguageTag tag(cx);
  {
    JSLinearString* locale = value.toString()->ensureLinear(cx);
    if (!locale) {
      return nullptr;
    }

    if (!intl::LanguageTagParser::parse(cx, locale, tag)) {
      return nullptr;
    }
  }

  JS::RootedVector<intl::UnicodeExtensionKeyword> keywords(cx);

  if (!GetProperty(cx, internals, internals, cx->names().numberingSystem,
                   &value)) {
    return nullptr;
  }

  {
    JSLinearString* numberingSystem = value.toString()->ensureLinear(cx);
    if (!numberingSystem) {
      return nullptr;
    }

    if (!keywords.emplaceBack("nu", numberingSystem)) {
      return nullptr;
    }
  }

  // |ApplyUnicodeExtensionToTag| applies the new keywords to the front of
  // the Unicode extension subtag. We're then relying on ICU to follow RFC
  // 6067, which states that any trailing keywords using the same key
  // should be ignored.
  if (!intl::ApplyUnicodeExtensionToTag(cx, tag, keywords)) {
    return nullptr;
  }

  UniqueChars locale = tag.toStringZ(cx);
  if (!locale) {
    return nullptr;
  }

  intl::NumberFormatterSkeleton skeleton(cx);

  if (!GetProperty(cx, internals, internals, cx->names().style, &value)) {
    return nullptr;
  }

  bool accountingSign = false;
  {
    JSLinearString* style = value.toString()->ensureLinear(cx);
    if (!style) {
      return nullptr;
    }

    if (StringEqualsLiteral(style, "currency")) {
      if (!GetProperty(cx, internals, internals, cx->names().currency,
                       &value)) {
        return nullptr;
      }
      JSLinearString* currency = value.toString()->ensureLinear(cx);
      if (!currency) {
        return nullptr;
      }

      if (!skeleton.currency(currency)) {
        return nullptr;
      }

      if (!GetProperty(cx, internals, internals, cx->names().currencyDisplay,
                       &value)) {
        return nullptr;
      }
      JSLinearString* currencyDisplay = value.toString()->ensureLinear(cx);
      if (!currencyDisplay) {
        return nullptr;
      }

      using CurrencyDisplay = intl::NumberFormatterSkeleton::CurrencyDisplay;

      CurrencyDisplay display;
      if (StringEqualsLiteral(currencyDisplay, "code")) {
        display = CurrencyDisplay::Code;
      } else if (StringEqualsLiteral(currencyDisplay, "symbol")) {
        display = CurrencyDisplay::Symbol;
      } else if (StringEqualsLiteral(currencyDisplay, "narrowSymbol")) {
        display = CurrencyDisplay::NarrowSymbol;
      } else {
        MOZ_ASSERT(StringEqualsLiteral(currencyDisplay, "name"));
        display = CurrencyDisplay::Name;
      }

      if (!skeleton.currencyDisplay(display)) {
        return nullptr;
      }

      if (!GetProperty(cx, internals, internals, cx->names().currencySign,
                       &value)) {
        return nullptr;
      }
      JSLinearString* currencySign = value.toString()->ensureLinear(cx);
      if (!currencySign) {
        return nullptr;
      }

      if (StringEqualsLiteral(currencySign, "accounting")) {
        accountingSign = true;
      } else {
        MOZ_ASSERT(StringEqualsLiteral(currencySign, "standard"));
      }
    } else if (StringEqualsLiteral(style, "percent")) {
      if (!skeleton.percent()) {
        return nullptr;
      }
    } else if (StringEqualsLiteral(style, "unit")) {
      if (!GetProperty(cx, internals, internals, cx->names().unit, &value)) {
        return nullptr;
      }
      JSLinearString* unit = value.toString()->ensureLinear(cx);
      if (!unit) {
        return nullptr;
      }

      if (!skeleton.unit(unit)) {
        return nullptr;
      }

      if (!GetProperty(cx, internals, internals, cx->names().unitDisplay,
                       &value)) {
        return nullptr;
      }
      JSLinearString* unitDisplay = value.toString()->ensureLinear(cx);
      if (!unitDisplay) {
        return nullptr;
      }

      using UnitDisplay = intl::NumberFormatterSkeleton::UnitDisplay;

      UnitDisplay display;
      if (StringEqualsLiteral(unitDisplay, "short")) {
        display = UnitDisplay::Short;
      } else if (StringEqualsLiteral(unitDisplay, "narrow")) {
        display = UnitDisplay::Narrow;
      } else {
        MOZ_ASSERT(StringEqualsLiteral(unitDisplay, "long"));
        display = UnitDisplay::Long;
      }

      if (!skeleton.unitDisplay(display)) {
        return nullptr;
      }
    } else {
      MOZ_ASSERT(StringEqualsLiteral(style, "decimal"));
    }
  }

  bool hasMinimumSignificantDigits;
  if (!HasProperty(cx, internals, cx->names().minimumSignificantDigits,
                   &hasMinimumSignificantDigits)) {
    return nullptr;
  }

  if (hasMinimumSignificantDigits) {
    if (!GetProperty(cx, internals, internals,
                     cx->names().minimumSignificantDigits, &value)) {
      return nullptr;
    }
    uint32_t minimumSignificantDigits = AssertedCast<uint32_t>(value.toInt32());

    if (!GetProperty(cx, internals, internals,
                     cx->names().maximumSignificantDigits, &value)) {
      return nullptr;
    }
    uint32_t maximumSignificantDigits = AssertedCast<uint32_t>(value.toInt32());

    if (!skeleton.significantDigits(minimumSignificantDigits,
                                    maximumSignificantDigits)) {
      return nullptr;
    }
  }

  bool hasMinimumFractionDigits;
  if (!HasProperty(cx, internals, cx->names().minimumFractionDigits,
                   &hasMinimumFractionDigits)) {
    return nullptr;
  }

  if (hasMinimumFractionDigits) {
    if (!GetProperty(cx, internals, internals,
                     cx->names().minimumFractionDigits, &value)) {
      return nullptr;
    }
    uint32_t minimumFractionDigits = AssertedCast<uint32_t>(value.toInt32());

    if (!GetProperty(cx, internals, internals,
                     cx->names().maximumFractionDigits, &value)) {
      return nullptr;
    }
    uint32_t maximumFractionDigits = AssertedCast<uint32_t>(value.toInt32());

    if (!skeleton.fractionDigits(minimumFractionDigits,
                                 maximumFractionDigits)) {
      return nullptr;
    }
  }

  if (!GetProperty(cx, internals, internals, cx->names().minimumIntegerDigits,
                   &value)) {
    return nullptr;
  }
  uint32_t minimumIntegerDigits = AssertedCast<uint32_t>(value.toInt32());

  if (!skeleton.integerWidth(minimumIntegerDigits)) {
    return nullptr;
  }

  if (!GetProperty(cx, internals, internals, cx->names().useGrouping, &value)) {
    return nullptr;
  }
  if (!skeleton.useGrouping(value.toBoolean())) {
    return nullptr;
  }

  if (!GetProperty(cx, internals, internals, cx->names().notation, &value)) {
    return nullptr;
  }

  {
    JSLinearString* notation = value.toString()->ensureLinear(cx);
    if (!notation) {
      return nullptr;
    }

    using Notation = intl::NumberFormatterSkeleton::Notation;

    Notation style;
    if (StringEqualsLiteral(notation, "standard")) {
      style = Notation::Standard;
    } else if (StringEqualsLiteral(notation, "scientific")) {
      style = Notation::Scientific;
    } else if (StringEqualsLiteral(notation, "engineering")) {
      style = Notation::Engineering;
    } else {
      MOZ_ASSERT(StringEqualsLiteral(notation, "compact"));

      if (!GetProperty(cx, internals, internals, cx->names().compactDisplay,
                       &value)) {
        return nullptr;
      }

      JSLinearString* compactDisplay = value.toString()->ensureLinear(cx);
      if (!compactDisplay) {
        return nullptr;
      }

      if (StringEqualsLiteral(compactDisplay, "short")) {
        style = Notation::CompactShort;
      } else {
        MOZ_ASSERT(StringEqualsLiteral(compactDisplay, "long"));
        style = Notation::CompactLong;
      }
    }

    if (!skeleton.notation(style)) {
      return nullptr;
    }
  }

  if (!GetProperty(cx, internals, internals, cx->names().signDisplay, &value)) {
    return nullptr;
  }

  {
    JSLinearString* signDisplay = value.toString()->ensureLinear(cx);
    if (!signDisplay) {
      return nullptr;
    }

    using SignDisplay = intl::NumberFormatterSkeleton::SignDisplay;

    SignDisplay display;
    if (StringEqualsLiteral(signDisplay, "auto")) {
      if (accountingSign) {
        display = SignDisplay::Accounting;
      } else {
        display = SignDisplay::Auto;
      }
    } else if (StringEqualsLiteral(signDisplay, "never")) {
      display = SignDisplay::Never;
    } else if (StringEqualsLiteral(signDisplay, "always")) {
      if (accountingSign) {
        display = SignDisplay::AccountingAlways;
      } else {
        display = SignDisplay::Always;
      }
    } else {
      MOZ_ASSERT(StringEqualsLiteral(signDisplay, "exceptZero"));
      if (accountingSign) {
        display = SignDisplay::AccountingExceptZero;
      } else {
        display = SignDisplay::ExceptZero;
      }
    }

    if (!skeleton.signDisplay(display)) {
      return nullptr;
    }
  }

  if (!skeleton.roundingModeHalfUp()) {
    return nullptr;
  }

  return skeleton.toFormatter(cx, locale.get());
}

static UFormattedNumber* NewUFormattedNumber(JSContext* cx) {
  UErrorCode status = U_ZERO_ERROR;
  UFormattedNumber* formatted = unumf_openResult(&status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return nullptr;
  }
  return formatted;
}

// We also support UFormattedNumber in addition to UFormattedValue, in case
// we're compiling against a system ICU which doesn't expose draft APIs.

#ifndef U_HIDE_DRAFT_API
using PartitionNumberPatternResult = const UFormattedValue*;
#else
using PartitionNumberPatternResult = const UFormattedNumber*;
#endif

static PartitionNumberPatternResult PartitionNumberPattern(
    JSContext* cx, const UNumberFormatter* nf, UFormattedNumber* formatted,
    HandleValue x) {
  UErrorCode status = U_ZERO_ERROR;
  if (x.isNumber()) {
    double num = x.toNumber();

    // ICU incorrectly formats NaN values with the sign bit set, as if they
    // were negative.  Replace all NaNs with a single pattern with sign bit
    // unset ("positive", that is) until ICU is fixed.
    if (MOZ_UNLIKELY(IsNaN(num))) {
      num = SpecificNaN<double>(0, 1);
    }

    unumf_formatDouble(nf, num, formatted, &status);
  } else {
    RootedBigInt bi(cx, x.toBigInt());

    int64_t num;
    if (BigInt::isInt64(bi, &num)) {
      unumf_formatInt(nf, num, formatted, &status);
    } else {
      JSLinearString* str = BigInt::toString<CanGC>(cx, bi, 10);
      if (!str) {
        return nullptr;
      }
      MOZ_ASSERT(str->hasLatin1Chars());

      // Tell the analysis the |unumf_formatDecimal| function can't GC.
      JS::AutoSuppressGCAnalysis nogc;

      const char* chars = reinterpret_cast<const char*>(str->latin1Chars(nogc));
      unumf_formatDecimal(nf, chars, str->length(), formatted, &status);
    }
  }
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return nullptr;
  }

#ifndef U_HIDE_DRAFT_API
  const UFormattedValue* formattedValue =
      unumf_resultAsValue(formatted, &status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return nullptr;
  }
  return formattedValue;
#else
  return formatted;
#endif
}

static JSString* FormattedNumberToString(
    JSContext* cx, PartitionNumberPatternResult formattedValue) {
#ifndef U_HIDE_DRAFT_API
  static_assert(
      std::is_same_v<PartitionNumberPatternResult, const UFormattedValue*>,
      "UFormattedValue arm");

  UErrorCode status = U_ZERO_ERROR;
  int32_t strLength;
  const char16_t* str = ufmtval_getString(formattedValue, &strLength, &status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return nullptr;
  }

  return NewStringCopyN<CanGC>(cx, str, AssertedCast<uint32_t>(strLength));
#else
  static_assert(
      std::is_same_v<PartitionNumberPatternResult, const UFormattedNumber*>,
      "UFormattedNumber arm");

  return CallICU(cx,
                 [formatted](UChar* chars, int32_t size, UErrorCode* status) {
                   return unumf_resultToString(formatted, chars, size, status);
                 });
#endif
}

static bool FormatNumeric(JSContext* cx, const UNumberFormatter* nf,
                          UFormattedNumber* formatted, HandleValue x,
                          MutableHandleValue result) {
  PartitionNumberPatternResult formattedValue =
      PartitionNumberPattern(cx, nf, formatted, x);
  if (!formattedValue) {
    return false;
  }

  JSString* str = FormattedNumberToString(cx, formattedValue);
  if (!str) {
    return false;
  }

  result.setString(str);
  return true;
}

enum class FormattingType { ForUnit, NotForUnit };

static FieldType GetFieldTypeForNumberField(UNumberFormatFields fieldName,
                                            HandleValue x,
                                            FormattingType formattingType) {
  // See intl/icu/source/i18n/unicode/unum.h for a detailed field list.  This
  // list is deliberately exhaustive: cases might have to be added/removed if
  // this code is compiled with a different ICU with more UNumberFormatFields
  // enum initializers.  Please guard such cases with appropriate ICU
  // version-testing #ifdefs, should cross-version divergence occur.
  switch (fieldName) {
    case UNUM_INTEGER_FIELD:
      if (x.isNumber()) {
        double d = x.toNumber();
        if (IsNaN(d)) {
          return &JSAtomState::nan;
        }
        if (!IsFinite(d)) {
          return &JSAtomState::infinity;
        }
      }
      return &JSAtomState::integer;

    case UNUM_GROUPING_SEPARATOR_FIELD:
      return &JSAtomState::group;

    case UNUM_DECIMAL_SEPARATOR_FIELD:
      return &JSAtomState::decimal;

    case UNUM_FRACTION_FIELD:
      return &JSAtomState::fraction;

    case UNUM_SIGN_FIELD: {
      // We coerce all NaNs to one with the sign bit unset, so all NaNs are
      // positive in our implementation.
      bool isNegative = x.isNumber()
                            ? !IsNaN(x.toNumber()) && IsNegative(x.toNumber())
                            : x.toBigInt()->isNegative();
      return isNegative ? &JSAtomState::minusSign : &JSAtomState::plusSign;
    }

    case UNUM_PERCENT_FIELD:
      // Percent fields are returned as "unit" elements when the number
      // formatter's style is "unit".
      if (formattingType == FormattingType::ForUnit) {
        return &JSAtomState::unit;
      }
      return &JSAtomState::percentSign;

    case UNUM_CURRENCY_FIELD:
      return &JSAtomState::currency;

    case UNUM_PERMILL_FIELD:
      MOZ_ASSERT_UNREACHABLE(
          "unexpected permill field found, even though "
          "we don't use any user-defined patterns that "
          "would require a permill field");
      break;

    case UNUM_EXPONENT_SYMBOL_FIELD:
      return &JSAtomState::exponentSeparator;

    case UNUM_EXPONENT_SIGN_FIELD:
      return &JSAtomState::exponentMinusSign;

    case UNUM_EXPONENT_FIELD:
      return &JSAtomState::exponentInteger;

#ifndef U_HIDE_DRAFT_API
    case UNUM_MEASURE_UNIT_FIELD:
      return &JSAtomState::unit;

    case UNUM_COMPACT_FIELD:
      return &JSAtomState::compact;
#endif

#ifndef U_HIDE_DEPRECATED_API
    case UNUM_FIELD_COUNT:
      MOZ_ASSERT_UNREACHABLE(
          "format field sentinel value returned by iterator!");
      break;
#endif
  }

  MOZ_ASSERT_UNREACHABLE(
      "unenumerated, undocumented format field returned by iterator");
  return nullptr;
}

struct Field {
  uint32_t begin;
  uint32_t end;
  FieldType type;

  // Needed for vector-resizing scratch space.
  Field() = default;

  Field(uint32_t begin, uint32_t end, FieldType type)
      : begin(begin), end(end), type(type) {}
};

class NumberFormatFields {
  using FieldsVector = Vector<Field, 16>;

  FieldsVector fields_;

 public:
  explicit NumberFormatFields(JSContext* cx) : fields_(cx) {}

  MOZ_MUST_USE bool append(FieldType type, int32_t begin, int32_t end);

  MOZ_MUST_USE ArrayObject* toArray(JSContext* cx,
                                    JS::HandleString overallResult,
                                    FieldType unitType);
};

bool NumberFormatFields::append(FieldType type, int32_t begin, int32_t end) {
  MOZ_ASSERT(begin >= 0);
  MOZ_ASSERT(end >= 0);
  MOZ_ASSERT(begin < end, "erm, aren't fields always non-empty?");

  return fields_.emplaceBack(uint32_t(begin), uint32_t(end), type);
}

ArrayObject* NumberFormatFields::toArray(JSContext* cx,
                                         HandleString overallResult,
                                         FieldType unitType) {
  // Merge sort the fields vector.  Expand the vector to have scratch space for
  // performing the sort.
  size_t fieldsLen = fields_.length();
  if (!fields_.growByUninitialized(fieldsLen)) {
    return nullptr;
  }

  MOZ_ALWAYS_TRUE(MergeSort(
      fields_.begin(), fieldsLen, fields_.begin() + fieldsLen,
      [](const Field& left, const Field& right, bool* lessOrEqual) {
        // Sort first by begin index, then to place
        // enclosing fields before nested fields.
        *lessOrEqual = left.begin < right.begin ||
                       (left.begin == right.begin && left.end > right.end);
        return true;
      }));

  // Delete the elements in the scratch space.
  fields_.shrinkBy(fieldsLen);

  // Then iterate over the sorted field list to generate a sequence of parts
  // (what ECMA-402 actually exposes).  A part is a maximal character sequence
  // entirely within no field or a single most-nested field.
  //
  // Diagrams may be helpful to illustrate how fields map to parts.  Consider
  // formatting -19,766,580,028,249.41, the US national surplus (negative
  // because it's actually a debt) on October 18, 2016.
  //
  //    var options =
  //      { style: "currency", currency: "USD", currencyDisplay: "name" };
  //    var usdFormatter = new Intl.NumberFormat("en-US", options);
  //    usdFormatter.format(-19766580028249.41);
  //
  // The formatted result is "-19,766,580,028,249.41 US dollars".  ICU
  // identifies these fields in the string:
  //
  //     UNUM_GROUPING_SEPARATOR_FIELD
  //                   |
  //   UNUM_SIGN_FIELD |  UNUM_DECIMAL_SEPARATOR_FIELD
  //    |   __________/|   |
  //    |  /   |   |   |   |
  //   "-19,766,580,028,249.41 US dollars"
  //     \________________/ |/ \_______/
  //             |          |      |
  //    UNUM_INTEGER_FIELD  |  UNUM_CURRENCY_FIELD
  //                        |
  //               UNUM_FRACTION_FIELD
  //
  // These fields map to parts as follows:
  //
  //         integer     decimal
  //       _____|________  |
  //      /  /| |\  |\  |\ |  literal
  //     /| / | | \ | \ | \|  |
  //   "-19,766,580,028,249.41 US dollars"
  //    |  \___|___|___/    |/ \________/
  //    |        |          |       |
  //    |      group        |   currency
  //    |                   |
  //   minusSign        fraction
  //
  // The sign is a part.  Each comma is a part, splitting the integer field
  // into parts for trillions/billions/&c. digits.  The decimal point is a
  // part.  Cents are a part.  The space between cents and currency is a part
  // (outside any field).  Last, the currency field is a part.
  //
  // Because parts fully partition the formatted string, we only track the
  // end of each part -- the beginning is implicitly the last part's end.
  struct Part {
    uint32_t end;
    FieldType type;
  };

  class PartGenerator {
    // The fields in order from start to end, then least to most nested.
    const FieldsVector& fields;

    // Index of the current field, in |fields|, being considered to
    // determine part boundaries.  |lastEnd <= fields[index].begin| is an
    // invariant.
    size_t index;

    // The end index of the last part produced, always less than or equal
    // to |limit|, strictly increasing.
    uint32_t lastEnd;

    // The length of the overall formatted string.
    const uint32_t limit;

    Vector<size_t, 4> enclosingFields;

    void popEnclosingFieldsEndingAt(uint32_t end) {
      MOZ_ASSERT_IF(enclosingFields.length() > 0,
                    fields[enclosingFields.back()].end >= end);

      while (enclosingFields.length() > 0 &&
             fields[enclosingFields.back()].end == end) {
        enclosingFields.popBack();
      }
    }

    bool nextPartInternal(Part* part) {
      size_t len = fields.length();
      MOZ_ASSERT(index <= len);

      // If we're out of fields, all that remains are part(s) consisting
      // of trailing portions of enclosing fields, and maybe a final
      // literal part.
      if (index == len) {
        if (enclosingFields.length() > 0) {
          const auto& enclosing = fields[enclosingFields.popCopy()];
          part->end = enclosing.end;
          part->type = enclosing.type;

          // If additional enclosing fields end where this part ends,
          // pop them as well.
          popEnclosingFieldsEndingAt(part->end);
        } else {
          part->end = limit;
          part->type = &JSAtomState::literal;
        }

        return true;
      }

      // Otherwise we still have a field to process.
      const Field* current = &fields[index];
      MOZ_ASSERT(lastEnd <= current->begin);
      MOZ_ASSERT(current->begin < current->end);

      // But first, deal with inter-field space.
      if (lastEnd < current->begin) {
        if (enclosingFields.length() > 0) {
          // Space between fields, within an enclosing field, is part
          // of that enclosing field, until the start of the current
          // field or the end of the enclosing field, whichever is
          // earlier.
          const auto& enclosing = fields[enclosingFields.back()];
          part->end = std::min(enclosing.end, current->begin);
          part->type = enclosing.type;
          popEnclosingFieldsEndingAt(part->end);
        } else {
          // If there's no enclosing field, the space is a literal.
          part->end = current->begin;
          part->type = &JSAtomState::literal;
        }

        return true;
      }

      // Otherwise, the part spans a prefix of the current field.  Find
      // the most-nested field containing that prefix.
      const Field* next;
      do {
        current = &fields[index];

        // If the current field is last, the part extends to its end.
        if (++index == len) {
          part->end = current->end;
          part->type = current->type;
          return true;
        }

        next = &fields[index];
        MOZ_ASSERT(current->begin <= next->begin);
        MOZ_ASSERT(current->begin < next->end);

        // If the next field nests within the current field, push an
        // enclosing field.  (If there are no nested fields, don't
        // bother pushing a field that'd be immediately popped.)
        if (current->end > next->begin) {
          if (!enclosingFields.append(index - 1)) {
            return false;
          }
        }

        // Do so until the next field begins after this one.
      } while (current->begin == next->begin);

      part->type = current->type;

      if (current->end <= next->begin) {
        // The next field begins after the current field ends.  Therefore
        // the current part ends at the end of the current field.
        part->end = current->end;
        popEnclosingFieldsEndingAt(part->end);
      } else {
        // The current field encloses the next one.  The current part
        // ends where the next field/part will start.
        part->end = next->begin;
      }

      return true;
    }

   public:
    PartGenerator(JSContext* cx, const FieldsVector& vec, uint32_t limit)
        : fields(vec),
          index(0),
          lastEnd(0),
          limit(limit),
          enclosingFields(cx) {}

    bool nextPart(bool* hasPart, Part* part) {
      // There are no parts left if we've partitioned the entire string.
      if (lastEnd == limit) {
        MOZ_ASSERT(enclosingFields.length() == 0);
        *hasPart = false;
        return true;
      }

      if (!nextPartInternal(part)) {
        return false;
      }

      *hasPart = true;
      lastEnd = part->end;
      return true;
    }
  };

  // Finally, generate the result array.
  size_t lastEndIndex = 0;
  RootedObject singlePart(cx);
  RootedValue propVal(cx);

  RootedArrayObject partsArray(cx, NewDenseEmptyArray(cx));
  if (!partsArray) {
    return nullptr;
  }

  PartGenerator gen(cx, fields_, overallResult->length());
  do {
    bool hasPart;
    Part part;
    if (!gen.nextPart(&hasPart, &part)) {
      return nullptr;
    }

    if (!hasPart) {
      break;
    }

    FieldType type = part.type;
    size_t endIndex = part.end;

    MOZ_ASSERT(lastEndIndex < endIndex);

    singlePart = NewBuiltinClassInstance<PlainObject>(cx);
    if (!singlePart) {
      return nullptr;
    }

    propVal.setString(cx->names().*type);
    if (!DefineDataProperty(cx, singlePart, cx->names().type, propVal)) {
      return nullptr;
    }

    JSLinearString* partSubstr = NewDependentString(
        cx, overallResult, lastEndIndex, endIndex - lastEndIndex);
    if (!partSubstr) {
      return nullptr;
    }

    propVal.setString(partSubstr);
    if (!DefineDataProperty(cx, singlePart, cx->names().value, propVal)) {
      return nullptr;
    }

    if (unitType != nullptr && type != &JSAtomState::literal) {
      propVal.setString(cx->names().*unitType);
      if (!DefineDataProperty(cx, singlePart, cx->names().unit, propVal)) {
        return nullptr;
      }
    }

    if (!NewbornArrayPush(cx, partsArray, ObjectValue(*singlePart))) {
      return nullptr;
    }

    lastEndIndex = endIndex;
  } while (true);

  MOZ_ASSERT(lastEndIndex == overallResult->length(),
             "result array must partition the entire string");

  return partsArray;
}

#ifndef U_HIDE_DRAFT_API
static bool FormattedNumberToParts(JSContext* cx,
                                   const UFormattedValue* formattedValue,
                                   HandleValue number,
                                   FieldType relativeTimeUnit,
                                   FormattingType formattingType,
                                   MutableHandleValue result) {
  MOZ_ASSERT(number.isNumeric());

  RootedString overallResult(cx, FormattedNumberToString(cx, formattedValue));
  if (!overallResult) {
    return false;
  }

  UErrorCode status = U_ZERO_ERROR;
  UConstrainedFieldPosition* fpos = ucfpos_open(&status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return false;
  }
  ScopedICUObject<UConstrainedFieldPosition, ucfpos_close> toCloseFpos(fpos);

  // We're only interested in UFIELD_CATEGORY_NUMBER fields.
  ucfpos_constrainCategory(fpos, UFIELD_CATEGORY_NUMBER, &status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return false;
  }

  // Vacuum up fields in the overall formatted string.

  NumberFormatFields fields(cx);

  while (true) {
    bool hasMore = ufmtval_nextPosition(formattedValue, fpos, &status);
    if (U_FAILURE(status)) {
      intl::ReportInternalError(cx);
      return false;
    }
    if (!hasMore) {
      break;
    }

    int32_t field = ucfpos_getField(fpos, &status);
    if (U_FAILURE(status)) {
      intl::ReportInternalError(cx);
      return false;
    }

    int32_t beginIndex, endIndex;
    ucfpos_getIndexes(fpos, &beginIndex, &endIndex, &status);
    if (U_FAILURE(status)) {
      intl::ReportInternalError(cx);
      return false;
    }

    FieldType type = GetFieldTypeForNumberField(UNumberFormatFields(field),
                                                number, formattingType);

    if (!fields.append(type, beginIndex, endIndex)) {
      return false;
    }
  }

  ArrayObject* array = fields.toArray(cx, overallResult, relativeTimeUnit);
  if (!array) {
    return false;
  }

  result.setObject(*array);
  return true;
}

bool js::intl::FormattedRelativeTimeToParts(
    JSContext* cx, const UFormattedValue* formattedValue, double timeValue,
    FieldType relativeTimeUnit, MutableHandleValue result) {
  Value tval = DoubleValue(timeValue);
  return FormattedNumberToParts(
      cx, formattedValue, HandleValue::fromMarkedLocation(&tval),
      relativeTimeUnit, FormattingType::NotForUnit, result);
}
#else
static ArrayObject* LegacyFormattedNumberToParts(
    JSContext* cx, const UFormattedNumber* formatted, HandleValue x,
    MutableHandleValue result) {
  RootedString overallResult(cx, FormattedNumberToString(cx, formatted));
  if (!overallResult) {
    return false;
  }

  UErrorCode status = U_ZERO_ERROR;
  UFieldPositionIterator* fpositer = ufieldpositer_open(&status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return false;
  }

  MOZ_ASSERT(fpositer);
  ScopedICUObject<UFieldPositionIterator, ufieldpositer_close> toClose(
      fpositer);

  unumf_resultGetAllFieldPositions(formatted, fpositer, &status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return false;
  }

  // Vacuum up fields in the overall formatted string.

  NumberFormatFields fields(cx, x);

  int32_t field, beginIndex, endIndex;
  while ((field = ufieldpositer_next(fpositer, &beginIndex, &endIndex)) >= 0) {
    if (!fields.append(field, beginIndex, endIndex)) {
      return false;
    }
  }

  ArrayObject* array = fields.toArray(cx, overallResult, nullptr);
  if (!array) {
    return false;
  }

  result.setObject(*array);
  return true;
}
#endif

static bool FormatNumericToParts(JSContext* cx, const UNumberFormatter* nf,
                                 UFormattedNumber* formatted, HandleValue x,
                                 FormattingType formattingType,
                                 MutableHandleValue result) {
  PartitionNumberPatternResult formattedValue =
      PartitionNumberPattern(cx, nf, formatted, x);
  if (!formattedValue) {
    return false;
  }

#ifndef U_HIDE_DRAFT_API
  return FormattedNumberToParts(cx, formattedValue, x, nullptr, formattingType,
                                result);
#else
  return LegacyFormattedNumberToParts(cx, formattedValue, x, result);
#endif
}

bool js::intl_FormatNumber(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 4);
  MOZ_ASSERT(args[0].isObject());
  MOZ_ASSERT(args[1].isNumeric());
  MOZ_ASSERT(args[2].isBoolean());
  MOZ_ASSERT(args[3].isBoolean());

  Rooted<NumberFormatObject*> numberFormat(
      cx, &args[0].toObject().as<NumberFormatObject>());

  // Obtain a cached UNumberFormatter object.
  UNumberFormatter* nf = numberFormat->getNumberFormatter();
  if (!nf) {
    nf = NewUNumberFormatter(cx, numberFormat);
    if (!nf) {
      return false;
    }
    numberFormat->setNumberFormatter(nf);

    intl::AddICUCellMemory(numberFormat,
                           NumberFormatObject::EstimatedMemoryUse);
  }

  // Obtain a cached UFormattedNumber object.
  UFormattedNumber* formatted = numberFormat->getFormattedNumber();
  if (!formatted) {
    formatted = NewUFormattedNumber(cx);
    if (!formatted) {
      return false;
    }
    numberFormat->setFormattedNumber(formatted);

    // UFormattedNumber memory tracked as part of UNumberFormatter.
  }

  // Use the UNumberFormatter to actually format the number.
  if (args[2].toBoolean()) {
    FormattingType formattingType = args[3].toBoolean()
                                        ? FormattingType::ForUnit
                                        : FormattingType::NotForUnit;
    return FormatNumericToParts(cx, nf, formatted, args[1], formattingType,
                                args.rval());
  }

  return FormatNumeric(cx, nf, formatted, args[1], args.rval());
}

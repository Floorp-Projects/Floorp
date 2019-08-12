/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of the Intl.RelativeTimeFormat proposal. */

#include "builtin/intl/RelativeTimeFormat.h"

#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"

#include "builtin/intl/CommonFunctions.h"
#include "builtin/intl/ICUStubs.h"
#include "builtin/intl/NumberFormat.h"
#include "builtin/intl/ScopedICUObject.h"
#include "gc/FreeOp.h"
#include "js/CharacterEncoding.h"
#include "js/PropertySpec.h"
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"
#include "vm/StringType.h"

#include "vm/NativeObject-inl.h"

using namespace js;

using js::intl::CallICU;
using js::intl::GetAvailableLocales;
using js::intl::IcuLocale;

/**************** RelativeTimeFormat *****************/

const ClassOps RelativeTimeFormatObject::classOps_ = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* enumerate */
    nullptr, /* newEnumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    RelativeTimeFormatObject::finalize};

const Class RelativeTimeFormatObject::class_ = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(RelativeTimeFormatObject::SLOT_COUNT) |
        JSCLASS_FOREGROUND_FINALIZE,
    &RelativeTimeFormatObject::classOps_};

static bool relativeTimeFormat_toSource(JSContext* cx, unsigned argc,
                                        Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setString(cx->names().RelativeTimeFormat);
  return true;
}

static const JSFunctionSpec relativeTimeFormat_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf",
                      "Intl_RelativeTimeFormat_supportedLocalesOf", 1, 0),
    JS_FS_END};

static const JSFunctionSpec relativeTimeFormat_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions",
                      "Intl_RelativeTimeFormat_resolvedOptions", 0, 0),
    JS_SELF_HOSTED_FN("format", "Intl_RelativeTimeFormat_format", 2, 0),
#ifndef U_HIDE_DRAFT_API
    JS_SELF_HOSTED_FN("formatToParts", "Intl_RelativeTimeFormat_formatToParts",
                      2, 0),
#endif
    JS_FN(js_toSource_str, relativeTimeFormat_toSource, 0, 0), JS_FS_END};

static const JSPropertySpec relativeTimeFormat_properties[] = {
    JS_STRING_SYM_PS(toStringTag, "Intl.RelativeTimeFormat", JSPROP_READONLY),
    JS_PS_END};

/**
 * RelativeTimeFormat constructor.
 * Spec: ECMAScript 402 API, RelativeTimeFormat, 1.1
 */
static bool RelativeTimeFormat(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Intl.RelativeTimeFormat")) {
    return false;
  }

  // Step 2 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_Null, &proto)) {
    return false;
  }

  if (!proto) {
    proto =
        GlobalObject::getOrCreateRelativeTimeFormatPrototype(cx, cx->global());
    if (!proto) {
      return false;
    }
  }

  Rooted<RelativeTimeFormatObject*> relativeTimeFormat(cx);
  relativeTimeFormat =
      NewObjectWithGivenProto<RelativeTimeFormatObject>(cx, proto);
  if (!relativeTimeFormat) {
    return false;
  }

  relativeTimeFormat->setReservedSlot(RelativeTimeFormatObject::INTERNALS_SLOT,
                                      NullValue());
  relativeTimeFormat->setReservedSlot(
      RelativeTimeFormatObject::URELATIVE_TIME_FORMAT_SLOT,
      PrivateValue(nullptr));

  HandleValue locales = args.get(0);
  HandleValue options = args.get(1);

  // Step 3.
  if (!intl::InitializeObject(cx, relativeTimeFormat,
                              cx->names().InitializeRelativeTimeFormat, locales,
                              options)) {
    return false;
  }

  args.rval().setObject(*relativeTimeFormat);
  return true;
}

void js::RelativeTimeFormatObject::finalize(FreeOp* fop, JSObject* obj) {
  MOZ_ASSERT(fop->onMainThread());

  constexpr auto RT_FORMAT_SLOT =
      RelativeTimeFormatObject::URELATIVE_TIME_FORMAT_SLOT;
  const Value& slot =
      obj->as<RelativeTimeFormatObject>().getReservedSlot(RT_FORMAT_SLOT);
  if (URelativeDateTimeFormatter* rtf =
          static_cast<URelativeDateTimeFormatter*>(slot.toPrivate())) {
    ureldatefmt_close(rtf);
  }
}

JSObject* js::CreateRelativeTimeFormatPrototype(JSContext* cx,
                                                HandleObject Intl,
                                                Handle<GlobalObject*> global) {
  RootedFunction ctor(cx);
  ctor = global->createConstructor(cx, &RelativeTimeFormat,
                                   cx->names().RelativeTimeFormat, 0);
  if (!ctor) {
    return nullptr;
  }

  RootedObject proto(
      cx, GlobalObject::createBlankPrototype<PlainObject>(cx, global));
  if (!proto) {
    return nullptr;
  }

  if (!LinkConstructorAndPrototype(cx, ctor, proto)) {
    return nullptr;
  }

  if (!JS_DefineFunctions(cx, ctor, relativeTimeFormat_static_methods)) {
    return nullptr;
  }

  if (!JS_DefineFunctions(cx, proto, relativeTimeFormat_methods)) {
    return nullptr;
  }

  if (!JS_DefineProperties(cx, proto, relativeTimeFormat_properties)) {
    return nullptr;
  }

  RootedValue ctorValue(cx, ObjectValue(*ctor));
  if (!DefineDataProperty(cx, Intl, cx->names().RelativeTimeFormat, ctorValue,
                          0)) {
    return nullptr;
  }

  return proto;
}

bool js::intl_RelativeTimeFormat_availableLocales(JSContext* cx, unsigned argc,
                                                  Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 0);

  // We're going to use ULocale availableLocales as per ICU recommendation:
  // https://ssl.icu-project.org/trac/ticket/12756
  return GetAvailableLocales(cx, uloc_countAvailable, uloc_getAvailable,
                             args.rval());
}

/**
 * Returns a new URelativeDateTimeFormatter with the locale and options of the
 * given RelativeTimeFormatObject.
 */
static URelativeDateTimeFormatter* NewURelativeDateTimeFormatter(
    JSContext* cx, Handle<RelativeTimeFormatObject*> relativeTimeFormat) {
  RootedObject internals(cx, intl::GetInternalsObject(cx, relativeTimeFormat));
  if (!internals) {
    return nullptr;
  }

  RootedValue value(cx);

  if (!GetProperty(cx, internals, internals, cx->names().locale, &value)) {
    return nullptr;
  }
  UniqueChars locale = intl::EncodeLocale(cx, value.toString());
  if (!locale) {
    return nullptr;
  }

  if (!GetProperty(cx, internals, internals, cx->names().style, &value)) {
    return nullptr;
  }

  UDateRelativeDateTimeFormatterStyle relDateTimeStyle;
  {
    JSLinearString* style = value.toString()->ensureLinear(cx);
    if (!style) {
      return nullptr;
    }

    if (StringEqualsAscii(style, "short")) {
      relDateTimeStyle = UDAT_STYLE_SHORT;
    } else if (StringEqualsAscii(style, "narrow")) {
      relDateTimeStyle = UDAT_STYLE_NARROW;
    } else {
      MOZ_ASSERT(StringEqualsAscii(style, "long"));
      relDateTimeStyle = UDAT_STYLE_LONG;
    }
  }

  UErrorCode status = U_ZERO_ERROR;
  URelativeDateTimeFormatter* rtf =
      ureldatefmt_open(IcuLocale(locale.get()), nullptr, relDateTimeStyle,
                       UDISPCTX_CAPITALIZATION_FOR_STANDALONE, &status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return nullptr;
  }
  return rtf;
}

enum class RelativeTimeNumeric {
  /**
   * Only strings with numeric components like `1 day ago`.
   */
  Always,
  /**
   * Natural-language strings like `yesterday` when possible,
   * otherwise strings with numeric components as in `7 months ago`.
   */
  Auto,
};

static bool intl_FormatRelativeTime(JSContext* cx,
                                    URelativeDateTimeFormatter* rtf, double t,
                                    URelativeDateTimeUnit unit,
                                    RelativeTimeNumeric numeric,
                                    MutableHandleValue result) {
  JSString* str = CallICU(
      cx,
      [rtf, t, unit, numeric](UChar* chars, int32_t size, UErrorCode* status) {
        auto fmt = numeric == RelativeTimeNumeric::Auto
                       ? ureldatefmt_format
                       : ureldatefmt_formatNumeric;
        return fmt(rtf, t, unit, chars, size, status);
      });
  if (!str) {
    return false;
  }

  result.setString(str);
  return true;
}

#ifndef U_HIDE_DRAFT_API
static bool intl_FormatToPartsRelativeTime(JSContext* cx,
                                           URelativeDateTimeFormatter* rtf,
                                           double t, URelativeDateTimeUnit unit,
                                           RelativeTimeNumeric numeric,
                                           MutableHandleValue result) {
  UErrorCode status = U_ZERO_ERROR;
  UFormattedRelativeDateTime* formatted = ureldatefmt_openResult(&status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return false;
  }
  ScopedICUObject<UFormattedRelativeDateTime, ureldatefmt_closeResult> toClose(
      formatted);

  if (numeric == RelativeTimeNumeric::Auto) {
    ureldatefmt_formatToResult(rtf, t, unit, formatted, &status);
  } else {
    ureldatefmt_formatNumericToResult(rtf, t, unit, formatted, &status);
  }
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return false;
  }

  const UFormattedValue* formattedValue =
      ureldatefmt_resultAsValue(formatted, &status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return false;
  }

  intl::FieldType unitType;
  switch (unit) {
    case UDAT_REL_UNIT_SECOND:
      unitType = &JSAtomState::second;
      break;
    case UDAT_REL_UNIT_MINUTE:
      unitType = &JSAtomState::minute;
      break;
    case UDAT_REL_UNIT_HOUR:
      unitType = &JSAtomState::hour;
      break;
    case UDAT_REL_UNIT_DAY:
      unitType = &JSAtomState::day;
      break;
    case UDAT_REL_UNIT_WEEK:
      unitType = &JSAtomState::week;
      break;
    case UDAT_REL_UNIT_MONTH:
      unitType = &JSAtomState::month;
      break;
    case UDAT_REL_UNIT_QUARTER:
      unitType = &JSAtomState::quarter;
      break;
    case UDAT_REL_UNIT_YEAR:
      unitType = &JSAtomState::year;
      break;
    default:
      MOZ_CRASH("unexpected relative time unit");
  }

  Value tval = DoubleValue(t);
  return intl::FormattedNumberToParts(cx, formattedValue,
                                      HandleValue::fromMarkedLocation(&tval),
                                      unitType, result);
}
#endif

bool js::intl_FormatRelativeTime(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 5);

  Rooted<RelativeTimeFormatObject*> relativeTimeFormat(cx);
  relativeTimeFormat = &args[0].toObject().as<RelativeTimeFormatObject>();

  bool formatToParts = args[4].toBoolean();

  // PartitionRelativeTimePattern, step 4.
  double t = args[1].toNumber();
  if (!mozilla::IsFinite(t)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DATE_NOT_FINITE, "RelativeTimeFormat",
                              formatToParts ? "formatToParts" : "format");
    return false;
  }

  // Obtain a cached URelativeDateTimeFormatter object.
  constexpr auto RT_FORMAT_SLOT =
      RelativeTimeFormatObject::URELATIVE_TIME_FORMAT_SLOT;
  void* priv = relativeTimeFormat->getReservedSlot(RT_FORMAT_SLOT).toPrivate();
  URelativeDateTimeFormatter* rtf =
      static_cast<URelativeDateTimeFormatter*>(priv);
  if (!rtf) {
    rtf = NewURelativeDateTimeFormatter(cx, relativeTimeFormat);
    if (!rtf) {
      return false;
    }
    relativeTimeFormat->setReservedSlot(RT_FORMAT_SLOT, PrivateValue(rtf));
  }

  URelativeDateTimeUnit relDateTimeUnit;
  {
    JSLinearString* unit = args[2].toString()->ensureLinear(cx);
    if (!unit) {
      return false;
    }

    // PartitionRelativeTimePattern, step 5.
    if (StringEqualsAscii(unit, "second") ||
        StringEqualsAscii(unit, "seconds")) {
      relDateTimeUnit = UDAT_REL_UNIT_SECOND;
    } else if (StringEqualsAscii(unit, "minute") ||
               StringEqualsAscii(unit, "minutes")) {
      relDateTimeUnit = UDAT_REL_UNIT_MINUTE;
    } else if (StringEqualsAscii(unit, "hour") ||
               StringEqualsAscii(unit, "hours")) {
      relDateTimeUnit = UDAT_REL_UNIT_HOUR;
    } else if (StringEqualsAscii(unit, "day") ||
               StringEqualsAscii(unit, "days")) {
      relDateTimeUnit = UDAT_REL_UNIT_DAY;
    } else if (StringEqualsAscii(unit, "week") ||
               StringEqualsAscii(unit, "weeks")) {
      relDateTimeUnit = UDAT_REL_UNIT_WEEK;
    } else if (StringEqualsAscii(unit, "month") ||
               StringEqualsAscii(unit, "months")) {
      relDateTimeUnit = UDAT_REL_UNIT_MONTH;
    } else if (StringEqualsAscii(unit, "quarter") ||
               StringEqualsAscii(unit, "quarters")) {
      relDateTimeUnit = UDAT_REL_UNIT_QUARTER;
    } else if (StringEqualsAscii(unit, "year") ||
               StringEqualsAscii(unit, "years")) {
      relDateTimeUnit = UDAT_REL_UNIT_YEAR;
    } else {
      if (auto unitChars = StringToNewUTF8CharsZ(cx, *unit)) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                 JSMSG_INVALID_OPTION_VALUE, "unit",
                                 unitChars.get());
      }
      return false;
    }
  }

  RelativeTimeNumeric relDateTimeNumeric;
  {
    JSLinearString* numeric = args[3].toString()->ensureLinear(cx);
    if (!numeric) {
      return false;
    }

    if (StringEqualsAscii(numeric, "auto")) {
      relDateTimeNumeric = RelativeTimeNumeric::Auto;
    } else {
      MOZ_ASSERT(StringEqualsAscii(numeric, "always"));
      relDateTimeNumeric = RelativeTimeNumeric::Always;
    }
  }

#ifndef U_HIDE_DRAFT_API
  return formatToParts
             ? intl_FormatToPartsRelativeTime(cx, rtf, t, relDateTimeUnit,
                                              relDateTimeNumeric, args.rval())
             : intl_FormatRelativeTime(cx, rtf, t, relDateTimeUnit,
                                       relDateTimeNumeric, args.rval());
#else
  MOZ_ASSERT(!formatToParts);
  return intl_FormatRelativeTime(cx, rtf, t, relDateTimeUnit,
                                 relDateTimeNumeric, args.rval());
#endif
}

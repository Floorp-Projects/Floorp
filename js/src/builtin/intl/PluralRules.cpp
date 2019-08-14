/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of the Intl.PluralRules proposal. */

#include "builtin/intl/PluralRules.h"

#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"

#include "builtin/Array.h"
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

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

using mozilla::AssertedCast;

using js::intl::CallICU;
using js::intl::GetAvailableLocales;
using js::intl::IcuLocale;

const JSClassOps PluralRulesObject::classOps_ = {nullptr, /* addProperty */
                                                 nullptr, /* delProperty */
                                                 nullptr, /* enumerate */
                                                 nullptr, /* newEnumerate */
                                                 nullptr, /* resolve */
                                                 nullptr, /* mayResolve */
                                                 PluralRulesObject::finalize};

const JSClass PluralRulesObject::class_ = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(PluralRulesObject::SLOT_COUNT) |
        JSCLASS_FOREGROUND_FINALIZE,
    &PluralRulesObject::classOps_};

static bool pluralRules_toSource(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setString(cx->names().PluralRules);
  return true;
}

static const JSFunctionSpec pluralRules_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf",
                      "Intl_PluralRules_supportedLocalesOf", 1, 0),
    JS_FS_END};

static const JSFunctionSpec pluralRules_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_PluralRules_resolvedOptions", 0,
                      0),
    JS_SELF_HOSTED_FN("select", "Intl_PluralRules_select", 1, 0),
    JS_FN(js_toSource_str, pluralRules_toSource, 0, 0), JS_FS_END};

/**
 * PluralRules constructor.
 * Spec: ECMAScript 402 API, PluralRules, 13.2.1
 */
static bool PluralRules(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Intl.PluralRules")) {
    return false;
  }

  // Step 2 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_Null, &proto)) {
    return false;
  }

  if (!proto) {
    proto = GlobalObject::getOrCreatePluralRulesPrototype(cx, cx->global());
    if (!proto) {
      return false;
    }
  }

  Rooted<PluralRulesObject*> pluralRules(cx);
  pluralRules = NewObjectWithGivenProto<PluralRulesObject>(cx, proto);
  if (!pluralRules) {
    return false;
  }

  HandleValue locales = args.get(0);
  HandleValue options = args.get(1);

  // Step 3.
  if (!intl::InitializeObject(cx, pluralRules,
                              cx->names().InitializePluralRules, locales,
                              options)) {
    return false;
  }

  args.rval().setObject(*pluralRules);
  return true;
}

void js::PluralRulesObject::finalize(JSFreeOp* fop, JSObject* obj) {
  MOZ_ASSERT(fop->onMainThread());

  auto* pluralRules = &obj->as<PluralRulesObject>();
  UPluralRules* pr = pluralRules->getPluralRules();
  UNumberFormatter* nf = pluralRules->getNumberFormatter();
  UFormattedNumber* formatted = pluralRules->getFormattedNumber();

  if (pr) {
    uplrules_close(pr);
  }
  if (nf) {
    unumf_close(nf);
  }
  if (formatted) {
    unumf_closeResult(formatted);
  }
}

JSObject* js::CreatePluralRulesPrototype(JSContext* cx, HandleObject Intl,
                                         Handle<GlobalObject*> global) {
  RootedFunction ctor(cx);
  ctor =
      global->createConstructor(cx, &PluralRules, cx->names().PluralRules, 0);
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

  if (!JS_DefineFunctions(cx, ctor, pluralRules_static_methods)) {
    return nullptr;
  }

  if (!JS_DefineFunctions(cx, proto, pluralRules_methods)) {
    return nullptr;
  }

  RootedValue ctorValue(cx, ObjectValue(*ctor));
  if (!DefineDataProperty(cx, Intl, cx->names().PluralRules, ctorValue, 0)) {
    return nullptr;
  }

  return proto;
}

bool js::intl_PluralRules_availableLocales(JSContext* cx, unsigned argc,
                                           Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 0);

  // We're going to use ULocale availableLocales as per ICU recommendation:
  // https://ssl.icu-project.org/trac/ticket/12756
  return GetAvailableLocales(cx, uloc_countAvailable, uloc_getAvailable,
                             args.rval());
}

/**
 * This creates a new UNumberFormatter with calculated digit formatting
 * properties for PluralRules.
 *
 * This is similar to NewUNumberFormatter but doesn't allow for currency or
 * percent types.
 */
static UNumberFormatter* NewUNumberFormatterForPluralRules(
    JSContext* cx, Handle<PluralRulesObject*> pluralRules) {
  RootedObject internals(cx, intl::GetInternalsObject(cx, pluralRules));
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

  intl::NumberFormatterSkeleton skeleton(cx);

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
  } else {
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

  if (!skeleton.roundingModeHalfUp()) {
    return nullptr;
  }

  return skeleton.toFormatter(cx, locale.get());
}

static UFormattedNumber* NewUFormattedNumberForPluralRules(JSContext* cx) {
  UErrorCode status = U_ZERO_ERROR;
  UFormattedNumber* formatted = unumf_openResult(&status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return nullptr;
  }
  return formatted;
}

/**
 * Returns a new UPluralRules with the locale and type options of the given
 * PluralRules.
 */
static UPluralRules* NewUPluralRules(JSContext* cx,
                                     Handle<PluralRulesObject*> pluralRules) {
  RootedObject internals(cx, intl::GetInternalsObject(cx, pluralRules));
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

  if (!GetProperty(cx, internals, internals, cx->names().type, &value)) {
    return nullptr;
  }

  UPluralType category;
  {
    JSLinearString* type = value.toString()->ensureLinear(cx);
    if (!type) {
      return nullptr;
    }

    if (StringEqualsAscii(type, "cardinal")) {
      category = UPLURAL_TYPE_CARDINAL;
    } else {
      MOZ_ASSERT(StringEqualsAscii(type, "ordinal"));
      category = UPLURAL_TYPE_ORDINAL;
    }
  }

  UErrorCode status = U_ZERO_ERROR;
  UPluralRules* pr =
      uplrules_openForType(IcuLocale(locale.get()), category, &status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return nullptr;
  }
  return pr;
}

bool js::intl_SelectPluralRule(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 2);

  Rooted<PluralRulesObject*> pluralRules(
      cx, &args[0].toObject().as<PluralRulesObject>());

  double x = args[1].toNumber();

  // Obtain a cached UPluralRules object.
  UPluralRules* pr = pluralRules->getPluralRules();
  if (!pr) {
    pr = NewUPluralRules(cx, pluralRules);
    if (!pr) {
      return false;
    }
    pluralRules->setPluralRules(pr);
  }

  // Obtain a cached UNumberFormat object.
  UNumberFormatter* nf = pluralRules->getNumberFormatter();
  if (!nf) {
    nf = NewUNumberFormatterForPluralRules(cx, pluralRules);
    if (!nf) {
      return false;
    }
    pluralRules->setNumberFormatter(nf);
  }

  // Obtain a cached UFormattedNumber object.
  UFormattedNumber* formatted = pluralRules->getFormattedNumber();
  if (!formatted) {
    formatted = NewUFormattedNumberForPluralRules(cx);
    if (!formatted) {
      return false;
    }
    pluralRules->setFormattedNumber(formatted);
  }

  UErrorCode status = U_ZERO_ERROR;
  unumf_formatDouble(nf, x, formatted, &status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return false;
  }

  JSString* str = CallICU(
      cx, [pr, formatted](UChar* chars, int32_t size, UErrorCode* status) {
        return uplrules_selectFormatted(pr, formatted, chars, size, status);
      });
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

bool js::intl_GetPluralCategories(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 1);

  Rooted<PluralRulesObject*> pluralRules(
      cx, &args[0].toObject().as<PluralRulesObject>());

  // Obtain a cached UPluralRules object.
  UPluralRules* pr = pluralRules->getPluralRules();
  if (!pr) {
    pr = NewUPluralRules(cx, pluralRules);
    if (!pr) {
      return false;
    }
    pluralRules->setPluralRules(pr);
  }

  UErrorCode status = U_ZERO_ERROR;
  UEnumeration* ue = uplrules_getKeywords(pr, &status);
  if (U_FAILURE(status)) {
    intl::ReportInternalError(cx);
    return false;
  }
  ScopedICUObject<UEnumeration, uenum_close> closeEnum(ue);

  RootedObject res(cx, NewDenseEmptyArray(cx));
  if (!res) {
    return false;
  }

  do {
    int32_t catSize;
    const char* cat = uenum_next(ue, &catSize, &status);
    if (U_FAILURE(status)) {
      intl::ReportInternalError(cx);
      return false;
    }

    if (!cat) {
      break;
    }

    MOZ_ASSERT(catSize >= 0);
    JSString* str = NewStringCopyN<CanGC>(cx, cat, catSize);
    if (!str) {
      return false;
    }

    if (!NewbornArrayPush(cx, res, StringValue(str))) {
      return false;
    }
  } while (true);

  args.rval().setObject(*res);
  return true;
}

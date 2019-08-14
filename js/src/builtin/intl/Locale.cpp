/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Intl.Locale implementation. */

#include "builtin/intl/Locale.h"

#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/Range.h"
#include "mozilla/TextUtils.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "jsapi.h"

#include "builtin/intl/CommonFunctions.h"
#include "js/TypeDecls.h"
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

const JSClass LocaleObject::class_ = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(LocaleObject::SLOT_COUNT),
};

static bool locale_toSource(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setString(cx->names().Locale);
  return true;
}

static const JSFunctionSpec locale_methods[] = {
    JS_SELF_HOSTED_FN("maximize", "Intl_Locale_maximize", 0, 0),
    JS_SELF_HOSTED_FN("minimize", "Intl_Locale_minimize", 0, 0),
    JS_SELF_HOSTED_FN("toString", "Intl_Locale_toString", 0, 0),
    JS_FN(js_toSource_str, locale_toSource, 0, 0), JS_FS_END};

static const JSPropertySpec locale_properties[] = {
    JS_SELF_HOSTED_GET("baseName", "$Intl_Locale_baseName_get", 0),
    JS_SELF_HOSTED_GET("calendar", "$Intl_Locale_calendar_get", 0),
    JS_SELF_HOSTED_GET("collation", "$Intl_Locale_collation_get", 0),
    JS_SELF_HOSTED_GET("hourCycle", "$Intl_Locale_hourCycle_get", 0),
    JS_SELF_HOSTED_GET("caseFirst", "$Intl_Locale_caseFirst_get", 0),
    JS_SELF_HOSTED_GET("numeric", "$Intl_Locale_numeric_get", 0),
    JS_SELF_HOSTED_GET("numberingSystem", "$Intl_Locale_numberingSystem_get",
                       0),
    JS_SELF_HOSTED_GET("language", "$Intl_Locale_language_get", 0),
    JS_SELF_HOSTED_GET("script", "$Intl_Locale_script_get", 0),
    JS_SELF_HOSTED_GET("region", "$Intl_Locale_region_get", 0),
    JS_STRING_SYM_PS(toStringTag, "Intl.Locale", JSPROP_READONLY),
    JS_PS_END};

static LocaleObject* CreateLocaleObject(JSContext* cx, HandleObject prototype) {
  RootedObject proto(cx, prototype);
  if (!proto) {
    proto = GlobalObject::getOrCreateLocalePrototype(cx, cx->global());
    if (!proto) {
      return nullptr;
    }
  }

  LocaleObject* locale = NewObjectWithGivenProto<LocaleObject>(cx, proto);
  if (!locale) {
    return nullptr;
  }

  locale->setReservedSlot(LocaleObject::INTERNALS_SLOT, NullValue());

  return locale;
}

/**
 * Intl.Locale( tag[, options] )
 */
static bool Locale(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Intl.Locale")) {
    return false;
  }

  // Steps 2-6 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_Null, &proto)) {
    return false;
  }

  Rooted<LocaleObject*> locale(cx, CreateLocaleObject(cx, proto));
  if (!locale) {
    return false;
  }

  HandleValue tag = args.get(0);
  HandleValue options = args.get(1);

  // Steps 7-37.
  if (!intl::InitializeObject(cx, locale, cx->names().InitializeLocale, tag,
                              options)) {
    return false;
  }

  // Step 38.
  args.rval().setObject(*locale);
  return true;
}

JSObject* js::CreateLocalePrototype(JSContext* cx, HandleObject Intl,
                                    Handle<GlobalObject*> global) {
  RootedFunction ctor(
      cx, GlobalObject::createConstructor(cx, &Locale, cx->names().Locale, 1));
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

  if (!DefinePropertiesAndFunctions(cx, proto, locale_properties,
                                    locale_methods)) {
    return nullptr;
  }

  RootedValue ctorValue(cx, ObjectValue(*ctor));
  if (!DefineDataProperty(cx, Intl, cx->names().Locale, ctorValue, 0)) {
    return nullptr;
  }

  return proto;
}

/* static */ bool js::GlobalObject::addLocaleConstructor(JSContext* cx,
                                                         HandleObject intl) {
  Handle<GlobalObject*> global = cx->global();

  {
    const Value& proto = global->getReservedSlot(LOCALE_PROTO);
    if (!proto.isUndefined()) {
      MOZ_ASSERT(proto.isObject());
      JS_ReportErrorASCII(cx,
                          "the Locale constructor can't be added multiple "
                          "times in the same global");
      return false;
    }
  }

  JSObject* localeProto = CreateLocalePrototype(cx, intl, global);
  if (!localeProto) {
    return false;
  }

  global->setReservedSlot(LOCALE_PROTO, ObjectValue(*localeProto));
  return true;
}

bool js::AddLocaleConstructor(JSContext* cx, JS::Handle<JSObject*> intl) {
  return GlobalObject::addLocaleConstructor(cx, intl);
}

bool js::intl_CreateUninitializedLocale(JSContext* cx, unsigned argc,
                                        Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 0);

  LocaleObject* locale = CreateLocaleObject(cx, nullptr);
  if (!locale) {
    return false;
  }

  args.rval().setObject(*locale);
  return true;
}

#ifdef DEBUG

template <typename CharT>
struct LanguageTagValidator {
  bool operator()(mozilla::Range<const CharT> language) const {
    // Tell the analysis the |std::all_of| function can't GC.
    JS::AutoSuppressGCAnalysis nogc;

    // BNF: unicode_language_subtag = alpha{2,3} | alpha{5,8} ;
    // Canonical form is lower case.
    return ((2 <= language.length() && language.length() <= 3) ||
            (5 <= language.length() && language.length() <= 8)) &&
           std::all_of(language.begin().get(), language.end().get(),
                       mozilla::IsAsciiLowercaseAlpha<CharT>);
  }
};

template <typename CharT>
struct ScriptTagValidator {
  bool operator()(mozilla::Range<const CharT> script) const {
    // Tell the analysis the |std::all_of| function can't GC.
    JS::AutoSuppressGCAnalysis nogc;

    // BNF: unicode_script_subtag = alpha{4} ;
    // Canonical form is title case.
    return script.length() == 4 && mozilla::IsAsciiUppercaseAlpha(script[0]) &&
           std::all_of(std::next(script.begin().get()), script.end().get(),
                       mozilla::IsAsciiLowercaseAlpha<CharT>);
  }
};

template <typename CharT>
struct RegionTagValidator {
  bool operator()(mozilla::Range<const CharT> region) const {
    // Tell the analysis the |std::all_of| function can't GC.
    JS::AutoSuppressGCAnalysis nogc;

    // BNF: unicode_region_subtag = (alpha{2} | digit{3}) ;
    // Canonical form is upper case.
    return (region.length() == 2 &&
            std::all_of(region.begin().get(), region.end().get(),
                        mozilla::IsAsciiUppercaseAlpha<CharT>)) ||
           (region.length() == 3 &&
            std::all_of(region.begin().get(), region.end().get(),
                        mozilla::IsAsciiDigit<CharT>));
  }
};

template <template <typename> class Validator, typename CharT>
static bool IsStructurallyValidSubtag(mozilla::Range<const CharT> subtag) {
  return Validator<CharT>{}(subtag);
}

template <template <typename> class Validator>
static bool IsStructurallyValidSubtag(JSLinearString* subtag) {
  JS::AutoCheckCannotGC nogc;
  return subtag->hasLatin1Chars()
             ? Validator<JS::Latin1Char>{}(subtag->latin1Range(nogc))
             : Validator<char16_t>{}(subtag->twoByteRange(nogc));
}

template <typename T>
static bool IsStructurallyValidLanguageTag(const T& language) {
  return IsStructurallyValidSubtag<LanguageTagValidator>(language);
}

template <typename T>
static bool IsStructurallyValidScriptTag(const T& script) {
  return IsStructurallyValidSubtag<ScriptTagValidator>(script);
}

template <typename T>
static bool IsStructurallyValidRegionTag(const T& region) {
  return IsStructurallyValidSubtag<RegionTagValidator>(region);
}

#endif /* DEBUG */

// unicode_language_subtag = alpha{2,3} | alpha{5,8} ;
static constexpr size_t LanguageTagMaxLength = 8;

// unicode_script_subtag = alpha{4} ;
static constexpr size_t ScriptTagMaxLength = 4;

// unicode_region_subtag = (alpha{2} | digit{3}) ;
static constexpr size_t RegionTagMaxLength = 3;

// Zero-terminated ICU Locale ID.
using LocaleId =
    js::Vector<char, LanguageTagMaxLength + 1 + ScriptTagMaxLength + 1 +
                         RegionTagMaxLength + 1>;

struct LocaleSubtags {
  using Subtag = mozilla::Range<const char>;

  Subtag language;
  Subtag script;
  Subtag region;

  LocaleSubtags(Subtag language, Subtag script, Subtag region)
      : language(language), script(script), region(region) {}
};

// Split the language, script, and region subtags from the input ICU locale ID.
//
// ICU provides |uloc_getLanguage|, |uloc_getScript|, and |uloc_getCountry| to
// retrieve these subtags, but unfortunately these functions are rather slow, so
// we use our own implementation.
static LocaleSubtags GetLocaleSubtags(const LocaleId& localeId) {
  // Locale ID should be zero-terminated for ICU.
  MOZ_ASSERT(localeId.back() == '\0');

  using Subtag = LocaleSubtags::Subtag;

  // mozilla::Range uses 'const' members, so we can't assign to it. Use |Maybe|
  // to defer initialization to workaround this.
  mozilla::Maybe<Subtag> language, script, region;

  size_t subtagStart = 0;
  for (size_t i = 0; i < localeId.length(); i++) {
    char c = localeId[i];

    // Skip over any characters until we hit either separator char or the end.
    if (c != '_' && c != '-' && c != '\0') {
      continue;
    }

    size_t length = i - subtagStart;
    const char* start = localeId.begin() + subtagStart;
    if (subtagStart == 0) {
      // unicode_language_subtag = alpha{2,3} | alpha{5,8} ;
      if ((2 <= length && length <= 3) || (5 <= length && length <= 8)) {
        language.emplace(start, length);
        MOZ_ASSERT(IsStructurallyValidLanguageTag(*language));
      }
    } else if (length == 4) {
      // unicode_script_subtag = alpha{4} ;
      script.emplace(start, length);
      MOZ_ASSERT(IsStructurallyValidScriptTag(*script));
    } else if (length == 2 || length == 3) {
      // unicode_region_subtag = (alpha{2} | digit{3}) ;
      region.emplace(start, length);
      MOZ_ASSERT(IsStructurallyValidRegionTag(*region));
    } else {
      // Ignore any trailing variant or extension subtags.
      break;
    }

    subtagStart = i + 1;
  }

  return {language.valueOr(Subtag()), script.valueOr(Subtag()),
          region.valueOr(Subtag())};
}

enum class LikelySubtags : bool { Add, Remove };

// Return true iff the input subtags are already maximized resp. minimized.
static bool HasLikelySubtags(LikelySubtags likelySubtags,
                             JSLinearString* language, JSLinearString* script,
                             JSLinearString* region) {
  // The language tag is already maximized if language, script, and region
  // subtags are present and no placeholder subtags ("und", "Zzzz", "ZZ")
  // are used.
  if (likelySubtags == LikelySubtags::Add) {
    return !StringEqualsAscii(language, "und") && script &&
           !StringEqualsAscii(script, "Zzzz") && region &&
           !StringEqualsAscii(region, "ZZ");
  }

  // The language tag is already minimized if it only contains a language
  // subtag whose value is not the placeholder value "und".
  return !StringEqualsAscii(language, "und") && !script && !region;
}

// Create an ICU locale ID from the given language, script, and region subtags.
static bool CreateLocaleForLikelySubtags(JSLinearString* language,
                                         JSLinearString* script,
                                         JSLinearString* region,
                                         LocaleId& locale) {
  MOZ_ASSERT(locale.length() == 0);

  auto appendSubtag = [&locale](JSLinearString* subtag) {
    if (locale.length() > 0 && !locale.append('_')) {
      return false;
    }
    if (!locale.growBy(subtag->length())) {
      return false;
    }
    char* dest = locale.end() - subtag->length();
    CopyChars(reinterpret_cast<Latin1Char*>(dest), *subtag);
    return true;
  };

  // Append the language subtag.
  if (!appendSubtag(language)) {
    return false;
  }

  // Append the script subtag if present.
  if (script && !appendSubtag(script)) {
    return false;
  }

  // Append the region subtag if present.
  if (region && !appendSubtag(region)) {
    return false;
  }

  // Zero-terminated for use with ICU.
  return locale.append('\0');
}

template <decltype(uloc_addLikelySubtags) likelySubtagsFn>
static bool CallLikelySubtags(JSContext* cx, const LocaleId& localeId,
                              LocaleId& result) {
  // Locale ID must be zero-terminated before passing it to ICU.
  MOZ_ASSERT(localeId.back() == '\0');
  MOZ_ASSERT(result.length() == 0);

  // Ensure there's enough room for the result.
  MOZ_ALWAYS_TRUE(result.resize(LocaleId::InlineLength));

  int32_t length = intl::CallICU(
      cx,
      [&localeId](char* chars, int32_t size, UErrorCode* status) {
        return likelySubtagsFn(localeId.begin(), chars, size, status);
      },
      result);
  if (length < 0) {
    return false;
  }

  MOZ_ASSERT(
      size_t(length) <= LocaleId::InlineLength,
      "Unexpected extra subtags were added by ICU. If this assertion ever "
      "fails, simply remove it and move on like nothing ever happended.");

  // Resize the vector to the actual string length.
  result.shrinkTo(length);

  // Zero-terminated for use with ICU.
  return result.append('\0');
}

// Return the array |[language, script or undefined, region or undefined]|.
static ArrayObject* CreateLikelySubtagsResult(JSContext* cx,
                                              HandleValue language,
                                              HandleValue script,
                                              HandleValue region) {
  enum LikelySubtagsResult {
    LikelySubtagsResult_Language = 0,
    LikelySubtagsResult_Script,
    LikelySubtagsResult_Region,

    LikelySubtagsResult_Length
  };

  ArrayObject* result =
      NewDenseFullyAllocatedArray(cx, LikelySubtagsResult_Length);
  if (!result) {
    return nullptr;
  }
  result->setDenseInitializedLength(LikelySubtagsResult_Length);

  result->initDenseElement(LikelySubtagsResult_Language, language);
  result->initDenseElement(LikelySubtagsResult_Script, script);
  result->initDenseElement(LikelySubtagsResult_Region, region);

  return result;
}

// The canonical way to compute the Unicode BCP 47 locale identifier with likely
// subtags is as follows:
//
// 1. Call uloc_forLanguageTag() to transform the input locale identifer into an
//    ICU locale ID.
// 2. Call uloc_addLikelySubtags() to add the likely subtags to the locale ID.
// 3. Call uloc_toLanguageTag() to transform the resulting locale ID back into
//    a Unicode BCP 47 locale identifier.
//
// Since uloc_forLanguageTag() and uloc_toLanguageTag() are both kind of slow
// and we know, by construction, that the input Unicode BCP 47 locale identifier
// only contains a language, script, and region subtag, we can avoid both calls
// if we implement their guts ourselves, see CreateLocaleForLikelySubtags().
// (Where "slow" means about 50% of the execution time of Locale.p.maximize.)
static ArrayObject* LikelySubtags(JSContext* cx, LikelySubtags likelySubtags,
                                  const CallArgs& args) {
  MOZ_ASSERT(args.length() == 3);
  MOZ_ASSERT(args[0].isString());
  MOZ_ASSERT(args[1].isString() || args[1].isUndefined());
  MOZ_ASSERT(args[2].isString() || args[2].isUndefined());

  RootedLinearString language(cx, args[0].toString()->ensureLinear(cx));
  if (!language) {
    return nullptr;
  }
  MOZ_ASSERT(IsStructurallyValidLanguageTag(language));

  RootedLinearString script(cx);
  if (args[1].isString()) {
    script = args[1].toString()->ensureLinear(cx);
    if (!script) {
      return nullptr;
    }
    MOZ_ASSERT(IsStructurallyValidScriptTag(script));
  }

  RootedLinearString region(cx);
  if (args[2].isString()) {
    region = args[2].toString()->ensureLinear(cx);
    if (!region) {
      return nullptr;
    }
    MOZ_ASSERT(IsStructurallyValidRegionTag(region));
  }

  // Return early if the input is already maximized/minimized.
  if (HasLikelySubtags(likelySubtags, language, script, region)) {
    return CreateLikelySubtagsResult(cx, args[0], args[1], args[2]);
  }

  // Create the locale ID for the input arguments.
  LocaleId locale(cx);
  if (!CreateLocaleForLikelySubtags(language, script, region, locale)) {
    return nullptr;
  }

  // UTS #35 requires that locale ID is maximized before its likely subtags are
  // removed, so we need to call uloc_addLikelySubtags() for both cases.
  // See <https://ssl.icu-project.org/trac/ticket/10220> and
  // <https://ssl.icu-project.org/trac/ticket/12345>.

  LocaleId localeLikelySubtags(cx);

  // Add likely subtags to the locale ID. When minimizing we can skip adding the
  // likely subtags for already maximized tags. (When maximizing we've already
  // verified above that the tag is missing likely subtags.)
  bool addLikelySubtags =
      likelySubtags == LikelySubtags::Add ||
      !HasLikelySubtags(LikelySubtags::Add, language, script, region);

  if (addLikelySubtags) {
    if (!CallLikelySubtags<uloc_addLikelySubtags>(cx, locale,
                                                  localeLikelySubtags)) {
      return nullptr;
    }
  }

  // Now that we've succesfully maximized the locale, we can minimize it.
  if (likelySubtags == LikelySubtags::Remove) {
    if (addLikelySubtags) {
      // Copy the maximized subtags back into |locale|.
      locale = std::move(localeLikelySubtags);
      localeLikelySubtags = LocaleId(cx);
    }

    // Remove likely subtags from the locale ID.
    if (!CallLikelySubtags<uloc_minimizeSubtags>(cx, locale,
                                                 localeLikelySubtags)) {
      return nullptr;
    }
  }

  // Retrieve the individual language, script, and region subtags from the
  // resulting locale ID.
  LocaleSubtags subtags = GetLocaleSubtags(localeLikelySubtags);

  // Convert each subtag back to a JSString. Use |undefined| for absent subtags.
  auto toValue = [cx](const auto& subtag, MutableHandleValue result) {
    if (subtag.length() > 0) {
      JSLinearString* str =
          NewStringCopyN<CanGC>(cx, subtag.begin().get(), subtag.length());
      if (!str) {
        return false;
      }
      result.setString(str);
    } else {
      result.setUndefined();
    }
    return true;
  };

  RootedValue languageValue(cx);
  if (!toValue(subtags.language, &languageValue)) {
    return nullptr;
  }
  if (languageValue.isUndefined()) {
    // ICU replaces "und" with the empty string. Handle this case separately.
    JSLinearString* str = NewStringCopyZ<CanGC>(cx, "und");
    if (!str) {
      return nullptr;
    }
    languageValue.setString(str);
  }

  RootedValue scriptValue(cx);
  if (!toValue(subtags.script, &scriptValue)) {
    return nullptr;
  }

  RootedValue regionValue(cx);
  if (!toValue(subtags.region, &regionValue)) {
    return nullptr;
  }

  // Return the language, script, and region subtags in an array.
  return CreateLikelySubtagsResult(cx, languageValue, scriptValue, regionValue);
}

bool js::intl_AddLikelySubtags(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  ArrayObject* result = LikelySubtags(cx, LikelySubtags::Add, args);
  if (!result) {
    return false;
  }
  args.rval().setObject(*result);
  return true;
}

bool js::intl_RemoveLikelySubtags(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  ArrayObject* result = LikelySubtags(cx, LikelySubtags::Remove, args);
  if (!result) {
    return false;
  }
  args.rval().setObject(*result);
  return true;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Intl.Locale implementation. */

#include "builtin/intl/Locale.h"

#include "mozilla/Assertions.h"
#include "jsapi.h"

#include "builtin/intl/CommonFunctions.h"
#include "js/TypeDecls.h"
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

const Class LocaleObject::class_ = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(LocaleObject::SLOT_COUNT),
};

static bool locale_toSource(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setString(cx->names().Locale);
  return true;
}

static const JSFunctionSpec locale_methods[] = {
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


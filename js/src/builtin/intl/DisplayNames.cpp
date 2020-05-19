/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Intl.DisplayNames implementation. */

#include "builtin/intl/DisplayNames.h"

#include "jspubtd.h"

#include "builtin/intl/CommonFunctions.h"
#include "gc/AllocKind.h"
#include "js/CallArgs.h"
#include "js/Class.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "vm/GlobalObject.h"
#include "vm/JSAtom.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/SelfHosting.h"
#include "vm/Stack.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

const JSClass DisplayNamesObject::class_ = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(DisplayNamesObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_DisplayNames),
    JS_NULL_CLASS_OPS, &DisplayNamesObject::classSpec_};

const JSClass& DisplayNamesObject::protoClass_ = PlainObject::class_;

static bool displayNames_toSource(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setString(cx->names().DisplayNames);
  return true;
}

static const JSFunctionSpec displayNames_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf",
                      "Intl_DisplayNames_supportedLocalesOf", 1, 0),
    JS_FS_END};

static const JSFunctionSpec displayNames_methods[] = {
    JS_SELF_HOSTED_FN("of", "Intl_DisplayNames_of", 1, 0),
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_DisplayNames_resolvedOptions", 0,
                      0),
    JS_FN(js_toSource_str, displayNames_toSource, 0, 0), JS_FS_END};

static const JSPropertySpec displayNames_properties[] = {
    JS_STRING_SYM_PS(toStringTag, "Intl.DisplayNames", JSPROP_READONLY),
    JS_PS_END};

static bool DisplayNames(JSContext* cx, unsigned argc, Value* vp);

const ClassSpec DisplayNamesObject::classSpec_ = {
    GenericCreateConstructor<DisplayNames, 0, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<DisplayNamesObject>,
    displayNames_static_methods,
    nullptr,
    displayNames_methods,
    displayNames_properties,
    nullptr,
    ClassSpec::DontDefineConstructor};

enum class DisplayNamesOptions {
  Standard,

  // Calendar display names are no longer available with the current spec
  // proposal text, but may be re-enabled in the future. For our internal use
  // we still need to have them present, so use a feature guard for now.
  EnableMozExtensions,
};

/**
 * Initialize a new Intl.DisplayNames object using the named self-hosted
 * function.
 */
static bool InitializeDisplayNamesObject(JSContext* cx, HandleObject obj,
                                         HandlePropertyName initializer,
                                         HandleValue locales,
                                         HandleValue options,
                                         DisplayNamesOptions dnoptions) {
  FixedInvokeArgs<4> args(cx);

  args[0].setObject(*obj);
  args[1].set(locales);
  args[2].set(options);
  args[3].setBoolean(dnoptions == DisplayNamesOptions::EnableMozExtensions);

  RootedValue ignored(cx);
  if (!CallSelfHostedFunction(cx, initializer, NullHandleValue, args,
                              &ignored)) {
    return false;
  }

  MOZ_ASSERT(ignored.isUndefined(),
             "Unexpected return value from non-legacy Intl object initializer");
  return true;
}

/**
 * Intl.DisplayNames ([ locales [ , options ]])
 */
static bool DisplayNames(JSContext* cx, const CallArgs& args,
                         DisplayNamesOptions dnoptions) {
  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Intl.DisplayNames")) {
    return false;
  }

  // Step 2 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
  RootedObject proto(cx);
  if (dnoptions == DisplayNamesOptions::Standard) {
    if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_DisplayNames,
                                            &proto)) {
      return false;
    }
  } else {
    RootedObject newTarget(cx, &args.newTarget().toObject());
    if (!GetPrototypeFromConstructor(cx, newTarget, JSProto_Null, &proto)) {
      return false;
    }
  }

  Rooted<DisplayNamesObject*> displayNames(cx);
  displayNames = NewObjectWithClassProto<DisplayNamesObject>(cx, proto);
  if (!displayNames) {
    return false;
  }

  HandleValue locales = args.get(0);
  HandleValue options = args.get(1);

  // Steps 3-26.
  if (!InitializeDisplayNamesObject(cx, displayNames,
                                    cx->names().InitializeDisplayNames, locales,
                                    options, dnoptions)) {
    return false;
  }

  // Step 27.
  args.rval().setObject(*displayNames);
  return true;
}

static bool DisplayNames(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return DisplayNames(cx, args, DisplayNamesOptions::Standard);
}

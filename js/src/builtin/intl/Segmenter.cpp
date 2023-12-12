/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Intl.Segmenter implementation. */

#include "builtin/intl/Segmenter.h"

#include "mozilla/Assertions.h"

#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/intl/CommonFunctions.h"
#include "gc/AllocKind.h"
#include "gc/GCContext.h"
#include "js/CallArgs.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"
#include "vm/PlainObject.h"
#include "vm/WellKnownAtom.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

const JSClass SegmenterObject::class_ = {
    "Intl.Segmenter",
    JSCLASS_HAS_RESERVED_SLOTS(SegmenterObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_Segmenter),
    nullptr,
    &SegmenterObject::classSpec_,
};

const JSClass& SegmenterObject::protoClass_ = PlainObject::class_;

static bool segmenter_toSource(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setString(cx->names().Segmenter);
  return true;
}

static const JSFunctionSpec segmenter_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf", "Intl_Segmenter_supportedLocalesOf",
                      1, 0),
    JS_FS_END,
};

static const JSFunctionSpec segmenter_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_Segmenter_resolvedOptions", 0,
                      0),
    JS_FN("toSource", segmenter_toSource, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec segmenter_properties[] = {
    JS_STRING_SYM_PS(toStringTag, "Intl.Segmenter", JSPROP_READONLY),
    JS_PS_END,
};

static bool Segmenter(JSContext* cx, unsigned argc, Value* vp);

const ClassSpec SegmenterObject::classSpec_ = {
    GenericCreateConstructor<Segmenter, 0, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<SegmenterObject>,
    segmenter_static_methods,
    nullptr,
    segmenter_methods,
    segmenter_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};

/**
 * Intl.Segmenter ([ locales [ , options ]])
 */
static bool Segmenter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Intl.Segmenter")) {
    return false;
  }

  // Steps 2-3 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_Segmenter,
                                          &proto)) {
    return false;
  }

  Rooted<SegmenterObject*> segmenter(cx);
  segmenter = NewObjectWithClassProto<SegmenterObject>(cx, proto);
  if (!segmenter) {
    return false;
  }

  HandleValue locales = args.get(0);
  HandleValue options = args.get(1);

  // Steps 4-13.
  if (!intl::InitializeObject(cx, segmenter, cx->names().InitializeSegmenter,
                              locales, options)) {
    return false;
  }

  // Step 14.
  args.rval().setObject(*segmenter);
  return true;
}

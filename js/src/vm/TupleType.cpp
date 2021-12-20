/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/TupleType.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/HashFunctions.h"

#include "jsapi.h"

#include "gc/Allocator.h"
#include "gc/Tracer.h"
#include "vm/JSContext.h"
#include "vm/SelfHosting.h"

#include "vm/JSObject-inl.h"

using namespace js;

static bool TupleConstructor(JSContext* cx, unsigned argc, Value* vp);

const JSClass TupleType::class_ = {"tuple", 0, JS_NULL_CLASS_OPS,
                                   &TupleType::classSpec_};

const JSClass TupleType::protoClass_ = {
    "Tuple.prototype", JSCLASS_HAS_CACHED_PROTO(JSProto_Tuple),
    JS_NULL_CLASS_OPS, &TupleType::classSpec_};

const ClassSpec TupleType::classSpec_ = {
    GenericCreateConstructor<TupleConstructor, 1, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<TupleType>,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr};

TupleType* TupleType::create(JSContext* cx) {
  Rooted<TaggedProto> proto(cx, TaggedProto(nullptr));
  return NewObjectWithGivenTaggedProto<TupleType>(cx, proto);
}

// Record and Tuple proposal section 9.2.1
static bool TupleConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (args.isConstructing()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_CONSTRUCTOR, "Tuple");
    return false;
  }

  if (args.length() > 0) {
    MOZ_CRASH("Only empty tuples are supported");
    return false;
  }

  TupleType* tup = TupleType::create(cx);
  if (!tup) {
    return false;
  }

  args.rval().setExtendedPrimitive(*tup);
  return true;
}

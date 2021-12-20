/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/RecordType.h"

#include "vm/JSContext.h"
#include "vm/JSObject-inl.h"

using namespace js;

static bool RecordConstructor(JSContext* cx, unsigned argc, Value* vp);

const JSClass RecordType::class_ = {"record", 0, JS_NULL_CLASS_OPS,
                                    &RecordType::classSpec_};

const ClassSpec RecordType::classSpec_ = {
    GenericCreateConstructor<RecordConstructor, 1, gc::AllocKind::FUNCTION>,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr};

RecordType* RecordType::create(JSContext* cx) {
  Rooted<TaggedProto> proto(cx, TaggedProto(nullptr));
  return NewObjectWithGivenTaggedProto<RecordType>(cx, proto);
}

// Record and Record proposal section 9.2.1
static bool RecordConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (args.isConstructing()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_CONSTRUCTOR, "Record");
    return false;
  }

  if (args.length() > 0) {
    MOZ_CRASH("Only empty records are supoprted.");
    return false;
  }

  RecordType* rec = RecordType::create(cx);
  if (!rec) {
    return false;
  }

  args.rval().setExtendedPrimitive(*rec);
  return true;
}

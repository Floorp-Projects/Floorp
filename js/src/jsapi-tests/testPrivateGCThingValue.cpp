/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "jsapi.h"

#include "js/CompilationAndEvaluation.h"  // JS::Compile
#include "js/HeapAPI.h"
#include "js/SourceText.h"  // JS::Source{Ownership,Text}
#include "jsapi-tests/tests.h"
#include "util/Text.h"

class TestTracer final : public JS::CallbackTracer {
  void onChild(JS::GCCellPtr thing, const char* name) override {
    if (thing.asCell() == expectedCell && thing.kind() == expectedKind) {
      found = true;
    }
  }

 public:
  js::gc::Cell* expectedCell;
  JS::TraceKind expectedKind;
  bool found;

  explicit TestTracer(JSContext* cx)
      : JS::CallbackTracer(cx),
        expectedCell(nullptr),
        expectedKind(static_cast<JS::TraceKind>(0)),
        found(false) {}
};

static const JSClass TestClass = {"TestClass", JSCLASS_HAS_RESERVED_SLOTS(1)};

BEGIN_TEST(testPrivateGCThingValue) {
  JS::RootedObject obj(cx, JS_NewObject(cx, &TestClass));
  CHECK(obj);

  // Make a JSScript to stick into a PrivateGCThingValue.
  static const char code[] = "'objet petit a'";

  JS::CompileOptions options(cx);
  options.setFileAndLine(__FILE__, __LINE__);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  CHECK(srcBuf.init(cx, code, js_strlen(code), JS::SourceOwnership::Borrowed));

  JS::RootedScript script(cx, JS::Compile(cx, options, srcBuf));
  CHECK(script);
  JS_SetReservedSlot(obj, 0, PrivateGCThingValue(script));

  TestTracer trc(cx);
  trc.expectedCell = script;
  trc.expectedKind = JS::TraceKind::Script;
  JS::TraceChildren(&trc, JS::GCCellPtr(obj, JS::TraceKind::Object));
  CHECK(trc.found);

  return true;
}
END_TEST(testPrivateGCThingValue)

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jspubtd.h"

#include "js/CompilationAndEvaluation.h"  // JS::Compile
#include "js/SourceText.h"                // JS::Source{Ownership,Text}
#include "jsapi-tests/tests.h"

JS::GCCellPtr GivesAndTakesCells(JS::GCCellPtr cell) { return cell; }

BEGIN_TEST(testGCCellPtr) {
  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  CHECK(obj);

  JS::RootedString str(cx, JS_NewStringCopyZ(cx, "probably foobar"));
  CHECK(str);

  const char* code = "function foo() { return 'bar'; }";

  JS::CompileOptions opts(cx);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  CHECK(srcBuf.init(cx, code, strlen(code), JS::SourceOwnership::Borrowed));

  JS::RootedScript script(cx, JS::Compile(cx, opts, srcBuf));
  CHECK(script);

  CHECK(!JS::GCCellPtr(nullptr));

  CHECK(JS::GCCellPtr(obj.get()));
  CHECK(JS::GCCellPtr(obj.get()).kind() == JS::TraceKind::Object);
  CHECK(JS::GCCellPtr(JS::ObjectValue(*obj)).kind() == JS::TraceKind::Object);

  CHECK(JS::GCCellPtr(str.get()));
  CHECK(JS::GCCellPtr(str.get()).kind() == JS::TraceKind::String);
  CHECK(JS::GCCellPtr(JS::StringValue(str)).kind() == JS::TraceKind::String);

  CHECK(JS::GCCellPtr(script.get()));
  CHECK(!JS::GCCellPtr(nullptr));
  CHECK(JS::GCCellPtr(script.get()).kind() == JS::TraceKind::Script);

  JS::GCCellPtr objcell(obj.get());
  JS::GCCellPtr scriptcell = JS::GCCellPtr(script.get());
  CHECK(GivesAndTakesCells(objcell));
  CHECK(GivesAndTakesCells(scriptcell));

  JS::GCCellPtr copy = objcell;
  CHECK(copy == objcell);

  return true;
}
END_TEST(testGCCellPtr)

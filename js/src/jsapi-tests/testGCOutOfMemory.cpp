/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Igor Bukanov
 */

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "js/CompilationAndEvaluation.h"  // JS::Evaluate
#include "js/SourceText.h"                // JS::Source{Ownership,Text}
#include "jsapi-tests/tests.h"

BEGIN_TEST(testGCOutOfMemory) {
  // Count the number of allocations until we hit OOM, and store it in 'max'.
  static const char source[] =
      "var max = 0; (function() {"
      "    var array = [];"
      "    for (; ; ++max)"
      "        array.push({});"
      "    array = []; array.push(0);"
      "})();";

  JS::CompileOptions opts(cx);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  CHECK(srcBuf.init(cx, source, strlen(source), JS::SourceOwnership::Borrowed));

  JS::RootedValue root(cx);
  bool ok = JS::Evaluate(cx, opts, srcBuf, &root);

  /* Check that we get OOM. */
  CHECK(!ok);
  CHECK(JS_GetPendingException(cx, &root));
  CHECK(root.isString());
  bool match = false;
  CHECK(JS_StringEqualsLiteral(cx, root.toString(), "out of memory", &match));
  CHECK(match);
  JS_ClearPendingException(cx);

  JS_GC(cx);

  // The above GC should have discarded everything. Verify that we can now
  // allocate half as many objects without OOMing.
  EVAL(
      "(function() {"
      "    var array = [];"
      "    for (var i = max >> 2; i != 0;) {"
      "        --i;"
      "        array.push({});"
      "    }"
      "})();",
      &root);
  CHECK(!JS_IsExceptionPending(cx));
  return true;
}

virtual JSContext* createContext() override {
  // Note that the max nursery size must be less than the whole heap size, or
  // the test will fail because 'max' (the number of allocations required for
  // OOM) will be based on the nursery size, and that will overflow the
  // tenured heap, which will cause the second pass with max/4 allocations to
  // OOM. (Actually, this only happens with nursery zeal, because normally
  // the nursery will start out with only a single chunk before triggering a
  // major GC.)
  JSContext* cx = JS_NewContext(1024 * 1024);
  if (!cx) {
    return nullptr;
  }
  JS_SetGCParameter(cx, JSGC_MAX_NURSERY_BYTES, js::gc::ChunkSize);
  setNativeStackQuota(cx);
  return cx;
}

virtual void destroyContext() override { JS_DestroyContext(cx); }

END_TEST(testGCOutOfMemory)

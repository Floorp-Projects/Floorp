/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */

#include "js/CompilationAndEvaluation.h"
#include "js/SourceText.h"
#include "jsapi-tests/tests.h"

using mozilla::ArrayLength;

BEGIN_TEST(testJSEvaluateScript) {
  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  CHECK(obj);

  static const char16_t src[] = u"var x = 5;";

  JS::RootedValue retval(cx);
  JS::CompileOptions opts(cx);
  JS::RootedObjectVector scopeChain(cx);
  CHECK(scopeChain.append(obj));

  JS::SourceText<char16_t> srcBuf;
  CHECK(srcBuf.init(cx, src, ArrayLength(src) - 1,
                    JS::SourceOwnership::Borrowed));

  CHECK(JS::Evaluate(cx, scopeChain, opts.setFileAndLine(__FILE__, __LINE__),
                     srcBuf, &retval));

  bool hasProp = true;
  CHECK(JS_AlreadyHasOwnProperty(cx, obj, "x", &hasProp));
  CHECK(hasProp);

  hasProp = false;
  CHECK(JS_HasProperty(cx, global, "x", &hasProp));
  CHECK(!hasProp);

  return true;
}
END_TEST(testJSEvaluateScript)

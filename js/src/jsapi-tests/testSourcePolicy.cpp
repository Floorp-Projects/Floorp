/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "js/CompilationAndEvaluation.h"  // JS::CompileFunction, JS::Evaluate
#include "js/MemoryFunctions.h"
#include "js/SourceText.h"  // JS::Source{Ownership,Text}
#include "jsapi-tests/tests.h"
#include "vm/JSScript.h"

BEGIN_TEST(testBug795104) {
  JS::RealmBehaviorsRef(cx->realm()).setDiscardSource(true);

  const size_t strLen = 60002;
  char* s = static_cast<char*>(JS_malloc(cx, strLen));
  CHECK(s);

  s[0] = '"';
  memset(s + 1, 'x', strLen - 2);
  s[strLen - 1] = '"';

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  CHECK(srcBuf.init(cx, s, strLen, JS::SourceOwnership::Borrowed));

  JS::CompileOptions opts(cx);

  // We don't want an rval for our JS::Evaluate call
  opts.setNoScriptRval(true);

  JS::RootedValue unused(cx);
  CHECK(JS::Evaluate(cx, opts, srcBuf, &unused));

  JS::RootedFunction fun(cx);
  JS::RootedObjectVector emptyScopeChain(cx);

  // But when compiling a function we don't want to use no-rval
  // mode, since it's not supported for functions.
  opts.setNoScriptRval(false);

  fun = JS::CompileFunction(cx, emptyScopeChain, opts, "f", 0, nullptr, srcBuf);
  CHECK(fun);

  JS_free(cx, s);

  return true;
}
END_TEST(testBug795104)

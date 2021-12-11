/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Test function with enclosing non-syntactic scope.
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "js/CallAndConstruct.h"
#include "js/CompilationAndEvaluation.h"  // JS::CompileFunction
#include "js/PropertyAndElement.h"        // JS_DefineProperty
#include "js/SourceText.h"                // JS::Source{Ownership,Text}
#include "jsapi-tests/tests.h"
#include "util/Text.h"
#include "vm/JSFunction.h"  // JSFunction
#include "vm/ScopeKind.h"   // ScopeKind

using namespace js;

BEGIN_TEST(testFunctionNonSyntactic) {
  JS::RootedObjectVector scopeChain(cx);

  {
    JS::RootedObject scopeObj(cx, JS_NewPlainObject(cx));
    CHECK(scopeObj);
    JS::RootedValue val(cx);
    val.setNumber(1);
    CHECK(JS_DefineProperty(cx, scopeObj, "foo", val, JSPROP_ENUMERATE));
    CHECK(scopeChain.append(scopeObj));
  }

  {
    JS::RootedObject scopeObj(cx, JS_NewPlainObject(cx));
    CHECK(scopeObj);
    JS::RootedValue val(cx);
    val.setNumber(20);
    CHECK(JS_DefineProperty(cx, scopeObj, "bar", val, JSPROP_ENUMERATE));
    CHECK(scopeChain.append(scopeObj));
  }

  {
    static const char src[] = "return foo + bar;";

    JS::SourceText<mozilla::Utf8Unit> srcBuf;
    CHECK(srcBuf.init(cx, src, js_strlen(src), JS::SourceOwnership::Borrowed));

    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, __LINE__);
    RootedFunction fun(cx, JS::CompileFunction(cx, scopeChain, options, "test",
                                               0, nullptr, srcBuf));
    CHECK(fun);

    CHECK(fun->enclosingScope()->kind() == ScopeKind::NonSyntactic);

    JS::RootedValue funVal(cx, JS::ObjectValue(*fun));
    JS::RootedValue rval(cx);
    CHECK(JS::Call(cx, JS::UndefinedHandleValue, funVal,
                   JS::HandleValueArray::empty(), &rval));
    CHECK(rval.isNumber());
    CHECK(rval.toNumber() == 21);
  }

  // With extra body bar.
  {
    const char* args[] = {
        "a = 300",
    };
    static const char src[] = "var x = 4000; return a + x + foo + bar;";

    JS::SourceText<mozilla::Utf8Unit> srcBuf;
    CHECK(srcBuf.init(cx, src, js_strlen(src), JS::SourceOwnership::Borrowed));

    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, __LINE__);
    RootedFunction fun(cx, JS::CompileFunction(cx, scopeChain, options, "test",
                                               1, args, srcBuf));
    CHECK(fun);

    CHECK(fun->enclosingScope()->kind() == ScopeKind::NonSyntactic);

    JS::RootedValue funVal(cx, JS::ObjectValue(*fun));
    JS::RootedValue rval(cx);
    CHECK(JS::Call(cx, JS::UndefinedHandleValue, funVal,
                   JS::HandleValueArray::empty(), &rval));
    CHECK(rval.isNumber());
    CHECK(rval.toNumber() == 4321);
  }

  return true;
}
END_TEST(testFunctionNonSyntactic)

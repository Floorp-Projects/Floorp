/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"
#include "vm/JSScript.h"

BEGIN_TEST(testBug795104)
{
    JS::CompileOptions opts(cx);
    JS::RealmBehaviorsRef(cx->compartment()).setDiscardSource(true);

    const size_t strLen = 60002;
    char* s = static_cast<char*>(JS_malloc(cx, strLen));
    CHECK(s);

    s[0] = '"';
    memset(s + 1, 'x', strLen - 2);
    s[strLen - 1] = '"';

    // We don't want an rval for our Evaluate call
    opts.setNoScriptRval(true);

    JS::RootedValue unused(cx);
    CHECK(JS::Evaluate(cx, opts, s, strLen, &unused));

    JS::RootedFunction fun(cx);
    JS::AutoObjectVector emptyScopeChain(cx);

    // But when compiling a function we don't want to use no-rval
    // mode, since it's not supported for functions.
    opts.setNoScriptRval(false);

    CHECK(JS::CompileFunction(cx, emptyScopeChain, opts, "f", 0, nullptr, s, strLen, &fun));
    CHECK(fun);

    JS_free(cx, s);

    return true;
}
END_TEST(testBug795104)

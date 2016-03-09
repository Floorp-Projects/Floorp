/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

using JS::CreateError;
using JS::Rooted;
using JS::ObjectValue;
using JS::Value;

static enum {
    NONE,
    SYMBOL_ITERATOR,
    SYMBOL_FOO,
    SYMBOL_EMPTY,
} uncaughtType = NONE;

BEGIN_TEST(testUncaughtSymbol)
{
    JSErrorReporter old = JS_SetErrorReporter(rt, UncaughtSymbolReporter);

    CHECK(uncaughtType == NONE);
    exec("throw Symbol.iterator;", __FILE__, __LINE__);
    CHECK(uncaughtType == SYMBOL_ITERATOR);

    uncaughtType = NONE;
    exec("throw Symbol('foo');", __FILE__, __LINE__);
    CHECK(uncaughtType == SYMBOL_FOO);

    uncaughtType = NONE;
    exec("throw Symbol();", __FILE__, __LINE__);
    CHECK(uncaughtType == SYMBOL_EMPTY);

    JS_SetErrorReporter(rt, old);

    return true;
}

static void
UncaughtSymbolReporter(JSContext* cx, const char* message, JSErrorReport* report)
{
    if (strcmp(message, "uncaught exception: Symbol(Symbol.iterator)") == 0)
        uncaughtType = SYMBOL_ITERATOR;
    else if (strcmp(message, "uncaught exception: Symbol(foo)") == 0)
        uncaughtType = SYMBOL_FOO;
    else if (strcmp(message, "uncaught exception: Symbol()") == 0)
        uncaughtType = SYMBOL_EMPTY;
}

END_TEST(testUncaughtSymbol)

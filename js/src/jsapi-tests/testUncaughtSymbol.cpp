/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

using JS::CreateError;
using JS::Rooted;
using JS::ObjectValue;
using JS::Value;

enum SymbolExceptionType {
    NONE,
    SYMBOL_ITERATOR,
    SYMBOL_FOO,
    SYMBOL_EMPTY,
};

BEGIN_TEST(testUncaughtSymbol)
{
    CHECK(!execDontReport("throw Symbol.iterator;", __FILE__, __LINE__));
    CHECK(GetSymbolExceptionType(cx) == SYMBOL_ITERATOR);

    CHECK(!execDontReport("throw Symbol('foo');", __FILE__, __LINE__));
    CHECK(GetSymbolExceptionType(cx) == SYMBOL_FOO);

    CHECK(!execDontReport("throw Symbol();", __FILE__, __LINE__));
    CHECK(GetSymbolExceptionType(cx) == SYMBOL_EMPTY);

    return true;
}

static SymbolExceptionType
GetSymbolExceptionType(JSContext* cx)
{
    JS::RootedValue exn(cx);
    MOZ_RELEASE_ASSERT(JS_GetPendingException(cx, &exn));
    MOZ_RELEASE_ASSERT(exn.isSymbol());
    JS_ClearPendingException(cx);

    js::ErrorReport report(cx);
    MOZ_RELEASE_ASSERT(report.init(cx, exn, js::ErrorReport::WithSideEffects));

    if (strcmp(report.message(), "uncaught exception: Symbol(Symbol.iterator)") == 0)
        return SYMBOL_ITERATOR;
    if (strcmp(report.message(), "uncaught exception: Symbol(foo)") == 0)
        return SYMBOL_FOO;
    if (strcmp(report.message(), "uncaught exception: Symbol()") == 0)
        return SYMBOL_EMPTY;
    MOZ_CRASH("Unexpected symbol");
}

END_TEST(testUncaughtSymbol)

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testSymbol_New)
{
    using namespace JS;

    RootedString desc(cx, nullptr);
    RootedSymbol sym1(cx);
    CHECK(sym1 = NewSymbol(cx, desc));
    CHECK_NULL(GetSymbolDescription(sym1));
    RootedValue v(cx, SymbolValue(sym1));
    CHECK_EQUAL(JS_TypeOfValue(cx, v), JSTYPE_SYMBOL);

    RootedSymbol sym2(cx);
    CHECK(sym2 = NewSymbol(cx, desc));
    CHECK(sym1 != sym2);

    CHECK(desc = JS_NewStringCopyZ(cx, "ponies"));
    CHECK(sym2 = NewSymbol(cx, desc));
    CHECK_SAME(StringValue(GetSymbolDescription(sym2)), StringValue(desc));

    return true;
}
END_TEST(testSymbol_New)

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

#ifdef JS_HAS_SYMBOLS
#define IF_JS_HAS_SYMBOLS(x) x
#else
#define IF_JS_HAS_SYMBOLS(x)
#endif

BEGIN_TEST(testForOfIterator_basicNonIterable)
{
    JS::RootedValue v(cx);
    // Hack to make it simple to produce an object that has a property
    // named Symbol.iterator.
    EVAL("var obj = {'@@iterator': 5"
         IF_JS_HAS_SYMBOLS(", [Symbol.iterator]: Array.prototype[Symbol.iterator]")
         "}; obj;", &v);
    JS::ForOfIterator iter(cx);
    bool ok = iter.init(v);
    CHECK(!ok);
    JS_ClearPendingException(cx);
    return true;
}
END_TEST(testForOfIterator_basicNonIterable)

BEGIN_TEST(testForOfIterator_bug515273_part1)
{
    JS::RootedValue v(cx);

    // Hack to make it simple to produce an object that has a property
    // named Symbol.iterator.
    EVAL("var obj = {'@@iterator': 5"
         IF_JS_HAS_SYMBOLS(", [Symbol.iterator]: Array.prototype[Symbol.iterator]")
         "}; obj;", &v);

    JS::ForOfIterator iter(cx);
    bool ok = iter.init(v, JS::ForOfIterator::AllowNonIterable);
    CHECK(!ok);
    JS_ClearPendingException(cx);
    return true;
}
END_TEST(testForOfIterator_bug515273_part1)

BEGIN_TEST(testForOfIterator_bug515273_part2)
{
    JS::RootedObject obj(cx,
			 JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
    CHECK(obj);
    JS::RootedValue v(cx, JS::ObjectValue(*obj));

    JS::ForOfIterator iter(cx);
    bool ok = iter.init(v, JS::ForOfIterator::AllowNonIterable);
    CHECK(ok);
    CHECK(!iter.valueIsIterable());
    return true;
}
END_TEST(testForOfIterator_bug515273_part2)

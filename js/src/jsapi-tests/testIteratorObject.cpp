/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testIteratorObject)
{
    using namespace js;
    JS::RootedValue result(cx);

    EVAL("new Map([['key1', 'value1'], ['key2', 'value2']]).entries()", &result);

    CHECK(result.isObject());
    JS::RootedObject obj1(cx, &result.toObject());
    ESClass class1 = ESClass::Other;
    CHECK(GetBuiltinClass(cx, obj1, &class1));
    CHECK(class1 == ESClass::MapIterator);

    EVAL("new Set(['value1', 'value2']).entries()", &result);

    CHECK(result.isObject());
    JS::RootedObject obj2(cx, &result.toObject());
    ESClass class2 = ESClass::Other;
    CHECK(GetBuiltinClass(cx, obj2, &class2));
    CHECK(class2 == ESClass::SetIterator);

    return true;
}
END_TEST(testIteratorObject)


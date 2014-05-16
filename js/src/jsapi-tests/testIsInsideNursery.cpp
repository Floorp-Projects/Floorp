/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testIsInsideNursery)
{
    /* Non-GC things are never inside the nursery. */
    CHECK(!js::gc::IsInsideNursery(rt, rt));
    CHECK(!js::gc::IsInsideNursery(rt, nullptr));
    CHECK(!js::gc::IsInsideNursery(nullptr));

    JS_GC(rt);

    JS::RootedObject object(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));

#ifdef JSGC_GENERATIONAL
    /* Objects are initially allocated in the nursery. */
    CHECK(js::gc::IsInsideNursery(rt, object));
    CHECK(js::gc::IsInsideNursery(object));
#else
    CHECK(!js::gc::IsInsideNursery(rt, object));
    CHECK(!js::gc::IsInsideNursery(object));
#endif

    JS_GC(rt);

    CHECK(!js::gc::IsInsideNursery(rt, object));
    CHECK(!js::gc::IsInsideNursery(object));

    return true;
}
END_TEST(testIsInsideNursery)

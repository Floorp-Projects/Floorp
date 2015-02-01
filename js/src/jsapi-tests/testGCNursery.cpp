/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Nursery.h"
#include "js/GCAPI.h"
#include "jsapi-tests/tests.h"

static int ranFinalizer = 0;

void
_finalize(js::FreeOp *fop, JSObject *obj)
{
    JS::AutoAssertGCCallback suppress(obj);
    ++ranFinalizer;
}

static const js::Class TenuredClass = {
    "TenuredClass",
    0,
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* ??? */
    _finalize,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    nullptr, /* trace */
    JS_NULL_CLASS_SPEC,
    JS_NULL_CLASS_EXT,
    JS_NULL_OBJECT_OPS
};

static const js::Class NurseryClass = {
    "NurseryClass",
    JSCLASS_FINALIZE_FROM_NURSERY | JSCLASS_HAS_RESERVED_SLOTS(1),
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* ??? */
    _finalize,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    nullptr, /* trace */
    JS_NULL_CLASS_SPEC,
    JS_NULL_CLASS_EXT,
    JS_NULL_OBJECT_OPS
};

BEGIN_TEST(testGCNurseryFinalizer)
{
#ifdef JS_GC_ZEAL
    // Running extra GCs during this test will make us get incorrect
    // finalization counts.
    AutoLeaveZeal nozeal(cx);
#endif /* JS_GC_ZEAL */

    JS::RootedObject obj(cx);

    obj = JS_NewObject(cx, Jsvalify(&TenuredClass));
    CHECK(!js::gc::IsInsideNursery(obj));

    // Null finalization list with empty nursery.
    rt->gc.minorGC(JS::gcreason::EVICT_NURSERY);
    CHECK(ranFinalizer == 0);

    // Null finalization list with non-empty nursery.
    obj = JS_NewPlainObject(cx);
    obj = JS_NewPlainObject(cx);
    obj = JS_NewPlainObject(cx);
    CHECK(js::gc::IsInsideNursery(obj));
    obj = nullptr;
    rt->gc.minorGC(JS::gcreason::EVICT_NURSERY);
    CHECK(ranFinalizer == 0);

    // Single finalizable nursery thing.
    obj = JS_NewObject(cx, Jsvalify(&NurseryClass));
    CHECK(js::gc::IsInsideNursery(obj));
    obj = nullptr;
    rt->gc.minorGC(JS::gcreason::EVICT_NURSERY);
    CHECK(ranFinalizer == 1);
    ranFinalizer = 0;

    // Multiple finalizable nursery things.
    obj = JS_NewObject(cx, Jsvalify(&NurseryClass));
    obj = JS_NewObject(cx, Jsvalify(&NurseryClass));
    obj = JS_NewObject(cx, Jsvalify(&NurseryClass));
    CHECK(js::gc::IsInsideNursery(obj));
    obj = nullptr;
    rt->gc.minorGC(JS::gcreason::EVICT_NURSERY);
    CHECK(ranFinalizer == 3);
    ranFinalizer = 0;

    // Interleaved finalizable things in nursery.
    obj = JS_NewPlainObject(cx);
    obj = JS_NewObject(cx, Jsvalify(&NurseryClass));
    obj = JS_NewPlainObject(cx);
    obj = JS_NewObject(cx, Jsvalify(&NurseryClass));
    obj = JS_NewPlainObject(cx);
    obj = JS_NewObject(cx, Jsvalify(&NurseryClass));
    obj = JS_NewPlainObject(cx);
    obj = JS_NewObject(cx, Jsvalify(&NurseryClass));
    obj = JS_NewPlainObject(cx);
    CHECK(js::gc::IsInsideNursery(obj));
    obj = nullptr;
    rt->gc.minorGC(JS::gcreason::EVICT_NURSERY);
    CHECK(ranFinalizer == 4);
    ranFinalizer = 0;

    return true;
}
END_TEST(testGCNurseryFinalizer)

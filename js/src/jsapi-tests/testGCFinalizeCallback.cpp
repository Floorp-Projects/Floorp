/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static const unsigned BufferSize = 20;
static unsigned FinalizeCalls = 0;
static JSFinalizeStatus StatusBuffer[BufferSize];
static bool IsCompartmentGCBuffer[BufferSize];

BEGIN_TEST(testGCFinalizeCallback)
{
    JS_SetGCParameter(rt, JSGC_MODE, JSGC_MODE_INCREMENTAL);
    JS_AddFinalizeCallback(rt, FinalizeCallback, nullptr);

    /* Full GC, non-incremental. */
    FinalizeCalls = 0;
    JS_GC(rt);
    CHECK(rt->gc.isFullGc());
    CHECK(checkSingleGroup());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsCompartmentGC(false));

    /* Full GC, incremental. */
    FinalizeCalls = 0;
    JS::PrepareForFullGC(rt);
    JS::IncrementalGC(rt, JS::gcreason::API, 1000000);
    CHECK(rt->gc.state() == js::gc::NO_INCREMENTAL);
    CHECK(rt->gc.isFullGc());
    CHECK(checkMultipleGroups());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsCompartmentGC(false));

    JS::RootedObject global1(cx, createGlobal());
    JS::RootedObject global2(cx, createGlobal());
    JS::RootedObject global3(cx, createGlobal());
    CHECK(global1);
    CHECK(global2);
    CHECK(global3);

    /* Compartment GC, non-incremental, single compartment. */
    FinalizeCalls = 0;
    JS::PrepareZoneForGC(global1->zone());
    JS::GCForReason(rt, JS::gcreason::API);
    CHECK(!rt->gc.isFullGc());
    CHECK(checkSingleGroup());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsCompartmentGC(true));

    /* Compartment GC, non-incremental, multiple compartments. */
    FinalizeCalls = 0;
    JS::PrepareZoneForGC(global1->zone());
    JS::PrepareZoneForGC(global2->zone());
    JS::PrepareZoneForGC(global3->zone());
    JS::GCForReason(rt, JS::gcreason::API);
    CHECK(!rt->gc.isFullGc());
    CHECK(checkSingleGroup());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsCompartmentGC(true));

    /* Compartment GC, incremental, single compartment. */
    FinalizeCalls = 0;
    JS::PrepareZoneForGC(global1->zone());
    JS::IncrementalGC(rt, JS::gcreason::API, 1000000);
    CHECK(rt->gc.state() == js::gc::NO_INCREMENTAL);
    CHECK(!rt->gc.isFullGc());
    CHECK(checkSingleGroup());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsCompartmentGC(true));

    /* Compartment GC, incremental, multiple compartments. */
    FinalizeCalls = 0;
    JS::PrepareZoneForGC(global1->zone());
    JS::PrepareZoneForGC(global2->zone());
    JS::PrepareZoneForGC(global3->zone());
    JS::IncrementalGC(rt, JS::gcreason::API, 1000000);
    CHECK(rt->gc.state() == js::gc::NO_INCREMENTAL);
    CHECK(!rt->gc.isFullGc());
    CHECK(checkMultipleGroups());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsCompartmentGC(true));

#ifdef JS_GC_ZEAL

    /* Full GC with reset due to new compartment, becoming compartment GC. */

    FinalizeCalls = 0;
    JS_SetGCZeal(cx, 9, 1000000);
    JS::PrepareForFullGC(rt);
    js::GCDebugSlice(rt, true, 1);
    CHECK(rt->gc.state() == js::gc::MARK);
    CHECK(rt->gc.isFullGc());

    JS::RootedObject global4(cx, createGlobal());
    js::GCDebugSlice(rt, true, 1);
    CHECK(rt->gc.state() == js::gc::NO_INCREMENTAL);
    CHECK(!rt->gc.isFullGc());
    CHECK(checkMultipleGroups());
    CHECK(checkFinalizeStatus());

    for (unsigned i = 0; i < FinalizeCalls - 1; ++i)
        CHECK(!IsCompartmentGCBuffer[i]);
    CHECK(IsCompartmentGCBuffer[FinalizeCalls - 1]);

    JS_SetGCZeal(cx, 0, 0);

#endif

    /*
     * Make some use of the globals here to ensure the compiler doesn't optimize
     * them away in release builds, causing the compartments to be collected and
     * the test to fail.
     */
    CHECK(JS_IsGlobalObject(global1));
    CHECK(JS_IsGlobalObject(global2));
    CHECK(JS_IsGlobalObject(global3));

    JS_RemoveFinalizeCallback(rt, FinalizeCallback);
    return true;
}

bool checkSingleGroup()
{
    CHECK(FinalizeCalls < BufferSize);
    CHECK(FinalizeCalls == 3);
    return true;
}

bool checkMultipleGroups()
{
    CHECK(FinalizeCalls < BufferSize);
    CHECK(FinalizeCalls % 2 == 1);
    CHECK((FinalizeCalls - 1) / 2 > 1);
    return true;
}

bool checkFinalizeStatus()
{
    /*
     * The finalize callback should be called twice for each compartment group
     * finalized, with status JSFINALIZE_GROUP_START and JSFINALIZE_GROUP_END,
     * and then once more with JSFINALIZE_COLLECTION_END.
     */

    for (unsigned i = 0; i < FinalizeCalls - 1; i += 2) {
        CHECK(StatusBuffer[i] == JSFINALIZE_GROUP_START);
        CHECK(StatusBuffer[i + 1] == JSFINALIZE_GROUP_END);
    }

    CHECK(StatusBuffer[FinalizeCalls - 1] == JSFINALIZE_COLLECTION_END);

    return true;
}

bool checkFinalizeIsCompartmentGC(bool isCompartmentGC)
{
    for (unsigned i = 0; i < FinalizeCalls; ++i)
        CHECK(IsCompartmentGCBuffer[i] == isCompartmentGC);

    return true;
}

static void
FinalizeCallback(JSFreeOp *fop, JSFinalizeStatus status, bool isCompartmentGC, void *data)
{
    if (FinalizeCalls < BufferSize) {
        StatusBuffer[FinalizeCalls] = status;
        IsCompartmentGCBuffer[FinalizeCalls] = isCompartmentGC;
    }
    ++FinalizeCalls;
}
END_TEST(testGCFinalizeCallback)

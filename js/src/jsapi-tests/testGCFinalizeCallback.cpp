/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static const unsigned BufferSize = 20;
static unsigned FinalizeCalls = 0;
static JSFinalizeStatus StatusBuffer[BufferSize];
static bool IsZoneGCBuffer[BufferSize];

BEGIN_TEST(testGCFinalizeCallback)
{
    JS_SetGCParameter(cx, JSGC_MODE, JSGC_MODE_INCREMENTAL);

    /* Full GC, non-incremental. */
    FinalizeCalls = 0;
    JS_GC(cx);
    CHECK(cx->gc.isFullGc());
    CHECK(checkSingleGroup());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsZoneGC(false));

    /* Full GC, incremental. */
    FinalizeCalls = 0;
    JS::PrepareForFullGC(cx);
    JS::StartIncrementalGC(cx, GC_NORMAL, JS::gcreason::API, 1000000);
    while (cx->gc.isIncrementalGCInProgress()) {
        JS::PrepareForFullGC(cx);
        JS::IncrementalGCSlice(cx, JS::gcreason::API, 1000000);
    }
    CHECK(!cx->gc.isIncrementalGCInProgress());
    CHECK(cx->gc.isFullGc());
    CHECK(checkMultipleGroups());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsZoneGC(false));

    JS::RootedObject global1(cx, createTestGlobal());
    JS::RootedObject global2(cx, createTestGlobal());
    JS::RootedObject global3(cx, createTestGlobal());
    CHECK(global1);
    CHECK(global2);
    CHECK(global3);

    /* Zone GC, non-incremental, single zone. */
    FinalizeCalls = 0;
    JS::PrepareZoneForGC(global1->zone());
    JS::GCForReason(cx, GC_NORMAL, JS::gcreason::API);
    CHECK(!cx->gc.isFullGc());
    CHECK(checkSingleGroup());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsZoneGC(true));

    /* Zone GC, non-incremental, multiple zones. */
    FinalizeCalls = 0;
    JS::PrepareZoneForGC(global1->zone());
    JS::PrepareZoneForGC(global2->zone());
    JS::PrepareZoneForGC(global3->zone());
    JS::GCForReason(cx, GC_NORMAL, JS::gcreason::API);
    CHECK(!cx->gc.isFullGc());
    CHECK(checkSingleGroup());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsZoneGC(true));

    /* Zone GC, incremental, single zone. */
    FinalizeCalls = 0;
    JS::PrepareZoneForGC(global1->zone());
    JS::StartIncrementalGC(cx, GC_NORMAL, JS::gcreason::API, 1000000);
    while (cx->gc.isIncrementalGCInProgress()) {
        JS::PrepareZoneForGC(global1->zone());
        JS::IncrementalGCSlice(cx, JS::gcreason::API, 1000000);
    }
    CHECK(!cx->gc.isIncrementalGCInProgress());
    CHECK(!cx->gc.isFullGc());
    CHECK(checkSingleGroup());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsZoneGC(true));

    /* Zone GC, incremental, multiple zones. */
    FinalizeCalls = 0;
    JS::PrepareZoneForGC(global1->zone());
    JS::PrepareZoneForGC(global2->zone());
    JS::PrepareZoneForGC(global3->zone());
    JS::StartIncrementalGC(cx, GC_NORMAL, JS::gcreason::API, 1000000);
    while (cx->gc.isIncrementalGCInProgress()) {
        JS::PrepareZoneForGC(global1->zone());
        JS::PrepareZoneForGC(global2->zone());
        JS::PrepareZoneForGC(global3->zone());
        JS::IncrementalGCSlice(cx, JS::gcreason::API, 1000000);
    }
    CHECK(!cx->gc.isIncrementalGCInProgress());
    CHECK(!cx->gc.isFullGc());
    CHECK(checkMultipleGroups());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsZoneGC(true));

#ifdef JS_GC_ZEAL

    /* Full GC with reset due to new zone, becoming zone GC. */

    FinalizeCalls = 0;
    JS_SetGCZeal(cx, 9, 1000000);
    JS::PrepareForFullGC(cx);
    js::SliceBudget budget(js::WorkBudget(1));
    cx->gc.startDebugGC(GC_NORMAL, budget);
    CHECK(cx->gc.state() == js::gc::State::Mark);
    CHECK(cx->gc.isFullGc());

    JS::RootedObject global4(cx, createTestGlobal());
    budget = js::SliceBudget(js::WorkBudget(1));
    cx->gc.debugGCSlice(budget);
    while (cx->gc.isIncrementalGCInProgress())
        cx->gc.debugGCSlice(budget);
    CHECK(!cx->gc.isIncrementalGCInProgress());
    CHECK(!cx->gc.isFullGc());
    CHECK(checkMultipleGroups());
    CHECK(checkFinalizeStatus());

    for (unsigned i = 0; i < FinalizeCalls - 1; ++i)
        CHECK(!IsZoneGCBuffer[i]);
    CHECK(IsZoneGCBuffer[FinalizeCalls - 1]);

    JS_SetGCZeal(cx, 0, 0);

#endif

    /*
     * Make some use of the globals here to ensure the compiler doesn't optimize
     * them away in release builds, causing the zones to be collected and
     * the test to fail.
     */
    CHECK(JS_IsGlobalObject(global1));
    CHECK(JS_IsGlobalObject(global2));
    CHECK(JS_IsGlobalObject(global3));

    return true;
}

JSObject* createTestGlobal()
{
    JS::CompartmentOptions options;
    return JS_NewGlobalObject(cx, getGlobalClass(), nullptr, JS::FireOnNewGlobalHook, options);
}

virtual bool init() override
{
    if (!JSAPITest::init())
        return false;

    JS_AddFinalizeCallback(cx, FinalizeCallback, nullptr);
    return true;
}

virtual void uninit() override
{
    JS_RemoveFinalizeCallback(cx, FinalizeCallback);
    JSAPITest::uninit();
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
     * The finalize callback should be called twice for each zone group
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

bool checkFinalizeIsZoneGC(bool isZoneGC)
{
    for (unsigned i = 0; i < FinalizeCalls; ++i)
        CHECK(IsZoneGCBuffer[i] == isZoneGC);

    return true;
}

static void
FinalizeCallback(JSFreeOp* fop, JSFinalizeStatus status, bool isZoneGC, void* data)
{
    if (FinalizeCalls < BufferSize) {
        StatusBuffer[FinalizeCalls] = status;
        IsZoneGCBuffer[FinalizeCalls] = isZoneGC;
    }
    ++FinalizeCalls;
}
END_TEST(testGCFinalizeCallback)

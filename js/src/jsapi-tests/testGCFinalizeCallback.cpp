/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tests.h"

#include <stdarg.h>

#include "jsfriendapi.h"
#include "jscntxt.h"

const unsigned BufferSize = 20;
static unsigned FinalizeCalls = 0;
static JSFinalizeStatus StatusBuffer[BufferSize];
static bool IsCompartmentGCBuffer[BufferSize];

static void
FinalizeCallback(JSFreeOp *fop, JSFinalizeStatus status, JSBool isCompartmentGC)
{
    if (FinalizeCalls < BufferSize) {
        StatusBuffer[FinalizeCalls] = status;
        IsCompartmentGCBuffer[FinalizeCalls] = isCompartmentGC;
    }
    ++FinalizeCalls;
}

BEGIN_TEST(testGCFinalizeCallback)
{
    JS_SetGCParameter(rt, JSGC_MODE, JSGC_MODE_INCREMENTAL);
    JS_SetFinalizeCallback(rt, FinalizeCallback);

    /* Full GC, non-incremental. */
    FinalizeCalls = 0;
    JS_GC(rt);
    CHECK(rt->gcIsFull);
    CHECK(checkSingleGroup());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsCompartmentGC(false));

    /* Full GC, incremental. */
    FinalizeCalls = 0;
    js::PrepareForFullGC(rt);
    js::IncrementalGC(rt, js::gcreason::API, 1000000);
    CHECK(rt->gcIncrementalState == js::gc::NO_INCREMENTAL);
    CHECK(rt->gcIsFull);
    CHECK(checkMultipleGroups());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsCompartmentGC(false));

    js::AutoObjectRooter global1(cx, createGlobal());
    js::AutoObjectRooter global2(cx, createGlobal());
    js::AutoObjectRooter global3(cx, createGlobal());
    CHECK(global1.object());
    CHECK(global2.object());
    CHECK(global3.object());

    /* Compartment GC, non-incremental, single compartment. */
    FinalizeCalls = 0;
    js::PrepareCompartmentForGC(global1.object()->compartment());
    js::GCForReason(rt, js::gcreason::API);
    CHECK(!rt->gcIsFull);
    CHECK(checkSingleGroup());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsCompartmentGC(true));

    /* Compartment GC, non-incremental, multiple compartments. */
    FinalizeCalls = 0;
    js::PrepareCompartmentForGC(global1.object()->compartment());
    js::PrepareCompartmentForGC(global2.object()->compartment());
    js::PrepareCompartmentForGC(global3.object()->compartment());
    js::GCForReason(rt, js::gcreason::API);
    CHECK(!rt->gcIsFull);
    CHECK(checkSingleGroup());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsCompartmentGC(true));

    /* Compartment GC, incremental, single compartment. */
    FinalizeCalls = 0;
    js::PrepareCompartmentForGC(global1.object()->compartment());
    js::IncrementalGC(rt, js::gcreason::API, 1000000);
    CHECK(rt->gcIncrementalState == js::gc::NO_INCREMENTAL);
    CHECK(!rt->gcIsFull);
    CHECK(checkSingleGroup());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsCompartmentGC(true));

    /* Compartment GC, incremental, multiple compartments. */
    FinalizeCalls = 0;
    js::PrepareCompartmentForGC(global1.object()->compartment());
    js::PrepareCompartmentForGC(global2.object()->compartment());
    js::PrepareCompartmentForGC(global3.object()->compartment());
    js::IncrementalGC(rt, js::gcreason::API, 1000000);
    CHECK(rt->gcIncrementalState == js::gc::NO_INCREMENTAL);
    CHECK(!rt->gcIsFull);
    CHECK(checkMultipleGroups());
    CHECK(checkFinalizeStatus());
    CHECK(checkFinalizeIsCompartmentGC(true));

#ifdef JS_GC_ZEAL

    /* Full GC with reset due to new compartment, becoming compartment GC. */

    FinalizeCalls = 0;
    JS_SetGCZeal(cx, 9, 1000000);
    js::PrepareForFullGC(rt);
    js::GCDebugSlice(rt, true, 1);
    CHECK(rt->gcIncrementalState == js::gc::MARK);
    CHECK(rt->gcIsFull);

    js::AutoObjectRooter global4(cx, createGlobal());
    js::GCDebugSlice(rt, true, 1);
    CHECK(rt->gcIncrementalState == js::gc::NO_INCREMENTAL);
    CHECK(!rt->gcIsFull);
    CHECK(checkMultipleGroups());
    CHECK(checkFinalizeStatus());

    for (unsigned i = 0; i < FinalizeCalls - 1; ++i)
        CHECK(!IsCompartmentGCBuffer[i]);
    CHECK(IsCompartmentGCBuffer[FinalizeCalls - 1]);

    JS_SetGCZeal(cx, 0, 0);

#endif

    JS_SetFinalizeCallback(rt, NULL);
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

END_TEST(testGCFinalizeCallback)

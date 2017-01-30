/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * TODO: Test weakmaps with gray values.  Currently these are fixed up by the
 * cycle collector.
 */

#include "gc/Heap.h"

#include "jsapi-tests/tests.h"

using namespace js;
using namespace js::gc;

class AutoNoAnalysisForTest
{
  public:
    AutoNoAnalysisForTest() {}
} JS_HAZ_GC_SUPPRESSED;

BEGIN_TEST(testGCGrayMarking)
{
    AutoNoAnalysisForTest disableAnalysis;

    CHECK(InitGlobals());
    JSAutoCompartment ac(cx, global1);

    JSObject* sameTarget = AllocTargetObject();
    CHECK(sameTarget);

    JSObject* sameSource = AllocSameCompartmentSourceObject(sameTarget);
    CHECK(sameSource);

    JSObject* crossTarget = AllocTargetObject();
    CHECK(crossTarget);

    JSObject* crossSource = AllocCrossCompartmentSourceObject(crossTarget);
    CHECK(crossSource);

    // Test GC with black roots marks objects black.

    JS::RootedObject blackRoot1(cx, sameSource);
    JS::RootedObject blackRoot2(cx, crossSource);

    JS_GC(cx);

    CHECK(IsMarkedBlack(sameSource));
    CHECK(IsMarkedBlack(crossSource));
    CHECK(IsMarkedBlack(sameTarget));
    CHECK(IsMarkedBlack(crossTarget));

    // Test GC with black and gray roots marks objects black.

    InitGrayRootTracer();
    grayRoots.grayRoot1 = sameSource;
    grayRoots.grayRoot2 = crossSource;

    JS_GC(cx);

    CHECK(IsMarkedBlack(sameSource));
    CHECK(IsMarkedBlack(crossSource));
    CHECK(IsMarkedBlack(sameTarget));
    CHECK(IsMarkedBlack(crossTarget));

    // Test GC with gray roots marks object gray.

    blackRoot1 = nullptr;
    blackRoot2 = nullptr;

    JS_GC(cx);

    CHECK(IsMarkedGray(sameSource));
    CHECK(IsMarkedGray(crossSource));
    CHECK(IsMarkedGray(sameTarget));
    CHECK(IsMarkedGray(crossTarget));

    // Test ExposeToActiveJS marks gray objects black.

    ExposeGCThingToActiveJS(JS::GCCellPtr(sameSource));
    ExposeGCThingToActiveJS(JS::GCCellPtr(crossSource));
    CHECK(IsMarkedBlack(sameSource));
    CHECK(IsMarkedBlack(crossSource));
    CHECK(IsMarkedBlack(sameTarget));
    CHECK(IsMarkedBlack(crossTarget));

    // Test a zone GC with black roots marks gray object in other zone black.

    JS_GC(cx);

    CHECK(IsMarkedGray(crossSource));
    CHECK(IsMarkedGray(crossTarget));

    blackRoot1 = crossSource;
    CHECK(ZoneGC(crossSource->zone()));

    CHECK(IsMarkedBlack(crossSource));
    CHECK(IsMarkedBlack(crossTarget));

    // Cleanup.

    global1 = nullptr;
    global2 = nullptr;
    RemoveGrayRootTracer();

    return true;
}

JS::PersistentRootedObject global1;
JS::PersistentRootedObject global2;

struct GrayRoots
{
    JSObject* grayRoot1;
    JSObject* grayRoot2;
};

GrayRoots grayRoots;

bool
InitGlobals()
{
    global1.init(cx, global);
    if (!createGlobal())
        return false;
    global2.init(cx, global);
    return global2 != nullptr;
}

void
InitGrayRootTracer()
{
    grayRoots.grayRoot1 = nullptr;
    grayRoots.grayRoot2 = nullptr;
    JS_SetGrayGCRootsTracer(cx, TraceGrayRoots, &grayRoots);
}

void
RemoveGrayRootTracer()
{
    grayRoots.grayRoot1 = nullptr;
    grayRoots.grayRoot2 = nullptr;
    JS_SetGrayGCRootsTracer(cx, nullptr, nullptr);
}

static void
TraceGrayRoots(JSTracer* trc, void* data)
{
    auto grayRoots = static_cast<GrayRoots*>(data);
    UnsafeTraceManuallyBarrieredEdge(trc, &grayRoots->grayRoot1, "gray root 1");
    UnsafeTraceManuallyBarrieredEdge(trc, &grayRoots->grayRoot2, "gray root 2");
}

JSObject*
AllocTargetObject()
{
    JS::RootedObject obj(cx, JS_NewPlainObject(cx));
    cx->gc.evictNursery();

    MOZ_ASSERT(obj->compartment() == global1->compartment());
    return obj;
}

JSObject*
AllocSameCompartmentSourceObject(JSObject* target)
{
    JS::RootedObject source(cx, JS_NewPlainObject(cx));
    if (!source)
        return nullptr;

    JS::RootedObject obj(cx, target);
    if (!JS_DefineProperty(cx, source, "ptr", obj, 0))
        return nullptr;

    cx->gc.evictNursery();

    MOZ_ASSERT(source->compartment() == global1->compartment());
    return source;
}

JSObject*
AllocCrossCompartmentSourceObject(JSObject* target)
{
    MOZ_ASSERT(target->compartment() == global1->compartment());
    JS::RootedObject obj(cx, target);
    JSAutoCompartment ac(cx, global2);
    if (!JS_WrapObject(cx, &obj))
        return nullptr;

    cx->gc.evictNursery();

    MOZ_ASSERT(obj->compartment() == global2->compartment());
    return obj;
}

bool
IsMarkedBlack(JSObject* obj)
{
    TenuredCell* cell = &obj->asTenured();
    return cell->isMarked(BLACK) && !cell->isMarked(GRAY);
}

bool
IsMarkedGray(JSObject* obj)
{
    TenuredCell* cell = &obj->asTenured();
    bool isGray = cell->isMarked(GRAY);
    MOZ_ASSERT_IF(isGray, cell->isMarked(BLACK));
    return isGray;
}

bool
ZoneGC(JS::Zone* zone)
{
    uint32_t oldMode = JS_GetGCParameter(cx, JSGC_MODE);
    JS_SetGCParameter(cx, JSGC_MODE, JSGC_MODE_ZONE);
    JS::PrepareZoneForGC(zone);
    cx->gc.gc(GC_NORMAL, JS::gcreason::API);
    CHECK(!cx->gc.isFullGc());
    JS_SetGCParameter(cx, JSGC_MODE, oldMode);
    return true;
}

END_TEST(testGCGrayMarking)

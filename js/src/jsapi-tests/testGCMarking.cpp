/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"

#include "js/RootingAPI.h"
#include "js/SliceBudget.h"
#include "jsapi-tests/tests.h"

class CCWTestTracer : public JS::CallbackTracer {
    void onChild(const JS::GCCellPtr& thing) override {
        numberOfThingsTraced++;

        printf("*thingp         = %p\n", thing.asCell());
        printf("*expectedThingp = %p\n", *expectedThingp);

        printf("kind         = %d\n", static_cast<int>(thing.kind()));
        printf("expectedKind = %d\n", static_cast<int>(expectedKind));

        if (thing.asCell() != *expectedThingp || thing.kind() != expectedKind)
            okay = false;
    }

  public:
    bool          okay;
    size_t        numberOfThingsTraced;
    void**        expectedThingp;
    JS::TraceKind expectedKind;

    CCWTestTracer(JSContext* cx, void** expectedThingp, JS::TraceKind expectedKind)
      : JS::CallbackTracer(cx),
        okay(true),
        numberOfThingsTraced(0),
        expectedThingp(expectedThingp),
        expectedKind(expectedKind)
    { }
};

BEGIN_TEST(testTracingIncomingCCWs)
{
    // Get two globals, in two different zones.

    JS::RootedObject global1(cx, JS::CurrentGlobalOrNull(cx));
    CHECK(global1);
    JS::CompartmentOptions options;
    JS::RootedObject global2(cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                                                    JS::FireOnNewGlobalHook, options));
    CHECK(global2);
    CHECK(global1->compartment() != global2->compartment());

    // Define an object in one compartment, that is wrapped by a CCW in another
    // compartment.

    JS::RootedObject obj(cx, JS_NewPlainObject(cx));
    CHECK(obj->compartment() == global1->compartment());

    JSAutoCompartment ac(cx, global2);
    JS::RootedObject wrapper(cx, obj);
    CHECK(JS_WrapObject(cx, &wrapper));
    JS::RootedValue v(cx, JS::ObjectValue(*wrapper));
    CHECK(JS_SetProperty(cx, global2, "ccw", v));

    // Ensure that |TraceIncomingCCWs| finds the object wrapped by the CCW.

    JS::CompartmentSet compartments;
    CHECK(compartments.init());
    CHECK(compartments.put(global1->compartment()));

    void* thing = obj.get();
    CCWTestTracer trc(cx, &thing, JS::TraceKind::Object);
    JS::TraceIncomingCCWs(&trc, compartments);
    CHECK(trc.numberOfThingsTraced == 1);
    CHECK(trc.okay);

    return true;
}
END_TEST(testTracingIncomingCCWs)

BEGIN_TEST(testIncrementalRoots)
{
    JSRuntime* rt = cx->runtime();

#ifdef JS_GC_ZEAL
    // Disable zeal modes because this test needs to control exactly when the GC happens.
    JS_SetGCZeal(cx, 0, 100);
#endif

    // Construct a big object graph to mark. In JS, the resulting object graph
    // is equivalent to:
    //
    //   leaf = {};
    //   leaf2 = {};
    //   root = { 'obj': { 'obj': ... { 'obj': leaf, 'leaf2': leaf2 } ... } }
    //
    // with leafOwner the object that has the 'obj' and 'leaf2' properties.

    JS::RootedObject obj(cx, JS_NewObject(cx, nullptr));
    if (!obj)
        return false;

    JS::RootedObject root(cx, obj);

    JS::RootedObject leaf(cx);
    JS::RootedObject leafOwner(cx);

    for (size_t i = 0; i < 3000; i++) {
        JS::RootedObject subobj(cx, JS_NewObject(cx, nullptr));
        if (!subobj)
            return false;
        if (!JS_DefineProperty(cx, obj, "obj", subobj, 0))
            return false;
        leafOwner = obj;
        obj = subobj;
        leaf = subobj;
    }

    // Give the leaf owner a second leaf.
    {
        JS::RootedObject leaf2(cx, JS_NewObject(cx, nullptr));
        if (!leaf2)
            return false;
        if (!JS_DefineProperty(cx, leafOwner, "leaf2", leaf2, 0))
            return false;
    }

    // This is marked during markRuntime
    JS::AutoObjectVector vec(cx);
    if (!vec.append(root))
        return false;

    // Tenure everything so intentionally unrooted objects don't move before we
    // can use them.
    cx->minorGC(JS::gcreason::API);

    // Release all roots except for the AutoObjectVector.
    obj = root = nullptr;

    // We need to manipulate interior nodes, but the JSAPI understandably wants
    // to make it difficult to do that without rooting things on the stack (by
    // requiring Handle parameters). We can do it anyway by using
    // fromMarkedLocation. The hazard analysis is OK with this because the
    // unrooted variables are not live after they've been pointed to via
    // fromMarkedLocation; you're essentially lying to the analysis, saying
    // that the unrooted variables are rooted.
    //
    // The analysis will report this lie in its listing of "unsafe references",
    // but we do not break the build based on those as there are too many false
    // positives.
    JSObject* unrootedLeaf = leaf;
    JS::Value unrootedLeafValue = JS::ObjectValue(*leaf);
    JSObject* unrootedLeafOwner = leafOwner;
    JS::HandleObject leafHandle = JS::HandleObject::fromMarkedLocation(&unrootedLeaf);
    JS::HandleValue leafValueHandle = JS::HandleValue::fromMarkedLocation(&unrootedLeafValue);
    JS::HandleObject leafOwnerHandle = JS::HandleObject::fromMarkedLocation(&unrootedLeafOwner);
    leaf = leafOwner = nullptr;

    // Do the root marking slice. This should mark 'root' and a bunch of its
    // descendants. It shouldn't make it all the way through (it gets a budget
    // of 1000, and the graph is about 3000 objects deep).
    js::SliceBudget budget(js::WorkBudget(1000));
    JS_SetGCParameter(cx, JSGC_MODE, JSGC_MODE_INCREMENTAL);
    rt->gc.startDebugGC(GC_NORMAL, budget);

    // We'd better be between iGC slices now. There's always a risk that
    // something will decide that we need to do a full GC (such as gczeal, but
    // that is turned off.)
    MOZ_ASSERT(JS::IsIncrementalGCInProgress(cx));

    // And assert that the mark bits are as we expect them to be.
    MOZ_ASSERT(vec[0]->asTenured().isMarked());
    MOZ_ASSERT(!leafHandle->asTenured().isMarked());
    MOZ_ASSERT(!leafOwnerHandle->asTenured().isMarked());

#ifdef DEBUG
    // Remember the current GC number so we can assert that no GC occurs
    // between operations.
    auto currentGCNumber = rt->gc.gcNumber();
#endif

    // Now do the incremental GC's worst nightmare: rip an unmarked object
    // 'leaf' out of the graph and stick it into an already-marked region (hang
    // it off the un-prebarriered root, in fact). The pre-barrier on the
    // overwrite of the source location should cause this object to be marked.
    if (!JS_SetProperty(cx, leafOwnerHandle, "obj", JS::UndefinedHandleValue))
        return false;
    MOZ_ASSERT(rt->gc.gcNumber() == currentGCNumber);
    if (!JS_SetProperty(cx, vec[0], "newobj", leafValueHandle))
        return false;
    MOZ_ASSERT(rt->gc.gcNumber() == currentGCNumber);
    MOZ_ASSERT(leafHandle->asTenured().isMarked());

    // Also take an unmarked object 'leaf2' from the graph and add an
    // additional edge from the root to it. This will not be marked by any
    // pre-barrier, but it is still in the live graph so it will eventually get
    // marked.
    //
    // Note that the root->leaf2 edge will *not* be marked through, since the
    // root is already marked, but that only matters if doing a compacting GC
    // and the compacting GC repeats the whole marking phase to update
    // pointers.
    {
        JS::RootedValue leaf2(cx);
        if (!JS_GetProperty(cx, leafOwnerHandle, "leaf2", &leaf2))
            return false;
        MOZ_ASSERT(rt->gc.gcNumber() == currentGCNumber);
        MOZ_ASSERT(!leaf2.toObject().asTenured().isMarked());
        if (!JS_SetProperty(cx, vec[0], "leafcopy", leaf2))
            return false;
        MOZ_ASSERT(rt->gc.gcNumber() == currentGCNumber);
        MOZ_ASSERT(!leaf2.toObject().asTenured().isMarked());
    }

    // Finish the GC using an unlimited budget.
    auto unlimited = js::SliceBudget::unlimited();
    rt->gc.debugGCSlice(unlimited);

    // Access the leaf object to try to trigger a crash if it is dead.
    if (!JS_SetProperty(cx, leafHandle, "toes", JS::UndefinedHandleValue))
        return false;

    return true;
}
END_TEST(testIncrementalRoots)

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
#include "vm/Realm.h"

static bool
ConstructCCW(JSContext* cx, const JSClass* globalClasp,
             JS::HandleObject global1, JS::MutableHandleObject wrapper,
             JS::MutableHandleObject global2, JS::MutableHandleObject wrappee)
{
    if (!global1) {
        fprintf(stderr, "null initial global");
        return false;
    }

    // Define a second global in a different zone.
    JS::RealmOptions options;
    global2.set(JS_NewGlobalObject(cx, globalClasp, nullptr,
                                   JS::FireOnNewGlobalHook, options));
    if (!global2) {
        fprintf(stderr, "failed to create second global");
        return false;
    }

    // This should always be false, regardless.
    if (global1->compartment() == global2->compartment()) {
        fprintf(stderr, "second global claims to be in global1's compartment");
        return false;
    }

    // This checks that the API obeys the implicit zone request.
    if (global1->zone() == global2->zone()) {
        fprintf(stderr, "global2 is in global1's zone");
        return false;
    }

    // Define an object in compartment 2, that is wrapped by a CCW into compartment 1.
    {
        JSAutoRealm ar(cx, global2);
        wrappee.set(JS_NewPlainObject(cx));
        if (wrappee->compartment() != global2->compartment()) {
            fprintf(stderr, "wrappee in wrong compartment");
            return false;
        }
    }

    wrapper.set(wrappee);
    if (!JS_WrapObject(cx, wrapper)) {
        fprintf(stderr, "failed to wrap");
        return false;
    }
    if (wrappee == wrapper) {
        fprintf(stderr, "expected wrapping");
        return false;
    }
    if (wrapper->compartment() != global1->compartment()) {
        fprintf(stderr, "wrapper in wrong compartment");
        return false;
    }

    return true;
}

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
#ifdef JS_GC_ZEAL
    // Disable zeal modes because this test needs to control exactly when the GC happens.
    JS_SetGCZeal(cx, 0, 100);
#endif
    JS_GC(cx);

    JS::RootedObject global1(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject wrapper(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject global2(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject wrappee(cx, JS::CurrentGlobalOrNull(cx));
    CHECK(ConstructCCW(cx, getGlobalClass(), global1, &wrapper, &global2, &wrappee));
    JS_GC(cx);
    CHECK(!js::gc::IsInsideNursery(wrappee));
    CHECK(!js::gc::IsInsideNursery(wrapper));

    JS::RootedValue v(cx, JS::ObjectValue(*wrapper));
    CHECK(JS_SetProperty(cx, global1, "ccw", v));

    // Ensure that |TraceIncomingCCWs| finds the object wrapped by the CCW.

    JS::CompartmentSet compartments;
    CHECK(compartments.init());
    CHECK(compartments.put(global2->compartment()));

    void* thing = wrappee.get();
    CCWTestTracer trc(cx, &thing, JS::TraceKind::Object);
    JS::TraceIncomingCCWs(&trc, compartments);
    CHECK(trc.numberOfThingsTraced == 1);
    CHECK(trc.okay);

    return true;
}
END_TEST(testTracingIncomingCCWs)

static size_t
countWrappers(JS::Compartment* comp)
{
    size_t count = 0;
    for (JS::Compartment::WrapperEnum e(comp); !e.empty(); e.popFront())
        ++count;
    return count;
}

BEGIN_TEST(testDeadNurseryCCW)
{
#ifdef JS_GC_ZEAL
    // Disable zeal modes because this test needs to control exactly when the GC happens.
    JS_SetGCZeal(cx, 0, 100);
#endif
    JS_GC(cx);

    JS::RootedObject global1(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject wrapper(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject global2(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject wrappee(cx, JS::CurrentGlobalOrNull(cx));
    CHECK(ConstructCCW(cx, getGlobalClass(), global1, &wrapper, &global2, &wrappee));
    CHECK(js::gc::IsInsideNursery(wrappee));
    CHECK(js::gc::IsInsideNursery(wrapper));

    // Now let the obj and wrapper die.
    wrappee = wrapper = nullptr;

    // Now a GC should clear the CCW.
    CHECK(countWrappers(global1->compartment()) == 1);
    cx->runtime()->gc.evictNursery();
    CHECK(countWrappers(global1->compartment()) == 0);

    // Check for corruption of the CCW table by doing a full GC to force sweeping.
    JS_GC(cx);

    return true;
}
END_TEST(testDeadNurseryCCW)

BEGIN_TEST(testLiveNurseryCCW)
{
#ifdef JS_GC_ZEAL
    // Disable zeal modes because this test needs to control exactly when the GC happens.
    JS_SetGCZeal(cx, 0, 100);
#endif
    JS_GC(cx);

    JS::RootedObject global1(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject wrapper(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject global2(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject wrappee(cx, JS::CurrentGlobalOrNull(cx));
    CHECK(ConstructCCW(cx, getGlobalClass(), global1, &wrapper, &global2, &wrappee));
    CHECK(js::gc::IsInsideNursery(wrappee));
    CHECK(js::gc::IsInsideNursery(wrapper));

    // Now a GC should not kill the CCW.
    CHECK(countWrappers(global1->compartment()) == 1);
    cx->runtime()->gc.evictNursery();
    CHECK(countWrappers(global1->compartment()) == 1);

    CHECK(!js::gc::IsInsideNursery(wrappee));
    CHECK(!js::gc::IsInsideNursery(wrapper));

    // Check for corruption of the CCW table by doing a full GC to force sweeping.
    JS_GC(cx);

    return true;
}
END_TEST(testLiveNurseryCCW)

BEGIN_TEST(testLiveNurseryWrapperCCW)
{
#ifdef JS_GC_ZEAL
    // Disable zeal modes because this test needs to control exactly when the GC happens.
    JS_SetGCZeal(cx, 0, 100);
#endif
    JS_GC(cx);

    JS::RootedObject global1(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject wrapper(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject global2(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject wrappee(cx, JS::CurrentGlobalOrNull(cx));
    CHECK(ConstructCCW(cx, getGlobalClass(), global1, &wrapper, &global2, &wrappee));
    CHECK(js::gc::IsInsideNursery(wrappee));
    CHECK(js::gc::IsInsideNursery(wrapper));

    // The wrapper contains a strong reference to the wrappee, so just dropping
    // the reference to the wrappee will not drop the CCW table entry as long
    // as the wrapper is held strongly. Thus, the minor collection here must
    // tenure both the wrapper and the wrappee and keep both in the table.
    wrappee = nullptr;

    // Now a GC should not kill the CCW.
    CHECK(countWrappers(global1->compartment()) == 1);
    cx->runtime()->gc.evictNursery();
    CHECK(countWrappers(global1->compartment()) == 1);

    CHECK(!js::gc::IsInsideNursery(wrapper));

    // Check for corruption of the CCW table by doing a full GC to force sweeping.
    JS_GC(cx);

    return true;
}
END_TEST(testLiveNurseryWrapperCCW)

BEGIN_TEST(testLiveNurseryWrappeeCCW)
{
#ifdef JS_GC_ZEAL
    // Disable zeal modes because this test needs to control exactly when the GC happens.
    JS_SetGCZeal(cx, 0, 100);
#endif
    JS_GC(cx);

    JS::RootedObject global1(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject wrapper(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject global2(cx, JS::CurrentGlobalOrNull(cx));
    JS::RootedObject wrappee(cx, JS::CurrentGlobalOrNull(cx));
    CHECK(ConstructCCW(cx, getGlobalClass(), global1, &wrapper, &global2, &wrappee));
    CHECK(js::gc::IsInsideNursery(wrappee));
    CHECK(js::gc::IsInsideNursery(wrapper));

    // Let the wrapper die. The wrapper should drop from the table when we GC,
    // even though there are other non-cross-compartment edges to it.
    wrapper = nullptr;

    // Now a GC should not kill the CCW.
    CHECK(countWrappers(global1->compartment()) == 1);
    cx->runtime()->gc.evictNursery();
    CHECK(countWrappers(global1->compartment()) == 0);

    CHECK(!js::gc::IsInsideNursery(wrappee));

    // Check for corruption of the CCW table by doing a full GC to force sweeping.
    JS_GC(cx);

    return true;
}
END_TEST(testLiveNurseryWrappeeCCW)

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
    cx->runtime()->gc.minorGC(JS::gcreason::API);

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
    MOZ_ASSERT(vec[0]->asTenured().isMarkedBlack());
    MOZ_ASSERT(!leafHandle->asTenured().isMarkedBlack());
    MOZ_ASSERT(!leafOwnerHandle->asTenured().isMarkedBlack());

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
    MOZ_ASSERT(leafHandle->asTenured().isMarkedBlack());

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
        MOZ_ASSERT(!leaf2.toObject().asTenured().isMarkedBlack());
        if (!JS_SetProperty(cx, vec[0], "leafcopy", leaf2))
            return false;
        MOZ_ASSERT(rt->gc.gcNumber() == currentGCNumber);
        MOZ_ASSERT(!leaf2.toObject().asTenured().isMarkedBlack());
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

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Class.h"
#include "jsapi-tests/tests.h"

using namespace JS;

struct BarkWhenTracedClass {
    static int finalizeCount;
    static int traceCount;

    static const JSClass class_;
    static void finalize(JSFreeOp* fop, JSObject* obj) { finalizeCount++; }
    static void trace(JSTracer* trc, JSObject* obj) { traceCount++; }
    static void reset() { finalizeCount = 0; traceCount = 0; }
};

int BarkWhenTracedClass::finalizeCount;
int BarkWhenTracedClass::traceCount;

static const JSClassOps BarkWhenTracedClassClassOps = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    BarkWhenTracedClass::finalize,
    nullptr,
    nullptr,
    nullptr,
    BarkWhenTracedClass::trace
};

const JSClass BarkWhenTracedClass::class_ = {
    "BarkWhenTracedClass",
    JSCLASS_FOREGROUND_FINALIZE,
    &BarkWhenTracedClassClassOps
};

struct Kennel {
    PersistentRootedObject obj;
    Kennel() { }
    explicit Kennel(JSContext* cx) : obj(cx) { }
    Kennel(JSContext* cx, const HandleObject& woof) : obj(cx, woof) { }
    void init(JSContext* cx, const HandleObject& woof) {
        obj.init(cx, woof);
    }
    void clear() {
        obj = nullptr;
    }
};

// A function for allocating a Kennel and a barker. Only allocating
// PersistentRooteds on the heap, and in this function, helps ensure that the
// conservative GC doesn't find stray references to the barker. Ugh.
MOZ_NEVER_INLINE static Kennel*
Allocate(JSContext* cx)
{
    RootedObject barker(cx, JS_NewObject(cx, &BarkWhenTracedClass::class_));
    if (!barker)
        return nullptr;

    return new Kennel(cx, barker);
}

// Do a GC, expecting |n| barkers to be finalized.
static bool
GCFinalizesNBarkers(JSContext* cx, int n)
{
    int preGCTrace = BarkWhenTracedClass::traceCount;
    int preGCFinalize = BarkWhenTracedClass::finalizeCount;

    JS_GC(cx);

    return (BarkWhenTracedClass::finalizeCount == preGCFinalize + n &&
            BarkWhenTracedClass::traceCount > preGCTrace);
}

// PersistentRooted instances protect their contents from being recycled.
BEGIN_TEST(test_PersistentRooted)
{
    BarkWhenTracedClass::reset();

    mozilla::UniquePtr<Kennel> kennel(Allocate(cx));
    CHECK(kennel.get());

    // GC should be able to find our barker.
    CHECK(GCFinalizesNBarkers(cx, 0));

    kennel = nullptr;

    // Now GC should not be able to find the barker.
    JS_GC(cx);
    CHECK(BarkWhenTracedClass::finalizeCount == 1);

    return true;
}
END_TEST(test_PersistentRooted)

// GC should not be upset by null PersistentRooteds.
BEGIN_TEST(test_PersistentRootedNull)
{
    BarkWhenTracedClass::reset();

    Kennel kennel(cx);
    CHECK(!kennel.obj);

    JS_GC(cx);
    CHECK(BarkWhenTracedClass::finalizeCount == 0);

    return true;
}
END_TEST(test_PersistentRootedNull)

// Copy construction works.
BEGIN_TEST(test_PersistentRootedCopy)
{
    BarkWhenTracedClass::reset();

    mozilla::UniquePtr<Kennel> kennel(Allocate(cx));
    CHECK(kennel.get());

    CHECK(GCFinalizesNBarkers(cx, 0));

    // Copy construction! AMAZING!
    mozilla::UniquePtr<Kennel> newKennel(new Kennel(*kennel));

    CHECK(GCFinalizesNBarkers(cx, 0));

    kennel = nullptr;

    CHECK(GCFinalizesNBarkers(cx, 0));

    newKennel = nullptr;

    // Now that kennel and nowKennel are both deallocated, GC should not be
    // able to find the barker.
    JS_GC(cx);
    CHECK(BarkWhenTracedClass::finalizeCount == 1);

    return true;
}
END_TEST(test_PersistentRootedCopy)

// Assignment works.
BEGIN_TEST(test_PersistentRootedAssign)
{
    BarkWhenTracedClass::reset();

    mozilla::UniquePtr<Kennel> kennel(Allocate(cx));
    CHECK(kennel.get());

    CHECK(GCFinalizesNBarkers(cx, 0));

    // Allocate a new, empty kennel.
    mozilla::UniquePtr<Kennel> kennel2(new Kennel(cx));

    // Assignment! ASTONISHING!
    *kennel2 = *kennel;

    // With both kennels referring to the same barker, it is held alive.
    CHECK(GCFinalizesNBarkers(cx, 0));

    kennel2 = nullptr;

    // The destination of the assignment alone holds the barker alive.
    CHECK(GCFinalizesNBarkers(cx, 0));

    // Allocate a second barker.
    kennel2 = mozilla::UniquePtr<Kennel>(Allocate(cx));
    CHECK(kennel2.get());

    *kennel = *kennel2;

    // Nothing refers to the first kennel any more.
    CHECK(GCFinalizesNBarkers(cx, 1));

    kennel = nullptr;
    kennel2 = nullptr;

    // Now that kennel and kennel2 are both deallocated, GC should not be
    // able to find the barker.
    JS_GC(cx);
    CHECK(BarkWhenTracedClass::finalizeCount == 2);

    return true;
}
END_TEST(test_PersistentRootedAssign)

static PersistentRootedObject gGlobalRoot;

// PersistentRooted instances can initialized in a separate step to allow for global PersistentRooteds.
BEGIN_TEST(test_GlobalPersistentRooted)
{
    BarkWhenTracedClass::reset();

    CHECK(!gGlobalRoot.initialized());

    {
        RootedObject barker(cx, JS_NewObject(cx, &BarkWhenTracedClass::class_));
        CHECK(barker);

        gGlobalRoot.init(cx, barker);
    }

    CHECK(gGlobalRoot.initialized());

    // GC should be able to find our barker.
    CHECK(GCFinalizesNBarkers(cx, 0));

    gGlobalRoot.reset();
    CHECK(!gGlobalRoot.initialized());

    // Now GC should not be able to find the barker.
    JS_GC(cx);
    CHECK(BarkWhenTracedClass::finalizeCount == 1);

    return true;
}
END_TEST(test_GlobalPersistentRooted)

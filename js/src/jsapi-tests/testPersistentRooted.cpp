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
    static void finalize(JSFreeOp *fop, JSObject *obj) { finalizeCount++; }
    static void trace(JSTracer *trc, JSObject *obj) { traceCount++; }
    static void reset() { finalizeCount = 0; traceCount = 0; }
};

int BarkWhenTracedClass::finalizeCount;
int BarkWhenTracedClass::traceCount;

const JSClass BarkWhenTracedClass::class_ = {
  "BarkWhenTracedClass", 0,
  JS_PropertyStub,
  JS_DeletePropertyStub,
  JS_PropertyStub,
  JS_StrictPropertyStub,
  JS_EnumerateStub,
  JS_ResolveStub,
  JS_ConvertStub,
  finalize,
  nullptr,
  nullptr,
  nullptr,
  trace
};

struct Kennel {
    PersistentRootedObject obj;
    explicit Kennel(JSContext *cx) : obj(cx) { }
    Kennel(JSContext *cx, const HandleObject &woof) : obj(cx, woof) { }
};

// A function for allocating a Kennel and a barker. Only allocating
// PersistentRooteds on the heap, and in this function, helps ensure that the
// conservative GC doesn't find stray references to the barker. Ugh.
MOZ_NEVER_INLINE static Kennel *
Allocate(JSContext *cx)
{
    RootedObject barker(cx, JS_NewObject(cx, &BarkWhenTracedClass::class_, JS::NullPtr(), JS::NullPtr()));
    if (!barker)
        return nullptr;

    return new Kennel(cx, barker);
}

// Do a GC, expecting |n| barkers to be finalized.
static bool
GCFinalizesNBarkers(JSContext *cx, int n)
{
    int preGCTrace = BarkWhenTracedClass::traceCount;
    int preGCFinalize = BarkWhenTracedClass::finalizeCount;

    JS_GC(JS_GetRuntime(cx));

    return (BarkWhenTracedClass::finalizeCount == preGCFinalize + n &&
            BarkWhenTracedClass::traceCount > preGCTrace);
}

// PersistentRooted instances protect their contents from being recycled.
BEGIN_TEST(test_PersistentRooted)
{
    BarkWhenTracedClass::reset();

    Kennel *kennel = Allocate(cx);
    CHECK(kennel);

    // GC should be able to find our barker.
    CHECK(GCFinalizesNBarkers(cx, 0));

    delete(kennel);

    // Now GC should not be able to find the barker.
    JS_GC(JS_GetRuntime(cx));
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

    JS_GC(JS_GetRuntime(cx));
    CHECK(BarkWhenTracedClass::finalizeCount == 0);

    return true;
}
END_TEST(test_PersistentRootedNull)

// Copy construction works.
BEGIN_TEST(test_PersistentRootedCopy)
{
    BarkWhenTracedClass::reset();

    Kennel *kennel = Allocate(cx);
    CHECK(kennel);

    CHECK(GCFinalizesNBarkers(cx, 0));

    // Copy construction! AMAZING!
    Kennel *newKennel = new Kennel(*kennel);

    CHECK(GCFinalizesNBarkers(cx, 0));

    delete(kennel);

    CHECK(GCFinalizesNBarkers(cx, 0));

    delete(newKennel);

    // Now that kennel and nowKennel are both deallocated, GC should not be
    // able to find the barker.
    JS_GC(JS_GetRuntime(cx));
    CHECK(BarkWhenTracedClass::finalizeCount == 1);

    return true;
}
END_TEST(test_PersistentRootedCopy)

// Assignment works.
BEGIN_TEST(test_PersistentRootedAssign)
{
    BarkWhenTracedClass::reset();

    Kennel *kennel = Allocate(cx);
    CHECK(kennel);

    CHECK(GCFinalizesNBarkers(cx, 0));

    // Allocate a new, empty kennel.
    Kennel *kennel2 = new Kennel(cx);

    // Assignment! ASTONISHING!
    *kennel2 = *kennel;

    // With both kennels referring to the same barker, it is held alive.
    CHECK(GCFinalizesNBarkers(cx, 0));

    delete(kennel2);

    // The destination of the assignment alone holds the barker alive.
    CHECK(GCFinalizesNBarkers(cx, 0));

    // Allocate a second barker.
    kennel2 = Allocate(cx);
    CHECK(kennel);

    *kennel = *kennel2;

    // Nothing refers to the first kennel any more.
    CHECK(GCFinalizesNBarkers(cx, 1));

    delete(kennel);
    delete(kennel2);

    // Now that kennel and kennel2 are both deallocated, GC should not be
    // able to find the barker.
    JS_GC(JS_GetRuntime(cx));
    CHECK(BarkWhenTracedClass::finalizeCount == 2);

    return true;
}
END_TEST(test_PersistentRootedAssign)

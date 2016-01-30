/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Barrier.h"
#include "js/RootingAPI.h"

#include "jsapi-tests/tests.h"

struct MyHeap
{
    explicit MyHeap(JSObject* obj) : weak(obj) {}
    js::WeakRef<JSObject*> weak;

    void trace(JSTracer* trc) {
        js::TraceWeakEdge(trc, &weak, "weak");
    }
};

BEGIN_TEST(testGCWeakRef)
{
    // Create an object and add a property to it so that we can read the
    // property back later to verify that object internals are not garbage.
    JS::RootedObject obj(cx, JS_NewPlainObject(cx));
    CHECK(obj);
    CHECK(JS_DefineProperty(cx, obj, "x", 42, 0));

    // Store the object behind a weak pointer and remove other references.
    JS::Rooted<MyHeap> heap(cx, MyHeap(obj));
    obj = nullptr;

    rt->gc.minorGC(JS::gcreason::API);

    // The minor collection should have treated the weak ref as a strong ref,
    // so the object should still be live, despite not having any other live
    // references.
    CHECK(heap.get().weak.unbarrieredGet() != nullptr);
    obj = heap.get().weak;
    JS::RootedValue v(cx);
    CHECK(JS_GetProperty(cx, obj, "x", &v));
    CHECK(v.isInt32());
    CHECK(v.toInt32() == 42);

    // A full collection with a second ref should keep the object as well.
    CHECK(obj == heap.get().weak);
    JS_GC(rt);
    CHECK(obj == heap.get().weak);
    v = JS::UndefinedValue();
    CHECK(JS_GetProperty(cx, obj, "x", &v));
    CHECK(v.isInt32());
    CHECK(v.toInt32() == 42);

    // A full collection after nulling the root should collect the object, or
    // at least null out the weak reference before returning to the mutator.
    obj = nullptr;
    JS_GC(rt);
    CHECK(heap.get().weak == nullptr);

    return true;
}
END_TEST(testGCWeakRef)


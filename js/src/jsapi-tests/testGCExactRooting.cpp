/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ds/TraceableFifo.h"
#include "gc/Policy.h"
#include "js/GCHashTable.h"
#include "js/GCVector.h"
#include "js/RootingAPI.h"

#include "jsapi-tests/tests.h"

using namespace js;

BEGIN_TEST(testGCExactRooting)
{
    JS::RootedObject rootCx(cx, JS_NewPlainObject(cx));
    JS::RootedObject rootRootingCx(JS::RootingContext::get(cx), JS_NewPlainObject(cx));

    JS_GC(cx);

    /* Use the objects we just created to ensure that they are still alive. */
    JS_DefineProperty(cx, rootCx, "foo", JS::UndefinedHandleValue, 0);
    JS_DefineProperty(cx, rootRootingCx, "foo", JS::UndefinedHandleValue, 0);

    return true;
}
END_TEST(testGCExactRooting)

BEGIN_TEST(testGCSuppressions)
{
    JS::AutoAssertNoGC nogc;
    JS::AutoCheckCannotGC checkgc;
    JS::AutoSuppressGCAnalysis noanalysis;

    JS::AutoAssertNoGC nogcCx(cx);
    JS::AutoCheckCannotGC checkgcCx(cx);
    JS::AutoSuppressGCAnalysis noanalysisCx(cx);

    return true;
}
END_TEST(testGCSuppressions)

struct MyContainer
{
    HeapPtr<JSObject*> obj;
    HeapPtr<JSString*> str;

    MyContainer() : obj(nullptr), str(nullptr) {}
    void trace(JSTracer* trc) {
        js::TraceNullableEdge(trc, &obj, "test container");
        js::TraceNullableEdge(trc, &str, "test container");
    }
};

namespace js {
template <typename Wrapper>
struct MutableWrappedPtrOperations<MyContainer, Wrapper> {
    HeapPtr<JSObject*>& obj() { return static_cast<Wrapper*>(this)->get().obj; }
    HeapPtr<JSString*>& str() { return static_cast<Wrapper*>(this)->get().str; }
};
} // namespace js

BEGIN_TEST(testGCRootedStaticStructInternalStackStorageAugmented)
{
    JS::Rooted<MyContainer> container(cx);
    container.obj() = JS_NewObject(cx, nullptr);
    container.str() = JS_NewStringCopyZ(cx, "Hello");

    JS_GC(cx);
    JS_GC(cx);

    JS::RootedObject obj(cx, container.obj());
    JS::RootedValue val(cx, StringValue(container.str()));
    CHECK(JS_SetProperty(cx, obj, "foo", val));
    obj = nullptr;
    val = UndefinedValue();

    {
        JS::RootedString actual(cx);
        bool same;

        // Automatic move from stack to heap.
        JS::PersistentRooted<MyContainer> heap(cx, container);

        // clear prior rooting.
        container.obj() = nullptr;
        container.str() = nullptr;

        obj = heap.obj();
        CHECK(JS_GetProperty(cx, obj, "foo", &val));
        actual = val.toString();
        CHECK(JS_StringEqualsAscii(cx, actual, "Hello", &same));
        CHECK(same);
        obj = nullptr;
        actual = nullptr;

        JS_GC(cx);
        JS_GC(cx);

        obj = heap.obj();
        CHECK(JS_GetProperty(cx, obj, "foo", &val));
        actual = val.toString();
        CHECK(JS_StringEqualsAscii(cx, actual, "Hello", &same));
        CHECK(same);
        obj = nullptr;
        actual = nullptr;
    }

    return true;
}
END_TEST(testGCRootedStaticStructInternalStackStorageAugmented)

static JS::PersistentRooted<JSObject*> sLongLived;
BEGIN_TEST(testGCPersistentRootedOutlivesRuntime)
{
    sLongLived.init(cx, JS_NewObject(cx, nullptr));
    CHECK(sLongLived);
    return true;
}
END_TEST(testGCPersistentRootedOutlivesRuntime)

// Unlike the above, the following test is an example of an invalid usage: for
// performance and simplicity reasons, PersistentRooted<Traceable> is not
// allowed to outlive the container it belongs to. The following commented out
// test can be used to verify that the relevant assertion fires as expected.
static JS::PersistentRooted<MyContainer> sContainer;
BEGIN_TEST(testGCPersistentRootedTraceableCannotOutliveRuntime)
{
    JS::Rooted<MyContainer> container(cx);
    container.obj() = JS_NewObject(cx, nullptr);
    container.str() = JS_NewStringCopyZ(cx, "Hello");
    sContainer.init(cx, container);

    // Commenting the following line will trigger an assertion that the
    // PersistentRooted outlives the runtime it is attached to.
    sContainer.reset();

    return true;
}
END_TEST(testGCPersistentRootedTraceableCannotOutliveRuntime)

using MyHashMap = js::GCHashMap<js::Shape*, JSObject*>;

BEGIN_TEST(testGCRootedHashMap)
{
    JS::Rooted<MyHashMap> map(cx, MyHashMap(cx));
    CHECK(map.init(15));
    CHECK(map.initialized());

    for (size_t i = 0; i < 10; ++i) {
        RootedObject obj(cx, JS_NewObject(cx, nullptr));
        RootedValue val(cx, UndefinedValue());
        // Construct a unique property name to ensure that the object creates a
        // new shape.
        char buffer[2];
        buffer[0] = 'a' + i;
        buffer[1] = '\0';
        CHECK(JS_SetProperty(cx, obj, buffer, val));
        CHECK(map.putNew(obj->as<NativeObject>().lastProperty(), obj));
    }

    JS_GC(cx);
    JS_GC(cx);

    for (auto r = map.all(); !r.empty(); r.popFront()) {
        RootedObject obj(cx, r.front().value());
        CHECK(obj->as<NativeObject>().lastProperty() == r.front().key());
    }

    return true;
}
END_TEST(testGCRootedHashMap)

static bool
FillMyHashMap(JSContext* cx, MutableHandle<MyHashMap> map)
{
    for (size_t i = 0; i < 10; ++i) {
        RootedObject obj(cx, JS_NewObject(cx, nullptr));
        RootedValue val(cx, UndefinedValue());
        // Construct a unique property name to ensure that the object creates a
        // new shape.
        char buffer[2];
        buffer[0] = 'a' + i;
        buffer[1] = '\0';
        if (!JS_SetProperty(cx, obj, buffer, val))
            return false;
        if (!map.putNew(obj->as<NativeObject>().lastProperty(), obj))
            return false;
    }
    return true;
}

static bool
CheckMyHashMap(JSContext* cx, Handle<MyHashMap> map)
{
    for (auto r = map.all(); !r.empty(); r.popFront()) {
        RootedObject obj(cx, r.front().value());
        if (obj->as<NativeObject>().lastProperty() != r.front().key())
            return false;
    }
    return true;
}

BEGIN_TEST(testGCHandleHashMap)
{
    JS::Rooted<MyHashMap> map(cx, MyHashMap(cx));
    CHECK(map.init(15));
    CHECK(map.initialized());

    CHECK(FillMyHashMap(cx, &map));

    JS_GC(cx);
    JS_GC(cx);

    CHECK(CheckMyHashMap(cx, map));

    return true;
}
END_TEST(testGCHandleHashMap)

using ShapeVec = GCVector<Shape*>;

BEGIN_TEST(testGCRootedVector)
{
    JS::Rooted<ShapeVec> shapes(cx, ShapeVec(cx));

    for (size_t i = 0; i < 10; ++i) {
        RootedObject obj(cx, JS_NewObject(cx, nullptr));
        RootedValue val(cx, UndefinedValue());
        // Construct a unique property name to ensure that the object creates a
        // new shape.
        char buffer[2];
        buffer[0] = 'a' + i;
        buffer[1] = '\0';
        CHECK(JS_SetProperty(cx, obj, buffer, val));
        CHECK(shapes.append(obj->as<NativeObject>().lastProperty()));
    }

    JS_GC(cx);
    JS_GC(cx);

    for (size_t i = 0; i < 10; ++i) {
        // Check the shape to ensure it did not get collected.
        char buffer[2];
        buffer[0] = 'a' + i;
        buffer[1] = '\0';
        bool match;
        CHECK(JS_StringEqualsAscii(cx, JSID_TO_STRING(shapes[i]->propid()), buffer, &match));
        CHECK(match);
    }

    // Ensure iterator enumeration works through the rooted.
    for (auto shape : shapes)
        CHECK(shape);

    CHECK(receiveConstRefToShapeVector(shapes));

    // Ensure rooted converts to handles.
    CHECK(receiveHandleToShapeVector(shapes));
    CHECK(receiveMutableHandleToShapeVector(&shapes));

    return true;
}

bool
receiveConstRefToShapeVector(const JS::Rooted<GCVector<Shape*>>& rooted)
{
    // Ensure range enumeration works through the reference.
    for (auto shape : rooted)
        CHECK(shape);
    return true;
}

bool
receiveHandleToShapeVector(JS::Handle<GCVector<Shape*>> handle)
{
    // Ensure range enumeration works through the handle.
    for (auto shape : handle)
        CHECK(shape);
    return true;
}

bool
receiveMutableHandleToShapeVector(JS::MutableHandle<GCVector<Shape*>> handle)
{
    // Ensure range enumeration works through the handle.
    for (auto shape : handle)
        CHECK(shape);
    return true;
}
END_TEST(testGCRootedVector)

BEGIN_TEST(testTraceableFifo)
{
    using ShapeFifo = TraceableFifo<Shape*>;
    JS::Rooted<ShapeFifo> shapes(cx, ShapeFifo(cx));
    CHECK(shapes.empty());

    for (size_t i = 0; i < 10; ++i) {
        RootedObject obj(cx, JS_NewObject(cx, nullptr));
        RootedValue val(cx, UndefinedValue());
        // Construct a unique property name to ensure that the object creates a
        // new shape.
        char buffer[2];
        buffer[0] = 'a' + i;
        buffer[1] = '\0';
        CHECK(JS_SetProperty(cx, obj, buffer, val));
        CHECK(shapes.pushBack(obj->as<NativeObject>().lastProperty()));
    }

    CHECK(shapes.length() == 10);

    JS_GC(cx);
    JS_GC(cx);

    for (size_t i = 0; i < 10; ++i) {
        // Check the shape to ensure it did not get collected.
        char buffer[2];
        buffer[0] = 'a' + i;
        buffer[1] = '\0';
        bool match;
        CHECK(JS_StringEqualsAscii(cx, JSID_TO_STRING(shapes.front()->propid()), buffer, &match));
        CHECK(match);
        CHECK(shapes.popFront());
    }

    CHECK(shapes.empty());
    return true;
}
END_TEST(testTraceableFifo)

using ShapeVec = GCVector<Shape*>;

static bool
FillVector(JSContext* cx, MutableHandle<ShapeVec> shapes)
{
    for (size_t i = 0; i < 10; ++i) {
        RootedObject obj(cx, JS_NewObject(cx, nullptr));
        RootedValue val(cx, UndefinedValue());
        // Construct a unique property name to ensure that the object creates a
        // new shape.
        char buffer[2];
        buffer[0] = 'a' + i;
        buffer[1] = '\0';
        if (!JS_SetProperty(cx, obj, buffer, val))
            return false;
        if (!shapes.append(obj->as<NativeObject>().lastProperty()))
            return false;
    }

    // Ensure iterator enumeration works through the mutable handle.
    for (auto shape : shapes) {
        if (!shape)
            return false;
    }

    return true;
}

static bool
CheckVector(JSContext* cx, Handle<ShapeVec> shapes)
{
    for (size_t i = 0; i < 10; ++i) {
        // Check the shape to ensure it did not get collected.
        char buffer[2];
        buffer[0] = 'a' + i;
        buffer[1] = '\0';
        bool match;
        if (!JS_StringEqualsAscii(cx, JSID_TO_STRING(shapes[i]->propid()), buffer, &match))
            return false;
        if (!match)
            return false;
    }

    // Ensure iterator enumeration works through the handle.
    for (auto shape : shapes) {
        if (!shape)
            return false;
    }

    return true;
}

BEGIN_TEST(testGCHandleVector)
{
    JS::Rooted<ShapeVec> vec(cx, ShapeVec(cx));

    CHECK(FillVector(cx, &vec));

    JS_GC(cx);
    JS_GC(cx);

    CHECK(CheckVector(cx, vec));

    return true;
}
END_TEST(testGCHandleVector)

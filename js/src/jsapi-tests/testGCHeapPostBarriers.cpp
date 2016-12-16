/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"

#include "js/RootingAPI.h"
#include "jsapi-tests/tests.h"
#include "vm/Runtime.h"

template <typename T>
static T* CreateGCThing(JSContext* cx)
{
    MOZ_CRASH();
    return nullptr;
}

template <>
JSObject* CreateGCThing(JSContext* cx)
{
    JS::RootedObject obj(cx, JS_NewPlainObject(cx));
    if (!obj)
        return nullptr;
    JS_DefineProperty(cx, obj, "x", 42, 0);
    return obj;
}

template <>
JSFunction* CreateGCThing(JSContext* cx)
{
    /*
     * We don't actually use the function as a function, so here we cheat and
     * cast a JSObject.
     */
    return static_cast<JSFunction*>(CreateGCThing<JSObject>(cx));
}

BEGIN_TEST(testGCHeapPostBarriers)
{
#ifdef JS_GC_ZEAL
    AutoLeaveZeal nozeal(cx);
#endif /* JS_GC_ZEAL */

    /* Sanity check - objects start in the nursery and then become tenured. */
    JS_GC(cx);
    JS::RootedObject obj(cx, CreateGCThing<JSObject>(cx));
    CHECK(js::gc::IsInsideNursery(obj.get()));
    JS_GC(cx);
    CHECK(!js::gc::IsInsideNursery(obj.get()));
    JS::RootedObject tenuredObject(cx, obj);

    /* Currently JSObject and JSFunction objects are nursery allocated. */
    CHECK(TestHeapPostBarriersForType<JSObject>());
    CHECK(TestHeapPostBarriersForType<JSFunction>());

    return true;
}

bool
CanAccessObject(JSObject* obj)
{
    JS::RootedObject rootedObj(cx, obj);
    JS::RootedValue value(cx);
    CHECK(JS_GetProperty(cx, rootedObj, "x", &value));
    CHECK(value.isInt32());
    CHECK(value.toInt32() == 42);
    return true;
}

template <typename T>
bool
TestHeapPostBarriersForType()
{
    CHECK((TestHeapPostBarriersForWrapper<T, JS::Heap<T*>>()));
    CHECK((TestHeapPostBarriersForWrapper<T, js::GCPtr<T*>>()));
    CHECK((TestHeapPostBarriersForWrapper<T, js::HeapPtr<T*>>()));
    return true;
}

template <typename T, typename W>
bool
TestHeapPostBarriersForWrapper()
{
    CHECK((TestHeapPostBarrierUpdate<T, W>()));
    CHECK((TestHeapPostBarrierInitFailure<T, W>()));
    return true;
}

template <typename T, typename W>
bool
TestHeapPostBarrierUpdate()
{
    // Normal case - allocate a heap object, write a nursery pointer into it and
    // check that it gets updated on minor GC.

    T* initialObj = CreateGCThing<T>(cx);
    CHECK(initialObj != nullptr);
    CHECK(js::gc::IsInsideNursery(initialObj));
    uintptr_t initialObjAsInt = uintptr_t(initialObj);

    W* ptr = nullptr;

    {
        auto heapPtr = cx->make_unique<W>();
        CHECK(heapPtr);

        W& wrapper = *heapPtr;
        CHECK(wrapper.get() == nullptr);
        wrapper = initialObj;
        CHECK(wrapper == initialObj);

        ptr = heapPtr.release();
    }

    cx->minorGC(JS::gcreason::API);

    CHECK(uintptr_t(ptr->get()) != initialObjAsInt);
    CHECK(!js::gc::IsInsideNursery(ptr->get()));
    CHECK(CanAccessObject(ptr->get()));

    return true;
}

template <typename T, typename W>
bool
TestHeapPostBarrierInitFailure()
{
    // Failure case - allocate a heap object, write a nursery pointer into it
    // and fail to complete initialization.

    T* initialObj = CreateGCThing<T>(cx);
    CHECK(initialObj != nullptr);
    CHECK(js::gc::IsInsideNursery(initialObj));

    {
        auto heapPtr = cx->make_unique<W>();
        CHECK(heapPtr);

        W& wrapper = *heapPtr;
        CHECK(wrapper.get() == nullptr);
        wrapper = initialObj;
        CHECK(wrapper == initialObj);
    }

    cx->minorGC(JS::gcreason::API);

    return true;
}

END_TEST(testGCHeapPostBarriers)

BEGIN_TEST(testUnbarrieredEquality)
{
    // Use ArrayBuffers because they have finalizers, which allows using them
    // in ObjectPtr without awkward conversations about nursery allocatability.
    JS::RootedObject robj(cx, JS_NewArrayBuffer(cx, 20));
    JS::RootedObject robj2(cx, JS_NewArrayBuffer(cx, 30));
    cx->gc.evictNursery(); // Need tenured objects

    // Need some bare pointers to compare against.
    JSObject* obj = robj;
    JSObject* obj2 = robj2;
    const JSObject* constobj = robj;
    const JSObject* constobj2 = robj2;

    // Make them gray. We will make sure they stay gray. (For most reads, the
    // barrier will unmark gray.)
    using namespace js::gc;
    TenuredCell* cell = &obj->asTenured();
    TenuredCell* cell2 = &obj2->asTenured();
    cell->markIfUnmarked(GRAY);
    cell2->markIfUnmarked(GRAY);
    MOZ_ASSERT(cell->isMarked(GRAY));
    MOZ_ASSERT(cell2->isMarked(GRAY));

    {
        JS::Heap<JSObject*> heap(obj);
        JS::Heap<JSObject*> heap2(obj2);
        CHECK(TestWrapper(obj, obj2, heap, heap2));
        CHECK(TestWrapper(constobj, constobj2, heap, heap2));
    }

    {
        JS::TenuredHeap<JSObject*> heap(obj);
        JS::TenuredHeap<JSObject*> heap2(obj2);
        CHECK(TestWrapper(obj, obj2, heap, heap2));
        CHECK(TestWrapper(constobj, constobj2, heap, heap2));
    }

    {
        JS::ObjectPtr objptr(obj);
        JS::ObjectPtr objptr2(obj2);
        CHECK(TestWrapper(obj, obj2, objptr, objptr2));
        CHECK(TestWrapper(constobj, constobj2, objptr, objptr2));
        objptr.finalize(cx);
        objptr2.finalize(cx);
    }

    // Sanity check that the barriers normally mark things black.
    {
        JS::Heap<JSObject*> heap(obj);
        JS::Heap<JSObject*> heap2(obj2);
        heap.get();
        heap2.get();
        CHECK(cell->isMarked(BLACK));
        CHECK(cell2->isMarked(BLACK));
    }

    return true;
}

template <typename ObjectT, typename WrapperT>
bool
TestWrapper(ObjectT obj, ObjectT obj2, WrapperT& wrapper, WrapperT& wrapper2)
{
    using namespace js::gc;

    const TenuredCell& cell = obj->asTenured();
    const TenuredCell& cell2 = obj2->asTenured();

    int x = 0;

    CHECK(cell.isMarked(GRAY));
    CHECK(cell2.isMarked(GRAY));
    x += obj == obj2;
    CHECK(cell.isMarked(GRAY));
    CHECK(cell2.isMarked(GRAY));
    x += obj == wrapper2;
    CHECK(cell.isMarked(GRAY));
    CHECK(cell2.isMarked(GRAY));
    x += wrapper == obj2;
    CHECK(cell.isMarked(GRAY));
    CHECK(cell2.isMarked(GRAY));
    x += wrapper == wrapper2;
    CHECK(cell.isMarked(GRAY));
    CHECK(cell2.isMarked(GRAY));

    CHECK(x == 0);

    x += obj != obj2;
    CHECK(cell.isMarked(GRAY));
    CHECK(cell2.isMarked(GRAY));
    x += obj != wrapper2;
    CHECK(cell.isMarked(GRAY));
    CHECK(cell2.isMarked(GRAY));
    x += wrapper != obj2;
    CHECK(cell.isMarked(GRAY));
    CHECK(cell2.isMarked(GRAY));
    x += wrapper != wrapper2;
    CHECK(cell.isMarked(GRAY));
    CHECK(cell2.isMarked(GRAY));

    CHECK(x == 4);

    return true;
}

END_TEST(testUnbarrieredEquality)

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
        CHECK(Passthrough(wrapper.get() == nullptr));
        wrapper = initialObj;
        CHECK(Passthrough(wrapper == initialObj));

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
        CHECK(Passthrough(wrapper.get() == nullptr));
        wrapper = initialObj;
        CHECK(Passthrough(wrapper == initialObj));
    }

    cx->minorGC(JS::gcreason::API);

    return true;
}

END_TEST(testGCHeapPostBarriers)

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef JSGC_GENERATIONAL

#include "js/RootingAPI.h"
#include "jsapi-tests/tests.h"

BEGIN_TEST(testGCHeapPostBarriers)
{
    /* Sanity check - objects start in the nursery and then become tenured. */
    JS_GC(cx->runtime());
    JS::RootedObject obj(cx, NurseryObject());
    CHECK(js::gc::IsInsideNursery(obj.get()));
    JS_GC(cx->runtime());
    CHECK(!js::gc::IsInsideNursery(obj.get()));
    JS::RootedObject tenuredObject(cx, obj);

    /* Currently JSObject and JSFunction objects are nursery allocated. */
    CHECK(TestHeapPostBarriers(NurseryObject()));
    CHECK(TestHeapPostBarriers(NurseryFunction()));

    return true;
}

MOZ_NEVER_INLINE bool
Passthrough(bool value)
{
    /* Work around a Win64 optimization bug in VS2010. (Bug 1033146) */
    return value;
}

template <typename T>
bool
TestHeapPostBarriers(T initialObj)
{
    CHECK(initialObj != nullptr);
    CHECK(js::gc::IsInsideNursery(initialObj));

    /* Construct Heap<> wrapper. */
    JS::Heap<T> *heapData = new JS::Heap<T>();
    CHECK(heapData);
    CHECK(Passthrough(heapData->get() == nullptr));
    heapData->set(initialObj);

    /* Store the pointer as an integer so that the hazard analysis will miss it. */
    uintptr_t initialObjAsInt = uintptr_t(initialObj);

    /* Perform minor GC and check heap wrapper is udated with new pointer. */
    js::MinorGC(cx, JS::gcreason::API);
    CHECK(uintptr_t(heapData->get()) != initialObjAsInt);
    CHECK(!js::gc::IsInsideNursery(heapData->get()));

    /* Check object is definitely still alive. */
    JS::Rooted<T> obj(cx, heapData->get());
    JS::RootedValue value(cx);
    CHECK(JS_GetProperty(cx, obj, "x", &value));
    CHECK(value.isInt32());
    CHECK(value.toInt32() == 42);

    delete heapData;
    return true;
}

JSObject *NurseryObject()
{
    JS::RootedObject obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
    if (!obj)
        return nullptr;
    JS_DefineProperty(cx, obj, "x", 42, 0);
    return obj;
}

JSFunction *NurseryFunction()
{
    /*
     * We don't actually use the function as a function, so here we cheat and
     * cast a JSObject.
     */
    return static_cast<JSFunction *>(NurseryObject());
}

END_TEST(testGCHeapPostBarriers)

#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef JSGC_GENERATIONAL

#include "gc/Barrier.h"
#include "jsapi-tests/tests.h"

using namespace JS;
using namespace js;

BEGIN_TEST(testGCStoreBufferRemoval)
{
    // Sanity check - objects start in the nursery and then become tenured.
    JS_GC(cx->runtime());
    JS::RootedObject obj(cx, NurseryObject());
    CHECK(js::gc::IsInsideNursery(rt, obj.get()));
    JS_GC(cx->runtime());
    CHECK(!js::gc::IsInsideNursery(rt, obj.get()));
    JS::RootedObject tenuredObject(cx, obj);

    // Test removal of store buffer entries added by RelocatablePtr<T>.
    {
        JSObject *badObject = reinterpret_cast<JSObject*>(1);
        JSObject *punnedPtr = nullptr;
        RelocatablePtrObject* relocPtr =
            reinterpret_cast<RelocatablePtrObject*>(&punnedPtr);
        new (relocPtr) RelocatablePtrObject;
        *relocPtr = NurseryObject();
        relocPtr->~RelocatablePtrObject();
        punnedPtr = badObject;
        JS_GC(cx->runtime());

        new (relocPtr) RelocatablePtrObject;
        *relocPtr = NurseryObject();
        *relocPtr = tenuredObject;
        relocPtr->~RelocatablePtrObject();
        punnedPtr = badObject;
        JS_GC(cx->runtime());

        new (relocPtr) RelocatablePtrObject;
        *relocPtr = NurseryObject();
        *relocPtr = nullptr;
        relocPtr->~RelocatablePtrObject();
        punnedPtr = badObject;
        JS_GC(cx->runtime());
    }

    // Test removal of store buffer entries added by RelocatableValue.
    {
        Value punnedValue;
        RelocatableValue *relocValue = reinterpret_cast<RelocatableValue*>(&punnedValue);
        new (relocValue) RelocatableValue;
        *relocValue = ObjectValue(*NurseryObject());
        relocValue->~RelocatableValue();
        punnedValue = ObjectValueCrashOnTouch();
        JS_GC(cx->runtime());

        new (relocValue) RelocatableValue;
        *relocValue = ObjectValue(*NurseryObject());
        *relocValue = ObjectValue(*tenuredObject);
        relocValue->~RelocatableValue();
        punnedValue = ObjectValueCrashOnTouch();
        JS_GC(cx->runtime());

        new (relocValue) RelocatableValue;
        *relocValue = ObjectValue(*NurseryObject());
        *relocValue = NullValue();
        relocValue->~RelocatableValue();
        punnedValue = ObjectValueCrashOnTouch();
        JS_GC(cx->runtime());
    }

    // Test removal of store buffer entries added by Heap<T>.
    {
        JSObject *badObject = reinterpret_cast<JSObject*>(1);
        JSObject *punnedPtr = nullptr;
        Heap<JSObject*>* heapPtr =
            reinterpret_cast<Heap<JSObject*>*>(&punnedPtr);
        new (heapPtr) Heap<JSObject*>;
        *heapPtr = NurseryObject();
        heapPtr->~Heap<JSObject*>();
        punnedPtr = badObject;
        JS_GC(cx->runtime());

        new (heapPtr) Heap<JSObject*>;
        *heapPtr = NurseryObject();
        *heapPtr = tenuredObject;
        heapPtr->~Heap<JSObject*>();
        punnedPtr = badObject;
        JS_GC(cx->runtime());

        new (heapPtr) Heap<JSObject*>;
        *heapPtr = NurseryObject();
        *heapPtr = nullptr;
        heapPtr->~Heap<JSObject*>();
        punnedPtr = badObject;
        JS_GC(cx->runtime());
    }

    return true;
}

JSObject *NurseryObject()
{
    return JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr());
}
END_TEST(testGCStoreBufferRemoval)

#endif

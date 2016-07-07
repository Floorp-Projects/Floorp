/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Barrier.h"
#include "jsapi-tests/tests.h"

using namespace JS;
using namespace js;

struct AutoIgnoreRootingHazards {
    // Force a nontrivial destructor so the compiler sees the whole RAII scope
    static volatile int depth;
    AutoIgnoreRootingHazards() { depth++; }
    ~AutoIgnoreRootingHazards() { depth--; }
} JS_HAZ_GC_SUPPRESSED;
volatile int AutoIgnoreRootingHazards::depth = 0;

BEGIN_TEST(testGCStoreBufferRemoval)
{
    // Sanity check - objects start in the nursery and then become tenured.
    JS_GC(cx);
    JS::RootedObject obj(cx, NurseryObject());
    CHECK(js::gc::IsInsideNursery(obj.get()));
    JS_GC(cx);
    CHECK(!js::gc::IsInsideNursery(obj.get()));
    JS::RootedObject tenuredObject(cx, obj);

    // Hide the horrors herein from the static rooting analysis.
    AutoIgnoreRootingHazards ignore;

    // Test removal of store buffer entries added by HeapPtr<T>.
    {
        JSObject* badObject = reinterpret_cast<JSObject*>(1);
        JSObject* punnedPtr = nullptr;
        HeapPtr<JSObject*>* relocPtr =
            reinterpret_cast<HeapPtr<JSObject*>*>(&punnedPtr);
        new (relocPtr) HeapPtr<JSObject*>;
        *relocPtr = NurseryObject();
        relocPtr->~HeapPtr<JSObject*>();
        punnedPtr = badObject;
        JS_GC(cx);

        new (relocPtr) HeapPtr<JSObject*>;
        *relocPtr = NurseryObject();
        *relocPtr = tenuredObject;
        relocPtr->~HeapPtr<JSObject*>();
        punnedPtr = badObject;
        JS_GC(cx);

        new (relocPtr) HeapPtr<JSObject*>;
        *relocPtr = NurseryObject();
        *relocPtr = nullptr;
        relocPtr->~HeapPtr<JSObject*>();
        punnedPtr = badObject;
        JS_GC(cx);
    }

    // Test removal of store buffer entries added by HeapPtr<Value>.
    {
        Value punnedValue;
        HeapPtr<Value>* relocValue = reinterpret_cast<HeapPtr<Value>*>(&punnedValue);
        new (relocValue) HeapPtr<Value>;
        *relocValue = ObjectValue(*NurseryObject());
        relocValue->~HeapPtr<Value>();
        punnedValue = ObjectValueCrashOnTouch();
        JS_GC(cx);

        new (relocValue) HeapPtr<Value>;
        *relocValue = ObjectValue(*NurseryObject());
        *relocValue = ObjectValue(*tenuredObject);
        relocValue->~HeapPtr<Value>();
        punnedValue = ObjectValueCrashOnTouch();
        JS_GC(cx);

        new (relocValue) HeapPtr<Value>;
        *relocValue = ObjectValue(*NurseryObject());
        *relocValue = NullValue();
        relocValue->~HeapPtr<Value>();
        punnedValue = ObjectValueCrashOnTouch();
        JS_GC(cx);
    }

    // Test removal of store buffer entries added by Heap<T>.
    {
        JSObject* badObject = reinterpret_cast<JSObject*>(1);
        JSObject* punnedPtr = nullptr;
        Heap<JSObject*>* heapPtr =
            reinterpret_cast<Heap<JSObject*>*>(&punnedPtr);
        new (heapPtr) Heap<JSObject*>;
        *heapPtr = NurseryObject();
        heapPtr->~Heap<JSObject*>();
        punnedPtr = badObject;
        JS_GC(cx);

        new (heapPtr) Heap<JSObject*>;
        *heapPtr = NurseryObject();
        *heapPtr = tenuredObject;
        heapPtr->~Heap<JSObject*>();
        punnedPtr = badObject;
        JS_GC(cx);

        new (heapPtr) Heap<JSObject*>;
        *heapPtr = NurseryObject();
        *heapPtr = nullptr;
        heapPtr->~Heap<JSObject*>();
        punnedPtr = badObject;
        JS_GC(cx);
    }

    return true;
}

JSObject* NurseryObject()
{
    return JS_NewPlainObject(cx);
}
END_TEST(testGCStoreBufferRemoval)

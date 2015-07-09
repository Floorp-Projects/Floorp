/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testGCExactRooting)
{
    JS::RootedObject rootCx(cx, JS_NewPlainObject(cx));
    JS::RootedObject rootRt(cx->runtime(), JS_NewPlainObject(cx));

    JS_GC(cx->runtime());

    /* Use the objects we just created to ensure that they are still alive. */
    JS_DefineProperty(cx, rootCx, "foo", JS::UndefinedHandleValue, 0);
    JS_DefineProperty(cx, rootRt, "foo", JS::UndefinedHandleValue, 0);

    return true;
}
END_TEST(testGCExactRooting)

BEGIN_TEST(testGCSuppressions)
{
    JS::AutoAssertOnGC nogc;
    JS::AutoCheckCannotGC checkgc;
    JS::AutoSuppressGCAnalysis noanalysis;

    JS::AutoAssertOnGC nogcRt(cx->runtime());
    JS::AutoCheckCannotGC checkgcRt(cx->runtime());
    JS::AutoSuppressGCAnalysis noanalysisRt(cx->runtime());

    return true;
}
END_TEST(testGCSuppressions)

struct MyContainer : public JS::StaticTraceable
{
    RelocatablePtrObject obj;
    RelocatablePtrString str;

    MyContainer() : obj(nullptr), str(nullptr) {}
    static void trace(MyContainer* self, JSTracer* trc) {
        if (self->obj)
            js::TraceEdge(trc, &self->obj, "test container");
        if (self->str)
            js::TraceEdge(trc, &self->str, "test container");
    }
};

namespace js {
template <>
struct RootedBase<MyContainer> {
    RelocatablePtrObject& obj() { return static_cast<Rooted<MyContainer>*>(this)->get().obj; }
    RelocatablePtrString& str() { return static_cast<Rooted<MyContainer>*>(this)->get().str; }
};
} // namespace js

BEGIN_TEST(testGCGenericRootedInternalStackStorage)
{
    JS::Rooted<MyContainer> container(cx);
    container.get().obj = JS_NewObject(cx, nullptr);
    container.get().str = JS_NewStringCopyZ(cx, "Hello");

    JS_GC(cx->runtime());
    JS_GC(cx->runtime());

    JS::RootedObject obj(cx, container.get().obj);
    JS::RootedValue val(cx, StringValue(container.get().str));
    CHECK(JS_SetProperty(cx, obj, "foo", val));
    return true;
}
END_TEST(testGCGenericRootedInternalStackStorage)

BEGIN_TEST(testGCGenericRootedInternalStackStorageAugmented)
{
    JS::Rooted<MyContainer> container(cx);
    container.obj() = JS_NewObject(cx, nullptr);
    container.str() = JS_NewStringCopyZ(cx, "Hello");

    JS_GC(cx->runtime());
    JS_GC(cx->runtime());

    JS::RootedObject obj(cx, container.obj());
    JS::RootedValue val(cx, StringValue(container.str()));
    CHECK(JS_SetProperty(cx, obj, "foo", val));
    return true;
}
END_TEST(testGCGenericRootedInternalStackStorageAugmented)

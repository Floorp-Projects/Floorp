/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"

#include "js/HeapAPI.h"
#include "jsapi-tests/tests.h"

class TestTracer : public JS::CallbackTracer
{
    void onChild(const JS::GCCellPtr& thing) override {
        if (thing.asCell() == expectedCell && thing.kind() == expectedKind)
            found = true;
    }

  public:
    js::gc::Cell* expectedCell;
    JS::TraceKind expectedKind;
    bool found;

    explicit TestTracer(JSContext* cx)
      : JS::CallbackTracer(cx),
        expectedCell(nullptr),
        expectedKind(static_cast<JS::TraceKind>(0)),
        found(false)
    { }
};

static const JSClass TestClass = {
    "TestClass",
    JSCLASS_HAS_RESERVED_SLOTS(1)
};

BEGIN_TEST(testPrivateGCThingValue)
{
    JS::RootedObject obj(cx, JS_NewObject(cx, &TestClass));
    CHECK(obj);

    // Make a JSScript to stick into a PrivateGCThingValue.
    JS::RootedScript script(cx);
    const char code[] = "'objet petit a'";
    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, __LINE__);
    CHECK(JS_CompileScript(cx, code, sizeof(code) - 1, options, &script));
    JS_SetReservedSlot(obj, 0, PrivateGCThingValue(script));

    TestTracer trc(cx);
    trc.expectedCell = script;
    trc.expectedKind = JS::TraceKind::Script;
    JS::TraceChildren(&trc, JS::GCCellPtr(obj, JS::TraceKind::Object));
    CHECK(trc.found);

    return true;
}
END_TEST(testPrivateGCThingValue)

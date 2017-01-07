/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jspubtd.h"

#include "gc/Heap.h"

#include "jsapi-tests/tests.h"

JS::GCCellPtr
GivesAndTakesCells(JS::GCCellPtr cell)
{
    return cell;
}

BEGIN_TEST(testGCCellPtr)
{
    JS::RootedObject obj(cx, JS_NewPlainObject(cx));
    CHECK(obj);

    JS::RootedString str(cx, JS_NewStringCopyZ(cx, "probably foobar"));
    CHECK(str);

    const char* code = "function foo() { return 'bar'; }";
    JS::CompileOptions opts(cx);
    JS::RootedScript script(cx);
    CHECK(JS_CompileScript(cx, code, strlen(code), opts, &script));
    CHECK(script);

    CHECK(!JS::GCCellPtr(nullptr));

    CHECK(JS::GCCellPtr(obj.get()));
    CHECK(JS::GCCellPtr(obj.get()).kind() == JS::TraceKind::Object);
    CHECK(JS::GCCellPtr(JS::ObjectValue(*obj)).kind() == JS::TraceKind::Object);

    CHECK(JS::GCCellPtr(str.get()));
    CHECK(JS::GCCellPtr(str.get()).kind() == JS::TraceKind::String);
    CHECK(JS::GCCellPtr(JS::StringValue(str)).kind() == JS::TraceKind::String);

    CHECK(JS::GCCellPtr(script.get()));
    CHECK(!JS::GCCellPtr(nullptr));
    CHECK(JS::GCCellPtr(script.get()).kind() == JS::TraceKind::Script);

    JS::GCCellPtr objcell(obj.get());
    JS::GCCellPtr scriptcell = JS::GCCellPtr(script.get());
    CHECK(GivesAndTakesCells(objcell));
    CHECK(GivesAndTakesCells(scriptcell));

    JS::GCCellPtr copy = objcell;
    CHECK(copy == objcell);

    CHECK(js::gc::detail::GetGCThingRuntime(scriptcell.unsafeAsUIntPtr()) == cx->runtime());

    return true;
}
END_TEST(testGCCellPtr)

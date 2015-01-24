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

    const char *code = "function foo() { return 'bar'; }";
    JS::CompileOptions opts(cx);
    JS::RootedScript script(cx);
    CHECK(JS_CompileScript(cx, obj, code, strlen(code), opts, &script));
    CHECK(script);

    CHECK(!JS::GCCellPtr::NullPtr());

    CHECK(JS::GCCellPtr(obj.get()));
    CHECK(JS::GCCellPtr(obj.get()).kind() == JSTRACE_OBJECT);
    CHECK(JS::GCCellPtr(JS::ObjectValue(*obj)).kind() == JSTRACE_OBJECT);

    CHECK(JS::GCCellPtr(str.get()));
    CHECK(JS::GCCellPtr(str.get()).kind() == JSTRACE_STRING);
    CHECK(JS::GCCellPtr(JS::StringValue(str)).kind() == JSTRACE_STRING);

    CHECK(JS::GCCellPtr(script.get()));
    CHECK(!JS::GCCellPtr::NullPtr());
    CHECK(JS::GCCellPtr(script.get()).kind() == JSTRACE_SCRIPT);

    JS::GCCellPtr objcell(obj.get());
    JS::GCCellPtr scriptcell = JS::GCCellPtr(script.get());
    CHECK(GivesAndTakesCells(objcell));
    CHECK(GivesAndTakesCells(scriptcell));

    JS::GCCellPtr copy = objcell;
    CHECK(copy == objcell);

    CHECK(js::gc::detail::GetGCThingRuntime(scriptcell.unsafeAsUIntPtr()) == rt);

    return true;
}
END_TEST(testGCCellPtr)

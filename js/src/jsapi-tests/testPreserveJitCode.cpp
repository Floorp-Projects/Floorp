/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

using namespace JS;

static void
ScriptCallback(JSRuntime *rt, void *data, JSScript *script)
{
    unsigned &count = *static_cast<unsigned *>(data);
    if (script->hasIonScript())
        ++count;
}

BEGIN_TEST(test_PreserveJitCode)
{
    CHECK(testPreserveJitCode(false, 0));
    CHECK(testPreserveJitCode(true, 1));
    return true;
}

unsigned
countIonScripts(JSObject *global)
{
    unsigned count = 0;
    js::IterateScripts(rt, global->compartment(), &count, ScriptCallback);
    return count;
}

bool
testPreserveJitCode(bool preserveJitCode, unsigned remainingIonScripts)
{
    rt->options().setBaseline(true);
    rt->options().setIon(true);
    rt->setOffthreadIonCompilationEnabled(false);

    RootedObject global(cx, createTestGlobal(preserveJitCode));
    CHECK(global);
    JSAutoCompartment ac(cx, global);

    CHECK_EQUAL(countIonScripts(global), 0u);

    const char *source =
        "var i = 0;\n"
        "var sum = 0;\n"
        "while (i < 10) {\n"
        "    sum += i;\n"
        "    ++i;\n"
        "}\n"
        "return sum;\n";
    unsigned length = strlen(source);

    JS::RootedFunction fun(cx);
    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, 1);
    CHECK(JS_CompileFunction(cx, global, "f", 0, nullptr, source, length, options, &fun));

    RootedValue value(cx);
    for (unsigned i = 0; i < 1500; ++i)
        CHECK(JS_CallFunction(cx, global, fun, JS::HandleValueArray::empty(), &value));
    CHECK_EQUAL(value.toInt32(), 45);
    CHECK_EQUAL(countIonScripts(global), 1u);

    GCForReason(rt, gcreason::API);
    CHECK_EQUAL(countIonScripts(global), remainingIonScripts);

    ShrinkingGC(rt, gcreason::API);
    CHECK_EQUAL(countIonScripts(global), 0u);

    return true;
}

JSObject *
createTestGlobal(bool preserveJitCode)
{
    JS::CompartmentOptions options;
    options.setVersion(JSVERSION_LATEST);
    options.setPreserveJitCode(preserveJitCode);
    return JS_NewGlobalObject(cx, getGlobalClass(), nullptr, JS::FireOnNewGlobalHook, options);
}
END_TEST(test_PreserveJitCode)

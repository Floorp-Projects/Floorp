/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// For js::jit::IsIonEnabled().
#include "jit/Ion.h"

#include "jsapi-tests/tests.h"

using namespace JS;

static void
ScriptCallback(JSRuntime* rt, void* data, JSScript* script)
{
    unsigned& count = *static_cast<unsigned*>(data);
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
countIonScripts(JSObject* global)
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

#ifdef JS_CODEGEN_ARM64
    // The ARM64 Ion JIT is not yet enabled, so this test will fail with
    // countIonScripts(global) == 0. Once Ion is enabled for ARM64, this test
    // should be passing again, and this code can be deleted.
    // Bug 1208526 - ARM64: Reenable jsapi-tests/testPreserveJitCode once Ion is enabled
    if (!js::jit::IsIonEnabled(cx))
        knownFail = true;
#endif

    CHECK_EQUAL(countIonScripts(global), 0u);

    const char* source =
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
    JS::AutoObjectVector emptyScopeChain(cx);
    CHECK(JS::CompileFunction(cx, emptyScopeChain, options, "f", 0, nullptr,
			      source, length, &fun));

    RootedValue value(cx);
    for (unsigned i = 0; i < 1500; ++i)
        CHECK(JS_CallFunction(cx, global, fun, JS::HandleValueArray::empty(), &value));
    CHECK_EQUAL(value.toInt32(), 45);
    CHECK_EQUAL(countIonScripts(global), 1u);

    GCForReason(rt, GC_NORMAL, gcreason::API);
    CHECK_EQUAL(countIonScripts(global), remainingIonScripts);

    GCForReason(rt, GC_SHRINK, gcreason::API);
    CHECK_EQUAL(countIonScripts(global), 0u);

    return true;
}

JSObject*
createTestGlobal(bool preserveJitCode)
{
    JS::CompartmentOptions options;
    options.setVersion(JSVERSION_LATEST);
    options.setPreserveJitCode(preserveJitCode);
    return JS_NewGlobalObject(cx, getGlobalClass(), nullptr, JS::FireOnNewGlobalHook, options);
}
END_TEST(test_PreserveJitCode)

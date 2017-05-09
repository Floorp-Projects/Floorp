/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

#include <stdio.h>

#include "js/Initialization.h"
#include "js/RootingAPI.h"

JSAPITest* JSAPITest::list;

bool JSAPITest::init()
{
    cx = createContext();
    if (!cx)
        return false;
    if (!JS::InitSelfHostedCode(cx))
        return false;
    JS_BeginRequest(cx);
    global.init(cx);
    createGlobal();
    if (!global)
        return false;
    JS_EnterCompartment(cx, global);
    return true;
}

void JSAPITest::uninit()
{
    if (oldCompartment) {
        JS_LeaveCompartment(cx, oldCompartment);
        oldCompartment = nullptr;
    }
    if (global) {
        JS_LeaveCompartment(cx, nullptr);
        global = nullptr;
    }
    if (cx) {
        JS_EndRequest(cx);
        destroyContext();
        cx = nullptr;
    }
}

bool JSAPITest::exec(const char* bytes, const char* filename, int lineno)
{
    JS::RootedValue v(cx);
    JS::CompileOptions opts(cx);
    opts.setFileAndLine(filename, lineno);
    return JS::Evaluate(cx, opts, bytes, strlen(bytes), &v) ||
        fail(JSAPITestString(bytes), filename, lineno);
}

bool JSAPITest::execDontReport(const char* bytes, const char* filename, int lineno)
{
    JS::RootedValue v(cx);
    JS::CompileOptions opts(cx);
    opts.setFileAndLine(filename, lineno);
    return JS::Evaluate(cx, opts, bytes, strlen(bytes), &v);
}

bool JSAPITest::evaluate(const char* bytes, const char* filename, int lineno,
                         JS::MutableHandleValue vp)
{
    JS::CompileOptions opts(cx);
    opts.setFileAndLine(filename, lineno);
    return JS::Evaluate(cx, opts, bytes, strlen(bytes), vp) ||
        fail(JSAPITestString(bytes), filename, lineno);
}

bool JSAPITest::definePrint()
{
    return JS_DefineFunction(cx, global, "print", (JSNative) print, 0, 0);
}

JSObject* JSAPITest::createGlobal(JSPrincipals* principals)
{
    /* Create the global object. */
    JS::RootedObject newGlobal(cx);
    JS::CompartmentOptions options;
    options.behaviors().setVersion(JSVERSION_LATEST);
    newGlobal = JS_NewGlobalObject(cx, getGlobalClass(), principals, JS::FireOnNewGlobalHook,
                                   options);
    if (!newGlobal)
        return nullptr;

    JSAutoCompartment ac(cx, newGlobal);

    // Populate the global object with the standard globals like Object and
    // Array.
    if (!JS_InitStandardClasses(cx, newGlobal))
        return nullptr;

    global = newGlobal;
    return newGlobal;
}

int main(int argc, char* argv[])
{
    int total = 0;
    int failures = 0;
    const char* filter = (argc == 2) ? argv[1] : nullptr;

    if (!JS_Init()) {
        printf("TEST-UNEXPECTED-FAIL | jsapi-tests | JS_Init() failed.\n");
        return 1;
    }

    for (JSAPITest* test = JSAPITest::list; test; test = test->next) {
        const char* name = test->name();
        if (filter && strstr(name, filter) == nullptr)
            continue;

        total += 1;

        printf("%s\n", name);
        if (!test->init()) {
            printf("TEST-UNEXPECTED-FAIL | %s | Failed to initialize.\n", name);
            failures++;
            test->uninit();
            continue;
        }

        if (test->run(test->global)) {
            printf("TEST-PASS | %s | ok\n", name);
        } else {
            JSAPITestString messages = test->messages();
            printf("%s | %s | %.*s\n",
                   (test->knownFail ? "TEST-KNOWN-FAIL" : "TEST-UNEXPECTED-FAIL"),
                   name, (int) messages.length(), messages.begin());
            if (!test->knownFail)
                failures++;
        }
        test->uninit();
    }

    MOZ_RELEASE_ASSERT(!JSRuntime::hasLiveRuntimes());
    JS_ShutDown();

    if (failures) {
        printf("\n%d unexpected failure%s.\n", failures, (failures == 1 ? "" : "s"));
        return 1;
    }
    printf("\nPassed: ran %d tests.\n", total);
    return 0;
}

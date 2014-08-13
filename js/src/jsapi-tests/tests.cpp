/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

#include <stdio.h>

#include "js/RootingAPI.h"

JSAPITest *JSAPITest::list;

bool JSAPITest::init()
{
    rt = createRuntime();
    if (!rt)
        return false;
    cx = createContext();
    if (!cx)
        return false;
    JS_BeginRequest(cx);
    JS::RootedObject global(cx, createGlobal());
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
        JS::RemoveObjectRoot(cx, &global);
        global = nullptr;
    }
    if (cx) {
        JS_EndRequest(cx);
        JS_DestroyContext(cx);
        cx = nullptr;
    }
    if (rt) {
        destroyRuntime();
        rt = nullptr;
    }
}

bool JSAPITest::exec(const char *bytes, const char *filename, int lineno)
{
    JS::RootedValue v(cx);
    JS::HandleObject global = JS::HandleObject::fromMarkedLocation(this->global.unsafeGet());
    return JS_EvaluateScript(cx, global, bytes, strlen(bytes), filename, lineno, &v) ||
        fail(JSAPITestString(bytes), filename, lineno);
}

bool JSAPITest::evaluate(const char *bytes, const char *filename, int lineno,
                         JS::MutableHandleValue vp)
{
    JS::HandleObject global = JS::HandleObject::fromMarkedLocation(this->global.unsafeGet());
    return JS_EvaluateScript(cx, global, bytes, strlen(bytes), filename, lineno, vp) ||
        fail(JSAPITestString(bytes), filename, lineno);
}

bool JSAPITest::definePrint()
{
    JS::HandleObject global = JS::HandleObject::fromMarkedLocation(this->global.unsafeGet());
    return JS_DefineFunction(cx, global, "print", (JSNative) print, 0, 0);
}

JSObject * JSAPITest::createGlobal(JSPrincipals *principals)
{
    /* Create the global object. */
    JS::CompartmentOptions options;
    options.setVersion(JSVERSION_LATEST);
    global = JS_NewGlobalObject(cx, getGlobalClass(), principals, JS::FireOnNewGlobalHook, options);
    if (!global)
        return nullptr;
    JS::AddNamedObjectRoot(cx, &global, "test-global");
    JS::HandleObject globalHandle = JS::HandleObject::fromMarkedLocation(global.unsafeGet());
    JSAutoCompartment ac(cx, globalHandle);

    /* Populate the global object with the standard globals, like Object and
       Array. */
    if (!JS_InitStandardClasses(cx, globalHandle)) {
        global = nullptr;
        JS::RemoveObjectRoot(cx, &global);
    }

    return global;
}

int main(int argc, char *argv[])
{
    int total = 0;
    int failures = 0;
    const char *filter = (argc == 2) ? argv[1] : nullptr;

    if (!JS_Init()) {
        printf("TEST-UNEXPECTED-FAIL | jsapi-tests | JS_Init() failed.\n");
        return 1;
    }

    for (JSAPITest *test = JSAPITest::list; test; test = test->next) {
        const char *name = test->name();
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

        JS::HandleObject global = JS::HandleObject::fromMarkedLocation(test->global.unsafeGet());
        if (test->run(global)) {
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

    JS_ShutDown();

    if (failures) {
        printf("\n%d unexpected failure%s.\n", failures, (failures == 1 ? "" : "s"));
        return 1;
    }
    printf("\nPassed: ran %d tests.\n", total);
    return 0;
}

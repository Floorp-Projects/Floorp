/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

using JS::CreateTypeError;
using JS::Rooted;
using JS::ObjectValue;
using JS::Value;

static size_t uncaughtCount = 0;

BEGIN_TEST(testUncaughtError)
{
    JSErrorReporter old = JS_SetErrorReporter(cx, UncaughtErrorReporter);

    CHECK(uncaughtCount == 0);

    Rooted<JSString*> empty(cx, JS_GetEmptyString(JS_GetRuntime(cx)));
    if (!empty)
        return false;

    Rooted<Value> err(cx);
    if (!CreateTypeError(cx, empty, empty, 0, 0, nullptr, empty, &err))
        return false;

    Rooted<JSObject*> errObj(cx, &err.toObject());
    if (!JS_SetProperty(cx, errObj, "fileName", err))
        return false;
    if (!JS_SetProperty(cx, errObj, "lineNumber", err))
        return false;
    if (!JS_SetProperty(cx, errObj, "columnNumber", err))
        return false;
    if (!JS_SetProperty(cx, errObj, "stack", err))
        return false;
    if (!JS_SetProperty(cx, errObj, "message", err))
        return false;

    JS_SetPendingException(cx, err);
    JS_ReportPendingException(cx);

    CHECK(uncaughtCount == 1);

    JS_SetErrorReporter(cx, old);

    return true;
}

static void
UncaughtErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    uncaughtCount++;
}

END_TEST(testUncaughtError)

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"

#include "jsapi-tests/tests.h"

using namespace JS;

static bool executor_called = false;

static bool
PromiseExecutor(JSContext* cx, unsigned argc, Value* vp)
{
#ifdef DEBUG
    CallArgs args = CallArgsFromVp(argc, vp);
#endif // DEBUG
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(args[0].toObject().is<JSFunction>());
    MOZ_ASSERT(args[1].toObject().is<JSFunction>());

    executor_called = true;
    return true;
}

static JSObject*
CreatePromise(JSContext* cx)
{
    RootedFunction executor(cx, JS_NewFunction(cx, PromiseExecutor, 2, 0, "executor"));
    if (!executor)
        return nullptr;
    return JS::NewPromiseObject(cx, executor);
}

BEGIN_TEST(testPromise_NewPromise)
{
    RootedObject promise(cx, CreatePromise(cx));
    CHECK(promise);
    CHECK(executor_called);

    return true;
}
END_TEST(testPromise_NewPromise)

BEGIN_TEST(testPromise_GetPromiseState)
{
    RootedObject promise(cx, CreatePromise(cx));
    if (!promise)
        return false;

    CHECK(JS::GetPromiseState(promise) == JS::PromiseState::Pending);

    return true;
}
END_TEST(testPromise_GetPromiseState)

BEGIN_TEST(testPromise_ResolvePromise)
{
    RootedObject promise(cx, CreatePromise(cx));
    if (!promise)
        return false;

    RootedValue result(cx);
    result.setInt32(42);
    JS::ResolvePromise(cx, promise, result);

    CHECK(JS::GetPromiseState(promise) == JS::PromiseState::Fulfilled);

    return true;
}
END_TEST(testPromise_ResolvePromise)

BEGIN_TEST(testPromise_RejectPromise)
{
    RootedObject promise(cx, CreatePromise(cx));
    if (!promise)
        return false;

    RootedValue result(cx);
    result.setInt32(42);
    JS::RejectPromise(cx, promise, result);

    CHECK(JS::GetPromiseState(promise) == JS::PromiseState::Rejected);

    return true;
}
END_TEST(testPromise_RejectPromise)

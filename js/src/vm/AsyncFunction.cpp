/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/AsyncFunction.h"

#include "jscompartment.h"

#include "builtin/Promise.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/SelfHosting.h"

using namespace js;
using namespace js::gc;

/* static */ bool
GlobalObject::initAsyncFunction(JSContext* cx, Handle<GlobalObject*> global)
{
    if (global->getReservedSlot(ASYNC_FUNCTION_PROTO).isObject())
        return true;

    RootedObject asyncFunctionProto(cx, NewSingletonObjectWithFunctionPrototype(cx, global));
    if (!asyncFunctionProto)
        return false;

    if (!DefineToStringTag(cx, asyncFunctionProto, cx->names().AsyncFunction))
        return false;

    RootedValue function(cx, global->getConstructor(JSProto_Function));
    if (!function.toObjectOrNull())
        return false;
    RootedObject proto(cx, &function.toObject());
    RootedAtom name(cx, cx->names().AsyncFunction);
    RootedObject asyncFunction(cx, NewFunctionWithProto(cx, AsyncFunctionConstructor, 1,
                                                        JSFunction::NATIVE_CTOR, nullptr, name,
                                                        proto));
    if (!asyncFunction)
        return false;
    if (!LinkConstructorAndPrototype(cx, asyncFunction, asyncFunctionProto))
        return false;

    global->setReservedSlot(ASYNC_FUNCTION, ObjectValue(*asyncFunction));
    global->setReservedSlot(ASYNC_FUNCTION_PROTO, ObjectValue(*asyncFunctionProto));
    return true;
}

static bool AsyncFunctionStart(JSContext* cx, HandleValue generatorVal, MutableHandleValue rval);

#define UNWRAPPED_ASYNC_WRAPPED_SLOT 1
#define WRAPPED_ASYNC_UNWRAPPED_SLOT 0

// Async Functions proposal 1.1.8 and 1.2.14.
static bool
WrappedAsyncFunction(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedFunction wrapped(cx, &args.callee().as<JSFunction>());
    RootedValue unwrappedVal(cx, wrapped->getExtendedSlot(WRAPPED_ASYNC_UNWRAPPED_SLOT));
    RootedFunction unwrapped(cx, &unwrappedVal.toObject().as<JSFunction>());
    RootedValue thisValue(cx, args.thisv());

    // Step 2.
    // Also does a part of 2.2 steps 1-2.
    RootedValue generatorVal(cx);
    InvokeArgs args2(cx);
    if (!args2.init(cx, argc))
        return false;
    for (size_t i = 0, len = argc; i < len; i++)
        args2[i].set(args[i]);
    if (Call(cx, unwrappedVal, thisValue, args2, &generatorVal)) {
        // Steps 3, 5.
        return AsyncFunctionStart(cx, generatorVal, args.rval());
    }

    // Step 4.
    RootedValue exc(cx);
    if (!GetAndClearException(cx, &exc))
        return false;
    RootedObject rejectPromise(cx, PromiseObject::unforgeableReject(cx, exc));
    if (!rejectPromise)
        return false;

    // Step 5.
    args.rval().setObject(*rejectPromise);
    return true;
}

// Async Functions proposal 2.1 steps 1, 3 (partially).
// In the spec it creates a function, but we create 2 functions `unwrapped` and
// `wrapped`.  `unwrapped` is a generator that corresponds to
//  the async function's body, replacing `await` with `yield`.  `wrapped` is a
// function that is visible to the outside, and handles yielded values.
JSObject*
js::WrapAsyncFunction(JSContext* cx, HandleFunction unwrapped)
{
    MOZ_ASSERT(unwrapped->isStarGenerator());

    // Create a new function with AsyncFunctionPrototype, reusing the name and
    // the length of `unwrapped`.

    // Step 1.
    RootedObject proto(cx, GlobalObject::getOrCreateAsyncFunctionPrototype(cx, cx->global()));
    if (!proto)
        return nullptr;

    RootedAtom funName(cx, unwrapped->name());
    uint16_t length;
    if (!unwrapped->getLength(cx, &length))
        return nullptr;

    // Steps 3 (partially).
    RootedFunction wrapped(cx, NewFunctionWithProto(cx, WrappedAsyncFunction, length,
                                                    JSFunction::NATIVE_FUN, nullptr,
                                                    funName, proto,
                                                    AllocKind::FUNCTION_EXTENDED,
                                                    TenuredObject));
    if (!wrapped)
        return nullptr;

    // Link them to each other to make GetWrappedAsyncFunction and
    // GetUnwrappedAsyncFunction work.
    unwrapped->setExtendedSlot(UNWRAPPED_ASYNC_WRAPPED_SLOT, ObjectValue(*wrapped));
    wrapped->setExtendedSlot(WRAPPED_ASYNC_UNWRAPPED_SLOT, ObjectValue(*unwrapped));

    return wrapped;
}

// Async Functions proposal 2.2 steps 3.f, 3.g.
static bool
AsyncFunctionThrown(JSContext* cx, MutableHandleValue rval)
{
    // Step 3.f.
    RootedValue exc(cx);
    if (!GetAndClearException(cx, &exc))
        return false;

    RootedObject rejectPromise(cx, PromiseObject::unforgeableReject(cx, exc));
    if (!rejectPromise)
        return false;

    // Step 3.g.
    rval.setObject(*rejectPromise);
    return true;
}

// Async Functions proposal 2.2 steps 3.d-e, 3.g.
static bool
AsyncFunctionReturned(JSContext* cx, HandleValue value, MutableHandleValue rval)
{
    // Steps 3.d-e.
    RootedObject resolveObj(cx, PromiseObject::unforgeableResolve(cx, value));
    if (!resolveObj)
        return false;

    // Step 3.g.
    rval.setObject(*resolveObj);
    return true;
}

static bool AsyncFunctionAwait(JSContext* cx, HandleValue generatorVal, HandleValue value,
                               MutableHandleValue rval);

enum class ResumeKind {
    Normal,
    Throw
};

// Async Functions proposal 2.2 steps 3-8, 2.4 steps 2-7, 2.5 steps 2-7.
static bool
AsyncFunctionResume(JSContext* cx, HandleValue generatorVal, ResumeKind kind,
                    HandleValue valueOrReason, MutableHandleValue rval)
{
    // Execution context switching is handled in generator.
    HandlePropertyName funName = kind == ResumeKind::Normal
                                 ? cx->names().StarGeneratorNext
                                 : cx->names().StarGeneratorThrow;
    FixedInvokeArgs<1> args(cx);
    args[0].set(valueOrReason);
    RootedValue result(cx);
    if (!CallSelfHostedFunction(cx, funName, generatorVal, args, &result))
        return AsyncFunctionThrown(cx, rval);

    RootedObject resultObj(cx, &result.toObject());
    RootedValue doneVal(cx);
    RootedValue value(cx);
    if (!GetProperty(cx, resultObj, resultObj, cx->names().done, &doneVal))
        return false;
    if (!GetProperty(cx, resultObj, resultObj, cx->names().value, &value))
        return false;

    if (doneVal.toBoolean())
        return AsyncFunctionReturned(cx, value, rval);

    return AsyncFunctionAwait(cx, generatorVal, value, rval);
}

// Async Functions proposal 2.2 steps 3-8.
static bool
AsyncFunctionStart(JSContext* cx, HandleValue generatorVal, MutableHandleValue rval)
{
    return AsyncFunctionResume(cx, generatorVal, ResumeKind::Normal, UndefinedHandleValue, rval);
}

#define AWAITED_FUNC_GENERATOR_SLOT 0

static bool AsyncFunctionAwaitedFulfilled(JSContext* cx, unsigned argc, Value* vp);
static bool AsyncFunctionAwaitedRejected(JSContext* cx, unsigned argc, Value* vp);

// Async Functions proposal 2.3 steps 1-8.
static bool
AsyncFunctionAwait(JSContext* cx, HandleValue generatorVal, HandleValue value,
                   MutableHandleValue rval)
{
    // Step 1 (implicit).

    // Steps 2-3.
    RootedObject resolveObj(cx, PromiseObject::unforgeableResolve(cx, value));
    if (!resolveObj)
        return false;

    Rooted<PromiseObject*> resolvePromise(cx, resolveObj.as<PromiseObject>());

    // Step 4.
    RootedAtom funName(cx, cx->names().empty);
    RootedFunction onFulfilled(cx, NewNativeFunction(cx, AsyncFunctionAwaitedFulfilled, 1,
                                                      funName, gc::AllocKind::FUNCTION_EXTENDED,
                                                      GenericObject));
    if (!onFulfilled)
        return false;

    // Step 5.
    RootedFunction onRejected(cx, NewNativeFunction(cx, AsyncFunctionAwaitedRejected, 1,
                                                    funName, gc::AllocKind::FUNCTION_EXTENDED,
                                                    GenericObject));
    if (!onRejected)
        return false;

    // Step 6.
    onFulfilled->setExtendedSlot(AWAITED_FUNC_GENERATOR_SLOT, generatorVal);
    onRejected->setExtendedSlot(AWAITED_FUNC_GENERATOR_SLOT, generatorVal);

    // Step 8.
    RootedValue onFulfilledVal(cx, ObjectValue(*onFulfilled));
    RootedValue onRejectedVal(cx, ObjectValue(*onRejected));
    RootedObject resultPromise(cx, OriginalPromiseThen(cx, resolvePromise, onFulfilledVal,
                                                       onRejectedVal));
    if (!resultPromise)
        return false;

    rval.setObject(*resultPromise);
    return true;
}

// Async Functions proposal 2.4.
static bool
AsyncFunctionAwaitedFulfilled(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);

    RootedFunction F(cx, &args.callee().as<JSFunction>());
    RootedValue value(cx, args[0]);

    // Step 1.
    RootedValue generatorVal(cx, F->getExtendedSlot(AWAITED_FUNC_GENERATOR_SLOT));

    // Steps 2-7.
    return AsyncFunctionResume(cx, generatorVal, ResumeKind::Normal, value, args.rval());
}

// Async Functions proposal 2.5.
static bool
AsyncFunctionAwaitedRejected(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);

    RootedFunction F(cx, &args.callee().as<JSFunction>());
    RootedValue reason(cx, args[0]);

    // Step 1.
    RootedValue generatorVal(cx, F->getExtendedSlot(AWAITED_FUNC_GENERATOR_SLOT));

    // Step 2-7.
    return AsyncFunctionResume(cx, generatorVal, ResumeKind::Throw, reason, args.rval());
}

JSFunction*
js::GetWrappedAsyncFunction(JSFunction* unwrapped)
{
    MOZ_ASSERT(unwrapped->isAsync());
    return &unwrapped->getExtendedSlot(UNWRAPPED_ASYNC_WRAPPED_SLOT).toObject().as<JSFunction>();
}

JSFunction*
js::GetUnwrappedAsyncFunction(JSFunction* wrapped)
{
    MOZ_ASSERT(IsWrappedAsyncFunction(wrapped));
    JSFunction* unwrapped = &wrapped->getExtendedSlot(WRAPPED_ASYNC_UNWRAPPED_SLOT).toObject().as<JSFunction>();
    MOZ_ASSERT(unwrapped->isAsync());
    return unwrapped;
}

bool
js::IsWrappedAsyncFunction(JSFunction* fun)
{
    return fun->maybeNative() == WrappedAsyncFunction;
}


/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/AsyncFunction.h"

#include "jscompartment.h"

#include "builtin/SelfHostingDefines.h"
#include "vm/GlobalObject.h"
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

JSFunction*
js::GetWrappedAsyncFunction(JSFunction* unwrapped)
{
    MOZ_ASSERT(unwrapped->isAsync());
    return &unwrapped->getExtendedSlot(ASYNC_WRAPPED_SLOT).toObject().as<JSFunction>();
}

JSFunction*
js::GetUnwrappedAsyncFunction(JSFunction* wrapper)
{
    JSFunction* unwrapped = &wrapper->getExtendedSlot(ASYNC_UNWRAPPED_SLOT).toObject().as<JSFunction>();
    MOZ_ASSERT(unwrapped->isAsync());
    return unwrapped;
}

bool
js::IsWrappedAsyncFunction(JSContext* cx, JSFunction* wrapper)
{
    return IsSelfHostedFunctionWithName(wrapper, cx->names().AsyncWrapped);
}

bool
js::CreateAsyncFunction(JSContext* cx, HandleFunction wrapper, HandleFunction unwrapped,
                        MutableHandleFunction result)
{
    // Create a new function with AsyncFunctionPrototype, reusing the script
    // and the environment of `wrapper` function, and the name and the length
    // of `unwrapped` function.
    RootedObject proto(cx, GlobalObject::getOrCreateAsyncFunctionPrototype(cx, cx->global()));
    RootedObject scope(cx, wrapper->environment());
    RootedAtom atom(cx, unwrapped->name());
    RootedFunction wrapped(cx, NewFunctionWithProto(cx, nullptr, 0,
                                                    JSFunction::INTERPRETED_LAMBDA,
                                                    scope, atom, proto,
                                                    AllocKind::FUNCTION_EXTENDED, TenuredObject));
    if (!wrapped)
        return false;

    wrapped->initScript(wrapper->nonLazyScript());

    // Link them each other to make GetWrappedAsyncFunction and
    // GetUnwrappedAsyncFunction work.
    unwrapped->setExtendedSlot(ASYNC_WRAPPED_SLOT, ObjectValue(*wrapped));
    wrapped->setExtendedSlot(ASYNC_UNWRAPPED_SLOT, ObjectValue(*unwrapped));

    // The script of `wrapper` is self-hosted, so `wrapped` should also be
    // set as self-hosted function.
    wrapped->setIsSelfHostedBuiltin();

    // Set LAZY_FUNCTION_NAME_SLOT to "AsyncWrapped" to make it detectable in
    // IsWrappedAsyncFunction.
    wrapped->setExtendedSlot(LAZY_FUNCTION_NAME_SLOT, StringValue(cx->names().AsyncWrapped));

    // The length of the script of `wrapper` is different than the length of
    // `unwrapped`.  We should set actual length as resolved length, to avoid
    // using the length of the script.
    uint16_t length;
    if (!unwrapped->getLength(cx, &length))
        return false;

    RootedValue lengthValue(cx, NumberValue(length));
    if (!DefineProperty(cx, wrapped, cx->names().length, lengthValue,
                        nullptr, nullptr, JSPROP_READONLY))
    {
        return false;
    }

    result.set(wrapped);
    return true;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/Promise.h"

#include "jscntxt.h"

#include "jsobjinlines.h"

using namespace js;

static const JSFunctionSpec promise_methods[] = {
    JS_SELF_HOSTED_FN("catch", "Promise_catch", 1, 0),
    JS_SELF_HOSTED_FN("then", "Promise_then", 2, 0),
    JS_FS_END
};

static const JSFunctionSpec promise_static_methods[] = {
    JS_SELF_HOSTED_FN("all", "Promise_all", 1, 0),
    JS_SELF_HOSTED_FN("race", "Promise_race", 1, 0),
    JS_SELF_HOSTED_FN("reject", "Promise_reject", 1, 0),
    JS_SELF_HOSTED_FN("resolve", "Promise_resolve", 1, 0),
    JS_FS_END
};

static bool
PromiseConstructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject* obj = NewBuiltinClassInstance(cx, &ShellPromiseObject::class_);
    if (!obj)
        return false;
    // TODO: store the resolve and reject callbacks.
    args.rval().setObject(*obj);
    return true;
}

static JSObject*
CreatePromisePrototype(JSContext* cx, JSProtoKey key)
{
    return cx->global()->createBlankPrototype(cx, &ShellPromiseObject::protoClass_);
}

const Class ShellPromiseObject::class_ = {
    "ShellPromise",
    JSCLASS_HAS_CACHED_PROTO(JSProto_ShellPromise),
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    nullptr, /* finalize */
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    nullptr, /* trace */
    {
        GenericCreateConstructor<PromiseConstructor, 2, gc::AllocKind::FUNCTION>,
        CreatePromisePrototype,
        promise_static_methods,
        nullptr,
        promise_methods
    }
};

const Class ShellPromiseObject::protoClass_ = {
    "ShellPromise",
    JSCLASS_HAS_CACHED_PROTO(JSProto_ShellPromise),
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    nullptr, /* finalize */
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    nullptr, /* trace */
    {
        DELEGATED_CLASSSPEC(&ShellPromiseObject::class_.spec),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        ClassSpec::IsDelegated
    }
};

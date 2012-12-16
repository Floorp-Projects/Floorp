/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsinterp.h"
#include "jsnum.h"
#include "jsobj.h"

#include "gc/Marking.h"

#include "jsfuninlines.h"

#include "selfhosted.out.h"

using namespace js;

static void
selfHosting_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    PrintError(cx, stderr, message, report, true);
}

static JSClass self_hosting_global_class = {
    "self-hosting-global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_PropertyStub,
    JS_PropertyStub,  JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub,
    JS_ConvertStub,   NULL
};

static JSBool
intrinsic_ToObject(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedValue val(cx, args[0]);
    RootedObject obj(cx, ToObject(cx, val));
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

static JSBool
intrinsic_ToInteger(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    double result;
    if (!ToInteger(cx, args[0], &result))
        return false;
    args.rval().setDouble(result);
    return true;
}

static JSBool
intrinsic_IsCallable(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Value val = args[0];
    bool isCallable = val.isObject() && val.toObject().isCallable();
    args.rval().setBoolean(isCallable);
    return true;
}

static JSBool
intrinsic_ThrowError(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() >= 1);
    uint32_t errorNumber = args[0].toInt32();

    char *errorArgs[3] = {NULL, NULL, NULL};
    for (unsigned i = 1; i < 4 && i < args.length(); i++) {
        RootedValue val(cx, args[i]);
        if (val.isInt32()) {
            JSString *str = ToString(cx, val);
            if (!str)
                return false;
            errorArgs[i - 1] = JS_EncodeString(cx, str);
        } else if (val.isString()) {
            errorArgs[i - 1] = JS_EncodeString(cx, ToString(cx, val));
        } else {
            errorArgs[i - 1] = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, val, NullPtr());
        }
        if (!errorArgs[i - 1])
            return false;
    }

    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, errorNumber,
                         errorArgs[0], errorArgs[1], errorArgs[2]);
    for (unsigned i = 0; i < 3; i++)
        js_free(errorArgs[i]);
    return false;
}

/*
 * Used to decompile values in the nearest non-builtin stack frame, falling
 * back to decompiling in the current frame. Helpful for printing higher-order
 * function arguments.
 *
 * The user must supply the argument number of the value in question; it
 * _cannot_ be automatically determined.
 */
static JSBool
intrinsic_DecompileArg(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 2);

    RootedValue value(cx, args[1]);
    ScopedFreePtr<char> str(DecompileArgument(cx, args[0].toInt32(), value));
    if (!str)
        return false;
    RootedAtom atom(cx, Atomize(cx, str, strlen(str)));
    if (!atom)
        return false;
    args.rval().setString(atom);
    return true;
}

static JSBool
intrinsic_MakeConstructible(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() >= 1);
    JS_ASSERT(args[0].isObject());
    RootedObject obj(cx, &args[0].toObject());
    JS_ASSERT(obj->isFunction());
    obj->toFunction()->setIsSelfHostedConstructor();
    return true;
}

JSFunctionSpec intrinsic_functions[] = {
    JS_FN("ToObject",           intrinsic_ToObject,             1,0),
    JS_FN("ToInteger",          intrinsic_ToInteger,            1,0),
    JS_FN("IsCallable",         intrinsic_IsCallable,           1,0),
    JS_FN("ThrowError",         intrinsic_ThrowError,           4,0),
    JS_FN("MakeConstructible",  intrinsic_MakeConstructible,    1,0),
    JS_FN("DecompileArg",       intrinsic_DecompileArg,         2,0),
    JS_FS_END
};
bool
JSRuntime::initSelfHosting(JSContext *cx)
{
    JS_ASSERT(!selfHostingGlobal_);
    RootedObject savedGlobal(cx, JS_GetGlobalObject(cx));
    if (!(selfHostingGlobal_ = JS_NewGlobalObject(cx, &self_hosting_global_class, NULL)))
        return false;
    JS_SetGlobalObject(cx, selfHostingGlobal_);
    JSAutoCompartment ac(cx, cx->global());
    RootedObject shg(cx, selfHostingGlobal_);

    if (!JS_DefineFunctions(cx, shg, intrinsic_functions))
        return false;

    CompileOptions options(cx);
    options.setFileAndLine("self-hosted", 1);
    options.setSelfHostingMode(true);

    /*
     * Set a temporary error reporter printing to stderr because it is too
     * early in the startup process for any other reporter to be registered
     * and we don't want errors in self-hosted code to be silently swallowed.
     */
    JSErrorReporter oldReporter = JS_SetErrorReporter(cx, selfHosting_ErrorReporter);
    Value rv;
    bool ok = false;

    char *filename = getenv("MOZ_SELFHOSTEDJS");
    if (filename) {
        RootedScript script(cx, Compile(cx, shg, options, filename));
        if (script)
            ok = Execute(cx, script, *shg.get(), &rv);
    } else {
        const char *src = selfhosted::raw_sources;
        uint32_t srcLen = selfhosted::GetRawScriptsSize();
        ok = Evaluate(cx, shg, options, src, srcLen, &rv);
    }
    JS_SetErrorReporter(cx, oldReporter);
    JS_SetGlobalObject(cx, savedGlobal);
    return ok;
}

void
JSRuntime::markSelfHostingGlobal(JSTracer *trc)
{
    MarkObjectRoot(trc, &selfHostingGlobal_, "self-hosting global");
}

bool
JSRuntime::getUnclonedSelfHostedValue(JSContext *cx, Handle<PropertyName*> name,
                                      MutableHandleValue vp)
{
    RootedObject shg(cx, selfHostingGlobal_);
    AutoCompartment ac(cx, shg);
    return JS_GetPropertyById(cx, shg, NameToId(name), vp.address());
}

bool
JSRuntime::cloneSelfHostedFunctionScript(JSContext *cx, Handle<PropertyName*> name,
                                         Handle<JSFunction*> targetFun)
{
    RootedValue funVal(cx);
    if (!getUnclonedSelfHostedValue(cx, name, &funVal))
        return false;

    RootedFunction sourceFun(cx, funVal.toObject().toFunction());
    Rooted<JSScript*> sourceScript(cx, sourceFun->nonLazyScript());
    JS_ASSERT(!sourceScript->enclosingStaticScope());
    RawScript cscript = CloneScript(cx, NullPtr(), targetFun, sourceScript);
    if (!cscript)
        return false;
    targetFun->setScript(cscript);
    cscript->setFunction(targetFun);
    JS_ASSERT(sourceFun->nargs == targetFun->nargs);
    targetFun->flags = sourceFun->flags | JSFunction::EXTENDED;
    return true;
}

bool
JSRuntime::cloneSelfHostedValue(JSContext *cx, Handle<PropertyName*> name, MutableHandleValue vp)
{
    RootedValue funVal(cx);
    if (!getUnclonedSelfHostedValue(cx, name, &funVal))
        return false;

    /*
     * We don't clone if we're operating in the self-hosting global, as that
     * means we're currently executing the self-hosting script while
     * initializing the runtime (see JSRuntime::initSelfHosting).
     */
    if (cx->global() == selfHostingGlobal_) {
        vp.set(funVal);
    } else if (funVal.isObject() && funVal.toObject().isFunction()) {
        RootedFunction fun(cx, funVal.toObject().toFunction());
        RootedObject clone(cx, CloneFunctionObject(cx, fun, cx->global(), fun->getAllocKind()));
        if (!clone)
            return false;
        vp.setObject(*clone);
    } else {
        vp.setUndefined();
    }
    return true;
}

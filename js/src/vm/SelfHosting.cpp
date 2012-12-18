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
#include "jstypedarrayinlines.h"

#include "vm/BooleanObject-inl.h"
#include "vm/NumberObject-inl.h"
#include "vm/RegExpObject-inl.h"
#include "vm/StringObject-inl.h"

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

/**
 * Handles an assertion failure in self-hosted code just like an assertion
 * failure in C++ code. Information about the failure can be provided in args[0].
 */
static JSBool
intrinsic_AssertionFailed(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
#ifdef DEBUG
    if (argc > 0) {
        // try to dump the informative string
        JSString *str = ToString(cx, args[0]);
        if (str) {
            const jschar *chars = str->getChars(cx);
            if (chars) {
                fprintf(stderr, "Self-hosted JavaScript assertion info: ");
                JSString::dumpChars(chars, str->length());
                fputc('\n', stderr);
            }
        }
    }
#endif
    JS_ASSERT(false);
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
    JS_ASSERT(args[0].toObject().isFunction());
    args[0].toObject().toFunction()->setIsSelfHostedConstructor();
    return true;
}

JSFunctionSpec intrinsic_functions[] = {
    JS_FN("ToObject",           intrinsic_ToObject,             1,0),
    JS_FN("ToInteger",          intrinsic_ToInteger,            1,0),
    JS_FN("IsCallable",         intrinsic_IsCallable,           1,0),
    JS_FN("ThrowError",         intrinsic_ThrowError,           4,0),
    JS_FN("AssertionFailed",    intrinsic_AssertionFailed,      1,0),
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
    Rooted<GlobalObject*> shg(cx, &selfHostingGlobal_->asGlobal());
    /*
     * During initialization of standard classes for the self-hosting global,
     * all self-hosted functions are ignored. Thus, we don't create cyclic
     * dependencies in the order of initialization.
     */
    if (!GlobalObject::initStandardClasses(cx, shg))
        return false;

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

typedef AutoObjectObjectHashMap CloneMemory;
static bool CloneValue(JSContext *cx, MutableHandleValue vp, CloneMemory &clonedObjects);

static bool
GetUnclonedValue(JSContext *cx, Handle<JSObject*> src, HandleId id, MutableHandleValue vp)
{
    AutoCompartment ac(cx, src);
    return JSObject::getGeneric(cx, src, src, id, vp);
}

static bool
CloneProperties(JSContext *cx, HandleObject obj, HandleObject clone, CloneMemory &clonedObjects)
{
    RootedId id(cx);
    RootedValue val(cx);
    AutoIdVector ids(cx);
    {
        AutoCompartment ac(cx, obj);
        if (!GetPropertyNames(cx, obj, JSITER_OWNONLY, &ids))
            return false;
    }
    for (uint32_t i = 0; i < ids.length(); i++) {
        id = ids[i];
        if (!GetUnclonedValue(cx, obj, id, &val) ||
            !CloneValue(cx, &val, clonedObjects) ||
            !JSObject::setGeneric(cx, clone, clone, id, &val, false))
        {
            return false;
        }
    }
    return true;
}
static RawObject
CloneDenseArray(JSContext *cx, HandleObject obj, CloneMemory &clonedObjects)
{
    uint32_t len = obj->getArrayLength();
    RootedObject clone(cx, NewDenseAllocatedArray(cx, len));
    clone->setDenseArrayInitializedLength(len);
    for (uint32_t i = 0; i < len; i++)
        JSObject::initDenseArrayElementWithType(cx, clone, i, UndefinedValue());
    RootedValue elt(cx);
    for (uint32_t i = 0; i < len; i++) {
        bool present;
        if (!obj->getElementIfPresent(cx, obj, obj, i, &elt, &present))
            return NULL;
        if (present) {
            if (!CloneValue(cx, &elt, clonedObjects))
                return NULL;
            JSObject::setDenseArrayElementWithType(cx, clone, i, elt);
        }
    }
    return clone;
}
static RawObject
CloneObject(JSContext *cx, HandleObject srcObj, CloneMemory &clonedObjects)
{
    CloneMemory::AddPtr p = clonedObjects.lookupForAdd(srcObj.get());
    if (p)
        return p->value;
    RootedObject clone(cx);
    if (srcObj->isFunction()) {
        RootedFunction fun(cx, srcObj->toFunction());
        clone = CloneFunctionObject(cx, fun, cx->global(), fun->getAllocKind());
    } else if (srcObj->isRegExp()) {
        RegExpObject &reobj = srcObj->asRegExp();
        RootedAtom source(cx, reobj.getSource());
        clone = RegExpObject::createNoStatics(cx, source, reobj.getFlags(), NULL);
    } else if (srcObj->isDate()) {
        clone = JS_NewDateObjectMsec(cx, srcObj->getDateUTCTime().toNumber());
    } else if (srcObj->isBoolean()) {
        clone = BooleanObject::create(cx, srcObj->asBoolean().unbox());
    } else if (srcObj->isNumber()) {
        clone = NumberObject::create(cx, srcObj->asNumber().unbox());
    } else if (srcObj->isString()) {
        Rooted<JSStableString*> str(cx, srcObj->asString().unbox()->ensureStable(cx));
        if (!str)
            return NULL;
        str = js_NewStringCopyN(cx, str->chars().get(), str->length())->ensureStable(cx);
        if (!str)
            return NULL;
        clone = StringObject::create(cx, str);
    } else if (srcObj->isDenseArray()) {
        return CloneDenseArray(cx, srcObj, clonedObjects);
    } else {
        if (srcObj->isArray()) {
            clone = NewDenseEmptyArray(cx);
        } else {
            JS_ASSERT(srcObj->isNative());
            clone = NewObjectWithClassProto(cx, srcObj->getClass(), NULL, cx->global(),
                                            srcObj->getAllocKind());
        }
    }
    if (!clone || !clonedObjects.relookupOrAdd(p, srcObj.get(), clone.get()) ||
        !CloneProperties(cx, srcObj, clone, clonedObjects))
    {
        return NULL;
    }
    return clone;
}

static bool
CloneValue(JSContext *cx, MutableHandleValue vp, CloneMemory &clonedObjects)
{
    if (vp.isObject()) {
        RootedObject obj(cx, &vp.toObject());
        RootedObject clone(cx, CloneObject(cx, obj, clonedObjects));
        if (!clone)
            return false;
        vp.setObject(*clone);
    } else if (vp.isBoolean() || vp.isNumber() || vp.isNullOrUndefined()) {
        // Nothing to do here: these are represented inline in the value
    } else if (vp.isString()) {
        Rooted<JSStableString*> str(cx, vp.toString()->ensureStable(cx));
        if (!str)
            return false;
        RootedString clone(cx, js_NewStringCopyN(cx, str->chars().get(), str->length()));
        if (!clone)
            return false;
        vp.setString(clone);
    } else {
        if (JSString *valSrc = JS_ValueToSource(cx, vp))
            printf("Error: Can't yet clone value: %s\n", JS_EncodeString(cx, valSrc));
        return false;
    }
    return true;
}

bool
JSRuntime::cloneSelfHostedFunctionScript(JSContext *cx, Handle<PropertyName*> name,
                                         Handle<JSFunction*> targetFun)
{
    RootedObject shg(cx, selfHostingGlobal_);
    RootedValue funVal(cx);
    RootedId id(cx, NameToId(name));
    if (!GetUnclonedValue(cx, shg, id, &funVal))
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
    RootedObject shg(cx, selfHostingGlobal_);
    RootedValue val(cx);
    RootedId id(cx, NameToId(name));
    if (!GetUnclonedValue(cx, shg, id, &val))
         return false;

    /*
     * We don't clone if we're operating in the self-hosting global, as that
     * means we're currently executing the self-hosting script while
     * initializing the runtime (see JSRuntime::initSelfHosting).
     */
    if (cx->global() != selfHostingGlobal_) {
        CloneMemory clonedObjects(cx);
        if (!clonedObjects.init() || !CloneValue(cx, &val, clonedObjects))
            return false;
    }
    vp.set(val);
    return true;
}

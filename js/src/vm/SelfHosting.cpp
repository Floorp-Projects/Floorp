/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsdate.h"
#include "jsinterp.h"
#include "jsnum.h"
#include "jsobj.h"

#include "builtin/Intl.h"
#include "builtin/ParallelArray.h"
#include "gc/Marking.h"
#include "vm/ForkJoin.h"
#include "vm/ParallelDo.h"
#include "vm/ThreadPool.h"

#include "jsfuninlines.h"
#include "jstypedarrayinlines.h"

#include "vm/BooleanObject-inl.h"
#include "vm/NumberObject-inl.h"
#include "vm/RegExpObject-inl.h"
#include "vm/StringObject-inl.h"

#include "selfhosted.out.h"

using namespace js;
using namespace js::selfhosted;

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

JSBool
js::intrinsic_ThrowError(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() >= 1);
    uint32_t errorNumber = args[0].toInt32();

    char *errorArgs[3] = {NULL, NULL, NULL};
    for (unsigned i = 1; i < 4 && i < args.length(); i++) {
        RootedValue val(cx, args[i]);
        if (val.isInt32()) {
            JSString *str = ToString<CanGC>(cx, val);
            if (!str)
                return false;
            errorArgs[i - 1] = JS_EncodeString(cx, str);
        } else if (val.isString()) {
            errorArgs[i - 1] = JS_EncodeString(cx, ToString<CanGC>(cx, val));
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
#ifdef DEBUG
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() > 0) {
        // try to dump the informative string
        JSString *str = ToString<CanGC>(cx, args[0]);
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
    ScopedJSFreePtr<char> str(DecompileArgument(cx, args[0].toInt32(), value));
    if (!str)
        return false;
    RootedAtom atom(cx, Atomize(cx, str, strlen(str)));
    if (!atom)
        return false;
    args.rval().setString(atom);
    return true;
}

/*
 * SetScriptHints(fun, flags): Sets various internal hints to the ion
 * compiler for use when compiling |fun| or calls to |fun|.  Flags
 * should be a dictionary object.
 *
 * The function |fun| should be a self-hosted function (in particular,
 * it *must* be a JS function).
 *
 * Possible flags:
 * - |cloneAtCallsite: true| will hint that |fun| should be cloned
 *   each callsite to improve TI resolution.  This is important for
 *   higher-order functions like |Array.map|.
 */
static JSBool
intrinsic_SetScriptHints(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() >= 2);
    JS_ASSERT(args[0].isObject() && args[0].toObject().isFunction());
    JS_ASSERT(args[1].isObject());

    RootedFunction fun(cx, args[0].toObject().toFunction());
    RootedScript funScript(cx, fun->nonLazyScript());
    RootedObject flags(cx, &args[1].toObject());

    RootedId id(cx);
    RootedValue propv(cx);

    id = AtomToId(Atomize(cx, "cloneAtCallsite", strlen("cloneAtCallsite")));
    if (!JSObject::getGeneric(cx, flags, flags, id, &propv))
        return false;
    if (ToBoolean(propv))
        funScript->shouldCloneAtCallsite = true;

    return true;
}

#ifdef DEBUG
/*
 * Dump(val): Dumps a value for debugging, even in parallel mode.
 */
JSBool
js::intrinsic_Dump(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedValue val(cx, args[0]);
    js_DumpValue(val);
    fprintf(stderr, "\n");
    args.rval().setUndefined();
    return true;
}
#endif

/*
 * ParallelDo(func, feedback): Invokes |func| many times in parallel.
 *
 * Executed based on the fork join pool described in vm/ForkJoin.h.
 * If func() has not been compiled for parallel execution, it will
 * first be invoked various times sequentially as a warmup phase,
 * which is used to gather TI information and to determine which
 * functions func() will invoke.
 *
 * func() should expect the following arguments:
 *
 *     func(id, n, warmup, args...)
 *
 * Here, |id| is the slice id. |n| is the total number of slices;
 * |warmup| is true if this is a warmup or recovery phase.
 * Typically, if |warmup| is true, you will want to do less work.
 *
 * The |feedback| argument is optional.  If provided, it should be a
 * closure.  This closure will be invoked with a double argument
 * representing the number of bailouts that occurred before a
 * successful parallel execution.  If the number is infinity, then
 * parallel execution was abandoned and |func| was simply invoked
 * sequentially.
 *
 * See ParallelArray.js for examples.
 */
static JSBool
intrinsic_ParallelDo(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return parallel::Do(cx, args);
}

/*
 * ParallelSlices(): Returns the number of parallel slices that will
 * be created by ParallelDo().
 */
static JSBool
intrinsic_ParallelSlices(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setInt32(ForkJoinSlices(cx));
    return true;
}

/*
 * NewParallelArray(init, ...args): Creates a new parallel array using
 * an initialization function |init|. All subsequent arguments are
 * passed to |init|. The new instance will be passed as the |this|
 * argument.
 */
JSBool
js::intrinsic_NewParallelArray(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JS_ASSERT(args[0].isObject() && args[0].toObject().isFunction());

    RootedFunction init(cx, args[0].toObject().toFunction());
    CallArgs args0 = CallArgsFromVp(argc - 1, vp + 1);
    if (!js::ParallelArrayObject::constructHelper(cx, &init, args0))
        return false;
    args.rval().set(args0.rval());
    return true;
}

/*
 * NewDenseArray(length): Allocates and returns a new dense array with
 * the given length where all values are initialized to holes.
 */
JSBool
js::intrinsic_NewDenseArray(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Check that index is an int32
    if (!args[0].isInt32()) {
        JS_ReportError(cx, "Expected int32 as second argument");
        return false;
    }
    uint32_t length = args[0].toInt32();

    // Make a new buffer and initialize it up to length.
    RootedObject buffer(cx, NewDenseAllocatedArray(cx, length));
    if (!buffer)
        return false;

    types::TypeObject *newtype = types::GetTypeCallerInitObject(cx, JSProto_Array);
    if (!newtype)
        return false;
    buffer->setType(newtype);

    JSObject::EnsureDenseResult edr = buffer->ensureDenseElements(cx, length, 0);
    switch (edr) {
      case JSObject::ED_OK:
        args.rval().setObject(*buffer);
        return true;

      case JSObject::ED_SPARSE: // shouldn't happen!
        JS_ASSERT(!"%EnsureDenseArrayElements() would yield sparse array");
        JS_ReportError(cx, "%EnsureDenseArrayElements() would yield sparse array");
        break;

      case JSObject::ED_FAILED:
        break;
    }
    return false;
}

/*
 * UnsafeSetElement(arr0, idx0, elem0, ..., arrN, idxN, elemN): For
 * each set of (arr, idx, elem) arguments that are passed, performs
 * the assignment |arr[idx] = elem|. |arr| must be either a dense array
 * or a typed array.
 *
 * If |arr| is a dense array, the index must be an int32 less than the
 * initialized length of |arr|. Use |%EnsureDenseResultArrayElements|
 * to ensure that the initialized length is long enough.
 *
 * If |arr| is a typed array, the index must be an int32 less than the
 * length of |arr|.
 */
JSBool
js::intrinsic_UnsafeSetElement(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if ((args.length() % 3) != 0) {
        JS_ReportError(cx, "Incorrect number of arguments, not divisible by 3");
        return false;
    }

    for (uint32_t base = 0; base < args.length(); base += 3) {
        uint32_t arri = base;
        uint32_t idxi = base+1;
        uint32_t elemi = base+2;

        JS_ASSERT(args[arri].isObject());
        JS_ASSERT(args[arri].toObject().isNative() ||
                  args[arri].toObject().isTypedArray());
        JS_ASSERT(args[idxi].isInt32());

        RootedObject arrobj(cx, &args[arri].toObject());
        uint32_t idx = args[idxi].toInt32();

        if (arrobj->isNative()) {
            JS_ASSERT(idx < arrobj->getDenseInitializedLength());
            JSObject::setDenseElementWithType(cx, arrobj, idx, args[elemi]);
        } else {
            JS_ASSERT(idx < TypedArray::length(arrobj));
            RootedValue tmp(cx, args[elemi]);
            // XXX: Always non-strict.
            if (!JSObject::setElement(cx, arrobj, arrobj, idx, &tmp, false))
                return false;
        }
    }

    args.rval().setUndefined();
    return true;
}

/*
 * ParallelTestsShouldPass(): Returns false if we are running in a
 * mode (such as --ion-eager) that is known to cause additional
 * bailouts or disqualifications for parallel array tests.
 *
 * This is needed because the parallel tests generally assert that,
 * under normal conditions, they will run without bailouts or
 * compilation failures, but this does not hold under "stress-testing"
 * conditions like --ion-eager or --no-ti.  However, running the tests
 * under those conditions HAS exposed bugs and thus we do not wish to
 * disable them entirely.  Instead, we simply disable the assertions
 * that state that no bailouts etc should occur.
 */
static JSBool
intrinsic_ParallelTestsShouldPass(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
#if defined(JS_THREADSAFE) && defined(JS_ION)
    args.rval().setBoolean(ion::IsEnabled(cx) &&
                           !ion::js_IonOptions.eagerCompilation);
#else
    args.rval().setBoolean(false);
#endif
    return true;
}

/*
 * ShouldForceSequential(): Returns true if parallel ops should take
 * the sequential fallback path.
 */
JSBool
js::intrinsic_ShouldForceSequential(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
#ifdef JS_THREADSAFE
    args.rval().setBoolean(cx->runtime->parallelWarmup ||
                           InParallelSection());
#else
    args.rval().setBoolean(true);
#endif
    return true;
}

/**
 * Returns the default locale as a well-formed, but not necessarily canonicalized,
 * BCP-47 language tag.
 */
static JSBool
intrinsic_RuntimeDefaultLocale(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    const char *locale = cx->runtime->getDefaultLocale();
    if (!locale) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEFAULT_LOCALE_ERROR);
        return false;
    }

    RootedString jslocale(cx, JS_NewStringCopyZ(cx, locale));
    if (!jslocale)
        return false;

    args.rval().setString(jslocale);
    return true;
}

JSFunctionSpec intrinsic_functions[] = {
    JS_FN("ToObject",             intrinsic_ToObject,             1,0),
    JS_FN("ToInteger",            intrinsic_ToInteger,            1,0),
    JS_FN("IsCallable",           intrinsic_IsCallable,           1,0),
    JS_FN("ThrowError",           intrinsic_ThrowError,           4,0),
    JS_FN("AssertionFailed",      intrinsic_AssertionFailed,      1,0),
    JS_FN("SetScriptHints",       intrinsic_SetScriptHints,       2,0),
    JS_FN("MakeConstructible",    intrinsic_MakeConstructible,    1,0),
    JS_FN("DecompileArg",         intrinsic_DecompileArg,         2,0),
    JS_FN("RuntimeDefaultLocale", intrinsic_RuntimeDefaultLocale, 0,0),

    JS_FN("ParallelDo",           intrinsic_ParallelDo,           2,0),
    JS_FN("ParallelSlices",       intrinsic_ParallelSlices,       0,0),
    JS_FN("NewParallelArray",     intrinsic_NewParallelArray,     3,0),
    JS_FN("NewDenseArray",        intrinsic_NewDenseArray,        1,0),
    JS_FN("UnsafeSetElement",     intrinsic_UnsafeSetElement,     3,0),
    JS_FN("ShouldForceSequential", intrinsic_ShouldForceSequential, 0,0),
    JS_FN("ParallelTestsShouldPass", intrinsic_ParallelTestsShouldPass, 0,0),

    // See jsdate.h for descriptions of the date_* functions.
    JS_FN("date_CheckThisDate", date_CheckThisDate, 2,0),

    // See builtin/Intl.h for descriptions of the intl_* functions.
    JS_FN("intl_availableCalendars", intl_availableCalendars, 1,0),
    JS_FN("intl_availableCollations", intl_availableCollations, 1,0),
    JS_FN("intl_Collator", intl_Collator, 2,0),
    JS_FN("intl_Collator_availableLocales", intl_Collator_availableLocales, 0,0),
    JS_FN("intl_CompareStrings", intl_CompareStrings, 3,0),
    JS_FN("intl_DateTimeFormat", intl_DateTimeFormat, 2,0),
    JS_FN("intl_DateTimeFormat_availableLocales", intl_DateTimeFormat_availableLocales, 0,0),
    JS_FN("intl_FormatDateTime", intl_FormatDateTime, 2,0),
    JS_FN("intl_FormatNumber", intl_FormatNumber, 2,0),
    JS_FN("intl_NumberFormat", intl_NumberFormat, 2,0),
    JS_FN("intl_NumberFormat_availableLocales", intl_NumberFormat_availableLocales, 0,0),
    JS_FN("intl_numberingSystem", intl_numberingSystem, 1,0),
    JS_FN("intl_patternForSkeleton", intl_patternForSkeleton, 2,0),

    // See jsnum.h for descriptions of the num_* functions.
    JS_FN("num_CheckThisNumber", num_CheckThisNumber, 2,0),

#ifdef DEBUG
    JS_FN("Dump",                 intrinsic_Dump,                 1,0),
#endif

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
        uint32_t srcLen = GetRawScriptsSize();

#ifdef USE_ZLIB
        const unsigned char *compressed = compressedSources;
        uint32_t compressedLen = GetCompressedSize();
        ScopedJSFreePtr<char> src(reinterpret_cast<char *>(cx->malloc_(srcLen)));
        if (!src || !DecompressString(compressed, compressedLen,
                                      reinterpret_cast<unsigned char *>(src.get()), srcLen))
        {
            return false;
        }
#else
        const char *src = rawSources;
#endif

        ok = Evaluate(cx, shg, options, src, srcLen, &rv);
    }
    JS_SetErrorReporter(cx, oldReporter);
    JS_SetGlobalObject(cx, savedGlobal);
    return ok;
}

void
JSRuntime::finishSelfHosting()
{
    selfHostingGlobal_ = NULL;
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
            !JS_DefinePropertyById(cx, clone, id, val.get(), NULL, NULL, 0))
        {
            return false;
        }
    }
    return true;
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
        str = js_NewStringCopyN<CanGC>(cx, str->chars().get(), str->length())->ensureStable(cx);
        if (!str)
            return NULL;
        clone = StringObject::create(cx, str);
    } else if (srcObj->isArray()) {
        clone = NewDenseEmptyArray(cx);
    } else {
        JS_ASSERT(srcObj->isNative());
        clone = NewObjectWithClassProto(cx, srcObj->getClass(), NULL, cx->global(),
                                        srcObj->getAllocKind());
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
        RootedString clone(cx, js_NewStringCopyN<CanGC>(cx, str->chars().get(), str->length()));
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

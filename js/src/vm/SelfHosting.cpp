/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsfriendapi.h"
#include "jsobj.h"
#include "selfhosted.out.h"

#include "builtin/Intl.h"
#include "builtin/ParallelArray.h"
#include "gc/Marking.h"
#include "vm/ForkJoin.h"
#include "vm/Interpreter.h"

#include "jsfuninlines.h"
#include "jsscriptinlines.h"

#include "vm/BooleanObject-inl.h"
#include "vm/NumberObject-inl.h"
#include "vm/StringObject-inl.h"

using namespace js;
using namespace js::selfhosted;

namespace js {

/*
 * A linked-list container for self-hosted prototypes that have need of a
 * Class for reserved slots. These are freed when self-hosting is destroyed at
 * the destruction of the last context.
 */
struct SelfHostedClass
{
    /* Next class in the list. */
    SelfHostedClass *next;

    /* Class of instances. */
    Class class_;

    /*
     * Create a new self-hosted proto with its class set to a new dynamically
     * allocated class with numSlots reserved slots.
     */
    static JSObject *newPrototype(JSContext *cx, uint32_t numSlots);

    static bool is(JSContext *cx, Class *clasp);

    SelfHostedClass(const char *name, uint32_t numSlots);
};

} /* namespace js */

JSObject *
SelfHostedClass::newPrototype(JSContext *cx, uint32_t numSlots)
{
    /* Allocate a new self hosted class and prepend it to the list. */
    SelfHostedClass *shClass = cx->new_<SelfHostedClass>("Self-hosted Class", numSlots);
    if (!shClass)
        return NULL;
    cx->runtime()->addSelfHostedClass(shClass);

    Rooted<GlobalObject *> global(cx, cx->global());
    RootedObject proto(cx, global->createBlankPrototype(cx, &shClass->class_));
    if (!proto)
        return NULL;

    return proto;
}

bool
SelfHostedClass::is(JSContext *cx, Class *clasp)
{
    SelfHostedClass *shClass = cx->runtime()->selfHostedClasses();
    while (shClass) {
        if (clasp == &shClass->class_)
            return true;
        shClass = shClass->next;
    }
    return false;
}

SelfHostedClass::SelfHostedClass(const char *name, uint32_t numSlots)
{
    mozilla::PodZero(this);

    class_.name = name;
    class_.flags = JSCLASS_HAS_RESERVED_SLOTS(numSlots);
    class_.addProperty = JS_PropertyStub;
    class_.delProperty = JS_DeletePropertyStub;
    class_.getProperty = JS_PropertyStub;
    class_.setProperty = JS_StrictPropertyStub;
    class_.enumerate = JS_EnumerateStub;
    class_.resolve = JS_ResolveStub;
    class_.convert = JS_ConvertStub;
}

static void
selfHosting_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    PrintError(cx, stderr, message, report, true);
}

static JSClass self_hosting_global_class = {
    "self-hosting-global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_DeletePropertyStub,
    JS_PropertyStub,  JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub,
    JS_ConvertStub,   NULL
};

bool
js::intrinsic_ToObject(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedValue val(cx, args[0]);
    RootedObject obj(cx, ToObject(cx, val));
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

static bool
intrinsic_ToInteger(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    double result;
    if (!ToInteger(cx, args[0], &result))
        return false;
    args.rval().setDouble(result);
    return true;
}

bool
js::intrinsic_IsCallable(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Value val = args[0];
    bool isCallable = val.isObject() && val.toObject().isCallable();
    args.rval().setBoolean(isCallable);
    return true;
}

bool
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
static bool
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

static bool
intrinsic_MakeConstructible(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 2);
    JS_ASSERT(args[0].isObject());
    JS_ASSERT(args[0].toObject().is<JSFunction>());
    JS_ASSERT(args[1].isObject());

    // Normal .prototype properties aren't enumerable.  But for this to clone
    // correctly, it must be enumerable.
    RootedObject ctor(cx, &args[0].toObject());
    if (!JSObject::defineProperty(cx, ctor, cx->names().classPrototype, args[1],
                                  JS_PropertyStub, JS_StrictPropertyStub,
                                  JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT))
    {
        return false;
    }

    ctor->as<JSFunction>().setIsSelfHostedConstructor();
    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_MakeWrappable(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() >= 1);
    JS_ASSERT(args[0].isObject());
    JS_ASSERT(args[0].toObject().is<JSFunction>());
    args[0].toObject().as<JSFunction>().makeWrappable();
    args.rval().setUndefined();
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
static bool
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
 * - |inline: true| will hint that |fun| be inlined regardless of
 *   JIT heuristics.
 */
static bool
intrinsic_SetScriptHints(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() >= 2);
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<JSFunction>());
    JS_ASSERT(args[1].isObject());

    RootedFunction fun(cx, &args[0].toObject().as<JSFunction>());
    RootedScript funScript(cx, fun->nonLazyScript());
    RootedObject flags(cx, &args[1].toObject());

    RootedId id(cx);
    RootedValue propv(cx);

    id = AtomToId(Atomize(cx, "cloneAtCallsite", strlen("cloneAtCallsite")));
    if (!JSObject::getGeneric(cx, flags, flags, id, &propv))
        return false;
    if (ToBoolean(propv))
        funScript->shouldCloneAtCallsite = true;

    id = AtomToId(Atomize(cx, "inline", strlen("inline")));
    if (!JSObject::getGeneric(cx, flags, flags, id, &propv))
        return false;
    if (ToBoolean(propv))
        funScript->shouldInline = true;

    args.rval().setUndefined();
    return true;
}

#ifdef DEBUG
/*
 * Dump(val): Dumps a value for debugging, even in parallel mode.
 */
bool
intrinsic_Dump(ThreadSafeContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    js_DumpValue(args[0]);
    if (args[0].isObject()) {
        fprintf(stderr, "\n");
        js_DumpObject(&args[0].toObject());
    }
    args.rval().setUndefined();
    return true;
}

const JSJitInfo intrinsic_Dump_jitInfo =
    JS_JITINFO_NATIVE_PARALLEL(JSParallelNativeThreadSafeWrapper<intrinsic_Dump>);

bool
intrinsic_ParallelSpew(ThreadSafeContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isString());

    ScopedThreadSafeStringInspector inspector(args[0].toString());
    if (!inspector.ensureChars(cx))
        return false;

    ScopedJSFreePtr<char> bytes(TwoByteCharsToNewUTF8CharsZ(cx, inspector.range()).c_str());
    parallel::Spew(parallel::SpewOps, bytes);

    args.rval().setUndefined();
    return true;
}

const JSJitInfo intrinsic_ParallelSpew_jitInfo =
    JS_JITINFO_NATIVE_PARALLEL(JSParallelNativeThreadSafeWrapper<intrinsic_ParallelSpew>);
#endif

/*
 * ForkJoin(func, feedback): Invokes |func| many times in parallel.
 *
 * See ForkJoin.cpp for details and ParallelArray.js for examples.
 */
static bool
intrinsic_ForkJoin(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return ForkJoin(cx, args);
}

/*
 * ForkJoinSlices(): Returns the number of parallel slices that will
 * be created by ForkJoin().
 */
static bool
intrinsic_ForkJoinSlices(JSContext *cx, unsigned argc, Value *vp)
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
bool
js::intrinsic_NewParallelArray(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JS_ASSERT(args[0].isObject() && args[0].toObject().is<JSFunction>());

    RootedFunction init(cx, &args[0].toObject().as<JSFunction>());
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
bool
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
 * UnsafePutElements(arr0, idx0, elem0, ..., arrN, idxN, elemN): For each set of
 * (arr, idx, elem) arguments that are passed, performs the assignment
 * |arr[idx] = elem|. |arr| must be either a dense array or a typed array.
 *
 * If |arr| is a dense array, the index must be an int32 less than the
 * initialized length of |arr|. Use |%EnsureDenseResultArrayElements|
 * to ensure that the initialized length is long enough.
 *
 * If |arr| is a typed array, the index must be an int32 less than the
 * length of |arr|.
 */
bool
js::intrinsic_UnsafePutElements(JSContext *cx, unsigned argc, Value *vp)
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
                  args[arri].toObject().is<TypedArrayObject>());
        JS_ASSERT(args[idxi].isInt32());

        RootedObject arrobj(cx, &args[arri].toObject());
        uint32_t idx = args[idxi].toInt32();

        if (arrobj->isNative()) {
            JS_ASSERT(idx < arrobj->getDenseInitializedLength());
            JSObject::setDenseElementWithType(cx, arrobj, idx, args[elemi]);
        } else {
            JS_ASSERT(idx < arrobj->as<TypedArrayObject>().length());
            RootedValue tmp(cx, args[elemi]);
            // XXX: Always non-strict.
            if (!JSObject::setElement(cx, arrobj, arrobj, idx, &tmp, false))
                return false;
        }
    }

    args.rval().setUndefined();
    return true;
}

bool
js::intrinsic_UnsafeSetReservedSlot(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 3);
    JS_ASSERT(args[0].isObject());
    JS_ASSERT(args[1].isInt32());

    args[0].toObject().setReservedSlot(args[1].toPrivateUint32(), args[2]);
    args.rval().setUndefined();
    return true;
}

bool
js::intrinsic_UnsafeGetReservedSlot(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 2);
    JS_ASSERT(args[0].isObject());
    JS_ASSERT(args[1].isInt32());

    args.rval().set(args[0].toObject().getReservedSlot(args[1].toPrivateUint32()));
    return true;
}

static bool
intrinsic_NewClassPrototype(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isInt32());

    JSObject *proto = SelfHostedClass::newPrototype(cx, args[0].toPrivateUint32());
    if (!proto)
        return false;

    args.rval().setObject(*proto);
    return true;
}

bool
js::intrinsic_NewObjectWithClassPrototype(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isObject());

    RootedObject proto(cx, &args[0].toObject());
    JSObject *result = NewObjectWithGivenProto(cx, proto->getClass(), proto, cx->global());
    if (!result)
        return false;

    args.rval().setObject(*result);
    return true;
}

bool
js::intrinsic_HaveSameClass(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 2);
    JS_ASSERT(args[0].isObject());
    JS_ASSERT(args[1].isObject());

    args.rval().setBoolean(args[0].toObject().getClass() == args[1].toObject().getClass());
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
static bool
intrinsic_ParallelTestsShouldPass(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(ParallelTestsShouldPass(cx));
    return true;
}

/*
 * ShouldForceSequential(): Returns true if parallel ops should take
 * the sequential fallback path.
 */
bool
js::intrinsic_ShouldForceSequential(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
#ifdef JS_THREADSAFE
    args.rval().setBoolean(cx->runtime()->parallelWarmup ||
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
static bool
intrinsic_RuntimeDefaultLocale(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    const char *locale = cx->runtime()->getDefaultLocale();
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

const JSFunctionSpec intrinsic_functions[] = {
    JS_FN("ToObject",                intrinsic_ToObject,                1,0),
    JS_FN("ToInteger",               intrinsic_ToInteger,               1,0),
    JS_FN("IsCallable",              intrinsic_IsCallable,              1,0),
    JS_FN("ThrowError",              intrinsic_ThrowError,              4,0),
    JS_FN("AssertionFailed",         intrinsic_AssertionFailed,         1,0),
    JS_FN("SetScriptHints",          intrinsic_SetScriptHints,          2,0),
    JS_FN("MakeConstructible",       intrinsic_MakeConstructible,       1,0),
    JS_FN("MakeWrappable",           intrinsic_MakeWrappable,           1,0),
    JS_FN("DecompileArg",            intrinsic_DecompileArg,            2,0),
    JS_FN("RuntimeDefaultLocale",    intrinsic_RuntimeDefaultLocale,    0,0),

    JS_FN("UnsafePutElements",               intrinsic_UnsafePutElements,               3,0),
    JS_FN("UnsafeSetReservedSlot",   intrinsic_UnsafeSetReservedSlot,   3,0),
    JS_FN("UnsafeGetReservedSlot",   intrinsic_UnsafeGetReservedSlot,   2,0),

    JS_FN("NewClassPrototype",       intrinsic_NewClassPrototype,       1,0),
    JS_FN("NewObjectWithClassPrototype", intrinsic_NewObjectWithClassPrototype, 1,0),
    JS_FN("HaveSameClass",           intrinsic_HaveSameClass,           2,0),

    JS_FN("ForkJoin",                intrinsic_ForkJoin,                2,0),
    JS_FN("ForkJoinSlices",          intrinsic_ForkJoinSlices,          0,0),
    JS_FN("NewParallelArray",        intrinsic_NewParallelArray,        3,0),
    JS_FN("NewDenseArray",           intrinsic_NewDenseArray,           1,0),
    JS_FN("ShouldForceSequential",   intrinsic_ShouldForceSequential,   0,0),
    JS_FN("ParallelTestsShouldPass", intrinsic_ParallelTestsShouldPass, 0,0),

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

    // See builtin/RegExp.h for descriptions of the regexp_* functions.
    JS_FN("regexp_exec_no_statics", regexp_exec_no_statics, 2,0),
    JS_FN("regexp_test_no_statics", regexp_test_no_statics, 2,0),

#ifdef DEBUG
    JS_FNINFO("Dump",
              JSNativeThreadSafeWrapper<intrinsic_Dump>,
              &intrinsic_Dump_jitInfo, 1,0),

    JS_FNINFO("ParallelSpew",
              JSNativeThreadSafeWrapper<intrinsic_ParallelSpew>,
              &intrinsic_ParallelSpew_jitInfo, 1,0),
#endif

    JS_FS_END
};

bool
JSRuntime::initSelfHosting(JSContext *cx)
{
    JS_ASSERT(!selfHostingGlobal_);
    RootedObject savedGlobal(cx, js::DefaultObjectForContextOrNull(cx));
    if (!(selfHostingGlobal_ = JS_NewGlobalObject(cx, &self_hosting_global_class,
                                                  NULL, JS::DontFireOnNewGlobalHook)))
        return false;
    JSAutoCompartment ac(cx, selfHostingGlobal_);
    js::SetDefaultObjectForContext(cx, selfHostingGlobal_);
    Rooted<GlobalObject*> shg(cx, &selfHostingGlobal_->as<GlobalObject>());
    /*
     * During initialization of standard classes for the self-hosting global,
     * all self-hosted functions are ignored. Thus, we don't create cyclic
     * dependencies in the order of initialization.
     */
    if (!GlobalObject::initStandardClasses(cx, shg))
        return false;

    if (!JS_DefineFunctions(cx, shg, intrinsic_functions))
        return false;

    JS_FireOnNewGlobalObject(cx, shg);

    /*
     * In self-hosting mode, scripts emit JSOP_CALLINTRINSIC instead of
     * JSOP_NAME or JSOP_GNAME to access unbound variables. JSOP_CALLINTRINSIC
     * does a name lookup in a special object, whose properties are filled in
     * lazily upon first access for a given global.
     *
     * As that object is inaccessible to client code, the lookups are
     * guaranteed to return the original objects, ensuring safe implementation
     * of self-hosted builtins.
     *
     * Additionally, the special syntax _CallFunction(receiver, ...args, fun)
     * is supported, for which bytecode is emitted that invokes |fun| with
     * |receiver| as the this-object and ...args as the arguments..
     */
    CompileOptions options(cx);
    options.setFileAndLine("self-hosted", 1);
    options.setSelfHostingMode(true);
    options.setCanLazilyParse(false);
    options.setSourcePolicy(CompileOptions::NO_SOURCE);
    options.setVersion(JSVERSION_LATEST);

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
    js::SetDefaultObjectForContext(cx, savedGlobal);
    return ok;
}

void
JSRuntime::finishSelfHosting()
{
    selfHostingGlobal_ = NULL;

    SelfHostedClass *shClass = selfHostedClasses_;
    while (shClass) {
        SelfHostedClass *tmp = shClass;
        shClass = shClass->next;
        js_delete(tmp);
    }
    selfHostedClasses_ = NULL;
}

void
JSRuntime::markSelfHostingGlobal(JSTracer *trc)
{
    if (selfHostingGlobal_)
        MarkObjectRoot(trc, &selfHostingGlobal_, "self-hosting global");
}

void
JSRuntime::addSelfHostedClass(SelfHostedClass *shClass)
{
    shClass->next = selfHostedClasses_;
    selfHostedClasses_ = shClass;
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

    if (SelfHostedClass::is(cx, obj->getClass())) {
        for (uint32_t i = 0; i < JSCLASS_RESERVED_SLOTS(obj->getClass()); i++) {
            val = obj->getReservedSlot(i);
            if (!CloneValue(cx, &val, clonedObjects))
                return false;
            clone->setReservedSlot(i, val);
        }

        /* Privates are not cloned, so be careful! */
        if (obj->hasPrivate())
            clone->setPrivate(obj->getPrivate());
    }

    return true;
}

static gc::AllocKind
GetObjectAllocKindForClone(JSRuntime *rt, JSObject *obj)
{
    if (!gc::IsInsideNursery(rt, (void *)obj))
        return obj->tenuredGetAllocKind();

    if (obj->is<JSFunction>())
        return obj->as<JSFunction>().getAllocKind();

    gc::AllocKind kind = gc::GetGCObjectFixedSlotsKind(obj->numFixedSlots());
    if (CanBeFinalizedInBackground(kind, obj->getClass()))
        kind = GetBackgroundAllocKind(kind);
    return kind;
}

static JSObject *
CloneObject(JSContext *cx, HandleObject srcObj, CloneMemory &clonedObjects)
{
    CloneMemory::AddPtr p = clonedObjects.lookupForAdd(srcObj.get());
    if (p)
        return p->value;
    RootedObject clone(cx);
    if (srcObj->is<JSFunction>()) {
        if (srcObj->as<JSFunction>().isWrappable()) {
            clone = srcObj;
            if (!cx->compartment()->wrap(cx, &clone))
                return NULL;
        } else {
            RootedFunction fun(cx, &srcObj->as<JSFunction>());
            clone = CloneFunctionObject(cx, fun, cx->global(), fun->getAllocKind(), TenuredObject);
        }
    } else if (srcObj->is<RegExpObject>()) {
        RegExpObject &reobj = srcObj->as<RegExpObject>();
        RootedAtom source(cx, reobj.getSource());
        clone = RegExpObject::createNoStatics(cx, source, reobj.getFlags(), NULL);
    } else if (srcObj->is<DateObject>()) {
        clone = JS_NewDateObjectMsec(cx, srcObj->as<DateObject>().UTCTime().toNumber());
    } else if (srcObj->is<BooleanObject>()) {
        clone = BooleanObject::create(cx, srcObj->as<BooleanObject>().unbox());
    } else if (srcObj->is<NumberObject>()) {
        clone = NumberObject::create(cx, srcObj->as<NumberObject>().unbox());
    } else if (srcObj->is<StringObject>()) {
        Rooted<JSStableString*> str(cx, srcObj->as<StringObject>().unbox()->ensureStable(cx));
        if (!str)
            return NULL;
        str = js_NewStringCopyN<CanGC>(cx, str->chars().get(), str->length())->ensureStable(cx);
        if (!str)
            return NULL;
        clone = StringObject::create(cx, str);
    } else if (srcObj->is<ArrayObject>()) {
        clone = NewDenseEmptyArray(cx, NULL, TenuredObject);
    } else {
        JS_ASSERT(srcObj->isNative());
        clone = NewObjectWithGivenProto(cx, srcObj->getClass(), NULL, cx->global(),
                                        GetObjectAllocKindForClone(cx->runtime(), srcObj),
                                        SingletonObject);
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
        MOZ_ASSUME_UNREACHABLE("Self-hosting CloneValue can't clone given value.");
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

    RootedFunction sourceFun(cx, &funVal.toObject().as<JSFunction>());
    RootedScript sourceScript(cx, sourceFun->nonLazyScript());
    JS_ASSERT(!sourceScript->enclosingStaticScope());
    JSScript *cscript = CloneScript(cx, NullPtr(), targetFun, sourceScript);
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

bool
JSRuntime::maybeWrappedSelfHostedFunction(JSContext *cx, Handle<PropertyName*> name,
                                          MutableHandleValue funVal)
{
    RootedObject shg(cx, selfHostingGlobal_);
    RootedId id(cx, NameToId(name));
    if (!GetUnclonedValue(cx, shg, id, funVal))
        return false;

    JS_ASSERT(funVal.isObject());
    JS_ASSERT(funVal.toObject().isCallable());

    if (!funVal.toObject().as<JSFunction>().isWrappable()) {
        funVal.setUndefined();
        return true;
    }

    return cx->compartment()->wrap(cx, funVal);
}

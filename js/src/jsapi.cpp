/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JavaScript API.
 */

#include "jsapi.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/PodOperations.h"
#include "mozilla/UniquePtr.h"

#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>

#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsdate.h"
#include "jsexn.h"
#include "jsfriendapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsmath.h"
#include "jsnum.h"
#include "jsobj.h"
#include "json.h"
#include "jsprf.h"
#include "jsproxy.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jstypes.h"
#include "jsutil.h"
#include "jswatchpoint.h"
#include "jsweakmap.h"
#include "jswrapper.h"
#include "prmjtime.h"

#include "asmjs/AsmJSLink.h"
#include "builtin/Eval.h"
#include "builtin/Intl.h"
#include "builtin/MapObject.h"
#include "builtin/RegExp.h"
#include "builtin/SymbolObject.h"
#ifdef ENABLE_BINARYDATA
#include "builtin/SIMD.h"
#include "builtin/TypedObject.h"
#endif
#include "frontend/BytecodeCompiler.h"
#include "frontend/FullParseHandler.h"  // for JS_BufferIsCompileableUnit
#include "frontend/Parser.h" // for JS_BufferIsCompileableUnit
#include "gc/Marking.h"
#include "jit/JitCommon.h"
#include "js/CharacterEncoding.h"
#include "js/SliceBudget.h"
#include "js/StructuredClone.h"
#if ENABLE_INTL_API
#include "unicode/uclean.h"
#include "unicode/utypes.h"
#endif // ENABLE_INTL_API
#include "vm/DateObject.h"
#include "vm/Debugger.h"
#include "vm/ErrorObject.h"
#include "vm/HelperThreads.h"
#include "vm/Interpreter.h"
#include "vm/NumericConversions.h"
#include "vm/RegExpStatics.h"
#include "vm/Runtime.h"
#include "vm/SavedStacks.h"
#include "vm/Shape.h"
#include "vm/StopIterationObject.h"
#include "vm/StringBuffer.h"
#include "vm/Symbol.h"
#include "vm/TypedArrayCommon.h"
#include "vm/WeakMapObject.h"
#include "vm/WrapperObject.h"
#include "vm/Xdr.h"

#include "jsatominlines.h"
#include "jsfuninlines.h"
#include "jsinferinlines.h"
#include "jsscriptinlines.h"

#include "vm/Interpreter-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/String-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::types;

using mozilla::Maybe;
using mozilla::PodCopy;
using mozilla::PodZero;
using mozilla::UniquePtr;

using JS::AutoGCRooter;

using js::frontend::Parser;

#ifdef HAVE_VA_LIST_AS_ARRAY
#define JS_ADDRESSOF_VA_LIST(ap) ((va_list *)(ap))
#else
#define JS_ADDRESSOF_VA_LIST(ap) (&(ap))
#endif

/* Make sure that char16_t is two bytes unsigned integer */
JS_STATIC_ASSERT((char16_t)-1 > 0);
JS_STATIC_ASSERT(sizeof(char16_t) == 2);

bool
JS::CallArgs::requireAtLeast(JSContext *cx, const char *fnname, unsigned required) {
    if (length() < required) {
        char numArgsStr[40];
        JS_snprintf(numArgsStr, sizeof numArgsStr, "%u", required - 1);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             fnname, numArgsStr, required == 2 ? "" : "s");
        return false;
    }

    return true;
}

JS_PUBLIC_API(int64_t)
JS_Now()
{
    return PRMJ_Now();
}

JS_PUBLIC_API(jsval)
JS_GetNaNValue(JSContext *cx)
{
    return cx->runtime()->NaNValue;
}

JS_PUBLIC_API(jsval)
JS_GetNegativeInfinityValue(JSContext *cx)
{
    return cx->runtime()->negativeInfinityValue;
}

JS_PUBLIC_API(jsval)
JS_GetPositiveInfinityValue(JSContext *cx)
{
    return cx->runtime()->positiveInfinityValue;
}

JS_PUBLIC_API(jsval)
JS_GetEmptyStringValue(JSContext *cx)
{
    return STRING_TO_JSVAL(cx->runtime()->emptyString);
}

JS_PUBLIC_API(JSString *)
JS_GetEmptyString(JSRuntime *rt)
{
    MOZ_ASSERT(rt->hasContexts());
    return rt->emptyString;
}

namespace js {

void
AssertHeapIsIdle(JSRuntime *rt)
{
    MOZ_ASSERT(!rt->isHeapBusy());
}

void
AssertHeapIsIdle(JSContext *cx)
{
    AssertHeapIsIdle(cx->runtime());
}

}

static void
AssertHeapIsIdleOrIterating(JSRuntime *rt)
{
    MOZ_ASSERT(!rt->isHeapCollecting());
}

static void
AssertHeapIsIdleOrIterating(JSContext *cx)
{
    AssertHeapIsIdleOrIterating(cx->runtime());
}

static void
AssertHeapIsIdleOrStringIsFlat(JSContext *cx, JSString *str)
{
    /*
     * We allow some functions to be called during a GC as long as the argument
     * is a flat string, since that will not cause allocation.
     */
    MOZ_ASSERT_IF(cx->runtime()->isHeapBusy(), str->isFlat());
}

JS_PUBLIC_API(bool)
JS_ConvertArguments(JSContext *cx, const CallArgs &args, const char *format, ...)
{
    va_list ap;
    bool ok;

    AssertHeapIsIdle(cx);

    va_start(ap, format);
    ok = JS_ConvertArgumentsVA(cx, args, format, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(bool)
JS_ConvertArgumentsVA(JSContext *cx, const CallArgs &args, const char *format, va_list ap)
{
    unsigned index = 0;
    bool required;
    char c;
    double d;
    JSString *str;
    RootedObject obj(cx);
    RootedValue val(cx);

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, args);
    required = true;
    while ((c = *format++) != '\0') {
        if (isspace(c))
            continue;
        if (c == '/') {
            required = false;
            continue;
        }
        if (index == args.length()) {
            if (required) {
                if (JSFunction *fun = ReportIfNotFunction(cx, args.calleev())) {
                    char numBuf[12];
                    JS_snprintf(numBuf, sizeof numBuf, "%u", args.length());
                    JSAutoByteString funNameBytes;
                    if (const char *name = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
                        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                             JSMSG_MORE_ARGS_NEEDED,
                                             name, numBuf, (args.length() == 1) ? "" : "s");
                    }
                }
                return false;
            }
            break;
        }
        MutableHandleValue arg = args[index++];
        switch (c) {
          case 'b':
            *va_arg(ap, bool *) = ToBoolean(arg);
            break;
          case 'c':
            if (!ToUint16(cx, arg, va_arg(ap, uint16_t *)))
                return false;
            break;
          case 'i':
          case 'j': // "j" was broken, you should not use it.
            if (!ToInt32(cx, arg, va_arg(ap, int32_t *)))
                return false;
            break;
          case 'u':
            if (!ToUint32(cx, arg, va_arg(ap, uint32_t *)))
                return false;
            break;
          case 'd':
            if (!ToNumber(cx, arg, va_arg(ap, double *)))
                return false;
            break;
          case 'I':
            if (!ToNumber(cx, arg, &d))
                return false;
            *va_arg(ap, double *) = ToInteger(d);
            break;
          case 'S':
            str = ToString<CanGC>(cx, arg);
            if (!str)
                return false;
            arg.setString(str);
            *va_arg(ap, JSString **) = str;
            break;
          case 'o':
            if (arg.isNullOrUndefined()) {
                obj = nullptr;
            } else {
                obj = ToObject(cx, arg);
                if (!obj)
                    return false;
            }
            arg.setObjectOrNull(obj);
            *va_arg(ap, JSObject **) = obj;
            break;
          case 'f':
            obj = ReportIfNotFunction(cx, arg);
            if (!obj)
                return false;
            arg.setObject(*obj);
            *va_arg(ap, JSFunction **) = &obj->as<JSFunction>();
            break;
          case 'v':
            *va_arg(ap, jsval *) = arg;
            break;
          case '*':
            break;
          default:
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_CHAR, format);
            return false;
        }
    }
    return true;
}

JS_PUBLIC_API(bool)
JS_ConvertValue(JSContext *cx, HandleValue value, JSType type, MutableHandleValue vp)
{
    bool ok;
    RootedObject obj(cx);
    JSString *str;
    double d;

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    switch (type) {
      case JSTYPE_VOID:
        vp.setUndefined();
        ok = true;
        break;
      case JSTYPE_OBJECT:
        if (value.isNullOrUndefined()) {
            obj.set(nullptr);
        } else {
            obj = ToObject(cx, value);
            if (!obj)
                return false;
        }
        ok = true;
        break;
      case JSTYPE_FUNCTION:
        vp.set(value);
        obj = ReportIfNotFunction(cx, vp);
        ok = (obj != nullptr);
        break;
      case JSTYPE_STRING:
        str = ToString<CanGC>(cx, value);
        ok = (str != nullptr);
        if (ok)
            vp.setString(str);
        break;
      case JSTYPE_NUMBER:
        ok = ToNumber(cx, value, &d);
        if (ok)
            vp.setDouble(d);
        break;
      case JSTYPE_BOOLEAN:
        vp.setBoolean(ToBoolean(value));
        return true;
      default: {
        char numBuf[12];
        JS_snprintf(numBuf, sizeof numBuf, "%d", (int)type);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_TYPE, numBuf);
        ok = false;
        break;
      }
    }
    return ok;
}

JS_PUBLIC_API(bool)
JS_ValueToObject(JSContext *cx, HandleValue value, MutableHandleObject objp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    if (value.isNullOrUndefined()) {
        objp.set(nullptr);
        return true;
    }
    JSObject *obj = ToObject(cx, value);
    if (!obj)
        return false;
    objp.set(obj);
    return true;
}

JS_PUBLIC_API(JSFunction *)
JS_ValueToFunction(JSContext *cx, HandleValue value)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    return ReportIfNotFunction(cx, value);
}

JS_PUBLIC_API(JSFunction *)
JS_ValueToConstructor(JSContext *cx, HandleValue value)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    return ReportIfNotFunction(cx, value);
}

JS_PUBLIC_API(JSString *)
JS_ValueToSource(JSContext *cx, HandleValue value)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    return ValueToSource(cx, value);
}

JS_PUBLIC_API(bool)
JS_DoubleIsInt32(double d, int32_t *ip)
{
    return mozilla::NumberIsInt32(d, ip);
}

JS_PUBLIC_API(int32_t)
JS_DoubleToInt32(double d)
{
    return ToInt32(d);
}

JS_PUBLIC_API(uint32_t)
JS_DoubleToUint32(double d)
{
    return ToUint32(d);
}

JS_PUBLIC_API(JSType)
JS_TypeOfValue(JSContext *cx, HandleValue value)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    return TypeOfValue(value);
}

JS_PUBLIC_API(bool)
JS_StrictlyEqual(JSContext *cx, jsval value1, jsval value2, bool *equal)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value1, value2);
    bool eq;
    if (!StrictlyEqual(cx, value1, value2, &eq))
        return false;
    *equal = eq;
    return true;
}

JS_PUBLIC_API(bool)
JS_LooselyEqual(JSContext *cx, HandleValue value1, HandleValue value2, bool *equal)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value1, value2);
    MOZ_ASSERT(equal);
    return LooselyEqual(cx, value1, value2, equal);
}

JS_PUBLIC_API(bool)
JS_SameValue(JSContext *cx, jsval value1, jsval value2, bool *same)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value1, value2);
    bool s;
    if (!SameValue(cx, value1, value2, &s))
        return false;
    *same = s;
    return true;
}

JS_PUBLIC_API(bool)
JS_IsBuiltinEvalFunction(JSFunction *fun)
{
    return IsAnyBuiltinEval(fun);
}

JS_PUBLIC_API(bool)
JS_IsBuiltinFunctionConstructor(JSFunction *fun)
{
    return fun->isBuiltinFunctionConstructor();
}

/************************************************************************/

/*
 * SpiderMonkey's initialization status is tracked here, and it controls things
 * that should happen only once across all runtimes.  It's an API requirement
 * that JS_Init (and JS_ShutDown, if called) be called in a thread-aware
 * manner, so this variable doesn't need to be atomic.
 *
 * The only reason at present for the restriction that you can't call
 * JS_Init/stuff/JS_ShutDown multiple times is the Windows PRMJ NowInit
 * initialization code, which uses PR_CallOnce to initialize the PRMJ_Now
 * subsystem.  (For reinitialization to be permitted, we'd need to "reset" the
 * called-once status -- doable, but more trouble than it's worth now.)
 * Initializing that subsystem from JS_Init eliminates the problem, but
 * initialization can take a comparatively long time (15ms or so), so we
 * really don't want to do it in JS_Init, and we really do want to do it only
 * when PRMJ_Now is eventually called.
 */
enum InitState { Uninitialized, Running, ShutDown };
static InitState jsInitState = Uninitialized;

#ifdef DEBUG
static unsigned
MessageParameterCount(const char *format)
{
    unsigned numfmtspecs = 0;
    for (const char *fmt = format; *fmt != '\0'; fmt++) {
        if (*fmt == '{' && isdigit(fmt[1]))
            ++numfmtspecs;
    }
    return numfmtspecs;
}

static void
CheckMessageParameterCounts()
{
    // Assert that each message format has the correct number of braced
    // parameters.
# define MSG_DEF(name, count, exception, format)           \
        MOZ_ASSERT(MessageParameterCount(format) == count);
# include "js.msg"
# undef MSG_DEF
}
#endif /* DEBUG */

JS_PUBLIC_API(bool)
JS_Init(void)
{
    MOZ_ASSERT(jsInitState == Uninitialized,
               "must call JS_Init once before any JSAPI operation except "
               "JS_SetICUMemoryFunctions");
    MOZ_ASSERT(!JSRuntime::hasLiveRuntimes(),
               "how do we have live runtimes before JS_Init?");

    PRMJ_NowInit();

#ifdef DEBUG
    CheckMessageParameterCounts();
#endif

    using js::TlsPerThreadData;
    if (!TlsPerThreadData.initialized() && !TlsPerThreadData.init())
        return false;

    if (!jit::InitializeIon())
        return false;

    if (!ForkJoinContext::initializeTls())
        return false;

#if EXPOSE_INTL_API
    UErrorCode err = U_ZERO_ERROR;
    u_init(&err);
    if (U_FAILURE(err))
        return false;
#endif // EXPOSE_INTL_API

    jsInitState = Running;
    return true;
}

JS_PUBLIC_API(void)
JS_ShutDown(void)
{
    MOZ_ASSERT(jsInitState == Running,
               "JS_ShutDown must only be called after JS_Init and can't race with it");
#ifdef DEBUG
    if (JSRuntime::hasLiveRuntimes()) {
        // Gecko is too buggy to assert this just yet.
        fprintf(stderr,
                "WARNING: YOU ARE LEAKING THE WORLD (at least one JSRuntime "
                "and everything alive inside it, that is) AT JS_ShutDown "
                "TIME.  FIX THIS!\n");
    }
#endif

    HelperThreadState().finish();

    PRMJ_NowShutdown();

#if EXPOSE_INTL_API
    u_cleanup();
#endif // EXPOSE_INTL_API

    jsInitState = ShutDown;
}

#ifdef DEBUG
JS_FRIEND_API(bool)
JS::isGCEnabled()
{
    return !TlsPerThreadData.get()->suppressGC;
}
#else
JS_FRIEND_API(bool) JS::isGCEnabled() { return true; }
#endif

JS_PUBLIC_API(JSRuntime *)
JS_NewRuntime(uint32_t maxbytes, uint32_t maxNurseryBytes, JSRuntime *parentRuntime)
{
    MOZ_ASSERT(jsInitState == Running,
               "must call JS_Init prior to creating any JSRuntimes");

    // Any parent runtime should be the topmost parent. This assert
    // isn't required for correctness, but ensuring that the parent
    // runtime is not destroyed before this one is more easily done
    // for the main runtime in the process.
    MOZ_ASSERT_IF(parentRuntime, !parentRuntime->parentRuntime);

    JSRuntime *rt = js_new<JSRuntime>(parentRuntime);
    if (!rt)
        return nullptr;

    if (!rt->init(maxbytes, maxNurseryBytes)) {
        JS_DestroyRuntime(rt);
        return nullptr;
    }

    return rt;
}

JS_PUBLIC_API(void)
JS_DestroyRuntime(JSRuntime *rt)
{
    js_delete(rt);
}

JS_PUBLIC_API(bool)
JS_SetICUMemoryFunctions(JS_ICUAllocFn allocFn, JS_ICUReallocFn reallocFn, JS_ICUFreeFn freeFn)
{
    MOZ_ASSERT(jsInitState == Uninitialized,
               "must call JS_SetICUMemoryFunctions before any other JSAPI "
               "operation (including JS_Init)");

#if EXPOSE_INTL_API
    UErrorCode status = U_ZERO_ERROR;
    u_setMemoryFunctions(/* context = */ nullptr, allocFn, reallocFn, freeFn, &status);
    return U_SUCCESS(status);
#else
    return true;
#endif
}

JS_PUBLIC_API(void *)
JS_GetRuntimePrivate(JSRuntime *rt)
{
    return rt->data;
}

JS_PUBLIC_API(void)
JS_SetRuntimePrivate(JSRuntime *rt, void *data)
{
    rt->data = data;
}

static void
StartRequest(JSContext *cx)
{
    JSRuntime *rt = cx->runtime();
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));

    if (rt->requestDepth) {
        rt->requestDepth++;
    } else {
        /* Indicate that a request is running. */
        rt->requestDepth = 1;
        rt->triggerActivityCallback(true);
    }
}

static void
StopRequest(JSContext *cx)
{
    JSRuntime *rt = cx->runtime();
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));

    MOZ_ASSERT(rt->requestDepth != 0);
    if (rt->requestDepth != 1) {
        rt->requestDepth--;
    } else {
        rt->gc.notifyRequestEnd();
        rt->requestDepth = 0;
        rt->triggerActivityCallback(false);
    }
}

JS_PUBLIC_API(void)
JS_BeginRequest(JSContext *cx)
{
    cx->outstandingRequests++;
    StartRequest(cx);
}

JS_PUBLIC_API(void)
JS_EndRequest(JSContext *cx)
{
    MOZ_ASSERT(cx->outstandingRequests != 0);
    cx->outstandingRequests--;
    StopRequest(cx);
}

JS_PUBLIC_API(bool)
JS_IsInRequest(JSRuntime *rt)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
    return rt->requestDepth != 0;
}

JS_PUBLIC_API(void)
JS_SetContextCallback(JSRuntime *rt, JSContextCallback cxCallback, void *data)
{
    rt->cxCallback = cxCallback;
    rt->cxCallbackData = data;
}

JS_PUBLIC_API(JSContext *)
JS_NewContext(JSRuntime *rt, size_t stackChunkSize)
{
    return NewContext(rt, stackChunkSize);
}

JS_PUBLIC_API(void)
JS_DestroyContext(JSContext *cx)
{
    MOZ_ASSERT(!cx->compartment());
    DestroyContext(cx, DCM_FORCE_GC);
}

JS_PUBLIC_API(void)
JS_DestroyContextNoGC(JSContext *cx)
{
    MOZ_ASSERT(!cx->compartment());
    DestroyContext(cx, DCM_NO_GC);
}

JS_PUBLIC_API(void *)
JS_GetContextPrivate(JSContext *cx)
{
    return cx->data;
}

JS_PUBLIC_API(void)
JS_SetContextPrivate(JSContext *cx, void *data)
{
    cx->data = data;
}

JS_PUBLIC_API(void *)
JS_GetSecondContextPrivate(JSContext *cx)
{
    return cx->data2;
}

JS_PUBLIC_API(void)
JS_SetSecondContextPrivate(JSContext *cx, void *data)
{
    cx->data2 = data;
}

JS_PUBLIC_API(JSRuntime *)
JS_GetRuntime(JSContext *cx)
{
    return cx->runtime();
}

JS_PUBLIC_API(JSRuntime *)
JS_GetParentRuntime(JSContext *cx)
{
    JSRuntime *rt = cx->runtime();
    return rt->parentRuntime ? rt->parentRuntime : rt;
}

JS_PUBLIC_API(JSContext *)
JS_ContextIterator(JSRuntime *rt, JSContext **iterp)
{
    JSContext *cx = *iterp;
    cx = cx ? cx->getNext() : rt->contextList.getFirst();
    *iterp = cx;
    return cx;
}

JS_PUBLIC_API(JSVersion)
JS_GetVersion(JSContext *cx)
{
    return VersionNumber(cx->findVersion());
}

JS_PUBLIC_API(void)
JS_SetVersionForCompartment(JSCompartment *compartment, JSVersion version)
{
    compartment->options().setVersion(version);
}

static const struct v2smap {
    JSVersion   version;
    const char  *string;
} v2smap[] = {
    {JSVERSION_ECMA_3,  "ECMAv3"},
    {JSVERSION_1_6,     "1.6"},
    {JSVERSION_1_7,     "1.7"},
    {JSVERSION_1_8,     "1.8"},
    {JSVERSION_ECMA_5,  "ECMAv5"},
    {JSVERSION_DEFAULT, js_default_str},
    {JSVERSION_DEFAULT, "1.0"},
    {JSVERSION_DEFAULT, "1.1"},
    {JSVERSION_DEFAULT, "1.2"},
    {JSVERSION_DEFAULT, "1.3"},
    {JSVERSION_DEFAULT, "1.4"},
    {JSVERSION_DEFAULT, "1.5"},
    {JSVERSION_UNKNOWN, nullptr},          /* must be last, nullptr is sentinel */
};

JS_PUBLIC_API(const char *)
JS_VersionToString(JSVersion version)
{
    int i;

    for (i = 0; v2smap[i].string; i++)
        if (v2smap[i].version == version)
            return v2smap[i].string;
    return "unknown";
}

JS_PUBLIC_API(JSVersion)
JS_StringToVersion(const char *string)
{
    int i;

    for (i = 0; v2smap[i].string; i++)
        if (strcmp(v2smap[i].string, string) == 0)
            return v2smap[i].version;
    return JSVERSION_UNKNOWN;
}

JS_PUBLIC_API(JS::RuntimeOptions &)
JS::RuntimeOptionsRef(JSRuntime *rt)
{
    return rt->options();
}

JS_PUBLIC_API(JS::RuntimeOptions &)
JS::RuntimeOptionsRef(JSContext *cx)
{
    return cx->runtime()->options();
}

JS_PUBLIC_API(JS::ContextOptions &)
JS::ContextOptionsRef(JSContext *cx)
{
    return cx->options();
}

JS_PUBLIC_API(const char *)
JS_GetImplementationVersion(void)
{
    return "JavaScript-C" MOZILLA_VERSION;
}

JS_PUBLIC_API(void)
JS_SetDestroyCompartmentCallback(JSRuntime *rt, JSDestroyCompartmentCallback callback)
{
    rt->destroyCompartmentCallback = callback;
}

JS_PUBLIC_API(void)
JS_SetDestroyZoneCallback(JSRuntime *rt, JSZoneCallback callback)
{
    rt->destroyZoneCallback = callback;
}

JS_PUBLIC_API(void)
JS_SetSweepZoneCallback(JSRuntime *rt, JSZoneCallback callback)
{
    rt->sweepZoneCallback = callback;
}

JS_PUBLIC_API(void)
JS_SetCompartmentNameCallback(JSRuntime *rt, JSCompartmentNameCallback callback)
{
    rt->compartmentNameCallback = callback;
}

JS_PUBLIC_API(void)
JS_SetWrapObjectCallbacks(JSRuntime *rt, const JSWrapObjectCallbacks *callbacks)
{
    rt->wrapObjectCallbacks = callbacks;
}

JS_PUBLIC_API(JSCompartment *)
JS_EnterCompartment(JSContext *cx, JSObject *target)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    JSCompartment *oldCompartment = cx->compartment();
    cx->enterCompartment(target->compartment());
    return oldCompartment;
}

JS_PUBLIC_API(void)
JS_LeaveCompartment(JSContext *cx, JSCompartment *oldCompartment)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    cx->leaveCompartment(oldCompartment);
}

JSAutoCompartment::JSAutoCompartment(JSContext *cx, JSObject *target)
  : cx_(cx),
    oldCompartment_(cx->compartment())
{
    AssertHeapIsIdleOrIterating(cx_);
    cx_->enterCompartment(target->compartment());
}

JSAutoCompartment::JSAutoCompartment(JSContext *cx, JSScript *target)
  : cx_(cx),
    oldCompartment_(cx->compartment())
{
    AssertHeapIsIdleOrIterating(cx_);
    cx_->enterCompartment(target->compartment());
}

JSAutoCompartment::~JSAutoCompartment()
{
    cx_->leaveCompartment(oldCompartment_);
}

JSAutoNullableCompartment::JSAutoNullableCompartment(JSContext *cx,
                                                     JSObject *targetOrNull)
  : cx_(cx),
    oldCompartment_(cx->compartment())
{
    AssertHeapIsIdleOrIterating(cx_);
    if (targetOrNull) {
        cx_->enterCompartment(targetOrNull->compartment());
    } else {
        cx_->enterNullCompartment();
    }
}

JSAutoNullableCompartment::~JSAutoNullableCompartment()
{
    cx_->leaveCompartment(oldCompartment_);
}

JS_PUBLIC_API(void)
JS_SetCompartmentPrivate(JSCompartment *compartment, void *data)
{
    compartment->data = data;
}

JS_PUBLIC_API(void *)
JS_GetCompartmentPrivate(JSCompartment *compartment)
{
    return compartment->data;
}

JS_PUBLIC_API(JSAddonId *)
JS::NewAddonId(JSContext *cx, HandleString str)
{
    return static_cast<JSAddonId *>(JS_InternJSString(cx, str));
}

JS_PUBLIC_API(JSString *)
JS::StringOfAddonId(JSAddonId *id)
{
    return id;
}

JS_PUBLIC_API(JSAddonId *)
JS::AddonIdOfObject(JSObject *obj)
{
    return obj->compartment()->addonId;
}

JS_PUBLIC_API(void)
JS_SetZoneUserData(JS::Zone *zone, void *data)
{
    zone->data = data;
}

JS_PUBLIC_API(void *)
JS_GetZoneUserData(JS::Zone *zone)
{
    return zone->data;
}

JS_PUBLIC_API(bool)
JS_WrapObject(JSContext *cx, MutableHandleObject objp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (objp)
        JS::ExposeObjectToActiveJS(objp);
    return cx->compartment()->wrap(cx, objp);
}

JS_PUBLIC_API(bool)
JS_WrapValue(JSContext *cx, MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JS::ExposeValueToActiveJS(vp);
    return cx->compartment()->wrap(cx, vp);
}

/*
 * Identity remapping. Not for casual consumers.
 *
 * Normally, an object's contents and its identity are inextricably linked.
 * Identity is determined by the address of the JSObject* in the heap, and
 * the contents are what is located at that address. Transplanting allows these
 * concepts to be separated through a combination of swapping (exchanging the
 * contents of two same-compartment objects) and remapping cross-compartment
 * identities by altering wrappers.
 *
 * The |origobj| argument should be the object whose identity needs to be
 * remapped, usually to another compartment. The contents of |origobj| are
 * destroyed.
 *
 * The |target| argument serves two purposes:
 *
 * First, |target| serves as a hint for the new identity of the object. The new
 * identity object will always be in the same compartment as |target|, but
 * if that compartment already had an object representing |origobj| (either a
 * cross-compartment wrapper for it, or |origobj| itself if the two arguments
 * are same-compartment), the existing object is used. Otherwise, |target|
 * itself is used. To avoid ambiguity, JS_TransplantObject always returns the
 * new identity.
 *
 * Second, the new identity object's contents will be those of |target|. A swap()
 * is used to make this happen if an object other than |target| is used.
 *
 * We don't have a good way to recover from failure in this function, so
 * we intentionally crash instead.
 */

JS_PUBLIC_API(JSObject *)
JS_TransplantObject(JSContext *cx, HandleObject origobj, HandleObject target)
{
    AssertHeapIsIdle(cx);
    MOZ_ASSERT(origobj != target);
    MOZ_ASSERT(!origobj->is<CrossCompartmentWrapperObject>());
    MOZ_ASSERT(!target->is<CrossCompartmentWrapperObject>());

    RootedValue origv(cx, ObjectValue(*origobj));
    RootedObject newIdentity(cx);

    {
        AutoDisableProxyCheck adpc(cx->runtime());

        JSCompartment *destination = target->compartment();

        if (origobj->compartment() == destination) {
            // If the original object is in the same compartment as the
            // destination, then we know that we won't find a wrapper in the
            // destination's cross compartment map and that the same
            // object will continue to work.
            if (!JSObject::swap(cx, origobj, target))
                MOZ_CRASH();
            newIdentity = origobj;
        } else if (WrapperMap::Ptr p = destination->lookupWrapper(origv)) {
            // There might already be a wrapper for the original object in
            // the new compartment. If there is, we use its identity and swap
            // in the contents of |target|.
            newIdentity = &p->value().get().toObject();

            // When we remove origv from the wrapper map, its wrapper, newIdentity,
            // must immediately cease to be a cross-compartment wrapper. Neuter it.
            destination->removeWrapper(p);
            NukeCrossCompartmentWrapper(cx, newIdentity);

            if (!JSObject::swap(cx, newIdentity, target))
                MOZ_CRASH();
        } else {
            // Otherwise, we use |target| for the new identity object.
            newIdentity = target;
        }

        // Now, iterate through other scopes looking for references to the
        // old object, and update the relevant cross-compartment wrappers.
        if (!RemapAllWrappersForObject(cx, origobj, newIdentity))
            MOZ_CRASH();

        // Lastly, update the original object to point to the new one.
        if (origobj->compartment() != destination) {
            RootedObject newIdentityWrapper(cx, newIdentity);
            AutoCompartment ac(cx, origobj);
            if (!JS_WrapObject(cx, &newIdentityWrapper))
                MOZ_CRASH();
            MOZ_ASSERT(Wrapper::wrappedObject(newIdentityWrapper) == newIdentity);
            if (!JSObject::swap(cx, origobj, newIdentityWrapper))
                MOZ_CRASH();
            origobj->compartment()->putWrapper(cx, CrossCompartmentKey(newIdentity), origv);
        }
    }

    // The new identity object might be one of several things. Return it to avoid
    // ambiguity.
    return newIdentity;
}

/*
 * Recompute all cross-compartment wrappers for an object, resetting state.
 * Gecko uses this to clear Xray wrappers when doing a navigation that reuses
 * the inner window and global object.
 */
JS_PUBLIC_API(bool)
JS_RefreshCrossCompartmentWrappers(JSContext *cx, HandleObject obj)
{
    return RemapAllWrappersForObject(cx, obj, obj);
}

JS_PUBLIC_API(bool)
JS_InitStandardClasses(JSContext *cx, HandleObject obj)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    assertSameCompartment(cx, obj);

    Rooted<GlobalObject*> global(cx, &obj->global());
    return GlobalObject::initStandardClasses(cx, global);
}

#define EAGER_ATOM(name)            NAME_OFFSET(name)

typedef struct JSStdName {
    size_t      atomOffset;     /* offset of atom pointer in JSAtomState */
    JSProtoKey  key;
    bool isDummy() const { return key == JSProto_Null; }
    bool isSentinel() const { return key == JSProto_LIMIT; }
} JSStdName;

static const JSStdName*
LookupStdName(JSRuntime *rt, HandleString name, const JSStdName *table)
{
    MOZ_ASSERT(name->isAtom());
    for (unsigned i = 0; !table[i].isSentinel(); i++) {
        if (table[i].isDummy())
            continue;
        JSAtom *atom = AtomStateOffsetToName(*rt->commonNames, table[i].atomOffset);
        MOZ_ASSERT(atom);
        if (name == atom)
            return &table[i];
    }

    return nullptr;
}

/*
 * Table of standard classes, indexed by JSProtoKey. For entries where the
 * JSProtoKey does not correspond to a class with a meaningful constructor, we
 * insert a null entry into the table.
 */
#define STD_NAME_ENTRY(name, code, init, clasp) { EAGER_ATOM(name), static_cast<JSProtoKey>(code) },
#define STD_DUMMY_ENTRY(name, code, init, dummy) { 0, JSProto_Null },
static const JSStdName standard_class_names[] = {
  JS_FOR_PROTOTYPES(STD_NAME_ENTRY, STD_DUMMY_ENTRY)
  { 0, JSProto_LIMIT }
};

/*
 * Table of top-level function and constant names and the JSProtoKey of the
 * standard class that initializes them.
 */
static const JSStdName builtin_property_names[] = {
    { EAGER_ATOM(eval), JSProto_Object },

    /* Global properties and functions defined by the Number class. */
    { EAGER_ATOM(NaN), JSProto_Number },
    { EAGER_ATOM(Infinity), JSProto_Number },
    { EAGER_ATOM(isNaN), JSProto_Number },
    { EAGER_ATOM(isFinite), JSProto_Number },
    { EAGER_ATOM(parseFloat), JSProto_Number },
    { EAGER_ATOM(parseInt), JSProto_Number },

    /* String global functions. */
    { EAGER_ATOM(escape), JSProto_String },
    { EAGER_ATOM(unescape), JSProto_String },
    { EAGER_ATOM(decodeURI), JSProto_String },
    { EAGER_ATOM(encodeURI), JSProto_String },
    { EAGER_ATOM(decodeURIComponent), JSProto_String },
    { EAGER_ATOM(encodeURIComponent), JSProto_String },
#if JS_HAS_UNEVAL
    { EAGER_ATOM(uneval), JSProto_String },
#endif
#ifdef ENABLE_BINARYDATA
    { EAGER_ATOM(SIMD), JSProto_SIMD },
    { EAGER_ATOM(TypedObject), JSProto_TypedObject },
#endif

    { 0, JSProto_LIMIT }
};

#undef EAGER_ATOM

JS_PUBLIC_API(bool)
JS_ResolveStandardClass(JSContext *cx, HandleObject obj, HandleId id, bool *resolved)
{
    JSRuntime *rt;
    const JSStdName *stdnm;

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);

    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
    *resolved = false;

    rt = cx->runtime();
    if (!rt->hasContexts() || !JSID_IS_ATOM(id))
        return true;

    RootedString idstr(cx, JSID_TO_STRING(id));

    /* Check whether we're resolving 'undefined', and define it if so. */
    JSAtom *undefinedAtom = cx->names().undefined;
    if (idstr == undefinedAtom) {
        *resolved = true;
        return JSObject::defineProperty(cx, obj, undefinedAtom->asPropertyName(),
                                        UndefinedHandleValue,
                                        JS_PropertyStub, JS_StrictPropertyStub,
                                        JSPROP_PERMANENT | JSPROP_READONLY);
    }

    /* Try for class constructors/prototypes named by well-known atoms. */
    stdnm = LookupStdName(rt, idstr, standard_class_names);

    /* Try less frequently used top-level functions and constants. */
    if (!stdnm)
        stdnm = LookupStdName(rt, idstr, builtin_property_names);

    // If this class is anonymous, then it doesn't exist as a global
    // property, so we won't resolve anything.
    JSProtoKey key = stdnm ? stdnm->key : JSProto_Null;
    if (key != JSProto_Null && !(ProtoKeyToClass(key)->flags & JSCLASS_IS_ANONYMOUS)) {
        if (!GlobalObject::ensureConstructor(cx, global, stdnm->key))
            return false;

        *resolved = true;
        return true;
    }

    // There is no such property to resolve. An ordinary resolve hook would
    // just return true at this point. But the global object is special in one
    // more way: its prototype chain is lazily initialized. That is,
    // global->getProto() might be null right now because we haven't created
    // Object.prototype yet. Force it now.
    if (!global->getOrCreateObjectPrototype(cx))
        return false;

    return true;
}

JS_PUBLIC_API(bool)
JS_EnumerateStandardClasses(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    MOZ_ASSERT(obj->is<GlobalObject>());
    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
    return GlobalObject::initStandardClasses(cx, global);
}

JS_PUBLIC_API(bool)
JS_GetClassObject(JSContext *cx, JSProtoKey key, MutableHandleObject objp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return GetBuiltinConstructor(cx, key, objp);
}

JS_PUBLIC_API(bool)
JS_GetClassPrototype(JSContext *cx, JSProtoKey key, MutableHandleObject objp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return GetBuiltinPrototype(cx, key, objp);
}

namespace JS {

JS_PUBLIC_API(void)
ProtoKeyToId(JSContext *cx, JSProtoKey key, MutableHandleId idp)
{
    idp.set(NameToId(ClassName(key, cx)));
}

} /* namespace JS */

JS_PUBLIC_API(JSProtoKey)
JS_IdToProtoKey(JSContext *cx, HandleId id)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    if (!JSID_IS_ATOM(id))
        return JSProto_Null;
    RootedString idstr(cx, JSID_TO_STRING(id));
    const JSStdName *stdnm = LookupStdName(cx->runtime(), idstr, standard_class_names);
    if (!stdnm)
        return JSProto_Null;

    MOZ_ASSERT(MOZ_ARRAY_LENGTH(standard_class_names) == JSProto_LIMIT + 1);
    return static_cast<JSProtoKey>(stdnm - standard_class_names);
}

JS_PUBLIC_API(JSObject *)
JS_GetObjectPrototype(JSContext *cx, HandleObject forObj)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, forObj);
    return forObj->global().getOrCreateObjectPrototype(cx);
}

JS_PUBLIC_API(JSObject *)
JS_GetFunctionPrototype(JSContext *cx, HandleObject forObj)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, forObj);
    return forObj->global().getOrCreateFunctionPrototype(cx);
}

JS_PUBLIC_API(JSObject *)
JS_GetArrayPrototype(JSContext *cx, HandleObject forObj)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, forObj);
    Rooted<GlobalObject*> global(cx, &forObj->global());
    return GlobalObject::getOrCreateArrayPrototype(cx, global);
}

JS_PUBLIC_API(JSObject *)
JS_GetErrorPrototype(JSContext *cx)
{
    CHECK_REQUEST(cx);
    Rooted<GlobalObject*> global(cx, cx->global());
    return GlobalObject::getOrCreateCustomErrorPrototype(cx, global, JSEXN_ERR);
}

JS_PUBLIC_API(JSObject *)
JS_GetGlobalForObject(JSContext *cx, JSObject *obj)
{
    AssertHeapIsIdle(cx);
    assertSameCompartment(cx, obj);
    return &obj->global();
}

extern JS_PUBLIC_API(bool)
JS_IsGlobalObject(JSObject *obj)
{
    return obj->is<GlobalObject>();
}

JS_PUBLIC_API(JSObject *)
JS_GetGlobalForCompartmentOrNull(JSContext *cx, JSCompartment *c)
{
    AssertHeapIsIdleOrIterating(cx);
    assertSameCompartment(cx, c);
    return c->maybeGlobal();
}

JS_PUBLIC_API(JSObject *)
JS::CurrentGlobalOrNull(JSContext *cx)
{
    AssertHeapIsIdleOrIterating(cx);
    CHECK_REQUEST(cx);
    if (!cx->compartment())
        return nullptr;
    return cx->global();
}

JS_PUBLIC_API(jsval)
JS_ComputeThis(JSContext *cx, jsval *vp)
{
    AssertHeapIsIdle(cx);
    assertSameCompartment(cx, JSValueArray(vp, 2));
    CallReceiver call = CallReceiverFromVp(vp);
    if (!BoxNonStrictThis(cx, call))
        return JSVAL_NULL;
    return call.thisv();
}

JS_PUBLIC_API(void *)
JS_malloc(JSContext *cx, size_t nbytes)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return static_cast<void *>(cx->runtime()->pod_malloc<uint8_t>(nbytes));
}

JS_PUBLIC_API(void *)
JS_realloc(JSContext *cx, void *p, size_t oldBytes, size_t newBytes)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return static_cast<void *>(cx->zone()->pod_realloc<uint8_t>(static_cast<uint8_t *>(p), oldBytes,
                                                                newBytes));
}

JS_PUBLIC_API(void)
JS_free(JSContext *cx, void *p)
{
    return js_free(p);
}

JS_PUBLIC_API(void)
JS_freeop(JSFreeOp *fop, void *p)
{
    return FreeOp::get(fop)->free_(p);
}

JS_PUBLIC_API(JSFreeOp *)
JS_GetDefaultFreeOp(JSRuntime *rt)
{
    return rt->defaultFreeOp();
}

JS_PUBLIC_API(void)
JS_updateMallocCounter(JSContext *cx, size_t nbytes)
{
    return cx->runtime()->updateMallocCounter(cx->zone(), nbytes);
}

JS_PUBLIC_API(char *)
JS_strdup(JSContext *cx, const char *s)
{
    AssertHeapIsIdle(cx);
    return DuplicateString(cx, s).release();
}

JS_PUBLIC_API(char *)
JS_strdup(JSRuntime *rt, const char *s)
{
    AssertHeapIsIdle(rt);
    size_t n = strlen(s) + 1;
    char *p = rt->pod_malloc<char>(n);
    if (!p)
        return nullptr;
    return static_cast<char*>(js_memcpy(p, s, n));
}

#undef JS_AddRoot

JS_PUBLIC_API(bool)
JS::AddValueRoot(JSContext *cx, JS::Heap<JS::Value> *vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return AddValueRoot(cx, vp->unsafeGet(), nullptr);
}

JS_PUBLIC_API(bool)
JS::AddStringRoot(JSContext *cx, JS::Heap<JSString *> *rp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return AddStringRoot(cx, rp->unsafeGet(), nullptr);
}

JS_PUBLIC_API(bool)
JS::AddObjectRoot(JSContext *cx, JS::Heap<JSObject *> *rp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return AddObjectRoot(cx, rp->unsafeGet(), nullptr);
}

JS_PUBLIC_API(bool)
JS::AddNamedValueRoot(JSContext *cx, JS::Heap<JS::Value> *vp, const char *name)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return AddValueRoot(cx, vp->unsafeGet(), name);
}

JS_PUBLIC_API(bool)
JS::AddNamedValueRootRT(JSRuntime *rt, JS::Heap<JS::Value> *vp, const char *name)
{
    return AddValueRootRT(rt, vp->unsafeGet(), name);
}

JS_PUBLIC_API(bool)
JS::AddNamedStringRoot(JSContext *cx, JS::Heap<JSString *> *rp, const char *name)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return AddStringRoot(cx, rp->unsafeGet(), name);
}

JS_PUBLIC_API(bool)
JS::AddNamedObjectRoot(JSContext *cx, JS::Heap<JSObject *> *rp, const char *name)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return AddObjectRoot(cx, rp->unsafeGet(), name);
}

JS_PUBLIC_API(bool)
JS::AddNamedScriptRoot(JSContext *cx, JS::Heap<JSScript *> *rp, const char *name)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return AddScriptRoot(cx, rp->unsafeGet(), name);
}

/* We allow unrooting from finalizers within the GC */

JS_PUBLIC_API(void)
JS::RemoveValueRoot(JSContext *cx, JS::Heap<JS::Value> *vp)
{
    CHECK_REQUEST(cx);
    RemoveRoot(cx->runtime(), (void *)vp);
    *vp = UndefinedValue();
}

JS_PUBLIC_API(void)
JS::RemoveStringRoot(JSContext *cx, JS::Heap<JSString *> *rp)
{
    CHECK_REQUEST(cx);
    RemoveRoot(cx->runtime(), (void *)rp);
    *rp = nullptr;
}

JS_PUBLIC_API(void)
JS::RemoveObjectRoot(JSContext *cx, JS::Heap<JSObject *> *rp)
{
    CHECK_REQUEST(cx);
    RemoveRoot(cx->runtime(), (void *)rp);
    *rp = nullptr;
}

JS_PUBLIC_API(void)
JS::RemoveScriptRoot(JSContext *cx, JS::Heap<JSScript *> *rp)
{
    CHECK_REQUEST(cx);
    RemoveRoot(cx->runtime(), (void *)rp);
    *rp = nullptr;
}

JS_PUBLIC_API(void)
JS::RemoveValueRootRT(JSRuntime *rt, JS::Heap<JS::Value> *vp)
{
    RemoveRoot(rt, (void *)vp);
    *vp = UndefinedValue();
}

JS_PUBLIC_API(void)
JS::RemoveStringRootRT(JSRuntime *rt, JS::Heap<JSString *> *rp)
{
    RemoveRoot(rt, (void *)rp);
    *rp = nullptr;
}

JS_PUBLIC_API(void)
JS::RemoveObjectRootRT(JSRuntime *rt, JS::Heap<JSObject *> *rp)
{
    RemoveRoot(rt, (void *)rp);
    *rp = nullptr;
}

JS_PUBLIC_API(void)
JS::RemoveScriptRootRT(JSRuntime *rt, JS::Heap<JSScript *> *rp)
{
    RemoveRoot(rt, (void *)rp);
    *rp = nullptr;
}

JS_PUBLIC_API(bool)
JS_AddExtraGCRootsTracer(JSRuntime *rt, JSTraceDataOp traceOp, void *data)
{
    return rt->gc.addBlackRootsTracer(traceOp, data);
}

JS_PUBLIC_API(void)
JS_RemoveExtraGCRootsTracer(JSRuntime *rt, JSTraceDataOp traceOp, void *data)
{
    return rt->gc.removeBlackRootsTracer(traceOp, data);
}

#ifdef DEBUG

typedef struct JSHeapDumpNode JSHeapDumpNode;

struct JSHeapDumpNode {
    void            *thing;
    JSGCTraceKind   kind;
    JSHeapDumpNode  *next;          /* next sibling */
    JSHeapDumpNode  *parent;        /* node with the thing that refer to thing
                                       from this node */
    char            edgeName[1];    /* name of the edge from parent->thing
                                       into thing */
};

typedef HashSet<void *, PointerHasher<void *, 3>, SystemAllocPolicy> VisitedSet;

class DumpingTracer
{
  public:
    DumpingTracer(JSRuntime *rt, JSTraceCallback callback)
      : base(rt, callback)
    {}

    JSTracer            base;
    VisitedSet          visited;
    bool                ok;
    void                *startThing;
    void                *thingToFind;
    void                *thingToIgnore;
    JSHeapDumpNode      *parentNode;
    JSHeapDumpNode      **lastNodep;
    char                buffer[200];
};

static void
DumpNotify(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    MOZ_ASSERT(trc->callback == DumpNotify);

    DumpingTracer *dtrc = (DumpingTracer *)trc;
    void *thing = *thingp;

    if (!dtrc->ok || thing == dtrc->thingToIgnore)
        return;

    /*
     * Check if we have already seen thing unless it is thingToFind to include
     * it to the graph each time we reach it and print all live things that
     * refer to thingToFind.
     *
     * This does not print all possible paths leading to thingToFind since
     * when a thing A refers directly or indirectly to thingToFind and A is
     * present several times in the graph, we will print only the first path
     * leading to A and thingToFind, other ways to reach A will be ignored.
     */
    if (dtrc->thingToFind != thing) {
        /*
         * The startThing check allows to avoid putting startThing into the
         * hash table before tracing startThing in JS_DumpHeap.
         */
        if (thing == dtrc->startThing)
            return;
        VisitedSet::AddPtr p = dtrc->visited.lookupForAdd(thing);
        if (p)
            return;
        if (!dtrc->visited.add(p, thing)) {
            dtrc->ok = false;
            return;
        }
    }

    const char *edgeName = dtrc->base.getTracingEdgeName(dtrc->buffer, sizeof(dtrc->buffer));
    size_t edgeNameSize = strlen(edgeName) + 1;
    size_t bytes = offsetof(JSHeapDumpNode, edgeName) + edgeNameSize;
    JSHeapDumpNode *node = (JSHeapDumpNode *) js_malloc(bytes);
    if (!node) {
        dtrc->ok = false;
        return;
    }

    node->thing = thing;
    node->kind = kind;
    node->next = nullptr;
    node->parent = dtrc->parentNode;
    js_memcpy(node->edgeName, edgeName, edgeNameSize);

    MOZ_ASSERT(!*dtrc->lastNodep);
    *dtrc->lastNodep = node;
    dtrc->lastNodep = &node->next;
}

/* Dump node and the chain that leads to thing it contains. */
static bool
DumpNode(DumpingTracer *dtrc, FILE* fp, JSHeapDumpNode *node)
{
    JSHeapDumpNode *prev, *following;
    size_t chainLimit;
    enum { MAX_PARENTS_TO_PRINT = 10 };

    JS_GetTraceThingInfo(dtrc->buffer, sizeof dtrc->buffer,
                         &dtrc->base, node->thing, node->kind, true);
    if (fprintf(fp, "%p %-22s via ", node->thing, dtrc->buffer) < 0)
        return false;

    /*
     * We need to print the parent chain in the reverse order. To do it in
     * O(N) time where N is the chain length we first reverse the chain while
     * searching for the top and then print each node while restoring the
     * chain order.
     */
    chainLimit = MAX_PARENTS_TO_PRINT;
    prev = nullptr;
    for (;;) {
        following = node->parent;
        node->parent = prev;
        prev = node;
        node = following;
        if (!node)
            break;
        if (chainLimit == 0) {
            if (fputs("...", fp) < 0)
                return false;
            break;
        }
        --chainLimit;
    }

    node = prev;
    prev = following;
    bool ok = true;
    do {
        /* Loop must continue even when !ok to restore the parent chain. */
        if (ok) {
            if (!prev) {
                /* Print edge from some runtime root or startThing. */
                if (fputs(node->edgeName, fp) < 0)
                    ok = false;
            } else {
                JS_GetTraceThingInfo(dtrc->buffer, sizeof dtrc->buffer,
                                     &dtrc->base, prev->thing, prev->kind,
                                     false);
                if (fprintf(fp, "(%p %s).%s",
                           prev->thing, dtrc->buffer, node->edgeName) < 0) {
                    ok = false;
                }
            }
        }
        following = node->parent;
        node->parent = prev;
        prev = node;
        node = following;
    } while (node);

    return ok && putc('\n', fp) >= 0;
}

JS_PUBLIC_API(bool)
JS_DumpHeap(JSRuntime *rt, FILE *fp, void* startThing, JSGCTraceKind startKind,
            void *thingToFind, size_t maxDepth, void *thingToIgnore)
{
    if (maxDepth == 0)
        return true;

    DumpingTracer dtrc(rt, DumpNotify);
    if (!dtrc.visited.init())
        return false;
    dtrc.ok = true;
    dtrc.startThing = startThing;
    dtrc.thingToFind = thingToFind;
    dtrc.thingToIgnore = thingToIgnore;
    dtrc.parentNode = nullptr;
    JSHeapDumpNode *node = nullptr;
    dtrc.lastNodep = &node;
    if (!startThing) {
        MOZ_ASSERT(startKind == JSTRACE_OBJECT);
        TraceRuntime(&dtrc.base);
    } else {
        JS_TraceChildren(&dtrc.base, startThing, startKind);
    }

    if (!node)
        return dtrc.ok;

    size_t depth = 1;
    JSHeapDumpNode *children, *next, *parent;
    bool thingToFindWasTraced = thingToFind && thingToFind == startThing;
    for (;;) {
        /*
         * Loop must continue even when !dtrc.ok to free all nodes allocated
         * so far.
         */
        if (dtrc.ok) {
            if (thingToFind == nullptr || thingToFind == node->thing)
                dtrc.ok = DumpNode(&dtrc, fp, node);

            /* Descend into children. */
            if (dtrc.ok &&
                depth < maxDepth &&
                (thingToFind != node->thing || !thingToFindWasTraced)) {
                dtrc.parentNode = node;
                children = nullptr;
                dtrc.lastNodep = &children;
                JS_TraceChildren(&dtrc.base, node->thing, node->kind);
                if (thingToFind == node->thing)
                    thingToFindWasTraced = true;
                if (children != nullptr) {
                    ++depth;
                    node = children;
                    continue;
                }
            }
        }

        /* Move to next or parents next and free the node. */
        for (;;) {
            next = node->next;
            parent = node->parent;
            js_free(node);
            node = next;
            if (node)
                break;
            if (!parent)
                return dtrc.ok;
            MOZ_ASSERT(depth > 1);
            --depth;
            node = parent;
        }
    }

    MOZ_ASSERT(depth == 1);
    return dtrc.ok;
}

#endif /* DEBUG */

extern JS_PUBLIC_API(bool)
JS_IsGCMarkingTracer(JSTracer *trc)
{
    return IS_GC_MARKING_TRACER(trc);
}

#ifdef DEBUG
extern JS_PUBLIC_API(bool)
JS_IsMarkingGray(JSTracer *trc)
{
    MOZ_ASSERT(JS_IsGCMarkingTracer(trc));
    return trc->callback == GCMarker::GrayCallback;
}
#endif

JS_PUBLIC_API(void)
JS_GC(JSRuntime *rt)
{
    AssertHeapIsIdle(rt);
    JS::PrepareForFullGC(rt);
    rt->gc.gc(GC_NORMAL, JS::gcreason::API);
}

JS_PUBLIC_API(void)
JS_MaybeGC(JSContext *cx)
{
    GCRuntime &gc = cx->runtime()->gc;
    if (!gc.maybeGC(cx->zone()))
        gc.maybePeriodicFullGC();
}

JS_PUBLIC_API(void)
JS_SetGCCallback(JSRuntime *rt, JSGCCallback cb, void *data)
{
    AssertHeapIsIdle(rt);
    rt->gc.setGCCallback(cb, data);
}

JS_PUBLIC_API(bool)
JS_AddFinalizeCallback(JSRuntime *rt, JSFinalizeCallback cb, void *data)
{
    AssertHeapIsIdle(rt);
    return rt->gc.addFinalizeCallback(cb, data);
}

JS_PUBLIC_API(void)
JS_RemoveFinalizeCallback(JSRuntime *rt, JSFinalizeCallback cb)
{
    rt->gc.removeFinalizeCallback(cb);
}

JS_PUBLIC_API(bool)
JS_AddWeakPointerCallback(JSRuntime *rt, JSWeakPointerCallback cb, void *data)
{
    AssertHeapIsIdle(rt);
    return rt->gc.addWeakPointerCallback(cb, data);
}

JS_PUBLIC_API(void)
JS_RemoveWeakPointerCallback(JSRuntime *rt, JSWeakPointerCallback cb)
{
    rt->gc.removeWeakPointerCallback(cb);
}

JS_PUBLIC_API(void)
JS_UpdateWeakPointerAfterGC(JS::Heap<JSObject *> *objp)
{
    JS_UpdateWeakPointerAfterGCUnbarriered(objp->unsafeGet());
}

JS_PUBLIC_API(void)
JS_UpdateWeakPointerAfterGCUnbarriered(JSObject **objp)
{
    if (IsObjectAboutToBeFinalized(objp))
        *objp = nullptr;
}

JS_PUBLIC_API(void)
JS_SetGCParameter(JSRuntime *rt, JSGCParamKey key, uint32_t value)
{
    rt->gc.setParameter(key, value);
}

JS_PUBLIC_API(uint32_t)
JS_GetGCParameter(JSRuntime *rt, JSGCParamKey key)
{
    return rt->gc.getParameter(key);
}

JS_PUBLIC_API(void)
JS_SetGCParameterForThread(JSContext *cx, JSGCParamKey key, uint32_t value)
{
    MOZ_ASSERT(key == JSGC_MAX_CODE_CACHE_BYTES);
}

JS_PUBLIC_API(uint32_t)
JS_GetGCParameterForThread(JSContext *cx, JSGCParamKey key)
{
    MOZ_ASSERT(key == JSGC_MAX_CODE_CACHE_BYTES);
    return 0;
}

static const size_t NumGCConfigs = 14;
struct JSGCConfig {
    JSGCParamKey key;
    uint32_t value;
};

JS_PUBLIC_API(void)
JS_SetGCParametersBasedOnAvailableMemory(JSRuntime *rt, uint32_t availMem)
{
    static const JSGCConfig minimal[NumGCConfigs] = {
        {JSGC_MAX_MALLOC_BYTES, 6 * 1024 * 1024},
        {JSGC_SLICE_TIME_BUDGET, 30},
        {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1500},
        {JSGC_HIGH_FREQUENCY_HIGH_LIMIT, 40},
        {JSGC_HIGH_FREQUENCY_LOW_LIMIT, 0},
        {JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX, 300},
        {JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN, 120},
        {JSGC_LOW_FREQUENCY_HEAP_GROWTH, 120},
        {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1500},
        {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1500},
        {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1500},
        {JSGC_ALLOCATION_THRESHOLD, 1},
        {JSGC_DECOMMIT_THRESHOLD, 1},
        {JSGC_MODE, JSGC_MODE_INCREMENTAL}
    };

    const JSGCConfig *config = minimal;
    if (availMem > 512) {
        static const JSGCConfig nominal[NumGCConfigs] = {
            {JSGC_MAX_MALLOC_BYTES, 6 * 1024 * 1024},
            {JSGC_SLICE_TIME_BUDGET, 30},
            {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1000},
            {JSGC_HIGH_FREQUENCY_HIGH_LIMIT, 500},
            {JSGC_HIGH_FREQUENCY_LOW_LIMIT, 100},
            {JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX, 300},
            {JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN, 150},
            {JSGC_LOW_FREQUENCY_HEAP_GROWTH, 150},
            {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1500},
            {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1500},
            {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1500},
            {JSGC_ALLOCATION_THRESHOLD, 30},
            {JSGC_DECOMMIT_THRESHOLD, 32},
            {JSGC_MODE, JSGC_MODE_COMPARTMENT}
        };

        config = nominal;
    }

    for (size_t i = 0; i < NumGCConfigs; i++)
        JS_SetGCParameter(rt, config[i].key, config[i].value);
}


JS_PUBLIC_API(JSString *)
JS_NewExternalString(JSContext *cx, const char16_t *chars, size_t length,
                     const JSStringFinalizer *fin)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JSString *s = JSExternalString::new_(cx, chars, length, fin);
    return s;
}

extern JS_PUBLIC_API(bool)
JS_IsExternalString(JSString *str)
{
    return str->isExternal();
}

extern JS_PUBLIC_API(const JSStringFinalizer *)
JS_GetExternalStringFinalizer(JSString *str)
{
    return str->asExternal().externalFinalizer();
}

static void
SetNativeStackQuota(JSRuntime *rt, StackKind kind, size_t stackSize)
{
    rt->nativeStackQuota[kind] = stackSize;
    if (rt->nativeStackBase)
        RecomputeStackLimit(rt, kind);
}

void
js::RecomputeStackLimit(JSRuntime *rt, StackKind kind)
{
    size_t stackSize = rt->nativeStackQuota[kind];
#if JS_STACK_GROWTH_DIRECTION > 0
    if (stackSize == 0) {
        rt->mainThread.nativeStackLimit[kind] = UINTPTR_MAX;
    } else {
        MOZ_ASSERT(rt->nativeStackBase <= size_t(-1) - stackSize);
        rt->mainThread.nativeStackLimit[kind] =
          rt->nativeStackBase + stackSize - 1;
    }
#else
    if (stackSize == 0) {
        rt->mainThread.nativeStackLimit[kind] = 0;
    } else {
        MOZ_ASSERT(rt->nativeStackBase >= stackSize);
        rt->mainThread.nativeStackLimit[kind] =
          rt->nativeStackBase - (stackSize - 1);
    }
#endif

    // If there's no pending interrupt request set on the runtime's main thread's
    // jitStackLimit, then update it so that it reflects the new nativeStacklimit.
    //
    // Note that, for now, we use the untrusted limit for ion. This is fine,
    // because it's the most conservative limit, and if we hit it, we'll bail
    // out of ion into the interpeter, which will do a proper recursion check.
    if (kind == StackForUntrustedScript) {
        JSRuntime::AutoLockForInterrupt lock(rt);
        if (rt->mainThread.jitStackLimit != uintptr_t(-1)) {
            rt->mainThread.jitStackLimit = rt->mainThread.nativeStackLimit[kind];
#if defined(JS_ARM_SIMULATOR) || defined(JS_MIPS_SIMULATOR)
            rt->mainThread.jitStackLimit = jit::Simulator::StackLimit();
#endif
        }
    }
}

JS_PUBLIC_API(void)
JS_SetNativeStackQuota(JSRuntime *rt, size_t systemCodeStackSize,
                       size_t trustedScriptStackSize,
                       size_t untrustedScriptStackSize)
{
    MOZ_ASSERT_IF(trustedScriptStackSize,
                  trustedScriptStackSize < systemCodeStackSize);
    if (!trustedScriptStackSize)
        trustedScriptStackSize = systemCodeStackSize;
    MOZ_ASSERT_IF(untrustedScriptStackSize,
                  untrustedScriptStackSize < trustedScriptStackSize);
    if (!untrustedScriptStackSize)
        untrustedScriptStackSize = trustedScriptStackSize;
    SetNativeStackQuota(rt, StackForSystemCode, systemCodeStackSize);
    SetNativeStackQuota(rt, StackForTrustedScript, trustedScriptStackSize);
    SetNativeStackQuota(rt, StackForUntrustedScript, untrustedScriptStackSize);
}

/************************************************************************/

JS_PUBLIC_API(int)
JS_IdArrayLength(JSContext *cx, JSIdArray *ida)
{
    return ida->length;
}

JS_PUBLIC_API(jsid)
JS_IdArrayGet(JSContext *cx, JSIdArray *ida, unsigned index)
{
    MOZ_ASSERT(index < unsigned(ida->length));
    return ida->vector[index];
}

JS_PUBLIC_API(void)
JS_DestroyIdArray(JSContext *cx, JSIdArray *ida)
{
    cx->runtime()->defaultFreeOp()->free_(ida);
}

JS_PUBLIC_API(bool)
JS_ValueToId(JSContext *cx, HandleValue value, MutableHandleId idp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    return ValueToId<CanGC>(cx, value, idp);
}

JS_PUBLIC_API(bool)
JS_StringToId(JSContext *cx, HandleString string, MutableHandleId idp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, string);
    RootedValue value(cx, StringValue(string));
    return ValueToId<CanGC>(cx, value, idp);
}

JS_PUBLIC_API(bool)
JS_IdToValue(JSContext *cx, jsid id, MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    vp.set(IdToValue(id));
    assertSameCompartment(cx, vp);
    return true;
}

JS_PUBLIC_API(bool)
JS_DefaultValue(JSContext *cx, HandleObject obj, JSType hint, MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    MOZ_ASSERT(obj != nullptr);
    MOZ_ASSERT(hint == JSTYPE_VOID || hint == JSTYPE_STRING || hint == JSTYPE_NUMBER);
    return JSObject::defaultValue(cx, obj, hint, vp);
}

JS_PUBLIC_API(bool)
JS_PropertyStub(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    return true;
}

JS_PUBLIC_API(bool)
JS_StrictPropertyStub(JSContext *cx, HandleObject obj, HandleId id, bool strict, MutableHandleValue vp)
{
    return true;
}

JS_PUBLIC_API(bool)
JS_DeletePropertyStub(JSContext *cx, HandleObject obj, HandleId id, bool *succeeded)
{
    *succeeded = true;
    return true;
}

JS_PUBLIC_API(bool)
JS_EnumerateStub(JSContext *cx, HandleObject obj)
{
    return true;
}

JS_PUBLIC_API(bool)
JS_ResolveStub(JSContext *cx, HandleObject obj, HandleId id)
{
    return true;
}

JS_PUBLIC_API(bool)
JS_ConvertStub(JSContext *cx, HandleObject obj, JSType type, MutableHandleValue vp)
{
    MOZ_ASSERT(type != JSTYPE_OBJECT && type != JSTYPE_FUNCTION);
    MOZ_ASSERT(obj);
    return DefaultValue(cx, obj, type, vp);
}

JS_PUBLIC_API(JSObject *)
JS_InitClass(JSContext *cx, HandleObject obj, HandleObject parent_proto,
             const JSClass *clasp, JSNative constructor, unsigned nargs,
             const JSPropertySpec *ps, const JSFunctionSpec *fs,
             const JSPropertySpec *static_ps, const JSFunctionSpec *static_fs)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, parent_proto);
    return js_InitClass(cx, obj, parent_proto, Valueify(clasp), constructor,
                        nargs, ps, fs, static_ps, static_fs);
}

JS_PUBLIC_API(bool)
JS_LinkConstructorAndPrototype(JSContext *cx, HandleObject ctor, HandleObject proto)
{
    return LinkConstructorAndPrototype(cx, ctor, proto);
}

JS_PUBLIC_API(const JSClass *)
JS_GetClass(JSObject *obj)
{
    return obj->getJSClass();
}

JS_PUBLIC_API(bool)
JS_InstanceOf(JSContext *cx, HandleObject obj, const JSClass *clasp, CallArgs *args)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
#ifdef DEBUG
    if (args) {
        assertSameCompartment(cx, obj);
        assertSameCompartment(cx, args->thisv(), args->calleev());
    }
#endif
    if (!obj || obj->getJSClass() != clasp) {
        if (args)
            ReportIncompatibleMethod(cx, *args, Valueify(clasp));
        return false;
    }
    return true;
}

JS_PUBLIC_API(bool)
JS_HasInstance(JSContext *cx, HandleObject obj, HandleValue value, bool *bp)
{
    AssertHeapIsIdle(cx);
    assertSameCompartment(cx, obj, value);
    return HasInstance(cx, obj, value, bp);
}

JS_PUBLIC_API(void *)
JS_GetPrivate(JSObject *obj)
{
    /* This function can be called by a finalizer. */
    return obj->fakeNativeGetPrivate();
}

JS_PUBLIC_API(void)
JS_SetPrivate(JSObject *obj, void *data)
{
    /* This function can be called by a finalizer. */
    obj->fakeNativeSetPrivate(data);
}

JS_PUBLIC_API(void *)
JS_GetInstancePrivate(JSContext *cx, HandleObject obj, const JSClass *clasp, CallArgs *args)
{
    if (!JS_InstanceOf(cx, obj, clasp, args))
        return nullptr;
    return obj->fakeNativeGetPrivate();
}

JS_PUBLIC_API(bool)
JS_GetPrototype(JSContext *cx, JS::Handle<JSObject*> obj, JS::MutableHandle<JSObject*> protop)
{
    return JSObject::getProto(cx, obj, protop);
}

JS_PUBLIC_API(bool)
JS_SetPrototype(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<JSObject*> proto)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, proto);

    bool succeeded;
    if (!JSObject::setProto(cx, obj, proto, &succeeded))
        return false;

    if (!succeeded) {
        RootedValue val(cx, ObjectValue(*obj));
        js_ReportValueError(cx, JSMSG_SETPROTOTYPEOF_FAIL, JSDVG_IGNORE_STACK, val, js::NullPtr());
        return false;
    }

    return true;
}

JS_PUBLIC_API(JSObject *)
JS_GetParent(JSObject *obj)
{
    MOZ_ASSERT(!obj->is<ScopeObject>());
    return obj->getParent();
}

JS_PUBLIC_API(bool)
JS_SetParent(JSContext *cx, HandleObject obj, HandleObject parent)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    MOZ_ASSERT(!obj->is<ScopeObject>());
    MOZ_ASSERT(parent || !obj->getParent());
    assertSameCompartment(cx, obj, parent);

    return JSObject::setParent(cx, obj, parent);
}

JS_PUBLIC_API(JSObject *)
JS_GetConstructor(JSContext *cx, HandleObject proto)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, proto);

    RootedValue cval(cx);
    if (!JSObject::getProperty(cx, proto, proto, cx->names().constructor, &cval))
        return nullptr;
    if (!IsFunctionObject(cval)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_NO_CONSTRUCTOR,
                             proto->getClass()->name);
        return nullptr;
    }
    return &cval.toObject();
}

namespace {

class AutoCompartmentRooter : private JS::CustomAutoRooter
{
  public:
    explicit AutoCompartmentRooter(JSContext *cx, JSCompartment *comp
                                   MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : CustomAutoRooter(cx), compartment(comp)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    operator JSCompartment *() {
        return compartment;
    }

    JSCompartment *operator->() {
        return compartment;
    }

  protected:
    virtual void trace(JSTracer *trc)
    {
        compartment->mark();
    }

  private:
    JSCompartment *compartment;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} /* anonymous namespace */

bool
JS::CompartmentOptions::extraWarnings(JSRuntime *rt) const
{
    return extraWarningsOverride_.get(rt->options().extraWarnings());
}

bool
JS::CompartmentOptions::extraWarnings(JSContext *cx) const
{
    return extraWarnings(cx->runtime());
}

JS::CompartmentOptions &
JS::CompartmentOptions::setZone(ZoneSpecifier spec)
{
    zone_.spec = spec;
    return *this;
}

JS::CompartmentOptions &
JS::CompartmentOptions::setSameZoneAs(JSObject *obj)
{
    zone_.pointer = static_cast<void *>(obj->zone());
    return *this;
}

JS::CompartmentOptions &
JS::CompartmentOptionsRef(JSCompartment *compartment)
{
    return compartment->options();
}

JS::CompartmentOptions &
JS::CompartmentOptionsRef(JSObject *obj)
{
    return obj->compartment()->options();
}

JS::CompartmentOptions &
JS::CompartmentOptionsRef(JSContext *cx)
{
    return cx->compartment()->options();
}

JS_PUBLIC_API(JSObject *)
JS_NewGlobalObject(JSContext *cx, const JSClass *clasp, JSPrincipals *principals,
                   JS::OnNewGlobalHookOption hookOption,
                   const JS::CompartmentOptions &options /* = JS::CompartmentOptions() */)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    MOZ_ASSERT(!cx->isExceptionPending());

    JSRuntime *rt = cx->runtime();

    Zone *zone;
    if (options.zoneSpecifier() == JS::SystemZone)
        zone = rt->gc.systemZone;
    else if (options.zoneSpecifier() == JS::FreshZone)
        zone = nullptr;
    else
        zone = static_cast<Zone *>(options.zonePointer());

    AutoCompartmentRooter compartment(cx, NewCompartment(cx, zone, principals, options));
    if (!compartment)
        return nullptr;

    // Lazily create the system zone.
    if (!rt->gc.systemZone && options.zoneSpecifier() == JS::SystemZone) {
        rt->gc.systemZone = compartment->zone();
        rt->gc.systemZone->isSystem = true;
    }

    Rooted<GlobalObject *> global(cx);
    {
        AutoCompartment ac(cx, compartment);
        global = GlobalObject::create(cx, Valueify(clasp));
    }

    if (!global)
        return nullptr;

    if (hookOption == JS::FireOnNewGlobalHook)
        JS_FireOnNewGlobalObject(cx, global);

    return global;
}

JS_PUBLIC_API(void)
JS_GlobalObjectTraceHook(JSTracer *trc, JSObject *global)
{
    MOZ_ASSERT(global->is<GlobalObject>());

    // Off thread parsing and compilation tasks create a dummy global which is then
    // merged back into the host compartment. Since it used to be a global, it will still
    // have this trace hook, but it does not have a meaning relative to its new compartment.
    // We can safely skip it.
    if (!global->isOwnGlobal())
        return;

    // Trace the compartment for any GC things that should only stick around if we know the
    // compartment is live.
    global->compartment()->trace(trc);

    JSTraceOp trace = global->compartment()->options().getTrace();
    if (trace)
        trace(trc, global);
}

JS_PUBLIC_API(void)
JS_FireOnNewGlobalObject(JSContext *cx, JS::HandleObject global)
{
    // This hook is infallible, because we don't really want arbitrary script
    // to be able to throw errors during delicate global creation routines.
    // This infallibility will eat OOM and slow script, but if that happens
    // we'll likely run up into them again soon in a fallible context.
    Rooted<js::GlobalObject*> globalObject(cx, &global->as<GlobalObject>());
    Debugger::onNewGlobalObject(cx, globalObject);
}

JS_PUBLIC_API(JSObject *)
JS_NewObject(JSContext *cx, const JSClass *jsclasp, HandleObject proto, HandleObject parent)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, proto, parent);

    const Class *clasp = Valueify(jsclasp);
    if (!clasp)
        clasp = &JSObject::class_;    /* default class is Object */

    MOZ_ASSERT(clasp != &JSFunction::class_);
    MOZ_ASSERT(!(clasp->flags & JSCLASS_IS_GLOBAL));

    JSObject *obj = NewObjectWithClassProto(cx, clasp, proto, parent);
    MOZ_ASSERT_IF(obj, obj->getParent());
    return obj;
}

JS_PUBLIC_API(JSObject *)
JS_NewObjectWithGivenProto(JSContext *cx, const JSClass *jsclasp, HandleObject proto, HandleObject parent)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, proto, parent);

    const Class *clasp = Valueify(jsclasp);
    if (!clasp)
        clasp = &JSObject::class_;    /* default class is Object */

    MOZ_ASSERT(clasp != &JSFunction::class_);
    MOZ_ASSERT(!(clasp->flags & JSCLASS_IS_GLOBAL));

    JSObject *obj = NewObjectWithGivenProto(cx, clasp, proto, parent);
    if (obj)
        MarkTypeObjectUnknownProperties(cx, obj->type());
    return obj;
}

JS_PUBLIC_API(JSObject *)
JS_NewObjectForConstructor(JSContext *cx, const JSClass *clasp, const CallArgs& args)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    Value callee = args.calleev();
    assertSameCompartment(cx, callee);
    RootedObject obj(cx, &callee.toObject());
    return CreateThis(cx, Valueify(clasp), obj);
}

JS_PUBLIC_API(bool)
JS_IsExtensible(JSContext *cx, HandleObject obj, bool *extensible)
{
    return JSObject::isExtensible(cx, obj, extensible);
}

JS_PUBLIC_API(bool)
JS_IsNative(JSObject *obj)
{
    return obj->isNative();
}

JS_PUBLIC_API(JSRuntime *)
JS_GetObjectRuntime(JSObject *obj)
{
    return obj->compartment()->runtimeFromMainThread();
}

JS_PUBLIC_API(bool)
JS_FreezeObject(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return JSObject::freeze(cx, obj);
}

JS_PUBLIC_API(bool)
JS_DeepFreezeObject(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    /* Assume that non-extensible objects are already deep-frozen, to avoid divergence. */
    bool extensible;
    if (!JSObject::isExtensible(cx, obj, &extensible))
        return false;
    if (!extensible)
        return true;

    if (!JSObject::freeze(cx, obj))
        return false;

    /* Walk slots in obj and if any value is a non-null object, seal it. */
    for (uint32_t i = 0, n = obj->fakeNativeSlotSpan(); i < n; ++i) {
        const Value &v = obj->fakeNativeGetSlot(i);
        if (v.isPrimitive())
            continue;
        RootedObject obj(cx, &v.toObject());
        if (!JS_DeepFreezeObject(cx, obj))
            return false;
    }

    return true;
}

static bool
LookupPropertyById(JSContext *cx, HandleObject obj, HandleId id,
                   MutableHandleObject objp, MutableHandleShape propp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);

    return JSObject::lookupGeneric(cx, obj, id, objp, propp);
}

#define AUTO_NAMELEN(s,n)   (((n) == (size_t)-1) ? js_strlen(s) : (n))

static bool
LookupResult(JSContext *cx, HandleObject obj, HandleObject obj2, HandleId id,
             HandleShape shape, MutableHandleValue vp)
{
    if (!shape) {
        /* XXX bad API: no way to tell "not defined" from "void value" */
        vp.setUndefined();
        return true;
    }

    if (!obj2->isNative()) {
        if (obj2->is<ProxyObject>()) {
            Rooted<PropertyDescriptor> desc(cx);
            if (!Proxy::getPropertyDescriptor(cx, obj2, id, &desc))
                return false;
            if (!desc.isShared()) {
                vp.set(desc.value());
                return true;
            }
        }
    } else if (IsImplicitDenseOrTypedArrayElement(shape)) {
        vp.set(obj2->as<NativeObject>().getDenseOrTypedArrayElement(JSID_TO_INT(id)));
        return true;
    } else {
        /* Peek at the native property's slot value, without doing a Get. */
        if (shape->hasSlot()) {
            vp.set(obj2->as<NativeObject>().getSlot(shape->slot()));
            return true;
        }
    }

    /* XXX bad API: no way to return "defined but value unknown" */
    vp.setBoolean(true);
    return true;
}

JS_PUBLIC_API(bool)
JS_LookupPropertyById(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    RootedObject obj2(cx);
    RootedShape prop(cx);

    return LookupPropertyById(cx, obj, id, &obj2, &prop) &&
           LookupResult(cx, obj, obj2, id, prop, vp);
}

JS_PUBLIC_API(bool)
JS_LookupElement(JSContext *cx, HandleObject obj, uint32_t index, MutableHandleValue vp)
{
    CHECK_REQUEST(cx);
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return JS_LookupPropertyById(cx, obj, id, vp);
}

JS_PUBLIC_API(bool)
JS_LookupProperty(JSContext *cx, HandleObject objArg, const char *name, MutableHandleValue vp)
{
    RootedObject obj(cx, objArg);
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;

    RootedId id(cx, AtomToId(atom));
    return JS_LookupPropertyById(cx, obj, id, vp);
}

JS_PUBLIC_API(bool)
JS_LookupUCProperty(JSContext *cx, HandleObject objArg, const char16_t *name, size_t namelen,
                    MutableHandleValue vp)
{
    RootedObject obj(cx, objArg);
    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return false;

    RootedId id(cx, AtomToId(atom));
    return JS_LookupPropertyById(cx, obj, id, vp);
}

JS_PUBLIC_API(bool)
JS_HasPropertyById(JSContext *cx, HandleObject obj, HandleId id, bool *foundp)
{
    RootedObject obj2(cx);
    RootedShape prop(cx);
    bool ok = LookupPropertyById(cx, obj, id, &obj2, &prop);
    *foundp = (prop != nullptr);
    return ok;
}

JS_PUBLIC_API(bool)
JS_HasElement(JSContext *cx, HandleObject obj, uint32_t index, bool *foundp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return JS_HasPropertyById(cx, obj, id, foundp);
}

JS_PUBLIC_API(bool)
JS_HasProperty(JSContext *cx, HandleObject obj, const char *name, bool *foundp)
{
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_HasPropertyById(cx, obj, id, foundp);
}

JS_PUBLIC_API(bool)
JS_HasUCProperty(JSContext *cx, HandleObject obj, const char16_t *name, size_t namelen, bool *foundp)
{
    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_HasPropertyById(cx, obj, id, foundp);
}

JS_PUBLIC_API(bool)
JS_AlreadyHasOwnPropertyById(JSContext *cx, HandleObject obj, HandleId id, bool *foundp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);

    if (!obj->isNative()) {
        RootedObject obj2(cx);
        RootedShape prop(cx);

        if (!LookupPropertyById(cx, obj, id, &obj2, &prop))
            return false;
        *foundp = (obj == obj2);
        return true;
    }

    // Check for an existing native property on the object. Be careful not to
    // call any lookup or resolve hooks.
    if (JSID_IS_INT(id)) {
        uint32_t index = JSID_TO_INT(id);

        if (obj->as<NativeObject>().containsDenseElement(index)) {
            *foundp = true;
            return true;
        }

        if (IsAnyTypedArray(obj) && index < AnyTypedArrayLength(obj)) {
            *foundp = true;
            return true;
        }
    }

    *foundp = obj->as<NativeObject>().contains(cx, id);
    return true;
}

JS_PUBLIC_API(bool)
JS_AlreadyHasOwnElement(JSContext *cx, HandleObject obj, uint32_t index, bool *foundp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return JS_AlreadyHasOwnPropertyById(cx, obj, id, foundp);
}

JS_PUBLIC_API(bool)
JS_AlreadyHasOwnProperty(JSContext *cx, HandleObject obj, const char *name, bool *foundp)
{
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_AlreadyHasOwnPropertyById(cx, obj, id, foundp);
}

JS_PUBLIC_API(bool)
JS_AlreadyHasOwnUCProperty(JSContext *cx, HandleObject obj, const char16_t *name, size_t namelen,
                           bool *foundp)
{
    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_AlreadyHasOwnPropertyById(cx, obj, id, foundp);
}

/* Wrapper functions to create wrappers with no corresponding JSJitInfo from API
 * function arguments.
 */
static JSPropertyOpWrapper
GetterWrapper(JSPropertyOp getter)
{
    JSPropertyOpWrapper ret;
    ret.op = getter;
    ret.info = nullptr;
    return ret;
}

static JSStrictPropertyOpWrapper
SetterWrapper(JSStrictPropertyOp setter)
{
    JSStrictPropertyOpWrapper ret;
    ret.op = setter;
    ret.info = nullptr;
    return ret;
}

static bool
DefinePropertyById(JSContext *cx, HandleObject obj, HandleId id, HandleValue value,
                   const JSPropertyOpWrapper &get, const JSStrictPropertyOpWrapper &set,
                   unsigned attrs, unsigned flags)
{
    PropertyOp getter = get.op;
    StrictPropertyOp setter = set.op;

    // JSPROP_READONLY has no meaning when accessors are involved. Ideally we'd
    // throw if this happens, but we've accepted it for long enough that it's
    // not worth trying to make callers change their ways. Just flip it off on
    // its way through the API layer so that we can enforce this internally.
    if (attrs & (JSPROP_GETTER | JSPROP_SETTER))
        attrs &= ~JSPROP_READONLY;

    // When we use DefineProperty, we need full scriptable Function objects rather
    // than JSNatives. However, we might be pulling this property descriptor off
    // of something with JSNative property descriptors. If we are, wrap them in
    // JS Function objects.
    if (attrs & JSPROP_NATIVE_ACCESSORS) {
        MOZ_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
        JSFunction::Flags zeroFlags = JSAPIToJSFunctionFlags(0);

        // We can't just use JS_NewFunctionById here because it assumes a
        // string id.
        RootedAtom atom(cx, JSID_IS_ATOM(id) ? JSID_TO_ATOM(id) : nullptr);
        attrs &= ~JSPROP_NATIVE_ACCESSORS;
        if (getter) {
            RootedObject global(cx, (JSObject*) &obj->global());
            JSFunction *getobj = NewFunction(cx, NullPtr(), (Native) getter, 0,
                                             zeroFlags, global, atom);
            if (!getobj)
                return false;

            if (get.info)
                getobj->setJitInfo(get.info);

            getter = JS_DATA_TO_FUNC_PTR(PropertyOp, getobj);
            attrs |= JSPROP_GETTER;
        }
        if (setter) {
            // Root just the getter, since the setter is not yet a JSObject.
            AutoRooterGetterSetter getRoot(cx, JSPROP_GETTER, &getter, nullptr);
            RootedObject global(cx, (JSObject*) &obj->global());
            JSFunction *setobj = NewFunction(cx, NullPtr(), (Native) setter, 1,
                                             zeroFlags, global, atom);
            if (!setobj)
                return false;

            if (set.info)
                setobj->setJitInfo(set.info);

            setter = JS_DATA_TO_FUNC_PTR(StrictPropertyOp, setobj);
            attrs |= JSPROP_SETTER;
        }
    }


    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id, value,
                            (attrs & JSPROP_GETTER)
                            ? JS_FUNC_TO_DATA_PTR(JSObject *, getter)
                            : nullptr,
                            (attrs & JSPROP_SETTER)
                            ? JS_FUNC_TO_DATA_PTR(JSObject *, setter)
                            : nullptr);

    return JSObject::defineGeneric(cx, obj, id, value, getter, setter, attrs);
}

JS_PUBLIC_API(bool)
JS_DefinePropertyById(JSContext *cx, HandleObject obj, HandleId id, HandleValue value,
                      unsigned attrs, JSPropertyOp getter, JSStrictPropertyOp setter)
{
    return DefinePropertyById(cx, obj, id, value, GetterWrapper(getter), SetterWrapper(setter),
                              attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefinePropertyById(JSContext *cx, HandleObject obj, HandleId id, HandleObject valueArg,
                      unsigned attrs, JSPropertyOp getter, JSStrictPropertyOp setter)
{
    RootedValue value(cx, ObjectValue(*valueArg));
    return DefinePropertyById(cx, obj, id, value, GetterWrapper(getter), SetterWrapper(setter),
                              attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefinePropertyById(JSContext *cx, HandleObject obj, HandleId id, HandleString valueArg,
                      unsigned attrs, JSPropertyOp getter, JSStrictPropertyOp setter)
{
    RootedValue value(cx, StringValue(valueArg));
    return DefinePropertyById(cx, obj, id, value, GetterWrapper(getter), SetterWrapper(setter),
                              attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefinePropertyById(JSContext *cx, HandleObject obj, HandleId id, int32_t valueArg,
                      unsigned attrs, JSPropertyOp getter, JSStrictPropertyOp setter)
{
    Value value = Int32Value(valueArg);
    return DefinePropertyById(cx, obj, id, HandleValue::fromMarkedLocation(&value),
                              GetterWrapper(getter), SetterWrapper(setter), attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefinePropertyById(JSContext *cx, HandleObject obj, HandleId id, uint32_t valueArg,
                      unsigned attrs, JSPropertyOp getter, JSStrictPropertyOp setter)
{
    Value value = UINT_TO_JSVAL(valueArg);
    return DefinePropertyById(cx, obj, id, HandleValue::fromMarkedLocation(&value),
                              GetterWrapper(getter), SetterWrapper(setter), attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefinePropertyById(JSContext *cx, HandleObject obj, HandleId id, double valueArg,
                      unsigned attrs, JSPropertyOp getter, JSStrictPropertyOp setter)
{
    Value value = NumberValue(valueArg);
    return DefinePropertyById(cx, obj, id, HandleValue::fromMarkedLocation(&value),
                              GetterWrapper(getter), SetterWrapper(setter), attrs, 0);
}

static bool
DefineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue value,
              unsigned attrs, JSPropertyOp getter, JSStrictPropertyOp setter)
{
    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return DefinePropertyById(cx, obj, id, value, GetterWrapper(getter), SetterWrapper(setter),
                              attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue value,
                 unsigned attrs, JSPropertyOp getter, JSStrictPropertyOp setter)
{
    return DefineElement(cx, obj, index, value, attrs, getter, setter);
}

JS_PUBLIC_API(bool)
JS_DefineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleObject valueArg,
                 unsigned attrs, JSPropertyOp getter, JSStrictPropertyOp setter)
{
    RootedValue value(cx, ObjectValue(*valueArg));
    return DefineElement(cx, obj, index, value, attrs, getter, setter);
}

JS_PUBLIC_API(bool)
JS_DefineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleString valueArg,
                 unsigned attrs, JSPropertyOp getter, JSStrictPropertyOp setter)
{
    RootedValue value(cx, StringValue(valueArg));
    return DefineElement(cx, obj, index, value, attrs, getter, setter);
}

JS_PUBLIC_API(bool)
JS_DefineElement(JSContext *cx, HandleObject obj, uint32_t index, int32_t valueArg,
                 unsigned attrs, JSPropertyOp getter, JSStrictPropertyOp setter)
{
    Value value = Int32Value(valueArg);
    return DefineElement(cx, obj, index, HandleValue::fromMarkedLocation(&value),
                         attrs, getter, setter);
}

JS_PUBLIC_API(bool)
JS_DefineElement(JSContext *cx, HandleObject obj, uint32_t index, uint32_t valueArg,
                 unsigned attrs, JSPropertyOp getter, JSStrictPropertyOp setter)
{
    Value value = UINT_TO_JSVAL(valueArg);
    return DefineElement(cx, obj, index, HandleValue::fromMarkedLocation(&value),
                         attrs, getter, setter);
}

JS_PUBLIC_API(bool)
JS_DefineElement(JSContext *cx, HandleObject obj, uint32_t index, double valueArg,
                 unsigned attrs, JSPropertyOp getter, JSStrictPropertyOp setter)
{
    Value value = NumberValue(valueArg);
    return DefineElement(cx, obj, index, HandleValue::fromMarkedLocation(&value),
                         attrs, getter, setter);
}

static bool
DefineProperty(JSContext *cx, HandleObject obj, const char *name, HandleValue value,
               const JSPropertyOpWrapper &getter, const JSStrictPropertyOpWrapper &setter,
               unsigned attrs, unsigned flags)
{
    AutoRooterGetterSetter gsRoot(cx, attrs, const_cast<JSPropertyOp *>(&getter.op),
                                  const_cast<JSStrictPropertyOp *>(&setter.op));

    RootedId id(cx);
    if (attrs & JSPROP_INDEX) {
        id.set(INT_TO_JSID(intptr_t(name)));
        attrs &= ~JSPROP_INDEX;
    } else {
        JSAtom *atom = Atomize(cx, name, strlen(name));
        if (!atom)
            return false;
        id = AtomToId(atom);
    }

    return DefinePropertyById(cx, obj, id, value, getter, setter, attrs, flags);
}


static bool
DefineSelfHostedProperty(JSContext *cx,
                         HandleObject obj,
                         const char *name,
                         const char *getterName,
                         const char *setterName,
                         unsigned attrs,
                         unsigned flags)
{
    RootedAtom nameAtom(cx, Atomize(cx, name, strlen(name)));
    if (!nameAtom)
        return false;

    RootedAtom getterNameAtom(cx, Atomize(cx, getterName, strlen(getterName)));
    if (!getterNameAtom)
        return false;

    RootedValue getterValue(cx);
    if (!cx->global()->getSelfHostedFunction(cx, getterNameAtom, nameAtom,
                                             0, &getterValue))
    {
        return false;
    }
    MOZ_ASSERT(getterValue.isObject() && getterValue.toObject().is<JSFunction>());
    RootedFunction getterFunc(cx, &getterValue.toObject().as<JSFunction>());
    JSPropertyOp getterOp = JS_DATA_TO_FUNC_PTR(PropertyOp, getterFunc.get());

    RootedFunction setterFunc(cx);
    if (setterName) {
        RootedAtom setterNameAtom(cx, Atomize(cx, setterName, strlen(setterName)));
        if (!setterNameAtom)
            return false;

        RootedValue setterValue(cx);
        if (!cx->global()->getSelfHostedFunction(cx, setterNameAtom, nameAtom,
                                                 0, &setterValue))
        {
            return false;
        }
        MOZ_ASSERT(setterValue.isObject() && setterValue.toObject().is<JSFunction>());
        setterFunc = &getterValue.toObject().as<JSFunction>();
    }
    JSStrictPropertyOp setterOp = JS_DATA_TO_FUNC_PTR(StrictPropertyOp, setterFunc.get());

    return DefineProperty(cx, obj, name, JS::UndefinedHandleValue,
                          GetterWrapper(getterOp), SetterWrapper(setterOp),
                          attrs, flags);
}

JS_PUBLIC_API(bool)
JS_DefineProperty(JSContext *cx, HandleObject obj, const char *name, HandleValue value,
                  unsigned attrs,
                  PropertyOp getter /* = nullptr */, JSStrictPropertyOp setter /* = nullptr */)
{
    return DefineProperty(cx, obj, name, value, GetterWrapper(getter), SetterWrapper(setter),
                          attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefineProperty(JSContext *cx, HandleObject obj, const char *name, HandleObject valueArg,
                  unsigned attrs,
                  PropertyOp getter /* = nullptr */, JSStrictPropertyOp setter /* = nullptr */)
{
    RootedValue value(cx, ObjectValue(*valueArg));
    return DefineProperty(cx, obj, name, value, GetterWrapper(getter), SetterWrapper(setter),
                          attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefineProperty(JSContext *cx, HandleObject obj, const char *name, HandleString valueArg,
                  unsigned attrs,
                  PropertyOp getter /* = nullptr */, JSStrictPropertyOp setter /* = nullptr */)
{
    RootedValue value(cx, StringValue(valueArg));
    return DefineProperty(cx, obj, name, value, GetterWrapper(getter), SetterWrapper(setter),
                          attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefineProperty(JSContext *cx, HandleObject obj, const char *name, int32_t valueArg,
                  unsigned attrs,
                  PropertyOp getter /* = nullptr */, JSStrictPropertyOp setter /* = nullptr */)
{
    Value value = Int32Value(valueArg);
    return DefineProperty(cx, obj, name, HandleValue::fromMarkedLocation(&value),
                          GetterWrapper(getter), SetterWrapper(setter), attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefineProperty(JSContext *cx, HandleObject obj, const char *name, uint32_t valueArg,
                  unsigned attrs,
                  PropertyOp getter /* = nullptr */, JSStrictPropertyOp setter /* = nullptr */)
{
    Value value = UINT_TO_JSVAL(valueArg);
    return DefineProperty(cx, obj, name, HandleValue::fromMarkedLocation(&value),
                          GetterWrapper(getter), SetterWrapper(setter), attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefineProperty(JSContext *cx, HandleObject obj, const char *name, double valueArg,
                  unsigned attrs,
                  PropertyOp getter /* = nullptr */, JSStrictPropertyOp setter /* = nullptr */)
{
    Value value = NumberValue(valueArg);
    return DefineProperty(cx, obj, name, HandleValue::fromMarkedLocation(&value),
                          GetterWrapper(getter), SetterWrapper(setter), attrs, 0);
}

static bool
DefineUCProperty(JSContext *cx, HandleObject obj, const char16_t *name, size_t namelen,
                 const Value &value_, PropertyOp getter, StrictPropertyOp setter, unsigned attrs,
                 unsigned flags)
{
    RootedValue value(cx, value_);
    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);
    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return DefinePropertyById(cx, obj, id, value, GetterWrapper(getter), SetterWrapper(setter),
                              attrs, flags);
}

JS_PUBLIC_API(bool)
JS_DefineUCProperty(JSContext *cx, HandleObject obj, const char16_t *name, size_t namelen,
                    HandleValue value, unsigned attrs,
                    JSPropertyOp getter, JSStrictPropertyOp setter)
{
    return DefineUCProperty(cx, obj, name, namelen, value, getter, setter, attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefineUCProperty(JSContext *cx, HandleObject obj, const char16_t *name, size_t namelen,
                    HandleObject valueArg, unsigned attrs,
                    JSPropertyOp getter, JSStrictPropertyOp setter)
{
    RootedValue value(cx, ObjectValue(*valueArg));
    return DefineUCProperty(cx, obj, name, namelen, value, getter, setter, attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefineUCProperty(JSContext *cx, HandleObject obj, const char16_t *name, size_t namelen,
                    HandleString valueArg, unsigned attrs,
                    JSPropertyOp getter, JSStrictPropertyOp setter)
{
    RootedValue value(cx, StringValue(valueArg));
    return DefineUCProperty(cx, obj, name, namelen, value, getter, setter, attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefineUCProperty(JSContext *cx, HandleObject obj, const char16_t *name, size_t namelen,
                    int32_t valueArg, unsigned attrs,
                    JSPropertyOp getter, JSStrictPropertyOp setter)
{
    Value value = Int32Value(valueArg);
    return DefineUCProperty(cx, obj, name, namelen, HandleValue::fromMarkedLocation(&value),
                            getter, setter, attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefineUCProperty(JSContext *cx, HandleObject obj, const char16_t *name, size_t namelen,
                    uint32_t valueArg, unsigned attrs,
                    JSPropertyOp getter, JSStrictPropertyOp setter)
{
    Value value = UINT_TO_JSVAL(valueArg);
    return DefineUCProperty(cx, obj, name, namelen, HandleValue::fromMarkedLocation(&value),
                            getter, setter, attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefineUCProperty(JSContext *cx, HandleObject obj, const char16_t *name, size_t namelen,
                    double valueArg, unsigned attrs,
                    JSPropertyOp getter, JSStrictPropertyOp setter)
{
    Value value = NumberValue(valueArg);
    return DefineUCProperty(cx, obj, name, namelen, HandleValue::fromMarkedLocation(&value),
                            getter, setter, attrs, 0);
}

JS_PUBLIC_API(JSObject *)
JS_DefineObject(JSContext *cx, HandleObject obj, const char *name, const JSClass *jsclasp,
                HandleObject proto, unsigned attrs)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, proto);

    const Class *clasp = Valueify(jsclasp);
    if (!clasp)
        clasp = &JSObject::class_;    /* default class is Object */

    RootedObject nobj(cx, NewObjectWithClassProto(cx, clasp, proto, obj));
    if (!nobj)
        return nullptr;

    RootedValue nobjValue(cx, ObjectValue(*nobj));
    if (!DefineProperty(cx, obj, name, nobjValue, GetterWrapper(nullptr), SetterWrapper(nullptr),
                        attrs, 0)) {
        return nullptr;
    }

    return nobj;
}

static inline Value
ValueFromScalar(double x)
{
    return DoubleValue(x);
}
static inline Value
ValueFromScalar(int32_t x)
{
    return Int32Value(x);
}

template<typename T>
static bool
DefineConstScalar(JSContext *cx, HandleObject obj, const JSConstScalarSpec<T> *cds)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JSPropertyOpWrapper noget = GetterWrapper(nullptr);
    JSStrictPropertyOpWrapper noset = SetterWrapper(nullptr);
    unsigned attrs = JSPROP_READONLY | JSPROP_PERMANENT;
    for (; cds->name; cds++) {
        RootedValue value(cx, ValueFromScalar(cds->val));
        if (!DefineProperty(cx, obj, cds->name, value, noget, noset, attrs, 0))
            return false;
    }
    return true;
}

JS_PUBLIC_API(bool)
JS_DefineConstDoubles(JSContext *cx, HandleObject obj, const JSConstDoubleSpec *cds)
{
    return DefineConstScalar(cx, obj, cds);
}
JS_PUBLIC_API(bool)
JS_DefineConstIntegers(JSContext *cx, HandleObject obj, const JSConstIntegerSpec *cis)
{
    return DefineConstScalar(cx, obj, cis);
}

JS_PUBLIC_API(bool)
JS_DefineProperties(JSContext *cx, HandleObject obj, const JSPropertySpec *ps)
{
    bool ok;
    for (ok = true; ps->name; ps++) {
        if (ps->flags & JSPROP_NATIVE_ACCESSORS) {
            // If you declare native accessors, then you should have a native
            // getter.
            MOZ_ASSERT(ps->getter.propertyOp.op);
            // If you do not have a self-hosted getter, you should not have a
            // self-hosted setter. This is the closest approximation to that
            // assertion we can have with our setup.
            MOZ_ASSERT_IF(ps->setter.propertyOp.info, ps->setter.propertyOp.op);

            ok = DefineProperty(cx, obj, ps->name, JS::UndefinedHandleValue,
                                ps->getter.propertyOp, ps->setter.propertyOp, ps->flags, 0);
        } else {
            // If you have self-hosted getter/setter, you can't have a
            // native one.
            MOZ_ASSERT(!ps->getter.propertyOp.op && !ps->setter.propertyOp.op);
            MOZ_ASSERT(ps->flags & JSPROP_GETTER);

            ok = DefineSelfHostedProperty(cx, obj, ps->name,
                                          ps->getter.selfHosted.funname,
                                          ps->setter.selfHosted.funname,
                                          ps->flags, 0);
        }
        if (!ok)
            break;
    }
    return ok;
}

JS_PUBLIC_API(bool)
JS::ParsePropertyDescriptorObject(JSContext *cx,
                                  HandleObject obj,
                                  HandleValue descObj,
                                  MutableHandle<JSPropertyDescriptor> desc)
{
    Rooted<PropDesc> d(cx);
    if (!d.initialize(cx, descObj))
        return false;
    d.populatePropertyDescriptor(obj, desc);
    return true;
}

static bool
GetPropertyDescriptorById(JSContext *cx, HandleObject obj, HandleId id,
                          MutableHandle<PropertyDescriptor> desc)
{
    RootedObject obj2(cx);
    RootedShape shape(cx);

    if (!LookupPropertyById(cx, obj, id, &obj2, &shape))
        return false;

    desc.clear();
    if (!shape)
        return true;

    desc.object().set(obj2);
    if (obj2->isNative()) {
        if (IsImplicitDenseOrTypedArrayElement(shape)) {
            desc.setEnumerable();
            desc.value().set(obj2->as<NativeObject>().getDenseOrTypedArrayElement(JSID_TO_INT(id)));
        } else {
            desc.setAttributes(shape->attributes());
            desc.setGetter(shape->getter());
            desc.setSetter(shape->setter());
            MOZ_ASSERT(desc.value().isUndefined());
            if (shape->hasSlot())
                desc.value().set(obj2->as<NativeObject>().getSlot(shape->slot()));
        }
    } else {
        if (obj2->is<ProxyObject>())
            return Proxy::getPropertyDescriptor(cx, obj2, id, desc);
        if (!JSObject::getGenericAttributes(cx, obj2, id, &desc.attributesRef()))
            return false;
        MOZ_ASSERT(desc.getter() == nullptr);
        MOZ_ASSERT(desc.setter() == nullptr);
        MOZ_ASSERT(desc.value().isUndefined());
    }
    return true;
}

JS_PUBLIC_API(bool)
JS_GetOwnPropertyDescriptorById(JSContext *cx, HandleObject obj, HandleId id,
                                MutableHandle<JSPropertyDescriptor> desc)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    return GetOwnPropertyDescriptor(cx, obj, id, desc);
}

JS_PUBLIC_API(bool)
JS_GetOwnPropertyDescriptor(JSContext *cx, HandleObject obj, const char *name,
                            MutableHandle<JSPropertyDescriptor> desc)
{
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_GetOwnPropertyDescriptorById(cx, obj, id, desc);
}

JS_PUBLIC_API(bool)
JS_GetPropertyDescriptorById(JSContext *cx, HandleObject obj, HandleId id,
                             MutableHandle<JSPropertyDescriptor> desc)
{
    return GetPropertyDescriptorById(cx, obj, id, desc);
}

JS_PUBLIC_API(bool)
JS_GetPropertyDescriptor(JSContext *cx, HandleObject obj, const char *name,
                         MutableHandle<JSPropertyDescriptor> desc)
{
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return atom && JS_GetPropertyDescriptorById(cx, obj, id, desc);
}

JS_PUBLIC_API(bool)
JS_GetPropertyById(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    return JS_ForwardGetPropertyTo(cx, obj, id, obj, vp);
}

JS_PUBLIC_API(bool)
JS_ForwardGetPropertyTo(JSContext *cx, JS::HandleObject obj, JS::HandleId id, JS::HandleObject onBehalfOf,
                        JS::MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);
    assertSameCompartment(cx, onBehalfOf);

    return JSObject::getGeneric(cx, obj, onBehalfOf, id, vp);
}

JS_PUBLIC_API(bool)
JS_GetElement(JSContext *cx, HandleObject objArg, uint32_t index, MutableHandleValue vp)
{
    return JS_ForwardGetElementTo(cx, objArg, index, objArg, vp);
}

JS_PUBLIC_API(bool)
JS_ForwardGetElementTo(JSContext *cx, HandleObject obj, uint32_t index, HandleObject onBehalfOf,
                       MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    return JSObject::getElement(cx, obj, onBehalfOf, index, vp);
}

JS_PUBLIC_API(bool)
JS_GetProperty(JSContext *cx, HandleObject obj, const char *name, MutableHandleValue vp)
{
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_GetPropertyById(cx, obj, id, vp);
}

JS_PUBLIC_API(bool)
JS_GetUCProperty(JSContext *cx, HandleObject obj, const char16_t *name, size_t namelen,
                 MutableHandleValue vp)
{
    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_GetPropertyById(cx, obj, id, vp);
}

JS_PUBLIC_API(bool)
JS_SetPropertyById(JSContext *cx, HandleObject obj, HandleId id, HandleValue v)
{
    RootedValue value(cx, v);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);

    return JSObject::setGeneric(cx, obj, obj, id, &value, false);
}

static bool
SetElement(JSContext *cx, HandleObject obj, uint32_t index, MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, vp);

    return JSObject::setElement(cx, obj, obj, index, vp, false);
}

JS_PUBLIC_API(bool)
JS_SetElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue v)
{
    RootedValue value(cx, v);
    return SetElement(cx, obj, index, &value);
}

JS_PUBLIC_API(bool)
JS_SetElement(JSContext *cx, HandleObject obj, uint32_t index, HandleObject v)
{
    RootedValue value(cx, ObjectOrNullValue(v));
    return SetElement(cx, obj, index, &value);
}

JS_PUBLIC_API(bool)
JS_SetElement(JSContext *cx, HandleObject obj, uint32_t index, HandleString v)
{
    RootedValue value(cx, StringValue(v));
    return SetElement(cx, obj, index, &value);
}

JS_PUBLIC_API(bool)
JS_SetElement(JSContext *cx, HandleObject obj, uint32_t index, int32_t v)
{
    RootedValue value(cx, NumberValue(v));
    return SetElement(cx, obj, index, &value);
}

JS_PUBLIC_API(bool)
JS_SetElement(JSContext *cx, HandleObject obj, uint32_t index, uint32_t v)
{
    RootedValue value(cx, NumberValue(v));
    return SetElement(cx, obj, index, &value);
}

JS_PUBLIC_API(bool)
JS_SetElement(JSContext *cx, HandleObject obj, uint32_t index, double v)
{
    RootedValue value(cx, NumberValue(v));
    return SetElement(cx, obj, index, &value);
}

JS_PUBLIC_API(bool)
JS_SetProperty(JSContext *cx, HandleObject obj, const char *name, HandleValue v)
{
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_SetPropertyById(cx, obj, id, v);
}

JS_PUBLIC_API(bool)
JS_SetUCProperty(JSContext *cx, HandleObject obj, const char16_t *name, size_t namelen,
                 HandleValue v)
{
    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_SetPropertyById(cx, obj, id, v);
}

JS_PUBLIC_API(bool)
JS_DeletePropertyById2(JSContext *cx, HandleObject obj, HandleId id, bool *result)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);

    return JSObject::deleteGeneric(cx, obj, id, result);
}

JS_PUBLIC_API(bool)
JS_DeleteElement2(JSContext *cx, HandleObject obj, uint32_t index, bool *result)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    return JSObject::deleteElement(cx, obj, index, result);
}

JS_PUBLIC_API(bool)
JS_DeleteProperty2(JSContext *cx, HandleObject obj, const char *name, bool *result)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JSObject::deleteGeneric(cx, obj, id, result);
}

JS_PUBLIC_API(bool)
JS_DeleteUCProperty2(JSContext *cx, HandleObject obj, const char16_t *name, size_t namelen,
                     bool *result)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JSObject::deleteGeneric(cx, obj, id, result);
}

JS_PUBLIC_API(bool)
JS_DeletePropertyById(JSContext *cx, HandleObject obj, HandleId id)
{
    bool junk;
    return JS_DeletePropertyById2(cx, obj, id, &junk);
}

JS_PUBLIC_API(bool)
JS_DeleteElement(JSContext *cx, HandleObject obj, uint32_t index)
{
    bool junk;
    return JS_DeleteElement2(cx, obj, index, &junk);
}

JS_PUBLIC_API(bool)
JS_DeleteProperty(JSContext *cx, HandleObject obj, const char *name)
{
    bool junk;
    return JS_DeleteProperty2(cx, obj, name, &junk);
}

JS_PUBLIC_API(void)
JS_SetAllNonReservedSlotsToUndefined(JSContext *cx, JSObject *objArg)
{
    RootedObject obj(cx, objArg);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    if (!obj->isNative())
        return;

    const Class *clasp = obj->getClass();
    unsigned numReserved = JSCLASS_RESERVED_SLOTS(clasp);
    unsigned numSlots = obj->as<NativeObject>().slotSpan();
    for (unsigned i = numReserved; i < numSlots; i++)
        obj->as<NativeObject>().setSlot(i, UndefinedValue());
}

JS_PUBLIC_API(JSIdArray *)
JS_Enumerate(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    AutoIdVector props(cx);
    JSIdArray *ida;
    if (!GetPropertyNames(cx, obj, JSITER_OWNONLY, &props) || !VectorToIdArray(cx, props, &ida))
        return nullptr;
    return ida;
}

/*
 * XXX reverse iterator for properties, unreverse and meld with jsinterp.c's
 *     prop_iterator_class somehow...
 * + preserve the obj->enumerate API while optimizing the native object case
 * + native case here uses a JSShape *, but that iterates in reverse!
 * + so we make non-native match, by reverse-iterating after JS_Enumerating
 */
static const uint32_t JSSLOT_ITER_INDEX = 0;

static void
prop_iter_finalize(FreeOp *fop, JSObject *obj)
{
    void *pdata = obj->as<NativeObject>().getPrivate();
    if (!pdata)
        return;

    if (obj->as<NativeObject>().getSlot(JSSLOT_ITER_INDEX).toInt32() >= 0) {
        /* Non-native case: destroy the ida enumerated when obj was created. */
        JSIdArray *ida = (JSIdArray *) pdata;
        fop->free_(ida);
    }
}

static void
prop_iter_trace(JSTracer *trc, JSObject *obj)
{
    void *pdata = obj->as<NativeObject>().getPrivate();
    if (!pdata)
        return;

    if (obj->as<NativeObject>().getSlot(JSSLOT_ITER_INDEX).toInt32() < 0) {
        /*
         * Native case: just mark the next property to visit. We don't need a
         * barrier here because the pointer is updated via setPrivate, which
         * always takes a barrier.
         */
        Shape *tmp = static_cast<Shape *>(pdata);
        MarkShapeUnbarriered(trc, &tmp, "prop iter shape");
        obj->as<NativeObject>().setPrivateUnbarriered(tmp);
    } else {
        /* Non-native case: mark each id in the JSIdArray private. */
        JSIdArray *ida = (JSIdArray *) pdata;
        MarkIdRange(trc, ida->length, ida->vector, "prop iter");
    }
}

static const Class prop_iter_class = {
    "PropertyIterator",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_HAS_RESERVED_SLOTS(1),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    prop_iter_finalize,
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    prop_iter_trace
};

JS_PUBLIC_API(JSObject *)
JS_NewPropertyIterator(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    RootedNativeObject iterobj(cx, NewNativeObjectWithClassProto(cx, &prop_iter_class,
                                                                 nullptr, obj));
    if (!iterobj)
        return nullptr;

    int index;
    if (obj->isNative()) {
        /* Native case: start with the last property in obj. */
        iterobj->setPrivateGCThing(obj->lastProperty());
        index = -1;
    } else {
        /* Non-native case: enumerate a JSIdArray and keep it via private. */
        JSIdArray *ida = JS_Enumerate(cx, obj);
        if (!ida)
            return nullptr;
        iterobj->setPrivate((void *)ida);
        index = ida->length;
    }

    /* iterobj cannot escape to other threads here. */
    iterobj->setSlot(JSSLOT_ITER_INDEX, Int32Value(index));
    return iterobj;
}

JS_PUBLIC_API(bool)
JS_NextProperty(JSContext *cx, HandleObject iterobj, MutableHandleId idp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, iterobj);
    int32_t i = iterobj->as<NativeObject>().getSlot(JSSLOT_ITER_INDEX).toInt32();
    if (i < 0) {
        /* Native case: private data is a property tree node pointer. */
        MOZ_ASSERT(iterobj->getParent()->isNative());
        Shape *shape = static_cast<Shape *>(iterobj->as<NativeObject>().getPrivate());

        while (shape->previous() && !shape->enumerable())
            shape = shape->previous();

        if (!shape->previous()) {
            MOZ_ASSERT(shape->isEmptyShape());
            idp.set(JSID_VOID);
        } else {
            iterobj->as<NativeObject>().setPrivateGCThing(const_cast<Shape *>(shape->previous().get()));
            idp.set(shape->propid());
        }
    } else {
        /* Non-native case: use the ida enumerated when iterobj was created. */
        JSIdArray *ida = (JSIdArray *) iterobj->as<NativeObject>().getPrivate();
        MOZ_ASSERT(i <= ida->length);
        STATIC_ASSUME(i <= ida->length);
        if (i == 0) {
            idp.set(JSID_VOID);
        } else {
            idp.set(ida->vector[--i]);
            iterobj->as<NativeObject>().setSlot(JSSLOT_ITER_INDEX, Int32Value(i));
        }
    }
    return true;
}

JS_PUBLIC_API(jsval)
JS_GetReservedSlot(JSObject *obj, uint32_t index)
{
    return obj->fakeNativeGetReservedSlot(index);
}

JS_PUBLIC_API(void)
JS_SetReservedSlot(JSObject *obj, uint32_t index, Value value)
{
    obj->fakeNativeSetReservedSlot(index, value);
}

JS_PUBLIC_API(JSObject *)
JS_NewArrayObject(JSContext *cx, const JS::HandleValueArray& contents)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    assertSameCompartment(cx, contents);
    return NewDenseCopiedArray(cx, contents.length(), contents.begin());
}

JS_PUBLIC_API(JSObject *)
JS_NewArrayObject(JSContext *cx, size_t length)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    return NewDenseFullyAllocatedArray(cx, length);
}

JS_PUBLIC_API(bool)
JS_IsArrayObject(JSContext *cx, JS::HandleObject obj)
{
    assertSameCompartment(cx, obj);
    return ObjectClassIs(obj, ESClass_Array, cx);
}

JS_PUBLIC_API(bool)
JS_IsArrayObject(JSContext *cx, JS::HandleValue value)
{
    if (!value.isObject())
        return false;
    RootedObject obj(cx, &value.toObject());
    return JS_IsArrayObject(cx, obj);
}

JS_PUBLIC_API(bool)
JS_GetArrayLength(JSContext *cx, HandleObject obj, uint32_t *lengthp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return GetLengthProperty(cx, obj, lengthp);
}

JS_PUBLIC_API(bool)
JS_SetArrayLength(JSContext *cx, HandleObject obj, uint32_t length)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return SetLengthProperty(cx, obj, length);
}

JS_PUBLIC_API(void)
JS_HoldPrincipals(JSPrincipals *principals)
{
    ++principals->refcount;
}

JS_PUBLIC_API(void)
JS_DropPrincipals(JSRuntime *rt, JSPrincipals *principals)
{
    int rc = --principals->refcount;
    if (rc == 0)
        rt->destroyPrincipals(principals);
}

JS_PUBLIC_API(void)
JS_SetSecurityCallbacks(JSRuntime *rt, const JSSecurityCallbacks *scb)
{
    MOZ_ASSERT(scb != &NullSecurityCallbacks);
    rt->securityCallbacks = scb ? scb : &NullSecurityCallbacks;
}

JS_PUBLIC_API(const JSSecurityCallbacks *)
JS_GetSecurityCallbacks(JSRuntime *rt)
{
    return (rt->securityCallbacks != &NullSecurityCallbacks) ? rt->securityCallbacks : nullptr;
}

JS_PUBLIC_API(void)
JS_SetTrustedPrincipals(JSRuntime *rt, const JSPrincipals *prin)
{
    rt->setTrustedPrincipals(prin);
}

extern JS_PUBLIC_API(void)
JS_InitDestroyPrincipalsCallback(JSRuntime *rt, JSDestroyPrincipalsOp destroyPrincipals)
{
    MOZ_ASSERT(destroyPrincipals);
    MOZ_ASSERT(!rt->destroyPrincipals);
    rt->destroyPrincipals = destroyPrincipals;
}

JS_PUBLIC_API(JSFunction *)
JS_NewFunction(JSContext *cx, JSNative native, unsigned nargs, unsigned flags,
               HandleObject parent, const char *name)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);

    RootedAtom atom(cx);
    if (name) {
        atom = Atomize(cx, name, strlen(name));
        if (!atom)
            return nullptr;
    }

    JSFunction::Flags funFlags = JSAPIToJSFunctionFlags(flags);
    return NewFunction(cx, NullPtr(), native, nargs, funFlags, parent, atom);
}

JS_PUBLIC_API(JSFunction *)
JS_NewFunctionById(JSContext *cx, JSNative native, unsigned nargs, unsigned flags,
                   HandleObject parent, HandleId id)
{
    MOZ_ASSERT(JSID_IS_STRING(id));
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    MOZ_ASSERT(native);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);

    RootedAtom name(cx, JSID_TO_ATOM(id));
    JSFunction::Flags funFlags = JSAPIToJSFunctionFlags(flags);
    return NewFunction(cx, NullPtr(), native, nargs, funFlags, parent, name);
}

JS_PUBLIC_API(JSFunction *)
JS::GetSelfHostedFunction(JSContext *cx, const char *selfHostedName, HandleId id, unsigned nargs)
{
    MOZ_ASSERT(JSID_IS_STRING(id));
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    RootedAtom name(cx, JSID_TO_ATOM(id));
    RootedAtom shName(cx, Atomize(cx, selfHostedName, strlen(selfHostedName)));
    if (!shName)
        return nullptr;
    RootedValue funVal(cx);
    if (!cx->global()->getSelfHostedFunction(cx, shName, name, nargs, &funVal))
        return nullptr;
    return &funVal.toObject().as<JSFunction>();
}

JS_PUBLIC_API(JSObject *)
JS_CloneFunctionObject(JSContext *cx, HandleObject funobj, HandleObject parentArg)
{
    RootedObject parent(cx, parentArg);

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);
    // Note that funobj can be in a different compartment.

    if (!parent)
        parent = cx->global();

    if (!funobj->is<JSFunction>()) {
        AutoCompartment ac(cx, funobj);
        RootedValue v(cx, ObjectValue(*funobj));
        ReportIsNotFunction(cx, v);
        return nullptr;
    }

    RootedFunction fun(cx, &funobj->as<JSFunction>());
    if (fun->isInterpretedLazy()) {
        AutoCompartment ac(cx, funobj);
        if (!fun->getOrCreateScript(cx))
            return nullptr;
    }
    /*
     * If a function was compiled to be lexically nested inside some other
     * script, we cannot clone it without breaking the compiler's assumptions.
     */
    if (fun->isInterpreted() && (fun->nonLazyScript()->enclosingStaticScope() ||
        (fun->nonLazyScript()->compileAndGo() && !parent->is<GlobalObject>())))
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_CLONE_FUNOBJ_SCOPE);
        return nullptr;
    }

    if (fun->isBoundFunction()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_CLONE_OBJECT);
        return nullptr;
    }

    if (fun->isNative() && IsAsmJSModuleNative(fun->native())) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_CLONE_OBJECT);
        return nullptr;
    }

    return CloneFunctionObject(cx, fun, parent, fun->getAllocKind());
}

JS_PUBLIC_API(JSObject *)
JS_GetFunctionObject(JSFunction *fun)
{
    return fun;
}

JS_PUBLIC_API(JSString *)
JS_GetFunctionId(JSFunction *fun)
{
    return fun->atom();
}

JS_PUBLIC_API(JSString *)
JS_GetFunctionDisplayId(JSFunction *fun)
{
    return fun->displayAtom();
}

JS_PUBLIC_API(uint16_t)
JS_GetFunctionArity(JSFunction *fun)
{
    return fun->nargs();
}

namespace JS {

JS_PUBLIC_API(bool)
IsCallable(JSObject *obj)
{
    return obj->isCallable();
}

JS_PUBLIC_API(bool)
IsConstructor(JSObject *obj)
{
    return obj->isConstructor();
}

} /* namespace JS */

JS_PUBLIC_API(bool)
JS_ObjectIsFunction(JSContext *cx, JSObject *obj)
{
    return obj->is<JSFunction>();
}

JS_PUBLIC_API(bool)
JS_IsNativeFunction(JSObject *funobj, JSNative call)
{
    if (!funobj->is<JSFunction>())
        return false;
    JSFunction *fun = &funobj->as<JSFunction>();
    return fun->isNative() && fun->native() == call;
}

extern JS_PUBLIC_API(bool)
JS_IsConstructor(JSFunction *fun)
{
    return fun->isNativeConstructor() || fun->isInterpretedConstructor();
}

JS_PUBLIC_API(JSObject*)
JS_BindCallable(JSContext *cx, HandleObject target, HandleObject newThis)
{
    RootedValue thisArg(cx, ObjectValue(*newThis));
    return js_fun_bind(cx, target, thisArg, nullptr, 0);
}

static bool
js_generic_native_method_dispatcher(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    const JSFunctionSpec *fs = (JSFunctionSpec *)
        args.callee().as<JSFunction>().getExtendedSlot(0).toPrivate();
    MOZ_ASSERT((fs->flags & JSFUN_GENERIC_NATIVE) != 0);

    if (argc < 1) {
        js_ReportMissingArg(cx, args.calleev(), 0);
        return false;
    }

    /*
     * Copy all actual (argc) arguments down over our |this| parameter, vp[1],
     * which is almost always the class constructor object, e.g. Array.  Then
     * call the corresponding prototype native method with our first argument
     * passed as |this|.
     */
    memmove(vp + 1, vp + 2, argc * sizeof(jsval));

    /* Clear the last parameter in case too few arguments were passed. */
    vp[2 + --argc].setUndefined();

    return fs->call.op(cx, argc, vp);
}

JS_PUBLIC_API(bool)
JS_DefineFunctions(JSContext *cx, HandleObject obj, const JSFunctionSpec *fs)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    for (; fs->name; fs++) {
        RootedAtom atom(cx);
        // If the name starts with "@@", it must be a well-known symbol.
        if (fs->name[0] != '@' || fs->name[1] != '@')
            atom = Atomize(cx, fs->name, strlen(fs->name));
        else if (strcmp(fs->name, "@@iterator") == 0)
            // FIXME: This atom should be a symbol: bug 918828.
            atom = cx->names().std_iterator;
        else
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_SYMBOL, fs->name);
        if (!atom)
            return false;

        Rooted<jsid> id(cx, AtomToId(atom));

        /*
         * Define a generic arity N+1 static method for the arity N prototype
         * method if flags contains JSFUN_GENERIC_NATIVE.
         */
        unsigned flags = fs->flags;
        if (flags & JSFUN_GENERIC_NATIVE) {
            // We require that any consumers using JSFUN_GENERIC_NATIVE stash
            // the prototype and constructor in the global slots before invoking
            // JS_DefineFunctions on the proto.
            JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(obj->getClass());
            MOZ_ASSERT(obj == &obj->global().getPrototype(key).toObject());
            RootedObject ctor(cx, &obj->global().getConstructor(key).toObject());

            flags &= ~JSFUN_GENERIC_NATIVE;
            JSFunction *fun = DefineFunction(cx, ctor, id,
                                             js_generic_native_method_dispatcher,
                                             fs->nargs + 1, flags,
                                             JSFunction::ExtendedFinalizeKind);
            if (!fun)
                return false;

            /*
             * As jsapi.h notes, fs must point to storage that lives as long
             * as fun->object lives.
             */
            fun->setExtendedSlot(0, PrivateValue(const_cast<JSFunctionSpec*>(fs)));
        }

        /*
         * Delay cloning self-hosted functions until they are called. This is
         * achieved by passing DefineFunction a nullptr JSNative which
         * produces an interpreted JSFunction where !hasScript. Interpreted
         * call paths then call InitializeLazyFunctionScript if !hasScript.
         */
        if (fs->selfHostedName) {
            MOZ_ASSERT(!fs->call.op);
            MOZ_ASSERT(!fs->call.info);

            RootedAtom shName(cx, Atomize(cx, fs->selfHostedName, strlen(fs->selfHostedName)));
            if (!shName)
                return false;
            RootedValue funVal(cx);
            if (!cx->global()->getSelfHostedFunction(cx, shName, atom, fs->nargs, &funVal))
                return false;
            if (!JSObject::defineGeneric(cx, obj, id, funVal, nullptr, nullptr, flags))
                return false;
        } else {
            JSFunction *fun = DefineFunction(cx, obj, id, fs->call.op, fs->nargs, flags);
            if (!fun)
                return false;
            if (fs->call.info)
                fun->setJitInfo(fs->call.info);
        }
    }
    return true;
}

JS_PUBLIC_API(JSFunction *)
JS_DefineFunction(JSContext *cx, HandleObject obj, const char *name, JSNative call,
                  unsigned nargs, unsigned attrs)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return nullptr;
    Rooted<jsid> id(cx, AtomToId(atom));
    return DefineFunction(cx, obj, id, call, nargs, attrs);
}

JS_PUBLIC_API(JSFunction *)
JS_DefineUCFunction(JSContext *cx, HandleObject obj,
                    const char16_t *name, size_t namelen, JSNative call,
                    unsigned nargs, unsigned attrs)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return nullptr;
    Rooted<jsid> id(cx, AtomToId(atom));
    return DefineFunction(cx, obj, id, call, nargs, attrs);
}

extern JS_PUBLIC_API(JSFunction *)
JS_DefineFunctionById(JSContext *cx, HandleObject obj, HandleId id, JSNative call,
                      unsigned nargs, unsigned attrs)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return DefineFunction(cx, obj, id, call, nargs, attrs);
}

struct AutoLastFrameCheck
{
    explicit AutoLastFrameCheck(JSContext *cx
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx)
    {
        MOZ_ASSERT(cx);
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AutoLastFrameCheck() {
        if (cx->isExceptionPending() &&
            !JS_IsRunning(cx) &&
            !cx->options().dontReportUncaught()) {
            js_ReportUncaughtException(cx);
        }
    }

  private:
    JSContext *cx;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/* Use the fastest available getc. */
#if defined(HAVE_GETC_UNLOCKED)
# define fast_getc getc_unlocked
#elif defined(HAVE__GETC_NOLOCK)
# define fast_getc _getc_nolock
#else
# define fast_getc getc
#endif

typedef Vector<char, 8, TempAllocPolicy> FileContents;

static bool
ReadCompleteFile(JSContext *cx, FILE *fp, FileContents &buffer)
{
    /* Get the complete length of the file, if possible. */
    struct stat st;
    int ok = fstat(fileno(fp), &st);
    if (ok != 0)
        return false;
    if (st.st_size > 0) {
        if (!buffer.reserve(st.st_size))
            return false;
    }

    // Read in the whole file. Note that we can't assume the data's length
    // is actually st.st_size, because 1) some files lie about their size
    // (/dev/zero and /dev/random), and 2) reading files in text mode on
    // Windows collapses "\r\n" pairs to single \n characters.
    for (;;) {
        int c = fast_getc(fp);
        if (c == EOF)
            break;
        if (!buffer.append(c))
            return false;
    }

    return true;
}

namespace {

class AutoFile
{
    FILE *fp_;
  public:
    AutoFile()
      : fp_(nullptr)
    {}
    ~AutoFile()
    {
        if (fp_ && fp_ != stdin)
            fclose(fp_);
    }
    FILE *fp() const { return fp_; }
    bool open(JSContext *cx, const char *filename);
    bool readAll(JSContext *cx, FileContents &buffer)
    {
        MOZ_ASSERT(fp_);
        return ReadCompleteFile(cx, fp_, buffer);
    }
};

} /* anonymous namespace */

/*
 * Open a source file for reading. Supports "-" and nullptr to mean stdin. The
 * return value must be fclosed unless it is stdin.
 */
bool
AutoFile::open(JSContext *cx, const char *filename)
{
    if (!filename || strcmp(filename, "-") == 0) {
        fp_ = stdin;
    } else {
        fp_ = fopen(filename, "r");
        if (!fp_) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_OPEN,
                                 filename, "No such file or directory");
            return false;
        }
    }
    return true;
}

JSObject * const JS::ReadOnlyCompileOptions::nullObjectPtr = nullptr;

void
JS::ReadOnlyCompileOptions::copyPODOptions(const ReadOnlyCompileOptions &rhs)
{
    version = rhs.version;
    versionSet = rhs.versionSet;
    utf8 = rhs.utf8;
    lineno = rhs.lineno;
    column = rhs.column;
    compileAndGo = rhs.compileAndGo;
    forEval = rhs.forEval;
    noScriptRval = rhs.noScriptRval;
    selfHostingMode = rhs.selfHostingMode;
    canLazilyParse = rhs.canLazilyParse;
    strictOption = rhs.strictOption;
    extraWarningsOption = rhs.extraWarningsOption;
    werrorOption = rhs.werrorOption;
    asmJSOption = rhs.asmJSOption;
    forceAsync = rhs.forceAsync;
    installedFile = rhs.installedFile;
    sourceIsLazy = rhs.sourceIsLazy;
    introductionType = rhs.introductionType;
    introductionLineno = rhs.introductionLineno;
    introductionOffset = rhs.introductionOffset;
    hasIntroductionInfo = rhs.hasIntroductionInfo;
}

JS::OwningCompileOptions::OwningCompileOptions(JSContext *cx)
    : ReadOnlyCompileOptions(),
      runtime(GetRuntime(cx)),
      elementRoot(cx),
      elementAttributeNameRoot(cx),
      introductionScriptRoot(cx)
{
}

JS::OwningCompileOptions::~OwningCompileOptions()
{
    // OwningCompileOptions always owns these, so these casts are okay.
    js_free(const_cast<char *>(filename_));
    js_free(const_cast<char16_t *>(sourceMapURL_));
    js_free(const_cast<char *>(introducerFilename_));
}

bool
JS::OwningCompileOptions::copy(JSContext *cx, const ReadOnlyCompileOptions &rhs)
{
    copyPODOptions(rhs);

    setMutedErrors(rhs.mutedErrors());
    setElement(rhs.element());
    setElementAttributeName(rhs.elementAttributeName());
    setIntroductionScript(rhs.introductionScript());

    return setFileAndLine(cx, rhs.filename(), rhs.lineno) &&
           setSourceMapURL(cx, rhs.sourceMapURL()) &&
           setIntroducerFilename(cx, rhs.introducerFilename());
}

bool
JS::OwningCompileOptions::setFile(JSContext *cx, const char *f)
{
    char *copy = nullptr;
    if (f) {
        copy = JS_strdup(cx, f);
        if (!copy)
            return false;
    }

    // OwningCompileOptions always owns filename_, so this cast is okay.
    js_free(const_cast<char *>(filename_));

    filename_ = copy;
    return true;
}

bool
JS::OwningCompileOptions::setFileAndLine(JSContext *cx, const char *f, unsigned l)
{
    if (!setFile(cx, f))
        return false;

    lineno = l;
    return true;
}

bool
JS::OwningCompileOptions::setSourceMapURL(JSContext *cx, const char16_t *s)
{
    UniquePtr<char16_t[], JS::FreePolicy> copy;
    if (s) {
        copy = DuplicateString(cx, s);
        if (!copy)
            return false;
    }

    // OwningCompileOptions always owns sourceMapURL_, so this cast is okay.
    js_free(const_cast<char16_t *>(sourceMapURL_));

    sourceMapURL_ = copy.release();
    return true;
}

bool
JS::OwningCompileOptions::setIntroducerFilename(JSContext *cx, const char *s)
{
    char *copy = nullptr;
    if (s) {
        copy = JS_strdup(cx, s);
        if (!copy)
            return false;
    }

    // OwningCompileOptions always owns introducerFilename_, so this cast is okay.
    js_free(const_cast<char *>(introducerFilename_));

    introducerFilename_ = copy;
    return true;
}

JS::CompileOptions::CompileOptions(JSContext *cx, JSVersion version)
    : ReadOnlyCompileOptions(), elementRoot(cx), elementAttributeNameRoot(cx),
      introductionScriptRoot(cx)
{
    this->version = (version != JSVERSION_UNKNOWN) ? version : cx->findVersion();

    compileAndGo = false;
    strictOption = cx->runtime()->options().strictMode();
    extraWarningsOption = cx->compartment()->options().extraWarnings(cx);
    werrorOption = cx->runtime()->options().werror();
    asmJSOption = cx->runtime()->options().asmJS();
}

bool
JS::Compile(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options,
            SourceBufferHolder &srcBuf, MutableHandleScript script)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    AutoLastFrameCheck lfc(cx);

    script.set(frontend::CompileScript(cx, &cx->tempLifoAlloc(), obj, NullPtr(), options, srcBuf));
    return !!script;
}

bool
JS::Compile(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options,
            const char16_t *chars, size_t length, MutableHandleScript script)
{
    SourceBufferHolder srcBuf(chars, length, SourceBufferHolder::NoOwnership);
    return Compile(cx, obj, options, srcBuf, script);
}

bool
JS::Compile(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options,
            const char *bytes, size_t length, MutableHandleScript script)
{
    mozilla::UniquePtr<char16_t, JS::FreePolicy> chars;
    if (options.utf8)
        chars.reset(UTF8CharsToNewTwoByteCharsZ(cx, UTF8Chars(bytes, length), &length).get());
    else
        chars.reset(InflateString(cx, bytes, &length));
    if (!chars)
        return false;

    return Compile(cx, obj, options, chars.get(), length, script);
}

bool
JS::Compile(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options, FILE *fp,
            MutableHandleScript script)
{
    FileContents buffer(cx);
    if (!ReadCompleteFile(cx, fp, buffer))
        return false;

    return Compile(cx, obj, options, buffer.begin(), buffer.length(), script);
}

bool
JS::Compile(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &optionsArg, const char *filename,
            MutableHandleScript script)
{
    AutoFile file;
    if (!file.open(cx, filename))
        return false;
    CompileOptions options(cx, optionsArg);
    options.setFileAndLine(filename, 1);
    return Compile(cx, obj, options, file.fp(), script);
}

JS_PUBLIC_API(bool)
JS::CanCompileOffThread(JSContext *cx, const ReadOnlyCompileOptions &options, size_t length)
{
    static const size_t TINY_LENGTH = 1000;
    static const size_t HUGE_LENGTH = 100 * 1000;

    // These are heuristics which the caller may choose to ignore (e.g., for
    // testing purposes).
    if (!options.forceAsync) {
        // Compiling off the main thread inolves creating a new Zone and other
        // significant overheads.  Don't bother if the script is tiny.
        if (length < TINY_LENGTH)
            return false;

        // If the parsing task would have to wait for GC to complete, it'll probably
        // be faster to just start it synchronously on the main thread unless the
        // script is huge.
        if (OffThreadParsingMustWaitForGC(cx->runtime()) && length < HUGE_LENGTH)
            return false;
    }

    return cx->runtime()->canUseParallelParsing() && CanUseExtraThreads();
}

JS_PUBLIC_API(bool)
JS::CompileOffThread(JSContext *cx, const ReadOnlyCompileOptions &options,
                     const char16_t *chars, size_t length,
                     OffThreadCompileCallback callback, void *callbackData)
{
    MOZ_ASSERT(CanCompileOffThread(cx, options, length));
    return StartOffThreadParseScript(cx, options, chars, length, callback, callbackData);
}

JS_PUBLIC_API(JSScript *)
JS::FinishOffThreadScript(JSContext *maybecx, JSRuntime *rt, void *token)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));

    if (maybecx) {
        RootedScript script(maybecx);
        {
            AutoLastFrameCheck lfc(maybecx);
            script = HelperThreadState().finishParseTask(maybecx, rt, token);
        }
        return script;
    } else {
        return HelperThreadState().finishParseTask(maybecx, rt, token);
    }
}

JS_PUBLIC_API(bool)
JS_CompileScript(JSContext *cx, JS::HandleObject obj, const char *ascii,
                 size_t length, const JS::CompileOptions &options, MutableHandleScript script)
{
    return Compile(cx, obj, options, ascii, length, script);
}

JS_PUBLIC_API(bool)
JS_CompileUCScript(JSContext *cx, JS::HandleObject obj, const char16_t *chars,
                   size_t length, const JS::CompileOptions &options, MutableHandleScript script)
{
    return Compile(cx, obj, options, chars, length, script);
}

JS_PUBLIC_API(bool)
JS_BufferIsCompilableUnit(JSContext *cx, HandleObject obj, const char *utf8, size_t length)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    cx->clearPendingException();

    char16_t *chars = JS::UTF8CharsToNewTwoByteCharsZ(cx, JS::UTF8Chars(utf8, length), &length).get();
    if (!chars)
        return true;

    // Return true on any out-of-memory error or non-EOF-related syntax error, so our
    // caller doesn't try to collect more buffered source.
    bool result = true;

    CompileOptions options(cx);
    options.setCompileAndGo(false);
    Parser<frontend::FullParseHandler> parser(cx, &cx->tempLifoAlloc(),
                                              options, chars, length,
                                              /* foldConstants = */ true, nullptr, nullptr);
    JSErrorReporter older = JS_SetErrorReporter(cx->runtime(), nullptr);
    if (!parser.parse(obj)) {
        // We ran into an error. If it was because we ran out of source, we
        // return false so our caller knows to try to collect more buffered
        // source.
        if (parser.isUnexpectedEOF())
            result = false;

        cx->clearPendingException();
    }
    JS_SetErrorReporter(cx->runtime(), older);

    js_free(chars);
    return result;
}

JS_PUBLIC_API(JSObject *)
JS_GetGlobalFromScript(JSScript *script)
{
    MOZ_ASSERT(!script->isCachedEval());
    return &script->global();
}

JS_PUBLIC_API(bool)
JS::CompileFunction(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options,
                    const char *name, unsigned nargs, const char *const *argnames,
                    SourceBufferHolder &srcBuf, MutableHandleFunction fun)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    RootedAtom funAtom(cx);
    AutoLastFrameCheck lfc(cx);

    if (name) {
        funAtom = Atomize(cx, name, strlen(name));
        if (!funAtom)
            return false;
    }

    AutoNameVector formals(cx);
    for (unsigned i = 0; i < nargs; i++) {
        RootedAtom argAtom(cx, Atomize(cx, argnames[i], strlen(argnames[i])));
        if (!argAtom || !formals.append(argAtom->asPropertyName()))
            return false;
    }

    fun.set(NewFunction(cx, NullPtr(), nullptr, 0, JSFunction::INTERPRETED, obj,
                        funAtom, JSFunction::FinalizeKind, TenuredObject));
    if (!fun)
        return false;

    if (!frontend::CompileFunctionBody(cx, fun, options, formals, srcBuf))
        return false;

    if (obj && funAtom && options.defineOnScope) {
        Rooted<jsid> id(cx, AtomToId(funAtom));
        RootedValue value(cx, ObjectValue(*fun));
        if (!JSObject::defineGeneric(cx, obj, id, value, nullptr, nullptr, JSPROP_ENUMERATE))
            return false;
    }

    return true;
}

JS_PUBLIC_API(bool)
JS::CompileFunction(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options,
                    const char *name, unsigned nargs, const char *const *argnames,
                    const char16_t *chars, size_t length, MutableHandleFunction fun)
{
    SourceBufferHolder srcBuf(chars, length, SourceBufferHolder::NoOwnership);
    return JS::CompileFunction(cx, obj, options, name, nargs, argnames, srcBuf, fun);
}

JS_PUBLIC_API(bool)
JS::CompileFunction(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options,
                    const char *name, unsigned nargs, const char *const *argnames,
                    const char *bytes, size_t length, MutableHandleFunction fun)
{
    mozilla::UniquePtr<char16_t, JS::FreePolicy> chars;
    if (options.utf8)
        chars.reset(UTF8CharsToNewTwoByteCharsZ(cx, UTF8Chars(bytes, length), &length).get());
    else
        chars.reset(InflateString(cx, bytes, &length));
    if (!chars)
        return false;

    return CompileFunction(cx, obj, options, name, nargs, argnames, chars.get(), length, fun);
}

JS_PUBLIC_API(bool)
JS_CompileUCFunction(JSContext *cx, JS::HandleObject obj, const char *name,
                     unsigned nargs, const char *const *argnames,
                     const char16_t *chars, size_t length,
                     const CompileOptions &options, MutableHandleFunction fun)
{
    return CompileFunction(cx, obj, options, name, nargs, argnames, chars, length, fun);
}

JS_PUBLIC_API(bool)
JS_CompileFunction(JSContext *cx, JS::HandleObject obj, const char *name,
                   unsigned nargs, const char *const *argnames,
                   const char *ascii, size_t length,
                   const JS::CompileOptions &options, MutableHandleFunction fun)
{
    return CompileFunction(cx, obj, options, name, nargs, argnames, ascii, length, fun);
}

JS_PUBLIC_API(JSString *)
JS_DecompileScript(JSContext *cx, HandleScript script, const char *name, unsigned indent)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    script->ensureNonLazyCanonicalFunction(cx);
    RootedFunction fun(cx, script->functionNonDelazifying());
    if (fun)
        return JS_DecompileFunction(cx, fun, indent);
    bool haveSource = script->scriptSource()->hasSourceData();
    if (!haveSource && !JSScript::loadSource(cx, script->scriptSource(), &haveSource))
        return nullptr;
    return haveSource ? script->sourceData(cx) : NewStringCopyZ<CanGC>(cx, "[no source]");
}

JS_PUBLIC_API(JSString *)
JS_DecompileFunction(JSContext *cx, HandleFunction fun, unsigned indent)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, fun);
    return FunctionToString(cx, fun, false, !(indent & JS_DONT_PRETTY_PRINT));
}

JS_PUBLIC_API(JSString *)
JS_DecompileFunctionBody(JSContext *cx, HandleFunction fun, unsigned indent)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, fun);
    return FunctionToString(cx, fun, true, !(indent & JS_DONT_PRETTY_PRINT));
}

MOZ_NEVER_INLINE static bool
ExecuteScript(JSContext *cx, HandleObject obj, HandleScript scriptArg, jsval *rval)
{
    RootedScript script(cx, scriptArg);

    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, scriptArg);
    AutoLastFrameCheck lfc(cx);
    return Execute(cx, script, *obj, rval);
}

MOZ_NEVER_INLINE JS_PUBLIC_API(bool)
JS_ExecuteScript(JSContext *cx, HandleObject obj, HandleScript scriptArg, MutableHandleValue rval)
{
    return ExecuteScript(cx, obj, scriptArg, rval.address());
}

MOZ_NEVER_INLINE JS_PUBLIC_API(bool)
JS_ExecuteScript(JSContext *cx, HandleObject obj, HandleScript scriptArg)
{
    return ExecuteScript(cx, obj, scriptArg, nullptr);
}

JS_PUBLIC_API(bool)
JS::CloneAndExecuteScript(JSContext *cx, HandleObject obj, HandleScript scriptArg)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    RootedScript script(cx, scriptArg);
    if (script->compartment() != cx->compartment()) {
        script = CloneScript(cx, NullPtr(), NullPtr(), script);
        if (!script)
            return false;
    }
    return ExecuteScript(cx, obj, script, nullptr);
}

JS_PUBLIC_API(bool)
JS_ExecuteScriptVersion(JSContext *cx, HandleObject obj, HandleScript script,
                        MutableHandleValue rval, JSVersion version)
{
    return ExecuteScript(cx, obj, script, rval.address());
}

JS_PUBLIC_API(bool)
JS_ExecuteScriptVersion(JSContext *cx, HandleObject obj, HandleScript script, JSVersion version)
{
    return ExecuteScript(cx, obj, script, nullptr);
}

static const unsigned LARGE_SCRIPT_LENGTH = 500*1024;

static bool
Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &optionsArg,
         SourceBufferHolder &srcBuf, JS::Value *rval)
{
    CompileOptions options(cx, optionsArg);
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    AutoLastFrameCheck lfc(cx);

    options.setCompileAndGo(obj->is<GlobalObject>());
    options.setNoScriptRval(!rval);
    SourceCompressionTask sct(cx);
    RootedScript script(cx, frontend::CompileScript(cx, &cx->tempLifoAlloc(),
                                                    obj, NullPtr(), options,
                                                    srcBuf, nullptr, 0, &sct));
    if (!script)
        return false;

    MOZ_ASSERT(script->getVersion() == options.version);

    bool result = Execute(cx, script, *obj, rval);
    if (!sct.complete())
        result = false;

    // After evaluation, the compiled script will not be run again.
    // script->ensureRanAnalysis allocated 1 analyze::Bytecode for every opcode
    // which for large scripts means significant memory. Perform a GC eagerly
    // to clear out this analysis data before anything happens to inhibit the
    // flushing of this memory (such as setting requestAnimationFrame).
    if (script->length() > LARGE_SCRIPT_LENGTH) {
        script = nullptr;
        PrepareZoneForGC(cx->zone());
        cx->runtime()->gc.gc(GC_NORMAL, JS::gcreason::FINISH_LARGE_EVALUATE);
    }

    return result;
}

static bool
Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &optionsArg,
         const char16_t *chars, size_t length, JS::Value *rval)
{
  SourceBufferHolder srcBuf(chars, length, SourceBufferHolder::NoOwnership);
  return ::Evaluate(cx, obj, optionsArg, srcBuf, rval);
}

static bool
Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options,
         const char *bytes, size_t length, JS::Value *rval)
{
    char16_t *chars;
    if (options.utf8)
        chars = UTF8CharsToNewTwoByteCharsZ(cx, JS::UTF8Chars(bytes, length), &length).get();
    else
        chars = InflateString(cx, bytes, &length);
    if (!chars)
        return false;

    SourceBufferHolder srcBuf(chars, length, SourceBufferHolder::GiveOwnership);
    bool ok = ::Evaluate(cx, obj, options, srcBuf, rval);
    return ok;
}

static bool
Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &optionsArg,
         const char *filename, JS::Value *rval)
{
    FileContents buffer(cx);
    {
        AutoFile file;
        if (!file.open(cx, filename) || !file.readAll(cx, buffer))
            return false;
    }

    CompileOptions options(cx, optionsArg);
    options.setFileAndLine(filename, 1);
    return Evaluate(cx, obj, options, buffer.begin(), buffer.length(), rval);
}

extern JS_PUBLIC_API(bool)
JS::Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &optionsArg,
             SourceBufferHolder &srcBuf, MutableHandleValue rval)
{
    return ::Evaluate(cx, obj, optionsArg, srcBuf, rval.address());
}

extern JS_PUBLIC_API(bool)
JS::Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &optionsArg,
             const char16_t *chars, size_t length, MutableHandleValue rval)
{
    return ::Evaluate(cx, obj, optionsArg, chars, length, rval.address());
}

extern JS_PUBLIC_API(bool)
JS::Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options,
             const char *bytes, size_t length, MutableHandleValue rval)
{
    return ::Evaluate(cx, obj, options, bytes, length, rval.address());
}

extern JS_PUBLIC_API(bool)
JS::Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &optionsArg,
             const char *filename, MutableHandleValue rval)
{
    return ::Evaluate(cx, obj, optionsArg, filename, rval.address());
}

extern JS_PUBLIC_API(bool)
JS::Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &optionsArg,
             SourceBufferHolder &srcBuf)
{
    return ::Evaluate(cx, obj, optionsArg, srcBuf, nullptr);
}

extern JS_PUBLIC_API(bool)
JS::Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &optionsArg,
             const char16_t *chars, size_t length)
{
    return ::Evaluate(cx, obj, optionsArg, chars, length, nullptr);
}

extern JS_PUBLIC_API(bool)
JS::Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options,
             const char *bytes, size_t length)
{
    return ::Evaluate(cx, obj, options, bytes, length, nullptr);
}

extern JS_PUBLIC_API(bool)
JS::Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &optionsArg,
             const char *filename)
{
    return ::Evaluate(cx, obj, optionsArg, filename, nullptr);
}

JS_PUBLIC_API(bool)
JS_EvaluateUCScript(JSContext *cx, HandleObject obj, const char16_t *chars, unsigned length,
                    const char *filename, unsigned lineno, MutableHandleValue rval)
{
    CompileOptions options(cx);
    options.setFileAndLine(filename, lineno);

    return ::Evaluate(cx, obj, options, chars, length, rval.address());
}

JS_PUBLIC_API(bool)
JS_EvaluateUCScript(JSContext *cx, HandleObject obj, SourceBufferHolder &srcBuf,
                    const char *filename, unsigned lineno, MutableHandleValue rval)
{
    CompileOptions options(cx);
    options.setFileAndLine(filename, lineno);

    return ::Evaluate(cx, obj, options, srcBuf, rval.address());
}

JS_PUBLIC_API(bool)
JS_EvaluateScript(JSContext *cx, HandleObject obj, const char *bytes, unsigned nbytes,
                  const char *filename, unsigned lineno, MutableHandleValue rval)
{
    CompileOptions options(cx);
    options.setFileAndLine(filename, lineno);

    return ::Evaluate(cx, obj, options, bytes, nbytes, rval.address());
}

JS_PUBLIC_API(bool)
JS_EvaluateScript(JSContext *cx, HandleObject obj, const char *bytes, unsigned nbytes,
                  const char *filename, unsigned lineno)
{
    CompileOptions options(cx);
    options.setFileAndLine(filename, lineno);

    return ::Evaluate(cx, obj, options, bytes, nbytes, nullptr);
}

JS_PUBLIC_API(bool)
JS_CallFunction(JSContext *cx, HandleObject obj, HandleFunction fun, const HandleValueArray& args,
                MutableHandleValue rval)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, fun, args);
    AutoLastFrameCheck lfc(cx);

    return Invoke(cx, ObjectOrNullValue(obj), ObjectValue(*fun), args.length(), args.begin(), rval);
}

JS_PUBLIC_API(bool)
JS_CallFunctionName(JSContext *cx, HandleObject obj, const char *name, const HandleValueArray& args,
                    MutableHandleValue rval)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, args);
    AutoLastFrameCheck lfc(cx);

    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;

    RootedValue v(cx);
    RootedId id(cx, AtomToId(atom));
    if (!JSObject::getGeneric(cx, obj, obj, id, &v))
        return false;

    return Invoke(cx, ObjectOrNullValue(obj), v, args.length(), args.begin(), rval);
}

JS_PUBLIC_API(bool)
JS_CallFunctionValue(JSContext *cx, HandleObject obj, HandleValue fval, const HandleValueArray& args,
                     MutableHandleValue rval)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, fval, args);
    AutoLastFrameCheck lfc(cx);

    return Invoke(cx, ObjectOrNullValue(obj), fval, args.length(), args.begin(), rval);
}

JS_PUBLIC_API(bool)
JS::Call(JSContext *cx, HandleValue thisv, HandleValue fval, const JS::HandleValueArray& args,
         MutableHandleValue rval)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, thisv, fval, args);
    AutoLastFrameCheck lfc(cx);

    return Invoke(cx, thisv, fval, args.length(), args.begin(), rval);
}

JS_PUBLIC_API(bool)
JS::Construct(JSContext *cx, HandleValue fval, const JS::HandleValueArray& args,
              MutableHandleValue rval)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, fval, args);
    AutoLastFrameCheck lfc(cx);

    return InvokeConstructor(cx, fval, args.length(), args.begin(), rval);
}

static JSObject *
JS_NewHelper(JSContext *cx, HandleObject ctor, const JS::HandleValueArray& inputArgs)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, ctor, inputArgs);

    // This is not a simple variation of JS_CallFunctionValue because JSOP_NEW
    // is not a simple variation of JSOP_CALL. We have to determine what class
    // of object to create, create it, and clamp the return value to an object,
    // among other details. InvokeConstructor does the hard work.
    InvokeArgs args(cx);
    if (!args.init(inputArgs.length()))
        return nullptr;

    args.setCallee(ObjectValue(*ctor));
    args.setThis(NullValue());
    PodCopy(args.array(), inputArgs.begin(), inputArgs.length());

    if (!InvokeConstructor(cx, args))
        return nullptr;

    if (!args.rval().isObject()) {
        /*
         * Although constructors may return primitives (via proxies), this
         * API is asking for an object, so we report an error.
         */
        JSAutoByteString bytes;
        if (js_ValueToPrintable(cx, args.rval(), &bytes)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_NEW_RESULT,
                                 bytes.ptr());
        }
        return nullptr;
    }

    return &args.rval().toObject();
}

JS_PUBLIC_API(JSObject *)
JS_New(JSContext *cx, HandleObject ctor, const JS::HandleValueArray& inputArgs)
{
    RootedObject obj(cx);
    {
        AutoLastFrameCheck lfc(cx);
        obj = JS_NewHelper(cx, ctor, inputArgs);
    }
    return obj;
}

JS_PUBLIC_API(JSInterruptCallback)
JS_SetInterruptCallback(JSRuntime *rt, JSInterruptCallback callback)
{
    JSInterruptCallback old = rt->interruptCallback;
    rt->interruptCallback = callback;
    return old;
}

JS_PUBLIC_API(JSInterruptCallback)
JS_GetInterruptCallback(JSRuntime *rt)
{
    return rt->interruptCallback;
}

JS_PUBLIC_API(void)
JS_RequestInterruptCallback(JSRuntime *rt)
{
    rt->requestInterrupt(JSRuntime::RequestInterruptAnyThread);
}

JS_PUBLIC_API(bool)
JS_IsRunning(JSContext *cx)
{
    return cx->currentlyRunning();
}

JS_PUBLIC_API(bool)
JS_SaveFrameChain(JSContext *cx)
{
    AssertHeapIsIdleOrIterating(cx);
    CHECK_REQUEST(cx);
    return cx->saveFrameChain();
}

JS_PUBLIC_API(void)
JS_RestoreFrameChain(JSContext *cx)
{
    AssertHeapIsIdleOrIterating(cx);
    CHECK_REQUEST(cx);
    cx->restoreFrameChain();
}

#ifdef MOZ_TRACE_JSCALLS
JS_PUBLIC_API(void)
JS_SetFunctionCallback(JSContext *cx, JSFunctionCallback fcb)
{
    cx->functionCallback = fcb;
}

JS_PUBLIC_API(JSFunctionCallback)
JS_GetFunctionCallback(JSContext *cx)
{
    return cx->functionCallback;
}
#endif

/************************************************************************/
JS_PUBLIC_API(JSString *)
JS_NewStringCopyN(JSContext *cx, const char *s, size_t n)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (!n)
        return cx->names().empty;
    return NewStringCopyN<CanGC>(cx, s, n);
}

JS_PUBLIC_API(JSString *)
JS_NewStringCopyZ(JSContext *cx, const char *s)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (!s || !*s)
        return cx->runtime()->emptyString;
    return NewStringCopyZ<CanGC>(cx, s);
}

JS_PUBLIC_API(bool)
JS_StringHasBeenInterned(JSContext *cx, JSString *str)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    if (!str->isAtom())
        return false;

    return AtomIsInterned(cx, &str->asAtom());
}

JS_PUBLIC_API(jsid)
INTERNED_STRING_TO_JSID(JSContext *cx, JSString *str)
{
    MOZ_ASSERT(str);
    MOZ_ASSERT(((size_t)str & JSID_TYPE_MASK) == 0);
    MOZ_ASSERT_IF(cx, JS_StringHasBeenInterned(cx, str));
    return AtomToId(&str->asAtom());
}

JS_PUBLIC_API(JSString *)
JS_InternJSString(JSContext *cx, HandleString str)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JSAtom *atom = AtomizeString(cx, str, InternAtom);
    MOZ_ASSERT_IF(atom, JS_StringHasBeenInterned(cx, atom));
    return atom;
}

JS_PUBLIC_API(JSString *)
JS_InternString(JSContext *cx, const char *s)
{
    return JS_InternStringN(cx, s, strlen(s));
}

JS_PUBLIC_API(JSString *)
JS_InternStringN(JSContext *cx, const char *s, size_t length)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JSAtom *atom = Atomize(cx, s, length, InternAtom);
    MOZ_ASSERT_IF(atom, JS_StringHasBeenInterned(cx, atom));
    return atom;
}

JS_PUBLIC_API(JSString *)
JS_NewUCString(JSContext *cx, char16_t *chars, size_t length)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return NewString<CanGC>(cx, chars, length);
}

JS_PUBLIC_API(JSString *)
JS_NewUCStringCopyN(JSContext *cx, const char16_t *s, size_t n)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (!n)
        return cx->names().empty;
    return NewStringCopyN<CanGC>(cx, s, n);
}

JS_PUBLIC_API(JSString *)
JS_NewUCStringCopyZ(JSContext *cx, const char16_t *s)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (!s)
        return cx->runtime()->emptyString;
    return NewStringCopyZ<CanGC>(cx, s);
}

JS_PUBLIC_API(JSString *)
JS_InternUCStringN(JSContext *cx, const char16_t *s, size_t length)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JSAtom *atom = AtomizeChars(cx, s, length, InternAtom);
    MOZ_ASSERT_IF(atom, JS_StringHasBeenInterned(cx, atom));
    return atom;
}

JS_PUBLIC_API(JSString *)
JS_InternUCString(JSContext *cx, const char16_t *s)
{
    return JS_InternUCStringN(cx, s, js_strlen(s));
}

JS_PUBLIC_API(size_t)
JS_GetStringLength(JSString *str)
{
    return str->length();
}

JS_PUBLIC_API(bool)
JS_StringIsFlat(JSString *str)
{
    return str->isFlat();
}

JS_PUBLIC_API(bool)
JS_StringHasLatin1Chars(JSString *str)
{
    return str->hasLatin1Chars();
}

JS_PUBLIC_API(const JS::Latin1Char *)
JS_GetLatin1StringCharsAndLength(JSContext *cx, const JS::AutoCheckCannotGC &nogc, JSString *str,
                                 size_t *plength)
{
    MOZ_ASSERT(plength);
    AssertHeapIsIdleOrStringIsFlat(cx, str);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, str);
    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return nullptr;
    *plength = linear->length();
    return linear->latin1Chars(nogc);
}

JS_PUBLIC_API(const char16_t *)
JS_GetTwoByteStringCharsAndLength(JSContext *cx, const JS::AutoCheckCannotGC &nogc, JSString *str,
                                  size_t *plength)
{
    MOZ_ASSERT(plength);
    AssertHeapIsIdleOrStringIsFlat(cx, str);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, str);
    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return nullptr;
    *plength = linear->length();
    return linear->twoByteChars(nogc);
}

JS_PUBLIC_API(const char16_t *)
JS_GetTwoByteExternalStringChars(JSString *str)
{
    return str->asExternal().twoByteChars();
}

JS_PUBLIC_API(bool)
JS_GetStringCharAt(JSContext *cx, JSString *str, size_t index, char16_t *res)
{
    AssertHeapIsIdleOrStringIsFlat(cx, str);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, str);

    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return false;

    *res = linear->latin1OrTwoByteChar(index);
    return true;
}

JS_PUBLIC_API(char16_t)
JS_GetFlatStringCharAt(JSFlatString *str, size_t index)
{
    return str->latin1OrTwoByteChar(index);
}

JS_PUBLIC_API(bool)
JS_CopyStringChars(JSContext *cx, mozilla::Range<char16_t> dest, JSString *str)
{
    AssertHeapIsIdleOrStringIsFlat(cx, str);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, str);

    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return false;

    MOZ_ASSERT(linear->length() <= dest.length());
    CopyChars(dest.start().get(), *linear);
    return true;
}

JS_PUBLIC_API(const Latin1Char *)
JS_Latin1InternedStringChars(const JS::AutoCheckCannotGC &nogc, JSString *str)
{
    MOZ_ASSERT(str->isAtom());
    JSFlatString *flat = str->ensureFlat(nullptr);
    if (!flat)
        return nullptr;
    return flat->latin1Chars(nogc);
}

JS_PUBLIC_API(const char16_t *)
JS_GetTwoByteInternedStringChars(const JS::AutoCheckCannotGC &nogc, JSString *str)
{
    MOZ_ASSERT(str->isAtom());
    JSFlatString *flat = str->ensureFlat(nullptr);
    if (!flat)
        return nullptr;
    return flat->twoByteChars(nogc);
}

extern JS_PUBLIC_API(JSFlatString *)
JS_FlattenString(JSContext *cx, JSString *str)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, str);
    JSFlatString *flat = str->ensureFlat(cx);
    if (!flat)
        return nullptr;
    return flat;
}

extern JS_PUBLIC_API(const Latin1Char *)
JS_GetLatin1FlatStringChars(const JS::AutoCheckCannotGC &nogc, JSFlatString *str)
{
    return str->latin1Chars(nogc);
}

extern JS_PUBLIC_API(const char16_t *)
JS_GetTwoByteFlatStringChars(const JS::AutoCheckCannotGC &nogc, JSFlatString *str)
{
    return str->twoByteChars(nogc);
}

JS_PUBLIC_API(bool)
JS_CompareStrings(JSContext *cx, JSString *str1, JSString *str2, int32_t *result)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    return CompareStrings(cx, str1, str2, result);
}

JS_PUBLIC_API(bool)
JS_StringEqualsAscii(JSContext *cx, JSString *str, const char *asciiBytes, bool *match)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    JSLinearString *linearStr = str->ensureLinear(cx);
    if (!linearStr)
        return false;
    *match = StringEqualsAscii(linearStr, asciiBytes);
    return true;
}

JS_PUBLIC_API(bool)
JS_FlatStringEqualsAscii(JSFlatString *str, const char *asciiBytes)
{
    return StringEqualsAscii(str, asciiBytes);
}

JS_PUBLIC_API(size_t)
JS_PutEscapedFlatString(char *buffer, size_t size, JSFlatString *str, char quote)
{
    return PutEscapedString(buffer, size, str, quote);
}

JS_PUBLIC_API(size_t)
JS_PutEscapedString(JSContext *cx, char *buffer, size_t size, JSString *str, char quote)
{
    AssertHeapIsIdle(cx);
    JSLinearString *linearStr = str->ensureLinear(cx);
    if (!linearStr)
        return size_t(-1);
    return PutEscapedString(buffer, size, linearStr, quote);
}

JS_PUBLIC_API(bool)
JS_FileEscapedString(FILE *fp, JSString *str, char quote)
{
    JSLinearString *linearStr = str->ensureLinear(nullptr);
    return linearStr && FileEscapedString(fp, linearStr, quote);
}

JS_PUBLIC_API(JSString *)
JS_NewDependentString(JSContext *cx, HandleString str, size_t start, size_t length)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return NewDependentString(cx, str, start, length);
}

JS_PUBLIC_API(JSString *)
JS_ConcatStrings(JSContext *cx, HandleString left, HandleString right)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return ConcatStrings<CanGC>(cx, left, right);
}

JS_PUBLIC_API(bool)
JS_DecodeBytes(JSContext *cx, const char *src, size_t srclen, char16_t *dst, size_t *dstlenp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    if (!dst) {
        *dstlenp = srclen;
        return true;
    }

    size_t dstlen = *dstlenp;

    if (srclen > dstlen) {
        CopyAndInflateChars(dst, src, dstlen);

        AutoSuppressGC suppress(cx);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BUFFER_TOO_SMALL);
        return false;
    }

    CopyAndInflateChars(dst, src, srclen);
    *dstlenp = srclen;
    return true;
}

static char *
EncodeLatin1(ExclusiveContext *cx, JSString *str)
{
    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return nullptr;

    JS::AutoCheckCannotGC nogc;
    if (linear->hasTwoByteChars())
        return JS::LossyTwoByteCharsToNewLatin1CharsZ(cx, linear->twoByteRange(nogc)).c_str();

    size_t len = str->length();
    Latin1Char *buf = cx->pod_malloc<Latin1Char>(len + 1);
    if (!buf)
        return nullptr;

    mozilla::PodCopy(buf, linear->latin1Chars(nogc), len);
    buf[len] = '\0';
    return reinterpret_cast<char*>(buf);
}

JS_PUBLIC_API(char *)
JS_EncodeString(JSContext *cx, JSString *str)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    return EncodeLatin1(cx, str);
}

JS_PUBLIC_API(char *)
JS_EncodeStringToUTF8(JSContext *cx, HandleString str)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return nullptr;

    JS::AutoCheckCannotGC nogc;
    return linear->hasLatin1Chars()
           ? JS::CharsToNewUTF8CharsZ(cx, linear->latin1Range(nogc)).c_str()
           : JS::CharsToNewUTF8CharsZ(cx, linear->twoByteRange(nogc)).c_str();
}

JS_PUBLIC_API(size_t)
JS_GetStringEncodingLength(JSContext *cx, JSString *str)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    if (!str->ensureLinear(cx))
        return size_t(-1);
    return str->length();
}

JS_PUBLIC_API(size_t)
JS_EncodeStringToBuffer(JSContext *cx, JSString *str, char *buffer, size_t length)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    /*
     * FIXME bug 612141 - fix DeflateStringToBuffer interface so the result
     * would allow to distinguish between insufficient buffer and encoding
     * error.
     */
    size_t writtenLength = length;
    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
         return size_t(-1);

    bool res;
    if (linear->hasLatin1Chars()) {
        JS::AutoCheckCannotGC nogc;
        res = DeflateStringToBuffer(nullptr, linear->latin1Chars(nogc), linear->length(), buffer,
                                    &writtenLength);
    } else {
        JS::AutoCheckCannotGC nogc;
        res = DeflateStringToBuffer(nullptr, linear->twoByteChars(nogc), linear->length(), buffer,
                                    &writtenLength);
    }
    if (res) {
        MOZ_ASSERT(writtenLength <= length);
        return writtenLength;
    }
    MOZ_ASSERT(writtenLength <= length);
    size_t necessaryLength = str->length();
    if (necessaryLength == size_t(-1))
        return size_t(-1);
    MOZ_ASSERT(writtenLength == length); // C strings are NOT encoded.
    return necessaryLength;
}

JS_PUBLIC_API(JS::Symbol *)
JS::NewSymbol(JSContext *cx, HandleString description)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (description)
        assertSameCompartment(cx, description);

    return Symbol::new_(cx, SymbolCode::UniqueSymbol, description);
}

JS_PUBLIC_API(JS::Symbol *)
JS::GetSymbolFor(JSContext *cx, HandleString key)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, key);

    return Symbol::for_(cx, key);
}

JS_PUBLIC_API(JSString *)
JS::GetSymbolDescription(HandleSymbol symbol)
{
    return symbol->description();
}

JS_PUBLIC_API(JS::SymbolCode)
JS::GetSymbolCode(Handle<Symbol*> symbol)
{
    return symbol->code();
}

JS_PUBLIC_API(JS::Symbol *)
JS::GetWellKnownSymbol(JSContext *cx, JS::SymbolCode which)
{
    return cx->runtime()->wellKnownSymbols->get(uint32_t(which));
}

JS_PUBLIC_API(bool)
JS_Stringify(JSContext *cx, MutableHandleValue vp, HandleObject replacer,
             HandleValue space, JSONWriteCallback callback, void *data)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, replacer, space);
    StringBuffer sb(cx);
    if (!sb.ensureTwoByteChars())
        return false;
    if (!js_Stringify(cx, vp, replacer, space, sb))
        return false;
    if (sb.empty() && !sb.append(cx->names().null))
        return false;
    return callback(sb.rawTwoByteBegin(), sb.length(), data);
}

JS_PUBLIC_API(bool)
JS_ParseJSON(JSContext *cx, const char16_t *chars, uint32_t len, MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return ParseJSONWithReviver(cx, mozilla::Range<const char16_t>(chars, len), NullHandleValue, vp);
}

JS_PUBLIC_API(bool)
JS_ParseJSON(JSContext *cx, HandleString str, MutableHandleValue vp)
{
    return JS_ParseJSONWithReviver(cx, str, NullHandleValue, vp);
}

JS_PUBLIC_API(bool)
JS_ParseJSONWithReviver(JSContext *cx, const char16_t *chars, uint32_t len, HandleValue reviver, MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return ParseJSONWithReviver(cx, mozilla::Range<const char16_t>(chars, len), reviver, vp);
}

JS_PUBLIC_API(bool)
JS_ParseJSONWithReviver(JSContext *cx, HandleString str, HandleValue reviver, MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, str);

    AutoStableStringChars stableChars(cx);
    if (!stableChars.init(cx, str))
        return false;

    return stableChars.isLatin1()
           ? ParseJSONWithReviver(cx, stableChars.latin1Range(), reviver, vp)
           : ParseJSONWithReviver(cx, stableChars.twoByteRange(), reviver, vp);
}

/************************************************************************/

JS_PUBLIC_API(void)
JS_ReportError(JSContext *cx, const char *format, ...)
{
    va_list ap;

    AssertHeapIsIdle(cx);
    va_start(ap, format);
    js_ReportErrorVA(cx, JSREPORT_ERROR, format, ap);
    va_end(ap);
}

JS_PUBLIC_API(void)
JS_ReportErrorNumber(JSContext *cx, JSErrorCallback errorCallback,
                     void *userRef, const unsigned errorNumber, ...)
{
    va_list ap;
    va_start(ap, errorNumber);
    JS_ReportErrorNumberVA(cx, errorCallback, userRef, errorNumber, ap);
    va_end(ap);
}

JS_PUBLIC_API(void)
JS_ReportErrorNumberVA(JSContext *cx, JSErrorCallback errorCallback,
                       void *userRef, const unsigned errorNumber,
                       va_list ap)
{
    AssertHeapIsIdle(cx);
    js_ReportErrorNumberVA(cx, JSREPORT_ERROR, errorCallback, userRef,
                           errorNumber, ArgumentsAreASCII, ap);
}

JS_PUBLIC_API(void)
JS_ReportErrorNumberUC(JSContext *cx, JSErrorCallback errorCallback,
                       void *userRef, const unsigned errorNumber, ...)
{
    va_list ap;

    AssertHeapIsIdle(cx);
    va_start(ap, errorNumber);
    js_ReportErrorNumberVA(cx, JSREPORT_ERROR, errorCallback, userRef,
                           errorNumber, ArgumentsAreUnicode, ap);
    va_end(ap);
}

JS_PUBLIC_API(void)
JS_ReportErrorNumberUCArray(JSContext *cx, JSErrorCallback errorCallback,
                            void *userRef, const unsigned errorNumber,
                            const char16_t **args)
{
    AssertHeapIsIdle(cx);
    js_ReportErrorNumberUCArray(cx, JSREPORT_ERROR, errorCallback, userRef,
                                errorNumber, args);
}

JS_PUBLIC_API(bool)
JS_ReportWarning(JSContext *cx, const char *format, ...)
{
    va_list ap;
    bool ok;

    AssertHeapIsIdle(cx);
    va_start(ap, format);
    ok = js_ReportErrorVA(cx, JSREPORT_WARNING, format, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(bool)
JS_ReportErrorFlagsAndNumber(JSContext *cx, unsigned flags,
                             JSErrorCallback errorCallback, void *userRef,
                             const unsigned errorNumber, ...)
{
    va_list ap;
    bool ok;

    AssertHeapIsIdle(cx);
    va_start(ap, errorNumber);
    ok = js_ReportErrorNumberVA(cx, flags, errorCallback, userRef,
                                errorNumber, ArgumentsAreASCII, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(bool)
JS_ReportErrorFlagsAndNumberUC(JSContext *cx, unsigned flags,
                               JSErrorCallback errorCallback, void *userRef,
                               const unsigned errorNumber, ...)
{
    va_list ap;
    bool ok;

    AssertHeapIsIdle(cx);
    va_start(ap, errorNumber);
    ok = js_ReportErrorNumberVA(cx, flags, errorCallback, userRef,
                                errorNumber, ArgumentsAreUnicode, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(void)
JS_ReportOutOfMemory(JSContext *cx)
{
    js_ReportOutOfMemory(cx);
}

JS_PUBLIC_API(void)
JS_ReportAllocationOverflow(JSContext *cx)
{
    js_ReportAllocationOverflow(cx);
}

JS_PUBLIC_API(JSErrorReporter)
JS_GetErrorReporter(JSRuntime *rt)
{
    return rt->errorReporter;
}

JS_PUBLIC_API(JSErrorReporter)
JS_SetErrorReporter(JSRuntime *rt, JSErrorReporter er)
{
    JSErrorReporter older;

    older = rt->errorReporter;
    rt->errorReporter = er;
    return older;
}

/************************************************************************/

/*
 * Dates.
 */
JS_PUBLIC_API(JSObject *)
JS_NewDateObject(JSContext *cx, int year, int mon, int mday, int hour, int min, int sec)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return js_NewDateObject(cx, year, mon, mday, hour, min, sec);
}

JS_PUBLIC_API(JSObject *)
JS_NewDateObjectMsec(JSContext *cx, double msec)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return js_NewDateObjectMsec(cx, msec);
}

JS_PUBLIC_API(bool)
JS_ObjectIsDate(JSContext *cx, HandleObject obj)
{
    assertSameCompartment(cx, obj);
    return ObjectClassIs(obj, ESClass_Date, cx);
}

JS_PUBLIC_API(void)
JS_ClearDateCaches(JSContext *cx)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    cx->runtime()->dateTimeInfo.updateTimeZoneAdjustment();
}

/************************************************************************/

/*
 * Regular Expressions.
 */
JS_PUBLIC_API(JSObject *)
JS_NewRegExpObject(JSContext *cx, HandleObject obj, const char *bytes, size_t length, unsigned flags)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    ScopedJSFreePtr<char16_t> chars(InflateString(cx, bytes, &length));
    if (!chars)
        return nullptr;

    RegExpStatics *res = obj->as<GlobalObject>().getRegExpStatics(cx);
    if (!res)
        return nullptr;

    RegExpObject *reobj = RegExpObject::create(cx, res, chars, length,
                                               RegExpFlag(flags), nullptr, cx->tempLifoAlloc());
    return reobj;
}

JS_PUBLIC_API(JSObject *)
JS_NewUCRegExpObject(JSContext *cx, HandleObject obj, const char16_t *chars, size_t length,
                     unsigned flags)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    RegExpStatics *res = obj->as<GlobalObject>().getRegExpStatics(cx);
    if (!res)
        return nullptr;

    return RegExpObject::create(cx, res, chars, length,
                                RegExpFlag(flags), nullptr, cx->tempLifoAlloc());
}

JS_PUBLIC_API(bool)
JS_SetRegExpInput(JSContext *cx, HandleObject obj, HandleString input, bool multiline)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, input);

    RegExpStatics *res = obj->as<GlobalObject>().getRegExpStatics(cx);
    if (!res)
        return false;

    res->reset(cx, input, !!multiline);
    return true;
}

JS_PUBLIC_API(bool)
JS_ClearRegExpStatics(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    MOZ_ASSERT(obj);

    RegExpStatics *res = obj->as<GlobalObject>().getRegExpStatics(cx);
    if (!res)
        return false;

    res->clear();
    return true;
}

JS_PUBLIC_API(bool)
JS_ExecuteRegExp(JSContext *cx, HandleObject obj, HandleObject reobj, char16_t *chars,
                 size_t length, size_t *indexp, bool test, MutableHandleValue rval)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    RegExpStatics *res = obj->as<GlobalObject>().getRegExpStatics(cx);
    if (!res)
        return false;

    RootedLinearString input(cx, NewStringCopyN<CanGC>(cx, chars, length));
    if (!input)
        return false;

    return ExecuteRegExpLegacy(cx, res, reobj->as<RegExpObject>(), input, indexp, test, rval);
}

JS_PUBLIC_API(JSObject *)
JS_NewRegExpObjectNoStatics(JSContext *cx, char *bytes, size_t length, unsigned flags)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    char16_t *chars = InflateString(cx, bytes, &length);
    if (!chars)
        return nullptr;
    RegExpObject *reobj = RegExpObject::createNoStatics(cx, chars, length,
                                                        RegExpFlag(flags), nullptr, cx->tempLifoAlloc());
    js_free(chars);
    return reobj;
}

JS_PUBLIC_API(JSObject *)
JS_NewUCRegExpObjectNoStatics(JSContext *cx, char16_t *chars, size_t length, unsigned flags)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return RegExpObject::createNoStatics(cx, chars, length,
                                         RegExpFlag(flags), nullptr, cx->tempLifoAlloc());
}

JS_PUBLIC_API(bool)
JS_ExecuteRegExpNoStatics(JSContext *cx, HandleObject obj, char16_t *chars, size_t length,
                          size_t *indexp, bool test, MutableHandleValue rval)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    RootedLinearString input(cx, NewStringCopyN<CanGC>(cx, chars, length));
    if (!input)
        return false;

    return ExecuteRegExpLegacy(cx, nullptr, obj->as<RegExpObject>(), input, indexp, test,
                               rval);
}

JS_PUBLIC_API(bool)
JS_ObjectIsRegExp(JSContext *cx, HandleObject obj)
{
    assertSameCompartment(cx, obj);
    return ObjectClassIs(obj, ESClass_RegExp, cx);
}

JS_PUBLIC_API(unsigned)
JS_GetRegExpFlags(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    RegExpGuard shared(cx);
    if (!RegExpToShared(cx, obj, &shared))
        return false;
    return shared.re()->getFlags();
}

JS_PUBLIC_API(JSString *)
JS_GetRegExpSource(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    RegExpGuard shared(cx);
    if (!RegExpToShared(cx, obj, &shared))
        return nullptr;
    return shared.re()->getSource();
}

/************************************************************************/

JS_PUBLIC_API(bool)
JS_SetDefaultLocale(JSRuntime *rt, const char *locale)
{
    AssertHeapIsIdle(rt);
    return rt->setDefaultLocale(locale);
}

JS_PUBLIC_API(void)
JS_ResetDefaultLocale(JSRuntime *rt)
{
    AssertHeapIsIdle(rt);
    rt->resetDefaultLocale();
}

JS_PUBLIC_API(void)
JS_SetLocaleCallbacks(JSRuntime *rt, JSLocaleCallbacks *callbacks)
{
    AssertHeapIsIdle(rt);
    rt->localeCallbacks = callbacks;
}

JS_PUBLIC_API(JSLocaleCallbacks *)
JS_GetLocaleCallbacks(JSRuntime *rt)
{
    /* This function can be called by a finalizer. */
    return rt->localeCallbacks;
}

/************************************************************************/

JS_PUBLIC_API(bool)
JS_IsExceptionPending(JSContext *cx)
{
    /* This function can be called by a finalizer. */
    return (bool) cx->isExceptionPending();
}

JS_PUBLIC_API(bool)
JS_GetPendingException(JSContext *cx, MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (!cx->isExceptionPending())
        return false;
    return cx->getPendingException(vp);
}

JS_PUBLIC_API(void)
JS_SetPendingException(JSContext *cx, HandleValue value)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    cx->setPendingException(value);
}

JS_PUBLIC_API(void)
JS_ClearPendingException(JSContext *cx)
{
    AssertHeapIsIdle(cx);
    cx->clearPendingException();
}

JS_PUBLIC_API(bool)
JS_ReportPendingException(JSContext *cx)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    // This can only fail due to oom.
    bool ok = js_ReportUncaughtException(cx);
    MOZ_ASSERT(!cx->isExceptionPending());
    return ok;
}

JS::AutoSaveExceptionState::AutoSaveExceptionState(JSContext *cx)
  : context(cx),
    wasPropagatingForcedReturn(cx->propagatingForcedReturn_),
    wasThrowing(cx->throwing),
    exceptionValue(cx)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (wasPropagatingForcedReturn)
        cx->clearPropagatingForcedReturn();
    if (wasThrowing) {
        exceptionValue = cx->unwrappedException_;
        cx->clearPendingException();
    }
}

void
JS::AutoSaveExceptionState::restore()
{
    context->propagatingForcedReturn_ = wasPropagatingForcedReturn;
    context->throwing = wasThrowing;
    context->unwrappedException_ = exceptionValue;
    drop();
}

JS::AutoSaveExceptionState::~AutoSaveExceptionState()
{
    if (!context->isExceptionPending()) {
        if (wasPropagatingForcedReturn)
            context->setPropagatingForcedReturn();
        if (wasThrowing) {
            context->throwing = true;
            context->unwrappedException_ = exceptionValue;
        }
    }
}

struct JSExceptionState {
    bool throwing;
    jsval exception;
};

JS_PUBLIC_API(JSExceptionState *)
JS_SaveExceptionState(JSContext *cx)
{
    JSExceptionState *state;

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    state = cx->pod_malloc<JSExceptionState>();
    if (state) {
        state->throwing =
            JS_GetPendingException(cx, MutableHandleValue::fromMarkedLocation(&state->exception));
        if (state->throwing && state->exception.isGCThing())
            AddValueRoot(cx, &state->exception, "JSExceptionState.exception");
    }
    return state;
}

JS_PUBLIC_API(void)
JS_RestoreExceptionState(JSContext *cx, JSExceptionState *state)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (state) {
        if (state->throwing)
            JS_SetPendingException(cx, HandleValue::fromMarkedLocation(&state->exception));
        else
            JS_ClearPendingException(cx);
        JS_DropExceptionState(cx, state);
    }
}

JS_PUBLIC_API(void)
JS_DropExceptionState(JSContext *cx, JSExceptionState *state)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (state) {
        if (state->throwing && state->exception.isGCThing()) {
            assertSameCompartment(cx, state->exception);
            RemoveRoot(cx->runtime(), &state->exception);
        }
        js_free(state);
    }
}

JS_PUBLIC_API(JSErrorReport *)
JS_ErrorFromException(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return js_ErrorFromException(cx, obj);
}

JS_PUBLIC_API(bool)
JS_ThrowStopIteration(JSContext *cx)
{
    AssertHeapIsIdle(cx);
    return ThrowStopIteration(cx);
}

JS_PUBLIC_API(bool)
JS_IsStopIteration(jsval v)
{
    return v.isObject() && v.toObject().is<StopIterationObject>();
}

JS_PUBLIC_API(intptr_t)
JS_GetCurrentThread()
{
    return reinterpret_cast<intptr_t>(PR_GetCurrentThread());
}

extern MOZ_NEVER_INLINE JS_PUBLIC_API(void)
JS_AbortIfWrongThread(JSRuntime *rt)
{
    if (!CurrentThreadCanAccessRuntime(rt))
        MOZ_CRASH();
    if (!js::TlsPerThreadData.get()->associatedWith(rt))
        MOZ_CRASH();
}

#ifdef JS_GC_ZEAL
JS_PUBLIC_API(void)
JS_SetGCZeal(JSContext *cx, uint8_t zeal, uint32_t frequency)
{
    cx->runtime()->gc.setZeal(zeal, frequency);
}

JS_PUBLIC_API(void)
JS_ScheduleGC(JSContext *cx, uint32_t count)
{
    cx->runtime()->gc.setNextScheduled(count);
}
#endif

JS_PUBLIC_API(void)
JS_SetParallelParsingEnabled(JSRuntime *rt, bool enabled)
{
    rt->setParallelParsingEnabled(enabled);
}

JS_PUBLIC_API(void)
JS_SetOffthreadIonCompilationEnabled(JSRuntime *rt, bool enabled)
{
    rt->setOffthreadIonCompilationEnabled(enabled);
}

JS_PUBLIC_API(void)
JS_SetGlobalJitCompilerOption(JSRuntime *rt, JSJitCompilerOption opt, uint32_t value)
{
    switch (opt) {
      case JSJITCOMPILER_BASELINE_WARMUP_TRIGGER:
        if (value == uint32_t(-1)) {
            jit::JitOptions defaultValues;
            value = defaultValues.baselineWarmUpThreshold;
        }
        jit::js_JitOptions.baselineWarmUpThreshold = value;
        break;
      case JSJITCOMPILER_ION_WARMUP_TRIGGER:
        if (value == uint32_t(-1)) {
            jit::js_JitOptions.resetCompilerWarmUpThreshold();
            break;
        }
        jit::js_JitOptions.setCompilerWarmUpThreshold(value);
        if (value == 0)
            jit::js_JitOptions.setEagerCompilation();
        break;
      case JSJITCOMPILER_ION_ENABLE:
        if (value == 1) {
            JS::RuntimeOptionsRef(rt).setIon(true);
            JitSpew(js::jit::JitSpew_IonScripts, "Enable ion");
        } else if (value == 0) {
            JS::RuntimeOptionsRef(rt).setIon(false);
            JitSpew(js::jit::JitSpew_IonScripts, "Disable ion");
        }
        break;
      case JSJITCOMPILER_BASELINE_ENABLE:
        if (value == 1) {
            JS::RuntimeOptionsRef(rt).setBaseline(true);
            ReleaseAllJITCode(rt->defaultFreeOp());
            JitSpew(js::jit::JitSpew_BaselineScripts, "Enable baseline");
        } else if (value == 0) {
            JS::RuntimeOptionsRef(rt).setBaseline(false);
            ReleaseAllJITCode(rt->defaultFreeOp());
            JitSpew(js::jit::JitSpew_BaselineScripts, "Disable baseline");
        }
        break;
      case JSJITCOMPILER_OFFTHREAD_COMPILATION_ENABLE:
        if (value == 1) {
            rt->setOffthreadIonCompilationEnabled(true);
            JitSpew(js::jit::JitSpew_IonScripts, "Enable offthread compilation");
        } else if (value == 0) {
            rt->setOffthreadIonCompilationEnabled(false);
            JitSpew(js::jit::JitSpew_IonScripts, "Disable offthread compilation");
        }
        break;
      case JSJITCOMPILER_SIGNALS_ENABLE:
        if (value == 1) {
            rt->setCanUseSignalHandlers(true);
            JitSpew(js::jit::JitSpew_IonScripts, "Enable signals");
        } else if (value == 0) {
            rt->setCanUseSignalHandlers(false);
            JitSpew(js::jit::JitSpew_IonScripts, "Disable signals");
        }
        break;
      default:
        break;
    }
}

JS_PUBLIC_API(int)
JS_GetGlobalJitCompilerOption(JSRuntime *rt, JSJitCompilerOption opt)
{
#ifndef JS_CODEGEN_NONE
    switch (opt) {
      case JSJITCOMPILER_BASELINE_WARMUP_TRIGGER:
        return jit::js_JitOptions.baselineWarmUpThreshold;
      case JSJITCOMPILER_ION_WARMUP_TRIGGER:
        return jit::js_JitOptions.forcedDefaultIonWarmUpThreshold;
      case JSJITCOMPILER_ION_ENABLE:
        return JS::RuntimeOptionsRef(rt).ion();
      case JSJITCOMPILER_BASELINE_ENABLE:
        return JS::RuntimeOptionsRef(rt).baseline();
      case JSJITCOMPILER_OFFTHREAD_COMPILATION_ENABLE:
        return rt->canUseOffthreadIonCompilation();
      case JSJITCOMPILER_SIGNALS_ENABLE:
        return rt->canUseSignalHandlers();
      default:
        break;
    }
#endif
    return 0;
}

/************************************************************************/

#if !defined(STATIC_EXPORTABLE_JS_API) && !defined(STATIC_JS_API) && defined(XP_WIN)

#include "jswin.h"

/*
 * Initialization routine for the JS DLL.
 */
BOOL WINAPI DllMain (HINSTANCE hDLL, DWORD dwReason, LPVOID lpReserved)
{
    return TRUE;
}

#endif

JS_PUBLIC_API(bool)
JS_IndexToId(JSContext *cx, uint32_t index, MutableHandleId id)
{
    return IndexToId(cx, index, id);
}

JS_PUBLIC_API(bool)
JS_CharsToId(JSContext* cx, JS::TwoByteChars chars, MutableHandleId idp)
{
    RootedAtom atom(cx, AtomizeChars(cx, chars.start().get(), chars.length()));
    if (!atom)
        return false;
#ifdef DEBUG
    uint32_t dummy;
    MOZ_ASSERT(!atom->isIndex(&dummy), "API misuse: |chars| must not encode an index");
#endif
    idp.set(AtomToId(atom));
    return true;
}

JS_PUBLIC_API(bool)
JS_IsIdentifier(JSContext *cx, HandleString str, bool *isIdentifier)
{
    assertSameCompartment(cx, str);

    JSLinearString* linearStr = str->ensureLinear(cx);
    if (!linearStr)
        return false;

    *isIdentifier = js::frontend::IsIdentifier(linearStr);
    return true;
}

namespace JS {

void
AutoFilename::reset(void *newScriptSource)
{
    if (newScriptSource)
        reinterpret_cast<ScriptSource*>(newScriptSource)->incref();
    if (scriptSource_)
        reinterpret_cast<ScriptSource*>(scriptSource_)->decref();
    scriptSource_ = newScriptSource;
}

const char *
AutoFilename::get() const
{
    return scriptSource_ ? reinterpret_cast<ScriptSource*>(scriptSource_)->filename() : nullptr;
}

JS_PUBLIC_API(bool)
DescribeScriptedCaller(JSContext *cx, AutoFilename *filename, unsigned *lineno)
{
    if (lineno)
        *lineno = 0;

    NonBuiltinFrameIter i(cx);
    if (i.done())
        return false;

    // If the caller is hidden, the embedding wants us to return false here so
    // that it can check its own stack (see HideScriptedCaller).
    if (i.activation()->scriptedCallerIsHidden())
        return false;

    if (filename)
        filename->reset(i.scriptSource());
    if (lineno)
        *lineno = i.computeLine();
    return true;
}

JS_PUBLIC_API(JSObject *)
GetScriptedCallerGlobal(JSContext *cx)
{
    NonBuiltinFrameIter i(cx);
    if (i.done())
        return nullptr;

    // If the caller is hidden, the embedding wants us to return null here so
    // that it can check its own stack (see HideScriptedCaller).
    if (i.activation()->scriptedCallerIsHidden())
        return nullptr;

    GlobalObject *global = i.activation()->compartment()->maybeGlobal();

    // Noone should be running code in the atoms compartment or running code in
    // a compartment without any live objects, so there should definitely be a
    // live global.
    MOZ_ASSERT(global);

    return global;
}

JS_PUBLIC_API(void)
HideScriptedCaller(JSContext *cx)
{
    MOZ_ASSERT(cx);

    // If there's no accessible activation on the stack, we'll return null from
    // DescribeScriptedCaller anyway, so there's no need to annotate anything.
    Activation *act = cx->runtime()->mainThread.activation();
    if (!act)
        return;
    act->hideScriptedCaller();
}

JS_PUBLIC_API(void)
UnhideScriptedCaller(JSContext *cx)
{
    Activation *act = cx->runtime()->mainThread.activation();
    if (!act)
        return;
    act->unhideScriptedCaller();
}

} /* namespace JS */

static PRStatus
CallOnce(void *func)
{
    JSInitCallback init = JS_DATA_TO_FUNC_PTR(JSInitCallback, func);
    return init() ? PR_SUCCESS : PR_FAILURE;
}

JS_PUBLIC_API(bool)
JS_CallOnce(JSCallOnceType *once, JSInitCallback func)
{
    return PR_CallOnceWithArg(once, CallOnce, JS_FUNC_TO_DATA_PTR(void *, func)) == PR_SUCCESS;
}

AutoGCRooter::AutoGCRooter(JSContext *cx, ptrdiff_t tag)
  : down(ContextFriendFields::get(cx)->autoGCRooters),
    tag_(tag),
    stackTop(&ContextFriendFields::get(cx)->autoGCRooters)
{
    MOZ_ASSERT(this != *stackTop);
    *stackTop = this;
}

AutoGCRooter::AutoGCRooter(ContextFriendFields *cx, ptrdiff_t tag)
  : down(cx->autoGCRooters),
    tag_(tag),
    stackTop(&cx->autoGCRooters)
{
    MOZ_ASSERT(this != *stackTop);
    *stackTop = this;
}

#ifdef DEBUG
JS_PUBLIC_API(void)
JS::AssertArgumentsAreSane(JSContext *cx, HandleValue value)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
}
#endif /* DEBUG */

JS_PUBLIC_API(void *)
JS_EncodeScript(JSContext *cx, HandleScript scriptArg, uint32_t *lengthp)
{
    XDREncoder encoder(cx);
    RootedScript script(cx, scriptArg);
    if (!encoder.codeScript(&script))
        return nullptr;
    return encoder.forgetData(lengthp);
}

JS_PUBLIC_API(void *)
JS_EncodeInterpretedFunction(JSContext *cx, HandleObject funobjArg, uint32_t *lengthp)
{
    XDREncoder encoder(cx);
    RootedFunction funobj(cx, &funobjArg->as<JSFunction>());
    if (!encoder.codeFunction(&funobj))
        return nullptr;
    return encoder.forgetData(lengthp);
}

JS_PUBLIC_API(JSScript *)
JS_DecodeScript(JSContext *cx, const void *data, uint32_t length)
{
    XDRDecoder decoder(cx, data, length);
    RootedScript script(cx);
    if (!decoder.codeScript(&script))
        return nullptr;
    return script;
}

JS_PUBLIC_API(JSObject *)
JS_DecodeInterpretedFunction(JSContext *cx, const void *data, uint32_t length)
{
    XDRDecoder decoder(cx, data, length);
    RootedFunction funobj(cx);
    if (!decoder.codeFunction(&funobj))
        return nullptr;
    return funobj;
}

JS_PUBLIC_API(bool)
JS_PreventExtensions(JSContext *cx, JS::HandleObject obj)
{
    return JSObject::preventExtensions(cx, obj);
}

JS_PUBLIC_API(void)
JS::SetAsmJSCacheOps(JSRuntime *rt, const JS::AsmJSCacheOps *ops)
{
    rt->asmJSCacheOps = *ops;
}

char *
JSAutoByteString::encodeLatin1(ExclusiveContext *cx, JSString *str)
{
    mBytes = EncodeLatin1(cx, str);
    return mBytes;
}

JS_PUBLIC_API(void)
JS::SetLargeAllocationFailureCallback(JSRuntime *rt, JS::LargeAllocationFailureCallback lafc,
                                      void *data)
{
    rt->largeAllocationFailureCallback = lafc;
    rt->largeAllocationFailureCallbackData = data;
}

JS_PUBLIC_API(void)
JS::SetOutOfMemoryCallback(JSRuntime *rt, OutOfMemoryCallback cb, void *data)
{
    rt->oomCallback = cb;
    rt->oomCallbackData = data;
}

JS_PUBLIC_API(bool)
JS::CaptureCurrentStack(JSContext *cx, JS::MutableHandleObject stackp, unsigned maxFrameCount)
{
    JSCompartment *compartment = cx->compartment();
    MOZ_ASSERT(compartment);
    Rooted<SavedFrame *> frame(cx);
    if (!compartment->savedStacks().saveCurrentStack(cx, &frame, maxFrameCount))
        return false;
    stackp.set(frame.get());
    return true;
}

JS_PUBLIC_API(Zone *)
JS::GetObjectZone(JSObject *obj)
{
    return obj->zone();
}

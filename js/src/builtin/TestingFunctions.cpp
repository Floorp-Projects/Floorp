/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jsfriendapi.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsobjinlines.h"
#include "jsprf.h"
#include "jswrapper.h"

#include "builtin/TestingFunctions.h"
#include "methodjit/MethodJIT.h"
#include "vm/ForkJoin.h"

#include "vm/Stack-inl.h"

using namespace js;
using namespace JS;

using mozilla::ArrayLength;

static JSBool
GetBuildConfiguration(JSContext *cx, unsigned argc, jsval *vp)
{
    RootedObject info(cx, JS_NewObject(cx, NULL, NULL, NULL));
    if (!info)
        return false;
    Value value;

#ifdef JSGC_ROOT_ANALYSIS
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "rooting-analysis", &value))
        return false;

#ifdef JSGC_USE_EXACT_ROOTING
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "exact-rooting", &value))
        return false;

#ifdef DEBUG
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "debug", &value))
        return false;

#ifdef JS_HAS_CTYPES
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "has-ctypes", &value))
        return false;

#ifdef JS_CPU_X86
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "x86", &value))
        return false;

#ifdef JS_CPU_X64
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "x64", &value))
        return false;

#ifdef MOZ_ASAN
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "asan", &value))
        return false;

#ifdef JS_GC_ZEAL
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "has-gczeal", &value))
        return false;

#ifdef JS_THREADSAFE
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "threadsafe", &value))
        return false;

#ifdef JS_MORE_DETERMINISTIC
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "more-deterministic", &value))
        return false;

#ifdef MOZ_PROFILING
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "profiling", &value))
        return false;

#ifdef INCLUDE_MOZILLA_DTRACE
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "dtrace", &value))
        return false;

#ifdef MOZ_TRACE_JSCALLS
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "trace-jscalls-api", &value))
        return false;

#ifdef JSGC_INCREMENTAL
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "incremental-gc", &value))
        return false;

#ifdef JSGC_GENERATIONAL
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "generational-gc", &value))
        return false;

#ifdef MOZ_VALGRIND
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "valgrind", &value))
        return false;

#ifdef JS_OOM_DO_BACKTRACES
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "oom-backtraces", &value))
        return false;

#ifdef JS_METHODJIT
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "methodjit", &value))
        return false;

    *vp = ObjectValue(*info);
    return true;
}

static JSBool
GC(JSContext *cx, unsigned argc, jsval *vp)
{
    /*
     * If the first argument is 'compartment', we collect any compartments
     * previously scheduled for GC via schedulegc. If the first argument is an
     * object, we collect the object's compartment (and any other compartments
     * scheduled for GC). Otherwise, we collect all compartments.
     */
    JSBool compartment = false;
    if (argc == 1) {
        Value arg = vp[2];
        if (arg.isString()) {
            if (!JS_StringEqualsAscii(cx, arg.toString(), "compartment", &compartment))
                return false;
        } else if (arg.isObject()) {
            PrepareZoneForGC(UnwrapObject(&arg.toObject())->zone());
            compartment = true;
        }
    }

#ifndef JS_MORE_DETERMINISTIC
    size_t preBytes = cx->runtime->gcBytes;
#endif

    if (compartment)
        PrepareForDebugGC(cx->runtime);
    else
        PrepareForFullGC(cx->runtime);
    GCForReason(cx->runtime, gcreason::API);

    char buf[256] = { '\0' };
#ifndef JS_MORE_DETERMINISTIC
    JS_snprintf(buf, sizeof(buf), "before %lu, after %lu\n",
                (unsigned long)preBytes, (unsigned long)cx->runtime->gcBytes);
#endif
    JSString *str = JS_NewStringCopyZ(cx, buf);
    if (!str)
        return false;
    *vp = STRING_TO_JSVAL(str);
    return true;
}

static const struct ParamPair {
    const char      *name;
    JSGCParamKey    param;
} paramMap[] = {
    {"maxBytes",            JSGC_MAX_BYTES },
    {"maxMallocBytes",      JSGC_MAX_MALLOC_BYTES},
    {"gcBytes",             JSGC_BYTES},
    {"gcNumber",            JSGC_NUMBER},
    {"sliceTimeBudget",     JSGC_SLICE_TIME_BUDGET}
};

static JSBool
GCParameter(JSContext *cx, unsigned argc, jsval *vp)
{
    JSString *str;
    if (argc == 0) {
        str = JS_ValueToString(cx, JSVAL_VOID);
        JS_ASSERT(str);
    } else {
        str = JS_ValueToString(cx, vp[2]);
        if (!str)
            return JS_FALSE;
        vp[2] = STRING_TO_JSVAL(str);
    }

    JSFlatString *flatStr = JS_FlattenString(cx, str);
    if (!flatStr)
        return false;

    size_t paramIndex = 0;
    for (;; paramIndex++) {
        if (paramIndex == ArrayLength(paramMap)) {
            JS_ReportError(cx,
                           "the first argument argument must be maxBytes, "
                           "maxMallocBytes, gcStackpoolLifespan, gcBytes or "
                           "gcNumber");
            return false;
        }
        if (JS_FlatStringEqualsAscii(flatStr, paramMap[paramIndex].name))
            break;
    }
    JSGCParamKey param = paramMap[paramIndex].param;

    if (argc == 1) {
        uint32_t value = JS_GetGCParameter(cx->runtime, param);
        vp[0] = JS_NumberValue(value);
        return true;
    }

    if (param == JSGC_NUMBER ||
        param == JSGC_BYTES) {
        JS_ReportError(cx, "Attempt to change read-only parameter %s",
                       paramMap[paramIndex].name);
        return false;
    }

    uint32_t value;
    if (!JS_ValueToECMAUint32(cx, vp[3], &value)) {
        JS_ReportError(cx,
                       "the second argument must be convertable to uint32_t "
                       "with non-zero value");
        return false;
    }

    if (param == JSGC_MAX_BYTES) {
        uint32_t gcBytes = JS_GetGCParameter(cx->runtime, JSGC_BYTES);
        if (value < gcBytes) {
            JS_ReportError(cx,
                           "attempt to set maxBytes to the value less than the current "
                           "gcBytes (%u)",
                           gcBytes);
            return false;
        }
    }

    JS_SetGCParameter(cx->runtime, param, value);
    *vp = JSVAL_VOID;
    return true;
}

static JSBool
IsProxy(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (argc != 1) {
        JS_ReportError(cx, "the function takes exactly one argument");
        return false;
    }
    if (!args[0].isObject()) {
        args.rval().setBoolean(false);
        return true;
    }
    args.rval().setBoolean(args[0].toObject().isProxy());
    return true;
}

static JSBool
InternalConst(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc != 1) {
        JS_ReportError(cx, "the function takes exactly one argument");
        return false;
    }

    JSString *str = JS_ValueToString(cx, vp[2]);
    if (!str)
        return false;
    JSFlatString *flat = JS_FlattenString(cx, str);
    if (!flat)
        return false;

    if (JS_FlatStringEqualsAscii(flat, "MARK_STACK_LENGTH")) {
        vp[0] = UINT_TO_JSVAL(js::MARK_STACK_LENGTH);
    } else {
        JS_ReportError(cx, "unknown const name");
        return false;
    }
    return true;
}

static JSBool
GCPreserveCode(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc != 0) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return JS_FALSE;
    }

    cx->runtime->alwaysPreserveCode = true;

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

#ifdef JS_GC_ZEAL
static JSBool
GCZeal(JSContext *cx, unsigned argc, jsval *vp)
{
    uint32_t zeal, frequency = JS_DEFAULT_ZEAL_FREQ;
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc > 2) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Too many arguments");
        return JS_FALSE;
    }
    if (!JS_ValueToECMAUint32(cx, argc < 1 ? JSVAL_VOID : args[0], &zeal))
        return JS_FALSE;
    if (argc >= 2)
        if (!JS_ValueToECMAUint32(cx, args[1], &frequency))
            return JS_FALSE;

    JS_SetGCZeal(cx, (uint8_t)zeal, frequency);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
ScheduleGC(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc != 1) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return JS_FALSE;
    }

    if (args[0].isInt32()) {
        /* Schedule a GC to happen after |arg| allocations. */
        JS_ScheduleGC(cx, args[0].toInt32());
    } else if (args[0].isObject()) {
        /* Ensure that |zone| is collected during the next GC. */
        Zone *zone = UnwrapObject(&args[0].toObject())->zone();
        PrepareZoneForGC(zone);
    } else if (args[0].isString()) {
        /* This allows us to schedule atomsCompartment for GC. */
        PrepareZoneForGC(args[0].toString()->zone());
    }

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
SelectForGC(JSContext *cx, unsigned argc, jsval *vp)
{
    JSRuntime *rt = cx->runtime;

    for (unsigned i = 0; i < argc; i++) {
        Value arg(JS_ARGV(cx, vp)[i]);
        if (arg.isObject()) {
            if (!rt->gcSelectedForMarking.append(&arg.toObject()))
                return false;
        }
    }

    *vp = JSVAL_VOID;
    return true;
}

static JSBool
VerifyPreBarriers(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Too many arguments");
        return JS_FALSE;
    }
    gc::VerifyBarriers(cx->runtime, gc::PreBarrierVerifier);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
VerifyPostBarriers(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc) {
        RootedObject callee(cx, &JS_CALLEE(cx, vp).toObject());
        ReportUsageError(cx, callee, "Too many arguments");
        return JS_FALSE;
    }
    gc::VerifyBarriers(cx->runtime, gc::PostBarrierVerifier);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
GCState(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc != 0) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Too many arguments");
        return false;
    }

    const char *state;
    gc::State globalState = cx->runtime->gcIncrementalState;
    if (globalState == gc::NO_INCREMENTAL)
        state = "none";
    else if (globalState == gc::MARK)
        state = "mark";
    else if (globalState == gc::SWEEP)
        state = "sweep";
    else
        JS_NOT_REACHED("Unobserveable global GC state");

    JSString *str = JS_NewStringCopyZ(cx, state);
    if (!str)
        return false;
    *vp = StringValue(str);
    return true;
}

static JSBool
DeterministicGC(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc != 1) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return JS_FALSE;
    }

    gc::SetDeterministicGC(cx, ToBoolean(vp[2]));
    *vp = JSVAL_VOID;
    return JS_TRUE;
}
#endif /* JS_GC_ZEAL */

static JSBool
GCSlice(JSContext *cx, unsigned argc, jsval *vp)
{
    bool limit = true;
    uint32_t budget = 0;
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc > 1) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return JS_FALSE;
    }

    if (argc == 1) {
        if (!JS_ValueToECMAUint32(cx, args[0], &budget))
            return false;
    } else {
        limit = false;
    }

    GCDebugSlice(cx->runtime, limit, budget);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
ValidateGC(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc != 1) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return JS_FALSE;
    }

    gc::SetValidateGC(cx, ToBoolean(vp[2]));
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
FullCompartmentChecks(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc != 1) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return JS_FALSE;
    }

    gc::SetFullCompartmentChecks(cx, ToBoolean(vp[2]));
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
NondeterminsticGetWeakMapKeys(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc != 1) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return false;
    }
    if (!args[0].isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_EXPECTED_TYPE,
                             "nondeterministicGetWeakMapKeys", "WeakMap",
                             InformalValueTypeName(args[0]));
        return false;
    }
    JSObject *arr;
    if (!JS_NondeterministicGetWeakMapKeys(cx, &args[0].toObject(), &arr))
        return false;
    if (!arr) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_EXPECTED_TYPE,
                             "nondeterministicGetWeakMapKeys", "WeakMap",
                             args[0].toObject().getClass()->name);
        return false;
    }
    args.rval().setObject(*arr);
    return true;
}

struct JSCountHeapNode {
    void                *thing;
    JSGCTraceKind       kind;
    JSCountHeapNode     *next;
};

typedef HashSet<void *, PointerHasher<void *, 3>, SystemAllocPolicy> VisitedSet;

typedef struct JSCountHeapTracer {
    JSTracer            base;
    VisitedSet          visited;
    JSCountHeapNode     *traceList;
    JSCountHeapNode     *recycleList;
    bool                ok;
} JSCountHeapTracer;

static void
CountHeapNotify(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    JS_ASSERT(trc->callback == CountHeapNotify);

    JSCountHeapTracer *countTracer = (JSCountHeapTracer *)trc;
    void *thing = *thingp;

    if (!countTracer->ok)
        return;

    VisitedSet::AddPtr p = countTracer->visited.lookupForAdd(thing);
    if (p)
        return;

    if (!countTracer->visited.add(p, thing)) {
        countTracer->ok = false;
        return;
    }

    JSCountHeapNode *node = countTracer->recycleList;
    if (node) {
        countTracer->recycleList = node->next;
    } else {
        node = js_pod_malloc<JSCountHeapNode>();
        if (!node) {
            countTracer->ok = false;
            return;
        }
    }
    node->thing = thing;
    node->kind = kind;
    node->next = countTracer->traceList;
    countTracer->traceList = node;
}

static const struct TraceKindPair {
    const char       *name;
    int32_t           kind;
} traceKindNames[] = {
    { "all",        -1                  },
    { "object",     JSTRACE_OBJECT      },
    { "string",     JSTRACE_STRING      },
};

static JSBool
CountHeap(JSContext *cx, unsigned argc, jsval *vp)
{
    void* startThing;
    JSGCTraceKind startTraceKind;
    jsval v;
    int32_t traceKind;
    JSString *str;
    JSCountHeapTracer countTracer;
    JSCountHeapNode *node;
    size_t counter;

    startThing = NULL;
    startTraceKind = JSTRACE_OBJECT;
    if (argc > 0) {
        v = JS_ARGV(cx, vp)[0];
        if (JSVAL_IS_TRACEABLE(v)) {
            startThing = JSVAL_TO_TRACEABLE(v);
            startTraceKind = JSVAL_TRACE_KIND(v);
        } else if (!JSVAL_IS_NULL(v)) {
            JS_ReportError(cx,
                           "the first argument is not null or a heap-allocated "
                           "thing");
            return JS_FALSE;
        }
    }

    traceKind = -1;
    if (argc > 1) {
        str = JS_ValueToString(cx, JS_ARGV(cx, vp)[1]);
        if (!str)
            return JS_FALSE;
        JSFlatString *flatStr = JS_FlattenString(cx, str);
        if (!flatStr)
            return JS_FALSE;
        for (size_t i = 0; ;) {
            if (JS_FlatStringEqualsAscii(flatStr, traceKindNames[i].name)) {
                traceKind = traceKindNames[i].kind;
                break;
            }
            if (++i == ArrayLength(traceKindNames)) {
                JSAutoByteString bytes(cx, str);
                if (!!bytes)
                    JS_ReportError(cx, "trace kind name '%s' is unknown", bytes.ptr());
                return JS_FALSE;
            }
        }
    }

    JS_TracerInit(&countTracer.base, JS_GetRuntime(cx), CountHeapNotify);
    if (!countTracer.visited.init()) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    countTracer.ok = true;
    countTracer.traceList = NULL;
    countTracer.recycleList = NULL;

    if (!startThing) {
        JS_TraceRuntime(&countTracer.base);
    } else {
        JS_SET_TRACING_NAME(&countTracer.base, "root");
        JS_CallTracer(&countTracer.base, startThing, startTraceKind);
    }

    counter = 0;
    while ((node = countTracer.traceList) != NULL) {
        if (traceKind == -1 || node->kind == traceKind)
            counter++;
        countTracer.traceList = node->next;
        node->next = countTracer.recycleList;
        countTracer.recycleList = node;
        JS_TraceChildren(&countTracer.base, node->thing, node->kind);
    }
    while ((node = countTracer.recycleList) != NULL) {
        countTracer.recycleList = node->next;
        js_free(node);
    }
    if (!countTracer.ok) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    *vp = JS_NumberValue((double) counter);
    return true;
}

static unsigned finalizeCount = 0;

static void
finalize_counter_finalize(JSFreeOp *fop, JSObject *obj)
{
    ++finalizeCount;
}

static JSClass FinalizeCounterClass = {
    "FinalizeCounter", JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,       /* addProperty */
    JS_PropertyStub,       /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    finalize_counter_finalize
};

static JSBool
MakeFinalizeObserver(JSContext *cx, unsigned argc, jsval *vp)
{
    RootedObject scope(cx, JS_GetGlobalForScopeChain(cx));
    if (!scope)
        return false;

    JSObject *obj = JS_NewObjectWithGivenProto(cx, &FinalizeCounterClass, NULL, scope);
    if (!obj)
        return false;

    *vp = OBJECT_TO_JSVAL(obj);
    return true;
}

static JSBool
FinalizeCount(JSContext *cx, unsigned argc, jsval *vp)
{
    *vp = INT_TO_JSVAL(finalizeCount);
    return true;
}

static JSBool
DumpHeapComplete(JSContext *cx, unsigned argc, jsval *vp)
{
    const char *fileName = NULL;
    JSAutoByteString fileNameBytes;
    if (argc > 0) {
        Value v = JS_ARGV(cx, vp)[0];
        if (v.isString()) {
            JSString *str = v.toString();
            if (!fileNameBytes.encodeLatin1(cx, str))
                return false;
            fileName = fileNameBytes.ptr();
        }
    }

    FILE *dumpFile;
    if (!fileName) {
        dumpFile = stdout;
    } else {
        dumpFile = fopen(fileName, "w");
        if (!dumpFile) {
            JS_ReportError(cx, "can't open %s", fileName);
            return false;
        }
    }

    js::DumpHeapComplete(JS_GetRuntime(cx), dumpFile);

    fclose(dumpFile);

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

JSBool
MJitChunkLimit(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc != 1) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return JS_FALSE;
    }

    if (cx->runtime->alwaysPreserveCode) {
        JS_ReportError(cx, "Can't change chunk limit after gcPreserveCode()");
        return JS_FALSE;
    }

    for (CompartmentsIter c(cx->runtime); !c.done(); c.next()) {
        if (c->lastAnimationTime != 0) {
            JS_ReportError(cx, "Can't change chunk limit if code may be preserved");
            return JS_FALSE;
        }
    }

    double t;
    if (!JS_ValueToNumber(cx, args[0], &t))
        return JS_FALSE;

#ifdef JS_METHODJIT
    mjit::SetChunkLimit((uint32_t) t);
#endif

    // Clear out analysis information which might refer to code compiled with
    // the previous chunk limit.
    JS_GC(cx->runtime);

    vp->setUndefined();
    return true;
}

static JSBool
Terminate(JSContext *cx, unsigned arg, jsval *vp)
{
    JS_ClearPendingException(cx);
    return JS_FALSE;
}

static JSBool
EnableSPSProfilingAssertions(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (argc == 0 || !args[0].isBoolean()) {
        RootedObject arg(cx, &args.callee());
        ReportUsageError(cx, arg, "Must have one boolean argument");
        return false;
    }

    static ProfileEntry stack[1000];
    static uint32_t stack_size = 0;

    SetRuntimeProfilingStack(cx->runtime, stack, &stack_size, 1000);
    cx->runtime->spsProfiler.enableSlowAssertions(args[0].toBoolean());
    cx->runtime->spsProfiler.enable(true);

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

static JSBool
DisplayName(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (argc == 0 || !args[0].isObject() || !args[0].toObject().isFunction()) {
        RootedObject arg(cx, &args.callee());
        ReportUsageError(cx, arg, "Must have one function argument");
        return false;
    }

    JSFunction *fun = args[0].toObject().toFunction();
    JSString *str = fun->displayAtom();
    vp->setString(str == NULL ? cx->runtime->emptyString : str);
    return true;
}

JSBool
js::testingFunc_inParallelSection(JSContext *cx, unsigned argc, jsval *vp)
{
    // If we were actually *in* a parallel section, then this function
    // would be inlined to TRUE in ion-generated code.
    JS_ASSERT(!InParallelSection());
    JS_SET_RVAL(cx, vp, JSVAL_FALSE);
    return true;
}

static JSFunctionSpecWithHelp TestingFunctions[] = {
    JS_FN_HELP("gc", ::GC, 0, 0,
"gc([obj] | 'compartment')",
"  Run the garbage collector. When obj is given, GC only its compartment.\n"
"  If 'compartment' is given, GC any compartments that were scheduled for\n"
"  GC via schedulegc."),

    JS_FN_HELP("gcparam", GCParameter, 2, 0,
"gcparam(name [, value])",
"  Wrapper for JS_[GS]etGCParameter. The name is either maxBytes,\n"
"  maxMallocBytes, gcBytes, gcNumber, or sliceTimeBudget."),

    JS_FN_HELP("getBuildConfiguration", GetBuildConfiguration, 0, 0,
"getBuildConfiguration()",
"  Return an object describing some of the configuration options SpiderMonkey\n"
"  was built with."),

    JS_FN_HELP("countHeap", CountHeap, 0, 0,
"countHeap([start[, kind]])",
"  Count the number of live GC things in the heap or things reachable from\n"
"  start when it is given and is not null. kind is either 'all' (default) to\n"
"  count all things or one of 'object', 'double', 'string', 'function'\n"
"  to count only things of that kind."),

    JS_FN_HELP("makeFinalizeObserver", MakeFinalizeObserver, 0, 0,
"makeFinalizeObserver()",
"  Get a special object whose finalization increases the counter returned\n"
"  by the finalizeCount function."),

    JS_FN_HELP("finalizeCount", FinalizeCount, 0, 0,
"finalizeCount()",
"  Return the current value of the finalization counter that is incremented\n"
"  each time an object returned by the makeFinalizeObserver is finalized."),

    JS_FN_HELP("gcPreserveCode", GCPreserveCode, 0, 0,
"gcPreserveCode()",
"  Preserve JIT code during garbage collections."),

#ifdef JS_GC_ZEAL
    JS_FN_HELP("gczeal", GCZeal, 2, 0,
"gczeal(level, [period])",
"  Specifies how zealous the garbage collector should be. Values for level:\n"
"    0: Normal amount of collection\n"
"    1: Collect when roots are added or removed\n"
"    2: Collect when memory is allocated\n"
"    3: Collect when the window paints (browser only)\n"
"    4: Verify pre write barriers between instructions\n"
"    5: Verify pre write barriers between paints\n"
"    6: Verify stack rooting\n"
"    7: Verify stack rooting (yes, it's the same as 6)\n"
"    8: Incremental GC in two slices: 1) mark roots 2) finish collection\n"
"    9: Incremental GC in two slices: 1) mark all 2) new marking and finish\n"
"   10: Incremental GC in multiple slices\n"
"   11: Verify post write barriers between instructions\n"
"   12: Verify post write barriers between paints\n"
"   13: Purge analysis state when memory is allocated\n"
"  Period specifies that collection happens every n allocations.\n"),

    JS_FN_HELP("schedulegc", ScheduleGC, 1, 0,
"schedulegc(num | obj)",
"  If num is given, schedule a GC after num allocations.\n"
"  If obj is given, schedule a GC of obj's compartment."),

    JS_FN_HELP("selectforgc", SelectForGC, 0, 0,
"selectforgc(obj1, obj2, ...)",
"  Schedule the given objects to be marked in the next GC slice."),

    JS_FN_HELP("verifyprebarriers", VerifyPreBarriers, 0, 0,
"verifyprebarriers()",
"  Start or end a run of the pre-write barrier verifier."),

    JS_FN_HELP("verifypostbarriers", VerifyPostBarriers, 0, 0,
"verifypostbarriers()",
"  Start or end a run of the post-write barrier verifier."),

    JS_FN_HELP("gcstate", GCState, 0, 0,
"gcstate()",
"  Report the global GC state."),

    JS_FN_HELP("deterministicgc", DeterministicGC, 1, 0,
"deterministicgc(true|false)",
"  If true, only allow determinstic GCs to run."),
#endif

    JS_FN_HELP("gcslice", GCSlice, 1, 0,
"gcslice(n)",
"  Run an incremental GC slice that marks about n objects."),

    JS_FN_HELP("validategc", ValidateGC, 1, 0,
"validategc(true|false)",
"  If true, a separate validation step is performed after an incremental GC."),

    JS_FN_HELP("fullcompartmentchecks", FullCompartmentChecks, 1, 0,
"fullcompartmentchecks(true|false)",
"  If true, check for compartment mismatches before every GC."),

    JS_FN_HELP("nondeterministicGetWeakMapKeys", NondeterminsticGetWeakMapKeys, 1, 0,
"nondeterministicGetWeakMapKeys(weakmap)",
"  Return an array of the keys in the given WeakMap."),

    JS_FN_HELP("internalConst", InternalConst, 1, 0,
"internalConst(name)",
"  Query an internal constant for the engine. See InternalConst source for\n"
"  the list of constant names."),

    JS_FN_HELP("isProxy", IsProxy, 1, 0,
"isProxy(obj)",
"  If true, obj is a proxy of some sort"),

    JS_FN_HELP("dumpHeapComplete", DumpHeapComplete, 1, 0,
"dumpHeapComplete([filename])",
"  Dump reachable and unreachable objects to a file."),

    JS_FN_HELP("mjitChunkLimit", MJitChunkLimit, 1, 0,
"mjitChunkLimit(N)",
"  Specify limit on compiled chunk size during mjit compilation."),

    JS_FN_HELP("terminate", Terminate, 0, 0,
"terminate()",
"  Terminate JavaScript execution, as if we had run out of\n"
"  memory or been terminated by the slow script dialog."),

    JS_FN_HELP("enableSPSProfilingAssertions", EnableSPSProfilingAssertions, 1, 0,
"enableSPSProfilingAssertions(slow)",
"  Enables SPS instrumentation and corresponding assertions. If 'slow' is\n"
"  true, then even slower assertions are enabled for all generated JIT code.\n"
"  When 'slow' is false, then instrumentation is enabled, but the slow\n"
"  assertions are disabled."),

    JS_FN_HELP("displayName", DisplayName, 1, 0,
"displayName(fn)",
"  Gets the display name for a function, which can possibly be a guessed or\n"
"  inferred name based on where the function was defined. This can be\n"
"  different from the 'name' property on the function."),

    JS_FN_HELP("inParallelSection", testingFunc_inParallelSection, 0, 0,
"inParallelSection()",
"  True if this code is executing within a parallel section."),

    JS_FS_HELP_END
};

bool
js::DefineTestingFunctions(JSContext *cx, HandleObject obj)
{
    return JS_DefineFunctionsWithHelp(cx, obj, TestingFunctions);
}

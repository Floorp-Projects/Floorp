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

#include "methodjit/MethodJIT.h"

#include "vm/Stack-inl.h"

using namespace js;
using namespace JS;

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

#ifdef JSGC_ROOT_ANALYSIS
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "rooting-analysis", &value))
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
    if (!JS_SetProperty(cx, info, "has-gczeal", &value))
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

#ifdef JS_HAS_XML_SUPPORT
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "e4x", &value))
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
     * object, we collect the object's compartment (any any other compartments
     * scheduled for GC). Otherwise, we collect call compartments.
     */
    JSBool compartment = false;
    if (argc == 1) {
        Value arg = vp[2];
        if (arg.isString()) {
            if (!JS_StringEqualsAscii(cx, arg.toString(), "compartment", &compartment))
                return false;
        } else if (arg.isObject()) {
            PrepareCompartmentForGC(UnwrapObject(&arg.toObject())->compartment());
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
        return JS_NewNumberValue(cx, value, &vp[0]);
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

#ifdef JS_GC_ZEAL
static JSBool
GCZeal(JSContext *cx, unsigned argc, jsval *vp)
{
    uint32_t zeal, frequency = JS_DEFAULT_ZEAL_FREQ;

    if (argc > 2) {
        ReportUsageError(cx, &JS_CALLEE(cx, vp).toObject(), "Too many arguments");
        return JS_FALSE;
    }
    if (!JS_ValueToECMAUint32(cx, argc < 1 ? JSVAL_VOID : vp[2], &zeal))
        return JS_FALSE;
    if (argc >= 2)
        if (!JS_ValueToECMAUint32(cx, vp[3], &frequency))
            return JS_FALSE;

    JS_SetGCZeal(cx, (uint8_t)zeal, frequency);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
ScheduleGC(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc != 1) {
        ReportUsageError(cx, &JS_CALLEE(cx, vp).toObject(), "Wrong number of arguments");
        return JS_FALSE;
    }

    Value arg(vp[2]);
    if (arg.isInt32()) {
        /* Schedule a GC to happen after |arg| allocations. */
        JS_ScheduleGC(cx, arg.toInt32());
    } else if (arg.isObject()) {
        /* Ensure that |comp| is collected during the next GC. */
        JSCompartment *comp = UnwrapObject(&arg.toObject())->compartment();
        PrepareCompartmentForGC(comp);
    } else if (arg.isString()) {
        /* This allows us to schedule atomsCompartment for GC. */
        PrepareCompartmentForGC(arg.toString()->compartment());
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
    if (argc) {
        ReportUsageError(cx, &JS_CALLEE(cx, vp).toObject(), "Too many arguments");
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
        ReportUsageError(cx, &JS_CALLEE(cx, vp).toObject(), "Too many arguments");
        return JS_FALSE;
    }
    gc::VerifyBarriers(cx->runtime, gc::PostBarrierVerifier);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
GCSlice(JSContext *cx, unsigned argc, jsval *vp)
{
    bool limit = true;
    uint32_t budget = 0;

    if (argc > 1) {
        ReportUsageError(cx, &JS_CALLEE(cx, vp).toObject(), "Wrong number of arguments");
        return JS_FALSE;
    }

    if (argc == 1) {
        if (!JS_ValueToECMAUint32(cx, vp[2], &budget))
            return false;
    } else {
        limit = false;
    }

    GCDebugSlice(cx->runtime, limit, budget);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
GCPreserveCode(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc != 0) {
        ReportUsageError(cx, &JS_CALLEE(cx, vp).toObject(), "Wrong number of arguments");
        return JS_FALSE;
    }

    cx->runtime->alwaysPreserveCode = true;

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
DeterministicGC(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc != 1) {
        ReportUsageError(cx, &JS_CALLEE(cx, vp).toObject(), "Wrong number of arguments");
        return JS_FALSE;
    }

    gc::SetDeterministicGC(cx, ToBoolean(vp[2]));
    *vp = JSVAL_VOID;
    return JS_TRUE;
}
#endif /* JS_GC_ZEAL */

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
        node = (JSCountHeapNode *) js_malloc(sizeof *node);
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
#if JS_HAS_XML_SUPPORT
    { "xml",        JSTRACE_XML         },
#endif
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

    return JS_NewNumberValue(cx, (double) counter, vp);
}

static unsigned finalizeCount = 0;

static void
finalize_counter_finalize(JSFreeOp *fop, JSObject *obj)
{
    JS_ATOMIC_INCREMENT(&finalizeCount);
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
    JSObject *scope = JS_GetGlobalForScopeChain(cx);
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

JSBool
MJitChunkLimit(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc != 1) {
        ReportUsageError(cx, &JS_CALLEE(cx, vp).toObject(), "Wrong number of arguments");
        return JS_FALSE;
    }

    if (cx->runtime->alwaysPreserveCode) {
        JS_ReportError(cx, "Can't change chunk limit after gcPreserveCode()");
        return JS_FALSE;
    }

    double t;
    if (!JS_ValueToNumber(cx, JS_ARGV(cx, vp)[0], &t))
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
    jsval arg = JS_ARGV(cx, vp)[0];
    if (argc == 0 || !JSVAL_IS_BOOLEAN(arg)) {
        ReportUsageError(cx, &JS_CALLEE(cx, vp).toObject(),
                         "Must have one boolean argument");
        return false;
    }

    static ProfileEntry stack[1000];
    static uint32_t stack_size = 0;

    if (JSVAL_TO_BOOLEAN(arg))
        SetRuntimeProfilingStack(cx->runtime, stack, &stack_size, 1000);
    else
        SetRuntimeProfilingStack(cx->runtime, NULL, NULL, 0);
    cx->runtime->spsProfiler.enableSlowAssertions(JSVAL_TO_BOOLEAN(arg));

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
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
"  count all things or one of 'object', 'double', 'string', 'function',\n"
"  'qname', 'namespace', 'xml' to count only things of that kind."),

    JS_FN_HELP("makeFinalizeObserver", MakeFinalizeObserver, 0, 0,
"makeFinalizeObserver()",
"  Get a special object whose finalization increases the counter returned\n"
"  by the finalizeCount function."),

    JS_FN_HELP("finalizeCount", FinalizeCount, 0, 0,
"finalizeCount()",
"  Return the current value of the finalization counter that is incremented\n"
"  each time an object returned by the makeFinalizeObserver is finalized."),

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
"    6: Verify stack rooting (ignoring XML and Reflect)\n"
"    7: Verify stack rooting (all roots)\n"
"    8: Incremental GC in two slices: 1) mark roots 2) finish collection\n"
"    9: Incremental GC in two slices: 1) mark all 2) new marking and finish\n"
"   10: Incremental GC in multiple slices\n"
"   11: Verify post write barriers between instructions\n"
"   12: Verify post write barriers between paints\n"
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

    JS_FN_HELP("gcslice", GCSlice, 1, 0,
"gcslice(n)",
"  Run an incremental GC slice that marks about n objects."),

    JS_FN_HELP("gcPreserveCode", GCPreserveCode, 0, 0,
"gcPreserveCode()",
"  Preserve JIT code during garbage collections."),

    JS_FN_HELP("deterministicgc", DeterministicGC, 1, 0,
"deterministicgc(true|false)",
"  If true, only allow determinstic GCs to run."),
#endif

    JS_FN_HELP("internalConst", InternalConst, 1, 0,
"internalConst(name)",
"  Query an internal constant for the engine. See InternalConst source for\n"
"  the list of constant names."),

    JS_FN_HELP("isProxy", IsProxy, 1, 0,
"isProxy(obj)",
"  If true, obj is a proxy of some sort"),

    JS_FN_HELP("mjitChunkLimit", MJitChunkLimit, 1, 0,
"mjitChunkLimit(N)",
"  Specify limit on compiled chunk size during mjit compilation."),

    JS_FN_HELP("terminate", Terminate, 0, 0,
"terminate()",
"  Terminate JavaScript execution, as if we had run out of\n"
"  memory or been terminated by the slow script dialog."),

    JS_FN_HELP("enableSPSProfilingAssertions", EnableSPSProfilingAssertions, 1, 0,
"enableSPSProfilingAssertions(enabled)",
"  Enables or disables the assertions related to SPS profiling. This is fairly\n"
"  expensive, so it shouldn't be enabled normally."),

    JS_FS_END
};

namespace js {

bool
DefineTestingFunctions(JSContext *cx, JSObject *obj)
{
    return JS_DefineFunctionsWithHelp(cx, obj, TestingFunctions);
}

} /* namespace js */

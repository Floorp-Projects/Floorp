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
#include "jsprf.h"
#include "jswrapper.h"

#include "methodjit/MethodJIT.h"

using namespace js;
using namespace JS;

static JSBool
GC(JSContext *cx, unsigned argc, jsval *vp)
{
    JSCompartment *comp = NULL;
    if (argc == 1) {
        Value arg = vp[2];
        if (arg.isObject())
            comp = UnwrapObject(&arg.toObject())->compartment();
    }

#ifndef JS_MORE_DETERMINISTIC
    size_t preBytes = cx->runtime->gcBytes;
#endif

    JS_CompartmentGC(cx, comp);

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
    JSBool compartment = JS_FALSE;

    if (argc > 3) {
        ReportUsageError(cx, &JS_CALLEE(cx, vp).toObject(), "Too many arguments");
        return JS_FALSE;
    }
    if (!JS_ValueToECMAUint32(cx, argc < 1 ? JSVAL_VOID : vp[2], &zeal))
        return JS_FALSE;
    if (argc >= 2)
        if (!JS_ValueToECMAUint32(cx, vp[3], &frequency))
            return JS_FALSE;
    if (argc >= 3)
        compartment = js_ValueToBoolean(vp[3]);

    JS_SetGCZeal(cx, (uint8_t)zeal, frequency, compartment);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
ScheduleGC(JSContext *cx, unsigned argc, jsval *vp)
{
    uint32_t count;
    bool compartment = false;

    if (argc != 1 && argc != 2) {
        ReportUsageError(cx, &JS_CALLEE(cx, vp).toObject(), "Wrong number of arguments");
        return JS_FALSE;
    }
    if (!JS_ValueToECMAUint32(cx, vp[2], &count))
        return JS_FALSE;
    if (argc == 2)
        compartment = js_ValueToBoolean(vp[3]);

    JS_ScheduleGC(cx, count, compartment);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
VerifyBarriers(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc) {
        ReportUsageError(cx, &JS_CALLEE(cx, vp).toObject(), "Too many arguments");
        return JS_FALSE;
    }
    gc::VerifyBarriers(cx);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
GCSlice(JSContext *cx, unsigned argc, jsval *vp)
{
    uint32_t budget;

    if (argc != 1) {
        ReportUsageError(cx, &JS_CALLEE(cx, vp).toObject(), "Wrong number of arguments");
        return JS_FALSE;
    }

    if (!JS_ValueToECMAUint32(cx, vp[2], &budget))
        return JS_FALSE;

    GCDebugSlice(cx, budget);
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

    gc::SetDeterministicGC(cx, js_ValueToBoolean(vp[2]));
    *vp = JSVAL_VOID;
    return JS_TRUE;
}
#endif /* JS_GC_ZEAL */

typedef struct JSCountHeapNode JSCountHeapNode;

struct JSCountHeapNode {
    void                *thing;
    JSGCTraceKind       kind;
    JSCountHeapNode     *next;
};

typedef struct JSCountHeapTracer {
    JSTracer            base;
    JSDHashTable        visited;
    bool                ok;
    JSCountHeapNode     *traceList;
    JSCountHeapNode     *recycleList;
} JSCountHeapTracer;

static void
CountHeapNotify(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    JSCountHeapTracer *countTracer;
    JSDHashEntryStub *entry;
    JSCountHeapNode *node;
    void *thing = *thingp;

    JS_ASSERT(trc->callback == CountHeapNotify);
    countTracer = (JSCountHeapTracer *)trc;
    if (!countTracer->ok)
        return;

    entry = (JSDHashEntryStub *)
            JS_DHashTableOperate(&countTracer->visited, thing, JS_DHASH_ADD);
    if (!entry) {
        countTracer->ok = false;
        return;
    }
    if (entry->key)
        return;
    entry->key = thing;

    node = countTracer->recycleList;
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
    if (!JS_DHashTableInit(&countTracer.visited, JS_DHashGetStubOps(),
                           NULL, sizeof(JSDHashEntryStub),
                           JS_DHASH_DEFAULT_CAPACITY(100))) {
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
    JS_DHashTableFinish(&countTracer.visited);
    if (!countTracer.ok) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    return JS_NewNumberValue(cx, (double) counter, vp);
}

static unsigned finalizeCount = 0;

static void
finalize_counter_finalize(JSContext *cx, JSObject *obj)
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
    JSObject *obj = JS_NewObjectWithGivenProto(cx, &FinalizeCounterClass, NULL,
                                               JS_GetGlobalObject(cx));
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
MJitCodeStats(JSContext *cx, unsigned argc, jsval *vp)
{
#ifdef JS_METHODJIT
    JSRuntime *rt = cx->runtime;
    AutoLockGC lock(rt);
    size_t n = 0;
    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c) {
        n += (*c)->sizeOfMjitCode();
    }
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(n));
#else
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
#endif
    return true;
}

JSBool
MJitChunkLimit(JSContext *cx, unsigned argc, jsval *vp)
{
    if (argc != 1) {
        ReportUsageError(cx, &JS_CALLEE(cx, vp).toObject(), "Wrong number of arguments");
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
    JS_GC(cx);

    vp->setUndefined();
    return true;
}

static JSBool
Terminate(JSContext *cx, unsigned arg, jsval *vp)
{
    JS_ClearPendingException(cx);
    return JS_FALSE;
}

static JSFunctionSpecWithHelp TestingFunctions[] = {
    JS_FN_HELP("gc", ::GC, 0, 0,
"gc([obj])",
"  Run the garbage collector. When obj is given, GC only its compartment."),

    JS_FN_HELP("gcparam", GCParameter, 2, 0,
"gcparam(name [, value])",
"  Wrapper for JS_[GS]etGCParameter. The name is either maxBytes,\n"
"  maxMallocBytes, gcBytes, gcNumber, or sliceTimeBudget."),

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
"gczeal(level, [period], [compartmentGC?])",
"  Specifies how zealous the garbage collector should be. Values for level:\n"
"    0: Normal amount of collection\n"
"    1: Collect when roots are added or removed\n"
"    2: Collect when memory is allocated\n"
"    3: Collect when the window paints (browser only)\n"
"    4: Verify write barriers between instructions\n"
"    5: Verify write barriers between paints\n"
"  Period specifies that collection happens every n allocations.\n"
"  If compartmentGC is true, the collections will be compartmental."),

    JS_FN_HELP("schedulegc", ScheduleGC, 1, 0,
"schedulegc(num, [compartmentGC?])",
"  Schedule a GC to happen after num allocations."),

    JS_FN_HELP("verifybarriers", VerifyBarriers, 0, 0,
"verifybarriers()",
"  Start or end a run of the write barrier verifier."),

    JS_FN_HELP("gcslice", GCSlice, 1, 0,
"gcslice(n)",
"  Run an incremental GC slice that marks about n objects."),

    JS_FN_HELP("deterministicgc", DeterministicGC, 1, 0,
"deterministicgc(true|false)",
"  If true, only allow determinstic GCs to run."),
#endif

    JS_FN_HELP("internalConst", InternalConst, 1, 0,
"internalConst(name)",
"  Query an internal constant for the engine. See InternalConst source for\n"
"  the list of constant names."),

#ifdef JS_METHODJIT
    JS_FN_HELP("mjitcodestats", MJitCodeStats, 0, 0,
"mjitcodestats()",
"Return stats on mjit code memory usage."),
#endif

    JS_FN_HELP("mjitChunkLimit", MJitChunkLimit, 1, 0,
"mjitChunkLimit(N)",
"  Specify limit on compiled chunk size during mjit compilation."),

    JS_FN_HELP("terminate", Terminate, 0, 0,
"terminate()",
"  Terminate JavaScript execution, as if we had run out of\n"
"  memory or been terminated by the slow script dialog."),

    JS_FS_END
};

namespace js {

bool
DefineTestingFunctions(JSContext *cx, JSObject *obj)
{
    return JS_DefineFunctionsWithHelp(cx, obj, TestingFunctions);
}

} /* namespace js */

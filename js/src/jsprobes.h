/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSPROBES_H
#define _JSPROBES_H

#ifdef INCLUDE_MOZILLA_DTRACE
#include "javascript-trace.h"
#endif
#include "jspubtd.h"
#include "jsprvtd.h"
#include "jsscript.h"
#include "jsobj.h"

#ifdef JS_METHODJIT
#include "methodjit/MethodJIT.h"
#endif

#include "vm/ObjectImpl-inl.h"

namespace js {

namespace mjit {
struct NativeAddressInfo;
struct JSActiveFrame;
}

namespace Probes {

/*
 * Static probes
 *
 * The probe points defined in this file are scattered around the SpiderMonkey
 * source tree. The presence of Probes::someEvent() means that someEvent is
 * about to happen or has happened. To the extent possible, probes should be
 * inserted in all paths associated with a given event, regardless of the
 * active runmode (interpreter/traceJIT/methodJIT/ionJIT).
 *
 * When a probe fires, it is handled by any probe handling backends that have
 * been compiled in. By default, most probes do nothing or at least do nothing
 * expensive, so the presence of the probe should have negligible effect on
 * running time. (Probes in slow paths may do something by default, as long as
 * there is no noticeable slowdown.)
 *
 * For some probes, the mere existence of the probe is too expensive even if it
 * does nothing when called. For example, just having consistent information
 * available for a function call entry/exit probe causes the JITs to
 * de-optimize function calls. In those cases, the JITs may query at compile
 * time whether a probe is desired, and omit the probe invocation if not. If a
 * probe is runtime-disabled at compilation time, it is not guaranteed to fire
 * within a compiled function if it is later enabled.
 *
 * Not all backends handle all of the probes listed here.
 */

/*
 * Internal use only: remember whether "profiling", whatever that means, is
 * currently active. Used for state management.
 */
extern bool ProfilingActive;

extern const char nullName[];
extern const char anonymousName[];

/* Called when first runtime is created for this process */
JSBool startEngine();

/* JSRuntime created, with currently valid fields */
bool createRuntime(JSRuntime *rt);

/* JSRuntime about to be destroyed */
bool destroyRuntime(JSRuntime *rt);

/* Total JS engine shutdown */
bool shutdown();

/*
 * Test whether we are tracking JS function call enter/exit. The JITs use this
 * to decide whether they can optimize in a way that would prevent probes from
 * firing.
 */
bool callTrackingActive(JSContext *);

/*
 * Test whether anything is looking for JIT native code registration events.
 * This information will not be collected otherwise.
 */
bool wantNativeAddressInfo(JSContext *);

/* Entering a JS function */
bool enterScript(JSContext *, RawScript, RawFunction , StackFrame *);

/* About to leave a JS function */
bool exitScript(JSContext *, RawScript, RawFunction , StackFrame *);

/* Executing a script */
bool startExecution(RawScript script);

/* Script has completed execution */
bool stopExecution(RawScript script);

/* Heap has been resized */
bool resizeHeap(JS::Zone *zone, size_t oldSize, size_t newSize);

/*
 * Object has been created. |obj| must exist (its class and size are read)
 */
bool createObject(JSContext *cx, JSObject *obj);

/* Resize events are being tracked. */
bool objectResizeActive();

/* Object has been resized */
bool resizeObject(JSContext *cx, JSObject *obj, size_t oldSize, size_t newSize);

/*
 * Object is about to be finalized. |obj| must still exist (its class is
 * read)
 */
bool finalizeObject(JSObject *obj);

/*
 * String has been created.
 *
 * |string|'s content is not (yet) valid. |length| is the length of the string
 * and does not imply anything about the amount of storage consumed to store
 * the string. (It may be a short string, an external string, or a rope, and
 * the encoding is not taken into consideration.)
 */
bool createString(JSContext *cx, JSString *string, size_t length);

/*
 * String is about to be finalized
 *
 * |string| must still have a valid length.
 */
bool finalizeString(JSString *string);

/* Script is about to be compiled */
bool compileScriptBegin(const char *filename, int lineno);

/* Script has just finished compilation */
bool compileScriptEnd(const char *filename, int lineno);

/* About to make a call from JS into native code */
bool calloutBegin(JSContext *cx, RawFunction fun);

/* Native code called by JS has terminated */
bool calloutEnd(JSContext *cx, RawFunction fun);

/* Unimplemented */
bool acquireMemory(JSContext *cx, void *address, size_t nbytes);
bool releaseMemory(JSContext *cx, void *address, size_t nbytes);

/*
 * Garbage collection probes
 *
 * GC timing is tricky and at the time of this writing is changing frequently.
 * GCStart/GCEnd are intended to bracket the entire garbage collection (either
 * global or single-compartment), but a separate thread may continue doing work
 * after GCEnd.
 *
 * Multiple compartments' GC will be interleaved during a global collection
 * (eg, compartment 1 starts, compartment 2 starts, compartment 1 ends, ...)
 */
bool GCStart();
bool GCEnd();

bool GCStartMarkPhase();
bool GCEndMarkPhase();

bool GCStartSweepPhase();
bool GCEndSweepPhase();

/*
 * Various APIs for inserting custom probe points. These might be used to mark
 * when something starts and stops, or for various other purposes the user has
 * in mind. These are useful to export to JS so that JS code can mark
 * application-meaningful events and phases of execution.
 *
 * Not all backends support these.
 */
bool CustomMark(JSString *string);
bool CustomMark(const char *string);
bool CustomMark(int marker);

/* JIT code observation */

enum JITReportGranularity {
    JITREPORT_GRANULARITY_NONE = 0,
    JITREPORT_GRANULARITY_FUNCTION = 1,
    JITREPORT_GRANULARITY_LINE = 2,
    JITREPORT_GRANULARITY_OP = 3
};

/*
 * Finest granularity of JIT information desired by all watchers.
 */
JITReportGranularity
JITGranularityRequested(JSContext *cx);

#ifdef JS_METHODJIT
/*
 * New method JIT code has been created
 */
bool
registerMJITCode(JSContext *cx, js::mjit::JITChunk *chunk,
                 mjit::JSActiveFrame *outerFrame,
                 mjit::JSActiveFrame **inlineFrames);

/*
 * Method JIT code is about to be discarded
 */
void
discardMJITCode(FreeOp *fop, mjit::JITScript *jscr, mjit::JITChunk *chunk, void* address);

/*
 * IC code has been allocated within the given JITChunk
 */
bool
registerICCode(JSContext *cx,
               mjit::JITChunk *chunk, RawScript script, jsbytecode* pc,
               void *start, size_t size);
#endif /* JS_METHODJIT */

/*
 * A whole region of code has been deallocated, containing any number of ICs.
 * (ICs are unregistered in a batch, so individual ICs are not registered.)
 */
void
discardExecutableRegion(void *start, size_t size);

/*
 * Internal: DTrace-specific functions to be called during Probes::enterScript
 * and Probes::exitScript. These will not be inlined, but the argument
 * marshalling required for these probe points is expensive enough that it
 * shouldn't really matter.
 */
void DTraceEnterJSFun(JSContext *cx, RawFunction fun, RawScript script);
void DTraceExitJSFun(JSContext *cx, RawFunction fun, RawScript script);

/*
 * Internal: ETW-specific probe functions
 */
#ifdef MOZ_ETW
// ETW Handlers
bool ETWCreateRuntime(JSRuntime *rt);
bool ETWDestroyRuntime(JSRuntime *rt);
bool ETWShutdown();
bool ETWCallTrackingActive();
bool ETWEnterJSFun(JSContext *cx, RawFunction fun, RawScript script, int counter);
bool ETWExitJSFun(JSContext *cx, RawFunction fun, RawScript script, int counter);
bool ETWCreateObject(JSContext *cx, JSObject *obj);
bool ETWFinalizeObject(JSObject *obj);
bool ETWResizeObject(JSContext *cx, JSObject *obj, size_t oldSize, size_t newSize);
bool ETWCreateString(JSContext *cx, JSString *string, size_t length);
bool ETWFinalizeString(JSString *string);
bool ETWCompileScriptBegin(const char *filename, int lineno);
bool ETWCompileScriptEnd(const char *filename, int lineno);
bool ETWCalloutBegin(JSContext *cx, RawFunction fun);
bool ETWCalloutEnd(JSContext *cx, RawFunction fun);
bool ETWAcquireMemory(JSContext *cx, void *address, size_t nbytes);
bool ETWReleaseMemory(JSContext *cx, void *address, size_t nbytes);
bool ETWGCStart();
bool ETWGCEnd();
bool ETWGCStartMarkPhase();
bool ETWGCEndMarkPhase();
bool ETWGCStartSweepPhase();
bool ETWGCEndSweepPhase();
bool ETWCustomMark(JSString *string);
bool ETWCustomMark(const char *string);
bool ETWCustomMark(int marker);
bool ETWStartExecution(RawScript script);
bool ETWStopExecution(JSContext *cx, RawScript script);
bool ETWResizeHeap(JSCompartment *compartment, size_t oldSize, size_t newSize);
#endif

} /* namespace Probes */

/*
 * Many probe handlers are implemented inline for minimal performance impact,
 * especially important when no backends are enabled.
 */

inline bool
Probes::callTrackingActive(JSContext *cx)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_FUNCTION_ENTRY_ENABLED() || JAVASCRIPT_FUNCTION_RETURN_ENABLED())
        return true;
#endif
#ifdef MOZ_TRACE_JSCALLS
    if (cx->functionCallback)
        return true;
#endif
#ifdef MOZ_ETW
    if (ProfilingActive && ETWCallTrackingActive())
        return true;
#endif
    return false;
}

inline bool
Probes::wantNativeAddressInfo(JSContext *cx)
{
    return (cx->reportGranularity >= JITREPORT_GRANULARITY_FUNCTION &&
            JITGranularityRequested(cx) >= JITREPORT_GRANULARITY_FUNCTION);
}

inline bool
Probes::enterScript(JSContext *cx, RawScript script, RawFunction maybeFun,
                    StackFrame *fp)
{
    bool ok = true;
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_FUNCTION_ENTRY_ENABLED())
        DTraceEnterJSFun(cx, maybeFun, script);
#endif
#ifdef MOZ_TRACE_JSCALLS
    cx->doFunctionCallback(maybeFun, script, 1);
#endif
#ifdef MOZ_ETW
    if (ProfilingActive && !ETWEnterJSFun(cx, maybeFun, script, 1))
        ok = false;
#endif

    JSRuntime *rt = cx->runtime;
    if (rt->spsProfiler.enabled()) {
        rt->spsProfiler.enter(cx, script, maybeFun);
        JS_ASSERT_IF(!fp->isGeneratorFrame(), !fp->hasPushedSPSFrame());
        fp->setPushedSPSFrame();
    }

    return ok;
}

inline bool
Probes::exitScript(JSContext *cx, RawScript script, RawFunction maybeFun,
                   StackFrame *fp)
{
    bool ok = true;

#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_FUNCTION_RETURN_ENABLED())
        DTraceExitJSFun(cx, maybeFun, script);
#endif
#ifdef MOZ_TRACE_JSCALLS
    cx->doFunctionCallback(maybeFun, script, 0);
#endif
#ifdef MOZ_ETW
    if (ProfilingActive && !ETWExitJSFun(cx, maybeFun, script, 0))
        ok = false;
#endif

    JSRuntime *rt = cx->runtime;
    /*
     * Coming from IonMonkey, the fp might not be known (fp == NULL), but
     * IonMonkey will only call exitScript() when absolutely necessary, so it is
     * guaranteed that fp->hasPushedSPSFrame() would have been true
     */
    if ((fp == NULL && rt->spsProfiler.enabled()) ||
        (fp != NULL && fp->hasPushedSPSFrame()))
    {
        rt->spsProfiler.exit(cx, script, maybeFun);
    }
    return ok;
}

inline bool
Probes::resizeHeap(JS::Zone *zone, size_t oldSize, size_t newSize)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWResizeHeap(zone, oldSize, newSize))
        ok = false;
#endif

    return ok;
}

#ifdef INCLUDE_MOZILLA_DTRACE
static const char *ObjectClassname(JSObject *obj) {
    if (!obj)
        return "(null object)";
    Class *clasp = obj->getClass();
    if (!clasp)
        return "(null)";
    const char *class_name = clasp->name;
    if (!class_name)
        return "(null class name)";
    return class_name;
}
#endif

inline bool
Probes::createObject(JSContext *cx, JSObject *obj)
{
    bool ok = true;

#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_OBJECT_CREATE_ENABLED())
        JAVASCRIPT_OBJECT_CREATE(ObjectClassname(obj), (uintptr_t)obj);
#endif
#ifdef MOZ_ETW
    if (ProfilingActive && !ETWCreateObject(cx, obj))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::finalizeObject(JSObject *obj)
{
    bool ok = true;

#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_OBJECT_FINALIZE_ENABLED()) {
        Class *clasp = obj->getClass();

        /* the first arg is NULL - reserved for future use (filename?) */
        JAVASCRIPT_OBJECT_FINALIZE(NULL, (char *)clasp->name, (uintptr_t)obj);
    }
#endif
#ifdef MOZ_ETW
    if (ProfilingActive && !ETWFinalizeObject(obj))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::objectResizeActive()
{
#ifdef MOZ_ETW
    if (ProfilingActive)
        return true;
#endif

    return false;
}

inline bool
Probes::resizeObject(JSContext *cx, JSObject *obj, size_t oldSize, size_t newSize)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWResizeObject(cx, obj, oldSize, newSize))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::createString(JSContext *cx, JSString *string, size_t length)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWCreateString(cx, string, length))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::finalizeString(JSString *string)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWFinalizeString(string))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::compileScriptBegin(const char *filename, int lineno)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWCompileScriptBegin(filename, lineno))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::compileScriptEnd(const char *filename, int lineno)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWCompileScriptEnd(filename, lineno))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::calloutBegin(JSContext *cx, RawFunction fun)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWCalloutBegin(cx, fun))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::calloutEnd(JSContext *cx, RawFunction fun)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWCalloutEnd(cx, fun))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::acquireMemory(JSContext *cx, void *address, size_t nbytes)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWAcquireMemory(cx, address, nbytes))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::releaseMemory(JSContext *cx, void *address, size_t nbytes)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWReleaseMemory(cx, address, nbytes))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::GCStart()
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWGCStart())
        ok = false;
#endif

    return ok;
}

inline bool
Probes::GCEnd()
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWGCEnd())
        ok = false;
#endif

    return ok;
}

inline bool
Probes::GCStartMarkPhase()
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWGCStartMarkPhase())
        ok = false;
#endif

    return ok;
}

inline bool
Probes::GCEndMarkPhase()
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWGCEndMarkPhase())
        ok = false;
#endif

    return ok;
}

inline bool
Probes::GCStartSweepPhase()
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWGCStartSweepPhase())
        ok = false;
#endif

    return ok;
}

inline bool
Probes::GCEndSweepPhase()
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWGCEndSweepPhase())
        ok = false;
#endif

    return ok;
}

inline bool
Probes::CustomMark(JSString *string)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWCustomMark(string))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::CustomMark(const char *string)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWCustomMark(string))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::CustomMark(int marker)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWCustomMark(marker))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::startExecution(RawScript script)
{
    bool ok = true;

#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_EXECUTE_START_ENABLED())
        JAVASCRIPT_EXECUTE_START((script->filename() ? (char *)script->filename() : nullName),
                                 script->lineno);
#endif
#ifdef MOZ_ETW
    if (ProfilingActive && !ETWStartExecution(script))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::stopExecution(RawScript script)
{
    bool ok = true;

#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_EXECUTE_DONE_ENABLED())
        JAVASCRIPT_EXECUTE_DONE((script->filename() ? (char *)script->filename() : nullName),
                                script->lineno);
#endif
#ifdef MOZ_ETW
    if (ProfilingActive && !ETWStopExecution(script))
        ok = false;
#endif

    return ok;
}

} /* namespace js */

/*
 * Internal functions for controlling various profilers. The profiler-specific
 * implementations of these are mostly in jsdbgapi.cpp.
 */

#endif /* _JSPROBES_H */

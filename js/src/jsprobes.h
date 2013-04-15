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

/*
 * Object has been created. |obj| must exist (its class and size are read)
 */
bool createObject(JSContext *cx, JSObject *obj);

/*
 * Object is about to be finalized. |obj| must still exist (its class is
 * read)
 */
bool finalizeObject(JSObject *obj);

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

    return ok;
}

} /* namespace js */

/*
 * Internal functions for controlling various profilers. The profiler-specific
 * implementations of these are mostly in jsdbgapi.cpp.
 */

#endif /* _JSPROBES_H */

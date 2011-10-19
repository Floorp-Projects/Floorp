/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * June 12, 2009.
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
 *
 * Contributor(s):
 *      Steve Fink <sfink@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _JSPROBES_H
#define _JSPROBES_H

#ifdef INCLUDE_MOZILLA_DTRACE
#include "javascript-trace.h"
#endif
#include "jspubtd.h"
#include "jsprvtd.h"
#include "jsscript.h"
#include "jsobj.h"

namespace js {

namespace mjit {
struct NativeAddressInfo;
struct Compiler_ActiveFrame;
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
bool enterJSFun(JSContext *, JSFunction *, JSScript *, int counter = 1);

/* About to leave a JS function */
bool exitJSFun(JSContext *, JSFunction *, JSScript *, int counter = 0);

/* Executing a script */
bool startExecution(JSContext *cx, JSScript *script);

/* Script has completed execution */
bool stopExecution(JSContext *cx, JSScript *script);

/* Heap has been resized */
bool resizeHeap(JSCompartment *compartment, size_t oldSize, size_t newSize);

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
bool compileScriptBegin(JSContext *cx, const char *filename, int lineno);

/* Script has just finished compilation */
bool compileScriptEnd(JSContext *cx, JSScript *script, const char *filename, int lineno);

/* About to make a call from JS into native code */
bool calloutBegin(JSContext *cx, JSFunction *fun);

/* Native code called by JS has terminated */
bool calloutEnd(JSContext *cx, JSFunction *fun);

/* Unimplemented */
bool acquireMemory(JSContext *cx, void *address, size_t nbytes);
bool releaseMemory(JSContext *cx, void *address, size_t nbytes);

/*
 * Garbage collection probes
 *
 * GC timing is tricky and at the time of this writing is changing frequently.
 * GCStart(NULL)/GCEnd(NULL) are intended to bracket the entire garbage
 * collection (either global or single-compartment), but a separate thread may
 * continue doing work after GCEnd.
 *
 * Multiple compartments' GC will be interleaved during a global collection
 * (eg, compartment 1 starts, compartment 2 starts, compartment 1 ends, ...)
 */
bool GCStart(JSCompartment *compartment);
bool GCEnd(JSCompartment *compartment);

bool GCStartMarkPhase(JSCompartment *compartment);
bool GCEndMarkPhase(JSCompartment *compartment);

bool GCStartSweepPhase(JSCompartment *compartment);
bool GCEndSweepPhase(JSCompartment *compartment);

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
 * Observer class for JIT code allocation/deallocation. Currently, this only
 * handles the method JIT, and does not get notifications when JIT code is
 * changed (patched) with no new allocation.
 */
class JITWatcher {
public:
    virtual JITReportGranularity granularityRequested() = 0;

#ifdef JS_METHODJIT
    virtual void registerMJITCode(JSContext *cx, js::mjit::JITScript *jscr,
                                  JSScript *script, JSFunction *fun,
                                  mjit::Compiler_ActiveFrame** inlineFrames,
                                  void *mainCodeAddress, size_t mainCodeSize,
                                  void *stubCodeAddress, size_t stubCodeSize) = 0;

    virtual void discardMJITCode(JSContext *cx, mjit::JITScript *jscr, JSScript *script,
                                 void* address) = 0;

    virtual void registerICCode(JSContext *cx,
                                js::mjit::JITScript *jscr, JSScript *script, jsbytecode* pc,
                                void *start, size_t size) = 0;
#endif

    virtual void discardExecutableRegion(void *start, size_t size) = 0;
};

/*
 * Register a JITWatcher subclass to be informed of JIT code
 * allocation/deallocation.
 */
bool
addJITWatcher(JITWatcher *watcher);

/*
 * Remove (and destroy) a registered JITWatcher. rt may be NULL. Returns false
 * if the watcher is not found.
 */
bool
removeJITWatcher(JSRuntime *rt, JITWatcher *watcher);

/*
 * Remove (and destroy) all registered JITWatchers. rt may be NULL.
 */
void
removeAllJITWatchers(JSRuntime *rt);

/*
 * Finest granularity of JIT information desired by all watchers.
 */
JITReportGranularity
JITGranularityRequested();

#ifdef JS_METHODJIT
/*
 * New method JIT code has been created
 */
void
registerMJITCode(JSContext *cx, js::mjit::JITScript *jscr,
                 JSScript *script, JSFunction *fun,
                 mjit::Compiler_ActiveFrame** inlineFrames,
                 void *mainCodeAddress, size_t mainCodeSize,
                 void *stubCodeAddress, size_t stubCodeSize);

/*
 * Method JIT code is about to be discarded
 */
void
discardMJITCode(JSContext *cx, mjit::JITScript *jscr, JSScript *script, void* address);

/*
 * IC code has been allocated within the given JITScript
 */
void
registerICCode(JSContext *cx,
               mjit::JITScript *jscr, JSScript *script, jsbytecode* pc,
               void *start, size_t size);
#endif /* JS_METHODJIT */

/*
 * A whole region of code has been deallocated, containing any number of ICs.
 * (ICs are unregistered in a batch, so individual ICs are not registered.)
 */
void
discardExecutableRegion(void *start, size_t size);

/*
 * Internal: DTrace-specific functions to be called during Probes::enterJSFun
 * and Probes::exitJSFun. These will not be inlined, but the argument
 * marshalling required for these probe points is expensive enough that it
 * shouldn't really matter.
 */
void DTraceEnterJSFun(JSContext *cx, JSFunction *fun, JSScript *script);
void DTraceExitJSFun(JSContext *cx, JSFunction *fun, JSScript *script);

/*
 * Internal: ETW-specific probe functions
 */
#ifdef MOZ_ETW
// ETW Handlers
bool ETWCreateRuntime(JSRuntime *rt);
bool ETWDestroyRuntime(JSRuntime *rt);
bool ETWShutdown();
bool ETWCallTrackingActive(JSContext *cx);
bool ETWEnterJSFun(JSContext *cx, JSFunction *fun, JSScript *script, int counter);
bool ETWExitJSFun(JSContext *cx, JSFunction *fun, JSScript *script, int counter);
bool ETWCreateObject(JSContext *cx, JSObject *obj);
bool ETWFinalizeObject(JSObject *obj);
bool ETWResizeObject(JSContext *cx, JSObject *obj, size_t oldSize, size_t newSize);
bool ETWCreateString(JSContext *cx, JSString *string, size_t length);
bool ETWFinalizeString(JSString *string);
bool ETWCompileScriptBegin(const char *filename, int lineno);
bool ETWCompileScriptEnd(const char *filename, int lineno);
bool ETWCalloutBegin(JSContext *cx, JSFunction *fun);
bool ETWCalloutEnd(JSContext *cx, JSFunction *fun);
bool ETWAcquireMemory(JSContext *cx, void *address, size_t nbytes);
bool ETWReleaseMemory(JSContext *cx, void *address, size_t nbytes);
bool ETWGCStart(JSCompartment *compartment);
bool ETWGCEnd(JSCompartment *compartment);
bool ETWGCStartMarkPhase(JSCompartment *compartment);
bool ETWGCEndMarkPhase(JSCompartment *compartment);
bool ETWGCStartSweepPhase(JSCompartment *compartment);
bool ETWGCEndSweepPhase(JSCompartment *compartment);
bool ETWCustomMark(JSString *string);
bool ETWCustomMark(const char *string);
bool ETWCustomMark(int marker);
bool ETWStartExecution(JSContext *cx, JSScript *script);
bool ETWStopExecution(JSContext *cx, JSScript *script);
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
    if (ProfilingActive && ETWCallTrackingActive(cx))
        return true;
#endif
    return false;
}

inline bool
Probes::wantNativeAddressInfo(JSContext *cx)
{
    return (cx->reportGranularity >= JITREPORT_GRANULARITY_FUNCTION &&
            JITGranularityRequested() >= JITREPORT_GRANULARITY_FUNCTION);
}

inline bool
Probes::enterJSFun(JSContext *cx, JSFunction *fun, JSScript *script, int counter)
{
    bool ok = true;
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_FUNCTION_ENTRY_ENABLED())
        DTraceEnterJSFun(cx, fun, script);
#endif
#ifdef MOZ_TRACE_JSCALLS
    cx->doFunctionCallback(fun, script, counter);
#endif
#ifdef MOZ_ETW
    if (ProfilingActive && !ETWEnterJSFun(cx, fun, script, counter))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::exitJSFun(JSContext *cx, JSFunction *fun, JSScript *script, int counter)
{
    bool ok = true;

#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_FUNCTION_RETURN_ENABLED())
        DTraceExitJSFun(cx, fun, script);
#endif
#ifdef MOZ_TRACE_JSCALLS
    if (counter > 0)
        counter = -counter;
    cx->doFunctionCallback(fun, script, counter);
#endif
#ifdef MOZ_ETW
    if (ProfilingActive && !ETWExitJSFun(cx, fun, script, counter))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::resizeHeap(JSCompartment *compartment, size_t oldSize, size_t newSize)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWResizeHeap(compartment, oldSize, newSize))
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
Probes::compileScriptBegin(JSContext *cx, const char *filename, int lineno)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWCompileScriptBegin(filename, lineno))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::compileScriptEnd(JSContext *cx, JSScript *script, const char *filename, int lineno)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWCompileScriptEnd(filename, lineno))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::calloutBegin(JSContext *cx, JSFunction *fun)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWCalloutBegin(cx, fun))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::calloutEnd(JSContext *cx, JSFunction *fun)
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
Probes::GCStart(JSCompartment *compartment)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWGCStart(compartment))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::GCEnd(JSCompartment *compartment)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWGCEnd(compartment))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::GCStartMarkPhase(JSCompartment *compartment)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWGCStartMarkPhase(compartment))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::GCEndMarkPhase(JSCompartment *compartment)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWGCEndMarkPhase(compartment))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::GCStartSweepPhase(JSCompartment *compartment)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWGCStartSweepPhase(compartment))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::GCEndSweepPhase(JSCompartment *compartment)
{
    bool ok = true;

#ifdef MOZ_ETW
    if (ProfilingActive && !ETWGCEndSweepPhase(compartment))
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
Probes::startExecution(JSContext *cx, JSScript *script)
{
    bool ok = true;

#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_EXECUTE_START_ENABLED())
        JAVASCRIPT_EXECUTE_START((script->filename ? (char *)script->filename : nullName),
                                 script->lineno);
#endif
#ifdef MOZ_ETW
    if (ProfilingActive && !ETWStartExecution(cx, script))
        ok = false;
#endif

    return ok;
}

inline bool
Probes::stopExecution(JSContext *cx, JSScript *script)
{
    bool ok = true;

#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_EXECUTE_DONE_ENABLED())
        JAVASCRIPT_EXECUTE_DONE((script->filename ? (char *)script->filename : nullName),
                                script->lineno);
#endif
#ifdef MOZ_ETW
    if (ProfilingActive && !ETWStopExecution(cx, script))
        ok = false;
#endif

    return ok;
}

struct AutoFunctionCallProbe {
    JSContext * const cx;
    JSFunction *fun;
    JSScript *script;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

    AutoFunctionCallProbe(JSContext *cx, JSFunction *fun, JSScript *script
                          JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), fun(fun), script(script)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        Probes::enterJSFun(cx, fun, script);
    }

    ~AutoFunctionCallProbe() {
        Probes::exitJSFun(cx, fun, script);
    }
};

} /* namespace js */

/*
 * Internal functions for controlling various profilers. The profiler-specific
 * implementations of these are mostly in jsdbgapi.cpp.
 */

#endif /* _JSPROBES_H */

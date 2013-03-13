/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_ETW

#include "jswin.h"
#include <evntprov.h>
#include <sys/types.h>

/* Generated from ETWProvider.man */
#include "ETWProvider.h"
#endif

#include "jsapi.h"
#include "jsutil.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsinterp.h"
#include "jsobj.h"
#include "jsprobes.h"
#include "jsscript.h"
#include "jsstr.h"

#include "methodjit/Compiler.h"

#include "jsobjinlines.h"

#define TYPEOF(cx,v)    (JSVAL_IS_NULL(v) ? JSTYPE_NULL : JS_TypeOfValue(cx,v))

using namespace js;

const char Probes::nullName[] = "(null)";
const char Probes::anonymousName[] = "(anonymous)";

bool Probes::ProfilingActive = true;

Probes::JITReportGranularity
Probes::JITGranularityRequested(JSContext *cx)
{
    if (cx->runtime->spsProfiler.enabled())
        return JITREPORT_GRANULARITY_LINE;
    return JITREPORT_GRANULARITY_NONE;
}

#ifdef JS_METHODJIT

bool
Probes::registerMJITCode(JSContext *cx, js::mjit::JITChunk *chunk,
                         js::mjit::JSActiveFrame *outerFrame,
                         js::mjit::JSActiveFrame **inlineFrames)
{
    if (cx->runtime->spsProfiler.enabled() &&
        !cx->runtime->spsProfiler.registerMJITCode(chunk, outerFrame, inlineFrames))
    {
        return false;
    }

    return true;
}

void
Probes::discardMJITCode(FreeOp *fop, mjit::JITScript *jscr, mjit::JITChunk *chunk, void* address)
{
    if (fop->runtime()->spsProfiler.enabled())
        fop->runtime()->spsProfiler.discardMJITCode(jscr, chunk, address);
}

bool
Probes::registerICCode(JSContext *cx,
                       mjit::JITChunk *chunk, RawScript script, jsbytecode* pc,
                       void *start, size_t size)
{
    if (cx->runtime->spsProfiler.enabled() &&
        !cx->runtime->spsProfiler.registerICCode(chunk, script, pc, start, size))
    {
        return false;
    }
    return true;
}
#endif

/* ICs are unregistered in a batch */
void
Probes::discardExecutableRegion(void *start, size_t size)
{
    /*
     * Not needed for SPS because ICs are disposed of when the normal JITChunk
     * is disposed of
     */
}

static JSRuntime *initRuntime;

JSBool
Probes::startEngine()
{
    bool ok = true;

    return ok;
}

bool
Probes::createRuntime(JSRuntime *rt)
{
    bool ok = true;

    static JSCallOnceType once = { 0 };
    initRuntime = rt;
    if (!JS_CallOnce(&once, Probes::startEngine))
        ok = false;

#ifdef MOZ_ETW
    if (!ETWCreateRuntime(rt))
        ok = false;
#endif

    return ok;
}

bool
Probes::destroyRuntime(JSRuntime *rt)
{
    bool ok = true;
#ifdef MOZ_ETW
    if (!ETWDestroyRuntime(rt))
        ok = false;
#endif

    return ok;
}

bool
Probes::shutdown()
{
    bool ok = true;
#ifdef MOZ_ETW
    if (!ETWShutdown())
        ok = false;
#endif

    return ok;
}

#ifdef INCLUDE_MOZILLA_DTRACE
static const char *
ScriptFilename(const RawScript script)
{
    if (!script)
        return Probes::nullName;
    if (!script->filename())
        return Probes::anonymousName;
    return script->filename();
}

static const char *
FunctionName(JSContext *cx, RawFunction fun, JSAutoByteString* bytes)
{
    if (!fun)
        return Probes::nullName;
    if (!fun->displayAtom())
        return Probes::anonymousName;
    return bytes->encodeLatin1(cx, fun->displayAtom()) ? bytes->ptr() : Probes::nullName;
}

/*
 * These functions call the DTrace macros for the JavaScript USDT probes.
 * Originally this code was inlined in the JavaScript code; however since
 * a number of operations are called, these have been placed into functions
 * to reduce any negative compiler optimization effect that the addition of
 * a number of usually unused lines of code would cause.
 */
void
Probes::DTraceEnterJSFun(JSContext *cx, RawFunction fun, RawScript script)
{
    JSAutoByteString funNameBytes;
    JAVASCRIPT_FUNCTION_ENTRY(ScriptFilename(script), Probes::nullName,
                              FunctionName(cx, fun, &funNameBytes));
}

void
Probes::DTraceExitJSFun(JSContext *cx, RawFunction fun, RawScript script)
{
    JSAutoByteString funNameBytes;
    JAVASCRIPT_FUNCTION_RETURN(ScriptFilename(script), Probes::nullName,
                               FunctionName(cx, fun, &funNameBytes));
}
#endif

#ifdef MOZ_ETW
static void
current_location(JSContext *cx, int* lineno, char const **filename)
{
    RawScript script = cx->stack.currentScript()
    if (! script) {
        *lineno = -1;
        *filename = "(uninitialized)";
        return;
    }
    *lineno = js_PCToLineNumber(cx, script, js_GetCurrentBytecodePC(cx));
    *filename = ScriptFilename(script);
}

/*
 * ETW (Event Tracing for Windows)
 *
 * These are here rather than in the .h file to avoid having to include
 * windows.h in a header.
 */
bool
Probes::ETWCallTrackingActive()
{
    return MCGEN_ENABLE_CHECK(MozillaSpiderMonkey_Context, EvtFunctionEntry);
}

bool
Probes::ETWCreateRuntime(JSRuntime *rt)
{
    static bool registered = false;
    if (!registered) {
        EventRegisterMozillaSpiderMonkey();
        registered = true;
    }
    return true;
}

bool
Probes::ETWDestroyRuntime(JSRuntime *rt)
{
    return true;
}

bool
Probes::ETWShutdown()
{
    EventUnregisterMozillaSpiderMonkey();
    return true;
}

bool
Probes::ETWEnterJSFun(JSContext *cx, RawFunction fun, RawScript script, int counter)
{
    int lineno = script ? script->lineno : -1;
    JSAutoByteString bytes;
    return (EventWriteEvtFunctionEntry(ScriptFilename(script), lineno,
                                       ObjectClassname((JSObject *)fun),
                                       FunctionName(cx, fun, &bytes)) == ERROR_SUCCESS);
}

bool
Probes::ETWExitJSFun(JSContext *cx, RawFunction fun, RawScript script, int counter)
{
    int lineno = script ? script->lineno : -1;
    JSAutoByteString bytes;
    return (EventWriteEvtFunctionExit(ScriptFilename(script), lineno,
                                      ObjectClassname((JSObject *)fun),
                                      FunctionName(cx, fun, &bytes)) == ERROR_SUCCESS);
}

bool
Probes::ETWCreateObject(JSContext *cx, RawObject obj)
{
    int lineno;
    const char * script_filename;
    current_location(cx, &lineno, &script_filename);

    return EventWriteEvtObjectCreate(script_filename, lineno,
                                     ObjectClassname(obj), reinterpret_cast<uint64_t_t>(obj),
                                     obj ? obj->computedSizeOfIncludingThis() : 0) == ERROR_SUCCESS;
}

bool
Probes::ETWFinalizeObject(RawObject obj)
{
    return EventWriteEvtObjectFinalize(ObjectClassname(obj),
                                       reinterpret_cast<uint64_t_t>(obj)) == ERROR_SUCCESS;
}

bool
Probes::ETWResizeObject(JSContext *cx, RawObject obj, size_t oldSize, size_t newSize)
{
    int lineno;
    const char *script_filename;
    current_location(cx, &lineno, &script_filename);

    return EventWriteEvtObjectResize(script_filename, lineno,
                                     ObjectClassname(obj), reinterpret_cast<uint64_t_t>(obj),
                                     oldSize, newSize) == ERROR_SUCCESS;
}

bool
Probes::ETWCreateString(JSContext *cx, RawString string, size_t length)
{
    int lineno;
    const char *script_filename;
    current_location(cx, &lineno, &script_filename);

    return EventWriteEvtStringCreate(script_filename, lineno,
                                     reinterpret_cast<uint64_t_t>(string), length) ==
           ERROR_SUCCESS;
}

bool
Probes::ETWFinalizeString(RawString string)
{
    return EventWriteEvtStringFinalize(reinterpret_cast<uint64_t>(string),
                                       string->length()) == ERROR_SUCCESS;
}

bool
Probes::ETWCompileScriptBegin(const char *filename, int lineno)
{
    return EventWriteEvtScriptCompileBegin(filename, lineno) == ERROR_SUCCESS;
}

bool
Probes::ETWCompileScriptEnd(const char *filename, int lineno)
{
    return EventWriteEvtScriptCompileEnd(filename, lineno) == ERROR_SUCCESS;
}

bool
Probes::ETWCalloutBegin(JSContext *cx, RawFunction fun)
{
    const char *script_filename;
    int lineno;
    JSAutoByteString bytes;
    current_location(cx, &lineno, &script_filename);

    return EventWriteEvtCalloutBegin(script_filename,
                                     lineno,
                                     ObjectClassname((JSObject *)fun),
                                     FunctionName(cx, fun, &bytes)) == ERROR_SUCCESS;
}

bool
Probes::ETWCalloutEnd(JSContext *cx, RawFunction fun)
{
        const char *script_filename;
        int lineno;
        JSAutoByteString bytes;
        current_location(cx, &lineno, &script_filename);

        return EventWriteEvtCalloutEnd(script_filename,
                                       lineno,
                                       ObjectClassname((JSObject *)fun),
                                       FunctionName(cx, fun, &bytes)) == ERROR_SUCCESS;
}

bool
Probes::ETWAcquireMemory(JSContext *cx, void *address, size_t nbytes)
{
    return EventWriteEvtMemoryAcquire(reinterpret_cast<uint64_t>(cx->compartment),
                                      reinterpret_cast<uint64_t>(address),
                                      nbytes) == ERROR_SUCCESS;
}

bool
Probes::ETWReleaseMemory(JSContext *cx, void *address, size_t nbytes)
{
    return EventWriteEvtMemoryRelease(reinterpret_cast<uint64_t>(cx->compartment),
                                      reinterpret_cast<uint64_t>(address),
                                      nbytes) == ERROR_SUCCESS;
}

bool
Probes::ETWGCStart(JSCompartment *compartment)
{
    return EventWriteEvtGCStart(reinterpret_cast<uint64_t>(compartment)) == ERROR_SUCCESS;
}

bool
Probes::ETWGCEnd(JSCompartment *compartment)
{
    return EventWriteEvtGCEnd(reinterpret_cast<uint64_t>(compartment)) == ERROR_SUCCESS;
}

bool
Probes::ETWGCStartMarkPhase(JSCompartment *compartment)
{
    return EventWriteEvtGCStartMarkPhase(reinterpret_cast<uint64_t>(compartment)) == ERROR_SUCCESS;
}

bool
Probes::ETWGCEndMarkPhase(JSCompartment *compartment)
{
    return EventWriteEvtGCEndMarkPhase(reinterpret_cast<uint64_t>(compartment)) == ERROR_SUCCESS;
}

bool
Probes::ETWGCStartSweepPhase(JSCompartment *compartment)
{
    return EventWriteEvtGCStartSweepPhase(reinterpret_cast<uint64_t>(compartment)) ==
           ERROR_SUCCESS;
}

bool
Probes::ETWGCEndSweepPhase(JSCompartment *compartment)
{
    return EventWriteEvtGCEndSweepPhase(reinterpret_cast<uint64_t>(compartment)) == ERROR_SUCCESS;
}

bool
Probes::ETWCustomMark(RawString string)
{
    const jschar *chars = string->getCharsZ(NULL);
    return !chars || EventWriteEvtCustomString(chars) == ERROR_SUCCESS;
}

bool
Probes::ETWCustomMark(const char *string)
{
    return EventWriteEvtCustomANSIString(string) == ERROR_SUCCESS;
}

bool
Probes::ETWCustomMark(int marker)
{
    return EventWriteEvtCustomInt(marker) == ERROR_SUCCESS;
}

bool
Probes::ETWStartExecution(RawScript script)
{
    int lineno = script ? script->lineno : -1;
    return EventWriteEvtExecuteStart(ScriptFilename(script), lineno) == ERROR_SUCCESS;
}

bool
Probes::ETWStopExecution(RawScript script)
{
    int lineno = script ? script->lineno : -1;
    return EventWriteEvtExecuteDone(ScriptFilename(script), lineno) == ERROR_SUCCESS;
}

bool
Probes::ETWResizeHeap(JS::Zone *zone, size_t oldSize, size_t newSize)
{
    return EventWriteEvtHeapResize(reinterpret_cast<uint64_t>(zone),
                                   oldSize, newSize) == ERROR_SUCCESS;
}

#endif

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

static Vector<Probes::JITWatcher*, 4, SystemAllocPolicy> jitWatchers;

bool
Probes::addJITWatcher(JITWatcher *watcher)
{
    return jitWatchers.append(watcher);
}

bool
Probes::removeJITWatcher(JSRuntime *rt, JITWatcher *watcher)
{
    JITWatcher **place = Find(jitWatchers, watcher);
    if (!place)
        return false;
    if (rt)
        rt->delete_(*place);
    else
        Foreground::delete_(*place);
    jitWatchers.erase(place);
    return true;
}

void
Probes::removeAllJITWatchers(JSRuntime *rt)
{
    if (rt) {
        for (JITWatcher **p = jitWatchers.begin(); p != jitWatchers.end(); ++p)
            rt->delete_(*p);
    } else {
        for (JITWatcher **p = jitWatchers.begin(); p != jitWatchers.end(); ++p)
            Foreground::delete_(*p);
    }
    jitWatchers.clear();
}

Probes::JITReportGranularity
Probes::JITGranularityRequested()
{
    JITReportGranularity want = JITREPORT_GRANULARITY_NONE;
    for (JITWatcher **p = jitWatchers.begin(); p != jitWatchers.end(); ++p) {
        JITReportGranularity request = (*p)->granularityRequested();
        if (request > want)
            want = request;
    }

    return want;
}

#ifdef JS_METHODJIT
/*
 * Flatten the tree of inlined frames into a series of native code regions, one
 * for each contiguous section of native code that belongs to a single
 * ActiveFrame. (Note that some of these regions may be zero-length, for
 * example if two ActiveFrames end at the same place.)
 */
typedef mjit::Compiler::ActiveFrame ActiveFrame;

bool
Probes::JITWatcher::CollectNativeRegions(RegionVector &regions,
                                         JSRuntime *rt,
                                         mjit::JITChunk *jit,
                                         mjit::JSActiveFrame *outerFrame,
                                         mjit::JSActiveFrame **inlineFrames)
{
    regions.resize(jit->nInlineFrames * 2 + 2);

    mjit::JSActiveFrame **stack =
        rt->array_new<mjit::JSActiveFrame*>(jit->nInlineFrames+2);
    if (!stack)
        return false;
    uint32_t depth = 0;
    uint32_t ip = 0;

    stack[depth++] = NULL;
    stack[depth++] = outerFrame;
    regions[0].frame = outerFrame;
    regions[0].script = outerFrame->script;
    regions[0].pc = outerFrame->script->code;
    regions[0].enter = true;
    ip++;

    for (uint32_t i = 0; i <= jit->nInlineFrames; i++) {
        mjit::JSActiveFrame *frame = (i < jit->nInlineFrames) ? inlineFrames[i] : outerFrame;

        // Not a down frame; pop the current frame, then pop until we reach
        // this frame's parent, recording subframe ends as we go
        while (stack[depth-1] != frame->parent) {
            depth--;
            JS_ASSERT(depth > 0);
            // Pop up from regions[ip-1].frame to top of the stack: start a
            // region in the destination frame and close off the source
            // (origin) frame at the end of its script
            mjit::JSActiveFrame *src = regions[ip-1].frame;
            mjit::JSActiveFrame *dst = stack[depth-1];
            JS_ASSERT_IF(!dst, i == jit->nInlineFrames);
            regions[ip].frame = dst;
            regions[ip].script = dst ? dst->script : NULL;
            regions[ip].pc = src->parentPC + 1;
            regions[ip-1].endpc = src->script->code + src->script->length;
            regions[ip].enter = false;
            ip++;
        }

        if (i < jit->nInlineFrames) {
            // Push a frame (enter an inlined function). Start a region at the
            // beginning of the new frame's script, and end the previous region
            // at parentPC.
            stack[depth++] = frame;

            regions[ip].frame = frame;
            regions[ip].script = frame->script;
            regions[ip].pc = frame->script->code;
            regions[ip-1].endpc = frame->parentPC;
            regions[ip].enter = true;
            ip++;
        }
    }

    // Final region is always zero-length and not particularly useful
    ip--;
    regions.popBack();

    mjit::JSActiveFrame *prev = NULL;
    for (NativeRegion *iter = regions.begin(); iter != regions.end(); ++iter) {
        mjit::JSActiveFrame *frame = iter->frame;
        if (iter->enter) {
            // Pushing down a frame, so region starts at the beginning of the
            // (destination) frame
            iter->mainOffset = frame->mainCodeStart;
            iter->stubOffset = frame->stubCodeStart;
        } else {
            // Popping up a level, so region starts at the end of the (source) frame
            iter->mainOffset = prev->mainCodeEnd;
            iter->stubOffset = prev->stubCodeEnd;
        }
        prev = frame;
    }

    JS_ASSERT(ip == 2 * jit->nInlineFrames + 1);
    rt->array_delete(stack);

    // All of the stub code comes immediately after the main code
    for (NativeRegion *iter = regions.begin(); iter != regions.end(); ++iter)
        iter->stubOffset += outerFrame->mainCodeEnd;

    return true;
}

void
Probes::registerMJITCode(JSContext *cx, js::mjit::JITChunk *chunk,
                         js::mjit::JSActiveFrame *outerFrame,
                         js::mjit::JSActiveFrame **inlineFrames,
                         void *mainCodeAddress, size_t mainCodeSize,
                         void *stubCodeAddress, size_t stubCodeSize)
{
    for (JITWatcher **p = jitWatchers.begin(); p != jitWatchers.end(); ++p)
        (*p)->registerMJITCode(cx, chunk, outerFrame,
                               inlineFrames,
                               mainCodeAddress, mainCodeSize,
                               stubCodeAddress, stubCodeSize);
}

void
Probes::discardMJITCode(FreeOp *fop, mjit::JITScript *jscr, mjit::JITChunk *chunk, void* address)
{
    for (JITWatcher **p = jitWatchers.begin(); p != jitWatchers.end(); ++p)
        (*p)->discardMJITCode(fop, jscr, chunk, address);
}

void
Probes::registerICCode(JSContext *cx,
                       mjit::JITChunk *chunk, JSScript *script, jsbytecode* pc,
                       void *start, size_t size)
{
    for (JITWatcher **p = jitWatchers.begin(); p != jitWatchers.end(); ++p)
        (*p)->registerICCode(cx, chunk, script, pc, start, size);
}
#endif

/* ICs are unregistered in a batch */
void
Probes::discardExecutableRegion(void *start, size_t size)
{
    for (JITWatcher **p = jitWatchers.begin(); p != jitWatchers.end(); ++p)
        (*p)->discardExecutableRegion(start, size);
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

    Probes::removeAllJITWatchers(NULL);

    return ok;
}

#ifdef INCLUDE_MOZILLA_DTRACE
static const char *
ScriptFilename(const JSScript *script)
{
    if (!script)
        return Probes::nullName;
    if (!script->filename)
        return Probes::anonymousName;
    return script->filename;
}

static const char *
FunctionName(JSContext *cx, const JSFunction *fun, JSAutoByteString* bytes)
{
    if (!fun)
        return Probes::nullName;
    if (!fun->atom)
        return Probes::anonymousName;
    return bytes->encode(cx, fun->atom) ? bytes->ptr() : Probes::nullName;
}

/*
 * These functions call the DTrace macros for the JavaScript USDT probes.
 * Originally this code was inlined in the JavaScript code; however since
 * a number of operations are called, these have been placed into functions
 * to reduce any negative compiler optimization effect that the addition of
 * a number of usually unused lines of code would cause.
 */
void
Probes::DTraceEnterJSFun(JSContext *cx, JSFunction *fun, JSScript *script)
{
    JSAutoByteString funNameBytes;
    JAVASCRIPT_FUNCTION_ENTRY(ScriptFilename(script), Probes::nullName,
                              FunctionName(cx, fun, &funNameBytes));
}

void
Probes::DTraceExitJSFun(JSContext *cx, JSFunction *fun, JSScript *script)
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
    JSScript *script = js_GetCurrentScript(cx);
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
Probes::ETWCallTrackingActive(JSContext *cx)
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
Probes::ETWEnterJSFun(JSContext *cx, JSFunction *fun, JSScript *script, int counter)
{
    int lineno = script ? script->lineno : -1;
    JSAutoByteString bytes;
    return (EventWriteEvtFunctionEntry(ScriptFilename(script), lineno,
                                       ObjectClassname((JSObject *)fun),
                                       FunctionName(cx, fun, &bytes)) == ERROR_SUCCESS);
}

bool
Probes::ETWExitJSFun(JSContext *cx, JSFunction *fun, JSScript *script, int counter)
{
    int lineno = script ? script->lineno : -1;
    JSAutoByteString bytes;
    return (EventWriteEvtFunctionExit(ScriptFilename(script), lineno,
                                      ObjectClassname((JSObject *)fun),
                                      FunctionName(cx, fun, &bytes)) == ERROR_SUCCESS);
}

bool
Probes::ETWCreateObject(JSContext *cx, JSObject *obj)
{
    int lineno;
    const char * script_filename;
    current_location(cx, &lineno, &script_filename);

    return EventWriteEvtObjectCreate(script_filename, lineno,
                                     ObjectClassname(obj), reinterpret_cast<uint64_t_t>(obj),
                                     obj ? obj->computedSizeOfIncludingThis() : 0) == ERROR_SUCCESS;
}

bool
Probes::ETWFinalizeObject(JSObject *obj)
{
    return EventWriteEvtObjectFinalize(ObjectClassname(obj),
                                       reinterpret_cast<uint64_t_t>(obj)) == ERROR_SUCCESS;
}

bool
Probes::ETWResizeObject(JSContext *cx, JSObject *obj, size_t oldSize, size_t newSize)
{
    int lineno;
    const char *script_filename;
    current_location(cx, &lineno, &script_filename);

    return EventWriteEvtObjectResize(script_filename, lineno,
                                     ObjectClassname(obj), reinterpret_cast<uint64_t_t>(obj),
                                     oldSize, newSize) == ERROR_SUCCESS;
}

bool
Probes::ETWCreateString(JSContext *cx, JSString *string, size_t length)
{
    int lineno;
    const char *script_filename;
    current_location(cx, &lineno, &script_filename);

    return EventWriteEvtStringCreate(script_filename, lineno,
                                     reinterpret_cast<uint64_t_t>(string), length) ==
           ERROR_SUCCESS;
}

bool
Probes::ETWFinalizeString(JSString *string)
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
Probes::ETWCalloutBegin(JSContext *cx, JSFunction *fun)
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
Probes::ETWCalloutEnd(JSContext *cx, JSFunction *fun)
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
Probes::ETWCustomMark(JSString *string)
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
Probes::ETWStartExecution(JSContext *cx, JSScript *script)
{
    int lineno = script ? script->lineno : -1;
    return EventWriteEvtExecuteStart(ScriptFilename(script), lineno) == ERROR_SUCCESS;
}

bool
Probes::ETWStopExecution(JSContext *cx, JSScript *script)
{
    int lineno = script ? script->lineno : -1;
    return EventWriteEvtExecuteDone(ScriptFilename(script), lineno) == ERROR_SUCCESS;
}

bool
Probes::ETWResizeHeap(JSCompartment *compartment, size_t oldSize, size_t newSize)
{
    return EventWriteEvtHeapResize(reinterpret_cast<uint64_t>(compartment),
                                   oldSize, newSize) == ERROR_SUCCESS;
}

#endif

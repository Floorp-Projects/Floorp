/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsprobes.h"

#include "jscntxt.h"
#include "jsscript.h"

#ifdef INCLUDE_MOZILLA_DTRACE
#include "jsscriptinlines.h" 
#endif

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
                       mjit::JITChunk *chunk, JSScript *script, jsbytecode* pc,
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

#ifdef INCLUDE_MOZILLA_DTRACE
static const char *
ScriptFilename(const JSScript *script)
{
    if (!script)
        return Probes::nullName;
    if (!script->filename())
        return Probes::anonymousName;
    return script->filename();
}

static const char *
FunctionName(JSContext *cx, JSFunction *fun, JSAutoByteString* bytes)
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

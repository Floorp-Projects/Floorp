/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 2007  Sun Microsystems, Inc. All Rights Reserved.
 *
 * Contributor(s):
 *      Brendan Eich <brendan@mozilla.org>
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

#ifdef MOZ_ETW
#include "jswin.h"
#include <evntprov.h>

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
#include "jsscript.h"
#include "jsstaticcheck.h"
#include "jsstr.h"

#ifdef __APPLE__
#include "sharkctl.h"
#endif

#include "jsprobes.h"
#include <sys/types.h>

#define TYPEOF(cx,v)    (JSVAL_IS_NULL(v) ? JSTYPE_NULL : JS_TypeOfValue(cx,v))

using namespace js;

const char Probes::nullName[] = "(null)";
const char Probes::anonymousName[] = "(anonymous)";

bool Probes::ProfilingActive = true;

bool
Probes::controlProfilers(JSContext *cx, bool toState)
{
    JSBool ok = JS_TRUE;
#if defined(MOZ_CALLGRIND) || defined(MOZ_VTUNE)
    jsval dummy;
#endif

    if (! ProfilingActive && toState) {
#if defined(MOZ_SHARK) && defined(__APPLE__)
        if (!Shark::Start())
            ok = JS_FALSE;
#endif
#ifdef MOZ_CALLGRIND
        if (! js_StartCallgrind(cx, 0, &dummy))
            ok = JS_FALSE;
#endif
#ifdef MOZ_VTUNE
        if (! js_ResumeVtune(cx, 0, &dummy))
            ok = JS_FALSE;
#endif
    } else if (ProfilingActive && ! toState) {
#if defined(MOZ_SHARK) && defined(__APPLE__)
        Shark::Stop();
#endif
#ifdef MOZ_CALLGRIND
        if (! js_StopCallgrind(cx, 0, &dummy))
            ok = JS_FALSE;
#endif
#ifdef MOZ_VTUNE
        if (! js_PauseVtune(cx, 0, &dummy))
            ok = JS_FALSE;
#endif
    }

    ProfilingActive = toState;

    return ok;
}

void
Probes::current_location(JSContext *cx, int* lineno, char const **filename)
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

const char *
Probes::FunctionClassname(const JSFunction *fun)
{
    return (fun && !FUN_INTERPRETED(fun) && !(fun->flags & JSFUN_TRCINFO) && FUN_CLASP(fun))
           ? (char *)FUN_CLASP(fun)->name
           : nullName;
}

const char *
Probes::ScriptFilename(JSScript *script)
{
    return (script && script->filename) ? (char *)script->filename : nullName;
}

int
Probes::FunctionLineNumber(JSContext *cx, const JSFunction *fun)
{
    if (fun && FUN_INTERPRETED(fun))
        return (int) JS_GetScriptBaseLineNumber(cx, FUN_SCRIPT(fun));

    return 0;
}

#ifdef INCLUDE_MOZILLA_DTRACE
/*
 * These functions call the DTrace macros for the JavaScript USDT probes.
 * Originally this code was inlined in the JavaScript code; however since
 * a number of operations are called, these have been placed into functions
 * to reduce any negative compiler optimization effect that the addition of
 * a number of usually unused lines of code would cause.
 */
void
Probes::enterJSFunImpl(JSContext *cx, JSFunction *fun, JSScript *script)
{
    JSAutoByteString funNameBytes;
    JAVASCRIPT_FUNCTION_ENTRY(ScriptFilename(script), FunctionClassname(fun),
                              FunctionName(cx, fun, &funNameBytes));
}

void
Probes::handleFunctionReturn(JSContext *cx, JSFunction *fun, JSScript *script)
{
    JSAutoByteString funNameBytes;
    JAVASCRIPT_FUNCTION_RETURN(ScriptFilename(script), FunctionClassname(fun),
                               FunctionName(cx, fun, &funNameBytes));
}

#endif

bool
Probes::startProfiling()
{
#if defined(MOZ_SHARK) && defined(__APPLE__)
    if (Shark::Start())
        return true;
#endif
    return false;
}

void
Probes::stopProfiling()
{
#if defined(MOZ_SHARK) && defined(__APPLE__)
    Shark::Stop();
#endif
}


#ifdef MOZ_ETW
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
                                     ObjectClassname(obj), reinterpret_cast<JSUint64>(obj),
                                     obj ? obj->slotsAndStructSize() : 0) == ERROR_SUCCESS;
}

bool
Probes::ETWFinalizeObject(JSObject *obj)
{
    return EventWriteEvtObjectFinalize(ObjectClassname(obj),
                                       reinterpret_cast<JSUint64>(obj)) == ERROR_SUCCESS;
}

bool
Probes::ETWResizeObject(JSContext *cx, JSObject *obj, size_t oldSize, size_t newSize)
{
    int lineno;
    const char *script_filename;
    current_location(cx, &lineno, &script_filename);

    return EventWriteEvtObjectResize(script_filename, lineno,
                                     ObjectClassname(obj), reinterpret_cast<JSUint64>(obj),
                                     oldSize, newSize) == ERROR_SUCCESS;
}

bool
Probes::ETWCreateString(JSContext *cx, JSString *string, size_t length)
{
    int lineno;
    const char *script_filename;
    current_location(cx, &lineno, &script_filename);

    return EventWriteEvtStringCreate(script_filename, lineno,
                                     reinterpret_cast<JSUint64>(string), length) == ERROR_SUCCESS;
}

bool
Probes::ETWFinalizeString(JSString *string)
{
    return EventWriteEvtStringFinalize(reinterpret_cast<JSUint64>(string), string->length()) == ERROR_SUCCESS;
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
    return EventWriteEvtMemoryAcquire(reinterpret_cast<JSUint64>(cx->compartment),
                                      reinterpret_cast<JSUint64>(address),
                                      nbytes) == ERROR_SUCCESS;
}

bool
Probes::ETWReleaseMemory(JSContext *cx, void *address, size_t nbytes)
{
    return EventWriteEvtMemoryRelease(reinterpret_cast<JSUint64>(cx->compartment),
                                      reinterpret_cast<JSUint64>(address),
                                      nbytes) == ERROR_SUCCESS;
}

bool
Probes::ETWGCStart(JSCompartment *compartment)
{
    return EventWriteEvtGCStart(reinterpret_cast<JSUint64>(compartment)) == ERROR_SUCCESS;
}

bool
Probes::ETWGCEnd(JSCompartment *compartment)
{
    return EventWriteEvtGCEnd(reinterpret_cast<JSUint64>(compartment)) == ERROR_SUCCESS;
}

bool
Probes::ETWGCStartMarkPhase(JSCompartment *compartment)
{
    return EventWriteEvtGCStartMarkPhase(reinterpret_cast<JSUint64>(compartment)) == ERROR_SUCCESS;
}

bool
Probes::ETWGCEndMarkPhase(JSCompartment *compartment)
{
    return EventWriteEvtGCEndMarkPhase(reinterpret_cast<JSUint64>(compartment)) == ERROR_SUCCESS;
}

bool
Probes::ETWGCStartSweepPhase(JSCompartment *compartment)
{
    return EventWriteEvtGCStartSweepPhase(reinterpret_cast<JSUint64>(compartment)) == ERROR_SUCCESS;
}

bool
Probes::ETWGCEndSweepPhase(JSCompartment *compartment)
{
    return EventWriteEvtGCEndSweepPhase(reinterpret_cast<JSUint64>(compartment)) == ERROR_SUCCESS;
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
    return EventWriteEvtHeapResize(reinterpret_cast<JSUint64>(compartment),
                                   oldSize, newSize) == ERROR_SUCCESS;
}

#endif

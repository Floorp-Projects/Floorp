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
#if defined(MOZ_SHARK) || defined(MOZ_CALLGRIND) || defined(MOZ_VTUNE)
    jsval dummy;
#endif

    if (! ProfilingActive && toState) {
#ifdef MOZ_SHARK
        if (! js_StartShark(cx, 0, &dummy))
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
#ifdef MOZ_SHARK
        if (! js_StopShark(cx, 0, &dummy))
            ok = JS_FALSE;
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
#ifdef MOZ_SHARK
    if (Shark::Start())
        return true;
#endif
    return false;
}

void
Probes::stopProfiling()
{
#ifdef MOZ_SHARK
    Shark::Stop();
#endif
}

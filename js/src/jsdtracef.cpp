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
#include "jsstr.h"

#include "jsdtracef.h"
#include <sys/types.h>

#define TYPEOF(cx,v)    (JSVAL_IS_NULL(v) ? JSTYPE_NULL : JS_TypeOfValue(cx,v))

using namespace js;

static char dempty[] = "<null>";

static char *
jsdtrace_fun_classname(const JSFunction *fun)
{
    return (fun && !FUN_INTERPRETED(fun) && !(fun->flags & JSFUN_TRCINFO) && FUN_CLASP(fun))
           ? (char *)FUN_CLASP(fun)->name
           : dempty;
}

static char *
jsdtrace_filename(JSStackFrame *fp)
{
    return (fp && fp->script && fp->script->filename) ? (char *)fp->script->filename : dempty;
}

static int
jsdtrace_fun_linenumber(JSContext *cx, const JSFunction *fun)
{
    if (fun && FUN_INTERPRETED(fun))
        return (int) JS_GetScriptBaseLineNumber(cx, FUN_SCRIPT(fun));

    return 0;
}

static int
jsdtrace_frame_linenumber(JSContext *cx, JSStackFrame *fp)
{
    if (fp)
        return (int) js_FramePCToLineNumber(cx, fp);

    return 0;
}

/*
 * This function is used to convert function arguments and return value (jsval)
 * into the following based on each value's type tag:
 *
 *      jsval      returned
 *      -------------------
 *      STRING  -> char *
 *      INT     -> int
 *      DOUBLE  -> double *
 *      BOOLEAN -> int
 *      OBJECT  -> void *
 *
 * All are presented as void * for DTrace consumers to use, after shifting or
 * masking out the JavaScript type bits. This allows D scripts to use ints and
 * booleans directly and copyinstr() for string arguments, when types are known
 * beforehand.
 *
 * This is used by the function-args and function-rval probes, which also
 * provide raw (unmasked) jsvals should type info be useful from D scripts.
 */
static void *
jsdtrace_jsvaltovoid(JSContext *cx, const js::Value &argval)
{
    if (argval.isNull())
        return (void *)JS_TYPE_STR(JSTYPE_NULL);

    if (argval.isUndefined())
        return (void *)JS_TYPE_STR(JSTYPE_VOID);

    if (argval.isBoolean())
        return (void *)argval.asBoolean();

    if (argval.isString())
        return (void *)js_GetStringBytes(cx, argval.asString());

    if (argval.isNumber()) {
        if (argval.isInt32())
            return (void *)argval.asInt32();
        // FIXME Now what?
        //return (void *)argval.asDouble();
    }

    return argval.asGCThing();
}

static char *
jsdtrace_fun_name(JSContext *cx, const JSFunction *fun)
{
    if (!fun)
        return dempty;

    JSAtom *atom = fun->atom;
    if (!atom) {
        /*
         * TODO: maybe do more work here to figure out the name of the property
         * or variable that held the anonymous function that we're calling, if anyone
         * cares; an easy workaround is to just give your anonymous functions names.
         */
        return dempty;
    }

    char *name = (char *)js_GetStringBytes(cx, ATOM_TO_STRING(atom));
    return name ? name : dempty;
}

/*
 * These functions call the DTrace macros for the JavaScript USDT probes.
 * Originally this code was inlined in the JavaScript code; however since
 * a number of operations are called, these have been placed into functions
 * to reduce any negative compiler optimization effect that the addition of
 * a number of usually unused lines of code would cause.
 */
void
DTrace::enterJSFunImpl(JSContext *cx, JSStackFrame *fp, const JSFunction *fun)
{
    JAVASCRIPT_FUNCTION_ENTRY(jsdtrace_filename(fp), jsdtrace_fun_classname(fun),
                              jsdtrace_fun_name(cx, fun));
}

void
DTrace::handleFunctionInfo(JSContext *cx, JSStackFrame *fp, JSStackFrame *dfp, JSFunction *fun)
{
    JAVASCRIPT_FUNCTION_INFO(jsdtrace_filename(fp), jsdtrace_fun_classname(fun),
                             jsdtrace_fun_name(cx, fun), jsdtrace_fun_linenumber(cx, fun),
                             jsdtrace_filename(dfp), jsdtrace_frame_linenumber(cx, dfp));
}

void
DTrace::handleFunctionArgs(JSContext *cx, JSStackFrame *fp, const JSFunction *fun, jsuint argc,
                           js::Value *argv)
{
    JAVASCRIPT_FUNCTION_ARGS(jsdtrace_filename(fp), jsdtrace_fun_classname(fun),
                             jsdtrace_fun_name(cx, fun), argc, (void *)argv,
                             (argc > 0) ? jsdtrace_jsvaltovoid(cx, argv[0]) : 0,
                             (argc > 1) ? jsdtrace_jsvaltovoid(cx, argv[1]) : 0,
                             (argc > 2) ? jsdtrace_jsvaltovoid(cx, argv[2]) : 0,
                             (argc > 3) ? jsdtrace_jsvaltovoid(cx, argv[3]) : 0,
                             (argc > 4) ? jsdtrace_jsvaltovoid(cx, argv[4]) : 0);
}

void
DTrace::handleFunctionRval(JSContext *cx, JSStackFrame *fp, JSFunction *fun, const js::Value &rval)
{
    JAVASCRIPT_FUNCTION_RVAL(jsdtrace_filename(fp), jsdtrace_fun_classname(fun),
                             jsdtrace_fun_name(cx, fun), jsdtrace_fun_linenumber(cx, fun),
                             NULL, jsdtrace_jsvaltovoid(cx, rval));
}

void
DTrace::handleFunctionReturn(JSContext *cx, JSStackFrame *fp, JSFunction *fun)
{
    JAVASCRIPT_FUNCTION_RETURN(jsdtrace_filename(fp), jsdtrace_fun_classname(fun),
                               jsdtrace_fun_name(cx, fun));
}

void
DTrace::ObjectCreationScope::handleCreationStart()
{
    JAVASCRIPT_OBJECT_CREATE_START(jsdtrace_filename(fp), (char *)clasp->name);
}

void
DTrace::ObjectCreationScope::handleCreationEnd()
{
    JAVASCRIPT_OBJECT_CREATE_DONE(jsdtrace_filename(fp), (char *)clasp->name);
}

void
DTrace::ObjectCreationScope::handleCreationImpl(JSObject *obj)
{
    JAVASCRIPT_OBJECT_CREATE(jsdtrace_filename(cx->fp), (char *)clasp->name, (uintptr_t)obj,
                             jsdtrace_frame_linenumber(cx, cx->fp));
}

void
DTrace::finalizeObjectImpl(JSObject *obj)
{
    Class *clasp = obj->getClass();

    /* the first arg is NULL - reserved for future use (filename?) */
    JAVASCRIPT_OBJECT_FINALIZE(NULL, (char *)clasp->name, (uintptr_t)obj);
}

void
DTrace::ExecutionScope::startExecution()
{
    JAVASCRIPT_EXECUTE_START(script->filename ? (char *)script->filename : dempty,
                             script->lineno);
}

void
DTrace::ExecutionScope::endExecution()
{
    JAVASCRIPT_EXECUTE_DONE(script->filename ? (char *)script->filename : dempty, script->lineno);
}

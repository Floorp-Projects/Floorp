/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * JS execution context.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#ifndef NSPR20
#include "prarena.h"
#else
#include "plarena.h"
#endif
#include "prlog.h"
#include "prclist.h"
#include "prprf.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsdbgapi.h"
#include "jsgc.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscan.h"
#include "jsscript.h"

JSInterpreterHooks *js_InterpreterHooks = NULL;

JS_FRIEND_API(void)
js_SetInterpreterHooks(JSInterpreterHooks *hooks)
{
    js_InterpreterHooks = hooks;
}

JSContext *
js_NewContext(JSRuntime *rt, size_t stacksize)
{
    JSContext *cx;

    cx = malloc(sizeof *cx);
    if (!cx)
	return NULL;
    memset(cx, 0, sizeof *cx);

    cx->runtime = rt;
#ifdef JS_THREADSAFE
    js_InitContextForLocking(cx);
#endif
    if (rt->contextList.next == (PRCList *)&rt->contextList) {
	/* First context on this runtime: initialize atoms and keywords. */
	if (!js_InitAtomState(cx, &rt->atomState) ||
	    !js_InitScanner(cx)) {
	    free(cx);
	    return NULL;
	}
    }
    /* Atomicly append cx to rt's context list. */
    JS_LOCK_RUNTIME_VOID(rt, PR_APPEND_LINK(&cx->links, &rt->contextList));

    cx->version = JSVERSION_DEFAULT;
    cx->jsop_eq = JSOP_EQ;
    cx->jsop_ne = JSOP_NE;
    PR_InitArenaPool(&cx->stackPool, "stack", stacksize, sizeof(jsval));
    PR_InitArenaPool(&cx->codePool, "code", 1024, sizeof(jsbytecode));
    PR_InitArenaPool(&cx->tempPool, "temp", 1024, sizeof(jsdouble));

#if JS_HAS_REGEXPS
    if (!js_InitRegExpStatics(cx, &cx->regExpStatics)) {
	js_DestroyContext(cx);
	return NULL;
    }
#endif
    return cx;
}

void
js_DestroyContext(JSContext *cx)
{
    JSRuntime *rt;
    JSBool rtempty;

    rt = cx->runtime;

    /* Remove cx from context list first. */
    JS_LOCK_RUNTIME(rt);
    PR_REMOVE_LINK(&cx->links);
    rtempty = (rt->contextList.next == (PRCList *)&rt->contextList);
    JS_UNLOCK_RUNTIME(rt);

    if (js_InterpreterHooks && js_InterpreterHooks->destroyContext) {
	/* This is a stub, but in case it removes roots, call it now. */
        js_InterpreterHooks->destroyContext(cx);
    }

    if (rtempty) {
	/* Unpin all pinned atoms before final GC. */
	js_UnpinPinnedAtoms(&rt->atomState);

	/* Unlock GC things held by runtime pointers. */
	js_UnlockGCThing(cx, rt->jsNaN);
	js_UnlockGCThing(cx, rt->jsNegativeInfinity);
	js_UnlockGCThing(cx, rt->jsPositiveInfinity);
	js_UnlockGCThing(cx, rt->emptyString);

	/*
	 * Clear these so they get recreated if the standard classes are
	 * initialized again.
	 */
	rt->jsNaN = NULL;
	rt->jsNegativeInfinity = NULL;
	rt->jsPositiveInfinity = NULL;
	rt->emptyString = NULL;

	/* Clear debugging state to remove GC roots. */
	JS_ClearAllTraps(cx);
	JS_ClearAllWatchPoints(cx);
    }

    /* Remove more GC roots in regExpStatics, then collect garbage. */
#if JS_HAS_REGEXPS
    js_FreeRegExpStatics(cx, &cx->regExpStatics);
#endif
    js_ForceGC(cx);

    if (rtempty) {
	/* Free atom state now that we've run the GC. */
	js_FreeAtomState(cx, &rt->atomState);
    }

    /* Free the stuff hanging off of cx. */
    PR_FinishArenaPool(&cx->stackPool);
    PR_FinishArenaPool(&cx->codePool);
    PR_FinishArenaPool(&cx->tempPool);
    if (cx->lastMessage)
	free(cx->lastMessage);
    free(cx);
}

JSContext *
js_ContextIterator(JSRuntime *rt, JSContext **iterp)
{
    JSContext *cx = *iterp;

    JS_LOCK_RUNTIME(rt);
    if (!cx)
	cx = (JSContext *)rt->contextList.next;
    if ((void *)cx == &rt->contextList)
	cx = NULL;
    else
	*iterp = (JSContext *)cx->links.next;
    JS_UNLOCK_RUNTIME(rt);
    return cx;
}

void
js_ReportErrorVA(JSContext *cx, const char *format, va_list ap)
{
    JSStackFrame *fp;
    JSErrorReport report, *reportp;
    char *last;

    fp = cx->fp;
    if (fp && fp->script && fp->pc) {
	report.filename = fp->script->filename;
	report.lineno = js_PCToLineNumber(fp->script, fp->pc);
	/* XXX should fetch line somehow */
	report.linebuf = NULL;
	report.tokenptr = NULL;
	reportp = &report;
    } else {
	reportp = NULL;
    }
    last = PR_vsmprintf(format, ap);
    if (!last)
	return;

    js_ReportErrorAgain(cx, last, reportp);
    free(last);
}

JS_FRIEND_API(void)
js_ReportErrorAgain(JSContext *cx, const char *message, JSErrorReport *reportp)
{
    JSErrorReporter onError;

    if (!message)
	return;
    if (cx->lastMessage)
	free(cx->lastMessage);
    cx->lastMessage = JS_strdup(cx, message);
    if (!cx->lastMessage)
	return;
    onError = cx->errorReporter;
    if (onError)
	(*onError)(cx, cx->lastMessage, reportp);
}

void
js_ReportIsNotDefined(JSContext *cx, const char *name)
{
    JS_ReportError(cx, "%s is not defined", name);
}

#if defined DEBUG && defined XP_UNIX
/* For gdb usage. */
void js_traceon(JSContext *cx)  { cx->tracefp = stderr; }
void js_traceoff(JSContext *cx) { cx->tracefp = NULL; }
#endif

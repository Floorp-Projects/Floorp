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
 * JS debugging API.
 */
#include <string.h>
#include "prtypes.h"
#include "prlog.h"
#include "prclist.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

typedef struct JSTrap {
    PRCList         links;
    JSScript        *script;
    jsbytecode      *pc;
    JSOp            op;
    JSTrapHandler   handler;
    void            *closure;
} JSTrap;

static JSTrap *
FindTrap(JSRuntime *rt, JSScript *script, jsbytecode *pc)
{
    JSTrap *trap;

    for (trap = (JSTrap *)rt->trapList.next;
	 trap != (JSTrap *)&rt->trapList;
	 trap = (JSTrap *)trap->links.next) {
	if (trap->script == script && trap->pc == pc)
	    return trap;
    }
    return NULL;
}

void
js_PatchOpcode(JSContext *cx, JSScript *script, jsbytecode *pc, JSOp op)
{
    JSTrap *trap;

    trap = FindTrap(cx->runtime, script, pc);
    if (trap)
	trap->op = op;
    else
	*pc = (jsbytecode)op;
}

JS_PUBLIC_API(JSBool)
JS_SetTrap(JSContext *cx, JSScript *script, jsbytecode *pc,
	   JSTrapHandler handler, void *closure)
{
    JSRuntime *rt;
    JSTrap *trap;

    rt = cx->runtime;
    trap = FindTrap(rt, script, pc);
    if (trap) {
	/* Restore opcode at pc so it can be saved again. */
	*pc = (jsbytecode)trap->op;
    } else {
	trap = JS_malloc(cx, sizeof *trap);
	if (!trap || !js_AddRoot(cx, &trap->closure, "trap->closure")) {
	    if (trap)
		JS_free(cx, trap);
	    return JS_FALSE;
	}
    }
    PR_APPEND_LINK(&trap->links, &rt->trapList);
    trap->script = script;
    trap->pc = pc;
    trap->op = *pc;
    trap->handler = handler;
    trap->closure = closure;
    *pc = JSOP_TRAP;
    return JS_TRUE;
}

JS_PUBLIC_API(JSOp)
JS_GetTrapOpcode(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    JSTrap *trap;

    trap = FindTrap(cx->runtime, script, pc);
    if (!trap) {
	PR_ASSERT(0);	/* XXX can't happen */
	return JSOP_LIMIT;
    }
    return trap->op;
}

static void
DestroyTrap(JSContext *cx, JSTrap *trap)
{
    PR_REMOVE_LINK(&trap->links);
    *trap->pc = (jsbytecode)trap->op;
    js_RemoveRoot(cx, &trap->closure);
    JS_free(cx, trap);
}

JS_PUBLIC_API(void)
JS_ClearTrap(JSContext *cx, JSScript *script, jsbytecode *pc,
	     JSTrapHandler *handlerp, void **closurep)
{
    JSTrap *trap;

    trap = FindTrap(cx->runtime, script, pc);
    if (handlerp)
	*handlerp = trap ? trap->handler : NULL;
    if (closurep)
	*closurep = trap ? trap->closure : NULL;
    if (trap)
	DestroyTrap(cx, trap);
}

JS_PUBLIC_API(void)
JS_ClearScriptTraps(JSContext *cx, JSScript *script)
{
    JSRuntime *rt;
    JSTrap *trap, *next;

    rt = cx->runtime;
    for (trap = (JSTrap *)rt->trapList.next;
	 trap != (JSTrap *)&rt->trapList;
	 trap = next) {
	next = (JSTrap *)trap->links.next;
	if (trap->script == script)
	    DestroyTrap(cx, trap);
    }
}

JS_PUBLIC_API(void)
JS_ClearAllTraps(JSContext *cx)
{
    JSRuntime *rt;
    JSTrap *trap, *next;

    rt = cx->runtime;
    for (trap = (JSTrap *)rt->trapList.next;
	 trap != (JSTrap *)&rt->trapList;
	 trap = next) {
	next = (JSTrap *)trap->links.next;
	DestroyTrap(cx, trap);
    }
}

JS_PUBLIC_API(JSTrapStatus)
JS_HandleTrap(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval)
{
    JSTrap *trap;
    JSTrapStatus status;
    jsint op;

    trap = FindTrap(cx->runtime, script, pc);
    if (!trap) {
	PR_ASSERT(0);	/* XXX can't happen */
	return JSTRAP_ERROR;
    }
    /*
     * It's important that we not use 'trap->' after calling the callback --
     * the callback might remove the trap!
     */
    op = (jsint)trap->op;
    status = trap->handler(cx, script, pc, rval, trap->closure);
    if (status == JSTRAP_CONTINUE) {
	/* By convention, return the true op to the interpreter in rval. */
	*rval = INT_TO_JSVAL(op);
    }
    return status;
}

JS_PUBLIC_API(JSBool)
JS_SetInterrupt(JSRuntime *rt, JSTrapHandler handler, void *closure)
{
    rt->interruptHandler = handler;
    rt->interruptHandlerData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ClearInterrupt(JSRuntime *rt, JSTrapHandler *handlerp, void **closurep)
{
    if (handlerp)
	*handlerp = (JSTrapHandler)rt->interruptHandler;
    if (closurep)
	*closurep = rt->interruptHandlerData;
    rt->interruptHandler = 0;
    rt->interruptHandlerData = 0;
    return JS_TRUE;
}


typedef struct JSWatchPoint {
    PRCList             links;
    JSObject            *object;	/* weak link, see js_FinalizeObject */
    jsval               userid;
    JSScopeProperty     *sprop;
    JSPropertyOp        setter;
    JSWatchPointHandler handler;
    void                *closure;
    jsrefcount          nrefs;
} JSWatchPoint;

#define HoldWatchPoint(wp) ((wp)->nrefs++)

static void
DropWatchPoint(JSContext *cx, JSWatchPoint *wp)
{
    if (--wp->nrefs != 0)
	return;
    wp->sprop->setter = wp->setter;
    JS_LOCK_OBJ_VOID(cx, wp->object,
		     js_DropScopeProperty(cx, (JSScope *)wp->object->map,
					  wp->sprop));
    PR_REMOVE_LINK(&wp->links);
    js_RemoveRoot(cx, &wp->closure);
    JS_free(cx, wp);
}

static JSWatchPoint *
FindWatchPoint(JSRuntime *rt, JSObject *obj, jsval userid)
{
    JSWatchPoint *wp;

    for (wp = (JSWatchPoint *)rt->watchPointList.next;
	 wp != (JSWatchPoint *)&rt->watchPointList;
	 wp = (JSWatchPoint *)wp->links.next) {
	if (wp->object == obj && wp->userid == userid)
	    return wp;
    }
    return NULL;
}

JSScopeProperty *
js_FindWatchPoint(JSRuntime *rt, JSObject *obj, jsval userid)
{
    JSWatchPoint *wp;

    wp = FindWatchPoint(rt, obj, userid);
    if (!wp)
	return NULL;
    return wp->sprop;
}

JSBool PR_CALLBACK
js_watch_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSRuntime *rt;
    JSWatchPoint *wp;
    JSScopeProperty *sprop;
    JSSymbol *sym;
    jsval userid, value;
    jsid symid;
    JSScope *scope;
    JSAtom *atom;
    JSBool ok;

    rt = cx->runtime;
    for (wp = (JSWatchPoint *)rt->watchPointList.next;
	 wp != (JSWatchPoint *)&rt->watchPointList;
	 wp = (JSWatchPoint *)wp->links.next) {
	sprop = wp->sprop;
	if (wp->object == obj && sprop->id == id) {
	    JS_LOCK_OBJ(cx, obj);
	    sym = sprop->symbols;
	    if (!sym) {
		userid = wp->userid;
		atom = NULL;
		if (JSVAL_IS_INT(userid)) {
		    symid = (jsid)userid;
		} else {
		    atom = js_ValueToStringAtom(cx, userid);
		    if (!atom) {
			JS_UNLOCK_OBJ(cx, obj);
			return JS_FALSE;
		    }
		    symid = (jsid)atom;
		}
		scope = (JSScope *) obj->map;
		PR_ASSERT(scope->props);
		ok = LOCKED_OBJ_GET_CLASS(obj)->addProperty(cx, obj, sprop->id,
							    &value);
		if (!ok) {
		    JS_UNLOCK_OBJ(cx, obj);
		    return JS_FALSE;
		}
		ok = (scope->ops->add(cx, scope, symid, sprop) != NULL);
		if (!ok) {
		    JS_UNLOCK_OBJ(cx, obj);
		    return JS_FALSE;
		}
		sym = sprop->symbols;
	    }
	    JS_UNLOCK_OBJ(cx, obj);
	    HoldWatchPoint(wp);
	    ok = wp->handler(cx, obj, js_IdToValue(sym_id(sym)),
			     OBJ_GET_SLOT(cx, obj, wp->sprop->slot), vp,
			     wp->closure);
	    if (ok)
		ok = wp->setter(cx, obj, id, vp);
	    DropWatchPoint(cx, wp);
	    return ok;
	}
    }
    PR_ASSERT(0);	/* XXX can't happen */
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetWatchPoint(JSContext *cx, JSObject *obj, jsval id,
		 JSWatchPointHandler handler, void *closure)
{
    JSAtom *atom;
    jsid symid;
    JSObject *pobj;
    JSScopeProperty *sprop;
    JSRuntime *rt;
    JSWatchPoint *wp;

    if (!OBJ_IS_NATIVE(obj)) {
	JS_ReportError(cx, "can't watch non-native objects of class %s",
		       OBJ_GET_CLASS(cx, obj)->name);
	return JS_FALSE;
    }

    if (JSVAL_IS_INT(id)) {
	symid = (jsid)id;
	atom = NULL;
    } else {
	atom = js_ValueToStringAtom(cx, id);
	if (!atom)
	    return JS_FALSE;
	symid = (jsid)atom;
    }

    if (!js_LookupProperty(cx, obj, symid, &pobj, (JSProperty **)&sprop))
	return JS_FALSE;
    rt = cx->runtime;
    if (!sprop) {
	/* Check for a deleted symbol watchpoint, which holds its property. */
	sprop = js_FindWatchPoint(rt, obj, id);
	if (sprop) {
#ifdef JS_THREADSAFE
	    /* Emulate js_LookupProperty if thread-safe. */
	    JS_LOCK_OBJ(cx, obj);
	    sprop->nrefs++;
#endif
	} else {
	    /* Make a new property in obj so we can watch for the first set. */
	    if (!js_DefineProperty(cx, obj, symid, JSVAL_VOID, NULL, NULL, 0,
				   (JSProperty **)&sprop)) {
		sprop = NULL;
	    }
	}
    } else if (pobj != obj) {
	/* Clone the prototype property so we can watch the right object. */
	jsval value;
	JSPropertyOp getter, setter;
	uintN attrs;

	if (OBJ_IS_NATIVE(pobj)) {
	    value = LOCKED_OBJ_GET_SLOT(pobj, sprop->slot);
	} else {
	    if (!OBJ_GET_PROPERTY(cx, pobj, id, &value)) {
		OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
		return JS_FALSE;
	    }
	}
	getter = sprop->getter;
	setter = sprop->setter;
	attrs = sprop->attrs;
	OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);

	if (!js_DefineProperty(cx, obj, symid, value, getter, setter, attrs,
			       (JSProperty **)&sprop)) {
	    sprop = NULL;
	}
    }
    if (!sprop)
	return JS_FALSE;

    wp = FindWatchPoint(rt, obj, id);
    if (!wp) {
	wp = JS_malloc(cx, sizeof *wp);
	if (!wp)
	    return JS_FALSE;
	if (!js_AddRoot(cx, &wp->closure, "wp->closure")) {
	    JS_free(cx, wp);
	    return JS_FALSE;
	}
	PR_APPEND_LINK(&wp->links, &rt->watchPointList);
	wp->object = obj;
	wp->userid = id;
	wp->sprop = js_HoldScopeProperty(cx, (JSScope *)obj->map, sprop);
	wp->setter = sprop->setter;
	sprop->setter = js_watch_set;
	wp->nrefs = 1;
    }
    wp->handler = handler;
    wp->closure = closure;
    OBJ_DROP_PROPERTY(cx, obj, (JSProperty *)sprop);
    return JS_TRUE;
}

JS_PUBLIC_API(void)
JS_ClearWatchPoint(JSContext *cx, JSObject *obj, jsval id,
		   JSWatchPointHandler *handlerp, void **closurep)
{
    JSRuntime *rt;
    JSWatchPoint *wp;

    rt = cx->runtime;
    for (wp = (JSWatchPoint *)rt->watchPointList.next;
	 wp != (JSWatchPoint *)&rt->watchPointList;
	 wp = (JSWatchPoint *)wp->links.next) {
	if (wp->object == obj && wp->userid == id) {
	    if (handlerp)
		*handlerp = wp->handler;
	    if (closurep)
		*closurep = wp->closure;
	    DropWatchPoint(cx, wp);
	    return;
	}
    }
    if (handlerp)
	*handlerp = NULL;
    if (closurep)
	*closurep = NULL;
}

JS_PUBLIC_API(void)
JS_ClearWatchPointsForObject(JSContext *cx, JSObject *obj)
{
    JSRuntime *rt;
    JSWatchPoint *wp, *next;

    rt = cx->runtime;
    for (wp = (JSWatchPoint *)rt->watchPointList.next;
	 wp != (JSWatchPoint *)&rt->watchPointList;
	 wp = next) {
	next = (JSWatchPoint *)wp->links.next;
	if (wp->object == obj)
	    DropWatchPoint(cx, wp);
    }
}

JS_PUBLIC_API(void)
JS_ClearAllWatchPoints(JSContext *cx)
{
    JSRuntime *rt;
    JSWatchPoint *wp, *next;

    rt = cx->runtime;
    for (wp = (JSWatchPoint *)rt->watchPointList.next;
	 wp != (JSWatchPoint *)&rt->watchPointList;
	 wp = next) {
	next = (JSWatchPoint *)wp->links.next;
	DropWatchPoint(cx, wp);
    }
}

JS_PUBLIC_API(uintN)
JS_PCToLineNumber(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    return js_PCToLineNumber(script, pc);
}

JS_PUBLIC_API(jsbytecode *)
JS_LineNumberToPC(JSContext *cx, JSScript *script, uintN lineno)
{
    return js_LineNumberToPC(script, lineno);
}

JS_PUBLIC_API(JSScript *)
JS_GetFunctionScript(JSContext *cx, JSFunction *fun)
{
    return fun->script;
}

JS_PUBLIC_API(JSPrincipals *)
JS_GetScriptPrincipals(JSContext *cx, JSScript *script)
{
    return script->principals;
}


/*
 *  Stack Frame Iterator
 */
JS_PUBLIC_API(JSStackFrame *)
JS_FrameIterator(JSContext *cx, JSStackFrame **iteratorp)
{
    *iteratorp = (*iteratorp == NULL) ? cx->fp : (*iteratorp)->down;
    return *iteratorp;
}

JS_PUBLIC_API(JSScript *)
JS_GetFrameScript(JSContext *cx, JSStackFrame *fp)
{
    return fp->script;
}

JS_PUBLIC_API(jsbytecode *)
JS_GetFramePC(JSContext *cx, JSStackFrame *fp)
{
    return fp->pc;
}

JS_PUBLIC_API(void *)
JS_GetFrameAnnotation(JSContext *cx, JSStackFrame *fp)
{
    if (fp->annotation) {
	JSPrincipals *principals = fp->script
	    ? fp->script->principals
	    : NULL;

	if (principals == NULL)
	    return NULL;

	if (principals->globalPrivilegesEnabled(cx, principals)) {
	    /*
	     * Only give out an annotation if privileges have not
	     * been revoked globally.
	     */
	    return fp->annotation;
	}
    }

    return NULL;
}

JS_PUBLIC_API(void)
JS_SetFrameAnnotation(JSContext *cx, JSStackFrame *fp, void *annotation)
{
    fp->annotation = annotation;
}

JS_PUBLIC_API(void *)
JS_GetFramePrincipalArray(JSContext *cx, JSStackFrame *fp)
{
    JSPrincipals *principals = fp->script
	? fp->script->principals
	: NULL;

    return principals
	? principals->getPrincipalArray(cx, principals)
	: NULL;
}

JS_PUBLIC_API(JSBool)
JS_IsNativeFrame(JSContext *cx, JSStackFrame *fp)
{
    return fp->fun && fp->fun->call;
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameObject(JSContext *cx, JSStackFrame *fp)
{
    return fp->scopeChain;
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameThis(JSContext *cx, JSStackFrame *fp)
{
    return fp->thisp;
}

JS_PUBLIC_API(JSFunction *)
JS_GetFrameFunction(JSContext *cx, JSStackFrame *fp)
{
    return fp->fun;
}

/************************************************************************/

JS_PUBLIC_API(const char *)
JS_GetScriptFilename(JSContext *cx, JSScript *script)
{
    return script->filename;
}

JS_PUBLIC_API(uintN)
JS_GetScriptBaseLineNumber(JSContext *cx, JSScript *script)
{
    return script->lineno;
}

JS_PUBLIC_API(uintN)
JS_GetScriptLineExtent(JSContext *cx, JSScript *script)
{
    return js_GetScriptLineExtent(script);
}

/***************************************************************************/

JS_PUBLIC_API(void)
JS_SetNewScriptHook(JSRuntime *rt, JSNewScriptHook hook, void *callerdata)
{
    rt->newScriptHook = hook;
    rt->newScriptHookData = callerdata;
}

JS_PUBLIC_API(void)
JS_SetDestroyScriptHook(JSRuntime *rt, JSDestroyScriptHook hook,
			void *callerdata)
{
    rt->destroyScriptHook = hook;
    rt->destroyScriptHookData = callerdata;
}

/***************************************************************************/

JS_PUBLIC_API(JSBool)
JS_EvaluateInStackFrame(JSContext *cx, JSStackFrame *fp,
			const char *bytes, uintN length,
			const char *filename, uintN lineno,
			jsval *rval)
{
    JSScript *script;
    JSBool ok;

    script = JS_CompileScriptForPrincipals(cx, fp->scopeChain,
					   fp->script ? fp->script->principals
						      : NULL,
					   bytes, length, filename, lineno);
    if (!script)
	return JS_FALSE;
    ok = js_Execute(cx, fp->scopeChain, script, fp->fun, fp, JS_TRUE, rval);
    js_DestroyScript(cx, script);
    return ok;
}

/************************************************************************/

JS_PUBLIC_API(JSScopeProperty *)
JS_PropertyIterator(JSObject *obj, JSScopeProperty **iteratorp)
{
    JSScopeProperty *sprop;
    JSScope *scope;

    sprop = *iteratorp;
    scope = (JSScope *) obj->map;
    sprop = (sprop == NULL) ? scope->props : sprop->next;
    *iteratorp = sprop;
    return sprop;
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyDesc(JSContext *cx, JSObject *obj, JSScopeProperty *sprop,
		   JSPropertyDesc *pd)
{
    JSSymbol *sym;

    sym = sprop->symbols;
    pd->id = sym ? js_IdToValue(sym_id(sym)) : JSVAL_VOID;
    pd->value = OBJ_GET_SLOT(cx, obj, sprop->slot);
    pd->flags = ((sprop->attrs & JSPROP_ENUMERATE)      ? JSPD_ENUMERATE : 0)
	      | ((sprop->attrs & JSPROP_READONLY)       ? JSPD_READONLY  : 0)
	      | ((sprop->attrs & JSPROP_PERMANENT)      ? JSPD_PERMANENT : 0)
	      | ((sprop->getter == js_GetArgument)      ? JSPD_ARGUMENT  : 0)
	      | ((sprop->getter == js_GetLocalVariable) ? JSPD_VARIABLE  : 0);
    pd->spare = 0;
    pd->slot = (pd->flags & (JSPD_ARGUMENT | JSPD_VARIABLE))
	       ? JSVAL_TO_INT(sprop->id)
	       : 0;
    if (!sym || !sym->next || (pd->flags & (JSPD_ARGUMENT | JSPD_VARIABLE))) {
	pd->alias = JSVAL_VOID;
    } else {
	pd->alias = js_IdToValue(sym_id(sym->next));
	pd->flags |= JSPD_ALIAS;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyDescArray(JSContext *cx, JSObject *obj, JSPropertyDescArray *pda)
{
    JSScope *scope;
    uint32 i, n;
    JSPropertyDesc *pd;
    JSScopeProperty *sprop;

    if (!OBJ_GET_CLASS(cx, obj)->enumerate(cx, obj))
	return JS_FALSE;
    scope = (JSScope *)obj->map;
    if (!scope->props) {
	pda->length = 0;
	pda->array = NULL;
	return JS_TRUE;
    }
    n = scope->map.freeslot;
    pd = JS_malloc(cx, (size_t)n * sizeof(JSPropertyDesc));
    if (!pd)
	return JS_FALSE;
    i = 0;
    for (sprop = scope->props; sprop; sprop = sprop->next) {
	if (!js_AddRoot(cx, &pd[i].id, NULL))
	    goto bad;
	if (!js_AddRoot(cx, &pd[i].value, NULL))
	    goto bad;
	JS_GetPropertyDesc(cx, obj, sprop, &pd[i]);
	if ((pd[i].flags & JSPD_ALIAS) && !js_AddRoot(cx, &pd[i].alias, NULL))
	    goto bad;
	if (++i == n)
	    break;
    }
    pda->length = i;
    pda->array = pd;
    return JS_TRUE;

bad:
    pda->length = i + 1;
    pda->array = pd;
    JS_PutPropertyDescArray(cx, pda);
    return JS_FALSE;
}

JS_PUBLIC_API(void)
JS_PutPropertyDescArray(JSContext *cx, JSPropertyDescArray *pda)
{
    JSPropertyDesc *pd;
    uint32 i;

    pd = pda->array;
    for (i = 0; i < pda->length; i++) {
	js_RemoveRoot(cx, &pd[i].id);
	js_RemoveRoot(cx, &pd[i].value);
	if (pd[i].flags & JSPD_ALIAS)
	    js_RemoveRoot(cx, &pd[i].alias);
    }
    JS_free(cx, pd);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/*
 * JS debugging API.
 */
#include "jsstddef.h"
#include <string.h>
#include "jstypes.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsclist.h"
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
    JSCList         links;
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
        trap = (JSTrap *) JS_malloc(cx, sizeof *trap);
        if (!trap || !js_AddRoot(cx, &trap->closure, "trap->closure")) {
            if (trap)
                JS_free(cx, trap);
            return JS_FALSE;
        }
    }
    JS_APPEND_LINK(&trap->links, &rt->trapList);
    trap->script = script;
    trap->pc = pc;
    trap->op = (JSOp)*pc;
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
        JS_ASSERT(0);   /* XXX can't happen */
        return JSOP_LIMIT;
    }
    return trap->op;
}

static void
DestroyTrap(JSContext *cx, JSTrap *trap)
{
    JS_REMOVE_LINK(&trap->links);
    *trap->pc = (jsbytecode)trap->op;
    js_RemoveRoot(cx->runtime, &trap->closure);
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
        JS_ASSERT(0);   /* XXX can't happen */
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
    JSCList             links;
    JSObject            *object;        /* weak link, see js_FinalizeObject */
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
    SPROP_SETTER(wp->sprop, wp->object) = wp->setter;
    JS_LOCK_OBJ_VOID(cx, wp->object,
                     js_DropScopeProperty(cx, OBJ_SCOPE(wp->object),
                                          wp->sprop));
    JS_REMOVE_LINK(&wp->links);
    js_RemoveRoot(cx->runtime, &wp->closure);
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

JSBool JS_DLL_CALLBACK
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
                scope = OBJ_SCOPE(obj);
                JS_ASSERT(scope->props);
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
                             (SPROP_HAS_VALID_SLOT(sprop))
                             ? OBJ_GET_SLOT(cx, obj, wp->sprop->slot)
                             : JSVAL_VOID,
                             vp, wp->closure);
            if (ok) {
                /*
                 * Create pseudo-frame for call to setter so that any 
                 * stackwalking security code in the setter will correctly
                 * identify the guilty party.
                 */
                JSObject *funobj = (JSObject *) wp->closure;
                JSFunction *fun = (JSFunction *) JS_GetPrivate(cx, funobj);
                JSStackFrame frame;
                memset(&frame, 0, sizeof(frame));
                frame.script = fun->script;
                frame.fun = fun;
                frame.down = cx->fp;
                cx->fp = &frame;
                ok = wp->setter(cx, obj, id, vp);
                cx->fp = frame.down;
            }
            DropWatchPoint(cx, wp);
            return ok;
        }
    }
    JS_ASSERT(0);       /* XXX can't happen */
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
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_WATCH,
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
            value = (SPROP_HAS_VALID_SLOT(sprop))
                    ? LOCKED_OBJ_GET_SLOT(pobj, sprop->slot)
                    : JSVAL_VOID;
        } else {
            if (!OBJ_GET_PROPERTY(cx, pobj, id, &value)) {
                OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
                return JS_FALSE;
            }
        }
        getter = SPROP_GETTER(sprop, pobj);
        setter = SPROP_SETTER(sprop, pobj);
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
        wp = (JSWatchPoint *) JS_malloc(cx, sizeof *wp);
        if (!wp)
            return JS_FALSE;
        if (!js_AddRoot(cx, &wp->closure, "wp->closure")) {
            JS_free(cx, wp);
            return JS_FALSE;
        }
        JS_APPEND_LINK(&wp->links, &rt->watchPointList);
        wp->object = obj;
        wp->userid = id;
        wp->sprop = js_HoldScopeProperty(cx, OBJ_SCOPE(obj), sprop);
        wp->setter = SPROP_SETTER(sprop, obj);
        SPROP_SETTER(sprop, obj) = js_watch_set;
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
    return fp->fun && fp->fun->native;
}

/* this is deprecated, use JS_GetFrameScopeChain instead */
JS_PUBLIC_API(JSObject *)
JS_GetFrameObject(JSContext *cx, JSStackFrame *fp)
{
    return fp->scopeChain;
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameScopeChain(JSContext *cx, JSStackFrame *fp)
{
    /* Force creation of argument and call objects if not yet created */
    (void) JS_GetFrameCallObject(cx, fp);
    return fp->scopeChain;
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameCallObject(JSContext *cx, JSStackFrame *fp)
{
    if (! fp->fun)
        return NULL;
#if JS_HAS_ARGS_OBJECT
    /* Force creation of argument object if not yet created */
    (void) js_GetArgsObject(cx, fp);
#endif
#if JS_HAS_CALL_OBJECT
    /*
     * XXX ill-defined: null return here means error was reported, unlike a
     *     null returned above or in the #else
     */
    return js_GetCallObject(cx, fp, NULL);
#else
    return NULL;
#endif /* JS_HAS_CALL_OBJECT */
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

JS_PUBLIC_API(JSObject *)
JS_GetFrameFunctionObject(JSContext *cx, JSStackFrame *fp)
{
    return fp->argv && fp->fun ? JSVAL_TO_OBJECT(fp->argv[-2]) : NULL;
}

JS_PUBLIC_API(JSBool)
JS_IsContructorFrame(JSContext *cx, JSStackFrame *fp)
{
    return fp->constructing;
}

JS_PUBLIC_API(JSBool)
JS_IsDebuggerFrame(JSContext *cx, JSStackFrame *fp)
{
    return fp->special & JSFRAME_DEBUGGER;
}

JS_PUBLIC_API(jsval)
JS_GetFrameReturnValue(JSContext *cx, JSStackFrame *fp)
{
    return fp->rval;
}

JS_PUBLIC_API(void)
JS_SetFrameReturnValue(JSContext *cx, JSStackFrame *fp, jsval rval)
{
    fp->rval = rval;
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
JS_EvaluateUCInStackFrame(JSContext *cx, JSStackFrame *fp,
                          const jschar *bytes, uintN length,
                          const char *filename, uintN lineno,
                          jsval *rval)
{
    JSScript *script;
    JSBool ok;

    script = JS_CompileUCScriptForPrincipals(cx, fp->scopeChain,
                                             fp->script ? fp->script->principals
                                             : NULL,
                                             bytes, length, filename, lineno);
    if (!script)
        return JS_FALSE;
    ok = js_Execute(cx, fp->scopeChain, script, fp,
                    JSFRAME_DEBUGGER | JSFRAME_EVAL, rval);
    js_DestroyScript(cx, script);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateInStackFrame(JSContext *cx, JSStackFrame *fp,
                        const char *bytes, uintN length,
                        const char *filename, uintN lineno,
                        jsval *rval)
{
    jschar *chars;
    JSBool ok;

    chars = js_InflateString(cx, bytes, length);
    if (!chars)
        return JS_FALSE;
    ok = JS_EvaluateUCInStackFrame(cx, fp, chars, length, filename, lineno,
                                   rval);
    JS_free(cx, chars);

    return ok;
}

/************************************************************************/

/* XXXbe this all needs to be reworked to avoid requiring JSScope types. */

JS_PUBLIC_API(JSScopeProperty *)
JS_PropertyIterator(JSObject *obj, JSScopeProperty **iteratorp)
{
    JSScopeProperty *sprop;
    JSScope *scope;

    sprop = *iteratorp;
    scope = OBJ_SCOPE(obj);
    sprop = (sprop == NULL) ? scope->props : sprop->next;
    *iteratorp = sprop;
    return sprop;
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyDesc(JSContext *cx, JSObject *obj, JSScopeProperty *sprop,
                   JSPropertyDesc *pd)
{
    JSSymbol *sym;
    JSPropertyOp getter;

    sym = sprop->symbols;
    pd->id = sym ? js_IdToValue(sym_id(sym)) : JSVAL_VOID;
    if (!sym || !js_GetProperty(cx, obj, sym_id(sym), &pd->value)) {
        pd->value = (SPROP_HAS_VALID_SLOT(sprop))
                    ? OBJ_GET_SLOT(cx, obj, sprop->slot)
                    : JSVAL_VOID;
    }
    getter = SPROP_GETTER(sprop, obj);
    pd->flags = ((sprop->attrs & JSPROP_ENUMERATE)      ? JSPD_ENUMERATE : 0)
              | ((sprop->attrs & JSPROP_READONLY)       ? JSPD_READONLY  : 0)
              | ((sprop->attrs & JSPROP_PERMANENT)      ? JSPD_PERMANENT : 0)
#if JS_HAS_CALL_OBJECT
              | ((getter == js_GetCallVariable)  ? JSPD_VARIABLE  : 0)
#endif /* JS_HAS_CALL_OBJECT */
              | ((getter == js_GetArgument)      ? JSPD_ARGUMENT  : 0)
              | ((getter == js_GetLocalVariable) ? JSPD_VARIABLE  : 0);
#if JS_HAS_CALL_OBJECT
    /* for Call Object 'real' getter isn't passed in to us */
    if (OBJ_GET_CLASS(cx, obj) == &js_CallClass &&
        getter == js_CallClass.getProperty) {
        /*
         * Property of a heavyweight function's variable object having the
         * class-default getter.  It's either an argument if permanent, or a
         * nested function if impermanent.  Local variables have a special
         * getter (js_GetCallVariable, tested above) and setter, and not the
         * class default.
         */
        pd->flags |= (sprop->attrs & JSPROP_PERMANENT)
                     ? JSPD_ARGUMENT
                     : JSPD_VARIABLE;
    }
#endif /* JS_HAS_CALL_OBJECT */
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
    JSClass *clasp;
    JSScope *scope;
    uint32 i, n;
    JSPropertyDesc *pd;
    JSScopeProperty *sprop;

    clasp = OBJ_GET_CLASS(cx, obj);
    if (!OBJ_IS_NATIVE(obj) || (clasp->flags & JSCLASS_NEW_ENUMERATE)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_CANT_DESCRIBE_PROPS, clasp->name);
        return JS_FALSE;
    }
    if (!clasp->enumerate(cx, obj))
        return JS_FALSE;

    /* have no props, or object's scope has not mutated from that of proto */
    scope = OBJ_SCOPE(obj);
    if (!scope->props ||
        (OBJ_GET_PROTO(cx,obj) && scope == OBJ_SCOPE(OBJ_GET_PROTO(cx,obj)))) {
        pda->length = 0;
        pda->array = NULL;
        return JS_TRUE;
    }

    n = JS_MIN(scope->map.freeslot, scope->map.nslots);
    pd = (JSPropertyDesc *) JS_malloc(cx, (size_t)n * sizeof(JSPropertyDesc));
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
        js_RemoveRoot(cx->runtime, &pd[i].id);
        js_RemoveRoot(cx->runtime, &pd[i].value);
        if (pd[i].flags & JSPD_ALIAS)
            js_RemoveRoot(cx->runtime, &pd[i].alias);
    }
    JS_free(cx, pd);
}

/************************************************************************/

JS_PUBLIC_API(JSBool)
JS_SetDebuggerHandler(JSRuntime *rt, JSTrapHandler handler, void *closure)
{
    rt->debuggerHandler = handler;
    rt->debuggerHandlerData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetSourceHandler(JSRuntime *rt, JSSourceHandler handler, void *closure)
{
    rt->sourceHandler = handler;
    rt->sourceHandlerData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetExecuteHook(JSRuntime *rt, JSInterpreterHook hook, void *closure)
{
    rt->executeHook = hook;
    rt->executeHookData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetCallHook(JSRuntime *rt, JSInterpreterHook hook, void *closure)
{
    rt->callHook = hook;
    rt->callHookData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetObjectHook(JSRuntime *rt, JSObjectHook hook, void *closure)
{
    rt->objectHook = hook;
    rt->objectHookData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetThrowHook(JSRuntime *rt, JSTrapHandler hook, void *closure)
{
    rt->throwHook = hook;
    rt->throwHookData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetDebugErrorHook(JSRuntime *rt, JSDebugErrorHook hook, void *closure)
{
    rt->debugErrorHook = hook;
    rt->debugErrorHookData = closure;
    return JS_TRUE;
}

/************************************************************************/

extern JS_FRIEND_DATA(JSScopeOps) js_list_scope_ops;

static size_t
GetSymbolTotalSize(JSContext *cx, JSSymbol *sym)
{
    JSScopeProperty *sprop;
    size_t nbytes;

    sprop = sym_property(sym);
    nbytes = sizeof *sprop;

    /* XXX don't count sprop->id, assume it's shared */

    if (sprop->attrs & JSPROP_GETTER)
        nbytes += JS_GetObjectTotalSize(cx, (JSObject *) sprop->getter);
    if (sprop->attrs & JSPROP_SETTER)
        nbytes += JS_GetObjectTotalSize(cx, (JSObject *) sprop->setter);

    /* XXX don't count sym_id, assume it's shared */
    /* XXX don't worry about aliases (extra symbols for an sprop) */
    nbytes += sizeof *sym;
    return nbytes;
}

typedef struct SymbolEnumArgs {
    JSContext *cx;
    size_t nbytes;
} SymbolEnumArgs;

static intN
SymbolEnumerator(JSHashEntry *he, intN i, void *arg)
{
    JSSymbol *sym = (JSSymbol *) he;
    SymbolEnumArgs *args = (SymbolEnumArgs *) arg;

    args->nbytes += GetSymbolTotalSize(args->cx, sym);
    return HT_ENUMERATE_NEXT;
}

JS_PUBLIC_API(size_t)
JS_GetObjectTotalSize(JSContext *cx, JSObject *obj)
{
    size_t nbytes;
    JSScope *scope;
    JSSymbol *sym;
    JSHashTable *table;
    SymbolEnumArgs args;

    nbytes = sizeof *obj + obj->map->nslots * sizeof obj->slots[0];
    if (OBJ_IS_NATIVE(obj)) {
        scope = OBJ_SCOPE(obj);
        if (scope->object == obj) {
            nbytes += sizeof *scope;
            if (scope->ops == &js_list_scope_ops) {
                for (sym = scope->data; sym; sym = (JSSymbol *) sym->entry.next)
                    nbytes += GetSymbolTotalSize(cx, sym);
            } else {
                table = scope->data;
                nbytes += sizeof *table;
                nbytes += JS_BIT(JS_HASH_BITS - table->shift)
                          * sizeof table->buckets[0];
                args.cx = cx;
                args.nbytes = 0;
                JS_HashTableEnumerateEntries(table, SymbolEnumerator, &args);
                nbytes += args.nbytes;
            }
        }
    }
    return nbytes;
}

static size_t
GetAtomTotalSize(JSContext *cx, JSAtom *atom)
{
    size_t nbytes;

    nbytes = sizeof *atom;
    if (ATOM_IS_STRING(atom)) {
        nbytes += sizeof(JSString);
        nbytes += (ATOM_TO_STRING(atom)->length + 1) * sizeof(jschar);
    } else if (ATOM_IS_DOUBLE(atom)) {
        nbytes += sizeof(jsdouble);
        nbytes += sizeof *ATOM_TO_DOUBLE(atom);
    } else if (ATOM_IS_OBJECT(atom)) {
        nbytes += JS_GetObjectTotalSize(cx, ATOM_TO_OBJECT(atom));
    }
    return nbytes;
}

JS_PUBLIC_API(size_t)
JS_GetFunctionTotalSize(JSContext *cx, JSFunction *fun)
{
    size_t nbytes, obytes;
    JSObject *obj;
    JSAtom *atom;

    nbytes = sizeof *fun;
    JS_ASSERT(fun->nrefs);
    obj = fun->object;
    if (obj) {
        obytes = JS_GetObjectTotalSize(cx, obj);
        if (fun->nrefs > 1)
            obytes = (obytes + fun->nrefs - 1) / fun->nrefs;
        nbytes += obytes;
    }
    if (fun->script)
        nbytes += JS_GetScriptTotalSize(cx, fun->script);
    atom = fun->atom;
    if (atom)
        nbytes += GetAtomTotalSize(cx, atom);
    return nbytes;
}

#include "jsemit.h"

JS_PUBLIC_API(size_t)
JS_GetScriptTotalSize(JSContext *cx, JSScript *script)
{
    size_t nbytes, pbytes;
    JSObject *obj;
    jsatomid i;
    jssrcnote *sn, *notes;
    JSTryNote *tn, *tnotes;
    JSPrincipals *principals;

    nbytes = sizeof *script;
    obj = script->object;
    if (obj)
        nbytes += JS_GetObjectTotalSize(cx, obj);

    nbytes += script->length * sizeof script->code[0];
    nbytes += script->atomMap.length * sizeof script->atomMap.vector[0];
    for (i = 0; i < script->atomMap.length; i++)
        nbytes += GetAtomTotalSize(cx, script->atomMap.vector[i]);

    if (script->filename)
        nbytes += strlen(script->filename) + 1;

    notes = script->notes;
    if (notes) {
        for (sn = notes; !SN_IS_TERMINATOR(sn); sn += SN_LENGTH(sn))
            continue;
        nbytes += (sn - notes + 1) * sizeof *sn;
    }

    tnotes = script->trynotes;
    if (tnotes) {
        for (tn = tnotes; tn->catchStart; tn++)
            continue;
        nbytes += (tn - tnotes + 1) * sizeof *tn;
    }

    principals = script->principals;
    if (principals) {
        JS_ASSERT(principals->refcount);
        pbytes = sizeof *principals;
        if (principals->refcount > 1)
            pbytes = (pbytes + principals->refcount - 1) / principals->refcount;
        nbytes += pbytes;
    }

    return nbytes;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * JS debugging API.
 */
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsclist.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstaticcheck.h"
#include "jsstr.h"

#include "jsatominlines.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"

#include "jsautooplen.h"

#include "methodjit/MethodJIT.h"
#include "methodjit/Retcon.h"

using namespace js;
using namespace js::gc;

typedef struct JSTrap {
    JSCList         links;
    JSScript        *script;
    jsbytecode      *pc;
    JSOp            op;
    JSTrapHandler   handler;
    jsval           closure;
} JSTrap;

#define DBG_LOCK(rt)            JS_ACQUIRE_LOCK((rt)->debuggerLock)
#define DBG_UNLOCK(rt)          JS_RELEASE_LOCK((rt)->debuggerLock)
#define DBG_LOCK_EVAL(rt,expr)  (DBG_LOCK(rt), (expr), DBG_UNLOCK(rt))

JS_PUBLIC_API(JSBool)
JS_GetDebugMode(JSContext *cx)
{
    return cx->compartment->debugMode;
}

#ifdef JS_METHODJIT
static bool
IsScriptLive(JSContext *cx, JSScript *script)
{
    for (AllFramesIter i(cx); !i.done(); ++i) {
        if (i.fp()->maybeScript() == script)
            return true;
    }
    return false;
}
#endif

JS_FRIEND_API(JSBool)
js_SetDebugMode(JSContext *cx, JSBool debug)
{
    cx->compartment->debugMode = debug;
#ifdef JS_METHODJIT
    for (JSScript *script = (JSScript *)cx->compartment->scripts.next;
         &script->links != &cx->compartment->scripts;
         script = (JSScript *)script->links.next) {
        if (script->debugMode != debug &&
            script->ncode &&
            script->ncode != JS_UNJITTABLE_METHOD &&
            !IsScriptLive(cx, script)) {
            /*
             * In the event that this fails, debug mode is left partially on,
             * leading to a small performance overhead but no loss of
             * correctness. We set the debug flag to false so that the caller
             * will not later attempt to use debugging features.
             */
            mjit::Recompiler recompiler(cx, script);
            if (!recompiler.recompile()) {
                cx->compartment->debugMode = JS_FALSE;
                return JS_FALSE;
            }
        }
    }
#endif
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetDebugMode(JSContext *cx, JSBool debug)
{
#ifdef DEBUG
    for (AllFramesIter i(cx); !i.done(); ++i)
        JS_ASSERT(!JS_IsScriptFrame(cx, i.fp()));
#endif

    return js_SetDebugMode(cx, debug);
}

static JSBool
CheckDebugMode(JSContext *cx)
{
    JSBool debugMode = JS_GetDebugMode(cx);
    /*
     * :TODO:
     * This probably should be an assertion, since it's indicative of a severe
     * API misuse.
     */
    if (!debugMode) {
        JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage,
                                     NULL, JSMSG_NEED_DEBUG_MODE);
    }
    return debugMode;
}

/*
 * NB: FindTrap must be called with rt->debuggerLock acquired.
 */
static JSTrap *
FindTrap(JSRuntime *rt, JSScript *script, jsbytecode *pc)
{
    JSTrap *trap;

    for (trap = (JSTrap *)rt->trapList.next;
         &trap->links != &rt->trapList;
         trap = (JSTrap *)trap->links.next) {
        if (trap->script == script && trap->pc == pc)
            return trap;
    }
    return NULL;
}

jsbytecode *
js_UntrapScriptCode(JSContext *cx, JSScript *script)
{
    jsbytecode *code;
    JSRuntime *rt;
    JSTrap *trap;

    code = script->code;
    rt = cx->runtime;
    DBG_LOCK(rt);
    for (trap = (JSTrap *)rt->trapList.next;
         &trap->links !=
                &rt->trapList;
         trap = (JSTrap *)trap->links.next) {
        if (trap->script == script &&
            (size_t)(trap->pc - script->code) < script->length) {
            if (code == script->code) {
                jssrcnote *sn, *notes;
                size_t nbytes;

                nbytes = script->length * sizeof(jsbytecode);
                notes = script->notes();
                for (sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn))
                    continue;
                nbytes += (sn - notes + 1) * sizeof *sn;

                code = (jsbytecode *) cx->malloc(nbytes);
                if (!code)
                    break;
                memcpy(code, script->code, nbytes);
                JS_PURGE_GSN_CACHE(cx);
            }
            code[trap->pc - script->code] = trap->op;
        }
    }
    DBG_UNLOCK(rt);
    return code;
}

JS_PUBLIC_API(JSBool)
JS_SetTrap(JSContext *cx, JSScript *script, jsbytecode *pc,
           JSTrapHandler handler, jsval closure)
{
    JSTrap *junk, *trap, *twin;
    JSRuntime *rt;
    uint32 sample;

    if (!CheckDebugMode(cx))
        return JS_FALSE;

    if (script == JSScript::emptyScript()) {
        JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage,
                                     NULL, JSMSG_READ_ONLY, "empty script");
        return JS_FALSE;
    }

    JS_ASSERT((JSOp) *pc != JSOP_TRAP);
    junk = NULL;
    rt = cx->runtime;
    DBG_LOCK(rt);
    trap = FindTrap(rt, script, pc);
    if (trap) {
        JS_ASSERT(trap->script == script && trap->pc == pc);
        JS_ASSERT(*pc == JSOP_TRAP);
    } else {
        sample = rt->debuggerMutations;
        DBG_UNLOCK(rt);
        trap = (JSTrap *) cx->malloc(sizeof *trap);
        if (!trap)
            return JS_FALSE;
        trap->closure = JSVAL_NULL;
        DBG_LOCK(rt);
        twin = (rt->debuggerMutations != sample)
               ? FindTrap(rt, script, pc)
               : NULL;
        if (twin) {
            junk = trap;
            trap = twin;
        } else {
            JS_APPEND_LINK(&trap->links, &rt->trapList);
            ++rt->debuggerMutations;
            trap->script = script;
            trap->pc = pc;
            trap->op = (JSOp)*pc;
            *pc = JSOP_TRAP;
        }
    }
    trap->handler = handler;
    trap->closure = closure;
    DBG_UNLOCK(rt);
    if (junk)
        cx->free(junk);

#ifdef JS_METHODJIT
    if (script->ncode != NULL && script->ncode != JS_UNJITTABLE_METHOD) {
        mjit::Recompiler recompiler(cx, script);
        if (!recompiler.recompile())
            return JS_FALSE;
    }
#endif

    return JS_TRUE;
}

JS_PUBLIC_API(JSOp)
JS_GetTrapOpcode(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    JSRuntime *rt;
    JSTrap *trap;
    JSOp op;

    rt = cx->runtime;
    DBG_LOCK(rt);
    trap = FindTrap(rt, script, pc);
    op = trap ? trap->op : (JSOp) *pc;
    DBG_UNLOCK(rt);
    return op;
}

static void
DestroyTrapAndUnlock(JSContext *cx, JSTrap *trap)
{
    ++cx->runtime->debuggerMutations;
    JS_REMOVE_LINK(&trap->links);
    *trap->pc = (jsbytecode)trap->op;
    DBG_UNLOCK(cx->runtime);
    cx->free(trap);
}

JS_PUBLIC_API(void)
JS_ClearTrap(JSContext *cx, JSScript *script, jsbytecode *pc,
             JSTrapHandler *handlerp, jsval *closurep)
{
    JSTrap *trap;
    
    DBG_LOCK(cx->runtime);
    trap = FindTrap(cx->runtime, script, pc);
    if (handlerp)
        *handlerp = trap ? trap->handler : NULL;
    if (closurep)
        *closurep = trap ? trap->closure : JSVAL_NULL;
    if (trap)
        DestroyTrapAndUnlock(cx, trap);
    else
        DBG_UNLOCK(cx->runtime);

#ifdef JS_METHODJIT
    if (script->ncode != NULL && script->ncode != JS_UNJITTABLE_METHOD) {
        mjit::Recompiler recompiler(cx, script);
        recompiler.recompile();
    }
#endif
}

JS_PUBLIC_API(void)
JS_ClearScriptTraps(JSContext *cx, JSScript *script)
{
    JSRuntime *rt;
    JSTrap *trap, *next;
    uint32 sample;

    rt = cx->runtime;
    DBG_LOCK(rt);
    for (trap = (JSTrap *)rt->trapList.next;
         &trap->links != &rt->trapList;
         trap = next) {
        next = (JSTrap *)trap->links.next;
        if (trap->script == script) {
            sample = rt->debuggerMutations;
            DestroyTrapAndUnlock(cx, trap);
            DBG_LOCK(rt);
            if (rt->debuggerMutations != sample + 1)
                next = (JSTrap *)rt->trapList.next;
        }
    }
    DBG_UNLOCK(rt);
}

JS_PUBLIC_API(void)
JS_ClearAllTraps(JSContext *cx)
{
    JSRuntime *rt;
    JSTrap *trap, *next;
    uint32 sample;

    rt = cx->runtime;
    DBG_LOCK(rt);
    for (trap = (JSTrap *)rt->trapList.next;
         &trap->links != &rt->trapList;
         trap = next) {
        next = (JSTrap *)trap->links.next;
        sample = rt->debuggerMutations;
        DestroyTrapAndUnlock(cx, trap);
        DBG_LOCK(rt);
        if (rt->debuggerMutations != sample + 1)
            next = (JSTrap *)rt->trapList.next;
    }
    DBG_UNLOCK(rt);
}

/*
 * NB: js_MarkTraps does not acquire cx->runtime->debuggerLock, since the
 * debugger should never be racing with the GC (i.e., the debugger must
 * respect the request model).
 */
void
js_MarkTraps(JSTracer *trc)
{
    JSRuntime *rt = trc->context->runtime;

    for (JSTrap *trap = (JSTrap *) rt->trapList.next;
         &trap->links != &rt->trapList;
         trap = (JSTrap *) trap->links.next) {
        MarkValue(trc, Valueify(trap->closure), "trap->closure");
    }
}

JS_PUBLIC_API(JSTrapStatus)
JS_HandleTrap(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval)
{
    JSTrap *trap;
    jsint op;
    JSTrapStatus status;

    DBG_LOCK(cx->runtime);
    trap = FindTrap(cx->runtime, script, pc);
    JS_ASSERT(!trap || trap->handler);
    if (!trap) {
        op = (JSOp) *pc;
        DBG_UNLOCK(cx->runtime);

        /* Defend against "pc for wrong script" API usage error. */
        JS_ASSERT(op != JSOP_TRAP);

#ifdef JS_THREADSAFE
        /* If the API was abused, we must fail for want of the real op. */
        if (op == JSOP_TRAP)
            return JSTRAP_ERROR;

        /* Assume a race with a debugger thread and try to carry on. */
        *rval = INT_TO_JSVAL(op);
        return JSTRAP_CONTINUE;
#else
        /* Always fail if single-threaded (must be an API usage error). */
        return JSTRAP_ERROR;
#endif
    }
    DBG_UNLOCK(cx->runtime);

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

#ifdef JS_TRACER
static void
JITInhibitingHookChange(JSRuntime *rt, bool wasInhibited)
{
    if (wasInhibited) {
        if (!rt->debuggerInhibitsJIT()) {
            for (JSCList *cl = rt->contextList.next; cl != &rt->contextList; cl = cl->next)
                js_ContextFromLinkField(cl)->updateJITEnabled();
        }
    } else if (rt->debuggerInhibitsJIT()) {
        for (JSCList *cl = rt->contextList.next; cl != &rt->contextList; cl = cl->next)
            js_ContextFromLinkField(cl)->traceJitEnabled = false;
    }
}

static void
LeaveTraceRT(JSRuntime *rt)
{
    JSThreadData *data = js_CurrentThreadData(rt);
    JSContext *cx = data ? data->traceMonitor.tracecx : NULL;
    JS_UNLOCK_GC(rt);

    if (cx)
        LeaveTrace(cx);
}
#endif

JS_PUBLIC_API(JSBool)
JS_SetInterrupt(JSRuntime *rt, JSInterruptHook hook, void *closure)
{
#ifdef JS_TRACER
    {
        AutoLockGC lock(rt);
        bool wasInhibited = rt->debuggerInhibitsJIT();
#endif
        rt->globalDebugHooks.interruptHook = hook;
        rt->globalDebugHooks.interruptHookData = closure;
#ifdef JS_TRACER
        JITInhibitingHookChange(rt, wasInhibited);
    }
    LeaveTraceRT(rt);
#endif
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ClearInterrupt(JSRuntime *rt, JSInterruptHook *hoop, void **closurep)
{
#ifdef JS_TRACER
    AutoLockGC lock(rt);
    bool wasInhibited = rt->debuggerInhibitsJIT();
#endif
    if (hoop)
        *hoop = rt->globalDebugHooks.interruptHook;
    if (closurep)
        *closurep = rt->globalDebugHooks.interruptHookData;
    rt->globalDebugHooks.interruptHook = 0;
    rt->globalDebugHooks.interruptHookData = 0;
#ifdef JS_TRACER
    JITInhibitingHookChange(rt, wasInhibited);
#endif
    return JS_TRUE;
}

/************************************************************************/

typedef struct JSWatchPoint {
    JSCList             links;
    JSObject            *object;        /* weak link, see js_FinalizeObject */
    const Shape         *shape;
    PropertyOp          setter;
    JSWatchPointHandler handler;
    JSObject            *closure;
    uintN               flags;
} JSWatchPoint;

#define JSWP_LIVE       0x1             /* live because set and not cleared */
#define JSWP_HELD       0x2             /* held while running handler/setter */

static bool
IsWatchedProperty(JSContext *cx, const Shape &shape);

/*
 * NB: DropWatchPointAndUnlock releases cx->runtime->debuggerLock in all cases.
 */
static JSBool
DropWatchPointAndUnlock(JSContext *cx, JSWatchPoint *wp, uintN flag)
{
    bool ok = true;
    JSRuntime *rt = cx->runtime;

    wp->flags &= ~flag;
    if (wp->flags != 0) {
        DBG_UNLOCK(rt);
        return ok;
    }

    /*
     * Remove wp from the list, then if there are no other watchpoints for
     * wp->shape in any scope, restore wp->shape->setter from wp.
     */
    ++rt->debuggerMutations;
    JS_REMOVE_LINK(&wp->links);

    const Shape *shape = wp->shape;
    PropertyOp setter = NULL;

    for (JSWatchPoint *wp2 = (JSWatchPoint *)rt->watchPointList.next;
         &wp2->links != &rt->watchPointList;
         wp2 = (JSWatchPoint *)wp2->links.next) {
        if (wp2->shape == shape) {
            setter = wp->setter;
            break;
        }
    }
    DBG_UNLOCK(rt);

    if (!setter) {
        JS_LOCK_OBJ(cx, wp->object);

        /*
         * If the property wasn't found on wp->object, or it isn't still being
         * watched, then someone else must have deleted or unwatched it, and we
         * don't need to change the property attributes.
         */
        const Shape *wprop = wp->object->nativeLookup(shape->id);
        if (wprop &&
            wprop->hasSetterValue() == shape->hasSetterValue() &&
            IsWatchedProperty(cx, *wprop)) {
            shape = wp->object->changeProperty(cx, wprop, 0, wprop->attributes(),
                                               wprop->getter(), wp->setter);
            if (!shape)
                ok = false;
        }
        JS_UNLOCK_OBJ(cx, wp->object);
    }

    cx->free(wp);
    return ok;
}

/*
 * NB: js_TraceWatchPoints does not acquire cx->runtime->debuggerLock, since
 * the debugger should never be racing with the GC (i.e., the debugger must
 * respect the request model).
 */
void
js_TraceWatchPoints(JSTracer *trc, JSObject *obj)
{
    JSRuntime *rt;
    JSWatchPoint *wp;

    rt = trc->context->runtime;

    for (wp = (JSWatchPoint *)rt->watchPointList.next;
         &wp->links != &rt->watchPointList;
         wp = (JSWatchPoint *)wp->links.next) {
        if (wp->object == obj) {
            wp->shape->trace(trc);
            if (wp->shape->hasSetterValue() && wp->setter)
                MarkObject(trc, *CastAsObject(wp->setter), "wp->setter");
            MarkObject(trc, *wp->closure, "wp->closure");
        }
    }
}

void
js_SweepWatchPoints(JSContext *cx)
{
    JSRuntime *rt;
    JSWatchPoint *wp, *next;
    uint32 sample;

    rt = cx->runtime;
    DBG_LOCK(rt);
    for (wp = (JSWatchPoint *)rt->watchPointList.next;
         &wp->links != &rt->watchPointList;
         wp = next) {
        next = (JSWatchPoint *)wp->links.next;
        if (IsAboutToBeFinalized(wp->object)) {
            sample = rt->debuggerMutations;

            /* Ignore failures. */
            DropWatchPointAndUnlock(cx, wp, JSWP_LIVE);
            DBG_LOCK(rt);
            if (rt->debuggerMutations != sample + 1)
                next = (JSWatchPoint *)rt->watchPointList.next;
        }
    }
    DBG_UNLOCK(rt);
}



/*
 * NB: FindWatchPoint must be called with rt->debuggerLock acquired.
 */
static JSWatchPoint *
FindWatchPoint(JSRuntime *rt, JSObject *obj, jsid id)
{
    JSWatchPoint *wp;

    for (wp = (JSWatchPoint *)rt->watchPointList.next;
         &wp->links != &rt->watchPointList;
         wp = (JSWatchPoint *)wp->links.next) {
        if (wp->object == obj && wp->shape->id == id)
            return wp;
    }
    return NULL;
}

const Shape *
js_FindWatchPoint(JSRuntime *rt, JSObject *obj, jsid id)
{
    JSWatchPoint *wp;
    const Shape *shape;

    DBG_LOCK(rt);
    wp = FindWatchPoint(rt, obj, id);
    shape = wp ? wp->shape : NULL;
    DBG_UNLOCK(rt);
    return shape;
}

JSBool
js_watch_set(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JSRuntime *rt = cx->runtime;
    DBG_LOCK(rt);
    for (JSWatchPoint *wp = (JSWatchPoint *)rt->watchPointList.next;
         &wp->links != &rt->watchPointList;
         wp = (JSWatchPoint *)wp->links.next) {
        const Shape *shape = wp->shape;
        if (wp->object == obj && SHAPE_USERID(shape) == id &&
            !(wp->flags & JSWP_HELD)) {
            wp->flags |= JSWP_HELD;
            DBG_UNLOCK(rt);

            JS_LOCK_OBJ(cx, obj);
            jsid propid = shape->id;
            jsid userid = SHAPE_USERID(shape);
            JS_UNLOCK_OBJ(cx, obj);

            /* NB: wp is held, so we can safely dereference it still. */
            if (!wp->handler(cx, obj, propid,
                             obj->containsSlot(shape->slot)
                             ? Jsvalify(obj->getSlotMT(cx, shape->slot))
                             : JSVAL_VOID,
                             Jsvalify(vp), wp->closure)) {
                DBG_LOCK(rt);
                DropWatchPointAndUnlock(cx, wp, JSWP_HELD);
                return JS_FALSE;
            }

            /*
             * Pass the output of the handler to the setter. Security wrappers
             * prevent any funny business between watchpoints and setters.
             */
            JSBool ok = !wp->setter ||
                        (shape->hasSetterValue()
                         ? ExternalInvoke(cx, obj,
                                          ObjectValue(*CastAsObject(wp->setter)),
                                          1, vp, vp)
                         : CallJSPropertyOpSetter(cx, wp->setter, obj, userid, vp));

            DBG_LOCK(rt);
            return DropWatchPointAndUnlock(cx, wp, JSWP_HELD) && ok;
        }
    }
    DBG_UNLOCK(rt);
    return JS_TRUE;
}

JSBool
js_watch_set_wrapper(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!obj)
        return false;

    JSObject &funobj = JS_CALLEE(cx, vp).toObject();
    JSFunction *wrapper = funobj.getFunctionPrivate();
    jsid userid = ATOM_TO_JSID(wrapper->atom);

    JS_SET_RVAL(cx, vp, argc ? JS_ARGV(cx, vp)[0] : UndefinedValue());
    return js_watch_set(cx, obj, userid, vp);
}

static bool
IsWatchedProperty(JSContext *cx, const Shape &shape)
{
    if (shape.hasSetterValue()) {
        JSObject *funobj = shape.setterObject();
        if (!funobj || !funobj->isFunction())
            return false;

        JSFunction *fun = GET_FUNCTION_PRIVATE(cx, funobj);
        return fun->maybeNative() == js_watch_set_wrapper;
    }
    return shape.setterOp() == js_watch_set;
}

PropertyOp
js_WrapWatchedSetter(JSContext *cx, jsid id, uintN attrs, PropertyOp setter)
{
    JSAtom *atom;
    JSFunction *wrapper;

    if (!(attrs & JSPROP_SETTER))
        return &js_watch_set;   /* & to silence schoolmarmish MSVC */

    if (JSID_IS_ATOM(id)) {
        atom = JSID_TO_ATOM(id);
    } else if (JSID_IS_INT(id)) {
        if (!js_ValueToStringId(cx, IdToValue(id), &id))
            return NULL;
        atom = JSID_TO_ATOM(id);
    } else {
        atom = NULL;
    }

    wrapper = js_NewFunction(cx, NULL, js_watch_set_wrapper, 1, 0,
                             setter ? CastAsObject(setter)->getParent() : NULL, atom);
    if (!wrapper)
        return NULL;
    return CastAsPropertyOp(FUN_OBJECT(wrapper));
}

JS_PUBLIC_API(JSBool)
JS_SetWatchPoint(JSContext *cx, JSObject *obj, jsid id,
                 JSWatchPointHandler handler, JSObject *closure)
{
    JSObject *origobj;
    Value v;
    uintN attrs;
    jsid propid;
    JSObject *pobj;
    JSProperty *prop;
    const Shape *shape;
    JSRuntime *rt;
    JSBool ok;
    JSWatchPoint *wp;
    PropertyOp watcher;

    origobj = obj;
    obj = obj->wrappedObject(cx);
    OBJ_TO_INNER_OBJECT(cx, obj);
    if (!obj)
        return JS_FALSE;

    AutoValueRooter idroot(cx);
    if (JSID_IS_INT(id)) {
        propid = id;
    } else {
        if (!js_ValueToStringId(cx, IdToValue(id), &propid))
            return JS_FALSE;
        propid = js_CheckForStringIndex(propid);
        idroot.set(IdToValue(propid));
    }

    /*
     * If, by unwrapping and innerizing, we changed the object, check
     * again to make sure that we're allowed to set a watch point.
     */
    if (origobj != obj && !CheckAccess(cx, obj, propid, JSACC_WATCH, &v, &attrs))
        return JS_FALSE;

    if (!obj->isNative()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_WATCH,
                             obj->getClass()->name);
        return JS_FALSE;
    }

    if (!js_LookupProperty(cx, obj, propid, &pobj, &prop))
        return JS_FALSE;
    shape = (Shape *) prop;
    rt = cx->runtime;
    if (!shape) {
        /* Check for a deleted symbol watchpoint, which holds its property. */
        shape = js_FindWatchPoint(rt, obj, propid);
        if (!shape) {
            /* Make a new property in obj so we can watch for the first set. */
            if (!js_DefineNativeProperty(cx, obj, propid, UndefinedValue(), NULL, NULL,
                                         JSPROP_ENUMERATE, 0, 0, &prop)) {
                return JS_FALSE;
            }
            shape = (Shape *) prop;
        }
    } else if (pobj != obj) {
        /* Clone the prototype property so we can watch the right object. */
        AutoValueRooter valroot(cx);
        PropertyOp getter, setter;
        uintN attrs, flags;
        intN shortid;

        if (pobj->isNative()) {
            valroot.set(pobj->containsSlot(shape->slot)
                        ? pobj->lockedGetSlot(shape->slot)
                        : UndefinedValue());
            getter = shape->getter();
            setter = shape->setter();
            attrs = shape->attributes();
            flags = shape->getFlags();
            shortid = shape->shortid;
            JS_UNLOCK_OBJ(cx, pobj);
        } else {
            if (!pobj->getProperty(cx, propid, valroot.addr()) ||
                !pobj->getAttributes(cx, propid, &attrs)) {
                return JS_FALSE;
            }
            getter = setter = NULL;
            flags = 0;
            shortid = 0;
        }

        /* Recall that obj is native, whether or not pobj is native. */
        if (!js_DefineNativeProperty(cx, obj, propid, valroot.value(),
                                     getter, setter, attrs, flags,
                                     shortid, &prop)) {
            return JS_FALSE;
        }
        shape = (Shape *) prop;
    }

    /*
     * At this point, prop/shape exists in obj, obj is locked, and we must
     * unlock the object before returning.
     */
    ok = JS_TRUE;
    DBG_LOCK(rt);
    wp = FindWatchPoint(rt, obj, propid);
    if (!wp) {
        DBG_UNLOCK(rt);
        watcher = js_WrapWatchedSetter(cx, propid, shape->attributes(), shape->setter());
        if (!watcher) {
            ok = JS_FALSE;
            goto out;
        }

        wp = (JSWatchPoint *) cx->malloc(sizeof *wp);
        if (!wp) {
            ok = JS_FALSE;
            goto out;
        }
        wp->handler = NULL;
        wp->closure = NULL;
        wp->object = obj;
        wp->setter = shape->setter();
        wp->flags = JSWP_LIVE;

        /* XXXbe nest in obj lock here */
        shape = js_ChangeNativePropertyAttrs(cx, obj, shape, 0, shape->attributes(),
                                             shape->getter(), watcher);
        if (!shape) {
            /* Self-link so DropWatchPointAndUnlock can JS_REMOVE_LINK it. */
            JS_INIT_CLIST(&wp->links);
            DBG_LOCK(rt);
            DropWatchPointAndUnlock(cx, wp, JSWP_LIVE);
            ok = JS_FALSE;
            goto out;
        }
        wp->shape = shape;

        /*
         * Now that wp is fully initialized, append it to rt's wp list.
         * Because obj is locked we know that no other thread could have added
         * a watchpoint for (obj, propid).
         */
        DBG_LOCK(rt);
        JS_ASSERT(!FindWatchPoint(rt, obj, propid));
        JS_APPEND_LINK(&wp->links, &rt->watchPointList);
        ++rt->debuggerMutations;
    }
    wp->handler = handler;
    wp->closure = reinterpret_cast<JSObject*>(closure);
    DBG_UNLOCK(rt);

out:
    JS_UNLOCK_OBJ(cx, obj);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_ClearWatchPoint(JSContext *cx, JSObject *obj, jsid id,
                   JSWatchPointHandler *handlerp, JSObject **closurep)
{
    JSRuntime *rt;
    JSWatchPoint *wp;

    rt = cx->runtime;
    DBG_LOCK(rt);
    for (wp = (JSWatchPoint *)rt->watchPointList.next;
         &wp->links != &rt->watchPointList;
         wp = (JSWatchPoint *)wp->links.next) {
        if (wp->object == obj && SHAPE_USERID(wp->shape) == id) {
            if (handlerp)
                *handlerp = wp->handler;
            if (closurep)
                *closurep = wp->closure;
            return DropWatchPointAndUnlock(cx, wp, JSWP_LIVE);
        }
    }
    DBG_UNLOCK(rt);
    if (handlerp)
        *handlerp = NULL;
    if (closurep)
        *closurep = NULL;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ClearWatchPointsForObject(JSContext *cx, JSObject *obj)
{
    JSRuntime *rt;
    JSWatchPoint *wp, *next;
    uint32 sample;

    rt = cx->runtime;
    DBG_LOCK(rt);
    for (wp = (JSWatchPoint *)rt->watchPointList.next;
         &wp->links != &rt->watchPointList;
         wp = next) {
        next = (JSWatchPoint *)wp->links.next;
        if (wp->object == obj) {
            sample = rt->debuggerMutations;
            if (!DropWatchPointAndUnlock(cx, wp, JSWP_LIVE))
                return JS_FALSE;
            DBG_LOCK(rt);
            if (rt->debuggerMutations != sample + 1)
                next = (JSWatchPoint *)rt->watchPointList.next;
        }
    }
    DBG_UNLOCK(rt);
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ClearAllWatchPoints(JSContext *cx)
{
    JSRuntime *rt;
    JSWatchPoint *wp, *next;
    uint32 sample;

    rt = cx->runtime;
    DBG_LOCK(rt);
    for (wp = (JSWatchPoint *)rt->watchPointList.next;
         &wp->links != &rt->watchPointList;
         wp = next) {
        next = (JSWatchPoint *)wp->links.next;
        sample = rt->debuggerMutations;
        if (!DropWatchPointAndUnlock(cx, wp, JSWP_LIVE))
            return JS_FALSE;
        DBG_LOCK(rt);
        if (rt->debuggerMutations != sample + 1)
            next = (JSWatchPoint *)rt->watchPointList.next;
    }
    DBG_UNLOCK(rt);
    return JS_TRUE;
}

/************************************************************************/

JS_PUBLIC_API(uintN)
JS_PCToLineNumber(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    return js_PCToLineNumber(cx, script, pc);
}

JS_PUBLIC_API(jsbytecode *)
JS_LineNumberToPC(JSContext *cx, JSScript *script, uintN lineno)
{
    return js_LineNumberToPC(script, lineno);
}

JS_PUBLIC_API(uintN)
JS_GetFunctionArgumentCount(JSContext *cx, JSFunction *fun)
{
    return fun->nargs;
}

JS_PUBLIC_API(JSBool)
JS_FunctionHasLocalNames(JSContext *cx, JSFunction *fun)
{
    return fun->hasLocalNames();
}

extern JS_PUBLIC_API(jsuword *)
JS_GetFunctionLocalNameArray(JSContext *cx, JSFunction *fun, void **markp)
{
    *markp = JS_ARENA_MARK(&cx->tempPool);
    return fun->getLocalNameArray(cx, &cx->tempPool);
}

extern JS_PUBLIC_API(JSAtom *)
JS_LocalNameToAtom(jsuword w)
{
    return JS_LOCAL_NAME_TO_ATOM(w);
}

extern JS_PUBLIC_API(JSString *)
JS_AtomKey(JSAtom *atom)
{
    return ATOM_TO_STRING(atom);
}

extern JS_PUBLIC_API(void)
JS_ReleaseFunctionLocalNameArray(JSContext *cx, void *mark)
{
    JS_ARENA_RELEASE(&cx->tempPool, mark);
}

JS_PUBLIC_API(JSScript *)
JS_GetFunctionScript(JSContext *cx, JSFunction *fun)
{
    return FUN_SCRIPT(fun);
}

JS_PUBLIC_API(JSNative)
JS_GetFunctionNative(JSContext *cx, JSFunction *fun)
{
    return Jsvalify(fun->maybeNative());
}

JS_PUBLIC_API(JSPrincipals *)
JS_GetScriptPrincipals(JSContext *cx, JSScript *script)
{
    return script->principals;
}

/************************************************************************/

/*
 *  Stack Frame Iterator
 */
JS_PUBLIC_API(JSStackFrame *)
JS_FrameIterator(JSContext *cx, JSStackFrame **iteratorp)
{
    *iteratorp = (*iteratorp == NULL) ? js_GetTopStackFrame(cx) : (*iteratorp)->prev();
    return *iteratorp;
}

JS_PUBLIC_API(JSScript *)
JS_GetFrameScript(JSContext *cx, JSStackFrame *fp)
{
    return fp->maybeScript();
}

JS_PUBLIC_API(jsbytecode *)
JS_GetFramePC(JSContext *cx, JSStackFrame *fp)
{
    return fp->pc(cx);
}

JS_PUBLIC_API(JSStackFrame *)
JS_GetScriptedCaller(JSContext *cx, JSStackFrame *fp)
{
    return js_GetScriptedCaller(cx, fp);
}

JSPrincipals *
js_StackFramePrincipals(JSContext *cx, JSStackFrame *fp)
{
    JSSecurityCallbacks *callbacks;

    if (fp->isFunctionFrame()) {
        callbacks = JS_GetSecurityCallbacks(cx);
        if (callbacks && callbacks->findObjectPrincipals) {
            if (&fp->fun()->compiledFunObj() != &fp->callee())
                return callbacks->findObjectPrincipals(cx, &fp->callee());
            /* FALL THROUGH */
        }
    }
    if (fp->isScriptFrame())
        return fp->script()->principals;
    return NULL;
}

JSPrincipals *
js_EvalFramePrincipals(JSContext *cx, JSObject *callee, JSStackFrame *caller)
{
    JSPrincipals *principals, *callerPrincipals;
    JSSecurityCallbacks *callbacks;

    callbacks = JS_GetSecurityCallbacks(cx);
    if (callbacks && callbacks->findObjectPrincipals)
        principals = callbacks->findObjectPrincipals(cx, callee);
    else
        principals = NULL;
    if (!caller)
        return principals;
    callerPrincipals = js_StackFramePrincipals(cx, caller);
    return (callerPrincipals && principals &&
            callerPrincipals->subsume(callerPrincipals, principals))
           ? principals
           : callerPrincipals;
}

JS_PUBLIC_API(void *)
JS_GetFrameAnnotation(JSContext *cx, JSStackFrame *fp)
{
    if (fp->annotation() && fp->isScriptFrame()) {
        JSPrincipals *principals = js_StackFramePrincipals(cx, fp);

        if (principals && principals->globalPrivilegesEnabled(cx, principals)) {
            /*
             * Give out an annotation only if privileges have not been revoked
             * or disabled globally.
             */
            return fp->annotation();
        }
    }

    return NULL;
}

JS_PUBLIC_API(void)
JS_SetFrameAnnotation(JSContext *cx, JSStackFrame *fp, void *annotation)
{
    fp->setAnnotation(annotation);
}

JS_PUBLIC_API(void *)
JS_GetFramePrincipalArray(JSContext *cx, JSStackFrame *fp)
{
    JSPrincipals *principals;

    principals = js_StackFramePrincipals(cx, fp);
    if (!principals)
        return NULL;
    return principals->getPrincipalArray(cx, principals);
}

JS_PUBLIC_API(JSBool)
JS_IsScriptFrame(JSContext *cx, JSStackFrame *fp)
{
    return !fp->isDummyFrame();
}

/* this is deprecated, use JS_GetFrameScopeChain instead */
JS_PUBLIC_API(JSObject *)
JS_GetFrameObject(JSContext *cx, JSStackFrame *fp)
{
    return &fp->scopeChain();
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameScopeChain(JSContext *cx, JSStackFrame *fp)
{
    JS_ASSERT(cx->stack().contains(fp));

    /* Force creation of argument and call objects if not yet created */
    (void) JS_GetFrameCallObject(cx, fp);
    return js_GetScopeChain(cx, fp);
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameCallObject(JSContext *cx, JSStackFrame *fp)
{
    JS_ASSERT(cx->stack().contains(fp));

    if (!fp->isFunctionFrame())
        return NULL;

    /* Force creation of argument object if not yet created */
    (void) js_GetArgsObject(cx, fp);

    /*
     * XXX ill-defined: null return here means error was reported, unlike a
     *     null returned above or in the #else
     */
    return js_GetCallObject(cx, fp);
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameThis(JSContext *cx, JSStackFrame *fp)
{
    if (fp->isDummyFrame())
        return NULL;
    return fp->computeThisObject(cx);
}

JS_PUBLIC_API(JSFunction *)
JS_GetFrameFunction(JSContext *cx, JSStackFrame *fp)
{
    return fp->maybeFun();
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameFunctionObject(JSContext *cx, JSStackFrame *fp)
{
    if (!fp->isFunctionFrame())
        return NULL;

    JS_ASSERT(fp->callee().isFunction());
    JS_ASSERT(fp->callee().getPrivate() == fp->fun());
    return &fp->callee();
}

JS_PUBLIC_API(JSBool)
JS_IsConstructorFrame(JSContext *cx, JSStackFrame *fp)
{
    return fp->isConstructing();
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameCalleeObject(JSContext *cx, JSStackFrame *fp)
{
    return fp->maybeCallee();
}

JS_PUBLIC_API(JSBool)
JS_GetValidFrameCalleeObject(JSContext *cx, JSStackFrame *fp, jsval *vp)
{
    Value v;

    if (!fp->getValidCalleeObject(cx, &v))
        return false;
    *vp = Jsvalify(v);
    return true;
}

JS_PUBLIC_API(JSBool)
JS_IsDebuggerFrame(JSContext *cx, JSStackFrame *fp)
{
    return fp->isDebuggerFrame();
}

JS_PUBLIC_API(jsval)
JS_GetFrameReturnValue(JSContext *cx, JSStackFrame *fp)
{
    return Jsvalify(fp->returnValue());
}

JS_PUBLIC_API(void)
JS_SetFrameReturnValue(JSContext *cx, JSStackFrame *fp, jsval rval)
{
    fp->setReturnValue(Valueify(rval));
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

JS_PUBLIC_API(JSVersion)
JS_GetScriptVersion(JSContext *cx, JSScript *script)
{
    return VersionNumber(script->getVersion());
}

/***************************************************************************/

JS_PUBLIC_API(void)
JS_SetNewScriptHook(JSRuntime *rt, JSNewScriptHook hook, void *callerdata)
{
    rt->globalDebugHooks.newScriptHook = hook;
    rt->globalDebugHooks.newScriptHookData = callerdata;
}

JS_PUBLIC_API(void)
JS_SetDestroyScriptHook(JSRuntime *rt, JSDestroyScriptHook hook,
                        void *callerdata)
{
    rt->globalDebugHooks.destroyScriptHook = hook;
    rt->globalDebugHooks.destroyScriptHookData = callerdata;
}

/***************************************************************************/

JS_PUBLIC_API(JSBool)
JS_EvaluateUCInStackFrame(JSContext *cx, JSStackFrame *fp,
                          const jschar *chars, uintN length,
                          const char *filename, uintN lineno,
                          jsval *rval)
{
    JS_ASSERT_NOT_ON_TRACE(cx);

    if (!CheckDebugMode(cx))
        return JS_FALSE;

    JSObject *scobj = JS_GetFrameScopeChain(cx, fp);
    if (!scobj)
        return false;

    /*
     * NB: This function breaks the assumption that the compiler can see all
     * calls and properly compute a static level. In order to get around this,
     * we use a static level that will cause us not to attempt to optimize
     * variable references made by this frame.
     */
    JSScript *script = Compiler::compileScript(cx, scobj, fp, js_StackFramePrincipals(cx, fp),
                                               TCF_COMPILE_N_GO, chars, length, NULL,
                                               filename, lineno, NULL,
                                               UpvarCookie::UPVAR_LEVEL_LIMIT);

    if (!script)
        return false;

    bool ok = Execute(cx, scobj, script, fp, JSFRAME_DEBUGGER | JSFRAME_EVAL, Valueify(rval));

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
    size_t len = length;
    
    if (!CheckDebugMode(cx))
        return JS_FALSE;

    chars = js_InflateString(cx, bytes, &len);
    if (!chars)
        return JS_FALSE;
    length = (uintN) len;
    ok = JS_EvaluateUCInStackFrame(cx, fp, chars, length, filename, lineno,
                                   rval);
    cx->free(chars);

    return ok;
}

/************************************************************************/

/* This all should be reworked to avoid requiring JSScopeProperty types. */

JS_PUBLIC_API(JSScopeProperty *)
JS_PropertyIterator(JSObject *obj, JSScopeProperty **iteratorp)
{
    const Shape *shape;

    /* The caller passes null in *iteratorp to get things started. */
    shape = (Shape *) *iteratorp;
    if (!shape) {
        shape = obj->lastProperty();
    } else {
        shape = shape->previous();
        if (!shape->previous()) {
            JS_ASSERT(JSID_IS_EMPTY(shape->id));
            shape = NULL;
        }
    }

    return *iteratorp = reinterpret_cast<JSScopeProperty *>(const_cast<Shape *>(shape));
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyDesc(JSContext *cx, JSObject *obj, JSScopeProperty *sprop,
                   JSPropertyDesc *pd)
{
    Shape *shape = (Shape *) sprop;
    pd->id = IdToJsval(shape->id);

    JSBool wasThrowing = cx->throwing;
    AutoValueRooter lastException(cx, cx->exception);
    cx->throwing = JS_FALSE;

    if (!js_GetProperty(cx, obj, shape->id, Valueify(&pd->value))) {
        if (!cx->throwing) {
            pd->flags = JSPD_ERROR;
            pd->value = JSVAL_VOID;
        } else {
            pd->flags = JSPD_EXCEPTION;
            pd->value = Jsvalify(cx->exception);
        }
    } else {
        pd->flags = 0;
    }

    cx->throwing = wasThrowing;
    if (wasThrowing)
        cx->exception = lastException.value();

    pd->flags |= (shape->enumerable() ? JSPD_ENUMERATE : 0)
              |  (!shape->writable()  ? JSPD_READONLY  : 0)
              |  (!shape->configurable() ? JSPD_PERMANENT : 0);
    pd->spare = 0;
    if (shape->getter() == js_GetCallArg) {
        pd->slot = shape->shortid;
        pd->flags |= JSPD_ARGUMENT;
    } else if (shape->getter() == js_GetCallVar) {
        pd->slot = shape->shortid;
        pd->flags |= JSPD_VARIABLE;
    } else {
        pd->slot = 0;
    }
    pd->alias = JSVAL_VOID;

    if (obj->containsSlot(shape->slot)) {
        for (Shape::Range r = obj->lastProperty()->all(); !r.empty(); r.popFront()) {
            const Shape &aprop = r.front();
            if (&aprop != shape && aprop.slot == shape->slot) {
                pd->alias = IdToJsval(aprop.id);
                break;
            }
        }
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyDescArray(JSContext *cx, JSObject *obj, JSPropertyDescArray *pda)
{
    Class *clasp = obj->getClass();
    if (!obj->isNative() || (clasp->flags & JSCLASS_NEW_ENUMERATE)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_CANT_DESCRIBE_PROPS, clasp->name);
        return JS_FALSE;
    }
    if (!clasp->enumerate(cx, obj))
        return JS_FALSE;

    /* Return an empty pda early if obj has no own properties. */
    if (obj->nativeEmpty()) {
        pda->length = 0;
        pda->array = NULL;
        return JS_TRUE;
    }

    uint32 n = obj->propertyCount();
    JSPropertyDesc *pd = (JSPropertyDesc *) cx->malloc(size_t(n) * sizeof(JSPropertyDesc));
    if (!pd)
        return JS_FALSE;
    uint32 i = 0;
    for (Shape::Range r = obj->lastProperty()->all(); !r.empty(); r.popFront()) {
        if (!js_AddRoot(cx, Valueify(&pd[i].id), NULL))
            goto bad;
        if (!js_AddRoot(cx, Valueify(&pd[i].value), NULL))
            goto bad;
        Shape *shape = const_cast<Shape *>(&r.front());
        if (!JS_GetPropertyDesc(cx, obj, reinterpret_cast<JSScopeProperty *>(shape), &pd[i]))
            goto bad;
        if ((pd[i].flags & JSPD_ALIAS) && !js_AddRoot(cx, Valueify(&pd[i].alias), NULL))
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
    cx->free(pd);
}

/************************************************************************/

JS_FRIEND_API(JSBool)
js_GetPropertyByIdWithFakeFrame(JSContext *cx, JSObject *obj, JSObject *scopeobj, jsid id,
                                jsval *vp)
{
    JS_ASSERT(scopeobj->isGlobal());

    DummyFrameGuard frame;
    if (!cx->stack().pushDummyFrame(cx, *scopeobj, &frame))
        return false;

    bool ok = JS_GetPropertyById(cx, obj, id, vp);

    JS_ASSERT(!frame.fp()->hasCallObj());
    JS_ASSERT(!frame.fp()->hasArgsObj());
    return ok;
}

JS_FRIEND_API(JSBool)
js_SetPropertyByIdWithFakeFrame(JSContext *cx, JSObject *obj, JSObject *scopeobj, jsid id,
                                jsval *vp)
{
    JS_ASSERT(scopeobj->isGlobal());

    DummyFrameGuard frame;
    if (!cx->stack().pushDummyFrame(cx, *scopeobj, &frame))
        return false;

    bool ok = JS_SetPropertyById(cx, obj, id, vp);

    JS_ASSERT(!frame.fp()->hasCallObj());
    JS_ASSERT(!frame.fp()->hasArgsObj());
    return ok;
}

JS_FRIEND_API(JSBool)
js_CallFunctionValueWithFakeFrame(JSContext *cx, JSObject *obj, JSObject *scopeobj, jsval funval,
                                  uintN argc, jsval *argv, jsval *rval)
{
    JS_ASSERT(scopeobj->isGlobal());

    DummyFrameGuard frame;
    if (!cx->stack().pushDummyFrame(cx, *scopeobj, &frame))
        return false;

    bool ok = JS_CallFunctionValue(cx, obj, funval, argc, argv, rval);

    JS_ASSERT(!frame.fp()->hasCallObj());
    JS_ASSERT(!frame.fp()->hasArgsObj());
    return ok;
}

/************************************************************************/

JS_PUBLIC_API(JSBool)
JS_SetDebuggerHandler(JSRuntime *rt, JSDebuggerHandler handler, void *closure)
{
    rt->globalDebugHooks.debuggerHandler = handler;
    rt->globalDebugHooks.debuggerHandlerData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetSourceHandler(JSRuntime *rt, JSSourceHandler handler, void *closure)
{
    rt->globalDebugHooks.sourceHandler = handler;
    rt->globalDebugHooks.sourceHandlerData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetExecuteHook(JSRuntime *rt, JSInterpreterHook hook, void *closure)
{
    rt->globalDebugHooks.executeHook = hook;
    rt->globalDebugHooks.executeHookData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetCallHook(JSRuntime *rt, JSInterpreterHook hook, void *closure)
{
#ifdef JS_TRACER
    {
        AutoLockGC lock(rt);
        bool wasInhibited = rt->debuggerInhibitsJIT();
#endif
        rt->globalDebugHooks.callHook = hook;
        rt->globalDebugHooks.callHookData = closure;
#ifdef JS_TRACER
        JITInhibitingHookChange(rt, wasInhibited);
    }
    if (hook)
        LeaveTraceRT(rt);
#endif
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetThrowHook(JSRuntime *rt, JSThrowHook hook, void *closure)
{
    rt->globalDebugHooks.throwHook = hook;
    rt->globalDebugHooks.throwHookData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetDebugErrorHook(JSRuntime *rt, JSDebugErrorHook hook, void *closure)
{
    rt->globalDebugHooks.debugErrorHook = hook;
    rt->globalDebugHooks.debugErrorHookData = closure;
    return JS_TRUE;
}

/************************************************************************/

JS_PUBLIC_API(size_t)
JS_GetObjectTotalSize(JSContext *cx, JSObject *obj)
{
    size_t nbytes = (obj->isFunction() && obj->getPrivate() == obj)
                    ? sizeof(JSFunction)
                    : sizeof *obj;

    if (obj->dslots) {
        nbytes += (obj->dslots[-1].toPrivateUint32() - JS_INITIAL_NSLOTS + 1)
                  * sizeof obj->dslots[0];
    }
    return nbytes;
}

static size_t
GetAtomTotalSize(JSContext *cx, JSAtom *atom)
{
    size_t nbytes;

    nbytes = sizeof(JSAtom *) + sizeof(JSDHashEntryStub);
    nbytes += sizeof(JSString);
    nbytes += (ATOM_TO_STRING(atom)->flatLength() + 1) * sizeof(jschar);
    return nbytes;
}

JS_PUBLIC_API(size_t)
JS_GetFunctionTotalSize(JSContext *cx, JSFunction *fun)
{
    size_t nbytes;

    nbytes = sizeof *fun;
    nbytes += JS_GetObjectTotalSize(cx, FUN_OBJECT(fun));
    if (FUN_INTERPRETED(fun))
        nbytes += JS_GetScriptTotalSize(cx, fun->u.i.script);
    if (fun->atom)
        nbytes += GetAtomTotalSize(cx, fun->atom);
    return nbytes;
}

#include "jsemit.h"

JS_PUBLIC_API(size_t)
JS_GetScriptTotalSize(JSContext *cx, JSScript *script)
{
    size_t nbytes, pbytes;
    jsatomid i;
    jssrcnote *sn, *notes;
    JSObjectArray *objarray;
    JSPrincipals *principals;

    nbytes = sizeof *script;
    if (script->u.object)
        nbytes += JS_GetObjectTotalSize(cx, script->u.object);

    nbytes += script->length * sizeof script->code[0];
    nbytes += script->atomMap.length * sizeof script->atomMap.vector[0];
    for (i = 0; i < script->atomMap.length; i++)
        nbytes += GetAtomTotalSize(cx, script->atomMap.vector[i]);

    if (script->filename)
        nbytes += strlen(script->filename) + 1;

    notes = script->notes();
    for (sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn))
        continue;
    nbytes += (sn - notes + 1) * sizeof *sn;

    if (script->objectsOffset != 0) {
        objarray = script->objects();
        i = objarray->length;
        nbytes += sizeof *objarray + i * sizeof objarray->vector[0];
        do {
            nbytes += JS_GetObjectTotalSize(cx, objarray->vector[--i]);
        } while (i != 0);
    }

    if (script->regexpsOffset != 0) {
        objarray = script->regexps();
        i = objarray->length;
        nbytes += sizeof *objarray + i * sizeof objarray->vector[0];
        do {
            nbytes += JS_GetObjectTotalSize(cx, objarray->vector[--i]);
        } while (i != 0);
    }

    if (script->trynotesOffset != 0) {
        nbytes += sizeof(JSTryNoteArray) +
            script->trynotes()->length * sizeof(JSTryNote);
    }

    principals = script->principals;
    if (principals) {
        JS_ASSERT(principals->refcount);
        pbytes = sizeof *principals;
        if (principals->refcount > 1)
            pbytes = JS_HOWMANY(pbytes, principals->refcount);
        nbytes += pbytes;
    }

    return nbytes;
}

JS_PUBLIC_API(uint32)
JS_GetTopScriptFilenameFlags(JSContext *cx, JSStackFrame *fp)
{
    if (!fp)
        fp = js_GetTopStackFrame(cx);
    while (fp) {
        if (fp->isScriptFrame())
            return JS_GetScriptFilenameFlags(fp->script());
        fp = fp->prev();
    }
    return 0;
 }

JS_PUBLIC_API(uint32)
JS_GetScriptFilenameFlags(JSScript *script)
{
    JS_ASSERT(script);
    if (!script->filename)
        return JSFILENAME_NULL;
    return js_GetScriptFilenameFlags(script->filename);
}

JS_PUBLIC_API(JSBool)
JS_FlagScriptFilenamePrefix(JSRuntime *rt, const char *prefix, uint32 flags)
{
    if (!js_SaveScriptFilenameRT(rt, prefix, flags))
        return JS_FALSE;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_IsSystemObject(JSContext *cx, JSObject *obj)
{
    return obj->isSystem();
}

JS_PUBLIC_API(JSBool)
JS_MakeSystemObject(JSContext *cx, JSObject *obj)
{
    obj->setSystem();
    return true;
}

/************************************************************************/

JS_FRIEND_API(void)
js_RevertVersion(JSContext *cx)
{
    cx->clearVersionOverride();
}

JS_PUBLIC_API(const JSDebugHooks *)
JS_GetGlobalDebugHooks(JSRuntime *rt)
{
    return &rt->globalDebugHooks;
}

const JSDebugHooks js_NullDebugHooks = {};

JS_PUBLIC_API(JSDebugHooks *)
JS_SetContextDebugHooks(JSContext *cx, const JSDebugHooks *hooks)
{
    JS_ASSERT(hooks);
    if (hooks != &cx->runtime->globalDebugHooks && hooks != &js_NullDebugHooks)
        LeaveTrace(cx);

#ifdef JS_TRACER
    AutoLockGC lock(cx->runtime);
#endif
    JSDebugHooks *old = const_cast<JSDebugHooks *>(cx->debugHooks);
    cx->debugHooks = hooks;
#ifdef JS_TRACER
    cx->updateJITEnabled();
#endif
    return old;
}

JS_PUBLIC_API(JSDebugHooks *)
JS_ClearContextDebugHooks(JSContext *cx)
{
    return JS_SetContextDebugHooks(cx, &js_NullDebugHooks);
}

#ifdef MOZ_SHARK

#include <CHUD/CHUD.h>

JS_PUBLIC_API(JSBool)
JS_StartChudRemote()
{
    if (chudIsRemoteAccessAcquired() &&
        (chudStartRemotePerfMonitor("Mozilla") == chudSuccess)) {
        return JS_TRUE;
    }

    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_StopChudRemote()
{
    if (chudIsRemoteAccessAcquired() &&
        (chudStopRemotePerfMonitor() == chudSuccess)) {
        return JS_TRUE;
    }

    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ConnectShark()
{
    if (!chudIsInitialized() && (chudInitialize() != chudSuccess))
        return JS_FALSE;

    if (chudAcquireRemoteAccess() != chudSuccess)
        return JS_FALSE;

    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_DisconnectShark()
{
    if (chudIsRemoteAccessAcquired() && (chudReleaseRemoteAccess() != chudSuccess))
        return JS_FALSE;

    return JS_TRUE;
}

JS_FRIEND_API(JSBool)
js_StartShark(JSContext *cx, uintN argc, jsval *vp)
{
    if (!JS_StartChudRemote()) {
        JS_ReportError(cx, "Error starting CHUD.");
        return JS_FALSE;
    }

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

JS_FRIEND_API(JSBool)
js_StopShark(JSContext *cx, uintN argc, jsval *vp)
{
    if (!JS_StopChudRemote()) {
        JS_ReportError(cx, "Error stopping CHUD.");
        return JS_FALSE;
    }

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

JS_FRIEND_API(JSBool)
js_ConnectShark(JSContext *cx, uintN argc, jsval *vp)
{
    if (!JS_ConnectShark()) {
        JS_ReportError(cx, "Error connecting to Shark.");
        return JS_FALSE;
    }

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

JS_FRIEND_API(JSBool)
js_DisconnectShark(JSContext *cx, uintN argc, jsval *vp)
{
    if (!JS_DisconnectShark()) {
        JS_ReportError(cx, "Error disconnecting from Shark.");
        return JS_FALSE;
    }

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

#endif /* MOZ_SHARK */

#ifdef MOZ_CALLGRIND

#include <valgrind/callgrind.h>

JS_FRIEND_API(JSBool)
js_StartCallgrind(JSContext *cx, uintN argc, jsval *vp)
{
    CALLGRIND_START_INSTRUMENTATION;
    CALLGRIND_ZERO_STATS;
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

JS_FRIEND_API(JSBool)
js_StopCallgrind(JSContext *cx, uintN argc, jsval *vp)
{
    CALLGRIND_STOP_INSTRUMENTATION;
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

JS_FRIEND_API(JSBool)
js_DumpCallgrind(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    char *cstr;

    jsval *argv = JS_ARGV(cx, vp);
    if (argc > 0 && JSVAL_IS_STRING(argv[0])) {
        str = JSVAL_TO_STRING(argv[0]);
        cstr = js_DeflateString(cx, str->chars(), str->length());
        if (cstr) {
            CALLGRIND_DUMP_STATS_AT(cstr);
            cx->free(cstr);
            return JS_TRUE;
        }
    }
    CALLGRIND_DUMP_STATS;

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

#endif /* MOZ_CALLGRIND */

#ifdef MOZ_VTUNE
#include <VTuneApi.h>

static const char *vtuneErrorMessages[] = {
  "unknown, error #0",
  "invalid 'max samples' field",
  "invalid 'samples per buffer' field",
  "invalid 'sample interval' field",
  "invalid path",
  "sample file in use",
  "invalid 'number of events' field",
  "unknown, error #7",
  "internal error",
  "bad event name",
  "VTStopSampling called without calling VTStartSampling",
  "no events selected for event-based sampling",
  "events selected cannot be run together",
  "no sampling parameters",
  "sample database already exists",
  "sampling already started",
  "time-based sampling not supported",
  "invalid 'sampling parameters size' field",
  "invalid 'event size' field",
  "sampling file already bound",
  "invalid event path",
  "invalid license",
  "invalid 'global options' field",

};

JS_FRIEND_API(JSBool)
js_StartVtune(JSContext *cx, uintN argc, jsval *vp)
{
    VTUNE_EVENT events[] = {
        { 1000000, 0, 0, 0, "CPU_CLK_UNHALTED.CORE" },
        { 1000000, 0, 0, 0, "INST_RETIRED.ANY" },
    };

    U32 n_events = sizeof(events) / sizeof(VTUNE_EVENT);
    char *default_filename = "mozilla-vtune.tb5";
    JSString *str;
    U32 status;

    VTUNE_SAMPLING_PARAMS params = {
        sizeof(VTUNE_SAMPLING_PARAMS),
        sizeof(VTUNE_EVENT),
        0, 0, /* Reserved fields */
        1,    /* Initialize in "paused" state */
        0,    /* Max samples, or 0 for "continuous" */
        4096, /* Samples per buffer */
        0.1,  /* Sampling interval in ms */
        1,    /* 1 for event-based sampling, 0 for time-based */

        n_events,
        events,
        default_filename,
    };

    jsval *argv = JS_ARGV(cx, vp);
    if (argc > 0 && JSVAL_IS_STRING(argv[0])) {
        str = JSVAL_TO_STRING(argv[0]);
        params.tb5Filename = js_DeflateString(cx, str->chars(), str->length());
    }

    status = VTStartSampling(&params);

    if (params.tb5Filename != default_filename)
        cx->free(params.tb5Filename);

    if (status != 0) {
        if (status == VTAPI_MULTIPLE_RUNS)
            VTStopSampling(0);
        if (status < sizeof(vtuneErrorMessages))
            JS_ReportError(cx, "Vtune setup error: %s",
                           vtuneErrorMessages[status]);
        else
            JS_ReportError(cx, "Vtune setup error: %d",
                           status);
        return false;
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

JS_FRIEND_API(JSBool)
js_StopVtune(JSContext *cx, uintN argc, jsval *vp)
{
    U32 status = VTStopSampling(1);
    if (status) {
        if (status < sizeof(vtuneErrorMessages))
            JS_ReportError(cx, "Vtune shutdown error: %s",
                           vtuneErrorMessages[status]);
        else
            JS_ReportError(cx, "Vtune shutdown error: %d",
                           status);
        return false;
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

JS_FRIEND_API(JSBool)
js_PauseVtune(JSContext *cx, uintN argc, jsval *vp)
{
    VTPause();
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

JS_FRIEND_API(JSBool)
js_ResumeVtune(JSContext *cx, uintN argc, jsval *vp)
{
    VTResume();
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

#endif /* MOZ_VTUNE */

#ifdef MOZ_TRACEVIS
/*
 * Ethogram - Javascript wrapper for TraceVis state
 *
 * ethology: The scientific study of animal behavior,
 *           especially as it occurs in a natural environment.
 * ethogram: A pictorial catalog of the behavioral patterns of
 *           an organism or a species.
 *
 */
#if defined(XP_WIN)
#include "jswin.h"
#else
#include <sys/time.h>
#endif
#include "jstracer.h"

#define ETHOGRAM_BUF_SIZE 65536

static JSBool
ethogram_construct(JSContext *cx, uintN argc, jsval *vp);
static void
ethogram_finalize(JSContext *cx, JSObject *obj);

static JSClass ethogram_class = {
    "Ethogram",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, ethogram_finalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

struct EthogramEvent {
    TraceVisState s;
    TraceVisExitReason r;
    int ts;
    int tus;
    JSString *filename;
    int lineno;
};

static int
compare_strings(const void *k1, const void *k2)
{
    return strcmp((const char *) k1, (const char *) k2) == 0;
}

class EthogramEventBuffer {
private:
    EthogramEvent mBuf[ETHOGRAM_BUF_SIZE];
    int mReadPos;
    int mWritePos;
    JSObject *mFilenames;
    int mStartSecond;

    struct EthogramScriptEntry {
        char *filename;
        JSString *jsfilename;

        EthogramScriptEntry *next;
    };
    EthogramScriptEntry *mScripts;

public:
    friend JSBool
    ethogram_construct(JSContext *cx, uintN argc, jsval *vp);

    inline void push(TraceVisState s, TraceVisExitReason r, char *filename, int lineno) {
        mBuf[mWritePos].s = s;
        mBuf[mWritePos].r = r;
#if defined(XP_WIN)
        FILETIME now;
        GetSystemTimeAsFileTime(&now);
        unsigned long long raw_us = 0.1 *
            (((unsigned long long) now.dwHighDateTime << 32ULL) |
             (unsigned long long) now.dwLowDateTime);
        unsigned int sec = raw_us / 1000000L;
        unsigned int usec = raw_us % 1000000L;
        mBuf[mWritePos].ts = sec - mStartSecond;
        mBuf[mWritePos].tus = usec;
#else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        mBuf[mWritePos].ts = tv.tv_sec - mStartSecond;
        mBuf[mWritePos].tus = tv.tv_usec;
#endif

        JSString *jsfilename = findScript(filename);
        mBuf[mWritePos].filename = jsfilename;
        mBuf[mWritePos].lineno = lineno;

        mWritePos = (mWritePos + 1) % ETHOGRAM_BUF_SIZE;
        if (mWritePos == mReadPos) {
            mReadPos = (mWritePos + 1) % ETHOGRAM_BUF_SIZE;
        }
    }

    inline EthogramEvent *pop() {
        EthogramEvent *e = &mBuf[mReadPos];
        mReadPos = (mReadPos + 1) % ETHOGRAM_BUF_SIZE;
        return e;
    }

    bool isEmpty() {
        return (mReadPos == mWritePos);
    }

    EthogramScriptEntry *addScript(JSContext *cx, JSObject *obj, char *filename, JSString *jsfilename) {
        JSHashNumber hash = JS_HashString(filename);
        JSHashEntry **hep = JS_HashTableRawLookup(traceVisScriptTable, hash, filename);
        if (*hep != NULL)
            return JS_FALSE;

        JS_HashTableRawAdd(traceVisScriptTable, hep, hash, filename, this);

        EthogramScriptEntry * entry = (EthogramScriptEntry *) JS_malloc(cx, sizeof(EthogramScriptEntry));
        if (entry == NULL)
            return NULL;

        entry->next = mScripts;
        mScripts = entry;
        entry->filename = filename;
        entry->jsfilename = jsfilename;

        return mScripts;
    }

    void removeScripts(JSContext *cx) {
        EthogramScriptEntry *se = mScripts;
        while (se != NULL) {
            char *filename = se->filename;

            JSHashNumber hash = JS_HashString(filename);
            JSHashEntry **hep = JS_HashTableRawLookup(traceVisScriptTable, hash, filename);
            JSHashEntry *he = *hep;
            if (he) {
                /* we hardly knew he */
                JS_HashTableRawRemove(traceVisScriptTable, hep, he);
            }

            EthogramScriptEntry *se_head = se;
            se = se->next;
            JS_free(cx, se_head);
        }
    }

    JSString *findScript(char *filename) {
        EthogramScriptEntry *se = mScripts;
        while (se != NULL) {
            if (compare_strings(se->filename, filename))
                return (se->jsfilename);
            se = se->next;
        }
        return NULL;
    }

    JSObject *filenames() {
        return mFilenames;
    }

    int length() {
        if (mWritePos < mReadPos)
            return (mWritePos + ETHOGRAM_BUF_SIZE) - mReadPos;
        else
            return mWritePos - mReadPos;
    }
};

static char jstv_empty[] = "<null>";

inline char *
jstv_Filename(JSStackFrame *fp)
{
    while (fp && !fp->isScriptFrame())
        fp = fp->prev();
    return (fp && fp->maybeScript() && fp->script()->filename)
           ? (char *)fp->script()->filename
           : jstv_empty;
}
inline uintN
jstv_Lineno(JSContext *cx, JSStackFrame *fp)
{
    while (fp && fp->pc(cx) == NULL)
        fp = fp->prev();
    return (fp && fp->pc(cx)) ? js_FramePCToLineNumber(cx, fp) : 0;
}

/* Collect states here and distribute to a matching buffer, if any */
JS_FRIEND_API(void)
js::StoreTraceVisState(JSContext *cx, TraceVisState s, TraceVisExitReason r)
{
    JSStackFrame *fp = cx->fp();

    char *script_file = jstv_Filename(fp);
    JSHashNumber hash = JS_HashString(script_file);

    JSHashEntry **hep = JS_HashTableRawLookup(traceVisScriptTable, hash, script_file);
    /* update event buffer, flag if overflowed */
    JSHashEntry *he = *hep;
    if (he) {
        EthogramEventBuffer *p;
        p = (EthogramEventBuffer *) he->value;

        p->push(s, r, script_file, jstv_Lineno(cx, fp));
    }
}

static JSBool
ethogram_construct(JSContext *cx, uintN argc, jsval *vp)
{
    EthogramEventBuffer *p;

    p = (EthogramEventBuffer *) JS_malloc(cx, sizeof(EthogramEventBuffer));

    p->mReadPos = p->mWritePos = 0;
    p->mScripts = NULL;
    p->mFilenames = JS_NewArrayObject(cx, 0, NULL);

#if defined(XP_WIN)
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    unsigned long long raw_us = 0.1 *
        (((unsigned long long) now.dwHighDateTime << 32ULL) |
         (unsigned long long) now.dwLowDateTime);
    unsigned int s = raw_us / 1000000L;
    p->mStartSecond = s;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    p->mStartSecond = tv.tv_sec;
#endif
    JSObject *obj;
    if (JS_IsConstructing(cx, vp)) {
        obj = JS_NewObject(cx, &ethogram_class, NULL, NULL);
        if (!obj)
            return JS_FALSE;
    } else {
        obj = JS_THIS_OBJECT(cx, vp);
    }

    jsval filenames = OBJECT_TO_JSVAL(p->filenames());
    if (!JS_DefineProperty(cx, obj, "filenames", filenames,
                           NULL, NULL, JSPROP_READONLY|JSPROP_PERMANENT))
        return JS_FALSE;

    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
    JS_SetPrivate(cx, obj, p);
    return JS_TRUE;
}

static void
ethogram_finalize(JSContext *cx, JSObject *obj)
{
    EthogramEventBuffer *p;
    p = (EthogramEventBuffer *) JS_GetInstancePrivate(cx, obj, &ethogram_class, NULL);
    if (!p)
        return;

    p->removeScripts(cx);

    JS_free(cx, p);
}

static JSBool
ethogram_addScript(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    char *filename = NULL;
    jsval *argv = JS_ARGV(cx, vp);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return false;
    if (argc < 1) {
        /* silently ignore no args */
        JS_SET_RVAL(cx, vp, JSVAL_VOID);
        return true;
    }
    if (JSVAL_IS_STRING(argv[0])) {
        str = JSVAL_TO_STRING(argv[0]);
        filename = js_DeflateString(cx, str->chars(), str->length());
        if (!filename)
            return false;
    }

    EthogramEventBuffer *p = (EthogramEventBuffer *) JS_GetInstancePrivate(cx, obj, &ethogram_class, argv);

    p->addScript(cx, obj, filename, str);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    jsval dummy;
    JS_CallFunctionName(cx, p->filenames(), "push", 1, argv, &dummy);
    return true;
}

static JSBool
ethogram_getAllEvents(JSContext *cx, uintN argc, jsval *vp)
{
    EthogramEventBuffer *p;
    jsval *argv = JS_ARGV(cx, vp);

    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    p = (EthogramEventBuffer *) JS_GetInstancePrivate(cx, obj, &ethogram_class, argv);
    if (!p)
        return JS_FALSE;

    if (p->isEmpty()) {
        JS_SET_RVAL(cx, vp, JSVAL_NULL);
        return JS_TRUE;
    }

    JSObject *rarray = JS_NewArrayObject(cx, 0, NULL);
    if (rarray == NULL) {
        JS_SET_RVAL(cx, vp, JSVAL_NULL);
        return JS_TRUE;
    }

    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(rarray));

    for (int i = 0; !p->isEmpty(); i++) {

        JSObject *x = JS_NewObject(cx, NULL, NULL, NULL);
        if (x == NULL)
            return JS_FALSE;

        EthogramEvent *e = p->pop();

        jsval state = INT_TO_JSVAL(e->s);
        jsval reason = INT_TO_JSVAL(e->r);
        jsval ts = INT_TO_JSVAL(e->ts);
        jsval tus = INT_TO_JSVAL(e->tus);

        jsval filename = STRING_TO_JSVAL(e->filename);
        jsval lineno = INT_TO_JSVAL(e->lineno);

        if (!JS_SetProperty(cx, x, "state", &state))
            return JS_FALSE;
        if (!JS_SetProperty(cx, x, "reason", &reason))
            return JS_FALSE;
        if (!JS_SetProperty(cx, x, "ts", &ts))
            return JS_FALSE;
        if (!JS_SetProperty(cx, x, "tus", &tus))
            return JS_FALSE;

        if (!JS_SetProperty(cx, x, "filename", &filename))
            return JS_FALSE;
        if (!JS_SetProperty(cx, x, "lineno", &lineno))
            return JS_FALSE;

        jsval element = OBJECT_TO_JSVAL(x);
        JS_SetElement(cx, rarray, i, &element);
    }

    return JS_TRUE;
}

static JSBool
ethogram_getNextEvent(JSContext *cx, uintN argc, jsval *vp)
{
    EthogramEventBuffer *p;
    jsval *argv = JS_ARGV(cx, vp);

    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    p = (EthogramEventBuffer *) JS_GetInstancePrivate(cx, obj, &ethogram_class, argv);
    if (!p)
        return JS_FALSE;

    JSObject *x = JS_NewObject(cx, NULL, NULL, NULL);
    if (x == NULL)
        return JS_FALSE;

    if (p->isEmpty()) {
        JS_SET_RVAL(cx, vp, JSVAL_NULL);
        return JS_TRUE;
    }

    EthogramEvent *e = p->pop();
    jsval state = INT_TO_JSVAL(e->s);
    jsval reason = INT_TO_JSVAL(e->r);
    jsval ts = INT_TO_JSVAL(e->ts);
    jsval tus = INT_TO_JSVAL(e->tus);

    jsval filename = STRING_TO_JSVAL(e->filename);
    jsval lineno = INT_TO_JSVAL(e->lineno);

    if (!JS_SetProperty(cx, x, "state", &state))
        return JS_FALSE;
    if (!JS_SetProperty(cx, x, "reason", &reason))
        return JS_FALSE;
    if (!JS_SetProperty(cx, x, "ts", &ts))
        return JS_FALSE;
    if (!JS_SetProperty(cx, x, "tus", &tus))
        return JS_FALSE;
    if (!JS_SetProperty(cx, x, "filename", &filename))
        return JS_FALSE;

    if (!JS_SetProperty(cx, x, "lineno", &lineno))
        return JS_FALSE;

    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(x));

    return JS_TRUE;
}

static JSFunctionSpec ethogram_methods[] = {
    JS_FN("addScript",    ethogram_addScript,    1,0),
    JS_FN("getAllEvents", ethogram_getAllEvents, 0,0),
    JS_FN("getNextEvent", ethogram_getNextEvent, 0,0),
    JS_FS_END
};

/*
 * An |Ethogram| organizes the output of a collection of files that should be
 * monitored together. A single object gets events for the group.
 */
JS_FRIEND_API(JSBool)
js_InitEthogram(JSContext *cx, uintN argc, jsval *vp)
{
    if (!traceVisScriptTable) {
        traceVisScriptTable = JS_NewHashTable(8, JS_HashString, compare_strings,
                                         NULL, NULL, NULL);
    }

    JS_InitClass(cx, JS_GetGlobalObject(cx), NULL, &ethogram_class,
                 ethogram_construct, 0, NULL, ethogram_methods,
                 NULL, NULL);

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

JS_FRIEND_API(JSBool)
js_ShutdownEthogram(JSContext *cx, uintN argc, jsval *vp)
{
    if (traceVisScriptTable)
        JS_HashTableDestroy(traceVisScriptTable);

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

#endif /* MOZ_TRACEVIS */

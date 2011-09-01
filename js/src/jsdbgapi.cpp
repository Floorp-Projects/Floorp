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
 *   Nick Fitzgerald <nfitzgerald@mozilla.com>
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
#include <stdarg.h>
#include "jsprvtd.h"
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h"
#include "jsclist.h"
#include "jshashtable.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstaticcheck.h"
#include "jsstr.h"
#include "jswatchpoint.h"
#include "jswrapper.h"
#include "vm/Debugger.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsinterpinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"

#include "vm/Stack-inl.h"

#include "jsautooplen.h"

#ifdef __APPLE__
#include "sharkctl.h"
#endif

using namespace js;
using namespace js::gc;

JS_PUBLIC_API(JSBool)
JS_GetDebugMode(JSContext *cx)
{
    return cx->compartment->debugMode();
}

JS_PUBLIC_API(JSBool)
JS_SetDebugMode(JSContext *cx, JSBool debug)
{
    return JS_SetDebugModeForCompartment(cx, cx->compartment, debug);
}

JS_PUBLIC_API(void)
JS_SetRuntimeDebugMode(JSRuntime *rt, JSBool debug)
{
    rt->debugMode = !!debug;
}

namespace js {

void
ScriptDebugPrologue(JSContext *cx, StackFrame *fp)
{
    JS_ASSERT(fp == cx->fp());

    if (fp->isFramePushedByExecute()) {
        if (JSInterpreterHook hook = cx->debugHooks->executeHook)
            fp->setHookData(hook(cx, Jsvalify(fp), true, 0, cx->debugHooks->executeHookData));
    } else {
        if (JSInterpreterHook hook = cx->debugHooks->callHook)
            fp->setHookData(hook(cx, Jsvalify(fp), true, 0, cx->debugHooks->callHookData));
    }
    Debugger::onEnterFrame(cx);
}

bool
ScriptDebugEpilogue(JSContext *cx, StackFrame *fp, bool okArg)
{
    JS_ASSERT(fp == cx->fp());
    JSBool ok = okArg;

    if (void *hookData = fp->maybeHookData()) {
        if (fp->isFramePushedByExecute()) {
            if (JSInterpreterHook hook = cx->debugHooks->executeHook)
                hook(cx, Jsvalify(fp), false, &ok, hookData);
        } else {
            if (JSInterpreterHook hook = cx->debugHooks->callHook)
                hook(cx, Jsvalify(fp), false, &ok, hookData);
        }
    }
    Debugger::onLeaveFrame(cx);

    return ok;
}

} /* namespace js */

JS_FRIEND_API(JSBool)
JS_SetDebugModeForCompartment(JSContext *cx, JSCompartment *comp, JSBool debug)
{
    return comp->setDebugModeFromC(cx, !!debug);
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

JS_PUBLIC_API(JSBool)
JS_SetSingleStepMode(JSContext *cx, JSScript *script, JSBool singleStep)
{
    assertSameCompartment(cx, script);
    if (!CheckDebugMode(cx))
        return JS_FALSE;

    return script->setStepModeFlag(cx, singleStep);
}

jsbytecode *
js_UntrapScriptCode(JSContext *cx, JSScript *script)
{
    jsbytecode *code = script->code;
    BreakpointSiteMap &sites = script->compartment()->breakpointSites;
    for (BreakpointSiteMap::Range r = sites.all(); !r.empty(); r.popFront()) {
        BreakpointSite *site = r.front().value;
        if (site->script == script && size_t(site->pc - script->code) < script->length) {
            if (code == script->code) {
                size_t nbytes = script->length * sizeof(jsbytecode);
                jssrcnote *notes = script->notes();
                jssrcnote *sn;
                for (sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn))
                    continue;
                nbytes += (sn - notes + 1) * sizeof *sn;

                code = (jsbytecode *) cx->malloc_(nbytes);
                if (!code)
                    break;
                memcpy(code, script->code, nbytes);
                GetGSNCache(cx)->purge();
            }
            code[site->pc - script->code] = site->realOpcode;
        }
    }
    return code;
}

JS_PUBLIC_API(JSBool)
JS_SetTrap(JSContext *cx, JSScript *script, jsbytecode *pc, JSTrapHandler handler, jsval closure)
{
    if (!CheckDebugMode(cx))
        return false;

    BreakpointSite *site = script->compartment()->getOrCreateBreakpointSite(cx, script, pc, NULL);
    if (!site)
        return false;
    site->setTrap(cx, handler, Valueify(closure));
    return true;
}

JS_PUBLIC_API(JSOp)
JS_GetTrapOpcode(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    BreakpointSite *site = script->compartment()->getBreakpointSite(pc);
    return site ? site->realOpcode : JSOp(*pc);
}

JS_PUBLIC_API(void)
JS_ClearTrap(JSContext *cx, JSScript *script, jsbytecode *pc,
             JSTrapHandler *handlerp, jsval *closurep)
{
    if (BreakpointSite *site = script->compartment()->getBreakpointSite(pc)) {
        site->clearTrap(cx, NULL, handlerp, Valueify(closurep));
    } else {
        if (handlerp)
            *handlerp = NULL;
        if (closurep)
            *closurep = JSVAL_VOID;
    }
}

JS_PUBLIC_API(void)
JS_ClearScriptTraps(JSContext *cx, JSScript *script)
{
    script->compartment()->clearTraps(cx, script);
}

JS_PUBLIC_API(void)
JS_ClearAllTrapsForCompartment(JSContext *cx)
{
    cx->compartment->clearTraps(cx, NULL);
}

#ifdef JS_TRACER
static void
JITInhibitingHookChange(JSRuntime *rt, bool wasInhibited)
{
    if (wasInhibited) {
        if (!rt->debuggerInhibitsJIT()) {
            for (JSCList *cl = rt->contextList.next; cl != &rt->contextList; cl = cl->next)
                JSContext::fromLinkField(cl)->updateJITEnabled();
        }
    } else if (rt->debuggerInhibitsJIT()) {
        for (JSCList *cl = rt->contextList.next; cl != &rt->contextList; cl = cl->next)
            JSContext::fromLinkField(cl)->traceJitEnabled = false;
    }
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

JS_PUBLIC_API(JSBool)
JS_SetWatchPoint(JSContext *cx, JSObject *obj, jsid id,
                 JSWatchPointHandler handler, JSObject *closure)
{
    assertSameCompartment(cx, obj);
    id = js_CheckForStringIndex(id);

    JSObject *origobj;
    Value v;
    uintN attrs;
    jsid propid;

    origobj = obj;
    OBJ_TO_INNER_OBJECT(cx, obj);
    if (!obj)
        return false;

    AutoValueRooter idroot(cx);
    if (JSID_IS_INT(id)) {
        propid = id;
    } else {
        if (!js_ValueToStringId(cx, IdToValue(id), &propid))
            return false;
        propid = js_CheckForStringIndex(propid);
        idroot.set(IdToValue(propid));
    }

    /*
     * If, by unwrapping and innerizing, we changed the object, check
     * again to make sure that we're allowed to set a watch point.
     */
    if (origobj != obj && !CheckAccess(cx, obj, propid, JSACC_WATCH, &v, &attrs))
        return false;

    if (!obj->isNative()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_WATCH,
                             obj->getClass()->name);
        return false;
    }

    types::MarkTypePropertyConfigured(cx, obj, propid);

    WatchpointMap *wpmap = cx->compartment->watchpointMap;
    if (!wpmap) {
        wpmap = cx->runtime->new_<WatchpointMap>();
        if (!wpmap || !wpmap->init()) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        cx->compartment->watchpointMap = wpmap;
    }
    return wpmap->watch(cx, obj, id, handler, closure);
}

JS_PUBLIC_API(JSBool)
JS_ClearWatchPoint(JSContext *cx, JSObject *obj, jsid id,
                   JSWatchPointHandler *handlerp, JSObject **closurep)
{
    assertSameCompartment(cx, obj, id);

    id = js_CheckForStringIndex(id);
    if (WatchpointMap *wpmap = cx->compartment->watchpointMap)
        wpmap->unwatch(obj, id, handlerp, closurep);
    return true;
}

JS_PUBLIC_API(JSBool)
JS_ClearWatchPointsForObject(JSContext *cx, JSObject *obj)
{
    assertSameCompartment(cx, obj);

    if (WatchpointMap *wpmap = cx->compartment->watchpointMap)
        wpmap->unwatchObject(obj);
    return true;
}

JS_PUBLIC_API(JSBool)
JS_ClearAllWatchPoints(JSContext *cx)
{
    if (JSCompartment *comp = cx->compartment) {
        if (WatchpointMap *wpmap = comp->watchpointMap)
            wpmap->clear();
    }
    return true;
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

JS_PUBLIC_API(jsbytecode *)
JS_EndPC(JSContext *cx, JSScript *script)
{
    return script->code + script->length;
}

JS_PUBLIC_API(JSBool)
JS_GetLinePCs(JSContext *cx, JSScript *script,
              uintN startLine, uintN maxLines,
              uintN* count, uintN** retLines, jsbytecode*** retPCs)
{
    uintN* lines;
    jsbytecode** pcs;
    size_t len = (script->length > maxLines ? maxLines : script->length);
    lines = (uintN*) cx->malloc_(len * sizeof(uintN));
    if (!lines)
        return JS_FALSE;

    pcs = (jsbytecode**) cx->malloc_(len * sizeof(jsbytecode*));
    if (!pcs) {
        cx->free_(lines);
        return JS_FALSE;
    }

    uintN lineno = script->lineno;
    uintN offset = 0;
    uintN i = 0;
    for (jssrcnote *sn = script->notes(); !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        offset += SN_DELTA(sn);
        JSSrcNoteType type = (JSSrcNoteType) SN_TYPE(sn);
        if (type == SRC_SETLINE || type == SRC_NEWLINE) {
            if (type == SRC_SETLINE)
                lineno = (uintN) js_GetSrcNoteOffset(sn, 0);
            else
                lineno++;

            if (lineno >= startLine) {
                lines[i] = lineno;
                pcs[i] = script->code + offset;
                if (++i >= maxLines)
                    break;
            }
        }
    }

    *count = i;
    if (retLines)
        *retLines = lines;
    else
        cx->free_(lines);

    if (retPCs)
        *retPCs = pcs;
    else
        cx->free_(pcs);

    return JS_TRUE;
}

JS_PUBLIC_API(uintN)
JS_GetFunctionArgumentCount(JSContext *cx, JSFunction *fun)
{
    return fun->nargs;
}

JS_PUBLIC_API(JSBool)
JS_FunctionHasLocalNames(JSContext *cx, JSFunction *fun)
{
    return fun->script()->bindings.hasLocalNames();
}

extern JS_PUBLIC_API(jsuword *)
JS_GetFunctionLocalNameArray(JSContext *cx, JSFunction *fun, void **markp)
{
    Vector<JSAtom *> localNames(cx);
    if (!fun->script()->bindings.getLocalNameArray(cx, &localNames))
        return NULL;

    /* Munge data into the API this method implements.  Avert your eyes! */
    *markp = JS_ARENA_MARK(&cx->tempPool);

    jsuword *names;
    JS_ARENA_ALLOCATE_CAST(names, jsuword *, &cx->tempPool, localNames.length() * sizeof *names);
    if (!names) {
        js_ReportOutOfMemory(cx);
        return NULL;
    }

    memcpy(names, localNames.begin(), localNames.length() * sizeof(jsuword));
    return names;
}

extern JS_PUBLIC_API(JSAtom *)
JS_LocalNameToAtom(jsuword w)
{
    return JS_LOCAL_NAME_TO_ATOM(w);
}

extern JS_PUBLIC_API(JSString *)
JS_AtomKey(JSAtom *atom)
{
    return atom;
}

extern JS_PUBLIC_API(void)
JS_ReleaseFunctionLocalNameArray(JSContext *cx, void *mark)
{
    JS_ARENA_RELEASE(&cx->tempPool, mark);
}

JS_PUBLIC_API(JSScript *)
JS_GetFunctionScript(JSContext *cx, JSFunction *fun)
{
    return fun->maybeScript();
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
    StackFrame *fp = Valueify(*iteratorp);
    *iteratorp = Jsvalify((fp == NULL) ? js_GetTopStackFrame(cx, FRAME_EXPAND_ALL) : fp->prev());
    return *iteratorp;
}

JS_PUBLIC_API(JSScript *)
JS_GetFrameScript(JSContext *cx, JSStackFrame *fp)
{
    return Valueify(fp)->maybeScript();
}

JS_PUBLIC_API(jsbytecode *)
JS_GetFramePC(JSContext *cx, JSStackFrame *fp)
{
    return Valueify(fp)->pcQuadratic(cx->stack);
}

JS_PUBLIC_API(JSStackFrame *)
JS_GetScriptedCaller(JSContext *cx, JSStackFrame *fp)
{
    return Jsvalify(js_GetScriptedCaller(cx, Valueify(fp)));
}

JS_PUBLIC_API(void *)
JS_GetFrameAnnotation(JSContext *cx, JSStackFrame *fpArg)
{
    StackFrame *fp = Valueify(fpArg);
    if (fp->annotation() && fp->isScriptFrame()) {
        JSPrincipals *principals = fp->scopeChain().principals(cx);

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
    Valueify(fp)->setAnnotation(annotation);
}

JS_PUBLIC_API(void *)
JS_GetFramePrincipalArray(JSContext *cx, JSStackFrame *fp)
{
    JSPrincipals *principals;

    principals = Valueify(fp)->scopeChain().principals(cx);
    if (!principals)
        return NULL;
    return principals->getPrincipalArray(cx, principals);
}

JS_PUBLIC_API(JSBool)
JS_IsScriptFrame(JSContext *cx, JSStackFrame *fp)
{
    return !Valueify(fp)->isDummyFrame();
}

/* this is deprecated, use JS_GetFrameScopeChain instead */
JS_PUBLIC_API(JSObject *)
JS_GetFrameObject(JSContext *cx, JSStackFrame *fp)
{
    return &Valueify(fp)->scopeChain();
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameScopeChain(JSContext *cx, JSStackFrame *fpArg)
{
    StackFrame *fp = Valueify(fpArg);
    JS_ASSERT(cx->stack.containsSlow(fp));

    js::AutoCompartment ac(cx, &fp->scopeChain());
    if (!ac.enter())
        return NULL;

    /* Force creation of argument and call objects if not yet created */
    (void) JS_GetFrameCallObject(cx, Jsvalify(fp));
    return GetScopeChain(cx, fp);
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameCallObject(JSContext *cx, JSStackFrame *fpArg)
{
    StackFrame *fp = Valueify(fpArg);
    JS_ASSERT(cx->stack.containsSlow(fp));

    if (!fp->isFunctionFrame())
        return NULL;

    js::AutoCompartment ac(cx, &fp->scopeChain());
    if (!ac.enter())
        return NULL;

    /*
     * XXX ill-defined: null return here means error was reported, unlike a
     *     null returned above or in the #else
     */
    if (!fp->hasCallObj() && fp->isNonEvalFunctionFrame())
        return CreateFunCallObject(cx, fp);
    return &fp->callObj();
}

JS_PUBLIC_API(JSBool)
JS_GetFrameThis(JSContext *cx, JSStackFrame *fpArg, jsval *thisv)
{
    StackFrame *fp = Valueify(fpArg);
    if (fp->isDummyFrame())
        return false;

    js::AutoCompartment ac(cx, &fp->scopeChain());
    if (!ac.enter())
        return false;

    if (!ComputeThis(cx, fp))
        return false;
    *thisv = Jsvalify(fp->thisValue());
    return true;
}

JS_PUBLIC_API(JSFunction *)
JS_GetFrameFunction(JSContext *cx, JSStackFrame *fp)
{
    return Valueify(fp)->maybeFun();
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameFunctionObject(JSContext *cx, JSStackFrame *fpArg)
{
    StackFrame *fp = Valueify(fpArg);
    if (!fp->isFunctionFrame())
        return NULL;

    JS_ASSERT(fp->callee().isFunction());
    JS_ASSERT(fp->callee().getPrivate() == fp->fun());
    return &fp->callee();
}

JS_PUBLIC_API(JSBool)
JS_IsConstructorFrame(JSContext *cx, JSStackFrame *fp)
{
    return Valueify(fp)->isConstructing();
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameCalleeObject(JSContext *cx, JSStackFrame *fp)
{
    return Valueify(fp)->maybeCalleev().toObjectOrNull();
}

JS_PUBLIC_API(JSBool)
JS_GetValidFrameCalleeObject(JSContext *cx, JSStackFrame *fp, jsval *vp)
{
    Value v;

    if (!Valueify(fp)->getValidCalleeObject(cx, &v))
        return false;
    *vp = v.isObject() ? Jsvalify(v) : JSVAL_VOID;
    *vp = Jsvalify(v);
    return true;
}

JS_PUBLIC_API(JSBool)
JS_IsDebuggerFrame(JSContext *cx, JSStackFrame *fp)
{
    return Valueify(fp)->isDebuggerFrame();
}

JS_PUBLIC_API(JSBool)
JS_IsGlobalFrame(JSContext *cx, JSStackFrame *fp)
{
    return Valueify(fp)->isGlobalFrame();
}

JS_PUBLIC_API(jsval)
JS_GetFrameReturnValue(JSContext *cx, JSStackFrame *fp)
{
    return Jsvalify(Valueify(fp)->returnValue());
}

JS_PUBLIC_API(void)
JS_SetFrameReturnValue(JSContext *cx, JSStackFrame *fpArg, jsval rval)
{
    StackFrame *fp = Valueify(fpArg);
#ifdef JS_METHODJIT
    JS_ASSERT_IF(fp->isScriptFrame(), fp->script()->debugMode);
#endif
    assertSameCompartment(cx, fp, rval);
    fp->setReturnValue(Valueify(rval));
}

/************************************************************************/

JS_PUBLIC_API(const char *)
JS_GetScriptFilename(JSContext *cx, JSScript *script)
{
    return script->filename;
}

JS_PUBLIC_API(const jschar *)
JS_GetScriptSourceMap(JSContext *cx, JSScript *script)
{
    return script->sourceMap;
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
JS_EvaluateUCInStackFrame(JSContext *cx, JSStackFrame *fpArg,
                          const jschar *chars, uintN length,
                          const char *filename, uintN lineno,
                          jsval *rval)
{
    JS_ASSERT_NOT_ON_TRACE(cx);

    if (!CheckDebugMode(cx))
        return false;

    JSObject *scobj = JS_GetFrameScopeChain(cx, fpArg);
    if (!scobj)
        return false;

    js::AutoCompartment ac(cx, scobj);
    if (!ac.enter())
        return false;

    StackFrame *fp = Valueify(fpArg);
    return EvaluateInScope(cx, scobj, fp, chars, length, filename, lineno, Valueify(rval));
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

    chars = InflateString(cx, bytes, &len);
    if (!chars)
        return JS_FALSE;
    length = (uintN) len;
    ok = JS_EvaluateUCInStackFrame(cx, fp, chars, length, filename, lineno,
                                   rval);
    cx->free_(chars);

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
            JS_ASSERT(JSID_IS_EMPTY(shape->propid));
            shape = NULL;
        }
    }

    return *iteratorp = reinterpret_cast<JSScopeProperty *>(const_cast<Shape *>(shape));
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyDesc(JSContext *cx, JSObject *obj, JSScopeProperty *sprop,
                   JSPropertyDesc *pd)
{
    assertSameCompartment(cx, obj);
    Shape *shape = (Shape *) sprop;
    pd->id = IdToJsval(shape->propid);

    JSBool wasThrowing = cx->isExceptionPending();
    Value lastException = UndefinedValue();
    if (wasThrowing)
        lastException = cx->getPendingException();
    cx->clearPendingException();

    if (!js_GetProperty(cx, obj, shape->propid, Valueify(&pd->value))) {
        if (!cx->isExceptionPending()) {
            pd->flags = JSPD_ERROR;
            pd->value = JSVAL_VOID;
        } else {
            pd->flags = JSPD_EXCEPTION;
            pd->value = Jsvalify(cx->getPendingException());
        }
    } else {
        pd->flags = 0;
    }

    if (wasThrowing)
        cx->setPendingException(lastException);

    pd->flags |= (shape->enumerable() ? JSPD_ENUMERATE : 0)
              |  (!shape->writable()  ? JSPD_READONLY  : 0)
              |  (!shape->configurable() ? JSPD_PERMANENT : 0);
    pd->spare = 0;
    if (shape->getter() == GetCallArg) {
        pd->slot = shape->shortid;
        pd->flags |= JSPD_ARGUMENT;
    } else if (shape->getter() == GetCallVar) {
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
                pd->alias = IdToJsval(aprop.propid);
                break;
            }
        }
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyDescArray(JSContext *cx, JSObject *obj, JSPropertyDescArray *pda)
{
    assertSameCompartment(cx, obj);
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
    JSPropertyDesc *pd = (JSPropertyDesc *) cx->malloc_(size_t(n) * sizeof(JSPropertyDesc));
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
    cx->free_(pd);
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
    return obj->slotsAndStructSize();
}

static size_t
GetAtomTotalSize(JSContext *cx, JSAtom *atom)
{
    size_t nbytes;

    nbytes = sizeof(JSAtom *) + sizeof(JSDHashEntryStub);
    nbytes += sizeof(JSString);
    nbytes += (atom->length() + 1) * sizeof(jschar);
    return nbytes;
}

JS_PUBLIC_API(size_t)
JS_GetFunctionTotalSize(JSContext *cx, JSFunction *fun)
{
    size_t nbytes;

    nbytes = sizeof *fun;
    nbytes += JS_GetObjectTotalSize(cx, fun);
    if (fun->isInterpreted())
        nbytes += JS_GetScriptTotalSize(cx, fun->script());
    if (fun->atom)
        nbytes += GetAtomTotalSize(cx, fun->atom);
    return nbytes;
}

#include "jsemit.h"

JS_PUBLIC_API(size_t)
JS_GetScriptTotalSize(JSContext *cx, JSScript *script)
{
    size_t nbytes, pbytes;
    jssrcnote *sn, *notes;
    JSObjectArray *objarray;
    JSPrincipals *principals;

    nbytes = sizeof *script;
    if (script->u.object)
        nbytes += JS_GetObjectTotalSize(cx, script->u.object);

    nbytes += script->length * sizeof script->code[0];
    nbytes += script->natoms * sizeof script->atoms[0];
    for (size_t i = 0; i < script->natoms; i++)
        nbytes += GetAtomTotalSize(cx, script->atoms[i]);

    if (script->filename)
        nbytes += strlen(script->filename) + 1;

    notes = script->notes();
    for (sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn))
        continue;
    nbytes += (sn - notes + 1) * sizeof *sn;

    if (JSScript::isValidOffset(script->objectsOffset)) {
        objarray = script->objects();
        size_t i = objarray->length;
        nbytes += sizeof *objarray + i * sizeof objarray->vector[0];
        do {
            nbytes += JS_GetObjectTotalSize(cx, objarray->vector[--i]);
        } while (i != 0);
    }

    if (JSScript::isValidOffset(script->regexpsOffset)) {
        objarray = script->regexps();
        size_t i = objarray->length;
        nbytes += sizeof *objarray + i * sizeof objarray->vector[0];
        do {
            nbytes += JS_GetObjectTotalSize(cx, objarray->vector[--i]);
        } while (i != 0);
    }

    if (JSScript::isValidOffset(script->trynotesOffset)) {
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

/************************************************************************/

/* Profiling-related API */

/* Thread-unsafe error management */

static char gLastError[2000];

static void
#ifdef __GNUC__
__attribute__((unused,format(printf,1,2)))
#endif
UnsafeError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    (void) vsnprintf(gLastError, sizeof(gLastError), format, args);
    va_end(args);

    gLastError[sizeof(gLastError) - 1] = '\0';
}

JS_PUBLIC_API(const char *)
JS_UnsafeGetLastProfilingError()
{
    return gLastError;
}

JS_PUBLIC_API(JSBool)
JS_StartProfiling(const char *profileName)
{
    JSBool ok = JS_TRUE;
#if defined(MOZ_SHARK) && defined(__APPLE__)
    if (!Shark::Start()) {
        UnsafeError("Failed to start Shark for %s", profileName);
        ok = JS_FALSE;
    }
#endif
#ifdef MOZ_VTUNE
    if (!js_StartVtune(profileName))
        ok = JS_FALSE;
#endif
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_StopProfiling(const char *profileName)
{
    JSBool ok = JS_TRUE;
#if defined(MOZ_SHARK) && defined(__APPLE__)
    Shark::Stop();
#endif
#ifdef MOZ_VTUNE
    if (!js_StopVtune())
        ok = JS_FALSE;
#endif
    return ok;
}

/*
 * Start or stop whatever platform- and configuration-specific profiling
 * backends are available.
 */
static JSBool
ControlProfilers(bool toState)
{
    JSBool ok = JS_TRUE;

    if (! Probes::ProfilingActive && toState) {
#if defined(MOZ_SHARK) && defined(__APPLE__)
        if (!Shark::Start()) {
            UnsafeError("Failed to start Shark");
            ok = JS_FALSE;
        }
#endif
#ifdef MOZ_CALLGRIND
        if (! js_StartCallgrind()) {
            UnsafeError("Failed to start Callgrind");
            ok = JS_FALSE;
        }
#endif
#ifdef MOZ_VTUNE
        if (! js_ResumeVtune())
            ok = JS_FALSE;
#endif
    } else if (Probes::ProfilingActive && ! toState) {
#if defined(MOZ_SHARK) && defined(__APPLE__)
        Shark::Stop();
#endif
#ifdef MOZ_CALLGRIND
        if (! js_StopCallgrind()) {
            UnsafeError("failed to stop Callgrind");
            ok = JS_FALSE;
        }
#endif
#ifdef MOZ_VTUNE
        if (! js_PauseVtune())
            ok = JS_FALSE;
#endif
    }

    Probes::ProfilingActive = toState;

    return ok;
}

/*
 * Pause/resume whatever profiling mechanism is currently compiled
 * in, if applicable. This will not affect things like dtrace.
 *
 * Do not mix calls to these APIs with calls to the individual
 * profilers' pause/resume functions, because only overall state is
 * tracked, not the state of each profiler.
 */
JS_PUBLIC_API(JSBool)
JS_PauseProfilers(const char *profileName)
{
    return ControlProfilers(false);
}

JS_PUBLIC_API(JSBool)
JS_ResumeProfilers(const char *profileName)
{
    return ControlProfilers(true);
}

JS_PUBLIC_API(JSBool)
JS_DumpProfile(const char *outfile, const char *profileName)
{
    JSBool ok = JS_TRUE;
#ifdef MOZ_CALLGRIND
    js_DumpCallgrind(outfile);
#endif
    return ok;
}

#ifdef MOZ_PROFILING

struct RequiredStringArg {
    JSContext *mCx;
    char *mBytes;
    RequiredStringArg(JSContext *cx, uintN argc, jsval *vp, size_t argi, const char *caller)
        : mCx(cx), mBytes(NULL)
    {
        if (argc <= argi) {
            JS_ReportError(cx, "%s: not enough arguments", caller);
        } else if (!JSVAL_IS_STRING(JS_ARGV(cx, vp)[argi])) {
            JS_ReportError(cx, "%s: invalid arguments (string expected)", caller);
        } else {
            mBytes = JS_EncodeString(cx, JSVAL_TO_STRING(JS_ARGV(cx, vp)[argi]));
        }
    }
    operator void*() {
        return (void*) mBytes;
    }
    ~RequiredStringArg() {
        if (mBytes)
            mCx->free_(mBytes);
    }
};

static JSBool
StartProfiling(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc == 0) {
        JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(JS_StartProfiling(NULL)));
        return JS_TRUE;
    }

    RequiredStringArg profileName(cx, argc, vp, 0, "startProfiling");
    if (!profileName)
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(JS_StartProfiling(profileName.mBytes)));
    return JS_TRUE;
}

static JSBool
StopProfiling(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc == 0) {
        JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(JS_StopProfiling(NULL)));
        return JS_TRUE;
    }

    RequiredStringArg profileName(cx, argc, vp, 0, "stopProfiling");
    if (!profileName)
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(JS_StopProfiling(profileName.mBytes)));
    return JS_TRUE;
}

static JSBool
PauseProfilers(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc == 0) {
        JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(JS_PauseProfilers(NULL)));
        return JS_TRUE;
    }

    RequiredStringArg profileName(cx, argc, vp, 0, "pauseProfiling");
    if (!profileName)
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(JS_PauseProfilers(profileName.mBytes)));
    return JS_TRUE;
}

static JSBool
ResumeProfilers(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc == 0) {
        JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(JS_ResumeProfilers(NULL)));
        return JS_TRUE;
    }

    RequiredStringArg profileName(cx, argc, vp, 0, "resumeProfiling");
    if (!profileName)
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(JS_ResumeProfilers(profileName.mBytes)));
    return JS_TRUE;
}

/* Usage: DumpProfile([filename[, profileName]]) */
static JSBool
DumpProfile(JSContext *cx, uintN argc, jsval *vp)
{
    bool ret;
    if (argc == 0) {
        ret = JS_DumpProfile(NULL, NULL);
    } else {
        RequiredStringArg filename(cx, argc, vp, 0, "dumpProfile");
        if (!filename)
            return JS_FALSE;

        if (argc == 1) {
            ret = JS_DumpProfile(filename.mBytes, NULL);
        } else {
            RequiredStringArg profileName(cx, argc, vp, 1, "dumpProfile");
            if (!profileName)
                return JS_FALSE;

            ret = JS_DumpProfile(filename.mBytes, profileName.mBytes);
        }
    }

    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(ret));
    return true;
}

#ifdef MOZ_SHARK

static JSBool
IgnoreAndReturnTrue(JSContext *cx, uintN argc, jsval *vp)
{
    JS_SET_RVAL(cx, vp, JSVAL_TRUE);
    return true;
}

#endif

#ifdef MOZ_CALLGRIND
static JSBool
StartCallgrind(JSContext *cx, uintN argc, jsval *vp)
{
    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(js_StartCallgrind()));
    return JS_TRUE;
}

static JSBool
StopCallgrind(JSContext *cx, uintN argc, jsval *vp)
{
    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(js_StopCallgrind()));
    return JS_TRUE;
}

static JSBool
DumpCallgrind(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc == 0) {
        JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(js_DumpCallgrind(NULL)));
        return JS_TRUE;
    }

    RequiredStringArg outFile(cx, argc, vp, 0, "dumpCallgrind");
    if (!outFile)
        return JS_FALSE;

    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(js_DumpCallgrind(outFile.mBytes)));
    return JS_TRUE;
}    
#endif

#ifdef MOZ_VTUNE
static JSBool
StartVtune(JSContext *cx, uintN argc, jsval *vp)
{
    RequiredStringArg profileName(cx, argc, vp, 0, "startVtune");
    if (!profileName)
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(js_StartVtune(profileName.mBytes)));
    return JS_TRUE;
}

static JSBool
StopVtune(JSContext *cx, uintN argc, jsval *vp)
{
    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(js_StopVtune()));
    return JS_TRUE;
}

static JSBool
PauseVtune(JSContext *cx, uintN argc, jsval *vp)
{
    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(js_PauseVtune()));
    return JS_TRUE;
}

static JSBool
ResumeVtune(JSContext *cx, uintN argc, jsval *vp)
{
    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(js_ResumeVtune()));
    return JS_TRUE;
}
#endif

static JSFunctionSpec profiling_functions[] = {
    JS_FN("startProfiling",  StartProfiling,      1,0),
    JS_FN("stopProfiling",   StopProfiling,       1,0),
    JS_FN("pauseProfilers",  PauseProfilers,      1,0),
    JS_FN("resumeProfilers", ResumeProfilers,     1,0),
    JS_FN("dumpProfile",     DumpProfile,         2,0),
#ifdef MOZ_SHARK
    /* Keep users of the old shark API happy. */
    JS_FN("connectShark",    IgnoreAndReturnTrue, 0,0),
    JS_FN("disconnectShark", IgnoreAndReturnTrue, 0,0),
    JS_FN("startShark",      StartProfiling,      0,0),
    JS_FN("stopShark",       StopProfiling,       0,0),
#endif
#ifdef MOZ_CALLGRIND
    JS_FN("startCallgrind", StartCallgrind,       0,0),
    JS_FN("stopCallgrind",  StopCallgrind,        0,0),
    JS_FN("dumpCallgrind",  DumpCallgrind,        1,0),
#endif
#ifdef MOZ_VTUNE
    JS_FN("startVtune",     js_StartVtune,        1,0),
    JS_FN("stopVtune",      js_StopVtune,         0,0),
    JS_FN("pauseVtune",     js_PauseVtune,        0,0),
    JS_FN("resumeVtune",    js_ResumeVtune,       0,0),
#endif
    JS_FS_END
};

#endif

JS_PUBLIC_API(JSBool)
JS_DefineProfilingFunctions(JSContext *cx, JSObject *obj)
{
    assertSameCompartment(cx, obj);
#ifdef MOZ_PROFILING
    return JS_DefineFunctions(cx, obj, profiling_functions);
#else
    return true;
#endif
}

#ifdef MOZ_CALLGRIND

#include <valgrind/callgrind.h>

JS_FRIEND_API(JSBool)
js_StartCallgrind()
{
    CALLGRIND_START_INSTRUMENTATION;
    CALLGRIND_ZERO_STATS;
    return true;
}

JS_FRIEND_API(JSBool)
js_StopCallgrind()
{
    CALLGRIND_STOP_INSTRUMENTATION;
    return true;
}

JS_FRIEND_API(JSBool)
js_DumpCallgrind(const char *outfile)
{
    if (outfile) {
        CALLGRIND_DUMP_STATS_AT(outfile);
    } else {
        CALLGRIND_DUMP_STATS;
    }

    return true;
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

bool
js_StartVtune(const char *profileName)
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

    if (profileName) {
        char filename[strlen(profileName) + strlen("-vtune.tb5") + 1];
        snprintf(filename, sizeof(filename), "%s-vtune.tb5", profileName);
        params.tb5Filename = filename;
    }

    status = VTStartSampling(&params);

    if (params.tb5Filename != default_filename)
        Foreground::free_(params.tb5Filename);

    if (status != 0) {
        if (status == VTAPI_MULTIPLE_RUNS)
            VTStopSampling(0);
        if (status < sizeof(vtuneErrorMessages))
            UnsafeError("Vtune setup error: %s", vtuneErrorMessages[status]);
        else
            UnsafeError("Vtune setup error: %d", status);
        return false;
    }
    return true;
}

bool
js_StopVtune()
{
    U32 status = VTStopSampling(1);
    if (status) {
        if (status < sizeof(vtuneErrorMessages))
            UnsafeError("Vtune shutdown error: %s", vtuneErrorMessages[status]);
        else
            UnsafeError("Vtune shutdown error: %d", status);
        return false;
    }
    return true;
}

bool
js_PauseVtune()
{
    VTPause();
    return true;
}

bool
js_ResumeVtune()
{
    VTResume();
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

#define ETHOGRAM_BUF_SIZE 65536

static JSBool
ethogram_construct(JSContext *cx, uintN argc, jsval *vp);
static void
ethogram_finalize(JSContext *cx, JSObject *obj);

static JSClass ethogram_class = {
    "Ethogram",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
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
            return NULL;

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
    while (fp && fp->pcQuadratic(cx->stack) == NULL)
        fp = fp->prev();
    return (fp && fp->pcQuadratic(cx->stack)) ? js_FramePCToLineNumber(cx, fp) : 0;
}

/* Collect states here and distribute to a matching buffer, if any */
JS_FRIEND_API(void)
js::StoreTraceVisState(JSContext *cx, TraceVisState s, TraceVisExitReason r)
{
    StackFrame *fp = cx->fp();

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
    if (!p)
        return JS_FALSE;

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
        filename = DeflateString(cx, str->chars(), str->length());
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

    for (uint32 i = 0; !p->isEmpty(); i++) {

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

#ifdef MOZ_TRACE_JSCALLS

JS_PUBLIC_API(void)
JS_SetFunctionCallback(JSContext *cx, JSFunctionCallback fcb)
{
    cx->functionCallback = fcb;
}

JS_PUBLIC_API(JSFunctionCallback)
JS_GetFunctionCallback(JSContext *cx)
{
    return cx->functionCallback;
}

#endif /* MOZ_TRACE_JSCALLS */

JS_PUBLIC_API(void)
JS_DumpBytecode(JSContext *cx, JSScript *script)
{
    JS_ASSERT(!cx->runtime->gcRunning);

#if defined(DEBUG)
    AutoArenaAllocator mark(&cx->tempPool);
    Sprinter sprinter;
    INIT_SPRINTER(cx, &sprinter, &cx->tempPool, 0);

    fprintf(stdout, "--- SCRIPT %s:%d ---\n", script->filename, script->lineno);
    js_Disassemble(cx, script, true, &sprinter);
    fputs(sprinter.base, stdout);
    fprintf(stdout, "--- END SCRIPT %s:%d ---\n", script->filename, script->lineno);
#endif
}

static void
DumpBytecodeScriptCallback(JSContext *cx, void *data, void *thing,
                           JSGCTraceKind traceKind, size_t thingSize)
{
    JS_ASSERT(traceKind == JSTRACE_SCRIPT);
    JS_ASSERT(!data);
    JSScript *script = static_cast<JSScript *>(thing);
    JS_DumpBytecode(cx, script);
}

JS_PUBLIC_API(void)
JS_DumpCompartmentBytecode(JSContext *cx)
{
    IterateCells(cx, cx->compartment, gc::FINALIZE_SCRIPT, NULL, DumpBytecodeScriptCallback);
}

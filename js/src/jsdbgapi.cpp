/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS debugging API.
 */

#include "jsdbgapi.h"

#include <string.h>

#include "jsapi.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsprvtd.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jstypes.h"
#include "jswatchpoint.h"

#include "frontend/SourceNotes.h"
#include "ion/AsmJS.h"
#ifdef JS_ION
#include "ion/AsmJSModule.h"
#endif
#include "vm/Debugger.h"
#include "vm/Interpreter.h"
#include "vm/Shape.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsscriptinlines.h"

#include "vm/Debugger-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::PodZero;

JS_PUBLIC_API(JSBool)
JS_GetDebugMode(JSContext *cx)
{
    return cx->compartment()->debugMode();
}

JS_PUBLIC_API(JSBool)
JS_SetDebugMode(JSContext *cx, JSBool debug)
{
    return JS_SetDebugModeForCompartment(cx, cx->compartment(), debug);
}

JS_PUBLIC_API(void)
JS_SetRuntimeDebugMode(JSRuntime *rt, JSBool debug)
{
    rt->debugMode = !!debug;
}

static bool
IsTopFrameConstructing(JSContext *cx, AbstractFramePtr frame)
{
    ScriptFrameIter iter(cx);
    JS_ASSERT(iter.abstractFramePtr() == frame);
    return iter.isConstructing();
}

JSTrapStatus
js::ScriptDebugPrologue(JSContext *cx, AbstractFramePtr frame)
{
    JS_ASSERT_IF(frame.isStackFrame(), frame.asStackFrame() == cx->interpreterFrame());

    if (!frame.script()->selfHosted) {
        if (frame.isFramePushedByExecute()) {
            if (JSInterpreterHook hook = cx->runtime()->debugHooks.executeHook)
                frame.setHookData(hook(cx, Jsvalify(frame), IsTopFrameConstructing(cx, frame),
                                       true, 0, cx->runtime()->debugHooks.executeHookData));
        } else {
            if (JSInterpreterHook hook = cx->runtime()->debugHooks.callHook)
                frame.setHookData(hook(cx, Jsvalify(frame), IsTopFrameConstructing(cx, frame),
                                       true, 0, cx->runtime()->debugHooks.callHookData));
        }
    }

    RootedValue rval(cx);
    JSTrapStatus status = Debugger::onEnterFrame(cx, frame, &rval);
    switch (status) {
      case JSTRAP_CONTINUE:
        break;
      case JSTRAP_THROW:
        cx->setPendingException(rval);
        break;
      case JSTRAP_ERROR:
        cx->clearPendingException();
        break;
      case JSTRAP_RETURN:
        frame.setReturnValue(rval);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("bad Debugger::onEnterFrame JSTrapStatus value");
    }
    return status;
}

bool
js::ScriptDebugEpilogue(JSContext *cx, AbstractFramePtr frame, bool okArg)
{
    JS_ASSERT_IF(frame.isStackFrame(), frame.asStackFrame() == cx->interpreterFrame());

    JSBool ok = okArg;

    // We don't add hook data for self-hosted scripts, so we don't need to check for them, here.
    if (void *hookData = frame.maybeHookData()) {
        if (frame.isFramePushedByExecute()) {
            if (JSInterpreterHook hook = cx->runtime()->debugHooks.executeHook)
                hook(cx, Jsvalify(frame), IsTopFrameConstructing(cx, frame), false, &ok, hookData);
        } else {
            if (JSInterpreterHook hook = cx->runtime()->debugHooks.callHook)
                hook(cx, Jsvalify(frame), IsTopFrameConstructing(cx, frame), false, &ok, hookData);
        }
    }

    return Debugger::onLeaveFrame(cx, frame, ok);
}

JSTrapStatus
js::DebugExceptionUnwind(JSContext *cx, AbstractFramePtr frame, jsbytecode *pc)
{
    JS_ASSERT(cx->compartment()->debugMode());

    if (!cx->runtime()->debugHooks.throwHook && cx->compartment()->getDebuggees().empty())
        return JSTRAP_CONTINUE;

    /* Call debugger throw hook if set. */
    RootedValue rval(cx);
    JSTrapStatus status = Debugger::onExceptionUnwind(cx, &rval);
    if (status == JSTRAP_CONTINUE) {
        if (JSThrowHook handler = cx->runtime()->debugHooks.throwHook) {
            RootedScript script(cx, frame.script());
            status = handler(cx, script, pc, rval.address(), cx->runtime()->debugHooks.throwHookData);
        }
    }

    switch (status) {
      case JSTRAP_ERROR:
        cx->clearPendingException();
        break;

      case JSTRAP_RETURN:
        cx->clearPendingException();
        frame.setReturnValue(rval);
        break;

      case JSTRAP_THROW:
        cx->setPendingException(rval);
        break;

      case JSTRAP_CONTINUE:
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("Invalid trap status");
    }

    return status;
}

JS_FRIEND_API(JSBool)
JS_SetDebugModeForAllCompartments(JSContext *cx, JSBool debug)
{
    AutoDebugModeGC dmgc(cx->runtime());

    for (CompartmentsIter c(cx->runtime()); !c.done(); c.next()) {
        // Ignore special compartments (atoms, JSD compartments)
        if (c->principals) {
            if (!c->setDebugModeFromC(cx, !!debug, dmgc))
                return false;
        }
    }
    return true;
}

JS_FRIEND_API(JSBool)
JS_SetDebugModeForCompartment(JSContext *cx, JSCompartment *comp, JSBool debug)
{
    AutoDebugModeGC dmgc(cx->runtime());
    return comp->setDebugModeFromC(cx, !!debug, dmgc);
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
JS_SetSingleStepMode(JSContext *cx, JSScript *scriptArg, JSBool singleStep)
{
    RootedScript script(cx, scriptArg);
    assertSameCompartment(cx, script);

    if (!CheckDebugMode(cx))
        return JS_FALSE;

    return script->setStepModeFlag(cx, singleStep);
}

JS_PUBLIC_API(JSBool)
JS_SetTrap(JSContext *cx, JSScript *scriptArg, jsbytecode *pc, JSTrapHandler handler, jsval closureArg)
{
    RootedScript script(cx, scriptArg);
    RootedValue closure(cx, closureArg);
    assertSameCompartment(cx, script, closure);

    if (!CheckDebugMode(cx))
        return false;

    BreakpointSite *site = script->getOrCreateBreakpointSite(cx, pc);
    if (!site)
        return false;
    site->setTrap(cx->runtime()->defaultFreeOp(), handler, closure);
    return true;
}

JS_PUBLIC_API(void)
JS_ClearTrap(JSContext *cx, JSScript *script, jsbytecode *pc,
             JSTrapHandler *handlerp, jsval *closurep)
{
    if (BreakpointSite *site = script->getBreakpointSite(pc)) {
        site->clearTrap(cx->runtime()->defaultFreeOp(), handlerp, closurep);
    } else {
        if (handlerp)
            *handlerp = NULL;
        if (closurep)
            *closurep = JSVAL_VOID;
    }
}

JS_PUBLIC_API(void)
JS_ClearScriptTraps(JSRuntime *rt, JSScript *script)
{
    script->clearTraps(rt->defaultFreeOp());
}

JS_PUBLIC_API(void)
JS_ClearAllTrapsForCompartment(JSContext *cx)
{
    cx->compartment()->clearTraps(cx->runtime()->defaultFreeOp());
}

JS_PUBLIC_API(JSBool)
JS_SetInterrupt(JSRuntime *rt, JSInterruptHook hook, void *closure)
{
    rt->debugHooks.interruptHook = hook;
    rt->debugHooks.interruptHookData = closure;
    for (InterpreterFrames *f = rt->interpreterFrames; f; f = f->older)
        f->enableInterruptsUnconditionally();
    return true;
}

JS_PUBLIC_API(JSBool)
JS_ClearInterrupt(JSRuntime *rt, JSInterruptHook *hoop, void **closurep)
{
    if (hoop)
        *hoop = rt->debugHooks.interruptHook;
    if (closurep)
        *closurep = rt->debugHooks.interruptHookData;
    rt->debugHooks.interruptHook = 0;
    rt->debugHooks.interruptHookData = 0;
    return JS_TRUE;
}

/************************************************************************/

JS_PUBLIC_API(JSBool)
JS_SetWatchPoint(JSContext *cx, JSObject *obj_, jsid id_,
                 JSWatchPointHandler handler, JSObject *closure_)
{
    assertSameCompartment(cx, obj_);

    RootedId id(cx, id_);
    RootedObject origobj(cx, obj_), closure(cx, closure_);
    RootedObject obj(cx, GetInnerObject(cx, origobj));
    if (!obj)
        return false;

    RootedValue v(cx);
    unsigned attrs;

    RootedId propid(cx);

    if (JSID_IS_INT(id)) {
        propid = id;
    } else if (JSID_IS_OBJECT(id)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_WATCH_PROP);
        return false;
    } else {
        RootedValue val(cx, IdToValue(id));
        if (!ValueToId<CanGC>(cx, val, &propid))
            return false;
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

    /*
     * Use sparse indexes for watched objects, as dense elements can be written
     * to without checking the watchpoint map.
     */
    if (!JSObject::sparsifyDenseElements(cx, obj))
        return false;

    types::MarkTypePropertyConfigured(cx, obj, propid);

    WatchpointMap *wpmap = cx->compartment()->watchpointMap;
    if (!wpmap) {
        wpmap = cx->runtime()->new_<WatchpointMap>();
        if (!wpmap || !wpmap->init()) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        cx->compartment()->watchpointMap = wpmap;
    }
    return wpmap->watch(cx, obj, propid, handler, closure);
}

JS_PUBLIC_API(JSBool)
JS_ClearWatchPoint(JSContext *cx, JSObject *obj, jsid id,
                   JSWatchPointHandler *handlerp, JSObject **closurep)
{
    assertSameCompartment(cx, obj, id);

    if (WatchpointMap *wpmap = cx->compartment()->watchpointMap)
        wpmap->unwatch(obj, id, handlerp, closurep);
    return true;
}

JS_PUBLIC_API(JSBool)
JS_ClearWatchPointsForObject(JSContext *cx, JSObject *obj)
{
    assertSameCompartment(cx, obj);

    if (WatchpointMap *wpmap = cx->compartment()->watchpointMap)
        wpmap->unwatchObject(obj);
    return true;
}

JS_PUBLIC_API(JSBool)
JS_ClearAllWatchPoints(JSContext *cx)
{
    if (JSCompartment *comp = cx->compartment()) {
        if (WatchpointMap *wpmap = comp->watchpointMap)
            wpmap->clear();
    }
    return true;
}

/************************************************************************/

JS_PUBLIC_API(unsigned)
JS_PCToLineNumber(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    return js::PCToLineNumber(script, pc);
}

JS_PUBLIC_API(jsbytecode *)
JS_LineNumberToPC(JSContext *cx, JSScript *script, unsigned lineno)
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
              unsigned startLine, unsigned maxLines,
              unsigned* count, unsigned** retLines, jsbytecode*** retPCs)
{
    size_t len = (script->length > maxLines ? maxLines : script->length);
    unsigned *lines = cx->pod_malloc<unsigned>(len);
    if (!lines)
        return JS_FALSE;

    jsbytecode **pcs = cx->pod_malloc<jsbytecode*>(len);
    if (!pcs) {
        js_free(lines);
        return JS_FALSE;
    }

    unsigned lineno = script->lineno;
    unsigned offset = 0;
    unsigned i = 0;
    for (jssrcnote *sn = script->notes(); !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        offset += SN_DELTA(sn);
        SrcNoteType type = (SrcNoteType) SN_TYPE(sn);
        if (type == SRC_SETLINE || type == SRC_NEWLINE) {
            if (type == SRC_SETLINE)
                lineno = (unsigned) js_GetSrcNoteOffset(sn, 0);
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
        js_free(lines);

    if (retPCs)
        *retPCs = pcs;
    else
        js_free(pcs);

    return JS_TRUE;
}

JS_PUBLIC_API(unsigned)
JS_GetFunctionArgumentCount(JSContext *cx, JSFunction *fun)
{
    return fun->nargs;
}

JS_PUBLIC_API(JSBool)
JS_FunctionHasLocalNames(JSContext *cx, JSFunction *fun)
{
    return fun->nonLazyScript()->bindings.count() > 0;
}

extern JS_PUBLIC_API(uintptr_t *)
JS_GetFunctionLocalNameArray(JSContext *cx, JSFunction *fun, void **memp)
{
    RootedScript script(cx, fun->nonLazyScript());
    BindingVector bindings(cx);
    if (!FillBindingVector(script, &bindings))
        return NULL;

    LifoAlloc &lifo = cx->tempLifoAlloc();

    // Store the LifoAlloc::Mark right before the allocation.
    LifoAlloc::Mark mark = lifo.mark();
    void *mem = lifo.alloc(sizeof(LifoAlloc::Mark) + bindings.length() * sizeof(uintptr_t));
    if (!mem) {
        js_ReportOutOfMemory(cx);
        return NULL;
    }
    *memp = mem;
    *reinterpret_cast<LifoAlloc::Mark*>(mem) = mark;

    // Munge data into the API this method implements.  Avert your eyes!
    uintptr_t *names = reinterpret_cast<uintptr_t*>((char*)mem + sizeof(LifoAlloc::Mark));
    for (size_t i = 0; i < bindings.length(); i++)
        names[i] = reinterpret_cast<uintptr_t>(bindings[i].name());

    return names;
}

extern JS_PUBLIC_API(JSAtom *)
JS_LocalNameToAtom(uintptr_t w)
{
    return reinterpret_cast<JSAtom *>(w);
}

extern JS_PUBLIC_API(JSString *)
JS_AtomKey(JSAtom *atom)
{
    return atom;
}

extern JS_PUBLIC_API(void)
JS_ReleaseFunctionLocalNameArray(JSContext *cx, void *mem)
{
    cx->tempLifoAlloc().release(*reinterpret_cast<LifoAlloc::Mark*>(mem));
}

JS_PUBLIC_API(JSScript *)
JS_GetFunctionScript(JSContext *cx, JSFunction *fun)
{
    if (fun->isNative())
        return NULL;
    if (fun->isInterpretedLazy()) {
        RootedFunction rootedFun(cx, fun);
        AutoCompartment funCompartment(cx, rootedFun);
        JSScript *script = rootedFun->getOrCreateScript(cx);
        if (!script)
            MOZ_CRASH();
        return script;
    }
    return fun->nonLazyScript();
}

JS_PUBLIC_API(JSNative)
JS_GetFunctionNative(JSContext *cx, JSFunction *fun)
{
    return fun->maybeNative();
}

JS_PUBLIC_API(JSPrincipals *)
JS_GetScriptPrincipals(JSScript *script)
{
    return script->principals();
}

JS_PUBLIC_API(JSPrincipals *)
JS_GetScriptOriginPrincipals(JSScript *script)
{
    return script->originPrincipals;
}

/************************************************************************/

JS_PUBLIC_API(JSFunction *)
JS_GetScriptFunction(JSContext *cx, JSScript *script)
{
    return script->function();
}

JS_PUBLIC_API(JSObject *)
JS_GetParentOrScopeChain(JSContext *cx, JSObject *obj)
{
    return obj->enclosingScope();
}

JS_PUBLIC_API(const char *)
JS_GetDebugClassName(JSObject *obj)
{
    if (obj->is<DebugScopeObject>())
        return obj->as<DebugScopeObject>().scope().getClass()->name;
    return obj->getClass()->name;
}

/************************************************************************/

JS_PUBLIC_API(const char *)
JS_GetScriptFilename(JSContext *cx, JSScript *script)
{
    return script->filename();
}

JS_PUBLIC_API(const jschar *)
JS_GetScriptSourceMap(JSContext *cx, JSScript *script)
{
    ScriptSource *source = script->scriptSource();
    JS_ASSERT(source);
    return source->hasSourceMap() ? source->sourceMap() : NULL;
}

JS_PUBLIC_API(unsigned)
JS_GetScriptBaseLineNumber(JSContext *cx, JSScript *script)
{
    return script->lineno;
}

JS_PUBLIC_API(unsigned)
JS_GetScriptLineExtent(JSContext *cx, JSScript *script)
{
    return js_GetScriptLineExtent(script);
}

JS_PUBLIC_API(JSVersion)
JS_GetScriptVersion(JSContext *cx, JSScript *script)
{
    return VersionNumber(script->getVersion());
}

JS_PUBLIC_API(bool)
JS_GetScriptIsSelfHosted(JSScript *script)
{
    return script->selfHosted;
}

/***************************************************************************/

JS_PUBLIC_API(void)
JS_SetNewScriptHook(JSRuntime *rt, JSNewScriptHook hook, void *callerdata)
{
    rt->debugHooks.newScriptHook = hook;
    rt->debugHooks.newScriptHookData = callerdata;
}

JS_PUBLIC_API(void)
JS_SetDestroyScriptHook(JSRuntime *rt, JSDestroyScriptHook hook,
                        void *callerdata)
{
    rt->debugHooks.destroyScriptHook = hook;
    rt->debugHooks.destroyScriptHookData = callerdata;
}

/***************************************************************************/

/* This all should be reworked to avoid requiring JSScopeProperty types. */

static JSBool
GetPropertyDesc(JSContext *cx, JSObject *obj_, HandleShape shape, JSPropertyDesc *pd)
{
    assertSameCompartment(cx, obj_);
    pd->id = IdToJsval(shape->propid());

    RootedObject obj(cx, obj_);

    JSBool wasThrowing = cx->isExceptionPending();
    RootedValue lastException(cx, UndefinedValue());
    if (wasThrowing)
        lastException = cx->getPendingException();
    cx->clearPendingException();

    Rooted<jsid> id(cx, shape->propid());
    RootedValue value(cx);
    if (!baseops::GetProperty(cx, obj, id, &value)) {
        if (!cx->isExceptionPending()) {
            pd->flags = JSPD_ERROR;
            pd->value = JSVAL_VOID;
        } else {
            pd->flags = JSPD_EXCEPTION;
            pd->value = cx->getPendingException();
        }
    } else {
        pd->flags = 0;
        pd->value = value;
    }

    if (wasThrowing)
        cx->setPendingException(lastException);

    pd->flags |= (shape->enumerable() ? JSPD_ENUMERATE : 0)
              |  (!shape->writable()  ? JSPD_READONLY  : 0)
              |  (!shape->configurable() ? JSPD_PERMANENT : 0);
    pd->spare = 0;
    pd->alias = JSVAL_VOID;

    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyDescArray(JSContext *cx, JSObject *obj_, JSPropertyDescArray *pda)
{
    RootedObject obj(cx, obj_);

    assertSameCompartment(cx, obj);
    uint32_t i = 0;
    JSPropertyDesc *pd = NULL;

    if (obj->is<DebugScopeObject>()) {
        AutoIdVector props(cx);
        if (!Proxy::enumerate(cx, obj, props))
            return false;

        pd = cx->pod_calloc<JSPropertyDesc>(props.length());
        if (!pd)
            return false;

        for (i = 0; i < props.length(); ++i) {
            pd[i].id = JSVAL_NULL;
            pd[i].value = JSVAL_NULL;
            if (!AddValueRoot(cx, &pd[i].id, NULL))
                goto bad;
            pd[i].id = IdToValue(props[i]);
            if (!AddValueRoot(cx, &pd[i].value, NULL))
                goto bad;
            if (!Proxy::get(cx, obj, obj, props.handleAt(i), MutableHandleValue::fromMarkedLocation(&pd[i].value)))
                goto bad;
        }

        pda->length = props.length();
        pda->array = pd;
        return true;
    }

    Class *clasp;
    clasp = obj->getClass();
    if (!obj->isNative() || (clasp->flags & JSCLASS_NEW_ENUMERATE)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_CANT_DESCRIBE_PROPS, clasp->name);
        return false;
    }
    if (!clasp->enumerate(cx, obj))
        return false;

    /* Return an empty pda early if obj has no own properties. */
    if (obj->nativeEmpty()) {
        pda->length = 0;
        pda->array = NULL;
        return true;
    }

    pd = cx->pod_malloc<JSPropertyDesc>(obj->propertyCount());
    if (!pd)
        return false;

    {
        Shape::Range<CanGC> r(cx, obj->lastProperty());
        RootedShape shape(cx);
        for (; !r.empty(); r.popFront()) {
            pd[i].id = JSVAL_NULL;
            pd[i].value = JSVAL_NULL;
            pd[i].alias = JSVAL_NULL;
            if (!AddValueRoot(cx, &pd[i].id, NULL))
                goto bad;
            if (!AddValueRoot(cx, &pd[i].value, NULL))
                goto bad;
            shape = const_cast<Shape *>(&r.front());
            if (!GetPropertyDesc(cx, obj, shape, &pd[i]))
                goto bad;
            if ((pd[i].flags & JSPD_ALIAS) && !AddValueRoot(cx, &pd[i].alias, NULL))
                goto bad;
            if (++i == obj->propertyCount())
                break;
        }
    }

    pda->length = i;
    pda->array = pd;
    return true;

bad:
    pda->length = i + 1;
    pda->array = pd;
    JS_PutPropertyDescArray(cx, pda);
    return false;
}

JS_PUBLIC_API(void)
JS_PutPropertyDescArray(JSContext *cx, JSPropertyDescArray *pda)
{
    JSPropertyDesc *pd;
    uint32_t i;

    pd = pda->array;
    for (i = 0; i < pda->length; i++) {
        js_RemoveRoot(cx->runtime(), &pd[i].id);
        js_RemoveRoot(cx->runtime(), &pd[i].value);
        if (pd[i].flags & JSPD_ALIAS)
            js_RemoveRoot(cx->runtime(), &pd[i].alias);
    }
    js_free(pd);
    pda->array = NULL;
    pda->length = 0;
}

/************************************************************************/

JS_PUBLIC_API(JSBool)
JS_SetDebuggerHandler(JSRuntime *rt, JSDebuggerHandler handler, void *closure)
{
    rt->debugHooks.debuggerHandler = handler;
    rt->debugHooks.debuggerHandlerData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetSourceHandler(JSRuntime *rt, JSSourceHandler handler, void *closure)
{
    rt->debugHooks.sourceHandler = handler;
    rt->debugHooks.sourceHandlerData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetExecuteHook(JSRuntime *rt, JSInterpreterHook hook, void *closure)
{
    rt->debugHooks.executeHook = hook;
    rt->debugHooks.executeHookData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetCallHook(JSRuntime *rt, JSInterpreterHook hook, void *closure)
{
    rt->debugHooks.callHook = hook;
    rt->debugHooks.callHookData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetThrowHook(JSRuntime *rt, JSThrowHook hook, void *closure)
{
    rt->debugHooks.throwHook = hook;
    rt->debugHooks.throwHookData = closure;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetDebugErrorHook(JSRuntime *rt, JSDebugErrorHook hook, void *closure)
{
    rt->debugHooks.debugErrorHook = hook;
    rt->debugHooks.debugErrorHookData = closure;
    return JS_TRUE;
}

/************************************************************************/

JS_PUBLIC_API(const JSDebugHooks *)
JS_GetGlobalDebugHooks(JSRuntime *rt)
{
    return &rt->debugHooks;
}

/************************************************************************/

JS_PUBLIC_API(void)
JS_DumpBytecode(JSContext *cx, JSScript *scriptArg)
{
#if defined(DEBUG)
    Rooted<JSScript*> script(cx, scriptArg);

    Sprinter sprinter(cx);
    if (!sprinter.init())
        return;

    fprintf(stdout, "--- SCRIPT %s:%d ---\n", script->filename(), script->lineno);
    js_Disassemble(cx, script, true, &sprinter);
    fputs(sprinter.string(), stdout);
    fprintf(stdout, "--- END SCRIPT %s:%d ---\n", script->filename(), script->lineno);
#endif
}

extern JS_PUBLIC_API(void)
JS_DumpPCCounts(JSContext *cx, JSScript *scriptArg)
{
    Rooted<JSScript*> script(cx, scriptArg);
    JS_ASSERT(script->hasScriptCounts);

    Sprinter sprinter(cx);
    if (!sprinter.init())
        return;

    fprintf(stdout, "--- SCRIPT %s:%d ---\n", script->filename(), script->lineno);
    js_DumpPCCounts(cx, script, &sprinter);
    fputs(sprinter.string(), stdout);
    fprintf(stdout, "--- END SCRIPT %s:%d ---\n", script->filename(), script->lineno);
}

namespace {

typedef Vector<JSScript *, 0, SystemAllocPolicy> ScriptsToDump;

static void
DumpBytecodeScriptCallback(JSRuntime *rt, void *data, JSScript *script)
{
    static_cast<ScriptsToDump *>(data)->append(script);
}

} /* anonymous namespace */

JS_PUBLIC_API(void)
JS_DumpCompartmentBytecode(JSContext *cx)
{
    ScriptsToDump scripts;
    IterateScripts(cx->runtime(), cx->compartment(), &scripts, DumpBytecodeScriptCallback);

    for (size_t i = 0; i < scripts.length(); i++) {
        if (scripts[i]->enclosingScriptsCompiledSuccessfully())
            JS_DumpBytecode(cx, scripts[i]);
    }
}

JS_PUBLIC_API(void)
JS_DumpCompartmentPCCounts(JSContext *cx)
{
    for (CellIter i(cx->zone(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (script->compartment() != cx->compartment())
            continue;

        if (script->hasScriptCounts && script->enclosingScriptsCompiledSuccessfully())
            JS_DumpPCCounts(cx, script);
    }

#if defined(JS_ION)
    for (unsigned thingKind = FINALIZE_OBJECT0; thingKind < FINALIZE_OBJECT_LIMIT; thingKind++) {
        for (CellIter i(cx->zone(), (AllocKind) thingKind); !i.done(); i.next()) {
            JSObject *obj = i.get<JSObject>();
            if (obj->compartment() != cx->compartment())
                continue;

            if (IsAsmJSModuleObject(obj)) {
                AsmJSModule &module = AsmJSModuleObjectToModule(obj);

                Sprinter sprinter(cx);
                if (!sprinter.init())
                    return;

                fprintf(stdout, "--- Asm.js Module ---\n");

                for (size_t i = 0; i < module.numFunctionCounts(); i++) {
                    ion::IonScriptCounts *counts = module.functionCounts(i);
                    DumpIonScriptCounts(&sprinter, counts);
                }

                fputs(sprinter.string(), stdout);
                fprintf(stdout, "--- END Asm.js Module ---\n");
            }
        }
    }
#endif
}

JS_FRIEND_API(JSBool)
js_CallContextDebugHandler(JSContext *cx)
{
    NonBuiltinScriptFrameIter iter(cx);
    JS_ASSERT(!iter.done());

    RootedValue rval(cx);
    RootedScript script(cx, iter.script());
    switch (js::CallContextDebugHandler(cx, script, iter.pc(), rval.address())) {
      case JSTRAP_ERROR:
        JS_ClearPendingException(cx);
        return JS_FALSE;
      case JSTRAP_THROW:
        JS_SetPendingException(cx, rval);
        return JS_FALSE;
      case JSTRAP_RETURN:
      case JSTRAP_CONTINUE:
      default:
        return JS_TRUE;
    }
}

JS_PUBLIC_API(JS::StackDescription *)
JS::DescribeStack(JSContext *cx, unsigned maxFrames)
{
    Vector<FrameDescription> frames(cx);

    for (NonBuiltinScriptFrameIter i(cx); !i.done(); ++i) {
        FrameDescription desc;
        desc.script = i.script();
        desc.lineno = PCToLineNumber(i.script(), i.pc());
        desc.fun = i.maybeCallee();
        if (!frames.append(desc))
            return NULL;
        if (frames.length() == maxFrames)
            break;
    }

    JS::StackDescription *desc = js_new<JS::StackDescription>();
    if (!desc)
        return NULL;

    desc->nframes = frames.length();
    desc->frames = frames.extractRawBuffer();
    return desc;
}

JS_PUBLIC_API(void)
JS::FreeStackDescription(JSContext *cx, JS::StackDescription *desc)
{
    js_delete(desc->frames);
    js_delete(desc);
}

class AutoPropertyDescArray
{
    JSContext *cx_;
    JSPropertyDescArray descArray_;

  public:
    AutoPropertyDescArray(JSContext *cx)
      : cx_(cx)
    {
        PodZero(&descArray_);
    }
    ~AutoPropertyDescArray()
    {
        if (descArray_.array)
            JS_PutPropertyDescArray(cx_, &descArray_);
    }

    void fetch(JSObject *obj) {
        JS_ASSERT(!descArray_.array);
        if (!JS_GetPropertyDescArray(cx_, obj, &descArray_))
            descArray_.array = NULL;
    }

    JSPropertyDescArray * operator ->() {
        return &descArray_;
    }
};

static const char *
FormatValue(JSContext *cx, const Value &vArg, JSAutoByteString &bytes)
{
    RootedValue v(cx, vArg);
    JSString *str = ToString<CanGC>(cx, v);
    if (!str)
        return NULL;
    const char *buf = bytes.encodeLatin1(cx, str);
    if (!buf)
        return NULL;
    const char *found = strstr(buf, "function ");
    if (found && (found - buf <= 2))
        return "[function]";
    return buf;
}

static char *
FormatFrame(JSContext *cx, const NonBuiltinScriptFrameIter &iter, char *buf, int num,
            JSBool showArgs, JSBool showLocals, JSBool showThisProps)
{
    RootedScript script(cx, iter.script());
    jsbytecode* pc = iter.pc();

    RootedObject scopeChain(cx, iter.scopeChain());
    JSAutoCompartment ac(cx, scopeChain);

    const char *filename = script->filename();
    unsigned lineno = PCToLineNumber(script, pc);
    RootedFunction fun(cx, iter.maybeCallee());
    RootedString funname(cx);
    if (fun)
        funname = fun->atom();

    RootedObject callObj(cx);
    AutoPropertyDescArray callProps(cx);

    if (!iter.isJit() && (showArgs || showLocals)) {
        JSAbstractFramePtr frame(Jsvalify(iter.abstractFramePtr()));
        callObj = frame.callObject(cx);
        if (callObj)
            callProps.fetch(callObj);
    }

    RootedValue thisVal(cx);
    AutoPropertyDescArray thisProps(cx);
    if (iter.computeThis()) {
        thisVal = iter.thisv();
        if (showThisProps && !thisVal.isPrimitive())
            thisProps.fetch(&thisVal.toObject());
    }

    // print the frame number and function name
    if (funname) {
        JSAutoByteString funbytes;
        buf = JS_sprintf_append(buf, "%d %s(", num, funbytes.encodeLatin1(cx, funname));
    } else if (fun) {
        buf = JS_sprintf_append(buf, "%d anonymous(", num);
    } else {
        buf = JS_sprintf_append(buf, "%d <TOP LEVEL>", num);
    }
    if (!buf)
        return buf;

    // print the function arguments
    if (showArgs && callObj) {
        uint32_t namedArgCount = 0;
        for (uint32_t i = 0; i < callProps->length; i++) {
            JSPropertyDesc* desc = &callProps->array[i];
            JSAutoByteString nameBytes;
            const char *name = NULL;
            if (JSVAL_IS_STRING(desc->id))
                name = FormatValue(cx, desc->id, nameBytes);

            JSAutoByteString valueBytes;
            const char *value = FormatValue(cx, desc->value, valueBytes);

            buf = JS_sprintf_append(buf, "%s%s%s%s%s%s",
                                    namedArgCount ? ", " : "",
                                    name ? name :"",
                                    name ? " = " : "",
                                    desc->value.isString() ? "\"" : "",
                                    value ? value : "?unknown?",
                                    desc->value.isString() ? "\"" : "");
            if (!buf)
                return buf;
            namedArgCount++;
        }

        // print any unnamed trailing args (found in 'arguments' object)
        RootedValue val(cx);
        if (JS_GetProperty(cx, callObj, "arguments", val.address()) && val.isObject()) {
            uint32_t argCount;
            RootedObject argsObj(cx, &val.toObject());
            if (JS_GetProperty(cx, argsObj, "length", val.address()) &&
                ToUint32(cx, val, &argCount) &&
                argCount > namedArgCount)
            {
                for (uint32_t k = namedArgCount; k < argCount; k++) {
                    char number[8];
                    JS_snprintf(number, 8, "%d", (int) k);

                    if (JS_GetProperty(cx, argsObj, number, val.address())) {
                        JSAutoByteString valueBytes;
                        const char *value = FormatValue(cx, val, valueBytes);
                        buf = JS_sprintf_append(buf, "%s%s%s%s",
                                                k ? ", " : "",
                                                val.isString() ? "\"" : "",
                                                value ? value : "?unknown?",
                                                val.isString() ? "\"" : "");
                        if (!buf)
                            return buf;
                    }
                }
            }
        }
    }

    // print filename and line number
    buf = JS_sprintf_append(buf, "%s [\"%s\":%d]\n",
                            fun ? ")" : "",
                            filename ? filename : "<unknown>",
                            lineno);
    if (!buf)
        return buf;

    // print local variables
    if (showLocals && callProps->array) {
        for (uint32_t i = 0; i < callProps->length; i++) {
            JSPropertyDesc* desc = &callProps->array[i];
            JSAutoByteString nameBytes;
            JSAutoByteString valueBytes;
            const char *name = FormatValue(cx, desc->id, nameBytes);
            const char *value = FormatValue(cx, desc->value, valueBytes);

            if (name && value) {
                buf = JS_sprintf_append(buf, "    %s = %s%s%s\n",
                                        name,
                                        desc->value.isString() ? "\"" : "",
                                        value,
                                        desc->value.isString() ? "\"" : "");
                if (!buf)
                    return buf;
            }
        }
    }

    // print the value of 'this'
    if (showLocals) {
        if (!thisVal.isUndefined()) {
            JSAutoByteString thisValBytes;
            RootedString thisValStr(cx, ToString<CanGC>(cx, thisVal));
            if (thisValStr) {
                if (const char *str = thisValBytes.encodeLatin1(cx, thisValStr)) {
                    buf = JS_sprintf_append(buf, "    this = %s\n", str);
                    if (!buf)
                        return buf;
                }
            }
        } else {
            buf = JS_sprintf_append(buf, "    <failed to get 'this' value>\n");
        }
    }

    // print the properties of 'this', if it is an object
    if (showThisProps && thisProps->array) {
        for (uint32_t i = 0; i < thisProps->length; i++) {
            JSPropertyDesc* desc = &thisProps->array[i];
            if (desc->flags & JSPD_ENUMERATE) {
                JSAutoByteString nameBytes;
                JSAutoByteString valueBytes;
                const char *name = FormatValue(cx, desc->id, nameBytes);
                const char *value = FormatValue(cx, desc->value, valueBytes);
                if (name && value) {
                    buf = JS_sprintf_append(buf, "    this.%s = %s%s%s\n",
                                            name,
                                            desc->value.isString() ? "\"" : "",
                                            value,
                                            desc->value.isString() ? "\"" : "");
                    if (!buf)
                        return buf;
                }
            }
        }
    }

    return buf;
}

JS_PUBLIC_API(char *)
JS::FormatStackDump(JSContext *cx, char *buf,
                      JSBool showArgs, JSBool showLocals,
                      JSBool showThisProps)
{
    int num = 0;

    for (NonBuiltinScriptFrameIter i(cx); !i.done(); ++i) {
        buf = FormatFrame(cx, i, buf, num, showArgs, showLocals, showThisProps);
        num++;
    }

    if (!num)
        buf = JS_sprintf_append(buf, "JavaScript stack is empty\n");

    return buf;
}

JSAbstractFramePtr::JSAbstractFramePtr(void *raw)
  : ptr_(uintptr_t(raw))
{ }

JSObject *
JSAbstractFramePtr::scopeChain(JSContext *cx)
{
    AbstractFramePtr frame = Valueify(*this);
    RootedObject scopeChain(cx, frame.scopeChain());
    AutoCompartment ac(cx, scopeChain);
    return GetDebugScopeForFrame(cx, frame);
}

JSObject *
JSAbstractFramePtr::callObject(JSContext *cx)
{
    AbstractFramePtr frame = Valueify(*this);
    if (!frame.isFunctionFrame())
        return NULL;

    JSObject *o = GetDebugScopeForFrame(cx, frame);

    /*
     * Given that fp is a function frame and GetDebugScopeForFrame always fills
     * in missing scopes, we can expect to find fp's CallObject on 'o'. Note:
     *  - GetDebugScopeForFrame wraps every ScopeObject (missing or not) with
     *    a DebugScopeObject proxy.
     *  - If fp is an eval-in-function, then fp has no callobj of its own and
     *    JS_GetFrameCallObject will return the innermost function's callobj.
     */
    while (o) {
        ScopeObject &scope = o->as<DebugScopeObject>().scope();
        if (scope.is<CallObject>())
            return o;
        o = o->enclosingScope();
    }
    return NULL;
}

JSFunction *
JSAbstractFramePtr::maybeFun()
{
    AbstractFramePtr frame = Valueify(*this);
    return frame.maybeFun();
}

JSScript *
JSAbstractFramePtr::script()
{
    AbstractFramePtr frame = Valueify(*this);
    return frame.script();
}

bool
JSAbstractFramePtr::getThisValue(JSContext *cx, MutableHandleValue thisv)
{
    AbstractFramePtr frame = Valueify(*this);

    RootedObject scopeChain(cx, frame.scopeChain());
    js::AutoCompartment ac(cx, scopeChain);
    if (!ComputeThis(cx, frame))
        return false;

    thisv.set(frame.thisValue());
    return true;
}

bool
JSAbstractFramePtr::isDebuggerFrame()
{
    AbstractFramePtr frame = Valueify(*this);
    return frame.isDebuggerFrame();
}

bool
JSAbstractFramePtr::evaluateInStackFrame(JSContext *cx,
                                         const char *bytes, unsigned length,
                                         const char *filename, unsigned lineno,
                                         MutableHandleValue rval)
{
    if (!CheckDebugMode(cx))
        return false;

    size_t len = length;
    jschar *chars = InflateString(cx, bytes, &len);
    if (!chars)
        return false;
    length = (unsigned) len;

    bool ok = evaluateUCInStackFrame(cx, chars, length, filename, lineno, rval);
    js_free(chars);

    return ok;
}

bool
JSAbstractFramePtr::evaluateUCInStackFrame(JSContext *cx,
                                           const jschar *chars, unsigned length,
                                           const char *filename, unsigned lineno,
                                           MutableHandleValue rval)
{
    /* Protect inlined chars from root analysis poisoning. */
    SkipRoot skipChars(cx, &chars);

    if (!CheckDebugMode(cx))
        return false;

    RootedObject scope(cx, scopeChain(cx));
    Rooted<Env*> env(cx, scope);
    if (!env)
        return false;

    AbstractFramePtr frame = Valueify(*this);
    if (!ComputeThis(cx, frame))
        return false;
    RootedValue thisv(cx, frame.thisValue());

    js::AutoCompartment ac(cx, env);
    return EvaluateInEnv(cx, env, thisv, frame, StableCharPtr(chars, length), length,
                         filename, lineno, rval);
}

JSBrokenFrameIterator::JSBrokenFrameIterator(JSContext *cx)
{
    NonBuiltinScriptFrameIter iter(cx);
    data_ = iter.copyData();
}

JSBrokenFrameIterator::~JSBrokenFrameIterator()
{
    js_free((ScriptFrameIter::Data *)data_);
}

bool
JSBrokenFrameIterator::done() const
{
    NonBuiltinScriptFrameIter iter(*(ScriptFrameIter::Data *)data_);
    return iter.done();
}

JSBrokenFrameIterator &
JSBrokenFrameIterator::operator++()
{
    ScriptFrameIter::Data *data = (ScriptFrameIter::Data *)data_;
    NonBuiltinScriptFrameIter iter(*data);
    ++iter;
    *data = iter.data_;
    return *this;
}

JSAbstractFramePtr
JSBrokenFrameIterator::abstractFramePtr() const
{
    NonBuiltinScriptFrameIter iter(*(ScriptFrameIter::Data *)data_);
    return Jsvalify(iter.abstractFramePtr());
}

jsbytecode *
JSBrokenFrameIterator::pc() const
{
    NonBuiltinScriptFrameIter iter(*(ScriptFrameIter::Data *)data_);
    return iter.pc();
}

bool
JSBrokenFrameIterator::isConstructing() const
{
    NonBuiltinScriptFrameIter iter(*(ScriptFrameIter::Data *)data_);
    return iter.isConstructing();
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
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
 * The Original Code is SpiderMonkey Debugger object.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributors:
 *   Jim Blandy <jimb@mozilla.com>
 *   Jason Orendorff <jorendorff@mozilla.com>
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

#include "vm/Debugger.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsemit.h"
#include "jsgcmark.h"
#include "jsobj.h"
#include "jstl.h"
#include "jswrapper.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsopcodeinlines.h"
#include "methodjit/Retcon.h"
#include "vm/Stack-inl.h"

using namespace js;


/*** Forward declarations ************************************************************************/

extern Class DebuggerFrame_class;

enum {
    JSSLOT_DEBUGFRAME_OWNER,
    JSSLOT_DEBUGFRAME_ARGUMENTS,
    JSSLOT_DEBUGFRAME_COUNT
};

extern Class DebuggerArguments_class;

enum {
    JSSLOT_DEBUGARGUMENTS_FRAME,
    JSSLOT_DEBUGARGUMENTS_COUNT
};

extern Class DebuggerObject_class;

enum {
    JSSLOT_DEBUGOBJECT_OWNER,
    JSSLOT_DEBUGOBJECT_COUNT
};

extern Class DebuggerScript_class;

enum {
    JSSLOT_DEBUGSCRIPT_OWNER,
    JSSLOT_DEBUGSCRIPT_HOLDER,  /* PrivateValue, cross-compartment pointer */
    JSSLOT_DEBUGSCRIPT_COUNT
};


/*** Utils ***************************************************************************************/

bool
ReportMoreArgsNeeded(JSContext *cx, const char *name, uintN required)
{
    JS_ASSERT(required > 0);
    JS_ASSERT(required <= 10);
    char s[2];
    s[0] = '0' + (required - 1);
    s[1] = '\0';
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                         name, s, required == 1 ? "" : "s");
    return false;
}

#define REQUIRE_ARGC(name, n)                                                 \
    JS_BEGIN_MACRO                                                            \
        if (argc < (n))                                                       \
            return ReportMoreArgsNeeded(cx, name, n);                         \
    JS_END_MACRO

bool
ReportObjectRequired(JSContext *cx)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
    return false;
}


/*** Breakpoints *********************************************************************************/

BreakpointSite::BreakpointSite(JSScript *script, jsbytecode *pc)
  : script(script), pc(pc), realOpcode(JSOp(*pc)), scriptObject(NULL), enabledCount(0),
    trapHandler(NULL), trapClosure(UndefinedValue())
{
    JS_ASSERT(realOpcode != JSOP_TRAP);
    JS_INIT_CLIST(&breakpoints);
}

/*
 * Precondition: script is live, meaning either it is a non-held script that is
 * on the stack or a held script that hasn't been GC'd.
 */
static JSObject *
ScriptScope(JSContext *cx, JSScript *script, JSObject *holder)
{
    if (holder)
        return holder;

    /*
     * The referent is a non-held script. There is no direct reference from
     * script to the scope, so find it on the stack.
     */
    for (AllFramesIter i(cx->stack.space()); ; ++i) {
        JS_ASSERT(!i.done());
        if (i.fp()->maybeScript() == script)
            return &i.fp()->scopeChain();
    }
    JS_NOT_REACHED("ScriptScope: live non-held script not on stack");
}

bool
BreakpointSite::recompile(JSContext *cx, bool forTrap)
{
#ifdef JS_METHODJIT
    if (script->hasJITCode()) {
        Maybe<AutoCompartment> ac;
        if (!forTrap) {
            ac.construct(cx, ScriptScope(cx, script, scriptObject));
            if (!ac.ref().enter())
                return false;
        }
        js::mjit::Recompiler recompiler(cx, script);
        if (!recompiler.recompile())
            return false;
    }
#endif
    return true;
}

bool
BreakpointSite::inc(JSContext *cx)
{
    if (enabledCount == 0 && !trapHandler) {
        JS_ASSERT(*pc == realOpcode);
        *pc = JSOP_TRAP;
        if (!recompile(cx, false)) {
            *pc = realOpcode;
            return false;
        }
    }
    enabledCount++;
    return true;
}

void
BreakpointSite::dec(JSContext *cx)
{
    JS_ASSERT(enabledCount > 0);
    JS_ASSERT(*pc == JSOP_TRAP);
    enabledCount--;
    if (enabledCount == 0 && !trapHandler) {
        *pc = realOpcode;
        recompile(cx, false);  /* ignore failure */
    }
}

bool
BreakpointSite::setTrap(JSContext *cx, JSTrapHandler handler, const Value &closure)
{
    if (enabledCount == 0) {
        *pc = JSOP_TRAP;
        if (!recompile(cx, true)) {
            *pc = realOpcode;
            return false;
        }
    }
    trapHandler = handler;
    trapClosure = closure;
    return true;
}

void
BreakpointSite::clearTrap(JSContext *cx, BreakpointSiteMap::Enum *e,
                          JSTrapHandler *handlerp, Value *closurep)
{
    if (handlerp)
        *handlerp = trapHandler;
    if (closurep)
        *closurep = trapClosure;

    trapHandler = NULL;
    trapClosure.setUndefined();
    if (enabledCount == 0) {
        *pc = realOpcode;
        recompile(cx, true);  /* ignore failure */
        destroyIfEmpty(cx->runtime, e);
    }
}

void
BreakpointSite::destroyIfEmpty(JSRuntime *rt, BreakpointSiteMap::Enum *e)
{
    if (JS_CLIST_IS_EMPTY(&breakpoints) && !trapHandler) {
        if (e)
            e->removeFront();
        else
            script->compartment->breakpointSites.remove(pc);
        rt->delete_(this);
    }
}

Breakpoint *
BreakpointSite::firstBreakpoint() const
{
    if (JS_CLIST_IS_EMPTY(&breakpoints))
        return NULL;
    return Breakpoint::fromSiteLinks(JS_NEXT_LINK(&breakpoints));
}

bool
BreakpointSite::hasBreakpoint(Breakpoint *bp)
{
    for (Breakpoint *p = firstBreakpoint(); p; p = p->nextInSite())
        if (p == bp)
            return true;
    return false;
}

Breakpoint::Breakpoint(Debugger *debugger, BreakpointSite *site, JSObject *handler)
    : debugger(debugger), site(site), handler(handler)
{
    JS_APPEND_LINK(&debuggerLinks, &debugger->breakpoints);
    JS_APPEND_LINK(&siteLinks, &site->breakpoints);
}

Breakpoint *
Breakpoint::fromDebuggerLinks(JSCList *links)
{
    return (Breakpoint *) ((unsigned char *) links - offsetof(Breakpoint, debuggerLinks));
}

Breakpoint *
Breakpoint::fromSiteLinks(JSCList *links)
{
    return (Breakpoint *) ((unsigned char *) links - offsetof(Breakpoint, siteLinks));
}

void
Breakpoint::destroy(JSContext *cx, BreakpointSiteMap::Enum *e)
{
    if (debugger->enabled)
        site->dec(cx);
    JS_REMOVE_LINK(&debuggerLinks);
    JS_REMOVE_LINK(&siteLinks);
    JSRuntime *rt = cx->runtime;
    site->destroyIfEmpty(rt, e);
    rt->delete_(this);
}

Breakpoint *
Breakpoint::nextInDebugger()
{
    JSCList *link = JS_NEXT_LINK(&debuggerLinks);
    return (link == &debugger->breakpoints) ? NULL : fromDebuggerLinks(link);
}

Breakpoint *
Breakpoint::nextInSite()
{
    JSCList *link = JS_NEXT_LINK(&siteLinks);
    return (link == &site->breakpoints) ? NULL : fromSiteLinks(link);
}


/*** Debugger hook dispatch **********************************************************************/

Debugger::Debugger(JSContext *cx, JSObject *dbg)
  : object(dbg), uncaughtExceptionHook(NULL), enabled(true),
    frames(cx), objects(cx), heldScripts(cx), nonHeldScripts(cx)
{
    assertSameCompartment(cx, dbg);

    JSRuntime *rt = cx->runtime;
    AutoLockGC lock(rt);
    JS_APPEND_LINK(&link, &rt->debuggerList);
    JS_INIT_CLIST(&breakpoints);
}

Debugger::~Debugger()
{
    JS_ASSERT(debuggees.empty());

    /* This always happens in the GC thread, so no locking is required. */
    JS_ASSERT(object->compartment()->rt->gcRunning);
    JS_REMOVE_LINK(&link);
}

bool
Debugger::init(JSContext *cx)
{
    bool ok = (frames.init() &&
               objects.init() &&
               debuggees.init() &&
               heldScripts.init() &&
               nonHeldScripts.init());
    if (!ok)
        js_ReportOutOfMemory(cx);
    return ok;
}

JS_STATIC_ASSERT(uintN(JSSLOT_DEBUGFRAME_OWNER) == uintN(JSSLOT_DEBUGOBJECT_OWNER));
JS_STATIC_ASSERT(uintN(JSSLOT_DEBUGFRAME_OWNER) == uintN(JSSLOT_DEBUGSCRIPT_OWNER));

Debugger *
Debugger::fromChildJSObject(JSObject *obj)
{
    JS_ASSERT(obj->clasp == &DebuggerFrame_class ||
              obj->clasp == &DebuggerObject_class ||
              obj->clasp == &DebuggerScript_class);
    JSObject *dbgobj = &obj->getReservedSlot(JSSLOT_DEBUGOBJECT_OWNER).toObject();
    return fromJSObject(dbgobj);
}

bool
Debugger::getScriptFrame(JSContext *cx, StackFrame *fp, Value *vp)
{
    JS_ASSERT(fp->isScriptFrame());
    FrameMap::AddPtr p = frames.lookupForAdd(fp);
    if (!p) {
        /* Create and populate the Debugger.Frame object. */
        JSObject *proto = &object->getReservedSlot(JSSLOT_DEBUG_FRAME_PROTO).toObject();
        JSObject *frameobj =
            NewNonFunction<WithProto::Given>(cx, &DebuggerFrame_class, proto, NULL);
        if (!frameobj || !frameobj->ensureClassReservedSlots(cx))
            return false;
        frameobj->setPrivate(fp);
        frameobj->setReservedSlot(JSSLOT_DEBUGFRAME_OWNER, ObjectValue(*object));

        if (!frames.add(p, fp, frameobj)) {
            js_ReportOutOfMemory(cx);
            return false;
        }
    }
    vp->setObject(*p->value);
    return true;
}

JSObject *
Debugger::getHook(Hook hook) const
{
    JS_ASSERT(hook >= 0 && hook < HookCount);
    const Value &v = object->getReservedSlot(JSSLOT_DEBUG_HOOK_START + hook);
    return v.isUndefined() ? NULL : &v.toObject();
}

bool
Debugger::hasAnyLiveHooks() const
{
    return enabled && (getHook(OnDebuggerStatement) ||
                       getHook(OnExceptionUnwind) ||
                       getHook(OnNewScript) ||
                       getHook(OnEnterFrame) ||
                       !JS_CLIST_IS_EMPTY(&breakpoints));
}

void
Debugger::slowPathOnEnterFrame(JSContext *cx)
{
    /* Build the list of recipients. */
    AutoValueVector triggered(cx);
    GlobalObject *global = cx->fp()->scopeChain().getGlobal();
    if (GlobalObject::DebuggerVector *debuggers = global->getDebuggers()) {
        for (Debugger **p = debuggers->begin(); p != debuggers->end(); p++) {
            Debugger *dbg = *p;
            JS_ASSERT(dbg->observesFrame(cx->fp()));
            if (dbg->observesEnterFrame() && !triggered.append(ObjectValue(*dbg->toJSObject())))
                return;
        }
    }

    /* Deliver the event, checking again as in dispatchHook. */
    for (Value *p = triggered.begin(); p != triggered.end(); p++) {
        Debugger *dbg = Debugger::fromJSObject(&p->toObject());
        if (dbg->debuggees.has(global) && dbg->observesEnterFrame())
            dbg->fireEnterFrame(cx);
    }
}

void
Debugger::slowPathOnLeaveFrame(JSContext *cx)
{
    StackFrame *fp = cx->fp();
    GlobalObject *global = fp->scopeChain().getGlobal();

    /*
     * FIXME This notifies only current debuggers, so it relies on a hack in
     * Debugger::removeDebuggeeGlobal to make sure only current debuggers have
     * Frame objects with .live === true.
     */
    if (GlobalObject::DebuggerVector *debuggers = global->getDebuggers()) {
        for (Debugger **p = debuggers->begin(); p != debuggers->end(); p++) {
            Debugger *dbg = *p;
            if (FrameMap::Ptr p = dbg->frames.lookup(fp)) {
                JSObject *frameobj = p->value;
                frameobj->setPrivate(NULL);
                dbg->frames.remove(p);
            }
        }
    }

    /*
     * If this is an eval frame, then from the debugger's perspective the
     * script is about to be destroyed. Remove any breakpoints in it.
     */
    if (fp->isEvalFrame()) {
        JSScript *script = fp->script();
        script->compartment->clearBreakpointsIn(cx, NULL, script, NULL);
    }
}

bool
Debugger::wrapDebuggeeValue(JSContext *cx, Value *vp)
{
    assertSameCompartment(cx, object);

    if (vp->isObject()) {
        JSObject *obj = &vp->toObject();

        ObjectWeakMap::AddPtr p = objects.lookupForAdd(obj);
        if (p) {
            vp->setObject(*p->value);
        } else {
            /* Create a new Debugger.Object for obj. */
            JSObject *proto = &object->getReservedSlot(JSSLOT_DEBUG_OBJECT_PROTO).toObject();
            JSObject *dobj =
                NewNonFunction<WithProto::Given>(cx, &DebuggerObject_class, proto, NULL);
            if (!dobj || !dobj->ensureClassReservedSlots(cx))
                return false;
            dobj->setPrivate(obj);
            dobj->setReservedSlot(JSSLOT_DEBUGOBJECT_OWNER, ObjectValue(*object));
            if (!objects.relookupOrAdd(p, obj, dobj)) {
                js_ReportOutOfMemory(cx);
                return false;
            }
            vp->setObject(*dobj);
        }
    } else if (!cx->compartment->wrap(cx, vp)) {
        vp->setUndefined();
        return false;
    }

    return true;
}

bool
Debugger::unwrapDebuggeeValue(JSContext *cx, Value *vp)
{
    assertSameCompartment(cx, object, *vp);
    if (vp->isObject()) {
        JSObject *dobj = &vp->toObject();
        if (dobj->clasp != &DebuggerObject_class) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_EXPECTED_TYPE,
                                 "Debugger", "Debugger.Object", dobj->clasp->name);
            return false;
        }

        Value owner = dobj->getReservedSlot(JSSLOT_DEBUGOBJECT_OWNER);
        if (owner.toObjectOrNull() != object) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 owner.isNull()
                                 ? JSMSG_DEBUG_OBJECT_PROTO
                                 : JSMSG_DEBUG_OBJECT_WRONG_OWNER);
            return false;
        }

        vp->setObject(*(JSObject *) dobj->getPrivate());
    }
    return true;
}

JSTrapStatus
Debugger::handleUncaughtException(AutoCompartment &ac, Value *vp, bool callHook)
{
    JSContext *cx = ac.context;
    if (cx->isExceptionPending()) {
        if (callHook && uncaughtExceptionHook) {
            Value fval = ObjectValue(*uncaughtExceptionHook);
            Value exc = cx->getPendingException();
            Value rv;
            cx->clearPendingException();
            if (ExternalInvoke(cx, ObjectValue(*object), fval, 1, &exc, &rv))
                return vp ? parseResumptionValue(ac, true, rv, vp, false) : JSTRAP_CONTINUE;
        }

        if (cx->isExceptionPending()) {
            JS_ReportPendingException(cx);
            cx->clearPendingException();
        }
    }
    ac.leave();
    return JSTRAP_ERROR;
}

bool
Debugger::newCompletionValue(AutoCompartment &ac, bool ok, Value val, Value *vp)
{
    JS_ASSERT_IF(ok, !ac.context->isExceptionPending());

    JSContext *cx = ac.context;
    jsid key;
    if (ok) {
        ac.leave();
        key = ATOM_TO_JSID(cx->runtime->atomState.returnAtom);
    } else if (cx->isExceptionPending()) {
        key = ATOM_TO_JSID(cx->runtime->atomState.throwAtom);
        val = cx->getPendingException();
        cx->clearPendingException();
        ac.leave();
    } else {
        ac.leave();
        vp->setNull();
        return true;
    }

    JSObject *obj = NewBuiltinClassInstance(cx, &js_ObjectClass);
    if (!obj ||
        !wrapDebuggeeValue(cx, &val) ||
        !DefineNativeProperty(cx, obj, key, val, PropertyStub, StrictPropertyStub,
                              JSPROP_ENUMERATE, 0, 0))
    {
        return false;
    }
    vp->setObject(*obj);
    return true;
}

JSTrapStatus
Debugger::parseResumptionValue(AutoCompartment &ac, bool ok, const Value &rv, Value *vp,
                               bool callHook)
{
    vp->setUndefined();
    if (!ok)
        return handleUncaughtException(ac, vp, callHook);
    if (rv.isUndefined()) {
        ac.leave();
        return JSTRAP_CONTINUE;
    }
    if (rv.isNull()) {
        ac.leave();
        return JSTRAP_ERROR;
    }

    /* Check that rv is {return: val} or {throw: val}. */
    JSContext *cx = ac.context;
    JSObject *obj;
    const Shape *shape;
    jsid returnId = ATOM_TO_JSID(cx->runtime->atomState.returnAtom);
    jsid throwId = ATOM_TO_JSID(cx->runtime->atomState.throwAtom);
    bool okResumption = rv.isObject();
    if (okResumption) {
        obj = &rv.toObject();
        okResumption = obj->isObject();
    }
    if (okResumption) {
        shape = obj->lastProperty();
        okResumption = shape->previous() &&
             !shape->previous()->previous() &&
             (shape->propid == returnId || shape->propid == throwId) &&
             shape->isDataDescriptor();
    }
    if (!okResumption) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_BAD_RESUMPTION);
        return handleUncaughtException(ac, vp, callHook);
    }

    if (!js_NativeGet(cx, obj, obj, shape, 0, vp) || !unwrapDebuggeeValue(cx, vp))
        return handleUncaughtException(ac, vp, callHook);

    ac.leave();
    if (!cx->compartment->wrap(cx, vp)) {
        vp->setUndefined();
        return JSTRAP_ERROR;
    }
    return shape->propid == returnId ? JSTRAP_RETURN : JSTRAP_THROW;
}

bool
CallMethodIfPresent(JSContext *cx, JSObject *obj, const char *name, int argc, Value *argv,
                    Value *rval)
{
    rval->setUndefined();
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    Value fval;
    return atom &&
           js_GetMethod(cx, obj, ATOM_TO_JSID(atom), JSGET_NO_METHOD_BARRIER, &fval) &&
           (!js_IsCallable(fval) ||
            ExternalInvoke(cx, ObjectValue(*obj), fval, argc, argv, rval));
}

JSTrapStatus
Debugger::fireDebuggerStatement(JSContext *cx, Value *vp)
{
    JSObject *hook = getHook(OnDebuggerStatement);
    JS_ASSERT(hook);
    JS_ASSERT(hook->isCallable());

    /* Grab cx->fp() before pushing a dummy frame. */
    StackFrame *fp = cx->fp();
    AutoCompartment ac(cx, object);
    if (!ac.enter())
        return JSTRAP_ERROR;

    Value argv[1];
    if (!getScriptFrame(cx, fp, argv))
        return handleUncaughtException(ac, vp, false);

    Value rv;
    bool ok = ExternalInvoke(cx, ObjectValue(*object), ObjectValue(*hook), 1, argv, &rv);
    return parseResumptionValue(ac, ok, rv, vp);
}

JSTrapStatus
Debugger::fireExceptionUnwind(JSContext *cx, Value *vp)
{
    JSObject *hook = getHook(OnExceptionUnwind);
    JS_ASSERT(hook);
    JS_ASSERT(hook->isCallable());

    StackFrame *fp = cx->fp();
    Value exc = cx->getPendingException();
    cx->clearPendingException();

    AutoCompartment ac(cx, object);
    if (!ac.enter())
        return JSTRAP_ERROR;

    Value argv[2];
    argv[1] = exc;
    if (!getScriptFrame(cx, fp, &argv[0]) || !wrapDebuggeeValue(cx, &argv[1]))
        return handleUncaughtException(ac, vp, false);

    Value rv;
    bool ok = ExternalInvoke(cx, ObjectValue(*object), ObjectValue(*hook), 2, argv, &rv);
    JSTrapStatus st = parseResumptionValue(ac, ok, rv, vp);
    if (st == JSTRAP_CONTINUE)
        cx->setPendingException(exc);
    return st;
}

void
Debugger::fireEnterFrame(JSContext *cx)
{
    JSObject *hook = getHook(OnEnterFrame);
    JS_ASSERT(hook);
    JS_ASSERT(hook->isCallable());

    StackFrame *fp = cx->fp();
    AutoCompartment ac(cx, object);
    if (!ac.enter())
        return;

    Value argv[1];
    if (!getScriptFrame(cx, fp, &argv[0])) {
        handleUncaughtException(ac, NULL, false);
        return;
    }
    Value rv;
    if (!ExternalInvoke(cx, ObjectValue(*object), ObjectValue(*hook), 1, argv, &rv))
        handleUncaughtException(ac, NULL, true);
}

void
Debugger::fireNewScript(JSContext *cx, JSScript *script, JSObject *obj, NewScriptKind kind)
{
    JSObject *hook = getHook(OnNewScript);
    JS_ASSERT(hook);
    JS_ASSERT(hook->isCallable());

    AutoCompartment ac(cx, object);
    if (!ac.enter())
        return;

    JSObject *dsobj =
        kind == NewHeldScript ? wrapHeldScript(cx, script, obj) : wrapNonHeldScript(cx, script);
    if (!dsobj) {
        handleUncaughtException(ac, NULL, false);
        return;
    }

    Value argv[1];
    argv[0].setObject(*dsobj);
    Value rv;
    if (!ExternalInvoke(cx, ObjectValue(*object), ObjectValue(*hook), 1, argv, &rv))
        handleUncaughtException(ac, NULL, true);
}

JSTrapStatus
Debugger::dispatchHook(JSContext *cx, js::Value *vp, Hook which)
{
    JS_ASSERT(which == OnDebuggerStatement || which == OnExceptionUnwind);

    /*
     * Determine which debuggers will receive this event, and in what order.
     * Make a copy of the list, since the original is mutable and we will be
     * calling into arbitrary JS.
     *
     * Note: In the general case, 'triggered' contains references to objects in
     * different compartments--every compartment *except* this one.
     */
    AutoValueVector triggered(cx);
    GlobalObject *global = cx->fp()->scopeChain().getGlobal();
    if (GlobalObject::DebuggerVector *debuggers = global->getDebuggers()) {
        for (Debugger **p = debuggers->begin(); p != debuggers->end(); p++) {
            Debugger *dbg = *p;
            if (dbg->enabled && dbg->getHook(which)) {
                if (!triggered.append(ObjectValue(*dbg->toJSObject())))
                    return JSTRAP_ERROR;
            }
        }
    }

    /*
     * Deliver the event to each debugger, checking again to make sure it
     * should still be delivered.
     */
    for (Value *p = triggered.begin(); p != triggered.end(); p++) {
        Debugger *dbg = Debugger::fromJSObject(&p->toObject());
        if (dbg->debuggees.has(global) && dbg->enabled && dbg->getHook(which)) {
            JSTrapStatus st = (which == OnDebuggerStatement)
                              ? dbg->fireDebuggerStatement(cx, vp)
                              : dbg->fireExceptionUnwind(cx, vp);
            if (st != JSTRAP_CONTINUE)
                return st;
        }
    }
    return JSTRAP_CONTINUE;
}

static bool
AddNewScriptRecipients(GlobalObject::DebuggerVector *src, AutoValueVector *dest)
{
    bool wasEmpty = dest->length() == 0;
    for (Debugger **p = src->begin(); p != src->end(); p++) {
        Debugger *dbg = *p;
        Value v = ObjectValue(*dbg->toJSObject());
        if (dbg->observesNewScript() &&
            (wasEmpty || Find(dest->begin(), dest->end(), v) == dest->end()) &&
            !dest->append(v))
        {
            return false;
        }
    }
    return true;
}

void
Debugger::slowPathOnNewScript(JSContext *cx, JSScript *script, JSObject *obj, NewScriptKind kind)
{
    /*
     * Build the list of recipients. For compile-and-go scripts, this is the
     * same as the generic Debugger::dispatchHook code, but non-compile-and-go
     * scripts are not tied to particular globals. We deliver them to every
     * debugger observing any global in the script's compartment.
     */
    AutoValueVector triggered(cx);
    GlobalObject *global;
    if (script->compileAndGo) {
        global = obj->getGlobal();
        if (GlobalObject::DebuggerVector *debuggers = global->getDebuggers()) {
            if (!AddNewScriptRecipients(debuggers, &triggered))
                return;
        }
    } else {
        global = NULL;
        GlobalObjectSet &debuggees = script->compartment->getDebuggees();
        for (GlobalObjectSet::Range r = debuggees.all(); !r.empty(); r.popFront()) {
            if (!AddNewScriptRecipients(r.front()->getDebuggers(), &triggered))
                return;
        }
    }

    /*
     * Deliver the event to each debugger, checking again as in
     * Debugger::dispatchHook.
     */
    for (Value *p = triggered.begin(); p != triggered.end(); p++) {
        Debugger *dbg = Debugger::fromJSObject(&p->toObject());
        if ((!global || dbg->debuggees.has(global)) && dbg->enabled && dbg->getHook(OnNewScript))
            dbg->fireNewScript(cx, script, obj, kind);
    }
}

JSTrapStatus
Debugger::onTrap(JSContext *cx, Value *vp)
{
    StackFrame *fp = cx->fp();
    GlobalObject *scriptGlobal = fp->scopeChain().getGlobal();
    jsbytecode *pc = cx->regs().pc;
    BreakpointSite *site = cx->compartment->getBreakpointSite(pc);
    JSOp op = site->realOpcode;

    /* Build list of breakpoint handlers. */
    Vector<Breakpoint *> triggered(cx);
    for (Breakpoint *bp = site->firstBreakpoint(); bp; bp = bp->nextInSite()) {
        if (!triggered.append(bp))
            return JSTRAP_ERROR;
    }

    Value frame = UndefinedValue();
    for (Breakpoint **p = triggered.begin(); p != triggered.end(); p++) {
        Breakpoint *bp = *p;

        /* Handlers can clear breakpoints. Check that bp still exists. */
        if (!site || !site->hasBreakpoint(bp))
            continue;

        Debugger *dbg = bp->debugger;
        if (dbg->enabled && dbg->debuggees.lookup(scriptGlobal)) {
            AutoCompartment ac(cx, dbg->object);
            if (!ac.enter())
                return JSTRAP_ERROR;

            Value argv[1];
            if (!dbg->getScriptFrame(cx, fp, &argv[0]))
                return dbg->handleUncaughtException(ac, vp, false);
            Value rv;
            bool ok = CallMethodIfPresent(cx, bp->handler, "hit", 1, argv, &rv);
            JSTrapStatus st = dbg->parseResumptionValue(ac, ok, rv, vp, true);
            if (st != JSTRAP_CONTINUE)
                return st;

            /* Calling JS code invalidates site. Reload it. */
            site = cx->compartment->getBreakpointSite(pc);
        }
    }

    if (site && site->trapHandler) {
        JSTrapStatus st = site->trapHandler(cx, fp->script(), pc, Jsvalify(vp),
                                            Jsvalify(site->trapClosure));
        if (st != JSTRAP_CONTINUE)
            return st;
    }

    /* By convention, return the true op to the interpreter in vp. */
    vp->setInt32(op);
    return JSTRAP_CONTINUE;
}


/*** Debugger JSObjects **************************************************************************/

void
Debugger::markKeysInCompartment(JSTracer *tracer, const ObjectWeakMap &map)
{
    JSCompartment *comp = tracer->context->runtime->gcCurrentCompartment;
    JS_ASSERT(comp);

    /*
     * WeakMap::Range is deliberately private, to discourage C++ code from
     * enumerating WeakMap keys. However in this case we need access, so we
     * make a base-class reference. Range is public in HashMap.
     */
    typedef HashMap<JSObject *, JSObject *, DefaultHasher<JSObject *>, RuntimeAllocPolicy> Map;
    const Map &storage = map;
    for (Map::Range r = storage.all(); !r.empty(); r.popFront()) {
        JSObject *key = r.front().key;
        if (key->compartment() == comp && IsAboutToBeFinalized(tracer->context, key))
            js::gc::MarkObject(tracer, *key, "cross-compartment WeakMap key");
    }
}

/*
 * Ordinarily, WeakMap keys and values are marked because at some point it was
 * discovered that the WeakMap was live; that is, some object containing the
 * WeakMap was marked during mark phase.
 *
 * However, during single-compartment GC, we have to do something about
 * cross-compartment WeakMaps in other compartments. Since those compartments
 * aren't being GC'd, the WeakMaps definitely will not be found during mark
 * phase. If their keys and values might need to be marked, we have to do it
 * manually.
 *
 * Each Debugger object keeps two cross-compartment WeakMaps: objects and
 * heldScripts.  Both have the nice property that all their values are in the
 * same compartment as the Debugger object, so we only need to mark the
 * keys. We must simply mark all keys that are in the compartment being GC'd.
 *
 * We must scan all Debugger objects regardless of whether they *currently*
 * have any debuggees in the compartment being GC'd, because the WeakMap
 * entries persist even when debuggees are removed.
 *
 * This happens during the initial mark phase, not iterative marking, because
 * all the edges being reported here are strong references.
 */
void
Debugger::markCrossCompartmentDebuggerObjectReferents(JSTracer *tracer)
{
    JSRuntime *rt = tracer->context->runtime;
    JSCompartment *comp = rt->gcCurrentCompartment;

    /*
     * Mark all objects in comp that are referents of Debugger.Objects in other
     * compartments.
     */
    for (JSCList *p = &rt->debuggerList; (p = JS_NEXT_LINK(p)) != &rt->debuggerList;) {
        Debugger *dbg = Debugger::fromLinks(p);
        if (dbg->object->compartment() != comp) {
            markKeysInCompartment(tracer, dbg->objects);
            markKeysInCompartment(tracer, dbg->heldScripts);
        }
    }
}

/*
 * This method has two tasks:
 *   1. Mark Debugger objects that are unreachable except for debugger hooks that
 *      may yet be called.
 *   2. Mark breakpoint handlers.
 *
 * This happens during the iterative part of the GC mark phase. This method
 * returns true if it has to mark anything; GC calls it repeatedly until it
 * returns false.
 */
bool
Debugger::markAllIteratively(GCMarker *trc, JSGCInvocationKind gckind)
{
    bool markedAny = false;

    /*
     * We must find all Debugger objects in danger of GC. This code is a little
     * convoluted since the easiest way to find them is via their debuggees.
     */
    JSRuntime *rt = trc->context->runtime;
    JSCompartment *comp = rt->gcCurrentCompartment;
    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++) {
        JSCompartment *dc = *c;

        /* If dc is being collected, mark breakpoint handlers in it. */
        if (!comp || dc == comp)
            markedAny = markedAny | dc->markBreakpointsIteratively(trc);

        /*
         * If this is a single-compartment GC, no compartment can debug itself, so skip
         * |comp|. If it's a global GC, then search every live compartment.
         */
        if (comp && dc == comp)
            continue;

        const GlobalObjectSet &debuggees = dc->getDebuggees();
        for (GlobalObjectSet::Range r = debuggees.all(); !r.empty(); r.popFront()) {
            GlobalObject *global = r.front();

            /*
             * Every debuggee has at least one debugger, so in this case
             * getDebuggers can't return NULL.
             */
            const GlobalObject::DebuggerVector *debuggers = global->getDebuggers();
            JS_ASSERT(debuggers);
            for (Debugger **p = debuggers->begin(); p != debuggers->end(); p++) {
                Debugger *dbg = *p;
                JSObject *obj = dbg->toJSObject();

                /*
                 * dbg is a Debugger with at least one debuggee. Check three things:
                 *   - dbg is actually in a compartment being GC'd
                 *   - it isn't already marked
                 *   - it actually has hooks that might be called
                 * IsAboutToBeFinalized covers the first two criteria.
                 */
                if (IsAboutToBeFinalized(trc->context, obj) && dbg->hasAnyLiveHooks()) {
                    /*
                     * obj could be reachable only via its live, enabled
                     * debugger hooks, which may yet be called.
                     */
                    MarkObject(trc, *obj, "enabled Debugger");
                    markedAny = true;
                }
            }
        }
    }
    return markedAny;
}

void
Debugger::traceObject(JSTracer *trc, JSObject *obj)
{
    if (Debugger *dbg = Debugger::fromJSObject(obj))
        dbg->trace(trc);
}

void
Debugger::trace(JSTracer *trc)
{
    if (uncaughtExceptionHook)
        MarkObject(trc, *uncaughtExceptionHook, "hooks");

    /*
     * Mark Debugger.Frame objects that are reachable from JS if we look them up
     * again (because the corresponding StackFrame is still on the stack).
     */
    for (FrameMap::Range r = frames.all(); !r.empty(); r.popFront()) {
        JSObject *frameobj = r.front().value;
        JS_ASSERT(frameobj->getPrivate());
        MarkObject(trc, *frameobj, "live Debugger.Frame");
    }

    /* Trace the referent -> Debugger.Object weak map. */
    objects.trace(trc);

    /*
     * Trace the weak map from JSFunctions and "Script" JSObjects to
     * Debugger.Script objects.
     */
    heldScripts.trace(trc);

    /* Trace the map for non-held scripts, which are explicitly freed. */
    for (ScriptMap::Range r = nonHeldScripts.all(); !r.empty(); r.popFront()) {
        JSObject *scriptobj = r.front().value;

        /*
         * nonHeldScripts should only refer to Debugger.Script objects for
         * scripts that haven't been freed yet.
         */
        JS_ASSERT(scriptobj->getPrivate());
        MarkObject(trc, *scriptobj, "live eval Debugger.Script");
    }
}

void
Debugger::sweepAll(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    JS_ASSERT(!rt->gcCurrentCompartment);

    for (JSCList *p = &rt->debuggerList; (p = JS_NEXT_LINK(p)) != &rt->debuggerList;) {
        Debugger *dbg = Debugger::fromLinks(p);

        if (IsAboutToBeFinalized(cx, dbg->object)) {
            /*
             * dbg is being GC'd. Detach it from its debuggees. In the case of
             * runtime-wide GC, the debuggee might be GC'd too. Since detaching
             * requires access to both objects, this must be done before
             * finalize time. However, in a per-compartment GC, it is
             * impossible for both objects to be GC'd (since they are in
             * different compartments), so in that case we just wait for
             * Debugger::finalize.
             */
            for (GlobalObjectSet::Enum e(dbg->debuggees); !e.empty(); e.popFront())
                dbg->removeDebuggeeGlobal(cx, e.front(), NULL, &e);
        }

    }

    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++) {
        /* For each debuggee being GC'd, detach it from all its debuggers. */
        GlobalObjectSet &debuggees = (*c)->getDebuggees();
        for (GlobalObjectSet::Enum e(debuggees); !e.empty(); e.popFront()) {
            GlobalObject *global = e.front();
            if (IsAboutToBeFinalized(cx, global))
                detachAllDebuggersFromGlobal(cx, global, &e);
        }
    }
}

void
Debugger::detachAllDebuggersFromGlobal(JSContext *cx, GlobalObject *global,
                                       GlobalObjectSet::Enum *compartmentEnum)
{
    const GlobalObject::DebuggerVector *debuggers = global->getDebuggers();
    JS_ASSERT(!debuggers->empty());
    while (!debuggers->empty())
        debuggers->back()->removeDebuggeeGlobal(cx, global, compartmentEnum, NULL);
}

void
Debugger::finalize(JSContext *cx, JSObject *obj)
{
    Debugger *dbg = fromJSObject(obj);
    if (!dbg)
        return;
    if (!dbg->debuggees.empty()) {
        /*
         * This happens only during per-compartment GC. See comment in
         * Debugger::sweepAll.
         */
        JS_ASSERT(cx->runtime->gcCurrentCompartment == dbg->object->compartment());
        for (GlobalObjectSet::Enum e(dbg->debuggees); !e.empty(); e.popFront())
            dbg->removeDebuggeeGlobal(cx, e.front(), NULL, &e);
    }
    cx->delete_(dbg);
}

Class Debugger::jsclass = {
    "Debugger", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUG_COUNT),
    PropertyStub, PropertyStub, PropertyStub, StrictPropertyStub,
    EnumerateStub, ResolveStub, ConvertStub, Debugger::finalize,
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    Debugger::traceObject
};

Debugger *
Debugger::fromThisValue(JSContext *cx, const CallArgs &args, const char *fnname)
{
    if (!args.thisv().isObject()) {
        ReportObjectRequired(cx);
        return NULL;
    }
    JSObject *thisobj = &args.thisv().toObject();
    if (thisobj->getClass() != &Debugger::jsclass) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Debugger", fnname, thisobj->getClass()->name);
        return NULL;
    }

    /*
     * Forbid Debugger.prototype, which is of the Debugger JSClass but isn't
     * really a Debugger object. The prototype object is distinguished by
     * having a NULL private value.
     */
    Debugger *dbg = fromJSObject(thisobj);
    if (!dbg) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Debugger", fnname, "prototype object");
    }
    return dbg;
}

#define THIS_DEBUGGER(cx, argc, vp, fnname, args, dbg)                       \
    CallArgs args = CallArgsFromVp(argc, vp);                                \
    Debugger *dbg = Debugger::fromThisValue(cx, args, fnname);               \
    if (!dbg)                                                                \
        return false

JSBool
Debugger::getEnabled(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGGER(cx, argc, vp, "get enabled", args, dbg);
    args.rval().setBoolean(dbg->enabled);
    return true;
}

JSBool
Debugger::setEnabled(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.set enabled", 1);
    THIS_DEBUGGER(cx, argc, vp, "set enabled", args, dbg);
    bool enabled = js_ValueToBoolean(args[0]);

    if (enabled != dbg->enabled) {
        for (Breakpoint *bp = dbg->firstBreakpoint(); bp; bp = bp->nextInDebugger()) {
            if (enabled) {
                if (!bp->site->inc(cx)) {
                    /*
                     * Roll back the changes on error to keep the
                     * BreakpointSite::enabledCount counters correct.
                     */
                    for (Breakpoint *bp2 = dbg->firstBreakpoint();
                         bp2 != bp;
                         bp2 = bp2->nextInDebugger())
                    {
                        bp->site->dec(cx);
                    }
                    return false;
                }
            } else {
                bp->site->dec(cx);
            }
        }
    }

    dbg->enabled = enabled;
    args.rval().setUndefined();
    return true;
}

JSBool
Debugger::getHookImpl(JSContext *cx, uintN argc, Value *vp, Hook which)
{
    JS_ASSERT(which >= 0 && which < HookCount);
    THIS_DEBUGGER(cx, argc, vp, "getHook", args, dbg);
    args.rval() = dbg->object->getReservedSlot(JSSLOT_DEBUG_HOOK_START + which);
    return true;
}

JSBool
Debugger::setHookImpl(JSContext *cx, uintN argc, Value *vp, Hook which)
{
    JS_ASSERT(which >= 0 && which < HookCount);
    REQUIRE_ARGC("Debugger.setHook", 1);
    THIS_DEBUGGER(cx, argc, vp, "setHook", args, dbg);
    const Value &v = args[0];
    if (v.isObject()) {
        if (!v.toObject().isCallable()) {
            js_ReportIsNotFunction(cx, vp, JSV2F_SEARCH_STACK);
            return false;
        }
    } else if (!v.isUndefined()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_CALLABLE_OR_UNDEFINED);
        return false;
    }
    dbg->object->setReservedSlot(JSSLOT_DEBUG_HOOK_START + which, v);
    args.rval().setUndefined();
    return true;
}

JSBool
Debugger::getOnDebuggerStatement(JSContext *cx, uintN argc, Value *vp)
{
    return getHookImpl(cx, argc, vp, OnDebuggerStatement);
}

JSBool
Debugger::setOnDebuggerStatement(JSContext *cx, uintN argc, Value *vp)
{
    return setHookImpl(cx, argc, vp, OnDebuggerStatement);
}

JSBool
Debugger::getOnExceptionUnwind(JSContext *cx, uintN argc, Value *vp)
{
    return getHookImpl(cx, argc, vp, OnExceptionUnwind);
}

JSBool
Debugger::setOnExceptionUnwind(JSContext *cx, uintN argc, Value *vp)
{
    return setHookImpl(cx, argc, vp, OnExceptionUnwind);
}

JSBool
Debugger::getOnNewScript(JSContext *cx, uintN argc, Value *vp)
{
    return getHookImpl(cx, argc, vp, OnNewScript);
}

JSBool
Debugger::setOnNewScript(JSContext *cx, uintN argc, Value *vp)
{
    return setHookImpl(cx, argc, vp, OnNewScript);
}

JSBool
Debugger::getOnEnterFrame(JSContext *cx, uintN argc, Value *vp)
{
    return getHookImpl(cx, argc, vp, OnEnterFrame);
}

JSBool
Debugger::setOnEnterFrame(JSContext *cx, uintN argc, Value *vp)
{
    return setHookImpl(cx, argc, vp, OnEnterFrame);
}

JSBool
Debugger::getUncaughtExceptionHook(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGGER(cx, argc, vp, "get uncaughtExceptionHook", args, dbg);
    args.rval().setObjectOrNull(dbg->uncaughtExceptionHook);
    return true;
}

JSBool
Debugger::setUncaughtExceptionHook(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.set uncaughtExceptionHook", 1);
    THIS_DEBUGGER(cx, argc, vp, "set uncaughtExceptionHook", args, dbg);
    if (!args[0].isNull() && (!args[0].isObject() || !args[0].toObject().isCallable())) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_ASSIGN_FUNCTION_OR_NULL,
                             "uncaughtExceptionHook");
        return false;
    }

    dbg->uncaughtExceptionHook = args[0].toObjectOrNull();
    args.rval().setUndefined();
    return true;
}

JSObject *
Debugger::unwrapDebuggeeArgument(JSContext *cx, const Value &v)
{
    /*
     * The argument to {add,remove,has}Debuggee may be
     *   - a Debugger.Object belonging to this Debugger: return its referent
     *   - a cross-compartment wrapper: return the wrapped object
     *   - any other non-Debugger.Object object: return it
     * If it is a primitive, or a Debugger.Object that belongs to some other
     * Debugger, throw a TypeError.
     */
    JSObject *obj = NonNullObject(cx, v);
    if (obj) {
        if (obj->clasp == &DebuggerObject_class) {
            Value rv = v;
            if (!unwrapDebuggeeValue(cx, &rv))
                return NULL;
            return &rv.toObject();
        }
        if (obj->isCrossCompartmentWrapper())
            return &obj->getProxyPrivate().toObject();
    }
    return obj;
}

JSBool
Debugger::addDebuggee(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.addDebuggee", 1);
    THIS_DEBUGGER(cx, argc, vp, "addDebuggee", args, dbg);
    JSObject *referent = dbg->unwrapDebuggeeArgument(cx, args[0]);
    if (!referent)
        return false;
    GlobalObject *global = referent->getGlobal();
    if (!dbg->addDebuggeeGlobal(cx, global))
        return false;

    Value v = ObjectValue(*referent);
    if (!dbg->wrapDebuggeeValue(cx, &v))
        return false;
    args.rval() = v;
    return true;
}

JSBool
Debugger::removeDebuggee(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.removeDebuggee", 1);
    THIS_DEBUGGER(cx, argc, vp, "removeDebuggee", args, dbg);
    JSObject *referent = dbg->unwrapDebuggeeArgument(cx, args[0]);
    if (!referent)
        return false;
    GlobalObject *global = referent->getGlobal();
    if (dbg->debuggees.has(global))
        dbg->removeDebuggeeGlobal(cx, global, NULL, NULL);
    args.rval().setUndefined();
    return true;
}

JSBool
Debugger::hasDebuggee(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.hasDebuggee", 1);
    THIS_DEBUGGER(cx, argc, vp, "hasDebuggee", args, dbg);
    JSObject *referent = dbg->unwrapDebuggeeArgument(cx, args[0]);
    if (!referent)
        return false;
    args.rval().setBoolean(!!dbg->debuggees.lookup(referent->getGlobal()));
    return true;
}

JSBool
Debugger::getDebuggees(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGGER(cx, argc, vp, "getDebuggees", args, dbg);
    JSObject *arrobj = NewDenseAllocatedArray(cx, dbg->debuggees.count(), NULL);
    if (!arrobj)
        return false;
    uintN i = 0;
    for (GlobalObjectSet::Enum e(dbg->debuggees); !e.empty(); e.popFront()) {
        Value v = ObjectValue(*e.front());
        if (!dbg->wrapDebuggeeValue(cx, &v))
            return false;
        arrobj->setDenseArrayElement(i++, v);
    }
    args.rval().setObject(*arrobj);
    return true;
}

JSBool
Debugger::getNewestFrame(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGGER(cx, argc, vp, "getNewestFrame", args, dbg);

    /*
     * cx->fp() would return the topmost frame in the current context.
     * Since there may be multiple contexts, use AllFramesIter instead.
     */
    for (AllFramesIter i(cx->stack.space()); !i.done(); ++i) {
        if (dbg->observesFrame(i.fp()))
            return dbg->getScriptFrame(cx, i.fp(), vp);
    }
    args.rval().setNull();
    return true;
}

JSBool
Debugger::clearAllBreakpoints(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGGER(cx, argc, vp, "clearAllBreakpoints", args, dbg);
    for (GlobalObjectSet::Range r = dbg->debuggees.all(); !r.empty(); r.popFront())
        r.front()->compartment()->clearBreakpointsIn(cx, dbg, NULL, NULL);
    return true;
}

JSBool
Debugger::construct(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Check that the arguments, if any, are cross-compartment wrappers. */
    for (uintN i = 0; i < argc; i++) {
        const Value &arg = args[i];
        if (!arg.isObject())
            return ReportObjectRequired(cx);
        JSObject *argobj = &arg.toObject();
        if (!argobj->isCrossCompartmentWrapper()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CCW_REQUIRED, "Debugger");
            return false;
        }
    }

    /* Get Debugger.prototype. */
    Value v;
    jsid prototypeId = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
    if (!args.callee().getProperty(cx, prototypeId, &v))
        return false;
    JSObject *proto = &v.toObject();
    JS_ASSERT(proto->getClass() == &Debugger::jsclass);

    /*
     * Make the new Debugger object. Each one has a reference to
     * Debugger.{Frame,Object,Script}.prototype in reserved slots. The rest of
     * the reserved slots are for hooks; they default to undefined.
     */
    JSObject *obj = NewNonFunction<WithProto::Given>(cx, &Debugger::jsclass, proto, NULL);
    if (!obj || !obj->ensureClassReservedSlots(cx))
        return false;
    for (uintN slot = JSSLOT_DEBUG_PROTO_START; slot < JSSLOT_DEBUG_PROTO_STOP; slot++)
        obj->setReservedSlot(slot, proto->getReservedSlot(slot));

    Debugger *dbg = cx->new_<Debugger>(cx, obj);
    if (!dbg)
        return false;
    obj->setPrivate(dbg);
    if (!dbg->init(cx)) {
        cx->delete_(dbg);
        return false;
    }

    /* Add the initial debuggees, if any. */
    for (uintN i = 0; i < argc; i++) {
        GlobalObject *debuggee = args[i].toObject().getProxyPrivate().toObject().getGlobal();
        if (!dbg->addDebuggeeGlobal(cx, debuggee))
            return false;
    }

    args.rval().setObject(*obj);
    return true;
}

bool
Debugger::addDebuggeeGlobal(JSContext *cx, GlobalObject *global)
{
    if (debuggees.has(global))
        return true;

    JSCompartment *debuggeeCompartment = global->compartment();

    /*
     * Check for cycles. If global's compartment is reachable from this
     * Debugger object's compartment by following debuggee-to-debugger links,
     * then adding global would create a cycle. (Typically nobody is debugging
     * the debugger, in which case we zip through this code without looping.)
     */
    Vector<JSCompartment *> visited(cx);
    if (!visited.append(object->compartment()))
        return false;
    for (size_t i = 0; i < visited.length(); i++) {
        JSCompartment *c = visited[i];
        if (c == debuggeeCompartment) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_LOOP);
            return false;
        }

        /*
         * Find all compartments containing debuggers debugging global objects
         * in c. Add those compartments to visited.
         */
        for (GlobalObjectSet::Range r = c->getDebuggees().all(); !r.empty(); r.popFront()) {
            GlobalObject::DebuggerVector *v = r.front()->getDebuggers();
            for (Debugger **p = v->begin(); p != v->end(); p++) {
                JSCompartment *next = (*p)->object->compartment();
                if (Find(visited, next) == visited.end() && !visited.append(next))
                    return false;
            }
        }
    }

    /* Refuse to enable debug mode for a compartment that has running scripts. */
    if (!debuggeeCompartment->debugMode() && debuggeeCompartment->hasScriptsOnStack(cx)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_NOT_IDLE);
        return false;
    }

    /*
     * Each debugger-debuggee relation must be stored in up to three places.
     * JSCompartment::addDebuggee enables debug mode if needed.
     */
    AutoCompartment ac(cx, global);
    if (!ac.enter())
        return false;
    GlobalObject::DebuggerVector *v = global->getOrCreateDebuggers(cx);
    if (!v || !v->append(this)) {
        js_ReportOutOfMemory(cx);
    } else {
        if (!debuggees.put(global)) {
            js_ReportOutOfMemory(cx);
        } else {
            if (global->getDebuggers()->length() > 1)
                return true;
            if (debuggeeCompartment->addDebuggee(cx, global))
                return true;

            /* Maintain consistency on error. */
            debuggees.remove(global);
        }
        JS_ASSERT(v->back() == this);
        v->popBack();
    }
    return false;
}

void
Debugger::removeDebuggeeGlobal(JSContext *cx, GlobalObject *global,
                               GlobalObjectSet::Enum *compartmentEnum,
                               GlobalObjectSet::Enum *debugEnum)
{
    /*
     * Each debuggee is in two HashSets: one for its compartment and one for
     * its debugger (this). The caller might be enumerating either set; if so,
     * use HashSet::Enum::removeFront rather than HashSet::remove below, to
     * avoid invalidating the live enumerator.
     */
    JS_ASSERT(global->compartment()->getDebuggees().has(global));
    JS_ASSERT_IF(compartmentEnum, compartmentEnum->front() == global);
    JS_ASSERT(debuggees.has(global));
    JS_ASSERT_IF(debugEnum, debugEnum->front() == global);

    /*
     * FIXME Debugger::slowPathOnLeaveFrame needs to kill all Debugger.Frame
     * objects referring to a particular js::StackFrame. This is hard if
     * Debugger objects that are no longer debugging the relevant global might
     * have live Frame objects. So we take the easy way out and kill them
     * here. This is a bug, since it's observable and contrary to the spec. One
     * possible fix would be to put such objects into a compartment-wide bag
     * which slowPathOnLeaveFrame would have to examine.
     */
    for (FrameMap::Enum e(frames); !e.empty(); e.popFront()) {
        js::StackFrame *fp = e.front().key;
        if (fp->scopeChain().getGlobal() == global) {
            e.front().value->setPrivate(NULL);
            e.removeFront();
        }
    }

    GlobalObject::DebuggerVector *v = global->getDebuggers();
    Debugger **p;
    for (p = v->begin(); p != v->end(); p++) {
        if (*p == this)
            break;
    }
    JS_ASSERT(p != v->end());

    /*
     * The relation must be removed from up to three places: *v and debuggees
     * for sure, and possibly the compartment's debuggee set.
     */
    v->erase(p);
    if (v->empty())
        global->compartment()->removeDebuggee(cx, global, compartmentEnum);
    if (debugEnum)
        debugEnum->removeFront();
    else
        debuggees.remove(global);
}

JSPropertySpec Debugger::properties[] = {
    JS_PSGS("enabled", Debugger::getEnabled, Debugger::setEnabled, 0),
    JS_PSGS("onDebuggerStatement", Debugger::getOnDebuggerStatement,
            Debugger::setOnDebuggerStatement, 0),
    JS_PSGS("onExceptionUnwind", Debugger::getOnExceptionUnwind,
            Debugger::setOnExceptionUnwind, 0),
    JS_PSGS("onNewScript", Debugger::getOnNewScript, Debugger::setOnNewScript, 0),
    JS_PSGS("onEnterFrame", Debugger::getOnEnterFrame, Debugger::setOnEnterFrame, 0),
    JS_PSGS("uncaughtExceptionHook", Debugger::getUncaughtExceptionHook,
            Debugger::setUncaughtExceptionHook, 0),
    JS_PS_END
};

JSFunctionSpec Debugger::methods[] = {
    JS_FN("addDebuggee", Debugger::addDebuggee, 1, 0),
    JS_FN("removeDebuggee", Debugger::removeDebuggee, 1, 0),
    JS_FN("hasDebuggee", Debugger::hasDebuggee, 1, 0),
    JS_FN("getDebuggees", Debugger::getDebuggees, 0, 0),
    JS_FN("getNewestFrame", Debugger::getNewestFrame, 0, 0),
    JS_FN("clearAllBreakpoints", Debugger::clearAllBreakpoints, 1, 0),
    JS_FS_END
};


/*** Debugger.Script *****************************************************************************/

/*
 * JSScripts' lifetimes fall into to two categories:
 *
 * - "Held scripts": JSScripts belonging to JSFunctions and JSScripts created
 *   using JSAPI have lifetimes determined by the garbage collector. A JSScript
 *   itself has no mark bit of its own. Instead, its holding object manages the
 *   JSScript as part of its own structure: the holder has a mark bit; when the
 *   holder is marked it calls js_TraceScript on its JSScript; and when the
 *   holder is freed it explicitly frees its JSScript.
 *
 *   Debugger.Script instances for held scripts are strong references to the
 *   holder (and thus to the script). Debugger::heldScripts weakly maps
 *   debuggee holding objects to the Debugger.Script objects for their
 *   JSScripts. We needn't act on a destroyScript event for a held script: if
 *   we get such an event we know its Debugger.Script is dead anyway, and its
 *   entry in Debugger::heldScripts will be cleaned up by the standard weak
 *   table code.
 *
 * - "Non-held scripts": JSScripts generated temporarily for a call to eval or
 *   JS_Evaluate*, live until the call completes, at which point the script is
 *   destroyed.
 *
 *   A Debugger.Script instance for a non-held script has no influence on the
 *   JSScript's lifetime. Debugger::nonHeldScripts maps live JSScripts to to
 *   their Debugger.Script objects.  When a destroyScript event tells us that
 *   a non-held script is dead, we remove its table entry, and clear its
 *   Debugger.Script object's script pointer, thus marking it dead.
 *
 * A Debugger.Script's private pointer points directly to the JSScript, or is
 * NULL if the Debugger.Script is dead. The JSSLOT_DEBUGSCRIPT_HOLDER slot
 * refers to the holding object, or is null for non-held JSScripts. The private
 * pointer is not traced; the holding object reference, if present, is traced
 * via DebuggerScript_trace.
 *
 * (We consider a script saved in and retrieved from the eval cache to have
 * been destroyed, and then --- mirabile dictu --- re-created at the same
 * address. The newScriptHook and destroyScriptHook hooks cooperate with this
 * view.)
 */
static inline JSScript *
GetScriptReferent(JSObject *obj)
{
    JS_ASSERT(obj->getClass() == &DebuggerScript_class);
    return (JSScript *) obj->getPrivate();
}

static inline void
ClearScriptReferent(JSObject *obj)
{
    JS_ASSERT(obj->getClass() == &DebuggerScript_class);
    obj->setPrivate(NULL);
}

static inline JSObject *
GetScriptHolder(JSObject *obj)
{
    JS_ASSERT(obj->getClass() == &DebuggerScript_class);
    Value v = obj->getReservedSlot(JSSLOT_DEBUGSCRIPT_HOLDER);
    return (JSObject *) v.toPrivate();
}

static void
DebuggerScript_trace(JSTracer *trc, JSObject *obj)
{
    if (!trc->context->runtime->gcCurrentCompartment) {
        Value v = obj->getReservedSlot(JSSLOT_DEBUGSCRIPT_HOLDER);
        if (!v.isUndefined()) {
            if (JSObject *obj = (JSObject *) v.toPrivate())
                MarkObject(trc, *obj, "Debugger.Script referent holder");
        }
    }
}

Class DebuggerScript_class = {
    "Script", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGSCRIPT_COUNT),
    PropertyStub, PropertyStub, PropertyStub, StrictPropertyStub,
    EnumerateStub, ResolveStub, ConvertStub, NULL,
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    DebuggerScript_trace
};

JSObject *
Debugger::newDebuggerScript(JSContext *cx, JSScript *script, JSObject *holder)
{
    assertSameCompartment(cx, object);

    JSObject *proto = &object->getReservedSlot(JSSLOT_DEBUG_SCRIPT_PROTO).toObject();
    JS_ASSERT(proto);
    JSObject *scriptobj = NewNonFunction<WithProto::Given>(cx, &DebuggerScript_class, proto, NULL);
    if (!scriptobj || !scriptobj->ensureClassReservedSlots(cx))
        return false;
    scriptobj->setPrivate(script);
    scriptobj->setReservedSlot(JSSLOT_DEBUGSCRIPT_OWNER, ObjectValue(*object));
    scriptobj->setReservedSlot(JSSLOT_DEBUGSCRIPT_HOLDER, PrivateValue(holder));

    return scriptobj;
}

JSObject *
Debugger::wrapHeldScript(JSContext *cx, JSScript *script, JSObject *obj)
{
    assertSameCompartment(cx, object);
    JS_ASSERT(cx->compartment != script->compartment);
    JS_ASSERT(script->compartment == obj->compartment());

    ScriptWeakMap::AddPtr p = heldScripts.lookupForAdd(obj);
    if (!p) {
        JSObject *scriptobj = newDebuggerScript(cx, script, obj);

        /* The allocation may have caused a GC, which can remove table entries. */
        if (!scriptobj || !heldScripts.relookupOrAdd(p, obj, scriptobj))
            return NULL;
    }

    JS_ASSERT(GetScriptReferent(p->value) == script);
    return p->value;
}

JSObject *
Debugger::wrapFunctionScript(JSContext *cx, JSFunction *fun)
{
    return wrapHeldScript(cx, fun->script(), fun);
}

JSObject *
Debugger::wrapJSAPIScript(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isScript());
    return wrapHeldScript(cx, obj->getScript(), obj);
}

JSObject *
Debugger::wrapNonHeldScript(JSContext *cx, JSScript *script)
{
    assertSameCompartment(cx, object);
    JS_ASSERT(cx->compartment != script->compartment);

    ScriptMap::AddPtr p = nonHeldScripts.lookupForAdd(script);
    if (!p) {
        JSObject *scriptobj = newDebuggerScript(cx, script, NULL);

        /* The allocation may have caused a GC, which can remove table entries. */
        if (!scriptobj || !nonHeldScripts.relookupOrAdd(p, script, scriptobj))
            return NULL;
    }

    JS_ASSERT(GetScriptReferent(p->value) == script);
    return p->value;
}

void
Debugger::slowPathOnDestroyScript(JSScript *script)
{
    /* Find all debuggers that might have Debugger.Script referring to this script. */
    js::GlobalObjectSet *debuggees = &script->compartment->getDebuggees();
    for (GlobalObjectSet::Range r = debuggees->all(); !r.empty(); r.popFront()) {
        GlobalObject::DebuggerVector *debuggers = r.front()->getDebuggers();
        for (Debugger **p = debuggers->begin(); p != debuggers->end(); p++)
            (*p)->destroyNonHeldScript(script);
    }
}

void
Debugger::destroyNonHeldScript(JSScript *script)
{
    ScriptMap::Ptr p = nonHeldScripts.lookup(script);
    if (p) {
        JS_ASSERT(GetScriptReferent(p->value) == script);
        ClearScriptReferent(p->value);
        nonHeldScripts.remove(p);
    }
}

static JSObject *
DebuggerScript_check(JSContext *cx, const Value &v, const char *clsname, const char *fnname,
                     bool checkLive)
{
    if (!v.isObject()) {
        ReportObjectRequired(cx);
        return NULL;
    }
    JSObject *thisobj = &v.toObject();
    if (thisobj->clasp != &DebuggerScript_class) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             clsname, fnname, thisobj->getClass()->name);
        return NULL;
    }

    /*
     * Check for Debugger.Script.prototype, which is of class DebuggerScript_class
     * but whose holding object is undefined.
     */
    if (thisobj->getReservedSlot(JSSLOT_DEBUGSCRIPT_HOLDER).isUndefined()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             clsname, fnname, "prototype object");
        return NULL;
    }

    if (checkLive && !GetScriptReferent(thisobj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_NOT_LIVE,
                             clsname, fnname, "script");
        return NULL;
    }

    return thisobj;
}

static JSObject *
DebuggerScript_checkThis(JSContext *cx, const CallArgs &args, const char *fnname, bool checkLive)
{
    return DebuggerScript_check(cx, args.thisv(), "Debugger.Script", fnname, checkLive);
}

#define THIS_DEBUGSCRIPT_SCRIPT_NEEDLIVE(cx, argc, vp, fnname, args, obj, script, checkLive) \
    CallArgs args = CallArgsFromVp(argc, vp);                                       \
    JSObject *obj = DebuggerScript_checkThis(cx, args, fnname, checkLive);          \
    if (!obj)                                                                       \
        return false;                                                               \
    JSScript *script = GetScriptReferent(obj)

#define THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, fnname, args, obj, script)            \
    THIS_DEBUGSCRIPT_SCRIPT_NEEDLIVE(cx, argc, vp, fnname, args, obj, script, false)
#define THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, argc, vp, fnname, args, obj, script)       \
    THIS_DEBUGSCRIPT_SCRIPT_NEEDLIVE(cx, argc, vp, fnname, args, obj, script, true)

static JSBool
DebuggerScript_getUrl(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, argc, vp, "get url", args, obj, script);

    JSString *str = js_NewStringCopyZ(cx, script->filename);
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static JSBool
DebuggerScript_getStartLine(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, argc, vp, "get startLine", args, obj, script);
    args.rval().setNumber(script->lineno);
    return true;
}

static JSBool
DebuggerScript_getLineCount(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, argc, vp, "get lineCount", args, obj, script);

    uintN maxLine = js_GetScriptLineExtent(script);
    args.rval().setNumber(jsdouble(maxLine));
    return true;
}

static JSBool
DebuggerScript_getLive(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, "get live", args, obj, script);
    args.rval().setBoolean(!!script);
    return true;
}

static JSBool
DebuggerScript_getChildScripts(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, argc, vp, "get live", args, obj, script);
    Debugger *dbg = Debugger::fromChildJSObject(obj);

    JSObject *result = NewDenseEmptyArray(cx);
    if (!result)
        return false;
    if (JSScript::isValidOffset(script->objectsOffset)) {
        /*
         * script->savedCallerFun indicates that this is a direct eval script
         * and the calling function is stored as script->objects()->vector[0].
         * It is not really a child script of this script, so skip it.
         */
        JSObjectArray *objects = script->objects();
        for (uint32 i = script->savedCallerFun ? 1 : 0; i < objects->length; i++) {
            JSObject *obj = objects->vector[i];
            if (obj->isFunction()) {
                JSObject *s = dbg->wrapFunctionScript(cx, (JSFunction *) obj);
                if (!s || !js_NewbornArrayPush(cx, result, ObjectValue(*s)))
                    return false;
            }
        }
    }
    args.rval().setObject(*result);
    return true;
}

static bool
ScriptOffset(JSContext *cx, JSScript *script, const Value &v, size_t *offsetp)
{
    double d;
    size_t off;

    bool ok = v.isNumber();
    if (ok) {
        d = v.toNumber();
        off = size_t(d);
    }
    if (!ok || off != d || !IsValidBytecodeOffset(cx, script, off)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_BAD_OFFSET);
        return false;
    }
    *offsetp = off;
    return true;
}

static JSBool
DebuggerScript_getOffsetLine(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.Script.getOffsetLine", 1);
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, argc, vp, "getOffsetLine", args, obj, script);
    size_t offset;
    if (!ScriptOffset(cx, script, args[0], &offset))
        return false;
    uintN lineno = JS_PCToLineNumber(cx, script, script->code + offset);
    args.rval().setNumber(lineno);
    return true;
}

class BytecodeRangeWithLineNumbers : private BytecodeRange
{
  public:
    using BytecodeRange::empty;
    using BytecodeRange::frontPC;
    using BytecodeRange::frontOpcode;
    using BytecodeRange::frontOffset;

    BytecodeRangeWithLineNumbers(JSContext *cx, JSScript *script)
      : BytecodeRange(cx, script), lineno(script->lineno), sn(script->notes()), snpc(script->code)
    {
        if (!SN_IS_TERMINATOR(sn))
            snpc += SN_DELTA(sn);
        updateLine();
    }

    void popFront() {
        BytecodeRange::popFront();
        if (!empty())
            updateLine();
    }

    size_t frontLineNumber() const { return lineno; }

  private:
    void updateLine() {
        /*
         * Determine the current line number by reading all source notes up to
         * and including the current offset.
         */
        while (!SN_IS_TERMINATOR(sn) && snpc <= frontPC()) {
            JSSrcNoteType type = (JSSrcNoteType) SN_TYPE(sn);
            if (type == SRC_SETLINE)
                lineno = size_t(js_GetSrcNoteOffset(sn, 0));
            else if (type == SRC_NEWLINE)
                lineno++;

            sn = SN_NEXT(sn);
            snpc += SN_DELTA(sn);
        }
    }

    size_t lineno;
    jssrcnote *sn;
    jsbytecode *snpc;
};

static const size_t NoEdges = -1;
static const size_t MultipleEdges = -2;

/*
 * FlowGraphSummary::populate(cx, script) computes a summary of script's
 * control flow graph used by DebuggerScript_{getAllOffsets,getLineOffsets}.
 *
 * jumpData[offset] is:
 *   - NoEdges if offset isn't the offset of an instruction, or if the
 *     instruction is apparently unreachable;
 *   - MultipleEdges if you can arrive at that instruction from
 *     instructions on multiple different lines OR it's the first
 *     instruction of the script;
 *   - otherwise, the (unique) line number of all instructions that can
 *     precede the instruction at offset.
 *
 * The generated graph does not contain edges for JSOP_RETSUB, which appears at
 * the end of finally blocks. The algorithm that uses this information works
 * anyway, because in non-exception cases, JSOP_RETSUB always returns to a
 * !FlowsIntoNext instruction (JSOP_GOTO/GOTOX or JSOP_RETRVAL) which generates
 * an edge if needed.
 */
class FlowGraphSummary : public Vector<size_t> {
  public:
    typedef Vector<size_t> Base;
    FlowGraphSummary(JSContext *cx) : Base(cx) {}

    void addEdge(size_t sourceLine, size_t targetOffset) {
        FlowGraphSummary &self = *this;
        if (self[targetOffset] == NoEdges)
            self[targetOffset] = sourceLine;
        else if (self[targetOffset] != sourceLine)
            self[targetOffset] = MultipleEdges;
    }

    void addEdgeFromAnywhere(size_t targetOffset) {
        (*this)[targetOffset] = MultipleEdges;
    }

    bool populate(JSContext *cx, JSScript *script) {
        if (!growBy(script->length))
            return false;
        FlowGraphSummary &self = *this;
        self[0] = MultipleEdges;
        for (size_t i = 1; i < script->length; i++)
            self[i] = NoEdges;

        size_t prevLine = script->lineno;
        JSOp prevOp = JSOP_NOP;
        for (BytecodeRangeWithLineNumbers r(cx, script); !r.empty(); r.popFront()) {
            size_t lineno = r.frontLineNumber();
            JSOp op = r.frontOpcode();

            if (FlowsIntoNext(prevOp))
                addEdge(prevLine, r.frontOffset());

            if (js_CodeSpec[op].type() == JOF_JUMP) {
                addEdge(lineno, r.frontOffset() + GET_JUMP_OFFSET(r.frontPC()));
            } else if (js_CodeSpec[op].type() == JOF_JUMPX) {
                addEdge(lineno, r.frontOffset() + GET_JUMPX_OFFSET(r.frontPC()));
            } else if (op == JSOP_TABLESWITCH || op == JSOP_TABLESWITCHX ||
                       op == JSOP_LOOKUPSWITCH || op == JSOP_LOOKUPSWITCHX) {
                bool table = op == JSOP_TABLESWITCH || op == JSOP_TABLESWITCHX;
                bool big = op == JSOP_TABLESWITCHX || op == JSOP_LOOKUPSWITCHX;

                jsbytecode *pc = r.frontPC();
                size_t offset = r.frontOffset();
                ptrdiff_t step = big ? JUMPX_OFFSET_LEN : JUMP_OFFSET_LEN;
                size_t defaultOffset = offset + (big ? GET_JUMPX_OFFSET(pc) : GET_JUMP_OFFSET(pc));
                pc += step;
                addEdge(lineno, defaultOffset);

                jsint ncases;
                if (table) {
                    jsint low = GET_JUMP_OFFSET(pc);
                    pc += JUMP_OFFSET_LEN;
                    ncases = GET_JUMP_OFFSET(pc) - low + 1;
                    pc += JUMP_OFFSET_LEN;
                } else {
                    ncases = (jsint) GET_UINT16(pc);
                    pc += UINT16_LEN;
                    JS_ASSERT(ncases > 0);
                }

                for (jsint i = 0; i < ncases; i++) {
                    if (!table)
                        pc += INDEX_LEN;
                    size_t target = offset + (big ? GET_JUMPX_OFFSET(pc) : GET_JUMP_OFFSET(pc));
                    addEdge(lineno, target);
                    pc += step;
                }
            }

            prevOp = op;
            prevLine = lineno;
        }

        return true;
    }
};

static JSBool
DebuggerScript_getAllOffsets(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, argc, vp, "getAllOffsets", args, obj, script);

    /*
     * First pass: determine which offsets in this script are jump targets and
     * which line numbers jump to them.
     */
    FlowGraphSummary flowData(cx);
    if (!flowData.populate(cx, script))
        return false;

    /* Second pass: build the result array. */
    JSObject *result = NewDenseEmptyArray(cx);
    if (!result)
        return false;
    for (BytecodeRangeWithLineNumbers r(cx, script); !r.empty(); r.popFront()) {
        size_t offset = r.frontOffset();
        size_t lineno = r.frontLineNumber();

        /* Make a note, if the current instruction is an entry point for the current line. */
        if (flowData[offset] != NoEdges && flowData[offset] != lineno) {
            /* Get the offsets array for this line. */
            JSObject *offsets;
            Value offsetsv;
            if (!result->arrayGetOwnDataElement(cx, lineno, &offsetsv))
                return false;

            jsid id;
            if (offsetsv.isObject()) {
                offsets = &offsetsv.toObject();
            } else {
                JS_ASSERT(offsetsv.isMagic(JS_ARRAY_HOLE));

                /*
                 * Create an empty offsets array for this line.
                 * Store it in the result array.
                 */
                offsets = NewDenseEmptyArray(cx);
                if (!offsets ||
                    !ValueToId(cx, NumberValue(lineno), &id) ||
                    !result->defineProperty(cx, id, ObjectValue(*offsets)))
                {
                    return false;
                }
            }

            /* Append the current offset to the offsets array. */
            if (!js_NewbornArrayPush(cx, offsets, NumberValue(offset)))
                return false;
        }
    }

    args.rval().setObject(*result);
    return true;
}

static JSBool
DebuggerScript_getLineOffsets(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, argc, vp, "getAllOffsets", args, obj, script);
    REQUIRE_ARGC("Debugger.Script.getLineOffsets", 1);

    /* Parse lineno argument. */
    size_t lineno;
    bool ok = false;
    if (args[0].isNumber()) {
        jsdouble d = args[0].toNumber();
        lineno = size_t(d);
        ok = (lineno == d);
    }
    if (!ok) {
        JS_ReportErrorNumber(cx,  js_GetErrorMessage, NULL, JSMSG_DEBUG_BAD_LINE);
        return false;
    }

    /*
     * First pass: determine which offsets in this script are jump targets and
     * which line numbers jump to them.
     */
    FlowGraphSummary flowData(cx);
    if (!flowData.populate(cx, script))
        return false;

    /* Second pass: build the result array. */
    JSObject *result = NewDenseEmptyArray(cx);
    if (!result)
        return false;
    for (BytecodeRangeWithLineNumbers r(cx, script); !r.empty(); r.popFront()) {
        size_t offset = r.frontOffset();

        /* If the op at offset is an entry point, append offset to result. */
        if (r.frontLineNumber() == lineno &&
            flowData[offset] != NoEdges &&
            flowData[offset] != lineno)
        {
            if (!js_NewbornArrayPush(cx, result, NumberValue(offset)))
                return false;
        }
    }

    args.rval().setObject(*result);
    return true;
}

static JSBool
DebuggerScript_setBreakpoint(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.Script.setBreakpoint", 2);
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, argc, vp, "setBreakpoint", args, obj, script);
    Debugger *dbg = Debugger::fromChildJSObject(obj);

    JSObject *holder = GetScriptHolder(obj);
    if (!dbg->observesScope(ScriptScope(cx, script, holder))) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_NOT_DEBUGGING);
        return false;
    }

    size_t offset;
    if (!ScriptOffset(cx, script, args[0], &offset))
        return false;

    JSObject *handler = NonNullObject(cx, args[1]);
    if (!handler)
        return false;

    JSCompartment *comp = script->compartment;
    jsbytecode *pc = script->code + offset;
    BreakpointSite *site = comp->getOrCreateBreakpointSite(cx, script, pc, holder);
    if (!site)
        return false;
    if (site->inc(cx)) {
        if (cx->runtime->new_<Breakpoint>(dbg, site, handler)) {
            args.rval().setUndefined();
            return true;
        }
        site->dec(cx);
    }
    site->destroyIfEmpty(cx->runtime, NULL);
    return false;
}

static JSBool
DebuggerScript_getBreakpoints(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, argc, vp, "getBreakpoints", args, obj, script);
    Debugger *dbg = Debugger::fromChildJSObject(obj);

    jsbytecode *pc;
    if (argc > 0) {
        size_t offset;
        if (!ScriptOffset(cx, script, args[0], &offset))
            return false;
        pc = script->code + offset;
    } else {
        pc = NULL;
    }

    JSObject *arr = NewDenseEmptyArray(cx);
    if (!arr)
        return false;
    JSCompartment *comp = script->compartment;
    for (BreakpointSiteMap::Range r = comp->breakpointSites.all(); !r.empty(); r.popFront()) {
        BreakpointSite *site = r.front().value;
        if (site->script == script && (!pc || site->pc == pc)) {
            for (Breakpoint *bp = site->firstBreakpoint(); bp; bp = bp->nextInSite()) {
                if (bp->debugger == dbg &&
                    !js_NewbornArrayPush(cx, arr, ObjectValue(*bp->getHandler())))
                {
                    return false;
                }
            }
        }
    }
    args.rval().setObject(*arr);
    return true;
}

static JSBool
DebuggerScript_clearBreakpoint(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.Script.clearBreakpoint", 1);
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, argc, vp, "clearBreakpoint", args, obj, script);
    Debugger *dbg = Debugger::fromChildJSObject(obj);

    JSObject *handler = NonNullObject(cx, args[0]);
    if (!handler)
        return false;

    script->compartment->clearBreakpointsIn(cx, dbg, script, handler);
    args.rval().setUndefined();
    return true;
}

static JSBool
DebuggerScript_clearAllBreakpoints(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, argc, vp, "clearBreakpoint", args, obj, script);
    Debugger *dbg = Debugger::fromChildJSObject(obj);
    script->compartment->clearBreakpointsIn(cx, dbg, script, NULL);
    args.rval().setUndefined();
    return true;
}

static JSBool
DebuggerScript_construct(JSContext *cx, uintN argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR, "Debugger.Script");
    return false;
}

static JSPropertySpec DebuggerScript_properties[] = {
    JS_PSG("url", DebuggerScript_getUrl, 0),
    JS_PSG("startLine", DebuggerScript_getStartLine, 0),
    JS_PSG("lineCount", DebuggerScript_getLineCount, 0),
    JS_PSG("live", DebuggerScript_getLive, 0),
    JS_PS_END
};

static JSFunctionSpec DebuggerScript_methods[] = {
    JS_FN("getChildScripts", DebuggerScript_getChildScripts, 0, 0),
    JS_FN("getAllOffsets", DebuggerScript_getAllOffsets, 0, 0),
    JS_FN("getLineOffsets", DebuggerScript_getLineOffsets, 1, 0),
    JS_FN("getOffsetLine", DebuggerScript_getOffsetLine, 0, 0),
    JS_FN("setBreakpoint", DebuggerScript_setBreakpoint, 2, 0),
    JS_FN("getBreakpoints", DebuggerScript_getBreakpoints, 1, 0),
    JS_FN("clearBreakpoint", DebuggerScript_clearBreakpoint, 1, 0),
    JS_FN("clearAllBreakpoints", DebuggerScript_clearAllBreakpoints, 0, 0),
    JS_FS_END
};


/*** Debugger.Frame ******************************************************************************/

Class DebuggerFrame_class = {
    "Frame", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGFRAME_COUNT),
    PropertyStub, PropertyStub, PropertyStub, StrictPropertyStub,
    EnumerateStub, ResolveStub, ConvertStub
};

static JSObject *
CheckThisFrame(JSContext *cx, const CallArgs &args, const char *fnname, bool checkLive)
{
    if (!args.thisv().isObject()) {
        ReportObjectRequired(cx);
        return NULL;
    }
    JSObject *thisobj = &args.thisv().toObject();
    if (thisobj->getClass() != &DebuggerFrame_class) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Debugger.Frame", fnname, thisobj->getClass()->name);
        return NULL;
    }

    /*
     * Forbid Debugger.Frame.prototype, which is of class DebuggerFrame_class
     * but isn't really a working Debugger.Frame object. The prototype object
     * is distinguished by having a NULL private value. Also, forbid popped
     * frames.
     */
    if (!thisobj->getPrivate()) {
        if (thisobj->getReservedSlot(JSSLOT_DEBUGFRAME_OWNER).isUndefined()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                                 "Debugger.Frame", fnname, "prototype object");
            return NULL;
        }
        if (checkLive) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_NOT_LIVE,
                                 "Debugger.Frame", fnname, "stack frame");
            return NULL;
        }
    }
    return thisobj;
}

#if DEBUG
static bool
StackContains(JSContext *cx, StackFrame *fp)
{
    for (AllFramesIter i(cx->stack.space()); !i.done(); ++i) {
        if (fp == i.fp())
            return true;
    }
    return false;
}
#endif

#define THIS_FRAME(cx, argc, vp, fnname, args, thisobj, fp)                  \
    CallArgs args = CallArgsFromVp(argc, vp);                                \
    JSObject *thisobj = CheckThisFrame(cx, args, fnname, true);              \
    if (!thisobj)                                                            \
        return false;                                                        \
    StackFrame *fp = (StackFrame *) thisobj->getPrivate();                   \
    JS_ASSERT(StackContains(cx, fp))

static JSBool
DebuggerFrame_getType(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get type", args, thisobj, fp);

    /*
     * Indirect eval frames are both isGlobalFrame() and isEvalFrame(), so the
     * order of checks here is significant.
     */
    args.rval().setString(fp->isEvalFrame()
                          ? cx->runtime->atomState.evalAtom
                          : fp->isGlobalFrame()
                          ? cx->runtime->atomState.globalAtom
                          : cx->runtime->atomState.callAtom);
    return true;
}

static JSBool
DebuggerFrame_getCallee(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get callee", args, thisobj, fp);
    Value calleev = (fp->isFunctionFrame() && !fp->isEvalFrame()) ? fp->calleev() : NullValue();
    if (!Debugger::fromChildJSObject(thisobj)->wrapDebuggeeValue(cx, &calleev))
        return false;
    args.rval() = calleev;
    return true;
}

static JSBool
DebuggerFrame_getGenerator(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get generator", args, thisobj, fp);
    args.rval().setBoolean(fp->isGeneratorFrame());
    return true;
}

static JSBool
DebuggerFrame_getConstructing(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get constructing", args, thisobj, fp);
    args.rval().setBoolean(fp->isFunctionFrame() && fp->isConstructing());
    return true;
}

static JSBool
DebuggerFrame_getThis(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get this", args, thisobj, fp);
    Value thisv;
    {
        AutoCompartment ac(cx, &fp->scopeChain());
        if (!ac.enter())
            return false;
        if (!ComputeThis(cx, fp))
            return false;
        thisv = fp->thisValue();
    }
    if (!Debugger::fromChildJSObject(thisobj)->wrapDebuggeeValue(cx, &thisv))
        return false;
    args.rval() = thisv;
    return true;
}

static JSBool
DebuggerFrame_getOlder(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get this", args, thisobj, thisfp);
    Debugger *dbg = Debugger::fromChildJSObject(thisobj);
    for (StackFrame *fp = thisfp->prev(); fp; fp = fp->prev()) {
        if (!fp->isDummyFrame() && dbg->observesFrame(fp))
            return dbg->getScriptFrame(cx, fp, vp);
    }
    args.rval().setNull();
    return true;
}

Class DebuggerArguments_class = {
    "Arguments", JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGARGUMENTS_COUNT),
    PropertyStub, PropertyStub, PropertyStub, StrictPropertyStub,
    EnumerateStub, ResolveStub, ConvertStub
};

/* The getter used for each element of frame.arguments. See DebuggerFrame_getArguments. */
static JSBool
DebuggerArguments_getArg(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    int32 i = args.callee().getReservedSlot(0).toInt32();

    /* Check that the this value is an Arguments object. */
    if (!args.thisv().isObject()) {
        ReportObjectRequired(cx);
        return false;
    }
    JSObject *argsobj = &args.thisv().toObject();
    if (argsobj->getClass() != &DebuggerArguments_class) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Arguments", "getArgument", argsobj->getClass()->name);
        return false;
    }

    /*
     * Put the Debugger.Frame into the this-value slot, then use THIS_FRAME
     * to check that it is still live and get the fp.
     */
    args.thisv() = argsobj->getReservedSlot(JSSLOT_DEBUGARGUMENTS_FRAME);
    THIS_FRAME(cx, argc, vp, "get argument", ca2, thisobj, fp);

    /*
     * Since getters can be extracted and applied to other objects,
     * there is no guarantee this object has an ith argument.
     */
    JS_ASSERT(i >= 0);
    Value arg;
    if (uintN(i) < fp->numActualArgs())
        arg = fp->canonicalActualArg(i);
    else
        arg.setUndefined();

    if (!Debugger::fromChildJSObject(thisobj)->wrapDebuggeeValue(cx, &arg))
        return false;
    args.rval() = arg;
    return true;
}

static JSBool
DebuggerFrame_getArguments(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get arguments", args, thisobj, fp);
    Value argumentsv = thisobj->getReservedSlot(JSSLOT_DEBUGFRAME_ARGUMENTS);
    if (!argumentsv.isUndefined()) {
        JS_ASSERT(argumentsv.isObjectOrNull());
        args.rval() = argumentsv;
        return true;
    }

    JSObject *argsobj;
    if (fp->hasArgs()) {
        /* Create an arguments object. */
        GlobalObject *global = args.callee().getGlobal();
        JSObject *proto;
        if (!js_GetClassPrototype(cx, global, JSProto_Array, &proto))
            return false;
        argsobj = NewNonFunction<WithProto::Given>(cx, &DebuggerArguments_class, proto, global);
        if (!argsobj ||
            !js_SetReservedSlot(cx, argsobj, JSSLOT_DEBUGARGUMENTS_FRAME, ObjectValue(*thisobj)))
        {
            return false;
        }

        JS_ASSERT(fp->numActualArgs() <= 0x7fffffff);
        int32 fargc = int32(fp->numActualArgs());
        if (!DefineNativeProperty(cx, argsobj, ATOM_TO_JSID(cx->runtime->atomState.lengthAtom),
                                  Int32Value(fargc), NULL, NULL,
                                  JSPROP_PERMANENT | JSPROP_READONLY, 0, 0))
        {
            return false;
        }

        for (int32 i = 0; i < fargc; i++) {
            JSObject *getobj =
                js_NewFunction(cx, NULL, DebuggerArguments_getArg, 0, 0, global, NULL);
            if (!getobj ||
                !js_SetReservedSlot(cx, getobj, 0, Int32Value(i)) ||
                !DefineNativeProperty(cx, argsobj, INT_TO_JSID(i), UndefinedValue(),
                                      JS_DATA_TO_FUNC_PTR(PropertyOp, getobj), NULL,
                                      JSPROP_ENUMERATE | JSPROP_SHARED | JSPROP_GETTER, 0, 0))
            {
                return false;
            }
        }
    } else {
        argsobj = NULL;
    }
    args.rval() = ObjectOrNullValue(argsobj);
    thisobj->setReservedSlot(JSSLOT_DEBUGFRAME_ARGUMENTS, args.rval());
    return true;
}

static JSBool
DebuggerFrame_getScript(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get script", args, thisobj, fp);
    Debugger *debug = Debugger::fromChildJSObject(thisobj);

    JSObject *scriptObject = NULL;
    if (fp->isFunctionFrame() && !fp->isEvalFrame()) {
        JSFunction *callee = fp->callee().getFunctionPrivate();
        if (callee->isInterpreted()) {
            scriptObject = debug->wrapFunctionScript(cx, callee);
            if (!scriptObject)
                return false;
        }
    } else if (fp->isScriptFrame()) {
        /*
         * eval, JS_Evaluate*, and JS_ExecuteScript all create non-function
         * script frames. However, scripts for JS_ExecuteScript are held by
         * script objects, and must go in heldScripts, whereas scripts for eval
         * and JS_Evaluate* latter are explicitly destroyed when the call
         * returns, and must go in nonHeldScripts. Distinguish the two cases by
         * checking whether the script has a Script object allocated to it.
         */
        JSScript *script = fp->script();
        scriptObject = (script->u.object)
                       ? debug->wrapJSAPIScript(cx, script->u.object)
                       : debug->wrapNonHeldScript(cx, script);
        if (!scriptObject)
            return false;
    }
    args.rval().setObjectOrNull(scriptObject);
    return true;
}

static JSBool
DebuggerFrame_getOffset(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get offset", args, thisobj, fp);
    if (fp->isScriptFrame()) {
        JSScript *script = fp->script();
        jsbytecode *pc = fp->pcQuadratic(cx);
        JS_ASSERT(script->code <= pc);
        JS_ASSERT(pc < script->code + script->length);
        size_t offset = pc - script->code;
        args.rval().setNumber(double(offset));
    } else {
        args.rval().setUndefined();
    }
    return true;
}

static JSBool
DebuggerFrame_getLive(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *thisobj = CheckThisFrame(cx, args, "get live", false);
    if (!thisobj)
        return false;
    StackFrame *fp = (StackFrame *) thisobj->getPrivate();
    args.rval().setBoolean(!!fp);
    return true;
}

namespace js {

JSBool
EvaluateInScope(JSContext *cx, JSObject *scobj, StackFrame *fp, const jschar *chars,
                uintN length, const char *filename, uintN lineno, Value *rval)
{
    assertSameCompartment(cx, scobj, fp);

    /*
     * NB: This function breaks the assumption that the compiler can see all
     * calls and properly compute a static level. In order to get around this,
     * we use a static level that will cause us not to attempt to optimize
     * variable references made by this frame.
     */
    JSScript *script = Compiler::compileScript(cx, scobj, fp, fp->scopeChain().principals(cx),
                                               TCF_COMPILE_N_GO, chars, length,
                                               filename, lineno, cx->findVersion(),
                                               NULL, UpvarCookie::UPVAR_LEVEL_LIMIT);

    if (!script)
        return false;

    bool ok = Execute(cx, script, *scobj, fp->thisValue(), EXECUTE_DEBUG, fp, rval);
    js_DestroyScript(cx, script, 6);
    return ok;
}

}

enum EvalBindingsMode { WithoutBindings, WithBindings };

static JSBool
DebuggerFrameEval(JSContext *cx, uintN argc, Value *vp, EvalBindingsMode mode)
{
    if (mode == WithBindings)
        REQUIRE_ARGC("Debugger.Frame.evalWithBindings", 2);
    else
        REQUIRE_ARGC("Debugger.Frame.eval", 1);
    THIS_FRAME(cx, argc, vp, mode == WithBindings ? "evalWithBindings" : "eval",
               args, thisobj, fp);
    Debugger *dbg = Debugger::fromChildJSObject(thisobj);

    /* Check the first argument, the eval code string. */
    if (!args[0].isString()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_EXPECTED_TYPE,
                             "Debugger.Frame.eval", "string", InformalValueTypeName(args[0]));
        return false;
    }
    JSLinearString *linearStr = args[0].toString()->ensureLinear(cx);
    if (!linearStr)
        return false;

    /*
     * Gather keys and values of bindings, if any. This must be done in the
     * debugger compartment, since that is where any exceptions must be
     * thrown.
     */
    AutoIdVector keys(cx);
    AutoValueVector values(cx);
    if (mode == WithBindings) {
        JSObject *bindingsobj = NonNullObject(cx, args[1]);
        if (!bindingsobj ||
            !GetPropertyNames(cx, bindingsobj, JSITER_OWNONLY, &keys) ||
            !values.growBy(keys.length()))
        {
            return false;
        }
        for (size_t i = 0; i < keys.length(); i++) {
            Value *valp = &values[i];
            if (!bindingsobj->getProperty(cx, bindingsobj, keys[i], valp) ||
                !dbg->unwrapDebuggeeValue(cx, valp))
            {
                return false;
            }
        }
    }

    AutoCompartment ac(cx, &fp->scopeChain());
    if (!ac.enter())
        return false;

    /* Get a scope object. */
    if (fp->isNonEvalFunctionFrame() && !fp->hasCallObj() && !CreateFunCallObject(cx, fp))
        return false;
    JSObject *scobj = GetScopeChain(cx, fp);
    if (!scobj)
        return false;

    /* If evalWithBindings, create the inner scope object. */
    if (mode == WithBindings) {
        /* TODO - Should probably create a With object here. */
        scobj = NewNonFunction<WithProto::Given>(cx, &js_ObjectClass, NULL, scobj);
        if (!scobj)
            return false;
        for (size_t i = 0; i < keys.length(); i++) {
            if (!cx->compartment->wrap(cx, &values[i]) ||
                !DefineNativeProperty(cx, scobj, keys[i], values[i], NULL, NULL, 0, 0, 0))
            {
                return false;
            }
        }
    }

    /* Run the code and produce the completion value. */
    Value rval;
    JS::Anchor<JSString *> anchor(linearStr);
    bool ok = EvaluateInScope(cx, scobj, fp, linearStr->chars(), linearStr->length(),
                              "debugger eval code", 1, &rval);
    return dbg->newCompletionValue(ac, ok, rval, vp);
}

static JSBool
DebuggerFrame_eval(JSContext *cx, uintN argc, Value *vp)
{
    return DebuggerFrameEval(cx, argc, vp, WithoutBindings);
}

static JSBool
DebuggerFrame_evalWithBindings(JSContext *cx, uintN argc, Value *vp)
{
    return DebuggerFrameEval(cx, argc, vp, WithBindings);
}

static JSBool
DebuggerFrame_construct(JSContext *cx, uintN argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR, "Debugger.Frame");
    return false;
}

static JSPropertySpec DebuggerFrame_properties[] = {
    JS_PSG("arguments", DebuggerFrame_getArguments, 0),
    JS_PSG("callee", DebuggerFrame_getCallee, 0),
    JS_PSG("constructing", DebuggerFrame_getConstructing, 0),
    JS_PSG("generator", DebuggerFrame_getGenerator, 0),
    JS_PSG("live", DebuggerFrame_getLive, 0),
    JS_PSG("offset", DebuggerFrame_getOffset, 0),
    JS_PSG("older", DebuggerFrame_getOlder, 0),
    JS_PSG("script", DebuggerFrame_getScript, 0),
    JS_PSG("this", DebuggerFrame_getThis, 0),
    JS_PSG("type", DebuggerFrame_getType, 0),
    JS_PS_END
};

static JSFunctionSpec DebuggerFrame_methods[] = {
    JS_FN("eval", DebuggerFrame_eval, 1, 0),
    JS_FN("evalWithBindings", DebuggerFrame_evalWithBindings, 1, 0),
    JS_FS_END
};


/*** Debugger.Object *****************************************************************************/

static void
DebuggerObject_trace(JSTracer *trc, JSObject *obj)
{
    if (!trc->context->runtime->gcCurrentCompartment) {
        if (JSObject *referent = (JSObject *) obj->getPrivate())
            MarkObject(trc, *referent, "Debugger.Object referent");
    }
}

Class DebuggerObject_class = {
    "Object", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGOBJECT_COUNT),
    PropertyStub, PropertyStub, PropertyStub, StrictPropertyStub,
    EnumerateStub, ResolveStub, ConvertStub, NULL,
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    DebuggerObject_trace
};

static JSObject *
DebuggerObject_checkThis(JSContext *cx, const CallArgs &args, const char *fnname)
{
    if (!args.thisv().isObject()) {
        ReportObjectRequired(cx);
        return NULL;
    }
    JSObject *thisobj = &args.thisv().toObject();
    if (thisobj->clasp != &DebuggerObject_class) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Debugger.Object", fnname, thisobj->getClass()->name);
        return NULL;
    }

    /*
     * Forbid Debugger.Object.prototype, which is of class DebuggerObject_class
     * but isn't a real working Debugger.Object. The prototype object is
     * distinguished by having no referent.
     */
    if (!thisobj->getPrivate()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Debugger.Object", fnname, "prototype object");
        return NULL;
    }
    return thisobj;
}

#define THIS_DEBUGOBJECT_REFERENT(cx, argc, vp, fnname, args, obj)            \
    CallArgs args = CallArgsFromVp(argc, vp);                                 \
    JSObject *obj = DebuggerObject_checkThis(cx, args, fnname);               \
    if (!obj)                                                                 \
        return false;                                                         \
    obj = (JSObject *) obj->getPrivate();                                     \
    JS_ASSERT(obj)

#define THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, fnname, args, dbg, obj) \
    CallArgs args = CallArgsFromVp(argc, vp);                                 \
    JSObject *obj = DebuggerObject_checkThis(cx, args, fnname);               \
    if (!obj)                                                                 \
        return false;                                                         \
    Debugger *dbg = Debugger::fromChildJSObject(obj);                         \
    obj = (JSObject *) obj->getPrivate();                                     \
    JS_ASSERT(obj)

static JSBool
DebuggerObject_construct(JSContext *cx, uintN argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR, "Debugger.Object");
    return false;
}

static JSBool
DebuggerObject_getProto(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "get proto", args, dbg, refobj);
    Value protov = ObjectOrNullValue(refobj->getProto());
    if (!dbg->wrapDebuggeeValue(cx, &protov))
        return false;
    args.rval() = protov;
    return true;
}

static JSBool
DebuggerObject_getClass(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_REFERENT(cx, argc, vp, "get class", args, refobj);
    const char *s = refobj->clasp->name;
    JSAtom *str = js_Atomize(cx, s, strlen(s));
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static JSBool
DebuggerObject_getCallable(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_REFERENT(cx, argc, vp, "get callable", args, refobj);
    args.rval().setBoolean(refobj->isCallable());
    return true;
}

static JSBool
DebuggerObject_getName(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "get name", args, dbg, obj);
    if (!obj->isFunction()) {
        args.rval().setUndefined();
        return true;
    }

    JSString *name = obj->getFunctionPrivate()->atom;
    if (!name) {
        args.rval().setUndefined();
        return true;
    }

    Value namev = StringValue(name);
    if (!dbg->wrapDebuggeeValue(cx, &namev))
        return false;
    args.rval() = namev;
    return true;
}

static JSBool
DebuggerObject_getParameterNames(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_REFERENT(cx, argc, vp, "get parameterNames", args, obj);
    if (!obj->isFunction()) {
        args.rval().setUndefined();
        return true;
    }

    const JSFunction *fun = obj->getFunctionPrivate();
    JSObject *result = NewDenseAllocatedArray(cx, fun->nargs, NULL);
    if (!result)
        return false;

    if (fun->isInterpreted()) {
        JS_ASSERT(fun->nargs == fun->script()->bindings.countArgs());

        if (fun->nargs > 0) {
            Vector<JSAtom *> names(cx);
            if (!fun->script()->bindings.getLocalNameArray(cx, &names))
                return false;

            for (size_t i = 0; i < fun->nargs; i++) {
                JSAtom *name = names[i];
                result->setDenseArrayElement(i, name ? StringValue(name) : UndefinedValue());
            }
        }
    } else {
        for (size_t i = 0; i < fun->nargs; i++)
            result->setDenseArrayElement(i, UndefinedValue());
    }

    args.rval().setObject(*result);
    return true;
}

static JSBool
DebuggerObject_getScript(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "get script", args, dbg, obj);

    args.rval().setUndefined();

    if (!obj->isFunction())
        return true;

    JSFunction *fun = obj->getFunctionPrivate();
    if (!fun->isInterpreted())
        return true;

    JSObject *scriptObject = dbg->wrapFunctionScript(cx, fun);
    if (!scriptObject)
        return false;

    args.rval().setObject(*scriptObject);
    return true;
}

static JSBool
DebuggerObject_getOwnPropertyDescriptor(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "getOwnPropertyDescriptor", args, dbg, obj);

    jsid id;
    if (!ValueToId(cx, argc >= 1 ? args[0] : UndefinedValue(), &id))
        return false;

    /* Bug: This can cause the debuggee to run! */
    AutoPropertyDescriptorRooter desc(cx);
    {
        AutoCompartment ac(cx, obj);
        if (!ac.enter() || !cx->compartment->wrapId(cx, &id))
            return false;

        ErrorCopier ec(ac, dbg->toJSObject());
        if (!GetOwnPropertyDescriptor(cx, obj, id, &desc))
            return false;
    }

    if (desc.obj) {
        /* Rewrap the debuggee values in desc for the debugger. */
        if (!dbg->wrapDebuggeeValue(cx, &desc.value))
            return false;
        if (desc.attrs & JSPROP_GETTER) {
            Value get = ObjectOrNullValue(CastAsObject(desc.getter));
            if (!dbg->wrapDebuggeeValue(cx, &get))
                return false;
            desc.getter = CastAsPropertyOp(get.toObjectOrNull());
        }
        if (desc.attrs & JSPROP_SETTER) {
            Value set = ObjectOrNullValue(CastAsObject(desc.setter));
            if (!dbg->wrapDebuggeeValue(cx, &set))
                return false;
            desc.setter = CastAsStrictPropertyOp(set.toObjectOrNull());
        }
    }

    return NewPropertyDescriptorObject(cx, &desc, &args.rval());
}

static JSBool
DebuggerObject_getOwnPropertyNames(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "getOwnPropertyNames", args, dbg, obj);

    AutoIdVector keys(cx);
    {
        AutoCompartment ac(cx, obj);
        if (!ac.enter())
            return false;

        ErrorCopier ec(ac, dbg->toJSObject());
        if (!GetPropertyNames(cx, obj, JSITER_OWNONLY | JSITER_HIDDEN, &keys))
            return false;
    }

    AutoValueVector vals(cx);
    if (!vals.resize(keys.length()))
        return false;

    for (size_t i = 0, len = keys.length(); i < len; i++) {
         jsid id = keys[i];
         if (JSID_IS_INT(id)) {
             JSString *str = js_ValueToString(cx, Int32Value(JSID_TO_INT(id)));
             if (!str)
                 return false;
             vals[i].setString(str);
         } else if (JSID_IS_ATOM(id)) {
             vals[i].setString(JSID_TO_STRING(id));
             if (!cx->compartment->wrap(cx, &vals[i]))
                 return false;
         } else {
             vals[i].setObject(*JSID_TO_OBJECT(id));
             if (!dbg->wrapDebuggeeValue(cx, &vals[i]))
                 return false;
         }
    }

    JSObject *aobj = NewDenseCopiedArray(cx, vals.length(), vals.begin());
    if (!aobj)
        return false;
    args.rval().setObject(*aobj);
    return true;
}

static bool
CheckArgCompartment(JSContext *cx, JSObject *obj, const Value &v,
                    const char *methodname, const char *propname)
{
    if (v.isObject() && v.toObject().compartment() != obj->compartment()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_COMPARTMENT_MISMATCH,
                             methodname, propname);
        return false;
    }
    return true;
}

/*
 * Convert Debugger.Objects in desc to debuggee values.
 * Reject non-callable getters and setters.
 */
static bool
UnwrapPropDesc(JSContext *cx, Debugger *dbg, JSObject *obj, PropDesc *desc)
{
    return (!desc->hasValue || (dbg->unwrapDebuggeeValue(cx, &desc->value) &&
                                CheckArgCompartment(cx, obj, desc->value, "defineProperty",
                                                    "value"))) &&
           (!desc->hasGet || (dbg->unwrapDebuggeeValue(cx, &desc->get) &&
                              CheckArgCompartment(cx, obj, desc->get, "defineProperty", "get") &&
                              desc->checkGetter(cx))) &&
           (!desc->hasSet || (dbg->unwrapDebuggeeValue(cx, &desc->set) &&
                              CheckArgCompartment(cx, obj, desc->set, "defineProperty", "set") &&
                              desc->checkSetter(cx)));
}

/*
 * Rewrap *idp and the fields of *desc for the current compartment.  Also:
 * defining a property on a proxy requires the pd field to contain a descriptor
 * object, so reconstitute desc->pd if needed.
 */
static bool
WrapIdAndPropDesc(JSContext *cx, JSObject *obj, jsid *idp, PropDesc *desc)
{
    JSCompartment *comp = cx->compartment;
    return comp->wrapId(cx, idp) &&
           comp->wrap(cx, &desc->value) &&
           comp->wrap(cx, &desc->get) &&
           comp->wrap(cx, &desc->set) &&
           (!obj->isProxy() || desc->makeObject(cx));
}

static JSBool
DebuggerObject_defineProperty(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "defineProperty", args, dbg, obj);
    REQUIRE_ARGC("Debugger.Object.defineProperty", 2);

    jsid id;
    if (!ValueToId(cx, args[0], &id))
        return JS_FALSE;

    const Value &descval = args[1];
    AutoPropDescArrayRooter descs(cx);
    PropDesc *desc = descs.append();
    if (!desc || !desc->initialize(cx, descval, false))
        return false;

    desc->pd.setUndefined();
    if (!UnwrapPropDesc(cx, dbg, obj, desc))
        return false;

    {
        AutoCompartment ac(cx, obj);
        if (!ac.enter() || !WrapIdAndPropDesc(cx, obj, &id, desc))
            return false;

        ErrorCopier ec(ac, dbg->toJSObject());
        bool dummy;
        if (!DefineProperty(cx, obj, id, *desc, true, &dummy))
            return false;
    }

    args.rval().setUndefined();
    return true;
}

static JSBool
DebuggerObject_defineProperties(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "defineProperties", args, dbg, obj);
    REQUIRE_ARGC("Debugger.Object.defineProperties", 1);
    JSObject *props = ToObject(cx, &args[0]);
    if (!props)
        return false;

    AutoIdVector ids(cx);
    AutoPropDescArrayRooter descs(cx);
    if (!ReadPropertyDescriptors(cx, props, false, &ids, &descs))
        return false;
    size_t n = ids.length();

    for (size_t i = 0; i < n; i++) {
        if (!UnwrapPropDesc(cx, dbg, obj, &descs[i]))
            return false;
    }

    {
        AutoCompartment ac(cx, obj);
        if (!ac.enter())
            return false;
        for (size_t i = 0; i < n; i++) {
            if (!WrapIdAndPropDesc(cx, obj, &ids[i], &descs[i]))
                return false;
        }

        ErrorCopier ec(ac, dbg->toJSObject());
        for (size_t i = 0; i < n; i++) {
            bool dummy;
            if (!DefineProperty(cx, obj, ids[i], descs[i], true, &dummy))
                return false;
        }
    }

    args.rval().setUndefined();
    return true;
}

/*
 * This does a non-strict delete, as a matter of API design. The case where the
 * property is non-configurable isn't necessarily exceptional here.
 */
static JSBool
DebuggerObject_deleteProperty(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "deleteProperty", args, dbg, obj);
    Value arg = argc > 0 ? args[0] : UndefinedValue();
    jsid id;
    if (!ValueToId(cx, arg, &id))
        return false;

    AutoCompartment ac(cx, obj);
    if (!ac.enter() || !cx->compartment->wrapId(cx, &id))
        return false;

    ErrorCopier ec(ac, dbg->toJSObject());
    return obj->deleteProperty(cx, id, &args.rval(), false);
}

enum SealHelperOp { Seal, Freeze, PreventExtensions };

static JSBool
DebuggerObject_sealHelper(JSContext *cx, uintN argc, Value *vp, SealHelperOp op, const char *name)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, name, args, dbg, obj);

    AutoCompartment ac(cx, obj);
    if (!ac.enter())
        return false;

    ErrorCopier ec(ac, dbg->toJSObject());
    bool ok;
    if (op == Seal) {
        ok = obj->seal(cx);
    } else if (op == Freeze) {
        ok = obj->freeze(cx);
    } else {
        JS_ASSERT(op == PreventExtensions);
        if (!obj->isExtensible()) {
            args.rval().setUndefined();
            return true;
        }
        AutoIdVector props(cx);
        ok = obj->preventExtensions(cx, &props);
    }
    if (!ok)
        return false;
    args.rval().setUndefined();
    return true;
}

static JSBool
DebuggerObject_seal(JSContext *cx, uintN argc, Value *vp)
{
    return DebuggerObject_sealHelper(cx, argc, vp, Seal, "seal");
}

static JSBool
DebuggerObject_freeze(JSContext *cx, uintN argc, Value *vp)
{
    return DebuggerObject_sealHelper(cx, argc, vp, Freeze, "freeze");
}

static JSBool
DebuggerObject_preventExtensions(JSContext *cx, uintN argc, Value *vp)
{
    return DebuggerObject_sealHelper(cx, argc, vp, PreventExtensions, "preventExtensions");
}

static JSBool
DebuggerObject_isSealedHelper(JSContext *cx, uintN argc, Value *vp, SealHelperOp op,
                              const char *name)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, name, args, dbg, obj);

    AutoCompartment ac(cx, obj);
    if (!ac.enter())
        return false;

    ErrorCopier ec(ac, dbg->toJSObject());
    bool r;
    if (op == Seal) {
        if (!obj->isSealed(cx, &r))
            return false;
    } else if (op == Freeze) {
        if (!obj->isFrozen(cx, &r))
            return false;
    } else {
        r = obj->isExtensible();
    }
    args.rval().setBoolean(r);
    return true;
}

static JSBool
DebuggerObject_isSealed(JSContext *cx, uintN argc, Value *vp)
{
    return DebuggerObject_isSealedHelper(cx, argc, vp, Seal, "isSealed");
}

static JSBool
DebuggerObject_isFrozen(JSContext *cx, uintN argc, Value *vp)
{
    return DebuggerObject_isSealedHelper(cx, argc, vp, Freeze, "isFrozen");
}

static JSBool
DebuggerObject_isExtensible(JSContext *cx, uintN argc, Value *vp)
{
    return DebuggerObject_isSealedHelper(cx, argc, vp, PreventExtensions, "isExtensible");
}

enum ApplyOrCallMode { ApplyMode, CallMode };

static JSBool
ApplyOrCall(JSContext *cx, uintN argc, Value *vp, ApplyOrCallMode mode)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "apply", args, dbg, obj);

    /*
     * Any JS exceptions thrown must be in the debugger compartment, so do
     * sanity checks and fallible conversions before entering the debuggee.
     */
    Value calleev = ObjectValue(*obj);
    if (!obj->isCallable()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Debugger.Object", "apply", obj->getClass()->name);
        return false;
    }

    /*
     * Unwrap Debugger.Objects. This happens in the debugger's compartment since
     * that is where any exceptions must be reported.
     */
    Value thisv = argc > 0 ? args[0] : UndefinedValue();
    if (!dbg->unwrapDebuggeeValue(cx, &thisv))
        return false;
    uintN callArgc = 0;
    Value *callArgv = NULL;
    AutoValueVector argv(cx);
    if (mode == ApplyMode) {
        if (argc >= 2 && !args[1].isNullOrUndefined()) {
            if (!args[1].isObject()) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_APPLY_ARGS,
                                     js_apply_str);
                return false;
            }
            JSObject *argsobj = &args[1].toObject();
            if (!js_GetLengthProperty(cx, argsobj, &callArgc))
                return false;
            callArgc = uintN(JS_MIN(callArgc, StackSpace::ARGS_LENGTH_MAX));
            if (!argv.growBy(callArgc) || !GetElements(cx, argsobj, callArgc, argv.begin()))
                return false;
            callArgv = argv.begin();
        }
    } else {
        callArgc = argc > 0 ? uintN(JS_MIN(argc - 1, StackSpace::ARGS_LENGTH_MAX)) : 0;
        callArgv = args.argv() + 1;
    }
    for (uintN i = 0; i < callArgc; i++) {
        if (!dbg->unwrapDebuggeeValue(cx, &callArgv[i]))
            return false;
    }

    /*
     * Enter the debuggee compartment and rewrap all input value for that compartment.
     * (Rewrapping always takes place in the destination compartment.)
     */
    AutoCompartment ac(cx, obj);
    if (!ac.enter() || !cx->compartment->wrap(cx, &calleev) || !cx->compartment->wrap(cx, &thisv))
        return false;
    for (uintN i = 0; i < callArgc; i++) {
        if (!cx->compartment->wrap(cx, &callArgv[i]))
            return false;
    }

    /*
     * Call the function. Use newCompletionValue to return to the debugger
     * compartment and populate args.rval().
     */
    Value rval;
    bool ok = ExternalInvoke(cx, thisv, calleev, callArgc, callArgv, &rval);
    return dbg->newCompletionValue(ac, ok, rval, &args.rval());
}

static JSBool
DebuggerObject_apply(JSContext *cx, uintN argc, Value *vp)
{
    return ApplyOrCall(cx, argc, vp, ApplyMode);
}

static JSBool
DebuggerObject_call(JSContext *cx, uintN argc, Value *vp)
{
    return ApplyOrCall(cx, argc, vp, CallMode);
}

static JSPropertySpec DebuggerObject_properties[] = {
    JS_PSG("proto", DebuggerObject_getProto, 0),
    JS_PSG("class", DebuggerObject_getClass, 0),
    JS_PSG("callable", DebuggerObject_getCallable, 0),
    JS_PSG("name", DebuggerObject_getName, 0),
    JS_PSG("parameterNames", DebuggerObject_getParameterNames, 0),
    JS_PSG("script", DebuggerObject_getScript, 0),
    JS_PS_END
};

static JSFunctionSpec DebuggerObject_methods[] = {
    JS_FN("getOwnPropertyDescriptor", DebuggerObject_getOwnPropertyDescriptor, 1, 0),
    JS_FN("getOwnPropertyNames", DebuggerObject_getOwnPropertyNames, 0, 0),
    JS_FN("defineProperty", DebuggerObject_defineProperty, 2, 0),
    JS_FN("defineProperties", DebuggerObject_defineProperties, 1, 0),
    JS_FN("deleteProperty", DebuggerObject_deleteProperty, 1, 0),
    JS_FN("seal", DebuggerObject_seal, 0, 0),
    JS_FN("freeze", DebuggerObject_freeze, 0, 0),
    JS_FN("preventExtensions", DebuggerObject_preventExtensions, 0, 0),
    JS_FN("isSealed", DebuggerObject_isSealed, 0, 0),
    JS_FN("isFrozen", DebuggerObject_isFrozen, 0, 0),
    JS_FN("isExtensible", DebuggerObject_isExtensible, 0, 0),
    JS_FN("apply", DebuggerObject_apply, 0, 0),
    JS_FN("call", DebuggerObject_call, 0, 0),
    JS_FS_END
};


/*** Glue ****************************************************************************************/

extern JS_PUBLIC_API(JSBool)
JS_DefineDebuggerObject(JSContext *cx, JSObject *obj)
{
    JSObject *objProto;
    if (!js_GetClassPrototype(cx, obj, JSProto_Object, &objProto))
        return false;

    JSObject *debugCtor;
    JSObject *debugProto = js_InitClass(cx, obj, objProto, &Debugger::jsclass, Debugger::construct,
                                        1, Debugger::properties, Debugger::methods, NULL, NULL,
                                        &debugCtor);
    if (!debugProto || !debugProto->ensureClassReservedSlots(cx))
        return false;

    JSObject *frameProto = js_InitClass(cx, debugCtor, objProto, &DebuggerFrame_class,
                                        DebuggerFrame_construct, 0,
                                        DebuggerFrame_properties, DebuggerFrame_methods,
                                        NULL, NULL);
    if (!frameProto)
        return false;

    JSObject *scriptProto = js_InitClass(cx, debugCtor, objProto, &DebuggerScript_class,
                                         DebuggerScript_construct, 0,
                                         DebuggerScript_properties, DebuggerScript_methods,
                                         NULL, NULL);
    if (!scriptProto)
        return false;

    JSObject *objectProto = js_InitClass(cx, debugCtor, objProto, &DebuggerObject_class,
                                         DebuggerObject_construct, 0,
                                         DebuggerObject_properties, DebuggerObject_methods,
                                         NULL, NULL);
    if (!objectProto)
        return false;

    debugProto->setReservedSlot(Debugger::JSSLOT_DEBUG_FRAME_PROTO, ObjectValue(*frameProto));
    debugProto->setReservedSlot(Debugger::JSSLOT_DEBUG_OBJECT_PROTO, ObjectValue(*objectProto));
    debugProto->setReservedSlot(Debugger::JSSLOT_DEBUG_SCRIPT_PROTO, ObjectValue(*scriptProto));
    return true;
}

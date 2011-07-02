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
 * The Original Code is SpiderMonkey Debug object.
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

#include "jsdbg.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsemit.h"
#include "jsgcmark.h"
#include "jsobj.h"
#include "jswrapper.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsopcodeinlines.h"
#include "methodjit/Retcon.h"
#include "vm/Stack-inl.h"

using namespace js;


// === Forward declarations

extern Class DebugFrame_class;

enum {
    JSSLOT_DEBUGFRAME_OWNER,
    JSSLOT_DEBUGFRAME_ARGUMENTS,
    JSSLOT_DEBUGFRAME_COUNT
};

extern Class DebugArguments_class;

enum {
    JSSLOT_DEBUGARGUMENTS_FRAME,
    JSSLOT_DEBUGARGUMENTS_COUNT
};

extern Class DebugObject_class;

enum {
    JSSLOT_DEBUGOBJECT_OWNER,
    JSSLOT_DEBUGOBJECT_COUNT
};

extern Class DebugScript_class;

enum {
    JSSLOT_DEBUGSCRIPT_OWNER,
    JSSLOT_DEBUGSCRIPT_HOLDER,  // PrivateValue, cross-compartment pointer
    JSSLOT_DEBUGSCRIPT_COUNT
};


// === Utils

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

#define REQUIRE_ARGC(name, n) \
    JS_BEGIN_MACRO \
        if (argc < (n)) \
            return ReportMoreArgsNeeded(cx, name, n); \
    JS_END_MACRO

bool
ReportObjectRequired(JSContext *cx)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
    return false;
}

JSObject *
CheckThisClass(JSContext *cx, Value *vp, Class *clasp, const char *fnname)
{
    if (!vp[1].isObject()) {
        ReportObjectRequired(cx);
        return NULL;
    }
    JSObject *thisobj = &vp[1].toObject();
    if (thisobj->getClass() != clasp) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             clasp->name, fnname, thisobj->getClass()->name);
        return NULL;
    }

    // Forbid e.g. Debug.prototype, which is of the Debug JSClass but isn't
    // really a Debug object. The prototype object is distinguished by
    // having a NULL private value.
    if ((clasp->flags & JSCLASS_HAS_PRIVATE) && !thisobj->getPrivate()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             clasp->name, fnname, "prototype object");
        return NULL;
    }
    return thisobj;
}

#define THISOBJ(cx, vp, classname, fnname, thisobj, private)                 \
    JSObject *thisobj =                                                      \
        CheckThisClass(cx, vp, &js::classname::jsclass, fnname);             \
    if (!thisobj)                                                            \
        return false;                                                        \
    js::classname *private = (classname *) thisobj->getPrivate();


// === Breakpoints

BreakpointSite::BreakpointSite(JSScript *script, jsbytecode *pc)
    : script(script), pc(pc), realOpcode(JSOp(*pc)), scriptObject(NULL), enabledCount(0),
      trapHandler(NULL), trapClosure(UndefinedValue())
{
    JS_ASSERT(realOpcode != JSOP_TRAP);
    JS_INIT_CLIST(&breakpoints);
}

// Precondition: script is live, meaning either it is an eval script that is on
// the stack or a non-eval script that hasn't been GC'd.
static JSObject *
ScriptScope(JSContext *cx, JSScript *script, JSObject *holder)
{
    if (holder)
        return holder;

    // The referent is an eval script. There is no direct reference from script
    // to the scope, so find it on the stack.
    for (AllFramesIter i(cx->stack.space()); ; ++i) {
        JS_ASSERT(!i.done());
        if (i.fp()->maybeScript() == script)
            return &i.fp()->scopeChain();
    }
    JS_NOT_REACHED("ScriptScope: live eval script not on stack");
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
        recompile(cx, false);  // errors ignored
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
        recompile(cx, true);  // ignore failure
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


Breakpoint::Breakpoint(Debug *debugger, BreakpointSite *site, JSObject *handler)
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


// === Debug hook dispatch

enum {
    JSSLOT_DEBUG_FRAME_PROTO,
    JSSLOT_DEBUG_OBJECT_PROTO,
    JSSLOT_DEBUG_SCRIPT_PROTO,
    JSSLOT_DEBUG_COUNT
};

Debug::Debug(JSObject *dbg, JSObject *hooks)
  : object(dbg), hooksObject(hooks), uncaughtExceptionHook(NULL), enabled(true),
    hasDebuggerHandler(false), hasThrowHandler(false),
    objects(dbg->compartment()->rt), heldScripts(dbg->compartment()->rt)
{
    // This always happens within a request on some cx.
    JSRuntime *rt = dbg->compartment()->rt;
    AutoLockGC lock(rt);
    JS_APPEND_LINK(&link, &rt->debuggerList);
    JS_INIT_CLIST(&breakpoints);
}

Debug::~Debug()
{
    JS_ASSERT(debuggees.empty());

    // This always happens in the GC thread, so no locking is required.
    JS_ASSERT(object->compartment()->rt->gcRunning);
    JS_REMOVE_LINK(&link);
}

bool
Debug::init(JSContext *cx)
{
    bool ok = (frames.init() &&
               objects.init() &&
               debuggees.init() &&
               heldScripts.init() &&
               evalScripts.init());
    if (!ok)
        js_ReportOutOfMemory(cx);
    return ok;
}

JS_STATIC_ASSERT(uintN(JSSLOT_DEBUGFRAME_OWNER) == uintN(JSSLOT_DEBUGOBJECT_OWNER));
JS_STATIC_ASSERT(uintN(JSSLOT_DEBUGFRAME_OWNER) == uintN(JSSLOT_DEBUGSCRIPT_OWNER));

Debug *
Debug::fromChildJSObject(JSObject *obj)
{
    JS_ASSERT(obj->clasp == &DebugFrame_class ||
              obj->clasp == &DebugObject_class ||
              obj->clasp == &DebugScript_class);
    JSObject *dbgobj = &obj->getReservedSlot(JSSLOT_DEBUGOBJECT_OWNER).toObject();
    return fromJSObject(dbgobj);
}

bool
Debug::getScriptFrame(JSContext *cx, StackFrame *fp, Value *vp)
{
    JS_ASSERT(fp->isScriptFrame());
    FrameMap::AddPtr p = frames.lookupForAdd(fp);
    if (!p) {
        // Create and populate the Debug.Frame object.
        JSObject *proto = &object->getReservedSlot(JSSLOT_DEBUG_FRAME_PROTO).toObject();
        JSObject *frameobj = NewNonFunction<WithProto::Given>(cx, &DebugFrame_class, proto, NULL);
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

void
Debug::slowPathLeaveStackFrame(JSContext *cx)
{
    StackFrame *fp = cx->fp();
    GlobalObject *global = fp->scopeChain().getGlobal();

    // FIXME This notifies only current debuggers, so it relies on a hack in
    // Debug::removeDebuggeeGlobal to make sure only current debuggers have
    // Frame objects with .live === true.
    if (GlobalObject::DebugVector *debuggers = global->getDebuggers()) {
        for (Debug **p = debuggers->begin(); p != debuggers->end(); p++) {
            Debug *dbg = *p;
            if (FrameMap::Ptr p = dbg->frames.lookup(fp)) {
                JSObject *frameobj = p->value;
                frameobj->setPrivate(NULL);
                dbg->frames.remove(p);
            }
        }
    }

    // If this is an eval frame, then from the debugger's perspective the
    // script is about to be destroyed. Remove any breakpoints in it.
    if (fp->isEvalFrame()) {
        JSScript *script = fp->script();
        script->compartment->clearBreakpointsIn(cx, NULL, script, NULL);
    }
}

bool
Debug::wrapDebuggeeValue(JSContext *cx, Value *vp)
{
    assertSameCompartment(cx, object);

    if (vp->isObject()) {
        JSObject *obj = &vp->toObject();

        ObjectWeakMap::AddPtr p = objects.lookupForAdd(obj);
        if (p) {
            vp->setObject(*p->value);
        } else {
            // Create a new Debug.Object for obj.
            JSObject *proto = &object->getReservedSlot(JSSLOT_DEBUG_OBJECT_PROTO).toObject();
            JSObject *dobj = NewNonFunction<WithProto::Given>(cx, &DebugObject_class, proto, NULL);
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
Debug::unwrapDebuggeeValue(JSContext *cx, Value *vp)
{
    assertSameCompartment(cx, object, *vp);
    if (vp->isObject()) {
        JSObject *dobj = &vp->toObject();
        if (dobj->clasp != &DebugObject_class) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_EXPECTED_TYPE,
                                 "Debug", "Debug.Object", dobj->clasp->name);
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
Debug::handleUncaughtException(AutoCompartment &ac, Value *vp, bool callHook)
{
    JSContext *cx = ac.context;
    if (cx->isExceptionPending()) {
        if (callHook && uncaughtExceptionHook) {
            Value fval = ObjectValue(*uncaughtExceptionHook);
            Value exc = cx->getPendingException();
            Value rv;
            cx->clearPendingException();
            if (ExternalInvoke(cx, ObjectValue(*object), fval, 1, &exc, &rv))
                return parseResumptionValue(ac, true, rv, vp, false);
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
Debug::newCompletionValue(AutoCompartment &ac, bool ok, Value val, Value *vp)
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
Debug::parseResumptionValue(AutoCompartment &ac, bool ok, const Value &rv, Value *vp,
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

    // Check that rv is {return: val} or {throw: val}.
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

bool
Debug::observesDebuggerStatement() const
{
    return enabled && hasDebuggerHandler;
}

JSTrapStatus
Debug::handleDebuggerStatement(JSContext *cx, Value *vp)
{
    // Grab cx->fp() before pushing a dummy frame.
    StackFrame *fp = cx->fp();

    JS_ASSERT(hasDebuggerHandler);
    AutoCompartment ac(cx, hooksObject);
    if (!ac.enter())
        return JSTRAP_ERROR;

    Value argv[1];
    if (!getScriptFrame(cx, fp, argv))
        return handleUncaughtException(ac, vp, false);

    Value rv;
    bool ok = CallMethodIfPresent(cx, hooksObject, "debuggerHandler", 1, argv, &rv);
    return parseResumptionValue(ac, ok, rv, vp);
}

bool
Debug::observesThrow() const
{
    return enabled && hasThrowHandler;
}

JSTrapStatus
Debug::handleThrow(JSContext *cx, Value *vp)
{
    StackFrame *fp = cx->fp();
    Value exc = cx->getPendingException();

    cx->clearPendingException();
    JS_ASSERT(hasThrowHandler);
    AutoCompartment ac(cx, hooksObject);
    if (!ac.enter())
        return JSTRAP_ERROR;

    Value argv[2];
    argv[1] = exc;
    if (!getScriptFrame(cx, fp, &argv[0]) || !wrapDebuggeeValue(cx, &argv[1]))
        return handleUncaughtException(ac, vp, false);

    Value rv;
    bool ok = CallMethodIfPresent(cx, hooksObject, "throw", 2, argv, &rv);
    JSTrapStatus st = parseResumptionValue(ac, ok, rv, vp);
    if (st == JSTRAP_CONTINUE)
        cx->setPendingException(exc);
    return st;
}

JSTrapStatus
Debug::dispatchHook(JSContext *cx, js::Value *vp, DebugObservesMethod observesEvent,
                    DebugHandleMethod handleEvent)
{
    // Determine which debuggers will receive this event, and in what order.
    // Make a copy of the list, since the original is mutable and we will be
    // calling into arbitrary JS.
    // Note: In the general case, 'triggered' contains references to objects in
    // different compartments--every compartment *except* this one.
    AutoValueVector triggered(cx);
    GlobalObject *global = cx->fp()->scopeChain().getGlobal();
    if (GlobalObject::DebugVector *debuggers = global->getDebuggers()) {
        for (Debug **p = debuggers->begin(); p != debuggers->end(); p++) {
            Debug *dbg = *p;
            if ((dbg->*observesEvent)()) {
                if (!triggered.append(ObjectValue(*dbg->toJSObject())))
                    return JSTRAP_ERROR;
            }
        }
    }

    // Deliver the event to each debugger, checking again to make sure it
    // should still be delivered.
    for (Value *p = triggered.begin(); p != triggered.end(); p++) {
        Debug *dbg = Debug::fromJSObject(&p->toObject());
        if (dbg->debuggees.has(global) && (dbg->*observesEvent)()) {
            JSTrapStatus st = (dbg->*handleEvent)(cx, vp);
            if (st != JSTRAP_CONTINUE)
                return st;
        }
    }
    return JSTRAP_CONTINUE;
}

JSTrapStatus
Debug::onTrap(JSContext *cx, Value *vp)
{
    StackFrame *fp = cx->fp();
    GlobalObject *scriptGlobal = fp->scopeChain().getGlobal();
    jsbytecode *pc = cx->regs().pc;
    BreakpointSite *site = cx->compartment->getBreakpointSite(pc);
    JSOp op = site->realOpcode;

    // Build list of breakpoint handlers.
    Vector<Breakpoint *> triggered(cx);
    for (Breakpoint *bp = site->firstBreakpoint(); bp; bp = bp->nextInSite()) {
        if (!triggered.append(bp))
            return JSTRAP_ERROR;
    }

    Value frame = UndefinedValue();
    for (Breakpoint **p = triggered.begin(); p != triggered.end(); p++) {
        Breakpoint *bp = *p;

        // Handlers can clear breakpoints. Check that bp still exists.
        if (!site || !site->hasBreakpoint(bp))
            continue;

        Debug *dbg = bp->debugger;
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

            // Calling JS code invalidates site. Reload it.
            site = cx->compartment->getBreakpointSite(pc);
        }
    }

    if (site && site->trapHandler) {
        JSTrapStatus st = site->trapHandler(cx, fp->script(), pc, Jsvalify(vp),
                                            Jsvalify(site->trapClosure));
        if (st != JSTRAP_CONTINUE)
            return st;
    }

    // By convention, return the true op to the interpreter in vp.
    vp->setInt32(op);
    return JSTRAP_CONTINUE;
}


// === Debug JSObjects

void
Debug::markKeysInCompartment(JSTracer *tracer, ObjectWeakMap &map)
{
    JSCompartment *comp = tracer->context->runtime->gcCurrentCompartment;
    JS_ASSERT(comp);

    typedef HashMap<JSObject *, JSObject *, DefaultHasher<JSObject *>, RuntimeAllocPolicy> Map;
    Map &storage = map;
    for (Map::Range r = storage.all(); !r.empty(); r.popFront()) {
        JSObject *key = r.front().key;
        if (key->compartment() == comp && IsAboutToBeFinalized(tracer->context, key))
            js::gc::MarkObject(tracer, *key, "cross-compartment WeakMap key");
    }
}

// Ordinarily, WeakMap keys and values are marked because at some point it was
// discovered that the WeakMap was live; that is, some object containing the
// WeakMap was marked during mark phase.
//
// However, during single-compartment GC, we have to do something about
// cross-compartment WeakMaps in other compartments. Since those compartments
// aren't being GC'd, the WeakMaps definitely will not be found during mark
// phase. If their keys and values might need to be marked, we have to do it
// manually.
//
// Each Debug object keeps two cross-compartment WeakMaps: objects and
// heldScripts.  Both have the nice property that all their values are in the
// same compartment as the Debug object, so we only need to mark the keys. We
// must simply mark all keys that are in the compartment being GC'd.
//
// We must scan all Debug objects regardless of whether they *currently* have
// any debuggees in the compartment being GC'd, because the WeakMap entries
// persist even when debuggees are removed.
//
// This happens during the initial mark phase, not iterative marking, because
// all the edges being reported here are strong references.
//
void 
Debug::markCrossCompartmentDebugObjectReferents(JSTracer *tracer)
{
    JSRuntime *rt = tracer->context->runtime;
    JSCompartment *comp = rt->gcCurrentCompartment;

    // Mark all objects in comp that are referents of Debug.Objects in other
    // compartments.
    for (JSCList *p = &rt->debuggerList; (p = JS_NEXT_LINK(p)) != &rt->debuggerList;) {
        Debug *dbg = Debug::fromLinks(p);
        if (dbg->object->compartment() != comp) {
            markKeysInCompartment(tracer, dbg->objects);
            markKeysInCompartment(tracer, dbg->heldScripts);
        }
    }         
}

// This method has two tasks:
//   1. Mark Debug objects that are unreachable except for debugger hooks that
//      may yet be called.
//   2. Mark breakpoint handlers.
//
// This happens during the incremental long tail of the GC mark phase. This
// method returns true if it has to mark anything; GC calls it repeatedly until
// it returns false.
//
bool
Debug::mark(GCMarker *trc, JSGCInvocationKind gckind)
{
    bool markedAny = false;

    // We must find all Debug objects in danger of GC. This code is a little
    // convoluted since the easiest way to find them is via their debuggees.
    JSRuntime *rt = trc->context->runtime;
    JSCompartment *comp = rt->gcCurrentCompartment;
    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++) {
        JSCompartment *dc = *c;

        // If dc is being collected, mark breakpoint handlers in it.
        if (!comp || dc == comp)
            markedAny = markedAny | dc->markBreakpointsIteratively(trc);

        // If this is a single-compartment GC, no compartment can debug itself, so skip
        // |comp|. If it's a global GC, then search every live compartment.
        if (comp ? dc != comp : !dc->isAboutToBeCollected(gckind)) {
            const GlobalObjectSet &debuggees = dc->getDebuggees();
            for (GlobalObjectSet::Range r = debuggees.all(); !r.empty(); r.popFront()) {
                GlobalObject *global = r.front();

                // Every debuggee has at least one debugger, so in this case
                // getDebuggers can't return NULL.
                const GlobalObject::DebugVector *debuggers = global->getDebuggers();
                JS_ASSERT(debuggers);
                for (Debug **p = debuggers->begin(); p != debuggers->end(); p++) {
                    Debug *dbg = *p;
                    JSObject *obj = dbg->toJSObject();

                    // dbg is a Debug with at least one debuggee. Check three things:
                    //   - dbg is actually in a compartment being GC'd
                    //   - it isn't already marked
                    //   - it actually has hooks that might be called
                    if ((!comp || obj->compartment() == comp) && !obj->isMarked()) {
                        if (dbg->hasAnyLiveHooks()) {
                            // obj could be reachable only via its live, enabled
                            // debugger hooks, which may yet be called.
                            MarkObject(trc, *obj, "enabled Debug");
                            markedAny = true;
                        }
                    }
                }
            }
        }
    }
    return markedAny;
}

void
Debug::traceObject(JSTracer *trc, JSObject *obj)
{
    if (Debug *dbg = Debug::fromJSObject(obj))
        dbg->trace(trc);
}

void
Debug::trace(JSTracer *trc)
{
    MarkObject(trc, *hooksObject, "hooks");
    if (uncaughtExceptionHook)
        MarkObject(trc, *uncaughtExceptionHook, "hooks");

    // Mark Debug.Frame objects that are reachable from JS if we look them up
    // again (because the corresponding StackFrame is still on the stack).
    for (FrameMap::Range r = frames.all(); !r.empty(); r.popFront()) {
        JSObject *frameobj = r.front().value;
        JS_ASSERT(frameobj->getPrivate());
        MarkObject(trc, *frameobj, "live Debug.Frame");
    }

    // Trace the referent -> Debug.Object weak map.
    objects.trace(trc);

    // Trace the weak map from JSFunctions and "Script" JSObjects to
    // Debug.Script objects.
    heldScripts.trace(trc);

    // Trace the map for eval scripts, which are explicitly freed.
    for (ScriptMap::Range r = evalScripts.all(); !r.empty(); r.popFront()) {
        JSObject *scriptobj = r.front().value;

        // evalScripts should only refer to Debug.Script objects for
        // scripts that haven't been freed yet.
        JS_ASSERT(scriptobj->getPrivate());
        MarkObject(trc, *scriptobj, "live eval Debug.Script");
    }
}

void
Debug::sweepAll(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    for (JSCList *p = &rt->debuggerList; (p = JS_NEXT_LINK(p)) != &rt->debuggerList;) {
        Debug *dbg = Debug::fromLinks(p);

        if (!dbg->object->isMarked()) {
            // If this Debug is being GC'd, detach it from its debuggees. In the case of
            // runtime-wide GC, the debuggee might be GC'd too. Since detaching requires
            // access to both objects, this must be done before finalize time. However, in
            // a per-compartment GC, it is impossible for both objects to be GC'd (since
            // they are in different compartments), so in that case we just wait for
            // Debug::finalize.
            for (GlobalObjectSet::Enum e(dbg->debuggees); !e.empty(); e.popFront())
                dbg->removeDebuggeeGlobal(cx, e.front(), NULL, &e);
        }

    }

    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++) {
        // For each debuggee being GC'd, detach it from all its debuggers.
        GlobalObjectSet &debuggees = (*c)->getDebuggees();
        for (GlobalObjectSet::Enum e(debuggees); !e.empty(); e.popFront()) {
            GlobalObject *global = e.front();
            if (!global->isMarked())
                detachAllDebuggersFromGlobal(cx, global, &e);
        }
    }
}

void
Debug::detachAllDebuggersFromGlobal(JSContext *cx, GlobalObject *global, GlobalObjectSet::Enum *compartmentEnum)
{
    const GlobalObject::DebugVector *debuggers = global->getDebuggers();
    JS_ASSERT(!debuggers->empty());
    while (!debuggers->empty())
        debuggers->back()->removeDebuggeeGlobal(cx, global, compartmentEnum, NULL);
}

void
Debug::finalize(JSContext *cx, JSObject *obj)
{
    Debug *dbg = (Debug *) obj->getPrivate();
    if (!dbg)
        return;
    if (!dbg->debuggees.empty()) {
        // This happens only during per-compartment GC. See comment in
        // Debug::sweepAll.
        JS_ASSERT(cx->runtime->gcCurrentCompartment == dbg->object->compartment());
        for (GlobalObjectSet::Enum e(dbg->debuggees); !e.empty(); e.popFront())
            dbg->removeDebuggeeGlobal(cx, e.front(), NULL, &e);
    }
    cx->delete_(dbg);
}

Class Debug::jsclass = {
    "Debug", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUG_COUNT),
    PropertyStub, PropertyStub, PropertyStub, StrictPropertyStub,
    EnumerateStub, ResolveStub, ConvertStub, Debug::finalize,
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    Debug::traceObject
};

JSBool
Debug::getHooks(JSContext *cx, uintN argc, Value *vp)
{
    THISOBJ(cx, vp, Debug, "get hooks", thisobj, dbg);
    vp->setObject(*dbg->hooksObject);
    return true;
}

JSBool
Debug::setHooks(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug.set hooks", 1);
    THISOBJ(cx, vp, Debug, "set hooks", thisobj, dbg);
    if (!vp[2].isObject())
        return ReportObjectRequired(cx);
    JSObject *hooksobj = &vp[2].toObject();

    bool hasDebuggerHandler, hasThrow;
    JSBool found;
    if (!JS_HasProperty(cx, hooksobj, "debuggerHandler", &found))
        return false;
    hasDebuggerHandler = !!found;
    if (!JS_HasProperty(cx, hooksobj, "throw", &found))
        return false;
    hasThrow = !!found;

    dbg->hooksObject = hooksobj;
    dbg->hasDebuggerHandler = hasDebuggerHandler;
    dbg->hasThrowHandler = hasThrow;
    vp->setUndefined();
    return true;
}

JSBool
Debug::getEnabled(JSContext *cx, uintN argc, Value *vp)
{
    THISOBJ(cx, vp, Debug, "get enabled", thisobj, dbg);
    vp->setBoolean(dbg->enabled);
    return true;
}

JSBool
Debug::setEnabled(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug.set enabled", 1);
    THISOBJ(cx, vp, Debug, "set enabled", thisobj, dbg);
    bool enabled = js_ValueToBoolean(vp[2]);

    if (enabled != dbg->enabled) {
        for (Breakpoint *bp = dbg->firstBreakpoint(); bp; bp = bp->nextInDebugger()) {
            if (enabled) {
                if (!bp->site->inc(cx)) {
                    // Roll back the changes on error to keep the
                    // BreakpointSite::enabledCount counters correct.
                    for (Breakpoint *bp2 = dbg->firstBreakpoint(); bp2 != bp; bp2 = bp2->nextInDebugger())
                        bp->site->dec(cx);
                    return false;
                }
            } else {
                bp->site->dec(cx);
            }
        }
    }

    dbg->enabled = enabled;
    vp->setUndefined();
    return true;
}

JSBool
Debug::getUncaughtExceptionHook(JSContext *cx, uintN argc, Value *vp)
{
    THISOBJ(cx, vp, Debug, "get uncaughtExceptionHook", thisobj, dbg);
    vp->setObjectOrNull(dbg->uncaughtExceptionHook);
    return true;
}

JSBool
Debug::setUncaughtExceptionHook(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug.set uncaughtExceptionHook", 1);
    THISOBJ(cx, vp, Debug, "set uncaughtExceptionHook", thisobj, dbg);
    if (!vp[2].isNull() && (!vp[2].isObject() || !vp[2].toObject().isCallable())) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_ASSIGN_FUNCTION_OR_NULL,
                             "uncaughtExceptionHook");
        return false;
    }

    dbg->uncaughtExceptionHook = vp[2].toObjectOrNull();
    vp->setUndefined();
    return true;
}

JSObject *
Debug::unwrapDebuggeeArgument(JSContext *cx, Value *vp)
{
    // The argument to {add,remove,has}Debuggee may be
    //   - a Debug.Object belonging to this Debug: return its referent
    //   - a cross-compartment wrapper: return the wrapped object
    //   - any other non-Debug.Object object: return it
    // If it is a primitive, or a Debug.Object that belongs to some other
    // Debug, throw a TypeError.
    Value v = JS_ARGV(cx, vp)[0];
    JSObject *obj = NonNullObject(cx, v);
    if (obj) {
        if (obj->clasp == &DebugObject_class) {
            if (!unwrapDebuggeeValue(cx, &v))
                return NULL;
            return &v.toObject();
        }
        if (obj->isCrossCompartmentWrapper())
            return &obj->getProxyPrivate().toObject();
    }
    return obj;
}

JSBool
Debug::addDebuggee(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug.addDebuggee", 1);
    THISOBJ(cx, vp, Debug, "addDebuggee", thisobj, dbg);
    JSObject *referent = dbg->unwrapDebuggeeArgument(cx, vp);
    if (!referent)
        return false;
    GlobalObject *global = referent->getGlobal();
    if (!dbg->debuggees.has(global) && !dbg->addDebuggeeGlobal(cx, global))
        return false;

    Value v = ObjectValue(*referent);
    if (!dbg->wrapDebuggeeValue(cx, &v))
        return false;
    *vp = v;
    return true;
}

JSBool
Debug::removeDebuggee(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug.removeDebuggee", 1);
    THISOBJ(cx, vp, Debug, "removeDebuggee", thisobj, dbg);
    JSObject *referent = dbg->unwrapDebuggeeArgument(cx, vp);
    if (!referent)
        return false;
    GlobalObject *global = referent->getGlobal();
    if (dbg->debuggees.has(global))
        dbg->removeDebuggeeGlobal(cx, global, NULL, NULL);
    vp->setUndefined();
    return true;
}

JSBool
Debug::hasDebuggee(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug.hasDebuggee", 1);
    THISOBJ(cx, vp, Debug, "hasDebuggee", thisobj, dbg);
    JSObject *referent = dbg->unwrapDebuggeeArgument(cx, vp);
    if (!referent)
        return false;
    vp->setBoolean(!!dbg->debuggees.lookup(referent->getGlobal()));
    return true;
}

JSBool
Debug::getDebuggees(JSContext *cx, uintN argc, Value *vp)
{
    THISOBJ(cx, vp, Debug, "getDebuggees", thisobj, dbg);
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
    vp->setObject(*arrobj);
    return true;
}

JSBool
Debug::getYoungestFrame(JSContext *cx, uintN argc, Value *vp)
{
    THISOBJ(cx, vp, Debug, "getYoungestFrame", thisobj, dbg);

    // cx->fp() would return the topmost frame in the current context.
    // Since there may be multiple contexts, use AllFramesIter instead.
    for (AllFramesIter i(cx->stack.space()); !i.done(); ++i) {
        if (dbg->observesFrame(i.fp()))
            return dbg->getScriptFrame(cx, i.fp(), vp);
    }
    vp->setNull();
    return true;
}

JSBool
Debug::clearAllBreakpoints(JSContext *cx, uintN argc, Value *vp)
{
    THISOBJ(cx, vp, Debug, "clearAllBreakpoints", thisobj, dbg);
    for (GlobalObjectSet::Range r = dbg->debuggees.all(); !r.empty(); r.popFront())
        r.front()->compartment()->clearBreakpointsIn(cx, dbg, NULL, NULL);
    return true;
}

JSBool
Debug::construct(JSContext *cx, uintN argc, Value *vp)
{
    // Check that the arguments, if any, are cross-compartment wrappers.
    Value *argv = vp + 2, *argvEnd = argv + argc;
    for (Value *p = argv; p != argvEnd; p++) {
        const Value &arg = *p;
        if (!arg.isObject())
            return ReportObjectRequired(cx);
        JSObject *argobj = &arg.toObject();
        if (!argobj->isCrossCompartmentWrapper()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CCW_REQUIRED, "Debug");
            return false;
        }
    }

    // Get Debug.prototype.
    Value v;
    jsid prototypeId = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
    if (!vp[0].toObject().getProperty(cx, prototypeId, &v))
        return false;
    JSObject *proto = &v.toObject();
    JS_ASSERT(proto->getClass() == &Debug::jsclass);

    // Make the new Debug object. Each one has a reference to
    // Debug.{Frame,Object,Script}.prototype in reserved slots.
    JSObject *obj = NewNonFunction<WithProto::Given>(cx, &Debug::jsclass, proto, NULL);
    if (!obj || !obj->ensureClassReservedSlots(cx))
        return false;
    for (uintN slot = JSSLOT_DEBUG_FRAME_PROTO; slot < JSSLOT_DEBUG_COUNT; slot++)
        obj->setReservedSlot(slot, proto->getReservedSlot(slot));

    JSObject *hooks = NewBuiltinClassInstance(cx, &js_ObjectClass);
    if (!hooks)
        return false;

    Debug *dbg = cx->new_<Debug>(obj, hooks);
    if (!dbg)
        return false;
    obj->setPrivate(dbg);
    if (!dbg->init(cx)) {
        cx->delete_(dbg);
        return false;
    }

    // Add the initial debuggees, if any.
    for (Value *p = argv; p != argvEnd; p++) {
        GlobalObject *debuggee = p->toObject().getProxyPrivate().toObject().getGlobal();
        if (!dbg->addDebuggeeGlobal(cx, debuggee))
            return false;
    }

    vp->setObject(*obj);
    return true;
}

bool
Debug::addDebuggeeGlobal(JSContext *cx, GlobalObject *obj)
{
    JSCompartment *debuggeeCompartment = obj->compartment();

    // Check for cycles. If obj's compartment is reachable from this Debug
    // object's compartment by following debuggee-to-debugger links, then
    // adding obj would create a cycle. (Typically nobody is debugging the
    // debugger, in which case we zip through this code without looping.)
    Vector<JSCompartment *> visited(cx);
    if (!visited.append(object->compartment()))
        return false;
    for (size_t i = 0; i < visited.length(); i++) {
        JSCompartment *c = visited[i];
        if (c == debuggeeCompartment) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_LOOP);
            return false;
        }

        // Find all compartments containing debuggers debugging global objects
        // in c. Add those compartments to visited.
        for (GlobalObjectSet::Range r = c->getDebuggees().all(); !r.empty(); r.popFront()) {
            GlobalObject::DebugVector *v = r.front()->getDebuggers();
            for (Debug **p = v->begin(); p != v->end(); p++) {
                JSCompartment *next = (*p)->object->compartment();
                if (visited.find(next) == visited.end() && !visited.append(next))
                    return false;
            }
        }
    }

    // Refuse to enable debug mode for a compartment that has running scripts.
    if (!debuggeeCompartment->debugMode() && debuggeeCompartment->hasScriptsOnStack(cx)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_NOT_IDLE);
        return false;
    }

    // Each debugger-debuggee relation must be stored in up to three places.
    // JSCompartment::addDebuggee enables debug mode if needed.
    AutoCompartment ac(cx, obj);
    if (!ac.enter())
        return false;
    GlobalObject::DebugVector *v = obj->getOrCreateDebuggers(cx);
    if (!v || !v->append(this)) {
        js_ReportOutOfMemory(cx);
        goto fail1;
    }
    if (!debuggees.put(obj)) {
        js_ReportOutOfMemory(cx);
        goto fail2;
    }
    if (obj->getDebuggers()->length() == 1 && !debuggeeCompartment->addDebuggee(cx, obj))
        goto fail3;
    return true;

    // Maintain consistency on error.
fail3:
    debuggees.remove(obj);
fail2:
    JS_ASSERT(v->back() == this);
    v->popBack();
fail1:
    return false;
}

void
Debug::removeDebuggeeGlobal(JSContext *cx, GlobalObject *global,
                            GlobalObjectSet::Enum *compartmentEnum,
                            GlobalObjectSet::Enum *debugEnum)
{
    // Each debuggee is in two HashSets: one for its compartment and one for
    // its debugger (this). The caller might be enumerating either set; if so,
    // use HashSet::Enum::removeFront rather than HashSet::remove below, to
    // avoid invalidating the live enumerator.
    JS_ASSERT(global->compartment()->getDebuggees().has(global));
    JS_ASSERT_IF(compartmentEnum, compartmentEnum->front() == global);
    JS_ASSERT(debuggees.has(global));
    JS_ASSERT_IF(debugEnum, debugEnum->front() == global);

    // FIXME Debug::slowPathLeaveStackFrame needs to kill all Debug.Frame
    // objects referring to a particular js::StackFrame. This is hard if Debug
    // objects that are no longer debugging the relevant global might have live
    // Frame objects. So we take the easy way out and kill them here. This is a
    // bug, since it's observable and contrary to the spec. One possible fix
    // would be to put such objects into a compartment-wide bag which
    // slowPathLeaveStackFrame would have to examine.
    for (FrameMap::Enum e(frames); !e.empty(); e.popFront()) {
        js::StackFrame *fp = e.front().key;
        if (fp->scopeChain().getGlobal() == global) {
            e.front().value->setPrivate(NULL);
            e.removeFront();
        }
    }

    GlobalObject::DebugVector *v = global->getDebuggers();
    Debug **p;
    for (p = v->begin(); p != v->end(); p++) {
        if (*p == this)
            break;
    }
    JS_ASSERT(p != v->end());

    // The relation must be removed from up to three places: *v and debuggees
    // for sure, and possibly the compartment's debuggee set.
    v->erase(p);
    if (v->empty())
        global->compartment()->removeDebuggee(cx, global, compartmentEnum);
    if (debugEnum)
        debugEnum->removeFront();
    else
        debuggees.remove(global);
}

JSPropertySpec Debug::properties[] = {
    JS_PSGS("hooks", Debug::getHooks, Debug::setHooks, 0),
    JS_PSGS("enabled", Debug::getEnabled, Debug::setEnabled, 0),
    JS_PSGS("uncaughtExceptionHook", Debug::getUncaughtExceptionHook,
            Debug::setUncaughtExceptionHook, 0),
    JS_PS_END
};

JSFunctionSpec Debug::methods[] = {
    JS_FN("addDebuggee", Debug::addDebuggee, 1, 0),
    JS_FN("removeDebuggee", Debug::removeDebuggee, 1, 0),
    JS_FN("hasDebuggee", Debug::hasDebuggee, 1, 0),
    JS_FN("getDebuggees", Debug::getDebuggees, 0, 0),
    JS_FN("getYoungestFrame", Debug::getYoungestFrame, 0, 0),
    JS_FN("clearAllBreakpoints", Debug::clearAllBreakpoints, 1, 0),
    JS_FS_END
};


// === Debug.Script

// JSScripts' lifetimes fall into to two categories:
//
// - "Held scripts": JSScripts belonging to JSFunctions and JSScripts created
//   using JSAPI have lifetimes determined by the garbage collector. A JSScript
//   itself has no mark bit of its own. Instead, its holding object manages the
//   JSScript as part of its own structure: the holder has a mark bit; when the
//   holder is marked it calls js_TraceScript on its JSScript; and when the
//   holder is freed it explicitly frees its JSScript.
//
//   Debug.Script instances for held scripts are strong references to the
//   holder (and thus to the script). Debug::heldScripts weakly maps debuggee
//   holding objects to the Debug.Script objects for their JSScripts. We
//   needn't act on a destroyScript event for a held script: if we get such an
//   event we know its Debug.Script is dead anyway, and its entry in
//   Debug::heldScripts will be cleaned up by the standard weak table code.
//
// - "Eval scripts": JSScripts generated temporarily for a call to 'eval' or a
//   related function live until the call completes, at which point the script
//   is destroyed.
//
//   A Debug.Script instance for an eval script has no influence on the
//   JSScript's lifetime. Debug::evalScripts maps live JSScripts to to their
//   Debug.Script objects.  When a destroyScript event tells us that an eval
//   script is dead, we remove its table entry, and clear its Debug.Script
//   object's script pointer, thus marking it dead.
//
// A Debug.Script's private pointer points directly to the JSScript, or is NULL
// if the Debug.Script is dead. The JSSLOT_DEBUGSCRIPT_HOLDER slot refers to
// the holding object, or is null for eval-like JSScripts. The private pointer
// is not traced; the holding object reference, if present, is traced via
// DebugScript_trace.
//
// (We consider a script saved in and retrieved from the eval cache to have
// been destroyed, and then --- mirabile dictu --- re-created at the same
// address. The newScriptHook and destroyScriptHook hooks cooperate with this
// view.)

static inline JSScript *
GetScriptReferent(JSObject *obj)
{
    JS_ASSERT(obj->getClass() == &DebugScript_class);
    return (JSScript *) obj->getPrivate();
}

static inline void
ClearScriptReferent(JSObject *obj)
{
    JS_ASSERT(obj->getClass() == &DebugScript_class);
    obj->setPrivate(NULL);
}

static inline JSObject *
GetScriptHolder(JSObject *obj)
{
    JS_ASSERT(obj->getClass() == &DebugScript_class);
    Value v = obj->getReservedSlot(JSSLOT_DEBUGSCRIPT_HOLDER);
    return (JSObject *) v.toPrivate();
}

static void
DebugScript_trace(JSTracer *trc, JSObject *obj)
{
    if (!trc->context->runtime->gcCurrentCompartment) {
        Value v = obj->getReservedSlot(JSSLOT_DEBUGSCRIPT_HOLDER);
        if (!v.isUndefined()) {
            if (JSObject *obj = (JSObject *) v.toPrivate())
                MarkObject(trc, *obj, "Debug.Script referent holder");
        }
    }
}

Class DebugScript_class = {
    "Script", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGSCRIPT_COUNT),
    PropertyStub, PropertyStub, PropertyStub, StrictPropertyStub,
    EnumerateStub, ResolveStub, ConvertStub, NULL,
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    DebugScript_trace
};

JSObject *
Debug::newDebugScript(JSContext *cx, JSScript *script, JSObject *holder)
{
    JSObject *proto = &object->getReservedSlot(JSSLOT_DEBUG_SCRIPT_PROTO).toObject();
    JS_ASSERT(proto);
    JSObject *scriptobj = NewNonFunction<WithProto::Given>(cx, &DebugScript_class, proto, NULL);
    if (!scriptobj || !scriptobj->ensureClassReservedSlots(cx))
        return false;
    scriptobj->setPrivate(script);
    scriptobj->setReservedSlot(JSSLOT_DEBUGSCRIPT_OWNER, ObjectValue(*object));
    scriptobj->setReservedSlot(JSSLOT_DEBUGSCRIPT_HOLDER, PrivateValue(holder));

    return scriptobj;
}

JSObject *
Debug::wrapHeldScript(JSContext *cx, JSScript *script, JSObject *obj)
{
    assertSameCompartment(cx, object);

    ScriptWeakMap::AddPtr p = heldScripts.lookupForAdd(obj);
    if (!p) {
        JSObject *scriptobj = newDebugScript(cx, script, obj);
        // The allocation may have caused a GC, which can remove table entries.
        if (!scriptobj || !heldScripts.relookupOrAdd(p, obj, scriptobj))
            return NULL;
    }

    JS_ASSERT(GetScriptReferent(p->value) == script);
    return p->value;
}

JSObject *
Debug::wrapFunctionScript(JSContext *cx, JSFunction *fun)
{
    return wrapHeldScript(cx, fun->script(), fun);
}

JSObject *
Debug::wrapJSAPIScript(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isScript());
    return wrapHeldScript(cx, obj->getScript(), obj);
}

JSObject *
Debug::wrapEvalScript(JSContext *cx, JSScript *script)
{
    JS_ASSERT(cx->compartment != script->compartment);
    ScriptMap::AddPtr p = evalScripts.lookupForAdd(script);
    if (!p) {
        JSObject *scriptobj = newDebugScript(cx, script, NULL);
        // The allocation may have caused a GC, which can remove table entries.
        if (!scriptobj || !evalScripts.relookupOrAdd(p, script, scriptobj))
            return NULL;
    }

    JS_ASSERT(GetScriptReferent(p->value) == script);
    return p->value;
}

void
Debug::slowPathOnDestroyScript(JSScript *script)
{
    // Find all debuggers that might have Debug.Script referring to this script.
    js::GlobalObjectSet *debuggees = &script->compartment->getDebuggees();
    for (GlobalObjectSet::Range r = debuggees->all(); !r.empty(); r.popFront()) {
        GlobalObject::DebugVector *debuggers = r.front()->getDebuggers();
        for (Debug **p = debuggers->begin(); p != debuggers->end(); p++)
            (*p)->destroyEvalScript(script);
    }
}

void
Debug::destroyEvalScript(JSScript *script)
{
    ScriptMap::Ptr p = evalScripts.lookup(script);
    if (p) {
        JS_ASSERT(GetScriptReferent(p->value) == script);
        ClearScriptReferent(p->value);
        evalScripts.remove(p);
    }
}

static JSObject *
DebugScript_check(JSContext *cx, const Value &v, const char *clsname, const char *fnname, bool checkLive)
{
    if (!v.isObject()) {
        ReportObjectRequired(cx);
        return NULL;
    }
    JSObject *thisobj = &v.toObject();
    if (thisobj->clasp != &DebugScript_class) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             clsname, fnname, thisobj->getClass()->name);
        return NULL;
    }

    // Check for Debug.Script.prototype, which is of class DebugScript_class
    // but whose holding object is undefined.
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
DebugScript_checkThis(JSContext *cx, Value *vp, const char *fnname, bool checkLive)
{
    return DebugScript_check(cx, vp[1], "Debug.Script", fnname, checkLive);
}

#define THIS_DEBUGSCRIPT_SCRIPT_NEEDLIVE(cx, vp, fnname, obj, script, checkLive)    \
    JSObject *obj = DebugScript_checkThis(cx, vp, fnname, checkLive);               \
    if (!obj)                                                                       \
        return false;                                                               \
    JSScript *script = GetScriptReferent(obj)

#define THIS_DEBUGSCRIPT_SCRIPT(cx, vp, fnname, obj, script)                  \
    THIS_DEBUGSCRIPT_SCRIPT_NEEDLIVE(cx, vp, fnname, obj, script, false)
#define THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, vp, fnname, obj, script)             \
    THIS_DEBUGSCRIPT_SCRIPT_NEEDLIVE(cx, vp, fnname, obj, script, true)


static JSBool
DebugScript_getLive(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGSCRIPT_SCRIPT(cx, vp, "get live", obj, script);
    vp->setBoolean(!!script);
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
DebugScript_getOffsetLine(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug.Script.getOffsetLine", 1);
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, vp, "getOffsetLine", obj, script);
    size_t offset;
    if (!ScriptOffset(cx, script, vp[2], &offset))
        return false;
    uintN lineno = JS_PCToLineNumber(cx, script, script->code + offset);
    vp->setNumber(lineno);
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
      : BytecodeRange(cx, script), lineno(script->lineno), sn(script->notes()), snpc(script->code) {
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
        // Determine the current line number by reading all source notes up to
        // and including the current offset.
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
 * control flow graph used by DebugScript_{getAllOffsets,getLineOffsets}.
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
DebugScript_getAllOffsets(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, vp, "getAllOffsets", obj, script);

    // First pass: determine which offsets in this script are jump targets and
    // which line numbers jump to them.
    FlowGraphSummary flowData(cx);
    if (!flowData.populate(cx, script))
        return false;

    // Second pass: build the result array.
    JSObject *result = NewDenseEmptyArray(cx);
    if (!result)
        return false;
    for (BytecodeRangeWithLineNumbers r(cx, script); !r.empty(); r.popFront()) {
        size_t offset = r.frontOffset();
        size_t lineno = r.frontLineNumber();

        // Make a note, if the current instruction is an entry point for the current line.
        if (flowData[offset] != NoEdges && flowData[offset] != lineno) {
            // Get the offsets array for this line.
            JSObject *offsets;
            Value offsetsv;
            if (!result->arrayGetOwnDataElement(cx, lineno, &offsetsv))
                return false;

            jsid id;
            if (offsetsv.isObject()) {
                offsets = &offsetsv.toObject();
            } else {
                JS_ASSERT(offsetsv.isMagic(JS_ARRAY_HOLE));

                // Create an empty offsets array for this line.
                // Store it in the result array.
                offsets = NewDenseEmptyArray(cx);
                if (!offsets ||
                    !ValueToId(cx, NumberValue(lineno), &id) ||
                    !result->defineProperty(cx, id, ObjectValue(*offsets)))
                {
                    return false;
                }
            }

            // Append the current offset to the offsets array.
            if (!js_NewbornArrayPush(cx, offsets, NumberValue(offset)))
                return false;
        }
    }

    vp->setObject(*result);
    return true;
}

static JSBool
DebugScript_getLineOffsets(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, vp, "getAllOffsets", obj, script);
    REQUIRE_ARGC("Debug.Script.getLineOffsets", 1);

    // Parse lineno argument.
    size_t lineno;
    bool ok = false;
    if (vp[2].isNumber()) {
        jsdouble d = vp[2].toNumber();
        lineno = size_t(d);
        ok = (lineno == d);
    }
    if (!ok) {
        JS_ReportErrorNumber(cx,  js_GetErrorMessage, NULL, JSMSG_DEBUG_BAD_LINE);
        return false;
    }

    // First pass: determine which offsets in this script are jump targets and
    // which line numbers jump to them.
    FlowGraphSummary flowData(cx);
    if (!flowData.populate(cx, script))
        return false;

    // Second pass: build the result array.
    JSObject *result = NewDenseEmptyArray(cx);
    if (!result)
        return false;
    for (BytecodeRangeWithLineNumbers r(cx, script); !r.empty(); r.popFront()) {
        size_t offset = r.frontOffset();

        // If the op at offset is an entry point, append offset to result.
        if (r.frontLineNumber() == lineno &&
            flowData[offset] != NoEdges &&
            flowData[offset] != lineno)
        {
            if (!js_NewbornArrayPush(cx, result, NumberValue(offset)))
                return false;
        }
    }

    vp->setObject(*result);
    return true;
}

JSBool
DebugScript_setBreakpoint(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug.Script.setBreakpoint", 2);
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, vp, "setBreakpoint", obj, script);
    Debug *dbg = Debug::fromChildJSObject(obj);

    JSObject *holder = GetScriptHolder(obj);
    if (!dbg->observesScope(ScriptScope(cx, script, holder))) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_NOT_DEBUGGING);
        return false;
    }

    size_t offset;
    if (!ScriptOffset(cx, script, vp[2], &offset))
        return false;

    JSObject *handler = NonNullObject(cx, vp[3]);
    if (!handler)
        return false;

    JSCompartment *comp = script->compartment;
    jsbytecode *pc = script->code + offset;
    BreakpointSite *site = comp->getOrCreateBreakpointSite(cx, script, pc, holder);
    if (!site->inc(cx))
        goto fail1;
    if (!cx->runtime->new_<Breakpoint>(dbg, site, handler))
        goto fail2;
    vp->setUndefined();
    return true;

fail2:
    site->dec(cx);
fail1:
    site->destroyIfEmpty(cx->runtime, NULL);
    return false;
}

JSBool
DebugScript_getBreakpoints(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, vp, "getBreakpoints", obj, script);
    Debug *dbg = Debug::fromChildJSObject(obj);

    jsbytecode *pc;
    if (argc > 0) {
        size_t offset;
        if (!ScriptOffset(cx, script, vp[2], &offset))
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
    vp->setObject(*arr);
    return true;
}

JSBool
DebugScript_clearBreakpoint(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug.Script.clearBreakpoint", 1);
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, vp, "clearBreakpoint", obj, script);
    Debug *dbg = Debug::fromChildJSObject(obj);

    JSObject *handler = NonNullObject(cx, vp[2]);
    if (!handler)
        return false;

    script->compartment->clearBreakpointsIn(cx, dbg, script, handler);
    vp->setUndefined();
    return true;
}

JSBool
DebugScript_clearAllBreakpoints(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGSCRIPT_LIVE_SCRIPT(cx, vp, "clearBreakpoint", obj, script);
    Debug *dbg = Debug::fromChildJSObject(obj);
    script->compartment->clearBreakpointsIn(cx, dbg, script, NULL);
    vp->setUndefined();
    return true;
}

static JSBool
DebugScript_construct(JSContext *cx, uintN argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR, "Debug.Script");
    return false;
}

static JSPropertySpec DebugScript_properties[] = {
    JS_PSG("live", DebugScript_getLive, 0),
    JS_PS_END
};

static JSFunctionSpec DebugScript_methods[] = {
    JS_FN("getAllOffsets", DebugScript_getAllOffsets, 0, 0),
    JS_FN("getLineOffsets", DebugScript_getLineOffsets, 1, 0),
    JS_FN("getOffsetLine", DebugScript_getOffsetLine, 0, 0),
    JS_FN("setBreakpoint", DebugScript_setBreakpoint, 2, 0),
    JS_FN("getBreakpoints", DebugScript_getBreakpoints, 1, 0),
    JS_FN("clearBreakpoint", DebugScript_clearBreakpoint, 1, 0),
    JS_FN("clearAllBreakpoints", DebugScript_clearAllBreakpoints, 0, 0),
    JS_FS_END
};


// === Debug.Frame

Class DebugFrame_class = {
    "Frame", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGFRAME_COUNT),
    PropertyStub, PropertyStub, PropertyStub, StrictPropertyStub,
    EnumerateStub, ResolveStub, ConvertStub
};

static JSObject *
CheckThisFrame(JSContext *cx, Value *vp, const char *fnname, bool checkLive)
{
    if (!vp[1].isObject()) {
        ReportObjectRequired(cx);
        return NULL;
    }
    JSObject *thisobj = &vp[1].toObject();
    if (thisobj->getClass() != &DebugFrame_class) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Debug.Frame", fnname, thisobj->getClass()->name);
        return NULL;
    }

    // Forbid Debug.Frame.prototype, which is of class DebugFrame_class but
    // isn't really a working Debug.Frame object. The prototype object is
    // distinguished by having a NULL private value. Also, forbid popped frames.
    if (!thisobj->getPrivate()) {
        if (thisobj->getReservedSlot(JSSLOT_DEBUGFRAME_OWNER).isUndefined()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                                 "Debug.Frame", fnname, "prototype object");
            return NULL;
        }
        if (checkLive) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_NOT_LIVE,
                                 "Debug.Frame", fnname, "stack frame");
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

#define THIS_FRAME(cx, vp, fnname, thisobj, fp)                              \
    JSObject *thisobj = CheckThisFrame(cx, vp, fnname, true);                \
    if (!thisobj)                                                            \
        return false;                                                        \
    StackFrame *fp = (StackFrame *) thisobj->getPrivate();                   \
    JS_ASSERT(StackContains(cx, fp))

static JSBool
DebugFrame_getType(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, vp, "get type", thisobj, fp);

    // Indirect eval frames are both isGlobalFrame() and isEvalFrame(), so the
    // order of checks here is significant.
    vp->setString(fp->isEvalFrame()
                  ? cx->runtime->atomState.evalAtom
                  : fp->isGlobalFrame()
                  ? cx->runtime->atomState.globalAtom
                  : cx->runtime->atomState.callAtom);
    return true;
}

static JSBool
DebugFrame_getCallee(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, vp, "get callee", thisobj, fp);
    *vp = (fp->isFunctionFrame() && !fp->isEvalFrame()) ? fp->calleev() : NullValue();
    return Debug::fromChildJSObject(thisobj)->wrapDebuggeeValue(cx, vp);
}

static JSBool
DebugFrame_getGenerator(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, vp, "get generator", thisobj, fp);
    vp->setBoolean(fp->isGeneratorFrame());
    return true;
}

static JSBool
DebugFrame_getConstructing(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, vp, "get constructing", thisobj, fp);
    vp->setBoolean(fp->isFunctionFrame() && fp->isConstructing());
    return true;
}

static JSBool
DebugFrame_getThis(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, vp, "get this", thisobj, fp);
    {
        AutoCompartment ac(cx, &fp->scopeChain());
        if (!ac.enter())
            return false;
        if (!ComputeThis(cx, fp))
            return false;
        *vp = fp->thisValue();
    }
    return Debug::fromChildJSObject(thisobj)->wrapDebuggeeValue(cx, vp);
}

static JSBool
DebugFrame_getOlder(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, vp, "get this", thisobj, thisfp);
    Debug *dbg = Debug::fromChildJSObject(thisobj);
    for (StackFrame *fp = thisfp->prev(); fp; fp = fp->prev()) {
        if (!fp->isDummyFrame() && dbg->observesFrame(fp))
            return dbg->getScriptFrame(cx, fp, vp);
    }
    vp->setNull();
    return true;
}

Class DebugArguments_class = {
    "Arguments", JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGARGUMENTS_COUNT),
    PropertyStub, PropertyStub, PropertyStub, StrictPropertyStub,
    EnumerateStub, ResolveStub, ConvertStub
};

// The getter used for each element of frame.arguments. See DebugFrame_getArguments.
JSBool
DebugArguments_getArg(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *callee = &CallArgsFromVp(argc, vp).callee();
    int32 i = callee->getReservedSlot(0).toInt32();

    // Check that the this value is an Arguments object.
    if (!vp[1].isObject()) {
        ReportObjectRequired(cx);
        return false;
    }
    JSObject *argsobj = &vp[1].toObject();
    if (argsobj->getClass() != &DebugArguments_class) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Arguments", "getArgument", argsobj->getClass()->name);
        return false;
    }

    // Put the Debug.Frame into the this-value slot, then use THIS_FRAME
    // to check that it is still live and get the fp.
    vp[1] = argsobj->getReservedSlot(JSSLOT_DEBUGARGUMENTS_FRAME);
    THIS_FRAME(cx, vp, "get argument", thisobj, fp);

    // Since getters can be extracted and applied to other objects,
    // there is no guarantee this object has an ith argument.
    JS_ASSERT(i >= 0);
    if (uintN(i) < fp->numActualArgs())
        *vp = fp->actualArgs()[i];
    else
        vp->setUndefined();
    return Debug::fromChildJSObject(thisobj)->wrapDebuggeeValue(cx, vp);
}

JSBool
DebugFrame_getArguments(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, vp, "get arguments", thisobj, fp);
    Value argumentsv = thisobj->getReservedSlot(JSSLOT_DEBUGFRAME_ARGUMENTS);
    if (!argumentsv.isUndefined()) {
        JS_ASSERT(argumentsv.isObjectOrNull());
        *vp = argumentsv;
        return true;
    }

    JSObject *argsobj;
    if (fp->hasArgs()) {
        // Create an arguments object.
        GlobalObject *global = CallArgsFromVp(argc, vp).callee().getGlobal();
        JSObject *proto;
        if (!js_GetClassPrototype(cx, global, JSProto_Array, &proto))
            return false;
        argsobj = NewNonFunction<WithProto::Given>(cx, &DebugArguments_class, proto, global);
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
            JSObject *getobj = js_NewFunction(cx, NULL, DebugArguments_getArg, 0, 0, global, NULL);
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
    *vp = ObjectOrNullValue(argsobj);
    thisobj->setReservedSlot(JSSLOT_DEBUGFRAME_ARGUMENTS, *vp);
    return true;
}

static JSBool
DebugFrame_getScript(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, vp, "get script", thisobj, fp);
    Debug *debug = Debug::fromChildJSObject(thisobj);

    JSObject *scriptObject = NULL;
    if (fp->isFunctionFrame() && !fp->isEvalFrame()) {
        JSFunction *callee = fp->callee().getFunctionPrivate();
        if (callee->isInterpreted()) {
            scriptObject = debug->wrapFunctionScript(cx, callee);
            if (!scriptObject)
                return false;
        }
    } else if (fp->isScriptFrame()) {
        JSScript *script = fp->script();
        // Both calling a JSAPI script object (via JS_ExecuteScript, say) and
        // calling 'eval' create non-function script frames. However, scripts
        // for the former are held by script objects, and must go in
        // heldScripts, whereas scripts for the latter are explicitly destroyed
        // when the call returns, and must go in evalScripts. Distinguish the
        // two cases by checking whether the script has a Script object
        // allocated to it.
        if (script->u.object) {
            JS_ASSERT(!fp->isEvalFrame());
            scriptObject = debug->wrapJSAPIScript(cx, script->u.object);
            if (!scriptObject)
                return false;
        } else {
            JS_ASSERT(fp->isEvalFrame());
            scriptObject = debug->wrapEvalScript(cx, script);
            if (!scriptObject)
                return false;
        }
    }
    vp->setObjectOrNull(scriptObject);
    return true;
}

static JSBool
DebugFrame_getOffset(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, vp, "get offset", thisobj, fp);
    if (fp->isScriptFrame()) {
        JSScript *script = fp->script();
        jsbytecode *pc = fp->pcQuadratic(cx);
        JS_ASSERT(script->code <= pc);
        JS_ASSERT(pc < script->code + script->length);
        size_t offset = pc - script->code;
        vp->setNumber(double(offset));
    } else {
        vp->setUndefined();
    }
    return true;
}

static JSBool
DebugFrame_getLive(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *thisobj = CheckThisFrame(cx, vp, "get live", false);
    if (!thisobj)
        return false;
    StackFrame *fp = (StackFrame *) thisobj->getPrivate();
    vp->setBoolean(!!fp);
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
    js_DestroyScript(cx, script);
    return ok;
}

}

enum EvalBindingsMode { WithoutBindings, WithBindings };

static JSBool
DebugFrameEval(JSContext *cx, uintN argc, Value *vp, EvalBindingsMode mode)
{
    REQUIRE_ARGC(mode == WithBindings ? "Debug.Frame.evalWithBindings" : "Debug.Frame.eval",
                 uintN(mode == WithBindings ? 2 : 1));
    THIS_FRAME(cx, vp, mode == WithBindings ? "evalWithBindings" : "eval", thisobj, fp);
    Debug *dbg = Debug::fromChildJSObject(&vp[1].toObject());

    // Check the first argument, the eval code string.
    if (!vp[2].isString()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_EXPECTED_TYPE,
                             "Debug.Frame.eval", "string", InformalValueTypeName(vp[2]));
        return false;
    }
    JSLinearString *linearStr = vp[2].toString()->ensureLinear(cx);
    if (!linearStr)
        return false;

    // Gather keys and values of bindings, if any. This must be done in the
    // debugger compartment, since that is where any exceptions must be
    // thrown.
    AutoIdVector keys(cx);
    AutoValueVector values(cx);
    if (mode == WithBindings) {
        JSObject *bindingsobj = NonNullObject(cx, vp[3]);
        if (!bindingsobj ||
            !GetPropertyNames(cx, bindingsobj, JSITER_OWNONLY, &keys) ||
            !values.growBy(keys.length()))
        {
            return false;
        }
        for (size_t i = 0; i < keys.length(); i++) {
            Value *vp = &values[i];
            if (!bindingsobj->getProperty(cx, bindingsobj, keys[i], vp) ||
                !dbg->unwrapDebuggeeValue(cx, vp))
            {
                return false;
            }
        }
    }

    AutoCompartment ac(cx, &fp->scopeChain());
    if (!ac.enter())
        return false;

    // Get a scope object.
    if (fp->isNonEvalFunctionFrame() && !fp->hasCallObj() && !CreateFunCallObject(cx, fp))
        return false;
    JSObject *scobj = GetScopeChain(cx, fp);
    if (!scobj)
        return false;

    // If evalWithBindings, create the inner scope object.
    if (mode == WithBindings) {
        // TODO - Should probably create a With object here.
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

    // Run the code and produce the completion value.
    Value rval;
    JS::Anchor<JSString *> anchor(linearStr);
    bool ok = EvaluateInScope(cx, scobj, fp, linearStr->chars(), linearStr->length(),
                              "debugger eval code", 1, &rval);
    return dbg->newCompletionValue(ac, ok, rval, vp);
}

static JSBool
DebugFrame_eval(JSContext *cx, uintN argc, Value *vp)
{
    return DebugFrameEval(cx, argc, vp, WithoutBindings);
}

static JSBool
DebugFrame_evalWithBindings(JSContext *cx, uintN argc, Value *vp)
{
    return DebugFrameEval(cx, argc, vp, WithBindings);
}

static JSBool
DebugFrame_construct(JSContext *cx, uintN argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR, "Debug.Frame");
    return false;
}

static JSPropertySpec DebugFrame_properties[] = {
    JS_PSG("arguments", DebugFrame_getArguments, 0),
    JS_PSG("callee", DebugFrame_getCallee, 0),
    JS_PSG("constructing", DebugFrame_getConstructing, 0),
    JS_PSG("generator", DebugFrame_getGenerator, 0),
    JS_PSG("live", DebugFrame_getLive, 0),
    JS_PSG("offset", DebugFrame_getOffset, 0),
    JS_PSG("older", DebugFrame_getOlder, 0),
    JS_PSG("script", DebugFrame_getScript, 0),
    JS_PSG("this", DebugFrame_getThis, 0),
    JS_PSG("type", DebugFrame_getType, 0),
    JS_PS_END
};

static JSFunctionSpec DebugFrame_methods[] = {
    JS_FN("eval", DebugFrame_eval, 1, 0),
    JS_FN("evalWithBindings", DebugFrame_evalWithBindings, 1, 0),
    JS_FS_END
};


// === Debug.Object

static void
DebugObject_trace(JSTracer *trc, JSObject *obj)
{
    if (!trc->context->runtime->gcCurrentCompartment) {
        if (JSObject *referent = (JSObject *) obj->getPrivate())
            MarkObject(trc, *referent, "Debug.Object referent");
    }
}

Class DebugObject_class = {
    "Object", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGOBJECT_COUNT),
    PropertyStub, PropertyStub, PropertyStub, StrictPropertyStub,
    EnumerateStub, ResolveStub, ConvertStub, NULL,
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    DebugObject_trace
};

static JSObject *
DebugObject_checkThis(JSContext *cx, Value *vp, const char *fnname)
{
    if (!vp[1].isObject()) {
        ReportObjectRequired(cx);
        return NULL;
    }
    JSObject *thisobj = &vp[1].toObject();
    if (thisobj->clasp != &DebugObject_class) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Debug.Object", fnname, thisobj->getClass()->name);
        return NULL;
    }

    // Forbid Debug.Object.prototype, which is of class DebugObject_class
    // but isn't a real working Debug.Object. The prototype object is
    // distinguished by having no referent.
    if (!thisobj->getPrivate()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Debug.Object", fnname, "prototype object");
        return NULL;
    }
    return thisobj;
}

#define THIS_DEBUGOBJECT_REFERENT(cx, vp, fnname, obj)                        \
    JSObject *obj = DebugObject_checkThis(cx, vp, fnname);                    \
    if (!obj)                                                                 \
        return false;                                                         \
    obj = (JSObject *) obj->getPrivate();                                     \
    JS_ASSERT(obj)

#define THIS_DEBUGOBJECT_OWNER_REFERENT(cx, vp, fnname, dbg, obj)             \
    JSObject *obj = DebugObject_checkThis(cx, vp, fnname);                    \
    if (!obj)                                                                 \
        return false;                                                         \
    Debug *dbg = Debug::fromChildJSObject(obj);                               \
    obj = (JSObject *) obj->getPrivate();                                     \
    JS_ASSERT(obj)

static JSBool
DebugObject_construct(JSContext *cx, uintN argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR, "Debug.Object");
    return false;
}

static JSBool
DebugObject_getProto(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, vp, "get proto", dbg, refobj);
    vp->setObjectOrNull(refobj->getProto());
    return dbg->wrapDebuggeeValue(cx, vp);
}

static JSBool
DebugObject_getClass(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_REFERENT(cx, vp, "get class", refobj);
    const char *s = refobj->clasp->name;
    JSAtom *str = js_Atomize(cx, s, strlen(s));
    if (!str)
        return false;
    vp->setString(str);
    return true;
}

static JSBool
DebugObject_getCallable(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_REFERENT(cx, vp, "get callable", refobj);
    vp->setBoolean(refobj->isCallable());
    return true;
}

static JSBool
DebugObject_getName(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, vp, "get name", dbg, obj);
    if (!obj->isFunction()) {
        vp->setUndefined();
        return true;
    }

    JSString *name = obj->getFunctionPrivate()->atom;
    if (!name) {
        vp->setUndefined();
        return true;
    }

    vp->setString(name);
    return dbg->wrapDebuggeeValue(cx, vp);
}

static JSBool
DebugObject_getParameterNames(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_REFERENT(cx, vp, "get parameterNames", obj);
    if (!obj->isFunction()) {
        vp->setUndefined();
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
                Value *elt = result->addressOfDenseArrayElement(i);
                if (name)
                    elt->setString(name);
                else
                    elt->setUndefined();
            }
        }
    } else {
        for (size_t i = 0; i < fun->nargs; i++)
            result->addressOfDenseArrayElement(i)->setUndefined();
    }

    vp->setObject(*result);
    return true;
}

static JSBool
DebugObject_getScript(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, vp, "get script", dbg, obj);

    vp->setUndefined();

    if (!obj->isFunction())
        return true;

    JSFunction *fun = obj->getFunctionPrivate();
    if (!fun->isInterpreted())
        return true;

    JSObject *scriptObject = dbg->wrapFunctionScript(cx, fun);
    if (!scriptObject)
        return false;

    vp->setObject(*scriptObject);
    return true;
}

enum ApplyOrCallMode { ApplyMode, CallMode };

static JSBool
ApplyOrCall(JSContext *cx, uintN argc, Value *vp, ApplyOrCallMode mode)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, vp, "apply", dbg, obj);

    // Any JS exceptions thrown must be in the debugger compartment, so do
    // sanity checks and fallible conversions before entering the debuggee.
    Value calleev = ObjectValue(*obj);
    if (!obj->isCallable()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Debug.Object", "apply", obj->getClass()->name);
        return false;
    }

    // Unwrap Debug.Objects. This happens in the debugger's compartment since
    // that is where any exceptions must be reported.
    Value thisv = argc > 0 ? vp[2] : UndefinedValue();
    if (!dbg->unwrapDebuggeeValue(cx, &thisv))
        return false;
    uintN callArgc = 0;
    Value *callArgv = NULL;
    AutoValueVector argv(cx);
    if (mode == ApplyMode) {
        if (argc >= 2 && !vp[3].isNullOrUndefined()) {
            if (!vp[3].isObject()) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_APPLY_ARGS, js_apply_str);
                return false;
            }
            JSObject *argsobj = &vp[3].toObject();
            if (!js_GetLengthProperty(cx, argsobj, &callArgc))
                return false;
            callArgc = uintN(JS_MIN(callArgc, JS_ARGS_LENGTH_MAX));
            if (!argv.growBy(callArgc) || !GetElements(cx, argsobj, callArgc, argv.begin()))
                return false;
            callArgv = argv.begin();
        }
    } else {
        callArgc = argc > 0 ? uintN(JS_MIN(argc - 1, JS_ARGS_LENGTH_MAX)) : 0;
        callArgv = vp + 3;
    }
    for (uintN i = 0; i < callArgc; i++) {
        if (!dbg->unwrapDebuggeeValue(cx, &callArgv[i]))
            return false;
    }

    // Enter the debuggee compartment and rewrap all input value for that compartment.
    // (Rewrapping always takes place in the destination compartment.)
    AutoCompartment ac(cx, obj);
    if (!ac.enter() || !cx->compartment->wrap(cx, &calleev) || !cx->compartment->wrap(cx, &thisv))
        return false;
    for (uintN i = 0; i < callArgc; i++) {
        if (!cx->compartment->wrap(cx, &callArgv[i]))
            return false;
    }

    // Call the function. Use newCompletionValue to return to the debugger
    // compartment and populate *vp.
    Value rval;
    bool ok = ExternalInvoke(cx, thisv, calleev, callArgc, callArgv, &rval);
    return dbg->newCompletionValue(ac, ok, rval, vp);
}

static JSBool
DebugObject_apply(JSContext *cx, uintN argc, Value *vp)
{
    return ApplyOrCall(cx, argc, vp, ApplyMode);
}

static JSBool
DebugObject_call(JSContext *cx, uintN argc, Value *vp)
{
    return ApplyOrCall(cx, argc, vp, CallMode);
}

static JSPropertySpec DebugObject_properties[] = {
    JS_PSG("proto", DebugObject_getProto, 0),
    JS_PSG("class", DebugObject_getClass, 0),
    JS_PSG("callable", DebugObject_getCallable, 0),
    JS_PSG("name", DebugObject_getName, 0),
    JS_PSG("parameterNames", DebugObject_getParameterNames, 0),
    JS_PSG("script", DebugObject_getScript, 0),
    JS_PS_END
};

static JSFunctionSpec DebugObject_methods[] = {
    JS_FN("apply", DebugObject_apply, 0, 0),
    JS_FN("call", DebugObject_call, 0, 0),
    JS_FS_END
};


// === Glue

extern JS_PUBLIC_API(JSBool)
JS_DefineDebugObject(JSContext *cx, JSObject *obj)
{
    JSObject *objProto;
    if (!js_GetClassPrototype(cx, obj, JSProto_Object, &objProto))
        return false;

    JSObject *debugCtor;
    JSObject *debugProto = js_InitClass(cx, obj, objProto, &Debug::jsclass, Debug::construct, 1,
                                        Debug::properties, Debug::methods, NULL, NULL, &debugCtor);
    if (!debugProto || !debugProto->ensureClassReservedSlots(cx))
        return false;

    JSObject *frameCtor;
    JSObject *frameProto = js_InitClass(cx, debugCtor, objProto, &DebugFrame_class,
                                        DebugFrame_construct, 0,
                                        DebugFrame_properties, DebugFrame_methods, NULL, NULL,
                                        &frameCtor);
    if (!frameProto)
        return false;

    JSObject *scriptCtor;
    JSObject *scriptProto = js_InitClass(cx, debugCtor, objProto, &DebugScript_class,
                                         DebugScript_construct, 0,
                                         DebugScript_properties, DebugScript_methods, NULL, NULL,
                                         &scriptCtor);
    if (!scriptProto || !scriptProto->ensureClassReservedSlots(cx))
        return false;

    JSObject *objectProto = js_InitClass(cx, debugCtor, objProto, &DebugObject_class,
                                         DebugObject_construct, 0,
                                         DebugObject_properties, DebugObject_methods, NULL, NULL);
    if (!objectProto)
        return false;

    debugProto->setReservedSlot(JSSLOT_DEBUG_FRAME_PROTO, ObjectValue(*frameProto));
    debugProto->setReservedSlot(JSSLOT_DEBUG_OBJECT_PROTO, ObjectValue(*objectProto));
    debugProto->setReservedSlot(JSSLOT_DEBUG_SCRIPT_PROTO, ObjectValue(*scriptProto));
    return true;
}

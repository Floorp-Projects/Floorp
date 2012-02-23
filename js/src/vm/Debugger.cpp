/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Debugger.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jswrapper.h"
#include "jsarrayinlines.h"
#include "jsgcinlines.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsopcodeinlines.h"

#include "frontend/BytecodeCompiler.h"
#include "frontend/BytecodeEmitter.h"
#include "gc/Marking.h"
#include "methodjit/Retcon.h"
#include "js/Vector.h"

#include "vm/Stack-inl.h"

using namespace js;


/*** Forward declarations ************************************************************************/

extern Class DebuggerFrame_class;

enum {
    JSSLOT_DEBUGFRAME_OWNER,
    JSSLOT_DEBUGFRAME_ARGUMENTS,
    JSSLOT_DEBUGFRAME_ONSTEP_HANDLER,
    JSSLOT_DEBUGFRAME_ONPOP_HANDLER,
    JSSLOT_DEBUGFRAME_COUNT
};

extern Class DebuggerArguments_class;

enum {
    JSSLOT_DEBUGARGUMENTS_FRAME,
    JSSLOT_DEBUGARGUMENTS_COUNT
};

extern Class DebuggerEnv_class;

enum {
    JSSLOT_DEBUGENV_OWNER,
    JSSLOT_DEBUGENV_COUNT
};

extern Class DebuggerObject_class;

enum {
    JSSLOT_DEBUGOBJECT_OWNER,
    JSSLOT_DEBUGOBJECT_COUNT
};

extern Class DebuggerScript_class;

enum {
    JSSLOT_DEBUGSCRIPT_OWNER,
    JSSLOT_DEBUGSCRIPT_COUNT
};


/*** Utils ***************************************************************************************/

bool
ReportMoreArgsNeeded(JSContext *cx, const char *name, unsigned required)
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

bool
ValueToIdentifier(JSContext *cx, const Value &v, jsid *idp)
{
    jsid id;
    if (!ValueToId(cx, v, &id))
        return false;
    if (!JSID_IS_ATOM(id) || !IsIdentifier(JSID_TO_ATOM(id))) {
        js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_UNEXPECTED_TYPE,
                                 JSDVG_SEARCH_STACK, v, NULL, "not an identifier", NULL);
        return false;
    }
    *idp = id;
    return true;
}

/*
 * A range of all the Debugger.Frame objects for a particular StackFrame.
 *
 * FIXME This checks only current debuggers, so it relies on a hack in
 * Debugger::removeDebuggeeGlobal to make sure only current debuggers have Frame
 * objects with .live === true.
 */
class Debugger::FrameRange {
    JSContext *cx;
    StackFrame *fp;

    /* The debuggers in |fp|'s compartment, or NULL if there are none. */
    GlobalObject::DebuggerVector *debuggers;

    /*
     * The index of the front Debugger.Frame's debugger in debuggers.
     * nextDebugger < debuggerCount if and only if the range is not empty.
     */
    size_t debuggerCount, nextDebugger;

    /*
     * If the range is not empty, this is front Debugger.Frame's entry in its
     * debugger's frame table.
     */
    FrameMap::Ptr entry;

  public:
    /*
     * Return a range containing all Debugger.Frame instances referring to |fp|.
     * |global| is |fp|'s global object; if NULL or omitted, we compute it
     * ourselves from |fp|.
     *
     * We keep an index into the compartment's debugger list, and a
     * FrameMap::Ptr into the current debugger's frame map. Thus, if the set of
     * debuggers in |fp|'s compartment changes, this range becomes invalid.
     * Similarly, if stack frames are added to or removed from frontDebugger(),
     * then the range's front is invalid until popFront is called.
     */
    FrameRange(JSContext *cx, StackFrame *fp, GlobalObject *global = NULL)
      : cx(cx), fp(fp) {
        nextDebugger = 0;

        /* Find our global, if we were not given one. */
        if (!global)
            global = &fp->global();

        /* The frame and global must match. */
        JS_ASSERT(&fp->global() == global);

        /* Find the list of debuggers we'll iterate over. There may be none. */
        debuggers = global->getDebuggers();
        if (debuggers) {
            debuggerCount = debuggers->length();
            findNext();
        } else {
            debuggerCount = 0;
        }
    }

    bool empty() const {
        return nextDebugger >= debuggerCount;
    }

    JSObject *frontFrame() const {
        JS_ASSERT(!empty());
        return entry->value;
    }

    Debugger *frontDebugger() const {
        JS_ASSERT(!empty());
        return (*debuggers)[nextDebugger];
    }

    /*
     * Delete the front frame from its Debugger's frame map. After this call,
     * the range's front is invalid until popFront is called.
     */
    void removeFrontFrame() const {
        JS_ASSERT(!empty());
        frontDebugger()->frames.remove(entry);
    }

    void popFront() {
        JS_ASSERT(!empty());
        nextDebugger++;
        findNext();
    }

  private:
    /*
     * Either make this range refer to the first appropriate Debugger.Frame at
     * or after nextDebugger, or make it empty.
     */
    void findNext() {
        while (!empty()) {
            Debugger *dbg = (*debuggers)[nextDebugger];
            entry = dbg->frames.lookup(fp);
            if (entry)
                break;
            nextDebugger++;
        }
    }
};


/*** Breakpoints *********************************************************************************/

BreakpointSite::BreakpointSite(JSScript *script, jsbytecode *pc)
  : script(script), pc(pc), scriptGlobal(NULL), enabledCount(0),
    trapHandler(NULL), trapClosure(UndefinedValue())
{
    JS_ASSERT(!script->hasBreakpointsAt(pc));
    JS_INIT_CLIST(&breakpoints);
}

void
BreakpointSite::recompile(FreeOp *fop)
{
#ifdef JS_METHODJIT
    if (script->hasJITInfo()) {
        mjit::Recompiler::clearStackReferences(fop, script);
        mjit::ReleaseScriptCode(fop, script);
    }
#endif
}

void
BreakpointSite::inc(FreeOp *fop)
{
    if (enabledCount == 0 && !trapHandler)
        recompile(fop);
    enabledCount++;
}

void
BreakpointSite::dec(FreeOp *fop)
{
    JS_ASSERT(enabledCount > 0);
    enabledCount--;
    if (enabledCount == 0 && !trapHandler)
        recompile(fop);
}

void
BreakpointSite::setTrap(FreeOp *fop, JSTrapHandler handler, const Value &closure)
{
    if (enabledCount == 0)
        recompile(fop);
    trapHandler = handler;
    trapClosure = closure;
}

void
BreakpointSite::clearTrap(FreeOp *fop, JSTrapHandler *handlerp, Value *closurep)
{
    if (handlerp)
        *handlerp = trapHandler;
    if (closurep)
        *closurep = trapClosure;

    trapHandler = NULL;
    trapClosure = UndefinedValue();
    if (enabledCount == 0) {
        if (!fop->runtime()->gcRunning) {
            /* If the GC is running then the script is being destroyed. */
            recompile(fop);
        }
        destroyIfEmpty(fop);
    }
}

void
BreakpointSite::destroyIfEmpty(FreeOp *fop)
{
    if (JS_CLIST_IS_EMPTY(&breakpoints) && !trapHandler)
        script->destroyBreakpointSite(fop, pc);
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
Breakpoint::destroy(FreeOp *fop)
{
    if (debugger->enabled)
        site->dec(fop);
    JS_REMOVE_LINK(&debuggerLinks);
    JS_REMOVE_LINK(&siteLinks);
    site->destroyIfEmpty(fop);
    fop->delete_(this);
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
    frames(cx), scripts(cx), objects(cx), environments(cx)
{
    assertSameCompartment(cx, dbg);

    JSRuntime *rt = cx->runtime;
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
    bool ok = debuggees.init() &&
              frames.init() &&
              scripts.init() &&
              objects.init() &&
              environments.init();
    if (!ok)
        js_ReportOutOfMemory(cx);
    return ok;
}

JS_STATIC_ASSERT(unsigned(JSSLOT_DEBUGFRAME_OWNER) == unsigned(JSSLOT_DEBUGSCRIPT_OWNER));
JS_STATIC_ASSERT(unsigned(JSSLOT_DEBUGFRAME_OWNER) == unsigned(JSSLOT_DEBUGOBJECT_OWNER));
JS_STATIC_ASSERT(unsigned(JSSLOT_DEBUGFRAME_OWNER) == unsigned(JSSLOT_DEBUGENV_OWNER));

Debugger *
Debugger::fromChildJSObject(JSObject *obj)
{
    JS_ASSERT(obj->getClass() == &DebuggerFrame_class ||
              obj->getClass() == &DebuggerScript_class ||
              obj->getClass() == &DebuggerObject_class ||
              obj->getClass() == &DebuggerEnv_class);
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
            NewObjectWithGivenProto(cx, &DebuggerFrame_class, proto, NULL);
        if (!frameobj)
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
    if (!enabled)
        return false;

    if (getHook(OnDebuggerStatement) ||
        getHook(OnExceptionUnwind) ||
        getHook(OnNewScript) ||
        getHook(OnEnterFrame))
    {
        return true;
    }

    /* If any breakpoints are in live scripts, return true. */
    for (Breakpoint *bp = firstBreakpoint(); bp; bp = bp->nextInDebugger()) {
        if (IsScriptMarked(&bp->site->script))
            return true;
    }

    for (FrameMap::Range r = frames.all(); !r.empty(); r.popFront()) {
        JSObject *frameObj = r.front().value;
        if (!frameObj->getReservedSlot(JSSLOT_DEBUGFRAME_ONSTEP_HANDLER).isUndefined() ||
            !frameObj->getReservedSlot(JSSLOT_DEBUGFRAME_ONPOP_HANDLER).isUndefined())
            return true;
    }

    return false;
}

JSTrapStatus
Debugger::slowPathOnEnterFrame(JSContext *cx, Value *vp)
{
    /* Build the list of recipients. */
    AutoValueVector triggered(cx);
    Rooted<GlobalObject*> global(cx, &cx->fp()->global());
    if (GlobalObject::DebuggerVector *debuggers = global->getDebuggers()) {
        for (Debugger **p = debuggers->begin(); p != debuggers->end(); p++) {
            Debugger *dbg = *p;
            JS_ASSERT(dbg->observesFrame(cx->fp()));
            if (dbg->observesEnterFrame() && !triggered.append(ObjectValue(*dbg->toJSObject())))
                return JSTRAP_ERROR;
        }
    }

    /* Deliver the event, checking again as in dispatchHook. */
    for (Value *p = triggered.begin(); p != triggered.end(); p++) {
        Debugger *dbg = Debugger::fromJSObject(&p->toObject());
        if (dbg->debuggees.has(global) && dbg->observesEnterFrame()) {
            JSTrapStatus status = dbg->fireEnterFrame(cx, vp);
            if (status != JSTRAP_CONTINUE)
                return status;
        }
    }

    return JSTRAP_CONTINUE;
}

/*
 * Handle leaving a frame with debuggers watching. |frameOk| indicates whether
 * the frame is exiting normally or abruptly. Set |cx|'s exception and/or
 * |cx->fp()|'s return value, and return a new success value.
 */
bool
Debugger::slowPathOnLeaveFrame(JSContext *cx, bool frameOk)
{
    StackFrame *fp = cx->fp();
    Rooted<GlobalObject*> global(cx, &fp->global());

    /* Save the frame's completion value. */
    JSTrapStatus status;
    RootedValue value(cx);
    Debugger::resultToCompletion(cx, frameOk, fp->returnValue(), &status, value.address());

    /* Build a list of the recipients. */
    AutoObjectVector frames(cx);
    for (FrameRange r(cx, fp, global); !r.empty(); r.popFront()) {
        if (!frames.append(r.frontFrame())) {
            cx->clearPendingException();
            return false;
        }
    }

    /* For each Debugger.Frame, fire its onPop handler, if any. */
    for (JSObject **p = frames.begin(); p != frames.end(); p++) {
        RootedObject frameobj(cx, *p);
        Debugger *dbg = Debugger::fromChildJSObject(frameobj);

        if (dbg->enabled &&
            !frameobj->getReservedSlot(JSSLOT_DEBUGFRAME_ONPOP_HANDLER).isUndefined()) {
            RootedValue handler(cx, frameobj->getReservedSlot(JSSLOT_DEBUGFRAME_ONPOP_HANDLER));

            AutoCompartment ac(cx, dbg->object);

            if (!ac.enter()) {
                status = JSTRAP_ERROR;
                break;
            }

            Value completion;
            if (!dbg->newCompletionValue(cx, status, value, &completion)) {
                status = dbg->handleUncaughtException(ac, NULL, false);
                break;
            }

            /* Call the onPop handler. */
            Value rval;
            bool hookOk = Invoke(cx, ObjectValue(*frameobj), handler, 1, &completion, &rval);
            Value nextValue;
            JSTrapStatus nextStatus = dbg->parseResumptionValue(ac, hookOk, rval, &nextValue);

            /*
             * At this point, we are back in the debuggee compartment, and any error has
             * been wrapped up as a completion value.
             */
            JS_ASSERT(cx->compartment == global->compartment());
            JS_ASSERT(!cx->isExceptionPending());

            /* JSTRAP_CONTINUE means "make no change". */
            if (nextStatus != JSTRAP_CONTINUE) {
                status = nextStatus;
                value = nextValue;
            }
        }
    }

    /*
     * Clean up all Debugger.Frame instances. Use a fresh FrameRange, as one
     * debugger's onPop handler could have caused another debugger to create its
     * own Debugger.Frame instance.
     */
    for (FrameRange r(cx, fp, global); !r.empty(); r.popFront()) {
        JSObject *frameobj = r.frontFrame();
        Debugger *dbg = r.frontDebugger();
        JS_ASSERT(dbg == Debugger::fromChildJSObject(frameobj));

        frameobj->setPrivate(NULL);

        /* If this frame had an onStep handler, adjust the script's count. */
        if (!frameobj->getReservedSlot(JSSLOT_DEBUGFRAME_ONSTEP_HANDLER).isUndefined() &&
            fp->isScriptFrame() &&
            !fp->script()->changeStepModeCount(cx, -1))
        {
            status = JSTRAP_ERROR;
            /* Don't exit the loop; we must mark all frames as dead. */
        }

        dbg->frames.remove(fp);
    }

    /*
     * If this is an eval frame, then from the debugger's perspective the
     * script is about to be destroyed. Remove any breakpoints in it.
     */
    if (fp->isEvalFrame()) {
        JSScript *script = fp->script();
        script->clearBreakpointsIn(cx->runtime->defaultFreeOp(), NULL, NULL);
    }

    /* Establish (status, value) as our resumption value. */
    switch (status) {
      case JSTRAP_RETURN:
        fp->setReturnValue(value);
        return true;

      case JSTRAP_THROW:
        cx->setPendingException(value);
        return false;

      case JSTRAP_ERROR:
        JS_ASSERT(!cx->isExceptionPending());
        return false;

      default:
        JS_NOT_REACHED("bad final trap status");
    }
}

bool
Debugger::wrapEnvironment(JSContext *cx, Handle<Env*> env, Value *rval)
{
    if (!env) {
        rval->setNull();
        return true;
    }

    /*
     * DebuggerEnv should only wrap a debug scope chain obtained (transitively)
     * from GetDebugScopeFor(Frame|Function).
     */
    JS_ASSERT(!env->isScope());

    JSObject *envobj;
    ObjectWeakMap::AddPtr p = environments.lookupForAdd(env);
    if (p) {
        envobj = p->value;
    } else {
        /* Create a new Debugger.Environment for env. */
        JSObject *proto = &object->getReservedSlot(JSSLOT_DEBUG_ENV_PROTO).toObject();
        envobj = NewObjectWithGivenProto(cx, &DebuggerEnv_class, proto, NULL);
        if (!envobj)
            return false;
        envobj->setPrivate(env);
        envobj->setReservedSlot(JSSLOT_DEBUGENV_OWNER, ObjectValue(*object));
        if (!environments.relookupOrAdd(p, env.value(), envobj)) {
            js_ReportOutOfMemory(cx);
            return false;
        }
    }
    rval->setObject(*envobj);
    return true;
}

bool
Debugger::wrapDebuggeeValue(JSContext *cx, Value *vp)
{
    assertSameCompartment(cx, object.get());

    if (vp->isObject()) {
        RootedObject obj(cx, &vp->toObject());

        ObjectWeakMap::AddPtr p = objects.lookupForAdd(obj);
        if (p) {
            vp->setObject(*p->value);
        } else {
            /* Create a new Debugger.Object for obj. */
            JSObject *proto = &object->getReservedSlot(JSSLOT_DEBUG_OBJECT_PROTO).toObject();
            JSObject *dobj =
                NewObjectWithGivenProto(cx, &DebuggerObject_class, proto, NULL);
            if (!dobj)
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
    assertSameCompartment(cx, object.get(), *vp);
    if (vp->isObject()) {
        JSObject *dobj = &vp->toObject();
        if (dobj->getClass() != &DebuggerObject_class) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_EXPECTED_TYPE,
                                 "Debugger", "Debugger.Object", dobj->getClass()->name);
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
            if (Invoke(cx, ObjectValue(*object), fval, 1, &exc, &rv))
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

void
Debugger::resultToCompletion(JSContext *cx, bool ok, const Value &rv,
                             JSTrapStatus *status, Value *value)
{
    JS_ASSERT_IF(ok, !cx->isExceptionPending());

    if (ok) {
        *status = JSTRAP_RETURN;
        *value = rv;
    } else if (cx->isExceptionPending()) {
        *status = JSTRAP_THROW;
        *value = cx->getPendingException();
        cx->clearPendingException();
    } else {
        *status = JSTRAP_ERROR;
        value->setUndefined();
    }
}

bool
Debugger::newCompletionValue(JSContext *cx, JSTrapStatus status, Value value_, Value *result)
{
    /*
     * We must be in the debugger's compartment, since that's where we want
     * to construct the completion value.
     */
    assertSameCompartment(cx, object.get());

    RootedId key(cx);
    RootedValue value(cx, value_);

    switch (status) {
      case JSTRAP_RETURN:
        key = NameToId(cx->runtime->atomState.returnAtom);
        break;

      case JSTRAP_THROW:
        key = NameToId(cx->runtime->atomState.throwAtom);
        break;

      case JSTRAP_ERROR:
        result->setNull();
        return true;

      default:
        JS_NOT_REACHED("bad status passed to Debugger::newCompletionValue");
    }

    /* Common tail for JSTRAP_RETURN and JSTRAP_THROW. */
    RootedObject obj(cx, NewBuiltinClassInstance(cx, &ObjectClass));
    if (!obj ||
        !wrapDebuggeeValue(cx, value.address()) ||
        !DefineNativeProperty(cx, obj, key, value, JS_PropertyStub, JS_StrictPropertyStub,
                              JSPROP_ENUMERATE, 0, 0))
    {
        return false;
    }

    result->setObject(*obj);
    return true;
}

bool
Debugger::receiveCompletionValue(AutoCompartment &ac, bool ok, Value val, Value *vp)
{
    JSContext *cx = ac.context;

    JSTrapStatus status;
    Value value;
    resultToCompletion(cx, ok, val, &status, &value);
    ac.leave();
    return newCompletionValue(cx, status, value, vp);
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
    jsid returnId = NameToId(cx->runtime->atomState.returnAtom);
    jsid throwId = NameToId(cx->runtime->atomState.throwAtom);
    bool okResumption = rv.isObject();
    if (okResumption) {
        obj = &rv.toObject();
        okResumption = obj->isObject();
    }
    if (okResumption) {
        shape = obj->lastProperty();
        okResumption = shape->previous() &&
             !shape->previous()->previous() &&
             (shape->propid() == returnId || shape->propid() == throwId) &&
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
    return shape->propid() == returnId ? JSTRAP_RETURN : JSTRAP_THROW;
}

bool
CallMethodIfPresent(JSContext *cx, HandleObject obj, const char *name, int argc, Value *argv,
                    Value *rval)
{
    rval->setUndefined();
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    Value fval;
    return atom &&
           GetMethod(cx, obj, RootedId(cx, AtomToId(atom)), 0, &fval) &&
           (!js_IsCallable(fval) ||
            Invoke(cx, ObjectValue(*obj), fval, argc, argv, rval));
}

JSTrapStatus
Debugger::fireDebuggerStatement(JSContext *cx, Value *vp)
{
    RootedObject hook(cx, getHook(OnDebuggerStatement));
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
    bool ok = Invoke(cx, ObjectValue(*object), ObjectValue(*hook), 1, argv, &rv);
    return parseResumptionValue(ac, ok, rv, vp);
}

JSTrapStatus
Debugger::fireExceptionUnwind(JSContext *cx, Value *vp)
{
    RootedObject hook(cx, getHook(OnExceptionUnwind));
    JS_ASSERT(hook);
    JS_ASSERT(hook->isCallable());

    StackFrame *fp = cx->fp();
    RootedValue exc(cx, cx->getPendingException());
    cx->clearPendingException();

    AutoCompartment ac(cx, object);
    if (!ac.enter())
        return JSTRAP_ERROR;

    Value argv[2];
    AutoValueArray avr(cx, argv, 2);

    argv[1] = exc;
    if (!getScriptFrame(cx, fp, &argv[0]) || !wrapDebuggeeValue(cx, &argv[1]))
        return handleUncaughtException(ac, vp, false);

    Value rv;
    bool ok = Invoke(cx, ObjectValue(*object), ObjectValue(*hook), 2, argv, &rv);
    JSTrapStatus st = parseResumptionValue(ac, ok, rv, vp);
    if (st == JSTRAP_CONTINUE)
        cx->setPendingException(exc);
    return st;
}

JSTrapStatus
Debugger::fireEnterFrame(JSContext *cx, Value *vp)
{
    RootedObject hook(cx, getHook(OnEnterFrame));
    JS_ASSERT(hook);
    JS_ASSERT(hook->isCallable());

    StackFrame *fp = cx->fp();
    AutoCompartment ac(cx, object);
    if (!ac.enter())
        return JSTRAP_ERROR;

    Value argv[1];
    if (!getScriptFrame(cx, fp, &argv[0]))
        return handleUncaughtException(ac, vp, false);

    Value rv;
    bool ok = Invoke(cx, ObjectValue(*object), ObjectValue(*hook), 1, argv, &rv);
    return parseResumptionValue(ac, ok, rv, vp);
}

void
Debugger::fireNewScript(JSContext *cx, Handle<JSScript*> script)
{
    RootedObject hook(cx, getHook(OnNewScript));
    JS_ASSERT(hook);
    JS_ASSERT(hook->isCallable());

    AutoCompartment ac(cx, object);
    if (!ac.enter())
        return;

    JSObject *dsobj = wrapScript(cx, script);
    if (!dsobj) {
        handleUncaughtException(ac, NULL, false);
        return;
    }

    Value argv[1];
    argv[0].setObject(*dsobj);
    Value rv;
    if (!Invoke(cx, ObjectValue(*object), ObjectValue(*hook), 1, argv, &rv))
        handleUncaughtException(ac, NULL, true);
}

JSTrapStatus
Debugger::dispatchHook(JSContext *cx, Value *vp, Hook which)
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
    Rooted<GlobalObject*> global(cx, &cx->fp()->global());
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
Debugger::slowPathOnNewScript(JSContext *cx, JSScript *script_, GlobalObject *compileAndGoGlobal_)
{
    Rooted<JSScript*> script(cx, script_);
    Rooted<GlobalObject*> compileAndGoGlobal(cx, compileAndGoGlobal_);

    JS_ASSERT(script->compileAndGo == !!compileAndGoGlobal);

    /*
     * Build the list of recipients. For compile-and-go scripts, this is the
     * same as the generic Debugger::dispatchHook code, but non-compile-and-go
     * scripts are not tied to particular globals. We deliver them to every
     * debugger observing any global in the script's compartment.
     */
    AutoValueVector triggered(cx);
    if (script->compileAndGo) {
        if (GlobalObject::DebuggerVector *debuggers = compileAndGoGlobal->getDebuggers()) {
            if (!AddNewScriptRecipients(debuggers, &triggered))
                return;
        }
    } else {
        GlobalObjectSet &debuggees = script->compartment()->getDebuggees();
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
        if ((!compileAndGoGlobal || dbg->debuggees.has(compileAndGoGlobal)) &&
            dbg->enabled && dbg->getHook(OnNewScript)) {
            dbg->fireNewScript(cx, script);
        }
    }
}

JSTrapStatus
Debugger::onTrap(JSContext *cx, Value *vp)
{
    StackFrame *fp = cx->fp();
    Rooted<JSScript*> script(cx, fp->script());
    Rooted<GlobalObject*> scriptGlobal(cx, &fp->global());
    jsbytecode *pc = cx->regs().pc;
    BreakpointSite *site = script->getBreakpointSite(pc);
    JSOp op = JSOp(*pc);

    /* Build list of breakpoint handlers. */
    Vector<Breakpoint *> triggered(cx);
    for (Breakpoint *bp = site->firstBreakpoint(); bp; bp = bp->nextInSite()) {
        if (!triggered.append(bp))
            return JSTRAP_ERROR;
    }

    for (Breakpoint **p = triggered.begin(); p != triggered.end(); p++) {
        Breakpoint *bp = *p;

        /* Handlers can clear breakpoints. Check that bp still exists. */
        if (!site || !site->hasBreakpoint(bp))
            continue;


        /*
         * There are two reasons we have to check whether dbg is enabled and
         * debugging scriptGlobal.
         *
         * One is just that one breakpoint handler can disable other Debuggers
         * or remove debuggees.
         *
         * The other has to do with non-compile-and-go scripts, which have no
         * specific global--until they are executed. Only now do we know which
         * global the script is running against.
         */
        Debugger *dbg = bp->debugger;
        if (dbg->enabled && dbg->debuggees.lookup(scriptGlobal)) {
            AutoCompartment ac(cx, dbg->object);
            if (!ac.enter())
                return JSTRAP_ERROR;

            Value argv[1];
            if (!dbg->getScriptFrame(cx, fp, &argv[0]))
                return dbg->handleUncaughtException(ac, vp, false);
            Value rv;
            bool ok = CallMethodIfPresent(cx, RootedObject(cx, bp->handler), "hit", 1, argv, &rv);
            JSTrapStatus st = dbg->parseResumptionValue(ac, ok, rv, vp, true);
            if (st != JSTRAP_CONTINUE)
                return st;

            /* Calling JS code invalidates site. Reload it. */
            site = script->getBreakpointSite(pc);
        }
    }

    if (site && site->trapHandler) {
        JSTrapStatus st = site->trapHandler(cx, fp->script(), pc, vp, site->trapClosure);
        if (st != JSTRAP_CONTINUE)
            return st;
    }

    /* By convention, return the true op to the interpreter in vp. */
    vp->setInt32(op);
    return JSTRAP_CONTINUE;
}

JSTrapStatus
Debugger::onSingleStep(JSContext *cx, Value *vp)
{
    StackFrame *fp = cx->fp();

    /*
     * We may be stepping over a JSOP_EXCEPTION, that pushes the context's
     * pending exception for a 'catch' clause to handle. Don't let the
     * onStep handlers mess with that (other than by returning a resumption
     * value).
     */
    RootedValue exception(cx, UndefinedValue());
    bool exceptionPending = cx->isExceptionPending();
    if (exceptionPending) {
        exception = cx->getPendingException();
        cx->clearPendingException();
    }

    /* We should only receive single-step traps for scripted frames. */
    JS_ASSERT(fp->isScriptFrame());

    /*
     * Build list of Debugger.Frame instances referring to this frame with
     * onStep handlers.
     */
    AutoObjectVector frames(cx);
    for (FrameRange r(cx, fp); !r.empty(); r.popFront()) {
        JSObject *frame = r.frontFrame();
        if (!frame->getReservedSlot(JSSLOT_DEBUGFRAME_ONSTEP_HANDLER).isUndefined() &&
            !frames.append(frame))
        {
            return JSTRAP_ERROR;
        }
    }

#ifdef DEBUG
    /*
     * Validate the single-step count on this frame's script, to ensure that
     * we're not receiving traps we didn't ask for. Even when frames is
     * non-empty (and thus we know this trap was requested), do the check
     * anyway, to make sure the count has the correct non-zero value.
     *
     * The converse --- ensuring that we do receive traps when we should --- can
     * be done with unit tests.
     */
    {
        uint32_t stepperCount = 0;
        JSScript *trappingScript = fp->script();
        GlobalObject *global = &fp->global();
        if (GlobalObject::DebuggerVector *debuggers = global->getDebuggers()) {
            for (Debugger **p = debuggers->begin(); p != debuggers->end(); p++) {
                Debugger *dbg = *p;
                for (FrameMap::Range r = dbg->frames.all(); !r.empty(); r.popFront()) {
                    StackFrame *frame = r.front().key;
                    JSObject *frameobj = r.front().value;
                    if (frame->isScriptFrame() &&
                        frame->script() == trappingScript &&
                        !frameobj->getReservedSlot(JSSLOT_DEBUGFRAME_ONSTEP_HANDLER).isUndefined())
                    {
                        stepperCount++;
                    }
                }
            }
        }
        if (trappingScript->compileAndGo)
            JS_ASSERT(stepperCount == trappingScript->stepModeCount());
        else
            JS_ASSERT(stepperCount <= trappingScript->stepModeCount());
    }
#endif

    /* Call all the onStep handlers we found. */
    for (JSObject **p = frames.begin(); p != frames.end(); p++) {
        JSObject *frame = *p;
        Debugger *dbg = Debugger::fromChildJSObject(frame);
        AutoCompartment ac(cx, dbg->object);
        if (!ac.enter())
            return JSTRAP_ERROR;
        const Value &handler = frame->getReservedSlot(JSSLOT_DEBUGFRAME_ONSTEP_HANDLER);
        Value rval;
        bool ok = Invoke(cx, ObjectValue(*frame), handler, 0, NULL, &rval);
        JSTrapStatus st = dbg->parseResumptionValue(ac, ok, rval, vp);
        if (st != JSTRAP_CONTINUE)
            return st;
    }

    vp->setUndefined();
    if (exceptionPending)
        cx->setPendingException(exception);
    return JSTRAP_CONTINUE;
}


/*** Debugger JSObjects **************************************************************************/

void
Debugger::markKeysInCompartment(JSTracer *tracer)
{
    /*
     * WeakMap::Range is deliberately private, to discourage C++ code from
     * enumerating WeakMap keys. However in this case we need access, so we
     * make a base-class reference. Range is public in HashMap.
     */
    typedef HashMap<HeapPtrObject, HeapPtrObject, DefaultHasher<HeapPtrObject>, RuntimeAllocPolicy>
        ObjectMap;
    const ObjectMap &objStorage = objects;
    for (ObjectMap::Range r = objStorage.all(); !r.empty(); r.popFront()) {
        const HeapPtrObject &key = r.front().key;
        HeapPtrObject tmp(key);
        gc::MarkObject(tracer, &tmp, "cross-compartment WeakMap key");
        JS_ASSERT(tmp == key);
    }

    const ObjectMap &envStorage = environments;
    for (ObjectMap::Range r = envStorage.all(); !r.empty(); r.popFront()) {
        const HeapPtrObject &key = r.front().key;
        HeapPtrObject tmp(key);
        js::gc::MarkObject(tracer, &tmp, "cross-compartment WeakMap key");
        JS_ASSERT(tmp == key);
    }

    typedef HashMap<HeapPtrScript, HeapPtrObject, DefaultHasher<HeapPtrScript>, RuntimeAllocPolicy>
        ScriptMap;
    const ScriptMap &scriptStorage = scripts;
    for (ScriptMap::Range r = scriptStorage.all(); !r.empty(); r.popFront()) {
        const HeapPtrScript &key = r.front().key;
        HeapPtrScript tmp(key);
        gc::MarkScript(tracer, &tmp, "cross-compartment WeakMap key");
        JS_ASSERT(tmp == key);
    }
}

/*
 * Ordinarily, WeakMap keys and values are marked because at some point it was
 * discovered that the WeakMap was live; that is, some object containing the
 * WeakMap was marked during mark phase.
 *
 * However, during compartment GC, we have to do something about
 * cross-compartment WeakMaps in non-GC'd compartments. If their keys and values
 * might need to be marked, we have to do it manually.
 *
 * Each Debugger object keeps three cross-compartment WeakMaps: objects, script,
 * and environments. They have the nice property that all their values are in
 * the same compartment as the Debugger object, so we only need to mark the
 * keys. We must simply mark all keys that are in a compartment being GC'd.
 *
 * We must scan all Debugger objects regardless of whether they *currently*
 * have any debuggees in a compartment being GC'd, because the WeakMap
 * entries persist even when debuggees are removed.
 *
 * This happens during the initial mark phase, not iterative marking, because
 * all the edges being reported here are strong references.
 */
void
Debugger::markCrossCompartmentDebuggerObjectReferents(JSTracer *tracer)
{
    JSRuntime *rt = tracer->runtime;

    /*
     * Mark all objects in comp that are referents of Debugger.Objects in other
     * compartments.
     */
    for (JSCList *p = &rt->debuggerList; (p = JS_NEXT_LINK(p)) != &rt->debuggerList;) {
        Debugger *dbg = Debugger::fromLinks(p);
        if (!dbg->object->compartment()->isCollecting())
            dbg->markKeysInCompartment(tracer);
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
Debugger::markAllIteratively(GCMarker *trc)
{
    bool markedAny = false;

    /*
     * Find all Debugger objects in danger of GC. This code is a little
     * convoluted since the easiest way to find them is via their debuggees.
     */
    JSRuntime *rt = trc->runtime;
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        GlobalObjectSet &debuggees = c->getDebuggees();
        for (GlobalObjectSet::Enum e(debuggees); !e.empty(); e.popFront()) {
            GlobalObject *global = e.front();
            if (!IsObjectMarked(&global))
                continue;
            else
                e.rekeyFront(global);

            /*
             * Every debuggee has at least one debugger, so in this case
             * getDebuggers can't return NULL.
             */
            const GlobalObject::DebuggerVector *debuggers = global->getDebuggers();
            JS_ASSERT(debuggers);
            for (Debugger * const *p = debuggers->begin(); p != debuggers->end(); p++) {
                Debugger *dbg = *p;

                /*
                 * dbg is a Debugger with at least one debuggee. Check three things:
                 *   - dbg is actually in a compartment being GC'd
                 *   - it isn't already marked
                 *   - it actually has hooks that might be called
                 */
                HeapPtrObject &dbgobj = dbg->toJSObjectRef();
                if (!dbgobj->compartment()->isCollecting())
                    continue;

                bool dbgMarked = IsObjectMarked(&dbgobj);
                if (!dbgMarked && dbg->hasAnyLiveHooks()) {
                    /*
                     * obj could be reachable only via its live, enabled
                     * debugger hooks, which may yet be called.
                     */
                    MarkObject(trc, &dbgobj, "enabled Debugger");
                    markedAny = true;
                    dbgMarked = true;
                }

                if (dbgMarked) {
                    /* Search for breakpoints to mark. */
                    for (Breakpoint *bp = dbg->firstBreakpoint(); bp; bp = bp->nextInDebugger()) {
                        if (IsScriptMarked(&bp->site->script)) {
                            /*
                             * The debugger and the script are both live.
                             * Therefore the breakpoint handler is live.
                             */
                            if (!IsObjectMarked(&bp->getHandlerRef())) {
                                MarkObject(trc, &bp->getHandlerRef(), "breakpoint handler");
                                markedAny = true;
                            }
                        }
                    }
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
        MarkObject(trc, &uncaughtExceptionHook, "hooks");

    /*
     * Mark Debugger.Frame objects. These are all reachable from JS, because the
     * corresponding StackFrames are still on the stack.
     *
     * (Once we support generator frames properly, we will need
     * weakly-referenced Debugger.Frame objects as well, for suspended generator
     * frames.)
     */
    for (FrameMap::Range r = frames.all(); !r.empty(); r.popFront()) {
        HeapPtrObject &frameobj = r.front().value;
        JS_ASSERT(frameobj->getPrivate());
        MarkObject(trc, &frameobj, "live Debugger.Frame");
    }

    /* Trace the weak map from JSScript instances to Debugger.Script objects. */
    scripts.trace(trc);

    /* Trace the referent -> Debugger.Object weak map. */
    objects.trace(trc);

    /* Trace the referent -> Debugger.Environment weak map. */
    environments.trace(trc);
}

void
Debugger::sweepAll(FreeOp *fop)
{
    JSRuntime *rt = fop->runtime();

    for (JSCList *p = &rt->debuggerList; (p = JS_NEXT_LINK(p)) != &rt->debuggerList;) {
        Debugger *dbg = Debugger::fromLinks(p);

        if (!IsObjectMarked(&dbg->object)) {
            /*
             * dbg is being GC'd. Detach it from its debuggees. The debuggee
             * might be GC'd too. Since detaching requires access to both
             * objects, this must be done before finalize time.
             */
            for (GlobalObjectSet::Enum e(dbg->debuggees); !e.empty(); e.popFront())
                dbg->removeDebuggeeGlobal(fop, e.front(), NULL, &e);
        }

    }

    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++) {
        /* For each debuggee being GC'd, detach it from all its debuggers. */
        GlobalObjectSet &debuggees = (*c)->getDebuggees();
        for (GlobalObjectSet::Enum e(debuggees); !e.empty(); e.popFront()) {
            GlobalObject *global = e.front();
            if (!IsObjectMarked(&global))
                detachAllDebuggersFromGlobal(fop, global, &e);
            else
                e.rekeyFront(global);
        }
    }
}

void
Debugger::detachAllDebuggersFromGlobal(FreeOp *fop, GlobalObject *global,
                                       GlobalObjectSet::Enum *compartmentEnum)
{
    const GlobalObject::DebuggerVector *debuggers = global->getDebuggers();
    JS_ASSERT(!debuggers->empty());
    while (!debuggers->empty())
        debuggers->back()->removeDebuggeeGlobal(fop, global, compartmentEnum, NULL);
}

void
Debugger::finalize(FreeOp *fop, JSObject *obj)
{
    Debugger *dbg = fromJSObject(obj);
    if (!dbg)
        return;
    JS_ASSERT(dbg->debuggees.empty());
    fop->delete_(dbg);
}

Class Debugger::jsclass = {
    "Debugger",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUG_COUNT),
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Debugger::finalize,
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
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
Debugger::getEnabled(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGGER(cx, argc, vp, "get enabled", args, dbg);
    args.rval().setBoolean(dbg->enabled);
    return true;
}

JSBool
Debugger::setEnabled(JSContext *cx, unsigned argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.set enabled", 1);
    THIS_DEBUGGER(cx, argc, vp, "set enabled", args, dbg);
    bool enabled = js_ValueToBoolean(args[0]);

    if (enabled != dbg->enabled) {
        for (Breakpoint *bp = dbg->firstBreakpoint(); bp; bp = bp->nextInDebugger()) {
            if (enabled)
                bp->site->inc(cx->runtime->defaultFreeOp());
            else
                bp->site->dec(cx->runtime->defaultFreeOp());
        }
    }

    dbg->enabled = enabled;
    args.rval().setUndefined();
    return true;
}

JSBool
Debugger::getHookImpl(JSContext *cx, unsigned argc, Value *vp, Hook which)
{
    JS_ASSERT(which >= 0 && which < HookCount);
    THIS_DEBUGGER(cx, argc, vp, "getHook", args, dbg);
    args.rval() = dbg->object->getReservedSlot(JSSLOT_DEBUG_HOOK_START + which);
    return true;
}

JSBool
Debugger::setHookImpl(JSContext *cx, unsigned argc, Value *vp, Hook which)
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
Debugger::getOnDebuggerStatement(JSContext *cx, unsigned argc, Value *vp)
{
    return getHookImpl(cx, argc, vp, OnDebuggerStatement);
}

JSBool
Debugger::setOnDebuggerStatement(JSContext *cx, unsigned argc, Value *vp)
{
    return setHookImpl(cx, argc, vp, OnDebuggerStatement);
}

JSBool
Debugger::getOnExceptionUnwind(JSContext *cx, unsigned argc, Value *vp)
{
    return getHookImpl(cx, argc, vp, OnExceptionUnwind);
}

JSBool
Debugger::setOnExceptionUnwind(JSContext *cx, unsigned argc, Value *vp)
{
    return setHookImpl(cx, argc, vp, OnExceptionUnwind);
}

JSBool
Debugger::getOnNewScript(JSContext *cx, unsigned argc, Value *vp)
{
    return getHookImpl(cx, argc, vp, OnNewScript);
}

JSBool
Debugger::setOnNewScript(JSContext *cx, unsigned argc, Value *vp)
{
    return setHookImpl(cx, argc, vp, OnNewScript);
}

JSBool
Debugger::getOnEnterFrame(JSContext *cx, unsigned argc, Value *vp)
{
    return getHookImpl(cx, argc, vp, OnEnterFrame);
}

JSBool
Debugger::setOnEnterFrame(JSContext *cx, unsigned argc, Value *vp)
{
    return setHookImpl(cx, argc, vp, OnEnterFrame);
}

JSBool
Debugger::getUncaughtExceptionHook(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGGER(cx, argc, vp, "get uncaughtExceptionHook", args, dbg);
    args.rval().setObjectOrNull(dbg->uncaughtExceptionHook);
    return true;
}

JSBool
Debugger::setUncaughtExceptionHook(JSContext *cx, unsigned argc, Value *vp)
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
        if (obj->getClass() == &DebuggerObject_class) {
            Value rv = v;
            if (!unwrapDebuggeeValue(cx, &rv))
                return NULL;
            return &rv.toObject();
        }
        if (IsCrossCompartmentWrapper(obj))
            return &GetProxyPrivate(obj).toObject();
    }
    return obj;
}

JSBool
Debugger::addDebuggee(JSContext *cx, unsigned argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.addDebuggee", 1);
    THIS_DEBUGGER(cx, argc, vp, "addDebuggee", args, dbg);
    RootedObject referent(cx, dbg->unwrapDebuggeeArgument(cx, args[0]));
    if (!referent)
        return false;
    Rooted<GlobalObject*> global(cx, &referent->global());
    if (!dbg->addDebuggeeGlobal(cx, global))
        return false;

    Value v = ObjectValue(*referent);
    if (!dbg->wrapDebuggeeValue(cx, &v))
        return false;
    args.rval() = v;
    return true;
}

JSBool
Debugger::removeDebuggee(JSContext *cx, unsigned argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.removeDebuggee", 1);
    THIS_DEBUGGER(cx, argc, vp, "removeDebuggee", args, dbg);
    JSObject *referent = dbg->unwrapDebuggeeArgument(cx, args[0]);
    if (!referent)
        return false;
    GlobalObject *global = &referent->global();
    if (dbg->debuggees.has(global))
        dbg->removeDebuggeeGlobal(cx->runtime->defaultFreeOp(), global, NULL, NULL);
    args.rval().setUndefined();
    return true;
}

JSBool
Debugger::hasDebuggee(JSContext *cx, unsigned argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.hasDebuggee", 1);
    THIS_DEBUGGER(cx, argc, vp, "hasDebuggee", args, dbg);
    JSObject *referent = dbg->unwrapDebuggeeArgument(cx, args[0]);
    if (!referent)
        return false;
    args.rval().setBoolean(!!dbg->debuggees.lookup(&referent->global()));
    return true;
}

JSBool
Debugger::getDebuggees(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGGER(cx, argc, vp, "getDebuggees", args, dbg);
    RootedObject arrobj(cx, NewDenseAllocatedArray(cx, dbg->debuggees.count(), NULL));
    if (!arrobj)
        return false;
    arrobj->ensureDenseArrayInitializedLength(cx, 0, dbg->debuggees.count());
    unsigned i = 0;
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
Debugger::getNewestFrame(JSContext *cx, unsigned argc, Value *vp)
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
Debugger::clearAllBreakpoints(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGGER(cx, argc, vp, "clearAllBreakpoints", args, dbg);
    for (GlobalObjectSet::Range r = dbg->debuggees.all(); !r.empty(); r.popFront())
        r.front()->compartment()->clearBreakpointsIn(cx->runtime->defaultFreeOp(), dbg, NULL);
    return true;
}

JSBool
Debugger::construct(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Check that the arguments, if any, are cross-compartment wrappers. */
    for (unsigned i = 0; i < argc; i++) {
        const Value &arg = args[i];
        if (!arg.isObject())
            return ReportObjectRequired(cx);
        JSObject *argobj = &arg.toObject();
        if (!IsCrossCompartmentWrapper(argobj)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CCW_REQUIRED, "Debugger");
            return false;
        }
    }

    /* Get Debugger.prototype. */
    Value v;
    if (!args.callee().getProperty(cx, cx->runtime->atomState.classPrototypeAtom, &v))
        return false;
    RootedObject proto(cx, &v.toObject());
    JS_ASSERT(proto->getClass() == &Debugger::jsclass);

    /*
     * Make the new Debugger object. Each one has a reference to
     * Debugger.{Frame,Object,Script}.prototype in reserved slots. The rest of
     * the reserved slots are for hooks; they default to undefined.
     */
    RootedObject obj(cx, NewObjectWithGivenProto(cx, &Debugger::jsclass, proto, NULL));
    if (!obj)
        return false;
    for (unsigned slot = JSSLOT_DEBUG_PROTO_START; slot < JSSLOT_DEBUG_PROTO_STOP; slot++)
        obj->setReservedSlot(slot, proto->getReservedSlot(slot));

    Debugger *dbg = cx->new_<Debugger>(cx, obj.reference());
    if (!dbg)
        return false;
    obj->setPrivate(dbg);
    if (!dbg->init(cx)) {
        cx->delete_(dbg);
        return false;
    }

    /* Add the initial debuggees, if any. */
    for (unsigned i = 0; i < argc; i++) {
        Rooted<GlobalObject*> debuggee(cx, &GetProxyPrivate(&args[i].toObject()).toObject().global());
        if (!dbg->addDebuggeeGlobal(cx, debuggee))
            return false;
    }

    args.rval().setObject(*obj);
    return true;
}

bool
Debugger::addDebuggeeGlobal(JSContext *cx, Handle<GlobalObject*> global)
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
    if (!debuggeeCompartment->debugMode() && debuggeeCompartment->hasScriptsOnStack()) {
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
    GlobalObject::DebuggerVector *v = GlobalObject::getOrCreateDebuggers(cx, global);
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
Debugger::removeDebuggeeGlobal(FreeOp *fop, GlobalObject *global,
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
     * have live Frame objects. So we take the easy way out and kill them here.
     * This is a bug, since it's observable and contrary to the spec. One
     * possible fix would be to put such objects into a compartment-wide bag
     * which slowPathOnLeaveFrame would have to examine.
     */
    for (FrameMap::Enum e(frames); !e.empty(); e.popFront()) {
        StackFrame *fp = e.front().key;
        if (&fp->global() == global) {
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
        global->compartment()->removeDebuggee(fop, global, compartmentEnum);
    if (debugEnum)
        debugEnum->removeFront();
    else
        debuggees.remove(global);
}

/*
 * A class for parsing 'findScripts' query arguments and searching for
 * scripts that match the criteria they represent.
 */
class Debugger::ScriptQuery {
  public:
    /* Construct a ScriptQuery to use matching scripts for |dbg|. */
    ScriptQuery(JSContext *cx, Debugger *dbg):
        cx(cx), debugger(dbg), compartments(cx), innermostForGlobal(cx) {}

    /*
     * Initialize this ScriptQuery. Raise an error and return false if we
     * haven't enough memory.
     */
    bool init() {
        if (!globals.init() ||
            !compartments.init() ||
            !innermostForGlobal.init())
        {
            js_ReportOutOfMemory(cx);
            return false;
        }

        return true;
    }

    /*
     * Parse the query object |query|, and prepare to match only the scripts
     * it specifies.
     */
    bool parseQuery(JSObject *query) {
        /*
         * Check for a 'global' property, which limits the results to those
         * scripts scoped to a particular global object.
         */
        Value global;
        if (!query->getProperty(cx, cx->runtime->atomState.globalAtom, &global))
            return false;
        if (global.isUndefined()) {
            matchAllDebuggeeGlobals();
        } else {
            JSObject *referent = debugger->unwrapDebuggeeArgument(cx, global);
            if (!referent)
                return false;
            GlobalObject *globalObject = &referent->global();

            /*
             * If the given global isn't a debuggee, just leave the set of
             * acceptable globals empty; we'll return no scripts.
             */
            if (debugger->debuggees.has(globalObject)) {
                if (!matchSingleGlobal(globalObject))
                    return false;
            }
        }

        /* Check for a 'url' property. */
        if (!query->getProperty(cx, cx->runtime->atomState.urlAtom, &url))
            return false;
        if (!url.isUndefined() && !url.isString()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_UNEXPECTED_TYPE,
                                 "query object's 'url' property", "neither undefined nor a string");
            return false;
        }

        /* Check for a 'line' property. */
        Value lineProperty;
        if (!query->getProperty(cx, cx->runtime->atomState.lineAtom, &lineProperty))
            return false;
        if (lineProperty.isUndefined()) {
            hasLine = false;
        } else if (lineProperty.isNumber()) {
            if (url.isUndefined()) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_QUERY_LINE_WITHOUT_URL);
                return false;
            }
            double doubleLine = lineProperty.toNumber();
            if (doubleLine <= 0 || (unsigned int) doubleLine != doubleLine) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_BAD_LINE);
                return false;
            }
            hasLine = true;
            line = doubleLine;
        } else {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_UNEXPECTED_TYPE,
                                 "query object's 'line' property",
                                 "neither undefined nor an integer");
            return false;
        }

        /* Check for an 'innermost' property. */
        Value innermostProperty;
        if (!query->getProperty(cx, cx->runtime->atomState.innermostAtom, &innermostProperty))
            return false;
        innermost = js_ValueToBoolean(innermostProperty);
        if (innermost) {
            /* Technically, we need only check hasLine, but this is clearer. */
            if (url.isUndefined() || !hasLine) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_QUERY_INNERMOST_WITHOUT_LINE_URL);
                return false;
            }
        }

        return true;
    }

    /* Set up this ScriptQuery appropriately for a missing query argument. */
    bool omittedQuery() {
        url.setUndefined();
        hasLine = false;
        innermost = false;
        return matchAllDebuggeeGlobals();
    }

    /*
     * Search all relevant compartments and the stack for scripts matching
     * this query, and append the matching scripts to |vector|.
     */
    bool findScripts(AutoScriptVector *vector) {
        if (!prepareQuery())
            return false;

        /* Search each compartment for debuggee scripts. */
        for (CompartmentSet::Range r = compartments.all(); !r.empty(); r.popFront()) {
            for (gc::CellIter i(r.front(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
                JSScript *script = i.get<JSScript>();
                GlobalObject *global = script->getGlobalObjectOrNull();
                if (global && !consider(script, global, vector))
                    return false;
            }
        }

        /*
         * Since eval scripts have no global, we need to find them via the call
         * stack, where frame's scope tells us the global in use.
         */
        for (ScriptFrameIter fri(cx); !fri.done(); ++fri) {
            if (fri.isEvalFrame()) {
                JSScript *script = fri.script();

                /*
                 * If eval scripts never have global objects set, then we don't need
                 * to check the existing script vector for duplicates, since we only
                 * include scripts with globals above.
                 */
                JS_ASSERT(!script->getGlobalObjectOrNull());

                GlobalObject *global = &fri.fp()->global();
                if (!consider(script, global, vector))
                    return false;
            }
        }

        /*
         * For most queries, we just accumulate results in 'vector' as we find
         * them. But if this is an 'innermost' query, then we've accumulated the
         * results in the 'innermostForGlobal' map. In that case, we now need to
         * walk that map and populate 'vector'.
         */
        if (innermost) {
            for (GlobalToScriptMap::Range r = innermostForGlobal.all(); !r.empty(); r.popFront()) {
                if (!vector->append(r.front().value)) {
                    js_ReportOutOfMemory(cx);
                    return false;
                }
            }
        }

        return true;
    }

  private:
    /* The context in which we should do our work. */
    JSContext *cx;

    /* The debugger for which we conduct queries. */
    Debugger *debugger;

    /* A script must run in one of these globals to match the query. */
    GlobalObjectSet globals;

    typedef HashSet<JSCompartment *, DefaultHasher<JSCompartment *>, RuntimeAllocPolicy>
        CompartmentSet;

    /* The smallest set of compartments that contains all globals in globals. */
    CompartmentSet compartments;

    /* If this is a string, matching scripts have urls equal to it. */
    Value url;

    /* url as a C string. */
    JSAutoByteString urlCString;

    /* True if the query contained a 'line' property. */
    bool hasLine;

    /* The line matching scripts must cover. */
    unsigned int line;

    /* True if the query has an 'innermost' property whose value is true. */
    bool innermost;

    typedef HashMap<GlobalObject *, JSScript *, DefaultHasher<GlobalObject *>, RuntimeAllocPolicy>
        GlobalToScriptMap;

    /*
     * For 'innermost' queries, a map from global objects to the innermost
     * script we've seen so far in that global. (Instantiation code size
     * explosion ho!)
     */
    GlobalToScriptMap innermostForGlobal;

    /* Arrange for this ScriptQuery to match only scripts that run in |global|. */
    bool matchSingleGlobal(GlobalObject *global) {
        JS_ASSERT(globals.count() == 0);
        if (!globals.put(global)) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        return true;
    }

    /*
     * Arrange for this ScriptQuery to match all scripts running in debuggee
     * globals.
     */
    bool matchAllDebuggeeGlobals() {
        JS_ASSERT(globals.count() == 0);
        /* Copy the debugger's set of debuggee globals to our global set. */
        for (GlobalObjectSet::Range r = debugger->debuggees.all(); !r.empty(); r.popFront()) {
            if (!globals.put(r.front())) {
                js_ReportOutOfMemory(cx);
                return false;
            }
        }
        return true;
    }

    /*
     * Given that parseQuery or omittedQuery has been called, prepare to
     * match scripts. Set urlCString as appropriate.
     */
    bool prepareQuery() {
        /*
         * Compute the proper value for |compartments|, given the present
         * value of |globals|.
         */
        for (GlobalObjectSet::Range r = globals.all(); !r.empty(); r.popFront()) {
            if (!compartments.put(r.front()->compartment())) {
                js_ReportOutOfMemory(cx);
                return false;
            }
        }

        /* Compute urlCString, if a url was given. */
        if (url.isString()) {
            if (!urlCString.encode(cx, url.toString()))
                return false;
        }

        return true;
    }

    /*
     * If |script|, a script in |global|, matches this query, append it to
     * |vector| or place it in |innermostForGlobal|, as appropriate. Return true
     * if no error occurs, false if an error occurs.
     */
    bool consider(JSScript *script, GlobalObject *global, AutoScriptVector *vector) {
        if (!globals.has(global))
            return true;
        if (urlCString.ptr()) {
            if (!script->filename || strcmp(script->filename, urlCString.ptr()) != 0)
                return true;
        }
        if (hasLine) {
            if (line < script->lineno || script->lineno + js_GetScriptLineExtent(script) < line)
                return true;
        }

        if (innermost) {
            /*
             * For 'innermost' queries, we don't place scripts in |vector| right
             * away; we may later find another script that is nested inside this
             * one. Instead, we record the innermost script we've found so far
             * for each global in innermostForGlobal, and only populate |vector|
             * at the bottom of findScripts, when we've traversed all the
             * scripts.
             *
             * So: check this script against the innermost one we've found so
             * far (if any), as recorded in innermostForGlobal, and replace that
             * if it's better.
             */
            GlobalToScriptMap::AddPtr p = innermostForGlobal.lookupForAdd(global);
            if (p) {
                /* Is our newly found script deeper than the last one we found? */
                JSScript *incumbent = p->value;
                if (script->staticLevel > incumbent->staticLevel)
                    p->value = script;
            } else {
                /*
                 * This is the first matching script we've encountered for this
                 * global, so it is thus the innermost such script.
                 */
                if (!innermostForGlobal.add(p, global, script)) {
                    js_ReportOutOfMemory(cx);
                    return false;
                }
            }
        } else {
            /* Record this matching script in the results vector. */
            if (!vector->append(script)) {
                js_ReportOutOfMemory(cx);
                return false;
            }
        }

        return true;
    }
};

JSBool
Debugger::findScripts(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGGER(cx, argc, vp, "findScripts", args, dbg);

    ScriptQuery query(cx, dbg);
    if (!query.init())
        return false;

    if (argc >= 1) {
        JSObject *queryObject = NonNullObject(cx, args[0]);
        if (!queryObject || !query.parseQuery(queryObject))
            return false;
    } else {
        if (!query.omittedQuery())
            return false;
    }

    /*
     * Accumulate the scripts in an AutoScriptVector, instead of creating
     * the JS array as we go, because we mustn't allocate JS objects or GC
     * while we use the CellIter.
     */
    AutoScriptVector scripts(cx);

    if (!query.findScripts(&scripts))
        return false;

    RootedObject result(cx, NewDenseAllocatedArray(cx, scripts.length(), NULL));
    if (!result)
        return false;

    result->ensureDenseArrayInitializedLength(cx, 0, scripts.length());

    for (size_t i = 0; i < scripts.length(); i++) {
        JSObject *scriptObject = dbg->wrapScript(cx, Rooted<JSScript*>(cx, scripts[i]));
        if (!scriptObject)
            return false;
        result->setDenseArrayElement(i, ObjectValue(*scriptObject));
    }

    args.rval().setObject(*result);
    return true;
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
    JS_FN("findScripts", Debugger::findScripts, 1, 0),
    JS_FS_END
};


/*** Debugger.Script *****************************************************************************/

static inline JSScript *
GetScriptReferent(JSObject *obj)
{
    JS_ASSERT(obj->getClass() == &DebuggerScript_class);
    return static_cast<JSScript *>(obj->getPrivate());
}

static inline void
SetScriptReferent(JSObject *obj, JSScript *script)
{
    JS_ASSERT(obj->getClass() == &DebuggerScript_class);
    obj->setPrivate(script);
}

static void
DebuggerScript_trace(JSTracer *trc, JSObject *obj)
{
    /* This comes from a private pointer, so no barrier needed. */
    if (JSScript *script = GetScriptReferent(obj)) {
        MarkCrossCompartmentScriptUnbarriered(trc, &script, "Debugger.Script referent");
        obj->setPrivateUnbarriered(script);
    }
}

Class DebuggerScript_class = {
    "Script",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGSCRIPT_COUNT),
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* hasInstance */
    DebuggerScript_trace
};

JSObject *
Debugger::newDebuggerScript(JSContext *cx, Handle<JSScript*> script)
{
    assertSameCompartment(cx, object.get());

    JSObject *proto = &object->getReservedSlot(JSSLOT_DEBUG_SCRIPT_PROTO).toObject();
    JS_ASSERT(proto);
    JSObject *scriptobj = NewObjectWithGivenProto(cx, &DebuggerScript_class, proto, NULL);
    if (!scriptobj)
        return NULL;
    scriptobj->setReservedSlot(JSSLOT_DEBUGSCRIPT_OWNER, ObjectValue(*object));
    scriptobj->setPrivate(script);

    return scriptobj;
}

JSObject *
Debugger::wrapScript(JSContext *cx, Handle<JSScript*> script)
{
    assertSameCompartment(cx, object.get());
    JS_ASSERT(cx->compartment != script->compartment());
    ScriptWeakMap::AddPtr p = scripts.lookupForAdd(script);
    if (!p) {
        JSObject *scriptobj = newDebuggerScript(cx, script);

        /* The allocation may have caused a GC, which can remove table entries. */
        if (!scriptobj || !scripts.relookupOrAdd(p, script.value(), scriptobj))
            return NULL;
    }

    JS_ASSERT(GetScriptReferent(p->value) == script);
    return p->value;
}

static JSObject *
DebuggerScript_check(JSContext *cx, const Value &v, const char *clsname, const char *fnname)
{
    if (!v.isObject()) {
        ReportObjectRequired(cx);
        return NULL;
    }
    JSObject *thisobj = &v.toObject();
    if (thisobj->getClass() != &DebuggerScript_class) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             clsname, fnname, thisobj->getClass()->name);
        return NULL;
    }

    /*
     * Check for Debugger.Script.prototype, which is of class DebuggerScript_class
     * but whose script is null.
     */
    if (!GetScriptReferent(thisobj)) {
        JS_ASSERT(!GetScriptReferent(thisobj));
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             clsname, fnname, "prototype object");
        return NULL;
    }

    return thisobj;
}

static JSObject *
DebuggerScript_checkThis(JSContext *cx, const CallArgs &args, const char *fnname)
{
    return DebuggerScript_check(cx, args.thisv(), "Debugger.Script", fnname);
}

#define THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, fnname, args, obj, script)            \
    CallArgs args = CallArgsFromVp(argc, vp);                                       \
    RootedObject obj(cx, DebuggerScript_checkThis(cx, args, fnname));               \
    if (!obj)                                                                       \
        return false;                                                               \
    Rooted<JSScript*> script(cx, GetScriptReferent(obj))

static JSBool
DebuggerScript_getUrl(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, "(get url)", args, obj, script);

    if (script->filename) {
        JSString *str = js_NewStringCopyZ(cx, script->filename);
        if (!str)
            return false;
        args.rval().setString(str);
    } else {
        args.rval().setNull();
    }
    return true;
}

static JSBool
DebuggerScript_getStartLine(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, "(get startLine)", args, obj, script);
    args.rval().setNumber(script->lineno);
    return true;
}

static JSBool
DebuggerScript_getLineCount(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, "(get lineCount)", args, obj, script);

    unsigned maxLine = js_GetScriptLineExtent(script);
    args.rval().setNumber(double(maxLine));
    return true;
}

static JSBool
DebuggerScript_getStaticLevel(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, "(get staticLevel)", args, obj, script);
    args.rval().setNumber(uint32_t(script->staticLevel));
    return true;
}

static JSBool
DebuggerScript_getChildScripts(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, "getChildScripts", args, obj, script);
    Debugger *dbg = Debugger::fromChildJSObject(obj);

    RootedObject result(cx, NewDenseEmptyArray(cx));
    if (!result)
        return false;
    if (script->hasObjects()) {
        /*
         * script->savedCallerFun indicates that this is a direct eval script
         * and the calling function is stored as script->objects()->vector[0].
         * It is not really a child script of this script, so skip it.
         */
        ObjectArray *objects = script->objects();
        for (uint32_t i = script->savedCallerFun ? 1 : 0; i < objects->length; i++) {
            JSObject *obj = objects->vector[i];
            if (obj->isFunction()) {
                JSFunction *fun = static_cast<JSFunction *>(obj);
                JSObject *s = dbg->wrapScript(cx, Rooted<JSScript*>(cx, fun->script()));
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
DebuggerScript_getOffsetLine(JSContext *cx, unsigned argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.Script.getOffsetLine", 1);
    THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, "getOffsetLine", args, obj, script);
    size_t offset;
    if (!ScriptOffset(cx, script, args[0], &offset))
        return false;
    unsigned lineno = JS_PCToLineNumber(cx, script, script->code + offset);
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

    BytecodeRangeWithLineNumbers(JSScript *script)
      : BytecodeRange(script), lineno(script->lineno), sn(script->notes()), snpc(script->code)
    {
        if (!SN_IS_TERMINATOR(sn))
            snpc += SN_DELTA(sn);
        updateLine();
        while (frontPC() != script->main())
            popFront();
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
            SrcNoteType type = (SrcNoteType) SN_TYPE(sn);
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
        unsigned mainOffset = script->main() - script->code;
        self[mainOffset] = MultipleEdges;
        for (size_t i = mainOffset + 1; i < script->length; i++)
            self[i] = NoEdges;

        size_t prevLine = script->lineno;
        JSOp prevOp = JSOP_NOP;
        for (BytecodeRangeWithLineNumbers r(script); !r.empty(); r.popFront()) {
            size_t lineno = r.frontLineNumber();
            JSOp op = r.frontOpcode();

            if (FlowsIntoNext(prevOp))
                addEdge(prevLine, r.frontOffset());

            if (js_CodeSpec[op].type() == JOF_JUMP) {
                addEdge(lineno, r.frontOffset() + GET_JUMP_OFFSET(r.frontPC()));
            } else if (op == JSOP_TABLESWITCH || op == JSOP_LOOKUPSWITCH) {
                jsbytecode *pc = r.frontPC();
                size_t offset = r.frontOffset();
                ptrdiff_t step = JUMP_OFFSET_LEN;
                size_t defaultOffset = offset + GET_JUMP_OFFSET(pc);
                pc += step;
                addEdge(lineno, defaultOffset);

                int ncases;
                if (op == JSOP_TABLESWITCH) {
                    int32_t low = GET_JUMP_OFFSET(pc);
                    pc += JUMP_OFFSET_LEN;
                    ncases = GET_JUMP_OFFSET(pc) - low + 1;
                    pc += JUMP_OFFSET_LEN;
                } else {
                    ncases = GET_UINT16(pc);
                    pc += UINT16_LEN;
                    JS_ASSERT(ncases > 0);
                }

                for (int i = 0; i < ncases; i++) {
                    if (op == JSOP_LOOKUPSWITCH)
                        pc += UINT32_INDEX_LEN;
                    size_t target = offset + GET_JUMP_OFFSET(pc);
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
DebuggerScript_getAllOffsets(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, "getAllOffsets", args, obj, script);

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
    for (BytecodeRangeWithLineNumbers r(script); !r.empty(); r.popFront()) {
        size_t offset = r.frontOffset();
        size_t lineno = r.frontLineNumber();

        /* Make a note, if the current instruction is an entry point for the current line. */
        if (flowData[offset] != NoEdges && flowData[offset] != lineno) {
            /* Get the offsets array for this line. */
            RootedObject offsets(cx);
            Value offsetsv;
            if (!result->arrayGetOwnDataElement(cx, lineno, &offsetsv))
                return false;

            if (offsetsv.isObject()) {
                offsets = &offsetsv.toObject();
            } else {
                JS_ASSERT(offsetsv.isMagic(JS_ARRAY_HOLE));

                /*
                 * Create an empty offsets array for this line.
                 * Store it in the result array.
                 */
                RootedId id(cx);
                offsets = NewDenseEmptyArray(cx);
                if (!offsets ||
                    !ValueToId(cx, NumberValue(lineno), id.address()) ||
                    !result->defineGeneric(cx, id, ObjectValue(*offsets)))
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
DebuggerScript_getLineOffsets(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, "getLineOffsets", args, obj, script);
    REQUIRE_ARGC("Debugger.Script.getLineOffsets", 1);

    /* Parse lineno argument. */
    size_t lineno;
    bool ok = false;
    if (args[0].isNumber()) {
        double d = args[0].toNumber();
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
    RootedObject result(cx, NewDenseEmptyArray(cx));
    if (!result)
        return false;
    for (BytecodeRangeWithLineNumbers r(script); !r.empty(); r.popFront()) {
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

bool
Debugger::observesScript(JSScript *script) const
{
    if (!enabled)
        return false;

    /* Does the script have a global stored in it? */
    if (GlobalObject *global = script->getGlobalObjectOrNull())
        return observesGlobal(global);

    /* Is the script in a compartment this Debugger is debugging? */
    JSCompartment *comp = script->compartment();
    for (GlobalObjectSet::Range r = debuggees.all(); !r.empty(); r.popFront()) {
        if (r.front()->compartment() == comp)
            return true;
    }

    return false;
}

static JSBool
DebuggerScript_setBreakpoint(JSContext *cx, unsigned argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.Script.setBreakpoint", 2);
    THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, "setBreakpoint", args, obj, script);
    Debugger *dbg = Debugger::fromChildJSObject(obj);

    if (!dbg->observesScript(script)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_NOT_DEBUGGING);
        return false;
    }

    size_t offset;
    if (!ScriptOffset(cx, script, args[0], &offset))
        return false;

    JSObject *handler = NonNullObject(cx, args[1]);
    if (!handler)
        return false;

    jsbytecode *pc = script->code + offset;
    Rooted<GlobalObject *> scriptGlobal(cx, script->getGlobalObjectOrNull());
    BreakpointSite *site = script->getOrCreateBreakpointSite(cx, pc, scriptGlobal);
    if (!site)
        return false;
    site->inc(cx->runtime->defaultFreeOp());
    if (cx->runtime->new_<Breakpoint>(dbg, site, handler)) {
        args.rval().setUndefined();
        return true;
    }
    site->dec(cx->runtime->defaultFreeOp());
    site->destroyIfEmpty(cx->runtime->defaultFreeOp());
    return false;
}

static JSBool
DebuggerScript_getBreakpoints(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, "getBreakpoints", args, obj, script);
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

    RootedObject arr(cx, NewDenseEmptyArray(cx));
    if (!arr)
        return false;

    for (unsigned i = 0; i < script->length; i++) {
        BreakpointSite *site = script->getBreakpointSite(script->code + i);
        if (site && (!pc || site->pc == pc)) {
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
DebuggerScript_clearBreakpoint(JSContext *cx, unsigned argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.Script.clearBreakpoint", 1);
    THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, "clearBreakpoint", args, obj, script);
    Debugger *dbg = Debugger::fromChildJSObject(obj);

    JSObject *handler = NonNullObject(cx, args[0]);
    if (!handler)
        return false;

    script->clearBreakpointsIn(cx->runtime->defaultFreeOp(), dbg, handler);
    args.rval().setUndefined();
    return true;
}

static JSBool
DebuggerScript_clearAllBreakpoints(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGSCRIPT_SCRIPT(cx, argc, vp, "clearAllBreakpoints", args, obj, script);
    Debugger *dbg = Debugger::fromChildJSObject(obj);
    script->clearBreakpointsIn(cx->runtime->defaultFreeOp(), dbg, NULL);
    args.rval().setUndefined();
    return true;
}

static JSBool
DebuggerScript_construct(JSContext *cx, unsigned argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR, "Debugger.Script");
    return false;
}

static JSPropertySpec DebuggerScript_properties[] = {
    JS_PSG("url", DebuggerScript_getUrl, 0),
    JS_PSG("startLine", DebuggerScript_getStartLine, 0),
    JS_PSG("lineCount", DebuggerScript_getLineCount, 0),
    JS_PSG("staticLevel", DebuggerScript_getStaticLevel, 0),
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
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
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
                                 "Debugger.Frame");
            return NULL;
        }
    }
    return thisobj;
}

#define THIS_FRAME(cx, argc, vp, fnname, args, thisobj, fp)                  \
    CallArgs args = CallArgsFromVp(argc, vp);                                \
    RootedObject thisobj(cx, CheckThisFrame(cx, args, fnname, true));        \
    if (!thisobj)                                                            \
        return false;                                                        \
    StackFrame *fp = (StackFrame *) thisobj->getPrivate();                   \
    JS_ASSERT(cx->stack.space().containsSlow(fp))

#define THIS_FRAME_OWNER(cx, argc, vp, fnname, args, thisobj, fp, dbg)       \
    THIS_FRAME(cx, argc, vp, fnname, args, thisobj, fp);                     \
    Debugger *dbg = Debugger::fromChildJSObject(thisobj)

static JSBool
DebuggerFrame_getType(JSContext *cx, unsigned argc, Value *vp)
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
DebuggerFrame_getEnvironment(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_FRAME_OWNER(cx, argc, vp, "get environment", args, thisobj, fp, dbg);

    Rooted<Env*> env(cx);
    {
        AutoCompartment ac(cx, fp->scopeChain());
        if (!ac.enter())
            return false;
        env = GetDebugScopeForFrame(cx, fp);
        if (!env)
            return false;
    }

    return dbg->wrapEnvironment(cx, env, &args.rval());
}

static JSBool
DebuggerFrame_getCallee(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get callee", args, thisobj, fp);
    Value calleev = (fp->isFunctionFrame() && !fp->isEvalFrame()) ? fp->calleev() : NullValue();
    if (!Debugger::fromChildJSObject(thisobj)->wrapDebuggeeValue(cx, &calleev))
        return false;
    args.rval() = calleev;
    return true;
}

static JSBool
DebuggerFrame_getGenerator(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get generator", args, thisobj, fp);
    args.rval().setBoolean(fp->isGeneratorFrame());
    return true;
}

static JSBool
DebuggerFrame_getConstructing(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get constructing", args, thisobj, fp);
    args.rval().setBoolean(fp->isFunctionFrame() && fp->isConstructing());
    return true;
}

static JSBool
DebuggerFrame_getThis(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get this", args, thisobj, fp);
    Value thisv;
    {
        AutoCompartment ac(cx, fp->scopeChain());
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
DebuggerFrame_getOlder(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get this", args, thisobj, thisfp);
    Debugger *dbg = Debugger::fromChildJSObject(thisobj);
    for (StackFrame *fp = thisfp->prev(); fp; fp = fp->prev()) {
        if (dbg->observesFrame(fp))
            return dbg->getScriptFrame(cx, fp, vp);
    }
    args.rval().setNull();
    return true;
}

Class DebuggerArguments_class = {
    "Arguments", JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGARGUMENTS_COUNT),
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

/* The getter used for each element of frame.arguments. See DebuggerFrame_getArguments. */
static JSBool
DebuggerArguments_getArg(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    int32_t i = args.callee().toFunction()->getExtendedSlot(0).toInt32();

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
    if (unsigned(i) < fp->numActualArgs()) {
        if (unsigned(i) < fp->numFormalArgs() && fp->script()->formalLivesInCallObject(i))
            arg = fp->callObj().arg(i);
        else if (fp->script()->argsObjAliasesFormals())
            arg = fp->argsObj().arg(i);
        else
            arg = fp->unaliasedActual(i);
    } else {
        arg.setUndefined();
    }

    if (!Debugger::fromChildJSObject(thisobj)->wrapDebuggeeValue(cx, &arg))
        return false;
    args.rval() = arg;
    return true;
}

static JSBool
DebuggerFrame_getArguments(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get arguments", args, thisobj, fp);
    Value argumentsv = thisobj->getReservedSlot(JSSLOT_DEBUGFRAME_ARGUMENTS);
    if (!argumentsv.isUndefined()) {
        JS_ASSERT(argumentsv.isObjectOrNull());
        args.rval() = argumentsv;
        return true;
    }

    RootedObject argsobj(cx);
    if (fp->hasArgs()) {
        /* Create an arguments object. */
        Rooted<GlobalObject*> global(cx);
        global = &args.callee().global();
        JSObject *proto = global->getOrCreateArrayPrototype(cx);
        if (!proto)
            return false;
        argsobj = NewObjectWithGivenProto(cx, &DebuggerArguments_class, proto, global);
        if (!argsobj)
            return false;
        SetReservedSlot(argsobj, JSSLOT_DEBUGARGUMENTS_FRAME, ObjectValue(*thisobj));

        JS_ASSERT(fp->numActualArgs() <= 0x7fffffff);
        int32_t fargc = int32_t(fp->numActualArgs());
        if (!DefineNativeProperty(cx, argsobj, cx->runtime->atomState.lengthAtom,
                                  Int32Value(fargc), NULL, NULL,
                                  JSPROP_PERMANENT | JSPROP_READONLY, 0, 0))
        {
            return false;
        }

        for (int32_t i = 0; i < fargc; i++) {
            RootedFunction getobj(cx);
            getobj = js_NewFunction(cx, NULL, DebuggerArguments_getArg, 0, 0, global, NULL,
                                    JSFunction::ExtendedFinalizeKind);
            if (!getobj ||
                !DefineNativeProperty(cx, argsobj, RootedId(cx, INT_TO_JSID(i)), UndefinedValue(),
                                      JS_DATA_TO_FUNC_PTR(PropertyOp, getobj.reference()), NULL,
                                      JSPROP_ENUMERATE | JSPROP_SHARED | JSPROP_GETTER, 0, 0))
            {
                return false;
            }
            getobj->setExtendedSlot(0, Int32Value(i));
        }
    } else {
        argsobj = NULL;
    }
    args.rval() = ObjectOrNullValue(argsobj);
    thisobj->setReservedSlot(JSSLOT_DEBUGFRAME_ARGUMENTS, args.rval());
    return true;
}

static JSBool
DebuggerFrame_getScript(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get script", args, thisobj, fp);
    Debugger *debug = Debugger::fromChildJSObject(thisobj);

    JSObject *scriptObject = NULL;
    if (fp->isFunctionFrame() && !fp->isEvalFrame()) {
        JSFunction &callee = fp->callee();
        if (callee.isInterpreted()) {
            scriptObject = debug->wrapScript(cx, Rooted<JSScript*>(cx, callee.script()));
            if (!scriptObject)
                return false;
        }
    } else if (fp->isScriptFrame()) {
        /*
         * We got eval, JS_Evaluate*, or JS_ExecuteScript non-function script
         * frames.
         */
        JSScript *script = fp->script();
        scriptObject = debug->wrapScript(cx, Rooted<JSScript*>(cx, script));
        if (!scriptObject)
            return false;
    }
    args.rval().setObjectOrNull(scriptObject);
    return true;
}

static JSBool
DebuggerFrame_getOffset(JSContext *cx, unsigned argc, Value *vp)
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
DebuggerFrame_getLive(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *thisobj = CheckThisFrame(cx, args, "get live", false);
    if (!thisobj)
        return false;
    StackFrame *fp = (StackFrame *) thisobj->getPrivate();
    args.rval().setBoolean(!!fp);
    return true;
}

static bool
IsValidHook(const Value &v)
{
    return v.isUndefined() || (v.isObject() && v.toObject().isCallable());
}

static JSBool
DebuggerFrame_getOnStep(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get onStep", args, thisobj, fp);
    (void) fp;  // Silence GCC warning
    Value handler = thisobj->getReservedSlot(JSSLOT_DEBUGFRAME_ONSTEP_HANDLER);
    JS_ASSERT(IsValidHook(handler));
    args.rval() = handler;
    return true;
}

static JSBool
DebuggerFrame_setOnStep(JSContext *cx, unsigned argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.Frame.set onStep", 1);
    THIS_FRAME(cx, argc, vp, "set onStep", args, thisobj, fp);
    if (!fp->isScriptFrame()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_NOT_SCRIPT_FRAME);
        return false;
    }
    if (!IsValidHook(args[0])) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_CALLABLE_OR_UNDEFINED);
        return false;
    }

    Value prior = thisobj->getReservedSlot(JSSLOT_DEBUGFRAME_ONSTEP_HANDLER);
    int delta = !args[0].isUndefined() - !prior.isUndefined();
    if (delta != 0) {
        /* Try to adjust this frame's script single-step mode count. */
        AutoCompartment ac(cx, fp->scopeChain());
        if (!ac.enter())
            return false;
        if (!fp->script()->changeStepModeCount(cx, delta))
            return false;
    }

    /* Now that the step mode switch has succeeded, we can install the handler. */
    thisobj->setReservedSlot(JSSLOT_DEBUGFRAME_ONSTEP_HANDLER, args[0]);
    args.rval().setUndefined();
    return true;
}

static JSBool
DebuggerFrame_getOnPop(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_FRAME(cx, argc, vp, "get onPop", args, thisobj, fp);
    (void) fp;  // Silence GCC warning
    Value handler = thisobj->getReservedSlot(JSSLOT_DEBUGFRAME_ONPOP_HANDLER);
    JS_ASSERT(IsValidHook(handler));
    args.rval() = handler;
    return true;
}

static JSBool
DebuggerFrame_setOnPop(JSContext *cx, unsigned argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.Frame.set onPop", 1);
    THIS_FRAME(cx, argc, vp, "set onPop", args, thisobj, fp);
    if (!fp->isScriptFrame()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_NOT_SCRIPT_FRAME);
        return false;
    }
    if (!IsValidHook(args[0])) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_CALLABLE_OR_UNDEFINED);
        return false;
    }

    thisobj->setReservedSlot(JSSLOT_DEBUGFRAME_ONPOP_HANDLER, args[0]);
    args.rval().setUndefined();
    return true;
}

JSBool
js::EvaluateInEnv(JSContext *cx, Handle<Env*> env, StackFrame *fp, const jschar *chars,
                  unsigned length, const char *filename, unsigned lineno, Value *rval)
{
    assertSameCompartment(cx, env, fp);

    JS_ASSERT(!IsPoisonedPtr(chars));
    SkipRoot skip(cx, &chars);

    if (fp) {
        /* Execute assumes an already-computed 'this" value. */
        if (!ComputeThis(cx, fp))
            return false;
    }

    /*
     * NB: This function breaks the assumption that the compiler can see all
     * calls and properly compute a static level. In order to get around this,
     * we use a static level that will cause us not to attempt to optimize
     * variable references made by this frame.
     */
    JSPrincipals *prin = fp->scopeChain()->principals(cx);
    bool compileAndGo = true;
    bool noScriptRval = false;
    bool needScriptGlobal = true;
    JSScript *script = frontend::CompileScript(cx, env, fp, prin, prin,
                                               compileAndGo, noScriptRval, needScriptGlobal,
                                               chars, length, filename, lineno,
                                               cx->findVersion(), NULL,
                                               UpvarCookie::UPVAR_LEVEL_LIMIT);
    if (!script)
        return false;

    script->isActiveEval = true;
    return ExecuteKernel(cx, script, *env, fp->thisValue(), EXECUTE_DEBUG, fp, rval);
}

enum EvalBindingsMode { WithoutBindings, WithBindings };

static JSBool
DebuggerFrameEval(JSContext *cx, unsigned argc, Value *vp, EvalBindingsMode mode)
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
    Rooted<JSLinearString*> linearStr(cx, args[0].toString()->ensureLinear(cx));
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
        RootedObject bindingsobj(cx, NonNullObject(cx, args[1]));
        if (!bindingsobj ||
            !GetPropertyNames(cx, bindingsobj, JSITER_OWNONLY, &keys) ||
            !values.growBy(keys.length()))
        {
            return false;
        }
        for (size_t i = 0; i < keys.length(); i++) {
            Value *valp = &values[i];
            if (!bindingsobj->getGeneric(cx, bindingsobj, RootedId(cx, keys[i]), valp) ||
                !dbg->unwrapDebuggeeValue(cx, valp))
            {
                return false;
            }
        }
    }

    AutoCompartment ac(cx, fp->scopeChain());
    if (!ac.enter())
        return false;

    Rooted<Env *> env(cx, GetDebugScopeForFrame(cx, fp));
    if (!env)
        return false;

    /* If evalWithBindings, create the inner environment. */
    if (mode == WithBindings) {
        /* TODO - This should probably be a Call object, like ES5 strict eval. */
        env = NewObjectWithGivenProto(cx, &ObjectClass, NULL, env);
        if (!env)
            return false;
        RootedId id(cx);
        for (size_t i = 0; i < keys.length(); i++) {
            id = keys[i];
            if (!cx->compartment->wrap(cx, &values[i]) ||
                !DefineNativeProperty(cx, env, id, values[i], NULL, NULL, 0, 0, 0))
            {
                return false;
            }
        }
    }

    /* Run the code and produce the completion value. */
    Value rval;
    JS::Anchor<JSString *> anchor(linearStr);
    bool ok = EvaluateInEnv(cx, env, fp, linearStr->chars(), linearStr->length(),
                            "debugger eval code", 1, &rval);
    return dbg->receiveCompletionValue(ac, ok, rval, vp);
}

static JSBool
DebuggerFrame_eval(JSContext *cx, unsigned argc, Value *vp)
{
    return DebuggerFrameEval(cx, argc, vp, WithoutBindings);
}

static JSBool
DebuggerFrame_evalWithBindings(JSContext *cx, unsigned argc, Value *vp)
{
    return DebuggerFrameEval(cx, argc, vp, WithBindings);
}

static JSBool
DebuggerFrame_construct(JSContext *cx, unsigned argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR, "Debugger.Frame");
    return false;
}

static JSPropertySpec DebuggerFrame_properties[] = {
    JS_PSG("arguments", DebuggerFrame_getArguments, 0),
    JS_PSG("callee", DebuggerFrame_getCallee, 0),
    JS_PSG("constructing", DebuggerFrame_getConstructing, 0),
    JS_PSG("environment", DebuggerFrame_getEnvironment, 0),
    JS_PSG("generator", DebuggerFrame_getGenerator, 0),
    JS_PSG("live", DebuggerFrame_getLive, 0),
    JS_PSG("offset", DebuggerFrame_getOffset, 0),
    JS_PSG("older", DebuggerFrame_getOlder, 0),
    JS_PSG("script", DebuggerFrame_getScript, 0),
    JS_PSG("this", DebuggerFrame_getThis, 0),
    JS_PSG("type", DebuggerFrame_getType, 0),
    JS_PSGS("onStep", DebuggerFrame_getOnStep, DebuggerFrame_setOnStep, 0),
    JS_PSGS("onPop", DebuggerFrame_getOnPop, DebuggerFrame_setOnPop, 0),
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
    /*
     * There is a barrier on private pointers, so the Unbarriered marking
     * is okay.
     */
    if (JSObject *referent = (JSObject *) obj->getPrivate()) {
        MarkCrossCompartmentObjectUnbarriered(trc, &referent, "Debugger.Object referent");
        obj->setPrivateUnbarriered(referent);
    }
}

Class DebuggerObject_class = {
    "Object",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGOBJECT_COUNT),
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
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
    if (thisobj->getClass() != &DebuggerObject_class) {
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
    RootedObject obj(cx, DebuggerObject_checkThis(cx, args, fnname));         \
    if (!obj)                                                                 \
        return false;                                                         \
    obj = (JSObject *) obj->getPrivate();                                     \
    JS_ASSERT(obj)

#define THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, fnname, args, dbg, obj) \
    CallArgs args = CallArgsFromVp(argc, vp);                                 \
    RootedObject obj(cx, DebuggerObject_checkThis(cx, args, fnname));         \
    if (!obj)                                                                 \
        return false;                                                         \
    Debugger *dbg = Debugger::fromChildJSObject(obj);                         \
    obj = (JSObject *) obj->getPrivate();                                     \
    JS_ASSERT(obj)

static JSBool
DebuggerObject_construct(JSContext *cx, unsigned argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR, "Debugger.Object");
    return false;
}

static JSBool
DebuggerObject_getProto(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "get proto", args, dbg, refobj);
    Value protov = ObjectOrNullValue(refobj->getProto());
    if (!dbg->wrapDebuggeeValue(cx, &protov))
        return false;
    args.rval() = protov;
    return true;
}

static JSBool
DebuggerObject_getClass(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGOBJECT_REFERENT(cx, argc, vp, "get class", args, refobj);
    const char *s = refobj->getClass()->name;
    JSAtom *str = js_Atomize(cx, s, strlen(s));
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static JSBool
DebuggerObject_getCallable(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGOBJECT_REFERENT(cx, argc, vp, "get callable", args, refobj);
    args.rval().setBoolean(refobj->isCallable());
    return true;
}

static JSBool
DebuggerObject_getName(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "get name", args, dbg, obj);
    if (!obj->isFunction()) {
        args.rval().setUndefined();
        return true;
    }

    JSString *name = obj->toFunction()->atom;
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
DebuggerObject_getParameterNames(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGOBJECT_REFERENT(cx, argc, vp, "get parameterNames", args, obj);
    if (!obj->isFunction()) {
        args.rval().setUndefined();
        return true;
    }

    RootedFunction fun(cx, obj->toFunction());
    JSObject *result = NewDenseAllocatedArray(cx, fun->nargs, NULL);
    if (!result)
        return false;
    result->ensureDenseArrayInitializedLength(cx, 0, fun->nargs);

    if (fun->isInterpreted()) {
        JS_ASSERT(fun->nargs == fun->script()->bindings.numArgs());

        if (fun->nargs > 0) {
            BindingNames names(cx);
            if (!fun->script()->bindings.getLocalNameArray(cx, &names))
                return false;

            for (size_t i = 0; i < fun->nargs; i++) {
                JSAtom *name = names[i].maybeAtom;
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
DebuggerObject_getScript(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "get script", args, dbg, obj);

    if (!obj->isFunction()) {
        args.rval().setUndefined();
        return true;
    }

    JSFunction *fun = obj->toFunction();
    if (!fun->isInterpreted()) {
        args.rval().setUndefined();
        return true;
    }

    JSObject *scriptObject = dbg->wrapScript(cx, Rooted<JSScript*>(cx, fun->script()));
    if (!scriptObject)
        return false;

    args.rval().setObject(*scriptObject);
    return true;
}

static JSBool
DebuggerObject_getEnvironment(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "get environment", args, dbg, obj);

    /* Don't bother switching compartments just to check obj's type and get its env. */
    if (!obj->isFunction() || !obj->toFunction()->isInterpreted()) {
        args.rval().setUndefined();
        return true;
    }

    Rooted<Env*> env(cx);
    {
        AutoCompartment ac(cx, obj);
        if (!ac.enter())
            return false;

        env = GetDebugScopeForFunction(cx, obj->toFunction());
        if (!env)
            return false;
    }

    return dbg->wrapEnvironment(cx, env, &args.rval());
}

static JSBool
DebuggerObject_getOwnPropertyDescriptor(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "getOwnPropertyDescriptor", args, dbg, obj);

    RootedId id(cx);
    if (!ValueToId(cx, argc >= 1 ? args[0] : UndefinedValue(), id.address()))
        return false;

    /* Bug: This can cause the debuggee to run! */
    AutoPropertyDescriptorRooter desc(cx);
    {
        AutoCompartment ac(cx, obj);
        if (!ac.enter() || !cx->compartment->wrapId(cx, id.address()))
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
DebuggerObject_getOwnPropertyNames(JSContext *cx, unsigned argc, Value *vp)
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
             JSString *str = js_IntToString(cx, JSID_TO_INT(id));
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

static JSBool
DebuggerObject_defineProperty(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "defineProperty", args, dbg, obj);
    REQUIRE_ARGC("Debugger.Object.defineProperty", 2);

    RootedId id(cx);
    if (!ValueToId(cx, args[0], id.address()))
        return false;

    const Value &descval = args[1];
    AutoPropDescArrayRooter descs(cx);
    if (!descs.reserve(3)) // desc, unwrappedDesc, rewrappedDesc
        return false;
    PropDesc *desc = descs.append();
    if (!desc || !desc->initialize(cx, descval, false))
        return false;
    desc->clearPd();

    PropDesc *unwrappedDesc = descs.append();
    if (!unwrappedDesc || !desc->unwrapDebuggerObjectsInto(cx, dbg, obj, unwrappedDesc))
        return false;

    {
        PropDesc *rewrappedDesc = descs.append();
        if (!rewrappedDesc)
            return false;
        RootedId wrappedId(cx);

        AutoCompartment ac(cx, obj);
        if (!ac.enter() || !unwrappedDesc->wrapInto(cx, obj, id, wrappedId.address(), rewrappedDesc))
            return false;

        ErrorCopier ec(ac, dbg->toJSObject());
        bool dummy;
        if (!DefineProperty(cx, obj, wrappedId, *rewrappedDesc, true, &dummy))
            return false;
    }

    args.rval().setUndefined();
    return true;
}

static JSBool
DebuggerObject_defineProperties(JSContext *cx, unsigned argc, Value *vp)
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

    AutoPropDescArrayRooter unwrappedDescs(cx);
    for (size_t i = 0; i < n; i++) {
        if (!unwrappedDescs.append())
            return false;
        if (!descs[i].unwrapDebuggerObjectsInto(cx, dbg, obj, &unwrappedDescs[i]))
            return false;
    }

    {
        AutoIdVector rewrappedIds(cx);
        AutoPropDescArrayRooter rewrappedDescs(cx);

        AutoCompartment ac(cx, obj);
        if (!ac.enter())
            return false;
        for (size_t i = 0; i < n; i++) {
            if (!rewrappedIds.append(jsid()) || !rewrappedDescs.append())
                return false;
            if (!unwrappedDescs[i].wrapInto(cx, obj, ids[i], &rewrappedIds[i], &rewrappedDescs[i]))
                return false;
        }

        ErrorCopier ec(ac, dbg->toJSObject());
        for (size_t i = 0; i < n; i++) {
            bool dummy;
            if (!DefineProperty(cx, obj, RootedId(cx, rewrappedIds[i]), rewrappedDescs[i], true, &dummy))
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
DebuggerObject_deleteProperty(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "deleteProperty", args, dbg, obj);
    Value nameArg = argc > 0 ? args[0] : UndefinedValue();

    AutoCompartment ac(cx, obj);
    if (!ac.enter() || !cx->compartment->wrap(cx, &nameArg))
        return false;

    ErrorCopier ec(ac, dbg->toJSObject());
    return obj->deleteByValue(cx, nameArg, &args.rval(), false);
}

enum SealHelperOp { Seal, Freeze, PreventExtensions };

static JSBool
DebuggerObject_sealHelper(JSContext *cx, unsigned argc, Value *vp, SealHelperOp op, const char *name)
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
        ok = obj->preventExtensions(cx);
    }
    if (!ok)
        return false;
    args.rval().setUndefined();
    return true;
}

static JSBool
DebuggerObject_seal(JSContext *cx, unsigned argc, Value *vp)
{
    return DebuggerObject_sealHelper(cx, argc, vp, Seal, "seal");
}

static JSBool
DebuggerObject_freeze(JSContext *cx, unsigned argc, Value *vp)
{
    return DebuggerObject_sealHelper(cx, argc, vp, Freeze, "freeze");
}

static JSBool
DebuggerObject_preventExtensions(JSContext *cx, unsigned argc, Value *vp)
{
    return DebuggerObject_sealHelper(cx, argc, vp, PreventExtensions, "preventExtensions");
}

static JSBool
DebuggerObject_isSealedHelper(JSContext *cx, unsigned argc, Value *vp, SealHelperOp op,
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
DebuggerObject_isSealed(JSContext *cx, unsigned argc, Value *vp)
{
    return DebuggerObject_isSealedHelper(cx, argc, vp, Seal, "isSealed");
}

static JSBool
DebuggerObject_isFrozen(JSContext *cx, unsigned argc, Value *vp)
{
    return DebuggerObject_isSealedHelper(cx, argc, vp, Freeze, "isFrozen");
}

static JSBool
DebuggerObject_isExtensible(JSContext *cx, unsigned argc, Value *vp)
{
    return DebuggerObject_isSealedHelper(cx, argc, vp, PreventExtensions, "isExtensible");
}

enum ApplyOrCallMode { ApplyMode, CallMode };

static JSBool
ApplyOrCall(JSContext *cx, unsigned argc, Value *vp, ApplyOrCallMode mode)
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
    unsigned callArgc = 0;
    Value *callArgv = NULL;
    AutoValueVector argv(cx);
    if (mode == ApplyMode) {
        if (argc >= 2 && !args[1].isNullOrUndefined()) {
            if (!args[1].isObject()) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_APPLY_ARGS,
                                     js_apply_str);
                return false;
            }
            RootedObject argsobj(cx, &args[1].toObject());
            if (!js_GetLengthProperty(cx, argsobj, &callArgc))
                return false;
            callArgc = unsigned(JS_MIN(callArgc, StackSpace::ARGS_LENGTH_MAX));
            if (!argv.growBy(callArgc) || !GetElements(cx, argsobj, callArgc, argv.begin()))
                return false;
            callArgv = argv.begin();
        }
    } else {
        callArgc = argc > 0 ? unsigned(JS_MIN(argc - 1, StackSpace::ARGS_LENGTH_MAX)) : 0;
        callArgv = args.array() + 1;
    }
    for (unsigned i = 0; i < callArgc; i++) {
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
    for (unsigned i = 0; i < callArgc; i++) {
        if (!cx->compartment->wrap(cx, &callArgv[i]))
            return false;
    }

    /*
     * Call the function. Use receiveCompletionValue to return to the debugger
     * compartment and populate args.rval().
     */
    Value rval;
    bool ok = Invoke(cx, thisv, calleev, callArgc, callArgv, &rval);
    return dbg->receiveCompletionValue(ac, ok, rval, &args.rval());
}

static JSBool
DebuggerObject_apply(JSContext *cx, unsigned argc, Value *vp)
{
    return ApplyOrCall(cx, argc, vp, ApplyMode);
}

static JSBool
DebuggerObject_call(JSContext *cx, unsigned argc, Value *vp)
{
    return ApplyOrCall(cx, argc, vp, CallMode);
}

static JSBool
DebuggerObject_makeDebuggeeValue(JSContext *cx, unsigned argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.Object.prototype.makeDebuggeeValue", 1);
    THIS_DEBUGOBJECT_OWNER_REFERENT(cx, argc, vp, "makeDebuggeeValue", args, dbg, referent);

    /* Non-objects are already debuggee values. */
    if (args[0].isObject()) {
        // Enter this Debugger.Object's referent's compartment, and wrap the
        // argument as appropriate for references from there.
        {
            AutoCompartment ac(cx, referent);
            if (!ac.enter() ||
                !cx->compartment->wrap(cx, &args[0]))
                return false;
        }

        // Back in the debugger's compartment, produce a new Debugger.Object
        // instance referring to the wrapped argument.
        if (!dbg->wrapDebuggeeValue(cx, &args[0]))
            return false;
    }

    args.rval() = args[0];
    return true;
}

static JSPropertySpec DebuggerObject_properties[] = {
    JS_PSG("proto", DebuggerObject_getProto, 0),
    JS_PSG("class", DebuggerObject_getClass, 0),
    JS_PSG("callable", DebuggerObject_getCallable, 0),
    JS_PSG("name", DebuggerObject_getName, 0),
    JS_PSG("parameterNames", DebuggerObject_getParameterNames, 0),
    JS_PSG("script", DebuggerObject_getScript, 0),
    JS_PSG("environment", DebuggerObject_getEnvironment, 0),
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
    JS_FN("makeDebuggeeValue", DebuggerObject_makeDebuggeeValue, 1, 0),
    JS_FS_END
};


/*** Debugger.Environment ************************************************************************/

static void
DebuggerEnv_trace(JSTracer *trc, JSObject *obj)
{
    /*
     * There is a barrier on private pointers, so the Unbarriered marking
     * is okay.
     */
    if (Env *referent = (JSObject *) obj->getPrivate()) {
        MarkCrossCompartmentObjectUnbarriered(trc, &referent, "Debugger.Environment referent");
        obj->setPrivateUnbarriered(referent);
    }
}

Class DebuggerEnv_class = {
    "Environment",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGENV_COUNT),
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* hasInstance */
    DebuggerEnv_trace
};

static JSObject *
DebuggerEnv_checkThis(JSContext *cx, const CallArgs &args, const char *fnname)
{
    if (!args.thisv().isObject()) {
        ReportObjectRequired(cx);
        return NULL;
    }
    JSObject *thisobj = &args.thisv().toObject();
    if (thisobj->getClass() != &DebuggerEnv_class) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Debugger.Environment", fnname, thisobj->getClass()->name);
        return NULL;
    }

    /*
     * Forbid Debugger.Environment.prototype, which is of class DebuggerEnv_class
     * but isn't a real working Debugger.Environment. The prototype object is
     * distinguished by having no referent.
     */
    if (!thisobj->getPrivate()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Debugger.Environment", fnname, "prototype object");
        return NULL;
    }
    return thisobj;
}

#define THIS_DEBUGENV(cx, argc, vp, fnname, args, envobj, env)                \
    CallArgs args = CallArgsFromVp(argc, vp);                                 \
    JSObject *envobj = DebuggerEnv_checkThis(cx, args, fnname);               \
    if (!envobj)                                                              \
        return false;                                                         \
    Rooted<Env*> env(cx, static_cast<Env *>(envobj->getPrivate()));           \
    JS_ASSERT(env);                                                           \
    JS_ASSERT(!env->isScope())

#define THIS_DEBUGENV_OWNER(cx, argc, vp, fnname, args, envobj, env, dbg)     \
    THIS_DEBUGENV(cx, argc, vp, fnname, args, envobj, env);                   \
    Debugger *dbg = Debugger::fromChildJSObject(envobj)

static JSBool
DebuggerEnv_construct(JSContext *cx, unsigned argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR, "Debugger.Environment");
    return false;
}

static bool
IsDeclarative(Env *env)
{
    return env->isDebugScope() && env->asDebugScope().isForDeclarative();
}

static bool
IsWith(Env *env)
{
    return env->isDebugScope() && env->asDebugScope().scope().isWith();
}

static JSBool
DebuggerEnv_getType(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGENV(cx, argc, vp, "get type", args, envobj, env);

    /* Don't bother switching compartments just to check env's class. */
    const char *s;
    if (IsDeclarative(env))
        s = "declarative";
    else if (IsWith(env))
        s = "with";
    else
        s = "object";

    JSAtom *str = js_Atomize(cx, s, strlen(s), InternAtom, NormalEncoding);
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static JSBool
DebuggerEnv_getParent(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGENV_OWNER(cx, argc, vp, "get parent", args, envobj, env, dbg);

    /* Don't bother switching compartments just to get env's parent. */
    Rooted<Env*> parent(cx, env->enclosingScope());
    return dbg->wrapEnvironment(cx, parent, &args.rval());
}

static JSBool
DebuggerEnv_getObject(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGENV_OWNER(cx, argc, vp, "get type", args, envobj, env, dbg);

    /*
     * Don't bother switching compartments just to check env's class and
     * possibly get its proto.
     */
    if (IsDeclarative(env)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_NO_SCOPE_OBJECT);
        return false;
    }

    JSObject *obj;
    if (IsWith(env)) {
        obj = &env->asDebugScope().scope().asWith().object();
    } else {
        obj = env;
        JS_ASSERT(!obj->isDebugScope());
    }

    Value rval = ObjectValue(*obj);
    if (!dbg->wrapDebuggeeValue(cx, &rval))
        return false;
    args.rval() = rval;
    return true;
}

static JSBool
DebuggerEnv_getCallee(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGENV_OWNER(cx, argc, vp, "get callee", args, envobj, env, dbg);

    args.rval().setNull();

    if (!env->isDebugScope())
        return true;

    JSObject &scope = env->asDebugScope().scope();
    if (!scope.isCall())
        return true;

    JSObject *callee = scope.asCall().getCallee();
    if (!callee)
        return true;

    args.rval() = ObjectValue(*callee);
    if (!dbg->wrapDebuggeeValue(cx, &args.rval()))
        return false;
    return true;
}

static JSBool
DebuggerEnv_names(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_DEBUGENV_OWNER(cx, argc, vp, "names", args, envobj, env, dbg);

    AutoIdVector keys(cx);
    {
        AutoCompartment ac(cx, env);
        if (!ac.enter())
            return false;

        ErrorCopier ec(ac, dbg->toJSObject());
        if (!GetPropertyNames(cx, env, JSITER_HIDDEN, &keys))
            return false;
    }

    RootedObject arr(cx, NewDenseEmptyArray(cx));
    if (!arr)
        return false;
    for (size_t i = 0, len = keys.length(); i < len; i++) {
         jsid id = keys[i];
         if (JSID_IS_ATOM(id) && IsIdentifier(JSID_TO_ATOM(id))) {
             if (!cx->compartment->wrapId(cx, &id))
                 return false;
             if (!js_NewbornArrayPush(cx, arr, StringValue(JSID_TO_STRING(id))))
                 return false;
         }
    }
    args.rval().setObject(*arr);
    return true;
}

static JSBool
DebuggerEnv_find(JSContext *cx, unsigned argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.Environment.find", 1);
    THIS_DEBUGENV_OWNER(cx, argc, vp, "find", args, envobj, env, dbg);

    RootedId id(cx);
    if (!ValueToIdentifier(cx, args[0], id.address()))
        return false;

    {
        AutoCompartment ac(cx, env);
        if (!ac.enter() || !cx->compartment->wrapId(cx, id.address()))
            return false;

        /* This can trigger resolve hooks. */
        ErrorCopier ec(ac, dbg->toJSObject());
        JSProperty *prop = NULL;
        JSObject *pobj;
        for (; env && !prop; env = env->enclosingScope()) {
            if (!env->lookupGeneric(cx, id, &pobj, &prop))
                return false;
            if (prop)
                break;
        }
    }

    return dbg->wrapEnvironment(cx, env, &args.rval());
}

static JSBool
DebuggerEnv_getVariable(JSContext *cx, unsigned argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.Environment.getVariable", 1);
    THIS_DEBUGENV_OWNER(cx, argc, vp, "getVariable", args, envobj, env, dbg);

    RootedId id(cx);
    if (!ValueToIdentifier(cx, args[0], id.address()))
        return false;

    Value v;
    {
        AutoCompartment ac(cx, env);
        if (!ac.enter() || !cx->compartment->wrapId(cx, id.address()))
            return false;

        /* This can trigger getters. */
        ErrorCopier ec(ac, dbg->toJSObject());
        if (!env->getGeneric(cx, id, &v))
            return false;
    }

    if (!dbg->wrapDebuggeeValue(cx, &v))
        return false;
    args.rval() = v;
    return true;
}

static JSBool
DebuggerEnv_setVariable(JSContext *cx, unsigned argc, Value *vp)
{
    REQUIRE_ARGC("Debugger.Environment.setVariable", 2);
    THIS_DEBUGENV_OWNER(cx, argc, vp, "setVariable", args, envobj, env, dbg);

    RootedId id(cx);
    if (!ValueToIdentifier(cx, args[0], id.address()))
        return false;

    RootedValue v(cx, args[1]);
    if (!dbg->unwrapDebuggeeValue(cx, v.address()))
        return false;

    {
        AutoCompartment ac(cx, env);
        if (!ac.enter() ||
            !cx->compartment->wrapId(cx, id.address()) ||
            !cx->compartment->wrap(cx, v.address()))
        {
            return false;
        }

        /* This can trigger setters. */
        ErrorCopier ec(ac, dbg->toJSObject());

        /* Make sure the environment actually has the specified binding. */
        bool has;
        if (!env->hasProperty(cx, id, &has))
            return false;
        if (!has) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_VARIABLE_NOT_FOUND);
            return false;
        }

        /* Just set the property. */
        if (!env->setGeneric(cx, id, v.address(), true))
            return false;
    }

    args.rval().setUndefined();
    return true;
}

static JSPropertySpec DebuggerEnv_properties[] = {
    JS_PSG("type", DebuggerEnv_getType, 0),
    JS_PSG("object", DebuggerEnv_getObject, 0),
    JS_PSG("parent", DebuggerEnv_getParent, 0),
    JS_PSG("callee", DebuggerEnv_getCallee, 0),
    JS_PS_END
};

static JSFunctionSpec DebuggerEnv_methods[] = {
    JS_FN("names", DebuggerEnv_names, 0, 0),
    JS_FN("find", DebuggerEnv_find, 1, 0),
    JS_FN("getVariable", DebuggerEnv_getVariable, 1, 0),
    JS_FN("setVariable", DebuggerEnv_setVariable, 2, 0),
    JS_FS_END
};



/*** Glue ****************************************************************************************/

extern JS_PUBLIC_API(JSBool)
JS_DefineDebuggerObject(JSContext *cx, JSObject *obj_)
{
    RootedObject obj(cx, obj_);

    RootedObject
        objProto(cx),
        debugCtor(cx),
        debugProto(cx),
        frameProto(cx),
        scriptProto(cx),
        objectProto(cx);

    objProto = obj->asGlobal().getOrCreateObjectPrototype(cx);
    if (!objProto)
        return false;


    debugProto = js_InitClass(cx, obj,
                              objProto, &Debugger::jsclass, Debugger::construct,
                              1, Debugger::properties, Debugger::methods, NULL, NULL,
                              debugCtor.address());
    if (!debugProto)
        return false;

    frameProto = js_InitClass(cx, debugCtor, objProto, &DebuggerFrame_class,
                              DebuggerFrame_construct, 0,
                              DebuggerFrame_properties, DebuggerFrame_methods,
                              NULL, NULL);
    if (!frameProto)
        return false;

    scriptProto = js_InitClass(cx, debugCtor, objProto, &DebuggerScript_class,
                               DebuggerScript_construct, 0,
                               DebuggerScript_properties, DebuggerScript_methods,
                               NULL, NULL);
    if (!scriptProto)
        return false;

    objectProto = js_InitClass(cx, debugCtor, objProto, &DebuggerObject_class,
                               DebuggerObject_construct, 0,
                               DebuggerObject_properties, DebuggerObject_methods,
                               NULL, NULL);
    if (!objectProto)
        return false;

    JSObject *envProto = js_InitClass(cx, debugCtor, objProto, &DebuggerEnv_class,
                                      DebuggerEnv_construct, 0,
                                      DebuggerEnv_properties, DebuggerEnv_methods,
                                      NULL, NULL);
    if (!envProto)
        return false;

    debugProto->setReservedSlot(Debugger::JSSLOT_DEBUG_FRAME_PROTO, ObjectValue(*frameProto));
    debugProto->setReservedSlot(Debugger::JSSLOT_DEBUG_OBJECT_PROTO, ObjectValue(*objectProto));
    debugProto->setReservedSlot(Debugger::JSSLOT_DEBUG_SCRIPT_PROTO, ObjectValue(*scriptProto));
    debugProto->setReservedSlot(Debugger::JSSLOT_DEBUG_ENV_PROTO, ObjectValue(*envProto));
    return true;
}

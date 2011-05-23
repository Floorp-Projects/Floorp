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
#include "jsgcmark.h"
#include "jsobj.h"
#include "jswrapper.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "vm/Stack-inl.h"

using namespace js;

// === Forward declarations

extern Class DebugFrame_class;

enum {
    JSSLOT_DEBUGFRAME_OWNER,
    JSSLOT_DEBUGFRAME_ARGUMENTS,
    JSSLOT_DEBUGFRAME_COUNT
};

extern Class DebugObject_class;

enum {
    JSSLOT_DEBUGOBJECT_OWNER,
    JSSLOT_DEBUGOBJECT_CCW,  // cross-compartment wrapper
    JSSLOT_DEBUGOBJECT_COUNT
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
        if (argc < n) \
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

    // Check for e.g. Debug.prototype, which is of the Debug JSClass but isn't
    // really a Debug object.
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

// === Debug hook dispatch

enum {
    JSSLOT_DEBUG_FRAME_PROTO,
    JSSLOT_DEBUG_OBJECT_PROTO,
    JSSLOT_DEBUG_COUNT
};

Debug::Debug(JSObject *dbg, JSObject *hooks)
  : object(dbg), hooksObject(hooks), uncaughtExceptionHook(NULL), enabled(true),
    hasDebuggerHandler(false), hasThrowHandler(false)
{
    // This always happens within a request on some cx.
    JSRuntime *rt = dbg->compartment()->rt;
    AutoLockGC lock(rt);
    JS_APPEND_LINK(&link, &rt->debuggerList);
}

Debug::~Debug()
{
    JS_ASSERT(object->compartment()->rt->gcRunning);
    if (!debuggees.empty()) {
        // This happens only during per-compartment GC. See comment in
        // Debug::sweepAll.
        JS_ASSERT(object->compartment()->rt->gcCurrentCompartment == object->compartment());
        for (GlobalObjectSet::Enum e(debuggees); !e.empty(); e.popFront())
            removeDebuggeeGlobal(e.front(), NULL, &e);
    }

    // This always happens in the GC thread, so no locking is required.
    JS_REMOVE_LINK(&link);
}

bool
Debug::init(JSContext *cx)
{
    bool ok = frames.init() && objects.init() && debuggees.init();
    if (!ok)
        js_ReportOutOfMemory(cx);
    return ok;
}

JS_STATIC_ASSERT(uintN(JSSLOT_DEBUGFRAME_OWNER) == uintN(JSSLOT_DEBUGOBJECT_OWNER));

Debug *
Debug::fromChildJSObject(JSObject *obj)
{
    JS_ASSERT(obj->clasp == &DebugFrame_class || obj->clasp == &DebugObject_class);
    JSObject *dbgobj = &obj->getReservedSlot(JSSLOT_DEBUGOBJECT_OWNER).toObject();
    return fromJSObject(dbgobj);
}

bool
Debug::getScriptFrame(JSContext *cx, StackFrame *fp, Value *vp)
{
    JS_ASSERT(fp->isScriptFrame());
    FrameMap::AddPtr p = frames.lookupForAdd(fp);
    if (!p) {
        // Create script Debug.Frame. First copy the arguments.
        JSObject *argsobj;
        if (fp->hasArgs()) {
            uintN argc = fp->numActualArgs();
            JS_ASSERT(uint(argc) == argc);
            argsobj = NewDenseAllocatedArray(cx, uint(argc), NULL);
            Value *argv = fp->actualArgs();
            for (uintN i = 0; i < argc; i++) {
                Value v = argv[i];
                if (!wrapDebuggeeValue(cx, &v))
                    return false;
                argsobj->setDenseArrayElement(i, v);
            }
            if (!argsobj)
                return false;
        } else {
            argsobj = NULL;
        }

        // Now create and populate the Debug.Frame object.
        JSObject *proto = &object->getReservedSlot(JSSLOT_DEBUG_FRAME_PROTO).toObject();
        JSObject *frameobj = NewNonFunction<WithProto::Given>(cx, &DebugFrame_class, proto, NULL);
        if (!frameobj || !frameobj->ensureClassReservedSlots(cx))
            return false;
        frameobj->setPrivate(fp);
        frameobj->setReservedSlot(JSSLOT_DEBUGFRAME_OWNER, ObjectValue(*object));
        frameobj->setReservedSlot(JSSLOT_DEBUGFRAME_ARGUMENTS, ObjectOrNullValue(argsobj));

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
}

bool
Debug::wrapDebuggeeValue(JSContext *cx, Value *vp)
{
    assertSameCompartment(cx, object);

    // FIXME This is not quite what we want. Ideally we would get a transparent
    // wrapper no matter what sort of object *vp is. Oh well!
    if (!cx->compartment->wrap(cx, vp)) {
        vp->setUndefined();
        return false;
    }

    if (vp->isObject()) {
        JSObject *ccwobj = &vp->toObject();
        vp->setUndefined();

        // Debug.Object can't reflect objects from the current compartment.
        // FIXME Ideally this shouldn't be possible. See FIXME comment above.
        if (!ccwobj->isCrossCompartmentWrapper()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_STREAMS_CROSSED);
            return false;
        }

        ObjectMap::AddPtr p = objects.lookupForAdd(ccwobj);
        if (p) {
            vp->setObject(*p->value);
        } else {
            // Create a new Debug.Object for ccwobj.
            JSObject *proto = &object->getReservedSlot(JSSLOT_DEBUG_OBJECT_PROTO).toObject();
            JSObject *dobj = NewNonFunction<WithProto::Given>(cx, &DebugObject_class, proto, NULL);
            if (!dobj || !dobj->ensureClassReservedSlots(cx))
                return false;
            dobj->setReservedSlot(JSSLOT_DEBUGOBJECT_OWNER, ObjectValue(*object));
            dobj->setReservedSlot(JSSLOT_DEBUGOBJECT_CCW, ObjectValue(*ccwobj));
            if (!objects.relookupOrAdd(p, ccwobj, dobj)) {
                js_ReportOutOfMemory(cx);
                return false;
            }
            vp->setObject(*dobj);
        }
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
        *vp = dobj->getReservedSlot(JSSLOT_DEBUGOBJECT_CCW);
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
    if (!rv.isObject() ||
        !(obj = &rv.toObject())->isObject() ||
        !(shape = obj->lastProperty())->previous() ||
        shape->previous()->previous() ||
        (shape->propid != returnId && shape->propid != throwId) ||
        !shape->isDataDescriptor())
    {
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
    // Grab cx->fp() and the exception value before preparing to call the hook.
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

// === Debug JSObjects

bool
Debug::mark(GCMarker *trc, JSCompartment *comp, JSGCInvocationKind gckind)
{
    // Debuggers are marked during the incremental long tail of the GC mark
    // phase. This method returns true if it has to mark anything; GC calls it
    // repeatedly until it returns false.
    bool markedAny = false;

    // Search for Debug objects in the given compartment. We do this by
    // searching all the compartments being debugged.
    JSRuntime *rt = trc->context->runtime;
    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++) {
        JSCompartment *dc = *c;

        // If comp is non-null, this is a per-compartment GC and we
        // search every dc, except for comp (since no compartment can
        // debug itself). If comp is null, this is a global GC and we
        // search every dc that is live.
        if (comp ? dc != comp : !dc->isAboutToBeCollected(gckind)) {
            const GlobalObjectSet &debuggees = dc->getDebuggees();
            for (GlobalObjectSet::Range r = debuggees.all(); !r.empty(); r.popFront()) {
                GlobalObject *global = r.front();

                // Every debuggee has at least one debugger, so in this case
                // getDebuggers can't return NULL.
                const GlobalObject::DebugVector *debuggers = global->getDebuggers();
                for (Debug **p = debuggers->begin(); p != debuggers->end(); p++) {
                    Debug *dbg = *p;
                    JSObject *obj = dbg->toJSObject();

                    // We only need to examine obj if it's in a compartment
                    // being GC'd and it isn't already marked.
                    if ((!comp || obj->compartment() == comp) && !obj->isMarked()) {
                        if (dbg->hasAnyLiveHooks()) {
                            // obj could be reachable only via its live, enabled
                            // debugger hooks, which may yet be called.
                            MarkObject(trc, *obj, "enabled Debug");
                            markedAny = true;
                        }
                    }

                    // Handling Debug.Objects:
                    //
                    // If comp is the debuggee's compartment, do nothing. No
                    // referent objects will be collected, since we have a
                    // wrapper of each one.
                    //
                    // If comp is the debugger's compartment, mark all
                    // Debug.Objects, since the referents might be alive and
                    // therefore the table entries must remain.
                    //
                    // If comp is null, then for each key (referent-wrapper)
                    // that is marked, mark the corresponding value.
                    //
                    if (!comp || obj->compartment() == comp) {
                        for (ObjectMap::Range r = dbg->objects.all(); !r.empty(); r.popFront()) {
                            // The unwrap() call below has the following effect: we
                            // mark the Debug.Object if the *referent* is alive,
                            // even if the CCW of the referent seems unreachable.
                            if (!r.front().value->isMarked() &&
                                (comp || r.front().key->unwrap()->isMarked()))
                            {
                                MarkObject(trc, *r.front().value,
                                           "Debug.Object with live referent");
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
Debug::trace(JSTracer *trc, JSObject *obj)
{
    if (Debug *dbg = (Debug *) obj->getPrivate()) {
        MarkObject(trc, *dbg->hooksObject, "hooks");
        if (dbg->uncaughtExceptionHook)
            MarkObject(trc, *dbg->uncaughtExceptionHook, "hooks");

        // Mark Debug.Frame objects that are reachable from JS if we look them up
        // again (because the corresponding StackFrame is still on the stack).
        for (FrameMap::Enum e(dbg->frames); !e.empty(); e.popFront()) {
            JSObject *frameobj = e.front().value;
            JS_ASSERT(frameobj->getPrivate());
            MarkObject(trc, *frameobj, "live Debug.Frame");
        }
    }
}

void
Debug::sweepAll(JSRuntime *rt)
{
    for (JSCList *p = &rt->debuggerList; (p = JS_NEXT_LINK(p)) != &rt->debuggerList;) {
        Debug *dbg = (Debug *) ((unsigned char *) p - offsetof(Debug, link));

        // If this Debug is being GC'd, detach it from its debuggees.  In the
        // case of runtime-wide GC, the debuggee might be GC'd too. Since
        // detaching requires access to both objects, this must be done before
        // finalize time. However, in a per-compartment GC, it is impossible
        // for both objects to be GC'd (since they are in different
        // compartments), so in that case we just wait for Debug::finalize.
        if (!dbg->object->isMarked()) {
            for (GlobalObjectSet::Enum e(dbg->debuggees); !e.empty(); e.popFront())
                dbg->removeDebuggeeGlobal(e.front(), NULL, &e);
        }

        // Sweep ObjectMap entries for referents being collected.
        for (ObjectMap::Enum e(dbg->objects); !e.empty(); e.popFront()) {
            JS_ASSERT(e.front().key->isMarked() == e.front().value->isMarked());
            if (!e.front().value->isMarked())
                e.removeFront();
        }
    }

    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++)
        sweepCompartment(*c);
}

void
Debug::detachAllDebuggersFromGlobal(GlobalObject *global, GlobalObjectSet::Enum *compartmentEnum)
{
    const GlobalObject::DebugVector *debuggers = global->getDebuggers();
    JS_ASSERT(!debuggers->empty());
    while (!debuggers->empty())
        debuggers->back()->removeDebuggeeGlobal(global, compartmentEnum, NULL);
}

void
Debug::sweepCompartment(JSCompartment *compartment)
{
    // For each debuggee being GC'd, detach it from all its debuggers.
    GlobalObjectSet &debuggees = compartment->getDebuggees();
    for (GlobalObjectSet::Enum e(debuggees); !e.empty(); e.popFront()) {
        GlobalObject *global = e.front();
        if (!global->isMarked())
            detachAllDebuggersFromGlobal(global, &e);
    }
}

void
Debug::detachFromCompartment(JSCompartment *comp)
{
    for (GlobalObjectSet::Enum e(comp->getDebuggees()); !e.empty(); e.popFront())
        detachAllDebuggersFromGlobal(e.front(), &e);
}

void
Debug::finalize(JSContext *cx, JSObject *obj)
{
    Debug *dbg = (Debug *) obj->getPrivate();
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
    Debug::trace
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
    dbg->enabled = js_ValueToBoolean(vp[2]);
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

JSBool
Debug::getYoungestFrame(JSContext *cx, uintN argc, Value *vp)
{
    THISOBJ(cx, vp, Debug, "getYoungestFrame", thisobj, dbg);
    StackFrame *fp = cx->fp();
    while (fp && !dbg->observesFrame(fp))
        fp = fp->prev();
    if (!fp) {
        vp->setNull();
        return true;
    }
    return dbg->getScriptFrame(cx, fp, vp);
}

JSBool
Debug::construct(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug", 1);

    // Confirm that the first argument is a cross-compartment wrapper.
    const Value &arg = vp[2];
    if (!arg.isObject())
        return ReportObjectRequired(cx);
    JSObject *argobj = &arg.toObject();
    if (!argobj->isCrossCompartmentWrapper()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CCW_REQUIRED, "Debug");
        return false;
    }

    // Check that the target compartment is in debug mode.
    GlobalObject *debuggee = argobj->getProxyPrivate().toObject().getGlobal();
    if (!debuggee->compartment()->debugMode) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NEED_DEBUG_MODE);
        return false;
    }

    // Get Debug.prototype.
    Value v;
    jsid prototypeId = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
    if (!vp[0].toObject().getProperty(cx, prototypeId, &v))
        return false;
    JSObject *proto = &v.toObject();
    JS_ASSERT(proto->getClass() == &Debug::jsclass);

    // Make the new Debug object. Each one has a reference to
    // Debug.{Frame,Object}.prototype in reserved slots.
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
    if (!dbg->init(cx) || !dbg->addDebuggeeGlobal(cx, debuggee)) {
        cx->delete_(dbg);
        return false;
    }
    vp->setObject(*obj);
    return true;
}

bool
Debug::addDebuggeeGlobal(JSContext *cx, GlobalObject *obj)
{
    // Each debugger-debuggee relation must be stored in up to three places.
    GlobalObject::DebugVector *v = obj->getOrCreateDebuggers(cx);
    if (!v || !v->append(this))
        goto fail1;
    if (!debuggees.put(obj))
        goto fail2;
    if (obj->getDebuggers()->length() == 1 && !obj->compartment()->addDebuggee(obj))
        goto fail3;
    return true;

    // Maintain consistency on error.
fail3:
    debuggees.remove(obj);
fail2:
    JS_ASSERT(v->back() == this);
    v->popBack();
fail1:
    js_ReportOutOfMemory(cx);
    return false;
}

void
Debug::removeDebuggeeGlobal(GlobalObject *global, GlobalObjectSet::Enum *compartmentEnum,
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
    if (v->empty()) {
        if (compartmentEnum)
            compartmentEnum->removeFront();
        else
            global->compartment()->removeDebuggee(global);
    }
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
    JS_FN("getYoungestFrame", Debug::getYoungestFrame, 0, 0),
    JS_FS_END
};

// === Debug.Frame

Class DebugFrame_class = {
    "Frame", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGFRAME_COUNT),
    PropertyStub, PropertyStub, PropertyStub, StrictPropertyStub,
    EnumerateStub, ResolveStub, ConvertStub, FinalizeStub,
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

    // Check for Debug.Frame.prototype, which is of class DebugFrame_class but
    // isn't really a working Debug.Frame object.
    if (!thisobj->getPrivate()) {
        if (thisobj->getReservedSlot(JSSLOT_DEBUGFRAME_OWNER).isUndefined()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                                 "Debug.Frame", fnname, "prototype object");
            return NULL;
        }
        if (checkLive) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_FRAME_NOT_LIVE,
                                 "Debug.Frame", fnname);
            return NULL;
        }
    }
    return thisobj;
}

#define THIS_FRAME(cx, vp, fnname, thisobj, fp)                              \
    JSObject *thisobj = CheckThisFrame(cx, vp, fnname, true);                \
    if (!thisobj)                                                            \
        return false;                                                        \
    StackFrame *fp = (StackFrame *) thisobj->getPrivate()

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

JSBool
DebugFrame_getArguments(JSContext *cx, uintN argc, Value *vp)
{
    THIS_FRAME(cx, vp, "get arguments", thisobj, fp);
    (void) fp;  // quell warning that fp is unused
    *vp = thisobj->getReservedSlot(JSSLOT_DEBUGFRAME_ARGUMENTS);
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

static JSBool
DebugFrame_eval(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug.Frame.eval", 1);
    THIS_FRAME(cx, vp, "eval", thisobj, fp);
    Debug *dbg = Debug::fromChildJSObject(&vp[1].toObject());

    if (!vp[2].isString()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_EXPECTED_TYPE,
                             "Debug.Frame.eval", "string", InformalValueTypeName(vp[2]));
        return false;
    }
    JSLinearString *linearStr = vp[2].toString()->ensureLinear(cx);
    if (!linearStr)
        return false;
    JS::Anchor<JSString *> anchor(linearStr);

    AutoCompartment ac(cx, &fp->scopeChain());
    if (!ac.enter())
        return false;
    Value rval;
    bool ok = JS_EvaluateUCInStackFrame(cx, Jsvalify(fp), linearStr->chars(), linearStr->length(),
                                        "debugger eval code", 1, Jsvalify(&rval));
    return dbg->newCompletionValue(ac, ok, rval, vp);
}

static JSBool
DebugFrame_construct(JSContext *cx, uintN argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR, "Debug.Frame");
    return false;
}

static JSPropertySpec DebugFrame_properties[] = {
    JS_PSG("type", DebugFrame_getType, 0),
    JS_PSG("this", DebugFrame_getThis, 0),
    JS_PSG("older", DebugFrame_getOlder, 0),
    JS_PSG("live", DebugFrame_getLive, 0),
    JS_PSG("callee", DebugFrame_getCallee, 0),
    JS_PSG("generator", DebugFrame_getGenerator, 0),
    JS_PSG("constructing", DebugFrame_getConstructing, 0),
    JS_PSG("arguments", DebugFrame_getArguments, 0),
    JS_PS_END
};

static JSFunctionSpec DebugFrame_methods[] = {
    JS_FN("eval", DebugFrame_eval, 1, 0),
    JS_FS_END
};

// === Debug.Object

Class DebugObject_class = {
    "Object", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGOBJECT_COUNT),
    PropertyStub, PropertyStub, PropertyStub, StrictPropertyStub,
    EnumerateStub, ResolveStub, ConvertStub, FinalizeStub,
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

    // Check for Debug.Object.prototype, which is of class DebugObject_class
    // but isn't a real working Debug.Object.
    if (thisobj->getReservedSlot(JSSLOT_DEBUGOBJECT_CCW).isUndefined()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Debug.Object", fnname, "prototype object");
        return NULL;
    }
    return thisobj;
}

#define THIS_DEBUGOBJECT_CCW(cx, vp, fnname, obj)                            \
    JSObject *obj = DebugObject_checkThis(cx, vp, fnname);                   \
    if (!obj)                                                                \
        return false;                                                        \
    obj = &obj->getReservedSlot(JSSLOT_DEBUGOBJECT_CCW).toObject();          \
    JS_ASSERT(obj->isCrossCompartmentWrapper())

#define THIS_DEBUGOBJECT_REFERENT(cx, vp, fnname, obj)                       \
    THIS_DEBUGOBJECT_CCW(cx, vp, fnname, obj);                               \
    JS_ASSERT(obj->isCrossCompartmentWrapper());                             \
    obj = JSWrapper::wrappedObject(obj)

static JSBool
DebugObject_construct(JSContext *cx, uintN argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR, "Debug.Object");
    return false;
}

static JSBool
DebugObject_getProto(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_REFERENT(cx, vp, "get proto", refobj);
    vp->setObjectOrNull(refobj->getProto());
    return Debug::fromChildJSObject(&vp[1].toObject())->wrapDebuggeeValue(cx, vp);
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
    THIS_DEBUGOBJECT_REFERENT(cx, vp, "get name", obj);
    if (obj->isFunction()) {
        if (JSString *name = obj->getFunctionPrivate()->atom) {
            vp->setString(name);
            return Debug::fromChildJSObject(&vp[1].toObject())->wrapDebuggeeValue(cx, vp);
        }
    }
    vp->setNull();
    return true;
}

static JSBool
DebugObject_apply(JSContext *cx, uintN argc, Value *vp)
{
    THIS_DEBUGOBJECT_REFERENT(cx, vp, "apply", obj);
    Debug *dbg = Debug::fromChildJSObject(&vp[1].toObject());

    // Any JS exceptions thrown must be in the debugger compartment, so do
    // sanity checks and fallible conversions before entering the debuggee.
    if (!obj->isCallable()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Debug.Object", "apply", obj->getClass()->name);
        return false;
    }

    // Unwrap Debug.Objects. This happens in the debugger's compartment since
    // that is where any exceptions must be reported.
    Value calleev = vp[1];
    Value thisv = argc > 0 ? vp[2] : UndefinedValue();
    AutoValueVector argv(cx);
    if (!dbg->unwrapDebuggeeValue(cx, &calleev) || !dbg->unwrapDebuggeeValue(cx, &thisv))
        return false;
    if (argc >= 2 && !vp[3].isNullOrUndefined()) {
        if (!vp[3].isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_APPLY_ARGS, js_apply_str);
            return false;
        }
        JSObject *argsobj = &vp[3].toObject();
        uintN length;
        if (!js_GetLengthProperty(cx, argsobj, &length))
            return false;
        length = uintN(JS_MIN(length, JS_ARGS_LENGTH_MAX));

        if (!argv.growBy(length) || !GetElements(cx, argsobj, length, argv.begin()))
            return false;
        for (uintN i = 0; i < length; i++) {
            if (!dbg->unwrapDebuggeeValue(cx, &argv[i]))
                return false;
        }
    }

    // Enter the debuggee compartment and rewrap all input value for that compartment.
    // (Rewrapping always takes place in the destination compartment.)
    AutoCompartment ac(cx, obj);
    if (!ac.enter() || !cx->compartment->wrap(cx, &calleev) || !cx->compartment->wrap(cx, &thisv))
        return false;
    for (Value *p = argv.begin(); p != argv.end(); ++p) {
        if (!cx->compartment->wrap(cx, p))
            return false;
    }

    // Call the function. Use newCompletionValue to return to the debugger
    // compartment and populate *vp.
    Value rval;
    bool ok = ExternalInvoke(cx, thisv, calleev, argv.length(), argv.begin(), &rval);
    return dbg->newCompletionValue(ac, ok, rval, vp);
}

static JSPropertySpec DebugObject_properties[] = {
    JS_PSG("proto", DebugObject_getProto, 0),
    JS_PSG("class", DebugObject_getClass, 0),
    JS_PSG("callable", DebugObject_getCallable, 0),
    JS_PSG("name", DebugObject_getName, 0),
    JS_PS_END
};

static JSFunctionSpec DebugObject_methods[] = {
    JS_FN("apply", DebugObject_apply, 0, 0),
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

    JSObject *objectProto = js_InitClass(cx, debugCtor, objProto, &DebugObject_class,
                                         DebugObject_construct, 0,
                                         DebugObject_properties, DebugObject_methods, NULL, NULL);
    if (!objectProto)
        return false;

    debugProto->setReservedSlot(JSSLOT_DEBUG_FRAME_PROTO, ObjectValue(*frameProto));
    debugProto->setReservedSlot(JSSLOT_DEBUG_OBJECT_PROTO, ObjectValue(*objectProto));
    return true;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsfriendapi.h"

#include "mozilla/PodOperations.h"

#include <stdint.h>

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsprf.h"
#include "jsproxy.h"
#include "jswatchpoint.h"
#include "jsweakmap.h"
#include "jswrapper.h"
#include "prmjtime.h"

#include "builtin/TestingFunctions.h"
#include "proxy/DeadObjectProxy.h"
#include "vm/ArgumentsObject.h"
#include "vm/WrapperObject.h"

#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "vm/NativeObject-inl.h"
#include "vm/ScopeObject-inl.h"

using namespace js;
using namespace JS;

using mozilla::Move;
using mozilla::PodArrayZero;
using mozilla::UniquePtr;

// Required by PerThreadDataFriendFields::getMainThread()
JS_STATIC_ASSERT(offsetof(JSRuntime, mainThread) ==
                 PerThreadDataFriendFields::RuntimeMainThreadOffset);

PerThreadDataFriendFields::PerThreadDataFriendFields()
{
    PodArrayZero(nativeStackLimit);
#if JS_STACK_GROWTH_DIRECTION > 0
    for (int i=0; i<StackKindCount; i++)
        nativeStackLimit[i] = UINTPTR_MAX;
#endif
    PodArrayZero(thingGCRooters);
}

JS_FRIEND_API(void)
js::SetSourceHook(JSRuntime *rt, UniquePtr<SourceHook> hook)
{
    rt->sourceHook = Move(hook);
}

JS_FRIEND_API(UniquePtr<SourceHook>)
js::ForgetSourceHook(JSRuntime *rt)
{
    return Move(rt->sourceHook);
}

#ifdef NIGHTLY_BUILD
JS_FRIEND_API(void)
js::SetAssertOnScriptEntryHook(JSRuntime *rt, AssertOnScriptEntryHook hook)
{
    rt->assertOnScriptEntryHook_ = hook;
}
#endif

JS_FRIEND_API(void)
JS_SetGrayGCRootsTracer(JSRuntime *rt, JSTraceDataOp traceOp, void *data)
{
    rt->gc.setGrayRootsTracer(traceOp, data);
}

JS_FRIEND_API(JSString *)
JS_GetAnonymousString(JSRuntime *rt)
{
    MOZ_ASSERT(rt->hasContexts());
    return rt->commonNames->anonymous;
}

JS_FRIEND_API(JSObject *)
JS_FindCompilationScope(JSContext *cx, HandleObject objArg)
{
    RootedObject obj(cx, objArg);

    /*
     * We unwrap wrappers here. This is a little weird, but it's what's being
     * asked of us.
     */
    if (obj->is<WrapperObject>())
        obj = UncheckedUnwrap(obj);

    /*
     * Innerize the target_obj so that we compile in the correct (inner)
     * scope.
     */
    return GetInnerObject(obj);
}

JS_FRIEND_API(JSFunction *)
JS_GetObjectFunction(JSObject *obj)
{
    if (obj->is<JSFunction>())
        return &obj->as<JSFunction>();
    return nullptr;
}

JS_FRIEND_API(bool)
JS_SplicePrototype(JSContext *cx, HandleObject obj, HandleObject proto)
{
    /*
     * Change the prototype of an object which hasn't been used anywhere
     * and does not share its type with another object. Unlike JS_SetPrototype,
     * does not nuke type information for the object.
     */
    CHECK_REQUEST(cx);

    if (!obj->hasSingletonType()) {
        /*
         * We can see non-singleton objects when trying to splice prototypes
         * due to mutable __proto__ (ugh).
         */
        return JS_SetPrototype(cx, obj, proto);
    }

    Rooted<TaggedProto> tagged(cx, TaggedProto(proto));
    return obj->splicePrototype(cx, obj->getClass(), tagged);
}

JS_FRIEND_API(JSObject *)
JS_NewObjectWithUniqueType(JSContext *cx, const JSClass *clasp, HandleObject proto,
                           HandleObject parent)
{
    /*
     * Create our object with a null proto and then splice in the correct proto
     * after we setSingletonType, so that we don't pollute the default
     * TypeObject attached to our proto with information about our object, since
     * we're not going to be using that TypeObject anyway.
     */
    RootedObject obj(cx, NewObjectWithGivenProto(cx, (const js::Class *)clasp, nullptr,
                                                 parent, SingletonObject));
    if (!obj)
        return nullptr;
    if (!JS_SplicePrototype(cx, obj, proto))
        return nullptr;
    return obj;
}

JS_FRIEND_API(void)
JS::PrepareZoneForGC(Zone *zone)
{
    zone->scheduleGC();
}

JS_FRIEND_API(void)
JS::PrepareForFullGC(JSRuntime *rt)
{
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
        zone->scheduleGC();
}

JS_FRIEND_API(void)
JS::PrepareForIncrementalGC(JSRuntime *rt)
{
    if (!JS::IsIncrementalGCInProgress(rt))
        return;

    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        if (zone->wasGCStarted())
            PrepareZoneForGC(zone);
    }
}

JS_FRIEND_API(bool)
JS::IsGCScheduled(JSRuntime *rt)
{
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        if (zone->isGCScheduled())
            return true;
    }

    return false;
}

JS_FRIEND_API(void)
JS::SkipZoneForGC(Zone *zone)
{
    zone->unscheduleGC();
}

JS_FRIEND_API(void)
JS::GCForReason(JSRuntime *rt, gcreason::Reason reason)
{
    rt->gc.gc(GC_NORMAL, reason);
}

JS_FRIEND_API(void)
JS::ShrinkingGC(JSRuntime *rt, gcreason::Reason reason)
{
    rt->gc.gc(GC_SHRINK, reason);
}

JS_FRIEND_API(void)
JS::IncrementalGC(JSRuntime *rt, gcreason::Reason reason, int64_t millis)
{
    rt->gc.gcSlice(GC_NORMAL, reason, millis);
}

JS_FRIEND_API(void)
JS::FinishIncrementalGC(JSRuntime *rt, gcreason::Reason reason)
{
    rt->gc.gcFinalSlice(GC_NORMAL, reason);
}

JS_FRIEND_API(JSPrincipals *)
JS_GetCompartmentPrincipals(JSCompartment *compartment)
{
    return compartment->principals;
}

JS_FRIEND_API(void)
JS_SetCompartmentPrincipals(JSCompartment *compartment, JSPrincipals *principals)
{
    // Short circuit if there's no change.
    if (principals == compartment->principals)
        return;

    // Any compartment with the trusted principals -- and there can be
    // multiple -- is a system compartment.
    const JSPrincipals *trusted = compartment->runtimeFromMainThread()->trustedPrincipals();
    bool isSystem = principals && principals == trusted;

    // Clear out the old principals, if any.
    if (compartment->principals) {
        JS_DropPrincipals(compartment->runtimeFromMainThread(), compartment->principals);
        compartment->principals = nullptr;
        // We'd like to assert that our new principals is always same-origin
        // with the old one, but JSPrincipals doesn't give us a way to do that.
        // But we can at least assert that we're not switching between system
        // and non-system.
        MOZ_ASSERT(compartment->isSystem == isSystem);
    }

    // Set up the new principals.
    if (principals) {
        JS_HoldPrincipals(principals);
        compartment->principals = principals;
    }

    // Update the system flag.
    compartment->isSystem = isSystem;
}

JS_FRIEND_API(JSPrincipals *)
JS_GetScriptPrincipals(JSScript *script)
{
    return script->principals();
}

JS_FRIEND_API(bool)
JS_ScriptHasMutedErrors(JSScript *script)
{
    return script->mutedErrors();
}

JS_FRIEND_API(bool)
JS_WrapPropertyDescriptor(JSContext *cx, JS::MutableHandle<js::PropertyDescriptor> desc)
{
    return cx->compartment()->wrap(cx, desc);
}

JS_FRIEND_API(void)
JS_TraceShapeCycleCollectorChildren(JSTracer *trc, void *shape)
{
    MarkCycleCollectorChildren(trc, static_cast<Shape *>(shape));
}

static bool
DefineHelpProperty(JSContext *cx, HandleObject obj, const char *prop, const char *value)
{
    RootedAtom atom(cx, Atomize(cx, value, strlen(value)));
    if (!atom)
        return false;
    return JS_DefineProperty(cx, obj, prop, atom, JSPROP_READONLY | JSPROP_PERMANENT,
                             JS_PropertyStub, JS_StrictPropertyStub);
}

JS_FRIEND_API(bool)
JS_DefineFunctionsWithHelp(JSContext *cx, HandleObject obj, const JSFunctionSpecWithHelp *fs)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    for (; fs->name; fs++) {
        JSAtom *atom = Atomize(cx, fs->name, strlen(fs->name));
        if (!atom)
            return false;

        Rooted<jsid> id(cx, AtomToId(atom));
        RootedFunction fun(cx, DefineFunction(cx, obj, id, fs->call, fs->nargs, fs->flags));
        if (!fun)
            return false;

        if (fs->usage) {
            if (!DefineHelpProperty(cx, fun, "usage", fs->usage))
                return false;
        }

        if (fs->help) {
            if (!DefineHelpProperty(cx, fun, "help", fs->help))
                return false;
        }
    }

    return true;
}

JS_FRIEND_API(bool)
js_ObjectClassIs(JSContext *cx, HandleObject obj, ESClassValue classValue)
{
    return ObjectClassIs(obj, classValue, cx);
}

JS_FRIEND_API(const char *)
js_ObjectClassName(JSContext *cx, HandleObject obj)
{
    return JSObject::className(cx, obj);
}

JS_FRIEND_API(JS::Zone *)
js::GetCompartmentZone(JSCompartment *comp)
{
    return comp->zone();
}

JS_FRIEND_API(bool)
js::IsSystemCompartment(JSCompartment *comp)
{
    return comp->isSystem;
}

JS_FRIEND_API(bool)
js::IsSystemZone(Zone *zone)
{
    return zone->isSystem;
}

JS_FRIEND_API(bool)
js::IsAtomsCompartment(JSCompartment *comp)
{
    return comp->runtimeFromAnyThread()->isAtomsCompartment(comp);
}

JS_FRIEND_API(bool)
js::IsInNonStrictPropertySet(JSContext *cx)
{
    jsbytecode *pc;
    JSScript *script = cx->currentScript(&pc, JSContext::ALLOW_CROSS_COMPARTMENT);
    return script && !script->strict() && (js_CodeSpec[*pc].format & JOF_SET);
}

JS_FRIEND_API(bool)
js::IsFunctionObject(JSObject *obj)
{
    return obj->is<JSFunction>();
}

JS_FRIEND_API(bool)
js::IsScopeObject(JSObject *obj)
{
    return obj->is<ScopeObject>();
}

JS_FRIEND_API(bool)
js::IsCallObject(JSObject *obj)
{
    return obj->is<CallObject>();
}

JS_FRIEND_API(JSObject *)
js::GetObjectParentMaybeScope(JSObject *obj)
{
    return obj->enclosingScope();
}

JS_FRIEND_API(JSObject *)
js::GetGlobalForObjectCrossCompartment(JSObject *obj)
{
    return &obj->global();
}

JS_FRIEND_API(JSObject *)
js::GetPrototypeNoProxy(JSObject *obj)
{
    MOZ_ASSERT(!obj->is<js::ProxyObject>());
    MOZ_ASSERT(!obj->getTaggedProto().isLazy());
    return obj->getTaggedProto().toObjectOrNull();
}

JS_FRIEND_API(void)
js::SetPendingExceptionCrossContext(JSContext *cx, JS::HandleValue v)
{
    cx->setPendingException(v);
}

JS_FRIEND_API(void)
js::AssertSameCompartment(JSContext *cx, JSObject *obj)
{
    assertSameCompartment(cx, obj);
}

#ifdef DEBUG
JS_FRIEND_API(void)
js::AssertSameCompartment(JSObject *objA, JSObject *objB)
{
    MOZ_ASSERT(objA->compartment() == objB->compartment());
}
#endif

JS_FRIEND_API(void)
js::NotifyAnimationActivity(JSObject *obj)
{
    obj->compartment()->lastAnimationTime = PRMJ_Now();
}

JS_FRIEND_API(uint32_t)
js::GetObjectSlotSpan(JSObject *obj)
{
    return obj->fakeNativeSlotSpan();
}

JS_FRIEND_API(bool)
js::IsObjectInContextCompartment(JSObject *obj, const JSContext *cx)
{
    return obj->compartment() == cx->compartment();
}

JS_FRIEND_API(bool)
js::RunningWithTrustedPrincipals(JSContext *cx)
{
    return cx->runningWithTrustedPrincipals();
}

JS_FRIEND_API(JSFunction *)
js::GetOutermostEnclosingFunctionOfScriptedCaller(JSContext *cx)
{
    ScriptFrameIter iter(cx);
    if (iter.done())
        return nullptr;

    if (!iter.isFunctionFrame())
        return nullptr;

    RootedFunction curr(cx, iter.callee());
    for (StaticScopeIter<NoGC> i(curr); !i.done(); i++) {
        if (i.type() == StaticScopeIter<NoGC>::FUNCTION)
            curr = &i.fun();
    }
    return curr;
}

JS_FRIEND_API(JSFunction *)
js::DefineFunctionWithReserved(JSContext *cx, JSObject *objArg, const char *name, JSNative call,
                               unsigned nargs, unsigned attrs)
{
    RootedObject obj(cx, objArg);
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return nullptr;
    Rooted<jsid> id(cx, AtomToId(atom));
    return DefineFunction(cx, obj, id, call, nargs, attrs, JSFunction::ExtendedFinalizeKind);
}

JS_FRIEND_API(JSFunction *)
js::NewFunctionWithReserved(JSContext *cx, JSNative native, unsigned nargs, unsigned flags,
                            JSObject *parentArg, const char *name)
{
    RootedObject parent(cx, parentArg);
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);

    RootedAtom atom(cx);
    if (name) {
        atom = Atomize(cx, name, strlen(name));
        if (!atom)
            return nullptr;
    }

    JSFunction::Flags funFlags = JSAPIToJSFunctionFlags(flags);
    return NewFunction(cx, NullPtr(), native, nargs, funFlags, parent, atom,
                       JSFunction::ExtendedFinalizeKind);
}

JS_FRIEND_API(JSFunction *)
js::NewFunctionByIdWithReserved(JSContext *cx, JSNative native, unsigned nargs, unsigned flags, JSObject *parentArg,
                                jsid id)
{
    RootedObject parent(cx, parentArg);
    MOZ_ASSERT(JSID_IS_STRING(id));
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);

    RootedAtom atom(cx, JSID_TO_ATOM(id));
    JSFunction::Flags funFlags = JSAPIToJSFunctionFlags(flags);
    return NewFunction(cx, NullPtr(), native, nargs, funFlags, parent, atom,
                       JSFunction::ExtendedFinalizeKind);
}

JS_FRIEND_API(const Value &)
js::GetFunctionNativeReserved(JSObject *fun, size_t which)
{
    MOZ_ASSERT(fun->as<JSFunction>().isNative());
    return fun->as<JSFunction>().getExtendedSlot(which);
}

JS_FRIEND_API(void)
js::SetFunctionNativeReserved(JSObject *fun, size_t which, const Value &val)
{
    MOZ_ASSERT(fun->as<JSFunction>().isNative());
    MOZ_ASSERT_IF(val.isObject(), val.toObject().compartment() == fun->compartment());
    fun->as<JSFunction>().setExtendedSlot(which, val);
}

JS_FRIEND_API(bool)
js::GetObjectProto(JSContext *cx, JS::Handle<JSObject*> obj, JS::MutableHandle<JSObject*> proto)
{
    if (IsProxy(obj))
        return JS_GetPrototype(cx, obj, proto);

    proto.set(reinterpret_cast<const shadow::Object*>(obj.get())->type->proto);
    return true;
}

JS_FRIEND_API(bool)
js::GetOriginalEval(JSContext *cx, HandleObject scope, MutableHandleObject eval)
{
    assertSameCompartment(cx, scope);
    Rooted<GlobalObject *> global(cx, &scope->global());
    return GlobalObject::getOrCreateEval(cx, global, eval);
}

JS_FRIEND_API(void)
js::SetReservedSlotWithBarrier(JSObject *obj, size_t slot, const js::Value &value)
{
    obj->fakeNativeSetSlot(slot, value);
}

JS_FRIEND_API(bool)
js::GetGeneric(JSContext *cx, JSObject *objArg, JSObject *receiverArg, jsid idArg,
               Value *vp)
{
    RootedObject obj(cx, objArg), receiver(cx, receiverArg);
    RootedId id(cx, idArg);
    RootedValue value(cx);
    if (!JSObject::getGeneric(cx, obj, receiver, id, &value))
        return false;
    *vp = value;
    return true;
}

void
js::SetPreserveWrapperCallback(JSRuntime *rt, PreserveWrapperCallback callback)
{
    rt->preserveWrapperCallback = callback;
}

/*
 * The below code is for temporary telemetry use. It can be removed when
 * sufficient data has been harvested.
 */

namespace js {
// Defined in vm/GlobalObject.cpp.
extern size_t sSetProtoCalled;
}

JS_FRIEND_API(size_t)
JS_SetProtoCalled(JSContext *)
{
    return sSetProtoCalled;
}

// Defined in jsiter.cpp.
extern size_t sCustomIteratorCount;

JS_FRIEND_API(size_t)
JS_GetCustomIteratorCount(JSContext *cx)
{
    return sCustomIteratorCount;
}

JS_FRIEND_API(unsigned)
JS_PCToLineNumber(JSScript *script, jsbytecode *pc)
{
    return PCToLineNumber(script, pc);
}

JS_FRIEND_API(bool)
JS_IsDeadWrapper(JSObject *obj)
{
    return IsDeadProxyObject(obj);
}

void
js::TraceWeakMaps(WeakMapTracer *trc)
{
    WeakMapBase::traceAllMappings(trc);
    WatchpointMap::traceAll(trc);
}

extern JS_FRIEND_API(bool)
js::AreGCGrayBitsValid(JSRuntime *rt)
{
    return rt->gc.areGrayBitsValid();
}

JS_FRIEND_API(bool)
js::ZoneGlobalsAreAllGray(JS::Zone *zone)
{
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
        JSObject *obj = comp->maybeGlobal();
        if (!obj || !JS::GCThingIsMarkedGray(obj))
            return false;
    }
    return true;
}

JS_FRIEND_API(JSGCTraceKind)
js::GCThingTraceKind(void *thing)
{
    MOZ_ASSERT(thing);
    return gc::GetGCThingTraceKind(thing);
}

JS_FRIEND_API(void)
js::VisitGrayWrapperTargets(Zone *zone, GCThingCallback callback, void *closure)
{
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
        for (JSCompartment::WrapperEnum e(comp); !e.empty(); e.popFront()) {
            gc::Cell *thing = e.front().key().wrapped;
            if (thing->isTenured() && thing->asTenured().isMarked(gc::GRAY))
                callback(closure, thing);
        }
    }
}

JS_FRIEND_API(JSObject *)
js::GetWeakmapKeyDelegate(JSObject *key)
{
    if (JSWeakmapKeyDelegateOp op = key->getClass()->ext.weakmapKeyDelegateOp)
        return op(key);
    return nullptr;
}

JS_FRIEND_API(JSLinearString *)
js::StringToLinearStringSlow(JSContext *cx, JSString *str)
{
    return str->ensureLinear(cx);
}

JS_FRIEND_API(void)
JS_SetAccumulateTelemetryCallback(JSRuntime *rt, JSAccumulateTelemetryDataCallback callback)
{
    rt->telemetryCallback = callback;
}

JS_FRIEND_API(JSObject *)
JS_CloneObject(JSContext *cx, HandleObject obj, HandleObject protoArg, HandleObject parent)
{
    Rooted<TaggedProto> proto(cx, TaggedProto(protoArg.get()));
    return CloneObject(cx, obj, proto, parent);
}

#ifdef DEBUG
JS_FRIEND_API(void)
js_DumpString(JSString *str)
{
    str->dump();
}

JS_FRIEND_API(void)
js_DumpAtom(JSAtom *atom)
{
    atom->dump();
}

JS_FRIEND_API(void)
js_DumpChars(const char16_t *s, size_t n)
{
    fprintf(stderr, "char16_t * (%p) = ", (void *) s);
    JSString::dumpChars(s, n);
    fputc('\n', stderr);
}

JS_FRIEND_API(void)
js_DumpObject(JSObject *obj)
{
    if (!obj) {
        fprintf(stderr, "NULL\n");
        return;
    }
    obj->dump();
}

#endif

static const char *
FormatValue(JSContext *cx, const Value &vArg, JSAutoByteString &bytes)
{
    RootedValue v(cx, vArg);

    /*
     * We could use Maybe<AutoCompartment> here, but G++ can't quite follow
     * that, and warns about uninitialized members being used in the
     * destructor.
     */
    RootedString str(cx);
    if (v.isObject()) {
        AutoCompartment ac(cx, &v.toObject());
        str = ToString<CanGC>(cx, v);
    } else {
        str = ToString<CanGC>(cx, v);
    }

    if (!str)
        return nullptr;
    const char *buf = bytes.encodeLatin1(cx, str);
    if (!buf)
        return nullptr;
    const char *found = strstr(buf, "function ");
    if (found && (found - buf <= 2))
        return "[function]";
    return buf;
}

static char *
FormatFrame(JSContext *cx, const ScriptFrameIter &iter, char *buf, int num,
            bool showArgs, bool showLocals, bool showThisProps)
{
    MOZ_ASSERT(!cx->isExceptionPending());
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

    RootedValue thisVal(cx);
    if (iter.hasUsableAbstractFramePtr() && iter.computeThis(cx)) {
        thisVal = iter.computedThisValue();
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

    if (showArgs && iter.hasArgs()) {
        BindingVector bindings(cx);
        if (fun && fun->isInterpreted()) {
            if (!FillBindingVector(script, &bindings))
                return buf;
        }


        bool first = true;
        for (unsigned i = 0; i < iter.numActualArgs(); i++) {
            RootedValue arg(cx);
            if (i < iter.numFormalArgs() && script->formalIsAliased(i)) {
                for (AliasedFormalIter fi(script); ; fi++) {
                    if (fi.frameIndex() == i) {
                        arg = iter.callObj().aliasedVar(fi);
                        break;
                    }
                }
            } else if (script->argsObjAliasesFormals() && iter.hasArgsObj()) {
                arg = iter.argsObj().arg(i);
            } else {
                arg = iter.unaliasedActual(i, DONT_CHECK_ALIASING);
            }

            JSAutoByteString valueBytes;
            const char *value = FormatValue(cx, arg, valueBytes);

            JSAutoByteString nameBytes;
            const char *name = nullptr;

            if (i < bindings.length()) {
                name = nameBytes.encodeLatin1(cx, bindings[i].name());
                if (!buf)
                    return nullptr;
            }

            if (value) {
                buf = JS_sprintf_append(buf, "%s%s%s%s%s%s",
                                        !first ? ", " : "",
                                        name ? name :"",
                                        name ? " = " : "",
                                        arg.isString() ? "\"" : "",
                                        value ? value : "?unknown?",
                                        arg.isString() ? "\"" : "");
                if (!buf)
                    return buf;

                first = false;
            } else {
                buf = JS_sprintf_append(buf, "    <Failed to get argument while inspecting stack frame>\n");
                if (!buf)
                    return buf;
                cx->clearPendingException();

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


    // Note: Right now we don't dump the local variables anymore, because
    // that is hard to support across all the JITs etc.

    // print the value of 'this'
    if (showLocals) {
        if (!thisVal.isUndefined()) {
            JSAutoByteString thisValBytes;
            RootedString thisValStr(cx, ToString<CanGC>(cx, thisVal));
            const char *str = nullptr;
            if (thisValStr &&
                (str = thisValBytes.encodeLatin1(cx, thisValStr)))
            {
                buf = JS_sprintf_append(buf, "    this = %s\n", str);
                if (!buf)
                    return buf;
            } else {
                buf = JS_sprintf_append(buf, "    <failed to get 'this' value>\n");
                cx->clearPendingException();
            }
        }
    }

    if (showThisProps && thisVal.isObject()) {
        RootedObject obj(cx, &thisVal.toObject());

        AutoIdVector keys(cx);
        if (!GetPropertyKeys(cx, obj, JSITER_OWNONLY, &keys)) {
            cx->clearPendingException();
            return buf;
        }

        RootedId id(cx);
        for (size_t i = 0; i < keys.length(); i++) {
            RootedId id(cx, keys[i]);
            RootedValue key(cx, IdToValue(id));
            RootedValue v(cx);

            if (!JSObject::getGeneric(cx, obj, obj, id, &v)) {
                buf = JS_sprintf_append(buf, "    <Failed to fetch property while inspecting stack frame>\n");
                cx->clearPendingException();
                continue;
            }

            JSAutoByteString nameBytes;
            JSAutoByteString valueBytes;
            const char *name = FormatValue(cx, key, nameBytes);
            const char *value = FormatValue(cx, v, valueBytes);
            if (name && value) {
                buf = JS_sprintf_append(buf, "    this.%s = %s%s%s\n",
                                        name,
                                        v.isString() ? "\"" : "",
                                        value,
                                        v.isString() ? "\"" : "");
                if (!buf)
                    return buf;
            } else {
                buf = JS_sprintf_append(buf, "    <Failed to format values while inspecting stack frame>\n");
                cx->clearPendingException();
            }
        }
    }

    MOZ_ASSERT(!cx->isExceptionPending());
    return buf;
}

JS_FRIEND_API(char *)
JS::FormatStackDump(JSContext *cx, char *buf, bool showArgs, bool showLocals, bool showThisProps)
{
    int num = 0;

    for (AllFramesIter i(cx); !i.done(); ++i) {
        buf = FormatFrame(cx, i, buf, num, showArgs, showLocals, showThisProps);
        num++;
    }

    if (!num)
        buf = JS_sprintf_append(buf, "JavaScript stack is empty\n");

    return buf;
}

struct DumpHeapTracer : public JSTracer
{
    FILE   *output;

    DumpHeapTracer(FILE *fp, JSRuntime *rt, JSTraceCallback callback,
                   WeakMapTraceKind weakTraceKind)
      : JSTracer(rt, callback, weakTraceKind), output(fp)
    {}
};

static char
MarkDescriptor(void *thing)
{
    gc::TenuredCell *cell = gc::TenuredCell::fromPointer(thing);
    if (cell->isMarked(gc::BLACK))
        return cell->isMarked(gc::GRAY) ? 'G' : 'B';
    else
        return cell->isMarked(gc::GRAY) ? 'X' : 'W';
}

static void
DumpHeapVisitZone(JSRuntime *rt, void *data, Zone *zone)
{
    DumpHeapTracer *dtrc = static_cast<DumpHeapTracer *>(data);
    fprintf(dtrc->output, "# zone %p\n", (void *)zone);
}

static void
DumpHeapVisitCompartment(JSRuntime *rt, void *data, JSCompartment *comp)
{
    char name[1024];
    if (rt->compartmentNameCallback)
        (*rt->compartmentNameCallback)(rt, comp, name, sizeof(name));
    else
        strcpy(name, "<unknown>");

    DumpHeapTracer *dtrc = static_cast<DumpHeapTracer *>(data);
    fprintf(dtrc->output, "# compartment %s [in zone %p]\n", name, (void *)comp->zone());
}

static void
DumpHeapVisitArena(JSRuntime *rt, void *data, gc::Arena *arena,
                   JSGCTraceKind traceKind, size_t thingSize)
{
    DumpHeapTracer *dtrc = static_cast<DumpHeapTracer *>(data);
    fprintf(dtrc->output, "# arena allockind=%u size=%u\n",
            unsigned(arena->aheader.getAllocKind()), unsigned(thingSize));
}

static void
DumpHeapVisitCell(JSRuntime *rt, void *data, void *thing,
                  JSGCTraceKind traceKind, size_t thingSize)
{
    DumpHeapTracer *dtrc = static_cast<DumpHeapTracer *>(data);
    char cellDesc[1024 * 32];
    JS_GetTraceThingInfo(cellDesc, sizeof(cellDesc), dtrc, thing, traceKind, true);
    fprintf(dtrc->output, "%p %c %s\n", thing, MarkDescriptor(thing), cellDesc);
    JS_TraceChildren(dtrc, thing, traceKind);
}

static void
DumpHeapVisitChild(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    if (gc::IsInsideNursery((js::gc::Cell *)*thingp))
        return;

    DumpHeapTracer *dtrc = static_cast<DumpHeapTracer *>(trc);
    char buffer[1024];
    fprintf(dtrc->output, "> %p %c %s\n", *thingp, MarkDescriptor(*thingp),
            dtrc->getTracingEdgeName(buffer, sizeof(buffer)));
}

static void
DumpHeapVisitRoot(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    if (gc::IsInsideNursery((js::gc::Cell *)*thingp))
        return;

    DumpHeapTracer *dtrc = static_cast<DumpHeapTracer *>(trc);
    char buffer[1024];
    fprintf(dtrc->output, "%p %c %s\n", *thingp, MarkDescriptor(*thingp),
            dtrc->getTracingEdgeName(buffer, sizeof(buffer)));
}

void
js::DumpHeapComplete(JSRuntime *rt, FILE *fp, js::DumpHeapNurseryBehaviour nurseryBehaviour)
{
#ifdef JSGC_GENERATIONAL
    if (nurseryBehaviour == js::CollectNurseryBeforeDump)
        rt->gc.evictNursery(JS::gcreason::API);
#endif

    DumpHeapTracer dtrc(fp, rt, DumpHeapVisitRoot, TraceWeakMapKeysValues);
    TraceRuntime(&dtrc);

    fprintf(dtrc.output, "==========\n");

    dtrc.setTraceCallback(DumpHeapVisitChild);
    IterateZonesCompartmentsArenasCells(rt, &dtrc,
                                        DumpHeapVisitZone,
                                        DumpHeapVisitCompartment,
                                        DumpHeapVisitArena,
                                        DumpHeapVisitCell);

    fflush(dtrc.output);
}

JS_FRIEND_API(const JSStructuredCloneCallbacks *)
js::GetContextStructuredCloneCallbacks(JSContext *cx)
{
    return cx->runtime()->structuredCloneCallbacks;
}

JS_FRIEND_API(bool)
js::ContextHasOutstandingRequests(const JSContext *cx)
{
    return cx->outstandingRequests > 0;
}

JS_FRIEND_API(void)
js::SetActivityCallback(JSRuntime *rt, ActivityCallback cb, void *arg)
{
    rt->activityCallback = cb;
    rt->activityCallbackArg = arg;
}

JS_FRIEND_API(bool)
js::IsContextRunningJS(JSContext *cx)
{
    return cx->currentlyRunning();
}

JS_FRIEND_API(JS::GCSliceCallback)
JS::SetGCSliceCallback(JSRuntime *rt, GCSliceCallback callback)
{
    return rt->gc.setSliceCallback(callback);
}

JS_FRIEND_API(int64_t)
GetMaxGCPauseSinceClear(JSRuntime *rt)
{
    return rt->gc.stats.getMaxGCPauseSinceClear();
}

JS_FRIEND_API(int64_t)
ClearMaxGCPauseAccumulator(JSRuntime *rt)
{
    return rt->gc.stats.clearMaxGCPauseAccumulator();
}

JS_FRIEND_API(bool)
JS::WasIncrementalGC(JSRuntime *rt)
{
    return rt->gc.isIncrementalGc();
}

char16_t *
GCDescription::formatMessage(JSRuntime *rt) const
{
    return rt->gc.stats.formatMessage();
}

char16_t *
GCDescription::formatJSON(JSRuntime *rt, uint64_t timestamp) const
{
    return rt->gc.stats.formatJSON(timestamp);
}

JS_FRIEND_API(void)
JS::NotifyDidPaint(JSRuntime *rt)
{
    rt->gc.notifyDidPaint();
}

JS_FRIEND_API(bool)
JS::IsIncrementalGCEnabled(JSRuntime *rt)
{
    return rt->gc.isIncrementalGCEnabled();
}

JS_FRIEND_API(bool)
JS::IsIncrementalGCInProgress(JSRuntime *rt)
{
    return rt->gc.isIncrementalGCInProgress();
}

JS_FRIEND_API(void)
JS::DisableIncrementalGC(JSRuntime *rt)
{
    rt->gc.disallowIncrementalGC();
}

JS::AutoDisableGenerationalGC::AutoDisableGenerationalGC(JSRuntime *rt)
  : gc(&rt->gc)
#if defined(JSGC_GENERATIONAL) && defined(JS_GC_ZEAL)
  , restartVerifier(false)
#endif
{
#if defined(JSGC_GENERATIONAL) && defined(JS_GC_ZEAL)
    restartVerifier = gc->endVerifyPostBarriers();
#endif
    gc->disableGenerationalGC();
}

JS::AutoDisableGenerationalGC::~AutoDisableGenerationalGC()
{
    gc->enableGenerationalGC();
#if defined(JSGC_GENERATIONAL) && defined(JS_GC_ZEAL)
    if (restartVerifier) {
        MOZ_ASSERT(gc->isGenerationalGCEnabled());
        gc->startVerifyPostBarriers();
    }
#endif
}

extern JS_FRIEND_API(bool)
JS::IsGenerationalGCEnabled(JSRuntime *rt)
{
    return rt->gc.isGenerationalGCEnabled();
}

JS_FRIEND_API(bool)
JS::IsIncrementalBarrierNeeded(JSRuntime *rt)
{
    return rt->gc.state() == gc::MARK && !rt->isHeapBusy();
}

JS_FRIEND_API(bool)
JS::IsIncrementalBarrierNeeded(JSContext *cx)
{
    return IsIncrementalBarrierNeeded(cx->runtime());
}

JS_FRIEND_API(void)
JS::IncrementalObjectBarrier(JSObject *obj)
{
    if (!obj)
        return;

    MOZ_ASSERT(!obj->zone()->runtimeFromMainThread()->isHeapMajorCollecting());

    JSObject::writeBarrierPre(obj);
}

JS_FRIEND_API(void)
JS::IncrementalReferenceBarrier(void *ptr, JSGCTraceKind kind)
{
    if (!ptr)
        return;

    if (kind == JSTRACE_STRING && StringIsPermanentAtom(static_cast<JSString *>(ptr)))
        return;

    gc::Cell *cell = static_cast<gc::Cell *>(ptr);

#ifdef DEBUG
    Zone *zone = kind == JSTRACE_OBJECT
                 ? static_cast<JSObject *>(cell)->zone()
                 : cell->asTenured().zone();
    MOZ_ASSERT(!zone->runtimeFromMainThread()->isHeapMajorCollecting());
#endif

    if (kind == JSTRACE_OBJECT)
        JSObject::writeBarrierPre(static_cast<JSObject*>(cell));
    else if (kind == JSTRACE_STRING)
        JSString::writeBarrierPre(static_cast<JSString*>(cell));
    else if (kind == JSTRACE_SYMBOL)
        JS::Symbol::writeBarrierPre(static_cast<JS::Symbol*>(cell));
    else if (kind == JSTRACE_SCRIPT)
        JSScript::writeBarrierPre(static_cast<JSScript*>(cell));
    else if (kind == JSTRACE_LAZY_SCRIPT)
        LazyScript::writeBarrierPre(static_cast<LazyScript*>(cell));
    else if (kind == JSTRACE_JITCODE)
        jit::JitCode::writeBarrierPre(static_cast<jit::JitCode*>(cell));
    else if (kind == JSTRACE_SHAPE)
        Shape::writeBarrierPre(static_cast<Shape*>(cell));
    else if (kind == JSTRACE_BASE_SHAPE)
        BaseShape::writeBarrierPre(static_cast<BaseShape*>(cell));
    else if (kind == JSTRACE_TYPE_OBJECT)
        types::TypeObject::writeBarrierPre(static_cast<types::TypeObject *>(cell));
    else
        MOZ_CRASH("invalid trace kind");
}

JS_FRIEND_API(void)
JS::IncrementalValueBarrier(const Value &v)
{
    js::HeapValue::writeBarrierPre(v);
}

JS_FRIEND_API(void)
JS::PokeGC(JSRuntime *rt)
{
    rt->gc.poke();
}

JS_FRIEND_API(JSCompartment *)
js::GetAnyCompartmentInZone(JS::Zone *zone)
{
    CompartmentsInZoneIter comp(zone);
    MOZ_ASSERT(!comp.done());
    return comp.get();
}

void
JS::ObjectPtr::updateWeakPointerAfterGC()
{
    if (js::gc::IsObjectAboutToBeFinalized(value.unsafeGet()))
        value = nullptr;
}

void
JS::ObjectPtr::trace(JSTracer *trc, const char *name)
{
    JS_CallObjectTracer(trc, &value, name);
}

JS_FRIEND_API(JSObject *)
js::GetTestingFunctions(JSContext *cx)
{
    RootedObject obj(cx, JS_NewObject(cx, nullptr, NullPtr(), NullPtr()));
    if (!obj)
        return nullptr;

    if (!DefineTestingFunctions(cx, obj, false))
        return nullptr;

    return obj;
}

#ifdef DEBUG
JS_FRIEND_API(unsigned)
js::GetEnterCompartmentDepth(JSContext *cx)
{
  return cx->getEnterCompartmentDepth();
}
#endif

JS_FRIEND_API(void)
js::SetDOMCallbacks(JSRuntime *rt, const DOMCallbacks *callbacks)
{
    rt->DOMcallbacks = callbacks;
}

JS_FRIEND_API(const DOMCallbacks *)
js::GetDOMCallbacks(JSRuntime *rt)
{
    return rt->DOMcallbacks;
}

static const void *gDOMProxyHandlerFamily = nullptr;
static uint32_t gDOMProxyExpandoSlot = 0;
static DOMProxyShadowsCheck gDOMProxyShadowsCheck;

JS_FRIEND_API(void)
js::SetDOMProxyInformation(const void *domProxyHandlerFamily, uint32_t domProxyExpandoSlot,
                           DOMProxyShadowsCheck domProxyShadowsCheck)
{
    gDOMProxyHandlerFamily = domProxyHandlerFamily;
    gDOMProxyExpandoSlot = domProxyExpandoSlot;
    gDOMProxyShadowsCheck = domProxyShadowsCheck;
}

const void *
js::GetDOMProxyHandlerFamily()
{
    return gDOMProxyHandlerFamily;
}

uint32_t
js::GetDOMProxyExpandoSlot()
{
    return gDOMProxyExpandoSlot;
}

DOMProxyShadowsCheck
js::GetDOMProxyShadowsCheck()
{
    return gDOMProxyShadowsCheck;
}

bool
js::detail::IdMatchesAtom(jsid id, JSAtom *atom)
{
    return id == INTERNED_STRING_TO_JSID(nullptr, atom);
}

JS_FRIEND_API(JSContext *)
js::DefaultJSContext(JSRuntime *rt)
{
    if (rt->defaultJSContextCallback) {
        JSContext *cx = rt->defaultJSContextCallback(rt);
        MOZ_ASSERT(cx);
        return cx;
    }
    MOZ_ASSERT(rt->contextList.getFirst() == rt->contextList.getLast());
    return rt->contextList.getFirst();
}

JS_FRIEND_API(void)
js::SetDefaultJSContextCallback(JSRuntime *rt, DefaultJSContextCallback cb)
{
    rt->defaultJSContextCallback = cb;
}

#ifdef DEBUG
JS_FRIEND_API(void)
js::Debug_SetActiveJSContext(JSRuntime *rt, JSContext *cx)
{
    rt->activeContext = cx;
}
#endif

JS_FRIEND_API(void)
js::SetCTypesActivityCallback(JSRuntime *rt, CTypesActivityCallback cb)
{
    rt->ctypesActivityCallback = cb;
}

js::AutoCTypesActivityCallback::AutoCTypesActivityCallback(JSContext *cx,
                                                           js::CTypesActivityType beginType,
                                                           js::CTypesActivityType endType
                                                           MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : cx(cx), callback(cx->runtime()->ctypesActivityCallback), endType(endType)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    if (callback)
        callback(cx, beginType);
}

JS_FRIEND_API(void)
js::SetObjectMetadataCallback(JSContext *cx, ObjectMetadataCallback callback)
{
    cx->compartment()->setObjectMetadataCallback(callback);
}

JS_FRIEND_API(bool)
js::SetObjectMetadata(JSContext *cx, HandleObject obj, HandleObject metadata)
{
    return JSObject::setMetadata(cx, obj, metadata);
}

JS_FRIEND_API(JSObject *)
js::GetObjectMetadata(JSObject *obj)
{
    return obj->getMetadata();
}

JS_FRIEND_API(void)
js::UnsafeDefineElement(JSContext *cx, JS::HandleObject obj, uint32_t index, JS::HandleValue value)
{
    MOZ_ASSERT(obj->isNative());
    MOZ_ASSERT(index < obj->as<NativeObject>().getDenseInitializedLength());
    obj->as<NativeObject>().setDenseElementWithType(cx, index, value);
}

JS_FRIEND_API(bool)
js_DefineOwnProperty(JSContext *cx, JSObject *objArg, jsid idArg,
                     JS::Handle<js::PropertyDescriptor> descriptor, bool *bp)
{
    RootedObject obj(cx, objArg);
    RootedId id(cx, idArg);
    js::AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id, descriptor.value());
    if (descriptor.hasGetterObject())
        assertSameCompartment(cx, descriptor.getterObject());
    if (descriptor.hasSetterObject())
        assertSameCompartment(cx, descriptor.setterObject());

    return DefineOwnProperty(cx, HandleObject(obj), id, descriptor, bp);
}

JS_FRIEND_API(bool)
js_ReportIsNotFunction(JSContext *cx, JS::HandleValue v)
{
    return ReportIsNotFunction(cx, v);
}

JS_FRIEND_API(void)
js::ReportErrorWithId(JSContext *cx, const char *msg, HandleId id)
{
    RootedValue idv(cx);
    if (!JS_IdToValue(cx, id, &idv))
        return;
    JSString *idstr = JS::ToString(cx, idv);
    if (!idstr)
        return;
    JSAutoByteString bytes(cx, idstr);
    if (!bytes)
        return;
    JS_ReportError(cx, msg, bytes.ptr());
}

#ifdef DEBUG
JS_PUBLIC_API(bool)
js::IsInRequest(JSContext *cx)
{
    return !!cx->runtime()->requestDepth;
}

bool
js::HasObjectMovedOpIfRequired(JSObject *obj) {
    return obj->is<GlobalObject>() || !!GetObjectClass(obj)->ext.objectMovedOp;
}
#endif

#ifdef JSGC_GENERATIONAL
JS_FRIEND_API(void)
JS_StoreObjectPostBarrierCallback(JSContext* cx,
                                  void (*callback)(JSTracer *trc, JSObject *key, void *data),
                                  JSObject *key, void *data)
{
    JSRuntime *rt = cx->runtime();
    if (IsInsideNursery(key))
        rt->gc.storeBuffer.putCallback(callback, key, data);
}

extern JS_FRIEND_API(void)
JS_StoreStringPostBarrierCallback(JSContext* cx,
                                  void (*callback)(JSTracer *trc, JSString *key, void *data),
                                  JSString *key, void *data)
{
    JSRuntime *rt = cx->runtime();
    if (IsInsideNursery(key))
        rt->gc.storeBuffer.putCallback(callback, key, data);
}
#endif /* JSGC_GENERATIONAL */

JS_FRIEND_API(bool)
js::ForwardToNative(JSContext *cx, JSNative native, const CallArgs &args)
{
    return native(cx, args.length(), args.base());
}

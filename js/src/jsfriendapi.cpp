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
#include "jswatchpoint.h"
#include "jsweakmap.h"
#include "jswrapper.h"
#include "prmjtime.h"

#include "builtin/TestingFunctions.h"
#include "vm/WrapperObject.h"

#include "jsfuninlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

using namespace js;
using namespace JS;

using mozilla::PodArrayZero;

// Required by PerThreadDataFriendFields::getMainThread()
JS_STATIC_ASSERT(offsetof(JSRuntime, mainThread) ==
                 PerThreadDataFriendFields::RuntimeMainThreadOffset);

PerThreadDataFriendFields::PerThreadDataFriendFields()
  : nativeStackLimit(0)
{
#if defined(JSGC_ROOT_ANALYSIS) || defined(JSGC_USE_EXACT_ROOTING)
    PodArrayZero(thingGCRooters);
#endif
#if defined(DEBUG) && defined(JS_GC_ZEAL) && defined(JSGC_ROOT_ANALYSIS) && !defined(JS_THREADSAFE)
    skipGCRooters = NULL;
#endif
}

JS_FRIEND_API(void)
JS_SetSourceHook(JSRuntime *rt, JS_SourceHook hook)
{
    rt->sourceHook = hook;
}

JS_FRIEND_API(void)
JS_SetGrayGCRootsTracer(JSRuntime *rt, JSTraceDataOp traceOp, void *data)
{
    rt->gcGrayRootTracer.op = traceOp;
    rt->gcGrayRootTracer.data = data;
}

JS_FRIEND_API(JSString *)
JS_GetAnonymousString(JSRuntime *rt)
{
    JS_ASSERT(rt->hasContexts());
    return rt->atomState.anonymous;
}

JS_FRIEND_API(JSObject *)
JS_FindCompilationScope(JSContext *cx, JSObject *objArg)
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
    if (JSObjectOp op = obj->getClass()->ext.innerObject)
        obj = op(cx, obj);
    return obj;
}

JS_FRIEND_API(JSFunction *)
JS_GetObjectFunction(JSObject *obj)
{
    if (obj->is<JSFunction>())
        return &obj->as<JSFunction>();
    return NULL;
}

JS_FRIEND_API(bool)
JS_SplicePrototype(JSContext *cx, JSObject *objArg, JSObject *protoArg)
{
    RootedObject obj(cx, objArg);
    RootedObject proto(cx, protoArg);
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
JS_NewObjectWithUniqueType(JSContext *cx, JSClass *clasp, JSObject *protoArg, JSObject *parentArg)
{
    RootedObject proto(cx, protoArg);
    RootedObject parent(cx, parentArg);
    /*
     * Create our object with a null proto and then splice in the correct proto
     * after we setSingletonType, so that we don't pollute the default
     * TypeObject attached to our proto with information about our object, since
     * we're not going to be using that TypeObject anyway.
     */
    RootedObject obj(cx, NewObjectWithGivenProto(cx, (js::Class *)clasp, NULL, parent, SingletonObject));
    if (!obj)
        return NULL;
    if (!JS_SplicePrototype(cx, obj, proto))
        return NULL;
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
    for (ZonesIter zone(rt); !zone.done(); zone.next())
        zone->scheduleGC();
}

JS_FRIEND_API(void)
JS::PrepareForIncrementalGC(JSRuntime *rt)
{
    if (!JS::IsIncrementalGCInProgress(rt))
        return;

    for (ZonesIter zone(rt); !zone.done(); zone.next()) {
        if (zone->wasGCStarted())
            PrepareZoneForGC(zone);
    }
}

JS_FRIEND_API(bool)
JS::IsGCScheduled(JSRuntime *rt)
{
    for (ZonesIter zone(rt); !zone.done(); zone.next()) {
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
    GC(rt, GC_NORMAL, reason);
}

JS_FRIEND_API(void)
JS::ShrinkingGC(JSRuntime *rt, gcreason::Reason reason)
{
    GC(rt, GC_SHRINK, reason);
}

JS_FRIEND_API(void)
JS::IncrementalGC(JSRuntime *rt, gcreason::Reason reason, int64_t millis)
{
    GCSlice(rt, GC_NORMAL, reason, millis);
}

JS_FRIEND_API(void)
JS::FinishIncrementalGC(JSRuntime *rt, gcreason::Reason reason)
{
    GCFinalSlice(rt, GC_NORMAL, reason);
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
    JSPrincipals *trusted = compartment->runtimeFromMainThread()->trustedPrincipals();
    bool isSystem = principals && principals == trusted;

    // Clear out the old principals, if any.
    if (compartment->principals) {
        JS_DropPrincipals(compartment->runtimeFromMainThread(), compartment->principals);
        compartment->principals = NULL;
        // We'd like to assert that our new principals is always same-origin
        // with the old one, but JSPrincipals doesn't give us a way to do that.
        // But we can at least assert that we're not switching between system
        // and non-system.
        JS_ASSERT(compartment->isSystem == isSystem);
    }

    // Set up the new principals.
    if (principals) {
        JS_HoldPrincipals(principals);
        compartment->principals = principals;
    }

    // Update the system flag.
    compartment->isSystem = isSystem;
}

JS_FRIEND_API(bool)
JS_WrapPropertyDescriptor(JSContext *cx, JS::MutableHandle<js::PropertyDescriptor> desc)
{
    return cx->compartment()->wrap(cx, desc);
}

JS_FRIEND_API(bool)
JS_WrapAutoIdVector(JSContext *cx, js::AutoIdVector &props)
{
    return cx->compartment()->wrap(cx, props);
}

JS_FRIEND_API(void)
JS_TraceShapeCycleCollectorChildren(JSTracer *trc, void *shape)
{
    MarkCycleCollectorChildren(trc, static_cast<Shape *>(shape));
}

static bool
DefineHelpProperty(JSContext *cx, HandleObject obj, const char *prop, const char *value)
{
    JSAtom *atom = Atomize(cx, value, strlen(value));
    if (!atom)
        return false;
    jsval v = STRING_TO_JSVAL(atom);
    return JS_DefineProperty(cx, obj, prop, v,
                             JS_PropertyStub, JS_StrictPropertyStub,
                             JSPROP_READONLY | JSPROP_PERMANENT);
}

JS_FRIEND_API(bool)
JS_DefineFunctionsWithHelp(JSContext *cx, JSObject *objArg, const JSFunctionSpecWithHelp *fs)
{
    RootedObject obj(cx, objArg);
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));

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
js::DefaultObjectForContextOrNull(JSContext *cx)
{
    return cx->maybeDefaultCompartmentObject();
}

JS_FRIEND_API(void)
js::SetDefaultObjectForContext(JSContext *cx, JSObject *obj)
{
    cx->setDefaultCompartmentObject(obj);
}

JS_FRIEND_API(void)
js::NotifyAnimationActivity(JSObject *obj)
{
    obj->compartment()->lastAnimationTime = PRMJ_Now();
}

JS_FRIEND_API(uint32_t)
js::GetObjectSlotSpan(JSObject *obj)
{
    return obj->slotSpan();
}

JS_FRIEND_API(bool)
js::IsObjectInContextCompartment(JSObject *obj, const JSContext *cx)
{
    return obj->compartment() == cx->compartment();
}

JS_FRIEND_API(bool)
js::IsOriginalScriptFunction(JSFunction *fun)
{
    return fun->nonLazyScript()->function() == fun;
}

JS_FRIEND_API(JSScript *)
js::GetOutermostEnclosingFunctionOfScriptedCaller(JSContext *cx)
{
    ScriptFrameIter iter(cx);
    if (iter.done())
        return NULL;

    if (!iter.isFunctionFrame())
        return NULL;

    RootedFunction scriptedCaller(cx, iter.callee());
    RootedScript outermost(cx, scriptedCaller->nonLazyScript());
    for (StaticScopeIter i(cx, scriptedCaller); !i.done(); i++) {
        if (i.type() == StaticScopeIter::FUNCTION)
            outermost = i.funScript();
    }
    return outermost;
}

JS_FRIEND_API(JSFunction *)
js::DefineFunctionWithReserved(JSContext *cx, JSObject *objArg, const char *name, JSNative call,
                               unsigned nargs, unsigned attrs)
{
    RootedObject obj(cx, objArg);
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return NULL;
    Rooted<jsid> id(cx, AtomToId(atom));
    return DefineFunction(cx, obj, id, call, nargs, attrs, JSFunction::ExtendedFinalizeKind);
}

JS_FRIEND_API(JSFunction *)
js::NewFunctionWithReserved(JSContext *cx, JSNative native, unsigned nargs, unsigned flags,
                            JSObject *parentArg, const char *name)
{
    RootedObject parent(cx, parentArg);
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);

    RootedAtom atom(cx);
    if (name) {
        atom = Atomize(cx, name, strlen(name));
        if (!atom)
            return NULL;
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
    JS_ASSERT(JSID_IS_STRING(id));
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);

    RootedAtom atom(cx, JSID_TO_ATOM(id));
    JSFunction::Flags funFlags = JSAPIToJSFunctionFlags(flags);
    return NewFunction(cx, NullPtr(), native, nargs, funFlags, parent, atom,
                       JSFunction::ExtendedFinalizeKind);
}

JS_FRIEND_API(JSObject *)
js::InitClassWithReserved(JSContext *cx, JSObject *objArg, JSObject *parent_protoArg,
                          JSClass *clasp, JSNative constructor, unsigned nargs,
                          const JSPropertySpec *ps, const JSFunctionSpec *fs,
                          const JSPropertySpec *static_ps, const JSFunctionSpec *static_fs)
{
    RootedObject obj(cx, objArg);
    RootedObject parent_proto(cx, parent_protoArg);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, parent_proto);
    return js_InitClass(cx, obj, parent_proto, Valueify(clasp), constructor,
                        nargs, ps, fs, static_ps, static_fs, NULL,
                        JSFunction::ExtendedFinalizeKind);
}

JS_FRIEND_API(const Value &)
js::GetFunctionNativeReserved(JSObject *fun, size_t which)
{
    JS_ASSERT(fun->as<JSFunction>().isNative());
    return fun->as<JSFunction>().getExtendedSlot(which);
}

JS_FRIEND_API(void)
js::SetFunctionNativeReserved(JSObject *fun, size_t which, const Value &val)
{
    JS_ASSERT(fun->as<JSFunction>().isNative());
    fun->as<JSFunction>().setExtendedSlot(which, val);
}

JS_FRIEND_API(void)
js::SetReservedSlotWithBarrier(JSObject *obj, size_t slot, const js::Value &value)
{
    obj->setSlot(slot, value);
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

JS_FRIEND_API(bool)
JS_IsDeadWrapper(JSObject *obj)
{
    if (!obj->is<ProxyObject>()) {
        return false;
    }

    return obj->as<ProxyObject>().handler()->family() == &DeadObjectProxy::sDeadObjectFamily;
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
    return rt->gcGrayBitsValid;
}

JS_FRIEND_API(JSGCTraceKind)
js::GCThingTraceKind(void *thing)
{
    JS_ASSERT(thing);
    return gc::GetGCThingTraceKind(thing);
}

JS_FRIEND_API(void)
js::VisitGrayWrapperTargets(Zone *zone, GCThingCallback callback, void *closure)
{
    JSRuntime *rt = zone->runtimeFromMainThread();
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
        for (JSCompartment::WrapperEnum e(comp); !e.empty(); e.popFront()) {
            gc::Cell *thing = e.front().key.wrapped;
            if (!IsInsideNursery(rt, thing) && thing->isMarked(gc::GRAY))
                callback(closure, thing);
        }
    }
}

JS_FRIEND_API(JSObject *)
js::GetWeakmapKeyDelegate(JSObject *key)
{
    if (JSWeakmapKeyDelegateOp op = key->getClass()->ext.weakmapKeyDelegateOp)
        return op(key);
    return NULL;
}

JS_FRIEND_API(void)
JS_SetAccumulateTelemetryCallback(JSRuntime *rt, JSAccumulateTelemetryDataCallback callback)
{
    rt->telemetryCallback = callback;
}

JS_FRIEND_API(JSObject *)
JS_CloneObject(JSContext *cx, JSObject *obj_, JSObject *proto_, JSObject *parent_)
{
    RootedObject obj(cx, obj_);
    Rooted<js::TaggedProto> proto(cx, proto_);
    RootedObject parent(cx, parent_);
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
js_DumpChars(const jschar *s, size_t n)
{
    fprintf(stderr, "jschar * (%p) = ", (void *) s);
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

struct JSDumpHeapTracer : public JSTracer
{
    FILE   *output;

    JSDumpHeapTracer(FILE *fp)
      : output(fp)
    {}
};

static char
MarkDescriptor(void *thing)
{
    gc::Cell *cell = static_cast<gc::Cell*>(thing);
    if (cell->isMarked(gc::BLACK))
        return cell->isMarked(gc::GRAY) ? 'G' : 'B';
    else
        return cell->isMarked(gc::GRAY) ? 'X' : 'W';
}

static void
DumpHeapVisitZone(JSRuntime *rt, void *data, Zone *zone)
{
    JSDumpHeapTracer *dtrc = static_cast<JSDumpHeapTracer *>(data);
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

    JSDumpHeapTracer *dtrc = static_cast<JSDumpHeapTracer *>(data);
    fprintf(dtrc->output, "# compartment %s [in zone %p]\n", name, (void *)comp->zone());
}

static void
DumpHeapVisitArena(JSRuntime *rt, void *data, gc::Arena *arena,
                   JSGCTraceKind traceKind, size_t thingSize)
{
    JSDumpHeapTracer *dtrc = static_cast<JSDumpHeapTracer *>(data);
    fprintf(dtrc->output, "# arena allockind=%u size=%u\n",
            unsigned(arena->aheader.getAllocKind()), unsigned(thingSize));
}

static void
DumpHeapVisitCell(JSRuntime *rt, void *data, void *thing,
                  JSGCTraceKind traceKind, size_t thingSize)
{
    JSDumpHeapTracer *dtrc = static_cast<JSDumpHeapTracer *>(data);
    char cellDesc[1024 * 32];
    JS_GetTraceThingInfo(cellDesc, sizeof(cellDesc), dtrc, thing, traceKind, true);
    fprintf(dtrc->output, "%p %c %s\n", thing, MarkDescriptor(thing), cellDesc);
    JS_TraceChildren(dtrc, thing, traceKind);
}

static void
DumpHeapVisitChild(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    JSDumpHeapTracer *dtrc = static_cast<JSDumpHeapTracer *>(trc);
    char buffer[1024];
    fprintf(dtrc->output, "> %p %c %s\n", *thingp, MarkDescriptor(*thingp),
            JS_GetTraceEdgeName(dtrc, buffer, sizeof(buffer)));
}

static void
DumpHeapVisitRoot(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    JSDumpHeapTracer *dtrc = static_cast<JSDumpHeapTracer *>(trc);
    char buffer[1024];
    fprintf(dtrc->output, "%p %c %s\n", *thingp, MarkDescriptor(*thingp),
            JS_GetTraceEdgeName(dtrc, buffer, sizeof(buffer)));
}

void
js::DumpHeapComplete(JSRuntime *rt, FILE *fp)
{
    JSDumpHeapTracer dtrc(fp);

    JS_TracerInit(&dtrc, rt, DumpHeapVisitRoot);
    dtrc.eagerlyTraceWeakMaps = TraceWeakMapKeysValues;
    TraceRuntime(&dtrc);

    fprintf(dtrc.output, "==========\n");

    JS_TracerInit(&dtrc, rt, DumpHeapVisitChild);
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

#ifdef JS_THREADSAFE
JS_FRIEND_API(bool)
js::ContextHasOutstandingRequests(const JSContext *cx)
{
    return cx->outstandingRequests > 0;
}
#endif

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
    JS::GCSliceCallback old = rt->gcSliceCallback;
    rt->gcSliceCallback = callback;
    return old;
}

JS_FRIEND_API(bool)
JS::WasIncrementalGC(JSRuntime *rt)
{
    return rt->gcIsIncremental;
}

jschar *
GCDescription::formatMessage(JSRuntime *rt) const
{
    return rt->gcStats.formatMessage();
}

jschar *
GCDescription::formatJSON(JSRuntime *rt, uint64_t timestamp) const
{
    return rt->gcStats.formatJSON(timestamp);
}

JS_FRIEND_API(void)
JS::NotifyDidPaint(JSRuntime *rt)
{
    if (rt->gcZeal() == gc::ZealFrameVerifierPreValue) {
        gc::VerifyBarriers(rt, gc::PreBarrierVerifier);
        return;
    }

    if (rt->gcZeal() == gc::ZealFrameVerifierPostValue) {
        gc::VerifyBarriers(rt, gc::PostBarrierVerifier);
        return;
    }

    if (rt->gcZeal() == gc::ZealFrameGCValue) {
        PrepareForFullGC(rt);
        GCSlice(rt, GC_NORMAL, gcreason::REFRESH_FRAME);
        return;
    }

    if (JS::IsIncrementalGCInProgress(rt) && !rt->gcInterFrameGC) {
        JS::PrepareForIncrementalGC(rt);
        GCSlice(rt, GC_NORMAL, gcreason::REFRESH_FRAME);
    }

    rt->gcInterFrameGC = false;
}

JS_FRIEND_API(bool)
JS::IsIncrementalGCEnabled(JSRuntime *rt)
{
    return rt->gcIncrementalEnabled && rt->gcMode == JSGC_MODE_INCREMENTAL;
}

JS_FRIEND_API(bool)
JS::IsIncrementalGCInProgress(JSRuntime *rt)
{
    return (rt->gcIncrementalState != gc::NO_INCREMENTAL && !rt->gcVerifyPreData);
}

JS_FRIEND_API(void)
JS::DisableIncrementalGC(JSRuntime *rt)
{
    rt->gcIncrementalEnabled = false;
}

extern JS_FRIEND_API(void)
JS::DisableGenerationalGC(JSRuntime *rt)
{
    rt->gcGenerationalEnabled = false;
#ifdef JSGC_GENERATIONAL
    MinorGC(rt, JS::gcreason::API);
    rt->gcNursery.disable();
    rt->gcStoreBuffer.disable();
#endif
}

extern JS_FRIEND_API(void)
JS::EnableGenerationalGC(JSRuntime *rt)
{
    rt->gcGenerationalEnabled = true;
#ifdef JSGC_GENERATIONAL
    rt->gcNursery.enable();
    rt->gcStoreBuffer.enable();
#endif
}

JS_FRIEND_API(bool)
JS::IsIncrementalBarrierNeeded(JSRuntime *rt)
{
    return (rt->gcIncrementalState == gc::MARK && !rt->isHeapBusy());
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

    JS_ASSERT(!obj->zone()->runtimeFromMainThread()->isHeapMajorCollecting());

    AutoMarkInDeadZone amn(obj->zone());

    JSObject::writeBarrierPre(obj);
}

JS_FRIEND_API(void)
JS::IncrementalReferenceBarrier(void *ptr, JSGCTraceKind kind)
{
    if (!ptr)
        return;

    gc::Cell *cell = static_cast<gc::Cell *>(ptr);
    Zone *zone = kind == JSTRACE_OBJECT
                 ? static_cast<JSObject *>(cell)->zone()
                 : cell->tenuredZone();

    JS_ASSERT(!zone->runtimeFromMainThread()->isHeapMajorCollecting());

    AutoMarkInDeadZone amn(zone);

    if (kind == JSTRACE_OBJECT)
        JSObject::writeBarrierPre(static_cast<JSObject*>(cell));
    else if (kind == JSTRACE_STRING)
        JSString::writeBarrierPre(static_cast<JSString*>(cell));
    else if (kind == JSTRACE_SCRIPT)
        JSScript::writeBarrierPre(static_cast<JSScript*>(cell));
    else if (kind == JSTRACE_LAZY_SCRIPT)
        LazyScript::writeBarrierPre(static_cast<LazyScript*>(cell));
    else if (kind == JSTRACE_SHAPE)
        Shape::writeBarrierPre(static_cast<Shape*>(cell));
    else if (kind == JSTRACE_BASE_SHAPE)
        BaseShape::writeBarrierPre(static_cast<BaseShape*>(cell));
    else if (kind == JSTRACE_TYPE_OBJECT)
        types::TypeObject::writeBarrierPre((types::TypeObject *) ptr);
    else
        MOZ_ASSUME_UNREACHABLE("invalid trace kind");
}

JS_FRIEND_API(void)
JS::IncrementalValueBarrier(const Value &v)
{
    js::HeapValue::writeBarrierPre(v);
}

JS_FRIEND_API(void)
JS::PokeGC(JSRuntime *rt)
{
    rt->gcPoke = true;
}

JS_FRIEND_API(JSCompartment *)
js::GetAnyCompartmentInZone(JS::Zone *zone)
{
    CompartmentsInZoneIter comp(zone);
    JS_ASSERT(!comp.done());
    return comp.get();
}

bool
JS::ObjectPtr::isAboutToBeFinalized()
{
    return JS_IsAboutToBeFinalized(&value);
}

void
JS::ObjectPtr::trace(JSTracer *trc, const char *name)
{
    JS_CallHeapObjectTracer(trc, &value, name);
}

JS_FRIEND_API(JSObject *)
js::GetTestingFunctions(JSContext *cx)
{
    RootedObject obj(cx, JS_NewObject(cx, NULL, NULL, NULL));
    if (!obj)
        return NULL;

    if (!DefineTestingFunctions(cx, obj))
        return NULL;

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

static void *gDOMProxyHandlerFamily = NULL;
static uint32_t gDOMProxyExpandoSlot = 0;
static DOMProxyShadowsCheck gDOMProxyShadowsCheck;

JS_FRIEND_API(void)
js::SetDOMProxyInformation(void *domProxyHandlerFamily, uint32_t domProxyExpandoSlot,
                           DOMProxyShadowsCheck domProxyShadowsCheck)
{
    gDOMProxyHandlerFamily = domProxyHandlerFamily;
    gDOMProxyExpandoSlot = domProxyExpandoSlot;
    gDOMProxyShadowsCheck = domProxyShadowsCheck;
}

void *
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
    // Clear any jitcode in the runtime, which behaves differently depending on
    // whether there is a creation callback.
    ReleaseAllJITCode(cx->runtime()->defaultFreeOp());

    cx->compartment()->objectMetadataCallback = callback;
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

JS_FRIEND_API(bool)
js_DefineOwnProperty(JSContext *cx, JSObject *objArg, jsid idArg,
                     JS::Handle<js::PropertyDescriptor> descriptor, bool *bp)
{
    RootedObject obj(cx, objArg);
    RootedId id(cx, idArg);
    JS_ASSERT(cx->runtime()->heapState == js::Idle);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id, descriptor.value());
    if (descriptor.hasGetterObject())
        assertSameCompartment(cx, descriptor.getterObject());
    if (descriptor.hasSetterObject())
        assertSameCompartment(cx, descriptor.setterObject());

    return DefineOwnProperty(cx, HandleObject(obj), id, descriptor, bp);
}

JS_FRIEND_API(bool)
js_ReportIsNotFunction(JSContext *cx, const JS::Value& v)
{
    return ReportIsNotFunction(cx, v);
}

#ifdef DEBUG
JS_PUBLIC_API(bool)
js::IsInRequest(JSContext *cx)
{
#ifdef JS_THREADSAFE
    return !!cx->runtime()->requestDepth;
#else
    return true;
#endif
}
#endif

void
AsmJSModuleSourceDesc::init(ScriptSource *scriptSource, uint32_t bufStart, uint32_t bufEnd)
{
    JS_ASSERT(scriptSource_ == NULL);
    scriptSource_ = scriptSource;
    bufStart_ = bufStart;
    bufEnd_ = bufEnd;
    scriptSource_->incref();
}

AsmJSModuleSourceDesc::~AsmJSModuleSourceDesc()
{
    if (scriptSource_)
        scriptSource_->decref();
}

#ifdef JSGC_GENERATIONAL
JS_FRIEND_API(void)
JS_StoreObjectPostBarrierCallback(JSContext* cx,
                                  void (*callback)(JSTracer *trc, void *key, void *data),
                                  JSObject *key, void *data)
{
    cx->runtime()->gcStoreBuffer.putCallback(callback, key, data);
}

extern JS_FRIEND_API(void)
JS_StoreStringPostBarrierCallback(JSContext* cx,
                                  void (*callback)(JSTracer *trc, void *key, void *data),
                                  JSString *key, void *data)
{
    cx->runtime()->gcStoreBuffer.putCallback(callback, key, data);
}
#endif /* JSGC_GENERATIONAL */

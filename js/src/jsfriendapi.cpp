/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/GuardObjects.h"
#include "mozilla/StandardInteger.h"

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsfriendapi.h"
#include "jswrapper.h"
#include "jsweakmap.h"
#include "jswatchpoint.h"

#include "builtin/TestingFunctions.h"

#include "jsobjinlines.h"

using namespace js;
using namespace JS;

// Required by PerThreadDataFriendFields::getMainThread()
JS_STATIC_ASSERT(offsetof(JSRuntime, mainThread) == sizeof(RuntimeFriendFields));

PerThreadDataFriendFields::PerThreadDataFriendFields()
{
#if defined(JSGC_ROOT_ANALYSIS) || defined(JSGC_USE_EXACT_ROOTING)
    PodArrayZero(thingGCRooters);
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
    rt->gcGrayRootsTraceOp = traceOp;
    rt->gcGrayRootsData = data;
}

JS_FRIEND_API(JSString *)
JS_GetAnonymousString(JSRuntime *rt)
{
    JS_ASSERT(rt->hasContexts());
    return rt->atomState.anonymous;
}

JS_FRIEND_API(JSObject *)
JS_FindCompilationScope(JSContext *cx, RawObject objArg)
{
    RootedObject obj(cx, objArg);

    /*
     * We unwrap wrappers here. This is a little weird, but it's what's being
     * asked of us.
     */
    if (obj->isWrapper())
        obj = UnwrapObject(obj);

    /*
     * Innerize the target_obj so that we compile in the correct (inner)
     * scope.
     */
    if (JSObjectOp op = obj->getClass()->ext.innerObject)
        obj = op(cx, obj);
    return obj;
}

JS_FRIEND_API(JSFunction *)
JS_GetObjectFunction(RawObject obj)
{
    if (obj->isFunction())
        return obj->toFunction();
    return NULL;
}

JS_FRIEND_API(JSObject *)
JS_GetGlobalForFrame(JSStackFrame *fp)
{
    return &Valueify(fp)->global();
}

JS_FRIEND_API(JSBool)
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
    return obj->splicePrototype(cx, tagged);
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
    RootedObject obj(cx, JS_NewObjectWithGivenProto(cx, clasp, NULL, parent));
    if (!obj || !JSObject::setSingletonType(cx, obj))
        return NULL;
    if (!JS_SplicePrototype(cx, obj, proto))
        return NULL;
    return obj;
}

JS_FRIEND_API(void)
js::PrepareCompartmentForGC(JSCompartment *comp)
{
    comp->scheduleGC();
}

JS_FRIEND_API(void)
js::PrepareForFullGC(JSRuntime *rt)
{
    for (CompartmentsIter c(rt); !c.done(); c.next())
        c->scheduleGC();
}

JS_FRIEND_API(void)
js::PrepareForIncrementalGC(JSRuntime *rt)
{
    if (!IsIncrementalGCInProgress(rt))
        return;

    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        if (c->wasGCStarted())
            PrepareCompartmentForGC(c);
    }
}

JS_FRIEND_API(bool)
js::IsGCScheduled(JSRuntime *rt)
{
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        if (c->isGCScheduled())
            return true;
    }

    return false;
}

JS_FRIEND_API(void)
js::SkipCompartmentForGC(JSCompartment *comp)
{
    comp->unscheduleGC();
}

JS_FRIEND_API(void)
js::GCForReason(JSRuntime *rt, gcreason::Reason reason)
{
    GC(rt, GC_NORMAL, reason);
}

JS_FRIEND_API(void)
js::ShrinkingGC(JSRuntime *rt, gcreason::Reason reason)
{
    GC(rt, GC_SHRINK, reason);
}

JS_FRIEND_API(void)
js::IncrementalGC(JSRuntime *rt, gcreason::Reason reason, int64_t millis)
{
    GCSlice(rt, GC_NORMAL, reason, millis);
}

JS_FRIEND_API(void)
js::FinishIncrementalGC(JSRuntime *rt, gcreason::Reason reason)
{
    GCFinalSlice(rt, GC_NORMAL, reason);
}

JS_FRIEND_API(void)
JS_ShrinkGCBuffers(JSRuntime *rt)
{
    ShrinkGCBuffers(rt);
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
    JSPrincipals *trusted = compartment->rt->trustedPrincipals();
    bool isSystem = principals && principals == trusted;

    // Clear out the old principals, if any.
    if (compartment->principals) {
        JS_DropPrincipals(compartment->rt, compartment->principals);
        compartment->principals = NULL;
        // We'd like to assert that our new principals is always same-origin
        // with the old one, but JSPrincipals doesn't give us a way to do that.
        // But we can at least assert that we're not switching between system
        // and non-system.
        JS_ASSERT(compartment->isSystemCompartment == isSystem);
    }

    // Set up the new principals.
    if (principals) {
        JS_HoldPrincipals(principals);
        compartment->principals = principals;
    }

    // Update the system flag.
    compartment->isSystemCompartment = isSystem;
}

JS_FRIEND_API(JSBool)
JS_WrapPropertyDescriptor(JSContext *cx, js::PropertyDescriptor *desc)
{
    return cx->compartment->wrap(cx, desc);
}

JS_FRIEND_API(JSBool)
JS_WrapAutoIdVector(JSContext *cx, js::AutoIdVector &props)
{
    return cx->compartment->wrap(cx, props);
}

JS_FRIEND_API(void)
JS_TraceShapeCycleCollectorChildren(JSTracer *trc, void *shape)
{
    MarkCycleCollectorChildren(trc, static_cast<RawShape>(shape));
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
    JS_ASSERT(cx->compartment != cx->runtime->atomsCompartment);

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    for (; fs->name; fs++) {
        JSAtom *atom = Atomize(cx, fs->name, strlen(fs->name));
        if (!atom)
            return false;

        Rooted<jsid> id(cx, AtomToId(atom));
        RootedFunction fun(cx, js_DefineFunction(cx, obj, id, fs->call, fs->nargs, fs->flags));
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

AutoSwitchCompartment::AutoSwitchCompartment(JSContext *cx, JSCompartment *newCompartment
                                             MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : cx(cx), oldCompartment(cx->compartment)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    cx->setCompartment(newCompartment);
}

AutoSwitchCompartment::AutoSwitchCompartment(JSContext *cx, JSHandleObject target
                                             MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : cx(cx), oldCompartment(cx->compartment)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    cx->setCompartment(target->compartment());
}

AutoSwitchCompartment::~AutoSwitchCompartment()
{
    /* The old compartment may have been destroyed, so we can't use cx->setCompartment. */
    cx->compartment = oldCompartment;
}

JS_FRIEND_API(bool)
js::IsSystemCompartment(const JSCompartment *c)
{
    return c->isSystemCompartment;
}

JS_FRIEND_API(bool)
js::IsAtomsCompartment(const JSCompartment *c)
{
    return c == c->rt->atomsCompartment;
}

JS_FRIEND_API(bool)
js::IsScopeObject(RawObject obj)
{
    return obj->isScope();
}

JS_FRIEND_API(JSObject *)
js::GetObjectParentMaybeScope(RawObject obj)
{
    return obj->enclosingScope();
}

JS_FRIEND_API(JSObject *)
js::GetGlobalForObjectCrossCompartment(RawObject obj)
{
    return &obj->global();
}

JS_FRIEND_API(void)
js::NotifyAnimationActivity(RawObject obj)
{
    obj->compartment()->lastAnimationTime = PRMJ_Now();
}

JS_FRIEND_API(uint32_t)
js::GetObjectSlotSpan(RawObject obj)
{
    return obj->slotSpan();
}

JS_FRIEND_API(bool)
js::IsObjectInContextCompartment(RawObject obj, const JSContext *cx)
{
    return obj->compartment() == cx->compartment;
}

JS_FRIEND_API(bool)
js::IsOriginalScriptFunction(JSFunction *fun)
{
    return fun->nonLazyScript()->function() == fun;
}

JS_FRIEND_API(JSScript *)
js::GetOutermostEnclosingFunctionOfScriptedCaller(JSContext *cx)
{
    if (!cx->hasfp())
        return NULL;

    StackFrame *fp = cx->fp();
    if (!fp->isFunctionFrame())
        return NULL;

    JSFunction *scriptedCaller = fp->fun();
    RootedScript outermost(cx, scriptedCaller->nonLazyScript());
    for (StaticScopeIter i(scriptedCaller); !i.done(); i++) {
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
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return NULL;
    Rooted<jsid> id(cx, AtomToId(atom));
    return js_DefineFunction(cx, obj, id, call, nargs, attrs, JSFunction::ExtendedFinalizeKind);
}

JS_FRIEND_API(JSFunction *)
js::NewFunctionWithReserved(JSContext *cx, JSNative native, unsigned nargs, unsigned flags,
                            JSObject *parentArg, const char *name)
{
    RootedObject parent(cx, parentArg);
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);

    RootedAtom atom(cx);
    if (name) {
        atom = Atomize(cx, name, strlen(name));
        if (!atom)
            return NULL;
    }

    JSFunction::Flags funFlags = JSAPIToJSFunctionFlags(flags);
    return js_NewFunction(cx, NullPtr(), native, nargs, funFlags, parent, atom,
                          JSFunction::ExtendedFinalizeKind);
}

JS_FRIEND_API(JSFunction *)
js::NewFunctionByIdWithReserved(JSContext *cx, JSNative native, unsigned nargs, unsigned flags, JSObject *parentArg,
                                jsid id)
{
    RootedObject parent(cx, parentArg);
    JS_ASSERT(JSID_IS_STRING(id));
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);

    RootedAtom atom(cx, JSID_TO_ATOM(id));
    JSFunction::Flags funFlags = JSAPIToJSFunctionFlags(flags);
    return js_NewFunction(cx, NullPtr(), native, nargs, funFlags, parent, atom,
                          JSFunction::ExtendedFinalizeKind);
}

JS_FRIEND_API(JSObject *)
js::InitClassWithReserved(JSContext *cx, JSObject *objArg, JSObject *parent_protoArg,
                          JSClass *clasp, JSNative constructor, unsigned nargs,
                          JSPropertySpec *ps, JSFunctionSpec *fs,
                          JSPropertySpec *static_ps, JSFunctionSpec *static_fs)
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
js::GetFunctionNativeReserved(RawObject fun, size_t which)
{
    JS_ASSERT(fun->toFunction()->isNative());
    return fun->toFunction()->getExtendedSlot(which);
}

JS_FRIEND_API(void)
js::SetFunctionNativeReserved(RawObject fun, size_t which, const Value &val)
{
    JS_ASSERT(fun->toFunction()->isNative());
    fun->toFunction()->setExtendedSlot(which, val);
}

JS_FRIEND_API(void)
js::SetReservedSlotWithBarrier(RawObject obj, size_t slot, const js::Value &value)
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

// Defined in jsxml.cpp.
extern size_t sE4XObjectsCreated;

JS_FRIEND_API(size_t)
JS_GetE4XObjectsCreated(JSContext *)
{
    return sE4XObjectsCreated;
}

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

JS_FRIEND_API(JSBool)
JS_IsDeadWrapper(JSObject *obj)
{
    if (!IsProxy(obj)) {
        return false;
    }

    BaseProxyHandler *handler = GetProxyHandler(obj);
    return handler->family() == &DeadObjectProxy::sDeadObjectFamily;
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
js::VisitGrayWrapperTargets(JSCompartment *comp, GCThingCallback callback, void *closure)
{
    for (JSCompartment::WrapperEnum e(comp); !e.empty(); e.popFront()) {
        gc::Cell *thing = e.front().key.wrapped;
        if (thing->isMarked(gc::GRAY))
            callback(closure, thing);
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
DumpHeapVisitCompartment(JSRuntime *rt, void *data, JSCompartment *comp)
{
    char name[1024];
    if (rt->compartmentNameCallback)
        (*rt->compartmentNameCallback)(rt, comp, name, sizeof(name));
    else
        strcpy(name, "<unknown>");

    JSDumpHeapTracer *dtrc = static_cast<JSDumpHeapTracer *>(data);
    fprintf(dtrc->output, "# compartment %s\n", name);
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
    char cellDesc[1024];
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
    TraceRuntime(&dtrc);

    fprintf(dtrc.output, "==========\n");

    JS_TracerInit(&dtrc, rt, DumpHeapVisitChild);
    IterateCompartmentsArenasCells(rt, &dtrc,
                                   DumpHeapVisitCompartment,
                                   DumpHeapVisitArena,
                                   DumpHeapVisitCell);

    fflush(dtrc.output);
}

JS_FRIEND_API(const JSStructuredCloneCallbacks *)
js::GetContextStructuredCloneCallbacks(JSContext *cx)
{
    return cx->runtime->structuredCloneCallbacks;
}

JS_FRIEND_API(JSVersion)
js::VersionSetMoarXML(JSVersion version, bool enable)
{
    return enable ? JSVersion(uint32_t(version) | VersionFlags::MOAR_XML)
                  : JSVersion(uint32_t(version) & ~VersionFlags::MOAR_XML);
}

JS_FRIEND_API(bool)
js::CanCallContextDebugHandler(JSContext *cx)
{
    return !!cx->runtime->debugHooks.debuggerHandler;
}

JS_FRIEND_API(JSTrapStatus)
js::CallContextDebugHandler(JSContext *cx, JSScript *script, jsbytecode *bc, Value *rval)
{
    if (!cx->runtime->debugHooks.debuggerHandler)
        return JSTRAP_RETURN;

    return cx->runtime->debugHooks.debuggerHandler(cx, script, bc, rval,
                                                   cx->runtime->debugHooks.debuggerHandlerData);
}

#ifdef JS_THREADSAFE
void *
js::GetOwnerThread(const JSContext *cx)
{
    return cx->runtime->ownerThread();
}

JS_FRIEND_API(bool)
js::ContextHasOutstandingRequests(const JSContext *cx)
{
    return cx->outstandingRequests > 0;
}
#endif

JS_FRIEND_API(bool)
js::HasUnrootedGlobal(const JSContext *cx)
{
    return cx->hasRunOption(JSOPTION_UNROOTED_GLOBAL);
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
    return !cx->stack.empty();
}

JS_FRIEND_API(const CompartmentVector&)
js::GetRuntimeCompartments(JSRuntime *rt)
{
    return rt->compartments;
}

JS_FRIEND_API(GCSliceCallback)
js::SetGCSliceCallback(JSRuntime *rt, GCSliceCallback callback)
{
    GCSliceCallback old = rt->gcSliceCallback;
    rt->gcSliceCallback = callback;
    return old;
}

JS_FRIEND_API(bool)
js::WasIncrementalGC(JSRuntime *rt)
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

JS_FRIEND_API(AnalysisPurgeCallback)
js::SetAnalysisPurgeCallback(JSRuntime *rt, AnalysisPurgeCallback callback)
{
    AnalysisPurgeCallback old = rt->analysisPurgeCallback;
    rt->analysisPurgeCallback = callback;
    return old;
}

JS_FRIEND_API(void)
js::NotifyDidPaint(JSRuntime *rt)
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

    if (IsIncrementalGCInProgress(rt) && !rt->gcInterFrameGC) {
        PrepareForIncrementalGC(rt);
        GCSlice(rt, GC_NORMAL, gcreason::REFRESH_FRAME);
    }

    rt->gcInterFrameGC = false;
}

JS_FRIEND_API(bool)
js::IsIncrementalGCEnabled(JSRuntime *rt)
{
    return rt->gcIncrementalEnabled && rt->gcMode == JSGC_MODE_INCREMENTAL;
}

JS_FRIEND_API(bool)
js::IsIncrementalGCInProgress(JSRuntime *rt)
{
    return (rt->gcIncrementalState != gc::NO_INCREMENTAL && !rt->gcVerifyPreData);
}

JS_FRIEND_API(void)
js::DisableIncrementalGC(JSRuntime *rt)
{
    rt->gcIncrementalEnabled = false;
}

JS_FRIEND_API(bool)
js::IsIncrementalBarrierNeeded(JSRuntime *rt)
{
    return (rt->gcIncrementalState == gc::MARK && !rt->isHeapBusy());
}

JS_FRIEND_API(bool)
js::IsIncrementalBarrierNeeded(JSContext *cx)
{
    return IsIncrementalBarrierNeeded(cx->runtime);
}

JS_FRIEND_API(void)
js::IncrementalReferenceBarrier(void *ptr)
{
    if (!ptr)
        return;

    gc::Cell *cell = static_cast<gc::Cell *>(ptr);
    JS_ASSERT(!cell->compartment()->rt->isHeapBusy());

    AutoMarkInDeadCompartment amn(cell->compartment());

    uint32_t kind = gc::GetGCThingTraceKind(ptr);
    if (kind == JSTRACE_OBJECT)
        JSObject::writeBarrierPre(reinterpret_cast<RawObject>(ptr));
    else if (kind == JSTRACE_STRING)
        JSString::writeBarrierPre(reinterpret_cast<RawString>(ptr));
    else if (kind == JSTRACE_SCRIPT)
        JSScript::writeBarrierPre(reinterpret_cast<RawScript>(ptr));
    else if (kind == JSTRACE_SHAPE)
        Shape::writeBarrierPre(reinterpret_cast<RawShape>(ptr));
    else if (kind == JSTRACE_BASE_SHAPE)
        BaseShape::writeBarrierPre(reinterpret_cast<RawBaseShape>(ptr));
    else if (kind == JSTRACE_TYPE_OBJECT)
        types::TypeObject::writeBarrierPre((types::TypeObject *) ptr);
    else
        JS_NOT_REACHED("invalid trace kind");
}

JS_FRIEND_API(void)
js::IncrementalValueBarrier(const Value &v)
{
    HeapValue::writeBarrierPre(v);
}

JS_FRIEND_API(void)
js::PokeGC(JSRuntime *rt)
{
    rt->gcPoke = true;
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

JS_FRIEND_API(void)
js::SetRuntimeProfilingStack(JSRuntime *rt, ProfileEntry *stack, uint32_t *size, uint32_t max)
{
    rt->spsProfiler.setProfilingStack(stack, size, max);
}

JS_FRIEND_API(void)
js::EnableRuntimeProfilingStack(JSRuntime *rt, bool enabled)
{
    rt->spsProfiler.enable(enabled);
}

JS_FRIEND_API(jsbytecode*)
js::ProfilingGetPC(JSRuntime *rt, JSScript *script, void *ip)
{
    return rt->spsProfiler.ipToPC(script, size_t(ip));
}

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

static void *gListBaseHandlerFamily = NULL;
static uint32_t gListBaseExpandoSlot = 0;

JS_FRIEND_API(void)
js::SetListBaseInformation(void *listBaseHandlerFamily, uint32_t listBaseExpandoSlot)
{
    gListBaseHandlerFamily = listBaseHandlerFamily;
    gListBaseExpandoSlot = listBaseExpandoSlot;
}

void *
js::GetListBaseHandlerFamily()
{
    return gListBaseHandlerFamily;
}

uint32_t
js::GetListBaseExpandoSlot()
{
    return gListBaseExpandoSlot;
}

JS_FRIEND_API(void)
js::SetCTypesActivityCallback(JSRuntime *rt, CTypesActivityCallback cb)
{
    rt->ctypesActivityCallback = cb;
}

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
    return rt->atomState.anonymousAtom;
}

JS_FRIEND_API(JSObject *)
JS_FindCompilationScope(JSContext *cx, JSObject *obj_)
{
    RootedObject obj(cx, obj_);

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
JS_GetObjectFunction(JSObject *obj)
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
JS_SplicePrototype(JSContext *cx, JSObject *obj, JSObject *proto)
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

    return obj->splicePrototype(cx, proto);
}

JS_FRIEND_API(JSObject *)
JS_NewObjectWithUniqueType(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent)
{
    JSObject *obj = JS_NewObject(cx, clasp, proto, parent);
    if (!obj || !obj->setSingletonType(cx))
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
    if (rt->gcIncrementalState == gc::NO_INCREMENTAL)
        return;

    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        if (c->needsBarrier())
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
js::IncrementalGC(JSRuntime *rt, gcreason::Reason reason)
{
    GCSlice(rt, GC_NORMAL, reason);
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

    // Clear out the old principals, if any.
    if (compartment->principals) {
        JS_DropPrincipals(compartment->rt, compartment->principals);
        compartment->principals = NULL;
    }

    // Set up the new principals.
    if (principals) {
        JS_HoldPrincipals(principals);
        compartment->principals = principals;
    }

    // Any compartment with the trusted principals -- and there can be
    // multiple -- is a system compartment.
    JSPrincipals *trusted = compartment->rt->trustedPrincipals();
    compartment->isSystemCompartment = principals && principals == trusted;
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
    MarkCycleCollectorChildren(trc, (Shape *)shape);
}

static bool
DefineHelpProperty(JSContext *cx, HandleObject obj, const char *prop, const char *value)
{
    JSAtom *atom = js_Atomize(cx, value, strlen(value));
    if (!atom)
        return false;
    jsval v = STRING_TO_JSVAL(atom);
    return JS_DefineProperty(cx, obj, prop, v,
                             JS_PropertyStub, JS_StrictPropertyStub,
                             JSPROP_READONLY | JSPROP_PERMANENT);
}

JS_FRIEND_API(bool)
JS_DefineFunctionsWithHelp(JSContext *cx, JSObject *obj_, const JSFunctionSpecWithHelp *fs)
{
    RootedObject obj(cx, obj_);

    JS_ASSERT(cx->compartment != cx->runtime->atomsCompartment);

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    for (; fs->name; fs++) {
        JSAtom *atom = js_Atomize(cx, fs->name, strlen(fs->name));
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
                                             JS_GUARD_OBJECT_NOTIFIER_PARAM_NO_INIT)
  : cx(cx), oldCompartment(cx->compartment)
{
    JS_GUARD_OBJECT_NOTIFIER_INIT;
    cx->setCompartment(newCompartment);
}

AutoSwitchCompartment::AutoSwitchCompartment(JSContext *cx, JSObject *target
                                             JS_GUARD_OBJECT_NOTIFIER_PARAM_NO_INIT)
  : cx(cx), oldCompartment(cx->compartment)
{
    JS_GUARD_OBJECT_NOTIFIER_INIT;
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
js::IsScopeObject(JSObject *obj)
{
    return obj->isScope();
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
js::IsObjectInContextCompartment(const JSObject *obj, const JSContext *cx)
{
    return obj->compartment() == cx->compartment;
}

JS_FRIEND_API(bool)
js::IsOriginalScriptFunction(JSFunction *fun)
{
    return fun->script()->function() == fun;
}

JS_FRIEND_API(JSFunction *)
js::DefineFunctionWithReserved(JSContext *cx, JSObject *obj_, const char *name, JSNative call,
                               unsigned nargs, unsigned attrs)
{
    RootedObject obj(cx, obj_);

    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    if (!atom)
        return NULL;
    Rooted<jsid> id(cx, AtomToId(atom));
    return js_DefineFunction(cx, obj, id, call, nargs, attrs, JSFunction::ExtendedFinalizeKind);
}

JS_FRIEND_API(JSFunction *)
js::NewFunctionWithReserved(JSContext *cx, JSNative native, unsigned nargs, unsigned flags,
                            JSObject *parent_, const char *name)
{
    RootedObject parent(cx, parent_);

    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    JSAtom *atom;

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);

    if (!name) {
        atom = NULL;
    } else {
        atom = js_Atomize(cx, name, strlen(name));
        if (!atom)
            return NULL;
    }

    return js_NewFunction(cx, NULL, native, nargs, flags, parent, atom,
                          JSFunction::ExtendedFinalizeKind);
}

JS_FRIEND_API(JSFunction *)
js::NewFunctionByIdWithReserved(JSContext *cx, JSNative native, unsigned nargs, unsigned flags, JSObject *parent_,
                                jsid id)
{
    RootedObject parent(cx, parent_);

    JS_ASSERT(JSID_IS_STRING(id));
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);

    return js_NewFunction(cx, NULL, native, nargs, flags, parent, JSID_TO_ATOM(id),
                          JSFunction::ExtendedFinalizeKind);
}

JS_FRIEND_API(JSObject *)
js::InitClassWithReserved(JSContext *cx, JSObject *obj_, JSObject *parent_proto,
                          JSClass *clasp, JSNative constructor, unsigned nargs,
                          JSPropertySpec *ps, JSFunctionSpec *fs,
                          JSPropertySpec *static_ps, JSFunctionSpec *static_fs)
{
    RootedObject obj(cx, obj_);

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, parent_proto);
    return js_InitClass(cx, obj, parent_proto, Valueify(clasp), constructor,
                        nargs, ps, fs, static_ps, static_fs, NULL,
                        JSFunction::ExtendedFinalizeKind);
}

JS_FRIEND_API(const Value &)
js::GetFunctionNativeReserved(JSObject *fun, size_t which)
{
    JS_ASSERT(fun->toFunction()->isNative());
    return fun->toFunction()->getExtendedSlot(which);
}

JS_FRIEND_API(void)
js::SetFunctionNativeReserved(JSObject *fun, size_t which, const Value &val)
{
    JS_ASSERT(fun->toFunction()->isNative());
    fun->toFunction()->setExtendedSlot(which, val);
}

JS_FRIEND_API(void)
js::SetReservedSlotWithBarrier(JSObject *obj, size_t slot, const js::Value &value)
{
    obj->setSlot(slot, value);
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

extern size_t sE4XObjectsCreated;

JS_FRIEND_API(size_t)
JS_GetE4XObjectsCreated(JSContext *)
{
    return sE4XObjectsCreated;
}

extern size_t sSetProtoCalled;

JS_FRIEND_API(size_t)
JS_SetProtoCalled(JSContext *)
{
    return sSetProtoCalled;
}

extern size_t sCustomIteratorCount;

JS_FRIEND_API(size_t)
JS_GetCustomIteratorCount(JSContext *cx)
{
    return sCustomIteratorCount;
}

void
js::TraceWeakMaps(WeakMapTracer *trc)
{
    WeakMapBase::traceAllMappings(trc);
    WatchpointMap::traceAll(trc);
}

JS_FRIEND_API(bool)
js::GCThingIsMarkedGray(void *thing)
{
    JS_ASSERT(thing);
    return reinterpret_cast<gc::Cell *>(thing)->isMarked(gc::GRAY);
}

JS_FRIEND_API(JSCompartment*)
js::GetGCThingCompartment(void *thing)
{
    JS_ASSERT(thing);
    return reinterpret_cast<gc::Cell *>(thing)->compartment();
}

JS_FRIEND_API(void)
js::VisitGrayWrapperTargets(JSCompartment *comp, GCThingCallback *callback, void *closure)
{
    for (WrapperMap::Enum e(comp->crossCompartmentWrappers); !e.empty(); e.popFront()) {
        gc::Cell *thing = e.front().key.wrapped;
        if (thing->isMarked(gc::GRAY))
            callback(closure, thing);
    }
}

JS_FRIEND_API(void)
JS_SetAccumulateTelemetryCallback(JSRuntime *rt, JSAccumulateTelemetryDataCallback callback)
{
    rt->telemetryCallback = callback;
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

extern void
DumpChars(const jschar *s, size_t n)
{
    if (n == SIZE_MAX) {
        n = 0;
        while (s[n])
            n++;
    }

    fputc('"', stderr);
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\n')
            fprintf(stderr, "\\n");
        else if (s[i] == '\t')
            fprintf(stderr, "\\t");
        else if (s[i] >= 32 && s[i] < 127)
            fputc(s[i], stderr);
        else if (s[i] <= 255)
            fprintf(stderr, "\\x%02x", (unsigned int) s[i]);
        else
            fprintf(stderr, "\\u%04x", (unsigned int) s[i]);
    }
    fputc('"', stderr);
}

JS_FRIEND_API(void)
js_DumpChars(const jschar *s, size_t n)
{
    fprintf(stderr, "jschar * (%p) = ", (void *) s);
    DumpChars(s, n);
    fputc('\n', stderr);
}

JS_FRIEND_API(void)
js_DumpObject(JSObject *obj)
{
    obj->dump();
}

#endif

struct DumpingChildInfo {
    void *node;
    JSGCTraceKind kind;

    DumpingChildInfo (void *n, JSGCTraceKind k)
        : node(n), kind(k)
    {}
};

typedef HashSet<void *, DefaultHasher<void *>, SystemAllocPolicy> PtrSet;

struct JSDumpHeapTracer : public JSTracer {
    PtrSet visited;
    FILE   *output;
    Vector<DumpingChildInfo, 0, SystemAllocPolicy> nodes;
    char   buffer[200];
    bool   rootTracing;

    JSDumpHeapTracer(FILE *fp)
      : output(fp)
    {}
};

static void
DumpHeapVisitChild(JSTracer *trc, void **thingp, JSGCTraceKind kind);

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
DumpHeapPushIfNew(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    JS_ASSERT(trc->callback == DumpHeapPushIfNew ||
              trc->callback == DumpHeapVisitChild);
    void *thing = *thingp;
    JSDumpHeapTracer *dtrc = static_cast<JSDumpHeapTracer *>(trc);

    /*
     * If we're tracing roots, print root information.  Do this even if we've
     * already seen thing, for complete root information.
     */
    if (dtrc->rootTracing) {
        fprintf(dtrc->output, "%p %c %s\n", thing, MarkDescriptor(thing),
                JS_GetTraceEdgeName(dtrc, dtrc->buffer, sizeof(dtrc->buffer)));
    }

    PtrSet::AddPtr ptrEntry = dtrc->visited.lookupForAdd(thing);
    if (ptrEntry || !dtrc->visited.add(ptrEntry, thing))
        return;

    dtrc->nodes.append(DumpingChildInfo(thing, kind));
}

static void
DumpHeapVisitChild(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    JS_ASSERT(trc->callback == DumpHeapVisitChild);
    JSDumpHeapTracer *dtrc = static_cast<JSDumpHeapTracer *>(trc);
    const char *edgeName = JS_GetTraceEdgeName(dtrc, dtrc->buffer, sizeof(dtrc->buffer));
    fprintf(dtrc->output, "> %p %c %s\n", *thingp, MarkDescriptor(*thingp), edgeName);
    DumpHeapPushIfNew(dtrc, thingp, kind);
}

void
js::DumpHeapComplete(JSRuntime *rt, FILE *fp)
{
    JSDumpHeapTracer dtrc(fp);
    JS_TracerInit(&dtrc, rt, DumpHeapPushIfNew);
    if (!dtrc.visited.init(10000))
        return;

    /* Store and log the root information. */
    dtrc.rootTracing = true;
    TraceRuntime(&dtrc);
    fprintf(dtrc.output, "==========\n");

    /* Log the graph. */
    dtrc.rootTracing = false;
    dtrc.callback = DumpHeapVisitChild;

    while (!dtrc.nodes.empty()) {
        DumpingChildInfo dci = dtrc.nodes.popCopy();
        JS_GetTraceThingInfo(dtrc.buffer, sizeof(dtrc.buffer),
                             &dtrc, dci.node, dci.kind, JS_TRUE);
        fprintf(fp, "%p %c %s\n", dci.node, MarkDescriptor(dci.node), dtrc.buffer);
        JS_TraceChildren(&dtrc, dci.node, dci.kind);
    }

    dtrc.visited.finish();
    fflush(dtrc.output);
}

namespace js {

JS_FRIEND_API(const JSStructuredCloneCallbacks *)
GetContextStructuredCloneCallbacks(JSContext *cx)
{
    return cx->runtime->structuredCloneCallbacks;
}

JS_FRIEND_API(JSVersion)
VersionSetMoarXML(JSVersion version, bool enable)
{
    return enable ? JSVersion(uint32_t(version) | VersionFlags::MOAR_XML)
                  : JSVersion(uint32_t(version) & ~VersionFlags::MOAR_XML);
}

JS_FRIEND_API(bool)
CanCallContextDebugHandler(JSContext *cx)
{
    return !!cx->runtime->debugHooks.debuggerHandler;
}

JS_FRIEND_API(JSTrapStatus)
CallContextDebugHandler(JSContext *cx, JSScript *script, jsbytecode *bc, Value *rval)
{
    if (!cx->runtime->debugHooks.debuggerHandler)
        return JSTRAP_RETURN;

    return cx->runtime->debugHooks.debuggerHandler(cx, script, bc, rval,
                                                   cx->runtime->debugHooks.debuggerHandlerData);
}

#ifdef JS_THREADSAFE
void *
GetOwnerThread(const JSContext *cx)
{
    return cx->runtime->ownerThread();
}

JS_FRIEND_API(unsigned)
GetContextOutstandingRequests(const JSContext *cx)
{
    return cx->outstandingRequests;
}
#endif

JS_FRIEND_API(JSCompartment *)
GetContextCompartment(const JSContext *cx)
{
    return cx->compartment;
}

JS_FRIEND_API(bool)
HasUnrootedGlobal(const JSContext *cx)
{
    return cx->hasRunOption(JSOPTION_UNROOTED_GLOBAL);
}

JS_FRIEND_API(void)
SetActivityCallback(JSRuntime *rt, ActivityCallback cb, void *arg)
{
    rt->activityCallback = cb;
    rt->activityCallbackArg = arg;
}

JS_FRIEND_API(bool)
IsContextRunningJS(JSContext *cx)
{
    return !cx->stack.empty();
}

JS_FRIEND_API(const CompartmentVector&)
GetRuntimeCompartments(JSRuntime *rt)
{
    return rt->compartments;
}

JS_FRIEND_API(size_t)
SizeOfJSContext()
{
    return sizeof(JSContext);
}

JS_FRIEND_API(GCSliceCallback)
SetGCSliceCallback(JSRuntime *rt, GCSliceCallback callback)
{
    GCSliceCallback old = rt->gcSliceCallback;
    rt->gcSliceCallback = callback;
    return old;
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
NotifyDidPaint(JSRuntime *rt)
{
    if (rt->gcZeal() == gc::ZealFrameVerifierValue) {
        gc::VerifyBarriers(rt);
        return;
    }

    if (rt->gcZeal() == gc::ZealFrameGCValue) {
        PrepareForFullGC(rt);
        GCSlice(rt, GC_NORMAL, gcreason::REFRESH_FRAME);
        return;
    }

    if (rt->gcIncrementalState != gc::NO_INCREMENTAL && !rt->gcInterFrameGC) {
        PrepareForIncrementalGC(rt);
        GCSlice(rt, GC_NORMAL, gcreason::REFRESH_FRAME);
    }

    rt->gcInterFrameGC = false;
}

extern JS_FRIEND_API(bool)
IsIncrementalGCEnabled(JSRuntime *rt)
{
    return rt->gcIncrementalEnabled && rt->gcMode == JSGC_MODE_INCREMENTAL;
}

extern JS_FRIEND_API(void)
DisableIncrementalGC(JSRuntime *rt)
{
    rt->gcIncrementalEnabled = false;
}

JS_FRIEND_API(bool)
IsIncrementalBarrierNeeded(JSRuntime *rt)
{
    return (rt->gcIncrementalState == gc::MARK && !rt->isHeapBusy());
}

JS_FRIEND_API(bool)
IsIncrementalBarrierNeeded(JSContext *cx)
{
    return IsIncrementalBarrierNeeded(cx->runtime);
}

JS_FRIEND_API(bool)
IsIncrementalBarrierNeededOnObject(JSObject *obj)
{
    return obj->compartment()->needsBarrier();
}

JS_FRIEND_API(bool)
IsIncrementalBarrierNeededOnScript(JSScript *script)
{
    return script->compartment()->needsBarrier();
}

extern JS_FRIEND_API(void)
IncrementalReferenceBarrier(void *ptr)
{
    if (!ptr)
        return;
    JS_ASSERT(!static_cast<gc::Cell *>(ptr)->compartment()->rt->isHeapBusy());
    uint32_t kind = gc::GetGCThingTraceKind(ptr);
    if (kind == JSTRACE_OBJECT)
        JSObject::writeBarrierPre((JSObject *) ptr);
    else if (kind == JSTRACE_STRING)
        JSString::writeBarrierPre((JSString *) ptr);
    else if (kind == JSTRACE_SCRIPT)
        JSScript::writeBarrierPre((JSScript *) ptr);
    else if (kind == JSTRACE_SHAPE)
        Shape::writeBarrierPre((Shape *) ptr);
    else if (kind == JSTRACE_BASE_SHAPE)
        BaseShape::writeBarrierPre((BaseShape *) ptr);
    else if (kind == JSTRACE_TYPE_OBJECT)
        types::TypeObject::writeBarrierPre((types::TypeObject *) ptr);
    else
        JS_NOT_REACHED("invalid trace kind");
}

extern JS_FRIEND_API(void)
IncrementalValueBarrier(const Value &v)
{
    HeapValue::writeBarrierPre(v);
}

extern JS_FRIEND_API(void)
PokeGC(JSRuntime *rt)
{
    rt->gcPoke = true;
}

JS_FRIEND_API(JSObject *)
GetTestingFunctions(JSContext *cx)
{
    JSObject *obj = JS_NewObject(cx, NULL, NULL, NULL);
    if (!obj)
        return NULL;

    if (!DefineTestingFunctions(cx, obj))
        return NULL;

    return obj;
}

JS_FRIEND_API(void)
SetRuntimeProfilingStack(JSRuntime *rt, ProfileEntry *stack, uint32_t *size,
                         uint32_t max)
{
    rt->spsProfiler.setProfilingStack(stack, size, max);
    ReleaseAllJITCode(rt->defaultFreeOp());
}

} // namespace js

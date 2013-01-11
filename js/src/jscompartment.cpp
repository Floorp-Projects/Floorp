/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "jscntxt.h"
#include "jsdate.h"
#include "jscompartment.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jsmath.h"
#include "jsproxy.h"
#include "jsscope.h"
#include "jswatchpoint.h"
#include "jswrapper.h"

#include "assembler/wtf/Platform.h"
#include "gc/Marking.h"
#include "js/MemoryMetrics.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/PolyIC.h"
#include "methodjit/MonoIC.h"
#include "methodjit/Retcon.h"
#include "vm/Debugger.h"
#include "yarr/BumpPointerAllocator.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"
#ifdef JS_ION
#include "ion/IonCompartment.h"
#include "ion/Ion.h"
#endif

#if ENABLE_YARR_JIT
#include "assembler/jit/ExecutableAllocator.h"
#endif

using namespace js;
using namespace js::gc;

using mozilla::DebugOnly;

JSCompartment::JSCompartment(JSRuntime *rt)
  : rt(rt),
    principals(NULL),
    global_(NULL),
    enterCompartmentDepth(0),
#ifdef JSGC_GENERATIONAL
    gcStoreBuffer(&gcNursery),
#endif
    ionUsingBarriers_(false),
    gcScheduled(false),
    gcState(NoGC),
    gcPreserveCode(false),
    gcBytes(0),
    gcTriggerBytes(0),
    gcHeapGrowthFactor(3.0),
    hold(false),
    isSystemCompartment(false),
    lastCodeRelease(0),
    analysisLifoAlloc(LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    typeLifoAlloc(LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    data(NULL),
    active(false),
    scheduledForDestruction(false),
    maybeAlive(true),
    lastAnimationTime(0),
    regExps(rt),
    propertyTree(thisForCtor()),
    gcMallocAndFreeBytes(0),
    gcTriggerMallocAndFreeBytes(0),
    gcIncomingGrayPointers(NULL),
    gcLiveArrayBuffers(NULL),
    gcWeakMapList(NULL),
    gcGrayRoots(),
    gcMallocBytes(0),
    debugModeBits(rt->debugMode ? DebugFromC : 0),
    rngState(0),
    watchpointMap(NULL),
    scriptCountsMap(NULL),
    debugScriptMap(NULL),
    debugScopes(NULL)
#ifdef JS_ION
    , ionCompartment_(NULL)
#endif
{
    /* Ensure that there are no vtables to mess us up here. */
    JS_ASSERT(reinterpret_cast<JS::shadow::Compartment *>(this) ==
              static_cast<JS::shadow::Compartment *>(this));

    setGCMaxMallocBytes(rt->gcMaxMallocBytes * 0.9);
}

JSCompartment::~JSCompartment()
{
#ifdef JS_ION
    js_delete(ionCompartment_);
#endif

    js_delete(watchpointMap);
    js_delete(scriptCountsMap);
    js_delete(debugScriptMap);
    js_delete(debugScopes);
}

bool
JSCompartment::init(JSContext *cx)
{
    /*
     * As a hack, we clear our timezone cache every time we create a new
     * compartment.  This ensures that the cache is always relatively fresh, but
     * shouldn't interfere with benchmarks which create tons of date objects
     * (unless they also create tons of iframes, which seems unlikely).
     */
    if (cx)
        cx->runtime->dateTimeInfo.updateTimeZoneAdjustment();

    activeAnalysis = activeInference = false;
    types.init(cx);

    if (!crossCompartmentWrappers.init())
        return false;

    if (!regExps.init(cx))
        return false;

    if (cx)
        InitRandom(cx->runtime, &rngState);

#ifdef JSGC_GENERATIONAL
    /*
     * If we are in the middle of post-barrier verification, we need to
     * immediately begin collecting verification data on new compartments.
     */
    if (rt->gcVerifyPostData) {
        if (!gcNursery.enable())
            return false;

        if (!gcStoreBuffer.enable())
            return false;
    } else {
        gcNursery.disable();
        gcStoreBuffer.disable();
    }
#endif

    return debuggees.init();
}

void
JSCompartment::setNeedsBarrier(bool needs, ShouldUpdateIon updateIon)
{
#ifdef JS_METHODJIT
    /* ClearAllFrames calls compileBarriers() and needs the old value. */
    bool old = compileBarriers();
    if (compileBarriers(needs) != old)
        mjit::ClearAllFrames(this);
#endif

#ifdef JS_ION
    if (updateIon == UpdateIon && needs != ionUsingBarriers_) {
        ion::ToggleBarriers(this, needs);
        ionUsingBarriers_ = needs;
    }
#endif

    needsBarrier_ = needs;
}

#ifdef JS_ION
ion::IonRuntime *
JSRuntime::createIonRuntime(JSContext *cx)
{
    ionRuntime_ = cx->new_<ion::IonRuntime>();

    if (!ionRuntime_)
        return NULL;

    if (!ionRuntime_->initialize(cx)) {
        js_delete(ionRuntime_);
        ionRuntime_ = NULL;

        if (cx->runtime->atomsCompartment->ionCompartment_) {
            js_delete(cx->runtime->atomsCompartment->ionCompartment_);
            cx->runtime->atomsCompartment->ionCompartment_ = NULL;
        }

        return NULL;
    }

    return ionRuntime_;
}

bool
JSCompartment::ensureIonCompartmentExists(JSContext *cx)
{
    using namespace js::ion;
    if (ionCompartment_)
        return true;

    IonRuntime *ionRuntime = cx->runtime->getIonRuntime(cx);
    if (!ionRuntime)
        return false;

    /* Set the compartment early, so linking works. */
    ionCompartment_ = cx->new_<IonCompartment>(ionRuntime);

    if (!ionCompartment_)
        return false;

    if (!ionCompartment_->initialize(cx)) {
        js_delete(ionCompartment_);
        ionCompartment_ = NULL;
        return false;
    }

    return true;
}
#endif

static bool
WrapForSameCompartment(JSContext *cx, HandleObject obj, Value *vp)
{
    JS_ASSERT(cx->compartment == obj->compartment());
    if (!cx->runtime->sameCompartmentWrapObjectCallback) {
        vp->setObject(*obj);
        return true;
    }

    JSObject *wrapped = cx->runtime->sameCompartmentWrapObjectCallback(cx, obj);
    if (!wrapped)
        return false;
    vp->setObject(*wrapped);
    return true;
}

bool
JSCompartment::putWrapper(const CrossCompartmentKey &wrapped, const js::Value &wrapper)
{
    JS_ASSERT(wrapped.wrapped);
    JS_ASSERT_IF(wrapped.kind == CrossCompartmentKey::StringWrapper, wrapper.isString());
    JS_ASSERT_IF(wrapped.kind != CrossCompartmentKey::StringWrapper, wrapper.isObject());
    // todo: uncomment when bug 815999 is fixed:
    // JS_ASSERT(!wrapped.wrapped->isMarked(gc::GRAY));
    return crossCompartmentWrappers.put(wrapped, wrapper);
}

bool
JSCompartment::wrap(JSContext *cx, Value *vp, JSObject *existing)
{
    JS_ASSERT(cx->compartment == this);
    JS_ASSERT_IF(existing, existing->compartment() == cx->compartment);
    JS_ASSERT_IF(existing, vp->isObject());
    JS_ASSERT_IF(existing, IsDeadProxyObject(existing));

    unsigned flags = 0;

    JS_CHECK_CHROME_RECURSION(cx, return false);

#ifdef DEBUG
    struct AutoDisableProxyCheck {
        JSRuntime *runtime;
        AutoDisableProxyCheck(JSRuntime *rt) : runtime(rt) {
            runtime->gcDisableStrictProxyCheckingCount++;
        }
        ~AutoDisableProxyCheck() { runtime->gcDisableStrictProxyCheckingCount--; }
    } adpc(rt);
#endif

    /* Only GC things have to be wrapped or copied. */
    if (!vp->isMarkable())
        return true;

    if (vp->isString()) {
        JSString *str = vp->toString();

        /* If the string is already in this compartment, we are done. */
        if (str->compartment() == this)
            return true;

        /* If the string is an atom, we don't have to copy. */
        if (str->isAtom()) {
            JS_ASSERT(str->compartment() == cx->runtime->atomsCompartment);
            return true;
        }
    }

    /*
     * Wrappers should really be parented to the wrapped parent of the wrapped
     * object, but in that case a wrapped global object would have a NULL
     * parent without being a proper global object (JSCLASS_IS_GLOBAL). Instead,
     * we parent all wrappers to the global object in their home compartment.
     * This loses us some transparency, and is generally very cheesy.
     */
    HandleObject global = cx->global();

    /* Unwrap incoming objects. */
    if (vp->isObject()) {
        RootedObject obj(cx, &vp->toObject());

        if (obj->compartment() == this)
            return WrapForSameCompartment(cx, obj, vp);

        /* Translate StopIteration singleton. */
        if (obj->isStopIteration()) {
            RootedValue vvp(cx, *vp);
            bool result = js_FindClassObject(cx, JSProto_StopIteration, &vvp);
            *vp = vvp;
            return result;
        }

        /* Unwrap the object, but don't unwrap outer windows. */
        obj = UnwrapObject(&vp->toObject(), /* stopAtOuter = */ true, &flags);

        if (obj->compartment() == this)
            return WrapForSameCompartment(cx, obj, vp);

        if (cx->runtime->preWrapObjectCallback) {
            obj = cx->runtime->preWrapObjectCallback(cx, global, obj, flags);
            if (!obj)
                return false;
        }

        if (obj->compartment() == this)
            return WrapForSameCompartment(cx, obj, vp);
        vp->setObject(*obj);

#ifdef DEBUG
        {
            JSObject *outer = GetOuterObject(cx, obj);
            JS_ASSERT(outer && outer == obj);
        }
#endif
    }

    RootedValue key(cx, *vp);

    /* If we already have a wrapper for this value, use it. */
    if (WrapperMap::Ptr p = crossCompartmentWrappers.lookup(key)) {
        *vp = p->value;
        if (vp->isObject()) {
            RootedObject obj(cx, &vp->toObject());
            JS_ASSERT(obj->isCrossCompartmentWrapper());
            JS_ASSERT(obj->getParent() == global);
        }
        return true;
    }

    if (vp->isString()) {
        RootedValue orig(cx, *vp);
        Rooted<JSStableString *> str(cx, vp->toString()->ensureStable(cx));
        if (!str)
            return false;
        RootedString wrapped(cx, js_NewStringCopyN(cx, str->chars().get(), str->length()));
        if (!wrapped)
            return false;
        vp->setString(wrapped);
        if (!putWrapper(orig, *vp))
            return false;

        if (str->compartment()->isGCMarking()) {
            /*
             * All string wrappers are dropped when collection starts, but we
             * just created a new one.  Mark the wrapped string to stop it being
             * finalized, because if it was then the pointer in this
             * compartment's wrapper map would be left dangling.
             */
            JSString *tmp = str;
            MarkStringUnbarriered(&rt->gcMarker, &tmp, "wrapped string");
            JS_ASSERT(tmp == str);
        }

        return true;
    }

    RootedObject obj(cx, &vp->toObject());

    JSObject *proto = Proxy::LazyProto;
    if (existing) {
        /* Is it possible to reuse |existing|? */
        if (!existing->getTaggedProto().isLazy() ||
            existing->getClass() != &ObjectProxyClass ||
            existing->getParent() != global ||
            obj->isCallable())
        {
            existing = NULL;
        }
    }

    /*
     * We hand in the original wrapped object into the wrap hook to allow
     * the wrap hook to reason over what wrappers are currently applied
     * to the object.
     */
    RootedObject wrapper(cx);
    wrapper = cx->runtime->wrapObjectCallback(cx, existing, obj, proto, global, flags);
    if (!wrapper)
        return false;

    // We maintain the invariant that the key in the cross-compartment wrapper
    // map is always directly wrapped by the value.
    JS_ASSERT(Wrapper::wrappedObject(wrapper) == &key.get().toObject());

    vp->setObject(*wrapper);

    if (!putWrapper(key, *vp))
        return false;

    return true;
}

bool
JSCompartment::wrap(JSContext *cx, JSString **strp)
{
    RootedValue value(cx, StringValue(*strp));
    if (!wrap(cx, value.address()))
        return false;
    *strp = value.get().toString();
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, HeapPtrString *strp)
{
    RootedValue value(cx, StringValue(*strp));
    if (!wrap(cx, value.address()))
        return false;
    *strp = value.get().toString();
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, JSObject **objp, JSObject *existing)
{
    if (!*objp)
        return true;
    RootedValue value(cx, ObjectValue(**objp));
    if (!wrap(cx, value.address(), existing))
        return false;
    *objp = &value.get().toObject();
    return true;
}

bool
JSCompartment::wrapId(JSContext *cx, jsid *idp)
{
    if (JSID_IS_INT(*idp))
        return true;
    RootedValue value(cx, IdToValue(*idp));
    if (!wrap(cx, value.address()))
        return false;
    RootedId id(cx);
    if (!ValueToId(cx, value.get(), &id))
        return false;

    *idp = id;
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, PropertyOp *propp)
{
    Value v = CastAsObjectJsval(*propp);
    if (!wrap(cx, &v))
        return false;
    *propp = CastAsPropertyOp(v.toObjectOrNull());
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, StrictPropertyOp *propp)
{
    Value v = CastAsObjectJsval(*propp);
    if (!wrap(cx, &v))
        return false;
    *propp = CastAsStrictPropertyOp(v.toObjectOrNull());
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, PropertyDescriptor *desc)
{
    return wrap(cx, &desc->obj) &&
           (!(desc->attrs & JSPROP_GETTER) || wrap(cx, &desc->getter)) &&
           (!(desc->attrs & JSPROP_SETTER) || wrap(cx, &desc->setter)) &&
           wrap(cx, &desc->value);
}

bool
JSCompartment::wrap(JSContext *cx, AutoIdVector &props)
{
    jsid *vector = props.begin();
    int length = props.length();
    for (size_t n = 0; n < size_t(length); ++n) {
        if (!wrapId(cx, &vector[n]))
            return false;
    }
    return true;
}

/*
 * This method marks pointers that cross compartment boundaries. It should be
 * called only for per-compartment GCs, since full GCs naturally follow pointers
 * across compartments.
 */
void
JSCompartment::markCrossCompartmentWrappers(JSTracer *trc)
{
    JS_ASSERT(!isCollecting());

    for (WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
        Value v = e.front().value;
        if (e.front().key.kind == CrossCompartmentKey::ObjectWrapper) {
            JSObject *wrapper = &v.toObject();

            /*
             * We have a cross-compartment wrapper. Its private pointer may
             * point into the compartment being collected, so we should mark it.
             */
            Value referent = GetProxyPrivate(wrapper);
            MarkValueRoot(trc, &referent, "cross-compartment wrapper");
            JS_ASSERT(referent == GetProxyPrivate(wrapper));

            if (IsFunctionProxy(wrapper)) {
                Value call = GetProxyCall(wrapper);
                MarkValueRoot(trc, &call, "cross-compartment wrapper");
                JS_ASSERT(call == GetProxyCall(wrapper));
            }
        }
    }
}

void
JSCompartment::mark(JSTracer *trc)
{
#ifdef JS_ION
    if (ionCompartment_)
        ionCompartment_->mark(trc, this);
#endif

    /*
     * If a compartment is on-stack, we mark its global so that
     * JSContext::global() remains valid.
     */
    if (enterCompartmentDepth && global_)
        MarkObjectRoot(trc, global_.unsafeGet(), "on-stack compartment global");
}

void
JSCompartment::markTypes(JSTracer *trc)
{
    /*
     * Mark all scripts, type objects and singleton JS objects in the
     * compartment. These can be referred to directly by type sets, which we
     * cannot modify while code which depends on these type sets is active.
     */
    JS_ASSERT(activeAnalysis || isPreservingCode());

    for (CellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        MarkScriptRoot(trc, &script, "mark_types_script");
        JS_ASSERT(script == i.get<JSScript>());
    }

    for (size_t thingKind = FINALIZE_OBJECT0; thingKind < FINALIZE_OBJECT_LIMIT; thingKind++) {
        ArenaHeader *aheader = arenas.getFirstArena(static_cast<AllocKind>(thingKind));
        if (aheader)
            rt->gcMarker.pushArenaList(aheader);
    }

    for (CellIterUnderGC i(this, FINALIZE_TYPE_OBJECT); !i.done(); i.next()) {
        types::TypeObject *type = i.get<types::TypeObject>();
        MarkTypeObjectRoot(trc, &type, "mark_types_scan");
        JS_ASSERT(type == i.get<types::TypeObject>());
    }

    gcTypesMarked = true;
}

void
JSCompartment::discardJitCode(FreeOp *fop, bool discardConstraints)
{
#ifdef JS_METHODJIT

    /*
     * Kick all frames on the stack into the interpreter, and release all JIT
     * code in the compartment unless code is being preserved, in which case
     * purge all caches in the JIT scripts. Even if we are not releasing all
     * JIT code, we still need to release code for scripts which are in the
     * middle of a native or getter stub call, as these stubs will have been
     * redirected to the interpoline.
     */
    mjit::ClearAllFrames(this);

    if (isPreservingCode()) {
        PurgeJITCaches(this);
    } else {
# ifdef JS_ION
        /* Only mark OSI points if code is being discarded. */
        ion::InvalidateAll(fop, this);
# endif
        for (CellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            mjit::ReleaseScriptCode(fop, script);
# ifdef JS_ION
            ion::FinishInvalidation(fop, script);
# endif

            /*
             * Use counts for scripts are reset on GC. After discarding code we
             * need to let it warm back up to get information such as which
             * opcodes are setting array holes or accessing getter properties.
             */
            script->resetUseCount();
        }

        types.sweepCompilerOutputs(fop, discardConstraints);
    }

#endif /* JS_METHODJIT */
}

bool
JSCompartment::isDiscardingJitCode(JSTracer *trc)
{
    if (!IS_GC_MARKING_TRACER(trc))
        return false;

    return !gcPreserveCode;
}

void
JSCompartment::sweep(FreeOp *fop, bool releaseTypes)
{
    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_DISCARD_CODE);
        discardJitCode(fop, !activeAnalysis && !gcPreserveCode);
    }

    /* This function includes itself in PHASE_SWEEP_TABLES. */
    sweepCrossCompartmentWrappers();

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_TABLES);

        /* Remove dead references held weakly by the compartment. */

        sweepBaseShapeTable();
        sweepInitialShapeTable();
        sweepNewTypeObjectTable(newTypeObjects);
        sweepNewTypeObjectTable(lazyTypeObjects);
        sweepBreakpoints(fop);

        if (global_ && IsObjectAboutToBeFinalized(global_.unsafeGet()))
            global_ = NULL;

#ifdef JS_ION
        if (ionCompartment_)
            ionCompartment_->sweep(fop);
#endif

        /*
         * JIT code increments activeUseCount for any RegExpShared used by jit
         * code for the lifetime of the JIT script. Thus, we must perform
         * sweeping after clearing jit code.
         */
        regExps.sweep(rt);

        if (debugScopes)
            debugScopes->sweep(rt);

        /* Finalize unreachable (key,value) pairs in all weak maps. */
        WeakMapBase::sweepCompartment(this);
    }

    if (!activeAnalysis && !gcPreserveCode) {
        JS_ASSERT(!types.constrainedOutputs);
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_DISCARD_ANALYSIS);

        /*
         * Clear the analysis pool, but don't release its data yet. While
         * sweeping types any live data will be allocated into the pool.
         */
        LifoAlloc oldAlloc(typeLifoAlloc.defaultChunkSize());
        oldAlloc.steal(&typeLifoAlloc);

        /*
         * Periodically release observed types for all scripts. This is safe to
         * do when there are no frames for the compartment on the stack.
         */
        if (active)
            releaseTypes = false;

        /*
         * Sweep analysis information and everything depending on it from the
         * compartment, including all remaining mjit code if inference is
         * enabled in the compartment.
         */
        if (types.inferenceEnabled) {
            gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_DISCARD_TI);

            for (CellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
                RawScript script = i.get<JSScript>();
                if (script->types) {
                    types::TypeScript::Sweep(fop, script);

                    if (releaseTypes) {
                        script->types->destroy();
                        script->types = NULL;
                    }
                }
            }
        }

        {
            gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_SWEEP_TYPES);
            types.sweep(fop);
        }

        {
            gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_CLEAR_SCRIPT_ANALYSIS);
            for (CellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
                JSScript *script = i.get<JSScript>();
                script->clearAnalysis();
                script->clearPropertyReadTypes();
            }
        }

        {
            gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_FREE_TI_ARENA);
            rt->freeLifoAlloc.transferFrom(&analysisLifoAlloc);
            rt->freeLifoAlloc.transferFrom(&oldAlloc);
        }
    }

    active = false;
}

/*
 * Remove dead wrappers from the table. We must sweep all compartments, since
 * string entries in the crossCompartmentWrappers table are not marked during
 * markCrossCompartmentWrappers.
 */
void
JSCompartment::sweepCrossCompartmentWrappers()
{
    gcstats::AutoPhase ap1(rt->gcStats, gcstats::PHASE_SWEEP_TABLES);
    gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_SWEEP_TABLES_WRAPPER);

    /* Remove dead wrappers from the table. */
    for (WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
        CrossCompartmentKey key = e.front().key;
        bool keyDying = IsCellAboutToBeFinalized(&key.wrapped);
        bool valDying = IsValueAboutToBeFinalized(e.front().value.unsafeGet());
        bool dbgDying = key.debugger && IsObjectAboutToBeFinalized(&key.debugger);
        if (keyDying || valDying || dbgDying) {
            JS_ASSERT(key.kind != CrossCompartmentKey::StringWrapper);
            e.removeFront();
        } else if (key.wrapped != e.front().key.wrapped || key.debugger != e.front().key.debugger) {
            e.rekeyFront(key);
        }
    }
}

void
JSCompartment::purge()
{
    dtoaCache.purge();
}

void
JSCompartment::resetGCMallocBytes()
{
    gcMallocBytes = ptrdiff_t(gcMaxMallocBytes);
}

void
JSCompartment::setGCMaxMallocBytes(size_t value)
{
    /*
     * For compatibility treat any value that exceeds PTRDIFF_T_MAX to
     * mean that value.
     */
    gcMaxMallocBytes = (ptrdiff_t(value) >= 0) ? value : size_t(-1) >> 1;
    resetGCMallocBytes();
}

void
JSCompartment::onTooMuchMalloc()
{
    TriggerCompartmentGC(this, gcreason::TOO_MUCH_MALLOC);
}


bool
JSCompartment::hasScriptsOnStack()
{
    for (AllFramesIter afi(rt->stackSpace); !afi.done(); ++afi) {
#ifdef JS_ION
        // If this is an Ion frame, check the IonActivation instead
        if (afi.isIon())
            continue;
#endif
        if (afi.interpFrame()->script()->compartment() == this)
            return true;
    }
#ifdef JS_ION
    for (ion::IonActivationIterator iai(rt); iai.more(); ++iai) {
        if (iai.activation()->compartment() == this)
            return true;
    }
#endif
    return false;
}

bool
JSCompartment::setDebugModeFromC(JSContext *cx, bool b, AutoDebugModeGC &dmgc)
{
    bool enabledBefore = debugMode();
    bool enabledAfter = (debugModeBits & ~unsigned(DebugFromC)) || b;

    // Debug mode can be enabled only when no scripts from the target
    // compartment are on the stack. It would even be incorrect to discard just
    // the non-live scripts' JITScripts because they might share ICs with live
    // scripts (bug 632343).
    //
    // We do allow disabling debug mode while scripts are on the stack.  In
    // that case the debug-mode code for those scripts remains, so subsequently
    // hooks may be called erroneously, even though debug mode is supposedly
    // off, and we have to live with it.
    //
    bool onStack = false;
    if (enabledBefore != enabledAfter) {
        onStack = hasScriptsOnStack();
        if (b && onStack) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_NOT_IDLE);
            return false;
        }
    }

    debugModeBits = (debugModeBits & ~unsigned(DebugFromC)) | (b ? DebugFromC : 0);
    JS_ASSERT(debugMode() == enabledAfter);
    if (enabledBefore != enabledAfter) {
        updateForDebugMode(cx->runtime->defaultFreeOp(), dmgc);
        if (!enabledAfter)
            DebugScopes::onCompartmentLeaveDebugMode(this);
    }
    return true;
}

void
JSCompartment::updateForDebugMode(FreeOp *fop, AutoDebugModeGC &dmgc)
{
    for (ContextIter acx(rt); !acx.done(); acx.next()) {
        if (acx->compartment == this)
            acx->updateJITEnabled();
    }

#ifdef JS_METHODJIT
    bool enabled = debugMode();

    JS_ASSERT_IF(enabled, !hasScriptsOnStack());

    for (gc::CellIter i(this, gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        script->debugMode = enabled;
    }

    // When we change a compartment's debug mode, whether we're turning it
    // on or off, we must always throw away all analyses: debug mode
    // affects various aspects of the analysis, which then get baked into
    // SSA results, which affects code generation in complicated ways. We
    // must also throw away all JIT code, as its soundness depends on the
    // analyses.
    //
    // It suffices to do a garbage collection cycle or to finish the
    // ongoing GC cycle. The necessary cleanup happens in
    // JSCompartment::sweep.
    //
    // dmgc makes sure we can't forget to GC, but it is also important not
    // to run any scripts in this compartment until the dmgc is destroyed.
    // That is the caller's responsibility.
    if (!rt->isHeapBusy())
        dmgc.scheduleGC(this);
#endif
}

bool
JSCompartment::addDebuggee(JSContext *cx, js::GlobalObject *global)
{
    AutoDebugModeGC dmgc(cx->runtime);
    return addDebuggee(cx, global, dmgc);
}

bool
JSCompartment::addDebuggee(JSContext *cx,
                           js::GlobalObject *global,
                           AutoDebugModeGC &dmgc)
{
    bool wasEnabled = debugMode();
    if (!debuggees.put(global)) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    debugModeBits |= DebugFromJS;
    if (!wasEnabled) {
        updateForDebugMode(cx->runtime->defaultFreeOp(), dmgc);
    }
    return true;
}

void
JSCompartment::removeDebuggee(FreeOp *fop,
                              js::GlobalObject *global,
                              js::GlobalObjectSet::Enum *debuggeesEnum)
{
    AutoDebugModeGC dmgc(rt);
    return removeDebuggee(fop, global, dmgc, debuggeesEnum);
}

void
JSCompartment::removeDebuggee(FreeOp *fop,
                              js::GlobalObject *global,
                              AutoDebugModeGC &dmgc,
                              js::GlobalObjectSet::Enum *debuggeesEnum)
{
    bool wasEnabled = debugMode();
    JS_ASSERT(debuggees.has(global));
    if (debuggeesEnum)
        debuggeesEnum->removeFront();
    else
        debuggees.remove(global);

    if (debuggees.empty()) {
        debugModeBits &= ~DebugFromJS;
        if (wasEnabled && !debugMode()) {
            DebugScopes::onCompartmentLeaveDebugMode(this);
            updateForDebugMode(fop, dmgc);
        }
    }
}

void
JSCompartment::clearBreakpointsIn(FreeOp *fop, js::Debugger *dbg, JSObject *handler)
{
    for (gc::CellIter i(this, gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (script->hasAnyBreakpointsOrStepMode())
            script->clearBreakpointsIn(fop, dbg, handler);
    }
}

void
JSCompartment::clearTraps(FreeOp *fop)
{
    for (gc::CellIter i(this, gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (script->hasAnyBreakpointsOrStepMode())
            script->clearTraps(fop);
    }
}

void
JSCompartment::sweepBreakpoints(FreeOp *fop)
{
    gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_TABLES_BREAKPOINT);

    if (rt->debuggerList.isEmpty())
        return;

    for (CellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (!script->hasAnyBreakpointsOrStepMode())
            continue;
        bool scriptGone = IsScriptAboutToBeFinalized(&script);
        JS_ASSERT(script == i.get<JSScript>());
        for (unsigned i = 0; i < script->length; i++) {
            BreakpointSite *site = script->getBreakpointSite(script->code + i);
            if (!site)
                continue;
            // nextbp is necessary here to avoid possibly reading *bp after
            // destroying it.
            Breakpoint *nextbp;
            for (Breakpoint *bp = site->firstBreakpoint(); bp; bp = nextbp) {
                nextbp = bp->nextInSite();
                if (scriptGone || IsObjectAboutToBeFinalized(&bp->debugger->toJSObjectRef()))
                    bp->destroy(fop);
            }
        }
    }
}

void
JSCompartment::sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf, size_t *compartmentObject,
                                   TypeInferenceSizes *tiSizes, size_t *shapesCompartmentTables,
                                   size_t *crossCompartmentWrappersArg, size_t *regexpCompartment,
                                   size_t *debuggeesSet)
{
    *compartmentObject = mallocSizeOf(this);
    sizeOfTypeInferenceData(tiSizes, mallocSizeOf);
    *shapesCompartmentTables = baseShapes.sizeOfExcludingThis(mallocSizeOf)
                             + initialShapes.sizeOfExcludingThis(mallocSizeOf)
                             + newTypeObjects.sizeOfExcludingThis(mallocSizeOf)
                             + lazyTypeObjects.sizeOfExcludingThis(mallocSizeOf);
    *crossCompartmentWrappersArg = crossCompartmentWrappers.sizeOfExcludingThis(mallocSizeOf);
    *regexpCompartment = regExps.sizeOfExcludingThis(mallocSizeOf);
    *debuggeesSet = debuggees.sizeOfExcludingThis(mallocSizeOf);
}

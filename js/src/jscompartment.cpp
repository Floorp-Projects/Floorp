/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscompartment.h"

#include "mozilla/DebugOnly.h"

#include "jscntxt.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jsproxy.h"
#include "jswatchpoint.h"
#include "jswrapper.h"

#include "gc/Marking.h"
#ifdef JS_ION
#include "ion/IonCompartment.h"
#endif
#include "js/RootingAPI.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"

#include "gc/Barrier-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::DebugOnly;

JSCompartment::JSCompartment(Zone *zone)
  : zone_(zone),
    rt(zone->rt),
    principals(NULL),
    isSystem(false),
    marked(true),
    global_(NULL),
    enterCompartmentDepth(0),
    lastCodeRelease(0),
    analysisLifoAlloc(ANALYSIS_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    data(NULL),
    objectMetadataCallback(NULL),
    lastAnimationTime(0),
    regExps(rt),
    propertyTree(thisForCtor()),
    gcIncomingGrayPointers(NULL),
    gcLiveArrayBuffers(NULL),
    gcWeakMapList(NULL),
    debugModeBits(rt->debugMode ? DebugFromC : 0),
    rngState(0),
    watchpointMap(NULL),
    scriptCountsMap(NULL),
    debugScriptMap(NULL),
    debugScopes(NULL),
    enumerators(NULL),
    compartmentStats(NULL)
#ifdef JS_ION
    , ionCompartment_(NULL)
#endif
{
    rt->numCompartments++;
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
    js_free(enumerators);

    rt->numCompartments--;
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
        cx->runtime()->dateTimeInfo.updateTimeZoneAdjustment();

    activeAnalysis = false;

    if (!crossCompartmentWrappers.init(0))
        return false;

    if (!regExps.init(cx))
        return false;

    enumerators = NativeIterator::allocateSentinel(cx);
    if (!enumerators)
        return false;

    return debuggees.init(0);
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

        if (cx->runtime()->atomsCompartment->ionCompartment_) {
            js_delete(cx->runtime()->atomsCompartment->ionCompartment_);
            cx->runtime()->atomsCompartment->ionCompartment_ = NULL;
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

    IonRuntime *ionRuntime = cx->runtime()->getIonRuntime(cx);
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
WrapForSameCompartment(JSContext *cx, HandleObject obj, MutableHandleValue vp)
{
    JS_ASSERT(cx->compartment() == obj->compartment());
    if (!cx->runtime()->sameCompartmentWrapObjectCallback) {
        vp.setObject(*obj);
        return true;
    }

    JSObject *wrapped = cx->runtime()->sameCompartmentWrapObjectCallback(cx, obj);
    if (!wrapped)
        return false;
    vp.setObject(*wrapped);
    return true;
}

bool
JSCompartment::putWrapper(const CrossCompartmentKey &wrapped, const js::Value &wrapper)
{
    JS_ASSERT(wrapped.wrapped);
    JS_ASSERT(!IsPoisonedPtr(wrapped.wrapped));
    JS_ASSERT(!IsPoisonedPtr(wrapped.debugger));
    JS_ASSERT(!IsPoisonedPtr(wrapper.toGCThing()));
    JS_ASSERT_IF(wrapped.kind == CrossCompartmentKey::StringWrapper, wrapper.isString());
    JS_ASSERT_IF(wrapped.kind != CrossCompartmentKey::StringWrapper, wrapper.isObject());
    return crossCompartmentWrappers.put(wrapped, wrapper);
}

bool
JSCompartment::wrap(JSContext *cx, MutableHandleValue vp, HandleObject existingArg)
{
    JS_ASSERT(cx->compartment() == this);
    JS_ASSERT_IF(existingArg, existingArg->compartment() == cx->compartment());
    JS_ASSERT_IF(existingArg, vp.isObject());
    JS_ASSERT_IF(existingArg, IsDeadProxyObject(existingArg));

    unsigned flags = 0;

    JS_CHECK_CHROME_RECURSION(cx, return false);

    AutoDisableProxyCheck adpc(rt);

    /* Only GC things have to be wrapped or copied. */
    if (!vp.isMarkable())
        return true;

    if (vp.isString()) {
        JSString *str = vp.toString();

        /* If the string is already in this compartment, we are done. */
        if (str->zone() == zone())
            return true;

        /* If the string is an atom, we don't have to copy. */
        if (str->isAtom()) {
            JS_ASSERT(str->zone() == cx->runtime()->atomsCompartment->zone());
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
    if (vp.isObject()) {
        RootedObject obj(cx, &vp.toObject());

        if (obj->compartment() == this)
            return WrapForSameCompartment(cx, obj, vp);

        /* Translate StopIteration singleton. */
        if (obj->isStopIteration())
            return js_FindClassObject(cx, JSProto_StopIteration, vp);

        /* Unwrap the object, but don't unwrap outer windows. */
        obj = UncheckedUnwrap(obj, /* stopAtOuter = */ true, &flags);

        if (obj->compartment() == this)
            return WrapForSameCompartment(cx, obj, vp);

        if (cx->runtime()->preWrapObjectCallback) {
            obj = cx->runtime()->preWrapObjectCallback(cx, global, obj, flags);
            if (!obj)
                return false;
        }

        if (obj->compartment() == this)
            return WrapForSameCompartment(cx, obj, vp);
        vp.setObject(*obj);

#ifdef DEBUG
        {
            JSObject *outer = GetOuterObject(cx, obj);
            JS_ASSERT(outer && outer == obj);
        }
#endif
    }

    RootedValue key(cx, vp);

    /* If we already have a wrapper for this value, use it. */
    if (WrapperMap::Ptr p = crossCompartmentWrappers.lookup(key)) {
        vp.set(p->value);
        if (vp.isObject()) {
            DebugOnly<JSObject *> obj = &vp.toObject();
            JS_ASSERT(obj->isCrossCompartmentWrapper());
            JS_ASSERT(obj->getParent() == global);
        }
        return true;
    }

    if (vp.isString()) {
        Rooted<JSLinearString *> str(cx, vp.toString()->ensureLinear(cx));
        if (!str)
            return false;

        JSString *wrapped = js_NewStringCopyN<CanGC>(cx, str->chars(), str->length());
        if (!wrapped)
            return false;

        vp.setString(wrapped);
        if (!putWrapper(key, vp))
            return false;

        if (str->zone()->isGCMarking()) {
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

    RootedObject proto(cx, Proxy::LazyProto);
    RootedObject obj(cx, &vp.toObject());
    RootedObject existing(cx, existingArg);
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
    wrapper = cx->runtime()->wrapObjectCallback(cx, existing, obj, proto, global, flags);
    if (!wrapper)
        return false;

    // We maintain the invariant that the key in the cross-compartment wrapper
    // map is always directly wrapped by the value.
    JS_ASSERT(Wrapper::wrappedObject(wrapper) == &key.get().toObject());

    vp.setObject(*wrapper);
    return putWrapper(key, vp);
}

bool
JSCompartment::wrap(JSContext *cx, JSString **strp)
{
    RootedValue value(cx, StringValue(*strp));
    if (!wrap(cx, &value))
        return false;
    *strp = value.get().toString();
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, HeapPtrString *strp)
{
    RootedValue value(cx, StringValue(*strp));
    if (!wrap(cx, &value))
        return false;
    *strp = value.get().toString();
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, JSObject **objp, JSObject *existingArg)
{
    if (!*objp)
        return true;
    RootedValue value(cx, ObjectValue(**objp));
    RootedObject existing(cx, existingArg);
    if (!wrap(cx, &value, existing))
        return false;
    *objp = &value.get().toObject();
    return true;
}

bool
JSCompartment::wrapId(JSContext *cx, jsid *idp)
{
    MOZ_ASSERT(*idp != JSID_VOID, "JSID_VOID is an out-of-band sentinel value");
    if (JSID_IS_INT(*idp))
        return true;
    RootedValue value(cx, IdToValue(*idp));
    if (!wrap(cx, &value))
        return false;
    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, value, &id))
        return false;

    *idp = id;
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, PropertyOp *propp)
{
    RootedValue value(cx, CastAsObjectJsval(*propp));
    if (!wrap(cx, &value))
        return false;
    *propp = CastAsPropertyOp(value.toObjectOrNull());
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, StrictPropertyOp *propp)
{
    RootedValue value(cx, CastAsObjectJsval(*propp));
    if (!wrap(cx, &value))
        return false;
    *propp = CastAsStrictPropertyOp(value.toObjectOrNull());
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, PropertyDescriptor *desc)
{
    if (!wrap(cx, &desc->obj))
        return false;

    if (desc->attrs & JSPROP_GETTER) {
        if (!wrap(cx, &desc->getter))
            return false;
    }
    if (desc->attrs & JSPROP_SETTER) {
        if (!wrap(cx, &desc->setter))
            return false;
    }

    RootedValue value(cx, desc->value);
    if (!wrap(cx, &value))
        return false;
    desc->value = value.get();
    return true;
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
    JS_ASSERT(!zone()->isCollecting());

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
        }
    }
}

/*
 * This method marks and keeps live all pointers in the cross compartment
 * wrapper map. It should be called only for minor GCs, since minor GCs cannot,
 * by their nature, apply the weak constraint to safely remove items from the
 * wrapper map.
 */
void
JSCompartment::markAllCrossCompartmentWrappers(JSTracer *trc)
{
    for (WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
        CrossCompartmentKey key = e.front().key;
        MarkGCThingRoot(trc, (void **)&key.wrapped, "CrossCompartmentKey::wrapped");
        if (key.debugger)
            MarkObjectRoot(trc, &key.debugger, "CrossCompartmentKey::debugger");
        MarkValueRoot(trc, e.front().value.unsafeGet(), "CrossCompartmentWrapper");
        if (key.wrapped != e.front().key.wrapped || key.debugger != e.front().key.debugger)
            e.rekeyFront(key);
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
JSCompartment::sweep(FreeOp *fop, bool releaseTypes)
{
    JS_ASSERT(!activeAnalysis);

    /* This function includes itself in PHASE_SWEEP_TABLES. */
    sweepCrossCompartmentWrappers();

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_TABLES);

        /* Remove dead references held weakly by the compartment. */

        sweepBaseShapeTable();
        sweepInitialShapeTable();
        sweepNewTypeObjectTable(newTypeObjects);
        sweepNewTypeObjectTable(lazyTypeObjects);
        sweepCallsiteClones();

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

    if (!zone()->isPreservingCode()) {
        JS_ASSERT(!types.constrainedOutputs);
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_DISCARD_ANALYSIS);
        gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_FREE_TI_ARENA);
        rt->freeLifoAlloc.transferFrom(&analysisLifoAlloc);
    } else {
        gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_DISCARD_ANALYSIS);
        types.sweepShapes(fop);
    }

    NativeIterator *ni = enumerators->next();
    while (ni != enumerators) {
        JSObject *iterObj = ni->iterObj();
        NativeIterator *next = ni->next();
        if (gc::IsObjectAboutToBeFinalized(&iterObj))
            ni->unlink();
        ni = next;
    }
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

bool
JSCompartment::hasScriptsOnStack()
{
    for (ActivationIterator iter(rt); !iter.done(); ++iter) {
        if (iter.activation()->compartment() == this)
            return true;
    }

    return false;
}

static bool
AddInnerLazyFunctionsFromScript(JSScript *script, AutoObjectVector &lazyFunctions)
{
    if (!script->hasObjects())
        return true;
    ObjectArray *objects = script->objects();
    for (size_t i = script->innerObjectsStart(); i < objects->length; i++) {
        JSObject *obj = objects->vector[i];
        if (obj->isFunction() && obj->toFunction()->isInterpretedLazy()) {
            if (!lazyFunctions.append(obj))
                return false;
        }
    }
    return true;
}

static bool
CreateLazyScriptsForCompartment(JSContext *cx)
{
    AutoObjectVector lazyFunctions(cx);

    // Find all root lazy functions in the compartment: those which have not been
    // compiled and which have a source object, indicating that their parent has
    // been compiled.
    for (gc::CellIter i(cx->zone(), JSFunction::FinalizeKind); !i.done(); i.next()) {
        JSObject *obj = i.get<JSObject>();
        if (obj->compartment() == cx->compartment() && obj->isFunction()) {
            JSFunction *fun = obj->toFunction();
            if (fun->isInterpretedLazy()) {
                LazyScript *lazy = fun->lazyScriptOrNull();
                if (lazy && lazy->sourceObject() && !lazy->maybeScript()) {
                    if (!lazyFunctions.append(fun))
                        return false;
                }
            }
        }
    }

    // Create scripts for each lazy function, updating the list of functions to
    // process with any newly exposed inner functions in created scripts.
    // A function cannot be delazified until its outer script exists.
    for (size_t i = 0; i < lazyFunctions.length(); i++) {
        JSFunction *fun = lazyFunctions[i]->toFunction();

        // lazyFunctions may have been populated with multiple functions for
        // a lazy script.
        if (!fun->isInterpretedLazy())
            continue;

        JSScript *script = fun->getOrCreateScript(cx);
        if (!script)
            return false;
        if (!AddInnerLazyFunctionsFromScript(script, lazyFunctions))
            return false;
    }

    // Repoint any clones of the original functions to their new script.
    for (gc::CellIter i(cx->zone(), JSFunction::FinalizeKind); !i.done(); i.next()) {
        JSObject *obj = i.get<JSObject>();
        if (obj->compartment() == cx->compartment() && obj->isFunction()) {
            JSFunction *fun = obj->toFunction();
            if (fun->isInterpretedLazy()) {
                LazyScript *lazy = fun->lazyScriptOrNull();
                if (lazy && lazy->maybeScript())
                    fun->getExistingScript();
            }
        }
    }

    return true;
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
        if (enabledAfter && !CreateLazyScriptsForCompartment(cx))
            return false;
    }

    debugModeBits = (debugModeBits & ~unsigned(DebugFromC)) | (b ? DebugFromC : 0);
    JS_ASSERT(debugMode() == enabledAfter);
    if (enabledBefore != enabledAfter) {
        updateForDebugMode(cx->runtime()->defaultFreeOp(), dmgc);
        if (!enabledAfter)
            DebugScopes::onCompartmentLeaveDebugMode(this);
    }
    return true;
}

void
JSCompartment::updateForDebugMode(FreeOp *fop, AutoDebugModeGC &dmgc)
{
    for (ContextIter acx(rt); !acx.done(); acx.next()) {
        if (acx->compartment() == this)
            acx->updateJITEnabled();
    }

#ifdef JS_ION
    JS_ASSERT_IF(debugMode(), !hasScriptsOnStack());

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
        dmgc.scheduleGC(zone());
#endif
}

bool
JSCompartment::addDebuggee(JSContext *cx, js::GlobalObject *global)
{
    AutoDebugModeGC dmgc(cx->runtime());
    return addDebuggee(cx, global, dmgc);
}

bool
JSCompartment::addDebuggee(JSContext *cx,
                           GlobalObject *globalArg,
                           AutoDebugModeGC &dmgc)
{
    Rooted<GlobalObject*> global(cx, globalArg);

    bool wasEnabled = debugMode();
    if (!wasEnabled && !CreateLazyScriptsForCompartment(cx))
        return false;
    if (!debuggees.put(global)) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    debugModeBits |= DebugFromJS;
    if (!wasEnabled) {
        updateForDebugMode(cx->runtime()->defaultFreeOp(), dmgc);
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
    for (gc::CellIter i(zone(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (script->compartment() == this && script->hasAnyBreakpointsOrStepMode())
            script->clearBreakpointsIn(fop, dbg, handler);
    }
}

void
JSCompartment::clearTraps(FreeOp *fop)
{
    MinorGC(rt, JS::gcreason::EVICT_NURSERY);
    for (gc::CellIter i(zone(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (script->compartment() == this && script->hasAnyBreakpointsOrStepMode())
            script->clearTraps(fop);
    }
}

void
JSCompartment::sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf, size_t *compartmentObject,
                                   JS::TypeInferenceSizes *tiSizes, size_t *shapesCompartmentTables,
                                   size_t *crossCompartmentWrappersArg, size_t *regexpCompartment,
                                   size_t *debuggeesSet, size_t *baselineStubsOptimized)
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
#ifdef JS_ION
    *baselineStubsOptimized = ionCompartment()
        ? ionCompartment()->optimizedStubSpace()->sizeOfExcludingThis(mallocSizeOf)
        : 0;
#else
    *baselineStubsOptimized = 0;
#endif
}

void
JSCompartment::adoptWorkerAllocator(Allocator *workerAllocator)
{
    zone()->allocator.arenas.adoptArenas(rt, &workerAllocator->arenas);
}

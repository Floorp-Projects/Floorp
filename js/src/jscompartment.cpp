/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
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

#if ENABLE_YARR_JIT
#include "assembler/jit/ExecutableAllocator.h"
#endif

using namespace mozilla;
using namespace js;
using namespace js::gc;

JSCompartment::JSCompartment(JSRuntime *rt)
  : rt(rt),
    principals(NULL),
    needsBarrier_(false),
    gcState(NoGCScheduled),
    gcPreserveCode(false),
    gcBytes(0),
    gcTriggerBytes(0),
    hold(false),
    lastCodeRelease(0),
    typeLifoAlloc(TYPE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    data(NULL),
    active(false),
    lastAnimationTime(0),
    regExps(rt),
    propertyTree(thisForCtor()),
    emptyTypeObject(NULL),
    gcMallocAndFreeBytes(0),
    gcTriggerMallocAndFreeBytes(0),
    gcMallocBytes(0),
    debugModeBits(rt->debugMode ? DebugFromC : 0),
    watchpointMap(NULL),
    scriptCountsMap(NULL),
    sourceMapMap(NULL),
    debugScriptMap(NULL)
{
    setGCMaxMallocBytes(rt->gcMaxMallocBytes * 0.9);
}

JSCompartment::~JSCompartment()
{
    Foreground::delete_(watchpointMap);
    Foreground::delete_(scriptCountsMap);
    Foreground::delete_(sourceMapMap);
    Foreground::delete_(debugScriptMap);
}

bool
JSCompartment::init(JSContext *cx)
{
    activeAnalysis = activeInference = false;
    types.init(cx);

    if (!crossCompartmentWrappers.init())
        return false;

    if (!regExps.init(cx))
        return false;

    return debuggees.init();
}

void
JSCompartment::setNeedsBarrier(bool needs)
{
#ifdef JS_METHODJIT
    if (needsBarrier_ != needs)
        mjit::ClearAllFrames(this);
#endif
    needsBarrier_ = needs;
}

static bool
WrapForSameCompartment(JSContext *cx, JSObject *obj, Value *vp)
{
    JS_ASSERT(cx->compartment == obj->compartment());
    if (cx->runtime->sameCompartmentWrapObjectCallback) {
        obj = cx->runtime->sameCompartmentWrapObjectCallback(cx, obj);
        if (!obj)
            return false;
    }
    vp->setObject(*obj);
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, Value *vp)
{
    JS_ASSERT(cx->compartment == this);

    unsigned flags = 0;

    JS_CHECK_RECURSION(cx, return false);

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
    RootedObject global(cx);
    if (cx->hasfp()) {
        global = &cx->fp()->global();
    } else {
        global = JS_ObjectToInnerObject(cx, cx->globalObject);
        if (!global)
            return false;
    }

    /* Unwrap incoming objects. */
    if (vp->isObject()) {
        Rooted<JSObject*> obj(cx, &vp->toObject());

        if (obj->compartment() == this)
            return WrapForSameCompartment(cx, obj, vp);

        /* Translate StopIteration singleton. */
        if (obj->isStopIteration())
            return js_FindClassObject(cx, NULL, JSProto_StopIteration, vp);

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
            if (global->getClass() != &dummy_class && obj->getParent() != global) {
                do {
                    if (!JSObject::setParent(cx, obj, global))
                        return false;
                    obj = obj->getProto();
                } while (obj && obj->isCrossCompartmentWrapper());
            }
        }
        return true;
    }

    if (vp->isString()) {
        RootedValue orig(cx, *vp);
        JSString *str = vp->toString();
        const jschar *chars = str->getChars(cx);
        if (!chars)
            return false;
        JSString *wrapped = js_NewStringCopyN(cx, chars, str->length());
        if (!wrapped)
            return false;
        vp->setString(wrapped);
        return crossCompartmentWrappers.put(orig, *vp);
    }

    RootedObject obj(cx, &vp->toObject());

    /*
     * Recurse to wrap the prototype. Long prototype chains will run out of
     * stack, causing an error in CHECK_RECURSE.
     *
     * Wrapping the proto before creating the new wrapper and adding it to the
     * cache helps avoid leaving a bad entry in the cache on OOM. But note that
     * if we wrapped both proto and parent, we would get infinite recursion
     * here (since Object.prototype->parent->proto leads to Object.prototype
     * itself).
     */
    RootedObject proto(cx, obj->getProto());
    if (!wrap(cx, proto.address()))
        return false;

    /*
     * We hand in the original wrapped object into the wrap hook to allow
     * the wrap hook to reason over what wrappers are currently applied
     * to the object.
     */
    RootedObject wrapper(cx, cx->runtime->wrapObjectCallback(cx, obj, proto, global, flags));
    if (!wrapper)
        return false;

    vp->setObject(*wrapper);

    if (wrapper->getProto() != proto && !SetProto(cx, wrapper, proto, false))
        return false;

    if (!crossCompartmentWrappers.put(key, *vp))
        return false;

    if (!JSObject::setParent(cx, wrapper, global))
        return false;
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, JSString **strp)
{
    RootedValue value(cx, StringValue(*strp));
    if (!wrap(cx, value.address()))
        return false;
    *strp = value.reference().toString();
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, HeapPtrString *strp)
{
    RootedValue value(cx, StringValue(*strp));
    if (!wrap(cx, value.address()))
        return false;
    *strp = value.reference().toString();
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, JSObject **objp)
{
    if (!*objp)
        return true;
    RootedValue value(cx, ObjectValue(**objp));
    if (!wrap(cx, value.address()))
        return false;
    *objp = &value.reference().toObject();
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
    return ValueToId(cx, value.reference(), idp);
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
JSCompartment::markTypes(JSTracer *trc)
{
    /*
     * Mark all scripts, type objects and singleton JS objects in the
     * compartment. These can be referred to directly by type sets, which we
     * cannot modify while code which depends on these type sets is active.
     */
    JS_ASSERT(activeAnalysis || gcPreserveCode);

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
}

void
JSCompartment::discardJitCode(FreeOp *fop)
{
#ifdef JS_METHODJIT

    /*
     * Kick all frames on the stack into the interpreter, and release all JIT
     * code in the compartment unless gcPreserveCode is set, in which case
     * purge all caches in the JIT scripts. Even if we are not releasing all
     * JIT code, we still need to release code for scripts which are in the
     * middle of a native or getter stub call, as these stubs will have been
     * redirected to the interpoline.
     */
    mjit::ClearAllFrames(this);

    if (gcPreserveCode) {
        for (CellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            for (int constructing = 0; constructing <= 1; constructing++) {
                for (int barriers = 0; barriers <= 1; barriers++) {
                    mjit::JITScript *jit = script->getJIT((bool) constructing, (bool) barriers);
                    if (jit)
                        jit->purgeCaches();
                }
            }
        }
    } else {
        for (CellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            mjit::ReleaseScriptCode(fop, script);

            /*
             * Use counts for scripts are reset on GC. After discarding code we
             * need to let it warm back up to get information such as which
             * opcodes are setting array holes or accessing getter properties.
             */
            script->resetUseCount();
        }
    }

#endif /* JS_METHODJIT */
}

void
JSCompartment::sweep(FreeOp *fop, bool releaseTypes)
{
    sweepCrossCompartmentWrappers();

    /* Remove dead references held weakly by the compartment. */

    sweepBaseShapeTable();
    sweepInitialShapeTable();
    sweepNewTypeObjectTable(newTypeObjects);
    sweepNewTypeObjectTable(lazyTypeObjects);

    if (emptyTypeObject && !IsTypeObjectMarked(emptyTypeObject.unsafeGet()))
        emptyTypeObject = NULL;

    sweepBreakpoints(fop);

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_DISCARD_CODE);
        discardJitCode(fop);
    }

    /* JIT code can hold references on RegExpShared, so sweep regexps after clearing code. */
    regExps.sweep(rt);

    if (!activeAnalysis && !gcPreserveCode) {
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
                JSScript *script = i.get<JSScript>();
                if (script->types) {
                    types::TypeScript::Sweep(fop, script);

                    if (releaseTypes) {
                        script->types->destroy();
                        script->types = NULL;
                        script->typesPurged = true;
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
            }
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
    /* Remove dead wrappers from the table. */
    for (WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
        CrossCompartmentKey key = e.front().key;
        bool keyMarked = IsCellMarked(&key.wrapped);
        bool valMarked = IsValueMarked(e.front().value.unsafeGet());
        bool dbgMarked = !key.debugger || IsObjectMarked(&key.debugger);
        JS_ASSERT_IF(!keyMarked && valMarked, key.kind == CrossCompartmentKey::StringWrapper);
        if (!keyMarked || !valMarked || !dbgMarked)
            e.removeFront();
        else
            e.rekeyFront(key);
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
    for (AllFramesIter i(rt->stackSpace); !i.done(); ++i) {
        JSScript *script = i.fp()->maybeScript();
        if (script && script->compartment() == this)
            return true;
    }
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
            cx->runtime->debugScopes->onCompartmentLeaveDebugMode(this);
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
    if (!rt->gcRunning)
        dmgc.scheduleGC(this);
#endif
}

bool
JSCompartment::addDebuggee(JSContext *cx, js::GlobalObject *global)
{
    bool wasEnabled = debugMode();
    if (!debuggees.put(global)) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    debugModeBits |= DebugFromJS;
    if (!wasEnabled) {
        AutoDebugModeGC dmgc(cx->runtime);
        updateForDebugMode(cx->runtime->defaultFreeOp(), dmgc);
    }
    return true;
}

void
JSCompartment::removeDebuggee(FreeOp *fop,
                              js::GlobalObject *global,
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
            AutoDebugModeGC dmgc(rt);
            fop->runtime()->debugScopes->onCompartmentLeaveDebugMode(this);
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
    if (JS_CLIST_IS_EMPTY(&rt->debuggerList))
        return;

    for (CellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (!script->hasAnyBreakpointsOrStepMode())
            continue;
        bool scriptGone = !IsScriptMarked(&script);
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
                if (scriptGone || !IsObjectMarked(&bp->debugger->toJSObjectRef()))
                    bp->destroy(fop);
            }
        }
    }
}

size_t
JSCompartment::sizeOfShapeTable(JSMallocSizeOfFun mallocSizeOf)
{
    return baseShapes.sizeOfExcludingThis(mallocSizeOf)
         + initialShapes.sizeOfExcludingThis(mallocSizeOf)
         + newTypeObjects.sizeOfExcludingThis(mallocSizeOf)
         + lazyTypeObjects.sizeOfExcludingThis(mallocSizeOf);
}

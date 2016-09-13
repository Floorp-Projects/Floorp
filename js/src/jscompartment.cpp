/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscompartmentinlines.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"

#include "jscntxt.h"
#include "jsfriendapi.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jswatchpoint.h"
#include "jswrapper.h"

#include "gc/Marking.h"
#include "gc/Policy.h"
#include "jit/JitCompartment.h"
#include "jit/JitOptions.h"
#include "js/Date.h"
#include "js/Proxy.h"
#include "js/RootingAPI.h"
#include "proxy/DeadObjectProxy.h"
#include "vm/Debugger.h"
#include "vm/StopIterationObject.h"
#include "vm/WrapperObject.h"

#include "jsatominlines.h"
#include "jsfuninlines.h"
#include "jsgcinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::jit;

using mozilla::DebugOnly;
using mozilla::PodArrayZero;

JSCompartment::JSCompartment(Zone* zone, const JS::CompartmentOptions& options = JS::CompartmentOptions())
  : creationOptions_(options.creationOptions()),
    behaviors_(options.behaviors()),
    zone_(zone),
    runtime_(zone->runtimeFromMainThread()),
    principals_(nullptr),
    isSystem_(false),
    isSelfHosting(false),
    marked(true),
    warnedAboutExprClosure(false),
    warnedAboutForEach(false),
#ifdef DEBUG
    firedOnNewGlobalObject(false),
#endif
    global_(nullptr),
    enterCompartmentDepth(0),
    performanceMonitoring(runtime_),
    data(nullptr),
    allocationMetadataBuilder(nullptr),
    lastAnimationTime(0),
    regExps(runtime_),
    globalWriteBarriered(0),
    detachedTypedObjects(0),
    objectMetadataState(ImmediateMetadata()),
    selfHostingScriptSource(nullptr),
    objectMetadataTable(nullptr),
    innerViews(zone, InnerViewTable()),
    lazyArrayBuffers(nullptr),
    wasm(zone),
    nonSyntacticLexicalEnvironments_(nullptr),
    gcIncomingGrayPointers(nullptr),
    debugModeBits(0),
    watchpointMap(nullptr),
    scriptCountsMap(nullptr),
    debugScriptMap(nullptr),
    debugEnvs(nullptr),
    enumerators(nullptr),
    lastCachedNativeIterator(nullptr),
    compartmentStats_(nullptr),
    scheduledForDestruction(false),
    maybeAlive(true),
    jitCompartment_(nullptr),
    mappedArgumentsTemplate_(nullptr),
    unmappedArgumentsTemplate_(nullptr),
    lcovOutput()
{
    PodArrayZero(sawDeprecatedLanguageExtension);
    runtime_->numCompartments++;
    MOZ_ASSERT_IF(creationOptions_.mergeable(),
                  creationOptions_.invisibleToDebugger());
}

JSCompartment::~JSCompartment()
{
    reportTelemetry();

    // Write the code coverage information in a file.
    JSRuntime* rt = runtimeFromMainThread();
    if (rt->lcovOutput.isEnabled())
        rt->lcovOutput.writeLCovResult(lcovOutput);

    js_delete(jitCompartment_);
    js_delete(watchpointMap);
    js_delete(scriptCountsMap);
    js_delete(debugScriptMap);
    js_delete(debugEnvs);
    js_delete(objectMetadataTable);
    js_delete(lazyArrayBuffers);
    js_delete(nonSyntacticLexicalEnvironments_),
    js_free(enumerators);

    runtime_->numCompartments--;
}

bool
JSCompartment::init(JSContext* maybecx)
{
    /*
     * maybecx is null when called to create the atoms compartment from
     * JSRuntime::init().
     *
     * As a hack, we clear our timezone cache every time we create a new
     * compartment. This ensures that the cache is always relatively fresh, but
     * shouldn't interfere with benchmarks that create tons of date objects
     * (unless they also create tons of iframes, which seems unlikely).
     */
    JS::ResetTimeZone();

    if (!crossCompartmentWrappers.init(0)) {
        if (maybecx)
            ReportOutOfMemory(maybecx);
        return false;
    }

    if (!regExps.init(maybecx))
        return false;

    enumerators = NativeIterator::allocateSentinel(maybecx);
    if (!enumerators)
        return false;

    if (!savedStacks_.init() || !varNames_.init()) {
        if (maybecx)
            ReportOutOfMemory(maybecx);
        return false;
    }

    return true;
}

jit::JitRuntime*
JSRuntime::createJitRuntime(JSContext* cx)
{
    // The shared stubs are created in the atoms compartment, which may be
    // accessed by other threads with an exclusive context.
    AutoLockForExclusiveAccess atomsLock(cx);

    MOZ_ASSERT(!jitRuntime_);

    jit::JitRuntime* jrt = cx->new_<jit::JitRuntime>(cx->runtime());
    if (!jrt)
        return nullptr;

    // Protect jitRuntime_ from being observed (by InterruptRunningJitCode)
    // while it is being initialized. Unfortunately, initialization depends on
    // jitRuntime_ being non-null, so we can't just wait to assign jitRuntime_.
    JitRuntime::AutoPreventBackedgePatching apbp(cx->runtime(), jrt);
    jitRuntime_ = jrt;

    AutoEnterOOMUnsafeRegion noOOM;
    if (!jitRuntime_->initialize(cx, atomsLock)) {
        // Handling OOM here is complicated: if we delete jitRuntime_ now, we
        // will destroy the ExecutableAllocator, even though there may still be
        // JitCode instances holding references to ExecutablePools.
        noOOM.crash("OOM in createJitRuntime");
    }

    return jitRuntime_;
}

bool
JSCompartment::ensureJitCompartmentExists(JSContext* cx)
{
    using namespace js::jit;
    if (jitCompartment_)
        return true;

    if (!zone()->getJitZone(cx))
        return false;

    /* Set the compartment early, so linking works. */
    jitCompartment_ = cx->new_<JitCompartment>();

    if (!jitCompartment_)
        return false;

    if (!jitCompartment_->initialize(cx)) {
        js_delete(jitCompartment_);
        jitCompartment_ = nullptr;
        return false;
    }

    return true;
}

#ifdef JSGC_HASH_TABLE_CHECKS
namespace {
struct CheckGCThingAfterMovingGCFunctor {
    template <class T> void operator()(T* t) { CheckGCThingAfterMovingGC(*t); }
};
} // namespace (anonymous)

void
JSCompartment::checkWrapperMapAfterMovingGC()
{
    /*
     * Assert that the postbarriers have worked and that nothing is left in
     * wrapperMap that points into the nursery, and that the hash table entries
     * are discoverable.
     */
    for (WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
        e.front().mutableKey().applyToWrapped(CheckGCThingAfterMovingGCFunctor());
        e.front().mutableKey().applyToDebugger(CheckGCThingAfterMovingGCFunctor());

        WrapperMap::Ptr ptr = crossCompartmentWrappers.lookup(e.front().key());
        MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &e.front());
    }
}
#endif

bool
JSCompartment::putWrapper(JSContext* cx, const CrossCompartmentKey& wrapped,
                          const js::Value& wrapper)
{
    MOZ_ASSERT(wrapped.is<JSString*>() == wrapper.isString());
    MOZ_ASSERT_IF(!wrapped.is<JSString*>(), wrapper.isObject());

    if (!crossCompartmentWrappers.put(wrapped, wrapper)) {
        ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

static JSString*
CopyStringPure(JSContext* cx, JSString* str)
{
    /*
     * Directly allocate the copy in the destination compartment, rather than
     * first flattening it (and possibly allocating in source compartment),
     * because we don't know whether the flattening will pay off later.
     */

    size_t len = str->length();
    JSString* copy;
    if (str->isLinear()) {
        /* Only use AutoStableStringChars if the NoGC allocation fails. */
        if (str->hasLatin1Chars()) {
            JS::AutoCheckCannotGC nogc;
            copy = NewStringCopyN<NoGC>(cx, str->asLinear().latin1Chars(nogc), len);
        } else {
            JS::AutoCheckCannotGC nogc;
            copy = NewStringCopyNDontDeflate<NoGC>(cx, str->asLinear().twoByteChars(nogc), len);
        }
        if (copy)
            return copy;

        AutoStableStringChars chars(cx);
        if (!chars.init(cx, str))
            return nullptr;

        return chars.isLatin1()
               ? NewStringCopyN<CanGC>(cx, chars.latin1Range().start().get(), len)
               : NewStringCopyNDontDeflate<CanGC>(cx, chars.twoByteRange().start().get(), len);
    }

    if (str->hasLatin1Chars()) {
        ScopedJSFreePtr<Latin1Char> copiedChars;
        if (!str->asRope().copyLatin1CharsZ(cx, copiedChars))
            return nullptr;

        return NewString<CanGC>(cx, copiedChars.forget(), len);
    }

    ScopedJSFreePtr<char16_t> copiedChars;
    if (!str->asRope().copyTwoByteCharsZ(cx, copiedChars))
        return nullptr;

    return NewStringDontDeflate<CanGC>(cx, copiedChars.forget(), len);
}

bool
JSCompartment::wrap(JSContext* cx, MutableHandleString strp)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(this));
    MOZ_ASSERT(cx->compartment() == this);

    /* If the string is already in this compartment, we are done. */
    JSString* str = strp;
    if (str->zoneFromAnyThread() == zone())
        return true;

    /* If the string is an atom, we don't have to copy. */
    if (str->isAtom()) {
        MOZ_ASSERT(str->isPermanentAtom() || str->zone()->isAtomsZone());
        return true;
    }

    /* Check the cache. */
    RootedValue key(cx, StringValue(str));
    if (WrapperMap::Ptr p = crossCompartmentWrappers.lookup(CrossCompartmentKey(key))) {
        strp.set(p->value().get().toString());
        return true;
    }

    /* No dice. Make a copy, and cache it. */
    JSString* copy = CopyStringPure(cx, str);
    if (!copy)
        return false;
    if (!putWrapper(cx, CrossCompartmentKey(key), StringValue(copy)))
        return false;

    strp.set(copy);
    return true;
}

bool
JSCompartment::wrap(JSContext* cx, MutableHandleObject obj, HandleObject existingArg)
{
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(this));
    MOZ_ASSERT(cx->compartment() == this);
    MOZ_ASSERT_IF(existingArg, existingArg->compartment() == cx->compartment());
    MOZ_ASSERT_IF(existingArg, IsDeadProxyObject(existingArg));

    if (!obj)
        return true;
    AutoDisableProxyCheck adpc(cx->runtime());

    // The gray bit handling here is a bit complicated.  The basic problem is
    // that generally the return value of wrap() escapes (and the input is an
    // object that _has_ "escaped") and in those cases the input must not be
    // gray and the return value must not be gray.  However, when `existingArg`
    // is non-null we're doing an internal wrapper remapping operation, and both
    // `obj` and `existingArg` might be gray, quite purposefully.
    //
    // This means that when `existingArg` is not null, we can't assert anything
    // useful about anything being non-gray.
    MOZ_ASSERT_IF(!existingArg, !ObjectIsMarkedGray(obj));

    // Wrappers should really be parented to the wrapped parent of the wrapped
    // object, but in that case a wrapped global object would have a nullptr
    // parent without being a proper global object (JSCLASS_IS_GLOBAL). Instead,
    // we parent all wrappers to the global object in their home compartment.
    // This loses us some transparency, and is generally very cheesy.
    HandleObject global = cx->global();
    RootedObject objGlobal(cx, &obj->global());
    MOZ_ASSERT(global);
    MOZ_ASSERT(objGlobal);

    if (obj->compartment() == this) {
        obj.set(ToWindowProxyIfWindow(obj));
        MOZ_ASSERT_IF(!existingArg, !ObjectIsMarkedGray(obj));
        return true;
    }

    // If we have a cross-compartment wrapper, make sure that the cx isn't
    // associated with the self-hosting global. We don't want to create
    // wrappers for objects in other runtimes, which may be the case for the
    // self-hosting global.
    MOZ_ASSERT(!cx->runtime()->isSelfHostingGlobal(global) &&
               !cx->runtime()->isSelfHostingGlobal(objGlobal));

    // Unwrap the object, but don't unwrap outer windows.
    RootedObject objectPassedToWrap(cx, obj);
    obj.set(UncheckedUnwrap(obj, /* stopAtWindowProxy = */ true));

    // We crossed a compartment boundary, so obj may be gray.  We really don't
    // want to return gray things from here if !existingArg, so make sure it's
    // not.
    if (!existingArg)
        ExposeObjectToActiveJS(obj);

    if (obj->compartment() == this) {
        MOZ_ASSERT(!IsWindow(obj));
        return true;
    }

    // Translate StopIteration singleton.
    if (obj->is<StopIterationObject>()) {
        // StopIteration isn't a constructor, but it's stored in GlobalObject
        // as one, out of laziness. Hence the GetBuiltinConstructor call here.
        RootedObject stopIteration(cx);
        if (!GetBuiltinConstructor(cx, JSProto_StopIteration, &stopIteration))
            return false;
        obj.set(stopIteration);
        MOZ_ASSERT_IF(!existingArg, !ObjectIsMarkedGray(obj));
        return true;
    }

    const JSWrapObjectCallbacks* cb = cx->runtime()->wrapObjectCallbacks;

    // Invoke the prewrap callback. We're a bit worried about infinite
    // recursion here, so we do a check - see bug 809295.
    JS_CHECK_SYSTEM_RECURSION(cx, return false);
    if (cb->preWrap) {
        cb->preWrap(cx, global, obj, objectPassedToWrap, obj);
        if (!obj)
            return false;
        MOZ_ASSERT_IF(!existingArg, !ObjectIsMarkedGray(obj));
    }
    MOZ_ASSERT(!IsWindow(obj));

    if (obj->compartment() == this)
        return true;

    // If we already have a wrapper for this value, use it.
    RootedValue key(cx, ObjectValue(*obj));
    if (WrapperMap::Ptr p = crossCompartmentWrappers.lookup(CrossCompartmentKey(key))) {
        obj.set(&p->value().get().toObject());
        MOZ_ASSERT(obj->is<CrossCompartmentWrapperObject>());
        // crossCompartmentWrappers has a read barrier, so obj is not gray now,
        // even if existing is non-null.
        MOZ_ASSERT(!ObjectIsMarkedGray(obj));
        return true;
    }

    RootedObject existing(cx, existingArg);
    if (existing) {
        // Is it possible to reuse |existing|?
        if (existing->hasStaticPrototype() ||
            // Note: Class asserted above, so all that's left to check is callability
            existing->isCallable() ||
            obj->isCallable())
        {
            existing = nullptr;
        }
    }

    RootedObject wrapper(cx, cb->wrap(cx, existing, obj));
    if (!wrapper)
        return false;
    MOZ_ASSERT_IF(!existingArg, !ObjectIsMarkedGray(wrapper));

    // We maintain the invariant that the key in the cross-compartment wrapper
    // map is always directly wrapped by the value.
    MOZ_ASSERT(Wrapper::wrappedObject(wrapper) == &key.get().toObject());

    if (!putWrapper(cx, CrossCompartmentKey(key), ObjectValue(*wrapper))) {
        // Enforce the invariant that all cross-compartment wrapper object are
        // in the map by nuking the wrapper if we couldn't add it.
        // Unfortunately it's possible for the wrapper to still be marked if we
        // took this path, for example if the object metadata callback stashes a
        // reference to it.
        if (wrapper->is<CrossCompartmentWrapperObject>())
            NukeCrossCompartmentWrapper(cx, wrapper);
        return false;
    }

    obj.set(wrapper);
    return true;
}

bool
JSCompartment::wrap(JSContext* cx, MutableHandle<PropertyDescriptor> desc)
{
    if (!wrap(cx, desc.object()))
        return false;

    if (desc.hasGetterObject()) {
        if (!wrap(cx, desc.getterObject()))
            return false;
    }
    if (desc.hasSetterObject()) {
        if (!wrap(cx, desc.setterObject()))
            return false;
    }

    return wrap(cx, desc.value());
}

bool
JSCompartment::wrap(JSContext* cx, MutableHandle<GCVector<Value>> vec)
{
    for (size_t i = 0; i < vec.length(); ++i) {
        if (!wrap(cx, vec[i]))
            return false;
    }
    return true;
}

LexicalEnvironmentObject*
JSCompartment::getOrCreateNonSyntacticLexicalEnvironment(JSContext* cx, HandleObject enclosing)
{
    if (!nonSyntacticLexicalEnvironments_) {
        nonSyntacticLexicalEnvironments_ = cx->new_<ObjectWeakMap>(cx);
        if (!nonSyntacticLexicalEnvironments_ || !nonSyntacticLexicalEnvironments_->init())
            return nullptr;
    }

    // The key is the unwrapped dynamic scope, as we may be creating different
    // WithEnvironmentObject wrappers each time.
    MOZ_ASSERT(!enclosing->as<WithEnvironmentObject>().isSyntactic());
    RootedObject key(cx, &enclosing->as<WithEnvironmentObject>().object());
    RootedObject lexicalEnv(cx, nonSyntacticLexicalEnvironments_->lookup(key));

    if (!lexicalEnv) {
        lexicalEnv = LexicalEnvironmentObject::createNonSyntactic(cx, enclosing);
        if (!lexicalEnv)
            return nullptr;
        if (!nonSyntacticLexicalEnvironments_->add(cx, key, lexicalEnv))
            return nullptr;
    }

    return &lexicalEnv->as<LexicalEnvironmentObject>();
}

LexicalEnvironmentObject*
JSCompartment::getNonSyntacticLexicalEnvironment(JSObject* enclosing) const
{
    if (!nonSyntacticLexicalEnvironments_)
        return nullptr;
    if (!enclosing->is<WithEnvironmentObject>())
        return nullptr;
    JSObject* key = &enclosing->as<WithEnvironmentObject>().object();
    JSObject* lexicalEnv = nonSyntacticLexicalEnvironments_->lookup(key);
    if (!lexicalEnv)
        return nullptr;
    return &lexicalEnv->as<LexicalEnvironmentObject>();
}

bool
JSCompartment::addToVarNames(JSContext* cx, JS::Handle<JSAtom*> name)
{
    if (varNames_.put(name.get()))
        return true;

    ReportOutOfMemory(cx);
    return false;
}

void
JSCompartment::traceOutgoingCrossCompartmentWrappers(JSTracer* trc)
{
    MOZ_ASSERT(trc->runtime()->isHeapMajorCollecting());
    MOZ_ASSERT(!zone()->isCollecting() || trc->runtime()->gc.isHeapCompacting());

    for (WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
        Value v = e.front().value().unbarrieredGet();
        if (e.front().key().is<JSObject*>()) {
            ProxyObject* wrapper = &v.toObject().as<ProxyObject>();

            /*
             * We have a cross-compartment wrapper. Its private pointer may
             * point into the compartment being collected, so we should mark it.
             */
            TraceEdge(trc, wrapper->slotOfPrivate(), "cross-compartment wrapper");
        }
    }
}

/* static */ void
JSCompartment::traceIncomingCrossCompartmentEdgesForZoneGC(JSTracer* trc)
{
    gcstats::AutoPhase ap(trc->runtime()->gc.stats, gcstats::PHASE_MARK_CCWS);
    MOZ_ASSERT(trc->runtime()->isHeapMajorCollecting());
    for (CompartmentsIter c(trc->runtime(), SkipAtoms); !c.done(); c.next()) {
        if (!c->zone()->isCollecting())
            c->traceOutgoingCrossCompartmentWrappers(trc);
    }
    Debugger::markIncomingCrossCompartmentEdges(trc);
}

void
JSCompartment::trace(JSTracer* trc)
{
    savedStacks_.trace(trc);

    // Atoms are always tenured.
    if (!trc->runtime()->isHeapMinorCollecting())
        varNames_.trace(trc);
}

void
JSCompartment::traceRoots(JSTracer* trc, js::gc::GCRuntime::TraceOrMarkRuntime traceOrMark)
{
    if (objectMetadataState.is<PendingMetadata>()) {
        TraceRoot(trc,
                  &objectMetadataState.as<PendingMetadata>(),
                  "on-stack object pending metadata");
    }

    if (!trc->runtime()->isHeapMinorCollecting()) {
        // JIT code and the global are never nursery allocated, so we only need
        // to trace them when not doing a minor collection.

        if (jitCompartment_)
            jitCompartment_->mark(trc, this);

        // If a compartment is on-stack, we mark its global so that
        // JSContext::global() remains valid.
        if (enterCompartmentDepth && global_.unbarrieredGet())
            TraceRoot(trc, global_.unsafeUnbarrieredForTracing(), "on-stack compartment global");
    }

    // Nothing below here needs to be treated as a root if we aren't marking
    // this zone for a collection.
    if (traceOrMark == js::gc::GCRuntime::MarkRuntime && !zone()->isCollecting())
        return;

    // During a GC, these are treated as weak pointers.
    if (traceOrMark == js::gc::GCRuntime::TraceRuntime) {
        if (watchpointMap)
            watchpointMap->markAll(trc);
    }

    /* Mark debug scopes, if present */
    if (debugEnvs)
        debugEnvs->mark(trc);

    if (lazyArrayBuffers)
        lazyArrayBuffers->trace(trc);

    if (objectMetadataTable)
        objectMetadataTable->trace(trc);

    // If code coverage is only enabled with the Debugger or the LCovOutput,
    // then the following comment holds.
    //
    // The scriptCountsMap maps JSScript weak-pointers to ScriptCounts
    // structures. It uses a HashMap instead of a WeakMap, so that we can keep
    // the data alive for the JSScript::finalize call. Thus, we do not trace the
    // keys of the HashMap to avoid adding a strong reference to the JSScript
    // pointers.
    //
    // If the code coverage is either enabled with the --dump-bytecode command
    // line option, or with the PCCount JSFriend API functions, then we mark the
    // keys of the map to hold the JSScript alive.
    if (scriptCountsMap &&
        trc->runtime()->profilingScripts &&
        !trc->runtime()->isHeapMinorCollecting())
    {
        MOZ_ASSERT_IF(!trc->runtime()->isBeingDestroyed(), collectCoverage());
        for (ScriptCountsMap::Range r = scriptCountsMap->all(); !r.empty(); r.popFront()) {
            JSScript* script = const_cast<JSScript*>(r.front().key());
            MOZ_ASSERT(script->hasScriptCounts());
            TraceRoot(trc, &script, "profilingScripts");
            MOZ_ASSERT(script == r.front().key(), "const_cast is only a work-around");
        }
    }

    if (nonSyntacticLexicalEnvironments_)
        nonSyntacticLexicalEnvironments_->trace(trc);

    wasm.trace(trc);
}

void
JSCompartment::finishRoots()
{
    if (watchpointMap)
        watchpointMap->clear();

    if (debugEnvs)
        debugEnvs->finish();

    if (lazyArrayBuffers)
        lazyArrayBuffers->clear();

    if (objectMetadataTable)
        objectMetadataTable->clear();

    clearScriptCounts();

    if (nonSyntacticLexicalEnvironments_)
        nonSyntacticLexicalEnvironments_->clear();
}

void
JSCompartment::sweepAfterMinorGC(JSTracer* trc)
{
    globalWriteBarriered = 0;

    if (innerViews.needsSweepAfterMinorGC())
        innerViews.sweepAfterMinorGC();

    crossCompartmentWrappers.sweepAfterMinorGC(trc);
}

void
JSCompartment::sweepSavedStacks()
{
    savedStacks_.sweep();
}

void
JSCompartment::sweepGlobalObject(FreeOp* fop)
{
    if (global_ && IsAboutToBeFinalized(&global_)) {
        if (isDebuggee())
            Debugger::detachAllDebuggersFromGlobal(fop, global_.unbarrieredGet());
        global_.set(nullptr);
    }
}

void
JSCompartment::sweepSelfHostingScriptSource()
{
    if (selfHostingScriptSource.unbarrieredGet() &&
        IsAboutToBeFinalized(&selfHostingScriptSource))
    {
        selfHostingScriptSource.set(nullptr);
    }
}

void
JSCompartment::sweepJitCompartment(FreeOp* fop)
{
    if (jitCompartment_)
        jitCompartment_->sweep(fop, this);
}

void
JSCompartment::sweepRegExps()
{
    /*
     * JIT code increments activeWarmUpCounter for any RegExpShared used by jit
     * code for the lifetime of the JIT script. Thus, we must perform
     * sweeping after clearing jit code.
     */
    regExps.sweep(runtimeFromAnyThread());
}

void
JSCompartment::sweepDebugEnvironments()
{
    JSRuntime* rt = runtimeFromAnyThread();
    if (debugEnvs)
        debugEnvs->sweep(rt);
}

void
JSCompartment::sweepNativeIterators()
{
    /* Sweep list of native iterators. */
    NativeIterator* ni = enumerators->next();
    while (ni != enumerators) {
        JSObject* iterObj = ni->iterObj();
        NativeIterator* next = ni->next();
        if (gc::IsAboutToBeFinalizedUnbarriered(&iterObj))
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
    crossCompartmentWrappers.sweep();
}

namespace {
struct TraceRootFunctor {
    JSTracer* trc;
    const char* name;
    TraceRootFunctor(JSTracer* trc, const char* name) : trc(trc), name(name) {}
    template <class T> void operator()(T* t) { return TraceRoot(trc, t, name); }
};
struct NeedsSweepUnbarrieredFunctor {
    template <class T> bool operator()(T* t) const { return IsAboutToBeFinalizedUnbarriered(t); }
};
} // namespace (anonymous)

void
CrossCompartmentKey::trace(JSTracer* trc)
{
    applyToWrapped(TraceRootFunctor(trc, "CrossCompartmentKey::wrapped"));
    applyToDebugger(TraceRootFunctor(trc, "CrossCompartmentKey::debugger"));
}

bool
CrossCompartmentKey::needsSweep()
{
    return applyToWrapped(NeedsSweepUnbarrieredFunctor()) ||
           applyToDebugger(NeedsSweepUnbarrieredFunctor());
}

void
JSCompartment::sweepTemplateObjects()
{
    if (mappedArgumentsTemplate_ && IsAboutToBeFinalized(&mappedArgumentsTemplate_))
        mappedArgumentsTemplate_.set(nullptr);

    if (unmappedArgumentsTemplate_ && IsAboutToBeFinalized(&unmappedArgumentsTemplate_))
        unmappedArgumentsTemplate_.set(nullptr);
}

/* static */ void
JSCompartment::fixupCrossCompartmentWrappersAfterMovingGC(JSTracer* trc)
{
    MOZ_ASSERT(trc->runtime()->gc.isHeapCompacting());

    for (CompartmentsIter comp(trc->runtime(), SkipAtoms); !comp.done(); comp.next()) {
        // Sweep the wrapper map to update its pointers to the wrappers.
        comp->sweepCrossCompartmentWrappers();
        // Trace the wrappers in the map to update their edges to their referents.
        comp->traceOutgoingCrossCompartmentWrappers(trc);
    }
}

void
JSCompartment::fixupAfterMovingGC()
{
    purge();
    fixupGlobal();
    objectGroups.fixupTablesAfterMovingGC();
    fixupScriptMapsAfterMovingGC();
}

void
JSCompartment::fixupGlobal()
{
    GlobalObject* global = *global_.unsafeGet();
    if (global)
        global_.set(MaybeForwarded(global));
}

void
JSCompartment::fixupScriptMapsAfterMovingGC()
{
    // Map entries are removed by JSScript::finalize, but we need to update the
    // script pointers here in case they are moved by the GC.

    if (scriptCountsMap) {
        for (ScriptCountsMap::Enum e(*scriptCountsMap); !e.empty(); e.popFront()) {
            JSScript* script = e.front().key();
            if (!IsAboutToBeFinalizedUnbarriered(&script) && script != e.front().key())
                e.rekeyFront(script);
        }
    }

    if (debugScriptMap) {
        for (DebugScriptMap::Enum e(*debugScriptMap); !e.empty(); e.popFront()) {
            JSScript* script = e.front().key();
            if (!IsAboutToBeFinalizedUnbarriered(&script) && script != e.front().key())
                e.rekeyFront(script);
        }
    }
}

#ifdef JSGC_HASH_TABLE_CHECKS
void
JSCompartment::checkScriptMapsAfterMovingGC()
{
    if (scriptCountsMap) {
        for (auto r = scriptCountsMap->all(); !r.empty(); r.popFront()) {
            JSScript* script = r.front().key();
            CheckGCThingAfterMovingGC(script);
            auto ptr = scriptCountsMap->lookup(script);
            MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
        }
    }

    if (debugScriptMap) {
        for (auto r = debugScriptMap->all(); !r.empty(); r.popFront()) {
            JSScript* script = r.front().key();
            CheckGCThingAfterMovingGC(script);
            DebugScript* ds = r.front().value();
            for (uint32_t i = 0; i < ds->numSites; i++) {
                BreakpointSite* site = ds->breakpoints[i];
                if (site)
                    CheckGCThingAfterMovingGC(site->script);
            }
            auto ptr = debugScriptMap->lookup(script);
            MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
        }
    }
}
#endif

void
JSCompartment::purge()
{
    dtoaCache.purge();
    lastCachedNativeIterator = nullptr;
}

void
JSCompartment::clearTables()
{
    global_.set(nullptr);

    // No scripts should have run in this compartment. This is used when
    // merging a compartment that has been used off thread into another
    // compartment and zone.
    MOZ_ASSERT(crossCompartmentWrappers.empty());
    MOZ_ASSERT(!jitCompartment_);
    MOZ_ASSERT(!debugEnvs);
    MOZ_ASSERT(enumerators->next() == enumerators);
    MOZ_ASSERT(regExps.empty());

    objectGroups.clearTables();
    if (savedStacks_.initialized())
        savedStacks_.clear();
    if (varNames_.initialized())
        varNames_.clear();
}

void
JSCompartment::setAllocationMetadataBuilder(const js::AllocationMetadataBuilder *builder)
{
    // Clear any jitcode in the runtime, which behaves differently depending on
    // whether there is a creation callback.
    ReleaseAllJITCode(runtime_->defaultFreeOp());

    allocationMetadataBuilder = builder;
}

void
JSCompartment::clearObjectMetadata()
{
    js_delete(objectMetadataTable);
    objectMetadataTable = nullptr;
}

void
JSCompartment::setNewObjectMetadata(JSContext* cx, HandleObject obj)
{
    assertSameCompartment(cx, this, obj);

    AutoEnterOOMUnsafeRegion oomUnsafe;
    if (JSObject* metadata = allocationMetadataBuilder->build(cx, obj, oomUnsafe)) {
        assertSameCompartment(cx, metadata);
        if (!objectMetadataTable) {
            objectMetadataTable = cx->new_<ObjectWeakMap>(cx);
            if (!objectMetadataTable || !objectMetadataTable->init())
                oomUnsafe.crash("setNewObjectMetadata");
        }
        if (!objectMetadataTable->add(cx, obj, metadata))
            oomUnsafe.crash("setNewObjectMetadata");
    }
}

static bool
AddInnerLazyFunctionsFromScript(JSScript* script, AutoObjectVector& lazyFunctions)
{
    if (!script->hasObjects())
        return true;
    ObjectArray* objects = script->objects();
    for (size_t i = 0; i < objects->length; i++) {
        JSObject* obj = objects->vector[i];
        if (obj->is<JSFunction>() && obj->as<JSFunction>().isInterpretedLazy()) {
            if (!lazyFunctions.append(obj))
                return false;
        }
    }
    return true;
}

static bool
AddLazyFunctionsForCompartment(JSContext* cx, AutoObjectVector& lazyFunctions, AllocKind kind)
{
    // Find all live root lazy functions in the compartment: those which have a
    // source object, indicating that they have a parent, and which do not have
    // an uncompiled enclosing script. The last condition is so that we don't
    // compile lazy scripts whose enclosing scripts failed to compile,
    // indicating that the lazy script did not escape the script.
    //
    // Some LazyScripts have a non-null |JSScript* script| pointer. We still
    // want to delazify in that case: this pointer is weak so the JSScript
    // could be destroyed at the next GC.

    for (auto i = cx->zone()->cellIter<JSObject>(kind); !i.done(); i.next()) {
        JSFunction* fun = &i->as<JSFunction>();

        // Sweeping is incremental; take care to not delazify functions that
        // are about to be finalized. GC things referenced by objects that are
        // about to be finalized (e.g., in slots) may already be freed.
        if (gc::IsAboutToBeFinalizedUnbarriered(&fun) ||
            fun->compartment() != cx->compartment())
        {
            continue;
        }

        if (fun->isInterpretedLazy()) {
            LazyScript* lazy = fun->lazyScriptOrNull();
            if (lazy && lazy->sourceObject() && !lazy->hasUncompiledEnclosingScript()) {
                if (!lazyFunctions.append(fun))
                    return false;
            }
        }
    }

    return true;
}

static bool
CreateLazyScriptsForCompartment(JSContext* cx)
{
    AutoObjectVector lazyFunctions(cx);

    if (!AddLazyFunctionsForCompartment(cx, lazyFunctions, AllocKind::FUNCTION))
        return false;

    // Methods, for instance {get method() {}}, are extended functions that can
    // be relazified, so we need to handle those as well.
    if (!AddLazyFunctionsForCompartment(cx, lazyFunctions, AllocKind::FUNCTION_EXTENDED))
        return false;

    // Create scripts for each lazy function, updating the list of functions to
    // process with any newly exposed inner functions in created scripts.
    // A function cannot be delazified until its outer script exists.
    for (size_t i = 0; i < lazyFunctions.length(); i++) {
        JSFunction* fun = &lazyFunctions[i]->as<JSFunction>();

        // lazyFunctions may have been populated with multiple functions for
        // a lazy script.
        if (!fun->isInterpretedLazy())
            continue;

        LazyScript* lazy = fun->lazyScript();
        bool lazyScriptHadNoScript = !lazy->maybeScript();

        JSScript* script = fun->getOrCreateScript(cx);
        if (!script)
            return false;
        if (lazyScriptHadNoScript && !AddInnerLazyFunctionsFromScript(script, lazyFunctions))
            return false;
    }

    return true;
}

bool
JSCompartment::ensureDelazifyScriptsForDebugger(JSContext* cx)
{
    MOZ_ASSERT(cx->compartment() == this);
    if (needsDelazificationForDebugger() && !CreateLazyScriptsForCompartment(cx))
        return false;
    debugModeBits &= ~DebuggerNeedsDelazification;
    return true;
}

void
JSCompartment::updateDebuggerObservesFlag(unsigned flag)
{
    MOZ_ASSERT(isDebuggee());
    MOZ_ASSERT(flag == DebuggerObservesAllExecution ||
               flag == DebuggerObservesCoverage ||
               flag == DebuggerObservesAsmJS);

    GlobalObject* global = zone()->runtimeFromMainThread()->gc.isForegroundSweeping()
                           ? unsafeUnbarrieredMaybeGlobal()
                           : maybeGlobal();
    const GlobalObject::DebuggerVector* v = global->getDebuggers();
    for (auto p = v->begin(); p != v->end(); p++) {
        Debugger* dbg = *p;
        if (flag == DebuggerObservesAllExecution ? dbg->observesAllExecution() :
            flag == DebuggerObservesCoverage ? dbg->observesCoverage() :
            dbg->observesAsmJS())
        {
            debugModeBits |= flag;
            return;
        }
    }

    debugModeBits &= ~flag;
}

void
JSCompartment::unsetIsDebuggee()
{
    if (isDebuggee()) {
        debugModeBits &= ~DebuggerObservesMask;
        DebugEnvironments::onCompartmentUnsetIsDebuggee(this);
    }
}

void
JSCompartment::updateDebuggerObservesCoverage()
{
    bool previousState = debuggerObservesCoverage();
    updateDebuggerObservesFlag(DebuggerObservesCoverage);
    if (previousState == debuggerObservesCoverage())
        return;

    if (debuggerObservesCoverage()) {
        // Interrupt any running interpreter frame. The scriptCounts are
        // allocated on demand when a script resume its execution.
        for (ActivationIterator iter(runtimeFromMainThread()); !iter.done(); ++iter) {
            if (iter->isInterpreter())
                iter->asInterpreter()->enableInterruptsUnconditionally();
        }
        return;
    }

    // If code coverage is enabled by any other means, keep it.
    if (collectCoverage())
        return;

    clearScriptCounts();
}

bool
JSCompartment::collectCoverage() const
{
    return collectCoverageForPGO() ||
           collectCoverageForDebug();
}

bool
JSCompartment::collectCoverageForPGO() const
{
    return !JitOptions.disablePgo;
}

bool
JSCompartment::collectCoverageForDebug() const
{
    return debuggerObservesCoverage() ||
           runtimeFromAnyThread()->profilingScripts ||
           runtimeFromAnyThread()->lcovOutput.isEnabled();
}

void
JSCompartment::clearScriptCounts()
{
    if (!scriptCountsMap)
        return;

    // Clear all hasScriptCounts_ flags of JSScript, in order to release all
    // ScriptCounts entry of the current compartment.
    for (ScriptCountsMap::Range r = scriptCountsMap->all(); !r.empty(); r.popFront()) {
        ScriptCounts* value = r.front().value();
        r.front().key()->takeOverScriptCountsMapEntry(value);
        js_delete(value);
    }

    js_delete(scriptCountsMap);
    scriptCountsMap = nullptr;
}

void
JSCompartment::clearBreakpointsIn(FreeOp* fop, js::Debugger* dbg, HandleObject handler)
{
    for (auto script = zone()->cellIter<JSScript>(); !script.done(); script.next()) {
        if (script->compartment() == this && script->hasAnyBreakpointsOrStepMode())
            script->clearBreakpointsIn(fop, dbg, handler);
    }
}

void
JSCompartment::addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                      size_t* tiAllocationSiteTables,
                                      size_t* tiArrayTypeTables,
                                      size_t* tiObjectTypeTables,
                                      size_t* compartmentObject,
                                      size_t* compartmentTables,
                                      size_t* innerViewsArg,
                                      size_t* lazyArrayBuffersArg,
                                      size_t* objectMetadataTablesArg,
                                      size_t* crossCompartmentWrappersArg,
                                      size_t* regexpCompartment,
                                      size_t* savedStacksSet,
                                      size_t* varNamesSet,
                                      size_t* nonSyntacticLexicalEnvironmentsArg,
                                      size_t* jitCompartment,
                                      size_t* privateData)
{
    *compartmentObject += mallocSizeOf(this);
    objectGroups.addSizeOfExcludingThis(mallocSizeOf, tiAllocationSiteTables,
                                        tiArrayTypeTables, tiObjectTypeTables,
                                        compartmentTables);
    wasm.addSizeOfExcludingThis(mallocSizeOf, compartmentTables);
    *innerViewsArg += innerViews.sizeOfExcludingThis(mallocSizeOf);
    if (lazyArrayBuffers)
        *lazyArrayBuffersArg += lazyArrayBuffers->sizeOfIncludingThis(mallocSizeOf);
    if (objectMetadataTable)
        *objectMetadataTablesArg += objectMetadataTable->sizeOfIncludingThis(mallocSizeOf);
    *crossCompartmentWrappersArg += crossCompartmentWrappers.sizeOfExcludingThis(mallocSizeOf);
    *regexpCompartment += regExps.sizeOfExcludingThis(mallocSizeOf);
    *savedStacksSet += savedStacks_.sizeOfExcludingThis(mallocSizeOf);
    *varNamesSet += varNames_.sizeOfExcludingThis(mallocSizeOf);
    if (nonSyntacticLexicalEnvironments_)
        *nonSyntacticLexicalEnvironmentsArg +=
            nonSyntacticLexicalEnvironments_->sizeOfIncludingThis(mallocSizeOf);
    if (jitCompartment_)
        *jitCompartment += jitCompartment_->sizeOfIncludingThis(mallocSizeOf);

    auto callback = runtime_->sizeOfIncludingThisCompartmentCallback;
    if (callback)
        *privateData += callback(mallocSizeOf, this);
}

void
JSCompartment::reportTelemetry()
{
    // Only report telemetry for web content and add-ons, not chrome JS.
    if (isSystem_)
        return;

    // Hazard analysis can't tell that the telemetry callbacks don't GC.
    JS::AutoSuppressGCAnalysis nogc;

    int id = creationOptions_.addonIdOrNull()
             ? JS_TELEMETRY_DEPRECATED_LANGUAGE_EXTENSIONS_IN_ADDONS
             : JS_TELEMETRY_DEPRECATED_LANGUAGE_EXTENSIONS_IN_CONTENT;

    // Call back into Firefox's Telemetry reporter.
    for (size_t i = 0; i < DeprecatedLanguageExtensionCount; i++) {
        if (sawDeprecatedLanguageExtension[i])
            runtime_->addTelemetry(id, i);
    }
}

void
JSCompartment::addTelemetry(const char* filename, DeprecatedLanguageExtension e)
{
    // Only report telemetry for web content and add-ons, not chrome JS.
    if (isSystem_)
        return;
    if (!creationOptions_.addonIdOrNull() && (!filename || strncmp(filename, "http", 4) != 0))
        return;

    sawDeprecatedLanguageExtension[e] = true;
}

AutoSetNewObjectMetadata::AutoSetNewObjectMetadata(ExclusiveContext* ecx
                                                   MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
    : CustomAutoRooter(ecx)
    , cx_(ecx->maybeJSContext())
    , prevState_(ecx->compartment()->objectMetadataState)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (cx_)
        cx_->compartment()->objectMetadataState = NewObjectMetadataState(DelayMetadata());
}

AutoSetNewObjectMetadata::~AutoSetNewObjectMetadata()
{
    // If we don't have a cx, we didn't change the metadata state, so no need to
    // reset it here.
    if (!cx_)
        return;

    if (!cx_->isExceptionPending() && cx_->compartment()->hasObjectPendingMetadata()) {
        // This destructor often runs upon exit from a function that is
        // returning an unrooted pointer to a Cell. The allocation metadata
        // callback often allocates; if it causes a GC, then the Cell pointer
        // being returned won't be traced or relocated.
        //
        // The only extant callbacks are those internal to SpiderMonkey that
        // capture the JS stack. In fact, we're considering removing general
        // callbacks altogther in bug 1236748. Since it's not running arbitrary
        // code, it's adequate to simply suppress GC while we run the callback.
        AutoSuppressGC autoSuppressGC(cx_);

        JSObject* obj = cx_->compartment()->objectMetadataState.as<PendingMetadata>();

        // Make sure to restore the previous state before setting the object's
        // metadata. SetNewObjectMetadata asserts that the state is not
        // PendingMetadata in order to ensure that metadata callbacks are called
        // in order.
        cx_->compartment()->objectMetadataState = prevState_;

        obj = SetNewObjectMetadata(cx_, obj);
    } else {
        cx_->compartment()->objectMetadataState = prevState_;
    }
}


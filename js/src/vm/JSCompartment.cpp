/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/JSCompartment-inl.h"

#include "mozilla/MemoryReporting.h"

#include <stddef.h>

#include "jsfriendapi.h"

#include "gc/Policy.h"
#include "gc/PublicIterators.h"
#include "jit/JitOptions.h"
#include "jit/JitRealm.h"
#include "js/Date.h"
#include "js/Proxy.h"
#include "js/RootingAPI.h"
#include "js/Wrapper.h"
#include "proxy/DeadObjectProxy.h"
#include "vm/Debugger.h"
#include "vm/Iteration.h"
#include "vm/JSContext.h"
#include "vm/WrapperObject.h"

#include "gc/GC-inl.h"
#include "gc/Marking-inl.h"
#include "vm/JSAtom-inl.h"
#include "vm/JSFunction-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/UnboxedObject-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::jit;

using mozilla::PodArrayZero;

JSCompartment::JSCompartment(Zone* zone)
  : zone_(zone),
    runtime_(zone->runtimeFromAnyThread())
{}

ObjectRealm::ObjectRealm(JS::Zone* zone)
  : innerViews(zone)
{}

ObjectRealm::~ObjectRealm()
{
    MOZ_ASSERT(enumerators == iteratorSentinel_.get());
}

Realm::Realm(JS::Zone* zone, const JS::RealmOptions& options)
  : JSCompartment(zone),
    creationOptions_(options.creationOptions()),
    behaviors_(options.behaviors()),
    global_(nullptr),
    objects_(zone),
    randomKeyGenerator_(runtime_->forkRandomKeyGenerator()),
    wasm(zone->runtimeFromMainThread()),
    performanceMonitoring(runtime_)
{
    MOZ_ASSERT_IF(creationOptions_.mergeable(),
                  creationOptions_.invisibleToDebugger());

    runtime_->numRealms++;
}

Realm::~Realm()
{
    // Write the code coverage information in a file.
    JSRuntime* rt = runtimeFromMainThread();
    if (rt->lcovOutput().isEnabled())
        rt->lcovOutput().writeLCovResult(lcovOutput);

#ifdef DEBUG
    // Avoid assertion destroying the unboxed layouts list if the embedding
    // leaked GC things.
    if (!runtime_->gc.shutdownCollectedEverything())
        objectGroups_.unboxedLayouts.clear();
#endif

    MOZ_ASSERT(runtime_->numRealms > 0);
    runtime_->numRealms--;
}

bool
JSCompartment::init(JSContext* cx)
{
    if (!crossCompartmentWrappers.init(0)) {
        ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

bool
ObjectRealm::init(JSContext* cx)
{
    if (!iteratorCache.init()) {
        ReportOutOfMemory(cx);
        return false;
    }

    NativeIteratorSentinel sentinel(NativeIterator::allocateSentinel(cx));
    if (!sentinel)
        return false;

    iteratorSentinel_ = std::move(sentinel);
    enumerators = iteratorSentinel_.get();
    return true;
}

bool
Realm::init(JSContext* cx)
{
    // Initialize JSCompartment. This is temporary until Realm and
    // JSCompartment are completely separated.
    if (!JSCompartment::init(cx))
        return false;

    /*
     * As a hack, we clear our timezone cache every time we create a new realm.
     * This ensures that the cache is always relatively fresh, but shouldn't
     * interfere with benchmarks that create tons of date objects (unless they
     * also create tons of iframes, which seems unlikely).
     */
    JS::ResetTimeZone();

    if (!objects_.init(cx))
        return false;

    if (!savedStacks_.init() ||
        !varNames_.init())
    {
        ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

jit::JitRuntime*
JSRuntime::createJitRuntime(JSContext* cx)
{
    // The shared stubs are created in the atoms zone, which may be
    // accessed by other threads with an exclusive context.
    AutoLockForExclusiveAccess atomsLock(cx);

    MOZ_ASSERT(!jitRuntime_);

    if (!CanLikelyAllocateMoreExecutableMemory()) {
        // Report OOM instead of potentially hitting the MOZ_CRASH below.
        ReportOutOfMemory(cx);
        return nullptr;
    }

    jit::JitRuntime* jrt = cx->new_<jit::JitRuntime>();
    if (!jrt)
        return nullptr;

    // Unfortunately, initialization depends on jitRuntime_ being non-null, so
    // we can't just wait to assign jitRuntime_.
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
Realm::ensureJitRealmExists(JSContext* cx)
{
    using namespace js::jit;
    if (jitRealm_)
        return true;

    if (!zone()->getJitZone(cx))
        return false;

    UniquePtr<JitRealm> jitRealm = cx->make_unique<JitRealm>();
    if (!jitRealm)
        return false;

    if (!jitRealm->initialize(cx))
        return false;

    jitRealm_ = std::move(jitRealm);
    return true;
}

#ifdef JSGC_HASH_TABLE_CHECKS

void
js::DtoaCache::checkCacheAfterMovingGC()
{
    MOZ_ASSERT(!s || !IsForwarded(s));
}

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

#endif // JSGC_HASH_TABLE_CHECKS

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
               ? NewStringCopyN<CanGC>(cx, chars.latin1Range().begin().get(), len)
               : NewStringCopyNDontDeflate<CanGC>(cx, chars.twoByteRange().begin().get(), len);
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
    MOZ_ASSERT(cx->compartment() == this);

    /* If the string is already in this compartment, we are done. */
    JSString* str = strp;
    if (str->zoneFromAnyThread() == zone())
        return true;

    /*
     * If the string is an atom, we don't have to copy, but we do need to mark
     * the atom as being in use by the new zone.
     */
    if (str->isAtom()) {
        cx->markAtom(&str->asAtom());
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

#ifdef ENABLE_BIGINT
bool
JSCompartment::wrap(JSContext* cx, MutableHandleBigInt bi)
{
    MOZ_ASSERT(cx->compartment() == this);

    if (bi->zone() == cx->zone())
        return true;

    BigInt* copy = BigInt::copy(cx, bi);
    if (!copy)
        return false;
    bi.set(copy);
    return true;
}
#endif

bool
JSCompartment::getNonWrapperObjectForCurrentCompartment(JSContext* cx, MutableHandleObject obj)
{
    // Ensure that we have entered a compartment.
    MOZ_ASSERT(cx->global());

    // If we have a cross-compartment wrapper, make sure that the cx isn't
    // associated with the self-hosting global. We don't want to create
    // wrappers for objects in other runtimes, which may be the case for the
    // self-hosting global.
    MOZ_ASSERT(!cx->runtime()->isSelfHostingGlobal(cx->global()));
    MOZ_ASSERT(!cx->runtime()->isSelfHostingGlobal(&obj->global()));

    // The object is already in the right compartment. Normally same-
    // compartment returns the object itself, however, windows are always
    // wrapped by a proxy, so we have to check for that case here manually.
    if (obj->compartment() == this) {
        obj.set(ToWindowProxyIfWindow(obj));
        return true;
    }

    // Note that if the object is same-compartment, but has been wrapped into a
    // different compartment, we need to unwrap it and return the bare same-
    // compartment object. Note again that windows are always wrapped by a
    // WindowProxy even when same-compartment so take care not to strip this
    // particular wrapper.
    RootedObject objectPassedToWrap(cx, obj);
    obj.set(UncheckedUnwrap(obj, /* stopAtWindowProxy = */ true));
    if (obj->compartment() == this) {
        MOZ_ASSERT(!IsWindow(obj));
        return true;
    }

    // Invoke the prewrap callback. The prewrap callback is responsible for
    // doing similar reification as above, but can account for any additional
    // embedder requirements.
    //
    // We're a bit worried about infinite recursion here, so we do a check -
    // see bug 809295.
    auto preWrap = cx->runtime()->wrapObjectCallbacks->preWrap;
    if (!CheckSystemRecursionLimit(cx))
        return false;
    if (preWrap) {
        preWrap(cx, cx->global(), obj, objectPassedToWrap, obj);
        if (!obj)
            return false;
    }
    MOZ_ASSERT(!IsWindow(obj));

    return true;
}

bool
JSCompartment::getOrCreateWrapper(JSContext* cx, HandleObject existing, MutableHandleObject obj)
{
    // If we already have a wrapper for this value, use it.
    RootedValue key(cx, ObjectValue(*obj));
    if (WrapperMap::Ptr p = crossCompartmentWrappers.lookup(CrossCompartmentKey(key))) {
        obj.set(&p->value().get().toObject());
        MOZ_ASSERT(obj->is<CrossCompartmentWrapperObject>());
        return true;
    }

    // Ensure that the wrappee is exposed in case we are creating a new wrapper
    // for a gray object.
    ExposeObjectToActiveJS(obj);

    // Create a new wrapper for the object.
    auto wrap = cx->runtime()->wrapObjectCallbacks->wrap;
    RootedObject wrapper(cx, wrap(cx, existing, obj));
    if (!wrapper)
        return false;

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
JSCompartment::wrap(JSContext* cx, MutableHandleObject obj)
{
    MOZ_ASSERT(cx->compartment() == this);

    if (!obj)
        return true;

    AutoDisableProxyCheck adpc;

    // Anything we're wrapping has already escaped into script, so must have
    // been unmarked-gray at some point in the past.
    MOZ_ASSERT(JS::ObjectIsNotGray(obj));

    // The passed object may already be wrapped, or may fit a number of special
    // cases that we need to check for and manually correct.
    if (!getNonWrapperObjectForCurrentCompartment(cx, obj))
        return false;

    // If the reification above did not result in a same-compartment object,
    // get or create a new wrapper object in this compartment for it.
    if (obj->compartment() != this) {
        if (!getOrCreateWrapper(cx, nullptr, obj))
            return false;
    }

    // Ensure that the wrapper is also exposed.
    ExposeObjectToActiveJS(obj);
    return true;
}

bool
JSCompartment::rewrap(JSContext* cx, MutableHandleObject obj, HandleObject existingArg)
{
    MOZ_ASSERT(cx->compartment() == this);
    MOZ_ASSERT(obj);
    MOZ_ASSERT(existingArg);
    MOZ_ASSERT(existingArg->compartment() == cx->compartment());
    MOZ_ASSERT(IsDeadProxyObject(existingArg));

    AutoDisableProxyCheck adpc;

    // It may not be possible to re-use existing; if so, clear it so that we
    // are forced to create a new wrapper. Note that this cannot call out to
    // |wrap| because of the different gray unmarking semantics.
    RootedObject existing(cx, existingArg);
    if (existing->hasStaticPrototype() ||
        // Note: Class asserted above, so all that's left to check is callability
        existing->isCallable() ||
        obj->isCallable())
    {
        existing.set(nullptr);
    }

    // The passed object may already be wrapped, or may fit a number of special
    // cases that we need to check for and manually correct.
    if (!getNonWrapperObjectForCurrentCompartment(cx, obj))
        return false;

    // If the reification above resulted in a same-compartment object, we do
    // not need to create or return an existing wrapper.
    if (obj->compartment() == this)
        return true;

    return getOrCreateWrapper(cx, existing, obj);
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
ObjectRealm::getOrCreateNonSyntacticLexicalEnvironment(JSContext* cx, HandleObject enclosing)
{
    MOZ_ASSERT(&ObjectRealm::get(enclosing) == this);

    if (!nonSyntacticLexicalEnvironments_) {
        auto map = cx->make_unique<ObjectWeakMap>(cx);
        if (!map || !map->init())
            return nullptr;

        nonSyntacticLexicalEnvironments_ = std::move(map);
    }

    // If a wrapped WithEnvironmentObject was passed in, unwrap it, as we may
    // be creating different WithEnvironmentObject wrappers each time.
    RootedObject key(cx, enclosing);
    if (enclosing->is<WithEnvironmentObject>()) {
        MOZ_ASSERT(!enclosing->as<WithEnvironmentObject>().isSyntactic());
        key = &enclosing->as<WithEnvironmentObject>().object();
    }
    RootedObject lexicalEnv(cx, nonSyntacticLexicalEnvironments_->lookup(key));

    if (!lexicalEnv) {
        // NOTE: The default global |this| value is set to key for compatibility
        // with existing users of the lexical environment cache.
        //  - When used by shared-global JSM loader, |this| must be the
        //    NonSyntacticVariablesObject passed as enclosing.
        //  - When used by SubscriptLoader, |this| must be the target object of
        //    the WithEnvironmentObject wrapper.
        //  - When used by XBL/DOM Events, we execute directly as a function and
        //    do not access the |this| value.
        // See js::GetFunctionThis / js::GetNonSyntacticGlobalThis
        MOZ_ASSERT(key->is<NonSyntacticVariablesObject>() || !key->is<EnvironmentObject>());
        lexicalEnv = LexicalEnvironmentObject::createNonSyntactic(cx, enclosing, /*thisv = */key);
        if (!lexicalEnv)
            return nullptr;
        if (!nonSyntacticLexicalEnvironments_->add(cx, key, lexicalEnv))
            return nullptr;
    }

    return &lexicalEnv->as<LexicalEnvironmentObject>();
}

LexicalEnvironmentObject*
ObjectRealm::getNonSyntacticLexicalEnvironment(JSObject* enclosing) const
{
    MOZ_ASSERT(&ObjectRealm::get(enclosing) == this);

    if (!nonSyntacticLexicalEnvironments_)
        return nullptr;
    // If a wrapped WithEnvironmentObject was passed in, unwrap it as in
    // getOrCreateNonSyntacticLexicalEnvironment.
    JSObject* key = enclosing;
    if (enclosing->is<WithEnvironmentObject>()) {
        MOZ_ASSERT(!enclosing->as<WithEnvironmentObject>().isSyntactic());
        key = &enclosing->as<WithEnvironmentObject>().object();
    }
    JSObject* lexicalEnv = nonSyntacticLexicalEnvironments_->lookup(key);
    if (!lexicalEnv)
        return nullptr;
    return &lexicalEnv->as<LexicalEnvironmentObject>();
}

bool
Realm::addToVarNames(JSContext* cx, JS::Handle<JSAtom*> name)
{
    MOZ_ASSERT(name);

    if (varNames_.put(name))
        return true;

    ReportOutOfMemory(cx);
    return false;
}

void
JSCompartment::traceOutgoingCrossCompartmentWrappers(JSTracer* trc)
{
    MOZ_ASSERT(JS::CurrentThreadIsHeapMajorCollecting());
    MOZ_ASSERT(!zone()->isCollectingFromAnyThread() || trc->runtime()->gc.isHeapCompacting());

    for (NonStringWrapperEnum e(this); !e.empty(); e.popFront()) {
        if (e.front().key().is<JSObject*>()) {
            Value v = e.front().value().unbarrieredGet();
            ProxyObject* wrapper = &v.toObject().as<ProxyObject>();

            /*
             * We have a cross-compartment wrapper. Its private pointer may
             * point into the compartment being collected, so we should mark it.
             */
            ProxyObject::traceEdgeToTarget(trc, wrapper);
        }
    }
}

/* static */ void
JSCompartment::traceIncomingCrossCompartmentEdgesForZoneGC(JSTracer* trc)
{
    gcstats::AutoPhase ap(trc->runtime()->gc.stats(), gcstats::PhaseKind::MARK_CCWS);
    MOZ_ASSERT(JS::CurrentThreadIsHeapMajorCollecting());
    for (CompartmentsIter c(trc->runtime()); !c.done(); c.next()) {
        if (!c->zone()->isCollecting())
            c->traceOutgoingCrossCompartmentWrappers(trc);
    }
    Debugger::traceIncomingCrossCompartmentEdges(trc);
}

void
Realm::traceGlobal(JSTracer* trc)
{
    // Trace things reachable from the realm's global. Note that these edges
    // must be swept too in case the realm is live but the global is not.

    savedStacks_.trace(trc);

    // Atoms are always tenured.
    if (!JS::CurrentThreadIsHeapMinorCollecting())
        varNames_.trace(trc);
}

void
ObjectRealm::trace(JSTracer* trc)
{
    if (lazyArrayBuffers)
        lazyArrayBuffers->trace(trc);

    if (objectMetadataTable)
        objectMetadataTable->trace(trc);

    if (nonSyntacticLexicalEnvironments_)
        nonSyntacticLexicalEnvironments_->trace(trc);
}

void
Realm::traceRoots(JSTracer* trc, js::gc::GCRuntime::TraceOrMarkRuntime traceOrMark)
{
    if (objectMetadataState_.is<PendingMetadata>()) {
        TraceRoot(trc,
                  &objectMetadataState_.as<PendingMetadata>(),
                  "on-stack object pending metadata");
    }

    if (!JS::CurrentThreadIsHeapMinorCollecting()) {
        // The global is never nursery allocated, so we don't need to
        // trace it when doing a minor collection.
        //
        // If a compartment is on-stack, we mark its global so that
        // JSContext::global() remains valid.
        if (shouldTraceGlobal() && global_.unbarrieredGet())
            TraceRoot(trc, global_.unsafeUnbarrieredForTracing(), "on-stack compartment global");
    }

    // Nothing below here needs to be treated as a root if we aren't marking
    // this zone for a collection.
    if (traceOrMark == js::gc::GCRuntime::MarkRuntime && !zone()->isCollectingFromAnyThread())
        return;

    /* Mark debug scopes, if present */
    if (debugEnvs_)
        debugEnvs_->trace(trc);

    objects_.trace(trc);

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
        !JS::CurrentThreadIsHeapMinorCollecting())
    {
        MOZ_ASSERT_IF(!trc->runtime()->isBeingDestroyed(), collectCoverage());
        for (ScriptCountsMap::Range r = scriptCountsMap->all(); !r.empty(); r.popFront()) {
            JSScript* script = const_cast<JSScript*>(r.front().key());
            MOZ_ASSERT(script->hasScriptCounts());
            TraceRoot(trc, &script, "profilingScripts");
            MOZ_ASSERT(script == r.front().key(), "const_cast is only a work-around");
        }
    }
}

void
ObjectRealm::finishRoots()
{
    if (lazyArrayBuffers)
        lazyArrayBuffers->clear();

    if (objectMetadataTable)
        objectMetadataTable->clear();

    if (nonSyntacticLexicalEnvironments_)
        nonSyntacticLexicalEnvironments_->clear();
}

void
Realm::finishRoots()
{
    if (debugEnvs_)
        debugEnvs_->finish();

    objects_.finishRoots();

    clearScriptCounts();
    clearScriptNames();
}

void
ObjectRealm::sweepAfterMinorGC()
{
    InnerViewTable& table = innerViews.get();
    if (table.needsSweepAfterMinorGC())
        table.sweepAfterMinorGC();
}

void
Realm::sweepAfterMinorGC()
{
    globalWriteBarriered = 0;
    dtoaCache.purge();
    objects_.sweepAfterMinorGC();
}

void
JSCompartment::sweepAfterMinorGC(JSTracer* trc)
{
    crossCompartmentWrappers.sweepAfterMinorGC(trc);

    for (RealmsInCompartmentIter r(this); !r.done(); r.next())
        r->sweepAfterMinorGC();
}

void
Realm::sweepSavedStacks()
{
    savedStacks_.sweep();
}

void
Realm::sweepGlobalObject()
{
    if (global_ && IsAboutToBeFinalized(&global_))
        global_.set(nullptr);
}

void
Realm::sweepSelfHostingScriptSource()
{
    if (selfHostingScriptSource.unbarrieredGet() &&
        IsAboutToBeFinalized(&selfHostingScriptSource))
    {
        selfHostingScriptSource.set(nullptr);
    }
}

void
Realm::sweepJitRealm()
{
    if (jitRealm_)
        jitRealm_->sweep(this);
}

void
Realm::sweepRegExps()
{
    /*
     * JIT code increments activeWarmUpCounter for any RegExpShared used by jit
     * code for the lifetime of the JIT script. Thus, we must perform
     * sweeping after clearing jit code.
     */
    regExps.sweep();
}

void
Realm::sweepDebugEnvironments()
{
    if (debugEnvs_)
        debugEnvs_->sweep();
}

void
ObjectRealm::sweepNativeIterators()
{
    /* Sweep list of native iterators. */
    NativeIterator* ni = enumerators->next();
    while (ni != enumerators) {
        JSObject* iterObj = ni->iterObj();
        NativeIterator* next = ni->next();
        if (gc::IsAboutToBeFinalizedUnbarriered(&iterObj))
            ni->unlink();
        MOZ_ASSERT_IF(ni->objectBeingIterated(),
                      &ObjectRealm::get(ni->objectBeingIterated()) == this);
        ni = next;
    }
}

void
Realm::sweepObjectRealm()
{
    objects_.sweepNativeIterators();
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

void
Realm::sweepVarNames()
{
    varNames_.sweep();
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
Realm::sweepTemplateObjects()
{
    if (mappedArgumentsTemplate_ && IsAboutToBeFinalized(&mappedArgumentsTemplate_))
        mappedArgumentsTemplate_.set(nullptr);

    if (unmappedArgumentsTemplate_ && IsAboutToBeFinalized(&unmappedArgumentsTemplate_))
        unmappedArgumentsTemplate_.set(nullptr);

    if (iterResultTemplate_ && IsAboutToBeFinalized(&iterResultTemplate_))
        iterResultTemplate_.set(nullptr);
}

/* static */ void
JSCompartment::fixupCrossCompartmentWrappersAfterMovingGC(JSTracer* trc)
{
    MOZ_ASSERT(trc->runtime()->gc.isHeapCompacting());

    for (CompartmentsIter comp(trc->runtime()); !comp.done(); comp.next()) {
        // Sweep the wrapper map to update keys (wrapped values) in other
        // compartments that may have been moved.
        comp->sweepCrossCompartmentWrappers();
        // Trace the wrappers in the map to update their cross-compartment edges
        // to wrapped values in other compartments that may have been moved.
        comp->traceOutgoingCrossCompartmentWrappers(trc);
    }
}

void
Realm::fixupAfterMovingGC()
{
    purge();
    fixupGlobal();
    objectGroups_.fixupTablesAfterMovingGC();
    fixupScriptMapsAfterMovingGC();
}

void
JSCompartment::fixupAfterMovingGC()
{
    MOZ_ASSERT(zone()->isGCCompacting());

    for (RealmsInCompartmentIter r(this); !r.done(); r.next())
        r->fixupAfterMovingGC();

    // Sweep the wrapper map to update values (wrapper objects) in this
    // compartment that may have been moved.
    sweepCrossCompartmentWrappers();
}

void
Realm::fixupGlobal()
{
    GlobalObject* global = *global_.unsafeGet();
    if (global)
        global_.set(MaybeForwarded(global));
}

void
Realm::fixupScriptMapsAfterMovingGC()
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

    if (scriptNameMap) {
        for (ScriptNameMap::Enum e(*scriptNameMap); !e.empty(); e.popFront()) {
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
Realm::checkScriptMapsAfterMovingGC()
{
    if (scriptCountsMap) {
        for (auto r = scriptCountsMap->all(); !r.empty(); r.popFront()) {
            JSScript* script = r.front().key();
            MOZ_ASSERT(script->realm() == this);
            CheckGCThingAfterMovingGC(script);
            auto ptr = scriptCountsMap->lookup(script);
            MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
        }
    }

    if (scriptNameMap) {
        for (auto r = scriptNameMap->all(); !r.empty(); r.popFront()) {
            JSScript* script = r.front().key();
            MOZ_ASSERT(script->realm() == this);
            CheckGCThingAfterMovingGC(script);
            auto ptr = scriptNameMap->lookup(script);
            MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
        }
    }

    if (debugScriptMap) {
        for (auto r = debugScriptMap->all(); !r.empty(); r.popFront()) {
            JSScript* script = r.front().key();
            MOZ_ASSERT(script->realm() == this);
            CheckGCThingAfterMovingGC(script);
            DebugScript* ds = r.front().value().get();
            for (uint32_t i = 0; i < ds->numSites; i++) {
                BreakpointSite* site = ds->breakpoints[i];
                if (site && site->type() == BreakpointSite::Type::JS)
                    CheckGCThingAfterMovingGC(site->asJS()->script);
            }
            auto ptr = debugScriptMap->lookup(script);
            MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
        }
    }
}
#endif

void
Realm::purge()
{
    dtoaCache.purge();
    newProxyCache.purge();
    objectGroups_.purge();
    objects_.iteratorCache.clearAndShrink();
    arraySpeciesLookup.purge();
}

void
Realm::clearTables()
{
    global_.set(nullptr);

    // No scripts should have run in this realm. This is used when merging
    // a realm that has been used off thread into another realm and zone.
    JS::GetCompartmentForRealm(this)->assertNoCrossCompartmentWrappers();
    MOZ_ASSERT(!jitRealm_);
    MOZ_ASSERT(!debugEnvs_);
    MOZ_ASSERT(objects_.enumerators->next() == objects_.enumerators);

    objectGroups_.clearTables();
    if (savedStacks_.initialized())
        savedStacks_.clear();
    if (varNames_.initialized())
        varNames_.clear();
}

void
Realm::setAllocationMetadataBuilder(const js::AllocationMetadataBuilder* builder)
{
    // Clear any jitcode in the runtime, which behaves differently depending on
    // whether there is a creation callback.
    ReleaseAllJITCode(runtime_->defaultFreeOp());

    allocationMetadataBuilder_ = builder;
}

void
Realm::forgetAllocationMetadataBuilder()
{
    // Unlike setAllocationMetadataBuilder, we don't have to discard all JIT
    // code here (code is still valid, just a bit slower because it doesn't do
    // inline GC allocations when a metadata builder is present), but we do want
    // to cancel off-thread Ion compilations to avoid races when Ion calls
    // hasAllocationMetadataBuilder off-thread.
    CancelOffThreadIonCompile(this);

    allocationMetadataBuilder_ = nullptr;
}

void
Realm::setNewObjectMetadata(JSContext* cx, HandleObject obj)
{
    MOZ_ASSERT(obj->realm() == this);
    assertSameCompartment(cx, JS::GetCompartmentForRealm(this), obj);

    AutoEnterOOMUnsafeRegion oomUnsafe;
    if (JSObject* metadata = allocationMetadataBuilder_->build(cx, obj, oomUnsafe)) {
        MOZ_ASSERT(metadata->realm() == obj->realm());
        assertSameCompartment(cx, metadata);

        if (!objects_.objectMetadataTable) {
            auto table = cx->make_unique<ObjectWeakMap>(cx);
            if (!table || !table->init())
                oomUnsafe.crash("setNewObjectMetadata");

            objects_.objectMetadataTable = std::move(table);
        }

        if (!objects_.objectMetadataTable->add(cx, obj, metadata))
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
AddLazyFunctionsForRealm(JSContext* cx, AutoObjectVector& lazyFunctions, AllocKind kind)
{
    // Find all live root lazy functions in the realm: those which have a
    // non-lazy enclosing script, and which do not have an uncompiled enclosing
    // script. The last condition is so that we don't compile lazy scripts
    // whose enclosing scripts failed to compile, indicating that the lazy
    // script did not escape the script.
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
            fun->realm() != cx->realm())
        {
            continue;
        }

        if (fun->isInterpretedLazy()) {
            LazyScript* lazy = fun->lazyScriptOrNull();
            if (lazy && !lazy->isEnclosingScriptLazy() && !lazy->hasUncompletedEnclosingScript()) {
                if (!lazyFunctions.append(fun))
                    return false;
            }
        }
    }

    return true;
}

static bool
CreateLazyScriptsForRealm(JSContext* cx)
{
    AutoObjectVector lazyFunctions(cx);

    if (!AddLazyFunctionsForRealm(cx, lazyFunctions, AllocKind::FUNCTION))
        return false;

    // Methods, for instance {get method() {}}, are extended functions that can
    // be relazified, so we need to handle those as well.
    if (!AddLazyFunctionsForRealm(cx, lazyFunctions, AllocKind::FUNCTION_EXTENDED))
        return false;

    // Create scripts for each lazy function, updating the list of functions to
    // process with any newly exposed inner functions in created scripts.
    // A function cannot be delazified until its outer script exists.
    RootedFunction fun(cx);
    for (size_t i = 0; i < lazyFunctions.length(); i++) {
        fun = &lazyFunctions[i]->as<JSFunction>();

        // lazyFunctions may have been populated with multiple functions for
        // a lazy script.
        if (!fun->isInterpretedLazy())
            continue;

        bool lazyScriptHadNoScript = !fun->lazyScript()->maybeScript();

        JSScript* script = JSFunction::getOrCreateScript(cx, fun);
        if (!script)
            return false;
        if (lazyScriptHadNoScript && !AddInnerLazyFunctionsFromScript(script, lazyFunctions))
            return false;
    }

    return true;
}

bool
Realm::ensureDelazifyScriptsForDebugger(JSContext* cx)
{
    AutoRealmUnchecked ar(cx, this);
    if (needsDelazificationForDebugger() && !CreateLazyScriptsForRealm(cx))
        return false;
    debugModeBits_ &= ~DebuggerNeedsDelazification;
    return true;
}

void
Realm::updateDebuggerObservesFlag(unsigned flag)
{
    MOZ_ASSERT(isDebuggee());
    MOZ_ASSERT(flag == DebuggerObservesAllExecution ||
               flag == DebuggerObservesCoverage ||
               flag == DebuggerObservesAsmJS ||
               flag == DebuggerObservesBinarySource);

    GlobalObject* global = zone()->runtimeFromMainThread()->gc.isForegroundSweeping()
                           ? unsafeUnbarrieredMaybeGlobal()
                           : maybeGlobal();
    const GlobalObject::DebuggerVector* v = global->getDebuggers();
    for (auto p = v->begin(); p != v->end(); p++) {
        Debugger* dbg = *p;
        if (flag == DebuggerObservesAllExecution ? dbg->observesAllExecution() :
            flag == DebuggerObservesCoverage ? dbg->observesCoverage() :
            flag == DebuggerObservesAsmJS ? dbg->observesAsmJS() :
            dbg->observesBinarySource())
        {
            debugModeBits_ |= flag;
            return;
        }
    }

    debugModeBits_ &= ~flag;
}

void
Realm::unsetIsDebuggee()
{
    if (isDebuggee()) {
        debugModeBits_ &= ~DebuggerObservesMask;
        DebugEnvironments::onRealmUnsetIsDebuggee(this);
    }
}

void
Realm::updateDebuggerObservesCoverage()
{
    bool previousState = debuggerObservesCoverage();
    updateDebuggerObservesFlag(DebuggerObservesCoverage);
    if (previousState == debuggerObservesCoverage())
        return;

    if (debuggerObservesCoverage()) {
        // Interrupt any running interpreter frame. The scriptCounts are
        // allocated on demand when a script resumes its execution.
        JSContext* cx = TlsContext.get();
        for (ActivationIterator iter(cx); !iter.done(); ++iter) {
            if (iter->isInterpreter())
                iter->asInterpreter()->enableInterruptsUnconditionally();
        }
        return;
    }

    // If code coverage is enabled by any other means, keep it.
    if (collectCoverage())
        return;

    clearScriptCounts();
    clearScriptNames();
}

bool
Realm::collectCoverage() const
{
    return collectCoverageForPGO() ||
           collectCoverageForDebug();
}

bool
Realm::collectCoverageForPGO() const
{
    return !JitOptions.disablePgo;
}

bool
Realm::collectCoverageForDebug() const
{
    return debuggerObservesCoverage() ||
           runtimeFromAnyThread()->profilingScripts ||
           runtimeFromAnyThread()->lcovOutput().isEnabled();
}

void
Realm::clearScriptCounts()
{
    if (!scriptCountsMap)
        return;

    // Clear all hasScriptCounts_ flags of JSScript, in order to release all
    // ScriptCounts entries of the current realm.
    for (ScriptCountsMap::Range r = scriptCountsMap->all(); !r.empty(); r.popFront())
        r.front().key()->clearHasScriptCounts();

    scriptCountsMap.reset();
}

void
Realm::clearScriptNames()
{
    scriptNameMap.reset();
}

void
Realm::clearBreakpointsIn(FreeOp* fop, js::Debugger* dbg, HandleObject handler)
{
    for (auto script = zone()->cellIter<JSScript>(); !script.done(); script.next()) {
        if (script->realm() == this && script->hasAnyBreakpointsOrStepMode())
            script->clearBreakpointsIn(fop, dbg, handler);
    }
}

void
JSCompartment::addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                      size_t* crossCompartmentWrappersArg)
{
    // Note that Realm inherits from JSCompartment (for now) so sizeof(*this) is
    // included in that.

    *crossCompartmentWrappersArg += crossCompartmentWrappers.sizeOfExcludingThis(mallocSizeOf);
}

void
ObjectRealm::addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                    size_t* innerViewsArg,
                                    size_t* lazyArrayBuffersArg,
                                    size_t* objectMetadataTablesArg,
                                    size_t* nonSyntacticLexicalEnvironmentsArg)
{
    *innerViewsArg += innerViews.sizeOfExcludingThis(mallocSizeOf);

    if (lazyArrayBuffers)
        *lazyArrayBuffersArg += lazyArrayBuffers->sizeOfIncludingThis(mallocSizeOf);

    if (objectMetadataTable)
        *objectMetadataTablesArg += objectMetadataTable->sizeOfIncludingThis(mallocSizeOf);

    if (auto& map = nonSyntacticLexicalEnvironments_)
        *nonSyntacticLexicalEnvironmentsArg += map->sizeOfIncludingThis(mallocSizeOf);
}

void
Realm::addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                              size_t* tiAllocationSiteTables,
                              size_t* tiArrayTypeTables,
                              size_t* tiObjectTypeTables,
                              size_t* realmObject,
                              size_t* realmTables,
                              size_t* innerViewsArg,
                              size_t* lazyArrayBuffersArg,
                              size_t* objectMetadataTablesArg,
                              size_t* crossCompartmentWrappersArg,
                              size_t* savedStacksSet,
                              size_t* varNamesSet,
                              size_t* nonSyntacticLexicalEnvironmentsArg,
                              size_t* jitRealm,
                              size_t* privateData,
                              size_t* scriptCountsMapArg)
{
    // This is temporary until Realm and JSCompartment are completely separated.
    JSCompartment::addSizeOfExcludingThis(mallocSizeOf, crossCompartmentWrappersArg);

    *realmObject += mallocSizeOf(this);
    objectGroups_.addSizeOfExcludingThis(mallocSizeOf, tiAllocationSiteTables,
                                         tiArrayTypeTables, tiObjectTypeTables,
                                         realmTables);
    wasm.addSizeOfExcludingThis(mallocSizeOf, realmTables);

    objects_.addSizeOfExcludingThis(mallocSizeOf,
                                    innerViewsArg,
                                    lazyArrayBuffersArg,
                                    objectMetadataTablesArg,
                                    nonSyntacticLexicalEnvironmentsArg);

    *savedStacksSet += savedStacks_.sizeOfExcludingThis(mallocSizeOf);
    *varNamesSet += varNames_.sizeOfExcludingThis(mallocSizeOf);

    if (jitRealm_)
        *jitRealm += jitRealm_->sizeOfIncludingThis(mallocSizeOf);

    auto callback = runtime_->sizeOfIncludingThisCompartmentCallback;
    if (callback)
        *privateData += callback(mallocSizeOf, this);

    if (scriptCountsMap) {
        *scriptCountsMapArg += scriptCountsMap->sizeOfIncludingThis(mallocSizeOf);
        for (auto r = scriptCountsMap->all(); !r.empty(); r.popFront()) {
            *scriptCountsMapArg += r.front().value()->sizeOfIncludingThis(mallocSizeOf);
        }
    }
}

mozilla::HashCodeScrambler
Realm::randomHashCodeScrambler()
{
    return mozilla::HashCodeScrambler(randomKeyGenerator_.next(),
                                      randomKeyGenerator_.next());
}

AutoSetNewObjectMetadata::AutoSetNewObjectMetadata(JSContext* cx
                                                   MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
    : CustomAutoRooter(cx)
    , cx_(cx->helperThread() ? nullptr : cx)
    , prevState_(cx->realm()->objectMetadataState_)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (cx_)
        cx_->realm()->objectMetadataState_ = NewObjectMetadataState(DelayMetadata());
}

AutoSetNewObjectMetadata::~AutoSetNewObjectMetadata()
{
    // If we don't have a cx, we didn't change the metadata state, so no need to
    // reset it here.
    if (!cx_)
        return;

    if (!cx_->isExceptionPending() && cx_->realm()->hasObjectPendingMetadata()) {
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

        JSObject* obj = cx_->realm()->objectMetadataState_.as<PendingMetadata>();

        // Make sure to restore the previous state before setting the object's
        // metadata. SetNewObjectMetadata asserts that the state is not
        // PendingMetadata in order to ensure that metadata callbacks are called
        // in order.
        cx_->realm()->objectMetadataState_ = prevState_;

        obj = SetNewObjectMetadata(cx_, obj);
    } else {
        cx_->realm()->objectMetadataState_ = prevState_;
    }
}

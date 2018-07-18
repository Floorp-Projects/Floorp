/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Realm-inl.h"

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

ObjectRealm::ObjectRealm(JS::Zone* zone)
  : innerViews(zone)
{}

ObjectRealm::~ObjectRealm()
{
    MOZ_ASSERT(enumerators == iteratorSentinel_.get());
}

Realm::Realm(Compartment* comp, const JS::RealmOptions& options)
  : JS::shadow::Realm(comp),
    zone_(comp->zone()),
    runtime_(comp->runtimeFromMainThread()),
    creationOptions_(options.creationOptions()),
    behaviors_(options.behaviors()),
    global_(nullptr),
    objects_(zone_),
    randomKeyGenerator_(runtime_->forkRandomKeyGenerator()),
    wasm(runtime_),
    performanceMonitoring(runtime_)
{
    MOZ_ASSERT_IF(creationOptions_.mergeable(),
                  creationOptions_.invisibleToDebugger());

    runtime_->numRealms++;
}

Realm::~Realm()
{
    MOZ_ASSERT(!hasBeenEnteredIgnoringJit());

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
    if (!jitRuntime_->initialize(cx)) {
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

#endif // JSGC_HASH_TABLE_CHECKS

LexicalEnvironmentObject*
ObjectRealm::getOrCreateNonSyntacticLexicalEnvironment(JSContext* cx, HandleObject enclosing)
{
    MOZ_ASSERT(&ObjectRealm::get(enclosing) == this);

    if (!nonSyntacticLexicalEnvironments_) {
        auto map = cx->make_unique<ObjectWeakMap>(cx);
        if (!map)
            return nullptr;

        if (!map->init()) {
            ReportOutOfMemory(cx);
            return nullptr;
        }

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
Realm::traceGlobal(JSTracer* trc)
{
    // Trace things reachable from the realm's global. Note that these edges
    // must be swept too in case the realm is live but the global is not.

    savedStacks_.trace(trc);

    // Atoms are always tenured.
    if (!JS::RuntimeHeapIsMinorCollecting())
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

    if (!JS::RuntimeHeapIsMinorCollecting()) {
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
        !JS::RuntimeHeapIsMinorCollecting())
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
Realm::sweepTemplateObjects()
{
    if (mappedArgumentsTemplate_ && IsAboutToBeFinalized(&mappedArgumentsTemplate_))
        mappedArgumentsTemplate_.set(nullptr);

    if (unmappedArgumentsTemplate_ && IsAboutToBeFinalized(&unmappedArgumentsTemplate_))
        unmappedArgumentsTemplate_.set(nullptr);

    if (iterResultTemplate_ && IsAboutToBeFinalized(&iterResultTemplate_))
        iterResultTemplate_.set(nullptr);
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
    compartment()->assertNoCrossCompartmentWrappers();
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
    MOZ_ASSERT(obj->maybeCCWRealm() == this);
    assertSameCompartment(cx, compartment(), obj);

    AutoEnterOOMUnsafeRegion oomUnsafe;
    if (JSObject* metadata = allocationMetadataBuilder_->build(cx, obj, oomUnsafe)) {
        MOZ_ASSERT(metadata->maybeCCWRealm() == obj->maybeCCWRealm());
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
            if (lazy && lazy->enclosingScriptHasEverBeenCompiled()) {
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
                              size_t* savedStacksSet,
                              size_t* varNamesSet,
                              size_t* nonSyntacticLexicalEnvironmentsArg,
                              size_t* jitRealm,
                              size_t* scriptCountsMapArg)
{
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

    if (scriptCountsMap) {
        *scriptCountsMapArg += scriptCountsMap->sizeOfIncludingThis(mallocSizeOf);
        for (auto r = scriptCountsMap->all(); !r.empty(); r.popFront())
            *scriptCountsMapArg += r.front().value()->sizeOfIncludingThis(mallocSizeOf);
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

JS_PUBLIC_API(void)
gc::TraceRealm(JSTracer* trc, JS::Realm* realm, const char* name)
{
    // The way GC works with compartments is basically incomprehensible.
    // For Realms, what we want is very simple: each Realm has a strong
    // reference to its GlobalObject, and vice versa.
    //
    // Here we simply trace our side of that edge. During GC,
    // GCRuntime::traceRuntimeCommon() marks all other realm roots, for
    // all realms.
    realm->traceGlobal(trc);
}

JS_PUBLIC_API(bool)
gc::RealmNeedsSweep(JS::Realm* realm)
{
    return realm->globalIsAboutToBeFinalized();
}

JS_PUBLIC_API(JS::Realm*)
JS::GetCurrentRealmOrNull(JSContext* cx)
{
    return cx->realm();
}

JS_PUBLIC_API(JS::Realm*)
JS::GetObjectRealmOrNull(JSObject* obj)
{
    return IsCrossCompartmentWrapper(obj) ? nullptr : obj->nonCCWRealm();
}

JS_PUBLIC_API(void*)
JS::GetRealmPrivate(JS::Realm* realm)
{
    return realm->realmPrivate();
}

JS_PUBLIC_API(void)
JS::SetRealmPrivate(JS::Realm* realm, void* data)
{
    realm->setRealmPrivate(data);
}

JS_PUBLIC_API(void)
JS::SetDestroyRealmCallback(JSContext* cx, JS::DestroyRealmCallback callback)
{
    cx->runtime()->destroyRealmCallback = callback;
}

JS_PUBLIC_API(void)
JS::SetRealmNameCallback(JSContext* cx, JS::RealmNameCallback callback)
{
    cx->runtime()->realmNameCallback = callback;
}

JS_PUBLIC_API(JSObject*)
JS::GetRealmGlobalOrNull(Handle<JS::Realm*> realm)
{
    return realm->maybeGlobal();
}

JS_PUBLIC_API(bool)
JS::InitRealmStandardClasses(JSContext* cx)
{
    MOZ_ASSERT(!cx->zone()->isAtomsZone());
    AssertHeapIsIdle();
    CHECK_REQUEST(cx);
    return GlobalObject::initStandardClasses(cx, cx->global());
}

JS_PUBLIC_API(JSObject*)
JS::GetRealmObjectPrototype(JSContext* cx)
{
    CHECK_REQUEST(cx);
    return GlobalObject::getOrCreateObjectPrototype(cx, cx->global());
}

JS_PUBLIC_API(JSObject*)
JS::GetRealmFunctionPrototype(JSContext* cx)
{
    CHECK_REQUEST(cx);
    return GlobalObject::getOrCreateFunctionPrototype(cx, cx->global());
}

JS_PUBLIC_API(JSObject*)
JS::GetRealmArrayPrototype(JSContext* cx)
{
    CHECK_REQUEST(cx);
    return GlobalObject::getOrCreateArrayPrototype(cx, cx->global());
}

JS_PUBLIC_API(JSObject*)
JS::GetRealmErrorPrototype(JSContext* cx)
{
    CHECK_REQUEST(cx);
    return GlobalObject::getOrCreateCustomErrorPrototype(cx, cx->global(), JSEXN_ERR);
}

JS_PUBLIC_API(JSObject*)
JS::GetRealmIteratorPrototype(JSContext* cx)
{
    CHECK_REQUEST(cx);
    return GlobalObject::getOrCreateIteratorPrototype(cx, cx->global());
}

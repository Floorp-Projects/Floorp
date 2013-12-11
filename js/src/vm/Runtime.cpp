/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Runtime-inl.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ThreadLocal.h"

#include <locale.h>
#include <string.h>

#if defined(DEBUG) && !defined(XP_WIN)
# include <sys/mman.h>
#endif

#include "jsatom.h"
#include "jsdtoa.h"
#include "jsgc.h"
#include "jsmath.h"
#include "jsnativestack.h"
#include "jsobj.h"
#include "jsscript.h"
#include "jswatchpoint.h"
#include "jswrapper.h"

#if defined(JS_ION)
# include "assembler/assembler/MacroAssembler.h"
#endif
#include "jit/AsmJSSignalHandlers.h"
#include "jit/JitCompartment.h"
#include "jit/PcScriptCache.h"
#include "js/MemoryMetrics.h"
#include "js/SliceBudget.h"
#include "yarr/BumpPointerAllocator.h"

#include "jscntxtinlines.h"
#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;

using mozilla::Atomic;
using mozilla::DebugOnly;
using mozilla::NegativeInfinity;
using mozilla::PodZero;
using mozilla::PodArrayZero;
using mozilla::PositiveInfinity;
using mozilla::ThreadLocal;
using JS::GenericNaN;
using JS::DoubleNaNValue;

/* static */ ThreadLocal<PerThreadData*> js::TlsPerThreadData;

#ifdef JS_THREADSAFE
/* static */ Atomic<size_t> JSRuntime::liveRuntimesCount;
#else
/* static */ size_t JSRuntime::liveRuntimesCount;
#endif

const JSSecurityCallbacks js::NullSecurityCallbacks = { };

PerThreadData::PerThreadData(JSRuntime *runtime)
  : PerThreadDataFriendFields(),
    runtime_(runtime),
    ionTop(nullptr),
    ionJSContext(nullptr),
    ionStackLimit(0),
    activation_(nullptr),
    asmJSActivationStack_(nullptr),
    dtoaState(nullptr),
    suppressGC(0),
    activeCompilations(0)
{}

PerThreadData::~PerThreadData()
{
    if (dtoaState)
        js_DestroyDtoaState(dtoaState);

    if (isInList())
        removeFromThreadList();
}

bool
PerThreadData::init()
{
    dtoaState = js_NewDtoaState();
    if (!dtoaState)
        return false;

    return true;
}

void
PerThreadData::addToThreadList()
{
    // PerThreadData which are created/destroyed off the main thread do not
    // show up in the runtime's thread list.
    JS_ASSERT(CurrentThreadCanAccessRuntime(runtime_));
    runtime_->threadList.insertBack(this);
}

void
PerThreadData::removeFromThreadList()
{
    JS_ASSERT(CurrentThreadCanAccessRuntime(runtime_));
    removeFrom(runtime_->threadList);
}

JSRuntime::JSRuntime(JSUseHelperThreads useHelperThreads)
  : JS::shadow::Runtime(
#ifdef JSGC_GENERATIONAL
        &gcStoreBuffer
#endif
    ),
    mainThread(this),
    interrupt(0),
    handlingSignal(false),
    operationCallback(nullptr),
#ifdef JS_THREADSAFE
    operationCallbackLock(nullptr),
    operationCallbackOwner(nullptr),
#else
    operationCallbackLockTaken(false),
#endif
#ifdef JS_WORKER_THREADS
    workerThreadState(nullptr),
    exclusiveAccessLock(nullptr),
    exclusiveAccessOwner(nullptr),
    mainThreadHasExclusiveAccess(false),
    numExclusiveThreads(0),
#endif
    systemZone(nullptr),
    numCompartments(0),
    localeCallbacks(nullptr),
    defaultLocale(nullptr),
    defaultVersion_(JSVERSION_DEFAULT),
#ifdef JS_THREADSAFE
    ownerThread_(nullptr),
#endif
    tempLifoAlloc(TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    freeLifoAlloc(TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    execAlloc_(nullptr),
    bumpAlloc_(nullptr),
    jitRuntime_(nullptr),
    selfHostingGlobal_(nullptr),
    nativeStackBase(0),
    cxCallback(nullptr),
    destroyCompartmentCallback(nullptr),
    destroyZoneCallback(nullptr),
    sweepZoneCallback(nullptr),
    compartmentNameCallback(nullptr),
    activityCallback(nullptr),
    activityCallbackArg(nullptr),
#ifdef JS_THREADSAFE
    requestDepth(0),
# ifdef DEBUG
    checkRequestDepth(0),
# endif
#endif
    gcSystemAvailableChunkListHead(nullptr),
    gcUserAvailableChunkListHead(nullptr),
    gcBytes(0),
    gcMaxBytes(0),
    gcMaxMallocBytes(0),
    gcNumArenasFreeCommitted(0),
    gcMarker(this),
    gcVerifyPreData(nullptr),
    gcVerifyPostData(nullptr),
    gcChunkAllocationSinceLastGC(false),
    gcNextFullGCTime(0),
    gcLastGCTime(0),
    gcJitReleaseTime(0),
    gcAllocationThreshold(30 * 1024 * 1024),
    gcHighFrequencyGC(false),
    gcHighFrequencyTimeThreshold(1000),
    gcHighFrequencyLowLimitBytes(100 * 1024 * 1024),
    gcHighFrequencyHighLimitBytes(500 * 1024 * 1024),
    gcHighFrequencyHeapGrowthMax(3.0),
    gcHighFrequencyHeapGrowthMin(1.5),
    gcLowFrequencyHeapGrowth(1.5),
    gcDynamicHeapGrowth(false),
    gcDynamicMarkSlice(false),
    gcDecommitThreshold(32 * 1024 * 1024),
    gcShouldCleanUpEverything(false),
    gcGrayBitsValid(false),
    gcIsNeeded(0),
    gcStats(thisFromCtor()),
    gcNumber(0),
    gcStartNumber(0),
    gcIsFull(false),
    gcTriggerReason(JS::gcreason::NO_REASON),
    gcStrictCompartmentChecking(false),
#ifdef DEBUG
    gcDisableStrictProxyCheckingCount(0),
#endif
    gcIncrementalState(gc::NO_INCREMENTAL),
    gcLastMarkSlice(false),
    gcSweepOnBackgroundThread(false),
    gcFoundBlackGrayEdges(false),
    gcSweepingZones(nullptr),
    gcZoneGroupIndex(0),
    gcZoneGroups(nullptr),
    gcCurrentZoneGroup(nullptr),
    gcSweepPhase(0),
    gcSweepZone(nullptr),
    gcSweepKindIndex(0),
    gcAbortSweepAfterCurrentGroup(false),
    gcArenasAllocatedDuringSweep(nullptr),
#ifdef DEBUG
    gcMarkingValidator(nullptr),
#endif
    gcInterFrameGC(0),
    gcSliceBudget(SliceBudget::Unlimited),
    gcIncrementalEnabled(true),
    gcGenerationalEnabled(true),
    gcManipulatingDeadZones(false),
    gcObjectsMarkedInDeadZones(0),
    gcPoke(false),
    heapState(Idle),
#ifdef JSGC_GENERATIONAL
    gcNursery(thisFromCtor()),
    gcStoreBuffer(thisFromCtor(), gcNursery),
#endif
#ifdef JS_GC_ZEAL
    gcZeal_(0),
    gcZealFrequency(0),
    gcNextScheduled(0),
    gcDeterministicOnly(false),
    gcIncrementalLimit(0),
#endif
    gcValidate(true),
    gcFullCompartmentChecks(false),
    gcCallback(nullptr),
    gcSliceCallback(nullptr),
    gcFinalizeCallback(nullptr),
    gcMallocBytes(0),
    gcMallocGCTriggered(false),
    scriptAndCountsVector(nullptr),
    NaNValue(DoubleNaNValue()),
    negativeInfinityValue(DoubleValue(NegativeInfinity())),
    positiveInfinityValue(DoubleValue(PositiveInfinity())),
    emptyString(nullptr),
    debugMode(false),
    spsProfiler(thisFromCtor()),
    profilingScripts(false),
    alwaysPreserveCode(false),
    hadOutOfMemory(false),
    haveCreatedContext(false),
    data(nullptr),
    gcLock(nullptr),
    gcLockOwner(nullptr),
    gcHelperThread(thisFromCtor()),
    signalHandlersInstalled_(false),
    defaultFreeOp_(thisFromCtor(), false),
    debuggerMutations(0),
    securityCallbacks(const_cast<JSSecurityCallbacks *>(&NullSecurityCallbacks)),
    DOMcallbacks(nullptr),
    destroyPrincipals(nullptr),
    structuredCloneCallbacks(nullptr),
    telemetryCallback(nullptr),
    propertyRemovals(0),
#if !EXPOSE_INTL_API
    thousandsSeparator(0),
    decimalSeparator(0),
    numGrouping(0),
#endif
    heapProtected_(false),
    mathCache_(nullptr),
    activeCompilations_(0),
    keepAtoms_(0),
    trustedPrincipals_(nullptr),
    atomsCompartment_(nullptr),
    beingDestroyed_(false),
    wrapObjectCallback(TransparentObjectWrapper),
    sameCompartmentWrapObjectCallback(nullptr),
    preWrapObjectCallback(nullptr),
    preserveWrapperCallback(nullptr),
#ifdef DEBUG
    noGCOrAllocationCheck(0),
#endif
    jitHardening(false),
    jitSupportsFloatingPoint(false),
    ionPcScriptCache(nullptr),
    threadPool(this),
    defaultJSContextCallback(nullptr),
    ctypesActivityCallback(nullptr),
    parallelWarmup(0),
    ionReturnOverride_(MagicValue(JS_ARG_POISON)),
    useHelperThreads_(useHelperThreads),
#ifdef JS_THREADSAFE
    cpuCount_(GetCPUCount()),
#else
    cpuCount_(1),
#endif
    parallelIonCompilationEnabled_(true),
    parallelParsingEnabled_(true),
    isWorkerRuntime_(false)
#ifdef DEBUG
    , enteredPolicy(nullptr)
#endif
{
    MOZ_ASSERT(cpuCount_ > 0, "GetCPUCount() seems broken");

    liveRuntimesCount++;

    setGCMode(JSGC_MODE_GLOBAL);

    /* Initialize infallibly first, so we can goto bad and JS_DestroyRuntime. */
    JS_INIT_CLIST(&onNewGlobalObjectWatchers);

    PodZero(&debugHooks);
    PodZero(&atomState);
    PodArrayZero(nativeStackQuota);
    PodZero(&asmJSCacheOps);

#if JS_STACK_GROWTH_DIRECTION > 0
    nativeStackLimit = UINTPTR_MAX;
#endif
}

static bool
JitSupportsFloatingPoint()
{
#if defined(JS_ION)
    if (!JSC::MacroAssembler::supportsFloatingPoint())
        return false;

#if defined(JS_ION) && WTF_ARM_ARCH_VERSION == 6
    if (!js::jit::hasVFP())
        return false;
#endif

    return true;
#else
    return false;
#endif
}

bool
JSRuntime::init(uint32_t maxbytes)
{
#ifdef JS_THREADSAFE
    ownerThread_ = PR_GetCurrentThread();

    operationCallbackLock = PR_NewLock();
    if (!operationCallbackLock)
        return false;

    gcLock = PR_NewLock();
    if (!gcLock)
        return false;
#endif

#ifdef JS_WORKER_THREADS
    exclusiveAccessLock = PR_NewLock();
    if (!exclusiveAccessLock)
        return false;
#endif

    if (!mainThread.init())
        return false;

    js::TlsPerThreadData.set(&mainThread);
    mainThread.addToThreadList();

    if (!js_InitGC(this, maxbytes))
        return false;

    if (!gcMarker.init(gcMode()))
        return false;

    const char *size = getenv("JSGC_MARK_STACK_LIMIT");
    if (size)
        SetMarkStackLimit(this, atoi(size));

    ScopedJSDeletePtr<Zone> atomsZone(new_<Zone>(this));
    if (!atomsZone)
        return false;

    JS::CompartmentOptions options;
    ScopedJSDeletePtr<JSCompartment> atomsCompartment(new_<JSCompartment>(atomsZone.get(), options));
    if (!atomsCompartment || !atomsCompartment->init(nullptr))
        return false;

    zones.append(atomsZone.get());
    atomsZone->compartments.append(atomsCompartment.get());

    atomsCompartment->isSystem = true;
    atomsZone->isSystem = true;
    atomsZone->setGCLastBytes(8192, GC_NORMAL);

    atomsZone.forget();
    this->atomsCompartment_ = atomsCompartment.forget();

    if (!InitAtoms(this))
        return false;

    if (!InitRuntimeNumberState(this))
        return false;

    dateTimeInfo.updateTimeZoneAdjustment();

    if (!scriptDataTable_.init())
        return false;

    if (!evalCache.init())
        return false;

    nativeStackBase = GetNativeStackBase();

    jitSupportsFloatingPoint = JitSupportsFloatingPoint();

#ifdef JS_ION
    signalHandlersInstalled_ = EnsureAsmJSSignalHandlersInstalled(this);
#endif
    return true;
}

JSRuntime::~JSRuntime()
{
    JS_ASSERT(!isHeapBusy());

    /* Free source hook early, as its destructor may want to delete roots. */
    sourceHook = nullptr;

    /* Off thread compilation and parsing depend on atoms still existing. */
    for (CompartmentsIter comp(this, SkipAtoms); !comp.done(); comp.next())
        CancelOffThreadIonCompile(comp, nullptr);
    WaitForOffThreadParsingToFinish(this);

#ifdef JS_WORKER_THREADS
    if (workerThreadState)
        workerThreadState->cleanup();
#endif

    /* Poison common names before final GC. */
    FinishCommonNames(this);

    /* Clear debugging state to remove GC roots. */
    for (CompartmentsIter comp(this, SkipAtoms); !comp.done(); comp.next()) {
        comp->clearTraps(defaultFreeOp());
        if (WatchpointMap *wpmap = comp->watchpointMap)
            wpmap->clear();
    }

    /* Clear the statics table to remove GC roots. */
    staticStrings.finish();

    /*
     * Flag us as being destroyed. This allows the GC to free things like
     * interned atoms and Ion trampolines.
     */
    beingDestroyed_ = true;

    /* Allow the GC to release scripts that were being profiled. */
    profilingScripts = false;

    JS::PrepareForFullGC(this);
    GC(this, GC_NORMAL, JS::gcreason::DESTROY_RUNTIME);

    /*
     * Clear the self-hosted global and delete self-hosted classes *after*
     * GC, as finalizers for objects check for clasp->finalize during GC.
     */
    finishSelfHosting();

    mainThread.removeFromThreadList();

#ifdef JS_WORKER_THREADS
    js_delete(workerThreadState);

    JS_ASSERT(!exclusiveAccessOwner);
    if (exclusiveAccessLock)
        PR_DestroyLock(exclusiveAccessLock);

    // Avoid bogus asserts during teardown.
    JS_ASSERT(!numExclusiveThreads);
    mainThreadHasExclusiveAccess = true;
#endif

#ifdef JS_THREADSAFE
    JS_ASSERT(!operationCallbackOwner);
    if (operationCallbackLock)
        PR_DestroyLock(operationCallbackLock);
#endif

    /*
     * Even though all objects in the compartment are dead, we may have keep
     * some filenames around because of gcKeepAtoms.
     */
    FreeScriptData(this);

#ifdef DEBUG
    /* Don't hurt everyone in leaky ol' Mozilla with a fatal JS_ASSERT! */
    if (hasContexts()) {
        unsigned cxcount = 0;
        for (ContextIter acx(this); !acx.done(); acx.next()) {
            fprintf(stderr,
"JS API usage error: found live context at %p\n",
                    (void *) acx.get());
            cxcount++;
        }
        fprintf(stderr,
"JS API usage error: %u context%s left in runtime upon JS_DestroyRuntime.\n",
                cxcount, (cxcount == 1) ? "" : "s");
    }
#endif

#if !EXPOSE_INTL_API
    FinishRuntimeNumberState(this);
#endif
    FinishAtoms(this);

    js_FinishGC(this);
    atomsCompartment_ = nullptr;

#ifdef JS_THREADSAFE
    if (gcLock)
        PR_DestroyLock(gcLock);
#endif

    js_free(defaultLocale);
    js_delete(bumpAlloc_);
    js_delete(mathCache_);
#ifdef JS_ION
    js_delete(jitRuntime_);
#endif
    js_delete(execAlloc_);  /* Delete after jitRuntime_. */

    js_delete(ionPcScriptCache);

#ifdef JSGC_GENERATIONAL
    gcStoreBuffer.disable();
    gcNursery.disable();
#endif

    DebugOnly<size_t> oldCount = liveRuntimesCount--;
    JS_ASSERT(oldCount > 0);

#ifdef JS_THREADSAFE
    js::TlsPerThreadData.set(nullptr);
#endif
}

void
NewObjectCache::clearNurseryObjects(JSRuntime *rt)
{
    for (unsigned i = 0; i < mozilla::ArrayLength(entries); ++i) {
        Entry &e = entries[i];
        JSObject *obj = reinterpret_cast<JSObject *>(&e.templateObject);
        if (IsInsideNursery(rt, e.key) ||
            IsInsideNursery(rt, obj->slots) ||
            IsInsideNursery(rt, obj->elements))
        {
            PodZero(&e);
        }
    }
}

void
JSRuntime::addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf, JS::RuntimeSizes *rtSizes)
{
    // Several tables in the runtime enumerated below can be used off thread.
    AutoLockForExclusiveAccess lock(this);

    rtSizes->object += mallocSizeOf(this);

    rtSizes->atomsTable += atoms().sizeOfExcludingThis(mallocSizeOf);

    for (ContextIter acx(this); !acx.done(); acx.next())
        rtSizes->contexts += acx->sizeOfIncludingThis(mallocSizeOf);

    rtSizes->dtoa += mallocSizeOf(mainThread.dtoaState);

    rtSizes->temporary += tempLifoAlloc.sizeOfExcludingThis(mallocSizeOf);

    if (execAlloc_)
        execAlloc_->addSizeOfCode(&rtSizes->code);

#ifdef JS_ION
    {
        AutoLockForOperationCallback lock(this);
        if (jitRuntime()) {
            if (JSC::ExecutableAllocator *ionAlloc = jitRuntime()->ionAlloc(this))
                ionAlloc->addSizeOfCode(&rtSizes->code);
        }
    }
#endif

    rtSizes->regexpData += bumpAlloc_ ? bumpAlloc_->sizeOfNonHeapData() : 0;

    rtSizes->interpreterStack += interpreterStack_.sizeOfExcludingThis(mallocSizeOf);

    rtSizes->gcMarker += gcMarker.sizeOfExcludingThis(mallocSizeOf);

    rtSizes->mathCache += mathCache_ ? mathCache_->sizeOfIncludingThis(mallocSizeOf) : 0;

    rtSizes->scriptData += scriptDataTable().sizeOfExcludingThis(mallocSizeOf);
    for (ScriptDataTable::Range r = scriptDataTable().all(); !r.empty(); r.popFront())
        rtSizes->scriptData += mallocSizeOf(r.front());
}

static bool
SignalBasedTriggersDisabled()
{
  // Don't bother trying to cache the getenv lookup; this should be called
  // infrequently.
  return !!getenv("JS_DISABLE_SLOW_SCRIPT_SIGNALS");
}

void
JSRuntime::triggerOperationCallback(OperationCallbackTrigger trigger)
{
    AutoLockForOperationCallback lock(this);

    /*
     * Invalidate ionTop to trigger its over-recursion check. Note this must be
     * set before interrupt, to avoid racing with js_InvokeOperationCallback,
     * into a weird state where interrupt is stuck at 0 but ionStackLimit is
     * MAXADDR.
     */
    mainThread.setIonStackLimit(-1);

    interrupt = 1;

#ifdef JS_ION
    /*
     * asm.js and, optionally, normal Ion code use memory protection and signal
     * handlers to halt running code.
     */
    if (!SignalBasedTriggersDisabled()) {
        TriggerOperationCallbackForAsmJSCode(this);
        jit::TriggerOperationCallbackForIonCode(this, trigger);
    }
#endif
}

void
JSRuntime::setJitHardening(bool enabled)
{
    jitHardening = enabled;
    if (execAlloc_)
        execAlloc_->setRandomize(enabled);
}

JSC::ExecutableAllocator *
JSRuntime::createExecutableAllocator(JSContext *cx)
{
    JS_ASSERT(!execAlloc_);
    JS_ASSERT(cx->runtime() == this);

    JSC::AllocationBehavior randomize =
        jitHardening ? JSC::AllocationCanRandomize : JSC::AllocationDeterministic;
    execAlloc_ = js_new<JSC::ExecutableAllocator>(randomize);
    if (!execAlloc_)
        js_ReportOutOfMemory(cx);
    return execAlloc_;
}

WTF::BumpPointerAllocator *
JSRuntime::createBumpPointerAllocator(JSContext *cx)
{
    JS_ASSERT(!bumpAlloc_);
    JS_ASSERT(cx->runtime() == this);

    bumpAlloc_ = js_new<WTF::BumpPointerAllocator>();
    if (!bumpAlloc_)
        js_ReportOutOfMemory(cx);
    return bumpAlloc_;
}

MathCache *
JSRuntime::createMathCache(JSContext *cx)
{
    JS_ASSERT(!mathCache_);
    JS_ASSERT(cx->runtime() == this);

    MathCache *newMathCache = js_new<MathCache>();
    if (!newMathCache) {
        js_ReportOutOfMemory(cx);
        return nullptr;
    }

    mathCache_ = newMathCache;
    return mathCache_;
}

bool
JSRuntime::setDefaultLocale(const char *locale)
{
    if (!locale)
        return false;
    resetDefaultLocale();
    defaultLocale = JS_strdup(this, locale);
    return defaultLocale != nullptr;
}

void
JSRuntime::resetDefaultLocale()
{
    js_free(defaultLocale);
    defaultLocale = nullptr;
}

const char *
JSRuntime::getDefaultLocale()
{
    if (defaultLocale)
        return defaultLocale;

    char *locale, *lang, *p;
#ifdef HAVE_SETLOCALE
    locale = setlocale(LC_ALL, nullptr);
#else
    locale = getenv("LANG");
#endif
    // convert to a well-formed BCP 47 language tag
    if (!locale || !strcmp(locale, "C"))
        locale = const_cast<char*>("und");
    lang = JS_strdup(this, locale);
    if (!lang)
        return nullptr;
    if ((p = strchr(lang, '.')))
        *p = '\0';
    while ((p = strchr(lang, '_')))
        *p = '-';

    defaultLocale = lang;
    return defaultLocale;
}

void
JSRuntime::setGCMaxMallocBytes(size_t value)
{
    /*
     * For compatibility treat any value that exceeds PTRDIFF_T_MAX to
     * mean that value.
     */
    gcMaxMallocBytes = (ptrdiff_t(value) >= 0) ? value : size_t(-1) >> 1;
    resetGCMallocBytes();
    for (ZonesIter zone(this, WithAtoms); !zone.done(); zone.next())
        zone->setGCMaxMallocBytes(value);
}

void
JSRuntime::updateMallocCounter(size_t nbytes)
{
    updateMallocCounter(nullptr, nbytes);
}

void
JSRuntime::updateMallocCounter(JS::Zone *zone, size_t nbytes)
{
    /* We tolerate any thread races when updating gcMallocBytes. */
    gcMallocBytes -= ptrdiff_t(nbytes);
    if (JS_UNLIKELY(gcMallocBytes <= 0))
        onTooMuchMalloc();
    else if (zone)
        zone->updateMallocCounter(nbytes);
}

JS_FRIEND_API(void)
JSRuntime::onTooMuchMalloc()
{
    if (!CurrentThreadCanAccessRuntime(this))
        return;

    if (!gcMallocGCTriggered)
        gcMallocGCTriggered = TriggerGC(this, JS::gcreason::TOO_MUCH_MALLOC);
}

JS_FRIEND_API(void *)
JSRuntime::onOutOfMemory(void *p, size_t nbytes)
{
    return onOutOfMemory(p, nbytes, nullptr);
}

JS_FRIEND_API(void *)
JSRuntime::onOutOfMemory(void *p, size_t nbytes, JSContext *cx)
{
    if (isHeapBusy())
        return nullptr;

    /*
     * Retry when we are done with the background sweeping and have stopped
     * all the allocations and released the empty GC chunks.
     */
    JS::ShrinkGCBuffers(this);
    gcHelperThread.waitBackgroundSweepOrAllocEnd();
    if (!p)
        p = js_malloc(nbytes);
    else if (p == reinterpret_cast<void *>(1))
        p = js_calloc(nbytes);
    else
      p = js_realloc(p, nbytes);
    if (p)
        return p;
    if (cx)
        js_ReportOutOfMemory(cx);
    return nullptr;
}

bool
JSRuntime::activeGCInAtomsZone()
{
    Zone *zone = atomsCompartment_->zone();
    return zone->needsBarrier() || zone->isGCScheduled() || zone->wasGCStarted();
}

#if defined(DEBUG) && !defined(XP_WIN)

AutoProtectHeapForCompilation::AutoProtectHeapForCompilation(JSRuntime *rt MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : runtime(rt)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    JS_ASSERT(!runtime->heapProtected_);
    runtime->heapProtected_ = true;

    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront()) {
        Chunk *chunk = r.front();
        // Note: Don't protect the last page in the chunk, which stores
        // immutable info and needs to be accessible for runtimeFromAnyThread()
        // in AutoThreadSafeAccess.
        if (mprotect(chunk, ChunkSize - sizeof(Arena), PROT_NONE))
            MOZ_CRASH();
    }
}

AutoProtectHeapForCompilation::~AutoProtectHeapForCompilation()
{
    JS_ASSERT(runtime->heapProtected_);
    JS_ASSERT(runtime->unprotectedArenas.empty());
    runtime->heapProtected_ = false;

    for (GCChunkSet::Range r(runtime->gcChunkSet.all()); !r.empty(); r.popFront()) {
        Chunk *chunk = r.front();
        if (mprotect(chunk, ChunkSize - sizeof(Arena), PROT_READ | PROT_WRITE))
            MOZ_CRASH();
    }
}

AutoThreadSafeAccess::AutoThreadSafeAccess(const Cell *cell MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : runtime(cell->runtimeFromAnyThread()), arena(nullptr)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    if (!runtime->heapProtected_)
        return;

    ArenaHeader *base = cell->arenaHeader();
    for (size_t i = 0; i < runtime->unprotectedArenas.length(); i++) {
        if (base == runtime->unprotectedArenas[i])
            return;
    }

    arena = base;

    if (mprotect(arena, sizeof(Arena), PROT_READ))
        MOZ_CRASH();

    if (!runtime->unprotectedArenas.append(arena))
        MOZ_CRASH();
}

AutoThreadSafeAccess::~AutoThreadSafeAccess()
{
    if (!arena)
        return;

    if (mprotect(arena, sizeof(Arena), PROT_NONE))
        MOZ_CRASH();

    JS_ASSERT(arena == runtime->unprotectedArenas.back());
    runtime->unprotectedArenas.popBack();
}

#endif // DEBUG && !XP_WIN

#ifdef JS_WORKER_THREADS

void
JSRuntime::setUsedByExclusiveThread(Zone *zone)
{
    JS_ASSERT(!zone->usedByExclusiveThread);
    zone->usedByExclusiveThread = true;
    numExclusiveThreads++;
}

void
JSRuntime::clearUsedByExclusiveThread(Zone *zone)
{
    JS_ASSERT(zone->usedByExclusiveThread);
    zone->usedByExclusiveThread = false;
    numExclusiveThreads--;
}

#endif // JS_WORKER_THREADS

#ifdef JS_THREADSAFE

bool
js::CurrentThreadCanAccessRuntime(JSRuntime *rt)
{
    DebugOnly<PerThreadData *> pt = js::TlsPerThreadData.get();
    JS_ASSERT(pt && pt->associatedWith(rt));
    return rt->ownerThread_ == PR_GetCurrentThread() || InExclusiveParallelSection();
}

bool
js::CurrentThreadCanAccessZone(Zone *zone)
{
    DebugOnly<PerThreadData *> pt = js::TlsPerThreadData.get();
    JS_ASSERT(pt && pt->associatedWith(zone->runtime_));
    return !InParallelSection() || InExclusiveParallelSection();
}

#else

bool
js::CurrentThreadCanAccessRuntime(JSRuntime *rt)
{
    return true;
}

bool
js::CurrentThreadCanAccessZone(Zone *zone)
{
    return true;
}

#endif

#ifdef DEBUG

void
JSRuntime::assertCanLock(RuntimeLock which)
{
#ifdef JS_WORKER_THREADS
    // In the switch below, each case falls through to the one below it. None
    // of the runtime locks are reentrant, and when multiple locks are acquired
    // it must be done in the order below.
    switch (which) {
      case ExclusiveAccessLock:
        JS_ASSERT(exclusiveAccessOwner != PR_GetCurrentThread());
      case WorkerThreadStateLock:
        JS_ASSERT_IF(workerThreadState, !workerThreadState->isLocked());
      case OperationCallbackLock:
        JS_ASSERT(!currentThreadOwnsOperationCallbackLock());
      case GCLock:
        JS_ASSERT(gcLockOwner != PR_GetCurrentThread());
        break;
      default:
        MOZ_CRASH();
    }
#endif // JS_THREADSAFE
}

#endif // DEBUG

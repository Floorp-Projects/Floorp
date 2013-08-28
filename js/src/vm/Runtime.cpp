/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Runtime-inl.h"

#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/Util.h"

#include <locale.h>
#include <string.h>

#include "jsatom.h"
#include "jsdtoa.h"
#include "jsgc.h"
#include "jsmath.h"
#include "jsnativestack.h"
#include "jsobj.h"
#include "jsscript.h"
#include "jswrapper.h"

#include "js/MemoryMetrics.h"
#include "jit/AsmJSSignalHandlers.h"
#include "jit/IonCompartment.h"
#include "jit/PcScriptCache.h"
#include "yarr/BumpPointerAllocator.h"

#include "jscntxtinlines.h"
#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;

using mozilla::Atomic;
using mozilla::DebugOnly;
using mozilla::PodZero;
using mozilla::ThreadLocal;

/* static */ ThreadLocal<PerThreadData*> js::TlsPerThreadData;

/* static */ Atomic<size_t> JSRuntime::liveRuntimesCount;

const JSSecurityCallbacks js::NullSecurityCallbacks = { };

PerThreadData::PerThreadData(JSRuntime *runtime)
  : PerThreadDataFriendFields(),
    runtime_(runtime),
    ionTop(NULL),
    ionJSContext(NULL),
    ionStackLimit(0),
    activation_(NULL),
    asmJSActivationStack_(NULL),
    dtoaState(NULL),
    suppressGC(0),
    gcKeepAtoms(0),
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
    runtime_->assertValidThread();
    runtime_->threadList.insertBack(this);
}

void
PerThreadData::removeFromThreadList()
{
    runtime_->assertValidThread();
    removeFrom(runtime_->threadList);
}

JSRuntime::JSRuntime(JSUseHelperThreads useHelperThreads)
  : mainThread(this),
    interrupt(0),
#ifdef JS_THREADSAFE
    operationCallbackLock(NULL),
#ifdef DEBUG
    operationCallbackOwner(NULL),
#endif
    exclusiveAccessLock(NULL),
    exclusiveAccessOwner(NULL),
    mainThreadHasExclusiveAccess(false),
    numExclusiveThreads(0),
#endif
    atomsCompartment(NULL),
    systemZone(NULL),
    numCompartments(0),
    localeCallbacks(NULL),
    defaultLocale(NULL),
    defaultVersion_(JSVERSION_DEFAULT),
#ifdef JS_THREADSAFE
    ownerThread_(NULL),
#endif
    tempLifoAlloc(TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    freeLifoAlloc(TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    execAlloc_(NULL),
    bumpAlloc_(NULL),
    ionRuntime_(NULL),
    selfHostingGlobal_(NULL),
    selfHostedClasses_(NULL),
    nativeStackBase(0),
    nativeStackQuota(0),
    interpreterFrames(NULL),
    cxCallback(NULL),
    destroyCompartmentCallback(NULL),
    compartmentNameCallback(NULL),
    activityCallback(NULL),
    activityCallbackArg(NULL),
#ifdef JS_THREADSAFE
    requestDepth(0),
# ifdef DEBUG
    checkRequestDepth(0),
# endif
#endif
    gcSystemAvailableChunkListHead(NULL),
    gcUserAvailableChunkListHead(NULL),
    gcBytes(0),
    gcMaxBytes(0),
    gcMaxMallocBytes(0),
    gcNumArenasFreeCommitted(0),
    gcMarker(this),
    gcVerifyPreData(NULL),
    gcVerifyPostData(NULL),
    gcChunkAllocationSinceLastGC(false),
    gcNextFullGCTime(0),
    gcLastGCTime(0),
    gcJitReleaseTime(0),
    gcMode(JSGC_MODE_GLOBAL),
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
    gcSweepingZones(NULL),
    gcZoneGroupIndex(0),
    gcZoneGroups(NULL),
    gcCurrentZoneGroup(NULL),
    gcSweepPhase(0),
    gcSweepZone(NULL),
    gcSweepKindIndex(0),
    gcAbortSweepAfterCurrentGroup(false),
    gcArenasAllocatedDuringSweep(NULL),
#ifdef DEBUG
    gcMarkingValidator(NULL),
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
    gcCallback(NULL),
    gcSliceCallback(NULL),
    gcFinalizeCallback(NULL),
    analysisPurgeCallback(NULL),
    analysisPurgeTriggerBytes(0),
    gcMallocBytes(0),
    scriptAndCountsVector(NULL),
    NaNValue(UndefinedValue()),
    negativeInfinityValue(UndefinedValue()),
    positiveInfinityValue(UndefinedValue()),
    emptyString(NULL),
    sourceHook(NULL),
    debugMode(false),
    spsProfiler(thisFromCtor()),
    profilingScripts(false),
    alwaysPreserveCode(false),
    hadOutOfMemory(false),
    data(NULL),
    gcLock(NULL),
    gcHelperThread(thisFromCtor()),
#ifdef JS_THREADSAFE
#ifdef JS_ION
    workerThreadState(NULL),
#endif
    sourceCompressorThread(),
#endif
    defaultFreeOp_(thisFromCtor(), false),
    debuggerMutations(0),
    securityCallbacks(const_cast<JSSecurityCallbacks *>(&NullSecurityCallbacks)),
    DOMcallbacks(NULL),
    destroyPrincipals(NULL),
    structuredCloneCallbacks(NULL),
    telemetryCallback(NULL),
    propertyRemovals(0),
#if !ENABLE_INTL_API
    thousandsSeparator(0),
    decimalSeparator(0),
    numGrouping(0),
#endif
    mathCache_(NULL),
    trustedPrincipals_(NULL),
    wrapObjectCallback(TransparentObjectWrapper),
    sameCompartmentWrapObjectCallback(NULL),
    preWrapObjectCallback(NULL),
    preserveWrapperCallback(NULL),
#ifdef DEBUG
    noGCOrAllocationCheck(0),
#endif
    jitHardening(false),
    jitSupportsFloatingPoint(false),
    ionPcScriptCache(NULL),
    threadPool(this),
    ctypesActivityCallback(NULL),
    parallelWarmup(0),
    ionReturnOverride_(MagicValue(JS_ARG_POISON)),
    useHelperThreads_(useHelperThreads),
    requestedHelperThreadCount(-1)
#ifdef DEBUG
    , enteredPolicy(NULL)
#endif
{
    liveRuntimesCount++;

    /* Initialize infallibly first, so we can goto bad and JS_DestroyRuntime. */
    JS_INIT_CLIST(&onNewGlobalObjectWatchers);

    PodZero(&debugHooks);
    PodZero(&atomState);

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
    if (!js::ion::hasVFP())
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

    if (!gcMarker.init())
        return false;

    const char *size = getenv("JSGC_MARK_STACK_LIMIT");
    if (size)
        SetMarkStackLimit(this, atoi(size));

    ScopedJSDeletePtr<Zone> atomsZone(new_<Zone>(this));
    if (!atomsZone)
        return false;

    JS::CompartmentOptions options;
    ScopedJSDeletePtr<JSCompartment> atomsCompartment(new_<JSCompartment>(atomsZone.get(), options));
    if (!atomsCompartment || !atomsCompartment->init(NULL))
        return false;

    zones.append(atomsZone.get());
    atomsZone->compartments.append(atomsCompartment.get());

    atomsCompartment->isSystem = true;
    atomsZone->isSystem = true;
    atomsZone->setGCLastBytes(8192, GC_NORMAL);

    atomsZone.forget();
    this->atomsCompartment = atomsCompartment.forget();

    if (!InitAtoms(this))
        return false;

    if (!InitRuntimeNumberState(this))
        return false;

    dateTimeInfo.updateTimeZoneAdjustment();

    if (!scriptDataTable.init())
        return false;

    if (!threadPool.init())
        return false;

#ifdef JS_THREADSAFE
    if (useHelperThreads() && !sourceCompressorThread.init())
        return false;
#endif

    if (!evalCache.init())
        return false;

    nativeStackBase = GetNativeStackBase();

    jitSupportsFloatingPoint = JitSupportsFloatingPoint();
    return true;
}

JSRuntime::~JSRuntime()
{
    mainThread.removeFromThreadList();

#ifdef JS_THREADSAFE
# ifdef JS_ION
    if (workerThreadState)
        js_delete(workerThreadState);
# endif
    sourceCompressorThread.finish();

    clearOwnerThread();

    JS_ASSERT(!operationCallbackOwner);
    if (operationCallbackLock)
        PR_DestroyLock(operationCallbackLock);

    JS_ASSERT(!exclusiveAccessOwner);
    if (exclusiveAccessLock)
        PR_DestroyLock(exclusiveAccessLock);
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

#if !ENABLE_INTL_API
    FinishRuntimeNumberState(this);
#endif
    FinishAtoms(this);

    js_FinishGC(this);
#ifdef JS_THREADSAFE
    if (gcLock)
        PR_DestroyLock(gcLock);
#endif

    js_delete(bumpAlloc_);
    js_delete(mathCache_);
#ifdef JS_ION
    js_delete(ionRuntime_);
#endif
    js_delete(execAlloc_);  /* Delete after ionRuntime_. */

    if (ionPcScriptCache)
        js_delete(ionPcScriptCache);

#ifdef JSGC_GENERATIONAL
    gcStoreBuffer.disable();
    gcNursery.disable();
#endif

    DebugOnly<size_t> oldCount = liveRuntimesCount--;
    JS_ASSERT(oldCount > 0);
}

#ifdef JS_THREADSAFE
void
JSRuntime::setOwnerThread()
{
    JS_ASSERT(ownerThread_ == (void *)0xc1ea12);  /* "clear" */
    JS_ASSERT(requestDepth == 0);
    JS_ASSERT(js::TlsPerThreadData.get() == NULL);
    ownerThread_ = PR_GetCurrentThread();
    js::TlsPerThreadData.set(&mainThread);
    nativeStackBase = GetNativeStackBase();
    if (nativeStackQuota)
        JS_SetNativeStackQuota(this, nativeStackQuota);
#ifdef XP_MACOSX
    asmJSMachExceptionHandler.setCurrentThread();
#endif
}

void
JSRuntime::clearOwnerThread()
{
    assertValidThread();
    JS_ASSERT(requestDepth == 0);
    ownerThread_ = (void *)0xc1ea12;  /* "clear" */
    js::TlsPerThreadData.set(NULL);
    nativeStackBase = 0;
#if JS_STACK_GROWTH_DIRECTION > 0
    mainThread.nativeStackLimit = UINTPTR_MAX;
#else
    mainThread.nativeStackLimit = 0;
#endif
#ifdef XP_MACOSX
    asmJSMachExceptionHandler.clearCurrentThread();
#endif
}

JS_FRIEND_API(void)
JSRuntime::abortIfWrongThread() const
{
    if (ownerThread_ != PR_GetCurrentThread())
        MOZ_CRASH();
    if (!js::TlsPerThreadData.get()->associatedWith(this))
        MOZ_CRASH();
}

#ifdef DEBUG
JS_FRIEND_API(void)
JSRuntime::assertValidThread() const
{
    JS_ASSERT(ownerThread_ == PR_GetCurrentThread());
    JS_ASSERT(js::TlsPerThreadData.get()->associatedWith(this));
}
#endif  /* DEBUG */
#endif  /* JS_THREADSAFE */

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
JSRuntime::sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf, JS::RuntimeSizes *rtSizes)
{
    // Several tables in the runtime enumerated below can be used off thread.
    AutoLockForExclusiveAccess lock(this);

    rtSizes->object = mallocSizeOf(this);

    rtSizes->atomsTable = atoms.sizeOfExcludingThis(mallocSizeOf);

    rtSizes->contexts = 0;
    for (ContextIter acx(this); !acx.done(); acx.next())
        rtSizes->contexts += acx->sizeOfIncludingThis(mallocSizeOf);

    rtSizes->dtoa = mallocSizeOf(mainThread.dtoaState);

    rtSizes->temporary = tempLifoAlloc.sizeOfExcludingThis(mallocSizeOf);

    rtSizes->code = JS::CodeSizes();
    if (execAlloc_)
        execAlloc_->sizeOfCode(&rtSizes->code);

    rtSizes->regexpData = bumpAlloc_ ? bumpAlloc_->sizeOfNonHeapData() : 0;

    rtSizes->interpreterStack = interpreterStack_.sizeOfExcludingThis(mallocSizeOf);

    rtSizes->gcMarker = gcMarker.sizeOfExcludingThis(mallocSizeOf);

    rtSizes->mathCache = mathCache_ ? mathCache_->sizeOfIncludingThis(mallocSizeOf) : 0;

    rtSizes->scriptData = scriptDataTable.sizeOfExcludingThis(mallocSizeOf);
    for (ScriptDataTable::Range r = scriptDataTable.all(); !r.empty(); r.popFront())
        rtSizes->scriptData += mallocSizeOf(r.front());
}

void
JSRuntime::triggerOperationCallback()
{
    AutoLockForOperationCallback lock(this);

    /*
     * Invalidate ionTop to trigger its over-recursion check. Note this must be
     * set before interrupt, to avoid racing with js_InvokeOperationCallback,
     * into a weird state where interrupt is stuck at 0 but ionStackLimit is
     * MAXADDR.
     */
    mainThread.setIonStackLimit(-1);

    /*
     * Use JS_ATOMIC_SET in the hope that it ensures the write will become
     * immediately visible to other processors polling the flag.
     */
    JS_ATOMIC_SET(&interrupt, 1);

#ifdef JS_ION
    /* asm.js code uses a separate mechanism to halt running code. */
    TriggerOperationCallbackForAsmJSCode(this);
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
        return NULL;
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
    return defaultLocale != NULL;
}

void
JSRuntime::resetDefaultLocale()
{
    js_free(defaultLocale);
    defaultLocale = NULL;
}

const char *
JSRuntime::getDefaultLocale()
{
    if (defaultLocale)
        return defaultLocale;

    char *locale, *lang, *p;
#ifdef HAVE_SETLOCALE
    locale = setlocale(LC_ALL, NULL);
#else
    locale = getenv("LANG");
#endif
    // convert to a well-formed BCP 47 language tag
    if (!locale || !strcmp(locale, "C"))
        locale = const_cast<char*>("und");
    lang = JS_strdup(this, locale);
    if (!lang)
        return NULL;
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
    for (ZonesIter zone(this); !zone.done(); zone.next())
        zone->setGCMaxMallocBytes(value);
}

void
JSRuntime::updateMallocCounter(size_t nbytes)
{
    updateMallocCounter(NULL, nbytes);
}

void
JSRuntime::updateMallocCounter(JS::Zone *zone, size_t nbytes)
{
    /* We tolerate any thread races when updating gcMallocBytes. */
    ptrdiff_t oldCount = gcMallocBytes;
    ptrdiff_t newCount = oldCount - ptrdiff_t(nbytes);
    gcMallocBytes = newCount;
    if (JS_UNLIKELY(newCount <= 0 && oldCount > 0))
        onTooMuchMalloc();
    else if (zone)
        zone->updateMallocCounter(nbytes);
}

JS_FRIEND_API(void)
JSRuntime::onTooMuchMalloc()
{
    TriggerGC(this, JS::gcreason::TOO_MUCH_MALLOC);
}

JS_FRIEND_API(void *)
JSRuntime::onOutOfMemory(void *p, size_t nbytes)
{
    return onOutOfMemory(p, nbytes, NULL);
}

JS_FRIEND_API(void *)
JSRuntime::onOutOfMemory(void *p, size_t nbytes, JSContext *cx)
{
    if (isHeapBusy())
        return NULL;

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
    return NULL;
}


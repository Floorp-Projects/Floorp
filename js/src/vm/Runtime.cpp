/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/Unused.h"

#if defined(XP_DARWIN)
#include <mach/mach.h>
#elif defined(XP_UNIX)
#include <sys/resource.h>
#elif defined(XP_WIN)
#include <processthreadsapi.h>
#include <windows.h>
#endif // defined(XP_DARWIN) || defined(XP_UNIX) || defined(XP_WIN)

#include <locale.h>
#include <string.h>

#ifdef JS_CAN_CHECK_THREADSAFE_ACCESSES
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
#include "jswin.h"
#include "jswrapper.h"

#include "builtin/Promise.h"
#include "gc/GCInternals.h"
#include "jit/arm/Simulator-arm.h"
#include "jit/arm64/vixl/Simulator-vixl.h"
#include "jit/IonBuilder.h"
#include "jit/JitCompartment.h"
#include "jit/mips32/Simulator-mips32.h"
#include "jit/mips64/Simulator-mips64.h"
#include "jit/PcScriptCache.h"
#include "js/Date.h"
#include "js/MemoryMetrics.h"
#include "js/SliceBudget.h"
#include "vm/Debugger.h"
#include "wasm/WasmSignalHandlers.h"

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
using JS::GenericNaN;
using JS::DoubleNaNValue;

/* static */ MOZ_THREAD_LOCAL(PerThreadData*) js::TlsPerThreadData;
/* static */ Atomic<size_t> JSRuntime::liveRuntimesCount;

namespace js {
    bool gCanUseExtraThreads = true;
} // namespace js

void
js::DisableExtraThreads()
{
    gCanUseExtraThreads = false;
}

const JSSecurityCallbacks js::NullSecurityCallbacks = { };

PerThreadData::PerThreadData(JSRuntime* runtime)
  : runtime_(runtime)
#ifdef JS_TRACE_LOGGING
  , traceLogger(nullptr)
#endif
  , autoFlushICache_(nullptr)
  , dtoaState(nullptr)
  , suppressGC(0)
#ifdef DEBUG
  , ionCompiling(false)
  , ionCompilingSafeForMinorGC(false)
  , performingGC(false)
  , gcSweeping(false)
#endif
{}

PerThreadData::~PerThreadData()
{
    if (dtoaState)
        DestroyDtoaState(dtoaState);
}

bool
PerThreadData::init()
{
    dtoaState = NewDtoaState();
    if (!dtoaState)
        return false;

    return true;
}

static const JSWrapObjectCallbacks DefaultWrapObjectCallbacks = {
    TransparentObjectWrapper,
    nullptr
};

static size_t
ReturnZeroSize(const void* p)
{
    return 0;
}

JSRuntime::JSRuntime(JSRuntime* parentRuntime)
  : mainThread(this),
    jitTop(nullptr),
    jitActivation(nullptr),
    jitStackLimit_(0xbad),
    jitStackLimitNoInterrupt_(0xbad),
#ifdef DEBUG
    ionBailAfter_(0),
#endif
    activation_(nullptr),
    profilingActivation_(nullptr),
    profilerSampleBufferGen_(0),
    profilerSampleBufferLapCount_(1),
    wasmActivationStack_(nullptr),
    entryMonitor(nullptr),
    noExecuteDebuggerTop(nullptr),
    parentRuntime(parentRuntime),
#ifdef DEBUG
    updateChildRuntimeCount(parentRuntime),
#endif
    interrupt_(false),
    telemetryCallback(nullptr),
    handlingSegFault(false),
    handlingJitInterrupt_(false),
    interruptCallbackDisabled(false),
    getIncumbentGlobalCallback(nullptr),
    enqueuePromiseJobCallback(nullptr),
    enqueuePromiseJobCallbackData(nullptr),
    promiseRejectionTrackerCallback(nullptr),
    promiseRejectionTrackerCallbackData(nullptr),
    startAsyncTaskCallback(nullptr),
    finishAsyncTaskCallback(nullptr),
    promiseTasksToDestroy(mutexid::PromiseTaskPtrVector),
    exclusiveAccessLock(mutexid::RuntimeExclusiveAccess),
#ifdef DEBUG
    mainThreadHasExclusiveAccess(false),
#endif
    numExclusiveThreads(0),
    numCompartments(0),
    localeCallbacks(nullptr),
    defaultLocale(nullptr),
    defaultVersion_(JSVERSION_DEFAULT),
    ownerThread_(js::ThisThread::GetId()),
    ownerThreadNative_(0),
    tempLifoAlloc(TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    jitRuntime_(nullptr),
    selfHostingGlobal_(nullptr),
    nativeStackBase(GetNativeStackBase()),
    destroyCompartmentCallback(nullptr),
    sizeOfIncludingThisCompartmentCallback(nullptr),
    destroyZoneCallback(nullptr),
    sweepZoneCallback(nullptr),
    compartmentNameCallback(nullptr),
    activityCallback(nullptr),
    activityCallbackArg(nullptr),
    requestDepth(0),
#ifdef DEBUG
    checkRequestDepth(0),
#endif
    gc(thisFromCtor()),
    gcInitialized(false),
#ifdef JS_SIMULATOR
    simulator_(nullptr),
#endif
    scriptAndCountsVector(nullptr),
    lcovOutput(),
    NaNValue(DoubleNaNValue()),
    negativeInfinityValue(DoubleValue(NegativeInfinity<double>())),
    positiveInfinityValue(DoubleValue(PositiveInfinity<double>())),
    emptyString(nullptr),
    spsProfiler(thisFromCtor()),
    profilingScripts(false),
    suppressProfilerSampling(false),
    hadOutOfMemory(false),
#if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)
    runningOOMTest(false),
#endif
    allowRelazificationForTesting(false),
    defaultFreeOp_(nullptr),
    debuggerMutations(0),
    securityCallbacks(&NullSecurityCallbacks),
    DOMcallbacks(nullptr),
    destroyPrincipals(nullptr),
    readPrincipals(nullptr),
    warningReporter(nullptr),
    buildIdOp(nullptr),
    propertyRemovals(0),
#if !EXPOSE_INTL_API
    thousandsSeparator(0),
    decimalSeparator(0),
    numGrouping(0),
#endif
    keepAtoms_(0),
    trustedPrincipals_(nullptr),
    beingDestroyed_(false),
    atoms_(nullptr),
    atomsCompartment_(nullptr),
    staticStrings(nullptr),
    commonNames(nullptr),
    permanentAtoms(nullptr),
    wellKnownSymbols(nullptr),
    wrapObjectCallbacks(&DefaultWrapObjectCallbacks),
    preserveWrapperCallback(nullptr),
    jitSupportsFloatingPoint(false),
    jitSupportsUnalignedAccesses(false),
    jitSupportsSimd(false),
    ionPcScriptCache(nullptr),
    scriptEnvironmentPreparer(nullptr),
    ctypesActivityCallback(nullptr),
    windowProxyClass_(nullptr),
    offthreadIonCompilationEnabled_(true),
    parallelParsingEnabled_(true),
    autoWritableJitCodeActive_(false),
#ifdef DEBUG
    enteredPolicy(nullptr),
#endif
    largeAllocationFailureCallback(nullptr),
    oomCallback(nullptr),
    debuggerMallocSizeOf(ReturnZeroSize),
    lastAnimationTime(0),
    performanceMonitoring(thisFromCtor()),
    ionLazyLinkListSize_(0),
    stackFormat_(parentRuntime ? js::StackFormat::Default
                               : js::StackFormat::SpiderMonkey)
{
    setGCStoreBufferPtr(&gc.storeBuffer);

    liveRuntimesCount++;

    /* Initialize infallibly first, so we can goto bad and JS_DestroyRuntime. */
    JS_INIT_CLIST(&onNewGlobalObjectWatchers);

    PodArrayZero(nativeStackQuota);
    PodZero(&asmJSCacheOps);
    lcovOutput.init();
}

bool
JSRuntime::init(uint32_t maxbytes, uint32_t maxNurseryBytes)
{
    MOZ_ASSERT(ownerThread_ == js::ThisThread::GetId());

    // Get a platform-native handle for the owner thread, used by
    // js::InterruptRunningJitCode to halt the runtime's main thread.
#ifdef XP_WIN
    size_t openFlags = THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME |
                       THREAD_QUERY_INFORMATION;
    HANDLE self = OpenThread(openFlags, false, GetCurrentThreadId());
    if (!self)
        return false;
    static_assert(sizeof(HANDLE) <= sizeof(ownerThreadNative_), "need bigger field");
    ownerThreadNative_ = (size_t)self;
#else
    static_assert(sizeof(pthread_t) <= sizeof(ownerThreadNative_), "need bigger field");
    ownerThreadNative_ = (size_t)pthread_self();
#endif

    if (!mainThread.init())
        return false;

    if (!regexpStack.init())
        return false;

    if (CanUseExtraThreads() && !EnsureHelperThreadsInitialized())
        return false;

    js::TlsPerThreadData.set(&mainThread);

    defaultFreeOp_ = js_new<js::FreeOp>(this);
    if (!defaultFreeOp_)
        return false;

    if (!gc.init(maxbytes, maxNurseryBytes))
        return false;

    ScopedJSDeletePtr<Zone> atomsZone(new_<Zone>(this));
    if (!atomsZone || !atomsZone->init(true))
        return false;

    JS::CompartmentOptions options;
    ScopedJSDeletePtr<JSCompartment> atomsCompartment(new_<JSCompartment>(atomsZone.get(), options));
    if (!atomsCompartment || !atomsCompartment->init(nullptr))
        return false;

    if (!gc.zones.append(atomsZone.get()))
        return false;
    if (!atomsZone->compartments.append(atomsCompartment.get()))
        return false;

    atomsCompartment->setIsSystem(true);
    atomsCompartment->setIsAtomsCompartment();

    atomsZone.forget();
    this->atomsCompartment_ = atomsCompartment.forget();

    if (!symbolRegistry_.init())
        return false;

    if (!scriptDataTable_.init())
        return false;

    /* The garbage collector depends on everything before this point being initialized. */
    gcInitialized = true;

    if (!InitRuntimeNumberState(this))
        return false;

    JS::ResetTimeZone();

#ifdef JS_SIMULATOR
    simulator_ = js::jit::Simulator::Create(contextFromMainThread());
    if (!simulator_)
        return false;
#endif

    jitSupportsFloatingPoint = js::jit::JitSupportsFloatingPoint();
    jitSupportsUnalignedAccesses = js::jit::JitSupportsUnalignedAccesses();
    jitSupportsSimd = js::jit::JitSupportsSimd();

    if (!wasm::EnsureSignalHandlers(this))
        return false;

    if (!spsProfiler.init())
        return false;

    if (!fx.initInstance())
        return false;

    if (!parentRuntime) {
        sharedImmutableStrings_ = js::SharedImmutableStringsCache::Create();
        if (!sharedImmutableStrings_)
            return false;
    }

    return true;
}

void
JSRuntime::destroyRuntime()
{
    MOZ_ASSERT(!isHeapBusy());
    MOZ_ASSERT(childRuntimeCount == 0);

    fx.destroyInstance();

    sharedIntlData.destroyInstance();

    if (gcInitialized) {
        /*
         * Finish any in-progress GCs first. This ensures the parseWaitingOnGC
         * list is empty in CancelOffThreadParses.
         */
        JSContext* cx = contextFromMainThread();
        if (JS::IsIncrementalGCInProgress(cx))
            FinishGC(cx);

        /* Free source hook early, as its destructor may want to delete roots. */
        sourceHook = nullptr;

        /*
         * Cancel any pending, in progress or completed Ion compilations and
         * parse tasks. Waiting for wasm and compression tasks is done
         * synchronously (on the main thread or during parse tasks), so no
         * explicit canceling is needed for these.
         */
        CancelOffThreadIonCompile(this);
        CancelOffThreadParses(this);

        /* Remove persistent GC roots. */
        gc.finishRoots();

        /*
         * Flag us as being destroyed. This allows the GC to free things like
         * interned atoms and Ion trampolines.
         */
        beingDestroyed_ = true;

        /* Allow the GC to release scripts that were being profiled. */
        profilingScripts = false;

        /* Set the profiler sampler buffer generation to invalid. */
        profilerSampleBufferGen_ = UINT32_MAX;

        JS::PrepareForFullGC(contextFromMainThread());
        gc.gc(GC_NORMAL, JS::gcreason::DESTROY_RUNTIME);
    }

    MOZ_ASSERT(ionLazyLinkListSize_ == 0);
    MOZ_ASSERT(ionLazyLinkList_.isEmpty());

    MOZ_ASSERT(!numExclusiveThreads);
    AutoLockForExclusiveAccess lock(this);

    /*
     * Even though all objects in the compartment are dead, we may have keep
     * some filenames around because of gcKeepAtoms.
     */
    FreeScriptData(this, lock);

#if !EXPOSE_INTL_API
    FinishRuntimeNumberState(this);
#endif

    gc.finish();
    atomsCompartment_ = nullptr;

    js_delete(defaultFreeOp_);

    js_free(defaultLocale);
    js_delete(jitRuntime_);

    js_delete(ionPcScriptCache);

    gc.storeBuffer.disable();
    gc.nursery.disable();

#ifdef JS_SIMULATOR
    js::jit::Simulator::Destroy(simulator_);
#endif

    DebugOnly<size_t> oldCount = liveRuntimesCount--;
    MOZ_ASSERT(oldCount > 0);

    js::TlsPerThreadData.set(nullptr);

#ifdef XP_WIN
    if (ownerThreadNative_)
        CloseHandle((HANDLE)ownerThreadNative_);
#endif
}

void
JSRuntime::addTelemetry(int id, uint32_t sample, const char* key)
{
    if (telemetryCallback)
        (*telemetryCallback)(id, sample, key);
}

void
JSRuntime::setTelemetryCallback(JSRuntime* rt, JSAccumulateTelemetryDataCallback callback)
{
    rt->telemetryCallback = callback;
}

void
JSRuntime::addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf, JS::RuntimeSizes* rtSizes)
{
    // Several tables in the runtime enumerated below can be used off thread.
    AutoLockForExclusiveAccess lock(this);

    // For now, measure the size of the derived class (JSContext).
    // TODO (bug 1281529): make memory reporting reflect the new
    // JSContext/JSRuntime world better.
    JSContext* cx = unsafeContextFromAnyThread();
    rtSizes->object += mallocSizeOf(cx);

    rtSizes->atomsTable += atoms(lock).sizeOfIncludingThis(mallocSizeOf);

    if (!parentRuntime) {
        rtSizes->atomsTable += mallocSizeOf(staticStrings);
        rtSizes->atomsTable += mallocSizeOf(commonNames);
        rtSizes->atomsTable += permanentAtoms->sizeOfIncludingThis(mallocSizeOf);
    }

    rtSizes->contexts += cx->sizeOfExcludingThis(mallocSizeOf);

    rtSizes->temporary += tempLifoAlloc.sizeOfExcludingThis(mallocSizeOf);

    rtSizes->interpreterStack += interpreterStack_.sizeOfExcludingThis(mallocSizeOf);

    if (MathCache* cache = cx->caches.maybeGetMathCache())
        rtSizes->mathCache += cache->sizeOfIncludingThis(mallocSizeOf);

    if (sharedImmutableStrings_) {
        rtSizes->sharedImmutableStringsCache +=
            sharedImmutableStrings_->sizeOfExcludingThis(mallocSizeOf);
    }

    rtSizes->sharedIntlData += sharedIntlData.sizeOfExcludingThis(mallocSizeOf);

    rtSizes->uncompressedSourceCache +=
        cx->caches.uncompressedSourceCache.sizeOfExcludingThis(mallocSizeOf);


    rtSizes->scriptData += scriptDataTable(lock).sizeOfExcludingThis(mallocSizeOf);
    for (ScriptDataTable::Range r = scriptDataTable(lock).all(); !r.empty(); r.popFront())
        rtSizes->scriptData += mallocSizeOf(r.front());

    if (jitRuntime_) {
        jitRuntime_->execAlloc().addSizeOfCode(&rtSizes->code);
        jitRuntime_->backedgeExecAlloc().addSizeOfCode(&rtSizes->code);
    }

    rtSizes->gc.marker += gc.marker.sizeOfExcludingThis(mallocSizeOf);
    rtSizes->gc.nurseryCommitted += gc.nursery.sizeOfHeapCommitted();
    rtSizes->gc.nurseryMallocedBuffers += gc.nursery.sizeOfMallocedBuffers(mallocSizeOf);
    gc.storeBuffer.addSizeOfExcludingThis(mallocSizeOf, &rtSizes->gc);
}

static bool
InvokeInterruptCallback(JSContext* cx)
{
    MOZ_ASSERT(cx->runtime()->requestDepth >= 1);

    cx->runtime()->gc.gcIfRequested();

    // A worker thread may have requested an interrupt after finishing an Ion
    // compilation.
    jit::AttachFinishedCompilations(cx);

    // Important: Additional callbacks can occur inside the callback handler
    // if it re-enters the JS engine. The embedding must ensure that the
    // callback is disconnected before attempting such re-entry.
    if (cx->runtime()->interruptCallbackDisabled)
        return true;

    bool stop = false;
    for (JSInterruptCallback cb : cx->runtime()->interruptCallbacks) {
        if (!cb(cx))
            stop = true;
    }

    if (!stop) {
        // Debugger treats invoking the interrupt callback as a "step", so
        // invoke the onStep handler.
        if (cx->compartment()->isDebuggee()) {
            ScriptFrameIter iter(cx);
            if (!iter.done() &&
                cx->compartment() == iter.compartment() &&
                iter.script()->stepModeEnabled())
            {
                RootedValue rval(cx);
                switch (Debugger::onSingleStep(cx, &rval)) {
                  case JSTRAP_ERROR:
                    return false;
                  case JSTRAP_CONTINUE:
                    return true;
                  case JSTRAP_RETURN:
                    // See note in Debugger::propagateForcedReturn.
                    Debugger::propagateForcedReturn(cx, iter.abstractFramePtr(), rval);
                    return false;
                  case JSTRAP_THROW:
                    cx->setPendingException(rval);
                    return false;
                  default:;
                }
            }
        }

        return true;
    }

    // No need to set aside any pending exception here: ComputeStackString
    // already does that.
    JSString* stack = ComputeStackString(cx);
    JSFlatString* flat = stack ? stack->ensureFlat(cx) : nullptr;

    const char16_t* chars;
    AutoStableStringChars stableChars(cx);
    if (flat && stableChars.initTwoByte(cx, flat))
        chars = stableChars.twoByteRange().begin().get();
    else
        chars = u"(stack not available)";
    JS_ReportErrorFlagsAndNumberUC(cx, JSREPORT_WARNING, GetErrorMessage, nullptr,
                                   JSMSG_TERMINATED, chars);

    return false;
}

void
JSRuntime::requestInterrupt(InterruptMode mode)
{
    interrupt_ = true;
    jitStackLimit_ = UINTPTR_MAX;

    if (mode == JSRuntime::RequestInterruptUrgent) {
        // If this interrupt is urgent (slow script dialog and garbage
        // collection among others), take additional steps to
        // interrupt corner cases where the above fields are not
        // regularly polled.  Wake both ilooping JIT code and
        // Atomics.wait().
        fx.lock();
        if (fx.isWaiting())
            fx.wake(FutexRuntime::WakeForJSInterrupt);
        fx.unlock();
        InterruptRunningJitCode(this);
    }
}

bool
JSRuntime::handleInterrupt(JSContext* cx)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
    if (interrupt_ || jitStackLimit_ == UINTPTR_MAX) {
        interrupt_ = false;
        cx->resetJitStackLimit();
        return InvokeInterruptCallback(cx);
    }
    return true;
}

bool
JSRuntime::setDefaultLocale(const char* locale)
{
    if (!locale)
        return false;
    resetDefaultLocale();
    defaultLocale = JS_strdup(contextFromMainThread(), locale);
    return defaultLocale != nullptr;
}

void
JSRuntime::resetDefaultLocale()
{
    js_free(defaultLocale);
    defaultLocale = nullptr;
}

const char*
JSRuntime::getDefaultLocale()
{
    if (defaultLocale)
        return defaultLocale;

    const char* locale;
#ifdef HAVE_SETLOCALE
    locale = setlocale(LC_ALL, nullptr);
#else
    locale = getenv("LANG");
#endif
    // convert to a well-formed BCP 47 language tag
    if (!locale || !strcmp(locale, "C"))
        locale = "und";

    char* lang = JS_strdup(contextFromMainThread(), locale);
    if (!lang)
        return nullptr;

    char* p;
    if ((p = strchr(lang, '.')))
        *p = '\0';
    while ((p = strchr(lang, '_')))
        *p = '-';

    defaultLocale = lang;
    return defaultLocale;
}

void
JSRuntime::traceSharedIntlData(JSTracer* trc)
{
    sharedIntlData.trace(trc);
}

void
JSRuntime::triggerActivityCallback(bool active)
{
    if (!activityCallback)
        return;

    /*
     * The activity callback must not trigger a GC: it would create a cirular
     * dependency between entering a request and Rooted's requirement of being
     * in a request. In practice this callback already cannot trigger GC. The
     * suppression serves to inform the exact rooting hazard analysis of this
     * property and ensures that it remains true in the future.
     */
    AutoSuppressGC suppress(contextFromMainThread());

    activityCallback(activityCallbackArg, active);
}

FreeOp::FreeOp(JSRuntime* maybeRuntime)
  : JSFreeOp(maybeRuntime)
{
    MOZ_ASSERT_IF(maybeRuntime, CurrentThreadCanAccessRuntime(maybeRuntime));
}

FreeOp::~FreeOp()
{
    for (size_t i = 0; i < freeLaterList.length(); i++)
        free_(freeLaterList[i]);

    if (!jitPoisonRanges.empty())
        jit::ExecutableAllocator::poisonCode(runtime(), jitPoisonRanges);
}

bool
FreeOp::isDefaultFreeOp() const
{
    return runtime_ && runtime_->defaultFreeOp() == this;
}

JSObject*
JSRuntime::getIncumbentGlobal(JSContext* cx)
{
    MOZ_ASSERT(cx->runtime()->getIncumbentGlobalCallback,
               "Must set a callback using JS_SetGetIncumbentGlobalCallback before using Promises");

    return cx->runtime()->getIncumbentGlobalCallback(cx);
}

bool
JSRuntime::enqueuePromiseJob(JSContext* cx, HandleFunction job, HandleObject promise,
                             HandleObject incumbentGlobal)
{
    MOZ_ASSERT(cx->runtime()->enqueuePromiseJobCallback,
               "Must set a callback using JS_SetEnqeueuPromiseJobCallback before using Promises");
    MOZ_ASSERT_IF(incumbentGlobal, !IsWrapper(incumbentGlobal) && !IsWindowProxy(incumbentGlobal));

    void* data = cx->runtime()->enqueuePromiseJobCallbackData;
    RootedObject allocationSite(cx);
    if (promise) {
        RootedObject unwrappedPromise(cx, promise);
        // While the job object is guaranteed to be unwrapped, the promise
        // might be wrapped. See the comments in
        // intrinsic_EnqueuePromiseReactionJob for details.
        if (IsWrapper(promise))
            unwrappedPromise = UncheckedUnwrap(promise);
        if (unwrappedPromise->is<PromiseObject>())
            allocationSite = JS::GetPromiseAllocationSite(unwrappedPromise);
    }
    return cx->runtime()->enqueuePromiseJobCallback(cx, job, allocationSite, incumbentGlobal, data);
}

void
JSRuntime::addUnhandledRejectedPromise(JSContext* cx, js::HandleObject promise)
{
    MOZ_ASSERT(promise->is<PromiseObject>());
    if (!cx->runtime()->promiseRejectionTrackerCallback)
        return;

    void* data = cx->runtime()->promiseRejectionTrackerCallbackData;
    cx->runtime()->promiseRejectionTrackerCallback(cx, promise,
                                                   PromiseRejectionHandlingState::Unhandled, data);
}

void
JSRuntime::removeUnhandledRejectedPromise(JSContext* cx, js::HandleObject promise)
{
    MOZ_ASSERT(promise->is<PromiseObject>());
    if (!cx->runtime()->promiseRejectionTrackerCallback)
        return;

    void* data = cx->runtime()->promiseRejectionTrackerCallbackData;
    cx->runtime()->promiseRejectionTrackerCallback(cx, promise,
                                                   PromiseRejectionHandlingState::Handled, data);
}

void
JSRuntime::updateMallocCounter(size_t nbytes)
{
    updateMallocCounter(nullptr, nbytes);
}

void
JSRuntime::updateMallocCounter(JS::Zone* zone, size_t nbytes)
{
    gc.updateMallocCounter(zone, nbytes);
}

JS_FRIEND_API(void*)
JSRuntime::onOutOfMemory(AllocFunction allocFunc, size_t nbytes, void* reallocPtr,
                         JSContext* maybecx)
{
    MOZ_ASSERT_IF(allocFunc != AllocFunction::Realloc, !reallocPtr);
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(this));

    if (isHeapBusy())
        return nullptr;

    if (!oom::IsSimulatedOOMAllocation()) {
        /*
         * Retry when we are done with the background sweeping and have stopped
         * all the allocations and released the empty GC chunks.
         */
        gc.onOutOfMallocMemory();
        void* p;
        switch (allocFunc) {
          case AllocFunction::Malloc:
            p = js_malloc(nbytes);
            break;
          case AllocFunction::Calloc:
            p = js_calloc(nbytes);
            break;
          case AllocFunction::Realloc:
            p = js_realloc(reallocPtr, nbytes);
            break;
          default:
            MOZ_CRASH();
        }
        if (p)
            return p;
    }

    if (maybecx)
        ReportOutOfMemory(maybecx);
    return nullptr;
}

void*
JSRuntime::onOutOfMemoryCanGC(AllocFunction allocFunc, size_t bytes, void* reallocPtr)
{
    if (largeAllocationFailureCallback && bytes >= LARGE_ALLOCATION)
        largeAllocationFailureCallback(largeAllocationFailureCallbackData);
    return onOutOfMemory(allocFunc, bytes, reallocPtr);
}

bool
JSRuntime::activeGCInAtomsZone()
{
    Zone* zone = atomsCompartment_->zone();
    return (zone->needsIncrementalBarrier() && !gc.isVerifyPreBarriersEnabled()) ||
           zone->wasGCStarted();
}

void
JSRuntime::setUsedByExclusiveThread(Zone* zone)
{
    MOZ_ASSERT(!zone->usedByExclusiveThread);
    zone->usedByExclusiveThread = true;
    numExclusiveThreads++;
}

void
JSRuntime::clearUsedByExclusiveThread(Zone* zone)
{
    MOZ_ASSERT(zone->usedByExclusiveThread);
    zone->usedByExclusiveThread = false;
    numExclusiveThreads--;
    if (gc.fullGCForAtomsRequested() && !keepAtoms())
        gc.triggerFullGCForAtoms();
}

bool
js::CurrentThreadCanAccessRuntime(const JSRuntime* rt)
{
    return rt->ownerThread_ == js::ThisThread::GetId();
}

bool
js::CurrentThreadCanAccessZone(Zone* zone)
{
    if (CurrentThreadCanAccessRuntime(zone->runtime_))
        return true;

    // Only zones in use by an exclusive thread can be used off the main thread.
    // We don't keep track of which thread owns such zones though, so this check
    // is imperfect.
    return zone->usedByExclusiveThread;
}

#ifdef DEBUG
bool
js::CurrentThreadIsPerformingGC()
{
    return TlsPerThreadData.get()->performingGC;
}
#endif

JS_FRIEND_API(void)
JS::UpdateJSContextProfilerSampleBufferGen(JSContext* cx, uint32_t generation,
                                           uint32_t lapCount)
{
    cx->setProfilerSampleBufferGen(generation);
    cx->updateProfilerSampleBufferLapCount(lapCount);
}

JS_FRIEND_API(bool)
JS::IsProfilingEnabledForContext(JSContext* cx)
{
    MOZ_ASSERT(cx);
    return cx->spsProfiler.enabled();
}

JSRuntime::IonBuilderList&
JSRuntime::ionLazyLinkList()
{
    MOZ_ASSERT(TlsPerThreadData.get()->runtimeFromMainThread(),
               "Should only be mutated by the main thread.");
    return ionLazyLinkList_;
}

void
JSRuntime::ionLazyLinkListRemove(jit::IonBuilder* builder)
{
    MOZ_ASSERT(TlsPerThreadData.get()->runtimeFromMainThread(),
            "Should only be mutated by the main thread.");
    MOZ_ASSERT(ionLazyLinkListSize_ > 0);

    builder->removeFrom(ionLazyLinkList());
    ionLazyLinkListSize_--;

    MOZ_ASSERT(ionLazyLinkList().isEmpty() == (ionLazyLinkListSize_ == 0));
}

void
JSRuntime::ionLazyLinkListAdd(jit::IonBuilder* builder)
{
    MOZ_ASSERT(TlsPerThreadData.get()->runtimeFromMainThread(),
            "Should only be mutated by the main thread.");
    ionLazyLinkList().insertFront(builder);
    ionLazyLinkListSize_++;
}

JSContext*
PerThreadData::contextFromMainThread()
{
    return runtime_->contextFromMainThread();
}

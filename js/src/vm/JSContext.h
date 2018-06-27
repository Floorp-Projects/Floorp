/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS execution context. */

#ifndef vm_JSContext_h
#define vm_JSContext_h

#include "mozilla/MemoryReporting.h"

#include "js/CharacterEncoding.h"
#include "js/GCVector.h"
#include "js/Result.h"
#include "js/Utility.h"
#include "js/Vector.h"
#include "threading/ProtectedData.h"
#include "vm/ErrorReporting.h"
#include "vm/MallocProvider.h"
#include "vm/Runtime.h"

struct DtoaState;

namespace js {

class AutoAtomsZone;
class AutoRealm;

namespace jit {
class JitContext;
class DebugModeOSRVolatileJitFrameIter;
} // namespace jit

namespace gc {
class AutoCheckCanAccessAtomsDuringGC;
class AutoSuppressNurseryCellAlloc;
}

typedef HashSet<Shape*> ShapeSet;

/* Detects cycles when traversing an object graph. */
class MOZ_RAII AutoCycleDetector
{
  public:
    using Vector = GCVector<JSObject*, 8>;

    AutoCycleDetector(JSContext* cx, HandleObject objArg
                      MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), obj(cx, objArg), cyclic(true)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AutoCycleDetector();

    bool init();

    bool foundCycle() { return cyclic; }

  private:
    JSContext* cx;
    RootedObject obj;
    bool cyclic;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

struct AutoResolving;

struct HelperThread;

using JobQueue = GCVector<JSObject*, 0, SystemAllocPolicy>;

class AutoLockForExclusiveAccess;
class AutoLockScriptData;

void ReportOverRecursed(JSContext* cx, unsigned errorNumber);

/* Thread Local Storage slot for storing the context for a thread. */
extern MOZ_THREAD_LOCAL(JSContext*) TlsContext;

enum class ContextKind
{
    // Context for the main thread of a JSRuntime.
    MainThread,

    // Context for a helper thread.
    HelperThread
};

#ifdef DEBUG
bool
CurrentThreadIsParseThread();
#endif

enum class InterruptReason : uint32_t
{
    GC = 1 << 0,
    AttachIonCompilations = 1 << 1,
    CallbackUrgent = 1 << 2,
    CallbackCanWait = 1 << 3,
};

} /* namespace js */

/*
 * A JSContext encapsulates the thread local state used when using the JS
 * runtime.
 */
struct JSContext : public JS::RootingContext,
                   public js::MallocProvider<JSContext>
{
    JSContext(JSRuntime* runtime, const JS::ContextOptions& options);
    ~JSContext();

    bool init(js::ContextKind kind);

  private:
    js::UnprotectedData<JSRuntime*> runtime_;
    js::WriteOnceData<js::ContextKind> kind_;

    // The thread on which this context is running if this is not the main thread.
    js::ThreadData<js::HelperThread*> helperThread_;

    friend class js::gc::AutoSuppressNurseryCellAlloc;
    js::ThreadData<size_t> nurserySuppressions_;

    js::ThreadData<JS::ContextOptions> options_;

    js::ThreadData<js::gc::ArenaLists*> arenas_;

  public:
    // This is used by helper threads to change the runtime their context is
    // currently operating on.
    void setRuntime(JSRuntime* rt);

    bool isMainThreadContext() const { return kind_ == js::ContextKind::MainThread; }

    inline js::gc::ArenaLists* arenas() const { return arenas_; }

    template <typename T>
    bool isInsideCurrentZone(T thing) const {
        return thing->zoneFromAnyThread() == zone_;
    }

    template <typename T>
    inline bool isInsideCurrentCompartment(T thing) const {
        return thing->compartment() == compartment();
    }

    void* onOutOfMemory(js::AllocFunction allocFunc, size_t nbytes, void* reallocPtr = nullptr) {
        if (helperThread()) {
            addPendingOutOfMemory();
            return nullptr;
        }
        return runtime_->onOutOfMemory(allocFunc, nbytes, reallocPtr, this);
    }

    /* Clear the pending exception (if any) due to OOM. */
    void recoverFromOutOfMemory();

    void updateMallocCounter(size_t nbytes);

    void reportAllocationOverflow() {
        js::ReportAllocationOverflow(this);
    }

    // Accessors for immutable runtime data.
    JSAtomState& names() { return *runtime_->commonNames; }
    js::StaticStrings& staticStrings() { return *runtime_->staticStrings; }
    js::SharedImmutableStringsCache& sharedImmutableStrings() {
        return runtime_->sharedImmutableStrings();
    }
    bool isPermanentAtomsInitialized() { return !!runtime_->permanentAtoms; }
    js::FrozenAtomSet& permanentAtoms() { return *runtime_->permanentAtoms; }
    js::WellKnownSymbols& wellKnownSymbols() { return *runtime_->wellKnownSymbols; }
    JS::BuildIdOp buildIdOp() { return runtime_->buildIdOp; }
    const JS::AsmJSCacheOps& asmJSCacheOps() { return runtime_->asmJSCacheOps; }
    js::PropertyName* emptyString() { return runtime_->emptyString; }
    js::FreeOp* defaultFreeOp() { return runtime_->defaultFreeOp(); }
    void* stackLimitAddress(JS::StackKind kind) { return &nativeStackLimit[kind]; }
    void* stackLimitAddressForJitCode(JS::StackKind kind);
    uintptr_t stackLimit(JS::StackKind kind) { return nativeStackLimit[kind]; }
    uintptr_t stackLimitForJitCode(JS::StackKind kind);
    size_t gcSystemPageSize() { return js::gc::SystemPageSize(); }
    bool jitSupportsFloatingPoint() const { return runtime_->jitSupportsFloatingPoint; }
    bool jitSupportsUnalignedAccesses() const { return runtime_->jitSupportsUnalignedAccesses; }
    bool jitSupportsSimd() const { return runtime_->jitSupportsSimd; }
    bool lcovEnabled() const { return runtime_->lcovOutput().isEnabled(); }

    /*
     * "Entering" a realm changes cx->realm (which changes cx->global). Note
     * that this does not push an Activation so it's possible for the caller's
     * realm to be != cx->realm(). This is not a problem since, in general, most
     * places in the VM cannot know that they were called from script (e.g.,
     * they may have been called through the JSAPI via JS_CallFunction) and thus
     * cannot expect there is a scripted caller.
     *
     * Realms should be entered/left in a LIFO fasion. To enter a realm, code
     * should prefer using AutoRealm over JS::EnterRealm/JS::LeaveRealm.
     *
     * Also note that the JIT can enter (same-compartment) realms without going
     * through these methods - it will update cx->realm_ directly.
     */
  private:
    inline void setRealm(JS::Realm* realm);
    inline void enterRealm(JS::Realm* realm);
    inline void enterAtomsZone(const js::AutoLockForExclusiveAccess& lock);

    friend class js::AutoAtomsZone;
    friend class js::AutoRealm;

  public:
    inline void enterRealmOf(JSObject* target);
    inline void enterRealmOf(JSScript* target);
    inline void enterRealmOf(js::ObjectGroup* target);
    inline void enterNullRealm();

    inline void setRealmForJitExceptionHandler(JS::Realm* realm);

    inline void leaveRealm(JS::Realm* oldRealm);
    inline void leaveAtomsZone(JS::Realm* oldRealm,
                               const js::AutoLockForExclusiveAccess& lock);

    void setHelperThread(js::HelperThread* helperThread);
    js::HelperThread* helperThread() const { return helperThread_; }

    bool isNurseryAllocSuppressed() const {
        return nurserySuppressions_;
    }

    // Threads may freely access any data in their realm, compartment and zone.
    JS::Compartment* compartment() const {
        return realm_ ? JS::GetCompartmentForRealm(realm_) : nullptr;
    }

    JS::Realm* realm() const {
        return realm_;
    }

#ifdef DEBUG
    bool inAtomsZone() const;
#endif

    JS::Zone* zone() const {
        MOZ_ASSERT_IF(!realm() && zone_, inAtomsZone());
        MOZ_ASSERT_IF(realm(), js::GetRealmZone(realm()) == zone_);
        return zoneRaw();
    }

    // For use when the context's zone is being read by another thread and the
    // compartment and zone pointers might not be in sync.
    JS::Zone* zoneRaw() const {
        return zone_;
    }

    // For JIT use.
    static size_t offsetOfZone() {
        return offsetof(JSContext, zone_);
    }

    // Zone local methods that can be used freely.
    inline js::LifoAlloc& typeLifoAlloc();

    // Current global. This is only safe to use within the scope of the
    // AutoRealm from which it's called.
    inline js::Handle<js::GlobalObject*> global() const;

    // Methods to access runtime data that must be protected by locks.
    js::AtomSet& atoms(const js::AutoAccessAtomsZone& access) {
        return runtime_->atoms(access);
    }
    const JS::Zone* atomsZone(const js::AutoAccessAtomsZone& access) {
        return runtime_->atomsZone(access);
    }
    js::SymbolRegistry& symbolRegistry(const js::AutoAccessAtomsZone& access) {
        return runtime_->symbolRegistry(access);
    }
    js::ScriptDataTable& scriptDataTable(js::AutoLockScriptData& lock) {
        return runtime_->scriptDataTable(lock);
    }

    // Methods to access other runtime data that checks locking internally.
    js::gc::AtomMarkingRuntime& atomMarking() {
        return runtime_->gc.atomMarking;
    }
    void markAtom(JSAtom* atom) {
        atomMarking().markAtom(this, atom);
    }
    void markAtom(JS::Symbol* symbol) {
        atomMarking().markAtom(this, symbol);
    }
    void markId(jsid id) {
        atomMarking().markId(this, id);
    }
    void markAtomValue(const js::Value& value) {
        atomMarking().markAtomValue(this, value);
    }

    // Methods specific to any HelperThread for the context.
    bool addPendingCompileError(js::CompileError** err);
    void addPendingOverRecursed();
    void addPendingOutOfMemory();

    JSRuntime* runtime() { return runtime_; }
    const JSRuntime* runtime() const { return runtime_; }

    static size_t offsetOfRealm() {
        return offsetof(JSContext, realm_);
    }

    friend class JS::AutoSaveExceptionState;
    friend class js::jit::DebugModeOSRVolatileJitFrameIter;
    friend void js::ReportOverRecursed(JSContext*, unsigned errorNumber);

  private:
    static JS::Error reportedError;
    static JS::OOM reportedOOM;

  public:
    inline JS::Result<> boolToResult(bool ok);

    /**
     * Intentionally awkward signpost method that is stationed on the
     * boundary between Result-using and non-Result-using code.
     */
    template <typename V, typename E>
    bool resultToBool(const JS::Result<V, E>& result) {
        return result.isOk();
    }

    template <typename V, typename E>
    V* resultToPtr(const JS::Result<V*, E>& result) {
        return result.isOk() ? result.unwrap() : nullptr;
    }

    mozilla::GenericErrorResult<JS::OOM&> alreadyReportedOOM();
    mozilla::GenericErrorResult<JS::Error&> alreadyReportedError();

    /*
     * Points to the most recent JitActivation pushed on the thread.
     * See JitActivation constructor in vm/Stack.cpp
     */
    js::ThreadData<js::jit::JitActivation*> jitActivation;

    // Information about the heap allocated backtrack stack used by RegExp JIT code.
    js::ThreadData<js::irregexp::RegExpStack> regexpStack;

    /*
     * Points to the most recent activation running on the thread.
     * See Activation comment in vm/Stack.h.
     */
    js::ThreadData<js::Activation*> activation_;

    /*
     * Points to the most recent profiling activation running on the
     * thread.
     */
    js::Activation* volatile profilingActivation_;

  public:
    js::Activation* activation() const {
        return activation_;
    }
    static size_t offsetOfActivation() {
        return offsetof(JSContext, activation_);
    }

    js::Activation* profilingActivation() const {
        return profilingActivation_;
    }
    static size_t offsetOfProfilingActivation() {
        return offsetof(JSContext, profilingActivation_);
    }

    static size_t offsetOfJitActivation() {
        return offsetof(JSContext, jitActivation);
    }

#ifdef DEBUG
    static size_t offsetOfInUnsafeCallWithABI() {
        return offsetof(JSContext, inUnsafeCallWithABI);
    }
#endif

  private:
    /* Space for interpreter frames. */
    js::ThreadData<js::InterpreterStack> interpreterStack_;

  public:
    js::InterpreterStack& interpreterStack() {
        return interpreterStack_.ref();
    }

    /* Base address of the native stack for the current thread. */
    const uintptr_t     nativeStackBase;

    /* The native stack size limit that runtime should not exceed. */
    js::ThreadData<size_t> nativeStackQuota[JS::StackKindCount];

  public:
    /* If non-null, report JavaScript entry points to this monitor. */
    js::ThreadData<JS::dbg::AutoEntryMonitor*> entryMonitor;

    /*
     * Stack of debuggers that currently disallow debuggee execution.
     *
     * When we check for NX we are inside the debuggee compartment, and thus a
     * stack of Debuggers that have prevented execution need to be tracked to
     * enter the correct Debugger compartment to report the error.
     */
    js::ThreadData<js::EnterDebuggeeNoExecute*> noExecuteDebuggerTop;

    js::ThreadData<js::ActivityCallback> activityCallback;
    js::ThreadData<void*>                activityCallbackArg;
    void triggerActivityCallback(bool active);

    /* The request depth for this thread. */
    js::ThreadData<unsigned> requestDepth;

#ifdef DEBUG
    js::ThreadData<unsigned> checkRequestDepth;
    js::ThreadData<uint32_t> inUnsafeCallWithABI;
    js::ThreadData<bool> hasAutoUnsafeCallWithABI;
#endif

#ifdef JS_SIMULATOR
  private:
    js::ThreadData<js::jit::Simulator*> simulator_;
  public:
    js::jit::Simulator* simulator() const;
    uintptr_t* addressOfSimulatorStackLimit();
#endif

#ifdef JS_TRACE_LOGGING
    js::ThreadData<js::TraceLoggerThread*> traceLogger;
#endif

  private:
    /* Pointer to the current AutoFlushICache. */
    js::ThreadData<js::jit::AutoFlushICache*> autoFlushICache_;
  public:

    js::jit::AutoFlushICache* autoFlushICache() const;
    void setAutoFlushICache(js::jit::AutoFlushICache* afc);

    // State used by util/DoubleToString.cpp.
    js::ThreadData<DtoaState*> dtoaState;

    /*
     * When this flag is non-zero, any attempt to GC will be skipped. It is used
     * to suppress GC when reporting an OOM (see ReportOutOfMemory) and in
     * debugging facilities that cannot tolerate a GC and would rather OOM
     * immediately, such as utilities exposed to GDB. Setting this flag is
     * extremely dangerous and should only be used when in an OOM situation or
     * in non-exposed debugging facilities.
     */
    js::ThreadData<int32_t> suppressGC;

#ifdef DEBUG
    // Whether this thread is actively Ion compiling.
    js::ThreadData<bool> ionCompiling;

    // Whether this thread is actively Ion compiling in a context where a minor
    // GC could happen simultaneously. If this is true, this thread cannot use
    // any pointers into the nursery.
    js::ThreadData<bool> ionCompilingSafeForMinorGC;

    // Whether this thread is currently performing GC.  This thread could be the
    // main thread or a helper thread while the main thread is running the
    // collector.
    js::ThreadData<bool> performingGC;

    // Whether this thread is currently sweeping GC things.  This thread could
    // be the main thread or a helper thread while the main thread is running
    // the mutator.  This is used to assert that destruction of GCPtr only
    // happens when we are sweeping.
    js::ThreadData<bool> gcSweeping;

    // Whether this thread is performing work in the background for a runtime's
    // GCHelperState.
    js::ThreadData<bool> gcHelperStateThread;

    // Whether this thread is currently manipulating possibly-gray GC things.
    js::ThreadData<size_t> isTouchingGrayThings;

    js::ThreadData<size_t> noGCOrAllocationCheck;
    js::ThreadData<size_t> noNurseryAllocationCheck;

    /*
     * If this is 0, all cross-compartment proxies must be registered in the
     * wrapper map. This checking must be disabled temporarily while creating
     * new wrappers. When non-zero, this records the recursion depth of wrapper
     * creation.
     */
    js::ThreadData<uintptr_t> disableStrictProxyCheckingCount;

    bool isAllocAllowed() { return noGCOrAllocationCheck == 0; }
    void disallowAlloc() { ++noGCOrAllocationCheck; }
    void allowAlloc() {
        MOZ_ASSERT(!isAllocAllowed());
        --noGCOrAllocationCheck;
    }

    bool isNurseryAllocAllowed() { return noNurseryAllocationCheck == 0; }
    void disallowNurseryAlloc() { ++noNurseryAllocationCheck; }
    void allowNurseryAlloc() {
        MOZ_ASSERT(!isNurseryAllocAllowed());
        --noNurseryAllocationCheck;
    }

    bool isStrictProxyCheckingEnabled() { return disableStrictProxyCheckingCount == 0; }
    void disableStrictProxyChecking() { ++disableStrictProxyCheckingCount; }
    void enableStrictProxyChecking() {
        MOZ_ASSERT(disableStrictProxyCheckingCount > 0);
        --disableStrictProxyCheckingCount;
    }
#endif

#if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)
    // We are currently running a simulated OOM test.
    js::ThreadData<bool> runningOOMTest;
#endif

    // True if we should assert that
    //     !comp->validAccessPtr || *comp->validAccessPtr
    // is true for every |comp| that we run JS code in.
    js::ThreadData<unsigned> enableAccessValidation;

    /*
     * Some regions of code are hard for the static rooting hazard analysis to
     * understand. In those cases, we trade the static analysis for a dynamic
     * analysis. When this is non-zero, we should assert if we trigger, or
     * might trigger, a GC.
     */
    js::ThreadData<int> inUnsafeRegion;

    // Count of AutoDisableGenerationalGC instances on the thread's stack.
    js::ThreadData<unsigned> generationalDisabled;

    // Some code cannot tolerate compacting GC so it can be disabled temporarily
    // with AutoDisableCompactingGC which uses this counter.
    js::ThreadData<unsigned> compactingDisabledCount;

    bool canCollectAtoms() const {
        // TODO: We may be able to improve this by collecting if
        // !isOffThreadParseRunning() (bug 1468422).
        return !runtime()->hasHelperThreadZones();
    }

  private:
    // Pools used for recycling name maps and vectors when parsing and
    // emitting bytecode. Purged on GC when there are no active script
    // compilations.
    js::ThreadData<js::frontend::NameCollectionPool> frontendCollectionPool_;
  public:

    js::frontend::NameCollectionPool& frontendCollectionPool() {
        return frontendCollectionPool_.ref();
    }

    void verifyIsSafeToGC() {
        MOZ_DIAGNOSTIC_ASSERT(!inUnsafeRegion,
                              "[AutoAssertNoGC] possible GC in GC-unsafe region");
    }

    /* Whether sampling should be enabled or not. */
  private:
    mozilla::Atomic<bool, mozilla::SequentiallyConsistent> suppressProfilerSampling;

  public:
    bool isProfilerSamplingEnabled() const {
        return !suppressProfilerSampling;
    }
    void disableProfilerSampling() {
        suppressProfilerSampling = true;
    }
    void enableProfilerSampling() {
        suppressProfilerSampling = false;
    }

#if defined(XP_DARWIN)
    js::wasm::MachExceptionHandler wasmMachExceptionHandler;
#endif

    /* Temporary arena pool used while compiling and decompiling. */
    static const size_t TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 4 * 1024;
  private:
    js::ThreadData<js::LifoAlloc> tempLifoAlloc_;
  public:
    js::LifoAlloc& tempLifoAlloc() { return tempLifoAlloc_.ref(); }
    const js::LifoAlloc& tempLifoAlloc() const { return tempLifoAlloc_.ref(); }

    js::ThreadData<uint32_t> debuggerMutations;

    // Cache for jit::GetPcScript().
    js::ThreadData<js::jit::PcScriptCache*> ionPcScriptCache;

  private:
    /* Exception state -- the exception member is a GC root by definition. */
    js::ThreadData<bool> throwing;            /* is there a pending exception? */
    js::ThreadData<JS::PersistentRooted<JS::Value>> unwrappedException_; /* most-recently-thrown exception */

    JS::Value& unwrappedException() {
        if (!unwrappedException_.ref().initialized())
            unwrappedException_.ref().init(this);
        return unwrappedException_.ref().get();
    }

    // True if the exception currently being thrown is by result of
    // ReportOverRecursed. See Debugger::slowPathOnExceptionUnwind.
    js::ThreadData<bool> overRecursed_;

    // True if propagating a forced return from an interrupt handler during
    // debug mode.
    js::ThreadData<bool> propagatingForcedReturn_;

    // A stack of live iterators that need to be updated in case of debug mode
    // OSR.
    js::ThreadData<js::jit::DebugModeOSRVolatileJitFrameIter*>
        liveVolatileJitFrameIter_;

  public:
    js::ThreadData<int32_t> reportGranularity;  /* see vm/Probes.h */

    js::ThreadData<js::AutoResolving*> resolvingList;

#ifdef DEBUG
    js::ThreadData<js::AutoEnterPolicy*> enteredPolicy;
#endif

    /* True if generating an error, to prevent runaway recursion. */
    js::ThreadData<bool> generatingError;

  private:
    /* State for object and array toSource conversion. */
    js::ThreadData<js::AutoCycleDetector::Vector> cycleDetectorVector_;

  public:
    js::AutoCycleDetector::Vector& cycleDetectorVector() {
        return cycleDetectorVector_.ref();
    }
    const js::AutoCycleDetector::Vector& cycleDetectorVector() const {
        return cycleDetectorVector_.ref();
    }

    /* Client opaque pointer. */
    js::UnprotectedData<void*> data;

    void initJitStackLimit();
    void resetJitStackLimit();

  public:
    JS::ContextOptions& options() {
        return options_.ref();
    }

    bool runtimeMatches(JSRuntime* rt) const {
        return runtime_ == rt;
    }

    // Number of JS_BeginRequest calls without the corresponding JS_EndRequest.
    js::ThreadData<unsigned> outstandingRequests;

    js::ThreadData<bool> jitIsBroken;

    void updateJITEnabled();

  private:
    /*
     * Youngest frame of a saved stack that will be picked up as an async stack
     * by any new Activation, and is nullptr when no async stack should be used.
     *
     * The JS::AutoSetAsyncStackForNewCalls class can be used to set this.
     *
     * New activations will reset this to nullptr on construction after getting
     * the current value, and will restore the previous value on destruction.
     */
    js::ThreadData<JS::PersistentRooted<js::SavedFrame*>> asyncStackForNewActivations_;
  public:

    js::SavedFrame*& asyncStackForNewActivations() {
        if (!asyncStackForNewActivations_.ref().initialized())
            asyncStackForNewActivations_.ref().init(this);
        return asyncStackForNewActivations_.ref().get();
    }

    /*
     * Value of asyncCause to be attached to asyncStackForNewActivations.
     */
    js::ThreadData<const char*> asyncCauseForNewActivations;

    /*
     * True if the async call was explicitly requested, e.g. via
     * callFunctionWithAsyncStack.
     */
    js::ThreadData<bool> asyncCallIsExplicit;

    bool currentlyRunningInInterpreter() const {
        return activation()->isInterpreter();
    }
    bool currentlyRunningInJit() const {
        return activation()->isJit();
    }
    js::InterpreterFrame* interpreterFrame() const {
        return activation()->asInterpreter()->current();
    }
    js::InterpreterRegs& interpreterRegs() const {
        return activation()->asInterpreter()->regs();
    }

    /*
     * Get the topmost script and optional pc on the stack. By default, this
     * function only returns a JSScript in the current realm, returning nullptr
     * if the current script is in a different realm. This behavior can be
     * overridden by passing AllowCrossRealm::Allow.
     */
    enum class AllowCrossRealm {
        DontAllow = false,
        Allow = true
    };
    inline JSScript*
    currentScript(jsbytecode** pc = nullptr,
                  AllowCrossRealm allowCrossRealm = AllowCrossRealm::DontAllow) const;

    inline js::Nursery& nursery();
    inline void minorGC(JS::gcreason::Reason reason);

  public:
    bool isExceptionPending() const {
        return throwing;
    }

    MOZ_MUST_USE
    bool getPendingException(JS::MutableHandleValue rval);

    bool isThrowingOutOfMemory();
    bool isThrowingDebuggeeWouldRun();
    bool isClosingGenerator();

    void setPendingException(const js::Value& v);

    void clearPendingException() {
        throwing = false;
        overRecursed_ = false;
        unwrappedException().setUndefined();
    }

    bool isThrowingOverRecursed() const { return throwing && overRecursed_; }
    bool isPropagatingForcedReturn() const { return propagatingForcedReturn_; }
    void setPropagatingForcedReturn() { propagatingForcedReturn_ = true; }
    void clearPropagatingForcedReturn() { propagatingForcedReturn_ = false; }

    /*
     * See JS_SetTrustedPrincipals in jsapi.h.
     * Note: !cx->compartment is treated as trusted.
     */
    inline bool runningWithTrustedPrincipals();

    JS_FRIEND_API(size_t) sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    void trace(JSTracer* trc);

    inline js::RuntimeCaches& caches();

  private:
    /*
     * The allocation code calls the function to indicate either OOM failure
     * when p is null or that a memory pressure counter has reached some
     * threshold when p is not null. The function takes the pointer and not
     * a boolean flag to minimize the amount of code in its inlined callers.
     */
    JS_FRIEND_API(void) checkMallocGCPressure(void* p);

  public:
    using InterruptCallbackVector = js::Vector<JSInterruptCallback, 2, js::SystemAllocPolicy>;

  private:
    js::ThreadData<InterruptCallbackVector> interruptCallbacks_;
  public:
    InterruptCallbackVector& interruptCallbacks() { return interruptCallbacks_.ref(); }

    js::ThreadData<bool> interruptCallbackDisabled;

    // Bitfield storing InterruptReason values.
    mozilla::Atomic<uint32_t, mozilla::Relaxed> interruptBits_;

    // Any thread can call requestInterrupt() to request that this thread
    // stop running. To stop this thread, requestInterrupt sets two fields:
    // interruptBits_ (a bitset of InterruptReasons) and jitStackLimit_ (set to
    // UINTPTR_MAX). The JS engine must continually poll one of these fields
    // and call handleInterrupt if either field has the interrupt value.
    //
    // The point of setting jitStackLimit_ to UINTPTR_MAX is that JIT code
    // already needs to guard on jitStackLimit_ in every function prologue to
    // avoid stack overflow, so we avoid a second branch on interruptBits_ by
    // setting jitStackLimit_ to a value that is guaranteed to fail the guard.)
    //
    // Note that the writes to interruptBits_ and jitStackLimit_ use a Relaxed
    // Atomic so, while the writes are guaranteed to eventually be visible to
    // this thread, it can happen in any order. handleInterrupt calls the
    // interrupt callback if either is set, so it really doesn't matter as long
    // as the JS engine is continually polling at least one field. In corner
    // cases, this relaxed ordering could lead to an interrupt handler being
    // called twice in succession after a single requestInterrupt call, but
    // that's fine.
    void requestInterrupt(js::InterruptReason reason);
    bool handleInterrupt();

    MOZ_ALWAYS_INLINE bool hasAnyPendingInterrupt() const {
        static_assert(sizeof(interruptBits_) == sizeof(uint32_t), "Assumed by JIT callers");
        return interruptBits_ != 0;
    }
    bool hasPendingInterrupt(js::InterruptReason reason) const {
        return interruptBits_ & uint32_t(reason);
    }

  public:
    void* addressOfInterruptBits() {
        return &interruptBits_;
    }
    void* addressOfJitStackLimit() {
        return &jitStackLimit;
    }
    void* addressOfJitStackLimitNoInterrupt() {
        return &jitStackLimitNoInterrupt;
    }

    /* Futex state, used by Atomics.wait() and Atomics.wake() on the Atomics object */
    js::FutexThread fx;

    // Buffer for OSR from baseline to Ion. To avoid holding on to this for
    // too long, it's also freed in EnterBaseline (after returning from JIT code).
    js::ThreadData<uint8_t*> osrTempData_;

    uint8_t* allocateOsrTempData(size_t size);
    void freeOsrTempData();

    // In certain cases, we want to optimize certain opcodes to typed instructions,
    // to avoid carrying an extra register to feed into an unbox. Unfortunately,
    // that's not always possible. For example, a GetPropertyCacheT could return a
    // typed double, but if it takes its out-of-line path, it could return an
    // object, and trigger invalidation. The invalidation bailout will consider the
    // return value to be a double, and create a garbage Value.
    //
    // To allow the GetPropertyCacheT optimization, we allow the ability for
    // GetPropertyCache to override the return value at the top of the stack - the
    // value that will be temporarily corrupt. This special override value is set
    // only in callVM() targets that are about to return *and* have invalidated
    // their callee.
    js::ThreadData<js::Value> ionReturnOverride_;

    bool hasIonReturnOverride() const {
        return !ionReturnOverride_.ref().isMagic(JS_ARG_POISON);
    }
    js::Value takeIonReturnOverride() {
        js::Value v = ionReturnOverride_;
        ionReturnOverride_ = js::MagicValue(JS_ARG_POISON);
        return v;
    }
    void setIonReturnOverride(const js::Value& v) {
        MOZ_ASSERT(!hasIonReturnOverride());
        MOZ_ASSERT(!v.isMagic());
        ionReturnOverride_ = v;
    }

    mozilla::Atomic<uintptr_t, mozilla::Relaxed> jitStackLimit;

    // Like jitStackLimit, but not reset to trigger interrupts.
    js::ThreadData<uintptr_t> jitStackLimitNoInterrupt;

    // Promise callbacks.
    js::ThreadData<JSGetIncumbentGlobalCallback> getIncumbentGlobalCallback;
    js::ThreadData<JSEnqueuePromiseJobCallback> enqueuePromiseJobCallback;
    js::ThreadData<void*> enqueuePromiseJobCallbackData;

    // Queue of pending jobs as described in ES2016 section 8.4.
    // Only used if internal job queue handling was activated using
    // `js::UseInternalJobQueues`.
    js::ThreadData<JS::PersistentRooted<js::JobQueue>*> jobQueue;
    js::ThreadData<bool> drainingJobQueue;
    js::ThreadData<bool> stopDrainingJobQueue;

    js::ThreadData<JSPromiseRejectionTrackerCallback> promiseRejectionTrackerCallback;
    js::ThreadData<void*> promiseRejectionTrackerCallbackData;

    JSObject* getIncumbentGlobal(JSContext* cx);
    bool enqueuePromiseJob(JSContext* cx, js::HandleFunction job, js::HandleObject promise,
                           js::HandleObject incumbentGlobal);
    void addUnhandledRejectedPromise(JSContext* cx, js::HandleObject promise);
    void removeUnhandledRejectedPromise(JSContext* cx, js::HandleObject promise);
}; /* struct JSContext */

inline JS::Result<>
JSContext::boolToResult(bool ok)
{
    if (MOZ_LIKELY(ok)) {
        MOZ_ASSERT(!isExceptionPending());
        MOZ_ASSERT(!isPropagatingForcedReturn());
        return JS::Ok();
    }
    return JS::Result<>(reportedError);
}

inline JSContext*
JSRuntime::mainContextFromOwnThread()
{
    MOZ_ASSERT(mainContextFromAnyThread() == js::TlsContext.get());
    return mainContextFromAnyThread();
}

namespace js {

struct MOZ_RAII AutoResolving {
  public:
    enum Kind {
        LOOKUP,
        WATCH
    };

    AutoResolving(JSContext* cx, HandleObject obj, HandleId id, Kind kind = LOOKUP
                  MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : context(cx), object(obj), id(id), kind(kind), link(cx->resolvingList)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        MOZ_ASSERT(obj);
        cx->resolvingList = this;
    }

    ~AutoResolving() {
        MOZ_ASSERT(context->resolvingList == this);
        context->resolvingList = link;
    }

    bool alreadyStarted() const {
        return link && alreadyStartedSlow();
    }

  private:
    bool alreadyStartedSlow() const;

    JSContext*          const context;
    HandleObject        object;
    HandleId            id;
    Kind                const kind;
    AutoResolving*      const link;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/*
 * Create and destroy functions for JSContext, which is manually allocated
 * and exclusively owned.
 */
extern JSContext*
NewContext(uint32_t maxBytes, uint32_t maxNurseryBytes, JSRuntime* parentRuntime);

extern void
DestroyContext(JSContext* cx);

enum ErrorArgumentsType {
    ArgumentsAreUnicode,
    ArgumentsAreASCII,
    ArgumentsAreLatin1,
    ArgumentsAreUTF8
};

/**
 * Report an exception, using printf-style APIs to generate the error
 * message.
 */
#ifdef va_start
extern bool
ReportErrorVA(JSContext* cx, unsigned flags, const char* format,
              ErrorArgumentsType argumentsType, va_list ap) MOZ_FORMAT_PRINTF(3, 0);

extern bool
ReportErrorNumberVA(JSContext* cx, unsigned flags, JSErrorCallback callback,
                    void* userRef, const unsigned errorNumber,
                    ErrorArgumentsType argumentsType, va_list ap);

extern bool
ReportErrorNumberUCArray(JSContext* cx, unsigned flags, JSErrorCallback callback,
                         void* userRef, const unsigned errorNumber,
                         const char16_t** args);
#endif

extern bool
ExpandErrorArgumentsVA(JSContext* cx, JSErrorCallback callback,
                       void* userRef, const unsigned errorNumber,
                       const char16_t** messageArgs,
                       ErrorArgumentsType argumentsType,
                       JSErrorReport* reportp, va_list ap);

extern bool
ExpandErrorArgumentsVA(JSContext* cx, JSErrorCallback callback,
                       void* userRef, const unsigned errorNumber,
                       const char16_t** messageArgs,
                       ErrorArgumentsType argumentsType,
                       JSErrorNotes::Note* notep, va_list ap);

/* |callee| requires a usage string provided by JS_DefineFunctionsWithHelp. */
extern void
ReportUsageErrorASCII(JSContext* cx, HandleObject callee, const char* msg);

/*
 * Prints a full report and returns true if the given report is non-nullptr
 * and the report doesn't have the JSREPORT_WARNING flag set or reportWarnings
 * is true.
 * Returns false otherwise.
 */
extern bool
PrintError(JSContext* cx, FILE* file, JS::ConstUTF8CharsZ toStringResult,
           JSErrorReport* report, bool reportWarnings);

extern void
ReportIsNotDefined(JSContext* cx, HandlePropertyName name);

extern void
ReportIsNotDefined(JSContext* cx, HandleId id);

/*
 * Report an attempt to access the property of a null or undefined value (v).
 */
extern void
ReportIsNullOrUndefined(JSContext* cx, int spindex, HandleValue v);

extern void
ReportMissingArg(JSContext* cx, js::HandleValue v, unsigned arg);

/*
 * Report error using js_DecompileValueGenerator(cx, spindex, v, fallback) as
 * the first argument for the error message. If the error message has less
 * then 3 arguments, use null for arg1 or arg2.
 */
extern bool
ReportValueErrorFlags(JSContext* cx, unsigned flags, const unsigned errorNumber,
                      int spindex, HandleValue v, HandleString fallback,
                      const char* arg1, const char* arg2);

inline void
ReportValueError(JSContext* cx, const unsigned errorNumber, int spindex, HandleValue v,
                 HandleString fallback, const char* arg1 = nullptr, const char* arg2 = nullptr)
{
    ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber, spindex, v, fallback, arg1, arg2);
}

JSObject*
CreateErrorNotesArray(JSContext* cx, JSErrorReport* report);

} /* namespace js */

extern const JSErrorFormatString js_ErrorFormatString[JSErr_Limit];

namespace js {

/************************************************************************/

/* AutoArrayRooter roots an external array of Values. */
class MOZ_RAII AutoArrayRooter : private JS::AutoGCRooter
{
  public:
    AutoArrayRooter(JSContext* cx, size_t len, Value* vec
                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : JS::AutoGCRooter(cx, JS::AutoGCRooter::Tag::Array), array_(vec), length_(len)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    Value* begin() {
        return array_;
    }

    size_t length() {
        return length_;
    }

    friend void JS::AutoGCRooter::trace(JSTracer* trc);

  private:
    Value* array_;
    size_t length_;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoAssertNoException
{
#ifdef DEBUG
    JSContext* cx;
    bool hadException;
#endif

  public:
    explicit AutoAssertNoException(JSContext* cx)
#ifdef DEBUG
      : cx(cx),
        hadException(cx->isExceptionPending())
#endif
    {
    }

    ~AutoAssertNoException()
    {
        MOZ_ASSERT_IF(!hadException, !cx->isExceptionPending());
    }
};

class MOZ_RAII AutoLockForExclusiveAccess
{
    JSRuntime* runtime;

    void init(JSRuntime* rt) {
        MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt) || CurrentThreadIsParseThread());
        runtime = rt;
        if (runtime->hasHelperThreadZones()) {
            runtime->exclusiveAccessLock.lock();
        } else {
            MOZ_ASSERT(!runtime->activeThreadHasExclusiveAccess);
#ifdef DEBUG
            runtime->activeThreadHasExclusiveAccess = true;
#endif
        }
    }

  public:
    explicit AutoLockForExclusiveAccess(JSContext* cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        init(cx->runtime());
    }
    explicit AutoLockForExclusiveAccess(JSRuntime* rt MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        init(rt);
    }
    ~AutoLockForExclusiveAccess() {
        if (runtime->hasHelperThreadZones()) {
            runtime->exclusiveAccessLock.unlock();
        } else {
            MOZ_ASSERT(runtime->activeThreadHasExclusiveAccess);
#ifdef DEBUG
            runtime->activeThreadHasExclusiveAccess = false;
#endif
        }
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_RAII AutoLockScriptData
{
    JSRuntime* runtime;

  public:
    explicit AutoLockScriptData(JSRuntime* rt MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt) || CurrentThreadIsParseThread());
        runtime = rt;
        if (runtime->hasHelperThreadZones()) {
            runtime->scriptDataLock.lock();
        } else {
            MOZ_ASSERT(!runtime->activeThreadHasScriptDataAccess);
#ifdef DEBUG
            runtime->activeThreadHasScriptDataAccess = true;
#endif
        }
    }
    ~AutoLockScriptData() {
        if (runtime->hasHelperThreadZones()) {
            runtime->scriptDataLock.unlock();
        } else {
            MOZ_ASSERT(runtime->activeThreadHasScriptDataAccess);
#ifdef DEBUG
            runtime->activeThreadHasScriptDataAccess = false;
#endif
        }
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

// A token used to prove you can safely access the atoms zone. This zone is
// accessed by the main thread and by off-thread parsing. There are two
// situations in which it is safe:
//
//  - the current thread holds the exclusive access lock (off-thread parsing may
//    be running and this must also take the lock for access)
//
//  - the GC is running and is collecting the atoms zone (this cannot be started
//    while off-thread parsing is happening)
class MOZ_STACK_CLASS AutoAccessAtomsZone
{
  public:
    MOZ_IMPLICIT AutoAccessAtomsZone(const AutoLockForExclusiveAccess& lock) {}
    MOZ_IMPLICIT AutoAccessAtomsZone(const gc::AutoCheckCanAccessAtomsDuringGC& canAccess) {}
};

class MOZ_RAII AutoKeepAtoms
{
    JSContext* cx;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    explicit inline AutoKeepAtoms(JSContext* cx
                           MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
    inline ~AutoKeepAtoms();
};

// Debugging RAII class which marks the current thread as performing an Ion
// compilation, for use by CurrentThreadCan{Read,Write}CompilationData
class MOZ_RAII AutoEnterIonCompilation
{
  public:
    explicit AutoEnterIonCompilation(bool safeForMinorGC
                                     MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;

#ifdef DEBUG
        JSContext* cx = TlsContext.get();
        MOZ_ASSERT(!cx->ionCompiling);
        MOZ_ASSERT(!cx->ionCompilingSafeForMinorGC);
        cx->ionCompiling = true;
        cx->ionCompilingSafeForMinorGC = safeForMinorGC;
#endif
    }

    ~AutoEnterIonCompilation() {
#ifdef DEBUG
        JSContext* cx = TlsContext.get();
        MOZ_ASSERT(cx->ionCompiling);
        cx->ionCompiling = false;
        cx->ionCompilingSafeForMinorGC = false;
#endif
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

// Should be used in functions called directly from JIT code (with
// masm.callWithABI) to assert invariants in debug builds.
class MOZ_RAII AutoUnsafeCallWithABI
{
#ifdef DEBUG
    JSContext* cx_;
    bool nested_;
#endif
    JS::AutoCheckCannotGC nogc;

  public:
#ifdef DEBUG
    AutoUnsafeCallWithABI();
    ~AutoUnsafeCallWithABI();
#endif
};

namespace gc {

// In debug builds, set/unset the performing GC flag for the current thread.
struct MOZ_RAII AutoSetThreadIsPerformingGC
{
#ifdef DEBUG
    AutoSetThreadIsPerformingGC()
      : cx(TlsContext.get())
    {
        MOZ_ASSERT(!cx->performingGC);
        cx->performingGC = true;
    }

    ~AutoSetThreadIsPerformingGC() {
        MOZ_ASSERT(cx->performingGC);
        cx->performingGC = false;
    }

  private:
    JSContext* cx;
#else
    AutoSetThreadIsPerformingGC() {}
#endif
};

// In debug builds, set/reset the GC sweeping flag for the current thread.
struct MOZ_RAII AutoSetThreadIsSweeping
{
#ifdef DEBUG
    AutoSetThreadIsSweeping()
      : cx(TlsContext.get()),
        prevState(cx->gcSweeping)
    {
        cx->gcSweeping = true;
    }

    ~AutoSetThreadIsSweeping() {
        cx->gcSweeping = prevState;
    }

  private:
    JSContext* cx;
    bool prevState;
#else
    AutoSetThreadIsSweeping() {}
#endif
};

} // namespace gc

} /* namespace js */

#endif /* vm_JSContext_h */

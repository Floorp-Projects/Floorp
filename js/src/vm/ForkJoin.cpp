/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(XP_WIN)
# include <io.h>     // for isatty()
#else
# include <unistd.h> // for isatty()
#endif

#include "vm/ForkJoin.h"

#include "mozilla/ThreadLocal.h"

#include "jscntxt.h"
#include "jslock.h"
#include "jsprf.h"

#include "builtin/TypedObject.h"

#ifdef JS_THREADSAFE
# include "jit/BaselineJIT.h"
# include "vm/Monitor.h"
#endif

#if defined(JS_THREADSAFE) && defined(JS_ION)
# include "jit/JitCommon.h"
# ifdef DEBUG
#  include "jit/Ion.h"
#  include "jit/JitCompartment.h"
#  include "jit/MIR.h"
#  include "jit/MIRGraph.h"
# endif
#endif // THREADSAFE && ION

#include "vm/Interpreter-inl.h"

using namespace js;
using namespace js::parallel;
using namespace js::jit;

using mozilla::ThreadLocal;

///////////////////////////////////////////////////////////////////////////
// Degenerate configurations
//
// When JS_THREADSAFE or JS_ION is not defined, we simply run the
// |func| callback sequentially.  We also forego the feedback
// altogether.

static bool
ExecuteSequentially(JSContext *cx_, HandleValue funVal, uint16_t *sliceStart,
                    uint16_t sliceEnd);

#if !defined(JS_THREADSAFE) || !defined(JS_ION)
bool
js::ForkJoin(JSContext *cx, CallArgs &args)
{
    RootedValue argZero(cx, args[0]);
    uint16_t sliceStart = uint16_t(args[1].toInt32());
    uint16_t sliceEnd = uint16_t(args[2].toInt32());
    if (!ExecuteSequentially(cx, argZero, &sliceStart, sliceEnd))
        return false;
    MOZ_ASSERT(sliceStart == sliceEnd);
    return true;
}

JSContext *
ForkJoinContext::acquireJSContext()
{
    return nullptr;
}

void
ForkJoinContext::releaseJSContext()
{
}

bool
ForkJoinContext::isMainThread() const
{
    return true;
}

JSRuntime *
ForkJoinContext::runtime()
{
    MOZ_ASSUME_UNREACHABLE("Not THREADSAFE build");
}

bool
ForkJoinContext::check()
{
    MOZ_ASSUME_UNREACHABLE("Not THREADSAFE build");
}

void
ForkJoinContext::requestGC(JS::gcreason::Reason reason)
{
    MOZ_ASSUME_UNREACHABLE("Not THREADSAFE build");
}

void
ForkJoinContext::requestZoneGC(JS::Zone *zone, JS::gcreason::Reason reason)
{
    MOZ_ASSUME_UNREACHABLE("Not THREADSAFE build");
}

bool
ForkJoinContext::setPendingAbortFatal(ParallelBailoutCause cause)
{
    MOZ_ASSUME_UNREACHABLE("Not THREADSAFE build");
    return false;
}

void
ParallelBailoutRecord::setCause(ParallelBailoutCause cause,
                                JSScript *outermostScript,
                                JSScript *currentScript,
                                jsbytecode *currentPc)
{
    MOZ_ASSUME_UNREACHABLE("Not THREADSAFE build");
}

void
js::ParallelBailoutRecord::updateCause(ParallelBailoutCause cause,
                                       JSScript *outermostScript,
                                       JSScript *currentScript,
                                       jsbytecode *currentPc)
{
    MOZ_ASSUME_UNREACHABLE("Not THREADSAFE build");
}

void
ParallelBailoutRecord::addTrace(JSScript *script,
                                jsbytecode *pc)
{
    MOZ_ASSUME_UNREACHABLE("Not THREADSAFE build");
}

bool
js::InExclusiveParallelSection()
{
    return false;
}

bool
js::ParallelTestsShouldPass(JSContext *cx)
{
    return false;
}

bool
js::intrinsic_SetForkJoinTargetRegion(JSContext *cx, unsigned argc, Value *vp)
{
    return true;
}

static bool
intrinsic_SetForkJoinTargetRegionPar(ForkJoinContext *cx, unsigned argc, Value *vp)
{
    return true;
}

JS_JITINFO_NATIVE_PARALLEL(js::intrinsic_SetForkJoinTargetRegionInfo,
                           intrinsic_SetForkJoinTargetRegionPar);

bool
js::intrinsic_ClearThreadLocalArenas(JSContext *cx, unsigned argc, Value *vp)
{
    return true;
}

static bool
intrinsic_ClearThreadLocalArenasPar(ForkJoinContext *cx, unsigned argc, Value *vp)
{
    return true;
}

JS_JITINFO_NATIVE_PARALLEL(js::intrinsic_ClearThreadLocalArenasInfo,
                           intrinsic_ClearThreadLocalArenasPar);

#endif // !JS_THREADSAFE || !JS_ION

///////////////////////////////////////////////////////////////////////////
// All configurations
//
// Some code that is shared between degenerate and parallel configurations.

static bool
ExecuteSequentially(JSContext *cx, HandleValue funVal, uint16_t *sliceStart,
                    uint16_t sliceEnd)
{
    FastInvokeGuard fig(cx, funVal);
    InvokeArgs &args = fig.args();
    if (!args.init(3))
        return false;
    args.setCallee(funVal);
    args.setThis(UndefinedValue());
    args[0].setInt32(0);
    args[1].setInt32(*sliceStart);
    args[2].setInt32(sliceEnd);
    if (!fig.invoke(cx))
        return false;
    *sliceStart = (uint16_t)(args.rval().toInt32());
    return true;
}

ThreadLocal<ForkJoinContext*> ForkJoinContext::tlsForkJoinContext;

/* static */ bool
ForkJoinContext::initialize()
{
    if (!tlsForkJoinContext.initialized()) {
        if (!tlsForkJoinContext.init())
            return false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////
// Parallel configurations
//
// The remainder of this file is specific to cases where both
// JS_THREADSAFE and JS_ION are enabled.

#if defined(JS_THREADSAFE) && defined(JS_ION)

///////////////////////////////////////////////////////////////////////////
// Class Declarations and Function Prototypes

namespace js {

// When writing tests, it is often useful to specify different modes
// of operation.
enum ForkJoinMode {
    // WARNING: If you change this enum, you MUST update
    // ForkJoinMode() in Utilities.js

    // The "normal" behavior: attempt parallel, fallback to
    // sequential.  If compilation is ongoing in a helper thread, then
    // run sequential warmup iterations in the meantime. If those
    // iterations wind up completing all the work, just abort.
    ForkJoinModeNormal,

    // Like normal, except that we will keep running warmup iterations
    // until compilations are complete, even if there is no more work
    // to do. This is useful in tests as a "setup" run.
    ForkJoinModeCompile,

    // Requires that compilation has already completed. Expects parallel
    // execution to proceed without a hitch. (Reports an error otherwise)
    ForkJoinModeParallel,

    // Requires that compilation has already completed. Expects
    // parallel execution to bailout once but continue after that without
    // further bailouts. (Reports an error otherwise)
    ForkJoinModeRecover,

    // Expects all parallel executions to yield a bailout.  If this is not
    // the case, reports an error.
    ForkJoinModeBailout,

    NumForkJoinModes
};

class ForkJoinOperation
{
  public:
    // For tests, make sure to keep this in sync with minItemsTestingThreshold.
    static const uint32_t MAX_BAILOUTS = 3;
    uint32_t bailouts;

    // Information about the bailout:
    ParallelBailoutCause bailoutCause;
    RootedScript bailoutScript;
    jsbytecode *bailoutBytecode;

    ForkJoinOperation(JSContext *cx, HandleFunction fun, uint16_t sliceStart,
                      uint16_t sliceEnd, ForkJoinMode mode);
    ExecutionStatus apply();

  private:
    // Most of the functions involved in managing the parallel
    // compilation follow a similar control-flow. They return RedLight
    // if they have either encountered a fatal error or completed the
    // execution, such that no further work is needed. In that event,
    // they take an `ExecutionStatus*` which they use to report
    // whether execution was successful or not. If the function
    // returns `GreenLight`, then the parallel operation is not yet
    // fully completed, so the state machine should carry on.
    enum TrafficLight {
        RedLight,
        GreenLight
    };

    struct WorklistData {
        // True if we enqueued the callees from the ion-compiled
        // version of this entry
        bool calleesEnqueued;

        // Last record useCount; updated after warmup
        // iterations;
        uint32_t useCount;

        // Number of continuous "stalls" --- meaning warmups
        // where useCount did not increase.
        uint32_t stallCount;

        void reset() {
            calleesEnqueued = false;
            useCount = 0;
            stallCount = 0;
        }
    };

    JSContext *cx_;
    HandleFunction fun_;
    uint16_t sliceStart_;
    uint16_t sliceEnd_;
    Vector<ParallelBailoutRecord, 16> bailoutRecords_;
    AutoScriptVector worklist_;
    Vector<WorklistData, 16> worklistData_;
    ForkJoinMode mode_;

    TrafficLight enqueueInitialScript(ExecutionStatus *status);
    TrafficLight compileForParallelExecution(ExecutionStatus *status);
    TrafficLight warmupExecution(bool stopIfComplete, ExecutionStatus *status);
    TrafficLight parallelExecution(ExecutionStatus *status);
    TrafficLight sequentialExecution(bool disqualified, ExecutionStatus *status);
    TrafficLight recoverFromBailout(ExecutionStatus *status);
    TrafficLight fatalError(ExecutionStatus *status);
    bool isInitialScript(HandleScript script);
    void determineBailoutCause();
    bool invalidateBailedOutScripts();
    ExecutionStatus sequentialExecution(bool disqualified);

    TrafficLight appendCallTargetsToWorklist(uint32_t index, ExecutionStatus *status);
    TrafficLight appendCallTargetToWorklist(HandleScript script, ExecutionStatus *status);
    bool addToWorklist(HandleScript script);
    inline bool hasScript(Vector<types::RecompileInfo> &scripts, JSScript *script);
}; // class ForkJoinOperation

class ForkJoinShared : public ParallelJob, public Monitor
{
    /////////////////////////////////////////////////////////////////////////
    // Constant fields

    JSContext *const cx_;                  // Current context
    ThreadPool *const threadPool_;         // The thread pool
    HandleFunction fun_;                   // The JavaScript function to execute
    uint16_t sliceStart_;                  // The starting slice id.
    uint16_t sliceEnd_;                    // The ending slice id + 1.
    PRLock *cxLock_;                       // Locks cx_ for parallel VM calls
    ParallelBailoutRecord *const records_; // Bailout records for each worker

    /////////////////////////////////////////////////////////////////////////
    // Per-thread arenas
    //
    // Each worker thread gets an arena to use when allocating.

    Vector<Allocator *, 16> allocators_;

    /////////////////////////////////////////////////////////////////////////
    // Locked Fields
    //
    // Only to be accessed while holding the lock.

    bool gcRequested_;              // True if a worker requested a GC
    JS::gcreason::Reason gcReason_; // Reason given to request GC
    Zone *gcZone_;                  // Zone for GC, or nullptr for full

    /////////////////////////////////////////////////////////////////////////
    // Asynchronous Flags
    //
    // These can be accessed without the lock and are thus atomic.

    // Set to true when parallel execution should abort.
    mozilla::Atomic<bool, mozilla::ReleaseAcquire> abort_;

    // Set to true when a worker bails for a fatal reason.
    mozilla::Atomic<bool, mozilla::ReleaseAcquire> fatal_;

  public:
    ForkJoinShared(JSContext *cx,
                   ThreadPool *threadPool,
                   HandleFunction fun,
                   uint16_t sliceStart,
                   uint16_t sliceEnd,
                   ParallelBailoutRecord *records);
    ~ForkJoinShared();

    bool init();

    ParallelResult execute();

    // Invoked from parallel worker threads:
    virtual bool executeFromWorker(ThreadPoolWorker *worker, uintptr_t stackLimit) MOZ_OVERRIDE;

    // Invoked only from the main thread:
    virtual bool executeFromMainThread(ThreadPoolWorker *worker) MOZ_OVERRIDE;

    // Executes the user-supplied function a worker or the main thread.
    void executePortion(PerThreadData *perThread, ThreadPoolWorker *worker);

    // Moves all the per-thread arenas into the main compartment and processes
    // any pending requests for a GC. This can only safely be invoked on the
    // main thread after the workers have completed.
    void transferArenasToCompartmentAndProcessGCRequests();


    // Requests a GC, either full or specific to a zone.
    void requestGC(JS::gcreason::Reason reason);
    void requestZoneGC(JS::Zone *zone, JS::gcreason::Reason reason);

    // Requests that computation abort.
    void setAbortFlagDueToInterrupt(ForkJoinContext &cx);
    void setAbortFlagAndRequestInterrupt(bool fatal);

    // Set the fatal flag for the next abort.
    void setPendingAbortFatal() { fatal_ = true; }

    JSRuntime *runtime() { return cx_->runtime(); }
    JS::Zone *zone() { return cx_->zone(); }
    JSCompartment *compartment() { return cx_->compartment(); }

    JSContext *acquireJSContext() { PR_Lock(cxLock_); return cx_; }
    void releaseJSContext() { PR_Unlock(cxLock_); }
};

class AutoEnterWarmup
{
    JSRuntime *runtime_;

  public:
    AutoEnterWarmup(JSRuntime *runtime) : runtime_(runtime) { runtime_->forkJoinWarmup++; }
    ~AutoEnterWarmup() { runtime_->forkJoinWarmup--; }
};

class AutoSetForkJoinContext
{
  public:
    AutoSetForkJoinContext(ForkJoinContext *threadCx) {
        ForkJoinContext::tlsForkJoinContext.set(threadCx);
    }

    ~AutoSetForkJoinContext() {
        ForkJoinContext::tlsForkJoinContext.set(nullptr);
    }
};

} // namespace js

///////////////////////////////////////////////////////////////////////////
// ForkJoinActivation
//
// Takes care of tidying up GC before we enter a fork join section. Also
// pauses the barrier verifier, as we cannot enter fork join with the runtime
// or the zone needing barriers.

ForkJoinActivation::ForkJoinActivation(JSContext *cx)
  : Activation(cx, ForkJoin),
    prevIonTop_(cx->mainThread().ionTop),
    av_(cx->runtime(), false)
{
    // Note: we do not allow GC during parallel sections.
    // Moreover, we do not wish to worry about making
    // write barriers thread-safe.  Therefore, we guarantee
    // that there is no incremental GC in progress and force
    // a minor GC to ensure no cross-generation pointers get
    // created:

    if (JS::IsIncrementalGCInProgress(cx->runtime())) {
        JS::PrepareForIncrementalGC(cx->runtime());
        JS::FinishIncrementalGC(cx->runtime(), JS::gcreason::API);
    }

    MinorGC(cx->runtime(), JS::gcreason::API);

    cx->runtime()->gcHelperThread.waitBackgroundSweepEnd();

    JS_ASSERT(!cx->runtime()->needsBarrier());
    JS_ASSERT(!cx->zone()->needsBarrier());
}

ForkJoinActivation::~ForkJoinActivation()
{
    cx_->mainThread().ionTop = prevIonTop_;
}

///////////////////////////////////////////////////////////////////////////
// js::ForkJoin() and ForkJoinOperation class
//
// These are the top-level objects that manage the parallel execution.
// They handle parallel compilation (if necessary), triggering
// parallel execution, and recovering from bailouts.

static const char *ForkJoinModeString(ForkJoinMode mode);

bool
js::ForkJoin(JSContext *cx, CallArgs &args)
{
    JS_ASSERT(args.length() == 4); // else the self-hosted code is wrong
    JS_ASSERT(args[0].isObject());
    JS_ASSERT(args[0].toObject().is<JSFunction>());
    JS_ASSERT(args[1].isInt32());
    JS_ASSERT(args[2].isInt32());
    JS_ASSERT(args[3].isInt32());
    JS_ASSERT(args[3].toInt32() < NumForkJoinModes);

    RootedFunction fun(cx, &args[0].toObject().as<JSFunction>());
    uint16_t sliceStart = (uint16_t)(args[1].toInt32());
    uint16_t sliceEnd = (uint16_t)(args[2].toInt32());
    ForkJoinMode mode = (ForkJoinMode)(args[3].toInt32());

    MOZ_ASSERT(sliceStart == args[1].toInt32());
    MOZ_ASSERT(sliceEnd == args[2].toInt32());
    MOZ_ASSERT(sliceStart <= sliceEnd);

    ForkJoinOperation op(cx, fun, sliceStart, sliceEnd, mode);
    ExecutionStatus status = op.apply();
    if (status == ExecutionFatal)
        return false;

    switch (mode) {
      case ForkJoinModeNormal:
      case ForkJoinModeCompile:
        return true;

      case ForkJoinModeParallel:
        if (status == ExecutionParallel && op.bailouts == 0)
            return true;
        break;

      case ForkJoinModeRecover:
        if (status != ExecutionSequential && op.bailouts > 0)
            return true;
        break;

      case ForkJoinModeBailout:
        if (status != ExecutionParallel)
            return true;
        break;

      case NumForkJoinModes:
        break;
    }

    const char *statusString = "?";
    switch (status) {
      case ExecutionSequential: statusString = "seq"; break;
      case ExecutionParallel: statusString = "par"; break;
      case ExecutionWarmup: statusString = "warmup"; break;
      case ExecutionFatal: statusString = "fatal"; break;
    }

    if (ParallelTestsShouldPass(cx)) {
        JS_ReportError(cx, "ForkJoin: mode=%s status=%s bailouts=%d",
                       ForkJoinModeString(mode), statusString, op.bailouts);
        return false;
    }
    return true;
}

static const char *
ForkJoinModeString(ForkJoinMode mode) {
    switch (mode) {
      case ForkJoinModeNormal: return "normal";
      case ForkJoinModeCompile: return "compile";
      case ForkJoinModeParallel: return "parallel";
      case ForkJoinModeRecover: return "recover";
      case ForkJoinModeBailout: return "bailout";
      case NumForkJoinModes: return "max";
    }
    return "???";
}

ForkJoinOperation::ForkJoinOperation(JSContext *cx, HandleFunction fun, uint16_t sliceStart,
                                     uint16_t sliceEnd, ForkJoinMode mode)
  : bailouts(0),
    bailoutCause(ParallelBailoutNone),
    bailoutScript(cx),
    bailoutBytecode(nullptr),
    cx_(cx),
    fun_(fun),
    sliceStart_(sliceStart),
    sliceEnd_(sliceEnd),
    bailoutRecords_(cx),
    worklist_(cx),
    worklistData_(cx),
    mode_(mode)
{ }

ExecutionStatus
ForkJoinOperation::apply()
{
    ExecutionStatus status;

    // High level outline of the procedure:
    //
    // - As we enter, we check for parallel script without "uncompiled" flag.
    // - If present, skip initial enqueue.
    // - While not too many bailouts:
    //   - While all scripts in worklist are not compiled:
    //     - For each script S in worklist:
    //       - Compile S if not compiled
    //         -> Error: fallback
    //       - If compiled, add call targets to worklist w/o checking uncompiled
    //         flag
    //     - If some compilations pending, run warmup iteration
    //     - Otherwise, clear "uncompiled targets" flag on main script and
    //       break from loop
    //   - Attempt parallel execution
    //     - If successful: return happily
    //     - If error: abort sadly
    //     - If bailout:
    //       - Invalidate any scripts that may need to be invalidated
    //       - Re-enqueue main script and any uncompiled scripts that were called
    // - Too many bailouts: Fallback to sequential

    JS_ASSERT_IF(!jit::IsBaselineEnabled(cx_), !jit::IsIonEnabled(cx_));
    if (!jit::IsBaselineEnabled(cx_) || !jit::IsIonEnabled(cx_))
        return sequentialExecution(true);

    SpewBeginOp(cx_, "ForkJoinOperation");

    // How many workers do we have, counting the main thread.
    unsigned numWorkers = cx_->runtime()->threadPool.numWorkers();

    if (!bailoutRecords_.resize(numWorkers))
        return SpewEndOp(ExecutionFatal);

    for (uint32_t i = 0; i < numWorkers; i++)
        bailoutRecords_[i].init(cx_);

    if (enqueueInitialScript(&status) == RedLight)
        return SpewEndOp(status);

    Spew(SpewOps, "Execution mode: %s", ForkJoinModeString(mode_));
    switch (mode_) {
      case ForkJoinModeNormal:
      case ForkJoinModeCompile:
      case ForkJoinModeBailout:
        break;

      case ForkJoinModeParallel:
      case ForkJoinModeRecover:
        // These two modes are used to check that every iteration can
        // be executed in parallel. They expect compilation to have
        // been done. But, when using gc zeal, it's possible that
        // compiled scripts were collected.
        if (ParallelTestsShouldPass(cx_) && worklist_.length() != 0) {
            JS_ReportError(cx_, "ForkJoin: compilation required in par or bailout mode");
            return SpewEndOp(ExecutionFatal);
        }
        break;

      case NumForkJoinModes:
        MOZ_ASSUME_UNREACHABLE("Invalid mode");
    }

    while (bailouts < MAX_BAILOUTS) {
        for (uint32_t i = 0; i < numWorkers; i++)
            bailoutRecords_[i].reset(cx_);

        if (compileForParallelExecution(&status) == RedLight)
            return SpewEndOp(status);

        JS_ASSERT(worklist_.length() == 0);
        if (parallelExecution(&status) == RedLight)
            return SpewEndOp(status);

        if (recoverFromBailout(&status) == RedLight)
            return SpewEndOp(status);
    }

    // After enough tries, just execute sequentially.
    return SpewEndOp(sequentialExecution(true));
}

ForkJoinOperation::TrafficLight
ForkJoinOperation::enqueueInitialScript(ExecutionStatus *status)
{
    // GreenLight: script successfully enqueued if necessary
    // RedLight: fatal error or fell back to sequential

    // The kernel should be a self-hosted function.
    if (!fun_->is<JSFunction>())
        return sequentialExecution(true, status);

    RootedFunction callee(cx_, &fun_->as<JSFunction>());

    if (!callee->isInterpreted() || !callee->isSelfHostedBuiltin())
        return sequentialExecution(true, status);

    // If the main script is already compiled, and we have no reason
    // to suspect any of its callees are not compiled, then we can
    // just skip the compilation step.
    RootedScript script(cx_, callee->getOrCreateScript(cx_));
    if (!script)
        return RedLight;

    if (script->hasParallelIonScript()) {
        // Notify that there's been activity on the entry script.
        JitCompartment *jitComp = cx_->compartment()->jitCompartment();
        if (!jitComp->notifyOfActiveParallelEntryScript(cx_, script)) {
            *status = ExecutionFatal;
            return RedLight;
        }

        if (!script->parallelIonScript()->hasUncompiledCallTarget()) {
            Spew(SpewOps, "Script %p:%s:%d already compiled, no uncompiled callees",
                 script.get(), script->filename(), script->lineno());
            return GreenLight;
        }

        Spew(SpewOps, "Script %p:%s:%d already compiled, may have uncompiled callees",
             script.get(), script->filename(), script->lineno());
    }

    // Otherwise, add to the worklist of scripts to process.
    if (addToWorklist(script) == RedLight)
        return fatalError(status);
    return GreenLight;
}

ForkJoinOperation::TrafficLight
ForkJoinOperation::compileForParallelExecution(ExecutionStatus *status)
{
    // GreenLight: all scripts compiled
    // RedLight: fatal error or completed work via warmups or fallback

    // This routine attempts to do whatever compilation is necessary
    // to execute a single parallel attempt. When it returns, either
    // (1) we have fallen back to sequential; (2) we have run enough
    // warmup runs to complete all the work; or (3) we have compiled
    // all scripts we think likely to be executed during a parallel
    // execution.

    RootedFunction fun(cx_);
    RootedScript script(cx_);

    // After 3 stalls, we stop waiting for a script to gather type
    // info and move on with execution.
    const uint32_t stallThreshold = 3;

    // This loop continues to iterate until the full contents of
    // `worklist` have been successfully compiled for parallel
    // execution. The compilations themselves typically occur on
    // helper threads. While we wait for the compilations to complete,
    // or for sufficient type information to be gathered, we execute
    // warmup iterations.
    while (true) {
        bool offMainThreadCompilationsInProgress = false;
        bool gatheringTypeInformation = false;

        // Walk over the worklist to check on the status of each entry.
        for (uint32_t i = 0; i < worklist_.length(); i++) {
            script = worklist_[i];
            script->ensureNonLazyCanonicalFunction(cx_);
            fun = script->functionNonDelazifying();

            // No baseline script means no type information, hence we
            // will not be able to compile very well.  In such cases,
            // we continue to run baseline iterations until either (1)
            // the potential callee *has* a baseline script or (2) the
            // potential callee's use count stops increasing,
            // indicating that they are not in fact a callee.
            if (!script->hasBaselineScript()) {
                uint32_t previousUseCount = worklistData_[i].useCount;
                uint32_t currentUseCount = script->getUseCount();
                if (previousUseCount < currentUseCount) {
                    worklistData_[i].useCount = currentUseCount;
                    worklistData_[i].stallCount = 0;
                    gatheringTypeInformation = true;

                    Spew(SpewCompile,
                         "Script %p:%s:%d has no baseline script, "
                         "but use count grew from %d to %d",
                         script.get(), script->filename(), script->lineno(),
                         previousUseCount, currentUseCount);
                } else {
                    uint32_t stallCount = ++worklistData_[i].stallCount;
                    if (stallCount < stallThreshold) {
                        gatheringTypeInformation = true;
                    }

                    Spew(SpewCompile,
                         "Script %p:%s:%d has no baseline script, "
                         "and use count has %u stalls at %d",
                         script.get(), script->filename(), script->lineno(),
                         stallCount, previousUseCount);
                }
                continue;
            }

            if (!script->hasParallelIonScript()) {
                // Script has not yet been compiled. Attempt to compile it.
                SpewBeginCompile(script);
                MethodStatus mstatus = jit::CanEnterInParallel(cx_, script);
                SpewEndCompile(mstatus);

                switch (mstatus) {
                  case Method_Error:
                    return fatalError(status);

                  case Method_CantCompile:
                    Spew(SpewCompile,
                         "Script %p:%s:%d cannot be compiled, "
                         "falling back to sequential execution",
                         script.get(), script->filename(), script->lineno());
                    return sequentialExecution(true, status);

                  case Method_Skipped:
                    // A "skipped" result either means that we are compiling
                    // in parallel OR some other transient error occurred.
                    if (script->isParallelIonCompilingOffThread()) {
                        Spew(SpewCompile,
                             "Script %p:%s:%d compiling off-thread",
                             script.get(), script->filename(), script->lineno());
                        offMainThreadCompilationsInProgress = true;
                        continue;
                    }
                    return sequentialExecution(false, status);

                  case Method_Compiled:
                    Spew(SpewCompile,
                         "Script %p:%s:%d compiled",
                         script.get(), script->filename(), script->lineno());
                    JS_ASSERT(script->hasParallelIonScript());

                    if (isInitialScript(script)) {
                        JitCompartment *jitComp = cx_->compartment()->jitCompartment();
                        if (!jitComp->notifyOfActiveParallelEntryScript(cx_, script)) {
                            *status = ExecutionFatal;
                            return RedLight;
                        }
                    }

                    break;
                }
            }

            // At this point, either the script was already compiled
            // or we just compiled it.  Check whether its "uncompiled
            // call target" flag is set and add the targets to our
            // worklist if so. Clear the flag after that, since we
            // will be compiling the call targets.
            JS_ASSERT(script->hasParallelIonScript());
            if (appendCallTargetsToWorklist(i, status) == RedLight)
                return RedLight;
        }

        // If there is compilation occurring in a helper thread, then
        // run a warmup iterations in the main thread while we wait.
        // There is a chance that this warmup will finish all the work
        // we have to do, so we should stop then, unless we are in
        // compile mode, in which case we'll continue to block.
        //
        // Note that even in compile mode, we can't block *forever*:
        // - OMTC compiles will finish;
        // - no work is being done, so use counts on not-yet-baselined
        //   scripts will not increase.
        if (offMainThreadCompilationsInProgress || gatheringTypeInformation) {
            bool stopIfComplete = (mode_ != ForkJoinModeCompile);
            if (warmupExecution(stopIfComplete, status) == RedLight)
                return RedLight;
            continue;
        }

        // All compilations are complete. However, be careful: it is
        // possible that a garbage collection occurred while we were
        // iterating and caused some of the scripts we thought we had
        // compiled to be collected. In that case, we will just have
        // to begin again.
        bool allScriptsPresent = true;
        for (uint32_t i = 0; i < worklist_.length(); i++) {
            if (!worklist_[i]->hasParallelIonScript()) {
                if (worklistData_[i].stallCount < stallThreshold) {
                    worklistData_[i].reset();
                    allScriptsPresent = false;

                    Spew(SpewCompile,
                         "Script %p:%s:%d is not stalled, "
                         "but no parallel ion script found, "
                         "restarting loop",
                         script.get(), script->filename(), script->lineno());
                }
            }
        }

        if (allScriptsPresent)
            break;
    }

    Spew(SpewCompile, "Compilation complete (final worklist length %d)",
         worklist_.length());

    // At this point, all scripts and their transitive callees are
    // either stalled (indicating they are unlikely to be called) or
    // in a compiled state.  Therefore we can clear the
    // "hasUncompiledCallTarget" flag on them and then clear the
    // worklist.
    for (uint32_t i = 0; i < worklist_.length(); i++) {
        if (worklist_[i]->hasParallelIonScript()) {
            JS_ASSERT(worklistData_[i].calleesEnqueued);
            worklist_[i]->parallelIonScript()->clearHasUncompiledCallTarget();
        } else {
            JS_ASSERT(worklistData_[i].stallCount >= stallThreshold);
        }
    }
    worklist_.clear();
    worklistData_.clear();
    return GreenLight;
}

ForkJoinOperation::TrafficLight
ForkJoinOperation::appendCallTargetsToWorklist(uint32_t index, ExecutionStatus *status)
{
    // GreenLight: call targets appended
    // RedLight: fatal error or completed work via warmups or fallback

    JS_ASSERT(worklist_[index]->hasParallelIonScript());

    // Check whether we have already enqueued the targets for
    // this entry and avoid doing it again if so.
    if (worklistData_[index].calleesEnqueued)
        return GreenLight;
    worklistData_[index].calleesEnqueued = true;

    // Iterate through the callees and enqueue them.
    RootedScript target(cx_);
    IonScript *ion = worklist_[index]->parallelIonScript();
    for (uint32_t i = 0; i < ion->callTargetEntries(); i++) {
        target = ion->callTargetList()[i];
        parallel::Spew(parallel::SpewCompile,
                       "Adding call target %s:%u",
                       target->filename(), target->lineno());
        if (appendCallTargetToWorklist(target, status) == RedLight)
            return RedLight;
    }

    return GreenLight;
}

ForkJoinOperation::TrafficLight
ForkJoinOperation::appendCallTargetToWorklist(HandleScript script, ExecutionStatus *status)
{
    // GreenLight: call target appended if necessary
    // RedLight: fatal error or completed work via warmups or fallback

    JS_ASSERT(script);

    // Fallback to sequential if disabled.
    if (!script->canParallelIonCompile()) {
        Spew(SpewCompile, "Skipping %p:%s:%u, canParallelIonCompile() is false",
             script.get(), script->filename(), script->lineno());
        return sequentialExecution(true, status);
    }

    if (script->hasParallelIonScript()) {
        // Skip if the code is expected to result in a bailout.
        if (script->parallelIonScript()->bailoutExpected()) {
            Spew(SpewCompile, "Skipping %p:%s:%u, bailout expected",
                 script.get(), script->filename(), script->lineno());
            return sequentialExecution(false, status);
        }
    }

    if (!addToWorklist(script))
        return fatalError(status);

    return GreenLight;
}

bool
ForkJoinOperation::addToWorklist(HandleScript script)
{
    for (uint32_t i = 0; i < worklist_.length(); i++) {
        if (worklist_[i] == script) {
            Spew(SpewCompile, "Skipping %p:%s:%u, already in worklist",
                 script.get(), script->filename(), script->lineno());
            return true;
        }
    }

    Spew(SpewCompile, "Enqueued %p:%s:%u",
         script.get(), script->filename(), script->lineno());

    // Note that we add all possibly compilable functions to the worklist,
    // even if they're already compiled. This is so that we can return
    // Method_Compiled and not Method_Skipped if we have a worklist full of
    // already-compiled functions.
    if (!worklist_.append(script))
        return false;

    // we have not yet enqueued the callees of this script
    if (!worklistData_.append(WorklistData()))
        return false;
    worklistData_[worklistData_.length() - 1].reset();

    return true;
}

ForkJoinOperation::TrafficLight
ForkJoinOperation::sequentialExecution(bool disqualified, ExecutionStatus *status)
{
    // RedLight: fatal error or completed work

    *status = sequentialExecution(disqualified);
    return RedLight;
}

ExecutionStatus
ForkJoinOperation::sequentialExecution(bool disqualified)
{
    // XXX use disqualified to set parallelIon to ION_DISABLED_SCRIPT?

    Spew(SpewOps, "Executing sequential execution (disqualified=%d).",
         disqualified);

    if (sliceStart_ == sliceEnd_)
        return ExecutionSequential;

    RootedValue funVal(cx_, ObjectValue(*fun_));
    if (!ExecuteSequentially(cx_, funVal, &sliceStart_, sliceEnd_))
        return ExecutionFatal;
    MOZ_ASSERT(sliceStart_ == sliceEnd_);
    return ExecutionSequential;
}

ForkJoinOperation::TrafficLight
ForkJoinOperation::fatalError(ExecutionStatus *status)
{
    // RedLight: fatal error

    *status = ExecutionFatal;
    return RedLight;
}

static const char *
BailoutExplanation(ParallelBailoutCause cause)
{
    switch (cause) {
      case ParallelBailoutNone:
        return "no particular reason";
      case ParallelBailoutCompilationSkipped:
        return "compilation failed (method skipped)";
      case ParallelBailoutCompilationFailure:
        return "compilation failed";
      case ParallelBailoutInterrupt:
        return "interrupted";
      case ParallelBailoutFailedIC:
        return "failed to attach stub to IC";
      case ParallelBailoutHeapBusy:
        return "heap busy flag set during interrupt";
      case ParallelBailoutMainScriptNotPresent:
        return "main script not present";
      case ParallelBailoutCalledToUncompiledScript:
        return "called to uncompiled script";
      case ParallelBailoutIllegalWrite:
        return "illegal write";
      case ParallelBailoutAccessToIntrinsic:
        return "access to intrinsic";
      case ParallelBailoutOverRecursed:
        return "over recursed";
      case ParallelBailoutOutOfMemory:
        return "out of memory";
      case ParallelBailoutUnsupported:
        return "unsupported";
      case ParallelBailoutUnsupportedVM:
        return "unsupported operation in VM call";
      case ParallelBailoutUnsupportedStringComparison:
        return "unsupported string comparison";
      case ParallelBailoutRequestedGC:
        return "requested GC";
      case ParallelBailoutRequestedZoneGC:
        return "requested zone GC";
      default:
        return "no known reason";
    }
}

bool
ForkJoinOperation::isInitialScript(HandleScript script)
{
    return fun_->is<JSFunction>() && (fun_->as<JSFunction>().nonLazyScript() == script);
}

void
ForkJoinOperation::determineBailoutCause()
{
    bailoutCause = ParallelBailoutNone;
    for (uint32_t i = 0; i < bailoutRecords_.length(); i++) {
        if (bailoutRecords_[i].cause == ParallelBailoutNone)
            continue;

        if (bailoutRecords_[i].cause == ParallelBailoutInterrupt)
            continue;

        bailoutCause = bailoutRecords_[i].cause;
        const char *causeStr = BailoutExplanation(bailoutCause);
        if (bailoutRecords_[i].depth) {
            bailoutScript = bailoutRecords_[i].trace[0].script;
            bailoutBytecode = bailoutRecords_[i].trace[0].bytecode;

            const char *filename = bailoutScript->filename();
            int line = JS_PCToLineNumber(cx_, bailoutScript, bailoutBytecode);
            JS_ReportWarning(cx_, "Bailed out of parallel operation: %s at %s:%d",
                             causeStr, filename, line);

            Spew(SpewBailouts, "Bailout from thread %d: cause %d at loc %s:%d",
                 i,
                 bailoutCause,
                 bailoutScript->filename(),
                 PCToLineNumber(bailoutScript, bailoutBytecode));
        } else {
            JS_ReportWarning(cx_, "Bailed out of parallel operation: %s",
                             causeStr);

            Spew(SpewBailouts, "Bailout from thread %d: cause %d, unknown loc",
                 i,
                 bailoutCause);
        }
    }
}

bool
ForkJoinOperation::invalidateBailedOutScripts()
{
    Vector<types::RecompileInfo> invalid(cx_);
    for (uint32_t i = 0; i < bailoutRecords_.length(); i++) {
        RootedScript script(cx_, bailoutRecords_[i].topScript);

        // No script to invalidate.
        if (!script || !script->hasParallelIonScript())
            continue;

        Spew(SpewBailouts,
             "Bailout from thread %d: cause %d, topScript %p:%s:%d",
             i,
             bailoutRecords_[i].cause,
             script.get(), script->filename(), script->lineno());

        switch (bailoutRecords_[i].cause) {
          // An interrupt is not the fault of the script, so don't
          // invalidate it.
          case ParallelBailoutInterrupt: continue;

          // An illegal write will not be made legal by invalidation.
          case ParallelBailoutIllegalWrite: continue;

          // For other cases, consider invalidation.
          default: break;
        }

        // Already invalidated.
        if (hasScript(invalid, script))
            continue;

        Spew(SpewBailouts, "Invalidating script %p:%s:%d due to cause %d",
             script.get(), script->filename(), script->lineno(),
             bailoutRecords_[i].cause);

        types::RecompileInfo co = script->parallelIonScript()->recompileInfo();

        if (!invalid.append(co))
            return false;

        // any script that we have marked for invalidation will need
        // to be recompiled
        if (!addToWorklist(script))
            return false;
    }

    Invalidate(cx_, invalid);

    return true;
}

ForkJoinOperation::TrafficLight
ForkJoinOperation::warmupExecution(bool stopIfComplete, ExecutionStatus *status)
{
    // GreenLight: warmup succeeded, still more work to do
    // RedLight: fatal error or warmup completed all work (check status)

    if (sliceStart_ == sliceEnd_) {
        Spew(SpewOps, "Warmup execution finished all the work.");

        if (stopIfComplete) {
            *status = ExecutionWarmup;
            return RedLight;
        }

        // If we finished all slices in warmup, be sure check the
        // interrupt flag. This is because we won't be running more JS
        // code, and thus no more automatic checking of the interrupt
        // flag.
        if (!CheckForInterrupt(cx_)) {
            *status = ExecutionFatal;
            return RedLight;
        }

        return GreenLight;
    }

    Spew(SpewOps, "Executing warmup from slice %d.", sliceStart_);

    AutoEnterWarmup warmup(cx_->runtime());
    RootedValue funVal(cx_, ObjectValue(*fun_));
    if (!ExecuteSequentially(cx_, funVal, &sliceStart_, sliceStart_ + 1)) {
        *status = ExecutionFatal;
        return RedLight;
    }

    return GreenLight;
}

ForkJoinOperation::TrafficLight
ForkJoinOperation::parallelExecution(ExecutionStatus *status)
{
    // GreenLight: bailout occurred, keep trying
    // RedLight: fatal error or all work completed

    // Recursive use of the ThreadPool is not supported.  Right now we
    // cannot get here because parallel code cannot invoke native
    // functions such as ForkJoin().
    JS_ASSERT(ForkJoinContext::current() == nullptr);

    if (sliceStart_ == sliceEnd_) {
        Spew(SpewOps, "Warmup execution finished all the work.");
        *status = ExecutionWarmup;
        return RedLight;
    }

    ForkJoinActivation activation(cx_);
    ThreadPool *threadPool = &cx_->runtime()->threadPool;
    ForkJoinShared shared(cx_, threadPool, fun_, sliceStart_, sliceEnd_, &bailoutRecords_[0]);
    if (!shared.init()) {
        *status = ExecutionFatal;
        return RedLight;
    }

    switch (shared.execute()) {
      case TP_SUCCESS:
        *status = ExecutionParallel;
        return RedLight;

      case TP_FATAL:
        *status = ExecutionFatal;
        return RedLight;

      case TP_RETRY_SEQUENTIALLY:
      case TP_RETRY_AFTER_GC:
        break; // bailout
    }

    return GreenLight;
}

ForkJoinOperation::TrafficLight
ForkJoinOperation::recoverFromBailout(ExecutionStatus *status)
{
    // GreenLight: bailout recovered, try to compile-and-run again
    // RedLight: fatal error

    bailouts += 1;
    determineBailoutCause();

    SpewBailout(bailouts, bailoutScript, bailoutBytecode, bailoutCause);

    // After any bailout, we always scan over callee list of main
    // function, if nothing else
    RootedScript mainScript(cx_, fun_->nonLazyScript());
    if (!addToWorklist(mainScript))
        return fatalError(status);

    // Also invalidate and recompile any callees that were implicated
    // by the bailout
    if (!invalidateBailedOutScripts())
        return fatalError(status);

    if (warmupExecution(/*stopIfComplete:*/true, status) == RedLight)
        return RedLight;

    return GreenLight;
}

bool
ForkJoinOperation::hasScript(Vector<types::RecompileInfo> &scripts, JSScript *script)
{
    for (uint32_t i = 0; i < scripts.length(); i++) {
        if (scripts[i] == script->parallelIonScript()->recompileInfo())
            return true;
    }
    return false;
}

// Can only enter callees with a valid IonScript.
template <uint32_t maxArgc>
class ParallelIonInvoke
{
    EnterJitCode enter_;
    void *jitcode_;
    void *calleeToken_;
    Value argv_[maxArgc + 2];
    uint32_t argc_;

  public:
    Value *args;

    ParallelIonInvoke(JSRuntime *rt,
                      HandleFunction callee,
                      uint32_t argc)
      : argc_(argc),
        args(argv_ + 2)
    {
        JS_ASSERT(argc <= maxArgc + 2);

        // Set 'callee' and 'this'.
        argv_[0] = ObjectValue(*callee);
        argv_[1] = UndefinedValue();

        // Find JIT code pointer.
        IonScript *ion = callee->nonLazyScript()->parallelIonScript();
        JitCode *code = ion->method();
        jitcode_ = code->raw();
        enter_ = rt->jitRuntime()->enterIon();
        calleeToken_ = CalleeToToken(callee);
    }

    bool invoke(PerThreadData *perThread) {
        RootedValue result(perThread);
        CALL_GENERATED_CODE(enter_, jitcode_, argc_ + 1, argv_ + 1, nullptr, calleeToken_,
                            nullptr, 0, result.address());
        return !result.isMagic();
    }
};

/////////////////////////////////////////////////////////////////////////////
// ForkJoinShared
//

ForkJoinShared::ForkJoinShared(JSContext *cx,
                               ThreadPool *threadPool,
                               HandleFunction fun,
                               uint16_t sliceStart,
                               uint16_t sliceEnd,
                               ParallelBailoutRecord *records)
  : cx_(cx),
    threadPool_(threadPool),
    fun_(fun),
    sliceStart_(sliceStart),
    sliceEnd_(sliceEnd),
    cxLock_(nullptr),
    records_(records),
    allocators_(cx),
    gcRequested_(false),
    gcReason_(JS::gcreason::NUM_REASONS),
    gcZone_(nullptr),
    abort_(false),
    fatal_(false)
{
}

bool
ForkJoinShared::init()
{
    // Create temporary arenas to hold the data allocated during the
    // parallel code.
    //
    // Note: you might think (as I did, initially) that we could use
    // compartment |Allocator| for the main thread.  This is not true,
    // because when executing parallel code we sometimes check what
    // arena list an object is in to decide if it is writable.  If we
    // used the compartment |Allocator| for the main thread, then the
    // main thread would be permitted to write to any object it wants.

    if (!Monitor::init())
        return false;

    cxLock_ = PR_NewLock();
    if (!cxLock_)
        return false;

    for (unsigned i = 0; i < threadPool_->numWorkers(); i++) {
        Allocator *allocator = cx_->new_<Allocator>(cx_->zone());
        if (!allocator)
            return false;

        if (!allocators_.append(allocator)) {
            js_delete(allocator);
            return false;
        }
    }

    return true;
}

ForkJoinShared::~ForkJoinShared()
{
    PR_DestroyLock(cxLock_);

    while (allocators_.length() > 0)
        js_delete(allocators_.popCopy());
}

ParallelResult
ForkJoinShared::execute()
{
    // Sometimes a GC request occurs *just before* we enter into the
    // parallel section.  Rather than enter into the parallel section
    // and then abort, we just check here and abort early.
    if (cx_->runtime()->interruptPar)
        return TP_RETRY_SEQUENTIALLY;

    AutoLockMonitor lock(*this);

    ParallelResult jobResult = TP_SUCCESS;
    {
        AutoUnlockMonitor unlock(*this);

        // Push parallel tasks and wait until they're all done.
        jobResult = threadPool_->executeJob(cx_, this, sliceStart_, sliceEnd_);
        if (jobResult == TP_FATAL)
            return TP_FATAL;
    }

    transferArenasToCompartmentAndProcessGCRequests();

    // Check if any of the workers failed.
    if (abort_) {
        if (fatal_)
            return TP_FATAL;
        return TP_RETRY_SEQUENTIALLY;
    }

#ifdef DEBUG
    Spew(SpewOps, "Completed parallel job [slices: %d, threads: %d, stolen: %d (work stealing:%s)]",
         sliceEnd_ - sliceStart_,
         threadPool_->numWorkers(),
         threadPool_->stolenSlices(),
         threadPool_->workStealing() ? "ON" : "OFF");
#endif

    // Everything went swimmingly. Give yourself a pat on the back.
    return jobResult;
}

void
ForkJoinShared::transferArenasToCompartmentAndProcessGCRequests()
{
    JSCompartment *comp = cx_->compartment();
    for (unsigned i = 0; i < threadPool_->numWorkers(); i++)
        comp->adoptWorkerAllocator(allocators_[i]);

    if (gcRequested_) {
        if (!gcZone_)
            TriggerGC(cx_->runtime(), gcReason_);
        else
            TriggerZoneGC(gcZone_, gcReason_);
        gcRequested_ = false;
        gcZone_ = nullptr;
    }
}

bool
ForkJoinShared::executeFromWorker(ThreadPoolWorker *worker, uintptr_t stackLimit)
{
    PerThreadData thisThread(cx_->runtime());
    if (!thisThread.init()) {
        setAbortFlagAndRequestInterrupt(true);
        return false;
    }
    TlsPerThreadData.set(&thisThread);

#ifdef JS_ARM_SIMULATOR
    stackLimit = Simulator::StackLimit();
#endif

    // Don't use setIonStackLimit() because that acquires the ionStackLimitLock, and the
    // lock has not been initialized in these cases.
    thisThread.jitStackLimit = stackLimit;
    executePortion(&thisThread, worker);
    TlsPerThreadData.set(nullptr);

    return !abort_;
}

bool
ForkJoinShared::executeFromMainThread(ThreadPoolWorker *worker)
{
    executePortion(&cx_->mainThread(), worker);
    return !abort_;
}

void
ForkJoinShared::executePortion(PerThreadData *perThread, ThreadPoolWorker *worker)
{
    // WARNING: This code runs ON THE PARALLEL WORKER THREAD.
    // Be careful when accessing cx_.

    // ForkJoinContext already contains an AutoAssertNoGC; however, the analysis
    // does not propagate this type information. We duplicate the assertion
    // here for maximum clarity.
    JS::AutoAssertNoGC nogc(runtime());

    Allocator *allocator = allocators_[worker->id()];
    ForkJoinContext cx(perThread, worker, allocator, this, &records_[worker->id()]);
    AutoSetForkJoinContext autoContext(&cx);

#ifdef DEBUG
    // Set the maximum worker and slice number for prettier spewing.
    cx.maxWorkerId = threadPool_->numWorkers();
#endif

    Spew(SpewOps, "Up");

    // Make a new IonContext for the slice, which is needed if we need to
    // re-enter the VM.
    IonContext icx(CompileRuntime::get(cx_->runtime()),
                   CompileCompartment::get(cx_->compartment()),
                   nullptr);

    JS_ASSERT(cx.bailoutRecord->topScript == nullptr);

    if (!fun_->nonLazyScript()->hasParallelIonScript()) {
        // Sometimes, particularly with GCZeal, the parallel ion
        // script can be collected between starting the parallel
        // op and reaching this point.  In that case, we just fail
        // and fallback.
        Spew(SpewOps, "Down (Script no longer present)");
        cx.bailoutRecord->setCause(ParallelBailoutMainScriptNotPresent);
        setAbortFlagAndRequestInterrupt(false);
    } else {
        ParallelIonInvoke<3> fii(cx_->runtime(), fun_, 3);

        fii.args[0] = Int32Value(worker->id());
        fii.args[1] = Int32Value(sliceStart_);
        fii.args[2] = Int32Value(sliceEnd_);

        bool ok = fii.invoke(perThread);
        JS_ASSERT(ok == !cx.bailoutRecord->topScript);
        if (!ok)
            setAbortFlagAndRequestInterrupt(false);
    }

    Spew(SpewOps, "Down");
}

void
ForkJoinShared::setAbortFlagDueToInterrupt(ForkJoinContext &cx)
{
    JS_ASSERT(cx_->runtime()->interruptPar);
    // The GC Needed flag should not be set during parallel
    // execution.  Instead, one of the requestGC() or
    // requestZoneGC() methods should be invoked.
    JS_ASSERT(!cx_->runtime()->gcIsNeeded);

    if (!abort_) {
        cx.bailoutRecord->setCause(ParallelBailoutInterrupt);
        setAbortFlagAndRequestInterrupt(false);
    }
}

void
ForkJoinShared::setAbortFlagAndRequestInterrupt(bool fatal)
{
    AutoLockMonitor lock(*this);

    abort_ = true;
    fatal_ = fatal_ || fatal;

    // Note: The ForkJoin trigger here avoids the expensive memory protection needed to
    // interrupt Ion code compiled for sequential execution.
    cx_->runtime()->requestInterrupt(JSRuntime::RequestInterruptAnyThreadForkJoin);
}

void
ForkJoinShared::requestGC(JS::gcreason::Reason reason)
{
    AutoLockMonitor lock(*this);

    gcZone_ = nullptr;
    gcReason_ = reason;
    gcRequested_ = true;
}

void
ForkJoinShared::requestZoneGC(JS::Zone *zone, JS::gcreason::Reason reason)
{
    AutoLockMonitor lock(*this);

    if (gcRequested_ && gcZone_ != zone) {
        // If a full GC has been requested, or a GC for another zone,
        // issue a request for a full GC.
        gcZone_ = nullptr;
        gcReason_ = reason;
        gcRequested_ = true;
    } else {
        // Otherwise, just GC this zone.
        gcZone_ = zone;
        gcReason_ = reason;
        gcRequested_ = true;
    }
}

/////////////////////////////////////////////////////////////////////////////
// ForkJoinContext
//

ForkJoinContext::ForkJoinContext(PerThreadData *perThreadData, ThreadPoolWorker *worker,
                                 Allocator *allocator, ForkJoinShared *shared,
                                 ParallelBailoutRecord *bailoutRecord)
  : ThreadSafeContext(shared->runtime(), perThreadData, Context_ForkJoin),
    bailoutRecord(bailoutRecord),
    targetRegionStart(nullptr),
    targetRegionEnd(nullptr),
    shared_(shared),
    worker_(worker),
    acquiredJSContext_(false),
    nogc_(shared->runtime())
{
    /*
     * Unsafely set the zone. This is used to track malloc counters and to
     * trigger GCs and is otherwise not thread-safe to access.
     */
    zone_ = shared->zone();

    /*
     * Unsafely set the compartment. This is used to get read-only access to
     * shared tables.
     */
    compartment_ = shared->compartment();

    allocator_ = allocator;
}

bool
ForkJoinContext::isMainThread() const
{
    return perThreadData == &shared_->runtime()->mainThread;
}

JSRuntime *
ForkJoinContext::runtime()
{
    return shared_->runtime();
}

JSContext *
ForkJoinContext::acquireJSContext()
{
    JSContext *cx = shared_->acquireJSContext();
    JS_ASSERT(!acquiredJSContext_);
    acquiredJSContext_ = true;
    return cx;
}

void
ForkJoinContext::releaseJSContext()
{
    JS_ASSERT(acquiredJSContext_);
    acquiredJSContext_ = false;
    return shared_->releaseJSContext();
}

bool
ForkJoinContext::hasAcquiredJSContext() const
{
    return acquiredJSContext_;
}

bool
ForkJoinContext::check()
{
    if (runtime()->interruptPar) {
        shared_->setAbortFlagDueToInterrupt(*this);
        return false;
    }
    return true;
}

void
ForkJoinContext::requestGC(JS::gcreason::Reason reason)
{
    shared_->requestGC(reason);
    bailoutRecord->setCause(ParallelBailoutRequestedGC);
    shared_->setAbortFlagAndRequestInterrupt(false);
}

void
ForkJoinContext::requestZoneGC(JS::Zone *zone, JS::gcreason::Reason reason)
{
    shared_->requestZoneGC(zone, reason);
    bailoutRecord->setCause(ParallelBailoutRequestedZoneGC);
    shared_->setAbortFlagAndRequestInterrupt(false);
}

bool
ForkJoinContext::setPendingAbortFatal(ParallelBailoutCause cause)
{
    shared_->setPendingAbortFatal();
    bailoutRecord->setCause(cause);
    return false;
}

//////////////////////////////////////////////////////////////////////////////
// ParallelBailoutRecord

void
js::ParallelBailoutRecord::init(JSContext *cx)
{
    reset(cx);
}

void
js::ParallelBailoutRecord::reset(JSContext *cx)
{
    topScript = nullptr;
    cause = ParallelBailoutNone;
    depth = 0;
}

void
js::ParallelBailoutRecord::setCause(ParallelBailoutCause cause,
                                    JSScript *outermostScript,
                                    JSScript *currentScript,
                                    jsbytecode *currentPc)
{
    this->cause = cause;
    updateCause(cause, outermostScript, currentScript, currentPc);
}

void
js::ParallelBailoutRecord::updateCause(ParallelBailoutCause cause,
                                       JSScript *outermostScript,
                                       JSScript *currentScript,
                                       jsbytecode *currentPc)
{
    JS_ASSERT_IF(outermostScript, currentScript);
    JS_ASSERT_IF(outermostScript, outermostScript->hasParallelIonScript());
    JS_ASSERT_IF(currentScript, outermostScript);
    JS_ASSERT_IF(!currentScript, !currentPc);

    if (this->cause == ParallelBailoutNone)
        this->cause = cause;

    if (outermostScript)
        this->topScript = outermostScript;

    if (currentScript)
        addTrace(currentScript, currentPc);
}

void
js::ParallelBailoutRecord::addTrace(JSScript *script,
                                    jsbytecode *pc)
{
    // Ideally, this should never occur, because we should always have
    // a script when we invoke setCause, but I havent' fully
    // refactored things to that point yet:
    if (topScript == nullptr && script != nullptr)
        topScript = script;

    if (depth < MaxDepth) {
        trace[depth].script = script;
        trace[depth].bytecode = pc;
        depth += 1;
    }
}

//////////////////////////////////////////////////////////////////////////////

//
// Debug spew
//

#ifdef DEBUG

static const char *
ExecutionStatusToString(ExecutionStatus status)
{
    switch (status) {
      case ExecutionFatal:
        return "fatal";
      case ExecutionSequential:
        return "sequential";
      case ExecutionWarmup:
        return "warmup";
      case ExecutionParallel:
        return "parallel";
    }
    return "(unknown status)";
}

static const char *
MethodStatusToString(MethodStatus status)
{
    switch (status) {
      case Method_Error:
        return "error";
      case Method_CantCompile:
        return "can't compile";
      case Method_Skipped:
        return "skipped";
      case Method_Compiled:
        return "compiled";
    }
    return "(unknown status)";
}

static unsigned
NumberOfDigits(unsigned n)
{
    if (n == 0)
        return 1;
    unsigned d = 0;
    while (n != 0) {
        d++;
        n /= 10;
    }
    return d;
}

static const size_t BufferSize = 4096;

class ParallelSpewer
{
    uint32_t depth;
    bool colorable;
    bool active[NumSpewChannels];

    const char *color(const char *colorCode) {
        if (!colorable)
            return "";
        return colorCode;
    }

    const char *reset() { return color("\x1b[0m"); }
    const char *bold() { return color("\x1b[1m"); }
    const char *red() { return color("\x1b[31m"); }
    const char *green() { return color("\x1b[32m"); }
    const char *yellow() { return color("\x1b[33m"); }
    const char *cyan() { return color("\x1b[36m"); }
    const char *workerColor(uint32_t id) {
        static const char *colors[] = {
            "\x1b[7m\x1b[31m", "\x1b[7m\x1b[32m", "\x1b[7m\x1b[33m",
            "\x1b[7m\x1b[34m", "\x1b[7m\x1b[35m", "\x1b[7m\x1b[36m",
            "\x1b[7m\x1b[37m",
            "\x1b[31m", "\x1b[32m", "\x1b[33m",
            "\x1b[34m", "\x1b[35m", "\x1b[36m",
            "\x1b[37m"
        };
        return color(colors[id % 14]);
    }

  public:
    ParallelSpewer()
      : depth(0)
    {
        const char *env;

        mozilla::PodArrayZero(active);
        env = getenv("PAFLAGS");
        if (env) {
            if (strstr(env, "ops"))
                active[SpewOps] = true;
            if (strstr(env, "compile"))
                active[SpewCompile] = true;
            if (strstr(env, "bailouts"))
                active[SpewBailouts] = true;
            if (strstr(env, "full")) {
                for (uint32_t i = 0; i < NumSpewChannels; i++)
                    active[i] = true;
            }
        }

        env = getenv("TERM");
        if (env && isatty(fileno(stderr))) {
            if (strcmp(env, "xterm-color") == 0 || strcmp(env, "xterm-256color") == 0)
                colorable = true;
        }
    }

    bool isActive(js::parallel::SpewChannel channel) {
        return active[channel];
    }

    void spewVA(js::parallel::SpewChannel channel, const char *fmt, va_list ap) {
        if (!active[channel])
            return;

        // Print into a buffer first so we use one fprintf, which usually
        // doesn't get interrupted when running with multiple threads.
        char buf[BufferSize];

        if (ForkJoinContext *cx = ForkJoinContext::current()) {
            // Print the format first into a buffer to right-justify the
            // worker ids.
            char bufbuf[BufferSize];
            JS_snprintf(bufbuf, BufferSize, "[%%sParallel:%%0%du%%s] ",
                        NumberOfDigits(cx->maxWorkerId));
            JS_snprintf(buf, BufferSize, bufbuf, workerColor(cx->workerId()),
                        cx->workerId(), reset());
        } else {
            JS_snprintf(buf, BufferSize, "[Parallel:M] ");
        }

        for (uint32_t i = 0; i < depth; i++)
            JS_snprintf(buf + strlen(buf), BufferSize, "  ");

        JS_vsnprintf(buf + strlen(buf), BufferSize, fmt, ap);
        JS_snprintf(buf + strlen(buf), BufferSize, "\n");

        fprintf(stderr, "%s", buf);
    }

    void spew(js::parallel::SpewChannel channel, const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        spewVA(channel, fmt, ap);
        va_end(ap);
    }

    void beginOp(JSContext *cx, const char *name) {
        if (!active[SpewOps])
            return;

        if (cx) {
            jsbytecode *pc;
            JSScript *script = cx->currentScript(&pc);
            if (script && pc) {
                NonBuiltinScriptFrameIter iter(cx);
                if (iter.done()) {
                    spew(SpewOps, "%sBEGIN %s%s (%s:%u)", bold(), name, reset(),
                         script->filename(), PCToLineNumber(script, pc));
                } else {
                    spew(SpewOps, "%sBEGIN %s%s (%s:%u -> %s:%u)", bold(), name, reset(),
                         iter.script()->filename(), PCToLineNumber(iter.script(), iter.pc()),
                         script->filename(), PCToLineNumber(script, pc));
                }
            } else {
                spew(SpewOps, "%sBEGIN %s%s", bold(), name, reset());
            }
        } else {
            spew(SpewOps, "%sBEGIN %s%s", bold(), name, reset());
        }

        depth++;
    }

    void endOp(ExecutionStatus status) {
        if (!active[SpewOps])
            return;

        JS_ASSERT(depth > 0);
        depth--;

        const char *statusColor;
        switch (status) {
          case ExecutionFatal:
            statusColor = red();
            break;
          case ExecutionSequential:
            statusColor = yellow();
            break;
          case ExecutionParallel:
            statusColor = green();
            break;
          default:
            statusColor = reset();
            break;
        }

        spew(SpewOps, "%sEND %s%s%s", bold(),
             statusColor, ExecutionStatusToString(status), reset());
    }

    void bailout(uint32_t count, HandleScript script,
                 jsbytecode *pc, ParallelBailoutCause cause) {
        if (!active[SpewOps])
            return;

        const char *filename = "";
        unsigned line=0, column=0;
        if (script) {
            line = PCToLineNumber(script, pc, &column);
            filename = script->filename();
        }

        spew(SpewOps, "%s%sBAILOUT %d%s: %d at %s:%d:%d", bold(), yellow(), count, reset(), cause, filename, line, column);
    }

    void beginCompile(HandleScript script) {
        if (!active[SpewCompile])
            return;

        spew(SpewCompile, "COMPILE %p:%s:%u", script.get(), script->filename(), script->lineno());
        depth++;
    }

    void endCompile(MethodStatus status) {
        if (!active[SpewCompile])
            return;

        JS_ASSERT(depth > 0);
        depth--;

        const char *statusColor;
        switch (status) {
          case Method_Error:
          case Method_CantCompile:
            statusColor = red();
            break;
          case Method_Skipped:
            statusColor = yellow();
            break;
          case Method_Compiled:
            statusColor = green();
            break;
          default:
            statusColor = reset();
            break;
        }

        spew(SpewCompile, "END %s%s%s", statusColor, MethodStatusToString(status), reset());
    }

    void spewMIR(MDefinition *mir, const char *fmt, va_list ap) {
        if (!active[SpewCompile])
            return;

        char buf[BufferSize];
        JS_vsnprintf(buf, BufferSize, fmt, ap);

        JSScript *script = mir->block()->info().script();
        spew(SpewCompile, "%s%s%s: %s (%s:%u)", cyan(), mir->opName(), reset(), buf,
             script->filename(), PCToLineNumber(script, mir->trackedPc()));
    }

    void spewBailoutIR(IonLIRTraceData *data) {
        if (!active[SpewBailouts])
            return;

        // If we didn't bail from a LIR/MIR but from a propagated parallel
        // bailout, don't bother printing anything since we've printed it
        // elsewhere.
        if (data->mirOpName && data->script) {
            spew(SpewBailouts, "%sBailout%s: %s / %s%s%s (block %d lir %d) (%s:%u)", yellow(), reset(),
                 data->lirOpName, cyan(), data->mirOpName, reset(),
                 data->blockIndex, data->lirIndex, data->script->filename(),
                 PCToLineNumber(data->script, data->pc));
        }
    }
};

// Singleton instance of the spewer.
static ParallelSpewer spewer;

bool
parallel::SpewEnabled(SpewChannel channel)
{
    return spewer.isActive(channel);
}

void
parallel::Spew(SpewChannel channel, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    spewer.spewVA(channel, fmt, ap);
    va_end(ap);
}

void
parallel::SpewBeginOp(JSContext *cx, const char *name)
{
    spewer.beginOp(cx, name);
}

ExecutionStatus
parallel::SpewEndOp(ExecutionStatus status)
{
    spewer.endOp(status);
    return status;
}

void
parallel::SpewBailout(uint32_t count, HandleScript script,
                      jsbytecode *pc, ParallelBailoutCause cause)
{
    spewer.bailout(count, script, pc, cause);
}

void
parallel::SpewBeginCompile(HandleScript script)
{
    spewer.beginCompile(script);
}

MethodStatus
parallel::SpewEndCompile(MethodStatus status)
{
    spewer.endCompile(status);
    return status;
}

void
parallel::SpewMIR(MDefinition *mir, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    spewer.spewMIR(mir, fmt, ap);
    va_end(ap);
}

void
parallel::SpewBailoutIR(IonLIRTraceData *data)
{
    spewer.spewBailoutIR(data);
}

#endif // DEBUG

bool
js::InExclusiveParallelSection()
{
    return InParallelSection() && ForkJoinContext::current()->hasAcquiredJSContext();
}

bool
js::ParallelTestsShouldPass(JSContext *cx)
{
    return jit::IsIonEnabled(cx) &&
           jit::IsBaselineEnabled(cx) &&
           !jit::js_JitOptions.eagerCompilation &&
           jit::js_JitOptions.baselineUsesBeforeCompile != 0 &&
           cx->runtime()->gcZeal() == 0;
}

void
js::RequestInterruptForForkJoin(JSRuntime *rt, JSRuntime::InterruptMode mode)
{
    if (mode != JSRuntime::RequestInterruptAnyThreadDontStopIon)
        rt->interruptPar = true;
}

bool
js::intrinsic_SetForkJoinTargetRegion(JSContext *cx, unsigned argc, Value *vp)
{
    // This version of SetForkJoinTargetRegion is called during
    // sequential execution. It is a no-op. The parallel version
    // is intrinsic_SetForkJoinTargetRegionPar(), below.
    return true;
}

static bool
intrinsic_SetForkJoinTargetRegionPar(ForkJoinContext *cx, unsigned argc, Value *vp)
{
    // Sets the *target region*, which is the portion of the output
    // buffer that the current iteration is permitted to write to.
    //
    // Note: it is important that the target region should be an
    // entire element (or several elements) of the output array and
    // not some region that spans from the middle of one element into
    // the middle of another. This is because the guarding code
    // assumes that handles, which never straddle across elements,
    // will either be contained entirely within the target region or
    // be contained entirely without of the region, and not straddling
    // the region nor encompassing it.

    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(argc == 3);
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedObject>());
    JS_ASSERT(args[1].isInt32());
    JS_ASSERT(args[2].isInt32());

    uint8_t *mem = args[0].toObject().as<TypedObject>().typedMem();
    int32_t start = args[1].toInt32();
    int32_t end = args[2].toInt32();

    cx->targetRegionStart = mem + start;
    cx->targetRegionEnd = mem + end;
    return true;
}

JS_JITINFO_NATIVE_PARALLEL(js::intrinsic_SetForkJoinTargetRegionInfo,
                           intrinsic_SetForkJoinTargetRegionPar);

bool
js::intrinsic_ClearThreadLocalArenas(JSContext *cx, unsigned argc, Value *vp)
{
    return true;
}

static bool
intrinsic_ClearThreadLocalArenasPar(ForkJoinContext *cx, unsigned argc, Value *vp)
{
    cx->allocator()->arenas.wipeDuringParallelExecution(cx->runtime());
    return true;
}

JS_JITINFO_NATIVE_PARALLEL(js::intrinsic_ClearThreadLocalArenasInfo,
                           intrinsic_ClearThreadLocalArenasPar);

#endif // JS_THREADSAFE && JS_ION

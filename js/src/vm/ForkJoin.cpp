/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
#include "jscompartment.h"

#include "vm/ForkJoin.h"
#include "vm/Monitor.h"
#include "gc/Marking.h"
#include "ion/BaselineJIT.h"

#ifdef JS_ION
#  include "ion/ParallelArrayAnalysis.h"
#endif

#ifdef JS_THREADSAFE
#  include "prthread.h"
#  include "prprf.h"
#endif

#if defined(DEBUG) && defined(JS_THREADSAFE) && defined(JS_ION)
#  include "ion/Ion.h"
#  include "ion/MIR.h"
#  include "ion/MIRGraph.h"
#  include "ion/IonCompartment.h"
#endif // DEBUG && THREADSAFE && ION

// For extracting stack extent for each thread.
#include "jsnativestack.h"

#include "jsinferinlines.h"

#include "vm/Interpreter-inl.h"

using namespace js;
using namespace js::parallel;
using namespace js::ion;

///////////////////////////////////////////////////////////////////////////
// Degenerate configurations
//
// When JS_THREADSAFE or JS_ION is not defined, we simply run the
// |func| callback sequentially.  We also forego the feedback
// altogether.

static bool
ExecuteSequentially(JSContext *cx_, HandleValue funVal, bool *complete);

#if !defined(JS_THREADSAFE) || !defined(JS_ION)
bool
js::ForkJoin(JSContext *cx, CallArgs &args)
{
    RootedValue argZero(cx, args[0]);
    bool complete = false; // since warmup is false, will always complete
    return ExecuteSequentially(cx, argZero, &complete);
}

uint32_t
js::ForkJoinSlices(JSContext *cx)
{
    return 1; // just the main thread
}

JSContext *
ForkJoinSlice::acquireContext()
{
    return NULL;
}

void
ForkJoinSlice::releaseContext()
{
}

bool
ForkJoinSlice::isMainThread()
{
    return true;
}

bool
ForkJoinSlice::InitializeTLS()
{
    return true;
}

JSRuntime *
ForkJoinSlice::runtime()
{
    JS_NOT_REACHED("Not THREADSAFE build");
}

bool
ForkJoinSlice::check()
{
    JS_NOT_REACHED("Not THREADSAFE build");
    return true;
}

void
ForkJoinSlice::requestGC(JS::gcreason::Reason reason)
{
    JS_NOT_REACHED("Not THREADSAFE build");
}

void
ForkJoinSlice::requestZoneGC(JS::Zone *zone, JS::gcreason::Reason reason)
{
    JS_NOT_REACHED("Not THREADSAFE build");
}

void
ParallelBailoutRecord::setCause(ParallelBailoutCause cause,
                                JSScript *outermostScript,
                                JSScript *currentScript,
                                jsbytecode *currentPc)
{
    JS_NOT_REACHED("Not THREADSAFE build");
}

void
ParallelBailoutRecord::addTrace(JSScript *script,
                                jsbytecode *pc)
{
    JS_NOT_REACHED("Not THREADSAFE build");
}

bool
js::ParallelTestsShouldPass(JSContext *cx)
{
    return false;
}

#endif // !JS_THREADSAFE || !JS_ION

///////////////////////////////////////////////////////////////////////////
// All configurations
//
// Some code that is shared between degenerate and parallel configurations.

static bool
ExecuteSequentially(JSContext *cx, HandleValue funVal, bool *complete)
{
    uint32_t numSlices = ForkJoinSlices(cx);
    FastInvokeGuard fig(cx, funVal);
    bool allComplete = true;
    for (uint32_t i = 0; i < numSlices; i++) {
        InvokeArgsGuard &args = fig.args();
        if (!args.pushed() && !cx->stack.pushInvokeArgs(cx, 3, &args))
            return false;
        args.setCallee(funVal);
        args.setThis(UndefinedValue());
        args[0].setInt32(i);
        args[1].setInt32(numSlices);
        args[2].setBoolean(!!cx->runtime()->parallelWarmup);
        if (!fig.invoke(cx))
            return false;
        allComplete = allComplete & args.rval().toBoolean();
    }
    *complete = allComplete;
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
    // ForkJoinMode() in ParallelArray.js

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

unsigned ForkJoinSlice::ThreadPrivateIndex;
bool ForkJoinSlice::TLSInitialized;

class ParallelDo
{
  public:
    // For tests, make sure to keep this in sync with minItemsTestingThreshold.
    const static uint32_t MAX_BAILOUTS = 3;
    uint32_t bailouts;

    // Information about the bailout:
    ParallelBailoutCause bailoutCause;
    RootedScript bailoutScript;
    jsbytecode *bailoutBytecode;

    ParallelDo(JSContext *cx, HandleObject fun, ForkJoinMode mode);
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
    HandleObject fun_;
    Vector<ParallelBailoutRecord, 16> bailoutRecords_;
    AutoScriptVector worklist_;
    Vector<WorklistData, 16> worklistData_;
    ForkJoinMode mode_;

    TrafficLight enqueueInitialScript(ExecutionStatus *status);
    TrafficLight compileForParallelExecution(ExecutionStatus *status);
    TrafficLight warmupExecution(bool stopIfComplete,
                                 ExecutionStatus *status);
    TrafficLight parallelExecution(ExecutionStatus *status);
    TrafficLight sequentialExecution(bool disqualified, ExecutionStatus *status);
    TrafficLight recoverFromBailout(ExecutionStatus *status);
    TrafficLight fatalError(ExecutionStatus *status);
    void determineBailoutCause();
    bool invalidateBailedOutScripts();
    ExecutionStatus sequentialExecution(bool disqualified);

    TrafficLight appendCallTargetsToWorklist(uint32_t index,
                                             ExecutionStatus *status);
    TrafficLight appendCallTargetToWorklist(HandleScript script,
                                            ExecutionStatus *status);
    bool addToWorklist(HandleScript script);
    inline bool hasScript(Vector<types::RecompileInfo> &scripts, JSScript *script);
}; // class ParallelDo

class ForkJoinShared : public TaskExecutor, public Monitor
{
    /////////////////////////////////////////////////////////////////////////
    // Constant fields

    JSContext *const cx_;          // Current context
    ThreadPool *const threadPool_; // The thread pool.
    HandleObject fun_;             // The JavaScript function to execute.
    const uint32_t numSlices_;     // Total number of threads.
    PRCondVar *rendezvousEnd_;     // Cond. var used to signal end of rendezvous.
    PRLock *cxLock_;               // Locks cx_ for parallel VM calls.
    ParallelBailoutRecord *const records_; // Bailout records for each slice

    /////////////////////////////////////////////////////////////////////////
    // Per-thread arenas
    //
    // Each worker thread gets an arena to use when allocating.

    Vector<Allocator *, 16> allocators_;

    /////////////////////////////////////////////////////////////////////////
    // Locked Fields
    //
    // Only to be accessed while holding the lock.

    uint32_t uncompleted_;          // Number of uncompleted worker threads
    uint32_t blocked_;              // Number of threads that have joined rendezvous
    uint32_t rendezvousIndex_;      // Number of rendezvous attempts
    bool gcRequested_;              // True if a worker requested a GC
    JS::gcreason::Reason gcReason_; // Reason given to request GC
    Zone *gcZone_;                  // Zone for GC, or NULL for full

    /////////////////////////////////////////////////////////////////////////
    // Asynchronous Flags
    //
    // These can be read without the lock (hence the |volatile| declaration).
    // All fields should be *written with the lock*, however.

    // Set to true when parallel execution should abort.
    volatile bool abort_;

    // Set to true when a worker bails for a fatal reason.
    volatile bool fatal_;

    // The main thread has requested a rendezvous.
    volatile bool rendezvous_;

    // Invoked only from the main thread:
    void executeFromMainThread();

    // Executes slice #threadId of the work, either from a worker or
    // the main thread.
    void executePortion(PerThreadData *perThread, uint32_t threadId);

    // Rendezvous protocol:
    //
    // Use AutoRendezvous rather than invoking initiateRendezvous() and
    // endRendezvous() directly.

    friend class AutoRendezvous;

    // Requests that the other threads stop.  Must be invoked from the main
    // thread.
    void initiateRendezvous(ForkJoinSlice &threadCx);

    // If a rendezvous has been requested, blocks until the main thread says
    // we may continue.
    void joinRendezvous(ForkJoinSlice &threadCx);

    // Permits other threads to resume execution.  Must be invoked from the
    // main thread after a call to initiateRendezvous().
    void endRendezvous(ForkJoinSlice &threadCx);

  public:
    ForkJoinShared(JSContext *cx,
                   ThreadPool *threadPool,
                   HandleObject fun,
                   uint32_t numSlices,
                   uint32_t uncompleted,
                   ParallelBailoutRecord *records);
    ~ForkJoinShared();

    bool init();

    ParallelResult execute();

    // Invoked from parallel worker threads:
    virtual void executeFromWorker(uint32_t threadId, uintptr_t stackLimit);

    // Moves all the per-thread arenas into the main compartment and
    // processes any pending requests for a GC.  This can only safely
    // be invoked on the main thread, either during a rendezvous or
    // after the workers have completed.
    void transferArenasToCompartmentAndProcessGCRequests();

    // Invoked during processing by worker threads to "check in".
    bool check(ForkJoinSlice &threadCx);

    // Requests a GC, either full or specific to a zone.
    void requestGC(JS::gcreason::Reason reason);
    void requestZoneGC(JS::Zone *zone, JS::gcreason::Reason reason);

    // Requests that computation abort.
    void setAbortFlag(bool fatal);

    JSRuntime *runtime() { return cx_->runtime(); }

    JSContext *acquireContext() { PR_Lock(cxLock_); return cx_; }
    void releaseContext() { PR_Unlock(cxLock_); }
};

class AutoEnterWarmup
{
    JSRuntime *runtime_;

  public:
    AutoEnterWarmup(JSRuntime *runtime) : runtime_(runtime) { runtime_->parallelWarmup++; }
    ~AutoEnterWarmup() { runtime_->parallelWarmup--; }
};

class AutoRendezvous
{
  private:
    ForkJoinSlice &threadCx;

  public:
    AutoRendezvous(ForkJoinSlice &threadCx)
        : threadCx(threadCx)
    {
        threadCx.shared->initiateRendezvous(threadCx);
    }

    ~AutoRendezvous() {
        threadCx.shared->endRendezvous(threadCx);
    }
};

class AutoSetForkJoinSlice
{
  public:
    AutoSetForkJoinSlice(ForkJoinSlice *threadCx) {
        PR_SetThreadPrivate(ForkJoinSlice::ThreadPrivateIndex, threadCx);
    }

    ~AutoSetForkJoinSlice() {
        PR_SetThreadPrivate(ForkJoinSlice::ThreadPrivateIndex, NULL);
    }
};

} // namespace js

///////////////////////////////////////////////////////////////////////////
// js::ForkJoin() and ParallelDo class
//
// These are the top-level objects that manage the parallel execution.
// They handle parallel compilation (if necessary), triggering
// parallel execution, and recovering from bailouts.

static const char *ForkJoinModeString(ForkJoinMode mode);

bool
js::ForkJoin(JSContext *cx, CallArgs &args)
{
    JS_ASSERT(args[0].isObject()); // else the self-hosted code is wrong
    JS_ASSERT(args[0].toObject().isFunction());

    ForkJoinMode mode = ForkJoinModeNormal;
    if (args.length() > 1) {
        JS_ASSERT(args[1].isInt32()); // else the self-hosted code is wrong
        JS_ASSERT(args[1].toInt32() < NumForkJoinModes);
        mode = (ForkJoinMode) args[1].toInt32();
    }

    RootedObject fun(cx, &args[0].toObject());
    ParallelDo op(cx, fun, mode);
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
    } else {
        return true;
    }
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

js::ParallelDo::ParallelDo(JSContext *cx,
                           HandleObject fun,
                           ForkJoinMode mode)
  : bailouts(0),
    bailoutCause(ParallelBailoutNone),
    bailoutScript(cx),
    bailoutBytecode(NULL),
    cx_(cx),
    fun_(fun),
    bailoutRecords_(cx),
    worklist_(cx),
    worklistData_(cx),
    mode_(mode)
{ }

ExecutionStatus
js::ParallelDo::apply()
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

    JS_ASSERT_IF(!ion::IsBaselineEnabled(cx_), !ion::IsEnabled(cx_));
    if (!ion::IsBaselineEnabled(cx_) || !ion::IsEnabled(cx_))
        return sequentialExecution(true);

    SpewBeginOp(cx_, "ParallelDo");

    uint32_t slices = ForkJoinSlices(cx_);

    if (!bailoutRecords_.resize(slices))
        return SpewEndOp(ExecutionFatal);

    for (uint32_t i = 0; i < slices; i++)
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
            return ExecutionFatal;
        }
        break;

      case NumForkJoinModes:
        JS_NOT_REACHED("Invalid mode");
    }

    while (bailouts < MAX_BAILOUTS) {
        for (uint32_t i = 0; i < slices; i++)
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

js::ParallelDo::TrafficLight
js::ParallelDo::enqueueInitialScript(ExecutionStatus *status)
{
    // GreenLight: script successfully enqueued if necessary
    // RedLight: fatal error or fell back to sequential

    // The kernel should be a self-hosted function.
    if (!fun_->isFunction())
        return sequentialExecution(true, status);

    RootedFunction callee(cx_, fun_->toFunction());

    if (!callee->isInterpreted() || !callee->isSelfHostedBuiltin())
        return sequentialExecution(true, status);

    // If the main script is already compiled, and we have no reason
    // to suspect any of its callees are not compiled, then we can
    // just skip the compilation step.
    RootedScript script(cx_, callee->getOrCreateScript(cx_));
    if (!script)
        return RedLight;
    if (script->hasParallelIonScript()) {
        if (!script->parallelIonScript()->hasUncompiledCallTarget()) {
            Spew(SpewOps, "Script %p:%s:%d already compiled, no uncompiled callees",
                 script.get(), script->filename(), script->lineno);
            return GreenLight;
        }

        Spew(SpewOps, "Script %p:%s:%d already compiled, may have uncompiled callees",
             script.get(), script->filename(), script->lineno);
    }

    // Otherwise, add to the worklist of scripts to process.
    if (addToWorklist(script) == RedLight)
        return fatalError(status);
    return GreenLight;
}

js::ParallelDo::TrafficLight
js::ParallelDo::compileForParallelExecution(ExecutionStatus *status)
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
            fun = script->function();

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
                         script.get(), script->filename(), script->lineno,
                         previousUseCount, currentUseCount);
                } else {
                    uint32_t stallCount = ++worklistData_[i].stallCount;
                    if (stallCount < stallThreshold) {
                        gatheringTypeInformation = true;
                    }

                    Spew(SpewCompile,
                         "Script %p:%s:%d has no baseline script, "
                         "and use count has %u stalls at %d",
                         script.get(), script->filename(), script->lineno,
                         stallCount, previousUseCount);
                }
                continue;
            }

            if (!script->hasParallelIonScript()) {
                // Script has not yet been compiled. Attempt to compile it.
                SpewBeginCompile(script);
                MethodStatus mstatus = ion::CanEnterInParallel(cx_, script);
                SpewEndCompile(mstatus);

                switch (mstatus) {
                  case Method_Error:
                    return fatalError(status);

                  case Method_CantCompile:
                    Spew(SpewCompile,
                         "Script %p:%s:%d cannot be compiled, "
                         "falling back to sequential execution",
                         script.get(), script->filename(), script->lineno);
                    return sequentialExecution(true, status);

                  case Method_Skipped:
                    // A "skipped" result either means that we are compiling
                    // in parallel OR some other transient error occurred.
                    if (script->isParallelIonCompilingOffThread()) {
                        Spew(SpewCompile,
                             "Script %p:%s:%d compiling off-thread",
                             script.get(), script->filename(), script->lineno);
                        offMainThreadCompilationsInProgress = true;
                        continue;
                    }
                    return sequentialExecution(false, status);

                  case Method_Compiled:
                    Spew(SpewCompile,
                         "Script %p:%s:%d compiled",
                         script.get(), script->filename(), script->lineno);
                    JS_ASSERT(script->hasParallelIonScript());
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
                         script.get(), script->filename(), script->lineno);
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

js::ParallelDo::TrafficLight
js::ParallelDo::appendCallTargetsToWorklist(uint32_t index,
                                            ExecutionStatus *status)
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
                       target->filename(), target->lineno);
        if (appendCallTargetToWorklist(target, status) == RedLight)
            return RedLight;
    }

    return GreenLight;
}

js::ParallelDo::TrafficLight
js::ParallelDo::appendCallTargetToWorklist(HandleScript script,
                                           ExecutionStatus *status)
{
    // GreenLight: call target appended if necessary
    // RedLight: fatal error or completed work via warmups or fallback

    JS_ASSERT(script);

    // Fallback to sequential if disabled.
    if (!script->canParallelIonCompile()) {
        Spew(SpewCompile, "Skipping %p:%s:%u, canParallelIonCompile() is false",
             script.get(), script->filename(), script->lineno);
        return sequentialExecution(true, status);
    }

    if (script->hasParallelIonScript()) {
        // Skip if the code is expected to result in a bailout.
        if (script->parallelIonScript()->bailoutExpected()) {
            Spew(SpewCompile, "Skipping %p:%s:%u, bailout expected",
                 script.get(), script->filename(), script->lineno);
            return sequentialExecution(false, status);
        }
    }

    if (!addToWorklist(script))
        return fatalError(status);

    return GreenLight;
}

bool
js::ParallelDo::addToWorklist(HandleScript script)
{
    for (uint32_t i = 0; i < worklist_.length(); i++) {
        if (worklist_[i] == script) {
            Spew(SpewCompile, "Skipping %p:%s:%u, already in worklist",
                 script.get(), script->filename(), script->lineno);
            return true;
        }
    }

    Spew(SpewCompile, "Enqueued %p:%s:%u",
         script.get(), script->filename(), script->lineno);

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

js::ParallelDo::TrafficLight
js::ParallelDo::sequentialExecution(bool disqualified, ExecutionStatus *status)
{
    // RedLight: fatal error or completed work

    *status = sequentialExecution(disqualified);
    return RedLight;
}

ExecutionStatus
js::ParallelDo::sequentialExecution(bool disqualified)
{
    // XXX use disqualified to set parallelIon to ION_DISABLED_SCRIPT?

    Spew(SpewOps, "Executing sequential execution (disqualified=%d).",
         disqualified);

    bool complete = false;
    RootedValue funVal(cx_, ObjectValue(*fun_));
    if (!ExecuteSequentially(cx_, funVal, &complete))
        return ExecutionFatal;

    // When invoked without the warmup flag set to true, the kernel
    // function OUGHT to complete successfully, barring an exception.
    JS_ASSERT(complete);
    return ExecutionSequential;
}

js::ParallelDo::TrafficLight
js::ParallelDo::fatalError(ExecutionStatus *status)
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
        return "at runtime, the behavior changed, invalidating compiled code (IC update)";
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
      case ParallelBailoutUnsupportedStringComparison:
        return "unsupported string comparison";
      case ParallelBailoutUnsupportedSparseArray:
        return "unsupported sparse array";
      case ParallelBailoutRequestedGC:
        return "requested GC";
      case ParallelBailoutRequestedZoneGC:
        return "requested zone GC";
      default:
        return "no known reason";
    }
}

void
js::ParallelDo::determineBailoutCause()
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
js::ParallelDo::invalidateBailedOutScripts()
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
             script.get(), script->filename(), script->lineno);

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
             script.get(), script->filename(), script->lineno,
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

js::ParallelDo::TrafficLight
js::ParallelDo::warmupExecution(bool stopIfComplete,
                                ExecutionStatus *status)
{
    // GreenLight: warmup succeeded, still more work to do
    // RedLight: fatal error or warmup completed all work (check status)

    Spew(SpewOps, "Executing warmup.");

    AutoEnterWarmup warmup(cx_->runtime());
    RootedValue funVal(cx_, ObjectValue(*fun_));
    bool complete;
    if (!ExecuteSequentially(cx_, funVal, &complete)) {
        *status = ExecutionFatal;
        return RedLight;
    }

    if (complete && stopIfComplete) {
        Spew(SpewOps, "Warmup execution finished all the work.");
        *status = ExecutionWarmup;
        return RedLight;
    }

    return GreenLight;
}

class AutoEnterParallelSection
{
  private:
    JSContext *cx_;
    uint8_t *prevIonTop_;

  public:
    AutoEnterParallelSection(JSContext *cx)
      : cx_(cx),
        prevIonTop_(cx->mainThread().ionTop)
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
    }

    ~AutoEnterParallelSection() {
        cx_->mainThread().ionTop = prevIonTop_;
    }
};

js::ParallelDo::TrafficLight
js::ParallelDo::parallelExecution(ExecutionStatus *status)
{
    // GreenLight: bailout occurred, keep trying
    // RedLight: fatal error or all work completed

    // Recursive use of the ThreadPool is not supported.  Right now we
    // cannot get here because parallel code cannot invoke native
    // functions such as ForkJoin().
    JS_ASSERT(ForkJoinSlice::Current() == NULL);

    AutoEnterParallelSection enter(cx_);

    ThreadPool *threadPool = &cx_->runtime()->threadPool;
    uint32_t numSlices = ForkJoinSlices(cx_);

    RootedObject rootedFun(cx_, fun_);
    ForkJoinShared shared(cx_, threadPool, rootedFun, numSlices, numSlices - 1,
                          &bailoutRecords_[0]);
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

js::ParallelDo::TrafficLight
js::ParallelDo::recoverFromBailout(ExecutionStatus *status)
{
    // GreenLight: bailout recovered, try to compile-and-run again
    // RedLight: fatal error

    bailouts += 1;
    determineBailoutCause();

    SpewBailout(bailouts, bailoutScript, bailoutBytecode, bailoutCause);

    // After any bailout, we always scan over callee list of main
    // function, if nothing else
    RootedScript mainScript(cx_, fun_->toFunction()->nonLazyScript());
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
js::ParallelDo::hasScript(Vector<types::RecompileInfo> &scripts, JSScript *script)
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
    EnterIonCode enter_;
    void *jitcode_;
    void *calleeToken_;
    Value argv_[maxArgc + 2];
    uint32_t argc_;

  public:
    Value *args;

    ParallelIonInvoke(JSCompartment *compartment,
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
        IonCode *code = ion->method();
        jitcode_ = code->raw();
        enter_ = compartment->ionCompartment()->enterJIT();
        calleeToken_ = CalleeToParallelToken(callee);
    }

    bool invoke(PerThreadData *perThread) {
        RootedValue result(perThread);
        enter_(jitcode_, argc_ + 1, argv_ + 1, NULL, calleeToken_, NULL, 0, result.address());
        return !result.isMagic();
    }
};

/////////////////////////////////////////////////////////////////////////////
// ForkJoinShared
//

ForkJoinShared::ForkJoinShared(JSContext *cx,
                               ThreadPool *threadPool,
                               HandleObject fun,
                               uint32_t numSlices,
                               uint32_t uncompleted,
                               ParallelBailoutRecord *records)
  : cx_(cx),
    threadPool_(threadPool),
    fun_(fun),
    numSlices_(numSlices),
    rendezvousEnd_(NULL),
    cxLock_(NULL),
    records_(records),
    allocators_(cx),
    uncompleted_(uncompleted),
    blocked_(0),
    rendezvousIndex_(0),
    gcRequested_(false),
    gcReason_(JS::gcreason::NUM_REASONS),
    gcZone_(NULL),
    abort_(false),
    fatal_(false),
    rendezvous_(false)
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

    rendezvousEnd_ = PR_NewCondVar(lock_);
    if (!rendezvousEnd_)
        return false;

    cxLock_ = PR_NewLock();
    if (!cxLock_)
        return false;

    for (unsigned i = 0; i < numSlices_; i++) {
        Allocator *allocator = cx_->runtime()->new_<Allocator>(cx_->zone());
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
    if (rendezvousEnd_)
        PR_DestroyCondVar(rendezvousEnd_);

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
    if (cx_->runtime()->interrupt)
        return TP_RETRY_SEQUENTIALLY;

    AutoLockMonitor lock(*this);

    // Notify workers to start and execute one portion on this thread.
    {
        AutoUnlockMonitor unlock(*this);
        if (!threadPool_->submitAll(cx_, this))
            return TP_FATAL;
        executeFromMainThread();
    }

    // Wait for workers to complete.
    while (uncompleted_ > 0)
        lock.wait();

    transferArenasToCompartmentAndProcessGCRequests();

    // Check if any of the workers failed.
    if (abort_) {
        if (fatal_)
            return TP_FATAL;
        else
            return TP_RETRY_SEQUENTIALLY;
    }

    // Everything went swimmingly. Give yourself a pat on the back.
    return TP_SUCCESS;
}

void
ForkJoinShared::transferArenasToCompartmentAndProcessGCRequests()
{
    JSCompartment *comp = cx_->compartment();
    for (unsigned i = 0; i < numSlices_; i++)
        comp->adoptWorkerAllocator(allocators_[i]);

    if (gcRequested_) {
        if (!gcZone_)
            TriggerGC(cx_->runtime(), gcReason_);
        else
            TriggerZoneGC(gcZone_, gcReason_);
        gcRequested_ = false;
        gcZone_ = NULL;
    }
}

void
ForkJoinShared::executeFromWorker(uint32_t workerId, uintptr_t stackLimit)
{
    JS_ASSERT(workerId < numSlices_ - 1);

    PerThreadData thisThread(cx_->runtime());
    TlsPerThreadData.set(&thisThread);
    // Don't use setIonStackLimit() because that acquires the ionStackLimitLock, and the
    // lock has not been initialized in these cases.
    thisThread.ionStackLimit = stackLimit;
    executePortion(&thisThread, workerId);
    TlsPerThreadData.set(NULL);

    AutoLockMonitor lock(*this);
    uncompleted_ -= 1;
    if (blocked_ == uncompleted_) {
        // Signal the main thread that we have terminated.  It will be either
        // working, arranging a rendezvous, or waiting for workers to
        // complete.
        lock.notify();
    }
}

void
ForkJoinShared::executeFromMainThread()
{
    executePortion(&cx_->mainThread(), numSlices_ - 1);
}

void
ForkJoinShared::executePortion(PerThreadData *perThread,
                               uint32_t threadId)
{
    // WARNING: This code runs ON THE PARALLEL WORKER THREAD.
    // Therefore, it should NOT access `cx_` in any way!

    Allocator *allocator = allocators_[threadId];
    ForkJoinSlice slice(perThread, threadId, numSlices_, allocator,
                        this, &records_[threadId]);
    AutoSetForkJoinSlice autoContext(&slice);

    Spew(SpewOps, "Up");

    // Make a new IonContext for the slice, which is needed if we need to
    // re-enter the VM.
    IonContext icx(cx_->compartment(), NULL);

    JS_ASSERT(slice.bailoutRecord->topScript == NULL);

    RootedObject fun(perThread, fun_);
    JS_ASSERT(fun->isFunction());
    RootedFunction callee(perThread, fun->toFunction());
    if (!callee->nonLazyScript()->hasParallelIonScript()) {
        // Sometimes, particularly with GCZeal, the parallel ion
        // script can be collected between starting the parallel
        // op and reaching this point.  In that case, we just fail
        // and fallback.
        Spew(SpewOps, "Down (Script no longer present)");
        slice.bailoutRecord->setCause(ParallelBailoutMainScriptNotPresent,
                                      NULL, NULL, NULL);
        setAbortFlag(false);
    } else {
        ParallelIonInvoke<3> fii(cx_->compartment(), callee, 3);

        fii.args[0] = Int32Value(slice.sliceId);
        fii.args[1] = Int32Value(slice.numSlices);
        fii.args[2] = BooleanValue(false);

        bool ok = fii.invoke(perThread);
        JS_ASSERT(ok == !slice.bailoutRecord->topScript);
        if (!ok)
            setAbortFlag(false);
    }

    Spew(SpewOps, "Down");
}

bool
ForkJoinShared::check(ForkJoinSlice &slice)
{
    JS_ASSERT(cx_->runtime()->interrupt);

    if (abort_)
        return false;

    if (slice.isMainThread()) {
        JS_ASSERT(!cx_->runtime()->gcIsNeeded);

        if (cx_->runtime()->interrupt) {
            // The GC Needed flag should not be set during parallel
            // execution.  Instead, one of the requestGC() or
            // requestZoneGC() methods should be invoked.
            JS_ASSERT(!cx_->runtime()->gcIsNeeded);

            // If interrupt is requested, bring worker threads to a halt,
            // service the interrupt, then let them start back up again.
            // AutoRendezvous autoRendezvous(slice);
            // if (!js_HandleExecutionInterrupt(cx_))
            //     return setAbortFlag(true);
            slice.bailoutRecord->setCause(ParallelBailoutInterrupt,
                                          NULL, NULL, NULL);
            setAbortFlag(false);
            return false;
        }
    } else if (rendezvous_) {
        joinRendezvous(slice);
    }

    return true;
}

void
ForkJoinShared::initiateRendezvous(ForkJoinSlice &slice)
{
    // The rendezvous protocol is always initiated by the main thread.  The
    // main thread sets the rendezvous flag to true.  Seeing this flag, other
    // threads will invoke |joinRendezvous()|, which causes them to (1) read
    // |rendezvousIndex| and (2) increment the |blocked| counter.  Once the
    // |blocked| counter is equal to |uncompleted|, all parallel threads have
    // joined the rendezvous, and so the main thread is signaled.  That will
    // cause this function to return.
    //
    // Some subtle points:
    //
    // - Worker threads may potentially terminate their work before they see
    //   the rendezvous flag.  In this case, they would decrement
    //   |uncompleted| rather than incrementing |blocked|.  Either way, if the
    //   two variables become equal, the main thread will be notified
    //
    // - The |rendezvousIndex| counter is used to detect the case where the
    //   main thread signals the end of the rendezvous and then starts another
    //   rendezvous before the workers have a chance to exit.  We circumvent
    //   this by having the workers read the |rendezvousIndex| counter as they
    //   enter the rendezvous, and then they only block until that counter is
    //   incremented.  Another alternative would be for the main thread to
    //   block in |endRendezvous()| until all workers have exited, but that
    //   would be slower and involve unnecessary synchronization.
    //
    //   Note that the main thread cannot ever get more than one rendezvous
    //   ahead of the workers, because it must wait for all of them to enter
    //   the rendezvous before it can end it, so the solution of using a
    //   counter is perfectly general and we need not fear rollover.

    JS_ASSERT(slice.isMainThread());
    JS_ASSERT(!rendezvous_ && blocked_ == 0);
    JS_ASSERT(cx_->runtime()->interrupt);

    AutoLockMonitor lock(*this);

    // Signal other threads we want to start a rendezvous.
    rendezvous_ = true;

    // Wait until all the other threads blocked themselves.
    while (blocked_ != uncompleted_)
        lock.wait();
}

void
ForkJoinShared::joinRendezvous(ForkJoinSlice &slice)
{
    JS_ASSERT(!slice.isMainThread());
    JS_ASSERT(rendezvous_);

    AutoLockMonitor lock(*this);
    const uint32_t index = rendezvousIndex_;
    blocked_ += 1;

    // If we're the last to arrive, let the main thread know about it.
    if (blocked_ == uncompleted_)
        lock.notify();

    // Wait until the main thread terminates the rendezvous.  We use a
    // separate condition variable here to distinguish between workers
    // notifying the main thread that they have completed and the main
    // thread notifying the workers to resume.
    while (rendezvousIndex_ == index)
        PR_WaitCondVar(rendezvousEnd_, PR_INTERVAL_NO_TIMEOUT);
}

void
ForkJoinShared::endRendezvous(ForkJoinSlice &slice)
{
    JS_ASSERT(slice.isMainThread());

    AutoLockMonitor lock(*this);
    rendezvous_ = false;
    blocked_ = 0;
    rendezvousIndex_++;

    // Signal other threads that rendezvous is over.
    PR_NotifyAllCondVar(rendezvousEnd_);
}

void
ForkJoinShared::setAbortFlag(bool fatal)
{
    AutoLockMonitor lock(*this);

    abort_ = true;
    fatal_ = fatal_ || fatal;

    cx_->runtime()->triggerOperationCallback();
}

void
ForkJoinShared::requestGC(JS::gcreason::Reason reason)
{
    AutoLockMonitor lock(*this);

    gcZone_ = NULL;
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
        gcZone_ = NULL;
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
// ForkJoinSlice
//

ForkJoinSlice::ForkJoinSlice(PerThreadData *perThreadData,
                             uint32_t sliceId, uint32_t numSlices,
                             Allocator *allocator, ForkJoinShared *shared,
                             ParallelBailoutRecord *bailoutRecord)
    : perThreadData(perThreadData),
      sliceId(sliceId),
      numSlices(numSlices),
      allocator(allocator),
      bailoutRecord(bailoutRecord),
      shared(shared)
{ }

bool
ForkJoinSlice::isMainThread()
{
    return perThreadData == &shared->runtime()->mainThread;
}

JSRuntime *
ForkJoinSlice::runtime()
{
    return shared->runtime();
}

JSContext *
ForkJoinSlice::acquireContext()
{
    return shared->acquireContext();
}

void
ForkJoinSlice::releaseContext()
{
    return shared->releaseContext();
}

bool
ForkJoinSlice::check()
{
    if (runtime()->interrupt)
        return shared->check(*this);
    else
        return true;
}

bool
ForkJoinSlice::InitializeTLS()
{
    if (!TLSInitialized) {
        TLSInitialized = true;
        PRStatus status = PR_NewThreadPrivateIndex(&ThreadPrivateIndex, NULL);
        return status == PR_SUCCESS;
    }
    return true;
}

void
ForkJoinSlice::requestGC(JS::gcreason::Reason reason)
{
    shared->requestGC(reason);
    bailoutRecord->setCause(ParallelBailoutRequestedGC,
                            NULL, NULL, NULL);
    shared->setAbortFlag(false);
}

void
ForkJoinSlice::requestZoneGC(JS::Zone *zone, JS::gcreason::Reason reason)
{
    shared->requestZoneGC(zone, reason);
    bailoutRecord->setCause(ParallelBailoutRequestedZoneGC,
                            NULL, NULL, NULL);
    shared->setAbortFlag(false);
}

/////////////////////////////////////////////////////////////////////////////

uint32_t
js::ForkJoinSlices(JSContext *cx)
{
    // Parallel workers plus this main thread.
    return cx->runtime()->threadPool.numWorkers() + 1;
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
    topScript = NULL;
    cause = ParallelBailoutNone;
    depth = 0;
}

void
js::ParallelBailoutRecord::setCause(ParallelBailoutCause cause,
                                    JSScript *outermostScript,
                                    JSScript *currentScript,
                                    jsbytecode *currentPc)
{
    JS_ASSERT_IF(outermostScript, currentScript);
    JS_ASSERT_IF(outermostScript, outermostScript->hasParallelIonScript());
    JS_ASSERT_IF(currentScript, outermostScript);
    JS_ASSERT_IF(!currentScript, !currentPc);

    this->cause = cause;

    if (outermostScript) {
        this->topScript = outermostScript;
    }

    if (currentScript) {
        addTrace(currentScript, currentPc);
    }
}

void
js::ParallelBailoutRecord::addTrace(JSScript *script,
                                    jsbytecode *pc)
{
    // Ideally, this should never occur, because we should always have
    // a script when we invoke setCause, but I havent' fully
    // refactored things to that point yet:
    if (topScript == NULL && script != NULL)
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
    const char *sliceColor(uint32_t id) {
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
        if (env) {
            if (strcmp(env, "xterm-color") == 0 || strcmp(env, "xterm-256color") == 0)
                colorable = true;
        }
    }

    bool isActive(SpewChannel channel) {
        return active[channel];
    }

    void spewVA(SpewChannel channel, const char *fmt, va_list ap) {
        if (!active[channel])
            return;

        // Print into a buffer first so we use one fprintf, which usually
        // doesn't get interrupted when running with multiple threads.
        char buf[BufferSize];

        if (ForkJoinSlice *slice = ForkJoinSlice::Current()) {
            PR_snprintf(buf, BufferSize, "[%sParallel:%u%s] ",
                        sliceColor(slice->sliceId), slice->sliceId, reset());
        } else {
            PR_snprintf(buf, BufferSize, "[Parallel:M] ");
        }

        for (uint32_t i = 0; i < depth; i++)
            PR_snprintf(buf + strlen(buf), BufferSize, "  ");

        PR_vsnprintf(buf + strlen(buf), BufferSize, fmt, ap);
        PR_snprintf(buf + strlen(buf), BufferSize, "\n");

        fprintf(stderr, "%s", buf);
    }

    void spew(SpewChannel channel, const char *fmt, ...) {
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
            JSScript *script = cx->stack.currentScript(&pc);
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

        spew(SpewCompile, "COMPILE %p:%s:%u", script.get(), script->filename(), script->lineno);
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
        PR_vsnprintf(buf, BufferSize, fmt, ap);

        JSScript *script = mir->block()->info().script();
        spew(SpewCompile, "%s%s%s: %s (%s:%u)", cyan(), mir->opName(), reset(), buf,
             script->filename(), PCToLineNumber(script, mir->trackedPc()));
    }

    void spewBailoutIR(uint32_t bblockId, uint32_t lirId,
                       const char *lir, const char *mir, JSScript *script, jsbytecode *pc) {
        if (!active[SpewBailouts])
            return;

        // If we didn't bail from a LIR/MIR but from a propagated parallel
        // bailout, don't bother printing anything since we've printed it
        // elsewhere.
        if (mir && script) {
            spew(SpewBailouts, "%sBailout%s: %s / %s%s%s (block %d lir %d) (%s:%u)", yellow(), reset(),
                 lir, cyan(), mir, reset(),
                 bblockId, lirId,
                 script->filename(), PCToLineNumber(script, pc));
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
parallel::SpewBailoutIR(uint32_t bblockId, uint32_t lirId,
                        const char *lir, const char *mir,
                        JSScript *script, jsbytecode *pc)
{
    spewer.spewBailoutIR(bblockId, lirId, lir, mir, script, pc);
}

#endif // DEBUG

bool
js::ParallelTestsShouldPass(JSContext *cx)
{
    return ion::IsEnabled(cx) &&
           ion::IsBaselineEnabled(cx) &&
           !ion::js_IonOptions.eagerCompilation &&
           ion::js_IonOptions.baselineUsesBeforeCompile != 0 &&
           cx->runtime()->gcZeal() == 0;
}

#endif // JS_THREADSAFE && JS_ION

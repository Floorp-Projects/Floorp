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
#include "jsinterpinlines.h"

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
ExecuteSequentially(JSContext *cx_, HandleValue funVal);

#if !defined(JS_THREADSAFE) || !defined(JS_ION)
bool
js::ForkJoin(JSContext *cx, CallArgs &args)
{
    RootedValue argZero(cx, args[0]);
    return ExecuteSequentially(cx, argZero);
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

#endif // !JS_THREADSAFE || !JS_ION

///////////////////////////////////////////////////////////////////////////
// All configurations
//
// Some code that is shared between degenerate and parallel configurations.

static bool
ExecuteSequentially(JSContext *cx, HandleValue funVal)
{
    uint32_t numSlices = ForkJoinSlices(cx);
    FastInvokeGuard fig(cx, funVal);
    for (uint32_t i = 0; i < numSlices; i++) {
        InvokeArgsGuard &args = fig.args();
        if (!args.pushed() && !cx->stack.pushInvokeArgs(cx, 3, &args))
            return false;
        args.setCallee(funVal);
        args.setThis(UndefinedValue());
        args[0].setInt32(i);
        args[1].setInt32(numSlices);
        args[2].setBoolean(!!cx->runtime->parallelWarmup);
        if (!fig.invoke(cx))
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

    ParallelDo(JSContext *cx, HandleObject fun);
    ExecutionStatus apply();

  private:
    JSContext *cx_;
    HeapPtrObject fun_;
    Vector<ParallelBailoutRecord, 16> bailoutRecords;

    inline bool executeSequentially();

    MethodStatus compileForParallelExecution();
    ExecutionStatus disqualifyFromParallelExecution();
    void determineBailoutCause();
    bool invalidateBailedOutScripts();
    bool warmupForParallelExecution();
    ParallelResult executeInParallel();
    inline static bool hasScript(Vector<types::RecompileInfo> &scripts,
                                 JSScript *script);
    inline bool hasNoPendingInvalidations();
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

    JSRuntime *runtime() { return cx_->runtime; }

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

bool
js::ForkJoin(JSContext *cx, CallArgs &args)
{
    JS_ASSERT(args[0].isObject());
    JS_ASSERT(args[0].toObject().isFunction());

    RootedObject fun(cx, &args[0].toObject());
    ParallelDo op(cx, fun);
    ExecutionStatus status = op.apply();
    if (status == ExecutionFatal)
        return false;

    if (args[1].isObject()) {
        RootedObject feedback(cx, &args[1].toObject());
        if (feedback && feedback->isFunction()) {
            InvokeArgsGuard feedbackArgs;
            if (!cx->stack.pushInvokeArgs(cx, 3, &feedbackArgs))
                return false;

            const char *resultString;
            switch (status) {
              case ExecutionParallel:
                resultString = (op.bailouts == 0 ? "success" : "bailout");
                break;

              case ExecutionFatal:
              case ExecutionSequential:
                resultString = "disqualified";
                break;
            }
            feedbackArgs.setCallee(ObjectValue(*feedback));
            feedbackArgs.setThis(UndefinedValue());
            feedbackArgs[0].setString(JS_NewStringCopyZ(cx, resultString));
            feedbackArgs[1].setInt32(op.bailouts);
            feedbackArgs[2].setInt32(op.bailoutCause);
            if (!Invoke(cx, feedbackArgs))
                return false;
        }
    }

    return true;
}

js::ParallelDo::ParallelDo(JSContext *cx, HandleObject fun)
  : bailouts(0),
    bailoutCause(ParallelBailoutNone),
    bailoutScript(cx),
    bailoutBytecode(NULL),
    cx_(cx),
    fun_(fun),
    bailoutRecords(cx)
{ }

ExecutionStatus
js::ParallelDo::apply()
{
    SpewBeginOp(cx_, "ParallelDo");

    uint32_t slices = ForkJoinSlices(cx_);

    if (!ion::IsEnabled(cx_))
        return SpewEndOp(disqualifyFromParallelExecution());

    if (!bailoutRecords.resize(slices))
        return SpewEndOp(ExecutionFatal);

    for (uint32_t i = 0; i < slices; i++)
        bailoutRecords[i].init(cx_);

    // Try to execute in parallel.  If a bailout occurs, re-warmup
    // and then try again.  Repeat this a few times.
    while (bailouts < MAX_BAILOUTS) {
        for (uint32_t i = 0; i < slices; i++)
            bailoutRecords[i].reset(cx_);

        MethodStatus status = compileForParallelExecution();
        if (status == Method_Error)
            return SpewEndOp(ExecutionFatal);
        if (status != Method_Compiled)
            return SpewEndOp(disqualifyFromParallelExecution());

        ParallelResult result = executeInParallel();
        switch (result) {
          case TP_RETRY_AFTER_GC:
            Spew(SpewBailouts, "Bailout due to GC request");
            break;

          case TP_RETRY_SEQUENTIALLY:
            Spew(SpewBailouts, "Bailout not categorized");
            break;

          case TP_SUCCESS:
            return SpewEndOp(ExecutionParallel);

          case TP_FATAL:
            return SpewEndOp(ExecutionFatal);
        }

        bailouts += 1;
        determineBailoutCause();

        SpewBailout(bailouts, bailoutScript, bailoutBytecode, bailoutCause);

        if (!invalidateBailedOutScripts())
            return SpewEndOp(ExecutionFatal);

        if (!warmupForParallelExecution())
            return SpewEndOp(ExecutionFatal);
    }

    // After enough tries, just execute sequentially.
    return SpewEndOp(disqualifyFromParallelExecution());
}

bool
js::ParallelDo::executeSequentially()
{
    RootedValue funVal(cx_, ObjectValue(*fun_));
    return ExecuteSequentially(cx_, funVal);
}

MethodStatus
js::ParallelDo::compileForParallelExecution()
{
    // The kernel should be a self-hosted function.
    if (!fun_->isFunction())
        return Method_Skipped;

    RootedFunction callee(cx_, fun_->toFunction());

    if (!callee->isInterpreted() || !callee->isSelfHostedBuiltin())
        return Method_Skipped;

    if (callee->isInterpretedLazy() && !callee->initializeLazyScript(cx_))
        return Method_Error;

    // If this function has not been run enough to enable parallel
    // execution, perform a warmup.
    RootedScript script(cx_, callee->nonLazyScript());
    if (script->getUseCount() < js_IonOptions.usesBeforeCompileParallel) {
        if (!warmupForParallelExecution())
            return Method_Error;
    }

    if (script->hasParallelIonScript() &&
        !script->parallelIonScript()->hasInvalidatedCallTarget())
    {
        Spew(SpewOps, "Already compiled");
        return Method_Compiled;
    }

    Spew(SpewOps, "Compiling all reachable functions");

    ParallelCompileContext compileContext(cx_);
    if (!compileContext.appendToWorklist(script))
        return Method_Error;

    MethodStatus status = compileContext.compileTransitively();
    if (status != Method_Compiled)
        return status;

    // It can happen that during transitive compilation, our
    // callee's parallel ion script is invalidated or GC'd. So
    // before we declare success, double check that it's still
    // compiled!
    if (!script->hasParallelIonScript())
        return Method_Skipped;

    return Method_Compiled;
}

ExecutionStatus
js::ParallelDo::disqualifyFromParallelExecution()
{
    if (!executeSequentially())
        return ExecutionFatal;
    return ExecutionSequential;
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
      default:
        return "no known reason";
    }
}

void
js::ParallelDo::determineBailoutCause()
{
    bailoutCause = ParallelBailoutNone;
    for (uint32_t i = 0; i < bailoutRecords.length(); i++) {
        if (bailoutRecords[i].cause == ParallelBailoutNone)
            continue;

        if (bailoutRecords[i].cause == ParallelBailoutInterrupt)
            continue;

        bailoutCause = bailoutRecords[i].cause;
        const char *causeStr = BailoutExplanation(bailoutCause);
        if (bailoutRecords[i].depth) {
            bailoutScript = bailoutRecords[i].trace[0].script;
            bailoutBytecode = bailoutRecords[i].trace[0].bytecode;

            const char *filename = bailoutScript->filename();
            int line = JS_PCToLineNumber(cx_, bailoutScript, bailoutBytecode);
            JS_ReportWarning(cx_, "Bailed out of parallel operation: %s at %s:%d",
                             causeStr, filename, line);
        } else {
            JS_ReportWarning(cx_, "Bailed out of parallel operation: %s",
                             causeStr);
        }
    }
}

bool
js::ParallelDo::invalidateBailedOutScripts()
{
    RootedScript script(cx_, fun_->toFunction()->nonLazyScript());

    // Sometimes the script is collected or invalidated already,
    // for example when a full GC runs at an inconvenient time.
    if (!script->hasParallelIonScript()) {
        return true;
    }

    Vector<types::RecompileInfo> invalid(cx_);
    for (uint32_t i = 0; i < bailoutRecords.length(); i++) {
        JSScript *script = bailoutRecords[i].topScript;

        // No script to invalidate.
        if (!script || !script->hasParallelIonScript())
            continue;

        switch (bailoutRecords[i].cause) {
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

        if (!invalid.append(script->parallelIonScript()->recompileInfo()))
            return false;
    }
    Invalidate(cx_, invalid);
    return true;
}

bool
js::ParallelDo::warmupForParallelExecution()
{
    AutoEnterWarmup warmup(cx_->runtime);
    return executeSequentially();
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
        // that there is no incremental GC in progress:

        if (JS::IsIncrementalGCInProgress(cx->runtime)) {
            JS::PrepareForIncrementalGC(cx->runtime);
            JS::FinishIncrementalGC(cx->runtime, JS::gcreason::API);
        }

        cx->runtime->gcHelperThread.waitBackgroundSweepEnd();
    }

    ~AutoEnterParallelSection() {
        cx_->mainThread().ionTop = prevIonTop_;
    }
};

ParallelResult
js::ParallelDo::executeInParallel()
{
    // Recursive use of the ThreadPool is not supported.
    if (ForkJoinSlice::Current() != NULL)
        return TP_RETRY_SEQUENTIALLY;

    AutoEnterParallelSection enter(cx_);

    ThreadPool *threadPool = &cx_->runtime->threadPool;
    uint32_t numSlices = ForkJoinSlices(cx_);

    RootedObject rootedFun(cx_, fun_);
    ForkJoinShared shared(cx_, threadPool, rootedFun, numSlices, numSlices - 1, &bailoutRecords[0]);
    if (!shared.init())
        return TP_RETRY_SEQUENTIALLY;

    return shared.execute();
}

/*static*/ bool
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
        Allocator *allocator = cx_->runtime->new_<Allocator>(cx_->zone());
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
    if (cx_->runtime->interrupt)
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
    JSCompartment *comp = cx_->compartment;
    for (unsigned i = 0; i < numSlices_; i++)
        comp->adoptWorkerAllocator(allocators_[i]);

    if (gcRequested_) {
        if (!gcZone_)
            TriggerGC(cx_->runtime, gcReason_);
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

    PerThreadData thisThread(cx_->runtime);
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
    IonContext icx(cx_->compartment, NULL);

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
        ParallelIonInvoke<3> fii(cx_->compartment, callee, 3);

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
    JS_ASSERT(cx_->runtime->interrupt);

    if (abort_)
        return false;

    if (slice.isMainThread()) {
        JS_ASSERT(!cx_->runtime->gcIsNeeded);

        if (cx_->runtime->interrupt) {
            // The GC Needed flag should not be set during parallel
            // execution.  Instead, one of the requestGC() or
            // requestZoneGC() methods should be invoked.
            JS_ASSERT(!cx_->runtime->gcIsNeeded);

            // If interrupt is requested, bring worker threads to a halt,
            // service the interrupt, then let them start back up again.
            // AutoRendezvous autoRendezvous(slice);
            // if (!js_HandleExecutionInterrupt(cx_))
            //     return setAbortFlag(true);
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
    JS_ASSERT(cx_->runtime->interrupt);

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

    cx_->runtime->triggerOperationCallback();
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
    shared->setAbortFlag(false);
}

void
ForkJoinSlice::requestZoneGC(JS::Zone *zone, JS::gcreason::Reason reason)
{
    shared->requestZoneGC(zone, reason);
    shared->setAbortFlag(false);
}

/////////////////////////////////////////////////////////////////////////////

uint32_t
js::ForkJoinSlices(JSContext *cx)
{
    // Parallel workers plus this main thread.
    return cx->runtime->threadPool.numWorkers() + 1;
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

#endif // JS_THREADSAFE && JS_ION

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ForkJoin_h
#define vm_ForkJoin_h

#include "mozilla/ThreadLocal.h"

#include "jscntxt.h"

#include "gc/GCInternals.h"

#include "jit/Ion.h"

///////////////////////////////////////////////////////////////////////////
// Read Me First
//
// The ForkJoin abstraction:
// -------------------------
//
// This is the building block for executing multi-threaded JavaScript with
// shared memory (as distinct from Web Workers).  The idea is that you have
// some (typically data-parallel) operation which you wish to execute in
// parallel across as many threads as you have available.
//
// The ForkJoin abstraction is intended to be used by self-hosted code
// to enable parallel execution.  At the top-level, it consists of a native
// function (exposed as the ForkJoin intrinsic) that is used like so:
//
//     ForkJoin(func, feedback, N)
//
// The intention of this statement is to start |N| copies of |func()|
// running in parallel.  Each copy will then do more or less 1/Nth of
// the total work, depending on workstealing-based load balancing.
//
// Typically, each of the N slices runs in a different worker thread,
// but that is not something you should rely upon---if work-stealing
// is enabled it could be that a single worker thread winds up
// handling multiple slices.
//
// The second argument, |feedback|, is an optional callback that will
// receiver information about how execution proceeded.  This is
// intended for use in unit testing but also for providing feedback to
// users.  Note that gathering the data to provide to |feedback| is
// not free and so execution will run somewhat slower if |feedback| is
// provided.
//
// func() should expect the following arguments:
//
//     func(id, n, warmup)
//
// Here, |id| is the slice id. |n| is the total number of slices. The
// parameter |warmup| is true for a *warmup or recovery phase*.
// Warmup phases are discussed below in more detail, but the general
// idea is that if |warmup| is true, |func| should only do a fixed
// amount of work.  If |warmup| is false, |func| should try to do all
// remaining work is assigned.
//
// Note that we implicitly assume that |func| is tracking how much
// work it has accomplished thus far; some techniques for doing this
// are discussed in |ParallelArray.js|.
//
// Warmups and Sequential Fallbacks
// --------------------------------
//
// ForkJoin can only execute code in parallel when it has been
// ion-compiled in Parallel Execution Mode. ForkJoin handles this part
// for you. However, because ion relies on having decent type
// information available, it is necessary to run the code sequentially
// for a few iterations first to get the various type sets "primed"
// with reasonable information.  We try to make do with just a few
// runs, under the hypothesis that parallel execution code which reach
// type stability relatively quickly.
//
// The general strategy of ForkJoin is as follows:
//
// - If the code has not yet been run, invoke `func` sequentially with
//   warmup set to true.  When warmup is true, `func` should try and
//   do less work than normal---just enough to prime type sets. (See
//   ParallelArray.js for a discussion of specifically how we do this
//   in the case of ParallelArray).
//
// - Try to execute the code in parallel.  Parallel execution mode has
//   three possible results: success, fatal error, or bailout.  If a
//   bailout occurs, it means that the code attempted some action
//   which is not possible in parallel mode.  This might be a
//   modification to shared state, but it might also be that it
//   attempted to take some theoreticaly pure action that has not been
//   made threadsafe (yet?).
//
// - If parallel execution is successful, ForkJoin returns true.
//
// - If parallel execution results in a fatal error, ForkJoin returns false.
//
// - If parallel execution results in a *bailout*, this is when things
//   get interesting.  In that case, the semantics of parallel
//   execution guarantee us that no visible side effects have occurred
//   (unless they were performed with the intrinsic
//   |UnsafePutElements()|, which can only be used in self-hosted
//   code).  We therefore reinvoke |func()| but with warmup set to
//   true.  The idea here is that often parallel bailouts result from
//   a failed type guard or other similar assumption, so rerunning the
//   warmup sequentially gives us a chance to recompile with more
//   data.  Because warmup is true, we do not expect this sequential
//   call to process all remaining data, just a chunk.  After this
//   recovery execution is complete, we again attempt parallel
//   execution.
//
// - If more than a fixed number of bailouts occur, we give up on
//   parallelization and just invoke |func()| N times in a row (once
//   for each worker) but with |warmup| set to false.
//
// Operation callback:
//
// During parallel execution, |slice.check()| must be periodically
// invoked to check for the operation callback. This is automatically
// done by the Ion-generated code. If the operation callback is
// necessary, |slice.check()| abort the parallel execution.
//
// Transitive compilation:
//
// One of the challenges for parallel compilation is that we
// (currently) have to abort when we encounter an uncompiled script.
// Therefore, we try to compile everything that might be needed
// beforehand. The exact strategy is described in `ParallelDo::apply()`
// in ForkJoin.cpp, but at the highest level the idea is:
//
// 1. We maintain a flag on every script telling us if that script and
//    its transitive callees are believed to be compiled. If that flag
//    is set, we can skip the initial compilation.
// 2. Otherwise, we maintain a worklist that begins with the main
//    script. We compile it and then examine the generated parallel IonScript,
//    which will have a list of callees. We enqueue those. Some of these
//    compilations may take place off the main thread, in which case
//    we will run warmup iterations while we wait for them to complete.
// 3. If the warmup iterations finish all the work, we're done.
// 4. If compilations fail, we fallback to sequential.
// 5. Otherwise, we will try running in parallel once we're all done.
//
// Bailout tracing and recording:
//
// When a bailout occurs, we record a bit of state so that we can
// recover with grace. Each |ForkJoinSlice| has a pointer to a
// |ParallelBailoutRecord| pre-allocated for this purpose. This
// structure is used to record the cause of the bailout, the JSScript
// which was executing, as well as the location in the source where
// the bailout occurred (in principle, we can record a full stack
// trace, but right now we only record the top-most frame). Note that
// the error location might not be in the same JSScript as the one
// which was executing due to inlining.
//
// Garbage collection and allocation:
//
// Code which executes on these parallel threads must be very careful
// with respect to garbage collection and allocation.  The typical
// allocation paths are UNSAFE in parallel code because they access
// shared state (the compartment's arena lists and so forth) without
// any synchronization.  They can also trigger GC in an ad-hoc way.
//
// To deal with this, the forkjoin code creates a distinct |Allocator|
// object for each slice.  You can access the appropriate object via
// the |ForkJoinSlice| object that is provided to the callbacks.  Once
// the execution is complete, all the objects found in these distinct
// |Allocator| is merged back into the main compartment lists and
// things proceed normally.
//
// In Ion-generated code, we will do allocation through the
// |Allocator| found in |ForkJoinSlice| (which is obtained via TLS).
// Also, no write barriers are emitted.  Conceptually, we should never
// need a write barrier because we only permit writes to objects that
// are newly allocated, and such objects are always black (to use
// incremental GC terminology).  However, to be safe, we also block
// upon entering a parallel section to ensure that any concurrent
// marking or incremental GC has completed.
//
// In the future, it should be possible to lift the restriction that
// we must block until inc. GC has completed and also to permit GC
// during parallel exeution. But we're not there yet.
//
// Load balancing (work stealing):

// The ForkJoin job is dynamically divided into a fixed number of slices,
// and is submitted for parallel execution in the pool. When the number
// of slices is big enough (typically greater than the number of workers
// in the pool) -and the workload is unbalanced- each worker thread
// will perform load balancing through work stealing. The number
// of slices is computed by the self-hosted function |ComputeNumSlices|
// and can be used to know how many slices will be executed by the
// runtime for an array of the given size.
//
// Current Limitations:
//
// - The API does not support recursive or nested use.  That is, the
//   JavaScript function given to |ForkJoin| should not itself invoke
//   |ForkJoin()|. Instead, use the intrinsic |InParallelSection()| to
//   check for recursive use and execute a sequential fallback.
//
///////////////////////////////////////////////////////////////////////////

namespace js {

class ForkJoinActivation : public Activation
{
    uint8_t *prevIonTop_;

    // We ensure that incremental GC be finished before we enter into a fork
    // join section, but the runtime/zone might still be marked as needing
    // barriers due to being in the middle of verifying barriers. Pause
    // verification during the fork join section.
    gc::AutoStopVerifyingBarriers av_;

  public:
    ForkJoinActivation(JSContext *cx);
    ~ForkJoinActivation();
};

class ForkJoinSlice;

bool ForkJoin(JSContext *cx, CallArgs &args);

struct IonLIRTraceData {
    uint32_t blockIndex;
    uint32_t lirIndex;
    uint32_t execModeInt;
    const char *lirOpName;
    const char *mirOpName;
    JSScript *script;
    jsbytecode *pc;
};

///////////////////////////////////////////////////////////////////////////
// Bailout tracking

enum ParallelBailoutCause {
    ParallelBailoutNone,

    // Compiler returned Method_Skipped
    ParallelBailoutCompilationSkipped,

    // Compiler returned Method_CantCompile
    ParallelBailoutCompilationFailure,

    // The periodic interrupt failed, which can mean that either
    // another thread canceled, the user interrupted us, etc
    ParallelBailoutInterrupt,

    // An IC update failed
    ParallelBailoutFailedIC,

    // Heap busy flag was set during interrupt
    ParallelBailoutHeapBusy,

    ParallelBailoutMainScriptNotPresent,
    ParallelBailoutCalledToUncompiledScript,
    ParallelBailoutIllegalWrite,
    ParallelBailoutAccessToIntrinsic,
    ParallelBailoutOverRecursed,
    ParallelBailoutOutOfMemory,
    ParallelBailoutUnsupported,
    ParallelBailoutUnsupportedVM,
    ParallelBailoutUnsupportedStringComparison,
    ParallelBailoutRequestedGC,
    ParallelBailoutRequestedZoneGC,
};

struct ParallelBailoutTrace {
    JSScript *script;
    jsbytecode *bytecode;
};

// See "Bailouts" section in comment above.
struct ParallelBailoutRecord {
    JSScript *topScript;
    ParallelBailoutCause cause;

    // Eventually we will support deeper traces,
    // but for now we gather at most a single frame.
    static const uint32_t MaxDepth = 1;
    uint32_t depth;
    ParallelBailoutTrace trace[MaxDepth];

    void init(JSContext *cx);
    void reset(JSContext *cx);
    void setCause(ParallelBailoutCause cause,
                  JSScript *outermostScript = nullptr,   // inliner (if applicable)
                  JSScript *currentScript = nullptr,     // inlinee (if applicable)
                  jsbytecode *currentPc = nullptr);
    void updateCause(ParallelBailoutCause cause,
                     JSScript *outermostScript,
                     JSScript *currentScript,
                     jsbytecode *currentPc);
    void addTrace(JSScript *script,
                  jsbytecode *pc);
};

struct ForkJoinShared;

class ForkJoinSlice : public ThreadSafeContext
{
  public:
    // The slice that is being processed.
    const uint16_t sliceId;

    // The worker that is doing the work.
    const uint32_t workerId;

    // Bailout record used to record the reason this thread stopped executing
    ParallelBailoutRecord *const bailoutRecord;

#ifdef DEBUG
    // Records the last instr. to execute on this thread.
    IonLIRTraceData traceData;

    // The maximum worker and slice id.
    uint16_t maxSliceId;
    uint32_t maxWorkerId;
#endif

    ForkJoinSlice(PerThreadData *perThreadData, uint16_t sliceId, uint32_t workerId,
                  Allocator *allocator, ForkJoinShared *shared,
                  ParallelBailoutRecord *bailoutRecord);

    // True if this is the main thread, false if it is one of the parallel workers.
    bool isMainThread() const;

    // When the code would normally trigger a GC, we don't trigger it
    // immediately but instead record that request here.  This will
    // cause |ExecuteForkJoinOp()| to invoke |TriggerGC()| or
    // |TriggerCompartmentGC()| as appropriate once the parallel
    // section is complete. This is done because those routines do
    // various preparations that are not thread-safe, and because the
    // full set of arenas is not available until the end of the
    // parallel section.
    void requestGC(JS::gcreason::Reason reason);
    void requestZoneGC(JS::Zone *zone, JS::gcreason::Reason reason);

    // Set the fatal flag for the next abort. Used to distinguish retry or
    // fatal aborts from VM functions.
    bool setPendingAbortFatal(ParallelBailoutCause cause);

    // Reports an unsupported operation, returning false if we are reporting
    // an error. Otherwise drop the warning on the floor.
    bool reportError(ParallelBailoutCause cause, unsigned report) {
        if (report & JSREPORT_ERROR)
            return setPendingAbortFatal(cause);
        return true;
    }

    // During the parallel phase, this method should be invoked
    // periodically, for example on every backedge, similar to the
    // interrupt check.  If it returns false, then the parallel phase
    // has been aborted and so you should bailout.  The function may
    // also rendesvous to perform GC or do other similar things.
    //
    // This function is guaranteed to have no effect if both
    // runtime()->interrupt is zero.  Ion-generated code takes
    // advantage of this by inlining the checks on those flags before
    // actually calling this function.  If this function ends up
    // getting called a lot from outside ion code, we can refactor
    // it into an inlined version with this check that calls a slower
    // version.
    bool check();

    // Be wary, the runtime is shared between all threads!
    JSRuntime *runtime();

    // Acquire and release the JSContext from the runtime.
    JSContext *acquireContext();
    void releaseContext();
    bool hasAcquiredContext() const;

    // Check the current state of parallel execution.
    static inline ForkJoinSlice *current();

    // Initializes the thread-local state.
    static bool initialize();

  private:
    friend class AutoSetForkJoinSlice;

    // Initialized by initialize()
    static mozilla::ThreadLocal<ForkJoinSlice*> tlsForkJoinSlice;

    ForkJoinShared *const shared;

    bool acquiredContext_;

    // ForkJoinSlice is allocated on the stack. It would be dangerous to GC
    // with it live because of the GC pointer fields stored in the context.
    JS::AutoAssertNoGC nogc_;
};

// Locks a JSContext for its scope. Be very careful, because locking a
// JSContext does *not* allow you to safely mutate the data in the
// JSContext unless you can guarantee that any of the other threads
// that want to access that data will also acquire the lock, which is
// generally not the case. For example, the lock is used in the IC
// code to allow us to atomically patch up the dispatch table, but we
// must be aware that other threads may be reading from the table even
// as we write to it (though they cannot be writing, since they must
// hold the lock to write).
class LockedJSContext
{
#if defined(JS_THREADSAFE) && defined(JS_ION)
    ForkJoinSlice *slice_;
#endif
    JSContext *cx_;

  public:
    LockedJSContext(ForkJoinSlice *slice)
#if defined(JS_THREADSAFE) && defined(JS_ION)
      : slice_(slice),
        cx_(slice->acquireContext())
#else
      : cx_(nullptr)
#endif
    { }

    ~LockedJSContext() {
#if defined(JS_THREADSAFE) && defined(JS_ION)
        slice_->releaseContext();
#endif
    }

    operator JSContext *() { return cx_; }
    JSContext *operator->() { return cx_; }
};

bool InExclusiveParallelSection();

bool ParallelTestsShouldPass(JSContext *cx);

///////////////////////////////////////////////////////////////////////////
// Debug Spew

namespace jit {
    class MDefinition;
}

namespace parallel {

enum ExecutionStatus {
    // Parallel or seq execution terminated in a fatal way, operation failed
    ExecutionFatal,

    // Parallel exec failed and so we fell back to sequential
    ExecutionSequential,

    // We completed the work in seq mode before parallel compilation completed
    ExecutionWarmup,

    // Parallel exec was successful after some number of bailouts
    ExecutionParallel
};

enum SpewChannel {
    SpewOps,
    SpewCompile,
    SpewBailouts,
    NumSpewChannels
};

#if defined(DEBUG) && defined(JS_THREADSAFE) && defined(JS_ION)

bool SpewEnabled(SpewChannel channel);
void Spew(SpewChannel channel, const char *fmt, ...);
void SpewBeginOp(JSContext *cx, const char *name);
void SpewBailout(uint32_t count, HandleScript script, jsbytecode *pc,
                 ParallelBailoutCause cause);
ExecutionStatus SpewEndOp(ExecutionStatus status);
void SpewBeginCompile(HandleScript script);
jit::MethodStatus SpewEndCompile(jit::MethodStatus status);
void SpewMIR(jit::MDefinition *mir, const char *fmt, ...);
void SpewBailoutIR(IonLIRTraceData *data);

#else

static inline bool SpewEnabled(SpewChannel channel) { return false; }
static inline void Spew(SpewChannel channel, const char *fmt, ...) { }
static inline void SpewBeginOp(JSContext *cx, const char *name) { }
static inline void SpewBailout(uint32_t count, HandleScript script,
                               jsbytecode *pc, ParallelBailoutCause cause) {}
static inline ExecutionStatus SpewEndOp(ExecutionStatus status) { return status; }
static inline void SpewBeginCompile(HandleScript script) { }
#ifdef JS_ION
static inline jit::MethodStatus SpewEndCompile(jit::MethodStatus status) { return status; }
static inline void SpewMIR(jit::MDefinition *mir, const char *fmt, ...) { }
#endif
static inline void SpewBailoutIR(IonLIRTraceData *data) { }

#endif // DEBUG && JS_THREADSAFE && JS_ION

} // namespace parallel
} // namespace js

/* static */ inline js::ForkJoinSlice *
js::ForkJoinSlice::current()
{
    return tlsForkJoinSlice.get();
}

namespace js {

static inline bool
InParallelSection()
{
    return ForkJoinSlice::current() != nullptr;
}

} // namespace js

#endif /* vm_ForkJoin_h */

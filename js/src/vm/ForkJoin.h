/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ForkJoin_h
#define vm_ForkJoin_h

#include "mozilla/ThreadLocal.h"

#include <stdarg.h>

#include "jscntxt.h"

#include "gc/ForkJoinNursery.h"
#include "gc/GCInternals.h"

#include "jit/Ion.h"

#ifdef DEBUG
  #define FORKJOIN_SPEW
#endif

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
//     ForkJoin(func, sliceStart, sliceEnd, mode, updatable)
//
// The intention of this statement is to start some some number (usually the
// number of hardware threads) of copies of |func()| running in parallel. Each
// copy will then do a portion of the total work, depending on
// workstealing-based load balancing.
//
// Typically, each of the N slices runs in a different worker thread, but that
// is not something you should rely upon---if work-stealing is enabled it
// could be that a single worker thread winds up handling multiple slices.
//
// The second and third arguments, |sliceStart| and |sliceEnd|, are the slice
// boundaries. These numbers must each fit inside an uint16_t.
//
// The fourth argument, |mode|, is an internal mode integer giving finer
// control over the behavior of ForkJoin. See the |ForkJoinMode| enum.
//
// The fifth argument, |updatable|, if not null, is an object that may
// be updated in a race-free manner by |func()| or its callees.
// Typically this is some sort of pre-sized array.  Only this object
// may be updated by |func()|, and updates must not race.  (A more
// general approach is perhaps desirable, eg passing an Array of
// objects that may be updated, but that is not presently needed.)
//
// func() should expect the following arguments:
//
//     func(workerId, sliceStart, sliceEnd)
//
// The |workerId| parameter is the id of the worker executing the function. It
// is 0 in sequential mode.
//
// The |sliceStart| and |sliceEnd| parameters are the current bounds that that
// the worker is handling. In parallel execution, these parameters are not
// used. In sequential execution, they tell the worker what slices should be
// processed. During the warm up phase, sliceEnd == sliceStart + 1.
//
// |func| can keep asking for more work from the scheduler by calling the
// intrinsic |GetForkJoinSlice(sliceStart, sliceEnd, id)|. When there are no
// more slices to hand out, ThreadPool::MAX_SLICE_ID is returned as a sentinel
// value. By exposing this function as an intrinsic, we reduce the number of
// JS-C++ boundary crossings incurred by workstealing, which may have many
// slices.
//
// In sequential execution, |func| should return the maximum computed slice id
// S for which all slices with id < S have already been processed. This is so
// ThreadPool can track the leftmost completed slice id to maintain
// determinism. Slices which have been completed in sequential execution
// cannot be re-run in parallel execution.
//
// In parallel execution, |func| MUST PROCESS ALL SLICES BEFORE RETURNING!
// Not doing so is an error and is protected by debug asserts in ThreadPool.
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
// Interrupts:
//
// During parallel execution, |cx.check()| must be periodically invoked to
// check for interrupts. This is automatically done by the Ion-generated
// code. If an interrupt has been requested |cx.check()| aborts parallel
// execution.
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
// recover with grace. Each |ForkJoinContext| has a pointer to a
// |ParallelBailoutRecord| pre-allocated for this purpose. This
// structure is used to record the cause of the bailout, the JSScript
// which was executing, as well as the location in the source where
// the bailout occurred (in principle, we can record a full stack
// trace, but right now we only record the top-most frame). Note that
// the error location might not be in the same JSScript as the one
// which was executing due to inlining.
//
// Garbage collection, allocation, and write barriers:
//
// Code which executes on these parallel threads must be very careful
// with respect to garbage collection and allocation.  The typical
// allocation paths are UNSAFE in parallel code because they access
// shared state (the compartment's arena lists and so forth) without
// any synchronization.  They can also trigger GC in an ad-hoc way.
//
// To deal with this, the forkjoin code creates a distinct |Allocator|
// object for each worker, which is used as follows.
//
// In a non-generational setting you can access the appropriate
// allocator via the |ForkJoinContext| object that is provided to the
// callbacks.  Once the parallel execution is complete, all the
// objects found in these distinct |Allocator| are merged back into
// the main compartment lists and things proceed normally.  (If it is
// known that the result array contains no references then no merging
// is necessary.)
//
// In a generational setting there is a per-thread |ForkJoinNursery|
// in addition to the per-thread Allocator.  All "simple" objects
// (meaning they are reasonably small, can be copied, and have no
// complicated finalization semantics) are allocated in the nurseries;
// other objects are allocated directly in the threads' Allocators,
// which serve as the tenured areas for the threads.
//
// When a thread's nursery fills up it can be collected independently
// of the other threads' nurseries, and does not require any of the
// threads to bail out of the parallel section.  The nursery is
// copy-collected, and the expectation is that the survival rate will
// be very low and the collection will be very cheap.
//
// When the parallel execution is complete, and only if merging of the
// Allocators into the main compartment is necessary, then the live
// objects of the nurseries are copied into the respective Allocators,
// in parallel, before the merging takes place.
//
// In Ion-generated code, we will do allocation through the
// |ForkJoinNursery| or |Allocator| found in |ForkJoinContext| (which
// is obtained via TLS).
//
// No write barriers are emitted.  We permit writes to thread-local
// objects, and such writes can create cross-generational pointers or
// pointers that may interact with incremental GC.  However, the
// per-thread generational collector scans its entire tenured area on
// each minor collection, and we block upon entering a parallel
// section to ensure that any concurrent marking or incremental GC has
// completed.
//
// In the future, it should be possible to lift the restriction that
// we must block until incremental GC has completed. But we're not
// there yet.
//
// Load balancing (work stealing):
//
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
    uint8_t *prevJitTop_;

    // We ensure that incremental GC be finished before we enter into a fork
    // join section, but the runtime/zone might still be marked as needing
    // barriers due to being in the middle of verifying barriers. Pause
    // verification during the fork join section.
    gc::AutoStopVerifyingBarriers av_;

  public:
    explicit ForkJoinActivation(JSContext *cx);
    ~ForkJoinActivation();
};

class ForkJoinContext;

bool ForkJoin(JSContext *cx, CallArgs &args);

///////////////////////////////////////////////////////////////////////////
// Bailout tracking

//
// The lattice of causes goes:
//
//       { everything else }
//               |
//           Interrupt
//           /       |
//   Unsupported   UnsupportedVM
//           \       /
//              None
//
enum ParallelBailoutCause {
    ParallelBailoutNone = 0,

    ParallelBailoutUnsupported,
    ParallelBailoutUnsupportedVM,

    // The periodic interrupt failed, which can mean that either
    // another thread canceled, the user interrupted us, etc
    ParallelBailoutInterrupt,

    // Compiler returned Method_Skipped
    ParallelBailoutCompilationSkipped,

    // Compiler returned Method_CantCompile
    ParallelBailoutCompilationFailure,

    // Propagating a failure, i.e., another thread requested the computation
    // be aborted.
    ParallelBailoutPropagate,

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
    ParallelBailoutUnsupportedStringComparison,
    ParallelBailoutRequestedGC,
    ParallelBailoutRequestedZoneGC
};

namespace jit {
class BailoutStack;
class JitFrameIterator;
class IonBailoutIterator;
class RematerializedFrame;
}

// See "Bailouts" section in comment above.
struct ParallelBailoutRecord
{
    // Captured Ion frames at the point of bailout. Stored younger-to-older,
    // i.e., the 0th frame is the youngest frame.
    Vector<jit::RematerializedFrame *> *frames_;
    ParallelBailoutCause cause;

    ParallelBailoutRecord()
      : frames_(nullptr),
        cause(ParallelBailoutNone)
    { }

    ~ParallelBailoutRecord();

    bool init(JSContext *cx);
    void reset();

    Vector<jit::RematerializedFrame *> &frames() { MOZ_ASSERT(frames_); return *frames_; }
    bool hasFrames() const { return frames_ && !frames_->empty(); }
    bool bailedOut() const { return cause != ParallelBailoutNone; }

    void joinCause(ParallelBailoutCause cause) {
        if (this->cause <= ParallelBailoutInterrupt &&
            (cause > ParallelBailoutInterrupt || cause > this->cause))
        {
            this->cause = cause;
        }
    }

    void rematerializeFrames(ForkJoinContext *cx, jit::JitFrameIterator &frameIter);
    void rematerializeFrames(ForkJoinContext *cx, jit::IonBailoutIterator &frameIter);
};

class ForkJoinShared;

class ForkJoinContext : public ThreadSafeContext
{
  public:
    // Bailout record used to record the reason this thread stopped executing
    ParallelBailoutRecord *const bailoutRecord;

#ifdef FORKJOIN_SPEW
    // The maximum worker id.
    uint32_t maxWorkerId;
#endif

    // When we run a par operation like mapPar, we create an out pointer
    // into a specific region of the destination buffer. Even though the
    // destination buffer is not thread-local, it is permissible to write into
    // it via the handles provided. These two fields identify the memory
    // region where writes are allowed so that the write guards can test for
    // it.
    //
    // Note: we only permit writes into the *specific region* that the user
    // is supposed to write. Normally, they only have access to this region
    // anyhow. But due to sequential fallback it is possible for handles into
    // other regions to escape into global variables in the sequential
    // execution and then get accessed by later parallel sections. Thus we
    // must be careful and ensure that the write is going through a handle
    // into the correct *region* of the buffer.
    uint8_t *targetRegionStart;
    uint8_t *targetRegionEnd;

    ForkJoinContext(PerThreadData *perThreadData, ThreadPoolWorker *worker,
                    Allocator *allocator, ForkJoinShared *shared,
                    ParallelBailoutRecord *bailoutRecord);

    bool initialize();

    // Get the worker id. The main thread by convention has the id of the max
    // worker thread id + 1.
    uint32_t workerId() const { return worker_->id(); }

    // Get a slice of work for the worker associated with the context.
    bool getSlice(uint16_t *sliceId) { return worker_->getSlice(this, sliceId); }

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
    // runtime()->interruptPar is zero.  Ion-generated code takes
    // advantage of this by inlining the checks on those flags before
    // actually calling this function.  If this function ends up
    // getting called a lot from outside ion code, we can refactor
    // it into an inlined version with this check that calls a slower
    // version.
    bool check();

    // Be wary, the runtime is shared between all threads!
    JSRuntime *runtime();

    // Acquire and release the JSContext from the runtime.
    JSContext *acquireJSContext();
    void releaseJSContext();
    bool hasAcquiredJSContext() const;

    // Check the current state of parallel execution.
    static inline ForkJoinContext *current();

    // Initializes the thread-local state.
    static bool initializeTls();

    // Used in inlining GetForkJoinSlice.
    static size_t offsetOfWorker() {
        return offsetof(ForkJoinContext, worker_);
    }

#ifdef JSGC_FJGENERATIONAL
    // There is already a nursery() method in ThreadSafeContext.
    gc::ForkJoinNursery &nursery() { return nursery_; }

    // Evacuate live data from the per-thread nursery into the per-thread
    // tenured area.
    void evacuateLiveData() { nursery_.evacuatingGC(); }

    // Used in inlining nursery allocation.  Note the nursery is a
    // member of the ForkJoinContext (a substructure), not a pointer.
    static size_t offsetOfFJNursery() {
        return offsetof(ForkJoinContext, nursery_);
    }
#endif

  private:
    friend class AutoSetForkJoinContext;

    // Initialized by initialize()
    static mozilla::ThreadLocal<ForkJoinContext*> tlsForkJoinContext;

    ForkJoinShared *const shared_;

#ifdef JSGC_FJGENERATIONAL
    gc::ForkJoinGCShared gcShared_;
    gc::ForkJoinNursery nursery_;
#endif

    ThreadPoolWorker *worker_;

    bool acquiredJSContext_;

    // ForkJoinContext is allocated on the stack. It would be dangerous to GC
    // with it live because of the GC pointer fields stored in the context.
    JS::AutoSuppressGCAnalysis nogc_;
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
    ForkJoinContext *cx_;
#endif
    JSContext *jscx_;

  public:
    explicit LockedJSContext(ForkJoinContext *cx)
#if defined(JS_THREADSAFE) && defined(JS_ION)
      : cx_(cx),
        jscx_(cx->acquireJSContext())
#else
      : jscx_(nullptr)
#endif
    { }

    ~LockedJSContext() {
#if defined(JS_THREADSAFE) && defined(JS_ION)
        cx_->releaseJSContext();
#endif
    }

    operator JSContext *() { return jscx_; }
    JSContext *operator->() { return jscx_; }
};

bool InExclusiveParallelSection();

bool ParallelTestsShouldPass(JSContext *cx);

void RequestInterruptForForkJoin(JSRuntime *rt, JSRuntime::InterruptMode mode);

bool intrinsic_SetForkJoinTargetRegion(JSContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo intrinsic_SetForkJoinTargetRegionInfo;

bool intrinsic_ClearThreadLocalArenas(JSContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo intrinsic_ClearThreadLocalArenasInfo;

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
    SpewGC,
    NumSpewChannels
};

#if defined(FORKJOIN_SPEW) && defined(JS_THREADSAFE) && defined(JS_ION)

bool SpewEnabled(SpewChannel channel);
void Spew(SpewChannel channel, const char *fmt, ...);
void SpewVA(SpewChannel channel, const char *fmt, va_list args);
void SpewBeginOp(JSContext *cx, const char *name);
void SpewBailout(uint32_t count, HandleScript script, jsbytecode *pc,
                 ParallelBailoutCause cause);
ExecutionStatus SpewEndOp(ExecutionStatus status);
void SpewBeginCompile(HandleScript script);
jit::MethodStatus SpewEndCompile(jit::MethodStatus status);
void SpewMIR(jit::MDefinition *mir, const char *fmt, ...);

#else

static inline bool SpewEnabled(SpewChannel channel) { return false; }
static inline void Spew(SpewChannel channel, const char *fmt, ...) { }
static inline void SpewVA(SpewChannel channel, const char *fmt, va_list args) { }
static inline void SpewBeginOp(JSContext *cx, const char *name) { }
static inline void SpewBailout(uint32_t count, HandleScript script,
                               jsbytecode *pc, ParallelBailoutCause cause) {}
static inline ExecutionStatus SpewEndOp(ExecutionStatus status) { return status; }
static inline void SpewBeginCompile(HandleScript script) { }
#ifdef JS_ION
static inline jit::MethodStatus SpewEndCompile(jit::MethodStatus status) { return status; }
static inline void SpewMIR(jit::MDefinition *mir, const char *fmt, ...) { }
#endif

#endif // FORKJOIN_SPEW && JS_THREADSAFE && JS_ION

} // namespace parallel
} // namespace js

/* static */ inline js::ForkJoinContext *
js::ForkJoinContext::current()
{
    return tlsForkJoinContext.get();
}

namespace js {

static inline bool
InParallelSection()
{
    return ForkJoinContext::current() != nullptr;
}

} // namespace js

#endif /* vm_ForkJoin_h */

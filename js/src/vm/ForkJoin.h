/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ForkJoin_h__
#define ForkJoin_h__

#include "vm/ThreadPool.h"
#include "jsgc.h"

// ForkJoin
//
// This is the building block for executing multi-threaded JavaScript with
// shared memory (as distinct from Web Workers).  The idea is that you have
// some (typically data-parallel) operation which you wish to execute in
// parallel across as many threads as you have available.  An example might be
// applying |map()| to a vector in parallel. To implement such a thing, you
// would define a subclass of |ForkJoinOp| to implement the operation and then
// invoke |ExecuteForkJoinOp()|, as follows:
//
//     class MyForkJoinOp {
//       ... define callbacks as appropriate for your operation ...
//     };
//     MyForkJoinOp op;
//     ExecuteForkJoinOp(cx, op);
//
// |ExecuteForkJoinOp()| will fire up the workers in the runtime's
// thread pool, have them execute the callback |parallel()| defined in
// the |ForkJoinOp| class, and then return once all the workers have
// completed.  You will receive |N| calls to the |parallel()|
// callback, where |N| is the value returned by |ForkJoinSlice()|.
// Each callback will be supplied with a |ForkJoinSlice| instance
// providing some context.
//
// Typically there will be one call to |parallel()| from each worker thread,
// but that is not something you should rely upon---if we implement
// work-stealing, for example, then it could be that a single worker thread
// winds up handling multiple slices.
//
// Operation callback:
//
// During parallel execution, you should periodically invoke |slice.check()|,
// which will handle the operation callback.  If the operation callback is
// necessary, |slice.check()| will arrange a rendezvous---that is, as each
// active worker invokes |check()|, it will come to a halt until everyone is
// blocked (Stop The World).  At this point, we perform the callback on the
// main thread, and then resume execution.  If a worker thread terminates
// before calling |check()|, that's fine too.  We assume that you do not do
// unbounded work without invoking |check()|.
//
// Sequential Fallback:
//
// It is assumed that anyone using this API must be prepared for a sequential
// fallback.  Therefore, the |ExecuteForkJoinOp()| returns a status code
// indicating whether a fatal error occurred (in which case you should just
// stop) or whether you should retry the operation, but executing
// sequentially.  An example of where the fallback would be useful is if the
// parallel code encountered an unexpected path that cannot safely be executed
// in parallel (writes to shared state, say).
//
// Garbage collection and allocation:
//
// Code which executes on these parallel threads must be very careful
// with respect to garbage collection and allocation.  Currently, we
// do not permit GC to occur when executing in parallel.  Furthermore,
// the typical allocation paths are UNSAFE in parallel code because
// they access shared state (the compartment's arena lists and so
// forth) without any synchronization.
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
// Current Limitations:
//
// - The API does not support recursive or nested use.  That is, the
//   |parallel()| callback of a |ForkJoinOp| may not itself invoke
//   |ExecuteForkJoinOp()|.  We may lift this limitation in the future.
//
// - No load balancing is performed between worker threads.  That means that
//   the fork-join system is best suited for problems that can be slice into
//   uniform bits.

namespace js {

// Parallel operations in general can have one of three states.  They may
// succeed, fail, or "bail", where bail indicates that the code encountered an
// unexpected condition and should be re-run sequentially.
enum ParallelResult { TP_SUCCESS, TP_RETRY_SEQUENTIALLY, TP_FATAL };

struct ForkJoinOp;

// Returns the number of slices that a fork-join op will have when
// executed.
uint32_t
ForkJoinSlices(JSContext *cx);

// Executes the given |TaskSet| in parallel using the runtime's |ThreadPool|,
// returning upon completion.  In general, if there are |N| workers in the
// threadpool, the problem will be divided into |N+1| slices, as the main
// thread will also execute one slice.
ParallelResult ExecuteForkJoinOp(JSContext *cx, ForkJoinOp &op);

class PerThreadData;
class ForkJoinShared;
class AutoRendezvous;
class AutoSetForkJoinSlice;

#ifdef DEBUG
struct IonLIRTraceData {
    uint32_t bblock;
    uint32_t lir;
    uint32_t execModeInt;
    const char *lirOpName;
    const char *mirOpName;
    JSScript *script;
    jsbytecode *pc;
};
#endif

struct ForkJoinSlice
{
  public:
    // PerThreadData corresponding to the current worker thread.
    PerThreadData *perThreadData;

    // Which slice should you process? Ranges from 0 to |numSlices|.
    const uint32_t sliceId;

    // How many slices are there in total?
    const uint32_t numSlices;

    // Allocator to use when allocating on this thread.  See
    // |ion::ParFunctions::ParNewGCThing()|.  This should move into
    // |perThreadData|.
    Allocator *const allocator;

    // If we took a parallel bailout, the script that bailed out is stored here.
    JSScript *abortedScript;

#ifdef DEBUG
    // Records the last instr. to execute on this thread.
    IonLIRTraceData traceData;
#endif

    ForkJoinSlice(PerThreadData *perThreadData, uint32_t sliceId, uint32_t numSlices,
                  Allocator *arenaLists, ForkJoinShared *shared);

    // True if this is the main thread, false if it is one of the parallel workers.
    bool isMainThread();

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

    // Check the current state of parallel execution.
    static inline ForkJoinSlice *Current();

    // Initializes the thread-local state.
    static bool InitializeTLS();

  private:
    friend class AutoRendezvous;
    friend class AutoSetForkJoinSlice;

#if defined(JS_THREADSAFE) && defined(JS_ION)
    // Initialized by InitializeTLS()
    static unsigned ThreadPrivateIndex;
    static bool TLSInitialized;
#endif

#if defined(JS_THREADSAFE) && defined(JS_ION)
    // Sets the abort flag and adjusts ionStackLimit so as to cause
    // the overrun check to fail.  This should lead to the operation
    // as a whole aborting.
    void triggerAbort();
#endif

    ForkJoinShared *const shared;
};

// Generic interface for specifying divisible operations that can be
// executed in a fork-join fashion.
struct ForkJoinOp
{
  public:
    // Invoked from each parallel thread to process one slice.  The
    // |ForkJoinSlice| which is supplied will also be available using TLS.
    //
    // Returns true on success, false to halt parallel execution.
    virtual bool parallel(ForkJoinSlice &slice) = 0;
};

static inline bool
InParallelSection()
{
    return ForkJoinSlice::Current() != NULL;
}

} // namespace js

/* static */ inline js::ForkJoinSlice *
js::ForkJoinSlice::Current()
{
#if defined(JS_THREADSAFE) && defined(JS_ION)
    return (ForkJoinSlice*) PR_GetThreadPrivate(ThreadPrivateIndex);
#else
    return NULL;
#endif
}

#endif // ForkJoin_h__

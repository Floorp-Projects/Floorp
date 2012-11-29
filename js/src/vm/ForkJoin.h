/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ForkJoin_h__
#define ForkJoin_h__

#include "vm/ThreadPool.h"

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
// |ExecuteForkJoinOp()| will fire up the workers in the runtime's thread
// pool, have them execute the callbacks defined in the |ForkJoinOp| class,
// and then return once all the workers have completed.
//
// There are three callbacks defined in |ForkJoinOp|.  The first, |pre()|, is
// invoked before the parallel section begins.  It informs you how many slices
// your problem will be divided into (effectively, how many worker threads
// there will be).  This is often useful for allocating an array for the
// workers to store their result or something like that.
//
// Next, you will receive |N| calls to the |parallel()| callback, where |N| is
// the number of slices that were specified in |pre()|.  Each callback will be
// supplied with a |ForkJoinSlice| instance providing some context.
//
// Typically there will be one call to |parallel()| from each worker thread,
// but that is not something you should rely upon---if we implement
// work-stealing, for example, then it could be that a single worker thread
// winds up handling multiple slices.
//
// Finally, after the operation is complete the |post()| callback is invoked,
// giving you a chance to collect the various results.
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

// Executes the given |TaskSet| in parallel using the runtime's |ThreadPool|,
// returning upon completion.  In general, if there are |N| workers in the
// threadpool, the problem will be divided into |N+1| slices, as the main
// thread will also execute one slice.
ParallelResult ExecuteForkJoinOp(JSContext *cx, ForkJoinOp &op);

class PerThreadData;
class ForkJoinShared;
class AutoRendezvous;
class AutoSetForkJoinSlice;
namespace gc { struct ArenaLists; }

struct ForkJoinSlice
{
  public:
    // PerThreadData corresponding to the current worker thread.
    PerThreadData *perThreadData;

    // Which slice should you process? Ranges from 0 to |numSlices|.
    const size_t sliceId;

    // How many slices are there in total?
    const size_t numSlices;

    // Top of the stack.  This should move into |perThreadData|.
    uintptr_t ionStackLimit;

    // Arenas to use when allocating on this thread.  See
    // |ion::ParFunctions::ParNewGCThing()|.  This should move into
    // |perThreadData|.
    gc::ArenaLists *const arenaLists;

    ForkJoinSlice(PerThreadData *perThreadData, size_t sliceId, size_t numSlices,
                  uintptr_t stackLimit, gc::ArenaLists *arenaLists,
                  ForkJoinShared *shared);

    // True if this is the main thread, false if it is one of the parallel workers.
    bool isMainThread();

    // Generally speaking, if a thread returns false, that is interpreted as a
    // "bailout"---meaning, a recoverable error.  If however you call this
    // function before returning false, then the error will be interpreted as
    // *fatal*.  This doesn't strike me as the most elegant solution here but
    // I don't know what'd be better.
    //
    // For convenience, *always* returns false.
    bool setFatal();

    // During the parallel phase, this method should be invoked periodically,
    // for example on every backedge, similar to the interrupt check.  If it
    // returns false, then the parallel phase has been aborted and so you
    // should bailout.  The function may also rendesvous to perform GC or do
    // other similar things.
    bool check();

    // Be wary, the runtime is shared between all threads!
    JSRuntime *runtime();

    static inline ForkJoinSlice *current();
    static bool Initialize();

  private:
    friend class AutoRendezvous;
    friend class AutoSetForkJoinSlice;

#ifdef JS_THREADSAFE
    // Initialized by Initialize()
    static PRUintn ThreadPrivateIndex;
#endif

    ForkJoinShared *const shared;
};

// Generic interface for specifying divisible operations that can be
// executed in a fork-join fashion.
struct ForkJoinOp
{
  public:
    // Invoked before parallel phase begins; informs the task set how many
    // slices there will be and gives it a chance to initialize per-slice data
    // structures.
    //
    // Returns true on success, false to halt parallel execution.
    virtual bool pre(size_t numSlices) = 0;

    // Invoked from each parallel thread to process one slice.  The
    // |ForkJoinSlice| which is supplied will also be available using TLS.
    //
    // Returns true on success, false to halt parallel execution.
    virtual bool parallel(ForkJoinSlice &slice) = 0;

    // Invoked after parallel phase ends if execution was successful
    // (not aborted)
    //
    // Returns true on success, false to halt parallel execution.
    virtual bool post(size_t numSlices) = 0;
};

} // namespace js

#endif // ForkJoin_h__

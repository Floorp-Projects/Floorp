/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jscntxt_h___
#define jscntxt_h___
/*
 * JS execution context.
 */
#include "jsarena.h" /* Added by JSIFY */
#include "jsclist.h"
#include "jslong.h"
#include "jsatom.h"
#include "jsversion.h"
#include "jsdhash.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jsobj.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsregexp.h"
#include "jsutil.h"
#include "jsarray.h"
#include "jstask.h"
#include "jsvector.h"

/*
 * js_GetSrcNote cache to avoid O(n^2) growth in finding a source note for a
 * given pc in a script. We use the script->code pointer to tag the cache,
 * instead of the script address itself, so that source notes are always found
 * by offset from the bytecode with which they were generated.
 */
typedef struct JSGSNCache {
    jsbytecode      *code;
    JSDHashTable    table;
#ifdef JS_GSNMETER
    uint32          hits;
    uint32          misses;
    uint32          fills;
    uint32          purges;
# define GSN_CACHE_METER(cache,cnt) (++(cache)->cnt)
#else
# define GSN_CACHE_METER(cache,cnt) /* nothing */
#endif
} JSGSNCache;

#define js_FinishGSNCache(cache) js_PurgeGSNCache(cache)

extern void
js_PurgeGSNCache(JSGSNCache *cache);

/* These helper macros take a cx as parameter and operate on its GSN cache. */
#define JS_PURGE_GSN_CACHE(cx)      js_PurgeGSNCache(&JS_GSN_CACHE(cx))
#define JS_METER_GSN_CACHE(cx,cnt)  GSN_CACHE_METER(&JS_GSN_CACHE(cx), cnt)

/* Forward declarations of nanojit types. */
namespace nanojit
{
    class Assembler;
    class CodeAlloc;
    class Fragment;
    class LirBuffer;
#ifdef DEBUG
    class LabelMap;
#endif
    template<typename K> struct DefaultHash;
    template<typename K, typename V, typename H> class HashMap;
    template<typename T> class Seq;
}

/* Tracer constants. */
static const size_t MONITOR_N_GLOBAL_STATES = 4;
static const size_t FRAGMENT_TABLE_SIZE = 512;
static const size_t MAX_NATIVE_STACK_SLOTS = 4096;
static const size_t MAX_CALL_STACK_ENTRIES = 500;
static const size_t MAX_GLOBAL_SLOTS = 4096;
static const size_t GLOBAL_SLOTS_BUFFER_SIZE = MAX_GLOBAL_SLOTS + 1;

/* Forward declarations of tracer types. */
class TreeInfo;
class VMAllocator;
class TraceRecorder;
class FrameInfoCache;
struct REHashFn;
struct REHashKey;
struct FrameInfo;
struct VMSideExit;
struct TreeFragment;
struct InterpState;
template<typename T> class Queue;
typedef Queue<uint16> SlotList;
struct REFragment;
typedef nanojit::HashMap<REHashKey, REFragment*, REHashFn> REHashMap;

#if defined(JS_JIT_SPEW) || defined(DEBUG)
struct FragPI;
typedef nanojit::HashMap<uint32, FragPI, nanojit::DefaultHash<uint32> > FragStatsMap;
#endif

/* Holds the execution state during trace execution. */
struct InterpState
{
    JSContext*     cx;                  // current VM context handle
    double*        stackBase;           // native stack base
    double*        sp;                  // native stack pointer, stack[0] is spbase[0]
    double*        eos;                 // first unusable word after the native stack / begin of globals
    FrameInfo**    callstackBase;       // call stack base
    void*          sor;                 // start of rp stack
    FrameInfo**    rp;                  // call stack pointer
    void*          eor;                 // first unusable word after the call stack
    VMSideExit*    lastTreeExitGuard;   // guard we exited on during a tree call
    VMSideExit*    lastTreeCallGuard;   // guard we want to grow from if the tree
                                        // call exit guard mismatched
    void*          rpAtLastTreeCall;    // value of rp at innermost tree call guard
    VMSideExit*    outermostTreeExitGuard; // the last side exit returned by js_CallTree
    TreeInfo*      outermostTree;       // the outermost tree we initially invoked
    uintN*         inlineCallCountp;    // inline call count counter
    VMSideExit**   innermostNestedGuardp;
    VMSideExit*    innermost;
    uint64         startTime;
    InterpState*   prev;

    // Used by _FAIL builtins; see jsbuiltins.h. The builtin sets the
    // JSBUILTIN_BAILED bit if it bails off trace and the JSBUILTIN_ERROR bit
    // if an error or exception occurred.
    uint32         builtinStatus;

    // Used to communicate the location of the return value in case of a deep bail.
    double*        deepBailSp;

    // Used when calling natives from trace to root the vp vector.
    uintN          nativeVpLen;
    jsval*         nativeVp;

    InterpState(JSContext *cx, JSTraceMonitor *tm, TreeInfo *ti,
                uintN &inlineCallCountp, VMSideExit** innermostNestedGuardp);
    ~InterpState();
};

/*
 * Storage for the execution state and store during trace execution. Generated
 * code depends on the fact that the globals begin |MAX_NATIVE_STACK_SLOTS|
 * doubles after the stack begins. Thus, on trace, |InterpState::eos| holds a
 * pointer to the first global.
 */
struct TraceNativeStorage
{
    double stack_global_buf[MAX_NATIVE_STACK_SLOTS + GLOBAL_SLOTS_BUFFER_SIZE];
    FrameInfo *callstack_buf[MAX_CALL_STACK_ENTRIES];

    double *stack() { return stack_global_buf; }
    double *global() { return stack_global_buf + MAX_NATIVE_STACK_SLOTS; }
    FrameInfo **callstack() { return callstack_buf; }
};

/* Holds data to track a single globa. */
struct GlobalState {
    JSObject*               globalObj;
    uint32                  globalShape;
    SlotList*               globalSlots;
};

/*
 * Trace monitor. Every JSThread (if JS_THREADSAFE) or JSRuntime (if not
 * JS_THREADSAFE) has an associated trace monitor that keeps track of loop
 * frequencies for all JavaScript code loaded into that runtime.
 */
struct JSTraceMonitor {
    /*
     * The context currently executing JIT-compiled code on this thread, or
     * NULL if none. Among other things, this can in certain cases prevent
     * last-ditch GC and suppress calls to JS_ReportOutOfMemory.
     *
     * !tracecx && !recorder: not on trace
     * !tracecx && recorder: recording
     * tracecx && !recorder: executing a trace
     * tracecx && recorder: executing inner loop, recording outer loop
     */
    JSContext               *tracecx;

    /*
     * Cached storage to use when executing on trace. While we may enter nested
     * traces, we always reuse the outer trace's storage, so never need more
     * than of these.
     */
    TraceNativeStorage      storage;

    /*
     * There are 3 allocators here. This might seem like overkill, but they
     * have different lifecycles, and by keeping them separate we keep the
     * amount of retained memory down significantly.
     *
     * The dataAlloc has the lifecycle of the monitor. It's flushed only
     * when the monitor is flushed.
     *
     * The traceAlloc has the same flush lifecycle as the dataAlloc, but
     * it is also *marked* when a recording starts and rewinds to the mark
     * point if recording aborts. So you can put things in it that are only
     * reachable on a successful record/compile cycle.
     *
     * The tempAlloc is flushed after each recording, successful or not.
     */

    VMAllocator*            dataAlloc;   /* A chunk allocator for fragments. */
    VMAllocator*            traceAlloc;  /* An allocator for trace metadata. */
    VMAllocator*            tempAlloc;   /* A temporary chunk allocator.  */
    nanojit::CodeAlloc*     codeAlloc;   /* An allocator for native code. */
    nanojit::Assembler*     assembler;
    nanojit::LirBuffer*     lirbuf;
    nanojit::LirBuffer*     reLirBuf;
    FrameInfoCache*         frameCache;
#ifdef DEBUG
    nanojit::LabelMap*      labels;
#endif

    TraceRecorder*          recorder;

    struct GlobalState      globalStates[MONITOR_N_GLOBAL_STATES];
    struct TreeFragment*    vmfragments[FRAGMENT_TABLE_SIZE];
    JSDHashTable            recordAttempts;

    /*
     * Maximum size of the code cache before we start flushing. 1/16 of this
     * size is used as threshold for the regular expression code cache.
     */
    uint32                  maxCodeCacheBytes;

    /*
     * If nonzero, do not flush the JIT cache after a deep bail. That would
     * free JITted code pages that we will later return to. Instead, set the
     * needFlush flag so that it can be flushed later.
     */
    JSBool                  needFlush;

    /*
     * reservedObjects is a linked list (via fslots[0]) of preallocated JSObjects.
     * The JIT uses this to ensure that leaving a trace tree can't fail.
     */
    JSBool                  useReservedObjects;
    JSObject                *reservedObjects;

    /*
     * Fragment map for the regular expression compiler.
     */
    REHashMap*              reFragments;

    /*
     * A temporary allocator for RE recording.
     */
    VMAllocator*            reTempAlloc;

#ifdef DEBUG
    /* Fields needed for fragment/guard profiling. */
    nanojit::Seq<nanojit::Fragment*>* branches;
    uint32                  lastFragID;
    /*
     * profAlloc has a lifetime which spans exactly from js_InitJIT to
     * js_FinishJIT.
     */
    VMAllocator*            profAlloc;
    FragStatsMap*           profTab;
#endif

    /* Flush the JIT cache. */
    void flush();

    /* Mark all objects baked into native code in the code cache. */
    void mark(JSTracer *trc);

    bool outOfMemory() const;
};

typedef struct InterpStruct InterpStruct;

/*
 * N.B. JS_ON_TRACE(cx) is true if JIT code is on the stack in the current
 * thread, regardless of whether cx is the context in which that trace is
 * executing.  cx must be a context on the current thread.
 */
#ifdef JS_TRACER
# define JS_ON_TRACE(cx)            (JS_TRACE_MONITOR(cx).tracecx != NULL)
#else
# define JS_ON_TRACE(cx)            JS_FALSE
#endif

#ifdef DEBUG
# define JS_EVAL_CACHE_METERING     1
# define JS_FUNCTION_METERING       1
#endif

/* Number of potentially reusable scriptsToGC to search for the eval cache. */
#ifndef JS_EVAL_CACHE_SHIFT
# define JS_EVAL_CACHE_SHIFT        6
#endif
#define JS_EVAL_CACHE_SIZE          JS_BIT(JS_EVAL_CACHE_SHIFT)

#ifdef JS_EVAL_CACHE_METERING
# define EVAL_CACHE_METER_LIST(_)   _(probe), _(hit), _(step), _(noscope)
# define identity(x)                x

struct JSEvalCacheMeter {
    uint64 EVAL_CACHE_METER_LIST(identity);
};

# undef identity
#endif

#ifdef JS_FUNCTION_METERING
# define FUNCTION_KIND_METER_LIST(_)                                          \
                        _(allfun), _(heavy), _(nofreeupvar), _(onlyfreevar),  \
                        _(display), _(flat), _(setupvar), _(badfunarg)
# define identity(x)    x

struct JSFunctionMeter {
    int32 FUNCTION_KIND_METER_LIST(identity);
};

# undef identity
#endif

struct JSLocalRootChunk;

#define JSLRS_CHUNK_SHIFT       8
#define JSLRS_CHUNK_SIZE        JS_BIT(JSLRS_CHUNK_SHIFT)
#define JSLRS_CHUNK_MASK        JS_BITMASK(JSLRS_CHUNK_SHIFT)

struct JSLocalRootChunk {
    jsval               roots[JSLRS_CHUNK_SIZE];
    JSLocalRootChunk    *down;
};

struct JSLocalRootStack {
    uint32              scopeMark;
    uint32              rootCount;
    JSLocalRootChunk    *topChunk;
    JSLocalRootChunk    firstChunk;

    /* See comments in js_NewFinalizableGCThing. */
    JSGCFreeLists       gcFreeLists;
};

const uint32 JSLRS_NULL_MARK = uint32(-1);

struct JSThreadData {
    JSGCFreeLists       gcFreeLists;

    /*
     * Flag indicating that we are waiving any soft limits on the GC heap
     * because we want allocations to be infallible (except when we hit
     * a hard quota).
     */
    bool                waiveGCQuota;

    /*
     * The GSN cache is per thread since even multi-cx-per-thread embeddings
     * do not interleave js_GetSrcNote calls.
     */
    JSGSNCache          gsnCache;

    /* Property cache for faster call/get/set invocation. */
    JSPropertyCache     propertyCache;

    /* Random number generator state, used by jsmath.cpp. */
    int64               rngSeed;

    /* Optional stack of heap-allocated scoped local GC roots. */
    JSLocalRootStack    *localRootStack;

#ifdef JS_TRACER
    /* Trace-tree JIT recorder/interpreter state. */
    JSTraceMonitor      traceMonitor;
#endif

    /* Lock-free hashed lists of scripts created by eval to garbage-collect. */
    JSScript            *scriptsToGC[JS_EVAL_CACHE_SIZE];

#ifdef JS_EVAL_CACHE_METERING
    JSEvalCacheMeter    evalCacheMeter;
#endif

    /*
     * Cache of reusable JSNativeEnumerators mapped by shape identifiers (as
     * stored in scope->shape). This cache is nulled by the GC and protected
     * by gcLock.
     */
#define NATIVE_ENUM_CACHE_LOG2  8
#define NATIVE_ENUM_CACHE_MASK  JS_BITMASK(NATIVE_ENUM_CACHE_LOG2)
#define NATIVE_ENUM_CACHE_SIZE  JS_BIT(NATIVE_ENUM_CACHE_LOG2)

#define NATIVE_ENUM_CACHE_HASH(shape)                                         \
    ((((shape) >> NATIVE_ENUM_CACHE_LOG2) ^ (shape)) & NATIVE_ENUM_CACHE_MASK)

    jsuword             nativeEnumCache[NATIVE_ENUM_CACHE_SIZE];

    void init();
    void finish();
    void mark(JSTracer *trc);
    void purge(JSContext *cx);
    void purgeGCFreeLists();
};

#ifdef JS_THREADSAFE

/*
 * Structure uniquely representing a thread.  It holds thread-private data
 * that can be accessed without a global lock.
 */
struct JSThread {
    /* Linked list of all contexts in use on this thread. */
    JSCList             contextList;

    /* Opaque thread-id, from NSPR's PR_GetCurrentThread(). */
    jsword              id;

    /* Indicates that the thread is waiting in ClaimTitle from jslock.cpp. */
    JSTitle             *titleToShare;

    /*
     * Thread-local version of JSRuntime.gcMallocBytes to avoid taking
     * locks on each JS_malloc.
     */
    ptrdiff_t           gcThreadMallocBytes;

    /*
     * Deallocator task for this thread.
     */
    JSFreePointerListTask *deallocatorTask;

    /* Factored out of JSThread for !JS_THREADSAFE embedding in JSRuntime. */
    JSThreadData        data;
};

/*
 * Only when JSThread::gcThreadMallocBytes exhausts the following limit we
 * update JSRuntime::gcMallocBytes.
 * .
 */
const size_t JS_GC_THREAD_MALLOC_LIMIT = 1 << 19;

#define JS_THREAD_DATA(cx)      (&(cx)->thread->data)

struct JSThreadsHashEntry {
    JSDHashEntryHdr     base;
    JSThread            *thread;
};

extern JSThread *
js_CurrentThread(JSRuntime *rt);

/*
 * The function takes the GC lock and does not release in successful return.
 * On error (out of memory) the function releases the lock but delegates
 * the error reporting to the caller.
 */
extern JSBool
js_InitContextThread(JSContext *cx);

/*
 * On entrance the GC lock must be held and it will be held on exit.
 */
extern void
js_ClearContextThread(JSContext *cx);

#endif /* JS_THREADSAFE */

typedef enum JSDestroyContextMode {
    JSDCM_NO_GC,
    JSDCM_MAYBE_GC,
    JSDCM_FORCE_GC,
    JSDCM_NEW_FAILED
} JSDestroyContextMode;

typedef enum JSRuntimeState {
    JSRTS_DOWN,
    JSRTS_LAUNCHING,
    JSRTS_UP,
    JSRTS_LANDING
} JSRuntimeState;

typedef enum JSBuiltinFunctionId {
    JSBUILTIN_ObjectToIterator,
    JSBUILTIN_CallIteratorNext,
    JSBUILTIN_LIMIT
} JSBuiltinFunctionId;

typedef struct JSPropertyTreeEntry {
    JSDHashEntryHdr     hdr;
    JSScopeProperty     *child;
} JSPropertyTreeEntry;

typedef struct JSSetSlotRequest JSSetSlotRequest;

struct JSSetSlotRequest {
    JSObject            *obj;           /* object containing slot to set */
    JSObject            *pobj;          /* new proto or parent reference */
    uint16              slot;           /* which to set, proto or parent */
    JSPackedBool        cycle;          /* true if a cycle was detected */
    JSSetSlotRequest    *next;          /* next request in GC worklist */
};

struct JSRuntime {
    /* Runtime state, synchronized by the stateChange/gcLock condvar/lock. */
    JSRuntimeState      state;

    /* Context create/destroy callback. */
    JSContextCallback   cxCallback;

    /*
     * Shape regenerated whenever a prototype implicated by an "add property"
     * property cache fill and induced trace guard has a readonly property or a
     * setter defined on it. This number proxies for the shapes of all objects
     * along the prototype chain of all objects in the runtime on which such an
     * add-property result has been cached/traced.
     *
     * See bug 492355 for more details.
     *
     * This comes early in JSRuntime to minimize the immediate format used by
     * trace-JITted code that reads it.
     */
    uint32              protoHazardShape;

    /* Garbage collector state, used by jsgc.c. */
    JSGCChunkInfo       *gcChunkList;
    JSGCArenaList       gcArenaList[FINALIZE_LIMIT];
    JSGCDoubleArenaList gcDoubleArenaList;
    JSDHashTable        gcRootsHash;
    JSDHashTable        *gcLocksHash;
    jsrefcount          gcKeepAtoms;
    size_t              gcBytes;
    size_t              gcLastBytes;
    size_t              gcMaxBytes;
    size_t              gcMaxMallocBytes;
    uint32              gcEmptyArenaPoolLifespan;
    uint32              gcLevel;
    uint32              gcNumber;
    JSTracer            *gcMarkingTracer;
    uint32              gcTriggerFactor;
    size_t              gcTriggerBytes;
    volatile JSBool     gcIsNeeded;
    volatile JSBool     gcFlushCodeCaches;

    /*
     * NB: do not pack another flag here by claiming gcPadding unless the new
     * flag is written only by the GC thread.  Atomic updates to packed bytes
     * are not guaranteed, so stores issued by one thread may be lost due to
     * unsynchronized read-modify-write cycles on other threads.
     */
    JSPackedBool        gcPoke;
    JSPackedBool        gcRunning;
    JSPackedBool        gcRegenShapes;

    /*
     * During gc, if rt->gcRegenShapes &&
     *   (scope->flags & JSScope::SHAPE_REGEN) == rt->gcRegenShapesScopeFlag,
     * then the scope's shape has already been regenerated during this GC.
     * To avoid having to sweep JSScopes, the bit's meaning toggles with each
     * shape-regenerating GC.
     *
     * FIXME Once scopes are GC'd (bug 505004), this will be obsolete.
     */
    uint8               gcRegenShapesScopeFlag;

#ifdef JS_GC_ZEAL
    jsrefcount          gcZeal;
#endif

    JSGCCallback        gcCallback;

    /*
     * Malloc counter to measure memory pressure for GC scheduling. It runs
     * from gcMaxMallocBytes down to zero.
     */
    ptrdiff_t           gcMallocBytes;

    /*
     * Stack of GC arenas containing things that the GC marked, where children
     * reached from those things have not yet been marked. This helps avoid
     * using too much native stack during recursive GC marking.
     */
    JSGCArenaInfo       *gcUntracedArenaStackTop;
#ifdef DEBUG
    size_t              gcTraceLaterCount;
#endif

    /*
     * Table for tracking iterators to ensure that we close iterator's state
     * before finalizing the iterable object.
     */
    js::Vector<JSObject*, 0, js::SystemAllocPolicy> gcIteratorTable;

    /*
     * The trace operation and its data argument to trace embedding-specific
     * GC roots.
     */
    JSTraceDataOp       gcExtraRootsTraceOp;
    void                *gcExtraRootsData;

    /*
     * Used to serialize cycle checks when setting __proto__ or __parent__ by
     * requesting the GC handle the required cycle detection. If the GC hasn't
     * been poked, it won't scan for garbage. This member is protected by
     * rt->gcLock.
     */
    JSSetSlotRequest    *setSlotRequests;

    /* Well-known numbers held for use by this runtime's contexts. */
    jsval               NaNValue;
    jsval               negativeInfinityValue;
    jsval               positiveInfinityValue;

#ifdef JS_THREADSAFE
    JSLock              *deflatedStringCacheLock;
#endif
    JSHashTable         *deflatedStringCache;
#ifdef DEBUG
    uint32              deflatedStringCacheBytes;
#endif

    JSString            *emptyString;

    /*
     * Builtin functions, lazily created and held for use by the trace recorder.
     *
     * This field would be #ifdef JS_TRACER, but XPConnect is compiled without
     * -DJS_TRACER and includes this header.
     */
    JSObject            *builtinFunctions[JSBUILTIN_LIMIT];

    /* List of active contexts sharing this runtime; protected by gcLock. */
    JSCList             contextList;

    /* Per runtime debug hooks -- see jsprvtd.h and jsdbgapi.h. */
    JSDebugHooks        globalDebugHooks;

#ifdef JS_TRACER
    /* True if any debug hooks not supported by the JIT are enabled. */
    bool debuggerInhibitsJIT() const {
        return (globalDebugHooks.interruptHandler ||
                globalDebugHooks.callHook ||
                globalDebugHooks.objectHook);
    }
#endif

    /* More debugging state, see jsdbgapi.c. */
    JSCList             trapList;
    JSCList             watchPointList;

    /* Client opaque pointers */
    void                *data;

#ifdef JS_THREADSAFE
    /* These combine to interlock the GC and new requests. */
    PRLock              *gcLock;
    PRCondVar           *gcDone;
    PRCondVar           *requestDone;
    uint32              requestCount;
    JSThread            *gcThread;

    /* Lock and owning thread pointer for JS_LOCK_RUNTIME. */
    PRLock              *rtLock;
#ifdef DEBUG
    jsword              rtLockOwner;
#endif

    /* Used to synchronize down/up state change; protected by gcLock. */
    PRCondVar           *stateChange;

    /*
     * State for sharing single-threaded titles, once a second thread tries to
     * lock a title.  The titleSharingDone condvar is protected by rt->gcLock
     * to minimize number of locks taken in JS_EndRequest.
     *
     * The titleSharingTodo linked list is likewise "global" per runtime, not
     * one-list-per-context, to conserve space over all contexts, optimizing
     * for the likely case that titles become shared rarely, and among a very
     * small set of threads (contexts).
     */
    PRCondVar           *titleSharingDone;
    JSTitle             *titleSharingTodo;

/*
 * Magic terminator for the rt->titleSharingTodo linked list, threaded through
 * title->u.link.  This hack allows us to test whether a title is on the list
 * by asking whether title->u.link is non-null.  We use a large, likely bogus
 * pointer here to distinguish this value from any valid u.count (small int)
 * value.
 */
#define NO_TITLE_SHARING_TODO   ((JSTitle *) 0xfeedbeef)

    /*
     * Lock serializing trapList and watchPointList accesses, and count of all
     * mutations to trapList and watchPointList made by debugger threads.  To
     * keep the code simple, we define debuggerMutations for the thread-unsafe
     * case too.
     */
    PRLock              *debuggerLock;

    JSDHashTable        threads;
#endif /* JS_THREADSAFE */
    uint32              debuggerMutations;

    /*
     * Security callbacks set on the runtime are used by each context unless
     * an override is set on the context.
     */
    JSSecurityCallbacks *securityCallbacks;

    /*
     * Shared scope property tree, and arena-pool for allocating its nodes.
     * The propertyRemovals counter is incremented for every JSScope::clear,
     * and for each JSScope::remove method call that frees a slot in an object.
     * See js_NativeGet and js_NativeSet in jsobj.c.
     */
    JSDHashTable        propertyTreeHash;
    JSScopeProperty     *propertyFreeList;
    JSArenaPool         propertyArenaPool;
    int32               propertyRemovals;

    /* Script filename table. */
    struct JSHashTable  *scriptFilenameTable;
    JSCList             scriptFilenamePrefixes;
#ifdef JS_THREADSAFE
    PRLock              *scriptFilenameTableLock;
#endif

    /* Number localization, used by jsnum.c */
    const char          *thousandsSeparator;
    const char          *decimalSeparator;
    const char          *numGrouping;

    /*
     * Weak references to lazily-created, well-known XML singletons.
     *
     * NB: Singleton objects must be carefully disconnected from the rest of
     * the object graph usually associated with a JSContext's global object,
     * including the set of standard class objects.  See jsxml.c for details.
     */
    JSObject            *anynameObject;
    JSObject            *functionNamespaceObject;

#ifndef JS_THREADSAFE
    JSThreadData        threadData;

#define JS_THREAD_DATA(cx)      (&(cx)->runtime->threadData)
#endif

    /*
     * Object shape (property cache structural type) identifier generator.
     *
     * Type 0 stands for the empty scope, and must not be regenerated due to
     * uint32 wrap-around. Since js_GenerateShape (in jsinterp.cpp) uses
     * atomic pre-increment, the initial value for the first typed non-empty
     * scope will be 1.
     *
     * If this counter overflows into SHAPE_OVERFLOW_BIT (in jsinterp.h), the
     * cache is disabled, to avoid aliasing two different types. It stays
     * disabled until a triggered GC at some later moment compresses live
     * types, minimizing rt->shapeGen in the process.
     */
    volatile uint32     shapeGen;

    /* Literal table maintained by jsatom.c functions. */
    JSAtomState         atomState;

#ifdef JS_THREADSAFE
    JSBackgroundThread    *deallocatorThread;
#endif

    /*
     * Various metering fields are defined at the end of JSRuntime. In this
     * way there is no need to recompile all the code that refers to other
     * fields of JSRuntime after enabling the corresponding metering macro.
     */
#ifdef JS_DUMP_ENUM_CACHE_STATS
    int32               nativeEnumProbes;
    int32               nativeEnumMisses;
# define ENUM_CACHE_METER(name)     JS_ATOMIC_INCREMENT(&cx->runtime->name)
#else
# define ENUM_CACHE_METER(name)     ((void) 0)
#endif

#ifdef JS_DUMP_LOOP_STATS
    /* Loop statistics, to trigger trace recording and compiling. */
    JSBasicStats        loopStats;
#endif

#ifdef DEBUG
    /* Function invocation metering. */
    jsrefcount          inlineCalls;
    jsrefcount          nativeCalls;
    jsrefcount          nonInlineCalls;
    jsrefcount          constructs;

    /* Title lock and scope property metering. */
    jsrefcount          claimAttempts;
    jsrefcount          claimedTitles;
    jsrefcount          deadContexts;
    jsrefcount          deadlocksAvoided;
    jsrefcount          liveScopes;
    jsrefcount          sharedTitles;
    jsrefcount          totalScopes;
    jsrefcount          liveScopeProps;
    jsrefcount          liveScopePropsPreSweep;
    jsrefcount          totalScopeProps;
    jsrefcount          livePropTreeNodes;
    jsrefcount          duplicatePropTreeNodes;
    jsrefcount          totalPropTreeNodes;
    jsrefcount          propTreeKidsChunks;

    /* String instrumentation. */
    jsrefcount          liveStrings;
    jsrefcount          totalStrings;
    jsrefcount          liveDependentStrings;
    jsrefcount          totalDependentStrings;
    jsrefcount          badUndependStrings;
    double              lengthSum;
    double              lengthSquaredSum;
    double              strdepLengthSum;
    double              strdepLengthSquaredSum;

    /* Script instrumentation. */
    jsrefcount          liveScripts;
    jsrefcount          totalScripts;
    jsrefcount          liveEmptyScripts;
    jsrefcount          totalEmptyScripts;
#endif /* DEBUG */

#ifdef JS_SCOPE_DEPTH_METER
    /*
     * Stats on runtime prototype chain lookups and scope chain depths, i.e.,
     * counts of objects traversed on a chain until the wanted id is found.
     */
    JSBasicStats        protoLookupDepthStats;
    JSBasicStats        scopeSearchDepthStats;

    /*
     * Stats on compile-time host environment and lexical scope chain lengths
     * (maximum depths).
     */
    JSBasicStats        hostenvScopeDepthStats;
    JSBasicStats        lexicalScopeDepthStats;
#endif

#ifdef JS_GCMETER
    JSGCStats           gcStats;
#endif

#ifdef JS_FUNCTION_METERING
    JSFunctionMeter     functionMeter;
    char                lastScriptFilename[1024];
#endif

    JSRuntime();
    ~JSRuntime();

    bool init(uint32 maxbytes);

    void setGCTriggerFactor(uint32 factor);
    void setGCLastBytes(size_t lastBytes);

    void* malloc(size_t bytes) { return ::js_malloc(bytes); }

    void* calloc(size_t bytes) { return ::js_calloc(bytes); }

    void* realloc(void* p, size_t bytes) { return ::js_realloc(p, bytes); }

    void free(void* p) { ::js_free(p); }

    bool isGCMallocLimitReached() const { return gcMallocBytes <= 0; }

    void resetGCMallocBytes() { gcMallocBytes = ptrdiff_t(gcMaxMallocBytes); }

    void setGCMaxMallocBytes(size_t value) {
        /*
         * For compatibility treat any value that exceeds PTRDIFF_T_MAX to
         * mean that value.
         */
        gcMaxMallocBytes = (ptrdiff_t(value) >= 0) ? value : size_t(-1) >> 1;
        resetGCMallocBytes();
    }
};

/* Common macros to access thread-local caches in JSThread or JSRuntime. */
#define JS_GSN_CACHE(cx)        (JS_THREAD_DATA(cx)->gsnCache)
#define JS_PROPERTY_CACHE(cx)   (JS_THREAD_DATA(cx)->propertyCache)
#define JS_TRACE_MONITOR(cx)    (JS_THREAD_DATA(cx)->traceMonitor)
#define JS_SCRIPTS_TO_GC(cx)    (JS_THREAD_DATA(cx)->scriptsToGC)

#ifdef JS_EVAL_CACHE_METERING
# define EVAL_CACHE_METER(x)    (JS_THREAD_DATA(cx)->evalCacheMeter.x++)
#else
# define EVAL_CACHE_METER(x)    ((void) 0)
#endif

#ifdef DEBUG
# define JS_RUNTIME_METER(rt, which)    JS_ATOMIC_INCREMENT(&(rt)->which)
# define JS_RUNTIME_UNMETER(rt, which)  JS_ATOMIC_DECREMENT(&(rt)->which)
#else
# define JS_RUNTIME_METER(rt, which)    /* nothing */
# define JS_RUNTIME_UNMETER(rt, which)  /* nothing */
#endif

#define JS_KEEP_ATOMS(rt)   JS_ATOMIC_INCREMENT(&(rt)->gcKeepAtoms);
#define JS_UNKEEP_ATOMS(rt) JS_ATOMIC_DECREMENT(&(rt)->gcKeepAtoms);

#ifdef JS_ARGUMENT_FORMATTER_DEFINED
/*
 * Linked list mapping format strings for JS_{Convert,Push}Arguments{,VA} to
 * formatter functions.  Elements are sorted in non-increasing format string
 * length order.
 */
struct JSArgumentFormatMap {
    const char          *format;
    size_t              length;
    JSArgumentFormatter formatter;
    JSArgumentFormatMap *next;
};
#endif

struct JSStackHeader {
    uintN               nslots;
    JSStackHeader       *down;
};

#define JS_STACK_SEGMENT(sh)    ((jsval *)(sh) + 2)

/*
 * Key and entry types for the JSContext.resolvingTable hash table, typedef'd
 * here because all consumers need to see these declarations (and not just the
 * typedef names, as would be the case for an opaque pointer-to-typedef'd-type
 * declaration), along with cx->resolvingTable.
 */
typedef struct JSResolvingKey {
    JSObject            *obj;
    jsid                id;
} JSResolvingKey;

typedef struct JSResolvingEntry {
    JSDHashEntryHdr     hdr;
    JSResolvingKey      key;
    uint32              flags;
} JSResolvingEntry;

#define JSRESFLAG_LOOKUP        0x1     /* resolving id from lookup */
#define JSRESFLAG_WATCH         0x2     /* resolving id from watch */

/*
 * Macros to push/pop JSTempValueRooter instances to context-linked stack of
 * temporary GC roots. If you need to protect a result value that flows out of
 * a C function across several layers of other functions, use the
 * js_LeaveLocalRootScopeWithResult internal API (see further below) instead.
 *
 * The macros also provide a simple way to get a single rooted pointer via
 * JS_PUSH_TEMP_ROOT_<KIND>(cx, NULL, &tvr). Then &tvr.u.<kind> gives the
 * necessary pointer.
 *
 * JSTempValueRooter.count defines the type of the rooted value referenced by
 * JSTempValueRooter.u union of type JSTempValueUnion. When count is positive
 * or zero, u.array points to a vector of jsvals. Otherwise it must be one of
 * the following constants:
 */
#define JSTVU_SINGLE        (-1)    /* u.value or u.<gcthing> is single jsval
                                       or non-JSString GC-thing pointer */
#define JSTVU_TRACE         (-2)    /* u.trace is a hook to trace a custom
                                     * structure */
#define JSTVU_SPROP         (-3)    /* u.sprop roots property tree node */
#define JSTVU_WEAK_ROOTS    (-4)    /* u.weakRoots points to saved weak roots */
#define JSTVU_COMPILER      (-5)    /* u.compiler roots JSCompiler* */
#define JSTVU_SCRIPT        (-6)    /* u.script roots JSScript* */
#define JSTVU_ENUMERATOR    (-7)    /* a pointer to JSTempValueRooter points
                                       to an instance of JSAutoEnumStateRooter
                                       with u.object storing the enumeration
                                       object */

/*
 * Here single JSTVU_SINGLE covers both jsval and pointers to almost (see note
 * below) any GC-thing via reinterpreting the thing as JSVAL_OBJECT. This works
 * because the GC-thing is aligned on a 0 mod 8 boundary, and object has the 0
 * jsval tag. So any GC-heap-allocated thing pointer may be tagged as if it
 * were an object and untagged, if it's then used only as an opaque pointer
 * until discriminated by other means than tag bits. This is how, for example,
 * js_GetGCThingTraceKind uses its |thing| parameter -- it consults GC-thing
 * flags stored separately from the thing to decide the kind of thing.
 *
 * Note well that JSStrings may be statically allocated (see the intStringTable
 * and unitStringTable static arrays), so this hack does not work for arbitrary
 * GC-thing pointers.
 */
#define JS_PUSH_TEMP_ROOT_COMMON(cx,x,tvr,cnt,kind)                           \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT((cx)->tempValueRooters != (tvr));                           \
        (tvr)->count = (cnt);                                                 \
        (tvr)->u.kind = (x);                                                  \
        (tvr)->down = (cx)->tempValueRooters;                                 \
        (cx)->tempValueRooters = (tvr);                                       \
    JS_END_MACRO

#define JS_POP_TEMP_ROOT(cx,tvr)                                              \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT((cx)->tempValueRooters == (tvr));                           \
        (cx)->tempValueRooters = (tvr)->down;                                 \
    JS_END_MACRO

#define JS_PUSH_TEMP_ROOT(cx,cnt,arr,tvr)                                     \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT((int)(cnt) >= 0);                                           \
        JS_PUSH_TEMP_ROOT_COMMON(cx, arr, tvr, (ptrdiff_t) (cnt), array);     \
    JS_END_MACRO

#define JS_PUSH_SINGLE_TEMP_ROOT(cx,val,tvr)                                  \
    JS_PUSH_TEMP_ROOT_COMMON(cx, val, tvr, JSTVU_SINGLE, value)

#define JS_PUSH_TEMP_ROOT_OBJECT(cx,obj,tvr)                                  \
    JS_PUSH_TEMP_ROOT_COMMON(cx, obj, tvr, JSTVU_SINGLE, object)

#define JS_PUSH_TEMP_ROOT_STRING(cx,str,tvr)                                  \
    JS_PUSH_SINGLE_TEMP_ROOT(cx, str ? STRING_TO_JSVAL(str) : JSVAL_NULL, tvr)

#define JS_PUSH_TEMP_ROOT_XML(cx,xml_,tvr)                                    \
    JS_PUSH_TEMP_ROOT_COMMON(cx, xml_, tvr, JSTVU_SINGLE, xml)

#define JS_PUSH_TEMP_ROOT_TRACE(cx,trace_,tvr)                                \
    JS_PUSH_TEMP_ROOT_COMMON(cx, trace_, tvr, JSTVU_TRACE, trace)

#define JS_PUSH_TEMP_ROOT_SPROP(cx,sprop_,tvr)                                \
    JS_PUSH_TEMP_ROOT_COMMON(cx, sprop_, tvr, JSTVU_SPROP, sprop)

#define JS_PUSH_TEMP_ROOT_WEAK_COPY(cx,weakRoots_,tvr)                        \
    JS_PUSH_TEMP_ROOT_COMMON(cx, weakRoots_, tvr, JSTVU_WEAK_ROOTS, weakRoots)

#define JS_PUSH_TEMP_ROOT_COMPILER(cx,pc,tvr)                                 \
    JS_PUSH_TEMP_ROOT_COMMON(cx, pc, tvr, JSTVU_COMPILER, compiler)

#define JS_PUSH_TEMP_ROOT_SCRIPT(cx,script_,tvr)                              \
    JS_PUSH_TEMP_ROOT_COMMON(cx, script_, tvr, JSTVU_SCRIPT, script)

#define JSRESOLVE_INFER         0xffff  /* infer bits from current bytecode */

struct JSContext {
    /*
     * If this flag is set, we were asked to call back the operation callback
     * as soon as possible.
     */
    volatile jsint      operationCallbackFlag;

    /* JSRuntime contextList linkage. */
    JSCList             link;

#if JS_HAS_XML_SUPPORT
    /*
     * Bit-set formed from binary exponentials of the XML_* tiny-ids defined
     * for boolean settings in jsxml.c, plus an XSF_CACHE_VALID bit.  Together
     * these act as a cache of the boolean XML.ignore* and XML.prettyPrinting
     * property values associated with this context's global object.
     */
    uint8               xmlSettingFlags;
    uint8               padding;
#else
    uint16              padding;
#endif

    /*
     * Classic Algol "display" static link optimization.
     */
#define JS_DISPLAY_SIZE 16U

    JSStackFrame        *display[JS_DISPLAY_SIZE];

    /* Runtime version control identifier. */
    uint16              version;

    /* Per-context options. */
    uint32              options;            /* see jsapi.h for JSOPTION_* */

    /* Locale specific callbacks for string conversion. */
    JSLocaleCallbacks   *localeCallbacks;

    /*
     * cx->resolvingTable is non-null and non-empty if we are initializing
     * standard classes lazily, or if we are otherwise recursing indirectly
     * from js_LookupProperty through a JSClass.resolve hook.  It is used to
     * limit runaway recursion (see jsapi.c and jsobj.c).
     */
    JSDHashTable        *resolvingTable;

    /*
     * True if generating an error, to prevent runaway recursion.
     * NB: generatingError packs with insideGCMarkCallback and throwing below.
     */
    JSPackedBool        generatingError;

    /* Flag to indicate that we run inside gcCallback(cx, JSGC_MARK_END). */
    JSPackedBool        insideGCMarkCallback;

    /* Exception state -- the exception member is a GC root by definition. */
    JSPackedBool        throwing;           /* is there a pending exception? */
    jsval               exception;          /* most-recently-thrown exception */

    /* Limit pointer for checking native stack consumption during recursion. */
    jsuword             stackLimit;

    /* Quota on the size of arenas used to compile and execute scripts. */
    size_t              scriptStackQuota;

    /* Data shared by threads in an address space. */
    JSRuntime * const   runtime;

    explicit JSContext(JSRuntime *rt) : runtime(rt) {}

    /* Stack arena pool and frame pointer register. */
    JS_REQUIRES_STACK
    JSArenaPool         stackPool;

    JS_REQUIRES_STACK
    JSStackFrame        *fp;

    /* Temporary arena pool used while compiling and decompiling. */
    JSArenaPool         tempPool;

    /* Top-level object and pointer to top stack frame's scope chain. */
    JSObject            *globalObject;

    /* Storage to root recently allocated GC things and script result. */
    JSWeakRoots         weakRoots;

    /* Regular expression class statics (XXX not shared globally). */
    JSRegExpStatics     regExpStatics;

    /* State for object and array toSource conversion. */
    JSSharpObjectMap    sharpObjectMap;
    JSHashTable         *busyArrayTable;

    /* Argument formatter support for JS_{Convert,Push}Arguments{,VA}. */
    JSArgumentFormatMap *argumentFormatMap;

    /* Last message string and trace file for debugging. */
    char                *lastMessage;
#ifdef DEBUG
    void                *tracefp;
    jsbytecode          *tracePrevPc;
#endif

    /* Per-context optional error reporter. */
    JSErrorReporter     errorReporter;

    /* Branch callback. */
    JSOperationCallback operationCallback;

    /* Interpreter activation count. */
    uintN               interpLevel;

    /* Client opaque pointers. */
    void                *data;
    void                *data2;

    /* GC and thread-safe state. */
    JSStackFrame        *dormantFrameChain; /* dormant stack frame to scan */
#ifdef JS_THREADSAFE
    JSThread            *thread;
    jsrefcount          requestDepth;
    /* Same as requestDepth but ignoring JS_SuspendRequest/JS_ResumeRequest */
    jsrefcount          outstandingRequests;
    JSTitle             *lockedSealedTitle; /* weak ref, for low-cost sealed
                                               title locking */
    JSCList             threadLinks;        /* JSThread contextList linkage */

#define CX_FROM_THREAD_LINKS(tl) \
    ((JSContext *)((char *)(tl) - offsetof(JSContext, threadLinks)))
#endif

    /* PDL of stack headers describing stack slots not rooted by argv, etc. */
    JSStackHeader       *stackHeaders;

    /* Stack of thread-stack-allocated temporary GC roots. */
    JSTempValueRooter   *tempValueRooters;

    /* Debug hooks associated with the current context. */
    const JSDebugHooks  *debugHooks;

    /* Security callbacks that override any defined on the runtime. */
    JSSecurityCallbacks *securityCallbacks;

    /* Pinned regexp pool used for regular expressions. */
    JSArenaPool         regexpPool;

    /* Stored here to avoid passing it around as a parameter. */
    uintN               resolveFlags;

#ifdef JS_TRACER
    /*
     * State for the current tree execution.  bailExit is valid if the tree has
     * called back into native code via a _FAIL builtin and has not yet bailed,
     * else garbage (NULL in debug builds).
     */
    InterpState         *interpState;
    VMSideExit          *bailExit;

    /*
     * True if traces may be executed. Invariant: The value of jitEnabled is
     * always equal to the expression in updateJITEnabled below.
     *
     * This flag and the fields accessed by updateJITEnabled are written only
     * in runtime->gcLock, to avoid race conditions that would leave the wrong
     * value in jitEnabled. (But the interpreter reads this without
     * locking. That can race against another thread setting debug hooks, but
     * we always read cx->debugHooks without locking anyway.)
     */
    bool                 jitEnabled;
#endif

    /* Caller must be holding runtime->gcLock. */
    void updateJITEnabled() {
#ifdef JS_TRACER
        jitEnabled = ((options & JSOPTION_JIT) &&
                      !runtime->debuggerInhibitsJIT() &&
                      debugHooks == &runtime->globalDebugHooks);
#endif
    }

#ifdef JS_THREADSAFE
    inline void createDeallocatorTask() {
        JS_ASSERT(!thread->deallocatorTask);
        if (runtime->deallocatorThread && !runtime->deallocatorThread->busy())
            thread->deallocatorTask = new JSFreePointerListTask();
    }

    inline void submitDeallocatorTask() {
        if (thread->deallocatorTask) {
            runtime->deallocatorThread->schedule(thread->deallocatorTask);
            thread->deallocatorTask = NULL;
        }
    }
#endif

    ptrdiff_t &getMallocCounter() {
#ifdef JS_THREADSAFE
        return thread->gcThreadMallocBytes;
#else
        return runtime->gcMallocBytes;
#endif
    }

    /*
     * Call this after allocating memory held by GC things, to update memory
     * pressure counters or report the OOM error if necessary.
     */
    inline void updateMallocCounter(void *p, size_t nbytes) {
        JS_ASSERT(ptrdiff_t(nbytes) >= 0);
        ptrdiff_t &counter = getMallocCounter();
        counter -= ptrdiff_t(nbytes);
        if (!p || counter <= 0)
            checkMallocGCPressure(p);
    }

    /*
     * Call this after successfully allocating memory held by GC things, to
     * update memory pressure counters.
     */
    inline void updateMallocCounter(size_t nbytes) {
        JS_ASSERT(ptrdiff_t(nbytes) >= 0);
        ptrdiff_t &counter = getMallocCounter();
        counter -= ptrdiff_t(nbytes);
        if (counter <= 0) {
            /*
             * Use 1 as an arbitrary non-null pointer indicating successful
             * allocation.
             */
            checkMallocGCPressure(reinterpret_cast<void *>(jsuword(1)));
        }
    }

    inline void* malloc(size_t bytes) {
        JS_ASSERT(bytes != 0);
        void *p = runtime->malloc(bytes);
        updateMallocCounter(p, bytes);
        return p;
    }

    inline void* mallocNoReport(size_t bytes) {
        JS_ASSERT(bytes != 0);
        void *p = runtime->malloc(bytes);
        if (!p)
            return NULL;
        updateMallocCounter(bytes);
        return p;
    }

    inline void* calloc(size_t bytes) {
        JS_ASSERT(bytes != 0);
        void *p = runtime->calloc(bytes);
        updateMallocCounter(p, bytes);
        return p;
    }

    inline void* realloc(void* p, size_t bytes) {
        void *orig = p;
        p = runtime->realloc(p, bytes);

        /*
         * For compatibility we do not account for realloc that increases
         * previously allocated memory.
         */
        updateMallocCounter(p, orig ? 0 : bytes);
        return p;
    }

#ifdef JS_THREADSAFE
    inline void free(void* p) {
        if (!p)
            return;
        if (thread) {
            JSFreePointerListTask* task = thread->deallocatorTask;
            if (task) {
                task->add(p);
                return;
            }
        }
        runtime->free(p);
    }
#else
    inline void free(void* p) {
        if (!p)
            return;
        runtime->free(p);
    }
#endif

    /*
     * In the common case that we'd like to allocate the memory for an object
     * with cx->malloc/free, we cannot use overloaded C++ operators (no
     * placement delete).  Factor the common workaround into one place.
     */
#define CREATE_BODY(parms)                                                    \
    void *memory = this->malloc(sizeof(T));                                   \
    if (!memory)                                                              \
        return NULL;                                                          \
    return new(memory) T parms;

    template <class T>
    JS_ALWAYS_INLINE T *create() {
        CREATE_BODY(())
    }

    template <class T, class P1>
    JS_ALWAYS_INLINE T *create(const P1 &p1) {
        CREATE_BODY((p1))
    }

    template <class T, class P1, class P2>
    JS_ALWAYS_INLINE T *create(const P1 &p1, const P2 &p2) {
        CREATE_BODY((p1, p2))
    }

    template <class T, class P1, class P2, class P3>
    JS_ALWAYS_INLINE T *create(const P1 &p1, const P2 &p2, const P3 &p3) {
        CREATE_BODY((p1, p2, p3))
    }
#undef CREATE_BODY

    template <class T>
    JS_ALWAYS_INLINE void destroy(T *p) {
        p->~T();
        this->free(p);
    }

private:

    /*
     * The allocation code calls the function to indicate either OOM failure
     * when p is null or that a memory pressure counter has reached some
     * threshold when p is not null. The function takes the pointer and not
     * a boolean flag to minimize the amount of code in its inlined callers.
     */
    void checkMallocGCPressure(void *p);
};

#ifdef JS_THREADSAFE
# define JS_THREAD_ID(cx)       ((cx)->thread ? (cx)->thread->id : 0)
#endif

#ifdef __cplusplus

static inline JSAtom **
FrameAtomBase(JSContext *cx, JSStackFrame *fp)
{
    return fp->imacpc
           ? COMMON_ATOMS_START(&cx->runtime->atomState)
           : fp->script->atomMap.vector;
}

/* FIXME(bug 332648): Move this into a public header. */
class JSAutoTempValueRooter
{
  public:
    JSAutoTempValueRooter(JSContext *cx, size_t len, jsval *vec
                          JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : mContext(cx) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_PUSH_TEMP_ROOT(mContext, len, vec, &mTvr);
    }
    explicit JSAutoTempValueRooter(JSContext *cx, jsval v = JSVAL_NULL
                                   JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : mContext(cx) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_PUSH_SINGLE_TEMP_ROOT(mContext, v, &mTvr);
    }
    JSAutoTempValueRooter(JSContext *cx, JSString *str
                          JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : mContext(cx) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_PUSH_TEMP_ROOT_STRING(mContext, str, &mTvr);
    }
    JSAutoTempValueRooter(JSContext *cx, JSObject *obj
                          JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : mContext(cx) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_PUSH_TEMP_ROOT_OBJECT(mContext, obj, &mTvr);
    }
    JSAutoTempValueRooter(JSContext *cx, JSScopeProperty *sprop
                          JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : mContext(cx) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_PUSH_TEMP_ROOT_SPROP(mContext, sprop, &mTvr);
    }

    ~JSAutoTempValueRooter() {
        JS_POP_TEMP_ROOT(mContext, &mTvr);
    }

    jsval value() { return mTvr.u.value; }
    jsval *addr() { return &mTvr.u.value; }

  protected:
    JSContext *mContext;

  private:
#ifndef AIX
    static void *operator new(size_t);
    static void operator delete(void *, size_t);
#endif

    JSTempValueRooter mTvr;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class JSAutoTempIdRooter
{
  public:
    explicit JSAutoTempIdRooter(JSContext *cx, jsid id = INT_TO_JSID(0)
                                JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : mContext(cx) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_PUSH_SINGLE_TEMP_ROOT(mContext, ID_TO_VALUE(id), &mTvr);
    }

    ~JSAutoTempIdRooter() {
        JS_POP_TEMP_ROOT(mContext, &mTvr);
    }

    jsid id() { return (jsid) mTvr.u.value; }
    jsid * addr() { return (jsid *) &mTvr.u.value; }

  private:
    JSContext *mContext;
    JSTempValueRooter mTvr;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class JSAutoIdArray {
  public:
    JSAutoIdArray(JSContext *cx, JSIdArray *ida
                  JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : cx(cx), idArray(ida) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        if (ida)
            JS_PUSH_TEMP_ROOT(cx, ida->length, ida->vector, &tvr);
    }
    ~JSAutoIdArray() {
        if (idArray) {
            JS_POP_TEMP_ROOT(cx, &tvr);
            JS_DestroyIdArray(cx, idArray);
        }
    }
    bool operator!() {
        return idArray == NULL;
    }
    jsid operator[](size_t i) const {
        JS_ASSERT(idArray);
        JS_ASSERT(i < size_t(idArray->length));
        return idArray->vector[i];
    }
    size_t length() const {
         return idArray->length;
    }
  private:
    JSContext * const cx;
    JSIdArray * const idArray;
    JSTempValueRooter tvr;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/* The auto-root for enumeration object and its state. */
class JSAutoEnumStateRooter : public JSTempValueRooter
{
  public:
    JSAutoEnumStateRooter(JSContext *cx, JSObject *obj, jsval *statep
                          JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : mContext(cx), mStatep(statep)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_ASSERT(obj);
        JS_ASSERT(statep);
        JS_PUSH_TEMP_ROOT_COMMON(cx, obj, this, JSTVU_ENUMERATOR, object);
    }

    ~JSAutoEnumStateRooter() {
        JS_POP_TEMP_ROOT(mContext, this);
    }

    void mark(JSTracer *trc) {
        JS_CALL_OBJECT_TRACER(trc, u.object, "enumerator_obj");
        js_MarkEnumeratorState(trc, u.object, *mStatep);
    }

  private:
    JSContext   *mContext;
    jsval       *mStatep;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class JSAutoResolveFlags
{
  public:
    JSAutoResolveFlags(JSContext *cx, uintN flags
                       JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : mContext(cx), mSaved(cx->resolveFlags) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        cx->resolveFlags = flags;
    }

    ~JSAutoResolveFlags() { mContext->resolveFlags = mSaved; }

  private:
    JSContext *mContext;
    uintN mSaved;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

#endif /* __cpluscplus */

/*
 * Slightly more readable macros for testing per-context option settings (also
 * to hide bitset implementation detail).
 *
 * JSOPTION_XML must be handled specially in order to propagate from compile-
 * to run-time (from cx->options to script->version/cx->version).  To do that,
 * we copy JSOPTION_XML from cx->options into cx->version as JSVERSION_HAS_XML
 * whenever options are set, and preserve this XML flag across version number
 * changes done via the JS_SetVersion API.
 *
 * But when executing a script or scripted function, the interpreter changes
 * cx->version, including the XML flag, to script->version.  Thus JSOPTION_XML
 * is a compile-time option that causes a run-time version change during each
 * activation of the compiled script.  That version change has the effect of
 * changing JS_HAS_XML_OPTION, so that any compiling done via eval enables XML
 * support.  If an XML-enabled script or function calls a non-XML function,
 * the flag bit will be cleared during the callee's activation.
 *
 * Note that JS_SetVersion API calls never pass JSVERSION_HAS_XML or'd into
 * that API's version parameter.
 *
 * Note also that script->version must contain this XML option flag in order
 * for XDR'ed scripts to serialize and deserialize with that option preserved
 * for detection at run-time.  We can't copy other compile-time options into
 * script->version because that would break backward compatibility (certain
 * other options, e.g. JSOPTION_VAROBJFIX, are analogous to JSOPTION_XML).
 */
#define JS_HAS_OPTION(cx,option)        (((cx)->options & (option)) != 0)
#define JS_HAS_STRICT_OPTION(cx)        JS_HAS_OPTION(cx, JSOPTION_STRICT)
#define JS_HAS_WERROR_OPTION(cx)        JS_HAS_OPTION(cx, JSOPTION_WERROR)
#define JS_HAS_COMPILE_N_GO_OPTION(cx)  JS_HAS_OPTION(cx, JSOPTION_COMPILE_N_GO)
#define JS_HAS_ATLINE_OPTION(cx)        JS_HAS_OPTION(cx, JSOPTION_ATLINE)

#define JSVERSION_MASK                  0x0FFF  /* see JSVersion in jspubtd.h */
#define JSVERSION_HAS_XML               0x1000  /* flag induced by XML option */
#define JSVERSION_ANONFUNFIX            0x2000  /* see jsapi.h, the comments
                                                   for JSOPTION_ANONFUNFIX */

#define JSVERSION_NUMBER(cx)            ((JSVersion)((cx)->version &          \
                                                     JSVERSION_MASK))
#define JS_HAS_XML_OPTION(cx)           ((cx)->version & JSVERSION_HAS_XML || \
                                         JSVERSION_NUMBER(cx) >= JSVERSION_1_6)

extern JSThreadData *
js_CurrentThreadData(JSRuntime *rt);

extern JSBool
js_InitThreads(JSRuntime *rt);

extern void
js_FinishThreads(JSRuntime *rt);

extern void
js_PurgeThreads(JSContext *cx);

extern void
js_TraceThreads(JSRuntime *rt, JSTracer *trc);

/*
 * Ensures the JSOPTION_XML and JSOPTION_ANONFUNFIX bits of cx->options are
 * reflected in cx->version, since each bit must travel with a script that has
 * it set.
 */
extern void
js_SyncOptionsToVersion(JSContext *cx);

/*
 * Common subroutine of JS_SetVersion and js_SetVersion, to update per-context
 * data that depends on version.
 */
extern void
js_OnVersionChange(JSContext *cx);

/*
 * Unlike the JS_SetVersion API, this function stores JSVERSION_HAS_XML and
 * any future non-version-number flags induced by compiler options.
 */
extern void
js_SetVersion(JSContext *cx, JSVersion version);

/*
 * Create and destroy functions for JSContext, which is manually allocated
 * and exclusively owned.
 */
extern JSContext *
js_NewContext(JSRuntime *rt, size_t stackChunkSize);

extern void
js_DestroyContext(JSContext *cx, JSDestroyContextMode mode);

/*
 * Return true if cx points to a context in rt->contextList, else return false.
 * NB: the caller (see jslock.c:ClaimTitle) must hold rt->gcLock.
 */
extern JSBool
js_ValidContextPointer(JSRuntime *rt, JSContext *cx);

static JS_INLINE JSContext *
js_ContextFromLinkField(JSCList *link)
{
    JS_ASSERT(link);
    return (JSContext *) ((uint8 *) link - offsetof(JSContext, link));
}

/*
 * If unlocked, acquire and release rt->gcLock around *iterp update; otherwise
 * the caller must be holding rt->gcLock.
 */
extern JSContext *
js_ContextIterator(JSRuntime *rt, JSBool unlocked, JSContext **iterp);

/*
 * Iterate through contexts with active requests. The caller must be holding
 * rt->gcLock in case of a thread-safe build, or otherwise guarantee that the
 * context list is not alternated asynchroniously.
 */
extern JS_FRIEND_API(JSContext *)
js_NextActiveContext(JSRuntime *, JSContext *);

#ifdef JS_THREADSAFE

/*
 * Count the number of contexts entered requests on the current thread.
 */
uint32
js_CountThreadRequests(JSContext *cx);

/*
 * This is a helper for code at can potentially run outside JS request to
 * ensure that the GC is not running when the function returns.
 *
 * This function must be called with the GC lock held.
 */
extern void
js_WaitForGC(JSRuntime *rt);

/*
 * If we're in one or more requests (possibly on more than one context)
 * running on the current thread, indicate, temporarily, that all these
 * requests are inactive so a possible GC can proceed on another thread.
 * This function returns the number of discounted requests. The number must
 * be passed later to js_ActivateRequestAfterGC to reactivate the requests.
 *
 * This function must be called with the GC lock held.
 */
uint32
js_DiscountRequestsForGC(JSContext *cx);

/*
 * This function must be called with the GC lock held.
 */
void
js_RecountRequestsAfterGC(JSRuntime *rt, uint32 requestDebit);

#else /* !JS_THREADSAFE */

# define js_WaitForGC(rt)    ((void) 0)

#endif

/*
 * JSClass.resolve and watchpoint recursion damping machinery.
 */
extern JSBool
js_StartResolving(JSContext *cx, JSResolvingKey *key, uint32 flag,
                  JSResolvingEntry **entryp);

extern void
js_StopResolving(JSContext *cx, JSResolvingKey *key, uint32 flag,
                 JSResolvingEntry *entry, uint32 generation);

/*
 * Local root set management.
 *
 * NB: the jsval parameters below may be properly tagged jsvals, or GC-thing
 * pointers cast to (jsval).  This relies on JSObject's tag being zero, but
 * on the up side it lets us push int-jsval-encoded scopeMark values on the
 * local root stack.
 */
extern JSBool
js_EnterLocalRootScope(JSContext *cx);

#define js_LeaveLocalRootScope(cx) \
    js_LeaveLocalRootScopeWithResult(cx, JSVAL_NULL)

extern void
js_LeaveLocalRootScopeWithResult(JSContext *cx, jsval rval);

extern void
js_ForgetLocalRoot(JSContext *cx, jsval v);

extern int
js_PushLocalRoot(JSContext *cx, JSLocalRootStack *lrs, jsval v);

/*
 * Report an exception, which is currently realized as a printf-style format
 * string and its arguments.
 */
typedef enum JSErrNum {
#define MSG_DEF(name, number, count, exception, format) \
    name = number,
#include "js.msg"
#undef MSG_DEF
    JSErr_Limit
} JSErrNum;

extern JS_FRIEND_API(const JSErrorFormatString *)
js_GetErrorMessage(void *userRef, const char *locale, const uintN errorNumber);

#ifdef va_start
extern JSBool
js_ReportErrorVA(JSContext *cx, uintN flags, const char *format, va_list ap);

extern JSBool
js_ReportErrorNumberVA(JSContext *cx, uintN flags, JSErrorCallback callback,
                       void *userRef, const uintN errorNumber,
                       JSBool charArgs, va_list ap);

extern JSBool
js_ExpandErrorArguments(JSContext *cx, JSErrorCallback callback,
                        void *userRef, const uintN errorNumber,
                        char **message, JSErrorReport *reportp,
                        bool charArgs, va_list ap);
#endif

extern void
js_ReportOutOfMemory(JSContext *cx);

/*
 * Report that cx->scriptStackQuota is exhausted.
 */
extern void
js_ReportOutOfScriptQuota(JSContext *cx);

extern void
js_ReportOverRecursed(JSContext *cx);

extern void
js_ReportAllocationOverflow(JSContext *cx);

#define JS_CHECK_RECURSION(cx, onerror)                                       \
    JS_BEGIN_MACRO                                                            \
        int stackDummy_;                                                      \
                                                                              \
        if (!JS_CHECK_STACK_SIZE(cx, stackDummy_)) {                          \
            js_ReportOverRecursed(cx);                                        \
            onerror;                                                          \
        }                                                                     \
    JS_END_MACRO

/*
 * Report an exception using a previously composed JSErrorReport.
 * XXXbe remove from "friend" API
 */
extern JS_FRIEND_API(void)
js_ReportErrorAgain(JSContext *cx, const char *message, JSErrorReport *report);

extern void
js_ReportIsNotDefined(JSContext *cx, const char *name);

/*
 * Report an attempt to access the property of a null or undefined value (v).
 */
extern JSBool
js_ReportIsNullOrUndefined(JSContext *cx, intN spindex, jsval v,
                           JSString *fallback);

extern void
js_ReportMissingArg(JSContext *cx, jsval *vp, uintN arg);

/*
 * Report error using js_DecompileValueGenerator(cx, spindex, v, fallback) as
 * the first argument for the error message. If the error message has less
 * then 3 arguments, use null for arg1 or arg2.
 */
extern JSBool
js_ReportValueErrorFlags(JSContext *cx, uintN flags, const uintN errorNumber,
                         intN spindex, jsval v, JSString *fallback,
                         const char *arg1, const char *arg2);

#define js_ReportValueError(cx,errorNumber,spindex,v,fallback)                \
    ((void)js_ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,          \
                                    spindex, v, fallback, NULL, NULL))

#define js_ReportValueError2(cx,errorNumber,spindex,v,fallback,arg1)          \
    ((void)js_ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,          \
                                    spindex, v, fallback, arg1, NULL))

#define js_ReportValueError3(cx,errorNumber,spindex,v,fallback,arg1,arg2)     \
    ((void)js_ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,          \
                                    spindex, v, fallback, arg1, arg2))

extern JSErrorFormatString js_ErrorFormatString[JSErr_Limit];

/*
 * See JS_SetThreadStackLimit in jsapi.c, where we check that the stack grows
 * in the expected direction.  On Unix-y systems, JS_STACK_GROWTH_DIRECTION is
 * computed on the build host by jscpucfg.c and written into jsautocfg.h.  The
 * macro is hardcoded in jscpucfg.h on Windows and Mac systems (for historical
 * reasons pre-dating autoconf usage).
 */
#if JS_STACK_GROWTH_DIRECTION > 0
# define JS_CHECK_STACK_SIZE(cx, lval)  ((jsuword)&(lval) < (cx)->stackLimit)
#else
# define JS_CHECK_STACK_SIZE(cx, lval)  ((jsuword)&(lval) > (cx)->stackLimit)
#endif

/*
 * If the operation callback flag was set, call the operation callback.
 * This macro can run the full GC. Return true if it is OK to continue and
 * false otherwise.
 */
#define JS_CHECK_OPERATION_LIMIT(cx) \
    (!(cx)->operationCallbackFlag || js_InvokeOperationCallback(cx))

/*
 * Invoke the operation callback and return false if the current execution
 * is to be terminated.
 */
extern JSBool
js_InvokeOperationCallback(JSContext *cx);

#ifndef JS_THREADSAFE
# define js_TriggerAllOperationCallbacks(rt, gcLocked) \
    js_TriggerAllOperationCallbacks (rt)
#endif

void
js_TriggerAllOperationCallbacks(JSRuntime *rt, JSBool gcLocked);

extern JSStackFrame *
js_GetScriptedCaller(JSContext *cx, JSStackFrame *fp);

extern jsbytecode*
js_GetCurrentBytecodePC(JSContext* cx);

extern bool
js_CurrentPCIsInImacro(JSContext *cx);

#ifdef JS_TRACER
/*
 * Reconstruct the JS stack and clear cx->tracecx. We must be currently in a
 * _FAIL builtin from trace on cx or another context on the same thread. The
 * machine code for the trace remains on the C stack when js_DeepBail returns.
 *
 * Implemented in jstracer.cpp.
 */
JS_FORCES_STACK JS_FRIEND_API(void)
js_DeepBail(JSContext *cx);
#endif

static JS_FORCES_STACK JS_INLINE void
js_LeaveTrace(JSContext *cx)
{
#ifdef JS_TRACER
    if (JS_ON_TRACE(cx))
        js_DeepBail(cx);
#endif
}

static JS_INLINE void
js_LeaveTraceIfGlobalObject(JSContext *cx, JSObject *obj)
{
    if (!obj->fslots[JSSLOT_PARENT])
        js_LeaveTrace(cx);
}

static JS_INLINE JSBool
js_CanLeaveTrace(JSContext *cx)
{
    JS_ASSERT(JS_ON_TRACE(cx));
#ifdef JS_TRACER
    return cx->bailExit != NULL;
#else
    return JS_FALSE;
#endif
}

/*
 * Get the current cx->fp, first lazily instantiating stack frames if needed.
 * (Do not access cx->fp directly except in JS_REQUIRES_STACK code.)
 *
 * Defined in jstracer.cpp if JS_TRACER is defined.
 */
static JS_FORCES_STACK JS_INLINE JSStackFrame *
js_GetTopStackFrame(JSContext *cx)
{
    js_LeaveTrace(cx);
    return cx->fp;
}

static JS_INLINE JSBool
js_IsPropertyCacheDisabled(JSContext *cx)
{
    return cx->runtime->shapeGen >= SHAPE_OVERFLOW_BIT;
}

static JS_INLINE uint32
js_RegenerateShapeForGC(JSContext *cx)
{
    JS_ASSERT(cx->runtime->gcRunning);
    JS_ASSERT(cx->runtime->gcRegenShapes);

    /*
     * Under the GC, compared with js_GenerateShape, we don't need to use
     * atomic increments but we still must make sure that after an overflow
     * the shape stays such.
     */
    uint32 shape = cx->runtime->shapeGen;
    shape = (shape + 1) | (shape & SHAPE_OVERFLOW_BIT);
    cx->runtime->shapeGen = shape;
    return shape;
}

namespace js {

/*
 * Policy that calls JSContext:: memory functions and reports errors to the
 * context.  Since the JSContext* given on construction is stored for the
 * lifetime of the container, this policy may only be used for containers whose
 * lifetime is a shorter than the given JSContext.
 */
class ContextAllocPolicy
{
    JSContext *mCx;

  public:
    ContextAllocPolicy(JSContext *cx) : mCx(cx) {}
    JSContext *context() const { return mCx; }

    void *malloc(size_t bytes) { return mCx->malloc(bytes); }
    void free(void *p) { mCx->free(p); }
    void *realloc(void *p, size_t bytes) { return mCx->realloc(p, bytes); }
    void reportAllocOverflow() const { js_ReportAllocationOverflow(mCx); }
};

}

#endif /* jscntxt_h___ */

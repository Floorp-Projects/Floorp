/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is SpiderMonkey code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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

#ifndef jscompartment_h___
#define jscompartment_h___

#include "jsclist.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsgcstats.h"
#include "jsobj.h"
#include "vm/GlobalObject.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251) /* Silence warning about JS_FRIEND_API and data members. */
#endif

namespace JSC { class ExecutableAllocator; }
namespace WTF { class BumpPointerAllocator; }

namespace js {

/* Holds the number of recording attemps for an address. */
typedef HashMap<jsbytecode*,
                size_t,
                DefaultHasher<jsbytecode*>,
                SystemAllocPolicy> RecordAttemptMap;

/* Holds the profile data for loops. */
typedef HashMap<jsbytecode*,
                LoopProfile*,
                DefaultHasher<jsbytecode*>,
                SystemAllocPolicy> LoopProfileMap;

class Oracle;

typedef HashSet<JSScript *,
                DefaultHasher<JSScript *>,
                SystemAllocPolicy> TracedScriptSet;

typedef HashMap<JSFunction *,
                JSString *,
                DefaultHasher<JSFunction *>,
                SystemAllocPolicy> ToSourceCache;

struct TraceMonitor;

/* Holds the execution state during trace execution. */
struct TracerState
{
    JSContext*     cx;                  // current VM context handle
    TraceMonitor*  traceMonitor;        // current TM
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
    TreeFragment*  outermostTree;       // the outermost tree we initially invoked
    VMSideExit**   innermostNestedGuardp;
    VMSideExit*    innermost;
    uint64         startTime;
    TracerState*   prev;

    // Used by _FAIL builtins; see jsbuiltins.h. The builtin sets the
    // JSBUILTIN_BAILED bit if it bails off trace and the JSBUILTIN_ERROR bit
    // if an error or exception occurred.
    uint32         builtinStatus;

    // Used to communicate the location of the return value in case of a deep bail.
    double*        deepBailSp;

    // Used when calling natives from trace to root the vp vector.
    uintN          nativeVpLen;
    js::Value*     nativeVp;

    TracerState(JSContext *cx, TraceMonitor *tm, TreeFragment *ti,
                VMSideExit** innermostNestedGuardp);
    ~TracerState();
};

/*
 * Storage for the execution state and store during trace execution. Generated
 * code depends on the fact that the globals begin |MAX_NATIVE_STACK_SLOTS|
 * doubles after the stack begins. Thus, on trace, |TracerState::eos| holds a
 * pointer to the first global.
 */
struct TraceNativeStorage
{
    /* Max number of stack slots/frame that may need to be restored in LeaveTree. */
    static const size_t MAX_NATIVE_STACK_SLOTS  = 4096;
    static const size_t MAX_CALL_STACK_ENTRIES  = 500;

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
 * Trace monitor. Every JSCompartment has an associated trace monitor
 * that keeps track of loop frequencies for all JavaScript code loaded
 * into that runtime.
 */
struct TraceMonitor {
    /*
     * The context currently executing JIT-compiled code in this compartment, or
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
     * State for the current tree execution.  bailExit is valid if the tree has
     * called back into native code via a _FAIL builtin and has not yet bailed,
     * else garbage (NULL in debug builds).
     */
    js::TracerState     *tracerState;
    js::VMSideExit      *bailExit;

    /* Counts the number of iterations run by the currently executing trace. */
    unsigned                iterationCounter;

    /*
     * Cached storage to use when executing on trace. While we may enter nested
     * traces, we always reuse the outer trace's storage, so never need more
     * than of these.
     */
    TraceNativeStorage      *storage;

    /*
     * There are 4 allocators here.  This might seem like overkill, but they
     * have different lifecycles, and by keeping them separate we keep the
     * amount of retained memory down significantly.  They are flushed (ie.
     * all the allocated memory is freed) periodically.
     *
     * - dataAlloc has the lifecycle of the monitor.  It's flushed only when
     *   the monitor is flushed.  It's used for fragments.
     *
     * - traceAlloc has the same flush lifecycle as the dataAlloc, but it is
     *   also *marked* when a recording starts and rewinds to the mark point
     *   if recording aborts.  So you can put things in it that are only
     *   reachable on a successful record/compile cycle like GuardRecords and
     *   SideExits.
     *
     * - tempAlloc is flushed after each recording, successful or not.  It's
     *   used to store LIR code and for all other elements in the LIR
     *   pipeline.
     *
     * - codeAlloc has the same lifetime as dataAlloc, but its API is
     *   different (CodeAlloc vs. VMAllocator).  It's used for native code.
     *   It's also a good idea to keep code and data separate to avoid I-cache
     *   vs. D-cache issues.
     */
    VMAllocator*            dataAlloc;
    VMAllocator*            traceAlloc;
    VMAllocator*            tempAlloc;
    nanojit::CodeAlloc*     codeAlloc;
    nanojit::Assembler*     assembler;
    FrameInfoCache*         frameCache;

    /* This gets incremented every time the monitor is flushed. */
    uintN                   flushEpoch;

    Oracle*                 oracle;
    TraceRecorder*          recorder;

    /* If we are profiling a loop, this tracks the current profile. Otherwise NULL. */
    LoopProfile*            profile;

    GlobalState             globalStates[MONITOR_N_GLOBAL_STATES];
    TreeFragment            *vmfragments[FRAGMENT_TABLE_SIZE];
    RecordAttemptMap*       recordAttempts;

    /* A hashtable mapping PC values to loop profiles for those loops. */
    LoopProfileMap*         loopProfiles;

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

    // Cached temporary typemap to avoid realloc'ing every time we create one.
    // This must be used in only one place at a given time. It must be cleared
    // before use.
    TypeMap*                cachedTempTypeMap;

    /* Scripts with recorded fragments. */
    TracedScriptSet         tracedScripts;

#ifdef DEBUG
    /* Fields needed for fragment/guard profiling. */
    nanojit::Seq<nanojit::Fragment*>* branches;
    uint32                  lastFragID;
    VMAllocator*            profAlloc;
    FragStatsMap*           profTab;

    void logFragProfile();
#endif

    TraceMonitor();
    ~TraceMonitor();

    bool init(JSRuntime* rt);

    bool ontrace() const {
        return !!tracecx;
    }

    /* Flush the JIT cache. */
    void flush();

    /* Sweep any cache entry pointing to dead GC things. */
    void sweep(JSContext *cx);

    /* Mark any tracer stacks that are active. */
    void mark(JSTracer *trc);

    bool outOfMemory() const;

    JS_FRIEND_API(void) getCodeAllocStats(size_t &total, size_t &frag_size, size_t &free_size) const;
    JS_FRIEND_API(size_t) getVMAllocatorsMainSize() const;
    JS_FRIEND_API(size_t) getVMAllocatorsReserveSize() const;
    JS_FRIEND_API(size_t) getTraceMonitorSize() const;
};

namespace mjit {
class JaegerCompartment;
}
}

/* Defined in jsapi.cpp */
extern JSClass js_dummy_class;

/* Number of potentially reusable scriptsToGC to search for the eval cache. */
#ifndef JS_EVAL_CACHE_SHIFT
# define JS_EVAL_CACHE_SHIFT        6
#endif
#define JS_EVAL_CACHE_SIZE          JS_BIT(JS_EVAL_CACHE_SHIFT)

namespace js {

class NativeIterCache {
    static const size_t SIZE = size_t(1) << 8;
    
    /* Cached native iterators. */
    JSObject            *data[SIZE];

    static size_t getIndex(uint32 key) {
        return size_t(key) % SIZE;
    }

  public:
    /* Native iterator most recently started. */
    JSObject            *last;

    NativeIterCache()
      : last(NULL) {
        PodArrayZero(data);
    }

    void purge() {
        PodArrayZero(data);
        last = NULL;
    }

    JSObject *get(uint32 key) const {
        return data[getIndex(key)];
    }

    void set(uint32 key, JSObject *iterobj) {
        data[getIndex(key)] = iterobj;
    }
};

class MathCache;

/*
 * A single-entry cache for some base-10 double-to-string conversions. This
 * helps date-format-xparb.js.  It also avoids skewing the results for
 * v8-splay.js when measured by the SunSpider harness, where the splay tree
 * initialization (which includes many repeated double-to-string conversions)
 * is erroneously included in the measurement; see bug 562553.
 */
class DtoaCache {
    double        d;
    jsint         base;
    JSFixedString *s;      // if s==NULL, d and base are not valid
  public:
    DtoaCache() : s(NULL) {}
    void purge() { s = NULL; }

    JSFixedString *lookup(jsint base, double d) {
        return this->s && base == this->base && d == this->d ? this->s : NULL;
    }

    void cache(jsint base, double d, JSFixedString *s) {
        this->base = base;
        this->d = d;
        this->s = s;
    }

};

struct ScriptFilenameEntry
{
    bool marked;
    char filename[1];
};

struct ScriptFilenameHasher
{
    typedef const char *Lookup;
    static HashNumber hash(const char *l) { return JS_HashString(l); }
    static bool match(const ScriptFilenameEntry *e, const char *l) {
        return strcmp(e->filename, l) == 0;
    }
};

typedef HashSet<ScriptFilenameEntry *,
                ScriptFilenameHasher,
                SystemAllocPolicy> ScriptFilenameTable;

} /* namespace js */

struct JS_FRIEND_API(JSCompartment) {
    JSRuntime                    *rt;
    JSPrincipals                 *principals;

    js::gc::ArenaList            arenas[js::gc::FINALIZE_LIMIT];
    js::gc::FreeLists            freeLists;

    uint32                       gcBytes;
    uint32                       gcTriggerBytes;
    size_t                       gcLastBytes;

    bool                         hold;
    bool                         isSystemCompartment;

#ifdef JS_TRACER
  private:
    /*
     * Trace-tree JIT recorder/interpreter state.  It's created lazily because
     * many compartments don't end up needing it.
     */
    js::TraceMonitor             *traceMonitor_;
#endif

  public:
    /* Hashed lists of scripts created by eval to garbage-collect. */
    JSScript                     *scriptsToGC[JS_EVAL_CACHE_SIZE];

    void                         *data;
    bool                         active;  // GC flag, whether there are active frames
    bool                         hasDebugModeCodeToDrop;
    js::WrapperMap               crossCompartmentWrappers;

#ifdef JS_METHODJIT
  private:
    /* This is created lazily because many compartments don't need it. */
    js::mjit::JaegerCompartment  *jaegerCompartment_;
    /*
     * This function is here so that xpconnect/src/xpcjsruntime.cpp doesn't
     * need to see the declaration of JaegerCompartment, which would require
     * #including MethodJIT.h into xpconnect/src/xpcjsruntime.cpp, which is
     * difficult due to reasons explained in bug 483677.
     */
  public:
    bool hasJaegerCompartment() {
        return !!jaegerCompartment_;
    }

    js::mjit::JaegerCompartment *jaegerCompartment() const {
        JS_ASSERT(jaegerCompartment_);
        return jaegerCompartment_;
    }

    bool ensureJaegerCompartmentExists(JSContext *cx);

    size_t getMjitCodeSize() const;
#endif
    WTF::BumpPointerAllocator    *regExpAllocator;

    /*
     * Shared scope property tree, and arena-pool for allocating its nodes.
     */
    js::PropertyTree             propertyTree;

#ifdef DEBUG
    /* Property metering. */
    jsrefcount                   livePropTreeNodes;
    jsrefcount                   totalPropTreeNodes;
    jsrefcount                   propTreeKidsChunks;
    jsrefcount                   liveDictModeNodes;
#endif

    /*
     * Runtime-shared empty scopes for well-known built-in objects that lack
     * class prototypes (the usual locus of an emptyShape). Mnemonic: ABCDEW
     */
    js::EmptyShape               *emptyArgumentsShape;
    js::EmptyShape               *emptyBlockShape;
    js::EmptyShape               *emptyCallShape;
    js::EmptyShape               *emptyDeclEnvShape;
    js::EmptyShape               *emptyEnumeratorShape;
    js::EmptyShape               *emptyWithShape;

    typedef js::HashSet<js::EmptyShape *,
                        js::DefaultHasher<js::EmptyShape *>,
                        js::SystemAllocPolicy> EmptyShapeSet;

    EmptyShapeSet                emptyShapes;

    /*
     * Initial shapes given to RegExp and String objects, encoding the initial
     * sets of built-in instance properties and the fixed slots where they must
     * be stored (see JSObject::JSSLOT_(REGEXP|STRING)_*). Later property
     * additions may cause these shapes to not be used by a RegExp or String
     * (even along the entire shape parent chain, should the object go into
     * dictionary mode). But because all the initial properties are
     * non-configurable, they will always map to fixed slots.
     */
    const js::Shape              *initialRegExpShape;
    const js::Shape              *initialStringShape;

  private:
    enum { DebugFromC = 1, DebugFromJS = 2 };

    uintN                        debugModeBits;  // see debugMode() below

  public:
    JSCList                      scripts;        // scripts in this compartment

    js::NativeIterCache          nativeIterCache;

    typedef js::Maybe<js::ToSourceCache> LazyToSourceCache;
    LazyToSourceCache            toSourceCache;

    js::ScriptFilenameTable      scriptFilenameTable;

    JSCompartment(JSRuntime *rt);
    ~JSCompartment();

    bool init();

    /* Mark cross-compartment wrappers. */
    void markCrossCompartmentWrappers(JSTracer *trc);

    bool wrap(JSContext *cx, js::Value *vp);
    bool wrap(JSContext *cx, JSString **strp);
    bool wrap(JSContext *cx, JSObject **objp);
    bool wrapId(JSContext *cx, jsid *idp);
    bool wrap(JSContext *cx, js::PropertyOp *op);
    bool wrap(JSContext *cx, js::StrictPropertyOp *op);
    bool wrap(JSContext *cx, js::PropertyDescriptor *desc);
    bool wrap(JSContext *cx, js::AutoIdVector &props);

    void sweep(JSContext *cx, uint32 releaseInterval);
    void purge(JSContext *cx);
    void finishArenaLists();
    void finalizeObjectArenaLists(JSContext *cx);
    void finalizeStringArenaLists(JSContext *cx);
    void finalizeShapeArenaLists(JSContext *cx);
    bool arenaListsAreEmpty();

    void setGCLastBytes(size_t lastBytes, JSGCInvocationKind gckind);
    void reduceGCTriggerBytes(uint32 amount);

    js::DtoaCache dtoaCache;

  private:
    js::MathCache                *mathCache;

    js::MathCache *allocMathCache(JSContext *cx);

    typedef js::HashMap<jsbytecode*,
                        size_t,
                        js::DefaultHasher<jsbytecode*>,
                        js::SystemAllocPolicy> BackEdgeMap;

    BackEdgeMap                  backEdgeTable;

    /*
     * Weak reference to each global in this compartment that is a debuggee.
     * Each global has its own list of debuggers.
     */
    js::GlobalObjectSet              debuggees;

  public:
    js::BreakpointSiteMap            breakpointSites;

  private:
    JSCompartment *thisForCtor() { return this; }

  public:
    js::MathCache *getMathCache(JSContext *cx) {
        return mathCache ? mathCache : allocMathCache(cx);
    }

#ifdef JS_TRACER
    bool hasTraceMonitor() {
        return !!traceMonitor_;
    }

    js::TraceMonitor *allocAndInitTraceMonitor(JSContext *cx);

    js::TraceMonitor *traceMonitor() const {
        JS_ASSERT(traceMonitor_);
        return traceMonitor_;
    }
#endif

    size_t backEdgeCount(jsbytecode *pc) const;
    size_t incBackEdgeCount(jsbytecode *pc);

    /*
     * There are dueling APIs for debug mode. It can be enabled or disabled via
     * JS_SetDebugModeForCompartment. It is automatically enabled and disabled
     * by Debugger objects. Therefore debugModeBits has the DebugFromC bit set
     * if the C API wants debug mode and the DebugFromJS bit set if debuggees
     * is non-empty.
     */
    bool debugMode() const { return !!debugModeBits; }

    /*
     * True if any scripts from this compartment are on the JS stack in the
     * calling thread. cx is a context in the calling thread, and it is assumed
     * that no other thread is using this compartment.
     */
    bool hasScriptsOnStack(JSContext *cx);

  private:
    /* This is called only when debugMode() has just toggled. */
    void updateForDebugMode(JSContext *cx);

  public:
    js::GlobalObjectSet &getDebuggees() { return debuggees; }
    bool addDebuggee(JSContext *cx, js::GlobalObject *global);
    void removeDebuggee(JSContext *cx, js::GlobalObject *global,
                        js::GlobalObjectSet::Enum *debuggeesEnum = NULL);
    bool setDebugModeFromC(JSContext *cx, bool b);

    js::BreakpointSite *getBreakpointSite(jsbytecode *pc);
    js::BreakpointSite *getOrCreateBreakpointSite(JSContext *cx, JSScript *script, jsbytecode *pc,
                                                  JSObject *scriptObject);
    void clearBreakpointsIn(JSContext *cx, js::Debugger *dbg, JSScript *script, JSObject *handler);
    void clearTraps(JSContext *cx, JSScript *script);
    bool markTrapClosuresIteratively(JSTracer *trc);

  private:
    void sweepBreakpoints(JSContext *cx);

  public:
    js::WatchpointMap *watchpointMap;
};

#define JS_SCRIPTS_TO_GC(cx)    ((cx)->compartment->scriptsToGC)
#define JS_PROPERTY_TREE(cx)    ((cx)->compartment->propertyTree)

/*
 * N.B. JS_ON_TRACE(cx) is true if JIT code is on the stack in the current
 * thread, regardless of whether cx is the context in which that trace is
 * executing. cx must be a context on the current thread.
 */
static inline bool
JS_ON_TRACE(const JSContext *cx)
{
#ifdef JS_TRACER
    if (JS_THREAD_DATA(cx)->onTraceCompartment)
        return JS_THREAD_DATA(cx)->onTraceCompartment->traceMonitor()->ontrace();
#endif
    return false;
}

#ifdef JS_TRACER
static inline js::TraceMonitor *
JS_TRACE_MONITOR_ON_TRACE(JSContext *cx)
{
    JS_ASSERT(JS_ON_TRACE(cx));
    return JS_THREAD_DATA(cx)->onTraceCompartment->traceMonitor();
}

/*
 * Only call this directly from the interpreter loop or the method jit.
 * Otherwise, we may get the wrong compartment, and thus the wrong
 * TraceMonitor.
 */
static inline js::TraceMonitor *
JS_TRACE_MONITOR_FROM_CONTEXT(JSContext *cx)
{
    return cx->compartment->traceMonitor();
}

/*
 * This one also creates the TraceMonitor if it doesn't already exist.
 * It returns NULL if the lazy creation fails due to OOM.
 */
static inline js::TraceMonitor *
JS_TRACE_MONITOR_FROM_CONTEXT_WITH_LAZY_INIT(JSContext *cx)
{
    if (!cx->compartment->hasTraceMonitor())
        return cx->compartment->allocAndInitTraceMonitor(cx);
        
    return cx->compartment->traceMonitor();
}
#endif

static inline js::TraceRecorder *
TRACE_RECORDER(JSContext *cx)
{
#ifdef JS_TRACER
    if (JS_THREAD_DATA(cx)->recordingCompartment)
        return JS_THREAD_DATA(cx)->recordingCompartment->traceMonitor()->recorder;
#endif
    return NULL;
}

static inline js::LoopProfile *
TRACE_PROFILER(JSContext *cx)
{
#ifdef JS_TRACER
    if (JS_THREAD_DATA(cx)->profilingCompartment)
        return JS_THREAD_DATA(cx)->profilingCompartment->traceMonitor()->profile;
#endif
    return NULL;
}

namespace js {
static inline MathCache *
GetMathCache(JSContext *cx)
{
    return cx->compartment->getMathCache(cx);
}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace js {

class PreserveCompartment {
  protected:
    JSContext *cx;
  private:
    JSCompartment *oldCompartment;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
  public:
     PreserveCompartment(JSContext *cx JS_GUARD_OBJECT_NOTIFIER_PARAM) : cx(cx) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        oldCompartment = cx->compartment;
    }

    ~PreserveCompartment() {
        cx->compartment = oldCompartment;
    }
};

class SwitchToCompartment : public PreserveCompartment {
  public:
    SwitchToCompartment(JSContext *cx, JSCompartment *newCompartment
                        JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : PreserveCompartment(cx)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        cx->compartment = newCompartment;
    }

    SwitchToCompartment(JSContext *cx, JSObject *target JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : PreserveCompartment(cx)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        cx->compartment = target->getCompartment();
    }

    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AssertCompartmentUnchanged {
  protected:
    JSContext * const cx;
    JSCompartment * const oldCompartment;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
  public:
     AssertCompartmentUnchanged(JSContext *cx JS_GUARD_OBJECT_NOTIFIER_PARAM)
     : cx(cx), oldCompartment(cx->compartment) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AssertCompartmentUnchanged() {
        JS_ASSERT(cx->compartment == oldCompartment);
    }
};

}

#endif /* jscompartment_h___ */

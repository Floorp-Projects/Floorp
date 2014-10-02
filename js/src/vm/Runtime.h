/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Runtime_h
#define vm_Runtime_h

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Scoped.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/UniquePtr.h"

#include <setjmp.h>

#include "jsatom.h"
#include "jsclist.h"
#ifdef DEBUG
# include "jsproxy.h"
#endif
#include "jsscript.h"

#ifdef XP_MACOSX
# include "asmjs/AsmJSSignalHandlers.h"
#endif
#include "ds/FixedSizeHash.h"
#include "frontend/ParseMaps.h"
#include "gc/GCRuntime.h"
#include "gc/Tracer.h"
#include "irregexp/RegExpStack.h"
#include "js/HashTable.h"
#include "js/Vector.h"
#include "vm/CommonPropertyNames.h"
#include "vm/DateTime.h"
#include "vm/MallocProvider.h"
#include "vm/SPSProfiler.h"
#include "vm/Stack.h"
#include "vm/Symbol.h"
#include "vm/ThreadPool.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4100) /* Silence unreferenced formal parameter warnings */
#endif

namespace js {

class PerThreadData;
struct ThreadSafeContext;
class AutoKeepAtoms;
#ifdef JS_TRACE_LOGGING
class TraceLogger;
#endif

/* Thread Local Storage slot for storing the runtime for a thread. */
extern mozilla::ThreadLocal<PerThreadData*> TlsPerThreadData;

} // namespace js

struct DtoaState;

extern void
js_ReportOutOfMemory(js::ThreadSafeContext *cx);

extern void
js_ReportAllocationOverflow(js::ThreadSafeContext *cx);

extern void
js_ReportOverRecursed(js::ThreadSafeContext *cx);

namespace js {

class Activation;
class ActivationIterator;
class AsmJSActivation;
class MathCache;

namespace jit {
class JitRuntime;
class JitActivation;
struct PcScriptCache;
class Simulator;
class SimulatorRuntime;
struct AutoFlushICache;
}

/*
 * GetSrcNote cache to avoid O(n^2) growth in finding a source note for a
 * given pc in a script. We use the script->code pointer to tag the cache,
 * instead of the script address itself, so that source notes are always found
 * by offset from the bytecode with which they were generated.
 */
struct GSNCache {
    typedef HashMap<jsbytecode *,
                    jssrcnote *,
                    PointerHasher<jsbytecode *, 0>,
                    SystemAllocPolicy> Map;

    jsbytecode      *code;
    Map             map;

    GSNCache() : code(nullptr) { }

    void purge();
};

/*
 * ScopeCoordinateName cache to avoid O(n^2) growth in finding the name
 * associated with a given aliasedvar operation.
 */
struct ScopeCoordinateNameCache {
    typedef HashMap<uint32_t,
                    jsid,
                    DefaultHasher<uint32_t>,
                    SystemAllocPolicy> Map;

    Shape *shape;
    Map map;

    ScopeCoordinateNameCache() : shape(nullptr) {}
    void purge();
};

typedef Vector<ScriptAndCounts, 0, SystemAllocPolicy> ScriptAndCountsVector;

struct EvalCacheEntry
{
    JSScript *script;
    JSScript *callerScript;
    jsbytecode *pc;
};

struct EvalCacheLookup
{
    explicit EvalCacheLookup(JSContext *cx) : str(cx), callerScript(cx) {}
    RootedLinearString str;
    RootedScript callerScript;
    JSVersion version;
    jsbytecode *pc;
};

struct EvalCacheHashPolicy
{
    typedef EvalCacheLookup Lookup;

    static HashNumber hash(const Lookup &l);
    static bool match(const EvalCacheEntry &entry, const EvalCacheLookup &l);
};

typedef HashSet<EvalCacheEntry, EvalCacheHashPolicy, SystemAllocPolicy> EvalCache;

struct LazyScriptHashPolicy
{
    struct Lookup {
        JSContext *cx;
        LazyScript *lazy;

        Lookup(JSContext *cx, LazyScript *lazy)
          : cx(cx), lazy(lazy)
        {}
    };

    static const size_t NumHashes = 3;

    static void hash(const Lookup &lookup, HashNumber hashes[NumHashes]);
    static bool match(JSScript *script, const Lookup &lookup);

    // Alternate methods for use when removing scripts from the hash without an
    // explicit LazyScript lookup.
    static void hash(JSScript *script, HashNumber hashes[NumHashes]);
    static bool match(JSScript *script, JSScript *lookup) { return script == lookup; }

    static void clear(JSScript **pscript) { *pscript = nullptr; }
    static bool isCleared(JSScript *script) { return !script; }
};

typedef FixedSizeHashSet<JSScript *, LazyScriptHashPolicy, 769> LazyScriptCache;

class PropertyIteratorObject;

class NativeIterCache
{
    static const size_t SIZE = size_t(1) << 8;

    /* Cached native iterators. */
    PropertyIteratorObject *data[SIZE];

    static size_t getIndex(uint32_t key) {
        return size_t(key) % SIZE;
    }

  public:
    /* Native iterator most recently started. */
    PropertyIteratorObject *last;

    NativeIterCache()
      : last(nullptr)
    {
        mozilla::PodArrayZero(data);
    }

    void purge() {
        last = nullptr;
        mozilla::PodArrayZero(data);
    }

    PropertyIteratorObject *get(uint32_t key) const {
        return data[getIndex(key)];
    }

    void set(uint32_t key, PropertyIteratorObject *iterobj) {
        data[getIndex(key)] = iterobj;
    }
};

/*
 * Cache for speeding up repetitive creation of objects in the VM.
 * When an object is created which matches the criteria in the 'key' section
 * below, an entry is filled with the resulting object.
 */
class NewObjectCache
{
    /* Statically asserted to be equal to sizeof(JSObject_Slots16) */
    static const unsigned MAX_OBJ_SIZE = 4 * sizeof(void*) + 16 * sizeof(Value);

    static void staticAsserts() {
        JS_STATIC_ASSERT(NewObjectCache::MAX_OBJ_SIZE == sizeof(JSObject_Slots16));
        JS_STATIC_ASSERT(gc::FINALIZE_OBJECT_LAST == gc::FINALIZE_OBJECT16_BACKGROUND);
    }

    struct Entry
    {
        /* Class of the constructed object. */
        const Class *clasp;

        /*
         * Key with one of three possible values:
         *
         * - Global for the object. The object must have a standard class for
         *   which the global's prototype can be determined, and the object's
         *   parent will be the global.
         *
         * - Prototype for the object (cannot be global). The object's parent
         *   will be the prototype's parent.
         *
         * - Type for the object. The object's parent will be the type's
         *   prototype's parent.
         */
        gc::Cell *key;

        /* Allocation kind for the constructed object. */
        gc::AllocKind kind;

        /* Number of bytes to copy from the template object. */
        uint32_t nbytes;

        /*
         * Template object to copy from, with the initial values of fields,
         * fixed slots (undefined) and private data (nullptr).
         */
        char templateObject[MAX_OBJ_SIZE];
    };

    Entry entries[41];  // TODO: reconsider size

  public:

    typedef int EntryIndex;

    NewObjectCache() { mozilla::PodZero(this); }
    void purge() { mozilla::PodZero(this); }

    /* Remove any cached items keyed on moved objects. */
    void clearNurseryObjects(JSRuntime *rt);

    /*
     * Get the entry index for the given lookup, return whether there was a hit
     * on an existing entry.
     */
    inline bool lookupProto(const Class *clasp, JSObject *proto, gc::AllocKind kind, EntryIndex *pentry);
    inline bool lookupGlobal(const Class *clasp, js::GlobalObject *global, gc::AllocKind kind,
                             EntryIndex *pentry);

    bool lookupType(js::types::TypeObject *type, gc::AllocKind kind, EntryIndex *pentry) {
        return lookup(type->clasp(), type, kind, pentry);
    }

    /*
     * Return a new object from a cache hit produced by a lookup method, or
     * nullptr if returning the object could possibly trigger GC (does not
     * indicate failure).
     */
    template <AllowGC allowGC>
    inline JSObject *newObjectFromHit(JSContext *cx, EntryIndex entry, js::gc::InitialHeap heap);

    /* Fill an entry after a cache miss. */
    void fillProto(EntryIndex entry, const Class *clasp, js::TaggedProto proto, gc::AllocKind kind, JSObject *obj);

    inline void fillGlobal(EntryIndex entry, const Class *clasp, js::GlobalObject *global,
                           gc::AllocKind kind, JSObject *obj);

    void fillType(EntryIndex entry, js::types::TypeObject *type, gc::AllocKind kind,
                  JSObject *obj)
    {
        MOZ_ASSERT(obj->type() == type);
        return fill(entry, type->clasp(), type, kind, obj);
    }

    /* Invalidate any entries which might produce an object with shape/proto. */
    void invalidateEntriesForShape(JSContext *cx, HandleShape shape, HandleObject proto);

  private:
    EntryIndex makeIndex(const Class *clasp, gc::Cell *key, gc::AllocKind kind) {
        uintptr_t hash = (uintptr_t(clasp) ^ uintptr_t(key)) + kind;
        return hash % mozilla::ArrayLength(entries);
    }

    bool lookup(const Class *clasp, gc::Cell *key, gc::AllocKind kind, EntryIndex *pentry) {
        *pentry = makeIndex(clasp, key, kind);
        Entry *entry = &entries[*pentry];

        /* N.B. Lookups with the same clasp/key but different kinds map to different entries. */
        return entry->clasp == clasp && entry->key == key;
    }

    void fill(EntryIndex entry_, const Class *clasp, gc::Cell *key, gc::AllocKind kind,
              JSObject *obj) {
        MOZ_ASSERT(unsigned(entry_) < mozilla::ArrayLength(entries));
        MOZ_ASSERT(entry_ == makeIndex(clasp, key, kind));
        Entry *entry = &entries[entry_];

        MOZ_ASSERT(!obj->hasDynamicSlots() && !obj->hasDynamicElements());

        entry->clasp = clasp;
        entry->key = key;
        entry->kind = kind;

        entry->nbytes = gc::Arena::thingSize(kind);
        js_memcpy(&entry->templateObject, obj, entry->nbytes);
    }

    static void copyCachedToObject(JSObject *dst, JSObject *src, gc::AllocKind kind) {
        js_memcpy(dst, src, gc::Arena::thingSize(kind));
#ifdef JSGC_GENERATIONAL
        Shape::writeBarrierPost(dst->shape_, &dst->shape_);
        types::TypeObject::writeBarrierPost(dst->type_, &dst->type_);
#endif
    }
};

/*
 * A FreeOp can do one thing: free memory. For convenience, it has delete_
 * convenience methods that also call destructors.
 *
 * FreeOp is passed to finalizers and other sweep-phase hooks so that we do not
 * need to pass a JSContext to those hooks.
 */
class FreeOp : public JSFreeOp
{
    Vector<void *, 0, SystemAllocPolicy> freeLaterList;

  public:
    static FreeOp *get(JSFreeOp *fop) {
        return static_cast<FreeOp *>(fop);
    }

    explicit FreeOp(JSRuntime *rt)
      : JSFreeOp(rt)
    {}

    ~FreeOp() {
        for (size_t i = 0; i < freeLaterList.length(); i++)
            free_(freeLaterList[i]);
    }

    inline void free_(void *p);
    inline void freeLater(void *p);

    template <class T>
    inline void delete_(T *p) {
        if (p) {
            p->~T();
            free_(p);
        }
    }
};

} /* namespace js */

namespace JS {
struct RuntimeSizes;
}

/* Various built-in or commonly-used names pinned on first context. */
struct JSAtomState
{
#define PROPERTYNAME_FIELD(idpart, id, text) js::ImmutablePropertyNamePtr id;
    FOR_EACH_COMMON_PROPERTYNAME(PROPERTYNAME_FIELD)
#undef PROPERTYNAME_FIELD
#define PROPERTYNAME_FIELD(name, code, init, clasp) js::ImmutablePropertyNamePtr name;
    JS_FOR_EACH_PROTOTYPE(PROPERTYNAME_FIELD)
#undef PROPERTYNAME_FIELD
};

namespace js {

/*
 * Storage for well-known symbols. It's a separate struct from the Runtime so
 * that it can be shared across multiple runtimes. As in JSAtomState, each
 * field is a smart pointer that's immutable once initialized.
 * `rt->wellKnownSymbols.iterator` is convertible to Handle<Symbol*>.
 *
 * Well-known symbols are never GC'd. The description() of each well-known
 * symbol is a permanent atom.
 */
struct WellKnownSymbols
{
    js::ImmutableSymbolPtr iterator;

    ImmutableSymbolPtr &get(size_t i) {
        MOZ_ASSERT(i < JS::WellKnownSymbolLimit);
        ImmutableSymbolPtr *symbols = reinterpret_cast<ImmutableSymbolPtr *>(this);
        return symbols[i];
    }
};

#define NAME_OFFSET(name)       offsetof(JSAtomState, name)

inline HandlePropertyName
AtomStateOffsetToName(const JSAtomState &atomState, size_t offset)
{
    return *reinterpret_cast<js::ImmutablePropertyNamePtr *>((char*)&atomState + offset);
}

// There are several coarse locks in the enum below. These may be either
// per-runtime or per-process. When acquiring more than one of these locks,
// the acquisition must be done in the order below to avoid deadlocks.
enum RuntimeLock {
    ExclusiveAccessLock,
    HelperThreadStateLock,
    InterruptLock,
    GCLock
};

#ifdef DEBUG
void AssertCurrentThreadCanLock(RuntimeLock which);
#else
inline void AssertCurrentThreadCanLock(RuntimeLock which) {}
#endif

inline bool
CanUseExtraThreads()
{
    extern bool gCanUseExtraThreads;
    return gCanUseExtraThreads;
}

void DisableExtraThreads();

/*
 * Encapsulates portions of the runtime/context that are tied to a
 * single active thread.  Instances of this structure can occur for
 * the main thread as |JSRuntime::mainThread|, for select operations
 * performed off thread, such as parsing, and for Parallel JS worker
 * threads.
 */
class PerThreadData : public PerThreadDataFriendFields
{
    /*
     * Backpointer to the full shared JSRuntime* with which this
     * thread is associated.  This is private because accessing the
     * fields of this runtime can provoke race conditions, so the
     * intention is that access will be mediated through safe
     * functions like |runtimeFromMainThread| and |associatedWith()| below.
     */
    JSRuntime *runtime_;

  public:
    /*
     * We save all conservative scanned roots in this vector so that
     * conservative scanning can be "replayed" deterministically. In DEBUG mode,
     * this allows us to run a non-incremental GC after every incremental GC to
     * ensure that no objects were missed.
     */
#ifdef DEBUG
    struct SavedGCRoot {
        void *thing;
        JSGCTraceKind kind;

        SavedGCRoot(void *thing, JSGCTraceKind kind) : thing(thing), kind(kind) {}
    };
    js::Vector<SavedGCRoot, 0, js::SystemAllocPolicy> gcSavedRoots;
#endif

    /*
     * If Baseline or Ion code is on the stack, and has called into C++, this
     * will be aligned to an exit frame.
     */
    uint8_t             *jitTop;

    /*
     * The current JSContext when entering JIT code. This field may only be used
     * from JIT code and C++ directly called by JIT code (otherwise it may refer
     * to the wrong JSContext).
     */
    JSContext           *jitJSContext;

    /*
     * The stack limit checked by JIT code. This stack limit may be temporarily
     * set to null to force JIT code to exit (e.g., for the operation callback).
     */
    uintptr_t            jitStackLimit;

    inline void setJitStackLimit(uintptr_t limit);

    // Information about the heap allocated backtrack stack used by RegExp JIT code.
    irregexp::RegExpStack regexpStack;

#ifdef JS_TRACE_LOGGING
    TraceLogger         *traceLogger;
#endif

    /*
     * asm.js maintains a stack of AsmJSModule activations (see AsmJS.h). This
     * stack is used by JSRuntime::requestInterrupt to stop long-running asm.js
     * without requiring dynamic polling operations in the generated
     * code. Since requestInterrupt may run on a separate thread than the
     * JSRuntime's owner thread all reads/writes must be synchronized (by
     * rt->interruptLock).
     */
  private:
    friend class js::Activation;
    friend class js::ActivationIterator;
    friend class js::jit::JitActivation;
    friend class js::AsmJSActivation;
#ifdef DEBUG
    friend void js::AssertCurrentThreadCanLock(RuntimeLock which);
#endif

    /*
     * Points to the most recent activation running on the thread.
     * See Activation comment in vm/Stack.h.
     */
    js::Activation *activation_;

    /*
     * Points to the most recent profiling activation running on the
     * thread.  Protected by rt->interruptLock.
     */
    js::Activation * volatile profilingActivation_;

    /* See AsmJSActivation comment. Protected by rt->interruptLock. */
    js::AsmJSActivation * volatile asmJSActivationStack_;

    /* Pointer to the current AutoFlushICache. */
    js::jit::AutoFlushICache *autoFlushICache_;

#if defined(JS_ARM_SIMULATOR) || defined(JS_MIPS_SIMULATOR)
    js::jit::Simulator *simulator_;
    uintptr_t simulatorStackLimit_;
#endif

  public:
    js::Activation *const *addressOfActivation() const {
        return &activation_;
    }
    static unsigned offsetOfAsmJSActivationStackReadOnly() {
        return offsetof(PerThreadData, asmJSActivationStack_);
    }
    static unsigned offsetOfActivation() {
        return offsetof(PerThreadData, activation_);
    }

    js::Activation *profilingActivation() const {
        return profilingActivation_;
    }

    js::AsmJSActivation *asmJSActivationStack() const {
        return asmJSActivationStack_;
    }
    static js::AsmJSActivation *innermostAsmJSActivation() {
        PerThreadData *ptd = TlsPerThreadData.get();
        return ptd ? ptd->asmJSActivationStack_ : nullptr;
    }

    js::Activation *activation() const {
        return activation_;
    }

    /* State used by jsdtoa.cpp. */
    DtoaState           *dtoaState;

    /*
     * When this flag is non-zero, any attempt to GC will be skipped. It is used
     * to suppress GC when reporting an OOM (see js_ReportOutOfMemory) and in
     * debugging facilities that cannot tolerate a GC and would rather OOM
     * immediately, such as utilities exposed to GDB. Setting this flag is
     * extremely dangerous and should only be used when in an OOM situation or
     * in non-exposed debugging facilities.
     */
    int32_t suppressGC;

#ifdef DEBUG
    // Whether this thread is actively Ion compiling.
    bool ionCompiling;
#endif

    // Number of active bytecode compilation on this thread.
    unsigned activeCompilations;

    explicit PerThreadData(JSRuntime *runtime);
    ~PerThreadData();

    bool init();

    bool associatedWith(const JSRuntime *rt) { return runtime_ == rt; }
    inline JSRuntime *runtimeFromMainThread();
    inline JSRuntime *runtimeIfOnOwnerThread();

    inline bool exclusiveThreadsPresent();
    inline void addActiveCompilation();
    inline void removeActiveCompilation();

    // For threads which may be associated with different runtimes, depending
    // on the work they are doing.
    class MOZ_STACK_CLASS AutoEnterRuntime
    {
        PerThreadData *pt;

      public:
        AutoEnterRuntime(PerThreadData *pt, JSRuntime *rt)
          : pt(pt)
        {
            MOZ_ASSERT(!pt->runtime_);
            pt->runtime_ = rt;
#if defined(JS_ARM_SIMULATOR) || defined(JS_MIPS_SIMULATOR)
            // The simulator has a pointer to its SimulatorRuntime, but helper threads
            // don't have a simulator as they don't run JIT code so this pointer need not
            // be updated. All the paths that the helper threads use access the
            // SimulatorRuntime via the PerThreadData.
            MOZ_ASSERT(!pt->simulator_);
#endif
        }

        ~AutoEnterRuntime() {
            pt->runtime_ = nullptr;
#if defined(JS_ARM_SIMULATOR) || defined(JS_MIPS_SIMULATOR)
            // Check that helper threads have not run JIT code and/or added a simulator.
            MOZ_ASSERT(!pt->simulator_);
#endif
        }
    };

    js::jit::AutoFlushICache *autoFlushICache() const;
    void setAutoFlushICache(js::jit::AutoFlushICache *afc);

#if defined(JS_ARM_SIMULATOR) || defined(JS_MIPS_SIMULATOR)
    js::jit::Simulator *simulator() const;
    void setSimulator(js::jit::Simulator *sim);
    js::jit::SimulatorRuntime *simulatorRuntime() const;
    uintptr_t *addressOfSimulatorStackLimit();
#endif
};

class AutoLockForExclusiveAccess;

void RecomputeStackLimit(JSRuntime *rt, StackKind kind);

} // namespace js

struct JSRuntime : public JS::shadow::Runtime,
                   public js::MallocProvider<JSRuntime>
{
    /*
     * Per-thread data for the main thread that is associated with
     * this JSRuntime, as opposed to any worker threads used in
     * parallel sections.  See definition of |PerThreadData| struct
     * above for more details.
     *
     * NB: This field is statically asserted to be at offset
     * sizeof(js::shadow::Runtime). See
     * PerThreadDataFriendFields::getMainThread.
     */
    js::PerThreadData mainThread;

    /*
     * If non-null, another runtime guaranteed to outlive this one and whose
     * permanent data may be used by this one where possible.
     */
    JSRuntime *parentRuntime;

    /*
     * If true, we've been asked to call the interrupt callback as soon as
     * possible.
     */
    mozilla::Atomic<bool, mozilla::Relaxed> interrupt;

    /*
     * If non-zero, ForkJoin should service an interrupt. This is a separate
     * flag from |interrupt| because we cannot use the mprotect trick with PJS
     * code and ignore the TriggerCallbackAnyThreadDontStopIon trigger.
     */
    mozilla::Atomic<bool, mozilla::Relaxed> interruptPar;

    /* Set when handling a signal for a thread associated with this runtime. */
    bool handlingSignal;

    JSInterruptCallback interruptCallback;

#ifdef DEBUG
    void assertCanLock(js::RuntimeLock which);
#else
    void assertCanLock(js::RuntimeLock which) {}
#endif

  private:
    /*
     * Lock taken when triggering an interrupt from another thread.
     * Protects all data that is touched in this process.
     */
    PRLock *interruptLock;
    PRThread *interruptLockOwner;

  public:
    class AutoLockForInterrupt {
        JSRuntime *rt;
      public:
        explicit AutoLockForInterrupt(JSRuntime *rt MOZ_GUARD_OBJECT_NOTIFIER_PARAM) : rt(rt) {
            MOZ_GUARD_OBJECT_NOTIFIER_INIT;
            rt->assertCanLock(js::InterruptLock);
            PR_Lock(rt->interruptLock);
            rt->interruptLockOwner = PR_GetCurrentThread();
        }
        ~AutoLockForInterrupt() {
            MOZ_ASSERT(rt->currentThreadOwnsInterruptLock());
            rt->interruptLockOwner = nullptr;
            PR_Unlock(rt->interruptLock);
        }

        MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
    };

    bool currentThreadOwnsInterruptLock() {
        return interruptLockOwner == PR_GetCurrentThread();
    }

  private:
    /*
     * Lock taken when using per-runtime or per-zone data that could otherwise
     * be accessed simultaneously by both the main thread and another thread
     * with an ExclusiveContext.
     *
     * Locking this only occurs if there is actually a thread other than the
     * main thread with an ExclusiveContext which could access such data.
     */
    PRLock *exclusiveAccessLock;
    mozilla::DebugOnly<PRThread *> exclusiveAccessOwner;
    mozilla::DebugOnly<bool> mainThreadHasExclusiveAccess;

    /* Number of non-main threads with an ExclusiveContext. */
    size_t numExclusiveThreads;

    friend class js::AutoLockForExclusiveAccess;

  public:
    void setUsedByExclusiveThread(JS::Zone *zone);
    void clearUsedByExclusiveThread(JS::Zone *zone);

#ifdef DEBUG
    bool currentThreadHasExclusiveAccess() {
        return (!numExclusiveThreads && mainThreadHasExclusiveAccess) ||
               exclusiveAccessOwner == PR_GetCurrentThread();
    }
#endif // DEBUG

    bool exclusiveThreadsPresent() const {
        return numExclusiveThreads > 0;
    }

    /* How many compartments there are across all zones. */
    size_t              numCompartments;

    /* Locale-specific callbacks for string conversion. */
    JSLocaleCallbacks *localeCallbacks;

    /* Default locale for Internationalization API */
    char *defaultLocale;

    /* Default JSVersion. */
    JSVersion defaultVersion_;

  private:
    /* See comment for JS_AbortIfWrongThread in jsapi.h. */
    void *ownerThread_;
    friend bool js::CurrentThreadCanAccessRuntime(JSRuntime *rt);
  public:

    /* Temporary arena pool used while compiling and decompiling. */
    static const size_t TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 4 * 1024;
    js::LifoAlloc tempLifoAlloc;

    /*
     * Free LIFO blocks are transferred to this allocator before being freed on
     * the background GC thread.
     */
    js::LifoAlloc freeLifoAlloc;

  private:
    /*
     * Both of these allocators are used for regular expression code which is shared at the
     * thread-data level.
     */
    js::jit::ExecutableAllocator *execAlloc_;
    js::jit::JitRuntime *jitRuntime_;

    /*
     * Self-hosting state cloned on demand into other compartments. Shared with the parent
     * runtime if there is one.
     */
    JSObject *selfHostingGlobal_;

    /* Space for interpreter frames. */
    js::InterpreterStack interpreterStack_;

    js::jit::ExecutableAllocator *createExecutableAllocator(JSContext *cx);
    js::jit::JitRuntime *createJitRuntime(JSContext *cx);

  public:
    js::jit::ExecutableAllocator *getExecAlloc(JSContext *cx) {
        return execAlloc_ ? execAlloc_ : createExecutableAllocator(cx);
    }
    js::jit::ExecutableAllocator &execAlloc() {
        MOZ_ASSERT(execAlloc_);
        return *execAlloc_;
    }
    js::jit::ExecutableAllocator *maybeExecAlloc() {
        return execAlloc_;
    }
    js::jit::JitRuntime *getJitRuntime(JSContext *cx) {
        return jitRuntime_ ? jitRuntime_ : createJitRuntime(cx);
    }
    js::jit::JitRuntime *jitRuntime() const {
        return jitRuntime_;
    }
    bool hasJitRuntime() const {
        return !!jitRuntime_;
    }
    js::InterpreterStack &interpreterStack() {
        return interpreterStack_;
    }

    //-------------------------------------------------------------------------
    // Self-hosting support
    //-------------------------------------------------------------------------

    bool initSelfHosting(JSContext *cx);
    void finishSelfHosting();
    void markSelfHostingGlobal(JSTracer *trc);
    bool isSelfHostingGlobal(JSObject *global) {
        return global == selfHostingGlobal_;
    }
    bool isSelfHostingCompartment(JSCompartment *comp);
    bool cloneSelfHostedFunctionScript(JSContext *cx, js::Handle<js::PropertyName*> name,
                                       js::Handle<JSFunction*> targetFun);
    bool cloneSelfHostedValue(JSContext *cx, js::Handle<js::PropertyName*> name,
                              js::MutableHandleValue vp);

    //-------------------------------------------------------------------------
    // Locale information
    //-------------------------------------------------------------------------

    /*
     * Set the default locale for the ECMAScript Internationalization API
     * (Intl.Collator, Intl.NumberFormat, Intl.DateTimeFormat).
     * Note that the Internationalization API encourages clients to
     * specify their own locales.
     * The locale string remains owned by the caller.
     */
    bool setDefaultLocale(const char *locale);

    /* Reset the default locale to OS defaults. */
    void resetDefaultLocale();

    /* Gets current default locale. String remains owned by context. */
    const char *getDefaultLocale();

    JSVersion defaultVersion() { return defaultVersion_; }
    void setDefaultVersion(JSVersion v) { defaultVersion_ = v; }

    /* Base address of the native stack for the current thread. */
    uintptr_t           nativeStackBase;

    /* The native stack size limit that runtime should not exceed. */
    size_t              nativeStackQuota[js::StackKindCount];

    /* Context create/destroy callback. */
    JSContextCallback   cxCallback;
    void               *cxCallbackData;

    /* Compartment destroy callback. */
    JSDestroyCompartmentCallback destroyCompartmentCallback;

    /* Zone destroy callback. */
    JSZoneCallback destroyZoneCallback;

    /* Zone sweep callback. */
    JSZoneCallback sweepZoneCallback;

    /* Call this to get the name of a compartment. */
    JSCompartmentNameCallback compartmentNameCallback;

    js::ActivityCallback  activityCallback;
    void                 *activityCallbackArg;
    void triggerActivityCallback(bool active);

    /* The request depth for this thread. */
    unsigned            requestDepth;

#ifdef DEBUG
    unsigned            checkRequestDepth;

    /*
     * To help embedders enforce their invariants, we allow them to specify in
     * advance which JSContext should be passed to JSAPI calls. If this is set
     * to a non-null value, the assertSameCompartment machinery does double-
     * duty (in debug builds) to verify that it matches the cx being used.
     */
    JSContext          *activeContext;
#endif

    /* Garbage collector state, used by jsgc.c. */
    js::gc::GCRuntime   gc;

    /* Garbage collector state has been sucessfully initialized. */
    bool                gcInitialized;

    bool isHeapBusy() { return gc.isHeapBusy(); }
    bool isHeapMajorCollecting() { return gc.isHeapMajorCollecting(); }
    bool isHeapMinorCollecting() { return gc.isHeapMinorCollecting(); }
    bool isHeapCollecting() { return gc.isHeapCollecting(); }
    bool isHeapCompacting() { return gc.isHeapCompacting(); }

    bool isFJMinorCollecting() { return gc.isFJMinorCollecting(); }

    int gcZeal() { return gc.zeal(); }

    void lockGC() {
        assertCanLock(js::GCLock);
        gc.lockGC();
    }

    void unlockGC() {
        gc.unlockGC();
    }

#if defined(JS_ARM_SIMULATOR) || defined(JS_MIPS_SIMULATOR)
    js::jit::SimulatorRuntime *simulatorRuntime_;
#endif

  public:
    void setNeedsIncrementalBarrier(bool needs) {
        needsIncrementalBarrier_ = needs;
    }

#if defined(JS_ARM_SIMULATOR) || defined(JS_MIPS_SIMULATOR)
    js::jit::SimulatorRuntime *simulatorRuntime() const;
    void setSimulatorRuntime(js::jit::SimulatorRuntime *srt);
#endif

    /* Strong references on scripts held for PCCount profiling API. */
    js::ScriptAndCountsVector *scriptAndCountsVector;

    /* Well-known numbers held for use by this runtime's contexts. */
    const js::Value     NaNValue;
    const js::Value     negativeInfinityValue;
    const js::Value     positiveInfinityValue;

    js::PropertyName    *emptyString;

    /* List of active contexts sharing this runtime. */
    mozilla::LinkedList<JSContext> contextList;

    bool hasContexts() const {
        return !contextList.isEmpty();
    }

    mozilla::UniquePtr<js::SourceHook> sourceHook;

#ifdef NIGHTLY_BUILD
    js::AssertOnScriptEntryHook assertOnScriptEntryHook_;
#endif

    /* SPS profiling metadata */
    js::SPSProfiler     spsProfiler;

    /* If true, new scripts must be created with PC counter information. */
    bool                profilingScripts;

    /* Whether sampling should be enabled or not. */
  private:
    bool                suppressProfilerSampling;

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

    /* Had an out-of-memory error which did not populate an exception. */
    bool                hadOutOfMemory;

    /* A context has been created on this runtime. */
    bool                haveCreatedContext;

    /* Linked list of all Debugger objects in the runtime. */
    mozilla::LinkedList<js::Debugger> debuggerList;

    /*
     * Head of circular list of all enabled Debuggers that have
     * onNewGlobalObject handler methods established.
     */
    JSCList             onNewGlobalObjectWatchers;

    /* Client opaque pointers */
    void                *data;

#ifdef XP_MACOSX
    js::AsmJSMachExceptionHandler asmJSMachExceptionHandler;
#endif

  private:
    // Whether asm.js signal handlers have been installed and can be used for
    // performing interrupt checks in loops.
    bool signalHandlersInstalled_;
    // Whether we should use them or they have been disabled for making
    // debugging easier. If signal handlers aren't installed, it is set to false.
    bool canUseSignalHandlers_;
  public:
    bool signalHandlersInstalled() const {
        return signalHandlersInstalled_;
    }
    bool canUseSignalHandlers() const {
        return canUseSignalHandlers_;
    }
    void setCanUseSignalHandlers(bool enable) {
        canUseSignalHandlers_ = signalHandlersInstalled_ && enable;
    }

  private:
    js::FreeOp          defaultFreeOp_;

  public:
    js::FreeOp *defaultFreeOp() {
        return &defaultFreeOp_;
    }

    uint32_t            debuggerMutations;

    const JSSecurityCallbacks *securityCallbacks;
    const js::DOMCallbacks *DOMcallbacks;
    JSDestroyPrincipalsOp destroyPrincipals;

    /* Structured data callbacks are runtime-wide. */
    const JSStructuredCloneCallbacks *structuredCloneCallbacks;

    /* Call this to accumulate telemetry data. */
    JSAccumulateTelemetryDataCallback telemetryCallback;

    /* Optional error reporter. */
    JSErrorReporter     errorReporter;

    /* AsmJSCache callbacks are runtime-wide. */
    JS::AsmJSCacheOps asmJSCacheOps;

    /*
     * The propertyRemovals counter is incremented for every JSObject::clear,
     * and for each JSObject::remove method call that frees a slot in the given
     * object. See js_NativeGet and js_NativeSet in jsobj.cpp.
     */
    uint32_t            propertyRemovals;

#if !EXPOSE_INTL_API
    /* Number localization, used by jsnum.cpp. */
    const char          *thousandsSeparator;
    const char          *decimalSeparator;
    const char          *numGrouping;
#endif

  private:
    js::MathCache *mathCache_;
    js::MathCache *createMathCache(JSContext *cx);
  public:
    js::MathCache *getMathCache(JSContext *cx) {
        return mathCache_ ? mathCache_ : createMathCache(cx);
    }
    js::MathCache *maybeGetMathCache() {
        return mathCache_;
    }

    js::GSNCache        gsnCache;
    js::ScopeCoordinateNameCache scopeCoordinateNameCache;
    js::NewObjectCache  newObjectCache;
    js::NativeIterCache nativeIterCache;
    js::UncompressedSourceCache uncompressedSourceCache;
    js::EvalCache       evalCache;
    js::LazyScriptCache lazyScriptCache;

    js::CompressedSourceSet compressedSourceSet;
    js::DateTimeInfo    dateTimeInfo;

    // Pool of maps used during parse/emit. This may be modified by threads
    // with an ExclusiveContext and requires a lock. Active compilations
    // prevent the pool from being purged during GCs.
  private:
    js::frontend::ParseMapPool parseMapPool_;
    unsigned activeCompilations_;
  public:
    js::frontend::ParseMapPool &parseMapPool() {
        MOZ_ASSERT(currentThreadHasExclusiveAccess());
        return parseMapPool_;
    }
    bool hasActiveCompilations() {
        return activeCompilations_ != 0;
    }
    void addActiveCompilation() {
        MOZ_ASSERT(currentThreadHasExclusiveAccess());
        activeCompilations_++;
    }
    void removeActiveCompilation() {
        MOZ_ASSERT(currentThreadHasExclusiveAccess());
        activeCompilations_--;
    }

    // Count of AutoKeepAtoms instances on the main thread's stack. When any
    // instances exist, atoms in the runtime will not be collected. Threads
    // with an ExclusiveContext do not increment this value, but the presence
    // of any such threads also inhibits collection of atoms. We don't scan the
    // stacks of exclusive threads, so we need to avoid collecting their
    // objects in another way. The only GC thing pointers they have are to
    // their exclusive compartment (which is not collected) or to the atoms
    // compartment. Therefore, we avoid collecting the atoms compartment when
    // exclusive threads are running.
  private:
    unsigned keepAtoms_;
    friend class js::AutoKeepAtoms;
  public:
    bool keepAtoms() {
        MOZ_ASSERT(CurrentThreadCanAccessRuntime(this));
        return keepAtoms_ != 0 || exclusiveThreadsPresent();
    }

  private:
    const JSPrincipals  *trustedPrincipals_;
  public:
    void setTrustedPrincipals(const JSPrincipals *p) { trustedPrincipals_ = p; }
    const JSPrincipals *trustedPrincipals() const { return trustedPrincipals_; }

  private:
    bool beingDestroyed_;
  public:
    bool isBeingDestroyed() const {
        return beingDestroyed_;
    }

  private:
    // Set of all atoms other than those in permanentAtoms and staticStrings.
    // Reading or writing this set requires the calling thread to have an
    // ExclusiveContext and hold a lock. Use AutoLockForExclusiveAccess.
    js::AtomSet *atoms_;

    // Compartment and associated zone containing all atoms in the runtime, as
    // well as runtime wide IonCode stubs. Modifying the contents of this
    // compartment requires the calling thread to have an ExclusiveContext and
    // hold a lock. Use AutoLockForExclusiveAccess.
    JSCompartment *atomsCompartment_;

    // Set of all live symbols produced by Symbol.for(). All such symbols are
    // allocated in the atomsCompartment. Reading or writing the symbol
    // registry requires the calling thread to have an ExclusiveContext and
    // hold a lock. Use AutoLockForExclusiveAccess.
    js::SymbolRegistry symbolRegistry_;

  public:
    bool initializeAtoms(JSContext *cx);
    void finishAtoms();

    void sweepAtoms();

    js::AtomSet &atoms() {
        MOZ_ASSERT(currentThreadHasExclusiveAccess());
        return *atoms_;
    }
    JSCompartment *atomsCompartment() {
        MOZ_ASSERT(currentThreadHasExclusiveAccess());
        return atomsCompartment_;
    }

    bool isAtomsCompartment(JSCompartment *comp) {
        return comp == atomsCompartment_;
    }

    // The atoms compartment is the only one in its zone.
    inline bool isAtomsZone(JS::Zone *zone);

    bool activeGCInAtomsZone();

    js::SymbolRegistry &symbolRegistry() {
        MOZ_ASSERT(currentThreadHasExclusiveAccess());
        return symbolRegistry_;
    }

    // Permanent atoms are fixed during initialization of the runtime and are
    // not modified or collected until the runtime is destroyed. These may be
    // shared with another, longer living runtime through |parentRuntime| and
    // can be freely accessed with no locking necessary.

    // Permanent atoms pre-allocated for general use.
    js::StaticStrings *staticStrings;

    // Cached pointers to various permanent property names.
    JSAtomState *commonNames;

    // All permanent atoms in the runtime, other than those in staticStrings.
    js::AtomSet *permanentAtoms;

    bool transformToPermanentAtoms();

    // Cached well-known symbols (ES6 rev 24 6.1.5.1). Like permanent atoms,
    // these are shared with the parentRuntime, if any.
    js::WellKnownSymbols *wellKnownSymbols;

    const JSWrapObjectCallbacks            *wrapObjectCallbacks;
    js::PreserveWrapperCallback            preserveWrapperCallback;

    // Table of bytecode and other data that may be shared across scripts
    // within the runtime. This may be modified by threads with an
    // ExclusiveContext and requires a lock.
  private:
    js::ScriptDataTable scriptDataTable_;
  public:
    js::ScriptDataTable &scriptDataTable() {
        MOZ_ASSERT(currentThreadHasExclusiveAccess());
        return scriptDataTable_;
    }

    bool                jitSupportsFloatingPoint;
    bool                jitSupportsSimd;

    // Used to reset stack limit after a signaled interrupt (i.e. jitStackLimit_ = -1)
    // has been noticed by Ion/Baseline.
    void resetJitStackLimit();

    // Cache for jit::GetPcScript().
    js::jit::PcScriptCache *ionPcScriptCache;

    js::ThreadPool threadPool;

    js::DefaultJSContextCallback defaultJSContextCallback;

    js::CTypesActivityCallback  ctypesActivityCallback;

    // Non-zero if this is a ForkJoin warmup execution.  See
    // js::ForkJoin() for more information.
    uint32_t forkJoinWarmup;

  private:
    static mozilla::Atomic<size_t> liveRuntimesCount;

  public:
    static bool hasLiveRuntimes() {
        return liveRuntimesCount > 0;
    }

    explicit JSRuntime(JSRuntime *parentRuntime);
    ~JSRuntime();

    bool init(uint32_t maxbytes, uint32_t maxNurseryBytes);

    JSRuntime *thisFromCtor() { return this; }

    /*
     * Call this after allocating memory held by GC things, to update memory
     * pressure counters or report the OOM error if necessary. If oomError and
     * cx is not null the function also reports OOM error.
     *
     * The function must be called outside the GC lock and in case of OOM error
     * the caller must ensure that no deadlock possible during OOM reporting.
     */
    void updateMallocCounter(size_t nbytes);
    void updateMallocCounter(JS::Zone *zone, size_t nbytes);

    void reportAllocationOverflow() { js_ReportAllocationOverflow(nullptr); }

    /*
     * The function must be called outside the GC lock.
     */
    JS_FRIEND_API(void) onTooMuchMalloc();

    /*
     * This should be called after system malloc/realloc returns nullptr to try
     * to recove some memory or to report an error. Failures in malloc and
     * calloc are signaled by p == null and p == reinterpret_cast<void *>(1).
     * Other values of p mean a realloc failure.
     *
     * The function must be called outside the GC lock.
     */
    JS_FRIEND_API(void *) onOutOfMemory(void *p, size_t nbytes);
    JS_FRIEND_API(void *) onOutOfMemory(void *p, size_t nbytes, JSContext *cx);

    /*  onOutOfMemory but can call the largeAllocationFailureCallback. */
    JS_FRIEND_API(void *) onOutOfMemoryCanGC(void *p, size_t bytes);

    // Ways in which the interrupt callback on the runtime can be triggered,
    // varying based on which thread is triggering the callback.
    enum InterruptMode {
        RequestInterruptMainThread,
        RequestInterruptAnyThread,
        RequestInterruptAnyThreadDontStopIon,
        RequestInterruptAnyThreadForkJoin
    };

    void requestInterrupt(InterruptMode mode);

    void addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf, JS::RuntimeSizes *runtime);

  private:
    JS::RuntimeOptions options_;

    // Settings for how helper threads can be used.
    bool offthreadIonCompilationEnabled_;
    bool parallelParsingEnabled_;

  public:

    // Note: these values may be toggled dynamically (in response to about:config
    // prefs changing).
    void setOffthreadIonCompilationEnabled(bool value) {
        offthreadIonCompilationEnabled_ = value;
    }
    bool canUseOffthreadIonCompilation() const {
        return offthreadIonCompilationEnabled_;
    }
    void setParallelParsingEnabled(bool value) {
        parallelParsingEnabled_ = value;
    }
    bool canUseParallelParsing() const {
        return parallelParsingEnabled_;
    }

    const JS::RuntimeOptions &options() const {
        return options_;
    }
    JS::RuntimeOptions &options() {
        return options_;
    }

#ifdef DEBUG
  public:
    js::AutoEnterPolicy *enteredPolicy;
#endif

    /* See comment for JS::SetLargeAllocationFailureCallback in jsapi.h. */
    JS::LargeAllocationFailureCallback largeAllocationFailureCallback;
    void *largeAllocationFailureCallbackData;

    /* See comment for JS::SetOutOfMemoryCallback in jsapi.h. */
    JS::OutOfMemoryCallback oomCallback;
    void *oomCallbackData;

    /*
     * These variations of malloc/calloc/realloc will call the
     * large-allocation-failure callback on OOM and retry the allocation.
     */

    static const unsigned LARGE_ALLOCATION = 25 * 1024 * 1024;

    template <typename T>
    T *pod_callocCanGC(size_t numElems) {
        T *p = pod_calloc<T>(numElems);
        if (MOZ_LIKELY(!!p))
            return p;
        if (numElems & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
            reportAllocationOverflow();
            return nullptr;
        }
        return (T *)onOutOfMemoryCanGC(reinterpret_cast<void *>(1), numElems * sizeof(T));
    }

    template <typename T>
    T *pod_reallocCanGC(T *p, size_t oldSize, size_t newSize) {
        T *p2 = pod_realloc<T>(p, oldSize, newSize);
        if (MOZ_LIKELY(!!p2))
            return p2;
        if (newSize & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
            reportAllocationOverflow();
            return nullptr;
        }
        return (T *)onOutOfMemoryCanGC(p, newSize * sizeof(T));
    }

    /*
     * Debugger.Memory functions like takeCensus use this embedding-provided
     * function to assess the size of malloc'd blocks of memory.
     */
    mozilla::MallocSizeOf debuggerMallocSizeOf;
};

namespace js {

// When entering JIT code, the calling JSContext* is stored into the thread's
// PerThreadData. This function retrieves the JSContext with the pre-condition
// that the caller is JIT code or C++ called directly from JIT code. This
// function should not be called from arbitrary locations since the JSContext
// may be the wrong one.
static inline JSContext *
GetJSContextFromJitCode()
{
    JSContext *cx = TlsPerThreadData.get()->jitJSContext;
    MOZ_ASSERT(cx);
    return cx;
}

/*
 * Flags accompany script version data so that a) dynamically created scripts
 * can inherit their caller's compile-time properties and b) scripts can be
 * appropriately compared in the eval cache across global option changes. An
 * example of the latter is enabling the top-level-anonymous-function-is-error
 * option: subsequent evals of the same, previously-valid script text may have
 * become invalid.
 */
namespace VersionFlags {
static const unsigned MASK      = 0x0FFF; /* see JSVersion in jspubtd.h */
} /* namespace VersionFlags */

static inline JSVersion
VersionNumber(JSVersion version)
{
    return JSVersion(uint32_t(version) & VersionFlags::MASK);
}

static inline JSVersion
VersionExtractFlags(JSVersion version)
{
    return JSVersion(uint32_t(version) & ~VersionFlags::MASK);
}

static inline void
VersionCopyFlags(JSVersion *version, JSVersion from)
{
    *version = JSVersion(VersionNumber(*version) | VersionExtractFlags(from));
}

static inline bool
VersionHasFlags(JSVersion version)
{
    return !!VersionExtractFlags(version);
}

static inline bool
VersionIsKnown(JSVersion version)
{
    return VersionNumber(version) != JSVERSION_UNKNOWN;
}

inline void
FreeOp::free_(void *p)
{
    js_free(p);
}

inline void
FreeOp::freeLater(void *p)
{
    // FreeOps other than the defaultFreeOp() are constructed on the stack,
    // and won't hold onto the pointers to free indefinitely.
    MOZ_ASSERT(this != runtime()->defaultFreeOp());

    if (!freeLaterList.append(p))
        CrashAtUnhandlableOOM("FreeOp::freeLater");
}

class AutoLockGC
{
  public:
    explicit AutoLockGC(JSRuntime *rt = nullptr
                        MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : runtime(rt)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        // Avoid MSVC warning C4390 for non-threadsafe builds.
        if (rt)
            rt->lockGC();
    }

    ~AutoLockGC()
    {
        if (runtime)
            runtime->unlockGC();
    }

    bool locked() const {
        return !!runtime;
    }

    void lock(JSRuntime *rt) {
        MOZ_ASSERT(rt);
        MOZ_ASSERT(!runtime);
        runtime = rt;
        rt->lockGC();
    }

  private:
    JSRuntime *runtime;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoUnlockGC
{
  private:
    JSRuntime *rt;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    explicit AutoUnlockGC(JSRuntime *rt
                          MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : rt(rt)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        rt->unlockGC();
    }
    ~AutoUnlockGC() { rt->lockGC(); }
};

class MOZ_STACK_CLASS AutoKeepAtoms
{
    PerThreadData *pt;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    explicit AutoKeepAtoms(PerThreadData *pt
                           MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : pt(pt)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        if (JSRuntime *rt = pt->runtimeIfOnOwnerThread()) {
            rt->keepAtoms_++;
        } else {
            // This should be a thread with an exclusive context, which will
            // always inhibit collection of atoms.
            MOZ_ASSERT(pt->exclusiveThreadsPresent());
        }
    }
    ~AutoKeepAtoms() {
        if (JSRuntime *rt = pt->runtimeIfOnOwnerThread()) {
            MOZ_ASSERT(rt->keepAtoms_);
            rt->keepAtoms_--;
        }
    }
};

inline void
PerThreadData::setJitStackLimit(uintptr_t limit)
{
    MOZ_ASSERT(runtime_->currentThreadOwnsInterruptLock());
    jitStackLimit = limit;
}

inline JSRuntime *
PerThreadData::runtimeFromMainThread()
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime_));
    return runtime_;
}

inline JSRuntime *
PerThreadData::runtimeIfOnOwnerThread()
{
    return (runtime_ && CurrentThreadCanAccessRuntime(runtime_)) ? runtime_ : nullptr;
}

inline bool
PerThreadData::exclusiveThreadsPresent()
{
    return runtime_->exclusiveThreadsPresent();
}

inline void
PerThreadData::addActiveCompilation()
{
    activeCompilations++;
    runtime_->addActiveCompilation();
}

inline void
PerThreadData::removeActiveCompilation()
{
    MOZ_ASSERT(activeCompilations);
    activeCompilations--;
    runtime_->removeActiveCompilation();
}

/************************************************************************/

static MOZ_ALWAYS_INLINE void
MakeRangeGCSafe(Value *vec, size_t len)
{
    mozilla::PodZero(vec, len);
}

static MOZ_ALWAYS_INLINE void
MakeRangeGCSafe(Value *beg, Value *end)
{
    mozilla::PodZero(beg, end - beg);
}

static MOZ_ALWAYS_INLINE void
MakeRangeGCSafe(jsid *beg, jsid *end)
{
    for (jsid *id = beg; id != end; ++id)
        *id = INT_TO_JSID(0);
}

static MOZ_ALWAYS_INLINE void
MakeRangeGCSafe(jsid *vec, size_t len)
{
    MakeRangeGCSafe(vec, vec + len);
}

static MOZ_ALWAYS_INLINE void
MakeRangeGCSafe(Shape **beg, Shape **end)
{
    mozilla::PodZero(beg, end - beg);
}

static MOZ_ALWAYS_INLINE void
MakeRangeGCSafe(Shape **vec, size_t len)
{
    mozilla::PodZero(vec, len);
}

static MOZ_ALWAYS_INLINE void
SetValueRangeToUndefined(Value *beg, Value *end)
{
    for (Value *v = beg; v != end; ++v)
        v->setUndefined();
}

static MOZ_ALWAYS_INLINE void
SetValueRangeToUndefined(Value *vec, size_t len)
{
    SetValueRangeToUndefined(vec, vec + len);
}

static MOZ_ALWAYS_INLINE void
SetValueRangeToNull(Value *beg, Value *end)
{
    for (Value *v = beg; v != end; ++v)
        v->setNull();
}

static MOZ_ALWAYS_INLINE void
SetValueRangeToNull(Value *vec, size_t len)
{
    SetValueRangeToNull(vec, vec + len);
}

/*
 * Allocation policy that uses JSRuntime::pod_malloc and friends, so that
 * memory pressure is properly accounted for. This is suitable for
 * long-lived objects owned by the JSRuntime.
 *
 * Since it doesn't hold a JSContext (those may not live long enough), it
 * can't report out-of-memory conditions itself; the caller must check for
 * OOM and take the appropriate action.
 *
 * FIXME bug 647103 - replace these *AllocPolicy names.
 */
class RuntimeAllocPolicy
{
    JSRuntime *const runtime;

  public:
    MOZ_IMPLICIT RuntimeAllocPolicy(JSRuntime *rt) : runtime(rt) {}

    template <typename T>
    T *pod_malloc(size_t numElems) {
        return runtime->pod_malloc<T>(numElems);
    }

    template <typename T>
    T *pod_calloc(size_t numElems) {
        return runtime->pod_calloc<T>(numElems);
    }

    template <typename T>
    T *pod_realloc(T *p, size_t oldSize, size_t newSize) {
        return runtime->pod_realloc<T>(p, oldSize, newSize);
    }

    void free_(void *p) { js_free(p); }
    void reportAllocOverflow() const {}
};

extern const JSSecurityCallbacks NullSecurityCallbacks;

// Debugging RAII class which marks the current thread as performing an Ion
// compilation, for use by CurrentThreadCan{Read,Write}CompilationData
class AutoEnterIonCompilation
{
  public:
    explicit AutoEnterIonCompilation(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;

#ifdef DEBUG
        PerThreadData *pt = js::TlsPerThreadData.get();
        MOZ_ASSERT(!pt->ionCompiling);
        pt->ionCompiling = true;
#endif
    }

    ~AutoEnterIonCompilation() {
#ifdef DEBUG
        PerThreadData *pt = js::TlsPerThreadData.get();
        MOZ_ASSERT(pt->ionCompiling);
        pt->ionCompiling = false;
#endif
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} /* namespace js */

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif /* vm_Runtime_h */

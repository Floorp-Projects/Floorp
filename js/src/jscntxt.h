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

/* JS execution context. */

#ifndef jscntxt_h___
#define jscntxt_h___

#include "mozilla/Attributes.h"

#include <string.h>

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsprvtd.h"
#include "jsatom.h"
#include "jsclist.h"
#include "jsdhash.h"
#include "jsgc.h"
#include "jspropertycache.h"
#include "jspropertytree.h"
#include "jsutil.h"
#include "prmjtime.h"

#include "ds/LifoAlloc.h"
#include "gc/Statistics.h"
#include "js/HashTable.h"
#include "js/Vector.h"
#include "vm/Stack.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4100) /* Silence unreferenced formal parameter warnings */
#pragma warning(push)
#pragma warning(disable:4355) /* Silence warning about "this" used in base member initializer list */
#endif

JS_BEGIN_EXTERN_C
struct DtoaState;
JS_END_EXTERN_C

struct JSSharpInfo {
    bool hasGen;
    bool isSharp;

    JSSharpInfo() : hasGen(false), isSharp(false) {}
};

typedef js::HashMap<JSObject *, JSSharpInfo> JSSharpTable;

struct JSSharpObjectMap {
    unsigned     depth;
    uint32_t     sharpgen;
    JSSharpTable table;

    JSSharpObjectMap(JSContext *cx) : depth(0), sharpgen(0), table(js::TempAllocPolicy(cx)) {
        table.init();
    }
};

namespace js {

namespace mjit {
class JaegerCompartment;
}

class WeakMapBase;
class InterpreterFrames;

class ScriptOpcodeCounts;
struct ScriptOpcodeCountsPair;

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

    GSNCache() : code(NULL) { }

    void purge();
};

inline GSNCache *
GetGSNCache(JSContext *cx);

struct PendingProxyOperation {
    PendingProxyOperation   *next;
    JSObject                *object;
};

typedef Vector<ScriptOpcodeCountsPair, 0, SystemAllocPolicy> ScriptOpcodeCountsVector;

struct ConservativeGCData
{
    /*
     * The GC scans conservatively between ThreadData::nativeStackBase and
     * nativeStackTop unless the latter is NULL.
     */
    uintptr_t           *nativeStackTop;

    union {
        jmp_buf         jmpbuf;
        uintptr_t       words[JS_HOWMANY(sizeof(jmp_buf), sizeof(uintptr_t))];
    } registerSnapshot;

    /*
     * Cycle collector uses this to communicate that the native stack of the
     * GC thread should be scanned only if the thread have more than the given
     * threshold of requests.
     */
    unsigned requestThreshold;

    ConservativeGCData()
      : nativeStackTop(NULL), requestThreshold(0)
    {}

    ~ConservativeGCData() {
#ifdef JS_THREADSAFE
        /*
         * The conservative GC scanner should be disabled when the thread leaves
         * the last request.
         */
        JS_ASSERT(!hasStackToScan());
#endif
    }

    JS_NEVER_INLINE void recordStackTop();

#ifdef JS_THREADSAFE
    void updateForRequestEnd(unsigned suspendCount) {
        if (suspendCount)
            recordStackTop();
        else
            nativeStackTop = NULL;
    }
#endif

    bool hasStackToScan() const {
        return !!nativeStackTop;
    }
};

} /* namespace js */

struct JSRuntime : js::RuntimeFriendFields
{
    /* Default compartment. */
    JSCompartment       *atomsCompartment;

    /* List of compartments (protected by the GC lock). */
    js::CompartmentVector compartments;

    /* See comment for JS_AbortIfWrongThread in jsapi.h. */
#ifdef JS_THREADSAFE
  public:
    void *ownerThread() const { return ownerThread_; }
    void clearOwnerThread();
    void setOwnerThread();
    JS_FRIEND_API(bool) onOwnerThread() const;
  private:
    void                *ownerThread_;
  public:
#else
  public:
    bool onOwnerThread() const { return true; }
#endif

    /* Keeper of the contiguous stack used by all contexts in this thread. */
    js::StackSpace stackSpace;

    /* Temporary arena pool used while compiling and decompiling. */
    static const size_t TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 1 << 12;
    js::LifoAlloc tempLifoAlloc;

  private:
    /*
     * Both of these allocators are used for regular expression code which is shared at the
     * thread-data level.
     */
    JSC::ExecutableAllocator *execAlloc_;
    WTF::BumpPointerAllocator *bumpAlloc_;

    JSC::ExecutableAllocator *createExecutableAllocator(JSContext *cx);
    WTF::BumpPointerAllocator *createBumpPointerAllocator(JSContext *cx);

  public:
    JSC::ExecutableAllocator *getExecutableAllocator(JSContext *cx) {
        return execAlloc_ ? execAlloc_ : createExecutableAllocator(cx);
    }
    WTF::BumpPointerAllocator *getBumpPointerAllocator(JSContext *cx) {
        return bumpAlloc_ ? bumpAlloc_ : createBumpPointerAllocator(cx);
    }

    /* Base address of the native stack for the current thread. */
    uintptr_t           nativeStackBase;

    /* The native stack size limit that runtime should not exceed. */
    size_t              nativeStackQuota;

    /*
     * Frames currently running in js::Interpret. See InterpreterFrames for
     * details.
     */
    js::InterpreterFrames *interpreterFrames;

    /* Context create/destroy callback. */
    JSContextCallback   cxCallback;

    /* Compartment create/destroy callback. */
    JSCompartmentCallback compartmentCallback;

    js::ActivityCallback  activityCallback;
    void                 *activityCallbackArg;

#ifdef JS_THREADSAFE
    /* Number of JS_SuspendRequest calls withot JS_ResumeRequest. */
    unsigned            suspendCount;

    /* The request depth for this thread. */
    unsigned            requestDepth;

# ifdef DEBUG
    unsigned            checkRequestDepth;
# endif
#endif

    /* Garbage collector state, used by jsgc.c. */

    /*
     * Set of all GC chunks with at least one allocated thing. The
     * conservative GC uses it to quickly check if a possible GC thing points
     * into an allocated chunk.
     */
    js::GCChunkSet      gcChunkSet;

    /*
     * Doubly-linked lists of chunks from user and system compartments. The GC
     * allocates its arenas from the corresponding list and when all arenas
     * in the list head are taken, then the chunk is removed from the list.
     * During the GC when all arenas in a chunk become free, that chunk is
     * removed from the list and scheduled for release.
     */
    js::gc::Chunk       *gcSystemAvailableChunkListHead;
    js::gc::Chunk       *gcUserAvailableChunkListHead;
    js::gc::ChunkPool   gcChunkPool;

    js::RootedValueMap  gcRootsHash;
    js::GCLocks         gcLocksHash;
    unsigned            gcKeepAtoms;
    size_t              gcBytes;
    size_t              gcMaxBytes;
    size_t              gcMaxMallocBytes;

    /*
     * Number of the committed arenas in all GC chunks including empty chunks.
     * The counter is volatile as it is read without the GC lock, see comments
     * in MaybeGC.
     */
    volatile uint32_t   gcNumArenasFreeCommitted;
    js::GCMarker        gcMarker;
    void                *gcVerifyData;
    bool                gcChunkAllocationSinceLastGC;
    int64_t             gcNextFullGCTime;
    int64_t             gcJitReleaseTime;
    JSGCMode            gcMode;
    volatile uintptr_t  gcIsNeeded;
    js::WeakMapBase     *gcWeakMapList;
    js::gcstats::Statistics gcStats;

    /* Incremented on every GC slice. */
    uint64_t            gcNumber;

    /* The gcNumber at the time of the most recent GC's first slice. */
    uint64_t            gcStartNumber;

    /* The reason that an interrupt-triggered GC should be called. */
    js::gcreason::Reason gcTriggerReason;

    /*
     * Compartment that triggered GC. If more than one Compatment need GC,
     * gcTriggerCompartment is reset to NULL and a global GC is performed.
     */
    JSCompartment       *gcTriggerCompartment;

    /* Compartment that is currently involved in per-compartment GC */
    JSCompartment       *gcCurrentCompartment;

    /*
     * If this is non-NULL, all marked objects must belong to this compartment.
     * This is used to look for compartment bugs.
     */
    JSCompartment       *gcCheckCompartment;

    /*
     * The current incremental GC phase. During non-incremental GC, this is
     * always NO_INCREMENTAL.
     */
    js::gc::State       gcIncrementalState;

    /* Indicates that a new compartment was created during incremental GC. */
    bool                gcCompartmentCreated;

    /* Indicates that the last incremental slice exhausted the mark stack. */
    bool                gcLastMarkSlice;

    /*
     * Indicates that a GC slice has taken place in the middle of an animation
     * frame, rather than at the beginning. In this case, the next slice will be
     * delayed so that we don't get back-to-back slices.
     */
    volatile uintptr_t  gcInterFrameGC;

    /* Default budget for incremental GC slice. See SliceBudget in jsgc.h. */
    int64_t             gcSliceBudget;

    /*
     * We disable incremental GC if we encounter a js::Class with a trace hook
     * that does not implement write barriers.
     */
    bool                gcIncrementalEnabled;

    /* Compartment that is undergoing an incremental GC. */
    JSCompartment       *gcIncrementalCompartment;

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
     * We can pack these flags as only the GC thread writes to them. Atomic
     * updates to packed bytes are not guaranteed, so stores issued by one
     * thread may be lost due to unsynchronized read-modify-write cycles on
     * other threads.
     */
    bool                gcPoke;
    bool                gcRunning;

    /*
     * These options control the zealousness of the GC. The fundamental values
     * are gcNextScheduled and gcDebugCompartmentGC. At every allocation,
     * gcNextScheduled is decremented. When it reaches zero, we do either a
     * full or a compartmental GC, based on gcDebugCompartmentGC.
     *
     * At this point, if gcZeal_ == 2 then gcNextScheduled is reset to the
     * value of gcZealFrequency. Otherwise, no additional GCs take place.
     *
     * You can control these values in several ways:
     *   - Pass the -Z flag to the shell (see the usage info for details)
     *   - Call gczeal() or schedulegc() from inside shell-executed JS code
     *     (see the help for details)
     *
     * If gzZeal_ == 1 then we perform GCs in select places (during MaybeGC and
     * whenever a GC poke happens). This option is mainly useful to embedders.
     *
     * We use gcZeal_ == 4 to enable write barrier verification. See the comment
     * in jsgc.cpp for more information about this.
     */
#ifdef JS_GC_ZEAL
    int                 gcZeal_;
    int                 gcZealFrequency;
    int                 gcNextScheduled;
    bool                gcDebugCompartmentGC;
    bool                gcDeterministicOnly;

    int gcZeal() { return gcZeal_; }

    bool needZealousGC() {
        if (gcNextScheduled > 0 && --gcNextScheduled == 0) {
            if (gcZeal() == js::gc::ZealAllocValue)
                gcNextScheduled = gcZealFrequency;
            return true;
        }
        return false;
    }
#else
    int gcZeal() { return 0; }
    bool needZealousGC() { return false; }
#endif

    JSGCCallback        gcCallback;
    js::GCSliceCallback gcSliceCallback;
    JSFinalizeCallback  gcFinalizeCallback;

  private:
    /*
     * Malloc counter to measure memory pressure for GC scheduling. It runs
     * from gcMaxMallocBytes down to zero.
     */
    volatile ptrdiff_t  gcMallocBytes;

  public:
    /*
     * The trace operations to trace embedding-specific GC roots. One is for
     * tracing through black roots and the other is for tracing through gray
     * roots. The black/gray distinction is only relevant to the cycle
     * collector.
     */
    JSTraceDataOp       gcBlackRootsTraceOp;
    void                *gcBlackRootsData;
    JSTraceDataOp       gcGrayRootsTraceOp;
    void                *gcGrayRootsData;

    /* Stack of thread-stack-allocated GC roots. */
    js::AutoGCRooter   *autoGCRooters;

    /* Strong references on scripts held for PCCount profiling API. */
    js::ScriptOpcodeCountsVector *scriptPCCounters;

    /* Well-known numbers held for use by this runtime's contexts. */
    js::Value           NaNValue;
    js::Value           negativeInfinityValue;
    js::Value           positiveInfinityValue;

    JSAtom              *emptyString;

    /* List of active contexts sharing this runtime. */
    JSCList             contextList;

    bool hasContexts() const {
        return !JS_CLIST_IS_EMPTY(&contextList);
    }

    /* Per runtime debug hooks -- see jsprvtd.h and jsdbgapi.h. */
    JSDebugHooks        debugHooks;

    /* If true, new compartments are initially in debug mode. */
    bool                debugMode;

    /* If true, new scripts must be created with PC counter information. */
    bool                profilingScripts;

    /* Had an out-of-memory error which did not populate an exception. */
    JSBool              hadOutOfMemory;

    /*
     * Linked list of all js::Debugger objects. This may be accessed by the GC
     * thread, if any, or a thread that is in a request and holds gcLock.
     */
    JSCList             debuggerList;

    /* Client opaque pointers */
    void                *data;

#ifdef JS_THREADSAFE
    /* These combine to interlock the GC and new requests. */
    PRLock              *gcLock;

    js::GCHelperThread  gcHelperThread;
#endif /* JS_THREADSAFE */

    uint32_t            debuggerMutations;

    const JSSecurityCallbacks *securityCallbacks;
    JSDestroyPrincipalsOp destroyPrincipals;

    /* Structured data callbacks are runtime-wide. */
    const JSStructuredCloneCallbacks *structuredCloneCallbacks;

    /* Call this to accumulate telemetry data. */
    JSAccumulateTelemetryDataCallback telemetryCallback;

    /*
     * The propertyRemovals counter is incremented for every JSObject::clear,
     * and for each JSObject::remove method call that frees a slot in the given
     * object. See js_NativeGet and js_NativeSet in jsobj.cpp.
     */
    int32_t             propertyRemovals;

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

    /*
     * Flag indicating that we are waiving any soft limits on the GC heap
     * because we want allocations to be infallible (except when we hit OOM).
     */
    bool                waiveGCQuota;

    /*
     * The GSN cache is per thread since even multi-cx-per-thread embeddings
     * do not interleave js_GetSrcNote calls.
     */
    js::GSNCache        gsnCache;

    /* Property cache for faster call/get/set invocation. */
    js::PropertyCache   propertyCache;

    /* State used by jsdtoa.cpp. */
    DtoaState           *dtoaState;

    /* List of currently pending operations on proxies. */
    js::PendingProxyOperation *pendingProxyOperation;

    js::ConservativeGCData conservativeGC;

  private:
    JSPrincipals        *trustedPrincipals_;
  public:
    void setTrustedPrincipals(JSPrincipals *p) { trustedPrincipals_ = p; }
    JSPrincipals *trustedPrincipals() const { return trustedPrincipals_; }

    /* Literal table maintained by jsatom.c functions. */
    JSAtomState         atomState;

    /* Tables of strings that are pre-allocated in the atomsCompartment. */
    js::StaticStrings   staticStrings;

    JSWrapObjectCallback wrapObjectCallback;
    JSPreWrapCallback    preWrapObjectCallback;
    js::PreserveWrapperCallback preserveWrapperCallback;

#ifdef DEBUG
    size_t              noGCOrAllocationCheck;
#endif

    /*
     * To ensure that cx->malloc does not cause a GC, we set this flag during
     * OOM reporting (in js_ReportOutOfMemory). If a GC is requested while
     * reporting the OOM, we ignore it.
     */
    int32_t             inOOMReport;

    bool                jitHardening;

    JSRuntime();
    ~JSRuntime();

    bool init(uint32_t maxbytes);

    JSRuntime *thisFromCtor() { return this; }

    /*
     * Call the system malloc while checking for GC memory pressure and
     * reporting OOM error when cx is not null. We will not GC from here.
     */
    void* malloc_(size_t bytes, JSContext *cx = NULL) {
        updateMallocCounter(cx, bytes);
        void *p = ::js_malloc(bytes);
        return JS_LIKELY(!!p) ? p : onOutOfMemory(NULL, bytes, cx);
    }

    /*
     * Call the system calloc while checking for GC memory pressure and
     * reporting OOM error when cx is not null. We will not GC from here.
     */
    void* calloc_(size_t bytes, JSContext *cx = NULL) {
        updateMallocCounter(cx, bytes);
        void *p = ::js_calloc(bytes);
        return JS_LIKELY(!!p) ? p : onOutOfMemory(reinterpret_cast<void *>(1), bytes, cx);
    }

    void* realloc_(void* p, size_t oldBytes, size_t newBytes, JSContext *cx = NULL) {
        JS_ASSERT(oldBytes < newBytes);
        updateMallocCounter(cx, newBytes - oldBytes);
        void *p2 = ::js_realloc(p, newBytes);
        return JS_LIKELY(!!p2) ? p2 : onOutOfMemory(p, newBytes, cx);
    }

    void* realloc_(void* p, size_t bytes, JSContext *cx = NULL) {
        /*
         * For compatibility we do not account for realloc that increases
         * previously allocated memory.
         */
        if (!p)
            updateMallocCounter(cx, bytes);
        void *p2 = ::js_realloc(p, bytes);
        return JS_LIKELY(!!p2) ? p2 : onOutOfMemory(p, bytes, cx);
    }

    inline void free_(void* p) {
        /* FIXME: Making this free in the background is buggy. Can it work? */
        js::Foreground::free_(p);
    }

    JS_DECLARE_NEW_METHODS(malloc_, JS_ALWAYS_INLINE)
    JS_DECLARE_DELETE_METHODS(free_, JS_ALWAYS_INLINE)

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

    /*
     * Call this after allocating memory held by GC things, to update memory
     * pressure counters or report the OOM error if necessary. If oomError and
     * cx is not null the function also reports OOM error.
     *
     * The function must be called outside the GC lock and in case of OOM error
     * the caller must ensure that no deadlock possible during OOM reporting.
     */
    void updateMallocCounter(JSContext *cx, size_t nbytes);

    /*
     * The function must be called outside the GC lock.
     */
    JS_FRIEND_API(void) onTooMuchMalloc();

    /*
     * This should be called after system malloc/realloc returns NULL to try
     * to recove some memory or to report an error. Failures in malloc and
     * calloc are signaled by p == null and p == reinterpret_cast<void *>(1).
     * Other values of p mean a realloc failure.
     *
     * The function must be called outside the GC lock.
     */
    JS_FRIEND_API(void *) onOutOfMemory(void *p, size_t nbytes, JSContext *cx);

    void triggerOperationCallback();

    void setJitHardening(bool enabled);
    bool getJitHardening() const {
        return jitHardening;
    }

    void sizeOfExcludingThis(JSMallocSizeOfFun mallocSizeOf, size_t *normal, size_t *temporary,
                             size_t *regexpCode, size_t *stackCommitted, size_t *gcMarker);
};

/* Common macros to access thread-local caches in JSRuntime. */
#define JS_PROPERTY_CACHE(cx)   (cx->runtime->propertyCache)

#define JS_KEEP_ATOMS(rt)   (rt)->gcKeepAtoms++;
#define JS_UNKEEP_ATOMS(rt) (rt)->gcKeepAtoms--;

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

namespace js {

template <typename T> class Root;
class CheckRoot;

struct AutoResolving;

static inline bool
OptionsHasXML(uint32_t options)
{
    return !!(options & JSOPTION_XML);
}

static inline bool
OptionsSameVersionFlags(uint32_t self, uint32_t other)
{
    static const uint32_t mask = JSOPTION_XML;
    return !((self & mask) ^ (other & mask));
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
static const unsigned MASK         = 0x0FFF; /* see JSVersion in jspubtd.h */
static const unsigned HAS_XML      = 0x1000; /* flag induced by XML option */
static const unsigned FULL_MASK    = 0x3FFF;
}

static inline JSVersion
VersionNumber(JSVersion version)
{
    return JSVersion(uint32_t(version) & VersionFlags::MASK);
}

static inline bool
VersionHasXML(JSVersion version)
{
    return !!(version & VersionFlags::HAS_XML);
}

/* @warning This is a distinct condition from having the XML flag set. */
static inline bool
VersionShouldParseXML(JSVersion version)
{
    return VersionHasXML(version) || VersionNumber(version) >= JSVERSION_1_6;
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

static inline unsigned
VersionFlagsToOptions(JSVersion version)
{
    unsigned copts = VersionHasXML(version) ? JSOPTION_XML : 0;
    JS_ASSERT((copts & JSCOMPILEOPTION_MASK) == copts);
    return copts;
}

static inline JSVersion
OptionFlagsToVersion(unsigned options, JSVersion version)
{
    return VersionSetXML(version, OptionsHasXML(options));
}

static inline bool
VersionIsKnown(JSVersion version)
{
    return VersionNumber(version) != JSVERSION_UNKNOWN;
}

typedef HashSet<JSObject *,
                DefaultHasher<JSObject *>,
                SystemAllocPolicy> BusyArraysSet;

} /* namespace js */

struct JSContext : js::ContextFriendFields
{
    explicit JSContext(JSRuntime *rt);
    JSContext *thisDuringConstruction() { return this; }
    ~JSContext();

    /* JSRuntime contextList linkage. */
    JSCList             link;

  private:
    /* See JSContext::findVersion. */
    JSVersion           defaultVersion;      /* script compilation version */
    JSVersion           versionOverride;     /* supercedes defaultVersion when valid */
    bool                hasVersionOverride;

    /* Exception state -- the exception member is a GC root by definition. */
    JSBool              throwing;            /* is there a pending exception? */
    js::Value           exception;           /* most-recently-thrown exception */

    /* Per-context run options. */
    unsigned            runOptions;          /* see jsapi.h for JSOPTION_* */

  public:
    int32_t             reportGranularity;  /* see jsprobes.h */

    /* Locale specific callbacks for string conversion. */
    JSLocaleCallbacks   *localeCallbacks;

    js::AutoResolving   *resolvingList;

    /* True if generating an error, to prevent runaway recursion. */
    bool                generatingError;

    /* GC heap compartment. */
    JSCompartment       *compartment;

    inline void setCompartment(JSCompartment *compartment);

    /* Current execution stack. */
    js::ContextStack    stack;

    /* ContextStack convenience functions */
    inline bool hasfp() const               { return stack.hasfp(); }
    inline js::StackFrame* fp() const       { return stack.fp(); }
    inline js::StackFrame* maybefp() const  { return stack.maybefp(); }
    inline js::FrameRegs& regs() const      { return stack.regs(); }
    inline js::FrameRegs* maybeRegs() const { return stack.maybeRegs(); }

    /* Set cx->compartment based on the current scope chain. */
    void resetCompartment();

    /* Wrap cx->exception for the current compartment. */
    void wrapPendingException();

  private:
    /* Lazily initialized pool of maps used during parse/emit. */
    js::ParseMapPool    *parseMapPool_;

  public:
    /* Top-level object and pointer to top stack frame's scope chain. */
    JSObject            *globalObject;

    /* State for object and array toSource conversion. */
    JSSharpObjectMap    sharpObjectMap;
    js::BusyArraysSet   busyArrays;

    /* Argument formatter support for JS_{Convert,Push}Arguments{,VA}. */
    JSArgumentFormatMap *argumentFormatMap;

    /* Last message string and log file for debugging. */
    char                *lastMessage;

    /* Per-context optional error reporter. */
    JSErrorReporter     errorReporter;

    /* Branch callback. */
    JSOperationCallback operationCallback;

    /* Client opaque pointers. */
    void                *data;
    void                *data2;

    inline js::RegExpStatics *regExpStatics();

  public:
    js::ParseMapPool &parseMapPool() {
        JS_ASSERT(parseMapPool_);
        return *parseMapPool_;
    }

    inline bool ensureParseMapPool();

    /*
     * The default script compilation version can be set iff there is no code running.
     * This typically occurs via the JSAPI right after a context is constructed.
     */
    inline bool canSetDefaultVersion() const;

    /* Force a version for future script compilation. */
    inline void overrideVersion(JSVersion newVersion);

    /* Set the default script compilation version. */
    void setDefaultVersion(JSVersion version) {
        defaultVersion = version;
    }

    void clearVersionOverride() { hasVersionOverride = false; }
    JSVersion getDefaultVersion() const { return defaultVersion; }
    bool isVersionOverridden() const { return hasVersionOverride; }

    JSVersion getVersionOverride() const {
        JS_ASSERT(isVersionOverridden());
        return versionOverride;
    }

    /*
     * Set the default version if possible; otherwise, force the version.
     * Return whether an override occurred.
     */
    inline bool maybeOverrideVersion(JSVersion newVersion);

    /*
     * If there is no code on the stack, turn the override version into the
     * default version.
     */
    void maybeMigrateVersionOverride() {
        JS_ASSERT(stack.empty());
        if (JS_UNLIKELY(isVersionOverridden())) {
            defaultVersion = versionOverride;
            clearVersionOverride();
        }
    }

    /*
     * Return:
     * - The override version, if there is an override version.
     * - The newest scripted frame's version, if there is such a frame.
     * - The default version.
     *
     * Note: if this ever shows up in a profile, just add caching!
     */
    inline JSVersion findVersion() const;

    void setRunOptions(unsigned ropts) {
        JS_ASSERT((ropts & JSRUNOPTION_MASK) == ropts);
        runOptions = ropts;
    }

    /* Note: may override the version. */
    inline void setCompileOptions(unsigned newcopts);

    unsigned getRunOptions() const { return runOptions; }
    inline unsigned getCompileOptions() const;
    inline unsigned allOptions() const;

    bool hasRunOption(unsigned ropt) const {
        JS_ASSERT((ropt & JSRUNOPTION_MASK) == ropt);
        return !!(runOptions & ropt);
    }

    bool hasStrictOption() const { return hasRunOption(JSOPTION_STRICT); }
    bool hasWErrorOption() const { return hasRunOption(JSOPTION_WERROR); }
    bool hasAtLineOption() const { return hasRunOption(JSOPTION_ATLINE); }

    js::LifoAlloc &tempLifoAlloc() { return runtime->tempLifoAlloc; }
    inline js::LifoAlloc &typeLifoAlloc();

#ifdef JS_THREADSAFE
    unsigned            outstandingRequests;/* number of JS_BeginRequest calls
                                               without the corresponding
                                               JS_EndRequest. */
#endif


#ifdef JSGC_ROOT_ANALYSIS

    /*
     * Stack allocated GC roots for stack GC heap pointers, which may be
     * overwritten if moved during a GC.
     */
    js::Root<js::gc::Cell*> *thingGCRooters[js::THING_ROOT_COUNT];

#ifdef DEBUG
    /*
     * Stack allocated list of stack locations which hold non-relocatable
     * GC heap pointers (where the target is rooted somewhere else) or integer
     * values which may be confused for GC heap pointers. These are used to
     * suppress false positives which occur when a rooting analysis treats the
     * location as holding a relocatable pointer, but have no other effect on
     * GC behavior.
     */
    js::CheckRoot *checkGCRooters;
#endif

#endif /* JSGC_ROOT_ANALYSIS */

    /* Stored here to avoid passing it around as a parameter. */
    unsigned               resolveFlags;

    /* Random number generator state, used by jsmath.cpp. */
    int64_t             rngSeed;

    /* Location to stash the iteration value between JSOP_MOREITER and JSOP_ITERNEXT. */
    js::Value           iterValue;

#ifdef JS_METHODJIT
    bool                 methodJitEnabled;

    inline js::mjit::JaegerCompartment *jaegerCompartment();
#endif

    bool                 inferenceEnabled;

    bool typeInferenceEnabled() { return inferenceEnabled; }

    /* Caller must be holding runtime->gcLock. */
    void updateJITEnabled();

#ifdef MOZ_TRACE_JSCALLS
    /* Function entry/exit debugging callback. */
    JSFunctionCallback    functionCallback;

    void doFunctionCallback(const JSFunction *fun,
                            const JSScript *scr,
                            int entering) const
    {
        if (functionCallback)
            functionCallback(fun, scr, this, entering);
    }
#endif

    DSTOffsetCache dstOffsetCache;

    /* List of currently active non-escaping enumerators (for-in). */
    JSObject *enumerators;

  private:
    /*
     * To go from a live generator frame (on the stack) to its generator object
     * (see comment js_FloatingFrameIfGenerator), we maintain a stack of active
     * generators, pushing and popping when entering and leaving generator
     * frames, respectively.
     */
    js::Vector<JSGenerator *, 2, js::SystemAllocPolicy> genStack;

  public:
    /* Return the generator object for the given generator frame. */
    JSGenerator *generatorFor(js::StackFrame *fp) const;

    /* Early OOM-check. */
    inline bool ensureGeneratorStackSpace();

    bool enterGenerator(JSGenerator *gen) {
        return genStack.append(gen);
    }

    void leaveGenerator(JSGenerator *gen) {
        JS_ASSERT(genStack.back() == gen);
        genStack.popBack();
    }

#ifdef JS_THREADSAFE
    /*
     * When non-null JSContext::free_ delegates the job to the background
     * thread.
     */
    js::GCHelperThread *gcBackgroundFree;
#endif

    inline void* malloc_(size_t bytes) {
        return runtime->malloc_(bytes, this);
    }

    inline void* mallocNoReport(size_t bytes) {
        JS_ASSERT(bytes != 0);
        return runtime->malloc_(bytes, NULL);
    }

    inline void* calloc_(size_t bytes) {
        JS_ASSERT(bytes != 0);
        return runtime->calloc_(bytes, this);
    }

    inline void* realloc_(void* p, size_t bytes) {
        return runtime->realloc_(p, bytes, this);
    }

    inline void* realloc_(void* p, size_t oldBytes, size_t newBytes) {
        return runtime->realloc_(p, oldBytes, newBytes, this);
    }

    inline void free_(void* p) {
#ifdef JS_THREADSAFE
        if (gcBackgroundFree) {
            gcBackgroundFree->freeLater(p);
            return;
        }
#endif
        runtime->free_(p);
    }

    JS_DECLARE_NEW_METHODS(malloc_, inline)
    JS_DECLARE_DELETE_METHODS(free_, inline)

    void purge();

    /* For DEBUG. */
    inline void assertValidStackDepth(unsigned depth);

    bool isExceptionPending() {
        return throwing;
    }

    js::Value getPendingException() {
        JS_ASSERT(throwing);
        return exception;
    }

    void setPendingException(js::Value v);

    void clearPendingException() {
        this->throwing = false;
        this->exception.setUndefined();
    }

    /*
     * Count of currently active compilations.
     * When there are compilations active for the context, the GC must not
     * purge the ParseMapPool.
     */
    unsigned activeCompilations;

#ifdef DEBUG
    /*
     * Controls whether a quadratic-complexity assertion is performed during
     * stack iteration, defaults to true.
     */
    bool stackIterAssertionEnabled;
#endif

    /*
     * See JS_SetTrustedPrincipals in jsapi.h.
     * Note: !cx->compartment is treated as trusted.
     */
    bool runningWithTrustedPrincipals() const;

    JS_FRIEND_API(size_t) sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf) const;

    static inline JSContext *fromLinkField(JSCList *link) {
        JS_ASSERT(link);
        return reinterpret_cast<JSContext *>(uintptr_t(link) - offsetof(JSContext, link));
    }

    void mark(JSTracer *trc);

  private:
    /*
     * The allocation code calls the function to indicate either OOM failure
     * when p is null or that a memory pressure counter has reached some
     * threshold when p is not null. The function takes the pointer and not
     * a boolean flag to minimize the amount of code in its inlined callers.
     */
    JS_FRIEND_API(void) checkMallocGCPressure(void *p);
}; /* struct JSContext */

namespace js {

struct AutoResolving {
  public:
    enum Kind {
        LOOKUP,
        WATCH
    };

    AutoResolving(JSContext *cx, JSObject *obj, jsid id, Kind kind = LOOKUP
                  JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : context(cx), object(obj), id(id), kind(kind), link(cx->resolvingList)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_ASSERT(obj);
        cx->resolvingList = this;
    }

    ~AutoResolving() {
        JS_ASSERT(context->resolvingList == this);
        context->resolvingList = link;
    }

    bool alreadyStarted() const {
        return link && alreadyStartedSlow();
    }

  private:
    bool alreadyStartedSlow() const;

    JSContext           *const context;
    JSObject            *const object;
    jsid                const id;
    Kind                const kind;
    AutoResolving       *const link;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

#ifdef JS_HAS_XML_SUPPORT
class AutoXMLRooter : private AutoGCRooter {
  public:
    AutoXMLRooter(JSContext *cx, JSXML *xml
                  JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : AutoGCRooter(cx, XML), xml(xml)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_ASSERT(xml);
    }

    friend void AutoGCRooter::trace(JSTracer *trc);

  private:
    JSXML * const xml;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};
#endif /* JS_HAS_XML_SUPPORT */

#ifdef JS_THREADSAFE
# define JS_LOCK_GC(rt)    PR_Lock((rt)->gcLock)
# define JS_UNLOCK_GC(rt)  PR_Unlock((rt)->gcLock)
#else
# define JS_LOCK_GC(rt)
# define JS_UNLOCK_GC(rt)
#endif

class AutoLockGC
{
  public:
    explicit AutoLockGC(JSRuntime *rt = NULL
                        MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : runtime(rt)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        // Avoid MSVC warning C4390 for non-threadsafe builds.
#ifdef JS_THREADSAFE
        if (rt)
            JS_LOCK_GC(rt);
#endif
    }

    ~AutoLockGC()
    {
#ifdef JS_THREADSAFE
        if (runtime)
            JS_UNLOCK_GC(runtime);
#endif
    }

    bool locked() const {
        return !!runtime;
    }

    void lock(JSRuntime *rt) {
        JS_ASSERT(rt);
        JS_ASSERT(!runtime);
        runtime = rt;
        JS_LOCK_GC(rt);
    }

  private:
    JSRuntime *runtime;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoUnlockGC {
  private:
    JSRuntime *rt;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    explicit AutoUnlockGC(JSRuntime *rt
                          JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : rt(rt)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_UNLOCK_GC(rt);
    }
    ~AutoUnlockGC() { JS_LOCK_GC(rt); }
};

class AutoKeepAtoms {
    JSRuntime *rt;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    explicit AutoKeepAtoms(JSRuntime *rt
                           JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : rt(rt)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_KEEP_ATOMS(rt);
    }
    ~AutoKeepAtoms() { JS_UNKEEP_ATOMS(rt); }
};

class AutoReleasePtr {
    JSContext   *cx;
    void        *ptr;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

    AutoReleasePtr(const AutoReleasePtr &other) MOZ_DELETE;
    AutoReleasePtr operator=(const AutoReleasePtr &other) MOZ_DELETE;

  public:
    explicit AutoReleasePtr(JSContext *cx, void *ptr
                            JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), ptr(ptr)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }
    ~AutoReleasePtr() { cx->free_(ptr); }
};

/*
 * FIXME: bug 602774: cleaner API for AutoReleaseNullablePtr
 */
class AutoReleaseNullablePtr {
    JSContext   *cx;
    void        *ptr;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

    AutoReleaseNullablePtr(const AutoReleaseNullablePtr &other) MOZ_DELETE;
    AutoReleaseNullablePtr operator=(const AutoReleaseNullablePtr &other) MOZ_DELETE;

  public:
    explicit AutoReleaseNullablePtr(JSContext *cx, void *ptr
                                    JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), ptr(ptr)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }
    void reset(void *ptr2) {
        if (ptr)
            cx->free_(ptr);
        ptr = ptr2;
    }
    ~AutoReleaseNullablePtr() { if (ptr) cx->free_(ptr); }
};

} /* namespace js */

class JSAutoResolveFlags
{
  public:
    JSAutoResolveFlags(JSContext *cx, unsigned flags
                       JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : mContext(cx), mSaved(cx->resolveFlags)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        cx->resolveFlags = flags;
    }

    ~JSAutoResolveFlags() { mContext->resolveFlags = mSaved; }

  private:
    JSContext *mContext;
    unsigned mSaved;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

namespace js {

/*
 * Enumerate all contexts in a runtime.
 */
class ContextIter {
    JSCList *begin;
    JSCList *end;

public:
    explicit ContextIter(JSRuntime *rt) {
        end = &rt->contextList;
        begin = end->next;
    }

    bool done() const {
        return begin == end;
    }

    void next() {
        JS_ASSERT(!done());
        begin = begin->next;
    }

    JSContext *get() const {
        JS_ASSERT(!done());
        return JSContext::fromLinkField(begin);
    }

    operator JSContext *() const {
        return get();
    }

    JSContext *operator ->() const {
        return get();
    }
};

} /* namespace js */

/*
 * Create and destroy functions for JSContext, which is manually allocated
 * and exclusively owned.
 */
extern JSContext *
js_NewContext(JSRuntime *rt, size_t stackChunkSize);

typedef enum JSDestroyContextMode {
    JSDCM_NO_GC,
    JSDCM_MAYBE_GC,
    JSDCM_FORCE_GC,
    JSDCM_NEW_FAILED
} JSDestroyContextMode;

extern void
js_DestroyContext(JSContext *cx, JSDestroyContextMode mode);

#ifdef va_start
extern JSBool
js_ReportErrorVA(JSContext *cx, unsigned flags, const char *format, va_list ap);

extern JSBool
js_ReportErrorNumberVA(JSContext *cx, unsigned flags, JSErrorCallback callback,
                       void *userRef, const unsigned errorNumber,
                       JSBool charArgs, va_list ap);

extern JSBool
js_ExpandErrorArguments(JSContext *cx, JSErrorCallback callback,
                        void *userRef, const unsigned errorNumber,
                        char **message, JSErrorReport *reportp,
                        bool charArgs, va_list ap);
#endif

namespace js {

/* |callee| requires a usage string provided by JS_DefineFunctionsWithHelp. */
extern void
ReportUsageError(JSContext *cx, JSObject *callee, const char *msg);

} /* namespace js */

extern void
js_ReportOutOfMemory(JSContext *cx);

extern JS_FRIEND_API(void)
js_ReportAllocationOverflow(JSContext *cx);

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
js_ReportIsNullOrUndefined(JSContext *cx, int spindex, const js::Value &v,
                           JSString *fallback);

extern void
js_ReportMissingArg(JSContext *cx, const js::Value &v, unsigned arg);

/*
 * Report error using js_DecompileValueGenerator(cx, spindex, v, fallback) as
 * the first argument for the error message. If the error message has less
 * then 3 arguments, use null for arg1 or arg2.
 */
extern JSBool
js_ReportValueErrorFlags(JSContext *cx, unsigned flags, const unsigned errorNumber,
                         int spindex, const js::Value &v, JSString *fallback,
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

#ifdef JS_THREADSAFE
# define JS_ASSERT_REQUEST_DEPTH(cx)  JS_ASSERT((cx)->runtime->requestDepth >= 1)
#else
# define JS_ASSERT_REQUEST_DEPTH(cx)  ((void) 0)
#endif

/*
 * If the operation callback flag was set, call the operation callback.
 * This macro can run the full GC. Return true if it is OK to continue and
 * false otherwise.
 */
#define JS_CHECK_OPERATION_LIMIT(cx)                                          \
    (JS_ASSERT_REQUEST_DEPTH(cx),                                             \
     (!cx->runtime->interrupt || js_InvokeOperationCallback(cx)))

/*
 * Invoke the operation callback and return false if the current execution
 * is to be terminated.
 */
extern JSBool
js_InvokeOperationCallback(JSContext *cx);

extern JSBool
js_HandleExecutionInterrupt(JSContext *cx);

extern jsbytecode*
js_GetCurrentBytecodePC(JSContext* cx);

extern JSScript *
js_GetCurrentScript(JSContext* cx);

namespace js {

#ifdef JS_METHODJIT
namespace mjit {
    void ExpandInlineFrames(JSCompartment *compartment);
}
#endif

} /* namespace js */

/* How much expansion of inlined frames to do when inspecting the stack. */
enum FrameExpandKind {
    FRAME_EXPAND_NONE = 0,
    FRAME_EXPAND_ALL = 1
};

namespace js {

/************************************************************************/

static JS_ALWAYS_INLINE void
MakeRangeGCSafe(Value *vec, size_t len)
{
    PodZero(vec, len);
}

static JS_ALWAYS_INLINE void
MakeRangeGCSafe(Value *beg, Value *end)
{
    PodZero(beg, end - beg);
}

static JS_ALWAYS_INLINE void
MakeRangeGCSafe(jsid *beg, jsid *end)
{
    for (jsid *id = beg; id != end; ++id)
        *id = INT_TO_JSID(0);
}

static JS_ALWAYS_INLINE void
MakeRangeGCSafe(jsid *vec, size_t len)
{
    MakeRangeGCSafe(vec, vec + len);
}

static JS_ALWAYS_INLINE void
MakeRangeGCSafe(const Shape **beg, const Shape **end)
{
    PodZero(beg, end - beg);
}

static JS_ALWAYS_INLINE void
MakeRangeGCSafe(const Shape **vec, size_t len)
{
    PodZero(vec, len);
}

static JS_ALWAYS_INLINE void
SetValueRangeToUndefined(Value *beg, Value *end)
{
    for (Value *v = beg; v != end; ++v)
        v->setUndefined();
}

static JS_ALWAYS_INLINE void
SetValueRangeToUndefined(Value *vec, size_t len)
{
    SetValueRangeToUndefined(vec, vec + len);
}

static JS_ALWAYS_INLINE void
SetValueRangeToNull(Value *beg, Value *end)
{
    for (Value *v = beg; v != end; ++v)
        v->setNull();
}

static JS_ALWAYS_INLINE void
SetValueRangeToNull(Value *vec, size_t len)
{
    SetValueRangeToNull(vec, vec + len);
}

class AutoObjectVector : public AutoVectorRooter<JSObject *>
{
  public:
    explicit AutoObjectVector(JSContext *cx
                              JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoVectorRooter<JSObject *>(cx, OBJVECTOR)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoShapeVector : public AutoVectorRooter<const Shape *>
{
  public:
    explicit AutoShapeVector(JSContext *cx
                             JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoVectorRooter<const Shape *>(cx, SHAPEVECTOR)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoValueArray : public AutoGCRooter
{
    js::Value *start_;
    unsigned length_;

  public:
    AutoValueArray(JSContext *cx, js::Value *start, unsigned length
                   JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoGCRooter(cx, VALARRAY), start_(start), length_(length)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    Value *start() { return start_; }
    unsigned length() const { return length_; }

    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/*
 * Allocation policy that uses JSRuntime::malloc_ and friends, so that
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
    RuntimeAllocPolicy(JSRuntime *rt) : runtime(rt) {}
    RuntimeAllocPolicy(JSContext *cx) : runtime(cx->runtime) {}
    void *malloc_(size_t bytes) { return runtime->malloc_(bytes); }
    void *realloc_(void *p, size_t bytes) { return runtime->realloc_(p, bytes); }
    void free_(void *p) { runtime->free_(p); }
    void reportAllocOverflow() const {}
};

/*
 * FIXME bug 647103 - replace these *AllocPolicy names.
 */
class ContextAllocPolicy
{
    JSContext *const cx;

  public:
    ContextAllocPolicy(JSContext *cx) : cx(cx) {}
    JSContext *context() const { return cx; }
    void *malloc_(size_t bytes) { return cx->malloc_(bytes); }
    void *realloc_(void *p, size_t oldBytes, size_t bytes) { return cx->realloc_(p, oldBytes, bytes); }
    void free_(void *p) { cx->free_(p); }
    void reportAllocOverflow() const { js_ReportAllocationOverflow(cx); }
};

} /* namespace js */

#ifdef _MSC_VER
#pragma warning(pop)
#pragma warning(pop)
#endif

#endif /* jscntxt_h___ */

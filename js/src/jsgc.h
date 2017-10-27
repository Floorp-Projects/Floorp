/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS Garbage Collector. */

#ifndef jsgc_h
#define jsgc_h

#include "mozilla/Atomics.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypeTraits.h"

#include "js/GCAPI.h"
#include "js/SliceBudget.h"
#include "js/Vector.h"
#include "threading/ConditionVariable.h"
#include "threading/Thread.h"
#include "vm/NativeObject.h"

namespace js {

namespace gcstats {
struct Statistics;
} // namespace gcstats

class Nursery;

namespace gc {

#define GCSTATES(D) \
    D(NotActive) \
    D(MarkRoots) \
    D(Mark) \
    D(Sweep) \
    D(Finalize) \
    D(Compact) \
    D(Decommit)
enum class State {
#define MAKE_STATE(name) name,
    GCSTATES(MAKE_STATE)
#undef MAKE_STATE
};

// Reasons we reset an ongoing incremental GC or perform a non-incremental GC.
#define GC_ABORT_REASONS(D) \
    D(None) \
    D(NonIncrementalRequested) \
    D(AbortRequested) \
    D(Unused1) \
    D(IncrementalDisabled) \
    D(ModeChange) \
    D(MallocBytesTrigger) \
    D(GCBytesTrigger) \
    D(ZoneChange) \
    D(CompartmentRevived)
enum class AbortReason {
#define MAKE_REASON(name) name,
    GC_ABORT_REASONS(MAKE_REASON)
#undef MAKE_REASON
};

/*
 * Map from C++ type to alloc kind for non-object types. JSObject does not have
 * a 1:1 mapping, so must use Arena::thingSize.
 *
 * The AllocKind is available as MapTypeToFinalizeKind<SomeType>::kind.
 */
template <typename T> struct MapTypeToFinalizeKind {};
#define EXPAND_MAPTYPETOFINALIZEKIND(allocKind, traceKind, type, sizedType) \
    template <> struct MapTypeToFinalizeKind<type> { \
        static const AllocKind kind = AllocKind::allocKind; \
    };
FOR_EACH_NONOBJECT_ALLOCKIND(EXPAND_MAPTYPETOFINALIZEKIND)
#undef EXPAND_MAPTYPETOFINALIZEKIND

template <typename T> struct ParticipatesInCC {};
#define EXPAND_PARTICIPATES_IN_CC(_, type, addToCCKind) \
    template <> struct ParticipatesInCC<type> { static const bool value = addToCCKind; };
JS_FOR_EACH_TRACEKIND(EXPAND_PARTICIPATES_IN_CC)
#undef EXPAND_PARTICIPATES_IN_CC

static inline bool
IsNurseryAllocable(AllocKind kind)
{
    MOZ_ASSERT(IsValidAllocKind(kind));
    static const bool map[] = {
        true,      /* AllocKind::FUNCTION */
        true,      /* AllocKind::FUNCTION_EXTENDED */
        false,     /* AllocKind::OBJECT0 */
        true,      /* AllocKind::OBJECT0_BACKGROUND */
        false,     /* AllocKind::OBJECT2 */
        true,      /* AllocKind::OBJECT2_BACKGROUND */
        false,     /* AllocKind::OBJECT4 */
        true,      /* AllocKind::OBJECT4_BACKGROUND */
        false,     /* AllocKind::OBJECT8 */
        true,      /* AllocKind::OBJECT8_BACKGROUND */
        false,     /* AllocKind::OBJECT12 */
        true,      /* AllocKind::OBJECT12_BACKGROUND */
        false,     /* AllocKind::OBJECT16 */
        true,      /* AllocKind::OBJECT16_BACKGROUND */
        false,     /* AllocKind::SCRIPT */
        false,     /* AllocKind::LAZY_SCRIPT */
        false,     /* AllocKind::SHAPE */
        false,     /* AllocKind::ACCESSOR_SHAPE */
        false,     /* AllocKind::BASE_SHAPE */
        false,     /* AllocKind::OBJECT_GROUP */
        false,     /* AllocKind::FAT_INLINE_STRING */
        false,     /* AllocKind::STRING */
        false,     /* AllocKind::EXTERNAL_STRING */
        false,     /* AllocKind::FAT_INLINE_ATOM */
        false,     /* AllocKind::ATOM */
        false,     /* AllocKind::SYMBOL */
        false,     /* AllocKind::JITCODE */
        false,     /* AllocKind::SCOPE */
        false,     /* AllocKind::REGEXP_SHARED */
    };
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(map) == size_t(AllocKind::LIMIT));
    return map[size_t(kind)];
}

static inline bool
IsBackgroundFinalized(AllocKind kind)
{
    MOZ_ASSERT(IsValidAllocKind(kind));
    static const bool map[] = {
        true,      /* AllocKind::FUNCTION */
        true,      /* AllocKind::FUNCTION_EXTENDED */
        false,     /* AllocKind::OBJECT0 */
        true,      /* AllocKind::OBJECT0_BACKGROUND */
        false,     /* AllocKind::OBJECT2 */
        true,      /* AllocKind::OBJECT2_BACKGROUND */
        false,     /* AllocKind::OBJECT4 */
        true,      /* AllocKind::OBJECT4_BACKGROUND */
        false,     /* AllocKind::OBJECT8 */
        true,      /* AllocKind::OBJECT8_BACKGROUND */
        false,     /* AllocKind::OBJECT12 */
        true,      /* AllocKind::OBJECT12_BACKGROUND */
        false,     /* AllocKind::OBJECT16 */
        true,      /* AllocKind::OBJECT16_BACKGROUND */
        false,     /* AllocKind::SCRIPT */
        true,      /* AllocKind::LAZY_SCRIPT */
        true,      /* AllocKind::SHAPE */
        true,      /* AllocKind::ACCESSOR_SHAPE */
        true,      /* AllocKind::BASE_SHAPE */
        true,      /* AllocKind::OBJECT_GROUP */
        true,      /* AllocKind::FAT_INLINE_STRING */
        true,      /* AllocKind::STRING */
        true,      /* AllocKind::EXTERNAL_STRING */
        true,      /* AllocKind::FAT_INLINE_ATOM */
        true,      /* AllocKind::ATOM */
        true,      /* AllocKind::SYMBOL */
        false,     /* AllocKind::JITCODE */
        true,      /* AllocKind::SCOPE */
        true,      /* AllocKind::REGEXP_SHARED */
    };
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(map) == size_t(AllocKind::LIMIT));
    return map[size_t(kind)];
}

static inline bool
CanBeFinalizedInBackground(AllocKind kind, const Class* clasp)
{
    MOZ_ASSERT(IsObjectAllocKind(kind));
    /* If the class has no finalizer or a finalizer that is safe to call on
     * a different thread, we change the alloc kind. For example,
     * AllocKind::OBJECT0 calls the finalizer on the active thread,
     * AllocKind::OBJECT0_BACKGROUND calls the finalizer on the gcHelperThread.
     * IsBackgroundFinalized is called to prevent recursively incrementing
     * the alloc kind; kind may already be a background finalize kind.
     */
    return (!IsBackgroundFinalized(kind) &&
            (!clasp->hasFinalize() || (clasp->flags & JSCLASS_BACKGROUND_FINALIZE)));
}

/* Capacity for slotsToThingKind */
const size_t SLOTS_TO_THING_KIND_LIMIT = 17;

extern const AllocKind slotsToThingKind[];

/* Get the best kind to use when making an object with the given slot count. */
static inline AllocKind
GetGCObjectKind(size_t numSlots)
{
    if (numSlots >= SLOTS_TO_THING_KIND_LIMIT)
        return AllocKind::OBJECT16;
    return slotsToThingKind[numSlots];
}

/* As for GetGCObjectKind, but for dense array allocation. */
static inline AllocKind
GetGCArrayKind(size_t numElements)
{
    /*
     * Dense arrays can use their fixed slots to hold their elements array
     * (less two Values worth of ObjectElements header), but if more than the
     * maximum number of fixed slots is needed then the fixed slots will be
     * unused.
     */
    JS_STATIC_ASSERT(ObjectElements::VALUES_PER_HEADER == 2);
    if (numElements > NativeObject::MAX_DENSE_ELEMENTS_COUNT ||
        numElements + ObjectElements::VALUES_PER_HEADER >= SLOTS_TO_THING_KIND_LIMIT)
    {
        return AllocKind::OBJECT2;
    }
    return slotsToThingKind[numElements + ObjectElements::VALUES_PER_HEADER];
}

static inline AllocKind
GetGCObjectFixedSlotsKind(size_t numFixedSlots)
{
    MOZ_ASSERT(numFixedSlots < SLOTS_TO_THING_KIND_LIMIT);
    return slotsToThingKind[numFixedSlots];
}

// Get the best kind to use when allocating an object that needs a specific
// number of bytes.
static inline AllocKind
GetGCObjectKindForBytes(size_t nbytes)
{
    MOZ_ASSERT(nbytes <= JSObject::MAX_BYTE_SIZE);

    if (nbytes <= sizeof(NativeObject))
        return AllocKind::OBJECT0;
    nbytes -= sizeof(NativeObject);

    size_t dataSlots = AlignBytes(nbytes, sizeof(Value)) / sizeof(Value);
    MOZ_ASSERT(nbytes <= dataSlots * sizeof(Value));
    return GetGCObjectKind(dataSlots);
}

static inline AllocKind
GetBackgroundAllocKind(AllocKind kind)
{
    MOZ_ASSERT(!IsBackgroundFinalized(kind));
    MOZ_ASSERT(IsObjectAllocKind(kind));
    return AllocKind(size_t(kind) + 1);
}

/* Get the number of fixed slots and initial capacity associated with a kind. */
static inline size_t
GetGCKindSlots(AllocKind thingKind)
{
    /* Using a switch in hopes that thingKind will usually be a compile-time constant. */
    switch (thingKind) {
      case AllocKind::FUNCTION:
      case AllocKind::OBJECT0:
      case AllocKind::OBJECT0_BACKGROUND:
        return 0;
      case AllocKind::FUNCTION_EXTENDED:
      case AllocKind::OBJECT2:
      case AllocKind::OBJECT2_BACKGROUND:
        return 2;
      case AllocKind::OBJECT4:
      case AllocKind::OBJECT4_BACKGROUND:
        return 4;
      case AllocKind::OBJECT8:
      case AllocKind::OBJECT8_BACKGROUND:
        return 8;
      case AllocKind::OBJECT12:
      case AllocKind::OBJECT12_BACKGROUND:
        return 12;
      case AllocKind::OBJECT16:
      case AllocKind::OBJECT16_BACKGROUND:
        return 16;
      default:
        MOZ_CRASH("Bad object alloc kind");
    }
}

static inline size_t
GetGCKindSlots(AllocKind thingKind, const Class* clasp)
{
    size_t nslots = GetGCKindSlots(thingKind);

    /* An object's private data uses the space taken by its last fixed slot. */
    if (clasp->flags & JSCLASS_HAS_PRIVATE) {
        MOZ_ASSERT(nslots > 0);
        nslots--;
    }

    /*
     * Functions have a larger alloc kind than AllocKind::OBJECT to reserve
     * space for the extra fields in JSFunction, but have no fixed slots.
     */
    if (clasp == FunctionClassPtr)
        nslots = 0;

    return nslots;
}

static inline size_t
GetGCKindBytes(AllocKind thingKind)
{
    return sizeof(JSObject_Slots0) + GetGCKindSlots(thingKind) * sizeof(Value);
}

/* The number of GC cycles an empty chunk can survive before been released. */
const size_t MAX_EMPTY_CHUNK_AGE = 4;

extern bool
InitializeStaticData();

} /* namespace gc */

class InterpreterFrame;

extern void
TraceRuntime(JSTracer* trc);

extern void
ReleaseAllJITCode(FreeOp* op);

extern void
PrepareForDebugGC(JSRuntime* rt);

/* Functions for managing cross compartment gray pointers. */

extern void
DelayCrossCompartmentGrayMarking(JSObject* src);

extern void
NotifyGCNukeWrapper(JSObject* o);

extern unsigned
NotifyGCPreSwap(JSObject* a, JSObject* b);

extern void
NotifyGCPostSwap(JSObject* a, JSObject* b, unsigned preResult);

typedef void (*IterateChunkCallback)(JSRuntime* rt, void* data, gc::Chunk* chunk);
typedef void (*IterateZoneCallback)(JSRuntime* rt, void* data, JS::Zone* zone);
typedef void (*IterateArenaCallback)(JSRuntime* rt, void* data, gc::Arena* arena,
                                     JS::TraceKind traceKind, size_t thingSize);
typedef void (*IterateCellCallback)(JSRuntime* rt, void* data, void* thing,
                                    JS::TraceKind traceKind, size_t thingSize);

/*
 * This function calls |zoneCallback| on every zone, |compartmentCallback| on
 * every compartment, |arenaCallback| on every in-use arena, and |cellCallback|
 * on every in-use cell in the GC heap.
 *
 * Note that no read barrier is triggered on the cells passed to cellCallback,
 * so no these pointers must not escape the callback.
 */
extern void
IterateHeapUnbarriered(JSContext* cx, void* data,
                       IterateZoneCallback zoneCallback,
                       JSIterateCompartmentCallback compartmentCallback,
                       IterateArenaCallback arenaCallback,
                       IterateCellCallback cellCallback);

/*
 * This function is like IterateZonesCompartmentsArenasCells, but does it for a
 * single zone.
 */
extern void
IterateHeapUnbarrieredForZone(JSContext* cx, Zone* zone, void* data,
                              IterateZoneCallback zoneCallback,
                              JSIterateCompartmentCallback compartmentCallback,
                              IterateArenaCallback arenaCallback,
                              IterateCellCallback cellCallback);

/*
 * Invoke chunkCallback on every in-use chunk.
 */
extern void
IterateChunks(JSContext* cx, void* data, IterateChunkCallback chunkCallback);

typedef void (*IterateScriptCallback)(JSRuntime* rt, void* data, JSScript* script);

/*
 * Invoke scriptCallback on every in-use script for
 * the given compartment or for all compartments if it is null.
 */
extern void
IterateScripts(JSContext* cx, JSCompartment* compartment,
               void* data, IterateScriptCallback scriptCallback);

extern void
FinalizeStringRT(JSRuntime* rt, JSString* str);

JSCompartment*
NewCompartment(JSContext* cx, JSPrincipals* principals,
               const JS::CompartmentOptions& options);

namespace gc {

/*
 * Merge all contents of source into target. This can only be used if source is
 * the only compartment in its zone.
 */
void
MergeCompartments(JSCompartment* source, JSCompartment* target);

/*
 * This structure overlays a Cell in the Nursery and re-purposes its memory
 * for managing the Nursery collection process.
 */
class RelocationOverlay
{
    /* The low bit is set so this should never equal a normal pointer. */
    static const uintptr_t Relocated = uintptr_t(0xbad0bad1);

    /* Set to Relocated when moved. */
    uintptr_t magic_;

    /* The location |this| was moved to. */
    Cell* newLocation_;

    /* A list entry to track all relocated things. */
    RelocationOverlay* next_;

  public:
    static RelocationOverlay* fromCell(Cell* cell) {
        return reinterpret_cast<RelocationOverlay*>(cell);
    }

    bool isForwarded() const {
        return magic_ == Relocated;
    }

    Cell* forwardingAddress() const {
        MOZ_ASSERT(isForwarded());
        return newLocation_;
    }

    void forwardTo(Cell* cell);

    RelocationOverlay*& nextRef() {
        MOZ_ASSERT(isForwarded());
        return next_;
    }

    RelocationOverlay* next() const {
        MOZ_ASSERT(isForwarded());
        return next_;
    }

    static bool isCellForwarded(Cell* cell) {
        return fromCell(cell)->isForwarded();
    }
};

// Functions for checking and updating GC thing pointers that might have been
// moved by compacting GC. Overloads are also provided that work with Values.
//
// IsForwarded    - check whether a pointer refers to an GC thing that has been
//                  moved.
//
// Forwarded      - return a pointer to the new location of a GC thing given a
//                  pointer to old location.
//
// MaybeForwarded - used before dereferencing a pointer that may refer to a
//                  moved GC thing without updating it. For JSObjects this will
//                  also update the object's shape pointer if it has been moved
//                  to allow slots to be accessed.

template <typename T>
inline bool IsForwarded(T* t);
inline bool IsForwarded(const JS::Value& value);

template <typename T>
inline T* Forwarded(T* t);

inline Value Forwarded(const JS::Value& value);

template <typename T>
inline T MaybeForwarded(T t);

#ifdef JSGC_HASH_TABLE_CHECKS

template <typename T>
inline bool IsGCThingValidAfterMovingGC(T* t);

template <typename T>
inline void CheckGCThingAfterMovingGC(T* t);

template <typename T>
inline void CheckGCThingAfterMovingGC(const ReadBarriered<T*>& t);

inline void CheckValueAfterMovingGC(const JS::Value& value);

#endif // JSGC_HASH_TABLE_CHECKS

#define JS_FOR_EACH_ZEAL_MODE(D)               \
            D(RootsChange, 1)                  \
            D(Alloc, 2)                        \
            D(FrameGC, 3)                      \
            D(VerifierPre, 4)                  \
            D(FrameVerifierPre, 5)             \
            D(GenerationalGC, 7)               \
            D(IncrementalRootsThenFinish, 8)   \
            D(IncrementalMarkAllThenFinish, 9) \
            D(IncrementalMultipleSlices, 10)   \
            D(IncrementalMarkingValidator, 11) \
            D(ElementsBarrier, 12)             \
            D(CheckHashTablesOnMinorGC, 13)    \
            D(Compact, 14)                     \
            D(CheckHeapAfterGC, 15)            \
            D(CheckNursery, 16)                \
            D(IncrementalSweepThenFinish, 17)

enum class ZealMode {
#define ZEAL_MODE(name, value) name = value,
    JS_FOR_EACH_ZEAL_MODE(ZEAL_MODE)
#undef ZEAL_MODE
    Limit = 17
};

enum VerifierType {
    PreBarrierVerifier
};

#ifdef JS_GC_ZEAL

extern const char* ZealModeHelpText;

/* Check that write barriers have been used correctly. See jsgc.cpp. */
void
VerifyBarriers(JSRuntime* rt, VerifierType type);

void
MaybeVerifyBarriers(JSContext* cx, bool always = false);

void DumpArenaInfo();

#else

static inline void
VerifyBarriers(JSRuntime* rt, VerifierType type)
{
}

static inline void
MaybeVerifyBarriers(JSContext* cx, bool always = false)
{
}

#endif

/*
 * Instances of this class set the |JSRuntime::suppressGC| flag for the duration
 * that they are live. Use of this class is highly discouraged. Please carefully
 * read the comment in vm/Runtime.h above |suppressGC| and take all appropriate
 * precautions before instantiating this class.
 */
class MOZ_RAII JS_HAZ_GC_SUPPRESSED AutoSuppressGC
{
    int32_t& suppressGC_;

  public:
    explicit AutoSuppressGC(JSContext* cx);

    ~AutoSuppressGC()
    {
        suppressGC_--;
    }
};

JSObject*
NewMemoryStatisticsObject(JSContext* cx);

const char*
StateName(State state);

inline bool
IsOOMReason(JS::gcreason::Reason reason)
{
    return reason == JS::gcreason::LAST_DITCH ||
           reason == JS::gcreason::MEM_PRESSURE;
}

} /* namespace gc */

/* Use this to avoid assertions when manipulating the wrapper map. */
class MOZ_RAII AutoDisableProxyCheck
{
  public:
#ifdef DEBUG
    AutoDisableProxyCheck();
    ~AutoDisableProxyCheck();
#else
    AutoDisableProxyCheck() {}
#endif
};

struct MOZ_RAII AutoDisableCompactingGC
{
    explicit AutoDisableCompactingGC(JSContext* cx);
    ~AutoDisableCompactingGC();

  private:
    JSContext* cx;
};

// This is the same as IsInsideNursery, but not inlined.
bool
UninlinedIsInsideNursery(const gc::Cell* cell);

} /* namespace js */

#endif /* jsgc_h */

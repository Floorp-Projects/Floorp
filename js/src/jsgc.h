/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS engine garbage collector API.
 */

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

} /* namespace gc */

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
VerifyBarriers(JSRuntime* rt, VerifierType type) {}

static inline void
MaybeVerifyBarriers(JSContext* cx, bool always = false) {}

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

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_MemoryMetrics_h
#define js_MemoryMetrics_h

// These declarations are highly likely to change in the future. Depend on them
// at your own risk.

#include "mozilla/MemoryReporting.h"
#include "mozilla/NullPtr.h"
#include "mozilla/PodOperations.h"

#include <string.h>

#include "jsalloc.h"
#include "jspubtd.h"

#include "js/HashTable.h"
#include "js/Utility.h"
#include "js/Vector.h"

class nsISupports;      // Needed for ObjectPrivateVisitor.

namespace js {

// In memory reporting, we have concept of "sundries", line items which are too
// small to be worth reporting individually.  Under some circumstances, a memory
// reporter gets tossed into the sundries bucket if it's smaller than
// MemoryReportingSundriesThreshold() bytes.
//
// We need to define this value here, rather than in the code which actually
// generates the memory reports, because NotableStringInfo uses this value.
JS_FRIEND_API(size_t) MemoryReportingSundriesThreshold();

// This hash policy avoids flattening ropes (which perturbs the site being
// measured and requires a JSContext) at the expense of doing a FULL ROPE COPY
// on every hash and match! Beware.
struct InefficientNonFlatteningStringHashPolicy
{
    typedef JSString *Lookup;
    static HashNumber hash(const Lookup &l);
    static bool match(const JSString *const &k, const Lookup &l);
};

// This file features many classes with numerous size_t fields, and each such
// class has one or more methods that need to operate on all of these fields.
// Writing these individually is error-prone -- it's easy to add a new field
// without updating all the required methods.  So we define a single macro list
// in each class to name the fields (and notable characteristics of them), and
// then use the following macros to transform those lists into the required
// methods.
//
// In some classes, one or more of the macro arguments aren't used.  We use '_'
// for those.
//
#define DECL_SIZE(gc, mSize)                      size_t mSize;
#define ZERO_SIZE(gc, mSize)                      mSize(0),
#define COPY_OTHER_SIZE(gc, mSize)                mSize(other.mSize),
#define ADD_OTHER_SIZE(gc, mSize)                 mSize += other.mSize;
#define ADD_SIZE_TO_N_IF_LIVE_GC_THING(gc, mSize) n += (gc == js::IsLiveGCThing) ? mSize : 0;

// Used to annotate which size_t fields measure live GC things and which don't.
enum {
    IsLiveGCThing,
    NotLiveGCThing
};

struct ZoneStatsPod
{
#define FOR_EACH_SIZE(macro) \
    macro(NotLiveGCThing, gcHeapArenaAdmin) \
    macro(NotLiveGCThing, unusedGCThings) \
    macro(IsLiveGCThing,  lazyScriptsGCHeap) \
    macro(NotLiveGCThing, lazyScriptsMallocHeap) \
    macro(IsLiveGCThing,  ionCodesGCHeap) \
    macro(IsLiveGCThing,  typeObjectsGCHeap) \
    macro(NotLiveGCThing, typeObjectsMallocHeap) \
    macro(NotLiveGCThing, typePool) \
    macro(IsLiveGCThing,  stringsShortGCHeap) \
    macro(IsLiveGCThing,  stringsNormalGCHeap) \
    macro(NotLiveGCThing, stringsNormalMallocHeap)

    ZoneStatsPod()
      : FOR_EACH_SIZE(ZERO_SIZE)
        extra()
    {}

    void add(const ZoneStatsPod &other) {
        FOR_EACH_SIZE(ADD_OTHER_SIZE)
        // Do nothing with |extra|.
    }

    size_t sizeOfLiveGCThings() const {
        size_t n = 0;
        FOR_EACH_SIZE(ADD_SIZE_TO_N_IF_LIVE_GC_THING)
        // Do nothing with |extra|.
        return n;
    }

    FOR_EACH_SIZE(DECL_SIZE)
    void *extra;    // This field can be used by embedders.

#undef FOR_EACH_SIZE
};

} // namespace js

namespace JS {

// Data for tracking memory usage of things hanging off objects.
struct ObjectsExtraSizes
{
#define FOR_EACH_SIZE(macro) \
    macro(js::NotLiveGCThing, mallocHeapSlots) \
    macro(js::NotLiveGCThing, mallocHeapElementsNonAsmJS) \
    macro(js::NotLiveGCThing, mallocHeapElementsAsmJS) \
    macro(js::NotLiveGCThing, nonHeapElementsAsmJS) \
    macro(js::NotLiveGCThing, nonHeapCodeAsmJS) \
    macro(js::NotLiveGCThing, mallocHeapAsmJSModuleData) \
    macro(js::NotLiveGCThing, mallocHeapArgumentsData) \
    macro(js::NotLiveGCThing, mallocHeapRegExpStatics) \
    macro(js::NotLiveGCThing, mallocHeapPropertyIteratorData) \
    macro(js::NotLiveGCThing, mallocHeapCtypesData)

    ObjectsExtraSizes()
      : FOR_EACH_SIZE(ZERO_SIZE)
        dummy()
    {}

    void add(const ObjectsExtraSizes &other) {
        FOR_EACH_SIZE(ADD_OTHER_SIZE)
    }

    size_t sizeOfLiveGCThings() const {
        size_t n = 0;
        FOR_EACH_SIZE(ADD_SIZE_TO_N_IF_LIVE_GC_THING)
        return n;
    }

    FOR_EACH_SIZE(DECL_SIZE)
    int dummy;  // present just to absorb the trailing comma from FOR_EACH_SIZE(ZERO_SIZE)

#undef FOR_EACH_SIZE
};

// Data for tracking analysis/inference memory usage.
struct TypeInferenceSizes
{
#define FOR_EACH_SIZE(macro) \
    macro(js::NotLiveGCThing, typeScripts) \
    macro(js::NotLiveGCThing, pendingArrays) \
    macro(js::NotLiveGCThing, allocationSiteTables) \
    macro(js::NotLiveGCThing, arrayTypeTables) \
    macro(js::NotLiveGCThing, objectTypeTables)

    TypeInferenceSizes()
      : FOR_EACH_SIZE(ZERO_SIZE)
        dummy()
    {}

    void add(const TypeInferenceSizes &other) {
        FOR_EACH_SIZE(ADD_OTHER_SIZE)
    }

    size_t sizeOfLiveGCThings() const {
        size_t n = 0;
        FOR_EACH_SIZE(ADD_SIZE_TO_N_IF_LIVE_GC_THING)
        return n;
    }

    FOR_EACH_SIZE(DECL_SIZE)
    int dummy;  // present just to absorb the trailing comma from FOR_EACH_SIZE(ZERO_SIZE)

#undef FOR_EACH_SIZE
};

// Data for tracking JIT-code memory usage.
struct CodeSizes
{
#define FOR_EACH_SIZE(macro) \
    macro(_, ion) \
    macro(_, baseline) \
    macro(_, regexp) \
    macro(_, other) \
    macro(_, unused)

    CodeSizes()
      : FOR_EACH_SIZE(ZERO_SIZE)
        dummy()
    {}

    FOR_EACH_SIZE(DECL_SIZE)
    int dummy;  // present just to absorb the trailing comma from FOR_EACH_SIZE(ZERO_SIZE)

#undef FOR_EACH_SIZE
};

// This class holds information about the memory taken up by identical copies of
// a particular string.  Multiple JSStrings may have their sizes aggregated
// together into one StringInfo object.
struct StringInfo
{
    StringInfo()
      : length(0), numCopies(0), shortGCHeap(0), normalGCHeap(0), normalMallocHeap(0)
    {}

    StringInfo(size_t len, size_t shorts, size_t normals, size_t chars)
      : length(len),
        numCopies(1),
        shortGCHeap(shorts),
        normalGCHeap(normals),
        normalMallocHeap(chars)
    {}

    void add(size_t shorts, size_t normals, size_t chars) {
        shortGCHeap += shorts;
        normalGCHeap += normals;
        normalMallocHeap += chars;
        numCopies++;
    }

    void add(const StringInfo& info) {
        MOZ_ASSERT(length == info.length);

        shortGCHeap += info.shortGCHeap;
        normalGCHeap += info.normalGCHeap;
        normalMallocHeap += info.normalMallocHeap;
        numCopies += info.numCopies;
    }

    size_t totalSizeOf() const {
        return shortGCHeap + normalGCHeap + normalMallocHeap;
    }

    size_t totalGCHeapSizeOf() const {
        return shortGCHeap + normalGCHeap;
    }

    // The string's length, excluding the null-terminator.
    size_t length;

    // How many copies of the string have we seen?
    size_t numCopies;

    // These are all totals across all copies of the string we've seen.
    size_t shortGCHeap;
    size_t normalGCHeap;
    size_t normalMallocHeap;
};

// Holds data about a notable string (one which uses more than
// NotableStringInfo::notableSize() bytes of memory), so we can report it
// individually.
//
// Essentially the only difference between this class and StringInfo is that
// NotableStringInfo holds a copy of the string's chars.
struct NotableStringInfo : public StringInfo
{
    NotableStringInfo();
    NotableStringInfo(JSString *str, const StringInfo &info);
    NotableStringInfo(const NotableStringInfo& info);
    NotableStringInfo(mozilla::MoveRef<NotableStringInfo> info);
    NotableStringInfo &operator=(mozilla::MoveRef<NotableStringInfo> info);

    ~NotableStringInfo() {
        js_free(buffer);
    }

    // A string needs to take up this many bytes of storage before we consider
    // it to be "notable".
    static size_t notableSize() {
        return js::MemoryReportingSundriesThreshold();
    }

    // The amount of memory we requested for |buffer|; i.e.
    // buffer = malloc(bufferSize).
    size_t bufferSize;
    char *buffer;
};

// These measurements relate directly to the JSRuntime, and not to zones and
// compartments within it.
struct RuntimeSizes
{
#define FOR_EACH_SIZE(macro) \
    macro(_, object) \
    macro(_, atomsTable) \
    macro(_, contexts) \
    macro(_, dtoa) \
    macro(_, temporary) \
    macro(_, regexpData) \
    macro(_, interpreterStack) \
    macro(_, gcMarker) \
    macro(_, mathCache) \
    macro(_, scriptData) \
    macro(_, scriptSources)

    RuntimeSizes()
      : FOR_EACH_SIZE(ZERO_SIZE)
        code()
    {}

    FOR_EACH_SIZE(DECL_SIZE)
    CodeSizes code;

#undef FOR_EACH_SIZE
};

struct ZoneStats : js::ZoneStatsPod
{
    ZoneStats() {
        strings.init();
    }

    ZoneStats(mozilla::MoveRef<ZoneStats> other)
        : ZoneStatsPod(other),
          strings(mozilla::OldMove(other->strings)),
          notableStrings(mozilla::OldMove(other->notableStrings))
    {}

    // Add other's numbers to this object's numbers.  Both objects'
    // notableStrings vectors must be empty at this point, because we can't
    // merge them.  (A NotableStringInfo contains only a prefix of the string,
    // so we can't tell whether two NotableStringInfo objects correspond to the
    // same string.)
    void add(const ZoneStats &other) {
        ZoneStatsPod::add(other);

        MOZ_ASSERT(notableStrings.empty());
        MOZ_ASSERT(other.notableStrings.empty());

        for (StringsHashMap::Range r = other.strings.all(); !r.empty(); r.popFront()) {
            StringsHashMap::AddPtr p = strings.lookupForAdd(r.front().key);
            if (p) {
                // We've seen this string before; add its size to our tally.
                p->value.add(r.front().value);
            } else {
                // We haven't seen this string before; add it to the hashtable.
                strings.add(p, r.front().key, r.front().value);
            }
        }
    }

    size_t sizeOfLiveGCThings() const {
        size_t n = ZoneStatsPod::sizeOfLiveGCThings();
        for (size_t i = 0; i < notableStrings.length(); i++) {
            const JS::NotableStringInfo& info = notableStrings[i];
            n += info.totalGCHeapSizeOf();
        }
        return n;
    }

    typedef js::HashMap<JSString*,
                        StringInfo,
                        js::InefficientNonFlatteningStringHashPolicy,
                        js::SystemAllocPolicy> StringsHashMap;

    StringsHashMap strings;
    js::Vector<NotableStringInfo, 0, js::SystemAllocPolicy> notableStrings;
};

struct CompartmentStats
{
#define FOR_EACH_SIZE(macro) \
    macro(js::IsLiveGCThing,  objectsGCHeapOrdinary) \
    macro(js::IsLiveGCThing,  objectsGCHeapFunction) \
    macro(js::IsLiveGCThing,  objectsGCHeapDenseArray) \
    macro(js::IsLiveGCThing,  objectsGCHeapSlowArray) \
    macro(js::IsLiveGCThing,  objectsGCHeapCrossCompartmentWrapper) \
    macro(js::NotLiveGCThing, objectsPrivate) \
    macro(js::IsLiveGCThing,  shapesGCHeapTreeGlobalParented) \
    macro(js::IsLiveGCThing,  shapesGCHeapTreeNonGlobalParented) \
    macro(js::IsLiveGCThing,  shapesGCHeapDict) \
    macro(js::IsLiveGCThing,  shapesGCHeapBase) \
    macro(js::NotLiveGCThing, shapesMallocHeapTreeTables) \
    macro(js::NotLiveGCThing, shapesMallocHeapDictTables) \
    macro(js::NotLiveGCThing, shapesMallocHeapTreeShapeKids) \
    macro(js::NotLiveGCThing, shapesMallocHeapCompartmentTables) \
    macro(js::IsLiveGCThing,  scriptsGCHeap) \
    macro(js::NotLiveGCThing, scriptsMallocHeapData) \
    macro(js::NotLiveGCThing, baselineData) \
    macro(js::NotLiveGCThing, baselineStubsFallback) \
    macro(js::NotLiveGCThing, baselineStubsOptimized) \
    macro(js::NotLiveGCThing, ionData) \
    macro(js::NotLiveGCThing, compartmentObject) \
    macro(js::NotLiveGCThing, crossCompartmentWrappersTable) \
    macro(js::NotLiveGCThing, regexpCompartment) \
    macro(js::NotLiveGCThing, debuggeesSet) \

    CompartmentStats()
      : FOR_EACH_SIZE(ZERO_SIZE)
        objectsExtra(),
        typeInference(),
        extra()
    {}

    CompartmentStats(const CompartmentStats &other)
      : FOR_EACH_SIZE(COPY_OTHER_SIZE)
        objectsExtra(other.objectsExtra),
        typeInference(other.typeInference),
        extra(other.extra)
    {}

    void add(const CompartmentStats &other) {
        FOR_EACH_SIZE(ADD_OTHER_SIZE)
        objectsExtra.add(other.objectsExtra);
        typeInference.add(other.typeInference);
        // Do nothing with |extra|.
    }

    size_t sizeOfLiveGCThings() const {
        size_t n = 0;
        FOR_EACH_SIZE(ADD_SIZE_TO_N_IF_LIVE_GC_THING)
        n += objectsExtra.sizeOfLiveGCThings();
        n += typeInference.sizeOfLiveGCThings();
        // Do nothing with |extra|.
        return n;
    }

    FOR_EACH_SIZE(DECL_SIZE)
    ObjectsExtraSizes  objectsExtra;
    TypeInferenceSizes typeInference;
    void               *extra;  // This field can be used by embedders.

#undef FOR_EACH_SIZE
};

struct RuntimeStats
{
#define FOR_EACH_SIZE(macro) \
    macro(_, gcHeapChunkTotal) \
    macro(_, gcHeapDecommittedArenas) \
    macro(_, gcHeapUnusedChunks) \
    macro(_, gcHeapUnusedArenas) \
    macro(_, gcHeapChunkAdmin) \
    macro(_, gcHeapGCThings) \

    RuntimeStats(mozilla::MallocSizeOf mallocSizeOf)
      : FOR_EACH_SIZE(ZERO_SIZE)
        runtime(),
        cTotals(),
        zTotals(),
        compartmentStatsVector(),
        zoneStatsVector(),
        currZoneStats(nullptr),
        mallocSizeOf_(mallocSizeOf)
    {}

    // Here's a useful breakdown of the GC heap.
    //
    // - rtStats.gcHeapChunkTotal
    //   - decommitted bytes
    //     - rtStats.gcHeapDecommittedArenas (decommitted arenas in non-empty chunks)
    //   - unused bytes
    //     - rtStats.gcHeapUnusedChunks (empty chunks)
    //     - rtStats.gcHeapUnusedArenas (empty arenas within non-empty chunks)
    //     - rtStats.zTotals.unusedGCThings (empty GC thing slots within non-empty arenas)
    //   - used bytes
    //     - rtStats.gcHeapChunkAdmin
    //     - rtStats.zTotals.gcHeapArenaAdmin
    //     - rtStats.gcHeapGCThings (in-use GC things)
    //       == rtStats.zTotals.sizeOfLiveGCThings() + rtStats.cTotals.sizeOfLiveGCThings()
    //
    // It's possible that some arenas in empty chunks may be decommitted, but
    // we don't count those under rtStats.gcHeapDecommittedArenas because (a)
    // it's rare, and (b) this means that rtStats.gcHeapUnusedChunks is a
    // multiple of the chunk size, which is good.

    FOR_EACH_SIZE(DECL_SIZE)

    RuntimeSizes runtime;

    CompartmentStats cTotals;   // The sum of this runtime's compartments' measurements.
    ZoneStats zTotals;          // The sum of this runtime's zones' measurements.

    js::Vector<CompartmentStats, 0, js::SystemAllocPolicy> compartmentStatsVector;
    js::Vector<ZoneStats, 0, js::SystemAllocPolicy> zoneStatsVector;

    ZoneStats *currZoneStats;

    mozilla::MallocSizeOf mallocSizeOf_;

    virtual void initExtraCompartmentStats(JSCompartment *c, CompartmentStats *cstats) = 0;
    virtual void initExtraZoneStats(JS::Zone *zone, ZoneStats *zstats) = 0;

#undef FOR_EACH_SIZE
};

class ObjectPrivateVisitor
{
  public:
    // Within CollectRuntimeStats, this method is called for each JS object
    // that has an nsISupports pointer.
    virtual size_t sizeOfIncludingThis(nsISupports *aSupports) = 0;

    // A callback that gets a JSObject's nsISupports pointer, if it has one.
    // Note: this function does *not* addref |iface|.
    typedef bool(*GetISupportsFun)(JSObject *obj, nsISupports **iface);
    GetISupportsFun getISupports_;

    ObjectPrivateVisitor(GetISupportsFun getISupports)
      : getISupports_(getISupports)
    {}
};

extern JS_PUBLIC_API(bool)
CollectRuntimeStats(JSRuntime *rt, RuntimeStats *rtStats, ObjectPrivateVisitor *opv);

extern JS_PUBLIC_API(size_t)
SystemCompartmentCount(JSRuntime *rt);

extern JS_PUBLIC_API(size_t)
UserCompartmentCount(JSRuntime *rt);

extern JS_PUBLIC_API(size_t)
PeakSizeOfTemporary(const JSRuntime *rt);

} // namespace JS

#endif /* js_MemoryMetrics_h */

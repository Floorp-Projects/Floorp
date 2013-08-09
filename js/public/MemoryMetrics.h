/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_MemoryMetrics_h
#define js_MemoryMetrics_h

// These declarations are not within jsapi.h because they are highly likely to
// change in the future. Depend on them at your own risk.

#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"

#include <string.h>

#include "jsalloc.h"
#include "jspubtd.h"

#include "js/HashTable.h"
#include "js/Utility.h"
#include "js/Vector.h"

class nsISupports;      // This is needed for ObjectPrivateVisitor.

namespace js {

// In memory reporting, we have concept of "sundries", line items which are too
// small to be worth reporting individually.  Under some circumstances, a memory
// reporter gets tossed into the sundries bucket if it's smaller than
// MemoryReportingSundriesThreshold() bytes.
//
// We need to define this value here, rather than in the code which actually
// generates the memory reports, because NotableStringInfo uses this value.
JS_FRIEND_API(size_t) MemoryReportingSundriesThreshold();

struct StringHashPolicy
{
    typedef JSString *Lookup;
    static HashNumber hash(const Lookup &l);
    static bool match(const JSString *const &k, const Lookup &l);
};

struct ZoneStatsPod
{
    ZoneStatsPod() {
        mozilla::PodZero(this);
    }

    void add(const ZoneStatsPod &other) {
        #define ADD(x)  this->x += other.x

        ADD(gcHeapArenaAdmin);
        ADD(gcHeapUnusedGcThings);

        ADD(gcHeapStringsNormal);
        ADD(gcHeapStringsShort);
        ADD(gcHeapLazyScripts);
        ADD(gcHeapTypeObjects);
        ADD(gcHeapIonCodes);

        ADD(stringCharsNonNotable);
        ADD(lazyScripts);
        ADD(typeObjects);
        ADD(typePool);

        #undef ADD
    }

    // This field can be used by embedders.
    void   *extra;

    size_t gcHeapArenaAdmin;
    size_t gcHeapUnusedGcThings;

    size_t gcHeapStringsNormal;
    size_t gcHeapStringsShort;

    size_t gcHeapLazyScripts;
    size_t gcHeapTypeObjects;
    size_t gcHeapIonCodes;

    size_t stringCharsNonNotable;
    size_t lazyScripts;
    size_t typeObjects;
    size_t typePool;
};

} // namespace js

namespace JS {

// Data for tracking memory usage of things hanging off objects.
struct ObjectsExtraSizes
{
    size_t slots;
    size_t elementsNonAsmJS;
    size_t elementsAsmJSHeap;
    size_t elementsAsmJSNonHeap;
    size_t asmJSModuleCode;
    size_t asmJSModuleData;
    size_t argumentsData;
    size_t regExpStatics;
    size_t propertyIteratorData;
    size_t ctypesData;
    size_t private_;    // The '_' suffix is required because |private| is a keyword.
                        // Note that this field is measured separately from the others.

    ObjectsExtraSizes() { memset(this, 0, sizeof(ObjectsExtraSizes)); }

    void add(ObjectsExtraSizes &sizes) {
        this->slots                += sizes.slots;
        this->elementsNonAsmJS     += sizes.elementsNonAsmJS;
        this->elementsAsmJSHeap    += sizes.elementsAsmJSHeap;
        this->elementsAsmJSNonHeap += sizes.elementsAsmJSNonHeap;
        this->asmJSModuleCode      += sizes.asmJSModuleCode;
        this->asmJSModuleData      += sizes.asmJSModuleData;
        this->argumentsData        += sizes.argumentsData;
        this->regExpStatics        += sizes.regExpStatics;
        this->propertyIteratorData += sizes.propertyIteratorData;
        this->ctypesData           += sizes.ctypesData;
        this->private_             += sizes.private_;
    }
};

// Data for tracking analysis/inference memory usage.
struct TypeInferenceSizes
{
    size_t typeScripts;
    size_t typeResults;
    size_t analysisPool;
    size_t pendingArrays;
    size_t allocationSiteTables;
    size_t arrayTypeTables;
    size_t objectTypeTables;

    TypeInferenceSizes() { memset(this, 0, sizeof(TypeInferenceSizes)); }

    void add(TypeInferenceSizes &sizes) {
        this->typeScripts          += sizes.typeScripts;
        this->typeResults          += sizes.typeResults;
        this->analysisPool         += sizes.analysisPool;
        this->pendingArrays        += sizes.pendingArrays;
        this->allocationSiteTables += sizes.allocationSiteTables;
        this->arrayTypeTables      += sizes.arrayTypeTables;
        this->objectTypeTables     += sizes.objectTypeTables;
    }
};

// Data for tracking JIT-code memory usage.
struct CodeSizes
{
    size_t ion;
    size_t baseline;
    size_t regexp;
    size_t other;
    size_t unused;

    CodeSizes() { memset(this, 0, sizeof(CodeSizes)); }
};


// This class holds information about the memory taken up by identical copies of
// a particular string.  Multiple JSStrings may have their sizes aggregated
// together into one StringInfo object.
struct StringInfo
{
    StringInfo()
      : length(0), numCopies(0), sizeOfShortStringGCThings(0),
        sizeOfNormalStringGCThings(0), sizeOfAllStringChars(0)
    {}

    StringInfo(size_t len, size_t shorts, size_t normals, size_t chars)
      : length(len),
        numCopies(1),
        sizeOfShortStringGCThings(shorts),
        sizeOfNormalStringGCThings(normals),
        sizeOfAllStringChars(chars)
    {}

    void add(size_t shorts, size_t normals, size_t chars) {
        sizeOfShortStringGCThings += shorts;
        sizeOfNormalStringGCThings += normals;
        sizeOfAllStringChars += chars;
        numCopies++;
    }

    void add(const StringInfo& info) {
        MOZ_ASSERT(length == info.length);

        sizeOfShortStringGCThings += info.sizeOfShortStringGCThings;
        sizeOfNormalStringGCThings += info.sizeOfNormalStringGCThings;
        sizeOfAllStringChars += info.sizeOfAllStringChars;
        numCopies += info.numCopies;
    }

    size_t totalSizeOf() const {
        return sizeOfShortStringGCThings + sizeOfNormalStringGCThings + sizeOfAllStringChars;
    }

    size_t totalGCThingSizeOf() const {
        return sizeOfShortStringGCThings + sizeOfNormalStringGCThings;
    }

    // The string's length, excluding the null-terminator.
    size_t length;

    // How many copies of the string have we seen?
    size_t numCopies;

    // These are all totals across all copies of the string we've seen.
    size_t sizeOfShortStringGCThings;
    size_t sizeOfNormalStringGCThings;
    size_t sizeOfAllStringChars;
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

// These measurements relate directly to the JSRuntime, and not to
// compartments within it.
struct RuntimeSizes
{
    RuntimeSizes() { memset(this, 0, sizeof(RuntimeSizes)); }

    size_t object;
    size_t atomsTable;
    size_t contexts;
    size_t dtoa;
    size_t temporary;
    size_t regexpData;
    size_t interpreterStack;
    size_t gcMarker;
    size_t mathCache;
    size_t scriptData;
    size_t scriptSources;

    CodeSizes code;
};

struct ZoneStats : js::ZoneStatsPod
{
    ZoneStats() {
        strings.init();
    }

    ZoneStats(mozilla::MoveRef<ZoneStats> other)
        : ZoneStatsPod(other),
          strings(mozilla::Move(other->strings)),
          notableStrings(mozilla::Move(other->notableStrings))
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

    typedef js::HashMap<JSString*, StringInfo, js::StringHashPolicy, js::SystemAllocPolicy>
        StringsHashMap;

    StringsHashMap strings;
    js::Vector<NotableStringInfo, 0, js::SystemAllocPolicy> notableStrings;

    // The size of all the live things in the GC heap that don't belong to any
    // compartment.
    size_t GCHeapThingsSize();
};

struct CompartmentStats
{
    CompartmentStats()
      : extra(NULL),
        gcHeapObjectsOrdinary(0),
        gcHeapObjectsFunction(0),
        gcHeapObjectsDenseArray(0),
        gcHeapObjectsSlowArray(0),
        gcHeapObjectsCrossCompartmentWrapper(0),
        gcHeapShapesTreeGlobalParented(0),
        gcHeapShapesTreeNonGlobalParented(0),
        gcHeapShapesDict(0),
        gcHeapShapesBase(0),
        gcHeapScripts(0),
        objectsExtra(),
        shapesExtraTreeTables(0),
        shapesExtraDictTables(0),
        shapesExtraTreeShapeKids(0),
        shapesCompartmentTables(0),
        scriptData(0),
        baselineData(0),
        baselineStubsFallback(0),
        baselineStubsOptimized(0),
        ionData(0),
        compartmentObject(0),
        crossCompartmentWrappersTable(0),
        regexpCompartment(0),
        debuggeesSet(0),
        typeInference()
    {}

    CompartmentStats(const CompartmentStats &other)
      : extra(other.extra),
        gcHeapObjectsOrdinary(other.gcHeapObjectsOrdinary),
        gcHeapObjectsFunction(other.gcHeapObjectsFunction),
        gcHeapObjectsDenseArray(other.gcHeapObjectsDenseArray),
        gcHeapObjectsSlowArray(other.gcHeapObjectsSlowArray),
        gcHeapObjectsCrossCompartmentWrapper(other.gcHeapObjectsCrossCompartmentWrapper),
        gcHeapShapesTreeGlobalParented(other.gcHeapShapesTreeGlobalParented),
        gcHeapShapesTreeNonGlobalParented(other.gcHeapShapesTreeNonGlobalParented),
        gcHeapShapesDict(other.gcHeapShapesDict),
        gcHeapShapesBase(other.gcHeapShapesBase),
        gcHeapScripts(other.gcHeapScripts),
        objectsExtra(other.objectsExtra),
        shapesExtraTreeTables(other.shapesExtraTreeTables),
        shapesExtraDictTables(other.shapesExtraDictTables),
        shapesExtraTreeShapeKids(other.shapesExtraTreeShapeKids),
        shapesCompartmentTables(other.shapesCompartmentTables),
        scriptData(other.scriptData),
        baselineData(other.baselineData),
        baselineStubsFallback(other.baselineStubsFallback),
        baselineStubsOptimized(other.baselineStubsOptimized),
        ionData(other.ionData),
        compartmentObject(other.compartmentObject),
        crossCompartmentWrappersTable(other.crossCompartmentWrappersTable),
        regexpCompartment(other.regexpCompartment),
        debuggeesSet(other.debuggeesSet),
        typeInference(other.typeInference)
    {
    }

    // This field can be used by embedders.
    void   *extra;

    // If you add a new number, remember to update the constructors, add(), and
    // maybe gcHeapThingsSize()!
    size_t gcHeapObjectsOrdinary;
    size_t gcHeapObjectsFunction;
    size_t gcHeapObjectsDenseArray;
    size_t gcHeapObjectsSlowArray;
    size_t gcHeapObjectsCrossCompartmentWrapper;
    size_t gcHeapShapesTreeGlobalParented;
    size_t gcHeapShapesTreeNonGlobalParented;
    size_t gcHeapShapesDict;
    size_t gcHeapShapesBase;
    size_t gcHeapScripts;
    ObjectsExtraSizes objectsExtra;

    size_t shapesExtraTreeTables;
    size_t shapesExtraDictTables;
    size_t shapesExtraTreeShapeKids;
    size_t shapesCompartmentTables;
    size_t scriptData;
    size_t baselineData;
    size_t baselineStubsFallback;
    size_t baselineStubsOptimized;
    size_t ionData;
    size_t compartmentObject;
    size_t crossCompartmentWrappersTable;
    size_t regexpCompartment;
    size_t debuggeesSet;

    TypeInferenceSizes typeInference;

    // Add cStats's numbers to this object's numbers.
    void add(CompartmentStats &cStats) {
        #define ADD(x)  this->x += cStats.x

        ADD(gcHeapObjectsOrdinary);
        ADD(gcHeapObjectsFunction);
        ADD(gcHeapObjectsDenseArray);
        ADD(gcHeapObjectsSlowArray);
        ADD(gcHeapObjectsCrossCompartmentWrapper);
        ADD(gcHeapShapesTreeGlobalParented);
        ADD(gcHeapShapesTreeNonGlobalParented);
        ADD(gcHeapShapesDict);
        ADD(gcHeapShapesBase);
        ADD(gcHeapScripts);
        objectsExtra.add(cStats.objectsExtra);

        ADD(shapesExtraTreeTables);
        ADD(shapesExtraDictTables);
        ADD(shapesExtraTreeShapeKids);
        ADD(shapesCompartmentTables);
        ADD(scriptData);
        ADD(baselineData);
        ADD(baselineStubsFallback);
        ADD(baselineStubsOptimized);
        ADD(ionData);
        ADD(compartmentObject);
        ADD(crossCompartmentWrappersTable);
        ADD(regexpCompartment);
        ADD(debuggeesSet);

        #undef ADD

        typeInference.add(cStats.typeInference);
    }

    // The size of all the live things in the GC heap.
    size_t GCHeapThingsSize();
};

struct RuntimeStats
{
    RuntimeStats(mozilla::MallocSizeOf mallocSizeOf)
      : runtime(),
        gcHeapChunkTotal(0),
        gcHeapDecommittedArenas(0),
        gcHeapUnusedChunks(0),
        gcHeapUnusedArenas(0),
        gcHeapUnusedGcThings(0),
        gcHeapChunkAdmin(0),
        gcHeapGcThings(0),
        cTotals(),
        zTotals(),
        compartmentStatsVector(),
        zoneStatsVector(),
        currZoneStats(NULL),
        mallocSizeOf_(mallocSizeOf)
    {}

    RuntimeSizes runtime;

    // If you add a new number, remember to update the constructor!

    // Here's a useful breakdown of the GC heap.
    //
    // - rtStats.gcHeapChunkTotal
    //   - decommitted bytes
    //     - rtStats.gcHeapDecommittedArenas (decommitted arenas in non-empty chunks)
    //   - unused bytes
    //     - rtStats.gcHeapUnusedChunks (empty chunks)
    //     - rtStats.gcHeapUnusedArenas (empty arenas within non-empty chunks)
    //     - rtStats.total.gcHeapUnusedGcThings (empty GC thing slots within non-empty arenas)
    //   - used bytes
    //     - rtStats.gcHeapChunkAdmin
    //     - rtStats.total.gcHeapArenaAdmin
    //     - rtStats.gcHeapGcThings (in-use GC things)
    //
    // It's possible that some arenas in empty chunks may be decommitted, but
    // we don't count those under rtStats.gcHeapDecommittedArenas because (a)
    // it's rare, and (b) this means that rtStats.gcHeapUnusedChunks is a
    // multiple of the chunk size, which is good.

    size_t gcHeapChunkTotal;
    size_t gcHeapDecommittedArenas;
    size_t gcHeapUnusedChunks;
    size_t gcHeapUnusedArenas;
    size_t gcHeapUnusedGcThings;
    size_t gcHeapChunkAdmin;
    size_t gcHeapGcThings;

    // The sum of all compartment's measurements.
    CompartmentStats cTotals;
    ZoneStats zTotals;

    js::Vector<CompartmentStats, 0, js::SystemAllocPolicy> compartmentStatsVector;
    js::Vector<ZoneStats, 0, js::SystemAllocPolicy> zoneStatsVector;

    ZoneStats *currZoneStats;

    mozilla::MallocSizeOf mallocSizeOf_;

    virtual void initExtraCompartmentStats(JSCompartment *c, CompartmentStats *cstats) = 0;
    virtual void initExtraZoneStats(JS::Zone *zone, ZoneStats *zstats) = 0;
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

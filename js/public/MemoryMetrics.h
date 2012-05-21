/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_MemoryMetrics_h
#define js_MemoryMetrics_h

/*
 * These declarations are not within jsapi.h because they are highly likely
 * to change in the future. Depend on them at your own risk.
 */

#include <string.h>

#include "jsalloc.h"
#include "jspubtd.h"

#include "js/Utility.h"
#include "js/Vector.h"

namespace JS {

/* Data for tracking analysis/inference memory usage. */
struct TypeInferenceSizes
{
    size_t scripts;
    size_t objects;
    size_t tables;
    size_t temporary;
};

// These measurements relate directly to the JSRuntime, and not to
// compartments within it.
struct RuntimeSizes
{
    RuntimeSizes()
      : object(0)
      , atomsTable(0)
      , contexts(0)
      , dtoa(0)
      , temporary(0)
      , mjitCode(0)
      , regexpCode(0)
      , unusedCodeMemory(0)
      , stackCommitted(0)
      , gcMarker(0)
      , mathCache(0)
      , scriptFilenames(0)
      , compartmentObjects(0)
    {}

    size_t object;
    size_t atomsTable;
    size_t contexts;
    size_t dtoa;
    size_t temporary;
    size_t mjitCode;
    size_t regexpCode;
    size_t unusedCodeMemory;
    size_t stackCommitted;
    size_t gcMarker;
    size_t mathCache;
    size_t scriptFilenames;

    // This is the exception to the "RuntimeSizes doesn't measure things within
    // compartments" rule.  We combine the sizes of all the JSCompartment
    // objects into a single measurement because each one is fairly small, and
    // they're all the same size.
    size_t compartmentObjects;
};

struct CompartmentStats
{
    CompartmentStats() {
        memset(this, 0, sizeof(*this));
    }

    void   *extra;
    size_t gcHeapArenaHeaders;
    size_t gcHeapArenaPadding;
    size_t gcHeapArenaUnused;

    size_t gcHeapObjectsNonFunction;
    size_t gcHeapObjectsFunction;
    size_t gcHeapStrings;
    size_t gcHeapShapesTree;
    size_t gcHeapShapesDict;
    size_t gcHeapShapesBase;
    size_t gcHeapScripts;
    size_t gcHeapTypeObjects;
#if JS_HAS_XML_SUPPORT
    size_t gcHeapXML;
#endif

    size_t objectSlots;
    size_t objectElements;
    size_t objectMisc;
    size_t stringChars;
    size_t shapesExtraTreeTables;
    size_t shapesExtraDictTables;
    size_t shapesExtraTreeShapeKids;
    size_t shapesCompartmentTables;
    size_t scriptData;
    size_t mjitData;
    size_t crossCompartmentWrappers;

    TypeInferenceSizes typeInferenceSizes;
};

struct RuntimeStats
{
    RuntimeStats(JSMallocSizeOfFun mallocSizeOf)
      : runtime()
      , gcHeapChunkTotal(0)
      , gcHeapCommitted(0)
      , gcHeapUnused(0)
      , gcHeapChunkCleanUnused(0)
      , gcHeapChunkDirtyUnused(0)
      , gcHeapChunkCleanDecommitted(0)
      , gcHeapChunkDirtyDecommitted(0)
      , gcHeapArenaUnused(0)
      , gcHeapChunkAdmin(0)
      , totalObjects(0)
      , totalShapes(0)
      , totalScripts(0)
      , totalStrings(0)
      , totalMjit(0)
      , totalTypeInference(0)
      , totalAnalysisTemp(0)
      , compartmentStatsVector()
      , currCompartmentStats(NULL)
      , mallocSizeOf(mallocSizeOf)
    {}

    RuntimeSizes runtime;

    size_t gcHeapChunkTotal;
    size_t gcHeapCommitted;
    size_t gcHeapUnused;
    size_t gcHeapChunkCleanUnused;
    size_t gcHeapChunkDirtyUnused;
    size_t gcHeapChunkCleanDecommitted;
    size_t gcHeapChunkDirtyDecommitted;
    size_t gcHeapArenaUnused;
    size_t gcHeapChunkAdmin;
    size_t totalObjects;
    size_t totalShapes;
    size_t totalScripts;
    size_t totalStrings;
    size_t totalMjit;
    size_t totalTypeInference;
    size_t totalAnalysisTemp;

    js::Vector<CompartmentStats, 0, js::SystemAllocPolicy> compartmentStatsVector;
    CompartmentStats *currCompartmentStats;

    JSMallocSizeOfFun mallocSizeOf;

    virtual void initExtraCompartmentStats(JSCompartment *c, CompartmentStats *cstats) = 0;
};

#ifdef JS_THREADSAFE

extern JS_PUBLIC_API(bool)
CollectRuntimeStats(JSRuntime *rt, RuntimeStats *rtStats);

extern JS_PUBLIC_API(int64_t)
GetExplicitNonHeapForRuntime(JSRuntime *rt, JSMallocSizeOfFun mallocSizeOf);

#endif /* JS_THREADSAFE */

extern JS_PUBLIC_API(size_t)
SystemCompartmentCount(const JSRuntime *rt);

extern JS_PUBLIC_API(size_t)
UserCompartmentCount(const JSRuntime *rt);

} // namespace JS

#endif // js_MemoryMetrics_h

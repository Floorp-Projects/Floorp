/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/MemoryMetrics.h"

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"

#include "jsapi.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsscript.h"

#include "ion/BaselineJIT.h"
#include "ion/Ion.h"
#include "vm/Shape.h"

#include "jsobjinlines.h"

using mozilla::DebugOnly;

using namespace js;

using JS::RuntimeStats;
using JS::ObjectPrivateVisitor;
using JS::ZoneStats;
using JS::CompartmentStats;

JS_FRIEND_API(size_t)
js::MemoryReportingSundriesThreshold()
{
    return 8 * 1024;
}

typedef HashSet<ScriptSource *, DefaultHasher<ScriptSource *>, SystemAllocPolicy> SourceSet;

struct IteratorClosure
{
    RuntimeStats *rtStats;
    ObjectPrivateVisitor *opv;
    SourceSet seenSources;
    IteratorClosure(RuntimeStats *rt, ObjectPrivateVisitor *v) : rtStats(rt), opv(v) {}
    bool init() {
        return seenSources.init();
    }
};

size_t
ZoneStats::GCHeapThingsSize()
{
    // These are just the GC-thing measurements.
    size_t n = 0;
    n += gcHeapStringsNormal;
    n += gcHeapStringsShort;
    n += gcHeapTypeObjects;
    n += gcHeapIonCodes;

    return n;
}

size_t
CompartmentStats::GCHeapThingsSize()
{
    // These are just the GC-thing measurements.
    size_t n = 0;
    n += gcHeapObjectsOrdinary;
    n += gcHeapObjectsFunction;
    n += gcHeapObjectsDenseArray;
    n += gcHeapObjectsSlowArray;
    n += gcHeapObjectsCrossCompartmentWrapper;
    n += gcHeapShapesTreeGlobalParented;
    n += gcHeapShapesTreeNonGlobalParented;
    n += gcHeapShapesDict;
    n += gcHeapShapesBase;
    n += gcHeapScripts;

    return n;
}

static void
DecommittedArenasChunkCallback(JSRuntime *rt, void *data, gc::Chunk *chunk)
{
    // This case is common and fast to check.  Do it first.
    if (chunk->decommittedArenas.isAllClear())
        return;

    size_t n = 0;
    for (size_t i = 0; i < gc::ArenasPerChunk; i++) {
        if (chunk->decommittedArenas.get(i))
            n += gc::ArenaSize;
    }
    JS_ASSERT(n > 0);
    *static_cast<size_t *>(data) += n;
}

static void
StatsCompartmentCallback(JSRuntime *rt, void *data, JSCompartment *compartment)
{
    // Append a new CompartmentStats to the vector.
    RuntimeStats *rtStats = static_cast<IteratorClosure *>(data)->rtStats;

    // CollectRuntimeStats reserves enough space.
    MOZ_ALWAYS_TRUE(rtStats->compartmentStatsVector.growBy(1));
    CompartmentStats &cStats = rtStats->compartmentStatsVector.back();
    rtStats->initExtraCompartmentStats(compartment, &cStats);

    compartment->compartmentStats = &cStats;

    // Measure the compartment object itself, and things hanging off it.
    compartment->sizeOfIncludingThis(rtStats->mallocSizeOf_,
                                     &cStats.compartmentObject,
                                     &cStats.typeInference,
                                     &cStats.shapesCompartmentTables,
                                     &cStats.crossCompartmentWrappersTable,
                                     &cStats.regexpCompartment,
                                     &cStats.debuggeesSet,
                                     &cStats.baselineStubsOptimized);
}

static void
StatsZoneCallback(JSRuntime *rt, void *data, Zone *zone)
{
    // Append a new CompartmentStats to the vector.
    RuntimeStats *rtStats = static_cast<IteratorClosure *>(data)->rtStats;

    // CollectRuntimeStats reserves enough space.
    MOZ_ALWAYS_TRUE(rtStats->zoneStatsVector.growBy(1));
    ZoneStats &zStats = rtStats->zoneStatsVector.back();
    rtStats->initExtraZoneStats(zone, &zStats);
    rtStats->currZoneStats = &zStats;

    zone->sizeOfIncludingThis(rtStats->mallocSizeOf_,
                              &zStats.typePool);
}

static void
StatsArenaCallback(JSRuntime *rt, void *data, gc::Arena *arena,
                   JSGCTraceKind traceKind, size_t thingSize)
{
    RuntimeStats *rtStats = static_cast<IteratorClosure *>(data)->rtStats;

    // The admin space includes (a) the header and (b) the padding between the
    // end of the header and the start of the first GC thing.
    size_t allocationSpace = arena->thingsSpan(thingSize);
    rtStats->currZoneStats->gcHeapArenaAdmin += gc::ArenaSize - allocationSpace;

    // We don't call the callback on unused things.  So we compute the
    // unused space like this:  arenaUnused = maxArenaUnused - arenaUsed.
    // We do this by setting arenaUnused to maxArenaUnused here, and then
    // subtracting thingSize for every used cell, in StatsCellCallback().
    rtStats->currZoneStats->gcHeapUnusedGcThings += allocationSpace;
}

static CompartmentStats *
GetCompartmentStats(JSCompartment *comp)
{
    return static_cast<CompartmentStats *>(comp->compartmentStats);
}

static void
StatsCellCallback(JSRuntime *rt, void *data, void *thing, JSGCTraceKind traceKind,
                  size_t thingSize)
{
    IteratorClosure *closure = static_cast<IteratorClosure *>(data);
    RuntimeStats *rtStats = closure->rtStats;
    ZoneStats *zStats = rtStats->currZoneStats;
    switch (traceKind) {
      case JSTRACE_OBJECT: {
        JSObject *obj = static_cast<JSObject *>(thing);
        CompartmentStats *cStats = GetCompartmentStats(obj->compartment());
        if (obj->isFunction())
            cStats->gcHeapObjectsFunction += thingSize;
        else if (obj->isArray())
            cStats->gcHeapObjectsDenseArray += thingSize;
        else if (obj->isCrossCompartmentWrapper())
            cStats->gcHeapObjectsCrossCompartmentWrapper += thingSize;
        else
            cStats->gcHeapObjectsOrdinary += thingSize;

        JS::ObjectsExtraSizes objectsExtra;
        obj->sizeOfExcludingThis(rtStats->mallocSizeOf_, &objectsExtra);
        cStats->objectsExtra.add(objectsExtra);

        // JSObject::sizeOfExcludingThis() doesn't measure objectsExtraPrivate,
        // so we do it here.
        if (ObjectPrivateVisitor *opv = closure->opv) {
            nsISupports *iface;
            if (opv->getISupports_(obj, &iface) && iface) {
                cStats->objectsExtra.private_ += opv->sizeOfIncludingThis(iface);
            }
        }
        break;
      }

      case JSTRACE_STRING: {
        JSString *str = static_cast<JSString *>(thing);

        size_t strSize = str->sizeOfExcludingThis(rtStats->mallocSizeOf_);

        // If we can't grow hugeStrings, let's just call this string non-huge.
        // We're probably about to OOM anyway.
        if (strSize >= JS::HugeStringInfo::MinSize() && zStats->hugeStrings.growBy(1)) {
            zStats->gcHeapStringsNormal += thingSize;
            JS::HugeStringInfo &info = zStats->hugeStrings.back();
            info.length = str->length();
            info.size = strSize;
            PutEscapedString(info.buffer, sizeof(info.buffer), &str->asLinear(), 0);
        } else if (str->isShort()) {
            MOZ_ASSERT(strSize == 0);
            zStats->gcHeapStringsShort += thingSize;
        } else {
            zStats->gcHeapStringsNormal += thingSize;
            zStats->stringCharsNonHuge += strSize;
        }
        break;
      }

      case JSTRACE_SHAPE: {
        Shape *shape = static_cast<Shape *>(thing);
        CompartmentStats *cStats = GetCompartmentStats(shape->compartment());
        size_t propTableSize, kidsSize;
        shape->sizeOfExcludingThis(rtStats->mallocSizeOf_, &propTableSize, &kidsSize);
        if (shape->inDictionary()) {
            cStats->gcHeapShapesDict += thingSize;
            cStats->shapesExtraDictTables += propTableSize;
            JS_ASSERT(kidsSize == 0);
        } else {
            if (shape->base()->getObjectParent() == shape->compartment()->maybeGlobal()) {
                cStats->gcHeapShapesTreeGlobalParented += thingSize;
            } else {
                cStats->gcHeapShapesTreeNonGlobalParented += thingSize;
            }
            cStats->shapesExtraTreeTables += propTableSize;
            cStats->shapesExtraTreeShapeKids += kidsSize;
        }
        break;
      }

      case JSTRACE_BASE_SHAPE: {
        BaseShape *base = static_cast<BaseShape *>(thing);
        CompartmentStats *cStats = GetCompartmentStats(base->compartment());
        cStats->gcHeapShapesBase += thingSize;
        break;
      }

      case JSTRACE_SCRIPT: {
        JSScript *script = static_cast<JSScript *>(thing);
        CompartmentStats *cStats = GetCompartmentStats(script->compartment());
        cStats->gcHeapScripts += thingSize;
        cStats->scriptData += script->sizeOfData(rtStats->mallocSizeOf_);
#ifdef JS_ION
        size_t baselineData = 0, baselineStubsFallback = 0;
        ion::SizeOfBaselineData(script, rtStats->mallocSizeOf_, &baselineData,
                                &baselineStubsFallback);
        cStats->baselineData += baselineData;
        cStats->baselineStubsFallback += baselineStubsFallback;
        cStats->ionData += ion::SizeOfIonData(script, rtStats->mallocSizeOf_);
#endif

        ScriptSource *ss = script->scriptSource();
        SourceSet::AddPtr entry = closure->seenSources.lookupForAdd(ss);
        if (!entry) {
            closure->seenSources.add(entry, ss); // Not much to be done on failure.
            rtStats->runtime.scriptSources += ss->sizeOfIncludingThis(rtStats->mallocSizeOf_);
        }
        break;
      }

      case JSTRACE_IONCODE: {
#ifdef JS_ION
        zStats->gcHeapIonCodes += thingSize;
        // The code for a script is counted in ExecutableAllocator::sizeOfCode().
#endif
        break;
      }

      case JSTRACE_TYPE_OBJECT: {
        types::TypeObject *obj = static_cast<types::TypeObject *>(thing);
        zStats->gcHeapTypeObjects += thingSize;
        zStats->typeObjects += obj->sizeOfExcludingThis(rtStats->mallocSizeOf_);
        break;
      }

    }

    // Yes, this is a subtraction:  see StatsArenaCallback() for details.
    zStats->gcHeapUnusedGcThings -= thingSize;
}

JS_PUBLIC_API(bool)
JS::CollectRuntimeStats(JSRuntime *rt, RuntimeStats *rtStats, ObjectPrivateVisitor *opv)
{
    if (!rtStats->compartmentStatsVector.reserve(rt->numCompartments))
        return false;

    if (!rtStats->zoneStatsVector.reserve(rt->zones.length()))
        return false;

    rtStats->gcHeapChunkTotal =
        size_t(JS_GetGCParameter(rt, JSGC_TOTAL_CHUNKS)) * gc::ChunkSize;

    rtStats->gcHeapUnusedChunks =
        size_t(JS_GetGCParameter(rt, JSGC_UNUSED_CHUNKS)) * gc::ChunkSize;

    IterateChunks(rt, &rtStats->gcHeapDecommittedArenas,
                  DecommittedArenasChunkCallback);

    // Take the per-compartment measurements.
    IteratorClosure closure(rtStats, opv);
    if (!closure.init())
        return false;
    rtStats->runtime.scriptSources = 0;
    IterateZonesCompartmentsArenasCells(rt, &closure, StatsZoneCallback, StatsCompartmentCallback,
                                        StatsArenaCallback, StatsCellCallback);

    // Take the "explicit/js/runtime/" measurements.
    rt->sizeOfIncludingThis(rtStats->mallocSizeOf_, &rtStats->runtime);

    DebugOnly<size_t> totalArenaSize = 0;

    rtStats->gcHeapGcThings = 0;
    for (size_t i = 0; i < rtStats->zoneStatsVector.length(); i++) {
        ZoneStats &zStats = rtStats->zoneStatsVector[i];

        rtStats->zTotals.add(zStats);
        rtStats->gcHeapGcThings += zStats.GCHeapThingsSize();
#ifdef DEBUG
        totalArenaSize += zStats.gcHeapArenaAdmin + zStats.gcHeapUnusedGcThings;
#endif
    }

    for (size_t i = 0; i < rtStats->compartmentStatsVector.length(); i++) {
        CompartmentStats &cStats = rtStats->compartmentStatsVector[i];

        rtStats->cTotals.add(cStats);
        rtStats->gcHeapGcThings += cStats.GCHeapThingsSize();
    }

#ifdef DEBUG
    totalArenaSize += rtStats->gcHeapGcThings;
    JS_ASSERT(totalArenaSize % gc::ArenaSize == 0);
#endif

    for (CompartmentsIter comp(rt); !comp.done(); comp.next())
        comp->compartmentStats = NULL;

    size_t numDirtyChunks =
        (rtStats->gcHeapChunkTotal - rtStats->gcHeapUnusedChunks) / gc::ChunkSize;
    size_t perChunkAdmin =
        sizeof(gc::Chunk) - (sizeof(gc::Arena) * gc::ArenasPerChunk);
    rtStats->gcHeapChunkAdmin = numDirtyChunks * perChunkAdmin;
    rtStats->gcHeapUnusedArenas -= rtStats->gcHeapChunkAdmin;

    // |gcHeapUnusedArenas| is the only thing left.  Compute it in terms of
    // all the others.  See the comment in RuntimeStats for explanation.
    rtStats->gcHeapUnusedArenas = rtStats->gcHeapChunkTotal -
                                  rtStats->gcHeapDecommittedArenas -
                                  rtStats->gcHeapUnusedChunks -
                                  rtStats->zTotals.gcHeapUnusedGcThings -
                                  rtStats->gcHeapChunkAdmin -
                                  rtStats->zTotals.gcHeapArenaAdmin -
                                  rtStats->gcHeapGcThings;
    return true;
}

JS_PUBLIC_API(size_t)
JS::SystemCompartmentCount(JSRuntime *rt)
{
    size_t n = 0;
    for (CompartmentsIter comp(rt); !comp.done(); comp.next()) {
        if (comp->isSystem)
            ++n;
    }
    return n;
}

JS_PUBLIC_API(size_t)
JS::UserCompartmentCount(JSRuntime *rt)
{
    size_t n = 0;
    for (CompartmentsIter comp(rt); !comp.done(); comp.next()) {
        if (!comp->isSystem)
            ++n;
    }
    return n;
}

JS_PUBLIC_API(size_t)
JS::PeakSizeOfTemporary(const JSRuntime *rt)
{
    return rt->tempLifoAlloc.peakSizeOfExcludingThis();
}


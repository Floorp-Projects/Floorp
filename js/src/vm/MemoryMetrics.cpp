/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/MemoryMetrics.h"

#include "mozilla/DebugOnly.h"

#include "jsapi.h"
#include "jscompartment.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsscript.h"

#include "jit/BaselineJIT.h"
#include "jit/Ion.h"
#include "vm/ArrayObject.h"
#include "vm/Runtime.h"
#include "vm/Shape.h"
#include "vm/String.h"
#include "vm/WrapperObject.h"

using mozilla::DebugOnly;
using mozilla::MallocSizeOf;
using mozilla::MoveRef;
using mozilla::OldMove;
using mozilla::PodEqual;

using namespace js;

using JS::RuntimeStats;
using JS::ObjectPrivateVisitor;
using JS::ZoneStats;
using JS::CompartmentStats;

namespace js {

JS_FRIEND_API(size_t)
MemoryReportingSundriesThreshold()
{
    return 8 * 1024;
}

/* static */ HashNumber
InefficientNonFlatteningStringHashPolicy::hash(const Lookup &l)
{
    ScopedJSFreePtr<jschar> ownedChars;
    const jschar *chars;
    if (l->hasPureChars()) {
        chars = l->pureChars();
    } else {
        // Slowest hash function evar!
        if (!l->copyNonPureChars(/* tcx */ nullptr, ownedChars))
            MOZ_CRASH("oom");
        chars = ownedChars;
    }

    return mozilla::HashString(chars, l->length());
}

/* static */ bool
InefficientNonFlatteningStringHashPolicy::match(const JSString *const &k, const Lookup &l)
{
    // We can't use js::EqualStrings, because that flattens our strings.
    if (k->length() != l->length())
        return false;

    const jschar *c1;
    ScopedJSFreePtr<jschar> ownedChars1;
    if (k->hasPureChars()) {
        c1 = k->pureChars();
    } else {
        if (!k->copyNonPureChars(/* tcx */ nullptr, ownedChars1))
            MOZ_CRASH("oom");
        c1 = ownedChars1;
    }

    const jschar *c2;
    ScopedJSFreePtr<jschar> ownedChars2;
    if (l->hasPureChars()) {
        c2 = l->pureChars();
    } else {
        if (!l->copyNonPureChars(/* tcx */ nullptr, ownedChars2))
            MOZ_CRASH("oom");
        c2 = ownedChars2;
    }

    return PodEqual(c1, c2, k->length());
}

} // namespace js

namespace JS {

NotableStringInfo::NotableStringInfo()
    : bufferSize(0),
      buffer(0)
{}

NotableStringInfo::NotableStringInfo(JSString *str, const StringInfo &info)
    : StringInfo(info)
{
    bufferSize = Min(str->length() + 1, size_t(4096));
    buffer = js_pod_malloc<char>(bufferSize);
    if (!buffer) {
        MOZ_CRASH("oom");
    }

    const jschar* chars;
    ScopedJSFreePtr<jschar> ownedChars;
    if (str->hasPureChars()) {
        chars = str->pureChars();
    } else {
        if (!str->copyNonPureChars(/* tcx */ nullptr, ownedChars))
            MOZ_CRASH("oom");
        chars = ownedChars;
    }

    // We might truncate |str| even if it's much shorter than 4096 chars, if
    // |str| contains unicode chars.  Since this is just for a memory reporter,
    // we don't care.
    PutEscapedString(buffer, bufferSize, chars, str->length(), /* quote */ 0);
}

NotableStringInfo::NotableStringInfo(const NotableStringInfo& info)
    : StringInfo(info),
      bufferSize(info.bufferSize)
{
    buffer = js_pod_malloc<char>(bufferSize);
    if (!buffer)
        MOZ_CRASH("oom");

    strcpy(buffer, info.buffer);
}

NotableStringInfo::NotableStringInfo(MoveRef<NotableStringInfo> info)
    : StringInfo(info)
{
    buffer = info->buffer;
    info->buffer = nullptr;
}

NotableStringInfo &NotableStringInfo::operator=(MoveRef<NotableStringInfo> info)
{
    this->~NotableStringInfo();
    new (this) NotableStringInfo(info);
    return *this;
}

} // namespace JS

typedef HashSet<ScriptSource *, DefaultHasher<ScriptSource *>, SystemAllocPolicy> SourceSet;

struct StatsClosure
{
    RuntimeStats *rtStats;
    ObjectPrivateVisitor *opv;
    SourceSet seenSources;
    StatsClosure(RuntimeStats *rt, ObjectPrivateVisitor *v) : rtStats(rt), opv(v) {}
    bool init() {
        return seenSources.init();
    }
};

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
StatsZoneCallback(JSRuntime *rt, void *data, Zone *zone)
{
    // Append a new CompartmentStats to the vector.
    RuntimeStats *rtStats = static_cast<StatsClosure *>(data)->rtStats;

    // CollectRuntimeStats reserves enough space.
    MOZ_ALWAYS_TRUE(rtStats->zoneStatsVector.growBy(1));
    ZoneStats &zStats = rtStats->zoneStatsVector.back();
    rtStats->initExtraZoneStats(zone, &zStats);
    rtStats->currZoneStats = &zStats;

    zone->addSizeOfIncludingThis(rtStats->mallocSizeOf_, &zStats.typePool);
}

static void
StatsCompartmentCallback(JSRuntime *rt, void *data, JSCompartment *compartment)
{
    // Append a new CompartmentStats to the vector.
    RuntimeStats *rtStats = static_cast<StatsClosure *>(data)->rtStats;

    // CollectRuntimeStats reserves enough space.
    MOZ_ALWAYS_TRUE(rtStats->compartmentStatsVector.growBy(1));
    CompartmentStats &cStats = rtStats->compartmentStatsVector.back();
    rtStats->initExtraCompartmentStats(compartment, &cStats);

    compartment->compartmentStats = &cStats;

    // Measure the compartment object itself, and things hanging off it.
    compartment->addSizeOfIncludingThis(rtStats->mallocSizeOf_,
                                        &cStats.typeInferencePendingArrays,
                                        &cStats.typeInferenceAllocationSiteTables,
                                        &cStats.typeInferenceArrayTypeTables,
                                        &cStats.typeInferenceObjectTypeTables,
                                        &cStats.compartmentObject,
                                        &cStats.shapesMallocHeapCompartmentTables,
                                        &cStats.crossCompartmentWrappersTable,
                                        &cStats.regexpCompartment,
                                        &cStats.debuggeesSet,
                                        &cStats.baselineStubsOptimized);
}

static void
StatsArenaCallback(JSRuntime *rt, void *data, gc::Arena *arena,
                   JSGCTraceKind traceKind, size_t thingSize)
{
    RuntimeStats *rtStats = static_cast<StatsClosure *>(data)->rtStats;

    // The admin space includes (a) the header and (b) the padding between the
    // end of the header and the start of the first GC thing.
    size_t allocationSpace = arena->thingsSpan(thingSize);
    rtStats->currZoneStats->gcHeapArenaAdmin += gc::ArenaSize - allocationSpace;

    // We don't call the callback on unused things.  So we compute the
    // unused space like this:  arenaUnused = maxArenaUnused - arenaUsed.
    // We do this by setting arenaUnused to maxArenaUnused here, and then
    // subtracting thingSize for every used cell, in StatsCellCallback().
    rtStats->currZoneStats->unusedGCThings += allocationSpace;
}

static CompartmentStats *
GetCompartmentStats(JSCompartment *comp)
{
    return static_cast<CompartmentStats *>(comp->compartmentStats);
}

enum Granularity {
    FineGrained,    // Corresponds to CollectRuntimeStats()
    CoarseGrained   // Corresponds to AddSizeOfTab()
};

template <Granularity granularity>
static void
StatsCellCallback(JSRuntime *rt, void *data, void *thing, JSGCTraceKind traceKind,
                  size_t thingSize)
{
    StatsClosure *closure = static_cast<StatsClosure *>(data);
    RuntimeStats *rtStats = closure->rtStats;
    ZoneStats *zStats = rtStats->currZoneStats;
    switch (traceKind) {
      case JSTRACE_OBJECT: {
        JSObject *obj = static_cast<JSObject *>(thing);
        CompartmentStats *cStats = GetCompartmentStats(obj->compartment());
        if (obj->is<JSFunction>())
            cStats->objectsGCHeapFunction += thingSize;
        else if (obj->is<ArrayObject>())
            cStats->objectsGCHeapDenseArray += thingSize;
        else if (obj->is<CrossCompartmentWrapperObject>())
            cStats->objectsGCHeapCrossCompartmentWrapper += thingSize;
        else
            cStats->objectsGCHeapOrdinary += thingSize;

        obj->addSizeOfExcludingThis(rtStats->mallocSizeOf_, &cStats->objectsExtra);

        if (ObjectPrivateVisitor *opv = closure->opv) {
            nsISupports *iface;
            if (opv->getISupports_(obj, &iface) && iface)
                cStats->objectsPrivate += opv->sizeOfIncludingThis(iface);
        }
        break;
      }

      case JSTRACE_STRING: {
        JSString *str = static_cast<JSString *>(thing);

        size_t strCharsSize = str->sizeOfExcludingThis(rtStats->mallocSizeOf_);
        MOZ_ASSERT_IF(str->isShort(), strCharsSize == 0);

        size_t shortStringThingSize  =  str->isShort() ? thingSize : 0;
        size_t normalStringThingSize = !str->isShort() ? thingSize : 0;

        // This string hashing is expensive.  Its results are unused when doing
        // coarse-grained measurements, and skipping it more than doubles the
        // profile speed for complex pages such as gmail.com.
        if (granularity == FineGrained) {
            ZoneStats::StringsHashMap::AddPtr p = zStats->strings.lookupForAdd(str);
            if (!p) {
                JS::StringInfo info(str->length(), shortStringThingSize,
                                    normalStringThingSize, strCharsSize);
                zStats->strings.add(p, str, info);
            } else {
                p->value.add(shortStringThingSize, normalStringThingSize, strCharsSize);
            }
        }

        zStats->stringsShortGCHeap += shortStringThingSize;
        zStats->stringsNormalGCHeap += normalStringThingSize;
        zStats->stringsNormalMallocHeap += strCharsSize;

        break;
      }

      case JSTRACE_SHAPE: {
        Shape *shape = static_cast<Shape *>(thing);
        CompartmentStats *cStats = GetCompartmentStats(shape->compartment());
        if (shape->inDictionary()) {
            cStats->shapesGCHeapDict += thingSize;

            // nullptr because kidsSize shouldn't be incremented in this case.
            shape->addSizeOfExcludingThis(rtStats->mallocSizeOf_,
                                          &cStats->shapesMallocHeapDictTables, nullptr);
        } else {
            JSObject *parent = shape->base()->getObjectParent();
            if (parent && parent->is<GlobalObject>())
                cStats->shapesGCHeapTreeGlobalParented += thingSize;
            else
                cStats->shapesGCHeapTreeNonGlobalParented += thingSize;

            shape->addSizeOfExcludingThis(rtStats->mallocSizeOf_,
                                          &cStats->shapesMallocHeapTreeTables,
                                          &cStats->shapesMallocHeapTreeShapeKids);
        }
        break;
      }

      case JSTRACE_BASE_SHAPE: {
        BaseShape *base = static_cast<BaseShape *>(thing);
        CompartmentStats *cStats = GetCompartmentStats(base->compartment());
        cStats->shapesGCHeapBase += thingSize;
        break;
      }

      case JSTRACE_SCRIPT: {
        JSScript *script = static_cast<JSScript *>(thing);
        CompartmentStats *cStats = GetCompartmentStats(script->compartment());
        cStats->scriptsGCHeap += thingSize;
        cStats->scriptsMallocHeapData += script->sizeOfData(rtStats->mallocSizeOf_);
        cStats->typeInferenceTypeScripts += script->sizeOfTypeScript(rtStats->mallocSizeOf_);
#ifdef JS_ION
        jit::AddSizeOfBaselineData(script, rtStats->mallocSizeOf_, &cStats->baselineData,
                                   &cStats->baselineStubsFallback);
        cStats->ionData += jit::SizeOfIonData(script, rtStats->mallocSizeOf_);
#endif

        ScriptSource *ss = script->scriptSource();
        SourceSet::AddPtr entry = closure->seenSources.lookupForAdd(ss);
        if (!entry) {
            closure->seenSources.add(entry, ss); // Not much to be done on failure.
            rtStats->runtime.scriptSources += ss->sizeOfIncludingThis(rtStats->mallocSizeOf_);
        }
        break;
      }

      case JSTRACE_LAZY_SCRIPT: {
        LazyScript *lazy = static_cast<LazyScript *>(thing);
        zStats->lazyScriptsGCHeap += thingSize;
        zStats->lazyScriptsMallocHeap += lazy->sizeOfExcludingThis(rtStats->mallocSizeOf_);
        break;
      }

      case JSTRACE_IONCODE: {
#ifdef JS_ION
        zStats->ionCodesGCHeap += thingSize;
        // The code for a script is counted in ExecutableAllocator::sizeOfCode().
#endif
        break;
      }

      case JSTRACE_TYPE_OBJECT: {
        types::TypeObject *obj = static_cast<types::TypeObject *>(thing);
        zStats->typeObjectsGCHeap += thingSize;
        zStats->typeObjectsMallocHeap += obj->sizeOfExcludingThis(rtStats->mallocSizeOf_);
        break;
      }

      default:
        MOZ_ASSUME_UNREACHABLE("invalid traceKind");
    }

    // Yes, this is a subtraction:  see StatsArenaCallback() for details.
    zStats->unusedGCThings -= thingSize;
}

static void
FindNotableStrings(ZoneStats &zStats)
{
    using namespace JS;

    // You should only run FindNotableStrings once per ZoneStats object
    // (although it's not going to break anything if you run it more than once,
    // unless you add to |strings| in the meantime).
    MOZ_ASSERT(zStats.notableStrings.empty());

    for (ZoneStats::StringsHashMap::Range r = zStats.strings.all(); !r.empty(); r.popFront()) {

        JSString *str = r.front().key;
        StringInfo &info = r.front().value;

        // If this string is too small, or if we can't grow the notableStrings
        // vector, skip this string.
        if (info.totalSizeOf() < NotableStringInfo::notableSize() ||
            !zStats.notableStrings.growBy(1))
            continue;

        zStats.notableStrings.back() = OldMove(NotableStringInfo(str, info));

        // We're moving this string from a non-notable to a notable bucket, so
        // subtract it out of the non-notable tallies.
        MOZ_ASSERT(zStats.stringsShortGCHeap >= info.shortGCHeap);
        MOZ_ASSERT(zStats.stringsNormalGCHeap >= info.normalGCHeap);
        MOZ_ASSERT(zStats.stringsNormalMallocHeap >= info.normalMallocHeap);
        zStats.stringsShortGCHeap -= info.shortGCHeap;
        zStats.stringsNormalGCHeap -= info.normalGCHeap;
        zStats.stringsNormalMallocHeap -= info.normalMallocHeap;
    }

    // zStats.strings holds unrooted JSString pointers, which we don't want to
    // expose out into the dangerous land where we might GC.
    zStats.strings.clear();
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
    StatsClosure closure(rtStats, opv);
    if (!closure.init())
        return false;
    IterateZonesCompartmentsArenasCells(rt, &closure, StatsZoneCallback, StatsCompartmentCallback,
                                        StatsArenaCallback, StatsCellCallback<FineGrained>);

    // Take the "explicit/js/runtime/" measurements.
    rt->addSizeOfIncludingThis(rtStats->mallocSizeOf_, &rtStats->runtime);

    for (size_t i = 0; i < rtStats->zoneStatsVector.length(); i++) {
        ZoneStats &zStats = rtStats->zoneStatsVector[i];

        rtStats->zTotals.add(zStats);

        // Move any strings which take up more than the sundries threshold
        // (counting all of their copies together) into notableStrings.
        FindNotableStrings(zStats);
    }

    FindNotableStrings(rtStats->zTotals);

    for (size_t i = 0; i < rtStats->compartmentStatsVector.length(); i++) {
        CompartmentStats &cStats = rtStats->compartmentStatsVector[i];
        rtStats->cTotals.add(cStats);
    }

    rtStats->gcHeapGCThings = rtStats->zTotals.sizeOfLiveGCThings() +
                              rtStats->cTotals.sizeOfLiveGCThings();

#ifdef DEBUG
    // Check that the in-arena measurements look ok.
    size_t totalArenaSize = rtStats->zTotals.gcHeapArenaAdmin +
                            rtStats->zTotals.unusedGCThings +
                            rtStats->gcHeapGCThings;
    JS_ASSERT(totalArenaSize % gc::ArenaSize == 0);
#endif

    for (CompartmentsIter comp(rt); !comp.done(); comp.next())
        comp->compartmentStats = nullptr;

    size_t numDirtyChunks =
        (rtStats->gcHeapChunkTotal - rtStats->gcHeapUnusedChunks) / gc::ChunkSize;
    size_t perChunkAdmin =
        sizeof(gc::Chunk) - (sizeof(gc::Arena) * gc::ArenasPerChunk);
    rtStats->gcHeapChunkAdmin = numDirtyChunks * perChunkAdmin;

    // |gcHeapUnusedArenas| is the only thing left.  Compute it in terms of
    // all the others.  See the comment in RuntimeStats for explanation.
    rtStats->gcHeapUnusedArenas = rtStats->gcHeapChunkTotal -
                                  rtStats->gcHeapDecommittedArenas -
                                  rtStats->gcHeapUnusedChunks -
                                  rtStats->zTotals.unusedGCThings -
                                  rtStats->gcHeapChunkAdmin -
                                  rtStats->zTotals.gcHeapArenaAdmin -
                                  rtStats->gcHeapGCThings;
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

namespace JS {

JS_PUBLIC_API(bool)
AddSizeOfTab(JSRuntime *rt, JSObject *obj, MallocSizeOf mallocSizeOf, ObjectPrivateVisitor *opv,
             TabSizes *sizes)
{
    class SimpleJSRuntimeStats : public JS::RuntimeStats
    {
      public:
        SimpleJSRuntimeStats(MallocSizeOf mallocSizeOf)
          : JS::RuntimeStats(mallocSizeOf)
        {}

        virtual void initExtraZoneStats(JS::Zone *zone, JS::ZoneStats *zStats)
            MOZ_OVERRIDE
        {}

        virtual void initExtraCompartmentStats(
            JSCompartment *c, JS::CompartmentStats *cStats) MOZ_OVERRIDE
        {}
    };

    SimpleJSRuntimeStats rtStats(mallocSizeOf);

    JS::Zone *zone = GetObjectZone(obj);

    if (!rtStats.compartmentStatsVector.reserve(zone->compartments.length()))
        return false;

    if (!rtStats.zoneStatsVector.reserve(1))
        return false;

    // Take the per-compartment measurements.
    StatsClosure closure(&rtStats, opv);
    if (!closure.init())
        return false;
    IterateZoneCompartmentsArenasCells(rt, zone, &closure, StatsZoneCallback,
                                       StatsCompartmentCallback, StatsArenaCallback,
                                       StatsCellCallback<CoarseGrained>);

    JS_ASSERT(rtStats.zoneStatsVector.length() == 1);
    rtStats.zTotals.add(rtStats.zoneStatsVector[0]);

    for (size_t i = 0; i < rtStats.compartmentStatsVector.length(); i++) {
        CompartmentStats &cStats = rtStats.compartmentStatsVector[i];
        rtStats.cTotals.add(cStats);
    }

    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next())
        comp->compartmentStats = NULL;

    rtStats.zTotals.addToTabSizes(sizes);
    rtStats.cTotals.addToTabSizes(sizes);

    return true;
}

} // namespace JS


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

#include "ion/BaselineJIT.h"
#include "ion/Ion.h"
#include "vm/Runtime.h"
#include "vm/Shape.h"
#include "vm/WrapperObject.h"
#include "vm/String.h"

#include "jsobjinlines.h"

using mozilla::DebugOnly;
using mozilla::Move;
using mozilla::MoveRef;
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
StringHashPolicy::hash(const Lookup &l)
{
    const jschar *chars = l->maybeChars();
    ScopedJSFreePtr<jschar> ownedChars;
    if (!chars) {
        if (!l->getCharsNonDestructive(/* tcx */ NULL, ownedChars))
            MOZ_CRASH("oom");
        chars = ownedChars;
    }

    return mozilla::HashString(chars, l->length());
}

/* static */ bool
StringHashPolicy::match(const JSString *const &k, const Lookup &l)
{
    // We can't use js::EqualStrings, because that flattens our strings.
    if (k->length() != l->length())
        return false;

    const jschar *c1 = k->maybeChars();
    ScopedJSFreePtr<jschar> ownedChars1;
    if (!c1) {
        if (!k->getCharsNonDestructive(/* tcx */ NULL, ownedChars1))
            MOZ_CRASH("oom");
        c1 = ownedChars1;
    }

    const jschar *c2 = l->maybeChars();
    ScopedJSFreePtr<jschar> ownedChars2;
    if (!c2) {
        if (!l->getCharsNonDestructive(/* tcx */ NULL, ownedChars2))
            MOZ_CRASH("oom");
        c2 = ownedChars2;
    }

    return PodEqual(c1, c2, k->length());
}

} // namespace js

namespace JS
{

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

    const jschar* chars = str->maybeChars();
    ScopedJSFreePtr<jschar> ownedChars;
    if (!chars) {
        if (!str->getCharsNonDestructive(/* tcx */ NULL, ownedChars))
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
    info->buffer = NULL;
}

NotableStringInfo &NotableStringInfo::operator=(MoveRef<NotableStringInfo> info)
{
    this->~NotableStringInfo();
    new (this) NotableStringInfo(info);
    return *this;
}

} // namespace JS

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
    n += gcHeapLazyScripts;
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
        if (obj->is<JSFunction>())
            cStats->gcHeapObjectsFunction += thingSize;
        else if (obj->is<ArrayObject>())
            cStats->gcHeapObjectsDenseArray += thingSize;
        else if (obj->is<CrossCompartmentWrapperObject>())
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

        size_t strCharsSize = str->sizeOfExcludingThis(rtStats->mallocSizeOf_);
        MOZ_ASSERT_IF(str->isShort(), strCharsSize == 0);

        size_t shortStringThingSize = str->isShort() ? thingSize : 0;
        size_t normalStringThingSize = !str->isShort() ? thingSize : 0;

        ZoneStats::StringsHashMap::AddPtr p = zStats->strings.lookupForAdd(str);
        if (!p) {
            JS::StringInfo info(str->length(), shortStringThingSize,
                                normalStringThingSize, strCharsSize);
            zStats->strings.add(p, str, info);
        } else {
            p->value.add(shortStringThingSize, normalStringThingSize, strCharsSize);
        }

        zStats->gcHeapStringsShort += shortStringThingSize;
        zStats->gcHeapStringsNormal += normalStringThingSize;
        zStats->stringCharsNonNotable += strCharsSize;

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
            JSObject *parent = shape->base()->getObjectParent();
            if (parent && parent->is<GlobalObject>())
                cStats->gcHeapShapesTreeGlobalParented += thingSize;
            else
                cStats->gcHeapShapesTreeNonGlobalParented += thingSize;
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

      case JSTRACE_LAZY_SCRIPT: {
        LazyScript *lazy = static_cast<LazyScript *>(thing);
        zStats->gcHeapLazyScripts += thingSize;
        zStats->lazyScripts += lazy->sizeOfExcludingThis(rtStats->mallocSizeOf_);
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

        zStats.notableStrings.back() = Move(NotableStringInfo(str, info));

        // We're moving this string from a non-notable to a notable bucket, so
        // subtract it out of the non-notable tallies.
        MOZ_ASSERT(zStats.gcHeapStringsShort >= info.sizeOfShortStringGCThings);
        MOZ_ASSERT(zStats.gcHeapStringsNormal >= info.sizeOfNormalStringGCThings);
        MOZ_ASSERT(zStats.stringCharsNonNotable >= info.sizeOfAllStringChars);
        zStats.gcHeapStringsShort -= info.sizeOfShortStringGCThings;
        zStats.gcHeapStringsNormal -= info.sizeOfNormalStringGCThings;
        zStats.stringCharsNonNotable -= info.sizeOfAllStringChars;
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

        // Move any strings which take up more than the sundries threshold
        // (counting all of their copies together) into notableStrings.
        FindNotableStrings(zStats);
    }

    FindNotableStrings(rtStats->zTotals);

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


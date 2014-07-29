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
#include "vm/Symbol.h"
#include "vm/WrapperObject.h"

using mozilla::DebugOnly;
using mozilla::MallocSizeOf;
using mozilla::Move;
using mozilla::PodCopy;
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

template <typename CharT>
static uint32_t
HashStringChars(JSString *s)
{
    ScopedJSFreePtr<CharT> ownedChars;
    const CharT *chars;
    JS::AutoCheckCannotGC nogc;
    if (s->isLinear()) {
        chars = s->asLinear().chars<CharT>(nogc);
    } else {
        // Slowest hash function evar!
        if (!s->asRope().copyChars<CharT>(/* tcx */ nullptr, ownedChars))
            MOZ_CRASH("oom");
        chars = ownedChars;
    }

    return mozilla::HashString(chars, s->length());
}

/* static */ HashNumber
InefficientNonFlatteningStringHashPolicy::hash(const Lookup &l)
{
    return l->hasLatin1Chars()
           ? HashStringChars<Latin1Char>(l)
           : HashStringChars<jschar>(l);
}

template <typename Char1, typename Char2>
static bool
EqualStringsPure(JSString *s1, JSString *s2)
{
    if (s1->length() != s2->length())
        return false;

    const Char1 *c1;
    ScopedJSFreePtr<Char1> ownedChars1;
    JS::AutoCheckCannotGC nogc;
    if (s1->isLinear()) {
        c1 = s1->asLinear().chars<Char1>(nogc);
    } else {
        if (!s1->asRope().copyChars<Char1>(/* tcx */ nullptr, ownedChars1))
            MOZ_CRASH("oom");
        c1 = ownedChars1;
    }

    const Char2 *c2;
    ScopedJSFreePtr<Char2> ownedChars2;
    if (s2->isLinear()) {
        c2 = s2->asLinear().chars<Char2>(nogc);
    } else {
        if (!s2->asRope().copyChars<Char2>(/* tcx */ nullptr, ownedChars2))
            MOZ_CRASH("oom");
        c2 = ownedChars2;
    }

    return EqualChars(c1, c2, s1->length());
}

/* static */ bool
InefficientNonFlatteningStringHashPolicy::match(const JSString *const &k, const Lookup &l)
{
    // We can't use js::EqualStrings, because that flattens our strings.
    JSString *s1 = const_cast<JSString *>(k);
    if (k->hasLatin1Chars()) {
        return l->hasLatin1Chars()
               ? EqualStringsPure<Latin1Char, Latin1Char>(s1, l)
               : EqualStringsPure<Latin1Char, jschar>(s1, l);
    }

    return l->hasLatin1Chars()
           ? EqualStringsPure<jschar, Latin1Char>(s1, l)
           : EqualStringsPure<jschar, jschar>(s1, l);
}

/* static */ HashNumber
CStringHashPolicy::hash(const Lookup &l)
{
    return mozilla::HashString(l);
}

/* static */ bool
CStringHashPolicy::match(const char *const &k, const Lookup &l)
{
    return strcmp(k, l) == 0;
}

} // namespace js

namespace JS {

NotableStringInfo::NotableStringInfo()
  : StringInfo(),
    buffer(0),
    length(0)
{
}

template <typename CharT>
static void
StoreStringChars(char *buffer, size_t bufferSize, JSString *str)
{
    const CharT* chars;
    ScopedJSFreePtr<CharT> ownedChars;
    JS::AutoCheckCannotGC nogc;
    if (str->isLinear()) {
        chars = str->asLinear().chars<CharT>(nogc);
    } else {
        if (!str->asRope().copyChars<CharT>(/* tcx */ nullptr, ownedChars))
            MOZ_CRASH("oom");
        chars = ownedChars;
    }

    // We might truncate |str| even if it's much shorter than 1024 chars, if
    // |str| contains unicode chars.  Since this is just for a memory reporter,
    // we don't care.
    PutEscapedString(buffer, bufferSize, chars, str->length(), /* quote */ 0);
}

NotableStringInfo::NotableStringInfo(JSString *str, const StringInfo &info)
  : StringInfo(info),
    length(str->length())
{
    size_t bufferSize = Min(str->length() + 1, size_t(MAX_SAVED_CHARS));
    buffer = js_pod_malloc<char>(bufferSize);
    if (!buffer) {
        MOZ_CRASH("oom");
    }

    if (str->hasLatin1Chars())
        StoreStringChars<Latin1Char>(buffer, bufferSize, str);
    else
        StoreStringChars<jschar>(buffer, bufferSize, str);
}

NotableStringInfo::NotableStringInfo(NotableStringInfo &&info)
  : StringInfo(Move(info)),
    length(info.length)
{
    buffer = info.buffer;
    info.buffer = nullptr;
}

NotableStringInfo &NotableStringInfo::operator=(NotableStringInfo &&info)
{
    MOZ_ASSERT(this != &info, "self-move assignment is prohibited");
    this->~NotableStringInfo();
    new (this) NotableStringInfo(Move(info));
    return *this;
}

NotableScriptSourceInfo::NotableScriptSourceInfo()
  : ScriptSourceInfo(),
    filename_(nullptr)
{
}

NotableScriptSourceInfo::NotableScriptSourceInfo(const char *filename, const ScriptSourceInfo &info)
  : ScriptSourceInfo(info)
{
    size_t bytes = strlen(filename) + 1;
    filename_ = js_pod_malloc<char>(bytes);
    if (!filename_)
        MOZ_CRASH("oom");
    PodCopy(filename_, filename, bytes);
}

NotableScriptSourceInfo::NotableScriptSourceInfo(NotableScriptSourceInfo &&info)
  : ScriptSourceInfo(Move(info))
{
    filename_ = info.filename_;
    info.filename_ = nullptr;
}

NotableScriptSourceInfo &NotableScriptSourceInfo::operator=(NotableScriptSourceInfo &&info)
{
    MOZ_ASSERT(this != &info, "self-move assignment is prohibited");
    this->~NotableScriptSourceInfo();
    new (this) NotableScriptSourceInfo(Move(info));
    return *this;
}


} // namespace JS

typedef HashSet<ScriptSource *, DefaultHasher<ScriptSource *>, SystemAllocPolicy> SourceSet;

struct StatsClosure
{
    RuntimeStats *rtStats;
    ObjectPrivateVisitor *opv;
    SourceSet seenSources;
    bool anonymize;

    StatsClosure(RuntimeStats *rt, ObjectPrivateVisitor *v, bool anon)
      : rtStats(rt),
        opv(v),
        anonymize(anon)
    {}

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
    if (!zStats.initStrings(rt))
        MOZ_CRASH("oom");
    rtStats->initExtraZoneStats(zone, &zStats);
    rtStats->currZoneStats = &zStats;

    zone->addSizeOfIncludingThis(rtStats->mallocSizeOf_,
                                 &zStats.typePool,
                                 &zStats.baselineStubsOptimized);
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
                                        &cStats.typeInferenceAllocationSiteTables,
                                        &cStats.typeInferenceArrayTypeTables,
                                        &cStats.typeInferenceObjectTypeTables,
                                        &cStats.compartmentObject,
                                        &cStats.shapesMallocHeapCompartmentTables,
                                        &cStats.crossCompartmentWrappersTable,
                                        &cStats.regexpCompartment,
                                        &cStats.debuggeesSet,
                                        &cStats.savedStacksSet);
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

// FineGrained is used for normal memory reporting.  CoarseGrained is used by
// AddSizeOfTab(), which aggregates all the measurements into a handful of
// high-level numbers, which means that fine-grained reporting would be a waste
// of effort.
enum Granularity {
    FineGrained,
    CoarseGrained
};

// The various kinds of hashing are expensive, and the results are unused when
// doing coarse-grained measurements. Skipping them more than doubles the
// profile speed for complex pages such as gmail.com.
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

        JS::StringInfo info;
        if (str->hasLatin1Chars()) {
            info.gcHeapLatin1 = thingSize;
            info.mallocHeapLatin1 = str->sizeOfExcludingThis(rtStats->mallocSizeOf_);
        } else {
            info.gcHeapTwoByte = thingSize;
            info.mallocHeapTwoByte = str->sizeOfExcludingThis(rtStats->mallocSizeOf_);
        }
        info.numCopies = 1;

        zStats->stringInfo.add(info);

        // The primary use case for anonymization is automated crash submission
        // (to help detect OOM crashes). In that case, we don't want to pay the
        // memory cost required to do notable string detection.
        if (granularity == FineGrained && !closure->anonymize) {
            ZoneStats::StringsHashMap::AddPtr p = zStats->allStrings->lookupForAdd(str);
            if (!p) {
                // Ignore failure -- we just won't record the string as notable.
                (void)zStats->allStrings->add(p, str, info);
            } else {
                p->value().add(info);
            }
        }
        break;
      }

      case JSTRACE_SYMBOL:
        zStats->symbolsGCHeap += thingSize;
        break;

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
        jit::AddSizeOfBaselineData(script, rtStats->mallocSizeOf_, &cStats->baselineData,
                                   &cStats->baselineStubsFallback);
        cStats->ionData += jit::SizeOfIonData(script, rtStats->mallocSizeOf_);

        ScriptSource *ss = script->scriptSource();
        SourceSet::AddPtr entry = closure->seenSources.lookupForAdd(ss);
        if (!entry) {
            (void)closure->seenSources.add(entry, ss); // Not much to be done on failure.

            JS::ScriptSourceInfo info;  // This zeroes all the sizes.
            ss->addSizeOfIncludingThis(rtStats->mallocSizeOf_, &info);
            MOZ_ASSERT(info.compressed == 0 || info.uncompressed == 0);

            rtStats->runtime.scriptSourceInfo.add(info);

            if (granularity == FineGrained) {
                const char* filename = ss->filename();
                if (!filename)
                    filename = "<no filename>";

                JS::RuntimeSizes::ScriptSourcesHashMap::AddPtr p =
                    rtStats->runtime.allScriptSources->lookupForAdd(filename);
                if (!p) {
                    // Ignore failure -- we just won't record the script source as notable.
                    (void)rtStats->runtime.allScriptSources->add(p, filename, info);
                } else {
                    p->value().add(info);
                }
            }
        }

        break;
      }

      case JSTRACE_LAZY_SCRIPT: {
        LazyScript *lazy = static_cast<LazyScript *>(thing);
        zStats->lazyScriptsGCHeap += thingSize;
        zStats->lazyScriptsMallocHeap += lazy->sizeOfExcludingThis(rtStats->mallocSizeOf_);
        break;
      }

      case JSTRACE_JITCODE: {
        zStats->jitCodesGCHeap += thingSize;
        // The code for a script is counted in ExecutableAllocator::sizeOfCode().
        break;
      }

      case JSTRACE_TYPE_OBJECT: {
        types::TypeObject *obj = static_cast<types::TypeObject *>(thing);
        zStats->typeObjectsGCHeap += thingSize;
        zStats->typeObjectsMallocHeap += obj->sizeOfExcludingThis(rtStats->mallocSizeOf_);
        break;
      }

      default:
        MOZ_CRASH("invalid traceKind");
    }

    // Yes, this is a subtraction:  see StatsArenaCallback() for details.
    zStats->unusedGCThings -= thingSize;
}

static bool
FindNotableStrings(ZoneStats &zStats)
{
    using namespace JS;

    // We should only run FindNotableStrings once per ZoneStats object.
    MOZ_ASSERT(zStats.notableStrings.empty());

    for (ZoneStats::StringsHashMap::Range r = zStats.allStrings->all(); !r.empty(); r.popFront()) {

        JSString *str = r.front().key();
        StringInfo &info = r.front().value();

        if (!info.isNotable())
            continue;

        if (!zStats.notableStrings.growBy(1))
            return false;

        zStats.notableStrings.back() = NotableStringInfo(str, info);

        // We're moving this string from a non-notable to a notable bucket, so
        // subtract it out of the non-notable tallies.
        zStats.stringInfo.subtract(info);
    }
    // Delete |allStrings| now, rather than waiting for zStats's destruction,
    // to reduce peak memory consumption during reporting.
    js_delete(zStats.allStrings);
    zStats.allStrings = nullptr;
    return true;
}

bool
ZoneStats::initStrings(JSRuntime *rt)
{
    isTotals = false;
    allStrings = rt->new_<StringsHashMap>();
    if (!allStrings)
        return false;
    if (!allStrings->init()) {
        js_delete(allStrings);
        allStrings = nullptr;
        return false;
    }
    return true;
}

static bool
FindNotableScriptSources(JS::RuntimeSizes &runtime)
{
    using namespace JS;

    // We should only run FindNotableScriptSources once per RuntimeSizes.
    MOZ_ASSERT(runtime.notableScriptSources.empty());

    for (RuntimeSizes::ScriptSourcesHashMap::Range r = runtime.allScriptSources->all();
         !r.empty();
         r.popFront())
    {
        const char *filename = r.front().key();
        ScriptSourceInfo &info = r.front().value();

        if (!info.isNotable())
            continue;

        if (!runtime.notableScriptSources.growBy(1))
            return false;

        runtime.notableScriptSources.back() = NotableScriptSourceInfo(filename, info);

        // We're moving this script source from a non-notable to a notable
        // bucket, so subtract its sizes from the non-notable tallies.
        runtime.scriptSourceInfo.subtract(info);
    }
    // Delete |allScriptSources| now, rather than waiting for zStats's
    // destruction, to reduce peak memory consumption during reporting.
    js_delete(runtime.allScriptSources);
    runtime.allScriptSources = nullptr;
    return true;
}

JS_PUBLIC_API(bool)
JS::CollectRuntimeStats(JSRuntime *rt, RuntimeStats *rtStats, ObjectPrivateVisitor *opv,
                        bool anonymize)
{
    if (!rtStats->compartmentStatsVector.reserve(rt->numCompartments))
        return false;

    if (!rtStats->zoneStatsVector.reserve(rt->gc.zones.length()))
        return false;

    rtStats->gcHeapChunkTotal =
        size_t(JS_GetGCParameter(rt, JSGC_TOTAL_CHUNKS)) * gc::ChunkSize;

    rtStats->gcHeapUnusedChunks =
        size_t(JS_GetGCParameter(rt, JSGC_UNUSED_CHUNKS)) * gc::ChunkSize;

    IterateChunks(rt, &rtStats->gcHeapDecommittedArenas,
                  DecommittedArenasChunkCallback);

    // Take the per-compartment measurements.
    StatsClosure closure(rtStats, opv, anonymize);
    if (!closure.init())
        return false;
    IterateZonesCompartmentsArenasCells(rt, &closure,
                                        StatsZoneCallback,
                                        StatsCompartmentCallback,
                                        StatsArenaCallback,
                                        StatsCellCallback<FineGrained>);

    // Take the "explicit/js/runtime/" measurements.
    rt->addSizeOfIncludingThis(rtStats->mallocSizeOf_, &rtStats->runtime);

    if (!FindNotableScriptSources(rtStats->runtime))
        return false;

    ZoneStatsVector &zs = rtStats->zoneStatsVector;
    ZoneStats &zTotals = rtStats->zTotals;

    // We don't look for notable strings for zTotals. So we first sum all the
    // zones' measurements to get the totals. Then we find the notable strings
    // within each zone.
    for (size_t i = 0; i < zs.length(); i++)
        zTotals.addSizes(zs[i]);

    for (size_t i = 0; i < zs.length(); i++)
        if (!FindNotableStrings(zs[i]))
            return false;

    MOZ_ASSERT(!zTotals.allStrings);

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

    for (CompartmentsIter comp(rt, WithAtoms); !comp.done(); comp.next())
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
    for (CompartmentsIter comp(rt, WithAtoms); !comp.done(); comp.next()) {
        if (comp->isSystem)
            ++n;
    }
    return n;
}

JS_PUBLIC_API(size_t)
JS::UserCompartmentCount(JSRuntime *rt)
{
    size_t n = 0;
    for (CompartmentsIter comp(rt, WithAtoms); !comp.done(); comp.next()) {
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
AddSizeOfTab(JSRuntime *rt, HandleObject obj, MallocSizeOf mallocSizeOf, ObjectPrivateVisitor *opv,
             TabSizes *sizes)
{
    class SimpleJSRuntimeStats : public JS::RuntimeStats
    {
      public:
        explicit SimpleJSRuntimeStats(MallocSizeOf mallocSizeOf)
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

    // Take the per-compartment measurements. No need to anonymize because
    // these measurements will be aggregated.
    StatsClosure closure(&rtStats, opv, /* anonymize = */ false);
    if (!closure.init())
        return false;
    IterateZoneCompartmentsArenasCells(rt, zone, &closure, StatsZoneCallback,
                                       StatsCompartmentCallback, StatsArenaCallback,
                                       StatsCellCallback<CoarseGrained>);

    JS_ASSERT(rtStats.zoneStatsVector.length() == 1);
    rtStats.zTotals.addSizes(rtStats.zoneStatsVector[0]);

    for (size_t i = 0; i < rtStats.compartmentStatsVector.length(); i++) {
        CompartmentStats &cStats = rtStats.compartmentStatsVector[i];
        rtStats.cTotals.add(cStats);
    }

    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next())
        comp->compartmentStats = nullptr;

    rtStats.zTotals.addToTabSizes(sizes);
    rtStats.cTotals.addToTabSizes(sizes);

    return true;
}

} // namespace JS


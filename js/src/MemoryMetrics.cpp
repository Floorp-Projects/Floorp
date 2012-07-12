/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/MemoryMetrics.h"

#include "mozilla/Assertions.h"

#include "jsapi.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsscript.h"

#include "jsobjinlines.h"

#ifdef JS_THREADSAFE

namespace JS {

using namespace js;

struct IteratorClosure
{
  RuntimeStats *rtStats;
  ObjectPrivateVisitor *opv;
  IteratorClosure(RuntimeStats *rt, ObjectPrivateVisitor *v) : rtStats(rt), opv(v) {}
};

size_t
CompartmentStats::gcHeapThingsSize()
{
    // These are just the GC-thing measurements.
    size_t n = 0;
    n += gcHeapObjectsNonFunction;
    n += gcHeapObjectsFunction;
    n += gcHeapStrings;
    n += gcHeapShapesTree;
    n += gcHeapShapesDict;
    n += gcHeapShapesBase;
    n += gcHeapScripts;
    n += gcHeapTypeObjects;
#if JS_HAS_XML_SUPPORT
    n += gcHeapXML;
#endif

#ifdef DEBUG
    size_t n2 = n;
    n2 += gcHeapArenaAdmin;
    n2 += gcHeapUnusedGcThings;
    // These numbers should sum to a multiple of the arena size.
    JS_ASSERT(n2 % gc::ArenaSize == 0);
#endif

    return n;
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
    rtStats->currCompartmentStats = &cStats;

    // Get the compartment-level numbers.
    compartment->sizeOfTypeInferenceData(&cStats.typeInferenceSizes, rtStats->mallocSizeOf);
    cStats.shapesCompartmentTables = compartment->sizeOfShapeTable(rtStats->mallocSizeOf);
    cStats.crossCompartmentWrappers =
        compartment->crossCompartmentWrappers.sizeOfExcludingThis(rtStats->mallocSizeOf);
}

static void
StatsChunkCallback(JSRuntime *rt, void *data, gc::Chunk *chunk)
{
    RuntimeStats *rtStats = static_cast<RuntimeStats *>(data);
    for (size_t i = 0; i < gc::ArenasPerChunk; i++)
        if (chunk->decommittedArenas.get(i))
            rtStats->gcHeapDecommittedArenas += gc::ArenaSize;
}

static void
StatsArenaCallback(JSRuntime *rt, void *data, gc::Arena *arena,
                   JSGCTraceKind traceKind, size_t thingSize)
{
    RuntimeStats *rtStats = static_cast<IteratorClosure *>(data)->rtStats;

    // The admin space includes (a) the header and (b) the padding between the
    // end of the header and the start of the first GC thing.
    size_t allocationSpace = arena->thingsSpan(thingSize);
    rtStats->currCompartmentStats->gcHeapArenaAdmin +=
        gc::ArenaSize - allocationSpace;

    // We don't call the callback on unused things.  So we compute the
    // unused space like this:  arenaUnused = maxArenaUnused - arenaUsed.
    // We do this by setting arenaUnused to maxArenaUnused here, and then
    // subtracting thingSize for every used cell, in StatsCellCallback().
    rtStats->currCompartmentStats->gcHeapUnusedGcThings += allocationSpace;
}

static void
StatsCellCallback(JSRuntime *rt, void *data, void *thing, JSGCTraceKind traceKind,
                  size_t thingSize)
{
    IteratorClosure *closure = static_cast<IteratorClosure *>(data);
    RuntimeStats *rtStats = closure->rtStats;
    CompartmentStats *cStats = rtStats->currCompartmentStats;
    switch (traceKind) {
    case JSTRACE_OBJECT:
    {
        JSObject *obj = static_cast<JSObject *>(thing);
        if (obj->isFunction()) {
            cStats->gcHeapObjectsFunction += thingSize;
        } else {
            cStats->gcHeapObjectsNonFunction += thingSize;
        }
        size_t slotsSize, elementsSize, miscSize;
        obj->sizeOfExcludingThis(rtStats->mallocSizeOf, &slotsSize,
                                 &elementsSize, &miscSize);
        cStats->objectSlots += slotsSize;
        cStats->objectElements += elementsSize;
        cStats->objectMisc += miscSize;

        if (ObjectPrivateVisitor *opv = closure->opv) {
            js::Class *clazz = js::GetObjectClass(obj);
            if (clazz->flags & JSCLASS_HAS_PRIVATE &&
                clazz->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS)
            {
                cStats->objectPrivate += opv->sizeOfIncludingThis(GetObjectPrivate(obj));
            }
        }
        break;
    }
    case JSTRACE_STRING:
    {
        JSString *str = static_cast<JSString *>(thing);
        cStats->gcHeapStrings += thingSize;
        cStats->stringChars += str->sizeOfExcludingThis(rtStats->mallocSizeOf);
        break;
    }
    case JSTRACE_SHAPE:
    {
        Shape *shape = static_cast<Shape*>(thing);
        size_t propTableSize, kidsSize;
        shape->sizeOfExcludingThis(rtStats->mallocSizeOf, &propTableSize, &kidsSize);
        if (shape->inDictionary()) {
            cStats->gcHeapShapesDict += thingSize;
            cStats->shapesExtraDictTables += propTableSize;
            JS_ASSERT(kidsSize == 0);
        } else {
            cStats->gcHeapShapesTree += thingSize;
            cStats->shapesExtraTreeTables += propTableSize;
            cStats->shapesExtraTreeShapeKids += kidsSize;
        }
        break;
    }
    case JSTRACE_BASE_SHAPE:
    {
        cStats->gcHeapShapesBase += thingSize;
        break;
    }
    case JSTRACE_SCRIPT:
    {
        JSScript *script = static_cast<JSScript *>(thing);
        cStats->gcHeapScripts += thingSize;
        cStats->scriptData += script->sizeOfData(rtStats->mallocSizeOf);
#ifdef JS_METHODJIT
        cStats->mjitData += script->sizeOfJitScripts(rtStats->mallocSizeOf);
#endif
        break;
    }
    case JSTRACE_TYPE_OBJECT:
    {
        types::TypeObject *obj = static_cast<types::TypeObject *>(thing);
        cStats->gcHeapTypeObjects += thingSize;
        obj->sizeOfExcludingThis(&cStats->typeInferenceSizes, rtStats->mallocSizeOf);
        break;
    }
#if JS_HAS_XML_SUPPORT
    case JSTRACE_XML:
    {
        cStats->gcHeapXML += thingSize;
        break;
    }
#endif
    }
    // Yes, this is a subtraction:  see StatsArenaCallback() for details.
    cStats->gcHeapUnusedGcThings -= thingSize;
}

JS_PUBLIC_API(bool)
CollectRuntimeStats(JSRuntime *rt, RuntimeStats *rtStats, ObjectPrivateVisitor *opv)
{
    if (!rtStats->compartmentStatsVector.reserve(rt->compartments.length()))
        return false;

    rtStats->gcHeapChunkTotal =
        size_t(JS_GetGCParameter(rt, JSGC_TOTAL_CHUNKS)) * gc::ChunkSize;

    rtStats->gcHeapUnusedChunks =
        size_t(JS_GetGCParameter(rt, JSGC_UNUSED_CHUNKS)) * gc::ChunkSize;

    // This just computes rtStats->gcHeapDecommittedArenas.
    IterateChunks(rt, rtStats, StatsChunkCallback);

    // Take the per-compartment measurements.
    IteratorClosure closure(rtStats, opv);
    IterateCompartmentsArenasCells(rt, &closure, StatsCompartmentCallback,
                                   StatsArenaCallback, StatsCellCallback);

    // Take the "explicit/js/runtime/" measurements.
    rt->sizeOfIncludingThis(rtStats->mallocSizeOf, &rtStats->runtime);

    rtStats->gcHeapGcThings = 0;
    for (size_t i = 0; i < rtStats->compartmentStatsVector.length(); i++) {
        CompartmentStats &cStats = rtStats->compartmentStatsVector[i];

        rtStats->totals.add(cStats);
        rtStats->gcHeapGcThings += cStats.gcHeapThingsSize();
    }

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
                                  rtStats->totals.gcHeapUnusedGcThings -
                                  rtStats->gcHeapChunkAdmin -
                                  rtStats->totals.gcHeapArenaAdmin -
                                  rtStats->gcHeapGcThings;
    return true;
}

JS_PUBLIC_API(int64_t)
GetExplicitNonHeapForRuntime(JSRuntime *rt, JSMallocSizeOfFun mallocSizeOf)
{
    // explicit/<compartment>/gc-heap/*
    size_t n = size_t(JS_GetGCParameter(rt, JSGC_TOTAL_CHUNKS)) * gc::ChunkSize;

    // explicit/runtime/mjit-code
    // explicit/runtime/regexp-code
    // explicit/runtime/stack-committed
    // explicit/runtime/unused-code-memory
    n += rt->sizeOfExplicitNonHeap();

    return int64_t(n);
}

JS_PUBLIC_API(size_t)
SystemCompartmentCount(const JSRuntime *rt)
{
    size_t n = 0;
    for (size_t i = 0; i < rt->compartments.length(); i++) {
        if (rt->compartments[i]->isSystemCompartment)
            ++n;
    }
    return n;
}

JS_PUBLIC_API(size_t)
UserCompartmentCount(const JSRuntime *rt)
{
    size_t n = 0;
    for (size_t i = 0; i < rt->compartments.length(); i++) {
        if (!rt->compartments[i]->isSystemCompartment)
            ++n;
    }
    return n;
}

} // namespace JS

#endif // JS_THREADSAFE

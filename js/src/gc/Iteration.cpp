/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscompartment.h"
#include "jsgc.h"

#include "gc/GCInternals.h"
#include "js/HashTable.h"
#include "vm/Runtime.h"

#include "jscntxtinlines.h"
#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;

void
js::TraceRuntime(JSTracer* trc)
{
    MOZ_ASSERT(!trc->isMarkingTracer());

    JSRuntime* rt = trc->runtime();
    rt->gc.evictNursery();
    AutoPrepareForTracing prep(rt->contextFromMainThread(), WithAtoms);
    gcstats::AutoPhase ap(rt->gc.stats, gcstats::PHASE_TRACE_HEAP);
    rt->gc.markRuntime(trc, GCRuntime::TraceRuntime, prep.session().lock);
}

static void
IterateCompartmentsArenasCells(JSContext* cx, Zone* zone, void* data,
                               JSIterateCompartmentCallback compartmentCallback,
                               IterateArenaCallback arenaCallback,
                               IterateCellCallback cellCallback)
{
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next())
        (*compartmentCallback)(cx, data, comp);

    for (auto thingKind : AllAllocKinds()) {
        JS::TraceKind traceKind = MapAllocToTraceKind(thingKind);
        size_t thingSize = Arena::thingSize(thingKind);

        for (ArenaIter aiter(zone, thingKind); !aiter.done(); aiter.next()) {
            Arena* arena = aiter.get();
            (*arenaCallback)(cx, data, arena, traceKind, thingSize);
            for (ArenaCellIterUnderGC iter(arena); !iter.done(); iter.next())
                (*cellCallback)(cx, data, iter.getCell(), traceKind, thingSize);
        }
    }
}

void
js::IterateZonesCompartmentsArenasCells(JSContext* cx, void* data,
                                        IterateZoneCallback zoneCallback,
                                        JSIterateCompartmentCallback compartmentCallback,
                                        IterateArenaCallback arenaCallback,
                                        IterateCellCallback cellCallback)
{
    AutoPrepareForTracing prop(cx, WithAtoms);

    for (ZonesIter zone(cx, WithAtoms); !zone.done(); zone.next()) {
        (*zoneCallback)(cx, data, zone);
        IterateCompartmentsArenasCells(cx, zone, data,
                                       compartmentCallback, arenaCallback, cellCallback);
    }
}

void
js::IterateZoneCompartmentsArenasCells(JSContext* cx, Zone* zone, void* data,
                                       IterateZoneCallback zoneCallback,
                                       JSIterateCompartmentCallback compartmentCallback,
                                       IterateArenaCallback arenaCallback,
                                       IterateCellCallback cellCallback)
{
    AutoPrepareForTracing prop(cx, WithAtoms);

    (*zoneCallback)(cx, data, zone);
    IterateCompartmentsArenasCells(cx, zone, data,
                                   compartmentCallback, arenaCallback, cellCallback);
}

void
js::IterateChunks(JSContext* cx, void* data, IterateChunkCallback chunkCallback)
{
    AutoPrepareForTracing prep(cx, SkipAtoms);

    for (auto chunk = cx->gc.allNonEmptyChunks(); !chunk.done(); chunk.next())
        chunkCallback(cx, data, chunk);
}

void
js::IterateScripts(JSContext* cx, JSCompartment* compartment,
                   void* data, IterateScriptCallback scriptCallback)
{
    MOZ_ASSERT(!cx->mainThread().suppressGC);
    AutoEmptyNursery empty(cx);
    AutoPrepareForTracing prep(cx, SkipAtoms);

    if (compartment) {
        Zone* zone = compartment->zone();
        for (auto script = zone->cellIter<JSScript>(empty); !script.done(); script.next()) {
            if (script->compartment() == compartment)
                scriptCallback(cx, data, script);
        }
    } else {
        for (ZonesIter zone(cx, SkipAtoms); !zone.done(); zone.next()) {
            for (auto script = zone->cellIter<JSScript>(empty); !script.done(); script.next())
                scriptCallback(cx, data, script);
        }
    }
}

void
js::IterateGrayObjects(Zone* zone, GCThingCallback cellCallback, void* data)
{
    JSRuntime* rt = zone->runtimeFromMainThread();
    AutoEmptyNursery empty(rt);
    AutoPrepareForTracing prep(rt->contextFromMainThread(), SkipAtoms);

    for (auto thingKind : ObjectAllocKinds()) {
        for (auto obj = zone->cellIter<JSObject>(thingKind, empty); !obj.done(); obj.next()) {
            if (obj->asTenured().isMarked(GRAY))
                cellCallback(data, JS::GCCellPtr(obj.get()));
        }
    }
}

JS_PUBLIC_API(void)
JS_IterateCompartments(JSContext* cx, void* data,
                       JSIterateCompartmentCallback compartmentCallback)
{
    AutoTraceSession session(cx);

    for (CompartmentsIter c(cx, WithAtoms); !c.done(); c.next())
        (*compartmentCallback)(cx, data, c);
}

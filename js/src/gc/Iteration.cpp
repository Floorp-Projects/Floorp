/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jsprf.h"

#include "js/HashTable.h"
#include "gc/GCInternals.h"

#include "jsobjinlines.h"
#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;

void
js::TraceRuntime(JSTracer *trc)
{
    JS_ASSERT(!IS_GC_MARKING_TRACER(trc));

    AutoPrepareForTracing prep(trc->runtime);
    MarkRuntime(trc);
}

void
js::IterateZonesCompartmentsArenasCells(JSRuntime *rt, void *data,
                                        IterateZoneCallback zoneCallback,
                                        JSIterateCompartmentCallback compartmentCallback,
                                        IterateArenaCallback arenaCallback,
                                        IterateCellCallback cellCallback)
{
    AutoPrepareForTracing prop(rt);

    for (ZonesIter zone(rt); !zone.done(); zone.next()) {
        (*zoneCallback)(rt, data, zone);

        for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next())
            (*compartmentCallback)(rt, data, comp);

        for (size_t thingKind = 0; thingKind != FINALIZE_LIMIT; thingKind++) {
            JSGCTraceKind traceKind = MapAllocToTraceKind(AllocKind(thingKind));
            size_t thingSize = Arena::thingSize(AllocKind(thingKind));

            for (ArenaIter aiter(zone, AllocKind(thingKind)); !aiter.done(); aiter.next()) {
                ArenaHeader *aheader = aiter.get();
                (*arenaCallback)(rt, data, aheader->getArena(), traceKind, thingSize);
                for (CellIterUnderGC iter(aheader); !iter.done(); iter.next())
                    (*cellCallback)(rt, data, iter.getCell(), traceKind, thingSize);
            }
        }
    }
}

void
js::IterateChunks(JSRuntime *rt, void *data, IterateChunkCallback chunkCallback)
{
    AutoPrepareForTracing prep(rt);

    for (js::GCChunkSet::Range r = rt->gcChunkSet.all(); !r.empty(); r.popFront())
        chunkCallback(rt, data, r.front());
}

void
js::IterateScripts(JSRuntime *rt, JSCompartment *compartment,
                   void *data, IterateScriptCallback scriptCallback)
{
    AutoPrepareForTracing prep(rt);

    if (compartment) {
        for (CellIterUnderGC i(compartment->zone(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            if (script->compartment() == compartment)
                scriptCallback(rt, data, script);
        }
    } else {
        for (ZonesIter zone(rt); !zone.done(); zone.next()) {
            for (CellIterUnderGC i(zone, gc::FINALIZE_SCRIPT); !i.done(); i.next())
                scriptCallback(rt, data, i.get<JSScript>());
        }
    }
}

void
js::IterateGrayObjects(Zone *zone, GCThingCallback cellCallback, void *data)
{
    AutoPrepareForTracing prep(zone->rt);

    for (size_t finalizeKind = 0; finalizeKind <= FINALIZE_OBJECT_LAST; finalizeKind++) {
        for (CellIterUnderGC i(zone, AllocKind(finalizeKind)); !i.done(); i.next()) {
            JSObject *obj = i.get<JSObject>();
            if (obj->isMarked(GRAY))
                cellCallback(data, obj);
        }
    }
}

JS_PUBLIC_API(void)
JS_IterateCompartments(JSRuntime *rt, void *data,
                       JSIterateCompartmentCallback compartmentCallback)
{
    JS_ASSERT(!rt->isHeapBusy());

    AutoTraceSession session(rt);
    rt->gcHelperThread.waitBackgroundSweepOrAllocEnd();

    for (CompartmentsIter c(rt); !c.done(); c.next())
        (*compartmentCallback)(rt, data, c);
}

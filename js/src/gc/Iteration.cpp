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

struct IterateArenaCallbackOp
{
    JSRuntime *rt;
    void *data;
    IterateArenaCallback callback;
    JSGCTraceKind traceKind;
    size_t thingSize;
    IterateArenaCallbackOp(JSRuntime *rt, void *data, IterateArenaCallback callback,
                           JSGCTraceKind traceKind, size_t thingSize)
        : rt(rt), data(data), callback(callback), traceKind(traceKind), thingSize(thingSize)
    {}
    void operator()(Arena *arena) { (*callback)(rt, data, arena, traceKind, thingSize); }
};

struct IterateCellCallbackOp
{
    JSRuntime *rt;
    void *data;
    IterateCellCallback callback;
    JSGCTraceKind traceKind;
    size_t thingSize;
    IterateCellCallbackOp(JSRuntime *rt, void *data, IterateCellCallback callback,
                          JSGCTraceKind traceKind, size_t thingSize)
        : rt(rt), data(data), callback(callback), traceKind(traceKind), thingSize(thingSize)
    {}
    void operator()(Cell *cell) { (*callback)(rt, data, cell, traceKind, thingSize); }
};

void
js::IterateCompartmentsArenasCells(JSRuntime *rt, void *data,
                                   JSIterateCompartmentCallback compartmentCallback,
                                   IterateArenaCallback arenaCallback,
                                   IterateCellCallback cellCallback)
{
    AutoPrepareForTracing prop(rt);

    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        (*compartmentCallback)(rt, data, c);

        for (size_t thingKind = 0; thingKind != FINALIZE_LIMIT; thingKind++) {
            JSGCTraceKind traceKind = MapAllocToTraceKind(AllocKind(thingKind));
            size_t thingSize = Arena::thingSize(AllocKind(thingKind));
            IterateArenaCallbackOp arenaOp(rt, data, arenaCallback, traceKind, thingSize);
            IterateCellCallbackOp cellOp(rt, data, cellCallback, traceKind, thingSize);
            ForEachArenaAndCell(c, AllocKind(thingKind), arenaOp, cellOp);
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
js::IterateCells(JSRuntime *rt, JSCompartment *compartment, AllocKind thingKind,
                 void *data, IterateCellCallback cellCallback)
{
    AutoPrepareForTracing prep(rt);

    JSGCTraceKind traceKind = MapAllocToTraceKind(thingKind);
    size_t thingSize = Arena::thingSize(thingKind);

    if (compartment) {
        for (CellIterUnderGC i(compartment, thingKind); !i.done(); i.next())
            cellCallback(rt, data, i.getCell(), traceKind, thingSize);
    } else {
        for (CompartmentsIter c(rt); !c.done(); c.next()) {
            for (CellIterUnderGC i(c, thingKind); !i.done(); i.next())
                cellCallback(rt, data, i.getCell(), traceKind, thingSize);
        }
    }
}

void
js::IterateGrayObjects(JSCompartment *compartment, GCThingCallback cellCallback, void *data)
{
    JS_ASSERT(compartment);
    AutoPrepareForTracing prep(compartment->rt);

    for (size_t finalizeKind = 0; finalizeKind <= FINALIZE_OBJECT_LAST; finalizeKind++) {
        for (CellIterUnderGC i(compartment, AllocKind(finalizeKind)); !i.done(); i.next()) {
            Cell *cell = i.getCell();
            if (cell->isMarked(GRAY))
                cellCallback(data, cell);
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

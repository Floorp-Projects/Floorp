/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "gc/GCInternals.h"
#include "js/HashTable.h"
#include "vm/JSCompartment.h"
#include "vm/Runtime.h"

#include "gc/PrivateIterators-inl.h"
#include "vm/JSContext-inl.h"

using namespace js;
using namespace js::gc;

static void
IterateRealmsArenasCellsUnbarriered(JSContext* cx, Zone* zone, void* data,
                                    JS::IterateRealmCallback realmCallback,
                                    IterateArenaCallback arenaCallback,
                                    IterateCellCallback cellCallback)
{
    {
        Rooted<Realm*> realm(cx);
        for (RealmsInZoneIter r(zone); !r.done(); r.next()) {
            realm = r;
            (*realmCallback)(cx, data, realm);
        }
    }

    for (auto thingKind : AllAllocKinds()) {
        JS::TraceKind traceKind = MapAllocToTraceKind(thingKind);
        size_t thingSize = Arena::thingSize(thingKind);

        for (ArenaIter aiter(zone, thingKind); !aiter.done(); aiter.next()) {
            Arena* arena = aiter.get();
            (*arenaCallback)(cx->runtime(), data, arena, traceKind, thingSize);
            for (ArenaCellIterUnbarriered iter(arena); !iter.done(); iter.next())
                (*cellCallback)(cx->runtime(), data, iter.getCell(), traceKind, thingSize);
        }
    }
}

void
js::IterateHeapUnbarriered(JSContext* cx, void* data,
                           IterateZoneCallback zoneCallback,
                           JS::IterateRealmCallback realmCallback,
                           IterateArenaCallback arenaCallback,
                           IterateCellCallback cellCallback)
{
    AutoPrepareForTracing prop(cx);

    for (ZonesIter zone(cx->runtime(), WithAtoms); !zone.done(); zone.next()) {
        (*zoneCallback)(cx->runtime(), data, zone);
        IterateRealmsArenasCellsUnbarriered(cx, zone, data,
                                            realmCallback, arenaCallback, cellCallback);
    }
}

void
js::IterateHeapUnbarrieredForZone(JSContext* cx, Zone* zone, void* data,
                                  IterateZoneCallback zoneCallback,
                                  JS::IterateRealmCallback realmCallback,
                                  IterateArenaCallback arenaCallback,
                                  IterateCellCallback cellCallback)
{
    AutoPrepareForTracing prop(cx);

    (*zoneCallback)(cx->runtime(), data, zone);
    IterateRealmsArenasCellsUnbarriered(cx, zone, data,
                                        realmCallback, arenaCallback, cellCallback);
}

void
js::IterateChunks(JSContext* cx, void* data, IterateChunkCallback chunkCallback)
{
    AutoPrepareForTracing prep(cx);
    AutoLockGC lock(cx->runtime());

    for (auto chunk = cx->runtime()->gc.allNonEmptyChunks(lock); !chunk.done(); chunk.next())
        chunkCallback(cx->runtime(), data, chunk);
}

void
js::IterateScripts(JSContext* cx, Realm* realm, void* data, IterateScriptCallback scriptCallback)
{
    MOZ_ASSERT(!cx->suppressGC);
    AutoEmptyNursery empty(cx);
    AutoPrepareForTracing prep(cx);
    JS::AutoSuppressGCAnalysis nogc;

    if (realm) {
        Zone* zone = realm->zone();
        for (auto script = zone->cellIter<JSScript>(empty); !script.done(); script.next()) {
            if (script->realm() == realm)
                scriptCallback(cx->runtime(), data, script, nogc);
        }
    } else {
        for (ZonesIter zone(cx->runtime(), SkipAtoms); !zone.done(); zone.next()) {
            for (auto script = zone->cellIter<JSScript>(empty); !script.done(); script.next())
                scriptCallback(cx->runtime(), data, script, nogc);
        }
    }
}

static void
IterateGrayObjects(Zone* zone, GCThingCallback cellCallback, void* data)
{
    for (auto kind : ObjectAllocKinds()) {
        for (GrayObjectIter obj(zone, kind); !obj.done(); obj.next()) {
            if (obj->asTenured().isMarkedGray())
                cellCallback(data, JS::GCCellPtr(obj.get()));
        }
    }
}

void
js::IterateGrayObjects(Zone* zone, GCThingCallback cellCallback, void* data)
{
    MOZ_ASSERT(!JS::CurrentThreadIsHeapBusy());
    AutoPrepareForTracing prep(TlsContext.get());
    ::IterateGrayObjects(zone, cellCallback, data);
}

void
js::IterateGrayObjectsUnderCC(Zone* zone, GCThingCallback cellCallback, void* data)
{
    mozilla::DebugOnly<JSRuntime*> rt = zone->runtimeFromMainThread();
    MOZ_ASSERT(JS::CurrentThreadIsHeapCycleCollecting());
    MOZ_ASSERT(!rt->gc.isIncrementalGCInProgress());
    ::IterateGrayObjects(zone, cellCallback, data);
}

JS_PUBLIC_API(void)
JS_IterateCompartments(JSContext* cx, void* data,
                       JSIterateCompartmentCallback compartmentCallback)
{
    AutoTraceSession session(cx->runtime());

    for (CompartmentsIter c(cx->runtime()); !c.done(); c.next())
        (*compartmentCallback)(cx, data, c);
}

JS_PUBLIC_API(void)
JS::IterateRealms(JSContext* cx, void* data, JS::IterateRealmCallback realmCallback)
{
    AutoTraceSession session(cx->runtime());

    Rooted<Realm*> realm(cx);
    for (RealmsIter r(cx->runtime()); !r.done(); r.next()) {
        realm = r;
        (*realmCallback)(cx, data, realm);
    }
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsweakmap.h"

#include <string.h>

#include "jsapi.h"
#include "jscntxt.h"
#include "jsfriendapi.h"
#include "jsobj.h"
#include "jswrapper.h"

#include "js/GCAPI.h"
#include "vm/GlobalObject.h"

#include "jsobjinlines.h"

using namespace js;
using namespace js::gc;

WeakMapBase::WeakMapBase(JSObject* memOf, Zone* zone)
  : memberOf(memOf),
    zone(zone),
    next(WeakMapNotInList),
    marked(false)
{
    MOZ_ASSERT_IF(memberOf, memberOf->compartment()->zone() == zone);
}

WeakMapBase::~WeakMapBase()
{
    MOZ_ASSERT(CurrentThreadIsGCSweeping() || CurrentThreadIsHandlingInitFailure());
    MOZ_ASSERT_IF(CurrentThreadIsGCSweeping(), !isInList());
    if (isInList())
        removeWeakMapFromList(this);
}

void
WeakMapBase::trace(JSTracer* tracer)
{
    MOZ_ASSERT(isInList());
    if (tracer->isMarkingTracer()) {
        marked = true;
        if (tracer->weakMapAction() == DoNotTraceWeakMaps) {
            // Do not trace any WeakMap entries at this time. Just record the
            // fact that the WeakMap has been marked. Entries are marked in the
            // iterative marking phase by markAllIteratively(), after as many
            // keys as possible have been marked already.
        } else {
            MOZ_ASSERT(tracer->weakMapAction() == ExpandWeakMaps);
            markEphemeronEntries(tracer);
        }
    } else {
        // If we're not actually doing garbage collection, the keys won't be marked
        // nicely as needed by the true ephemeral marking algorithm --- custom tracers
        // such as the cycle collector must use their own means for cycle detection.
        // So here we do a conservative approximation: pretend all keys are live.
        if (tracer->weakMapAction() == DoNotTraceWeakMaps)
            return;

        nonMarkingTraceValues(tracer);
        if (tracer->weakMapAction() == TraceWeakMapKeysValues)
            nonMarkingTraceKeys(tracer);
    }
}

void
WeakMapBase::unmarkZone(JS::Zone* zone)
{
    for (WeakMapBase* m = zone->gcWeakMapList; m; m = m->next)
        m->marked = false;
}

void
WeakMapBase::markAll(JS::Zone* zone, JSTracer* tracer)
{
    MOZ_ASSERT(tracer->weakMapAction() != DoNotTraceWeakMaps);
    for (WeakMapBase* m = zone->gcWeakMapList; m; m = m->next) {
        m->trace(tracer);
        if (m->memberOf)
            TraceEdge(tracer, &m->memberOf, "memberOf");
    }
}

bool
WeakMapBase::markZoneIteratively(JS::Zone* zone, JSTracer* tracer)
{
    bool markedAny = false;
    for (WeakMapBase* m = zone->gcWeakMapList; m; m = m->next) {
        if (m->marked && m->markIteratively(tracer))
            markedAny = true;
    }
    return markedAny;
}

bool
WeakMapBase::findInterZoneEdges(JS::Zone* zone)
{
    for (WeakMapBase* m = zone->gcWeakMapList; m; m = m->next) {
        if (!m->findZoneEdges())
            return false;
    }
    return true;
}

void
WeakMapBase::sweepZone(JS::Zone* zone)
{
    WeakMapBase** tailPtr = &zone->gcWeakMapList;
    for (WeakMapBase* m = zone->gcWeakMapList; m; ) {
        WeakMapBase* next = m->next;
        if (m->marked) {
            m->sweep();
            *tailPtr = m;
            tailPtr = &m->next;
        } else {
            /* Destroy the hash map now to catch any use after this point. */
            m->finish();
            m->next = WeakMapNotInList;
        }
        m = next;
    }
    *tailPtr = nullptr;

#ifdef DEBUG
    for (WeakMapBase* m = zone->gcWeakMapList; m; m = m->next)
        MOZ_ASSERT(m->isInList() && m->marked);
#endif
}

void
WeakMapBase::traceAllMappings(WeakMapTracer* tracer)
{
    JSRuntime* rt = tracer->runtime;
    for (ZonesIter zone(rt, SkipAtoms); !zone.done(); zone.next()) {
        for (WeakMapBase* m = zone->gcWeakMapList; m; m = m->next) {
            // The WeakMapTracer callback is not allowed to GC.
            JS::AutoSuppressGCAnalysis nogc;
            m->traceMappings(tracer);
        }
    }
}

bool
WeakMapBase::saveZoneMarkedWeakMaps(JS::Zone* zone, WeakMapSet& markedWeakMaps)
{
    for (WeakMapBase* m = zone->gcWeakMapList; m; m = m->next) {
        if (m->marked && !markedWeakMaps.put(m))
            return false;
    }
    return true;
}

void
WeakMapBase::restoreMarkedWeakMaps(WeakMapSet& markedWeakMaps)
{
    for (WeakMapSet::Range r = markedWeakMaps.all(); !r.empty(); r.popFront()) {
        WeakMapBase* map = r.front();
        MOZ_ASSERT(map->zone->isGCMarking());
        MOZ_ASSERT(!map->marked);
        map->marked = true;
    }
}

void
WeakMapBase::removeWeakMapFromList(WeakMapBase* weakmap)
{
    JS::Zone* zone = weakmap->zone;
    for (WeakMapBase** p = &zone->gcWeakMapList; *p; p = &(*p)->next) {
        if (*p == weakmap) {
            *p = (*p)->next;
            weakmap->next = WeakMapNotInList;
            break;
        }
    }
}

bool
ObjectValueMap::findZoneEdges()
{
    /*
     * For unmarked weakmap keys with delegates in a different zone, add a zone
     * edge to ensure that the delegate zone finishes marking before the key
     * zone.
     */
    JS::AutoSuppressGCAnalysis nogc;
    for (Range r = all(); !r.empty(); r.popFront()) {
        JSObject* key = r.front().key();
        if (key->asTenured().isMarked(BLACK) && !key->asTenured().isMarked(GRAY))
            continue;
        JSWeakmapKeyDelegateOp op = key->getClass()->ext.weakmapKeyDelegateOp;
        if (!op)
            continue;
        JSObject* delegate = op(key);
        if (!delegate)
            continue;
        Zone* delegateZone = delegate->zone();
        if (delegateZone == zone)
            continue;
        if (!delegateZone->gcZoneGroupEdges.put(key->zone()))
            return false;
    }
    return true;
}

ObjectWeakMap::ObjectWeakMap(JSContext* cx)
  : map(cx, nullptr)
{}

bool
ObjectWeakMap::init()
{
    return map.init();
}

ObjectWeakMap::~ObjectWeakMap()
{
    WeakMapBase::removeWeakMapFromList(&map);
}

JSObject*
ObjectWeakMap::lookup(const JSObject* obj)
{
    MOZ_ASSERT(map.initialized());
    if (ObjectValueMap::Ptr p = map.lookup(const_cast<JSObject*>(obj)))
        return &p->value().toObject();
    return nullptr;
}

bool
ObjectWeakMap::add(JSContext* cx, JSObject* obj, JSObject* target)
{
    MOZ_ASSERT(obj && target);
    MOZ_ASSERT(map.initialized());

    MOZ_ASSERT(!map.has(obj));
    if (!map.put(obj, ObjectValue(*target))) {
        ReportOutOfMemory(cx);
        return false;
    }
    if (IsInsideNursery(obj))
        cx->runtime()->gc.storeBuffer.putGeneric(StoreBufferRef(&map, obj));

    return true;
}

void
ObjectWeakMap::clear()
{
    MOZ_ASSERT(map.initialized());
    map.clear();
}

void
ObjectWeakMap::trace(JSTracer* trc)
{
    MOZ_ASSERT(map.initialized());
    map.trace(trc);
}

size_t
ObjectWeakMap::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    MOZ_ASSERT(map.initialized());
    return map.sizeOfExcludingThis(mallocSizeOf);
}

#ifdef JSGC_HASH_TABLE_CHECKS
void
ObjectWeakMap::checkAfterMovingGC()
{
    MOZ_ASSERT(map.initialized());
    for (ObjectValueMap::Range r = map.all(); !r.empty(); r.popFront()) {
        CheckGCThingAfterMovingGC(r.front().key().get());
        CheckGCThingAfterMovingGC(&r.front().value().toObject());
    }
}
#endif // JSGC_HASH_TABLE_CHECKS

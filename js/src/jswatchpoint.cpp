/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsatom.h"
#include "jswatchpoint.h"

#include "gc/Marking.h"

#include "jsobjinlines.h"

using namespace js;
using namespace js::gc;

inline HashNumber
DefaultHasher<WatchKey>::hash(const Lookup &key)
{
    return DefaultHasher<JSObject *>::hash(key.object.get()) ^ HashId(key.id.get());
}

class AutoEntryHolder {
    typedef WatchpointMap::Map Map;
    Map &map;
    Map::Ptr p;
    uint32_t gen;
    RootedVarObject obj;
    RootedVarId id;

  public:
    AutoEntryHolder(JSContext *cx, Map &map, Map::Ptr p)
        : map(map), p(p), gen(map.generation()), obj(cx, p->key.object), id(cx, p->key.id) {
        JS_ASSERT(!p->value.held);
        p->value.held = true;
    }

    ~AutoEntryHolder() {
        if (gen != map.generation())
            p = map.lookup(WatchKey(obj, id));
        if (p)
            p->value.held = false;
    }
};

bool
WatchpointMap::init()
{
    return map.init();
}

bool
WatchpointMap::watch(JSContext *cx, HandleObject obj, HandleId id,
                     JSWatchPointHandler handler, HandleObject closure)
{
    JS_ASSERT(JSID_IS_STRING(id) || JSID_IS_INT(id));

    if (!obj->setWatched(cx))
        return false;

    Watchpoint w;
    w.handler = handler;
    w.closure = closure;
    w.held = false;
    if (!map.put(WatchKey(obj, id), w)) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    return true;
}

void
WatchpointMap::unwatch(JSObject *obj, jsid id,
                       JSWatchPointHandler *handlerp, JSObject **closurep)
{
    if (Map::Ptr p = map.lookup(WatchKey(obj, id))) {
        if (handlerp)
            *handlerp = p->value.handler;
        if (closurep)
            *closurep = p->value.closure;
        map.remove(p);
    }
}

void
WatchpointMap::unwatchObject(JSObject *obj)
{
    for (Map::Enum r(map); !r.empty(); r.popFront()) {
        Map::Entry &e = r.front();
        if (e.key.object == obj)
            r.removeFront();
    }
}

void
WatchpointMap::clear()
{
    map.clear();
}

bool
WatchpointMap::triggerWatchpoint(JSContext *cx, HandleObject obj, HandleId id, Value *vp)
{
    Map::Ptr p = map.lookup(WatchKey(obj, id));
    if (!p || p->value.held)
        return true;

    AutoEntryHolder holder(cx, map, p);

    /* Copy the entry, since GC would invalidate p. */
    JSWatchPointHandler handler = p->value.handler;
    RootedVarObject closure(cx, p->value.closure);

    /* Determine the property's old value. */
    Value old;
    old.setUndefined();
    if (obj->isNative()) {
        if (const Shape *shape = obj->nativeLookup(cx, id)) {
            if (shape->hasSlot())
                old = obj->nativeGetSlot(shape->slot());
        }
    }

    /* Call the handler. */
    return handler(cx, obj, id, old, vp, closure);
}

bool
WatchpointMap::markAllIteratively(JSTracer *trc)
{
    JSRuntime *rt = trc->runtime;
    bool mutated = false;
    for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
        if (c->watchpointMap)
            mutated |= c->watchpointMap->markIteratively(trc);
    }
    return mutated;
}

bool
WatchpointMap::markIteratively(JSTracer *trc)
{
    bool marked = false;
    for (Map::Range r = map.all(); !r.empty(); r.popFront()) {
        Map::Entry &e = r.front();
        bool objectIsLive = !IsAboutToBeFinalized(e.key.object);
        if (objectIsLive || e.value.held) {
            if (!objectIsLive) {
                HeapPtrObject tmp(e.key.object);
                MarkObject(trc, &tmp, "held Watchpoint object");
                JS_ASSERT(tmp == e.key.object);
                marked = true;
            }

            const HeapId &id = e.key.id;
            JS_ASSERT(JSID_IS_STRING(id) || JSID_IS_INT(id));
            HeapId tmp(id.get());
            MarkId(trc, &tmp, "WatchKey::id");
            JS_ASSERT(tmp.get() == id.get());

            if (e.value.closure && IsAboutToBeFinalized(e.value.closure)) {
                MarkObject(trc, &e.value.closure, "Watchpoint::closure");
                marked = true;
            }
        }
    }
    return marked;
}

void
WatchpointMap::markAll(JSTracer *trc)
{
    for (Map::Range r = map.all(); !r.empty(); r.popFront()) {
        Map::Entry &e = r.front();
        HeapPtrObject tmpObj(e.key.object);
        MarkObject(trc, &tmpObj, "held Watchpoint object");
        JS_ASSERT(tmpObj == e.key.object);

        const HeapId &id = e.key.id;
        JS_ASSERT(JSID_IS_STRING(id) || JSID_IS_INT(id));
        HeapId tmpId(id.get());
        MarkId(trc, &tmpId, "WatchKey::id");
        JS_ASSERT(tmpId.get() == id.get());

        MarkObject(trc, &e.value.closure, "Watchpoint::closure");
    }
}

void
WatchpointMap::sweepAll(JSRuntime *rt)
{
    for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
        if (WatchpointMap *wpmap = c->watchpointMap)
            wpmap->sweep();
    }
}

void
WatchpointMap::sweep()
{
    for (Map::Enum r(map); !r.empty(); r.popFront()) {
        Map::Entry &e = r.front();
        if (IsAboutToBeFinalized(e.key.object)) {
            JS_ASSERT(!e.value.held);
            r.removeFront();
        }
    }
}

void
WatchpointMap::traceAll(WeakMapTracer *trc)
{
    JSRuntime *rt = trc->runtime;
    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c) {
        if (WatchpointMap *wpmap = (*c)->watchpointMap)
            wpmap->trace(trc);
    }
}

void
WatchpointMap::trace(WeakMapTracer *trc)
{
    for (Map::Range r = map.all(); !r.empty(); r.popFront()) {
        Map::Entry &e = r.front();
        trc->callback(trc, NULL,
                      e.key.object.get(), JSTRACE_OBJECT,
                      e.value.closure.get(), JSTRACE_OBJECT);
    }
}

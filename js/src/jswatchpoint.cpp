/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is an implementation of watchpoints for SpiderMonkey.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Orendorff <jorendorff@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    JS_ASSERT(id.value() == js_CheckForStringIndex(id));
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
    JS_ASSERT(id == js_CheckForStringIndex(id));
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
    JS_ASSERT(id.value() == js_CheckForStringIndex(id));
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

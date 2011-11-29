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

#include "jswatchpoint.h"
#include "jsatom.h"
#include "jsgcmark.h"
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
    uint32 gen;
    WatchKey key;

  public:
    AutoEntryHolder(Map &map, Map::Ptr p)
        : map(map), p(p), gen(map.generation()), key(p->key) {
        JS_ASSERT(!p->value.held);
        p->value.held = true;
    }

    ~AutoEntryHolder() {
        if (gen != map.generation())
            p = map.lookup(key);
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
WatchpointMap::watch(JSContext *cx, JSObject *obj, jsid id,
                     JSWatchPointHandler handler, JSObject *closure)
{
    JS_ASSERT(id == js_CheckForStringIndex(id));
    JS_ASSERT(JSID_IS_STRING(id) || JSID_IS_INT(id));

    obj->setWatched(cx);
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
WatchpointMap::triggerWatchpoint(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JS_ASSERT(id == js_CheckForStringIndex(id));
    Map::Ptr p = map.lookup(WatchKey(obj, id));
    if (!p || p->value.held)
        return true;

    AutoEntryHolder holder(map, p);

    /* Copy the entry, since GC would invalidate p. */
    JSWatchPointHandler handler = p->value.handler;
    JSObject *closure = p->value.closure;

    /* Determine the property's old value. */
    Value old;
    old.setUndefined();
    if (obj->isNative()) {
        if (const Shape *shape = obj->nativeLookup(cx, id)) {
            uint32 slot = shape->slot;
            if (obj->containsSlot(slot)) {
                if (shape->isMethod()) {
                    /*
                     * The existing watched property is a method. Trip
                     * the method read barrier in order to avoid
                     * passing an uncloned function object to the
                     * handler.
                     */
                    Value method = ObjectValue(shape->methodObject());
                    if (!obj->methodReadBarrier(cx, *shape, &method))
                        return false;
                    shape = obj->nativeLookup(cx, id);
                    JS_ASSERT(shape->isDataDescriptor());
                    JS_ASSERT(!shape->isMethod());
                    old = method;
                } else {
                    old = obj->nativeGetSlot(slot);
                }
            }
        }
    }

    /* Call the handler. */
    return handler(cx, obj, id, old, vp, closure);
}

bool
WatchpointMap::markAllIteratively(JSTracer *trc)
{
    JSRuntime *rt = trc->runtime;
    if (rt->gcCurrentCompartment) {
        WatchpointMap *wpmap = rt->gcCurrentCompartment->watchpointMap;
        return wpmap && wpmap->markIteratively(trc);
    }

    bool mutated = false;
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        if (c->watchpointMap)
            mutated |= c->watchpointMap->markIteratively(trc);
    }
    return mutated;
}

bool
WatchpointMap::markIteratively(JSTracer *trc)
{
    JSContext *cx = trc->context;
    bool marked = false;
    for (Map::Range r = map.all(); !r.empty(); r.popFront()) {
        Map::Entry &e = r.front();
        bool objectIsLive = !IsAboutToBeFinalized(cx, e.key.object);
        if (objectIsLive || e.value.held) {
            if (!objectIsLive) {
                MarkObject(trc, e.key.object, "held Watchpoint object");
                marked = true;
            }

            const HeapId &id = e.key.id;
            JS_ASSERT(JSID_IS_STRING(id) || JSID_IS_INT(id));
            MarkId(trc, id, "WatchKey::id");

            if (e.value.closure && IsAboutToBeFinalized(cx, e.value.closure)) {
                MarkObject(trc, e.value.closure, "Watchpoint::closure");
                marked = true;
            }
        }
    }
    return marked;
}

void
WatchpointMap::sweepAll(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    if (rt->gcCurrentCompartment) {
        if (WatchpointMap *wpmap = rt->gcCurrentCompartment->watchpointMap)
            wpmap->sweep(cx);
    } else {
        for (CompartmentsIter c(rt); !c.done(); c.next()) {
            if (WatchpointMap *wpmap = c->watchpointMap)
                wpmap->sweep(cx);
        }
    }
}

void
WatchpointMap::sweep(JSContext *cx)
{
    for (Map::Enum r(map); !r.empty(); r.popFront()) {
        Map::Entry &e = r.front();
        if (IsAboutToBeFinalized(cx, e.key.object)) {
            JS_ASSERT(!e.value.held);
            r.removeFront();
        }
    }
}

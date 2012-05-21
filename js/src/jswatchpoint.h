/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jswatchpoint_h___
#define jswatchpoint_h___

#include "jsalloc.h"
#include "jsprvtd.h"
#include "jsapi.h"
#include "jsfriendapi.h"

#include "gc/Barrier.h"
#include "js/HashTable.h"

namespace js {

struct WatchKey {
    WatchKey() {}
    WatchKey(JSObject *obj, jsid id) : object(obj), id(id) {}
    WatchKey(const WatchKey &key) : object(key.object.get()), id(key.id.get()) {}
    HeapPtrObject object;
    HeapId id;
};

struct Watchpoint {
    JSWatchPointHandler handler;
    HeapPtrObject closure;
    bool held;  /* true if currently running handler */
};

template <>
struct DefaultHasher<WatchKey> {
    typedef WatchKey Lookup;
    static inline js::HashNumber hash(const Lookup &key);

    static bool match(const WatchKey &k, const Lookup &l) {
        return k.object == l.object && k.id.get() == l.id.get();
    }
};

class WatchpointMap {
  public:
    typedef HashMap<WatchKey, Watchpoint, DefaultHasher<WatchKey>, SystemAllocPolicy> Map;

    bool init();
    bool watch(JSContext *cx, HandleObject obj, HandleId id,
               JSWatchPointHandler handler, HandleObject closure);
    void unwatch(JSObject *obj, jsid id,
                 JSWatchPointHandler *handlerp, JSObject **closurep);
    void unwatchObject(JSObject *obj);
    void clear();

    bool triggerWatchpoint(JSContext *cx, HandleObject obj, HandleId id, Value *vp);

    static bool markAllIteratively(JSTracer *trc);
    bool markIteratively(JSTracer *trc);
    void markAll(JSTracer *trc);
    static void sweepAll(JSRuntime *rt);
    void sweep();

    static void traceAll(WeakMapTracer *trc);
    void trace(WeakMapTracer *trc);

  private:
    Map map;
};

}

#endif /* jswatchpoint_h___ */

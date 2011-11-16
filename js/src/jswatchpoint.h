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

#ifndef jswatchpoint_h___
#define jswatchpoint_h___

#include "jsalloc.h"
#include "jsprvtd.h"
#include "jsapi.h"

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
    bool watch(JSContext *cx, JSObject *obj, jsid id,
               JSWatchPointHandler handler, JSObject *closure);
    void unwatch(JSObject *obj, jsid id,
                 JSWatchPointHandler *handlerp, JSObject **closurep);
    void unwatchObject(JSObject *obj);
    void clear();

    bool triggerWatchpoint(JSContext *cx, JSObject *obj, jsid id, Value *vp);

    static bool markAllIteratively(JSTracer *trc);
    bool markIteratively(JSTracer *trc);
    static void sweepAll(JSContext *cx);
    void sweep(JSContext *cx);

  private:
    Map map;
};

}

#endif /* jswatchpoint_h___ */

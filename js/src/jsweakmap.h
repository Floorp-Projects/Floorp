/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andreas Gal <gal@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef jsweakmap_h___
#define jsweakmap_h___

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jscntxt.h"
#include "jsobj.h"
#include "jsgcmark.h"

#include "js/HashTable.h"

namespace js {

// A subclass template of js::HashMap whose keys and values may be garbage-collected. When
// a key is collected, the table entry disappears, dropping its reference to the value.
//
// More precisely:
//
//     A WeakMap entry is collected if and only if either the WeakMap or the entry's key
//     is collected. If an entry is not collected, it remains in the WeakMap and it has a
//     strong reference to the value.
//
// You must call this table's 'mark' method when the object of which it is a part is
// reached by the garbage collection tracer. Once a table is known to be live, the
// implementation takes care of the iterative marking needed for weak tables and removing
// table entries when collection is complete.
//
// You may provide your own MarkPolicy class to specify how keys and values are marked; a
// policy template provides default definitions for some common key/value type
// combinations.
//
// Details:
//
// The interface is as for a js::HashMap, with the following additions:
//
// - You must call the WeakMap's 'trace' member function when you discover that the map is
//   part of a live object. (You'll typically call this from the containing type's 'trace'
//   function.)
//
// - There is no AllocPolicy parameter; these are used with our garbage collector, so
//   RuntimeAllocPolicy is hard-wired.
//
// - Optional fourth and fifth parameters are the MarkPolicies for the key and value type.
//   A MarkPolicy has the constructor:
//
//     MarkPolicy(JSTracer *)
//
//   and the following member functions:
//
//     bool isMarked(const Type &x)
//        Return true if x has been marked as live by the garbage collector.
//
//     bool mark(Type &x)
//        Return false if x is already marked. Otherwise, mark x and return true.
//
//   If omitted, the MarkPolicy parameter defaults to js::DefaultMarkPolicy<Type>,
//   a policy template with the obvious definitions for some typical
//   SpiderMonkey type combinations.

// The value for the next pointer for maps not in the map list.
static WeakMapBase * const WeakMapNotInList = reinterpret_cast<WeakMapBase *>(1);

typedef Vector<WeakMapBase *, 0, SystemAllocPolicy> WeakMapVector;

// Common base class for all WeakMap specializations. The collector uses this to call
// their markIteratively and sweep methods.
class WeakMapBase {
  public:
    WeakMapBase(JSObject *memOf) : memberOf(memOf), next(WeakMapNotInList) { }
    virtual ~WeakMapBase() { }

    void trace(JSTracer *tracer) {
        if (IS_GC_MARKING_TRACER(tracer)) {
            // We don't do anything with a WeakMap at trace time. Rather, we wait until as
            // many keys as possible have been marked, and add ourselves to the list of
            // known-live WeakMaps to be scanned in the iterative marking phase, by
            // markAllIteratively.
            JS_ASSERT(!tracer->eagerlyTraceWeakMaps);

            // Add ourselves to the list if we are not already in the list. We can already
            // be in the list if the weak map is marked more than once due delayed marking.
            if (next == WeakMapNotInList) {
                JSRuntime *rt = tracer->runtime;
                next = rt->gcWeakMapList;
                rt->gcWeakMapList = this;
            }
        } else {
            // If we're not actually doing garbage collection, the keys won't be marked
            // nicely as needed by the true ephemeral marking algorithm --- custom tracers
            // such as the cycle collector must use their own means for cycle detection.
            // So here we do a conservative approximation: pretend all keys are live.
            if (tracer->eagerlyTraceWeakMaps)
                nonMarkingTrace(tracer);
        }
    }

    // Garbage collector entry points.

    // Check all weak maps that have been marked as live so far in this garbage
    // collection, and mark the values of all entries that have become strong references
    // to them. Return true if we marked any new values, indicating that we need to make
    // another pass. In other words, mark my marked maps' marked members' mid-collection.
    static bool markAllIteratively(JSTracer *tracer);

    // Remove entries whose keys are dead from all weak maps marked as live in this
    // garbage collection.
    static void sweepAll(JSTracer *tracer);

    // Trace all delayed weak map bindings. Used by the cycle collector.
    static void traceAllMappings(WeakMapTracer *tracer);

    void check() { JS_ASSERT(next == WeakMapNotInList); }

    // Remove everything from the live weak map list.
    static void resetWeakMapList(JSRuntime *rt);

    // Save and restore the live weak map list to a vector.
    static bool saveWeakMapList(JSRuntime *rt, WeakMapVector &vector);
    static void restoreWeakMapList(JSRuntime *rt, WeakMapVector &vector);

  protected:
    // Instance member functions called by the above. Instantiations of WeakMap override
    // these with definitions appropriate for their Key and Value types.
    virtual void nonMarkingTrace(JSTracer *tracer) = 0;
    virtual bool markIteratively(JSTracer *tracer) = 0;
    virtual void sweep(JSTracer *tracer) = 0;
    virtual void traceMappings(WeakMapTracer *tracer) = 0;

    // Object that this weak map is part of, if any.
    JSObject *memberOf;

  private:
    // Link in a list of WeakMaps to mark iteratively and sweep in this garbage
    // collection, headed by JSRuntime::gcWeakMapList. The last element of the list
    // has NULL as its next. Maps not in the list have WeakMapNotInList as their
    // next.  We must distinguish these cases to avoid creating infinite lists
    // when a weak map gets traced twice due to delayed marking.
    WeakMapBase *next;
};

template <class Key, class Value,
          class HashPolicy = DefaultHasher<Key> >
class WeakMap : public HashMap<Key, Value, HashPolicy, RuntimeAllocPolicy>, public WeakMapBase {
  private:
    typedef HashMap<Key, Value, HashPolicy, RuntimeAllocPolicy> Base;
    typedef typename Base::Enum Enum;

  public:
    typedef typename Base::Range Range;

    explicit WeakMap(JSRuntime *rt, JSObject *memOf=NULL) : Base(rt), WeakMapBase(memOf) { }
    explicit WeakMap(JSContext *cx, JSObject *memOf=NULL) : Base(cx), WeakMapBase(memOf) { }

    /* Use with caution, as result can be affected by garbage collection. */
    Range nondeterministicAll() {
        return Base::all();
    }

  private:
    bool markValue(JSTracer *trc, Value *x) {
        if (gc::IsMarked(*x))
            return false;
        gc::Mark(trc, x, "WeakMap entry");
        return true;
    }

    void nonMarkingTrace(JSTracer *trc) {
        for (Range r = Base::all(); !r.empty(); r.popFront())
            markValue(trc, &r.front().value);
    }

    bool markIteratively(JSTracer *trc) {
        bool markedAny = false;
        for (Range r = Base::all(); !r.empty(); r.popFront()) {
            /* If the entry is live, ensure its key and value are marked. */
            if (gc::IsMarked(r.front().key) && markValue(trc, &r.front().value))
                markedAny = true;
            JS_ASSERT_IF(gc::IsMarked(r.front().key), gc::IsMarked(r.front().value));
        }
        return markedAny;
    }

    void sweep(JSTracer *trc) {
        /* Remove all entries whose keys remain unmarked. */
        for (Enum e(*this); !e.empty(); e.popFront()) {
            if (!gc::IsMarked(e.front().key))
                e.removeFront();
        }

#if DEBUG
        /*
         * Once we've swept, all remaining edges should stay within the
         * known-live part of the graph.
         */
        for (Range r = Base::all(); !r.empty(); r.popFront()) {
            JS_ASSERT(gc::IsMarked(r.front().key));
            JS_ASSERT(gc::IsMarked(r.front().value));
        }
#endif
    }

    /* memberOf can be NULL, which means that the map is not part of a JSObject. */
    void traceMappings(WeakMapTracer *tracer) {
        for (Range r = Base::all(); !r.empty(); r.popFront()) {
            gc::Cell *key = gc::ToMarkable(r.front().key);
            gc::Cell *value = gc::ToMarkable(r.front().value);
            if (key && value) {
                tracer->callback(tracer, memberOf,
                                 key, gc::TraceKind(r.front().key),
                                 value, gc::TraceKind(r.front().value));
            }
        }
    }
};

} /* namespace js */

extern JSObject *
js_InitWeakMapClass(JSContext *cx, JSObject *obj);

#endif

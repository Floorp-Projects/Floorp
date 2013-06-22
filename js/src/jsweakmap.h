/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsweakmap_h
#define jsweakmap_h

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jscompartment.h"
#include "jsobj.h"

#include "gc/Marking.h"
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
// You must call this table's 'trace' method when the object of which it is a part is
// reached by the garbage collection tracer. Once a table is known to be live, the
// implementation takes care of the iterative marking needed for weak tables and removing
// table entries when collection is complete.

// The value for the next pointer for maps not in the map list.
static WeakMapBase * const WeakMapNotInList = reinterpret_cast<WeakMapBase *>(1);

typedef Vector<WeakMapBase *, 0, SystemAllocPolicy> WeakMapVector;

// Common base class for all WeakMap specializations. The collector uses this to call
// their markIteratively and sweep methods.
class WeakMapBase {
  public:
    WeakMapBase(JSObject *memOf, JSCompartment *c);
    virtual ~WeakMapBase();

    void trace(JSTracer *tracer) {
        if (IS_GC_MARKING_TRACER(tracer)) {
            // We don't do anything with a WeakMap at trace time. Rather, we wait until as
            // many keys as possible have been marked, and add ourselves to the list of
            // known-live WeakMaps to be scanned in the iterative marking phase, by
            // markAllIteratively.
            JS_ASSERT(tracer->eagerlyTraceWeakMaps == DoNotTraceWeakMaps);

            // Add ourselves to the list if we are not already in the list. We can already
            // be in the list if the weak map is marked more than once due delayed marking.
            if (next == WeakMapNotInList) {
                next = compartment->gcWeakMapList;
                compartment->gcWeakMapList = this;
            }
        } else {
            // If we're not actually doing garbage collection, the keys won't be marked
            // nicely as needed by the true ephemeral marking algorithm --- custom tracers
            // such as the cycle collector must use their own means for cycle detection.
            // So here we do a conservative approximation: pretend all keys are live.
            if (tracer->eagerlyTraceWeakMaps == DoNotTraceWeakMaps)
                return;

            nonMarkingTraceValues(tracer);
            if (tracer->eagerlyTraceWeakMaps == TraceWeakMapKeysValues)
                nonMarkingTraceKeys(tracer);
        }
    }

    // Garbage collector entry points.

    // Check all weak maps in a compartment that have been marked as live in this garbage
    // collection, and mark the values of all entries that have become strong references
    // to them. Return true if we marked any new values, indicating that we need to make
    // another pass. In other words, mark my marked maps' marked members' mid-collection.
    static bool markCompartmentIteratively(JSCompartment *c, JSTracer *tracer);

    // Remove entries whose keys are dead from all weak maps in a compartment marked as
    // live in this garbage collection.
    static void sweepCompartment(JSCompartment *c);

    // Trace all delayed weak map bindings. Used by the cycle collector.
    static void traceAllMappings(WeakMapTracer *tracer);

    void check() { JS_ASSERT(next == WeakMapNotInList); }

    // Remove everything from the weak map list for a compartment.
    static void resetCompartmentWeakMapList(JSCompartment *c);

    // Save the live weak map list for a compartment, appending the data to a vector.
    static bool saveCompartmentWeakMapList(JSCompartment *c, WeakMapVector &vector);

    // Restore live weak map lists for multiple compartments from a vector.
    static void restoreCompartmentWeakMapLists(WeakMapVector &vector);

    // Remove a weakmap from the live weakmaps list
    static void removeWeakMapFromList(WeakMapBase *weakmap);

  protected:
    // Instance member functions called by the above. Instantiations of WeakMap override
    // these with definitions appropriate for their Key and Value types.
    virtual void nonMarkingTraceKeys(JSTracer *tracer) = 0;
    virtual void nonMarkingTraceValues(JSTracer *tracer) = 0;
    virtual bool markIteratively(JSTracer *tracer) = 0;
    virtual void sweep() = 0;
    virtual void traceMappings(WeakMapTracer *tracer) = 0;

    // Object that this weak map is part of, if any.
    JSObject *memberOf;

    // Compartment that this weak map is part of.
    JSCompartment *compartment;

  private:
    // Link in a list of WeakMaps to mark iteratively and sweep in this garbage
    // collection, headed by JSCompartment::gcWeakMapList. The last element of
    // the list has NULL as its next. Maps not in the list have WeakMapNotInList
    // as their next.  We must distinguish these cases to avoid creating
    // infinite lists when a weak map gets traced twice due to delayed marking.
    WeakMapBase *next;
};

template <class Key, class Value,
          class HashPolicy = DefaultHasher<Key> >
class WeakMap : public HashMap<Key, Value, HashPolicy, RuntimeAllocPolicy>, public WeakMapBase
{
  public:
    typedef HashMap<Key, Value, HashPolicy, RuntimeAllocPolicy> Base;
    typedef typename Base::Enum Enum;
    typedef typename Base::Range Range;

    explicit WeakMap(JSContext *cx, JSObject *memOf=NULL)
        : Base(cx), WeakMapBase(memOf, cx->compartment()) { }

  private:
    bool markValue(JSTracer *trc, Value *x) {
        if (gc::IsMarked(x))
            return false;
        gc::Mark(trc, x, "WeakMap entry");
        JS_ASSERT(gc::IsMarked(x));
        return true;
    }

    void nonMarkingTraceKeys(JSTracer *trc) {
        for (Enum e(*this); !e.empty(); e.popFront()) {
            Key key(e.front().key);
            gc::Mark(trc, &key, "WeakMap Key");
            if (key != e.front().key)
                e.rekeyFront(key, key);
        }
    }

    void nonMarkingTraceValues(JSTracer *trc) {
        for (Range r = Base::all(); !r.empty(); r.popFront())
            gc::Mark(trc, &r.front().value, "WeakMap entry");
    }

    bool keyNeedsMark(JSObject *key) {
        if (JSWeakmapKeyDelegateOp op = key->getClass()->ext.weakmapKeyDelegateOp) {
            JSObject *delegate = op(key);
            /*
             * Check if the delegate is marked with any color to properly handle
             * gray marking when the key's delegate is black and the map is
             * gray.
             */
            return delegate && gc::IsObjectMarked(&delegate);
        }
        return false;
    }

    bool keyNeedsMark(gc::Cell *cell) {
        return false;
    }

    bool markIteratively(JSTracer *trc) {
        bool markedAny = false;
        for (Enum e(*this); !e.empty(); e.popFront()) {
            /* If the entry is live, ensure its key and value are marked. */
            Key prior(e.front().key);
            if (gc::IsMarked(const_cast<Key *>(&e.front().key))) {
                if (markValue(trc, &e.front().value))
                    markedAny = true;
                if (prior != e.front().key)
                    e.rekeyFront(e.front().key);
            } else if (keyNeedsMark(e.front().key)) {
                gc::Mark(trc, const_cast<Key *>(&e.front().key), "proxy-preserved WeakMap key");
                if (prior != e.front().key)
                    e.rekeyFront(e.front().key);
                gc::Mark(trc, &e.front().value, "WeakMap entry");
                markedAny = true;
            }
        }
        return markedAny;
    }

    void sweep() {
        /* Remove all entries whose keys remain unmarked. */
        for (Enum e(*this); !e.empty(); e.popFront()) {
            Key k(e.front().key);
            if (gc::IsAboutToBeFinalized(&k))
                e.removeFront();
            else if (k != e.front().key)
                e.rekeyFront(k, k);
        }
        /*
         * Once we've swept, all remaining edges should stay within the
         * known-live part of the graph.
         */
        assertEntriesNotAboutToBeFinalized();
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

protected:
    void assertEntriesNotAboutToBeFinalized() {
#if DEBUG
        for (Range r = Base::all(); !r.empty(); r.popFront()) {
            Key k(r.front().key);
            JS_ASSERT(!gc::IsAboutToBeFinalized(&k));
            JS_ASSERT(!gc::IsAboutToBeFinalized(&r.front().value));
            JS_ASSERT(k == r.front().key);
        }
#endif
    }
};

} /* namespace js */

extern JSObject *
js_InitWeakMapClass(JSContext *cx, js::HandleObject obj);

#endif /* jsweakmap_h */

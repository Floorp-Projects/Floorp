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
//     bool mark(const Type &x)
//        Return false if x is already marked. Otherwise, mark x and return true.
//
//   If omitted, the MarkPolicy parameter defaults to js::DefaultMarkPolicy<Type>,
//   a policy template with the obvious definitions for some typical
//   SpiderMonkey type combinations.

// A policy template holding default marking algorithms for common type combinations. This
// provides default types for WeakMap's MarkPolicy template parameter.
template <class Type> class DefaultMarkPolicy;

// Common base class for all WeakMap specializations. The collector uses this to call
// their markIteratively and sweep methods.
class WeakMapBase {
  public:
    WeakMapBase() : next(NULL) { }
    virtual ~WeakMapBase() { }

    void trace(JSTracer *tracer) {
        if (IS_GC_MARKING_TRACER(tracer)) {
            // We don't do anything with a WeakMap at trace time. Rather, we wait until as
            // many keys as possible have been marked, and add ourselves to the list of
            // known-live WeakMaps to be scanned in the iterative marking phase, by
            // markAllIteratively.
            JS_ASSERT(!tracer->eagerlyTraceWeakMaps);
            JSRuntime *rt = tracer->context->runtime;
            next = rt->gcWeakMapList;
            rt->gcWeakMapList = this;
        } else {
            // If we're not actually doing garbage collection, the keys won't be marked
            // nicely as needed by the true ephemeral marking algorithm --- custom tracers
            // must use their own means for cycle detection. So here we do a conservative
            // approximation: pretend all keys are live.
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

  protected:
    // Instance member functions called by the above. Instantiations of WeakMap override
    // these with definitions appropriate for their Key and Value types.
    virtual void nonMarkingTrace(JSTracer *tracer) = 0;
    virtual bool markIteratively(JSTracer *tracer) = 0;
    virtual void sweep(JSTracer *tracer) = 0;

  private:
    // Link in a list of WeakMaps to mark iteratively and sweep in this garbage
    // collection, headed by JSRuntime::gcWeakMapList.
    WeakMapBase *next;
};

template <class Key, class Value,
          class HashPolicy = DefaultHasher<Key>,
          class KeyMarkPolicy = DefaultMarkPolicy<Key>,
          class ValueMarkPolicy = DefaultMarkPolicy<Value> >
class WeakMap : public HashMap<Key, Value, HashPolicy, RuntimeAllocPolicy>, public WeakMapBase {
  private:
    typedef HashMap<Key, Value, HashPolicy, RuntimeAllocPolicy> Base;
    typedef typename Base::Enum Enum;

  public:
    typedef typename Base::Range Range;

    explicit WeakMap(JSRuntime *rt) : Base(rt) { }
    explicit WeakMap(JSContext *cx) : Base(cx) { }

    // Use with caution, as result can be affected by garbage collection.
    Range nondeterministicAll() {
        return Base::all();
    }

  private:
    void nonMarkingTrace(JSTracer *trc) {
        ValueMarkPolicy vp(trc);
        for (Range r = Base::all(); !r.empty(); r.popFront())
            vp.mark(r.front().value);
    }

    bool markIteratively(JSTracer *trc) {
        KeyMarkPolicy kp(trc);
        ValueMarkPolicy vp(trc);
        bool markedAny = false;
        for (Range r = Base::all(); !r.empty(); r.popFront()) {
            const Key &k = r.front().key;
            const Value &v = r.front().value;
            /* If the entry is live, ensure its key and value are marked. */
            if (kp.isMarked(k)) {
                markedAny |= vp.mark(v);
            } else if (kp.overrideKeyMarking(k)) {
                // We always mark wrapped natives.  This will cause leaks, but WeakMap+CC
                // integration is currently busted anyways.  When WeakMap+CC integration is
                // fixed in Bug 668855, XPC wrapped natives should only be marked during
                // non-BLACK marking (ie grey marking).
                kp.mark(k);
                vp.mark(v);
                markedAny = true;
            }
            JS_ASSERT_IF(kp.isMarked(k), vp.isMarked(v));
        }
        return markedAny;
    }

    void sweep(JSTracer *trc) {
        KeyMarkPolicy kp(trc);

        /* Remove all entries whose keys remain unmarked. */
        for (Enum e(*this); !e.empty(); e.popFront()) {
            if (!kp.isMarked(e.front().key))
                e.removeFront();
        }

#if DEBUG
        ValueMarkPolicy vp(trc);
        /*
         * Once we've swept, all remaining edges should stay within the
         * known-live part of the graph.
         */
        for (Range r = Base::all(); !r.empty(); r.popFront()) {
            JS_ASSERT(kp.isMarked(r.front().key));
            JS_ASSERT(vp.isMarked(r.front().value));
        }
#endif
    }
};

template <>
class DefaultMarkPolicy<HeapValue> {
  private:
    JSTracer *tracer;
  public:
    DefaultMarkPolicy(JSTracer *t) : tracer(t) { }
    bool isMarked(const HeapValue &x) {
        if (x.isMarkable())
            return !IsAboutToBeFinalized(tracer->context, x);
        return true;
    }
    bool mark(const HeapValue &x) {
        if (isMarked(x))
            return false;
        js::gc::MarkValue(tracer, x, "WeakMap entry");
        return true;
    }
    bool overrideKeyMarking(const HeapValue &k) { return false; }
};

template <>
class DefaultMarkPolicy<HeapPtrObject> {
  private:
    JSTracer *tracer;
  public:
    DefaultMarkPolicy(JSTracer *t) : tracer(t) { }
    bool isMarked(const HeapPtrObject &x) {
        return !IsAboutToBeFinalized(tracer->context, x);
    }
    bool mark(const HeapPtrObject &x) {
        if (isMarked(x))
            return false;
        js::gc::MarkObject(tracer, x, "WeakMap entry");
        return true;
    }
    bool overrideKeyMarking(const HeapPtrObject &k) {
        // We only need to worry about extra marking of keys when
        // we're doing a GC marking pass.
        if (!IS_GC_MARKING_TRACER(tracer))
            return false;
        return k->getClass()->ext.isWrappedNative;
    }
};

template <>
class DefaultMarkPolicy<HeapPtrScript> {
  private:
    JSTracer *tracer;
  public:
    DefaultMarkPolicy(JSTracer *t) : tracer(t) { }
    bool isMarked(const HeapPtrScript &x) {
        return !IsAboutToBeFinalized(tracer->context, x);
    }
    bool mark(const HeapPtrScript &x) {
        if (isMarked(x))
            return false;
        js::gc::MarkScript(tracer, x, "WeakMap entry");
        return true;
    }
    bool overrideKeyMarking(const HeapPtrScript &k) { return false; }
};

}

extern JSObject *
js_InitWeakMapClass(JSContext *cx, JSObject *obj);

#endif

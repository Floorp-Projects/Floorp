/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsweakmap_h
#define jsweakmap_h

#include "mozilla/Move.h"

#include "jscompartment.h"
#include "jsfriendapi.h"
#include "jsobj.h"

#include "gc/Marking.h"
#include "gc/StoreBuffer.h"
#include "js/HashTable.h"

namespace js {

class WeakMapBase;

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
static WeakMapBase * const WeakMapNotInList = reinterpret_cast<WeakMapBase*>(1);

typedef HashSet<WeakMapBase*, DefaultHasher<WeakMapBase*>, SystemAllocPolicy> WeakMapSet;

// Common base class for all WeakMap specializations. The collector uses this to call
// their markIteratively and sweep methods.
class WeakMapBase {
    friend void js::GCMarker::enterWeakMarkingMode();

  public:
    WeakMapBase(JSObject* memOf, JS::Zone* zone);
    virtual ~WeakMapBase();

    void trace(JSTracer* tracer);

    // Garbage collector entry points.

    // Unmark all weak maps in a zone.
    static void unmarkZone(JS::Zone* zone);

    // Mark all the weakmaps in a zone.
    static void markAll(JS::Zone* zone, JSTracer* tracer);

    // Check all weak maps in a zone that have been marked as live in this garbage
    // collection, and mark the values of all entries that have become strong references
    // to them. Return true if we marked any new values, indicating that we need to make
    // another pass. In other words, mark my marked maps' marked members' mid-collection.
    static bool markZoneIteratively(JS::Zone* zone, JSTracer* tracer);

    // Add zone edges for weakmaps with key delegates in a different zone.
    static bool findInterZoneEdges(JS::Zone* zone);

    // Sweep the weak maps in a zone, removing dead weak maps and removing
    // entries of live weak maps whose keys are dead.
    static void sweepZone(JS::Zone* zone);

    // Trace all delayed weak map bindings. Used by the cycle collector.
    static void traceAllMappings(WeakMapTracer* tracer);

    bool isInList() { return next != WeakMapNotInList; }

    // Save information about which weak maps are marked for a zone.
    static bool saveZoneMarkedWeakMaps(JS::Zone* zone, WeakMapSet& markedWeakMaps);

    // Restore information about which weak maps are marked for many zones.
    static void restoreMarkedWeakMaps(WeakMapSet& markedWeakMaps);

    // Remove a weakmap from its zone's weakmaps list.
    static void removeWeakMapFromList(WeakMapBase* weakmap);

    // Any weakmap key types that want to participate in the non-iterative
    // ephemeron marking must override this method.
    virtual void maybeMarkEntry(JSTracer* trc, gc::Cell* markedCell, JS::GCCellPtr l) = 0;

    virtual void markEphemeronEntries(JSTracer* trc) = 0;

  protected:
    // Instance member functions called by the above. Instantiations of WeakMap override
    // these with definitions appropriate for their Key and Value types.
    virtual void nonMarkingTraceKeys(JSTracer* tracer) = 0;
    virtual void nonMarkingTraceValues(JSTracer* tracer) = 0;
    virtual bool markIteratively(JSTracer* tracer) = 0;
    virtual bool findZoneEdges() = 0;
    virtual void sweep() = 0;
    virtual void traceMappings(WeakMapTracer* tracer) = 0;
    virtual void finish() = 0;

    // Object that this weak map is part of, if any.
    HeapPtrObject memberOf;

    // Zone containing this weak map.
    JS::Zone* zone;

    // Link in a list of all WeakMaps in a Zone, headed by
    // JS::Zone::gcWeakMapList. The last element of the list has nullptr as its
    // next. Maps not in the list have WeakMapNotInList as their next.
    WeakMapBase* next;

    // Whether this object has been traced during garbage collection.
    bool marked;
};

template <typename T>
static T extractUnbarriered(BarrieredBase<T> v)
{
    return v.get();
}
template <typename T>
static T* extractUnbarriered(T* v)
{
    return v;
}

template <class Key, class Value,
          class HashPolicy = DefaultHasher<Key> >
class WeakMap : public HashMap<Key, Value, HashPolicy, RuntimeAllocPolicy>, public WeakMapBase
{
  public:
    typedef HashMap<Key, Value, HashPolicy, RuntimeAllocPolicy> Base;
    typedef typename Base::Enum Enum;
    typedef typename Base::Lookup Lookup;
    typedef typename Base::Range Range;
    typedef typename Base::Ptr Ptr;
    typedef typename Base::AddPtr AddPtr;

    explicit WeakMap(JSContext* cx, JSObject* memOf = nullptr)
        : Base(cx->runtime()), WeakMapBase(memOf, cx->compartment()->zone()) { }

    bool init(uint32_t len = 16) {
        if (!Base::init(len))
            return false;
        next = zone->gcWeakMapList;
        zone->gcWeakMapList = this;
        marked = JS::IsIncrementalGCInProgress(zone->runtimeFromMainThread());
        return true;
    }

    // Overwritten to add a read barrier to prevent an incorrectly gray value
    // from escaping the weak map. See the comment before UnmarkGrayChildren in
    // gc/Marking.cpp
    Ptr lookup(const Lookup& l) const {
        Ptr p = Base::lookup(l);
        if (p)
            exposeGCThingToActiveJS(p->value());
        return p;
    }

    AddPtr lookupForAdd(const Lookup& l) const {
        AddPtr p = Base::lookupForAdd(l);
        if (p)
            exposeGCThingToActiveJS(p->value());
        return p;
    }

    Ptr lookupWithDefault(const Key& k, const Value& defaultValue) {
        Ptr p = Base::lookupWithDefault(k, defaultValue);
        if (p)
            exposeGCThingToActiveJS(p->value());
        return p;
    }

    // The WeakMap and some part of the key are marked. If the entry is marked
    // according to the exact semantics of this WeakMap, then mark the value.
    // (For a standard WeakMap, the entry is marked if either the key its
    // delegate is marked.)
    void maybeMarkEntry(JSTracer* trc, gc::Cell* markedCell, JS::GCCellPtr origKey) override
    {
        MOZ_ASSERT(marked);

        gc::Cell* l = origKey.asCell();
        Ptr p = Base::lookup(reinterpret_cast<Lookup&>(l));
        MOZ_ASSERT(p.found());

        Key key(p->key());
        if (gc::IsMarked(&key)) {
            TraceEdge(trc, &p->value(), "ephemeron value");
        } else if (keyNeedsMark(key)) {
            TraceEdge(trc, &p->value(), "WeakMap ephemeron value");
            TraceEdge(trc, &key, "proxy-preserved WeakMap ephemeron key");
            MOZ_ASSERT(key == p->key()); // No moving
        }
        key.unsafeSet(nullptr); // Prevent destructor from running barriers.
    }

  protected:
    static void addWeakEntry(JSTracer* trc, JS::GCCellPtr key, gc::WeakMarkable markable)
    {
        GCMarker& marker = *static_cast<GCMarker*>(trc);

        auto p = marker.weakKeys.get(key);
        if (p) {
            gc::WeakEntryVector& weakEntries = p->value;
            if (!weakEntries.append(Move(markable)))
                marker.abortLinearWeakMarking();
        } else {
            gc::WeakEntryVector weakEntries;
            MOZ_ALWAYS_TRUE(weakEntries.append(Move(markable)));
            if (!marker.weakKeys.put(JS::GCCellPtr(key), Move(weakEntries)))
                marker.abortLinearWeakMarking();
        }
    }

    void markEphemeronEntries(JSTracer* trc) override {
        MOZ_ASSERT(marked);
        for (Enum e(*this); !e.empty(); e.popFront()) {
            Key key(e.front().key());

            // If the entry is live, ensure its key and value are marked.
            if (gc::IsMarked(&key)) {
                (void) markValue(trc, &e.front().value());
                MOZ_ASSERT(key == e.front().key()); // No moving
            } else if (keyNeedsMark(key)) {
                TraceEdge(trc, &e.front().value(), "WeakMap entry value");
                TraceEdge(trc, &key, "proxy-preserved WeakMap entry key");
                MOZ_ASSERT(key == e.front().key()); // No moving
            } else if (trc->isWeakMarkingTracer()) {
                // Entry is not yet known to be live. Record it in the list of
                // weak keys. Or rather, record this weakmap and the lookup key
                // so we can repeat the lookup when we need to (to allow
                // incremental weak marking, we can't just store a pointer to
                // the entry.) Also record the delegate, if any, because
                // marking the delegate must also mark the entry.
                JS::GCCellPtr weakKey(extractUnbarriered(key));
                gc::WeakMarkable markable(this, weakKey);
                addWeakEntry(trc, weakKey, markable);
                if (JSObject* delegate = getDelegate(key))
                    addWeakEntry(trc, JS::GCCellPtr(delegate), markable);
            }
            key.unsafeSet(nullptr); // Prevent destructor from running barriers.
        }
    }

  private:
    void exposeGCThingToActiveJS(const JS::Value& v) const { JS::ExposeValueToActiveJS(v); }
    void exposeGCThingToActiveJS(JSObject* obj) const { JS::ExposeObjectToActiveJS(obj); }

    bool markValue(JSTracer* trc, Value* x) {
        if (gc::IsMarked(x))
            return false;
        TraceEdge(trc, x, "WeakMap entry value");
        MOZ_ASSERT(gc::IsMarked(x));
        return true;
    }

    void nonMarkingTraceKeys(JSTracer* trc) override {
        for (Enum e(*this); !e.empty(); e.popFront()) {
            Key key(e.front().key());
            TraceEdge(trc, &key, "WeakMap entry key");
            if (key != e.front().key())
                entryMoved(e, key);
        }
    }

    void nonMarkingTraceValues(JSTracer* trc) override {
        for (Range r = Base::all(); !r.empty(); r.popFront())
            TraceEdge(trc, &r.front().value(), "WeakMap entry value");
    }

    JSObject* getDelegate(JSObject* key) const {
        JSWeakmapKeyDelegateOp op = key->getClass()->ext.weakmapKeyDelegateOp;
        return op ? op(key) : nullptr;
    }

    JSObject* getDelegate(gc::Cell* cell) const {
        return nullptr;
    }

    bool keyNeedsMark(JSObject* key) const {
        JSObject* delegate = getDelegate(key);
        /*
         * Check if the delegate is marked with any color to properly handle
         * gray marking when the key's delegate is black and the map is gray.
         */
        return delegate && gc::IsMarkedUnbarriered(&delegate);
    }

    bool keyNeedsMark(gc::Cell* cell) const {
        return false;
    }

    bool markIteratively(JSTracer* trc) override {
        bool markedAny = false;
        for (Enum e(*this); !e.empty(); e.popFront()) {
            /* If the entry is live, ensure its key and value are marked. */
            Key key(e.front().key());
            if (gc::IsMarked(const_cast<Key*>(&key))) {
                if (markValue(trc, &e.front().value()))
                    markedAny = true;
                if (e.front().key() != key)
                    entryMoved(e, key);
            } else if (keyNeedsMark(key)) {
                TraceEdge(trc, &e.front().value(), "WeakMap entry value");
                TraceEdge(trc, &key, "proxy-preserved WeakMap entry key");
                if (e.front().key() != key)
                    entryMoved(e, key);
                markedAny = true;
            }
            key.unsafeSet(nullptr); // Prevent destructor from running barriers.
        }
        return markedAny;
    }

    bool findZoneEdges() override {
        // This is overridden by ObjectValueMap.
        return true;
    }

    void sweep() override {
        /* Remove all entries whose keys remain unmarked. */
        for (Enum e(*this); !e.empty(); e.popFront()) {
            Key k(e.front().key());
            if (gc::IsAboutToBeFinalized(&k))
                e.removeFront();
            else if (k != e.front().key())
                entryMoved(e, k);
        }
        /*
         * Once we've swept, all remaining edges should stay within the
         * known-live part of the graph.
         */
        assertEntriesNotAboutToBeFinalized();
    }

    void finish() override {
        Base::finish();
    }

    /* memberOf can be nullptr, which means that the map is not part of a JSObject. */
    void traceMappings(WeakMapTracer* tracer) override {
        for (Range r = Base::all(); !r.empty(); r.popFront()) {
            gc::Cell* key = gc::ToMarkable(r.front().key());
            gc::Cell* value = gc::ToMarkable(r.front().value());
            if (key && value) {
                tracer->trace(memberOf,
                              JS::GCCellPtr(r.front().key().get()),
                              JS::GCCellPtr(r.front().value().get()));
            }
        }
    }

    /* Rekey an entry when moved, ensuring we do not trigger barriers. */
    void entryMoved(Enum& e, const Key& k) {
        e.rekeyFront(k);
    }

  protected:
    void assertEntriesNotAboutToBeFinalized() {
#if DEBUG
        for (Range r = Base::all(); !r.empty(); r.popFront()) {
            Key k(r.front().key());
            MOZ_ASSERT(!gc::IsAboutToBeFinalized(&k));
            MOZ_ASSERT(!gc::IsAboutToBeFinalized(&r.front().value()));
            MOZ_ASSERT(k == r.front().key());
        }
#endif
    }
};

/* WeakMap methods exposed so they can be installed in the self-hosting global. */

extern JSObject*
InitBareWeakMapCtor(JSContext* cx, js::HandleObject obj);

extern bool
WeakMap_has(JSContext* cx, unsigned argc, Value* vp);

extern bool
WeakMap_get(JSContext* cx, unsigned argc, Value* vp);

extern bool
WeakMap_set(JSContext* cx, unsigned argc, Value* vp);

extern bool
WeakMap_delete(JSContext* cx, unsigned argc, Value* vp);

extern bool
WeakMap_clear(JSContext* cx, unsigned argc, Value* vp);

extern JSObject*
InitWeakMapClass(JSContext* cx, HandleObject obj);

} /* namespace js */

#endif /* jsweakmap_h */

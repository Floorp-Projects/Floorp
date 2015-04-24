/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Marking_h
#define gc_Marking_h

#include "gc/Barrier.h"

namespace js {

/*** Tracing ***/

// Trace through an edge in the live object graph on behalf of tracing. The
// effect of tracing the edge depends on the JSTracer being used.
template <typename T>
void
TraceEdge(JSTracer* trc, BarrieredBase<T>* thingp, const char* name);

// Trace through a "root" edge. These edges are the initial edges in the object
// graph traversal. Root edges are asserted to only be traversed in the initial
// phase of a GC.
template <typename T>
void
TraceRoot(JSTracer* trc, T* thingp, const char* name);

// Like TraceEdge, but for edges that do not use one of the automatic barrier
// classes and, thus, must be treated specially for moving GC. This method is
// separate from TraceEdge to make accidental use of such edges more obvious.
template <typename T>
void
TraceManuallyBarrieredEdge(JSTracer* trc, T* thingp, const char* name);

// Trace all edges contained in the given array.
template <typename T>
void
TraceRange(JSTracer* trc, size_t len, BarrieredBase<T>* vec, const char* name);

// Trace all root edges in the given array.
template <typename T>
void
TraceRootRange(JSTracer* trc, size_t len, T* vec, const char* name);

// Trace an edge that crosses compartment boundaries. If the compartment of the
// destination thing is not being GC'd, then the edge will not be traced.
template <typename T>
void
TraceCrossCompartmentEdge(JSTracer* trc, JSObject* src, BarrieredBase<T>* dst,
                          const char* name);

// As above but with manual barriers.
template <typename T>
void
TraceManuallyBarrieredCrossCompartmentEdge(JSTracer* trc, JSObject* src, T* dst,
                                           const char* name);

// Permanent atoms and well-known symbols are shared between runtimes and must
// use a separate marking path so that we can filter them out of normal heap
// tracing.
template <typename T>
void
TraceProcessGlobalRoot(JSTracer* trc, T* thing, const char* name);

namespace gc {

/* Return true if the pointer is nullptr, or if it is a tagged pointer to
 * nullptr.
 */
MOZ_ALWAYS_INLINE bool
IsNullTaggedPointer(void* p)
{
    return uintptr_t(p) < 32;
}

/*** Externally Typed Marking ***/

void
TraceGenericPointerRoot(JSTracer* trc, Cell** thingp, const char* name);

void
TraceManuallyBarrieredGenericPointerEdge(JSTracer* trc, Cell** thingp, const char* name);

/*** Slot Marking ***/

void
MarkObjectSlots(JSTracer* trc, NativeObject* obj, uint32_t start, uint32_t nslots);

/*** Special Cases ***/

/*
 * Trace through the shape and any shapes it contains to mark
 * non-shape children. This is exposed to the JS API as
 * JS_TraceShapeCycleCollectorChildren.
 */
void
MarkCycleCollectorChildren(JSTracer* trc, Shape* shape);

void
PushArena(GCMarker* gcmarker, ArenaHeader* aheader);

/*** Generic ***/

template <typename T>
bool
IsMarkedUnbarriered(T* thingp);

template <typename T>
bool
IsMarked(BarrieredBase<T>* thingp);

template <typename T>
bool
IsMarked(ReadBarriered<T>* thingp);

template <typename T>
bool
IsAboutToBeFinalizedUnbarriered(T* thingp);

template <typename T>
bool
IsAboutToBeFinalized(BarrieredBase<T>* thingp);

template <typename T>
bool
IsAboutToBeFinalized(ReadBarriered<T>* thingp);

inline Cell*
ToMarkable(const Value& v)
{
    if (v.isMarkable())
        return (Cell*)v.toGCThing();
    return nullptr;
}

inline Cell*
ToMarkable(Cell* cell)
{
    return cell;
}

/*
 * HashKeyRef represents a reference to a HashMap key. This should normally
 * be used through the HashTableWriteBarrierPost function.
 */
template <typename Map, typename Key>
class HashKeyRef : public BufferableRef
{
    Map* map;
    Key key;

  public:
    HashKeyRef(Map* m, const Key& k) : map(m), key(k) {}

    void mark(JSTracer* trc) {
        Key prior = key;
        typename Map::Ptr p = map->lookup(key);
        if (!p)
            return;
        JS::AutoOriginalTraceLocation reloc(trc, (void**)&*p);
        TraceManuallyBarrieredEdge(trc, &key, "HashKeyRef");
        map->rekeyIfMoved(prior, key);
    }
};

} /* namespace gc */

void
TraceChildren(JSTracer* trc, void* thing, JSGCTraceKind kind);

bool
UnmarkGrayShapeRecursively(Shape* shape);

template<typename T>
void
CheckTracedThing(JSTracer* trc, T thing);

} /* namespace js */

#endif /* gc_Marking_h */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Marking_h
#define gc_Marking_h

#include "gc/Barrier.h"

class JSAtom;
class JSLinearString;

namespace js {

class ArgumentsObject;
class ArrayBufferObject;
class ArrayBufferViewObject;
class SharedArrayBufferObject;
class BaseShape;
class DebugScopeObject;
class GCMarker;
class GlobalObject;
class LazyScript;
class NestedScopeObject;
class SavedFrame;
class ScopeObject;
class Shape;
class UnownedBaseShape;

template<class> class HeapPtr;

namespace jit {
class JitCode;
struct IonScript;
struct VMFunction;
}

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

namespace gc {

/*** Object Marking ***/

/*
 * These functions expose marking functionality for all of the different GC
 * thing kinds. For each GC thing, there are several variants. As an example,
 * these are the variants generated for JSObject. They are listed from most to
 * least desirable for use:
 *
 * MarkObject(JSTracer* trc, const HeapPtrObject& thing, const char* name);
 *     This function should be used for marking JSObjects, in preference to all
 *     others below. Use it when you have HeapPtrObject, which automatically
 *     implements write barriers.
 *
 * MarkObjectRoot(JSTracer* trc, JSObject* thing, const char* name);
 *     This function is only valid during the root marking phase of GC (i.e.,
 *     when MarkRuntime is on the stack).
 *
 * MarkObjectUnbarriered(JSTracer* trc, JSObject* thing, const char* name);
 *     Like MarkObject, this function can be called at any time. It is more
 *     forgiving, since it doesn't demand a HeapPtr as an argument. Its use
 *     should always be accompanied by a comment explaining how write barriers
 *     are implemented for the given field.
 *
 * Additionally, the functions MarkObjectRange and MarkObjectRootRange are
 * defined for marking arrays of object pointers.
 *
 * The following functions are provided to test whether a GC thing is marked
 * under different circumstances:
 *
 * IsObjectAboutToBeFinalized(JSObject** thing);
 *     This function is indended to be used in code used to sweep GC things.  It
 *     indicates whether the object will will be finialized in the current group
 *     of compartments being swept.  Note that this will return false for any
 *     object not in the group of compartments currently being swept, as even if
 *     it is unmarked it may still become marked before it is swept.
 *
 * IsObjectMarked(JSObject** thing);
 *     This function is indended to be used in rare cases in code used to mark
 *     GC things.  It indicates whether the object is currently marked.
 *
 * UpdateObjectIfRelocated(JSObject** thingp);
 *     In some circumstances -- e.g. optional weak marking -- it is necessary
 *     to look at the pointer before marking it strongly or weakly. In these
 *     cases, the following must be called to update the pointer before use.
 */

#define DeclMarker(base, type)                                                                    \
void Mark##base(JSTracer* trc, BarrieredBase<type*>* thing, const char* name);                    \
void Mark##base##Root(JSTracer* trc, type** thingp, const char* name);                            \
void Mark##base##Unbarriered(JSTracer* trc, type** thingp, const char* name);                     \
void Mark##base##Range(JSTracer* trc, size_t len, HeapPtr<type*>* thing, const char* name);       \
void Mark##base##RootRange(JSTracer* trc, size_t len, type** thing, const char* name);            \
bool Is##base##Marked(type** thingp);                                                             \
bool Is##base##Marked(BarrieredBase<type*>* thingp);                                              \
bool Is##base##AboutToBeFinalized(type** thingp);                                                 \
bool Is##base##AboutToBeFinalized(BarrieredBase<type*>* thingp);                                  \
type* Update##base##IfRelocated(JSRuntime* rt, BarrieredBase<type*>* thingp);                     \
type* Update##base##IfRelocated(JSRuntime* rt, type** thingp);
#undef DeclMarker

void
MarkPermanentAtom(JSTracer* trc, JSAtom* atom, const char* name);

void
MarkWellKnownSymbol(JSTracer* trc, JS::Symbol* sym);

/* Return true if the pointer is nullptr, or if it is a tagged pointer to
 * nullptr.
 */
MOZ_ALWAYS_INLINE bool
IsNullTaggedPointer(void* p)
{
    return uintptr_t(p) < 32;
}

/*** Externally Typed Marking ***/

/*
 * Note: this must only be called by the GC and only when we are tracing through
 * MarkRoots. It is explicitly for ConservativeStackMarking and should go away
 * after we transition to exact rooting.
 */
void
MarkKind(JSTracer* trc, void** thingp, JSGCTraceKind kind);

void
MarkGCThingRoot(JSTracer* trc, void** thingp, const char* name);

void
MarkGCThingUnbarriered(JSTracer* trc, void** thingp, const char* name);

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

inline bool
IsAboutToBeFinalized(const js::jit::VMFunction** vmfunc)
{
    /*
     * Preserves entries in the WeakCache<VMFunction, JitCode>
     * iff the JitCode has been marked.
     */
    return false;
}

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
        trc->setTracingLocation(&*p);
        TraceManuallyBarrieredEdge(trc, &key, "HashKeyRef");
        map->rekeyIfMoved(prior, key);
    }
};

} /* namespace gc */

void
TraceChildren(JSTracer* trc, void* thing, JSGCTraceKind kind);

bool
UnmarkGrayShapeRecursively(Shape* shape);

} /* namespace js */

#endif /* gc_Marking_h */

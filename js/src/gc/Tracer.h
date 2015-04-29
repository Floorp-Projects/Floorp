/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_Tracer_h
#define js_Tracer_h

#include "jsfriendapi.h"

#include "gc/Barrier.h"

namespace js {

// Internal Tracing API
//
// Tracing is an abstract visitation of each edge in a JS heap graph.[1] The
// most common (and performance sensitive) use of this infrastructure is for GC
// "marking" as part of the mark-and-sweep collector; however, this
// infrastructure is much more general than that and is used for many other
// purposes as well.
//
// One commonly misunderstood subtlety of the tracing architecture is the role
// of graph verticies versus graph edges. Graph verticies are the heap
// allocations -- GC things -- that are returned by Allocate. Graph edges are
// pointers -- including tagged pointers like Value and jsid -- that link the
// allocations into a complex heap. The tracing API deals *only* with edges.
// Any action taken on the target of a graph edge is independent to the tracing
// itself.
//
// Another common misunderstanding relates to the role of the JSTracer. The
// JSTracer instance determines what tracing does when visiting an edge; it
// does not itself participate in the tracing process, other than to be passed
// through as opaque data. It works like a closure in that respect.
//
// Tracing implementations internal to SpiderMonkey should use these interfaces
// instead of the public interfaces in js/TracingAPI.h. Unlike the public
// tracing methods, these work on internal types and avoid an external call.
//
// Note that the implementations for these methods are, surprisingly, in
// js/src/gc/Marking.cpp. This is so that the compiler can inline as much as
// possible in the common, marking pathways. Conceptually, however, they remain
// as part of the generic "tracing" architecture, rather than the more specific
// marking implementation of tracing.
//
// 1 - In SpiderMonkey, we call this concept tracing rather than visiting
//     because "visiting" is already used by the compiler. Also, it's been
//     called "tracing" forever and changing it would be extremely difficult at
//     this point.

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

// Trace a root edge that uses the base GC thing type, instead of a more
// specific type.
void
TraceGenericPointerRoot(JSTracer* trc, gc::Cell** thingp, const char* name);

// Trace a non-root edge that uses the base GC thing type, instead of a more
// specific type.
void
TraceManuallyBarrieredGenericPointerEdge(JSTracer* trc, gc::Cell** thingp, const char* name);

// Object slots are not stored as a contiguous vector, so marking them as such
// will lead to the wrong indicies, if such are requested when tracing.
void
TraceObjectSlots(JSTracer* trc, NativeObject* obj, uint32_t start, uint32_t nslots);

// Depricated. Please use one of the strongly typed variants above.
void
TraceChildren(JSTracer* trc, void* thing, JSGCTraceKind kind);

} // namespace js

#endif /* js_Tracer_h */

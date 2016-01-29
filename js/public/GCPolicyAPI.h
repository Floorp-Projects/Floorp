/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// GC Policy Mechanism

// A GCPolicy controls how the GC interacts with both direct pointers to GC
// things (e.g. JSObject* or JSString*), tagged and/or optional pointers to GC
// things (e.g.  Value or jsid), and C++ aggregate types (e.g.
// JSPropertyDescriptor or GCHashMap).
//
// The GCPolicy provides at a minimum:
//
//   static T initial()
//       - Tells the GC how to construct an empty T.
//
//   static void trace(JSTracer, T* tp, const char* name)
//       - Tells the GC how to traverse the edge. In the case of an aggregate,
//         describe how to trace the children.
//
//   static bool needsSweep(T* tp)
//       - Tells the GC how to determine if an edge is about to be finalized,
//         and potentially updates the edge for moving GC if not. For
//         aggregates, it determines the weakness semantics of storing the
//         aggregate inside a weak container of some sort. For example, you
//         might specialize a weak table's key type GCPolicy to describe
//         when an entry should be kept during sweeping. This is the primary
//         reason that GC-supporting weak containers can override the [sweep?]
//         policy on a per-container basis.

#ifndef GCPolicyAPI_h
#define GCPolicyAPI_h

#include "js/TraceKind.h"
#include "js/TracingAPI.h"

class JSAtom;
class JSFunction;
class JSObject;
class JSScript;
class JSString;
namespace JS {
class Symbol;
}

namespace js {

// Defines a policy for aggregate types with non-GC, i.e. C storage. This
// policy dispatches to the underlying aggregate for GC interactions.
template <typename T>
struct StructGCPolicy
{
    static T initial() {
        return T();
    }

    static void trace(JSTracer* trc, T* tp, const char* name) {
        tp->trace(trc);
    }

    static bool needsSweep(T* tp) {
        return tp->needsSweep();
    }
};

// The default GC policy attempts to defer to methods on the underlying type.
// Most C++ structures that contain a default constructor, a trace function and
// a sweep function will work out of the box with Rooted, Handle, GCVector,
// and GCHash{Set,Map}.
template <typename T> struct GCPolicy : public StructGCPolicy<T> {};

// This policy ignores any GC interaction, e.g. for non-GC types.
template <typename T>
struct IgnoreGCPolicy {
    static T initial() { return T(); }
    static void trace(JSTracer* trc, T* t, const char* name) {}
    static bool needsSweep(T* v) { return false; }
};
template <> struct GCPolicy<uint32_t> : public IgnoreGCPolicy<uint32_t> {};
template <> struct GCPolicy<uint64_t> : public IgnoreGCPolicy<uint64_t> {};

template <typename T>
struct GCPointerPolicy
{
    static T initial() { return nullptr; }
    static void trace(JSTracer* trc, T* vp, const char* name) {
        if (*vp)
            js::UnsafeTraceManuallyBarrieredEdge(trc, vp, name);
    }
    static void needsSweep(T* vp) {
        return js::gc::EdgeNeedsSweep(vp);
    }
};
template <> struct GCPolicy<JS::Symbol*> : public GCPointerPolicy<JS::Symbol*> {};
template <> struct GCPolicy<JSAtom*> : public GCPointerPolicy<JSAtom*> {};
template <> struct GCPolicy<JSFunction*> : public GCPointerPolicy<JSFunction*> {};
template <> struct GCPolicy<JSObject*> : public GCPointerPolicy<JSObject*> {};
template <> struct GCPolicy<JSScript*> : public GCPointerPolicy<JSScript*> {};
template <> struct GCPolicy<JSString*> : public GCPointerPolicy<JSString*> {};

template <typename T>
struct GCPolicy<JS::Heap<T>>
{
    static void trace(JSTracer* trc, JS::Heap<T>* thingp, const char* name) {
        JS::TraceEdge(trc, thingp, name);
    }
    static bool needsSweep(JS::Heap<T>* thingp) {
        return gc::EdgeNeedsSweep(thingp);
    }
};

} // namespace js

#endif // GCPolicyAPI_h

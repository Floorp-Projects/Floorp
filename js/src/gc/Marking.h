/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Marking and sweeping APIs for use by implementations of different GC cell
 * kinds.
 */

#ifndef gc_Marking_h
#define gc_Marking_h

#include "js/TypeDecls.h"
#include "vm/TaggedProto.h"

class JSLinearString;
class JSRope;
class JSTracer;

namespace js {
class BaseShape;
class GCMarker;
class LazyScript;
class NativeObject;
class ObjectGroup;
class Shape;
class WeakMapBase;

namespace jit {
class JitCode;
}  // namespace jit

#ifdef DEBUG
// Return true if this trace is happening on behalf of gray buffering during
// the marking phase of incremental GC.
bool IsBufferGrayRootsTracer(JSTracer* trc);

bool IsUnmarkGrayTracer(JSTracer* trc);
#endif

namespace gc {

class Arena;
struct Cell;
class TenuredCell;

/*** Special Cases ***/

void PushArena(GCMarker* gcmarker, Arena* arena);

/*** Liveness ***/

// The IsMarkedInternal and IsAboutToBeFinalizedInternal function templates are
// used to implement the IsMarked and IsAboutToBeFinalized set of functions.
// These internal functions are instantiated for the base GC types and should
// not be called directly.
//
// Note that there are two function templates declared for each, not one
// template and a specialization. This is necessary so that pointer arguments
// (e.g. JSObject**) and tagged value arguments (e.g. JS::Value*) are routed to
// separate implementations.

template <typename T>
bool IsMarkedInternal(JSRuntime* rt, T* thing);
template <typename T>
bool IsMarkedInternal(JSRuntime* rt, T** thing);

template <typename T>
bool IsMarkedBlackInternal(JSRuntime* rt, T* thing);
template <typename T>
bool IsMarkedBlackInternal(JSRuntime* rt, T** thing);

template <typename T>
bool IsAboutToBeFinalizedInternal(T* thingp);
template <typename T>
bool IsAboutToBeFinalizedInternal(T** thingp);

// Report whether a GC thing has been marked with any color. Things which are in
// zones that are not currently being collected or are owned by another runtime
// are always reported as being marked.
template <typename T>
inline bool IsMarkedUnbarriered(JSRuntime* rt, T* thingp) {
  return IsMarkedInternal(rt, ConvertToBase(thingp));
}

// Report whether a GC thing has been marked with any color. Things which are in
// zones that are not currently being collected or are owned by another runtime
// are always reported as being marked.
template <typename T>
inline bool IsMarked(JSRuntime* rt, WriteBarriered<T>* thingp) {
  return IsMarkedInternal(rt,
                          ConvertToBase(thingp->unsafeUnbarrieredForTracing()));
}

// Report whether a GC thing has been marked black.
template <typename T>
inline bool IsMarkedBlackUnbarriered(JSRuntime* rt, T* thingp) {
  return IsMarkedBlackInternal(rt, ConvertToBase(thingp));
}

// Report whether a GC thing has been marked black.
template <typename T>
inline bool IsMarkedBlack(JSRuntime* rt, WriteBarriered<T>* thingp) {
  return IsMarkedBlackInternal(
      rt, ConvertToBase(thingp->unsafeUnbarrieredForTracing()));
}

template <typename T>
inline bool IsAboutToBeFinalizedUnbarriered(T* thingp) {
  return IsAboutToBeFinalizedInternal(ConvertToBase(thingp));
}

template <typename T>
inline bool IsAboutToBeFinalized(WriteBarriered<T>* thingp) {
  return IsAboutToBeFinalizedInternal(
      ConvertToBase(thingp->unsafeUnbarrieredForTracing()));
}

template <typename T>
inline bool IsAboutToBeFinalized(ReadBarriered<T>* thingp) {
  return IsAboutToBeFinalizedInternal(
      ConvertToBase(thingp->unsafeUnbarrieredForTracing()));
}

bool IsAboutToBeFinalizedDuringSweep(TenuredCell& tenured);

inline Cell* ToMarkable(const Value& v) {
  if (v.isGCThing()) {
    return (Cell*)v.toGCThing();
  }
  return nullptr;
}

inline Cell* ToMarkable(Cell* cell) { return cell; }

} /* namespace gc */

// The return value indicates if anything was unmarked.
bool UnmarkGrayShapeRecursively(Shape* shape);

template <typename T>
void CheckTracedThing(JSTracer* trc, T* thing);

template <typename T>
void CheckTracedThing(JSTracer* trc, T thing);

namespace gc {

// Functions for checking and updating GC thing pointers that might have been
// moved by compacting GC. Overloads are also provided that work with Values.
//
// IsForwarded    - check whether a pointer refers to an GC thing that has been
//                  moved.
//
// Forwarded      - return a pointer to the new location of a GC thing given a
//                  pointer to old location.
//
// MaybeForwarded - used before dereferencing a pointer that may refer to a
//                  moved GC thing without updating it. For JSObjects this will
//                  also update the object's shape pointer if it has been moved
//                  to allow slots to be accessed.

template <typename T>
inline bool IsForwarded(const T* t);
inline bool IsForwarded(const JS::Value& value);

template <typename T>
inline T* Forwarded(const T* t);

inline Value Forwarded(const JS::Value& value);

template <typename T>
inline T MaybeForwarded(T t);

#ifdef JSGC_HASH_TABLE_CHECKS

template <typename T>
inline bool IsGCThingValidAfterMovingGC(T* t);

template <typename T>
inline void CheckGCThingAfterMovingGC(T* t);

template <typename T>
inline void CheckGCThingAfterMovingGC(const WeakHeapPtr<T*>& t);

inline void CheckValueAfterMovingGC(const JS::Value& value);

#endif  // JSGC_HASH_TABLE_CHECKS

} /* namespace gc */
} /* namespace js */

#endif /* gc_Marking_h */

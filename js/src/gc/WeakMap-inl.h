/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_WeakMap_inl_h
#define gc_WeakMap_inl_h

#include "gc/WeakMap.h"

#include "js/TraceKind.h"
#include "vm/JSContext.h"

namespace js {

template <typename T>
static T extractUnbarriered(const WriteBarriered<T>& v) {
  return v.get();
}

template <typename T>
static T* extractUnbarriered(T* v) {
  return v;
}

inline /* static */ JSObject* WeakMapBase::getDelegate(JSObject* key) {
  return UncheckedUnwrapWithoutExpose(key);
}

inline /* static */ JSObject* WeakMapBase::getDelegate(JSScript* script) {
  return nullptr;
}

inline /* static */ JSObject* WeakMapBase::getDelegate(LazyScript* script) {
  return nullptr;
}

template <class K, class V>
WeakMap<K, V>::WeakMap(JSContext* cx, JSObject* memOf)
    : Base(cx->zone()), WeakMapBase(memOf, cx->zone()) {
  using ElemType = typename K::ElementType;
  using NonPtrType = typename mozilla::RemovePointer<ElemType>::Type;

  // The object's TraceKind needs to be added to CC graph if this object is
  // used as a WeakMap key, otherwise the key is considered to be pointed from
  // somewhere unknown, and results in leaking the subgraph which contains the
  // key. See the comments in NoteWeakMapsTracer::trace for more details.
  static_assert(JS::IsCCTraceKind(NonPtrType::TraceKind),
                "Object's TraceKind should be added to CC graph.");

  zone()->gcWeakMapList().insertFront(this);
  if (zone()->wasGCStarted()) {
    marked = true;
    markColor = gc::MarkColor::Black;
  }
}

// Trace a WeakMap entry based on 'markedCell' getting marked, where 'origKey'
// is the key in the weakmap. These will probably be the same, but can be
// different eg when markedCell is a delegate for origKey.
//
// This implementation does not use 'markedCell'; it looks up origKey and checks
// the mark bits on everything it cares about, one of which will be
// markedCell. But a subclass might use it to optimize the liveness check.
template <class K, class V>
void WeakMap<K, V>::markEntry(GCMarker* marker, gc::Cell* markedCell,
                              JS::GCCellPtr origKey) {
  MOZ_ASSERT(marked);

  // If this cast fails, then you're instantiating the WeakMap with a
  // Lookup that can't be constructed from a Cell*. The WeakKeyTable
  // mechanism is indexed with a GCCellPtr, so that won't work.
  Ptr p = Base::lookup(static_cast<Lookup>(origKey.asCell()));
  MOZ_ASSERT(p.found());

  K key(p->key());
  MOZ_ASSERT((markedCell == extractUnbarriered(key)) ||
             (markedCell == getDelegate(key)));
  if (marker->isMarked(&key)) {
    TraceEdge(marker, &p->value(), "ephemeron value");
  } else if (keyNeedsMark(marker, key)) {
    TraceEdge(marker, &p->value(), "WeakMap ephemeron value");
    TraceEdge(marker, &key, "proxy-preserved WeakMap ephemeron key");
    MOZ_ASSERT(key == p->key());  // No moving
  }
  key.unsafeSet(nullptr);  // Prevent destructor from running barriers.
}

template <class K, class V>
void WeakMap<K, V>::trace(JSTracer* trc) {
  MOZ_ASSERT_IF(JS::RuntimeHeapIsBusy(), isInList());

  TraceNullableEdge(trc, &memberOf, "WeakMap owner");

  if (trc->isMarkingTracer()) {
    MOZ_ASSERT(trc->weakMapAction() == ExpandWeakMaps);
    auto marker = GCMarker::fromTracer(trc);

    // Don't change the map color from black to gray. This can happen when a
    // barrier pushes the map object onto the black mark stack when it's already
    // present on the gray mark stack, which is marked later.
    if (marked && markColor == gc::MarkColor::Black &&
        marker->markColor() == gc::MarkColor::Gray) {
      return;
    }

    marked = true;
    markColor = marker->markColor();
    (void)markIteratively(marker);
    return;
  }

  if (trc->weakMapAction() == DoNotTraceWeakMaps) {
    return;
  }

  // Trace keys only if weakMapAction() says to.
  if (trc->weakMapAction() == TraceWeakMapKeysValues) {
    for (Enum e(*this); !e.empty(); e.popFront()) {
      TraceEdge(trc, &e.front().mutableKey(), "WeakMap entry key");
    }
  }

  // Always trace all values (unless weakMapAction() is
  // DoNotTraceWeakMaps).
  for (Range r = Base::all(); !r.empty(); r.popFront()) {
    TraceEdge(trc, &r.front().value(), "WeakMap entry value");
  }
}

template <class K, class V>
/* static */ void WeakMap<K, V>::addWeakEntry(
    GCMarker* marker, JS::GCCellPtr key, const gc::WeakMarkable& markable) {
  Zone* zone = key.asCell()->asTenured().zone();

  auto p = zone->gcWeakKeys().get(key);
  if (p) {
    gc::WeakEntryVector& weakEntries = p->value;
    if (!weakEntries.append(markable)) {
      marker->abortLinearWeakMarking();
    }
  } else {
    gc::WeakEntryVector weakEntries;
    MOZ_ALWAYS_TRUE(weakEntries.append(markable));
    if (!zone->gcWeakKeys().put(JS::GCCellPtr(key), std::move(weakEntries))) {
      marker->abortLinearWeakMarking();
    }
  }
}

template <class K, class V>
bool WeakMap<K, V>::markIteratively(GCMarker* marker) {
  MOZ_ASSERT(marked);
  if (marker->markColor() == gc::MarkColor::Black &&
      markColor == gc::MarkColor::Gray) {
    return false;
  }

  bool markedAny = false;

  for (Enum e(*this); !e.empty(); e.popFront()) {
    // If the entry is live, ensure its key and value are marked.
    bool keyIsMarked = marker->isMarked(&e.front().mutableKey());
    if (!keyIsMarked && keyNeedsMark(marker, e.front().key())) {
      TraceEdge(marker, &e.front().mutableKey(),
                "proxy-preserved WeakMap entry key");
      keyIsMarked = true;
      markedAny = true;
    }

    if (keyIsMarked) {
      if (!marker->isMarked(&e.front().value())) {
        TraceEdge(marker, &e.front().value(), "WeakMap entry value");
        markedAny = true;
      }
    } else if (marker->isWeakMarkingTracer()) {
      // Entry is not yet known to be live. Record this weakmap and
      // the lookup key in the list of weak keys. Also record the
      // delegate, if any, because marking the delegate also marks
      // the entry.
      JS::GCCellPtr weakKey(extractUnbarriered(e.front().key()));
      gc::WeakMarkable markable(this, weakKey);
      addWeakEntry(marker, weakKey, markable);
      if (JSObject* delegate = getDelegate(e.front().key())) {
        addWeakEntry(marker, JS::GCCellPtr(delegate), markable);
      }
    }
  }

  return markedAny;
}

template <class K, class V>
inline bool WeakMap<K, V>::keyNeedsMark(GCMarker* marker, JSObject* key) const {
  JSObject* delegate = getDelegate(key);
  /*
   * Check if the delegate is marked with any color to properly handle
   * gray marking when the key's delegate is black and the map is gray.
   */
  return delegate && marker->isMarkedUnbarriered(&delegate);
}

template <class K, class V>
inline bool WeakMap<K, V>::keyNeedsMark(GCMarker* marker,
                                        JSScript* script) const {
  return false;
}

template <class K, class V>
inline bool WeakMap<K, V>::keyNeedsMark(GCMarker* marker,
                                        LazyScript* script) const {
  return false;
}

template <class K, class V>
void WeakMap<K, V>::sweep() {
  /* Remove all entries whose keys remain unmarked. */
  for (Enum e(*this); !e.empty(); e.popFront()) {
    if (gc::IsAboutToBeFinalized(&e.front().mutableKey())) {
      e.removeFront();
    }
  }

#if DEBUG
  // Once we've swept, all remaining edges should stay within the known-live
  // part of the graph.
  assertEntriesNotAboutToBeFinalized();
#endif
}

// memberOf can be nullptr, which means that the map is not part of a JSObject.
template <class K, class V>
void WeakMap<K, V>::traceMappings(WeakMapTracer* tracer) {
  for (Range r = Base::all(); !r.empty(); r.popFront()) {
    gc::Cell* key = gc::ToMarkable(r.front().key());
    gc::Cell* value = gc::ToMarkable(r.front().value());
    if (key && value) {
      tracer->trace(memberOf, JS::GCCellPtr(r.front().key().get()),
                    JS::GCCellPtr(r.front().value().get()));
    }
  }
}

#if DEBUG
template <class K, class V>
void WeakMap<K, V>::assertEntriesNotAboutToBeFinalized() {
  for (Range r = Base::all(); !r.empty(); r.popFront()) {
    K k(r.front().key());
    MOZ_ASSERT(!gc::IsAboutToBeFinalized(&k));
    JSObject* delegate = getDelegate(k);
    if (delegate) {
      MOZ_ASSERT(!gc::IsAboutToBeFinalizedUnbarriered(&delegate),
                 "weakmap marking depends on a key tracing its delegate");
    }
    MOZ_ASSERT(!gc::IsAboutToBeFinalized(&r.front().value()));
    MOZ_ASSERT(k == r.front().key());
  }
}
#endif

#ifdef JS_GC_ZEAL
template <class K, class V>
bool WeakMap<K, V>::checkMarking() const {
  bool ok = true;
  for (Range r = Base::all(); !r.empty(); r.popFront()) {
    gc::Cell* key = gc::ToMarkable(r.front().key());
    gc::Cell* value = gc::ToMarkable(r.front().value());
    if (key && value) {
      if (!gc::CheckWeakMapEntryMarking(this, key, value)) {
        ok = false;
      }
    }
  }
  return ok;
}
#endif

} /* namespace js */

#endif /* gc_WeakMap_inl_h */

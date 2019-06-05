/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_WeakMap_inl_h
#define gc_WeakMap_inl_h

#include "gc/WeakMap.h"
#include "gc/PublicIterators.h"

#include "gc/Zone.h"
#include "js/TraceKind.h"
#include "vm/JSContext.h"

namespace js {
namespace gc {
namespace detail {

template <typename T>
static T extractUnbarriered(const WriteBarriered<T>& v) {
  return v.get();
}

template <typename T>
static T* extractUnbarriered(T* v) {
  return v;
}

template <typename T>
static JS::Zone* GetZone(T t) {
  return t->zone();
}

static JS::Zone* GetZone(js::HeapPtr<JS::Value>& t) {
  if (!t.isGCThing()) {
    return nullptr;
  }
  return t.toGCThing()->asTenured().zone();
}

// Only objects have delegates, so default to returning nullptr. Note that some
// compilation units will only ever use the object version.
static MOZ_MAYBE_UNUSED JSObject* GetDelegateHelper(gc::Cell* key) {
  return nullptr;
}

static JSObject* GetDelegateHelper(JSObject* key) {
  JSObject* delegate = UncheckedUnwrapWithoutExpose(key);
  return (key == delegate) ? nullptr : delegate;
}

} /* namespace detail */
} /* namespace gc */

// Use a helper function to do overload resolution to handle cases like
// Heap<ObjectSubclass*>: find everything that is convertible to JSObject* (and
// avoid calling barriers).
template <typename T>
inline /* static */ JSObject* WeakMapBase::getDelegate(const T& key) {
  using namespace gc::detail;
  return GetDelegateHelper(extractUnbarriered(key));
}

template <>
inline /* static */ JSObject* WeakMapBase::getDelegate(gc::Cell* const&) =
    delete;

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
    markColor = CellColor::Black;
  }
}

namespace gc {

// Compute the correct color to mark a weakmap entry value based on the map and
// key colors.
struct AutoSetValueColor : gc::AutoSetMarkColor {
  AutoSetValueColor(GCMarker& marker, CellColor mapColor, CellColor keyColor)
      : gc::AutoSetMarkColor(
            marker, mapColor == keyColor ? mapColor : CellColor::Gray) {}
};

}  // namespace gc

// Trace a WeakMap entry based on 'markedCell' getting marked, where 'origKey'
// is the key in the weakmap. These will probably be the same, but can be
// different eg when markedCell is a delegate for origKey.
//
// This implementation does not use 'markedCell'; it looks up origKey and checks
// the mark bits on everything it cares about, one of which will be
// markedCell. But a subclass might use it to optimize the liveness check.
//
// markEntry is called when encountering a weakmap key during marking, or when
// entering weak marking mode.
template <class K, class V>
void WeakMap<K, V>::markEntry(GCMarker* marker, gc::Cell* markedCell,
                              gc::Cell* origKey) {
  using namespace gc::detail;

  Ptr p = Base::lookup(static_cast<Lookup>(origKey));
  // We should only be processing <weakmap,key> pairs where the key exists in
  // the weakmap. Such pairs are inserted when a weakmap is marked, and are
  // removed by barriers if the key is removed from the weakmap. Failure here
  // probably means gcWeakKeys is not being properly traced during a minor GC,
  // or the weakmap keys are not being updated when tenured.
  MOZ_ASSERT(p.found());

  K key(p->key());
  MOZ_ASSERT((markedCell == extractUnbarriered(key)) ||
             (markedCell == getDelegate(key)));

  CellColor delegateColor = getDelegateColor(key);
  if (IsMarked(delegateColor) && key->zone()->isGCMarking()) {
    gc::AutoSetMarkColor autoColor(*marker, delegateColor);
    TraceEdge(marker, &key, "proxy-preserved WeakMap ephemeron key");
    MOZ_ASSERT(key == p->key(), "no moving GC");
  }
  CellColor keyColor = getCellColor(key);
  if (IsMarked(keyColor)) {
    JS::Zone* valueZone = GetZone(p->value());
    if (valueZone && valueZone->isGCMarking()) {
      gc::AutoSetValueColor autoColor(*marker, markColor, keyColor);
      TraceEdge(marker, &p->value(), "WeakMap ephemeron value");
    }
  }
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
    if (markColor == CellColor::Black &&
        marker->markColor() == gc::MarkColor::Gray) {
      return;
    }

    markColor = GetCellColor(marker->markColor());
    (void)markEntries(marker);
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
    GCMarker* marker, gc::Cell* key, const gc::WeakMarkable& markable) {
  Zone* zone = key->asTenured().zone();
  auto& weakKeys = zone->gcWeakKeys(key);
  auto p = weakKeys.get(key);
  if (p) {
    gc::WeakEntryVector& weakEntries = p->value;
    if (!weakEntries.append(markable)) {
      marker->abortLinearWeakMarking();
    }
  } else {
    gc::WeakEntryVector weakEntries;
    MOZ_ALWAYS_TRUE(weakEntries.append(markable));
    if (!weakKeys.put(key, std::move(weakEntries))) {
      marker->abortLinearWeakMarking();
    }
  }
}

template <class K, class V>
bool WeakMap<K, V>::markEntries(GCMarker* marker) {
  MOZ_ASSERT(IsMarked(markColor));
  bool markedAny = false;

  for (Enum e(*this); !e.empty(); e.popFront()) {
    // If the entry is live, ensure its key and value are marked.
    CellColor keyColor = getCellColor(e.front().key().get());
    CellColor delegateColor = getDelegateColor(e.front().key());
    if (IsMarked(delegateColor) && keyColor < delegateColor) {
      gc::AutoSetMarkColor autoColor(*marker, delegateColor);
      TraceEdge(marker, &e.front().mutableKey(),
                "proxy-preserved WeakMap entry key");
      markedAny = true;
      keyColor = delegateColor;
    }

    if (IsMarked(keyColor)) {
      gc::AutoSetValueColor autoColor(*marker, markColor, keyColor);
      if (!marker->isMarked(&e.front().value())) {
        TraceEdge(marker, &e.front().value(), "WeakMap entry value");
        markedAny = true;
      }
    }

    // Changes in the map's mark color will be handled in this code, but
    // changes in the key's mark color are handled through the weak keys table.
    // So we only need to populate the table if the key is less marked than the
    // map, to catch later updates in the key's mark color.
    if (keyColor < markColor) {
      MOZ_ASSERT(marker->weakMapAction() == ExpandWeakMaps);
      // Entry is not yet known to be live. Record this weakmap and the lookup
      // key in the list of weak keys. If the key has a delegate, then the
      // lookup key is the delegate (because marking the key will end up
      // marking the delegate and thereby mark the entry.)
      gc::Cell* weakKey = gc::detail::extractUnbarriered(e.front().key());
      gc::WeakMarkable markable(this, weakKey);
      if (JSObject* delegate = getDelegate(e.front().key())) {
        addWeakEntry(marker, delegate, markable);
      } else {
        addWeakEntry(marker, weakKey, markable);
      }
    }
  }

  return markedAny;
}

template <class K, class V>
void WeakMap<K, V>::postSeverDelegate(GCMarker* marker, gc::Cell* key) {
  if (IsMarked(markColor)) {
    // We only stored the delegate, not the key, and we're severing the
    // delegate from the key. So store the key.
    gc::WeakMarkable markable(this, key);
    addWeakEntry(marker, key, markable);
  }
}

template <class K, class V>
inline WeakMapBase::CellColor WeakMap<K, V>::getDelegateColor(
    JSObject* key) const {
  JSObject* delegate = getDelegate(key);
  return delegate ? getCellColor(delegate) : CellColor::White;
}

template <class K, class V>
inline WeakMapBase::CellColor WeakMap<K, V>::getDelegateColor(
    JSScript* script) const {
  return CellColor::White;
}

template <class K, class V>
inline WeakMapBase::CellColor WeakMap<K, V>::getDelegateColor(
    LazyScript* script) const {
  return CellColor::White;
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

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_WeakMap_inl_h
#define gc_WeakMap_inl_h

#include "gc/WeakMap.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/Unused.h"

#include <algorithm>
#include <type_traits>

#include "gc/PublicIterators.h"
#include "gc/Zone.h"
#include "js/TraceKind.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"

namespace js {
namespace gc {

// Specializations for barriered types.
template <typename T>
inline Cell* ToMarkable(WriteBarriered<T>* thingp) {
  return ToMarkable(thingp->get());
}

namespace detail {

template <typename T>
static T ExtractUnbarriered(const WriteBarriered<T>& v) {
  return v.get();
}

template <typename T>
static T* ExtractUnbarriered(T* v) {
  return v;
}

// Return the effective cell color given the current marking state.
// This must be kept in sync with ShouldMark in Marking.cpp.
template <typename T>
static CellColor GetEffectiveColor(JSRuntime* rt, const T& item) {
  Cell* cell = ToMarkable(item);
  if (!cell->isTenured()) {
    return CellColor::Black;
  }
  const TenuredCell& t = cell->asTenured();
  if (rt != t.runtimeFromAnyThread()) {
    return CellColor::Black;
  }
  if (!t.zoneFromAnyThread()->shouldMarkInZone()) {
    return CellColor::Black;
  }
  return cell->color();
}

// Only objects have delegates, so default to returning nullptr. Note that some
// compilation units will only ever use the object version.
static MOZ_MAYBE_UNUSED JSObject* GetDelegateInternal(gc::Cell* key) {
  return nullptr;
}

static JSObject* GetDelegateInternal(JSObject* key) {
  JSObject* delegate = UncheckedUnwrapWithoutExpose(key);
  return (key == delegate) ? nullptr : delegate;
}

// Use a helper function to do overload resolution to handle cases like
// Heap<ObjectSubclass*>: find everything that is convertible to JSObject* (and
// avoid calling barriers).
template <typename T>
static inline JSObject* GetDelegate(const T& key) {
  return GetDelegateInternal(ExtractUnbarriered(key));
}

template <>
inline JSObject* GetDelegate(gc::Cell* const&) = delete;

} /* namespace detail */
} /* namespace gc */

template <class K, class V>
WeakMap<K, V>::WeakMap(JSContext* cx, JSObject* memOf)
    : Base(cx->zone()), WeakMapBase(memOf, cx->zone()) {
  using ElemType = typename K::ElementType;
  using NonPtrType = std::remove_pointer_t<ElemType>;

  // The object's TraceKind needs to be added to CC graph if this object is
  // used as a WeakMap key, otherwise the key is considered to be pointed from
  // somewhere unknown, and results in leaking the subgraph which contains the
  // key. See the comments in NoteWeakMapsTracer::trace for more details.
  static_assert(JS::IsCCTraceKind(NonPtrType::TraceKind),
                "Object's TraceKind should be added to CC graph.");

  zone()->gcWeakMapList().insertFront(this);
  if (zone()->wasGCStarted()) {
    mapColor = CellColor::Black;
  }
}

// Trace a WeakMap entry based on 'markedCell' getting marked, where 'origKey'
// is the key in the weakmap. In the absence of delegates, these will be the
// same, but when a delegate is marked then origKey will be its wrapper.
// `markedCell` is only used for an assertion.
//
// markKey is called when encountering a weakmap key during marking, or when
// entering weak marking mode.
template <class K, class V>
void WeakMap<K, V>::markKey(GCMarker* marker, gc::Cell* markedCell,
                            gc::Cell* origKey) {
  using namespace gc::detail;

#if DEBUG
  if (!mapColor) {
    fprintf(stderr, "markKey called on an unmarked map %p", this);
    Zone* zone = markedCell->asTenured().zoneFromAnyThread();
    fprintf(stderr, "  markedCell=%p from zone %p state %d mark %d\n",
            markedCell, zone, zone->gcState(),
            int(debug::GetMarkInfo(markedCell)));
    zone = origKey->asTenured().zoneFromAnyThread();
    fprintf(stderr, "  origKey=%p from zone %p state %d mark %d\n", origKey,
            zone, zone->gcState(), int(debug::GetMarkInfo(markedCell)));
    if (memberOf) {
      zone = memberOf->asTenured().zoneFromAnyThread();
      fprintf(stderr, "  memberOf=%p from zone %p state %d mark %d\n",
              memberOf.get(), zone, zone->gcState(),
              int(debug::GetMarkInfo(memberOf.get())));
    }
  }
#endif

  MOZ_ASSERT(mapColor);

  Ptr p = Base::lookup(static_cast<Lookup>(origKey));
  // We should only be processing <weakmap,key> pairs where the key exists in
  // the weakmap. Such pairs are inserted when a weakmap is marked, and are
  // removed by barriers if the key is removed from the weakmap. Failure here
  // probably means gcWeakKeys is not being properly traced during a minor GC,
  // or the weakmap keys are not being updated when tenured.
  MOZ_ASSERT(p.found());

  mozilla::DebugOnly<gc::Cell*> oldKey = gc::ToMarkable(p->key());
  MOZ_ASSERT((markedCell == oldKey) ||
             (markedCell == gc::detail::GetDelegate(p->key())));

  markEntry(marker, p->mutableKey(), p->value());
  MOZ_ASSERT(oldKey == gc::ToMarkable(p->key()), "no moving GC");
}

// If the entry is live, ensure its key and value are marked. Also make sure
// the key is at least as marked as the delegate, so it cannot get discarded
// and then recreated by rewrapping the delegate.
template <class K, class V>
bool WeakMap<K, V>::markEntry(GCMarker* marker, K& key, V& value) {
  bool marked = false;
  JSRuntime* rt = zone()->runtimeFromAnyThread();
  CellColor keyColor = gc::detail::GetEffectiveColor(rt, key);
  JSObject* delegate = gc::detail::GetDelegate(key);

  if (delegate) {
    CellColor delegateColor = gc::detail::GetEffectiveColor(rt, delegate);
    // The key needs to stay alive while both the delegate and map are live.
    CellColor proxyPreserveColor = std::min(delegateColor, mapColor);
    if (keyColor < proxyPreserveColor) {
      gc::AutoSetMarkColor autoColor(*marker, proxyPreserveColor);
      TraceWeakMapKeyEdge(marker, zone(), &key,
                          "proxy-preserved WeakMap entry key");
      MOZ_ASSERT(key->color() >= proxyPreserveColor);
      marked = true;
      keyColor = proxyPreserveColor;
    }
  }

  if (keyColor) {
    gc::Cell* cellValue = gc::ToMarkable(&value);
    if (cellValue) {
      gc::AutoSetMarkColor autoColor(*marker, std::min(mapColor, keyColor));
      CellColor valueColor = gc::detail::GetEffectiveColor(rt, cellValue);
      if (valueColor < marker->markColor()) {
        TraceEdge(marker, &value, "WeakMap entry value");
        MOZ_ASSERT(cellValue->color() >= std::min(mapColor, keyColor));
        marked = true;
      }
    }
  }

  return marked;
}

template <class K, class V>
void WeakMap<K, V>::trace(JSTracer* trc) {
  MOZ_ASSERT_IF(JS::RuntimeHeapIsBusy(), isInList());

  TraceNullableEdge(trc, &memberOf, "WeakMap owner");

  if (trc->isMarkingTracer()) {
    MOZ_ASSERT(trc->weakMapAction() == ExpandWeakMaps);
    auto marker = GCMarker::fromTracer(trc);

    // Don't downgrade the map color from black to gray. This can happen when a
    // barrier pushes the map object onto the black mark stack when it's
    // already present on the gray mark stack, which is marked later.
    if (mapColor < marker->markColor()) {
      mapColor = marker->markColor();
      mozilla::Unused << markEntries(marker);
    }
    return;
  }

  if (trc->weakMapAction() == DoNotTraceWeakMaps) {
    return;
  }

  // Trace keys only if weakMapAction() says to.
  if (trc->weakMapAction() == TraceWeakMapKeysValues) {
    for (Enum e(*this); !e.empty(); e.popFront()) {
      TraceWeakMapKeyEdge(trc, zone(), &e.front().mutableKey(),
                          "WeakMap entry key");
    }
  }

  // Always trace all values (unless weakMapAction() is
  // DoNotTraceWeakMaps).
  for (Range r = Base::all(); !r.empty(); r.popFront()) {
    TraceEdge(trc, &r.front().value(), "WeakMap entry value");
  }
}

template <class K, class V>
/* static */ void WeakMap<K, V>::forgetKey(UnbarrieredKey key) {
  // Remove the key or its delegate from weakKeys.
  if (zone()->needsIncrementalBarrier()) {
    JSRuntime* rt = zone()->runtimeFromMainThread();
    if (JSObject* delegate = js::gc::detail::GetDelegate(key)) {
      js::gc::WeakKeyTable& weakKeys = delegate->zone()->gcWeakKeys(delegate);
      rt->gc.marker.forgetWeakKey(weakKeys, this, delegate, key);
    } else {
      js::gc::WeakKeyTable& weakKeys = key->zone()->gcWeakKeys(key);
      rt->gc.marker.forgetWeakKey(weakKeys, this, key, key);
    }
  }
}

template <class K, class V>
/* static */ void WeakMap<K, V>::clear() {
  Base::clear();
  JSRuntime* rt = zone()->runtimeFromMainThread();
  if (zone()->needsIncrementalBarrier()) {
    rt->gc.marker.forgetWeakMap(this, zone());
  }
}

template <class K, class V>
/* static */ void WeakMap<K, V>::addWeakEntry(
    GCMarker* marker, JS::Zone* keyZone, gc::Cell* key,
    const gc::WeakMarkable& markable) {
  auto& weakKeys = keyZone->gcWeakKeys(key);
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
  MOZ_ASSERT(mapColor);
  bool markedAny = false;

  for (Enum e(*this); !e.empty(); e.popFront()) {
    if (markEntry(marker, e.front().mutableKey(), e.front().value())) {
      markedAny = true;
    }

    JSRuntime* rt = zone()->runtimeFromAnyThread();
    CellColor keyColor =
        gc::detail::GetEffectiveColor(rt, e.front().key().get());

    // Changes in the map's mark color will be handled in this code, but
    // changes in the key's mark color are handled through the weak keys table.
    // So we only need to populate the table if the key is less marked than the
    // map, to catch later updates in the key's mark color.
    if (keyColor < mapColor) {
      MOZ_ASSERT(marker->weakMapAction() == ExpandWeakMaps);
      // The final color of the key is not yet known. Record this weakmap and
      // the lookup key in the list of weak keys. If the key has a delegate,
      // then the lookup key is the delegate (because marking the key will end
      // up marking the delegate and thereby mark the entry.)
      auto& weakKey = e.front().key();
      gc::Cell* weakKeyCell = gc::detail::ExtractUnbarriered(weakKey);
      gc::WeakMarkable markable(this, weakKeyCell);
      if (JSObject* delegate = gc::detail::GetDelegate(weakKey)) {
        addWeakEntry(marker, delegate->zone(), delegate, markable);
      } else {
        addWeakEntry(marker, weakKey.get()->zone(), weakKeyCell, markable);
      }
    }
  }

  return markedAny;
}

template <class K, class V>
void WeakMap<K, V>::postSeverDelegate(GCMarker* marker, JSObject* key,
                                      Compartment* comp) {
  if (mapColor) {
    // We only stored the delegate, not the key, and we're severing the
    // delegate from the key. So store the key.
    gc::WeakMarkable markable(this, key);
    addWeakEntry(marker, key->zone(), key, markable);
  }
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
    JSObject* delegate = gc::detail::GetDelegate(k);
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

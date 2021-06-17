/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/PropMap-inl.h"

#include "gc/Allocator.h"
#include "gc/HashUtil.h"
#include "vm/JSObject.h"

#include "vm/ObjectFlags-inl.h"

using namespace js;

void LinkedPropMap::handOffTableTo(LinkedPropMap* next) {
  MOZ_ASSERT(hasTable());
  MOZ_ASSERT(!next->hasTable());

  next->data_.table = data_.table;
  data_.table = nullptr;

  // Note: for tables currently only sizeof(PropMapTable) is tracked.
  RemoveCellMemory(this, sizeof(PropMapTable), MemoryUse::PropMapTable);
  AddCellMemory(next, sizeof(PropMapTable), MemoryUse::PropMapTable);
}

void SharedPropMap::fixupAfterMovingGC() {
  SharedChildrenPtr& childrenRef = treeDataRef().children;
  if (childrenRef.isNone()) {
    return;
  }

  if (!hasChildrenSet()) {
    SharedPropMapAndIndex child = childrenRef.toSingleChild();
    if (gc::IsForwarded(child.map())) {
      child = SharedPropMapAndIndex(gc::Forwarded(child.map()), child.index());
      childrenRef.setSingleChild(child);
    }
    return;
  }

  SharedChildrenSet* set = childrenRef.toChildrenSet();
  for (SharedChildrenSet::Enum e(*set); !e.empty(); e.popFront()) {
    SharedPropMapAndIndex child = e.front();
    if (IsForwarded(child.map())) {
      child = SharedPropMapAndIndex(Forwarded(child.map()), child.index());
      e.mutableFront() = child;
    }
  }
}

void SharedPropMap::removeChild(JSFreeOp* fop, SharedPropMap* child) {
  SharedPropMapAndIndex& parentRef = child->treeDataRef().parent;
  MOZ_ASSERT(parentRef.map() == this);

  uint32_t index = parentRef.index();
  parentRef.setNone();

  SharedChildrenPtr& childrenRef = treeDataRef().children;
  MOZ_ASSERT(!childrenRef.isNone());

  if (!hasChildrenSet()) {
    MOZ_ASSERT(childrenRef.toSingleChild().map() == child);
    MOZ_ASSERT(childrenRef.toSingleChild().index() == index);
    childrenRef.setNone();
    return;
  }

  SharedChildrenSet* set = childrenRef.toChildrenSet();
  {
    uint32_t nextIndex = SharedPropMap::indexOfNextProperty(index);
    SharedChildrenHasher::Lookup lookup(
        child->getPropertyInfoWithKey(nextIndex), index);
    auto p = set->lookup(lookup);
    MOZ_ASSERT(p, "Child must be in children set");
    set->remove(p);
  }

  MOZ_ASSERT(set->count() >= 1);

  if (set->count() == 1) {
    // Convert from set form back to single child form.
    SharedChildrenSet::Range r = set->all();
    SharedPropMapAndIndex remainingChild = r.front();
    childrenRef.setSingleChild(remainingChild);
    clearHasChildrenSet();
    fop->delete_(this, set, MemoryUse::PropMapChildren);
  }
}

void LinkedPropMap::purgeTable(JSFreeOp* fop) {
  MOZ_ASSERT(hasTable());
  fop->delete_(this, data_.table, MemoryUse::PropMapTable);
  data_.table = nullptr;
}

uint32_t PropMap::approximateEntryCount() const {
  // Returns a number that's guaranteed to tbe >= the exact number of properties
  // in this map (including previous maps). This is used to reserve space in the
  // HashSet when allocating a table for this map.

  const PropMap* map = this;
  uint32_t count = 0;
  JS::AutoCheckCannotGC nogc;
  while (true) {
    if (!map->hasPrevious()) {
      return count + PropMap::Capacity;
    }
    if (PropMapTable* table = map->asLinked()->maybeTable(nogc)) {
      return count + table->entryCount();
    }
    count += PropMap::Capacity;
    map = map->asLinked()->previous();
  }
}

bool PropMapTable::init(JSContext* cx, LinkedPropMap* map) {
  if (!set_.reserve(map->approximateEntryCount())) {
    ReportOutOfMemory(cx);
    return false;
  }

  PropMap* curMap = map;
  while (true) {
    for (uint32_t i = 0; i < PropMap::Capacity; i++) {
      if (curMap->hasKey(i)) {
        PropertyKey key = curMap->getKey(i);
        set_.putNewInfallible(key, PropMapAndIndex(curMap, i));
      }
    }
    if (!curMap->hasPrevious()) {
      break;
    }
    curMap = curMap->asLinked()->previous();
  }

  return true;
}

void PropMapTable::trace(JSTracer* trc) {
  purgeCache();

  for (Set::Enum e(set_); !e.empty(); e.popFront()) {
    PropMap* map = e.front().map();
    TraceManuallyBarrieredEdge(trc, &map, "PropMapTable map");
    if (map != e.front().map()) {
      e.mutableFront() = PropMapAndIndex(map, e.front().index());
    }
  }
}

#ifdef JSGC_HASH_TABLE_CHECKS
void PropMapTable::checkAfterMovingGC() {
  for (Set::Enum e(set_); !e.empty(); e.popFront()) {
    PropMap* map = e.front().map();
    MOZ_ASSERT(map);
    CheckGCThingAfterMovingGC(map);

    PropertyKey key = map->getKey(e.front().index());
    MOZ_RELEASE_ASSERT(!key.isVoid());

    auto p = lookupRaw(key);
    MOZ_RELEASE_ASSERT(p.found() && *p == e.front());
  }
}
#endif

#ifdef DEBUG
bool LinkedPropMap::canSkipMarkingTable() {
  if (!hasTable()) {
    return true;
  }

  PropMapTable* table = data_.table;
  uint32_t count = 0;

  PropMap* map = this;
  while (true) {
    for (uint32_t i = 0; i < Capacity; i++) {
      if (map->hasKey(i)) {
        PropertyKey key = map->getKey(i);
        PropMapTable::Ptr p = table->lookupRaw(key);
        MOZ_ASSERT(*p == PropMapAndIndex(map, i));
        count++;
      }
    }
    if (!map->hasPrevious()) {
      break;
    }
    map = map->asLinked()->previous();
  }

  return count == table->entryCount();
}
#endif

bool LinkedPropMap::createTable(JSContext* cx) {
  MOZ_ASSERT(canHaveTable());
  MOZ_ASSERT(!hasTable());

  UniquePtr<PropMapTable> table = cx->make_unique<PropMapTable>();
  if (!table) {
    return false;
  }

  if (!table->init(cx, this)) {
    return false;
  }

  data_.table = table.release();
  // TODO: The contents of PropMapTable is not currently tracked, only the
  // object itself.
  AddCellMemory(this, sizeof(PropMapTable), MemoryUse::PropMapTable);
  return true;
}

JS::ubi::Node::Size JS::ubi::Concrete<PropMap>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return js::gc::Arena::thingSize(get().asTenured().getAllocKind());
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Zone_inl_h
#define gc_Zone_inl_h

#include "gc/Zone.h"

#include "vm/Runtime.h"

inline bool JS::Zone::maybeGetUniqueId(js::gc::Cell* cell, uint64_t* uidp) {
  MOZ_ASSERT(uidp);
  MOZ_ASSERT(js::CurrentThreadCanAccessZone(this) ||
             js::CurrentThreadIsPerformingGC());

  if (cell->is<JSObject>()) {
    JSObject* obj = cell->as<JSObject>();
    if (obj->is<js::NativeObject>()) {
      auto* nobj = &obj->as<js::NativeObject>();
      if (!nobj->hasUniqueId()) {
        return false;
      }

      *uidp = nobj->uniqueId();
      return true;
    }
  }

  // Get an existing uid, if one has been set.
  auto p = uniqueIds().readonlyThreadsafeLookup(cell);
  if (!p) {
    return false;
  }

  *uidp = p->value();

  return true;
}

inline bool JS::Zone::getOrCreateUniqueId(js::gc::Cell* cell, uint64_t* uidp) {
  MOZ_ASSERT(uidp);
  MOZ_ASSERT(js::CurrentThreadCanAccessZone(this) ||
             js::CurrentThreadIsPerformingGC());

  if (cell->is<JSObject>()) {
    JSObject* obj = cell->as<JSObject>();
    if (obj->is<js::NativeObject>()) {
      auto* nobj = &obj->as<js::NativeObject>();
      if (nobj->hasUniqueId()) {
        *uidp = nobj->uniqueId();
        return true;
      }

      *uidp = js::gc::NextCellUniqueId(runtimeFromAnyThread());
      JSContext* cx = runtimeFromMainThread()->mainContextFromOwnThread();
      return nobj->setUniqueId(cx, *uidp);
    }
  }

  // Get an existing uid, if one has been set.
  auto p = uniqueIds().lookupForAdd(cell);
  if (p) {
    *uidp = p->value();
    return true;
  }

  MOZ_ASSERT(js::CurrentThreadCanAccessZone(this));

  // Set a new uid on the cell.
  *uidp = js::gc::NextCellUniqueId(runtimeFromAnyThread());
  if (!uniqueIds().add(p, cell, *uidp)) {
    return false;
  }

  // If the cell was in the nursery, hopefully unlikely, then we need to
  // tell the nursery about it so that it can sweep the uid if the thing
  // does not get tenured.
  if (IsInsideNursery(cell) &&
      !runtimeFromMainThread()->gc.nursery().addedUniqueIdToCell(cell)) {
    uniqueIds().remove(cell);
    return false;
  }

  return true;
}

inline bool JS::Zone::setOrUpdateUniqueId(JSContext* cx, js::gc::Cell* cell,
                                          uint64_t uid) {
  MOZ_ASSERT(js::CurrentThreadCanAccessZone(this));

  if (cell->is<JSObject>()) {
    JSObject* obj = cell->as<JSObject>();
    if (obj->is<js::NativeObject>()) {
      auto* nobj = &obj->as<js::NativeObject>();
      return nobj->setOrUpdateUniqueId(cx, uid);
    }
  }

  // If the cell was in the nursery, hopefully unlikely, then we need to
  // tell the nursery about it so that it can sweep the uid if the thing
  // does not get tenured.
  if (IsInsideNursery(cell) &&
      !runtimeFromMainThread()->gc.nursery().addedUniqueIdToCell(cell)) {
    return false;
  }

  return uniqueIds().put(cell, uid);
}

inline uint64_t JS::Zone::getUniqueIdInfallible(js::gc::Cell* cell) {
  uint64_t uid;
  js::AutoEnterOOMUnsafeRegion oomUnsafe;
  if (!getOrCreateUniqueId(cell, &uid)) {
    oomUnsafe.crash("failed to allocate uid");
  }
  return uid;
}

inline bool JS::Zone::hasUniqueId(js::gc::Cell* cell) {
  MOZ_ASSERT(js::CurrentThreadCanAccessZone(this) ||
             js::CurrentThreadIsPerformingGC());

  if (cell->is<JSObject>()) {
    JSObject* obj = cell->as<JSObject>();
    if (obj->is<js::NativeObject>()) {
      return obj->as<js::NativeObject>().hasUniqueId();
    }
  }

  return uniqueIds().has(cell);
}

inline void JS::Zone::transferUniqueId(js::gc::Cell* tgt, js::gc::Cell* src) {
  MOZ_ASSERT(src != tgt);
  MOZ_ASSERT(!IsInsideNursery(tgt));
  MOZ_ASSERT(js::CurrentThreadCanAccessRuntime(runtimeFromMainThread()));
  MOZ_ASSERT(js::CurrentThreadCanAccessZone(this));
  MOZ_ASSERT(!uniqueIds().has(tgt));
  uniqueIds().rekeyIfMoved(src, tgt);
}

inline void JS::Zone::removeUniqueId(js::gc::Cell* cell) {
  MOZ_ASSERT(js::CurrentThreadCanAccessZone(this));
  // The cell may no longer be in the hash table if it was swapped with a
  // NativeObject.
  uniqueIds().remove(cell);
}

#endif  // gc_Zone_inl_h

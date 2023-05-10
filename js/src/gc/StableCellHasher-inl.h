/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_StableCellHasher_inl_h
#define gc_StableCellHasher_inl_h

#include "gc/StableCellHasher.h"

#include "gc/Cell.h"
#include "gc/Zone.h"
#include "vm/JSObject.h"
#include "vm/NativeObject.h"
#include "vm/Runtime.h"

namespace js {
namespace gc {

extern uint64_t NextCellUniqueId(JSRuntime* rt);

inline bool MaybeGetUniqueId(Cell* cell, uint64_t* uidp) {
  MOZ_ASSERT(uidp);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cell->runtimeFromAnyThread()) ||
             CurrentThreadIsPerformingGC());

  if (cell->is<JSObject>()) {
    JSObject* obj = cell->as<JSObject>();
    if (obj->is<NativeObject>()) {
      auto* nobj = &obj->as<NativeObject>();
      if (!nobj->hasUniqueId()) {
        return false;
      }

      *uidp = nobj->uniqueId();
      return true;
    }
  }

  // Get an existing uid, if one has been set.
  auto p = cell->zone()->uniqueIds().readonlyThreadsafeLookup(cell);
  if (!p) {
    return false;
  }

  *uidp = p->value();

  return true;
}

inline bool GetOrCreateUniqueId(Cell* cell, uint64_t* uidp) {
  MOZ_ASSERT(uidp);
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cell->runtimeFromAnyThread()) ||
             CurrentThreadIsPerformingGC());

  if (cell->is<JSObject>()) {
    JSObject* obj = cell->as<JSObject>();
    if (obj->is<NativeObject>()) {
      auto* nobj = &obj->as<NativeObject>();
      if (nobj->hasUniqueId()) {
        *uidp = nobj->uniqueId();
        return true;
      }

      JSRuntime* runtime = cell->runtimeFromMainThread();
      *uidp = NextCellUniqueId(runtime);
      JSContext* cx = runtime->mainContextFromOwnThread();
      return nobj->setUniqueId(cx, *uidp);
    }
  }

  // Get an existing uid, if one has been set.
  UniqueIdMap& uniqueIds = cell->zone()->uniqueIds();
  auto p = uniqueIds.lookupForAdd(cell);
  if (p) {
    *uidp = p->value();
    return true;
  }

  // Set a new uid on the cell.
  JSRuntime* runtime = cell->runtimeFromMainThread();
  *uidp = NextCellUniqueId(runtime);
  if (!uniqueIds.add(p, cell, *uidp)) {
    return false;
  }

  // If the cell was in the nursery, hopefully unlikely, then we need to
  // tell the nursery about it so that it can sweep the uid if the thing
  // does not get tenured.
  if (IsInsideNursery(cell) &&
      !runtime->gc.nursery().addedUniqueIdToCell(cell)) {
    uniqueIds.remove(cell);
    return false;
  }

  return true;
}

inline bool SetOrUpdateUniqueId(JSContext* cx, Cell* cell, uint64_t uid) {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cell->runtimeFromAnyThread()));

  if (cell->is<JSObject>()) {
    JSObject* obj = cell->as<JSObject>();
    if (obj->is<NativeObject>()) {
      auto* nobj = &obj->as<NativeObject>();
      return nobj->setOrUpdateUniqueId(cx, uid);
    }
  }

  // If the cell was in the nursery, hopefully unlikely, then we need to
  // tell the nursery about it so that it can sweep the uid if the thing
  // does not get tenured.
  JSRuntime* runtime = cell->runtimeFromMainThread();
  if (IsInsideNursery(cell) &&
      !runtime->gc.nursery().addedUniqueIdToCell(cell)) {
    return false;
  }

  return cell->zone()->uniqueIds().put(cell, uid);
}

inline uint64_t GetUniqueIdInfallible(Cell* cell) {
  uint64_t uid;
  AutoEnterOOMUnsafeRegion oomUnsafe;
  if (!GetOrCreateUniqueId(cell, &uid)) {
    oomUnsafe.crash("failed to allocate uid");
  }
  return uid;
}

inline bool HasUniqueId(Cell* cell) {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cell->runtimeFromAnyThread()) ||
             CurrentThreadIsPerformingGC());

  if (cell->is<JSObject>()) {
    JSObject* obj = cell->as<JSObject>();
    if (obj->is<NativeObject>()) {
      return obj->as<NativeObject>().hasUniqueId();
    }
  }

  return cell->zone()->uniqueIds().has(cell);
}

inline void TransferUniqueId(Cell* tgt, Cell* src) {
  MOZ_ASSERT(src != tgt);
  MOZ_ASSERT(!IsInsideNursery(tgt));
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(tgt->runtimeFromAnyThread()));
  MOZ_ASSERT(src->zone() == tgt->zone());

  Zone* zone = tgt->zone();
  MOZ_ASSERT(!zone->uniqueIds().has(tgt));
  zone->uniqueIds().rekeyIfMoved(src, tgt);
}

inline void RemoveUniqueId(Cell* cell) {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cell->runtimeFromAnyThread()));
  // The cell may no longer be in the hash table if it was swapped with a
  // NativeObject.
  cell->zone()->uniqueIds().remove(cell);
}

}  // namespace gc
}  // namespace js

#endif  // gc_StableCellHasher_inl_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Caches_inl_h
#define vm_Caches_inl_h

#include "vm/Caches.h"

#include "gc/Allocator.h"
#include "gc/GCTrace.h"
#include "vm/Probes.h"
#include "vm/Realm.h"

#include "vm/JSObject-inl.h"

namespace js {

inline bool NewObjectCache::lookupProto(const JSClass* clasp, JSObject* proto,
                                        gc::AllocKind kind,
                                        EntryIndex* pentry) {
  MOZ_ASSERT(!proto->is<GlobalObject>());
  return lookup(clasp, proto, kind, pentry);
}

inline bool NewObjectCache::lookupGlobal(const JSClass* clasp,
                                         GlobalObject* global,
                                         gc::AllocKind kind,
                                         EntryIndex* pentry) {
  return lookup(clasp, global, kind, pentry);
}

inline void NewObjectCache::fillGlobal(EntryIndex entry, const JSClass* clasp,
                                       GlobalObject* global, gc::AllocKind kind,
                                       NativeObject* obj) {
  // MOZ_ASSERT(global == obj->getGlobal());
  return fill(entry, clasp, global, kind, obj);
}

inline NativeObject* NewObjectCache::newObjectFromHit(JSContext* cx,
                                                      EntryIndex entryIndex,
                                                      gc::InitialHeap heap) {
  MOZ_ASSERT(unsigned(entryIndex) < mozilla::ArrayLength(entries));
  Entry* entry = &entries[entryIndex];

  NativeObject* templateObj =
      reinterpret_cast<NativeObject*>(&entry->templateObject);

  // Do an end run around JSObject::group() to avoid doing AutoUnprotectCell
  // on the templateObj, which is not a GC thing and can't use
  // runtimeFromAnyThread.
  ObjectGroup* group = templateObj->group_;

  // If we did the lookup based on the proto we might have a group/object from a
  // different (same-compartment) realm, so we have to do a realm check.
  if (group->realm() != cx->realm()) {
    return nullptr;
  }

  MOZ_ASSERT(!group->hasUnanalyzedPreliminaryObjects());

  {
    AutoSweepObjectGroup sweepGroup(group);
    if (group->shouldPreTenure(sweepGroup)) {
      heap = gc::TenuredHeap;
    }
  }

  if (cx->runtime()->gc.upcomingZealousGC()) {
    return nullptr;
  }

  NativeObject* obj = static_cast<NativeObject*>(AllocateObject<NoGC>(
      cx, entry->kind, /* nDynamicSlots = */ 0, heap, group->clasp()));
  if (!obj) {
    return nullptr;
  }

  copyCachedToObject(obj, templateObj, entry->kind);

  if (group->clasp()->shouldDelayMetadataBuilder()) {
    cx->realm()->setObjectPendingMetadata(cx, obj);
  } else {
    obj = static_cast<NativeObject*>(SetNewObjectMetadata(cx, obj));
  }

  probes::CreateObject(cx, obj);
  gc::gcTracer.traceCreateObject(obj);
  return obj;
}

} /* namespace js */

#endif /* vm_Caches_inl_h */

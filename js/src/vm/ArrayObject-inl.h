/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ArrayObject_inl_h
#define vm_ArrayObject_inl_h

#include "vm/ArrayObject.h"

#include "gc/Allocator.h"
#include "gc/GCProbes.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

namespace js {

/* static */ MOZ_ALWAYS_INLINE ArrayObject* ArrayObject::create(
    JSContext* cx, gc::AllocKind kind, gc::InitialHeap heap,
    Handle<Shape*> shape, uint32_t length, uint32_t slotSpan,
    AutoSetNewObjectMetadata& metadata, gc::AllocSite* site) {
  debugCheckNewObject(shape, kind, heap);

  const JSClass* clasp = &ArrayObject::class_;
  MOZ_ASSERT(shape);
  MOZ_ASSERT(shape->getObjectClass() == clasp);
  MOZ_ASSERT(clasp->isNativeObject());
  MOZ_ASSERT(!clasp->hasFinalize());

  // Note: the slot span is passed as argument to allow more constant folding
  // below for the common case of slotSpan == 0.
  MOZ_ASSERT(shape->slotSpan() == slotSpan);

  // Arrays can use their fixed slots to store elements, so can't have shapes
  // which allow named properties to be stored in the fixed slots.
  MOZ_ASSERT(shape->numFixedSlots() == 0);

  size_t nDynamicSlots = calculateDynamicSlots(0, slotSpan, clasp);
  ArrayObject* aobj =
      cx->newCell<ArrayObject>(kind, nDynamicSlots, heap, clasp, site);
  if (!aobj) {
    return nullptr;
  }

  aobj->initShape(shape);
  // NOTE: Dynamic slots are created internally by Allocate<JSObject>.
  if (!nDynamicSlots) {
    aobj->initEmptyDynamicSlots();
  }

  MOZ_ASSERT(clasp->shouldDelayMetadataBuilder());
  cx->realm()->setObjectPendingMetadata(cx, aobj);

  aobj->initFixedElements(kind, length);

  if (slotSpan > 0) {
    aobj->initDynamicSlots(slotSpan);
  }

  gc::gcprobes::CreateObject(aobj);
  return aobj;
}

}  // namespace js

#endif  // vm_ArrayObject_inl_h

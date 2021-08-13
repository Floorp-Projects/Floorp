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
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/ObjectOperations-inl.h"  // js::GetElement

namespace js {

/* static */ inline ArrayObject* ArrayObject::create(
    JSContext* cx, gc::AllocKind kind, gc::InitialHeap heap, HandleShape shape,
    uint32_t length, AutoSetNewObjectMetadata& metadata, gc::AllocSite* site) {
  debugCheckNewObject(shape, kind, heap);

  const JSClass* clasp = &ArrayObject::class_;
  MOZ_ASSERT(shape);
  MOZ_ASSERT(shape->getObjectClass() == clasp);
  MOZ_ASSERT(clasp->isNativeObject());
  MOZ_ASSERT(!clasp->hasFinalize());

  // Arrays can use their fixed slots to store elements, so can't have shapes
  // which allow named properties to be stored in the fixed slots.
  MOZ_ASSERT(shape->numFixedSlots() == 0);

  size_t slotSpan = shape->slotSpan();
  size_t nDynamicSlots = calculateDynamicSlots(0, slotSpan, clasp);
  JSObject* obj =
      js::AllocateObject(cx, kind, nDynamicSlots, heap, clasp, site);
  if (!obj) {
    return nullptr;
  }

  ArrayObject* aobj = static_cast<ArrayObject*>(obj);
  aobj->initShape(shape);
  // NOTE: Dynamic slots are created internally by Allocate<JSObject>.
  if (!nDynamicSlots) {
    aobj->initEmptyDynamicSlots();
  }

  MOZ_ASSERT(clasp->shouldDelayMetadataBuilder());
  cx->realm()->setObjectPendingMetadata(cx, aobj);

  uint32_t capacity =
      gc::GetGCKindSlots(kind) - ObjectElements::VALUES_PER_HEADER;

  aobj->setFixedElements();
  new (aobj->getElementsHeader()) ObjectElements(capacity, length);

  if (slotSpan > 0) {
    aobj->initializeSlotRange(0, slotSpan);
  }

  gc::gcprobes::CreateObject(aobj);
  return aobj;
}

}  // namespace js

#endif  // vm_ArrayObject_inl_h

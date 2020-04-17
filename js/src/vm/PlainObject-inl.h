/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_PlainObject_inl_h
#define vm_PlainObject_inl_h

#include "vm/PlainObject.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT, MOZ_ASSERT_IF

#include "gc/Allocator.h"     // js::gc::InitialHeap
#include "js/RootingAPI.h"    // JS::Handle, JS::Rooted
#include "vm/NativeObject.h"  // js::NativeObject::create
#include "vm/ObjectGroup.h"   // js::ObjectGroup, js::GenericObject
#include "vm/Shape.h"         // js::Shape

#include "gc/ObjectKind-inl.h"  // js::gc::GetGCObjectKind
#include "vm/JSObject-inl.h"  // js::GetInitialHeap, js::NewBuiltinClassInstance

/* static */ inline JS::Result<js::PlainObject*, JS::OOM&>
js::PlainObject::createWithTemplate(JSContext* cx,
                                    JS::Handle<PlainObject*> templateObject) {
  JS::Rooted<ObjectGroup*> group(cx, templateObject->group());
  MOZ_ASSERT(group->clasp() == &PlainObject::class_);

  gc::InitialHeap heap = GetInitialHeap(GenericObject, group);

  JS::Rooted<Shape*> shape(cx, templateObject->lastProperty());

  gc::AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots());
  MOZ_ASSERT(gc::CanChangeToBackgroundAllocKind(kind, shape->getObjectClass()));
  kind = gc::ForegroundToBackgroundAllocKind(kind);

  return NativeObject::create(cx, kind, heap, shape, group)
      .map([](NativeObject* obj) { return &obj->as<PlainObject>(); });
}

inline js::gc::AllocKind js::PlainObject::allocKindForTenure() const {
  gc::AllocKind kind = gc::GetGCObjectFixedSlotsKind(numFixedSlots());
  MOZ_ASSERT(!gc::IsBackgroundFinalized(kind));
  MOZ_ASSERT(gc::CanChangeToBackgroundAllocKind(kind, getClass()));
  return gc::ForegroundToBackgroundAllocKind(kind);
}

namespace js {

/* Make an object with pregenerated shape from a NEWOBJECT bytecode. */
static inline PlainObject* CopyInitializerObject(
    JSContext* cx, JS::Handle<PlainObject*> baseobj,
    NewObjectKind newKind = GenericObject) {
  MOZ_ASSERT(!baseobj->inDictionaryMode());

  gc::AllocKind allocKind =
      gc::GetGCObjectFixedSlotsKind(baseobj->numFixedSlots());
  allocKind = gc::ForegroundToBackgroundAllocKind(allocKind);
  MOZ_ASSERT_IF(baseobj->isTenured(),
                allocKind == baseobj->asTenured().getAllocKind());
  JS::Rooted<PlainObject*> obj(
      cx, NewBuiltinClassInstance<PlainObject>(cx, allocKind, newKind));
  if (!obj) {
    return nullptr;
  }

  if (!obj->setLastProperty(cx, baseobj->lastProperty())) {
    return nullptr;
  }

  return obj;
}

}  // namespace js

#endif  // vm_PlainObject_inl_h

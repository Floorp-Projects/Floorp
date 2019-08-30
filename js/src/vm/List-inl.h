/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_List_inl_h
#define vm_List_inl_h

#include "vm/List.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include <stdint.h>  // uint32_t

#include "gc/Rooting.h"
#include "js/Value.h"  // JS::Value, JS::ObjectValue
#include "vm/JSContext.h"
#include "vm/NativeObject.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/Realm-inl.h"  // js::AutoRealm

inline /* static */ js::ListObject* js::ListObject::create(JSContext* cx) {
  return NewObjectWithNullTaggedProto<ListObject>(cx);
}

inline bool js::ListObject::append(JSContext* cx, JS::Handle<JS::Value> value) {
  uint32_t len = length();

  if (!ensureElements(cx, len + 1)) {
    return false;
  }

  ensureDenseInitializedLength(cx, len, 1);
  setDenseElementWithType(cx, len, value);
  return true;
}

inline JS::Value js::ListObject::popFirst(JSContext* cx) {
  uint32_t len = length();
  MOZ_ASSERT(len > 0);

  JS::Value entry = get(0);
  if (!tryShiftDenseElements(1)) {
    moveDenseElements(0, 1, len - 1);
    setDenseInitializedLength(len - 1);
    shrinkElements(cx, len - 1);
  }

  MOZ_ASSERT(length() == len - 1);
  return entry;
}

template <class T>
inline T& js::ListObject::popFirstAs(JSContext* cx) {
  return popFirst(cx).toObject().as<T>();
}

namespace js {

/**
 * Stores an empty ListObject in the given fixed slot of |obj|.
 */
inline MOZ_MUST_USE bool StoreNewListInFixedSlot(JSContext* cx,
                                                 JS::Handle<NativeObject*> obj,
                                                 uint32_t slot) {
  AutoRealm ar(cx, obj);
  ListObject* list = ListObject::create(cx);
  if (!list) {
    return false;
  }

  obj->setFixedSlot(slot, JS::ObjectValue(*list));
  return true;
}

}  // namespace js

#endif  // vm_List_inl_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ReceiverGuard_inl_h
#define vm_ReceiverGuard_inl_h

#include "vm/ReceiverGuard.h"

#include "vm/JSObject.h"
#include "wasm/TypedObject.h"

namespace js {

MOZ_ALWAYS_INLINE
ReceiverGuard::ReceiverGuard(JSObject* obj) : group_(nullptr), shape_(nullptr) {
  if (obj->isNative() || IsProxy(obj)) {
    shape_ = obj->shape();
    return;
  }
  MOZ_ASSERT(obj->is<TypedObject>());
  group_ = obj->group();
}

}  // namespace js

#endif /* vm_ReceiverGuard_inl_h */

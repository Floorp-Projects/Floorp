/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Watchtower.h"

#include "js/Id.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/NativeObject.h"

#include "vm/JSObject-inl.h"

using namespace js;

static bool ReshapeForShadowedProp(JSContext* cx, HandleNativeObject obj,
                                   HandleId id) {
  MOZ_ASSERT(obj->isUsedAsPrototype());

  // Lookups on integer ids cannot be cached through prototypes.
  if (JSID_IS_INT(id)) {
    return true;
  }

  RootedObject proto(cx, obj->staticPrototype());
  while (proto) {
    // Lookups will not be cached through non-native protos.
    if (!proto->is<NativeObject>()) {
      break;
    }

    if (proto->as<NativeObject>().contains(cx, id)) {
      return JSObject::setInvalidatedTeleporting(cx, proto);
    }

    proto = proto->staticPrototype();
  }

  return true;
}

// static
bool Watchtower::watchPropertyAddSlow(JSContext* cx, HandleNativeObject obj,
                                      HandleId id) {
  MOZ_ASSERT(watchesPropertyAdd(obj));

  // If |obj| is a prototype of another object, check if we're shadowing a
  // property on its proto chain. In this case we need to reshape that object
  // for shape teleporting to work correctly.
  //
  // See also the 'Shape Teleporting Optimization' comment in jit/CacheIR.cpp.
  if (obj->isUsedAsPrototype()) {
    if (!ReshapeForShadowedProp(cx, obj, id)) {
      return false;
    }
  }

  return true;
}

static bool ReshapeForProtoMutation(JSContext* cx, HandleObject obj) {
  // To avoid the JIT guarding on each prototype in the proto chain to detect
  // prototype mutation, we can instead reshape the rest of the proto chain such
  // that a guard on any of them is sufficient. To avoid excessive reshaping and
  // invalidation, we apply heuristics to decide when to apply this and when
  // to require a guard.
  //
  // There are two cases:
  //
  // (1) The object is not marked IsUsedAsPrototype. This is the common case.
  //     Because shape implies proto, we rely on the caller changing the
  //     object's shape. The JIT guards on this object's shape or prototype so
  //     there's nothing we have to do here for objects on the proto chain.
  //
  // (2) The object is marked IsUsedAsPrototype. This implies the object may be
  //     participating in shape teleporting. To invalidate JIT ICs depending on
  //     the proto chain being unchanged, set the InvalidatedTeleporting shape
  //     flag for this object and objects on its proto chain.
  //
  //     This flag disables future shape teleporting attempts, so next time this
  //     happens the loop below will be a no-op.
  //
  // NOTE: We only handle NativeObjects and don't propagate reshapes through
  //       any non-native objects on the chain.
  //
  // See Also:
  //  - GeneratePrototypeGuards
  //  - GeneratePrototypeHoleGuards

  MOZ_ASSERT(obj->isUsedAsPrototype());

  RootedObject pobj(cx, obj);

  while (pobj && pobj->is<NativeObject>()) {
    if (!pobj->hasInvalidatedTeleporting()) {
      if (!JSObject::setInvalidatedTeleporting(cx, pobj)) {
        return false;
      }
    }
    pobj = pobj->staticPrototype();
  }

  return true;
}

// static
bool Watchtower::watchProtoChangeSlow(JSContext* cx, HandleObject obj) {
  MOZ_ASSERT(watchesProtoChange(obj));

  if (obj->isUsedAsPrototype()) {
    if (!ReshapeForProtoMutation(cx, obj)) {
      return false;
    }
  }

  return true;
}

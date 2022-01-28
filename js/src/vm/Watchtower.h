/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Watchtower_h
#define vm_Watchtower_h

#include "js/Id.h"
#include "js/TypeDecls.h"
#include "vm/JSContext.h"
#include "vm/NativeObject.h"

namespace js {

// [SMDOC] Watchtower
//
// Watchtower is a framework to hook into changes to certain objects. This gives
// us the ability to, for instance, invalidate caches or purge Warp code on
// object layout changes.
//
// Watchtower is only used for objects with certain ObjectFlags set on the
// Shape. This minimizes performance overhead for most objects.
//
// We currently use Watchtower for:
//
// - Invalidating the shape teleporting optimization. See the "Shape Teleporting
//   Optimization" SMDOC comment in CacheIR.cpp.
class Watchtower {
  static bool watchPropertyAddSlow(JSContext* cx, HandleNativeObject obj,
                                   HandleId id);
  static bool watchProtoChangeSlow(JSContext* cx, HandleObject obj);

 public:
  static bool watchesPropertyAdd(NativeObject* obj) {
    return obj->isUsedAsPrototype();
  }
  static bool watchesProtoChange(JSObject* obj) {
    return obj->isUsedAsPrototype();
  }

  static bool watchPropertyAdd(JSContext* cx, HandleNativeObject obj,
                               HandleId id) {
    if (MOZ_LIKELY(!watchesPropertyAdd(obj))) {
      return true;
    }
    return watchPropertyAddSlow(cx, obj, id);
  }
  static bool watchProtoChange(JSContext* cx, HandleObject obj) {
    if (MOZ_LIKELY(!watchesProtoChange(obj))) {
      return true;
    }
    return watchProtoChangeSlow(cx, obj);
  }
};

}  // namespace js

#endif /* vm_Watchtower_h */

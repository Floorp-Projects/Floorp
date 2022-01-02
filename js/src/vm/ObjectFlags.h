/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ObjectFlags_h
#define vm_ObjectFlags_h

#include "util/EnumFlags.h"  // js::EnumFlags

namespace js {

// Flags set on the Shape which describe the referring object. Once set these
// cannot be unset (except during object densification of sparse indexes), and
// are transferred from shape to shape as the object's last property changes.
//
// If you add a new flag here, please add appropriate code to JSObject::dump to
// dump it as part of the object representation.
enum class ObjectFlag : uint16_t {
  IsUsedAsPrototype = 1 << 0,
  NotExtensible = 1 << 1,
  Indexed = 1 << 2,
  HasInterestingSymbol = 1 << 3,

  // If set, the shape's property map may contain an enumerable property. This
  // only accounts for (own) shape properties: if the flag is not set, the
  // object may still have (enumerable) dense elements, typed array elements, or
  // a JSClass enumeration hook.
  HasEnumerable = 1 << 4,

  FrozenElements = 1 << 5,  // See ObjectElements::FROZEN comment.

  // If set, the shape teleporting optimization can no longer be used for
  // accessing properties on this object.
  // See: JSObject::hasInvalidatedTeleporting, ProtoChainSupportsTeleporting.
  InvalidatedTeleporting = 1 << 6,

  ImmutablePrototype = 1 << 7,

  // See JSObject::isQualifiedVarObj().
  QualifiedVarObj = 1 << 8,

  // If set, the object may have a non-writable property or an accessor
  // property.
  //
  // * This is only set for PlainObjects because we only need it for these
  //   objects and setting it for other objects confuses insertInitialShape.
  //
  // * This flag does not account for properties named "__proto__". This is
  //   because |Object.prototype| has a "__proto__" accessor property and we
  //   don't want to include it because it would result in the flag being set on
  //   most proto chains. Code using this flag must check for "__proto__"
  //   property names separately.
  HasNonWritableOrAccessorPropExclProto = 1 << 9,

  // If set, the object either mutated or deleted an accessor property. This is
  // used to invalidate IC/Warp code specializing on specific getter/setter
  // objects. See also the SMDOC comment in vm/GetterSetter.h.
  HadGetterSetterChange = 1 << 10,
};

using ObjectFlags = EnumFlags<ObjectFlag>;

}  // namespace js

#endif /* vm_ObjectFlags_h */

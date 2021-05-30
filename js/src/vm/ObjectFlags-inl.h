/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ObjectFlags_inl_h
#define vm_ObjectFlags_inl_h

#include "vm/ObjectFlags.h"

#include "vm/PlainObject.h"
#include "vm/PropertyInfo.h"
#include "vm/PropertyKey.h"
#include "vm/Shape.h"

namespace js {

MOZ_ALWAYS_INLINE ObjectFlags GetObjectFlagsForNewProperty(
    Shape* last, jsid id, PropertyFlags propFlags, JSContext* cx) {
  ObjectFlags flags = last->objectFlags();

  uint32_t index;
  if (IdIsIndex(id, &index)) {
    flags.setFlag(ObjectFlag::Indexed);
  } else if (JSID_IS_SYMBOL(id) && JSID_TO_SYMBOL(id)->isInterestingSymbol()) {
    flags.setFlag(ObjectFlag::HasInterestingSymbol);
  }

  if ((!propFlags.isDataProperty() || !propFlags.writable()) &&
      last->getObjectClass() == &PlainObject::class_ &&
      !id.isAtom(cx->names().proto)) {
    flags.setFlag(ObjectFlag::HasNonWritableOrAccessorPropExclProto);
  }

  return flags;
}

}  // namespace js

#endif /* vm_ObjectFlags_inl_h */

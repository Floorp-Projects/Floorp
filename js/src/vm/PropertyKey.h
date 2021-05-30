/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_PropertyKey_h
#define vm_PropertyKey_h

#include "mozilla/HashFunctions.h"  // mozilla::HashGeneric

#include "js/HashTable.h"   // js::DefaultHasher
#include "js/Id.h"          // JS::PropertyKey
#include "vm/StringType.h"  // JSAtom::hash
#include "vm/SymbolType.h"  // JS::Symbol::hash

namespace js {

static MOZ_ALWAYS_INLINE js::HashNumber HashId(jsid id) {
  // HashGeneric alone would work, but bits of atom and symbol addresses
  // could then be recovered from the hash code. See bug 1330769.
  if (MOZ_LIKELY(JSID_IS_ATOM(id))) {
    return id.toAtom()->hash();
  }
  if (JSID_IS_SYMBOL(id)) {
    return JSID_TO_SYMBOL(id)->hash();
  }
  return mozilla::HashGeneric(JSID_BITS(id));
}

}  // namespace js

namespace mozilla {

template <>
struct DefaultHasher<jsid> {
  using Lookup = jsid;
  static HashNumber hash(jsid id) { return js::HashId(id); }
  static bool match(jsid id1, jsid id2) { return id1 == id2; }
};

}  // namespace mozilla

#endif /* vm_PropertyKey_h */

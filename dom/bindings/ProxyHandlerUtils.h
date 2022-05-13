/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ProxyHandlerUtils_h
#define mozilla_dom_ProxyHandlerUtils_h

#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/TextUtils.h"

#include "js/Id.h"
#include "js/Object.h"  // JS::GetClass
#include "js/PropertyDescriptor.h"
#include "js/String.h"  // JS::AtomToLinearString, JS::GetLinearString{CharAt,Length}
#include "js/TypeDecls.h"

#include "jsfriendapi.h"  // js::StringIsArrayIndex

namespace mozilla::dom {

extern jsid s_length_id;

// A return value of UINT32_MAX indicates "not an array index".  Note, in
// particular, that UINT32_MAX itself is not a valid array index in general.
inline uint32_t GetArrayIndexFromId(JS::Handle<jsid> id) {
  // Much like js::IdIsIndex, except with a fast path for "length" and another
  // fast path for starting with a lowercase ascii char.  Is that second one
  // really needed?  I guess it is because StringIsArrayIndex is out of line...
  // as of now, use id.get() instead of id otherwise operands mismatch error
  // occurs.
  if (MOZ_LIKELY(id.isInt())) {
    return id.toInt();
  }
  if (MOZ_LIKELY(id.get() == s_length_id)) {
    return UINT32_MAX;
  }
  if (MOZ_UNLIKELY(!id.isAtom())) {
    return UINT32_MAX;
  }

  JSLinearString* str = JS::AtomToLinearString(id.toAtom());
  if (MOZ_UNLIKELY(JS::GetLinearStringLength(str) == 0)) {
    return UINT32_MAX;
  }

  char16_t firstChar = JS::GetLinearStringCharAt(str, 0);
  if (MOZ_LIKELY(IsAsciiLowercaseAlpha(firstChar))) {
    return UINT32_MAX;
  }

  uint32_t i;
  return js::StringIsArrayIndex(str, &i) ? i : UINT32_MAX;
}

inline bool IsArrayIndex(uint32_t index) { return index < UINT32_MAX; }

}  // namespace mozilla::dom

#endif /* mozilla_dom_ProxyHandlerUtils_h */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CSSPropFlags_h
#define mozilla_CSSPropFlags_h

#include "mozilla/TypedEnumBits.h"

namespace mozilla {

enum class CSSPropFlags : uint8_t
{
  // This property is not parsed. It is only there for internal use like
  // attribute mapping.
  Inaccessible = 1 << 0,

  // This property's getComputedStyle implementation requires layout to
  // be flushed.
  GetCSNeedsLayoutFlush = 1 << 1,

  // The following two flags along with the pref defines where the this
  // property can be used:
  // * If none of the two flags is presented, the pref completely controls
  //   the availability of this property. And in that case, if it has no
  //   pref, this property is usable everywhere.
  // * If any of the flags is set, this property is always enabled in the
  //   specific contexts regardless of the value of the pref. If there is
  //   no pref for this property at all in this case, it is an internal-
  //   only property, which cannot be used anywhere else, and should be
  //   wrapped in "#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL".
  // Note that, these flags have no effect on the use of aliases of this
  // property.
  // Furthermore, for the purposes of animation (including triggering
  // transitions) these flags are ignored. That is, if the property is disabled
  // by a pref, we will *not* run animations or transitions on it even in
  // UA sheets or chrome.
  EnabledInUASheets = 1 << 2,
  EnabledInChrome = 1 << 3,
  EnabledInUASheetsAndChrome = EnabledInUASheets | EnabledInChrome,
  EnabledMask = EnabledInUASheetsAndChrome,

  // This property can be animated on the compositor.
  CanAnimateOnCompositor = 1 << 4,

  // This property is an internal property that is not represented in
  // the DOM. Properties with this flag are defined in an #ifndef
  // CSS_PROP_LIST_EXCLUDE_INTERNAL section.
  Internal = 1 << 5,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(CSSPropFlags)

} // namespace mozilla

#endif // mozilla_CSSPropFlags_h

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* enum to represent a level in the cascade */

#ifndef mozilla_SheetType_h
#define mozilla_SheetType_h

#include <stdint.h>

namespace mozilla {

// The "origins" of the CSS cascade, from lowest precedence to
// highest (for non-!important rules).
//
// Be sure to update NS_RULE_NODE_LEVEL_MASK when changing the number
// of sheet types; static assertions enforce this.
enum class SheetType : uint8_t {
  Agent, // CSS
  User, // CSS
  PresHint,
  SVGAttrAnimation,
  Doc, // CSS
  ScopedDoc,
  StyleAttr,
  Override, // CSS
  Animation,
  Transition,

  Count,
  Unknown = 0xff
};

} // namespace mozilla

#endif // mozilla_SheetType_h

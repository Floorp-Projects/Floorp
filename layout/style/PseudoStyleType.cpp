/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PseudoStyleType.h"

#include <ostream>

namespace mozilla {

std::ostream& operator<<(std::ostream& aStream, PseudoStyleType aType) {
  switch (aType) {
#define CSS_PSEUDO_ELEMENT(_name, _value, _flags) \
  case PseudoStyleType::_name:                    \
    aStream << _value;                            \
    break;
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT

#define CSS_ANON_BOX(_name, _str) \
  case PseudoStyleType::_name:    \
    aStream << _str;              \
    break;
#include "nsCSSAnonBoxList.h"
#undef CSS_ANON_BOX

    case PseudoStyleType::XULTree:
    case PseudoStyleType::NotPseudo:
    default:
      // Output nothing.
      break;
  }

  return aStream;
}

};  // namespace mozilla

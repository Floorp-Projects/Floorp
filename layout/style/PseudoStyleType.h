/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PseudoStyleType_h
#define mozilla_PseudoStyleType_h

#include <cstdint>
#include <iosfwd>

namespace mozilla {

// The kind of pseudo-style that we have. This can be:
//
//  * CSS pseudo-elements (see nsCSSPseudoElements.h).
//  * Anonymous boxes (see nsCSSAnonBoxes.h).
//  * XUL tree pseudo-element stuff.
//
// This roughly corresponds to the `PseudoElement` enum in Rust code.
enum class PseudoStyleType : uint8_t {
// If CSS pseudo-elements stop being first here, change GetPseudoType.
#define CSS_PSEUDO_ELEMENT(_name, _value, _flags) _name,
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT
  CSSPseudoElementsEnd,
  AnonBoxesStart = CSSPseudoElementsEnd,
  InheritingAnonBoxesStart = CSSPseudoElementsEnd,

  // Dummy variant so the next variant also has the same discriminant as
  // AnonBoxesStart.
  __reset_1 = AnonBoxesStart - 1,

#define CSS_ANON_BOX(_name, _str) _name,
#define CSS_NON_INHERITING_ANON_BOX(_name, _str)
#define CSS_WRAPPER_ANON_BOX(_name, _str)
#include "nsCSSAnonBoxList.h"
#undef CSS_ANON_BOX
#undef CSS_WRAPPER_ANON_BOX
#undef CSS_NON_INHERITING_ANON_BOX

  // Wrapper anon boxes are inheriting anon boxes.
  WrapperAnonBoxesStart,

  // Dummy variant so the next variant also has the same discriminant as
  // WrapperAnonBoxesStart.
  __reset_2 = WrapperAnonBoxesStart - 1,

#define CSS_ANON_BOX(_name, _str)
#define CSS_NON_INHERITING_ANON_BOX(_name, _str)
#define CSS_WRAPPER_ANON_BOX(_name, _str) _name,
#include "nsCSSAnonBoxList.h"
#undef CSS_ANON_BOX
#undef CSS_WRAPPER_ANON_BOX
#undef CSS_NON_INHERITING_ANON_BOX

  WrapperAnonBoxesEnd,
  InheritingAnonBoxesEnd = WrapperAnonBoxesEnd,
  NonInheritingAnonBoxesStart = WrapperAnonBoxesEnd,

  __reset_3 = NonInheritingAnonBoxesStart - 1,

#define CSS_ANON_BOX(_name, _str)
#define CSS_NON_INHERITING_ANON_BOX(_name, _str) _name,
#include "nsCSSAnonBoxList.h"
#undef CSS_ANON_BOX
#undef CSS_NON_INHERITING_ANON_BOX

  NonInheritingAnonBoxesEnd,
  AnonBoxesEnd = NonInheritingAnonBoxesEnd,

  XULTree = AnonBoxesEnd,
  NotPseudo,
  MAX
};

std::ostream& operator<<(std::ostream&, PseudoStyleType);

class PseudoStyle final {
 public:
  using Type = PseudoStyleType;

  // This must match EAGER_PSEUDO_COUNT in Rust code.
  static const size_t kEagerPseudoCount = 4;

  static bool IsPseudoElement(Type aType) {
    return aType < Type::CSSPseudoElementsEnd;
  }

  static bool IsAnonBox(Type aType) {
    return aType >= Type::AnonBoxesStart && aType < Type::AnonBoxesEnd;
  }

  static bool IsInheritingAnonBox(Type aType) {
    return aType >= Type::InheritingAnonBoxesStart &&
           aType < Type::InheritingAnonBoxesEnd;
  }

  static bool IsNonInheritingAnonBox(Type aType) {
    return aType >= Type::NonInheritingAnonBoxesStart &&
           aType < Type::NonInheritingAnonBoxesEnd;
  }

  static bool IsWrapperAnonBox(Type aType) {
    return aType >= Type::WrapperAnonBoxesStart &&
           aType < Type::WrapperAnonBoxesEnd;
  }
};

}  // namespace mozilla

#endif

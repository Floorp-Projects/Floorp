/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS anonymous boxes */

#ifndef nsCSSAnonBoxes_h___
#define nsCSSAnonBoxes_h___

#include "nsAtom.h"
#include "nsGkAtoms.h"
#include "mozilla/PseudoStyleType.h"

class nsCSSAnonBoxes {
  using PseudoStyleType = mozilla::PseudoStyleType;
  using PseudoStyle = mozilla::PseudoStyle;

 public:
  static bool IsTreePseudoElement(nsAtom* aPseudo);
  static bool IsNonElement(PseudoStyleType aPseudo) {
    return aPseudo == PseudoStyleType::mozText ||
           aPseudo == PseudoStyleType::oofPlaceholder ||
           aPseudo == PseudoStyleType::firstLetterContinuation;
  }

  enum class NonInheriting : uint8_t {
#define CSS_ANON_BOX(_name, _value) /* nothing */
#define CSS_NON_INHERITING_ANON_BOX(_name, _value) _name,
#include "nsCSSAnonBoxList.h"
#undef CSS_NON_INHERITING_ANON_BOX
#undef CSS_ANON_BOX
    _Count
  };

  // Get the NonInheriting type for a given pseudo tag.  The pseudo tag must
  // test true for IsNonInheritingAnonBox.
  static NonInheriting NonInheritingTypeForPseudoType(PseudoStyleType aType) {
    MOZ_ASSERT(PseudoStyle::IsNonInheritingAnonBox(aType));
    static_assert(sizeof(PseudoStyleType) == sizeof(uint8_t), "");
    return static_cast<NonInheriting>(
        static_cast<uint8_t>(aType) -
        static_cast<uint8_t>(PseudoStyleType::NonInheritingAnonBoxesStart));
  }

#ifdef DEBUG
  static nsStaticAtom* GetAtomBase();
  static void AssertAtoms();
#endif

// Alias nsCSSAnonBoxes::foo() to nsGkAtoms::AnonBox_foo.
#define CSS_ANON_BOX(name_, value_)                       \
  static nsCSSAnonBoxPseudoStaticAtom* name_() {          \
    return const_cast<nsCSSAnonBoxPseudoStaticAtom*>(     \
        static_cast<const nsCSSAnonBoxPseudoStaticAtom*>( \
            nsGkAtoms::AnonBox_##name_));                 \
  }
#include "nsCSSAnonBoxList.h"
#undef CSS_ANON_BOX
};

#endif /* nsCSSAnonBoxes_h___ */

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

class nsCSSAnonBoxes {
public:
  static bool IsAnonBox(nsAtom *aAtom);
#ifdef MOZ_XUL
  static bool IsTreePseudoElement(nsAtom* aPseudo);
#endif
  static bool IsNonElement(nsAtom* aPseudo)
  {
    return aPseudo == nsCSSAnonBoxes::mozText() ||
           aPseudo == nsCSSAnonBoxes::oofPlaceholder() ||
           aPseudo == nsCSSAnonBoxes::firstLetterContinuation();
  }

  typedef uint8_t NonInheritingBase;
  enum class NonInheriting : NonInheritingBase {
#define CSS_ANON_BOX(_name, _value) /* nothing */
#define CSS_NON_INHERITING_ANON_BOX(_name, _value) _name,
#include "nsCSSAnonBoxList.h"
#undef CSS_NON_INHERITING_ANON_BOX
#undef CSS_ANON_BOX
    _Count
  };

  // Be careful using this: if we have a lot of non-inheriting anon box types it
  // might not be very fast.  We may want to think of ways to handle that
  // (e.g. by moving to an enum instead of an atom, like we did for
  // pseudo-elements, or by adding a new value of the pseudo-element enum for
  // non-inheriting anon boxes or something).
  static bool IsNonInheritingAnonBox(nsAtom* aPseudo)
  {
    return
#define CSS_ANON_BOX(_name, _value) /* nothing */
#define CSS_NON_INHERITING_ANON_BOX(_name, _value) \
      nsGkAtoms::AnonBox_##_name == aPseudo ||
#include "nsCSSAnonBoxList.h"
#undef CSS_NON_INHERITING_ANON_BOX
#undef CSS_ANON_BOX
      false;
  }

#ifdef DEBUG
  // NOTE(emilio): DEBUG only because this does a pretty slow linear search. Try
  // to use IsNonInheritingAnonBox if you know the atom is an anon box already
  // or, even better, nothing like this.  Note that this function returns true
  // for wrapper anon boxes as well, since they're all inheriting.
  static bool IsInheritingAnonBox(nsAtom* aPseudo)
  {
    return
#define CSS_ANON_BOX(_name, _value) nsGkAtoms::AnonBox_##_name == aPseudo ||
#define CSS_NON_INHERITING_ANON_BOX(_name, _value) /* nothing */
#include "nsCSSAnonBoxList.h"
#undef CSS_NON_INHERITING_ANON_BOX
#undef CSS_ANON_BOX
      false;
  }
#endif // DEBUG

  // This function is rather slow; you probably don't want to use it outside
  // asserts unless you have to.
  static bool IsWrapperAnonBox(nsAtom* aPseudo) {
    // We commonly get null passed here, and want to quickly return false for
    // it.
    return aPseudo &&
      (
#define CSS_ANON_BOX(_name, _value) /* nothing */
#define CSS_WRAPPER_ANON_BOX(_name, _value) nsGkAtoms::AnonBox_##_name == aPseudo ||
#define CSS_NON_INHERITING_ANON_BOX(_name, _value) /* nothing */
#include "nsCSSAnonBoxList.h"
#undef CSS_NON_INHERITING_ANON_BOX
#undef CSS_WRAPPER_ANON_BOX
#undef CSS_ANON_BOX
       false);
  }

  // Get the NonInheriting type for a given pseudo tag.  The pseudo tag must
  // test true for IsNonInheritingAnonBox.
  static NonInheriting NonInheritingTypeForPseudoTag(nsAtom* aPseudo);

#ifdef DEBUG
  static void AssertAtoms();
#endif

  // Alias nsCSSAnonBoxes::foo() to alias nsGkAtoms::AnonBox_foo.
  #define CSS_ANON_BOX(name_, value_)                     \
    static nsICSSAnonBoxPseudo* name_()                   \
    {                                                     \
      return const_cast<nsICSSAnonBoxPseudo*>(            \
        static_cast<const nsICSSAnonBoxPseudo*>(          \
          nsGkAtoms::AnonBox_##name_));                   \
    }
  #include "nsCSSAnonBoxList.h"
  #undef CSS_ANON_BOX
};

#endif /* nsCSSAnonBoxes_h___ */

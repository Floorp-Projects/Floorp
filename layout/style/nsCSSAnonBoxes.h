/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS anonymous boxes */

#ifndef nsCSSAnonBoxes_h___
#define nsCSSAnonBoxes_h___

#include "nsAtom.h"
#include "nsStaticAtom.h"

// Trivial subclass of nsStaticAtom so that function signatures can require an
// atom from this atom list.
class nsICSSAnonBoxPseudo : public nsStaticAtom
{
public:
  constexpr nsICSSAnonBoxPseudo(const char16_t* aStr, uint32_t aLength,
                                uint32_t aStringOffset)
    : nsStaticAtom(aStr, aLength, aStringOffset)
  {}
};

namespace mozilla {
namespace detail {

struct CSSAnonBoxAtoms
{
  #define CSS_ANON_BOX(name_, value_) NS_STATIC_ATOM_DECL_STRING(name_, value_)
  #include "nsCSSAnonBoxList.h"
  #undef CSS_ANON_BOX

  enum class Atoms {
    #define CSS_ANON_BOX(name_, value_) \
      NS_STATIC_ATOM_ENUM(name_)
    #include "nsCSSAnonBoxList.h"
    #undef CSS_ANON_BOX
    AtomsCount
  };

  const nsICSSAnonBoxPseudo mAtoms[static_cast<size_t>(Atoms::AtomsCount)];
};

} // namespace detail
} // namespace mozilla

class nsCSSAnonBoxes {
public:

  static void RegisterStaticAtoms();

  static bool IsAnonBox(nsAtom *aAtom);
#ifdef MOZ_XUL
  static bool IsTreePseudoElement(nsAtom* aPseudo);
#endif
  static bool IsNonElement(nsAtom* aPseudo)
  {
    return aPseudo == mozText || aPseudo == oofPlaceholder ||
           aPseudo == firstLetterContinuation;
  }

private:
  static const nsStaticAtom* const sAtoms;
  static constexpr size_t sAtomsLen =
    static_cast<size_t>(mozilla::detail::CSSAnonBoxAtoms::Atoms::AtomsCount);

public:
  #define CSS_ANON_BOX(name_, value_) \
    NS_STATIC_ATOM_DECL_PTR(nsICSSAnonBoxPseudo, name_)
  #include "nsCSSAnonBoxList.h"
  #undef CSS_ANON_BOX

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
#define CSS_NON_INHERITING_ANON_BOX(_name, _value) _name == aPseudo ||
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
#define CSS_ANON_BOX(_name, _value) _name == aPseudo ||
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
#define CSS_WRAPPER_ANON_BOX(_name, _value) _name == aPseudo ||
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
};

#endif /* nsCSSAnonBoxes_h___ */
